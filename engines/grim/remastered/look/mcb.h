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

#ifndef GRIM_MCB_H
#define GRIM_MCB_H

#include "common/array.h"
#include "common/scummsys.h"

#include "engines/grim/remastered/look/s3tc.h"

namespace Common {
class SeekableReadStream;
}

namespace Grim {

class Mcb {
public:
	enum {
		kHeaderSize = 16,
		kImageHeaderSize = 12,
		kDdsHeaderSize = 128,
		kDdsStructSize = 124,
		kImageTypeStb = 0,
		kImageTypeDds = 2,
		kMaxFileMips = 32,
		kMaxSurfaceMips = 8
	};

	struct Header {
		uint32 totalSize;
		uint32 version;
		uint32 imageCount;
	};

	struct Level {
		uint width;
		uint height;
		uint32 offset;
		uint32 size;
	};

	Mcb();

	bool load(Common::SeekableReadStream *stream);
	void clear();

	bool isLoaded() const { return _loaded; }
	const Header &getHeader() const { return _header; }
	S3tc::Format getFormat() const { return _format; }
	uint getWidth() const { return _width; }
	uint getHeight() const { return _height; }
	uint getFileMipCount() const { return _fileMipCount; }
	uint getLevelCount() const { return _levels.size(); }
	uint getSurfaceMipCount() const { return _surfaceMipCount; }
	uint getUploadedMipCount() const;

	uint getLevelWidth(uint level) const;
	uint getLevelHeight(uint level) const;
	uint32 getLevelSize(uint level) const;
	const byte *getLevelData(uint level) const;
	uint32 getUncompressedSize(uint level) const;
	bool uncompress(uint level, byte *rgba) const;

private:
	bool loadDds(const byte *data, uint32 size);
	bool checkLevel(uint level, const char *caller) const;

	bool _loaded;
	Header _header;
	S3tc::Format _format;
	uint _width;
	uint _height;
	uint _fileMipCount;
	uint _surfaceMipCount;
	Common::Array<byte> _data;
	Common::Array<Level> _levels;
};

} // End of namespace Grim

#endif
