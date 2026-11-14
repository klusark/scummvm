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

#include "common/endian.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/remastered/look/mcb.h"

namespace Grim {

enum {
	kDdsMagic = 0x20534444,
	kFourCcDxt1 = 0x31545844,
	kFourCcDxt3 = 0x33545844,
	kFourCcDxt5 = 0x35545844
};

Mcb::Mcb() {
	clear();
}

void Mcb::clear() {
	_loaded = false;
	_header.totalSize = 0;
	_header.version = 0;
	_header.imageCount = 0;
	_format = S3tc::kDxt1;
	_width = 0;
	_height = 0;
	_fileMipCount = 0;
	_surfaceMipCount = 0;
	_data.clear();
	_levels.clear();
}

bool Mcb::load(Common::SeekableReadStream *stream) {
	clear();

	if (!stream) {
		warning("Mcb::load(): no stream");
		return false;
	}

	byte header[kHeaderSize];

	if (stream->read(header, kHeaderSize) != kHeaderSize) {
		warning("Mcb::load(): truncated header");
		return false;
	}

	_header.totalSize = READ_LE_UINT32(header + 4);
	_header.version = READ_LE_UINT32(header + 8);
	_header.imageCount = READ_LE_UINT32(header + 12);

	for (uint32 image = 0; image < _header.imageCount; image++) {
		byte imageHeader[kImageHeaderSize];

		if (stream->read(imageHeader, kImageHeaderSize) != kImageHeaderSize) {
			warning("Mcb::load(): truncated header of image %u", image);
			break;
		}

		uint32 type = READ_LE_UINT32(imageHeader + 4);
		uint32 size = READ_LE_UINT32(imageHeader + 8);

		if ((int64)size > stream->size() - stream->pos()) {
			warning("Mcb::load(): image %u is truncated", image);
			break;
		}

		if (type == kImageTypeDds) {
			Common::Array<byte> data;

			data.resize(size);

			if (size > 0 && stream->read(data.data(), size) != size) {
				warning("Mcb::load(): could not read image %u", image);
				break;
			}

			loadDds(data.data(), size);
		} else {
			if (type == kImageTypeStb)
				warning("Mcb::load(): image %u is an stb image, not supported", image);

			stream->skip(size);
		}
	}

	return _loaded;
}

bool Mcb::loadDds(const byte *data, uint32 size) {
	if (size < kDdsHeaderSize) {
		warning("Mcb::loadDds(): the DDS data is only %u bytes", size);
		return false;
	}

	if (READ_LE_UINT32(data) != (uint32)kDdsMagic || READ_LE_UINT32(data + 4) != (uint32)kDdsStructSize)
		return false;

	uint32 height = READ_LE_UINT32(data + 12);
	uint32 width = READ_LE_UINT32(data + 16);
	uint32 mipCount = READ_LE_UINT32(data + 28);
	uint32 fourCc = READ_LE_UINT32(data + 84);
	S3tc::Format format;

	if (fourCc == (uint32)kFourCcDxt1)
		format = S3tc::kDxt1;
	else if (fourCc == (uint32)kFourCcDxt3)
		format = S3tc::kDxt3;
	else if (fourCc == (uint32)kFourCcDxt5)
		format = S3tc::kDxt5;
	else
		return false;

	if (width == 0 || height == 0 || width > S3tc::kMaxDimension || height > S3tc::kMaxDimension) {
		warning("Mcb::loadDds(): invalid dimensions %ux%u", width, height);
		return false;
	}

	if (mipCount == 0)
		mipCount = 1;

	if (mipCount > kMaxFileMips) {
		warning("Mcb::loadDds(): %u mip levels declared, using %u", mipCount, (uint)kMaxFileMips);
		mipCount = kMaxFileMips;
	}

	uint32 payloadSize = size - kDdsHeaderSize;
	Common::Array<Level> levels;
	uint32 offset = 0;
	uint levelWidth = width;
	uint levelHeight = height;

	for (uint32 level = 0; level < mipCount; level++) {
		uint32 levelSize = S3tc::getLevelSize(format, levelWidth, levelHeight);

		if (levelSize == 0 || payloadSize - offset < levelSize) {
			warning("Mcb::loadDds(): the data ends at mip level %u of %u", level, mipCount);
			break;
		}

		Level info;

		info.width = levelWidth;
		info.height = levelHeight;
		info.offset = offset;
		info.size = levelSize;
		levels.push_back(info);

		offset += levelSize;
		levelWidth = levelWidth > 1 ? levelWidth >> 1 : 1;
		levelHeight = levelHeight > 1 ? levelHeight >> 1 : 1;
	}

	if (levels.empty())
		return false;

	_loaded = true;
	_format = format;
	_width = width;
	_height = height;
	_fileMipCount = mipCount;
	_surfaceMipCount = mipCount > kMaxSurfaceMips ? (uint)kMaxSurfaceMips : mipCount;

	if (_surfaceMipCount > levels.size())
		_surfaceMipCount = levels.size();

	_levels = levels;
	_data.resize(payloadSize);
	memcpy(_data.data(), data + kDdsHeaderSize, payloadSize);

	return true;
}

uint Mcb::getUploadedMipCount() const {
	if (!_loaded)
		return 0;

	uint count = 0;
	uint width = _width;

	for (uint level = 0; level < _surfaceMipCount; level++) {
		count++;

		if (width < 2)
			break;

		width >>= 1;
	}

	return count;
}

bool Mcb::checkLevel(uint level, const char *caller) const {
	if (!_loaded || level >= _levels.size()) {
		warning("Mcb::%s(): level %u is not available", caller, level);
		return false;
	}

	return true;
}

uint Mcb::getLevelWidth(uint level) const {
	if (!checkLevel(level, "getLevelWidth"))
		return 0;

	return _levels[level].width;
}

uint Mcb::getLevelHeight(uint level) const {
	if (!checkLevel(level, "getLevelHeight"))
		return 0;

	return _levels[level].height;
}

uint32 Mcb::getLevelSize(uint level) const {
	if (!checkLevel(level, "getLevelSize"))
		return 0;

	return _levels[level].size;
}

const byte *Mcb::getLevelData(uint level) const {
	if (!checkLevel(level, "getLevelData"))
		return nullptr;

	return &_data[_levels[level].offset];
}

uint32 Mcb::getUncompressedSize(uint level) const {
	if (!checkLevel(level, "getUncompressedSize"))
		return 0;

	return _levels[level].width * _levels[level].height * 4;
}

bool Mcb::uncompress(uint level, byte *rgba) const {
	if (!checkLevel(level, "uncompress"))
		return false;

	const Level &info = _levels[level];

	return S3tc::decodeLevel(_format, info.width, info.height, &_data[info.offset], info.size, rgba);
}

} // End of namespace Grim
