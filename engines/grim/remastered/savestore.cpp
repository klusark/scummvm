/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/compression/deflate.h"
#include "common/config-manager.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "graphics/pixelformat.h"
#include "graphics/surface.h"

#include "engines/grim/remastered/overlay.h"
#include "engines/grim/remastered/savestore.h"

namespace Grim {

static const char *const slotPrefix = "grim-remastered-savedata";
static const char *const gameFileName = "grim.gsv";
static const char *const metaFileName = "meta.gsv";

// Where each year of the game starts and ends in the list of the 60 puzzles
static const int yearStart[4] = { 0, 17, 32, 46 };
static const int yearEnd[4] = { 17, 32, 46, 60 };

// The bits of the puzzles of the year in the metadata
static const int puzzleBits = 22;

Common::String SaveStore::getSlotName(int slot) {
	return Common::String::format("%s%04d", slotPrefix, slot);
}

Common::FSNode SaveStore::getRootNode() {
	return Common::FSNode(ConfMan.getPath("savepath"));
}

bool SaveStore::parseSlotName(const Common::String &name, int *slot) {
	if (!name.hasPrefixIgnoreCase(slotPrefix))
		return false;

	const char *digits = name.c_str() + strlen(slotPrefix);
	int value = 0;
	int count = 0;

	for (; *digits; digits++) {
		if (*digits < '0' || *digits > '9' || count >= 9)
			return false;

		value = value * 10 + (*digits - '0');
		count++;
	}

	if (!count)
		return false;

	*slot = value;

	return true;
}

uint32 SaveStore::adler32(const byte *data, uint32 size) {
	uint32 sumA = 1;
	uint32 sumB = 0;

	for (uint32 i = 0; i < size; i++) {
		sumA = (sumA + data[i]) % 65521;
		sumB = (sumB + sumA) % 65521;
	}

	return (sumB << 16) | sumA;
}

// The only compressor that the common code offers writes a gzip stream. Its deflate data is the
// same as the one of a zlib stream, so only the header and the trailer are exchanged.
bool SaveStore::compress(const byte *data, uint32 size, Common::Array<byte> &result) {
	Common::MemoryWriteStreamDynamic *memory = new Common::MemoryWriteStreamDynamic(DisposeAfterUse::YES);
	Common::WriteStream *gzip = Common::wrapCompressedWriteStream(memory);

	if (gzip == memory) {
		warning("SaveStore::compress(): the compression is not available");
		delete gzip;

		return false;
	}

	gzip->write(data, size);
	gzip->finalize();

	const byte *packed = memory->getData();
	uint32 packedSize = (uint32)memory->size();
	bool valid = !gzip->err() && packedSize > (uint32)(kGzipHeaderSize + kGzipTrailerSize) &&
			packed[0] == 0x1F && packed[1] == 0x8B && packed[2] == 8 && packed[3] == 0;

	if (valid) {
		uint32 bodySize = packedSize - kGzipHeaderSize - kGzipTrailerSize;
		uint32 checksum = adler32(data, size);

		result.resize(2 + bodySize + 4);
		result[0] = 0x78;
		result[1] = 0x9C;
		memcpy(&result[2], packed + kGzipHeaderSize, bodySize);
		WRITE_BE_UINT32(&result[2 + bodySize], checksum);
	} else {
		warning("SaveStore::compress(): the compressed data has an unexpected format");
	}

	delete gzip;

	return valid;
}

bool SaveStore::uncompress(const byte *data, uint32 size, Common::Array<byte> &result) {
	unsigned long length = kMaxUncompressedSize;

	result.resize(kMaxUncompressedSize);

	if (!Common::inflateZlib(&result[0], &length, data, size)) {
		result.clear();

		return false;
	}

	result.resize((uint)length);

	return true;
}

bool SaveStore::writeFile(const Common::FSNode &node, const byte *data, uint32 size) {
	Common::SeekableWriteStream *file = node.createWriteStream();

	if (!file)
		return false;

	if (size)
		file->write(data, size);

	file->finalize();

	bool written = !file->err();

	delete file;

	return written;
}

// The game file is written first: a slot without it is not listed because it has no metadata
int SaveStore::writeSlot(int slot, const Common::Array<byte> &compressed, int setIndex, const char *mural, const char *title) {
	Common::FSNode folder = getRootNode().getChild(getSlotName(slot));

	if (!folder.exists() && !folder.createDirectory()) {
		warning("SaveStore::writeSlot(): cannot create the folder of the slot %d", slot);
		return kStatusUnknownError;
	}

	if (!writeFile(folder.getChild(gameFileName), compressed.empty() ? nullptr : &compressed[0], (uint32)compressed.size())) {
		warning("SaveStore::writeSlot(): cannot write %s of the slot %d", gameFileName, slot);
		return kStatusUnknownError;
	}

	byte meta[kMetaSize];

	memset(meta, 0, sizeof(meta));
	WRITE_LE_UINT32(meta, packMetadata(setIndex, mural));

	// The title takes 127 characters and the end of the string
	Common::strlcpy((char *)meta + 4, title, kTitleSize);

	if (!writeFile(folder.getChild(metaFileName), meta, sizeof(meta))) {
		warning("SaveStore::writeSlot(): cannot write %s of the slot %d", metaFileName, slot);
		return kStatusUnknownError;
	}

	return kStatusOk;
}

int SaveStore::readSlot(int slot, Common::Array<byte> &compressed) {
	Common::FSNode file = getRootNode().getChild(getSlotName(slot)).getChild(gameFileName);

	if (!file.exists())
		return kStatusUnknownError;

	Common::SeekableReadStream *stream = file.createReadStream();

	if (!stream)
		return kStatusUnknownError;

	int64 size = stream->size();

	// A file without data counts as a missing one
	if (size <= 0 || size > 0x7FFFFFFF) {
		delete stream;

		return kStatusUnknownError;
	}

	compressed.resize((uint)size);

	bool read = stream->read(&compressed[0], (uint32)size) == (uint32)size;

	delete stream;

	if (!read) {
		compressed.clear();

		return kStatusUnknownError;
	}

	return kStatusOk;
}

// The files cannot be removed through the file system nodes: they are emptied, which takes the slot
// out of the list because it needs a meta.gsv of the exact size
int SaveStore::eraseSlot(int slot) {
	Common::FSNode folder = getRootNode().getChild(getSlotName(slot));

	if (!folder.exists() || !folder.isDirectory())
		return kStatusUnknownError;

	bool erased = writeFile(folder.getChild(gameFileName), nullptr, 0);

	erased = writeFile(folder.getChild(metaFileName), nullptr, 0) && erased;

	return erased ? kStatusOk : kStatusUnknownError;
}

void SaveStore::listSlots(Common::Array<SlotInfo> &slots) {
	Common::FSList folders;

	slots.clear();

	if (!getRootNode().getChildren(folders, Common::FSNode::kListDirectoriesOnly))
		return;

	for (uint i = 0; i < folders.size(); i++) {
		int slot = 0;

		if (!parseSlotName(folders[i].getName(), &slot))
			continue;

		Common::SeekableReadStream *meta = folders[i].getChild(metaFileName).createReadStream();

		if (!meta)
			continue;

		byte data[kMetaSize];
		bool valid = meta->size() == kMetaSize && meta->read(data, kMetaSize) == (uint32)kMetaSize;

		delete meta;

		if (!valid)
			continue;

		SlotInfo info;

		info.slot = slot;
		unpackMetadata(READ_LE_UINT32(data), &info.setIndex, &info.mural);
		data[kMetaSize - 1] = '\0';
		info.title = (const char *)data + 4;

		// The slots go in order of number, a new one goes before the first one with a higher number
		uint position = 0;

		while (position < slots.size() && slots[position].slot < slot)
			position++;

		slots.insert_at(position, info);
	}
}

// The metadata keeps the set, the year and the puzzles of that year. The puzzles of the years
// before are all solved: the year is the one of the first puzzle that is not.
uint32 SaveStore::packMetadata(int setIndex, const char *mural) {
	char puzzles[kMuralLength + 1];
	int first = kMuralLength;

	memset(puzzles, '0', kMuralLength);
	puzzles[kMuralLength] = '\0';

	for (int i = 0; i < kMuralLength && mural[i]; i++)
		puzzles[i] = mural[i];

	for (int i = 0; i < kMuralLength; i++) {
		if (puzzles[i] == '0') {
			first = i;
			break;
		}
	}

	int year = (first > 16 ? 1 : 0) + (first > 31 ? 1 : 0) + (first > 45 ? 1 : 0);
	uint32 solved = 0;

	for (int k = 0; k < puzzleBits && yearStart[year] + k != yearEnd[year]; k++) {
		if (puzzles[yearStart[year] + k] == '1')
			solved |= 1u << (puzzleBits - 1 - k);
	}

	return ((uint32)setIndex << 24) | ((uint32)year << puzzleBits) | solved;
}

void SaveStore::unpackMetadata(uint32 packed, int *setIndex, Common::String *mural) {
	char puzzles[kMuralLength + 1];
	int year = (packed >> puzzleBits) & 3;

	*setIndex = (int)(packed >> 24);
	memset(puzzles, '0', kMuralLength);
	puzzles[kMuralLength] = '\0';

	for (int i = 0; year > 0 && i < yearEnd[year - 1]; i++)
		puzzles[i] = '1';

	for (int k = 0; k < puzzleBits && yearStart[year] + k != yearEnd[year]; k++) {
		if ((packed >> (puzzleBits - 1 - k)) & 1)
			puzzles[yearStart[year] + k] = '1';
	}

	*mural = puzzles;
}

void SaveStore::writeTag(Common::WriteStream *stream, const char *tag) {
	stream->write(tag, 4);
}

bool SaveStore::readTag(Common::ReadStream *stream, const char *tag) {
	char found[5];

	memset(found, 0, sizeof(found));
	stream->read(found, 4);

	if (strcmp(found, tag)) {
		warning("SaveStore::readTag(): improper tag %s found in the save file, expected %s", found, tag);

		return false;
	}

	return true;
}

// The module of the commentary does not exist yet: nothing is available and nothing was heard
void SaveStore::writeCommentary(Common::WriteStream *stream) {
	byte heard[kCommentarySize];

	memset(heard, 0, sizeof(heard));
	stream->writeSint32LE(-1);
	stream->write(heard, sizeof(heard));
}

bool SaveStore::readCommentary(Common::ReadStream *stream) {
	byte heard[kCommentarySize];

	stream->readSint32LE();
	stream->read(heard, sizeof(heard));

	return !stream->err() && !stream->eos();
}

// The solved puzzles are light and the others dark, like the mural of the game menu
Graphics::Surface *SaveStore::makeMuralThumbnail(const char *mural) {
	enum {
		kThumbnailWidth = 160,
		kThumbnailHeight = 50,
		kLight = 200
	};

	byte pixels[OverlayManager::kMuralWidth * OverlayManager::kMuralHeight * 4];
	Graphics::Surface small;

	memset(pixels, 0, sizeof(pixels));
	OverlayManager::renderMural(mural, kMuralLength, pixels);
	small.create(OverlayManager::kMuralWidth, OverlayManager::kMuralHeight, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));

	for (int y = 0; y < OverlayManager::kMuralHeight; y++) {
		for (int x = 0; x < OverlayManager::kMuralWidth; x++) {
			int alpha = pixels[(y * OverlayManager::kMuralWidth + x) * 4 + 3];
			int value = kLight * (255 - alpha) / 255;

			*(uint16 *)small.getBasePtr(x, y) = (uint16)small.format.RGBToColor((byte)value, (byte)value, (byte)value);
		}
	}

	Graphics::Surface *scaled = small.scale(kThumbnailWidth, kThumbnailHeight, true);

	small.free();

	return scaled;
}

} // End of namespace Grim
