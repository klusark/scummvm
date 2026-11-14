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

#ifndef GRIM_REMASTERED_LOOK_LOOK_CONSTS_H
#define GRIM_REMASTERED_LOOK_LOOK_CONSTS_H

#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_types.h"

namespace Grim {

class RemasteredLookConsts {
public:
	enum Layout {
		kLayoutLitMeshVertex,
		kLayoutShadowMesh,
		kLayoutZShapeVertex,
		kLayoutSmoothLightingResolve,
		kLayoutCharLightVertex,
		kLayoutCharLightFragment,
		kLayoutCharAoVertex,
		kLayoutCharAoFragment,
		kLayoutCharResolve,
		kLayoutCharForwardResolve,
		kLayoutDeferredResolve,
		kLayoutDeferredDirect,
		kLayoutDeferredOmni,
		kLayoutDeferredSpot,
		kLayoutPostProcess,
		kLayoutLitMeshFragment,
		kLayoutForwardMeshVertex,
		kLayoutQuad,
		kLayoutColorQuad,
		kLayoutText,
		kNumLayouts
	};

	enum {
		kLitMeshViewDistance = 0,
		kLitMeshModel = 16,
		kLitMeshView = 80,
		kLitMeshSize = 144
	};

	enum {
		kShadowMeshLocalToScreen = 0,
		kShadowMeshLocalToWorld = 64,
		kShadowMeshSize = 128
	};

	enum {
		kProjectedViewDistance = 0,
		kProjectedLocalToCamera = 16,
		kProjectedSize = 80
	};

	enum {
		kSmoothTexelSize = 0,
		kSmoothSize = 8
	};

	enum {
		kLightWorldToLocal = 0,
		kLightInner = 64,
		kLightOuter = 80,
		kLightPosition = 96,
		kLightVertical = 112,
		kLightShading = 128,
		kLightWorldToShadowMap = 144,
		kLightShadow = 208,
		kLightCameraToWorld = 224,
		kLightViewDistance = 288,
		kLightSize = 304
	};

	enum {
		kAoViewDistance = 0,
		kAoWorldToLocal = 16,
		kAoCameraToWorld = 80,
		kAoSize = 144
	};

	enum {
		kResolveEnvShadow = 0,
		kResolveShadowBlack = 16,
		kResolveShadowWhite = 32,
		kResolveIris = 48,
		kResolveSize = 64
	};

	enum {
		kForwardResolveCameraToWorld = 64,
		kForwardResolveAoPosition = 128,
		kForwardResolveWorldToLocal = 144,
		kForwardResolveInner = 208,
		kForwardResolveOuter = 224,
		kForwardResolvePosition = 240,
		kForwardResolveVertical = 256,
		kForwardResolveShading = 272,
		kForwardResolveWorldToShadowMap = 288,
		kForwardResolveShadow = 352,
		kForwardResolveSize = 368
	};

	enum {
		kDeferredResolveViewDistance = 0,
		kDeferredResolveSize = 16
	};

	enum {
		kCutsceneSettings = 0,
		kCutsceneColor = 16,
		kCutsceneVector = 32,
		kCutsceneDirection = 48,
		kCutsceneDirectSize = 48,
		kCutsceneOmniSize = 48,
		kCutsceneSpotSize = 64
	};

	enum {
		kPostProcessPosOffset = 0,
		kPostProcessOpt1 = 16,
		kPostProcessOpt2 = 32,
		kPostProcessSharpness = 48,
		kPostProcessThreshold = 52,
		kPostProcessThresholdMin = 56,
		kPostProcessSize = 60
	};

	enum {
		kQuadMultiplyColor = 0,
		kQuadTexelDesaturateGamma = 16,
		kQuadColorKey = 32,
		kQuadSize = 48
	};

	enum {
		kColorQuadFill = 0,
		kColorQuadSize = 16
	};

	enum {
		kTextColor = 0,
		kTextShadowColor = 16,
		kTextOrigin = 32,
		kTextPixelSize = 40,
		kTextTexelSize = 48,
		kTextSize = 56
	};

	static const LookConstLayout *getLayout(int layout);
	static const LookConstLayout *getVertexLayout(int program);
	static const LookConstLayout *getPixelLayout(int program);
};

} // End of namespace Grim

#endif
