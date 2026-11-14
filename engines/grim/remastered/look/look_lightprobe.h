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

#ifndef GRIM_REMASTERED_LOOK_LOOK_LIGHTPROBE_H
#define GRIM_REMASTERED_LOOK_LOOK_LIGHTPROBE_H

#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/remastered/look/look_scene.h"
#include "engines/grim/remastered/look/look_types.h"

namespace Grim {

class RemasteredLookLightprobe {
public:
	enum {
		kMaxLattices = 32,
		kNumProbes = 7,
		kNumFaces = 6,
		kNameSize = 32,
		kNoLight = 0xFFFF,
		kFlagSingleProbe = 1,
		kFlagNoShadowMeshes = 2,
		kKindNone = 0,
		kKindSpot = 1,
		kKindDirectional = 2
	};

	struct Lattice {
		char name[kNameSize];
		float size;
		float probe[kNumProbes][kNumFaces][3];
		float position[kNumProbes][3];
		float center[3];
		float step[3];
		float step10[3];
		uint32 flags;
		uint32 keylightIndex;
		uint32 keylightKind;
		bool inKeylight;
		bool constantsBuilt;
		LookConstBlock fragmentConstants;

		Lattice();
		void clear();
	};

	struct Info {
		uint32 keylightIndex;
		uint32 keylightType;
		uint32 latticesInKeylight;
		float worldToShadowMap[16];
		float bias;
		float softness;
		float quality;
		float enabled;

		Info();
	};

	RemasteredLookLightprobe();

	void setScene(RemasteredLookScene *scene) { _scene = scene; }

	void beginFrame();
	void onLightDataUpdate();
	void beginLattice(const char *name, const float *position, float radius, const char *overrideName);
	void endLattice();
	Lattice *getCurrentLattice();
	Info &getInfo();
	int getLattice(const char *name);
	int getKeylightAttributes(LookConstBlock &block, uint base);

private:
	void initialize();
	void clearLattices();
	void update(Lattice &lattice);
	void getCameraRelativeValues(const float *center, const float *step, float &distance, float &value);
	void buildShadowMatrix(const LookLightInfo &light, int type);

	void accumulateGradient(Lattice &lattice, int count, const float *worldToLocal, const float *inner, const float *outer,
			float innerRadius, float radialPower, float verticalInner, float verticalPower);
	void accumulatePoint(Lattice &lattice, int count, const float *worldToLocal, const float *lightPosition, const float *inner,
			const float *outer, float innerRadius, float radialPower, float shadingBias, float shadingLimit);
	void accumulateSpot(Lattice &lattice, int count, const float *worldToLocal, const float *inner, const float *outer,
			float innerRadius, float radialPower, float verticalInner, float verticalPower, float shadingBias, float shadingLimit);
	void accumulateDirectional(Lattice &lattice, int circle, int count, const float *worldToLocal, const float *inner,
			const float *outer, float innerRadius, float radialPower, float verticalInner, float verticalPower,
			float shadingBias, float shadingLimit);

	static float clampHelper(float value);
	static float clampInline(float value);
	static float inverseOf(float divisor);
	static float powerOf(float base, float exponent);
	static bool isFiniteValue(float value);
	static void transformVector(const float *matrix, const float *vector, float w, float *out);
	static float vectorDistanceSquared(const float *first, const float *second);
	static float dotSelf(const float *first, const float *second);
	static void directionBetween(const float *first, const float *second, float *out);
	static float faceComponent(int face, const float *vector);
	static void mixColors(const float *first, const float *second, float amount, float *out);
	static void addScaled(float *target, const float *source, float scale);

	RemasteredLookScene *_scene;
	bool _initialized;
	Lattice _lattices[kMaxLattices];
	int _currentLattice;
	Info _info;
	Common::String _previousSetup;
	const void *_previousCamera;
};

} // End of namespace Grim

#endif
