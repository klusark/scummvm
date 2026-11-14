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

#ifndef GRIM_S3TC_H
#define GRIM_S3TC_H

#include "common/scummsys.h"

namespace Grim {

class S3tc {
public:
	enum Format {
		kDxt1 = 0,
		kDxt3 = 1,
		kDxt5 = 2
	};

	enum {
		kBlockPixels = 16,
		kMaxDimension = 16384
	};

	static uint getBlockBytes(Format format);
	static uint32 getLevelSize(Format format, uint width, uint height);
	static void decodeBlock(Format format, const byte *block, byte *rgba);
	static bool decodeLevel(Format format, uint width, uint height, const byte *blocks, uint32 size, byte *rgba);

private:
	static void expand565(uint color, byte *rgb);
	static void decodeColorBlock(const byte *block, bool isDxt1, byte *rgba);
	static void decodeDxt3Alpha(const byte *block, byte *rgba);
	static void decodeDxt5Alpha(const byte *block, byte *rgba);
};

} // End of namespace Grim

#endif
