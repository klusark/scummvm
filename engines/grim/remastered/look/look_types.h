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

#ifndef GRIM_REMASTERED_LOOK_LOOK_TYPES_H
#define GRIM_REMASTERED_LOOK_LOOK_TYPES_H

#include "common/scummsys.h"

namespace Grim {

enum LookFormat {
	kLookFormatRgba8,
	kLookFormatRgb565,
	kLookFormatZ16,
	kLookFormatFloat1x32,
	kLookFormatFloat1x16,
	kLookFormatFloat2x16,
	kLookFormatFloat4x16,
	kLookFormatDxt1,
	kLookFormatDxt3,
	kLookFormatDxt5,
	kLookFormatDepth
};

enum LookBlend {
	kLookBlendPremultiplied = 0,
	kLookBlendAdd = 1,
	kLookBlendMin = 2,
	kLookBlendMax = 3
};

enum LookDepthFunc {
	kLookDepthAlways = 0,
	kLookDepthLess = 1,
	kLookDepthLessEqual = 2,
	kLookDepthEqual = 3,
	kLookDepthGreaterEqual = 4,
	kLookDepthGreater = 5
};

enum LookCull {
	kLookCullFront = -1,
	kLookCullNone = 0,
	kLookCullBack = 1
};

enum LookVertexFormat {
	kLookVertexTextured,
	kLookVertexVector3,
	kLookVertexQuad,
	kLookVertexText,
	kLookVertexShadow,
	kLookNumVertexFormats
};

enum LookWrap {
	kLookWrapRepeat = 0,
	kLookWrapMirror = 1,
	kLookWrapClamp = 2
};

enum LookConstKind {
	kLookConstFloat,
	kLookConstVec2,
	kLookConstVec4,
	kLookConstMat4,
	kLookConstVec4Array
};

enum LookStage {
	kLookStageVertex,
	kLookStagePixel
};

enum LookProgram {
	kLookProgramTexturedLit,
	kLookProgramUntexturedLit,
	kLookProgramSmoothLighting,
	kLookProgramCopy,
	kLookProgramSmoothLightingResolve,
	kLookProgramShadowMesh,
	kLookProgramShadow,
	kLookProgramZQuad,
	kLookProgramZQuad565,
	kLookProgramDeferredZShape,
	kLookProgramTexturedQuad,
	kLookProgramTexturedQuad565Linear,
	kLookProgramCharLightGradient,
	kLookProgramCharLightPoint,
	kLookProgramCharLightSpot,
	kLookProgramCharLightSpotShadow,
	kLookProgramCharLightSpotShadowEnv,
	kLookProgramCharLightDirectional,
	kLookProgramCharLightDirectionalShadow,
	kLookProgramCharLightDirectionalShadowEnv,
	kLookProgramCharLightViz,
	kLookProgramCharAo,
	kLookProgramCharResolve,
	kLookProgramCharResolveShadows,
	kLookProgramDeferredResolve,
	kLookProgramDeferredLightDirect,
	kLookProgramDeferredLightOmni,
	kLookProgramDeferredLightSpot,
	kLookProgramPostProcess,
	kLookProgramText,
	kLookProgramColorQuad,
	kLookProgramTexturedGammaQuad,
	kLookProgramForwardTextured,
	kLookProgramForwardTexturedSpot,
	kLookProgramForwardTexturedDirectional,
	kLookProgramForwardUntextured,
	kLookProgramForwardUntexturedSpot,
	kLookProgramForwardUntexturedDirectional,
	kLookProgramForwardTexturedLow,
	kLookProgramForwardTexturedSpotLow,
	kLookProgramForwardTexturedDirectionalLow,
	kLookProgramForwardUntexturedLow,
	kLookProgramForwardUntexturedSpotLow,
	kLookProgramForwardUntexturedDirectionalLow,
	kLookNumPrograms
};

enum {
	kLookMaxTextures = 8,
	kLookMaxTargets = 4,
	kLookMaxConstBytes = 848,
	kLookMaxLevels = 16
};

struct LookSampler {
	int wrap;
	int unused;
	int filter;
	int mip;
};

struct LookConstMember {
	const char *name;
	int kind;
	uint offset;
	uint count;
};

struct LookConstLayout {
	const char *blockName;
	uint size;
	uint numMembers;
	const LookConstMember *members;
};

struct LookConstBlock {
	const LookConstLayout *layout;
	byte data[kLookMaxConstBytes];

	LookConstBlock();
	void clear();
	void setLayout(const LookConstLayout *newLayout);
	void setFloats(uint offset, const float *values, uint count);
	void setFloat(uint offset, float value);
	float *getFloats(uint offset);
};

struct LookImageLevel {
	const byte *data;
	uint size;
	uint width;
	uint height;
};

struct LookImage {
	LookFormat format;
	uint width;
	uint height;
	uint numLevels;
	LookImageLevel levels[kLookMaxLevels];

	LookImage();
};

struct LookCaps {
	bool compressedTextures;
	uint maxTextureSize;
	uint maxDrawBuffers;

	LookCaps();
};

class RemasteredLookTarget;
class RemasteredLookTexture;
class RemasteredLookBuffer;

struct RemasteredLookTargetSetup {
	RemasteredLookTarget *color[kLookMaxTargets];
	RemasteredLookTarget *depth;

	RemasteredLookTargetSetup();
};

struct LookCall {
	int primitive;
	int program;
	LookConstBlock vertexConstants;
	LookConstBlock pixelConstants;
	bool hasVertexConstants;
	bool hasPixelConstants;
	int depthFunc;
	bool depthWrite;
	bool blendEnable;
	int blendMode;
	uint numTextures;
	RemasteredLookTexture *textures[kLookMaxTextures];
	uint numSamplers;
	LookSampler samplers[kLookMaxTextures];
	int count;
	uint first;
	RemasteredLookBuffer *vertices;
	RemasteredLookBuffer *indices;

	LookCall();
	void reset();
	void setVertexConstants(const LookConstBlock &block);
	void setPixelConstants(const LookConstBlock &block);
};

} // End of namespace Grim

#endif
