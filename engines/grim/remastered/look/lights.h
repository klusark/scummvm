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

#ifndef GRIM_REMASTERED_LOOK_LIGHTS_H
#define GRIM_REMASTERED_LOOK_LIGHTS_H

#include "common/scummsys.h"
#include "common/str.h"

namespace Common {
class RandomSource;
}

namespace Grim {

struct LightColors {
	float innerColor[3];
	float outerColor[3];
	float intensity;

	LightColors();
};

struct CharacterLight {
	enum Type {
		kTypeGradient = 0,
		kTypePoint = 1,
		kTypeSpot = 2,
		kTypeDirectional = 3,
		kTypeDirectionalRectangle = 4,
		kNumTypes = 5
	};

	enum AnimationMode {
		kAnimationNone = 0,
		kAnimationSine = 1,
		kAnimationRandom = 2
	};

	enum ShadowMode {
		kShadowNone = 0,
		kShadowPlain = 1,
		kShadowEnvironment = 2
	};

	enum LodType {
		kLodAll = 0,
		kLodDeferred = 1,
		kLodForward = 2
	};

	Common::String name;
	byte type;
	float position[3];
	float rotation[3];
	float scale[3];
	float shadingBias;
	float shadingLimit;
	float innerRadius;
	float falloffPower;
	float verticalInnerRadius;
	float verticalFalloffPower;
	byte animationMode;
	float animationFrequency;
	byte shadowMode;
	byte shadowQuality;
	float shadowBias;
	float shadowSoftness;
	float shadowNear;
	float shadowMaxDistance;
	byte lodType;
	bool hasKeylightSetups;
	Common::String keylightSetups;
	LightColors colorsA;
	LightColors colorsB;

	LightColors fromColors;
	LightColors toColors;
	LightColors currentColors;
	float animationPeriod;
	float animationTime;
	int keylightState;

	CharacterLight();

	void resetAnimationState();
	void updateAnimation(float step, Common::RandomSource &randomSource);
	void resetKeylightState(const char *setupName);
	bool isVisible(int roomLodType) const;
	bool hasIntensity() const;
	void getInnerColor(float color[3]) const;
	void getOuterColor(float color[3]) const;
	float getTypeShadowBias() const;
	void getExtent(float extent[3]) const;
};

struct LightCompositor {
	Common::String name;
	float position[3];
	float shadowWhite[3];
	float shadowBlack[3];
	float minLuminance;
	float maxLuminance;
	float power;
	float aoStrength;

	LightCompositor();
};

} // End of namespace Grim

#endif
