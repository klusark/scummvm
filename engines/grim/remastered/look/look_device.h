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

#ifndef GRIM_REMASTERED_LOOK_LOOK_DEVICE_H
#define GRIM_REMASTERED_LOOK_LOOK_DEVICE_H

#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_types.h"

namespace Grim {

class RemasteredLookTexture {
public:
	RemasteredLookTexture() : _format(kLookFormatRgba8), _width(0), _height(0), _numLevels(1), _renderTarget(false) {}
	virtual ~RemasteredLookTexture() {}

	LookFormat getFormat() const { return _format; }
	uint getWidth() const { return _width; }
	uint getHeight() const { return _height; }
	uint getNumLevels() const { return _numLevels; }
	bool isRenderTarget() const { return _renderTarget; }
	bool hasMips() const { return _numLevels > 1; }

protected:
	LookFormat _format;
	uint _width;
	uint _height;
	uint _numLevels;
	bool _renderTarget;
};

class RemasteredLookTarget {
public:
	RemasteredLookTarget() : _format(kLookFormatRgba8), _width(0), _height(0) {}
	virtual ~RemasteredLookTarget() {}

	LookFormat getFormat() const { return _format; }
	uint getWidth() const { return _width; }
	uint getHeight() const { return _height; }

protected:
	LookFormat _format;
	uint _width;
	uint _height;
};

class RemasteredLookBuffer {
public:
	RemasteredLookBuffer() : _format(kLookVertexVector3), _numElements(0), _indexed(false) {}
	virtual ~RemasteredLookBuffer() {}

	LookVertexFormat getVertexFormat() const { return _format; }
	uint getNumElements() const { return _numElements; }
	bool isIndexBuffer() const { return _indexed; }

protected:
	LookVertexFormat _format;
	uint _numElements;
	bool _indexed;
};

class RemasteredLookDevice {
public:
	virtual ~RemasteredLookDevice() {}

	virtual bool open(int hostWidth, int hostHeight) = 0;
	virtual void close() = 0;
	virtual const LookCaps &getCaps() const = 0;

	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;

	virtual RemasteredLookTarget *createTarget(uint width, uint height, LookFormat format) = 0;
	virtual RemasteredLookTexture *getTargetTexture(RemasteredLookTarget *target) = 0;
	virtual void destroyTarget(RemasteredLookTarget *&target) = 0;

	virtual RemasteredLookTexture *createTexture(const LookImage &image) = 0;
	virtual void updateTexture(RemasteredLookTexture *texture, const byte *pixels, uint width, uint height) = 0;
	virtual void destroyTexture(RemasteredLookTexture *&texture) = 0;

	virtual RemasteredLookBuffer *createVertexBuffer(LookVertexFormat format, const void *data, uint numVertices) = 0;
	virtual RemasteredLookBuffer *createIndexBuffer(const uint16 *indices, uint count) = 0;
	virtual void destroyBuffer(RemasteredLookBuffer *&buffer) = 0;

	virtual void setTargets(const RemasteredLookTargetSetup &setup) = 0;
	virtual void setDepth(int func, bool write) = 0;
	virtual void setCull(int mode) = 0;
	virtual void setColorMask(bool red, bool green, bool blue, bool alpha) = 0;
	virtual void clear(const float rgba[4], float depth) = 0;
	virtual void drawCall(const LookCall &call) = 0;

	virtual bool readTarget(RemasteredLookTarget *target, byte *rgba, uint width, uint height) = 0;
	virtual bool isContextLost() = 0;
	virtual void abandon() = 0;
};

RemasteredLookDevice *createRemasteredLookDevice();

} // End of namespace Grim

#endif
