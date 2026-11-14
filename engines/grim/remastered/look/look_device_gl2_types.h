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

#ifndef GRIM_REMASTERED_LOOK_LOOK_DEVICE_GL2_TYPES_H
#define GRIM_REMASTERED_LOOK_LOOK_DEVICE_GL2_TYPES_H

#include "common/array.h"
#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_device.h"

namespace Grim {

class RemasteredLookTextureGl : public RemasteredLookTexture {
public:
	RemasteredLookTextureGl() : id(0), wrap(-1), minFilter(-1), magFilter(-1), uploadedAsRgba(false) {}

	void configure(LookFormat format, uint width, uint height, uint numLevels, bool renderTarget) {
		_format = format;
		_width = width;
		_height = height;
		_numLevels = numLevels;
		_renderTarget = renderTarget;
	}

	uint id;
	int wrap;
	int minFilter;
	int magFilter;
	bool uploadedAsRgba;
};

class RemasteredLookTargetGl : public RemasteredLookTarget {
public:
	RemasteredLookTargetGl() : texture(nullptr), renderbuffer(0) {}

	void configure(LookFormat format, uint width, uint height) {
		_format = format;
		_width = width;
		_height = height;
	}

	RemasteredLookTextureGl *texture;
	uint renderbuffer;
};

class RemasteredLookBufferGl : public RemasteredLookBuffer {
public:
	void configure(LookVertexFormat format, uint numElements, bool indexed) {
		_format = format;
		_numElements = numElements;
		_indexed = indexed;
	}

	Common::Array<byte> data;
};

} // End of namespace Grim

#endif
