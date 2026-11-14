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

#ifndef GRIM_REMASTERED_LOOK_SETX_H
#define GRIM_REMASTERED_LOOK_SETX_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/remastered/look/lights.h"

namespace Common {
class RandomSource;
class SeekableReadStream;
}

namespace Grim {

class SysStruct;

struct ShadowCaster {
	enum Type {
		kTypeBox = 0
	};

	Common::String name;
	byte type;
	float position[3];
	float rotation[3];
	float scale[3];
	float rotationSpeed[3];
	float currentRotation[3];

	ShadowCaster();
};

struct ZShape {
	enum Type {
		kTypeBox = 0,
		kTypePlane = 1
	};

	Common::String name;
	byte type;
	float position[3];
	float rotation[3];
	float scale[3];
	byte drawPass;
	byte planeTessellation;
	bool hasSetupFilter;
	Common::String setupFilter;

	ZShape();

	bool matchesSetup(const char *setupName) const;
};

class SetX {
public:
	enum {
		kVersionMin = 1,
		kVersionMax = 4,
		kMaxEntries = 128,
		kMaxNameSize = 31,
		kMaxSetupStringSize = 255
	};

	SetX();

	bool load(Common::SeekableReadStream *stream);
	bool loadForSet(SysStruct *hs, const char *setFileName);
	void clear();

	static bool getFileNameForSet(const char *setFileName, Common::String &fileName);

	bool isLoaded() const;
	int getVersion() const;
	uint32 getId() const;
	const Common::String &getFileName() const;

	uint getLightCount() const;
	uint getShadowLightCount() const;
	CharacterLight &getLight(uint index);
	const CharacterLight &getLight(uint index) const;

	uint getCompositorCount() const;
	const LightCompositor &getCompositor(uint index) const;
	const LightCompositor *findClosestCompositor(const float cameraPosition[3]) const;

	uint getShadowCasterCount() const;
	const ShadowCaster &getShadowCaster(uint index) const;

	uint getZShapeCount() const;
	const ZShape &getZShape(uint index) const;

	void resetKeylightStates(const char *setupName);
	void update(float frameTimeFlex, Common::RandomSource &randomSource);

private:
	bool _loaded;
	int _version;
	uint32 _id;
	Common::String _fileName;
	uint _shadowLightCount;
	Common::Array<CharacterLight> _lights;
	Common::Array<LightCompositor> _compositors;
	Common::Array<ShadowCaster> _shadowCasters;
	Common::Array<ZShape> _zShapes;

	bool readBody(Common::SeekableReadStream *stream, int version);

	static bool readBytes(Common::SeekableReadStream *stream, void *buffer, uint32 size);
	static bool readByteValue(Common::SeekableReadStream *stream, byte &value);
	static bool readUint16Value(Common::SeekableReadStream *stream, uint16 &value);
	static bool readUint32Value(Common::SeekableReadStream *stream, uint32 &value);
	static bool readFloatValue(Common::SeekableReadStream *stream, float &value);
	static bool readVector(Common::SeekableReadStream *stream, float vector[3]);
	static bool readColors(Common::SeekableReadStream *stream, LightColors &colors);
	static bool readName(Common::SeekableReadStream *stream, Common::String &name);
	static bool readSetupString(Common::SeekableReadStream *stream, bool &present, Common::String &text);
	static bool readLight(Common::SeekableReadStream *stream, int version, CharacterLight &light);
	static bool readCompositor(Common::SeekableReadStream *stream, int version, LightCompositor &compositor);
	static bool readShadowCaster(Common::SeekableReadStream *stream, ShadowCaster &caster);
	static bool readZShape(Common::SeekableReadStream *stream, ZShape &shape);
	static float wrapAngle(float angle);
};

} // End of namespace Grim

#endif
