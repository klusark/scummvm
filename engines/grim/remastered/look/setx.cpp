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

#include <math.h>
#include <string.h>

#include "common/endian.h"
#include "common/random.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/remastered/look/setx.h"
#include "engines/grim/std/std_sys.h"

namespace Grim {

LightColors::LightColors() : intensity(0.0f) {
	for (int i = 0; i < 3; i++) {
		innerColor[i] = 0.0f;
		outerColor[i] = 0.0f;
	}
}

CharacterLight::CharacterLight() :
		type(0), shadingBias(0.0f), shadingLimit(0.0f), innerRadius(0.0f), falloffPower(0.0f),
		verticalInnerRadius(0.0f), verticalFalloffPower(0.0f), animationMode(0), animationFrequency(0.0f),
		shadowMode(0), shadowQuality(0), shadowBias(0.0f), shadowSoftness(0.0f), shadowNear(0.0f),
		shadowMaxDistance(0.0f), lodType(0), hasKeylightSetups(false), animationPeriod(0.0f),
		animationTime(0.0f), keylightState(0) {
	for (int i = 0; i < 3; i++) {
		position[i] = 0.0f;
		rotation[i] = 0.0f;
		scale[i] = 0.0f;
	}
}

void CharacterLight::resetAnimationState() {
	fromColors = colorsA;
	toColors = colorsB;
	currentColors = colorsA;
	animationPeriod = 0.0f;
	animationTime = 0.0f;
	keylightState = 0;
}

void CharacterLight::updateAnimation(float step, Common::RandomSource &randomSource) {
	if (animationMode == kAnimationNone)
		return;

	float blend = 0.0f;

	if (animationMode == kAnimationRandom) {
		animationTime += step;

		if (animationTime > animationPeriod) {
			animationTime = 0.0f;

			float basePeriod = 0.0f;
			if (animationFrequency != 0.0f)
				basePeriod = 1.0f / animationFrequency;

			animationPeriod = basePeriod;

			float periodRandom = (float)randomSource.getRandomNumber(10000) / 10000.0f;
			animationPeriod = ((periodRandom * animationPeriod) * 1.25f) + (((1.0f - periodRandom) * animationPeriod) * 0.75f);

			fromColors = toColors;

			float intensityRandom = (float)randomSource.getRandomNumber(10000) / 10000.0f;
			toColors.intensity = (intensityRandom * colorsB.intensity) + ((1.0f - intensityRandom) * colorsA.intensity);

			float innerRandom = (float)randomSource.getRandomNumber(10000) / 10000.0f;
			for (int i = 0; i < 3; i++)
				toColors.innerColor[i] = (colorsB.innerColor[i] * innerRandom) + (colorsA.innerColor[i] * (1.0f - innerRandom));

			float outerRandom = (float)randomSource.getRandomNumber(10000) / 10000.0f;
			for (int i = 0; i < 3; i++)
				toColors.outerColor[i] = (colorsB.outerColor[i] * outerRandom) + (colorsA.outerColor[i] * (1.0f - outerRandom));
		}

		if (animationPeriod != 0.0f)
			blend = animationTime / animationPeriod;
	} else if (animationMode == kAnimationSine) {
		animationTime += step;
		blend = (sinf(animationTime * animationFrequency) + 1.0f) * 0.5f;
	}

	float inverse = 1.0f - blend;

	currentColors.intensity = (toColors.intensity * blend) + (fromColors.intensity * inverse);

	for (int i = 0; i < 3; i++) {
		currentColors.innerColor[i] = (toColors.innerColor[i] * blend) + (fromColors.innerColor[i] * inverse);
		currentColors.outerColor[i] = (toColors.outerColor[i] * blend) + (fromColors.outerColor[i] * inverse);
	}
}

void CharacterLight::resetKeylightState(const char *setupName) {
	const char *needle = setupName ? setupName : "";

	keylightState = 0;

	if (hasKeylightSetups)
		keylightState = keylightSetups.contains(needle) ? 1 : 0;
}

bool CharacterLight::isVisible(int roomLodType) const {
	return roomLodType == kLodAll || lodType == kLodAll || (int)lodType == roomLodType;
}

bool CharacterLight::hasIntensity() const {
	return fabsf(currentColors.intensity) > 0.0019607844f;
}

void CharacterLight::getInnerColor(float color[3]) const {
	for (int i = 0; i < 3; i++)
		color[i] = currentColors.innerColor[i] * currentColors.intensity;
}

void CharacterLight::getOuterColor(float color[3]) const {
	for (int i = 0; i < 3; i++)
		color[i] = currentColors.outerColor[i] * currentColors.intensity;
}

float CharacterLight::getTypeShadowBias() const {
	float factor = 1.0f;

	if (type == kTypeDirectional || type == kTypeDirectionalRectangle)
		factor = 8.0f;

	return shadowBias * factor;
}

void CharacterLight::getExtent(float extent[3]) const {
	for (int i = 0; i < 3; i++)
		extent[i] = scale[i] + scale[i];
}

LightCompositor::LightCompositor() : minLuminance(0.0f), maxLuminance(1.0f), power(1.0f), aoStrength(0.5f) {
	for (int i = 0; i < 3; i++) {
		position[i] = 0.0f;
		shadowWhite[i] = 1.0f;
		shadowBlack[i] = 0.0f;
	}
}

ShadowCaster::ShadowCaster() : type(0) {
	for (int i = 0; i < 3; i++) {
		position[i] = 0.0f;
		rotation[i] = 0.0f;
		scale[i] = 0.0f;
		rotationSpeed[i] = 0.0f;
		currentRotation[i] = 0.0f;
	}
}

ZShape::ZShape() : type(0), drawPass(0), planeTessellation(1), hasSetupFilter(false) {
	for (int i = 0; i < 3; i++) {
		position[i] = 0.0f;
		rotation[i] = 0.0f;
		scale[i] = 0.0f;
	}
}

bool ZShape::matchesSetup(const char *setupName) const {
	const char *needle = setupName ? setupName : "";

	return !hasSetupFilter || setupFilter.contains(needle);
}

SetX::SetX() : _loaded(false), _version(0), _id(0), _shadowLightCount(0) {
}

void SetX::clear() {
	_loaded = false;
	_version = 0;
	_id = 0;
	_fileName.clear();
	_shadowLightCount = 0;
	_lights.clear();
	_compositors.clear();
	_shadowCasters.clear();
	_zShapes.clear();
}

bool SetX::getFileNameForSet(const char *setFileName, Common::String &fileName) {
	const char *dot = strchr(setFileName, '.');

	if (!dot)
		return false;

	fileName = Common::String(setFileName, dot) + ".setx";

	return true;
}

bool SetX::loadForSet(SysStruct *hs, const char *setFileName) {
	Common::String fileName;

	if (!getFileNameForSet(setFileName, fileName))
		return false;

	Common::SeekableReadStream *stream = hs->openFile(fileName.c_str());

	if (!stream)
		return false;

	bool result = load(stream);

	delete stream;

	if (result)
		_fileName = fileName;

	return result;
}

bool SetX::load(Common::SeekableReadStream *stream) {
	clear();

	byte magic[4] = { 0, 0, 0, 0 };
	uint16 version = 0;

	readBytes(stream, magic, 4);

	if (!readUint16Value(stream, version))
		return false;

	if (version < kVersionMin || version > kVersionMax) {
		warning("SetX::load(): unsupported version %d", version);
		return false;
	}

	return readBody(stream, version);
}

bool SetX::readBody(Common::SeekableReadStream *stream, int version) {
	uint32 id = 0;
	uint16 lightCount = 0;
	uint16 compositorCount = 0;
	uint16 shadowCasterCount = 0;
	uint16 zShapeCount = 0;

	if (!readUint32Value(stream, id))
		return false;

	if (!readUint16Value(stream, lightCount) || !readUint16Value(stream, compositorCount) || !readUint16Value(stream, shadowCasterCount))
		return false;

	if (version >= 3 && !readUint16Value(stream, zShapeCount))
		return false;

	if (lightCount >= kMaxEntries || compositorCount >= kMaxEntries || shadowCasterCount >= kMaxEntries || zShapeCount >= kMaxEntries) {
		warning("SetX::readBody(): too many entries (%d lights, %d compositors, %d shadow casters, %d z shapes)",
			lightCount, compositorCount, shadowCasterCount, zShapeCount);
		return false;
	}

	_version = version;

	for (uint i = 0; i < lightCount; i++) {
		CharacterLight light;

		if (!readLight(stream, version, light)) {
			warning("SetX::readBody(): light %u is truncated or invalid", i);
			return false;
		}

		light.resetAnimationState();

		if (light.shadowMode != CharacterLight::kShadowNone)
			_shadowLightCount++;

		_lights.push_back(light);
	}

	for (uint i = 0; i < compositorCount; i++) {
		LightCompositor compositor;

		if (!readCompositor(stream, version, compositor)) {
			warning("SetX::readBody(): compositor %u is truncated or invalid", i);
			return false;
		}

		_compositors.push_back(compositor);
	}

	for (uint i = 0; i < shadowCasterCount; i++) {
		ShadowCaster caster;

		if (!readShadowCaster(stream, caster)) {
			warning("SetX::readBody(): shadow caster %u is truncated or invalid", i);
			return false;
		}

		_shadowCasters.push_back(caster);
	}

	for (uint i = 0; i < zShapeCount; i++) {
		ZShape shape;

		if (!readZShape(stream, shape)) {
			warning("SetX::readBody(): z shape %u is truncated or invalid", i);
			return false;
		}

		_zShapes.push_back(shape);
	}

	_id = id;
	_loaded = true;

	return true;
}

bool SetX::readBytes(Common::SeekableReadStream *stream, void *buffer, uint32 size) {
	return stream->read(buffer, size) == size;
}

bool SetX::readByteValue(Common::SeekableReadStream *stream, byte &value) {
	byte buffer[1];

	if (!readBytes(stream, buffer, 1))
		return false;

	value = buffer[0];

	return true;
}

bool SetX::readUint16Value(Common::SeekableReadStream *stream, uint16 &value) {
	byte buffer[2];

	if (!readBytes(stream, buffer, 2))
		return false;

	value = READ_LE_UINT16(buffer);

	return true;
}

bool SetX::readUint32Value(Common::SeekableReadStream *stream, uint32 &value) {
	byte buffer[4];

	if (!readBytes(stream, buffer, 4))
		return false;

	value = READ_LE_UINT32(buffer);

	return true;
}

bool SetX::readFloatValue(Common::SeekableReadStream *stream, float &value) {
	byte buffer[4];

	if (!readBytes(stream, buffer, 4))
		return false;

	value = READ_LE_FLOAT32(buffer);

	return true;
}

bool SetX::readVector(Common::SeekableReadStream *stream, float vector[3]) {
	byte buffer[12];

	if (!readBytes(stream, buffer, 12))
		return false;

	for (int i = 0; i < 3; i++)
		vector[i] = READ_LE_FLOAT32(buffer + i * 4);

	return true;
}

bool SetX::readColors(Common::SeekableReadStream *stream, LightColors &colors) {
	byte buffer[28];

	if (!readBytes(stream, buffer, 28))
		return false;

	for (int i = 0; i < 3; i++) {
		colors.innerColor[i] = READ_LE_FLOAT32(buffer + i * 4);
		colors.outerColor[i] = READ_LE_FLOAT32(buffer + 12 + i * 4);
	}

	colors.intensity = READ_LE_FLOAT32(buffer + 24);

	return true;
}

bool SetX::readName(Common::SeekableReadStream *stream, Common::String &name) {
	byte length = 0;
	char buffer[256];

	if (!readByteValue(stream, length))
		return false;

	if (!readBytes(stream, buffer, length))
		return false;

	buffer[length] = 0;

	if (length > kMaxNameSize)
		warning("SetX::readName(): name of %d bytes is longer than %d characters", length, (int)kMaxNameSize);

	name = Common::String(buffer);

	return true;
}

bool SetX::readSetupString(Common::SeekableReadStream *stream, bool &present, Common::String &text) {
	uint16 length = 0;
	char buffer[kMaxSetupStringSize + 1];

	if (!readUint16Value(stream, length))
		return false;

	if (length > kMaxSetupStringSize) {
		warning("SetX::readSetupString(): string of %d bytes is longer than %d characters", length, (int)kMaxSetupStringSize);
		return false;
	}

	if (!readBytes(stream, buffer, length))
		return false;

	buffer[length] = 0;
	present = length != 0;
	text = Common::String(buffer);

	return true;
}

bool SetX::readLight(Common::SeekableReadStream *stream, int version, CharacterLight &light) {
	if (!readName(stream, light.name) || !readByteValue(stream, light.type))
		return false;

	if (!readVector(stream, light.position) || !readVector(stream, light.rotation) || !readVector(stream, light.scale))
		return false;

	if (!readFloatValue(stream, light.shadingBias) || !readFloatValue(stream, light.shadingLimit) ||
			!readFloatValue(stream, light.innerRadius) || !readFloatValue(stream, light.falloffPower) ||
			!readFloatValue(stream, light.verticalInnerRadius) || !readFloatValue(stream, light.verticalFalloffPower))
		return false;

	if (!readByteValue(stream, light.animationMode) || !readFloatValue(stream, light.animationFrequency))
		return false;

	if (!readByteValue(stream, light.shadowMode) || !readByteValue(stream, light.shadowQuality))
		return false;

	if (!readFloatValue(stream, light.shadowBias) || !readFloatValue(stream, light.shadowSoftness) ||
			!readFloatValue(stream, light.shadowNear) || !readFloatValue(stream, light.shadowMaxDistance))
		return false;

	light.lodType = 0;
	light.hasKeylightSetups = false;
	light.keylightSetups.clear();

	if (version >= 4) {
		if (!readByteValue(stream, light.lodType) || !readSetupString(stream, light.hasKeylightSetups, light.keylightSetups))
			return false;
	}

	if (!readColors(stream, light.colorsA))
		return false;

	if (light.animationMode != CharacterLight::kAnimationNone) {
		if (!readColors(stream, light.colorsB))
			return false;
	} else {
		light.colorsB = light.colorsA;
	}

	return true;
}

bool SetX::readCompositor(Common::SeekableReadStream *stream, int version, LightCompositor &compositor) {
	if (!readName(stream, compositor.name))
		return false;

	if (!readVector(stream, compositor.position) || !readVector(stream, compositor.shadowWhite) || !readVector(stream, compositor.shadowBlack))
		return false;

	if (!readFloatValue(stream, compositor.minLuminance) || !readFloatValue(stream, compositor.maxLuminance) ||
			!readFloatValue(stream, compositor.power))
		return false;

	compositor.aoStrength = 0.5f;

	if (version >= 2 && !readFloatValue(stream, compositor.aoStrength))
		return false;

	return true;
}

bool SetX::readShadowCaster(Common::SeekableReadStream *stream, ShadowCaster &caster) {
	if (!readName(stream, caster.name) || !readByteValue(stream, caster.type))
		return false;

	if (!readVector(stream, caster.position) || !readVector(stream, caster.rotation) ||
			!readVector(stream, caster.scale) || !readVector(stream, caster.rotationSpeed))
		return false;

	for (int i = 0; i < 3; i++)
		caster.currentRotation[i] = caster.rotation[i];

	return true;
}

bool SetX::readZShape(Common::SeekableReadStream *stream, ZShape &shape) {
	if (!readName(stream, shape.name) || !readByteValue(stream, shape.type))
		return false;

	if (!readVector(stream, shape.position) || !readVector(stream, shape.rotation) || !readVector(stream, shape.scale))
		return false;

	if (!readByteValue(stream, shape.drawPass) || !readSetupString(stream, shape.hasSetupFilter, shape.setupFilter))
		return false;

	shape.planeTessellation = 1;

	if (shape.type == ZShape::kTypePlane && !readByteValue(stream, shape.planeTessellation))
		return false;

	return true;
}

bool SetX::isLoaded() const {
	return _loaded;
}

int SetX::getVersion() const {
	return _version;
}

uint32 SetX::getId() const {
	return _id;
}

const Common::String &SetX::getFileName() const {
	return _fileName;
}

uint SetX::getLightCount() const {
	return _lights.size();
}

uint SetX::getShadowLightCount() const {
	return _shadowLightCount;
}

CharacterLight &SetX::getLight(uint index) {
	return _lights[index];
}

const CharacterLight &SetX::getLight(uint index) const {
	return _lights[index];
}

uint SetX::getCompositorCount() const {
	return _compositors.size();
}

const LightCompositor &SetX::getCompositor(uint index) const {
	return _compositors[index];
}

const LightCompositor *SetX::findClosestCompositor(const float cameraPosition[3]) const {
	int closest = -1;
	float closestDistance = -1.0f;

	for (uint i = 0; i < _compositors.size(); i++) {
		const LightCompositor &compositor = _compositors[i];
		float dx = cameraPosition[0] - compositor.position[0];
		float dy = cameraPosition[1] - compositor.position[1];
		float dz = cameraPosition[2] - compositor.position[2];
		float distance = (dz * dz) + ((dy * dy) + ((dx * dx) + 0.0f));

		if (closestDistance < 0.0f || closestDistance > distance) {
			closest = i;
			closestDistance = distance;
		}
	}

	if (closest < 0)
		return nullptr;

	return &_compositors[closest];
}

uint SetX::getShadowCasterCount() const {
	return _shadowCasters.size();
}

const ShadowCaster &SetX::getShadowCaster(uint index) const {
	return _shadowCasters[index];
}

uint SetX::getZShapeCount() const {
	return _zShapes.size();
}

const ZShape &SetX::getZShape(uint index) const {
	return _zShapes[index];
}

void SetX::resetKeylightStates(const char *setupName) {
	for (uint i = 0; i < _lights.size(); i++)
		_lights[i].resetKeylightState(setupName);
}

float SetX::wrapAngle(float angle) {
	if (angle > 360.0f && angle < 1.0e9f) {
		do
			angle += -360.0f;
		while (angle > 360.0f);
	}

	if (angle < 0.0f && angle > -1.0e9f) {
		do
			angle += 360.0f;
		while (angle < 0.0f);
	}

	return angle;
}

void SetX::update(float frameTimeFlex, Common::RandomSource &randomSource) {
	float step = frameTimeFlex * 10.0f;

	for (uint i = 0; i < _lights.size(); i++)
		_lights[i].updateAnimation(step, randomSource);

	for (uint i = 0; i < _shadowCasters.size(); i++) {
		ShadowCaster &caster = _shadowCasters[i];

		for (int axis = 0; axis < 3; axis++)
			caster.currentRotation[axis] = wrapAngle(((caster.rotationSpeed[axis] * 90.0f) * step) + caster.currentRotation[axis]);
	}
}

} // End of namespace Grim
