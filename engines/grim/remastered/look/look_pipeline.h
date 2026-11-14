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

#ifndef GRIM_REMASTERED_LOOK_LOOK_PIPELINE_H
#define GRIM_REMASTERED_LOOK_LOOK_PIPELINE_H

#include "common/array.h"
#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_device.h"
#include "engines/grim/remastered/look/look_scene.h"
#include "engines/grim/remastered/look/look_types.h"

namespace Grim {

struct LookBitmapDraw {
	RemasteredLookTexture *texture;
	bool isZ;
	bool is565;
	int destinationX;
	int destinationY;
	bool hasRect;
	int rectX;
	int rectY;
	int rectWidth;
	int rectHeight;
	int textureWidth;
	int textureHeight;
	int sourceWidth;
	int sourceHeight;
	bool keyFlag;
	uint key565;

	LookBitmapDraw();
};

struct LookSpriteDraw {
	RemasteredLookTexture *texture;
	float corner0[3];
	float corner1[3];
	int textureWidth;
	int textureHeight;

	LookSpriteDraw();
};

class RemasteredLookForward;

class RemasteredLookPipeline {
	friend class RemasteredLookForward;

public:
	enum {
		kShadowMapResolution = 1024
	};

	explicit RemasteredLookPipeline(RemasteredLookDevice *device);
	~RemasteredLookPipeline();

	bool open(int hostWidth, int hostHeight, float resolutionScaling);
	void close();
	bool isOpen() const { return _open; }
	bool reallocate(float resolutionScaling);

	void setScene(RemasteredLookScene *scene);
	RemasteredLookDevice *getDevice() const { return _remasteredLookDevice; }
	RemasteredLookForward *getForward() const { return _forward; }
	bool isForwardActive() const;

	void beginFrame(bool lightingSmoothing);
	void setModelCastsShadows(bool value) { _modelCastsShadows = value; }
	void postCameraChange();

	void drawSubmesh(RemasteredLookBuffer *vertices, RemasteredLookTexture *texture, const float *matrix12, float red, float green, float blue);
	void drawBitmap(const LookBitmapDraw &draw);
	void drawSprite(const LookSpriteDraw &draw);
	bool drawShadowGeometry(const float *vertices, uint count, uint colour565, bool softwareEnabled);

	void renderScene();
	void present(RemasteredLookTexture *software, float softwareAlpha, bool deferredEnabled, bool softwareEnabled,
			RemasteredLookTexture *overlay, bool frontBufferVisible, bool widescreen, float resolutionScaling);

	uint getTargetWidth() const { return _targetWidth; }
	uint getTargetHeight() const { return _targetHeight; }
	RemasteredLookTexture *getFinalTexture() const;

private:
	enum Pass {
		kPassShadows,
		kPassAlbedo,
		kPassSmoothLighting,
		kPassBitmapUnderlays,
		kPassBitmapOverlays,
		kPassVirtualDepth,
		kPassShadowMeshes,
		kPassZShapes,
		kPassAo,
		kPassLights,
		kNumPasses
	};

	enum Shape {
		kShapeBoxLight,
		kShapeBoxSolid,
		kShapeQuad,
		kNumShapes
	};

	void allocateTargets(float resolutionScaling);
	void freeTargets();
	void createShapes();
	void freeShapes();
	void freeFrameBuffers();
	RemasteredLookBuffer *createFrameQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1);
	RemasteredLookTexture *getTexture(RemasteredLookTarget *target) const;

	void setTargets(RemasteredLookTarget *first, RemasteredLookTarget *depth);
	void queue(int pass, const LookCall &call);
	void issue(int pass, bool clearQueue, const float *shadowMatrix);
	void issueCall(const LookCall &call);

	void initCall(LookCall &call, int program, int depthFunc, bool depthWrite) const;
	void setFullQuad(LookCall &call, bool flip) const;
	void fillLitMeshConstants(LookConstBlock &block, const float *matrix12, float red, float green, float blue) const;
	void setLightTextures(LookCall &call, bool withLight) const;
	void pointSamplers(LookCall &call, uint count) const;

	void flushScene();
	void smoothLightingPass();
	void virtualDepthPass();
	void renderZShapes();
	void rasterizeLights();
	void rasterizeCharacterLights();
	void rasterizeCutsceneLights();
	void shadowMapRender(const float *lightMatrix4, int type, float sizeX, float sizeY, float nearDistance, float farDistance,
			float *worldToShadowMap);
	void resolve();
	void postProcess();
	void presentSurface(RemasteredLookTexture *texture, float alpha, bool linear, bool widescreen, bool software);

	RemasteredLookDevice *_remasteredLookDevice;
	RemasteredLookScene *_scene;
	bool _open;
	int _hostWidth;
	int _hostHeight;
	uint _targetWidth;
	uint _targetHeight;
	float _resolutionScaling;

	RemasteredLookTarget *_albedo;
	RemasteredLookTarget *_position;
	RemasteredLookTarget *_normal;
	RemasteredLookTarget *_light;
	RemasteredLookTarget *_depth;
	RemasteredLookTarget *_resolved;
	RemasteredLookTarget *_virtualDepth;
	RemasteredLookTarget *_background;
	RemasteredLookTarget *_final;
	RemasteredLookTarget *_shadowDepth;
	RemasteredLookTarget *_shadowMap;

	RemasteredLookBuffer *_shapeVertices[kNumShapes];
	RemasteredLookBuffer *_shapeIndices[kNumShapes];
	RemasteredLookBuffer *_fullQuads[2];
	RemasteredLookBuffer *_planeVertices[6];
	RemasteredLookBuffer *_planeIndices[6];
	uint _planeCounts[6];

	Common::Array<LookCall> _passes[kNumPasses];
	Common::Array<RemasteredLookBuffer *> _frameBuffers;
	LookCamera _camera;

	bool _smoothing;
	bool _modelCastsShadows;
	int _framesSinceCameraChange;
	RemasteredLookTarget *_stageTarget;
	RemasteredLookForward *_forward;
	bool _stageForward;
};

} // End of namespace Grim

#endif
