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

#ifndef GRIM_REMASTERED_LOOK_LOOK_SCENE_H
#define GRIM_REMASTERED_LOOK_LOOK_SCENE_H

#include "common/scummsys.h"
#include "common/str.h"

namespace Grim {

struct LookCamera {
	float view[12];
	float invView[12];
	float viewDistance;

	LookCamera();
};

struct LookLightInfo {
	float position[3];
	float matrix[12];
	float inner[3];
	float outer[3];
	float shadingBias;
	float shadingLimit;
	float innerRadius;
	float falloffPower;
	float verticalInner;
	float verticalPower;
	int shadowMode;
	int shadowQuality;
	float shadowBias;
	float shadowSoftness;
	float sizeX;
	float sizeY;
	float nearDistance;
	float sizeZ;
	float maxDistance;
	int keylight;

	LookLightInfo();
};

struct LookCutsceneLight {
	uint type;
	float position[3];
	float direction[3];
	float color[3];
	float settings[4];

	LookCutsceneLight();
};

struct LookCompositorInfo {
	float white[3];
	float black[3];
	float minLuminance;
	float maxLuminance;
	float power;
	float aoStrength;

	LookCompositorInfo();
};

class RemasteredLookScene {
public:
	virtual ~RemasteredLookScene() {}

	virtual uint getCharacterLightCount() const = 0;
	virtual uint getShadowCharacterLightCount() const = 0;
	virtual uint getCutsceneLightCount() const = 0;
	virtual bool getCharacterLightInfo(uint index, int type, LookLightInfo &info) const = 0;
	virtual bool getCutsceneLightInfo(uint index, LookCutsceneLight &light) const = 0;
	virtual uint getShadowCasterCount() const = 0;
	virtual bool getShadowCasterInfo(uint index, float matrix[12]) const = 0;
	virtual uint getZShapeCount() const = 0;
	virtual bool getZShapeInfo(uint index, int type, int drawPass, float matrix[12], int &tessellation) const = 0;
	virtual bool getCompositor(LookCompositorInfo &info) const = 0;
	virtual uint getActorAoCount() const = 0;
	virtual bool getActorAoInfo(uint index, float matrix[12]) const = 0;
	virtual uint getRenderThingCount() const = 0;
	virtual void getRenderThingSphere(uint index, float center[3], float &radius) const = 0;
	virtual void getIris(float &size, float &centerX, float &centerY) const = 0;
	virtual float getGamma() const = 0;
	virtual void getCamera(LookCamera &camera) const = 0;
	virtual uint getAdvancedLighting() const = 0;
	virtual void setLightLodType(int type) = 0;
	virtual void getCurrentSetupName(Common::String &name) const = 0;
	virtual const void *getCameraIdentity() const = 0;
	virtual void resetCharacterLightKeylightStates() = 0;
	virtual void setCharacterLightKeylightState(uint index, int value) = 0;
};

} // End of namespace Grim

#endif
