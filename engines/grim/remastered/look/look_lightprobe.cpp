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

#include "common/textconsole.h"

#include "engines/grim/remastered/look/look_consts.h"
#include "engines/grim/remastered/look/look_lightprobe.h"
#include "engines/grim/remastered/look/look_math.h"

#include <math.h>
#include <string.h>

namespace Grim {

static const float faceDirections[6][3] = {
	{ 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }
};

static const float directionalFactors[2] = { 1.0f, 1.0f };

RemasteredLookLightprobe::Lattice::Lattice() {
	clear();
}

void RemasteredLookLightprobe::Lattice::clear() {
	memset(name, 0, sizeof(name));
	size = 0.0f;
	memset(probe, 0, sizeof(probe));
	memset(position, 0, sizeof(position));
	memset(center, 0, sizeof(center));
	memset(step, 0, sizeof(step));
	memset(step10, 0, sizeof(step10));
	flags = 0;
	keylightIndex = 0;
	keylightKind = 0;
	inKeylight = false;
	constantsBuilt = false;
	fragmentConstants.clear();
}

RemasteredLookLightprobe::Info::Info() : keylightIndex(kNoLight), keylightType(kNoLight), latticesInKeylight(0), bias(0.0f),
		softness(0.0f), quality(0.0f), enabled(0.0f) {
	memset(worldToShadowMap, 0, sizeof(worldToShadowMap));
}

RemasteredLookLightprobe::RemasteredLookLightprobe() : _scene(nullptr), _initialized(false), _currentLattice(-1), _previousCamera(nullptr) {
	initialize();
}

void RemasteredLookLightprobe::initialize() {
	if (_initialized)
		return;

	_initialized = true;
	_info = Info();
	clearLattices();
}

void RemasteredLookLightprobe::clearLattices() {
	for (int i = 0; i < kMaxLattices; i++)
		_lattices[i].clear();
}

float RemasteredLookLightprobe::clampHelper(float value) {
	double result = 0.0;

	if (value >= 0.0f) {
		result = value;

		if (value > 1.0f)
			return 1.0f;
	}

	return (float)result;
}

float RemasteredLookLightprobe::clampInline(float value) {
	if (value < 0.0f)
		return 0.0f;

	if (1.0f < value)
		return 1.0f;

	return value;
}

float RemasteredLookLightprobe::inverseOf(float divisor) {
	if (divisor == 0.0f)
		return 0.0f;

	return (float)(1.0 / (double)divisor);
}

float RemasteredLookLightprobe::powerOf(float base, float exponent) {
	return (float)pow((double)base, (double)exponent);
}

bool RemasteredLookLightprobe::isFiniteValue(float value) {
	return value - value == 0.0f;
}

void RemasteredLookLightprobe::transformVector(const float *matrix, const float *vector, float w, float *out) {
	for (int i = 0; i < 3; i++) {
		double sum = (double)matrix[4 + i] * (double)vector[1] + (double)vector[0] * (double)matrix[i];

		sum = sum + (double)vector[2] * (double)matrix[8 + i];
		sum = sum + (double)matrix[12 + i] * (double)w;
		out[i] = (float)sum;
	}
}

float RemasteredLookLightprobe::vectorDistanceSquared(const float *first, const float *second) {
	float sum = 0.0f;

	for (int i = 0; i < 3; i++) {
		float difference = first[i] - second[i];

		sum = (float)((double)difference * (double)difference + (double)sum);
	}

	return sum;
}

float RemasteredLookLightprobe::dotSelf(const float *first, const float *second) {
	float sum = 0.0f;

	for (int i = 0; i < 3; i++)
		sum = (float)((double)second[i] * (double)first[i] + (double)sum);

	return sum;
}

void RemasteredLookLightprobe::directionBetween(const float *first, const float *second, float *out) {
	float difference[3];

	for (int i = 0; i < 3; i++)
		difference[i] = first[i] - second[i];

	RemasteredLookMath::normalize3(difference, out);
}

float RemasteredLookLightprobe::faceComponent(int face, const float *vector) {
	switch (face) {
	case 0:
		return vector[0];
	case 1:
		return -vector[0];
	case 2:
		return vector[1];
	case 3:
		return -vector[1];
	case 4:
		return vector[2];
	case 5:
		return -vector[2];
	default:
		return 0.0f;
	}
}

void RemasteredLookLightprobe::mixColors(const float *first, const float *second, float amount, float *out) {
	float complement = (float)(1.0 - (double)amount);

	for (int i = 0; i < 3; i++)
		out[i] = (float)((double)second[i] * (double)amount + (double)complement * (double)first[i]);
}

void RemasteredLookLightprobe::addScaled(float *target, const float *source, float scale) {
	for (int i = 0; i < 3; i++)
		target[i] = (float)((double)source[i] * (double)scale + (double)target[i]);
}

RemasteredLookLightprobe::Info &RemasteredLookLightprobe::getInfo() {
	initialize();

	return _info;
}

RemasteredLookLightprobe::Lattice *RemasteredLookLightprobe::getCurrentLattice() {
	initialize();

	if (_currentLattice < 0)
		return nullptr;

	return &_lattices[_currentLattice];
}

void RemasteredLookLightprobe::endLattice() {
	_currentLattice = -1;
}

void RemasteredLookLightprobe::onLightDataUpdate() {
	_previousSetup.clear();
	_previousCamera = nullptr;
}

int RemasteredLookLightprobe::getLattice(const char *name) {
	int firstFree = kMaxLattices;

	for (int i = 0; i < kMaxLattices; i++) {
		if (!strcmp(_lattices[i].name, name))
			return i;

		if (firstFree >= kMaxLattices && !_lattices[i].name[0])
			firstFree = i;
	}

	if (firstFree >= kMaxLattices)
		return -1;

	uint length = strlen(name);

	if (length >= 31)
		length = 31;

	memcpy(_lattices[firstFree].name, name, length);
	_lattices[firstFree].name[length] = '\0';
	_lattices[firstFree].size = 0.0f;
	_lattices[firstFree].flags = 0;

	return firstFree;
}

void RemasteredLookLightprobe::beginLattice(const char *name, const float *position, float radius, const char *overrideName) {
	const char *key = overrideName ? overrideName : name;
	int index = getLattice(key);

	if (index < 0)
		return;

	Lattice &lattice = _lattices[index];

	if (overrideName && lattice.size >= 1.0e-5f) {
		lattice.constantsBuilt = false;
	} else {
		if (position[0] == position[0] && position[1] == position[1] && position[2] == position[2]) {
			lattice.center[0] = position[0];
			lattice.center[1] = position[1];
			lattice.center[2] = position[2];
		}

		if (radius == radius && isFiniteValue(radius)) {
			if (lattice.size < 1.0e-4f)
				lattice.size = radius;

			lattice.step[0] = 0.25f * lattice.size;
			lattice.step[1] = 0.25f * lattice.size;
			lattice.step[2] = 0.5f * lattice.size;

			for (int i = 0; i < 3; i++)
				lattice.step10[i] = lattice.step[i] * 10.0f;
		}

		update(lattice);

		if (lattice.inKeylight)
			_info.latticesInKeylight++;
	}

	_currentLattice = index;
}

void RemasteredLookLightprobe::getCameraRelativeValues(const float *center, const float *step, float &distance, float &value) {
	LookCamera camera;

	_scene->getCamera(camera);

	float sum = vectorDistanceSquared(camera.invView + 9, center);

	distance = (float)sqrt((double)sum);

	float reciprocal = (float)(1.0 / ((double)distance + 1.0));
	float length = (float)sqrt((double)dotSelf(step, step));

	value = length * reciprocal;
}

void RemasteredLookLightprobe::update(Lattice &lattice) {
	memset(lattice.probe, 0, sizeof(lattice.probe));
	lattice.keylightIndex = kNoLight;
	lattice.keylightKind = 0;
	lattice.constantsBuilt = false;

	for (int k = 0; k < kNumProbes; k++) {
		lattice.position[k][0] = lattice.center[0];
		lattice.position[k][1] = lattice.center[1];
		lattice.position[k][2] = lattice.center[2];
	}

	lattice.position[1][0] = lattice.step[0] + lattice.position[1][0];
	lattice.position[2][0] = lattice.position[2][0] - lattice.step[0];
	lattice.position[3][1] = lattice.step[1] + lattice.position[3][1];
	lattice.position[4][1] = lattice.position[4][1] - lattice.step[1];
	lattice.position[5][2] = lattice.step[2] + lattice.position[5][2];
	lattice.position[6][2] = lattice.position[6][2] - lattice.step[2];

	float distance = 0.0f;
	float value = 0.0f;

	getCameraRelativeValues(lattice.center, lattice.step, distance, value);

	if (value >= 0.02f)
		lattice.flags &= ~1u;
	else
		lattice.flags |= 1u;

	if (value >= 0.0099999998f)
		lattice.flags &= ~2u;
	else
		lattice.flags |= 2u;

	if (lattice.flags && strstr(lattice.name, "manny"))
		lattice.flags &= 0xFFFFFFFCu;

	int count = (lattice.flags & kFlagSingleProbe) != 0 ? 1 : 7;

	lattice.inKeylight = false;

	uint lightCount = _scene->getCharacterLightCount();

	for (int type = 0; type < 5; type++) {
		for (uint i = 0; i < lightCount; i++) {
			LookLightInfo light;

			if (!_scene->getCharacterLightInfo(i, type, light))
				continue;

			float lightMatrix[16];
			float worldToLocal[16];

			RemasteredLookMath::expand34(light.matrix, lightMatrix);
			RemasteredLookMath::invert(lightMatrix, worldToLocal);

			float lightPosition[3] = { lightMatrix[12], lightMatrix[13], lightMatrix[14] };
			bool accumulate = true;

			if (light.keylight && !(lattice.keylightIndex < lightCount)) {
				lattice.keylightIndex = i;

				float lengths[3];

				for (int row = 0; row < 3; row++) {
					const float *axis = lightMatrix + row * 4;
					float sum = (float)(((double)axis[0] * (double)axis[0] + (double)axis[1] * (double)axis[1]) + (double)axis[2] * (double)axis[2]);

					lengths[row] = (float)sqrt((double)sum);
				}

				float longest = lengths[1];

				if ((double)lengths[1] <= (double)lengths[0])
					longest = lengths[0];

				if (!((double)lengths[2] <= (double)longest))
					longest = lengths[2];

				float radius = (float)((double)longest * (double)longest + (double)longest * (double)longest);

				radius = (float)sqrt((double)radius);
				radius = radius + lattice.size;

				float lightDistance = vectorDistanceSquared(lightPosition, lattice.center);

				if ((double)radius * (double)radius >= (double)lightDistance) {
					lattice.inKeylight = true;
					accumulate = false;
				}
			}

			if (!accumulate)
				continue;

			switch (type) {
			case 0:
				accumulateGradient(lattice, count, worldToLocal, light.inner, light.outer, light.innerRadius, light.falloffPower,
						light.verticalInner, light.verticalPower);
				break;
			case 1:
				accumulatePoint(lattice, count, worldToLocal, lightPosition, light.inner, light.outer, light.innerRadius,
						light.falloffPower, light.shadingBias, light.shadingLimit);
				break;
			case 2:
				accumulateSpot(lattice, count, worldToLocal, light.inner, light.outer, light.innerRadius, light.falloffPower,
						light.verticalInner, light.verticalPower, light.shadingBias, light.shadingLimit);
				break;
			case 3:
				accumulateDirectional(lattice, 0, count, worldToLocal, light.inner, light.outer, light.innerRadius, light.falloffPower,
						light.verticalInner, light.verticalPower, light.shadingBias, light.shadingLimit);
				break;
			case 4:
				accumulateDirectional(lattice, 1, count, worldToLocal, light.inner, light.outer, light.innerRadius, light.falloffPower,
						light.verticalInner, light.verticalPower, light.shadingBias, light.shadingLimit);
				break;
			default:
				break;
			}
		}
	}

	for (int p = 0; p < count; p++) {
		for (int f = 0; f < kNumFaces; f++) {
			for (int c = 0; c < 3; c++) {
				float scaled = (float)((double)lattice.probe[p][f][c] * 1.5);
				float remapped = (float)((((double)scaled - 0.5) * 1.2000000476837158) + 0.5);

				lattice.probe[p][f][c] = (0.0f <= remapped) ? remapped : 0.0f;
			}
		}
	}

	for (int p = count; p < kNumProbes; p++)
		memcpy(lattice.probe[p], lattice.probe[0], sizeof(lattice.probe[0]));
}

void RemasteredLookLightprobe::accumulateGradient(Lattice &lattice, int count, const float *worldToLocal, const float *inner,
		const float *outer, float innerRadius, float radialPower, float verticalInner, float verticalPower) {
	float inverseVertical = inverseOf((float)(1.0 - (double)verticalInner));
	float inverseRadial = inverseOf((float)(1.0 - (double)innerRadius));
	float amplitude = count <= 1 ? 0.5f : 1.0f;

	for (int p = 0; p < count; p++) {
		float local[3];

		transformVector(worldToLocal, lattice.position[p], 1.0f, local);

		float height = (float)(((double)local[2] - 1.0) * -0.5);

		height = clampInline(height);

		float vertical = (float)(((double)height - (double)verticalInner) * (double)inverseVertical);

		vertical = clampInline(vertical);
		vertical = powerOf(vertical, verticalPower);
		vertical = clampInline(vertical);

		float radius = (float)((double)local[1] * (double)local[1] + (double)local[0] * (double)local[0]);

		radius = (float)sqrt((double)radius);

		float radial = (float)(((double)radius - (double)innerRadius) * (double)inverseRadial);

		radial = clampInline(radial);
		radial = powerOf(radial, radialPower);
		radial = clampInline(radial);

		float attenuation = (float)((1.0 - (double)radial) * (1.0 - (double)vertical));
		float color[3];

		mixColors(inner, outer, attenuation, color);

		for (int f = 0; f < kNumFaces; f++)
			addScaled(lattice.probe[p][f], color, amplitude);
	}
}

void RemasteredLookLightprobe::accumulatePoint(Lattice &lattice, int count, const float *worldToLocal, const float *lightPosition,
		const float *inner, const float *outer, float innerRadius, float radialPower, float shadingBias, float shadingLimit) {
	float inverseRadial = inverseOf((float)(1.0 - (double)innerRadius));
	float inverseShading = inverseOf((float)((double)shadingLimit - (double)shadingBias));

	for (int p = 0; p < count; p++) {
		float local[3];

		transformVector(worldToLocal, lattice.position[p], 1.0f, local);

		if (!(local[0] >= -1.0f && local[0] <= 1.0f && local[1] >= -1.0f && local[1] <= 1.0f))
			continue;

		if (!(local[2] >= -1.0f && local[2] <= 1.0f))
			continue;

		float sum = (float)(((double)local[2] * (double)local[2]) + ((double)local[0] * (double)local[0] + (double)local[1] * (double)local[1]));
		float distance = (float)sqrt((double)sum);
		float t = (float)(((double)distance - (double)innerRadius) * (double)inverseRadial);

		t = clampHelper(t);

		float fall = powerOf(t, radialPower);

		fall = clampHelper(fall);

		float weight = (float)(1.0 - (double)fall);

		if (!(weight > 0.0f))
			continue;

		float direction[3];

		directionBetween(lightPosition, lattice.position[p], direction);

		for (int f = 0; f < kNumFaces; f++) {
			float s = (float)(((double)faceComponent(f, direction) - (double)shadingBias) * (double)inverseShading);

			s = clampInline(s);

			float k = (float)((double)s * (double)weight);

			if (!(k > 0.0f))
				continue;

			float color[3];

			mixColors(inner, outer, fall, color);
			addScaled(lattice.probe[p][f], color, k);
		}
	}
}

void RemasteredLookLightprobe::accumulateSpot(Lattice &lattice, int count, const float *worldToLocal, const float *inner,
		const float *outer, float innerRadius, float radialPower, float verticalInner, float verticalPower, float shadingBias,
		float shadingLimit) {
	float inverseVertical = inverseOf((float)(1.0 - (double)verticalInner));
	float inverseRadial = inverseOf((float)(1.0 - (double)innerRadius));
	float inverseShading = inverseOf((float)((double)shadingLimit - (double)shadingBias));

	for (int p = 0; p < count; p++) {
		float local[3];

		transformVector(worldToLocal, lattice.position[p], 1.0f, local);

		if (!(local[0] >= -1.0f))
			continue;

		if (local[0] > 1.0f || local[1] < -1.0f || local[1] > 1.0f || local[2] < -1.0f || local[2] > 1.0f)
			continue;

		double apexOffset = (double)local[2] - 1.0;
		float apexVector[3] = { local[0], local[1], (float)apexOffset };
		float spotDirection[3];

		RemasteredLookMath::normalize3(apexVector, spotDirection);

		float cone = (float)((-0.92387953251 - (double)spotDirection[2]) * 13.13707118432);

		cone = clampHelper(cone);

		float coneFactor = (float)(1.0 - (double)cone);
		float radialInput = (float)(((double)coneFactor - (double)innerRadius) * (double)inverseRadial);

		radialInput = clampHelper(radialInput);

		float radial = powerOf(radialInput, radialPower);

		radial = clampHelper(radial);

		double radialComplement = 1.0 - (double)radial;
		float height = (float)(apexOffset * -0.5);

		height = clampHelper(height);

		float verticalInput = (float)(((double)height - (double)verticalInner) * (double)inverseVertical);

		verticalInput = clampHelper(verticalInput);

		float vertical = powerOf(verticalInput, verticalPower);

		vertical = clampHelper(vertical);

		float attenuation = (float)((1.0 - (double)vertical) * radialComplement);

		if (!(attenuation > 0.0f))
			continue;

		for (int f = 0; f < kNumFaces; f++) {
			float transformed[3];
			float normal[3];

			transformVector(worldToLocal, faceDirections[f], 0.0f, transformed);
			RemasteredLookMath::normalize3(transformed, normal);

			float dot = (float)((((double)normal[1] * (double)spotDirection[1] + (double)spotDirection[0] * (double)normal[0])) +
					(double)normal[2] * (double)spotDirection[2]);
			float s = (float)(((double)(-dot) - (double)shadingBias) * (double)inverseShading);

			s = clampInline(s);

			float k = (float)((double)s * (double)attenuation);

			if (!(0.0f < k))
				continue;

			float color[3];

			mixColors(inner, outer, attenuation, color);
			addScaled(lattice.probe[p][f], color, k);
		}
	}
}

void RemasteredLookLightprobe::accumulateDirectional(Lattice &lattice, int circle, int count, const float *worldToLocal,
		const float *inner, const float *outer, float innerRadius, float radialPower, float verticalInner, float verticalPower,
		float shadingBias, float shadingLimit) {
	float inverseVertical = inverseOf((float)(1.0 - (double)verticalInner));
	float inverseRadial = inverseOf((float)(1.0 - (double)innerRadius));
	float inverseShading = inverseOf((float)((double)shadingLimit - (double)shadingBias));

	for (int p = 0; p < count; p++) {
		float local[3];

		transformVector(worldToLocal, lattice.position[p], 1.0f, local);

		if (!(local[0] >= -1.0f && local[0] <= 1.0f && local[1] >= -1.0f && local[1] <= 1.0f))
			continue;

		if (!(local[2] >= -1.0f && local[2] <= 1.0f))
			continue;

		float height = (float)(((double)local[2] - 1.0) * -0.5);
		float inside = height < 0.0f || height != height ? 0.0f : 1.0f;
		float heightClamped = clampHelper(height);
		float verticalInput = (float)(((double)heightClamped - (double)verticalInner) * (double)inverseVertical);

		verticalInput = clampHelper(verticalInput);

		float vertical = powerOf(verticalInput, verticalPower);

		vertical = clampHelper(vertical);

		float radius;

		if (circle) {
			float sum = (float)((double)local[1] * (double)local[1] + (double)local[0] * (double)local[0]);

			radius = (float)sqrt((double)sum);
		} else {
			float first = fabsf(local[1]);
			float second = fabsf(local[0]);

			radius = (first <= second) ? second : first;
		}

		float radialInput = (float)(((double)radius - (double)innerRadius) * (double)inverseRadial);

		radialInput = clampHelper(radialInput);

		float radial = powerOf(radialInput, radialPower);

		radial = clampHelper(radial);

		float attenuation = (float)((1.0 - (double)radial) * (1.0 - (double)vertical));

		if (!(attenuation > 0.0f))
			continue;

		for (int f = 0; f < kNumFaces; f++) {
			float transformed[3];
			float normal[3];

			transformVector(worldToLocal, faceDirections[f], 0.0f, transformed);
			RemasteredLookMath::normalize3(transformed, normal);

			float s = (float)(((double)normal[2] - (double)shadingBias) * (double)inverseShading);

			s = clampInline(s);

			float k = (float)((((double)s * (double)attenuation) * (double)inside) * (double)directionalFactors[circle]);

			if (!(k > 0.0f))
				continue;

			float color[3];

			mixColors(inner, outer, attenuation, color);
			addScaled(lattice.probe[p][f], color, k);
		}
	}
}

void RemasteredLookLightprobe::buildShadowMatrix(const LookLightInfo &light, int type) {
	float lightMatrix[16];
	float viewProjection[16];

	RemasteredLookMath::expand34(light.matrix, lightMatrix);
	RemasteredLookMath::buildShadowMatrices(lightMatrix, type, light.sizeX, light.sizeY, light.nearDistance, light.sizeZ, viewProjection,
			_info.worldToShadowMap);

	_info.bias = 0.0049999999f * light.shadowBias;
	_info.softness = 0.0009765625f * light.shadowSoftness;
	_info.quality = (float)light.shadowQuality;
	_info.enabled = 1.0f;
}

void RemasteredLookLightprobe::beginFrame() {
	initialize();

	Common::String setup;

	_scene->getCurrentSetupName(setup);

	const void *camera = _scene->getCameraIdentity();

	if (setup != _previousSetup || camera != _previousCamera) {
		clearLattices();
		_scene->resetCharacterLightKeylightStates();

		_info.keylightIndex = kNoLight;
		_info.keylightType = kNoLight;

		LookCamera lookCamera;

		_scene->getCamera(lookCamera);

		float cameraPosition[3] = { lookCamera.invView[9], lookCamera.invView[10], lookCamera.invView[11] };
		uint lightCount = _scene->getCharacterLightCount();
		int best = -1;
		int bestType = -1;
		double bestDistance = -1.0;

		for (int type = 2; type < 5; type++) {
			for (uint i = 0; i < lightCount; i++) {
				LookLightInfo light;

				if (!_scene->getCharacterLightInfo(i, type, light))
					continue;

				if (light.keylight) {
					best = (int)i;
					bestType = type;
					bestDistance = 0.0;
					break;
				}

				if (light.shadowMode > 0) {
					float distance = vectorDistanceSquared(cameraPosition, light.matrix + 9);
					double reach = (double)light.maxDistance * (double)light.maxDistance;

					if (reach >= (double)distance && (best == -1 || bestDistance > (double)distance)) {
						bestDistance = (double)distance;
						best = (int)i;
						bestType = type;
					}
				}
			}
		}

		if (best != -1) {
			_scene->setCharacterLightKeylightState((uint)best, 1);
			_info.keylightIndex = (uint32)best;
			_info.keylightType = (uint32)bestType;
		}

		memset(_info.worldToShadowMap, 0, sizeof(_info.worldToShadowMap));
		_info.bias = 0.0f;
		_info.softness = 0.0f;
		_info.quality = 0.0f;
		_info.enabled = 0.0f;

		LookLightInfo keylight;

		if (_info.keylightIndex < lightCount && _scene->getCharacterLightInfo(_info.keylightIndex, _info.keylightType, keylight))
			buildShadowMatrix(keylight, (int)_info.keylightType);
	}

	_previousSetup = setup;
	_previousCamera = camera;
	_info.latticesInKeylight = 0;
	_currentLattice = -1;
}

int RemasteredLookLightprobe::getKeylightAttributes(LookConstBlock &block, uint base) {
	initialize();

	uint lightCount = _scene->getCharacterLightCount();

	if (!(_info.keylightIndex < lightCount))
		return 0;

	LookLightInfo light;

	if (!_scene->getCharacterLightInfo(_info.keylightIndex, _info.keylightType, light))
		return 0;

	float inverseShading = inverseOf((float)((double)light.shadingLimit - (double)light.shadingBias));
	float inverseRadial = inverseOf((float)(1.0 - (double)light.innerRadius));
	float inverseVertical = inverseOf((float)(1.0 - (double)light.verticalInner));
	float user = (float)(_info.keylightType == 4);
	float lightMatrix[16];
	float worldToLocal[16];

	RemasteredLookMath::expand34(light.matrix, lightMatrix);
	RemasteredLookMath::invert(lightMatrix, worldToLocal);

	float inner[4] = { light.inner[0], light.inner[1], light.inner[2], light.innerRadius };
	float outer[4] = { light.outer[0], light.outer[1], light.outer[2], light.falloffPower };
	float position[4] = { light.position[0], light.position[1], light.position[2], inverseRadial };
	float vertical[4] = { light.verticalInner, light.verticalPower, inverseVertical, user };
	float shading[4] = { light.shadingBias, inverseShading, 0.0f, 0.0f };
	float shadow[4] = { _info.bias, _info.softness, _info.quality, _info.enabled };

	shadow[1] = (float)(1024.0 * 0.0009765625 * (double)shadow[1]);

	block.setFloats(base + RemasteredLookConsts::kLightWorldToLocal, worldToLocal, 16);
	block.setFloats(base + RemasteredLookConsts::kLightInner, inner, 4);
	block.setFloats(base + RemasteredLookConsts::kLightOuter, outer, 4);
	block.setFloats(base + RemasteredLookConsts::kLightPosition, position, 4);
	block.setFloats(base + RemasteredLookConsts::kLightVertical, vertical, 4);
	block.setFloats(base + RemasteredLookConsts::kLightShading, shading, 4);
	block.setFloats(base + RemasteredLookConsts::kLightWorldToShadowMap, _info.worldToShadowMap, 16);
	block.setFloats(base + RemasteredLookConsts::kLightShadow, shadow, 4);

	return _info.keylightType == 2 ? 1 : 2;
}

} // End of namespace Grim
