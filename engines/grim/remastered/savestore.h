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

#ifndef GRIM_SAVESTORE_H
#define GRIM_SAVESTORE_H

#include "common/array.h"
#include "common/fs.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Graphics {
struct Surface;
}

namespace Grim {

/**
 * The save slots of the remastered. Each slot is a folder inside the saves folder with two
 * files: grim.gsv, the zlib stream of the blocks of the game, and meta.gsv, with the title and
 * the puzzles that are solved. The folder is read and written through the file system nodes
 * because the save file manager only knows about flat files.
 */
class SaveStore {
public:
	// What GetSaveStatus answers to the scripts
	enum Status {
		kStatusWaiting = -1,
		kStatusOk = 0,
		kStatusUnknownError = 1,
		kStatusCorrupt = 2,
		kStatusNoSpace = 3,
		kStatusNotFound = 4
	};

	enum {
		kVersion = 5,
		kMuralLength = 60,
		kTitleSize = 128,
		kMetaSize = 132,
		kMaxUncompressedSize = 0x1000000,
		kMaxCompressedSize = 0x400000
	};

	struct SlotInfo {
		int slot;
		int setIndex;
		Common::String title;
		Common::String mural;
	};

	static Common::String getSlotName(int slot);
	static Common::FSNode getRootNode();

	// The zlib stream, without any header of ours around it
	static bool compress(const byte *data, uint32 size, Common::Array<byte> &result);
	static bool uncompress(const byte *data, uint32 size, Common::Array<byte> &result);

	// Return 0 or the status that the scripts get
	static int writeSlot(int slot, const Common::Array<byte> &compressed, int setIndex, const char *mural, const char *title);
	static int readSlot(int slot, Common::Array<byte> &compressed);
	static int eraseSlot(int slot);

	// The slots that have a meta.gsv of the right size, by slot number
	static void listSlots(Common::Array<SlotInfo> &slots);

	static uint32 packMetadata(int setIndex, const char *mural);
	static void unpackMetadata(uint32 packed, int *setIndex, Common::String *mural);

	static void writeTag(Common::WriteStream *stream, const char *tag);
	static bool readTag(Common::ReadStream *stream, const char *tag);

	// The block of the commentary: the module writes and reads it
	static void writeCommentary(Common::WriteStream *stream);
	static bool readCommentary(Common::ReadStream *stream);

	// The mural of the slot, for the launcher
	static Graphics::Surface *makeMuralThumbnail(const char *mural);

private:
	enum {
		kGzipHeaderSize = 10,
		kGzipTrailerSize = 8
	};

	static bool parseSlotName(const Common::String &name, int *slot);
	static uint32 adler32(const byte *data, uint32 size);
	static bool writeFile(const Common::FSNode &node, const byte *data, uint32 size);
};

} // End of namespace Grim

#endif
