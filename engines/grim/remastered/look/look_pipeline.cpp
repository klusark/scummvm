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
#include "common/util.h"

#include "engines/grim/remastered/look/look_consts.h"
#include "engines/grim/remastered/look/look_forward.h"
#include "engines/grim/remastered/look/look_math.h"
#include "engines/grim/remastered/look/look_pipeline.h"

#include <math.h>
#include <string.h>

namespace Grim {

static const LookSampler pointSampler = { kLookWrapClamp, 0, 0, 0 };
static const LookSampler linearSampler = { kLookWrapClamp, 1, 1, 0 };
static const LookSampler meshSampler = { kLookWrapRepeat, 3, 3, 2 };

static const float zeroColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static const float blackColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static const float oneColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

static const float boxCorners[8][3] = {
	{ -1.0f, -1.0f, -1.0f }, { 1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, -1.0f }, { -1.0f, 1.0f, -1.0f },
	{ -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f }
};

static const uint16 boxIndicesLight[36] = {
	0, 1, 2, 2, 3, 0, 4, 7, 6, 6, 5, 4, 4, 0, 3, 3, 7, 4, 2, 1, 5, 5, 6, 2, 4, 5, 1, 1, 0, 4, 2, 6, 7, 7, 3, 2
};

static const uint16 boxIndicesSolid[36] = {
	0, 2, 1, 2, 0, 3, 4, 6, 7, 6, 4, 5, 4, 3, 0, 3, 4, 7, 2, 5, 1, 5, 2, 6, 4, 1, 5, 1, 4, 0, 2, 7, 6, 7, 2, 3
};

static const float quadCorners[6][3] = {
	{ -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 0.0f }
};

LookBitmapDraw::LookBitmapDraw() : texture(nullptr), isZ(false), is565(false), destinationX(0), destinationY(0), hasRect(false),
		rectX(0), rectY(0), rectWidth(0), rectHeight(0), textureWidth(0), textureHeight(0), sourceWidth(0), sourceHeight(0),
		keyFlag(false), key565(0) {
}

LookSpriteDraw::LookSpriteDraw() : texture(nullptr), textureWidth(0), textureHeight(0) {
	memset(corner0, 0, sizeof(corner0));
	memset(corner1, 0, sizeof(corner1));
}

RemasteredLookPipeline::RemasteredLookPipeline(RemasteredLookDevice *device) : _remasteredLookDevice(device), _scene(nullptr), _open(false), _hostWidth(0), _hostHeight(0),
		_targetWidth(0), _targetHeight(0), _resolutionScaling(2.0f), _albedo(nullptr), _position(nullptr), _normal(nullptr),
		_light(nullptr), _depth(nullptr), _resolved(nullptr), _virtualDepth(nullptr), _background(nullptr), _final(nullptr),
		_shadowDepth(nullptr), _shadowMap(nullptr), _smoothing(false), _modelCastsShadows(true),
		_framesSinceCameraChange(0), _stageTarget(nullptr), _forward(nullptr), _stageForward(false) {
	_forward = new RemasteredLookForward(this);

	for (int i = 0; i < kNumShapes; i++) {
		_shapeVertices[i] = nullptr;
		_shapeIndices[i] = nullptr;
	}

	for (int i = 0; i < 2; i++)
		_fullQuads[i] = nullptr;

	for (int i = 0; i < 6; i++) {
		_planeVertices[i] = nullptr;
		_planeIndices[i] = nullptr;
		_planeCounts[i] = 0;
	}
}

RemasteredLookPipeline::~RemasteredLookPipeline() {
	close();
	delete _forward;
}

bool RemasteredLookPipeline::open(int hostWidth, int hostHeight, float resolutionScaling) {
	if (_open)
		return true;

	_hostWidth = hostWidth;
	_hostHeight = hostHeight;
	allocateTargets(resolutionScaling);

	if (!_albedo || !_position || !_normal || !_light || !_depth || !_resolved || !_virtualDepth || !_background || !_final ||
			!_shadowDepth || !_shadowMap) {
		warning("RemasteredLookPipeline::open(): the render targets could not be created");
		freeTargets();
		return false;
	}

	createShapes();
	_open = true;

	return true;
}

void RemasteredLookPipeline::close() {
	if (!_open)
		return;

	freeFrameBuffers();
	freeShapes();
	freeTargets();

	for (int i = 0; i < kNumPasses; i++)
		_passes[i].clear();

	_open = false;
}

bool RemasteredLookPipeline::reallocate(float resolutionScaling) {
	if (!_open)
		return false;

	freeFrameBuffers();

	for (int i = 0; i < kNumPasses; i++)
		_passes[i].clear();

	freeTargets();
	allocateTargets(resolutionScaling);

	if (!_albedo || !_position || !_normal || !_light || !_depth || !_resolved || !_virtualDepth || !_background || !_final ||
			!_shadowDepth || !_shadowMap) {
		warning("RemasteredLookPipeline::reallocate(): the render targets could not be created");
		freeTargets();
		freeShapes();
		_open = false;
		return false;
	}

	return true;
}

void RemasteredLookPipeline::allocateTargets(float resolutionScaling) {
	_resolutionScaling = resolutionScaling;

	float ratioHeight = (float)_hostHeight / 480.0f;
	float ratioWidth = (float)_hostWidth / 640.0f;
	float ratio = ratioHeight < ratioWidth ? ratioHeight : ratioWidth;
	float scale = (float)(((double)resolutionScaling * 0.25 + 0.5) * (double)ratio);

	if (scale < 1.0f)
		scale = 1.0f;

	_targetWidth = (uint)ceilf(640.0f * scale);
	_targetHeight = (uint)ceilf(480.0f * scale);

	_position = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatFloat1x32);
	_normal = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatRgba8);
	_albedo = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatRgba8);
	_light = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatFloat4x16);
	_depth = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatDepth);
	_resolved = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatRgba8);
	_virtualDepth = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatFloat1x16);
	_background = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatRgba8);
	_final = _remasteredLookDevice->createTarget(_targetWidth, _targetHeight, kLookFormatRgba8);
	_shadowDepth = _remasteredLookDevice->createTarget(kShadowMapResolution, kShadowMapResolution, kLookFormatDepth);
	_shadowMap = _remasteredLookDevice->createTarget(kShadowMapResolution, kShadowMapResolution, kLookFormatFloat2x16);
}

void RemasteredLookPipeline::freeTargets() {
	RemasteredLookTarget **targets[11] = { &_albedo, &_position, &_normal, &_light, &_depth, &_resolved, &_virtualDepth, &_background,
			&_final, &_shadowDepth, &_shadowMap };

	for (int i = 0; i < 11; i++) {
		if (*targets[i])
			_remasteredLookDevice->destroyTarget(*targets[i]);
	}

	_stageTarget = nullptr;
}

void RemasteredLookPipeline::createShapes() {
	float vertices[8 * 3];

	for (int i = 0; i < 8; i++) {
		vertices[i * 3] = boxCorners[i][0];
		vertices[i * 3 + 1] = boxCorners[i][1];
		vertices[i * 3 + 2] = boxCorners[i][2];
	}

	_shapeVertices[kShapeBoxLight] = _remasteredLookDevice->createVertexBuffer(kLookVertexVector3, vertices, 8);
	_shapeIndices[kShapeBoxLight] = _remasteredLookDevice->createIndexBuffer(boxIndicesLight, 36);
	_shapeVertices[kShapeBoxSolid] = _remasteredLookDevice->createVertexBuffer(kLookVertexVector3, vertices, 8);
	_shapeIndices[kShapeBoxSolid] = _remasteredLookDevice->createIndexBuffer(boxIndicesSolid, 36);

	float quad[6 * 3];

	for (int i = 0; i < 6; i++) {
		quad[i * 3] = quadCorners[i][0];
		quad[i * 3 + 1] = quadCorners[i][1];
		quad[i * 3 + 2] = quadCorners[i][2];
	}

	_shapeVertices[kShapeQuad] = _remasteredLookDevice->createVertexBuffer(kLookVertexVector3, quad, 6);

	for (int flip = 0; flip < 2; flip++) {
		float v0 = flip ? 0.0f : 1.0f;
		float v1 = flip ? 1.0f : 0.0f;
		float data[6 * 5] = {
			-1.0f, 1.0f, 0.0f, 0.0f, v0,
			1.0f, 1.0f, 0.0f, 1.0f, v0,
			-1.0f, -1.0f, 0.0f, 0.0f, v1,
			1.0f, 1.0f, 0.0f, 1.0f, v0,
			1.0f, -1.0f, 0.0f, 1.0f, v1,
			-1.0f, -1.0f, 0.0f, 0.0f, v1
		};

		_fullQuads[flip] = _remasteredLookDevice->createVertexBuffer(kLookVertexQuad, data, 6);
	}

	static const int planeSizes[6] = { 5, 10, 20, 30, 40, 50 };

	for (int index = 0; index < 6; index++) {
		int n = planeSizes[index];
		uint numVertices = (uint)((n + 1) * (n + 1));
		Common::Array<float> data;
		Common::Array<uint16> indices;
		float step = (float)(2.0 / (double)n);
		float y = -1.0f;

		for (int row = 0; row <= n; row++) {
			float x = -1.0f;

			for (int column = 0; column <= n; column++) {
				data.push_back(x);
				data.push_back(y);
				data.push_back(0.0f);
				x = (float)((double)x + (double)step);
			}

			y = (float)((double)y + (double)step);
		}

		for (int row = 0; row < n; row++) {
			for (int column = 0; column < n; column++) {
				uint16 base = (uint16)(row * (n + 1) + column);

				indices.push_back(base);
				indices.push_back((uint16)(base + 1));
				indices.push_back((uint16)(base + n + 2));
				indices.push_back((uint16)(base + n + 2));
				indices.push_back((uint16)(base + n + 1));
				indices.push_back(base);
			}
		}

		_planeVertices[index] = _remasteredLookDevice->createVertexBuffer(kLookVertexVector3, data.data(), numVertices);
		_planeIndices[index] = _remasteredLookDevice->createIndexBuffer(indices.data(), indices.size());
		_planeCounts[index] = indices.size();
	}
}

void RemasteredLookPipeline::freeShapes() {
	for (int i = 0; i < kNumShapes; i++) {
		if (_shapeVertices[i])
			_remasteredLookDevice->destroyBuffer(_shapeVertices[i]);

		if (_shapeIndices[i])
			_remasteredLookDevice->destroyBuffer(_shapeIndices[i]);
	}

	for (int i = 0; i < 2; i++) {
		if (_fullQuads[i])
			_remasteredLookDevice->destroyBuffer(_fullQuads[i]);
	}

	for (int i = 0; i < 6; i++) {
		if (_planeVertices[i])
			_remasteredLookDevice->destroyBuffer(_planeVertices[i]);

		if (_planeIndices[i])
			_remasteredLookDevice->destroyBuffer(_planeIndices[i]);
	}
}

void RemasteredLookPipeline::freeFrameBuffers() {
	for (uint i = 0; i < _frameBuffers.size(); i++)
		_remasteredLookDevice->destroyBuffer(_frameBuffers[i]);

	_frameBuffers.clear();
}

RemasteredLookBuffer *RemasteredLookPipeline::createFrameQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1) {
	float data[6 * 5] = {
		x, y + height, 0.0f, u0, v0,
		x + width, y + height, 0.0f, u1, v0,
		x, y, 0.0f, u0, v1,
		x + width, y + height, 0.0f, u1, v0,
		x + width, y, 0.0f, u1, v1,
		x, y, 0.0f, u0, v1
	};

	RemasteredLookBuffer *buffer = _remasteredLookDevice->createVertexBuffer(kLookVertexQuad, data, 6);

	_frameBuffers.push_back(buffer);

	return buffer;
}

RemasteredLookTexture *RemasteredLookPipeline::getTexture(RemasteredLookTarget *target) const {
	return target ? _remasteredLookDevice->getTargetTexture(target) : nullptr;
}

RemasteredLookTexture *RemasteredLookPipeline::getFinalTexture() const {
	return getTexture(_final);
}

void RemasteredLookPipeline::setTargets(RemasteredLookTarget *first, RemasteredLookTarget *depth) {
	RemasteredLookTargetSetup setup;

	setup.color[0] = first;
	setup.depth = depth;
	_remasteredLookDevice->setTargets(setup);
}

void RemasteredLookPipeline::queue(int pass, const LookCall &call) {
	_passes[pass].push_back(call);
}

void RemasteredLookPipeline::issueCall(const LookCall &call) {
	_remasteredLookDevice->drawCall(call);
}

void RemasteredLookPipeline::issue(int pass, bool clearQueue, const float *shadowMatrix) {
	Common::Array<LookCall> &calls = _passes[pass];

	for (uint i = 0; i < calls.size(); i++) {
		if (shadowMatrix) {
			float *localToScreen = calls[i].vertexConstants.getFloats(RemasteredLookConsts::kShadowMeshLocalToScreen);
			float *localToWorld = calls[i].vertexConstants.getFloats(RemasteredLookConsts::kShadowMeshLocalToWorld);

			RemasteredLookMath::multiply(localToWorld, shadowMatrix, localToScreen);
		}

		issueCall(calls[i]);
	}

	if (clearQueue)
		calls.clear();
}

void RemasteredLookPipeline::initCall(LookCall &call, int program, int depthFunc, bool depthWrite) const {
	call.reset();
	call.program = program;
	call.depthFunc = depthFunc;
	call.depthWrite = depthWrite;
}

void RemasteredLookPipeline::setFullQuad(LookCall &call, bool flip) const {
	call.vertices = _fullQuads[flip ? 1 : 0];
	call.indices = nullptr;
	call.count = -2;
	call.first = 0;
}

void RemasteredLookPipeline::fillLitMeshConstants(LookConstBlock &block, const float *matrix12, float red, float green, float blue) const {
	float view4[16];

	block.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutLitMeshVertex));
	block.setFloat(RemasteredLookConsts::kLitMeshViewDistance, _camera.viewDistance);
	block.setFloat(RemasteredLookConsts::kLitMeshViewDistance + 4, red);
	block.setFloat(RemasteredLookConsts::kLitMeshViewDistance + 8, green);
	block.setFloat(RemasteredLookConsts::kLitMeshViewDistance + 12, blue);

	float model4[16];

	RemasteredLookMath::expand34(matrix12, model4);
	RemasteredLookMath::expand34(_camera.view, view4);
	block.setFloats(RemasteredLookConsts::kLitMeshModel, model4, 16);
	block.setFloats(RemasteredLookConsts::kLitMeshView, view4, 16);
}

void RemasteredLookPipeline::pointSamplers(LookCall &call, uint count) const {
	call.numSamplers = count;

	for (uint i = 0; i < count; i++)
		call.samplers[i] = pointSampler;
}

void RemasteredLookPipeline::setLightTextures(LookCall &call, bool withLight) const {
	call.numTextures = 7;
	call.textures[0] = getTexture(_albedo);
	call.textures[1] = getTexture(_position);
	call.textures[2] = getTexture(_normal);
	call.textures[3] = withLight ? getTexture(_light) : nullptr;
	call.textures[4] = getTexture(_virtualDepth);
	call.textures[5] = getTexture(_shadowMap);
	call.textures[6] = getTexture(_background);
	pointSamplers(call, 7);
	call.samplers[5] = linearSampler;
}

void RemasteredLookPipeline::setScene(RemasteredLookScene *scene) {
	_scene = scene;
	_forward->setScene(scene);
}

bool RemasteredLookPipeline::isForwardActive() const {
	return _forward->isActive();
}

void RemasteredLookPipeline::beginFrame(bool lightingSmoothing) {
	freeFrameBuffers();

	bool forwardStage = _forward->isActive();

	_forward->beginFrame();

	_passes[kPassShadows].clear();
	_passes[kPassAlbedo].clear();
	_passes[kPassBitmapUnderlays].clear();
	_passes[kPassBitmapOverlays].clear();
	_passes[kPassVirtualDepth].clear();
	_passes[kPassSmoothLighting].clear();
	_passes[kPassShadowMeshes].clear();

	if (!forwardStage)
		_smoothing = lightingSmoothing;

	_modelCastsShadows = true;
	_stageForward = forwardStage;

	if (_framesSinceCameraChange < 30)
		_framesSinceCameraChange++;

	if (_scene && _scene->getCharacterLightCount() != 0)
		_stageTarget = _background;
	else if (_scene && _scene->getCutsceneLightCount() != 0)
		_stageTarget = forwardStage ? _final : _resolved;
	else
		_stageTarget = _background;
}

void RemasteredLookPipeline::postCameraChange() {
	_forward->postCameraChange();
}

void RemasteredLookPipeline::drawSubmesh(RemasteredLookBuffer *vertices, RemasteredLookTexture *texture, const float *matrix12, float red, float green, float blue) {
	if (!_open || !vertices)
		return;

	if (_forward->isActive()) {
		_forward->drawSubmesh(vertices, texture, matrix12, red, green, blue);
		return;
	}

	_scene->getCamera(_camera);

	LookCall call;

	initCall(call, texture ? kLookProgramTexturedLit : kLookProgramUntexturedLit, kLookDepthGreater, true);
	fillLitMeshConstants(call.vertexConstants, matrix12, red, green, blue);
	call.hasVertexConstants = true;
	call.pixelConstants = call.vertexConstants;
	call.hasPixelConstants = true;
	call.numTextures = texture ? 1 : 0;
	call.textures[0] = texture;
	call.numSamplers = 1;
	call.samplers[0] = meshSampler;
	call.vertices = vertices;
	queue(kPassAlbedo, call);

	if (_smoothing && texture) {
		LookCall smooth = call;

		smooth.program = kLookProgramSmoothLighting;
		smooth.depthFunc = kLookDepthLess;
		smooth.depthWrite = false;
		smooth.blendEnable = true;
		smooth.blendMode = kLookBlendAdd;
		smooth.numTextures = 4;
		smooth.textures[1] = getTexture(_albedo);
		smooth.textures[2] = getTexture(_position);
		smooth.textures[3] = getTexture(_normal);
		smooth.numSamplers = 4;
		smooth.samplers[1] = pointSampler;
		smooth.samplers[2] = pointSampler;
		smooth.samplers[3] = pointSampler;
		queue(kPassSmoothLighting, smooth);
	}

	if (_modelCastsShadows && _scene->getShadowCharacterLightCount()) {
		LookCall shadow;

		initCall(shadow, kLookProgramShadowMesh, kLookDepthLess, true);
		shadow.vertexConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutShadowMesh));

		float model4[16];

		RemasteredLookMath::expand34(matrix12, model4);
		shadow.vertexConstants.setFloats(RemasteredLookConsts::kShadowMeshLocalToWorld, model4, 16);
		shadow.hasVertexConstants = true;
		shadow.vertices = vertices;
		queue(kPassShadowMeshes, shadow);
	}
}

void RemasteredLookPipeline::drawBitmap(const LookBitmapDraw &draw) {
	if (!_open)
		return;

	if (!draw.texture) {
		warning("RemasteredLookPipeline::drawBitmap(): no texture for a %s bitmap %dx%d at %d,%d (565 %d, key %d/%u, rect %d %d,%d %dx%d)",
				draw.isZ ? "Z" : "colour", draw.textureWidth, draw.textureHeight, draw.destinationX, draw.destinationY, draw.is565, draw.keyFlag,
				draw.key565, draw.hasRect, draw.rectX, draw.rectY, draw.rectWidth, draw.rectHeight);
		return;
	}

	float targetWidth = (float)_targetWidth;
	float targetHeight = (float)_targetHeight;
	float ratioX = targetWidth / 640.0f;
	float ratioY = targetHeight / 480.0f;
	float scale = ratioX < ratioY ? ratioX : ratioY;
	float offsetX = (float)(((double)targetWidth - 640.0 * (double)scale) * 0.5);
	float offsetY = (float)(((double)targetHeight - 480.0 * (double)scale) * 0.5);
	float pixelX = (float)((double)draw.destinationX * (double)scale + (double)offsetX);
	float pixelY = (float)((double)draw.destinationY * (double)scale + (double)offsetY);
	float rectX = draw.hasRect ? (float)draw.rectX : 0.0f;
	float rectY = draw.hasRect ? (float)draw.rectY : 0.0f;
	float rectWidth = draw.hasRect ? (float)draw.rectWidth : (float)draw.textureWidth;
	float rectHeight = draw.hasRect ? (float)draw.rectHeight : (float)draw.textureHeight;
	float shrink = (draw.sourceWidth == 226 || draw.sourceHeight == 214) ? -1.0f : 0.0f;
	float width = (float)((double)scale * ((double)rectWidth + (double)shrink));
	float height = (float)((double)scale * ((double)rectHeight + (double)shrink));
	float scaleX = (float)(2.0 / (double)targetWidth);
	float scaleY = (float)(-2.0 / (double)targetHeight);
	float ndcX = (float)((double)pixelX * (double)scaleX - 1.0);
	float ndcY = (float)((double)pixelY * (double)scaleY + 1.0);
	float ndcWidth = width * scaleX;
	float ndcHeight = height * scaleY;
	float inverseWidth = (float)(1.0 / (double)draw.textureWidth);
	float inverseHeight = (float)(1.0 / (double)draw.textureHeight);
	float u0 = rectX * inverseWidth;
	float u1 = (rectX + rectWidth) * inverseWidth;
	float v0 = rectY * inverseHeight;
	float v1 = (rectY + rectHeight) * inverseHeight;

	RemasteredLookBuffer *quad = createFrameQuad(ndcX, ndcY, ndcWidth, ndcHeight, u0, v1, u1, v0);
	LookCall call;
	int program = draw.isZ ? kLookProgramZQuad565 : (draw.is565 ? kLookProgramTexturedQuad565Linear : kLookProgramTexturedQuad);

	initCall(call, program, kLookDepthAlways, false);
	call.blendEnable = !draw.isZ;
	call.blendMode = kLookBlendPremultiplied;
	call.numTextures = 1;
	call.textures[0] = draw.texture;
	call.numSamplers = 1;
	call.samplers[0].wrap = kLookWrapClamp;
	call.samplers[0].filter = (program == kLookProgramTexturedQuad565Linear) ? 0 : 1;
	call.samplers[0].unused = call.samplers[0].filter;
	call.samplers[0].mip = 0;
	call.vertices = quad;

	LookConstBlock &block = call.pixelConstants;

	block.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutQuad));
	block.setFloats(RemasteredLookConsts::kQuadMultiplyColor, oneColor, 4);

	float texel[4] = { inverseWidth, inverseHeight, 0.0f, _scene->getGamma() };

	block.setFloats(RemasteredLookConsts::kQuadTexelDesaturateGamma, texel, 4);

	float key[4] = { 2.0f, 2.0f, 2.0f, 1.0f };

	if (draw.keyFlag) {
		key[0] = (float)((draw.key565 >> 8) & 0xF8) / 255.0f;
		key[1] = (float)((draw.key565 >> 3) & 0xFC) / 255.0f;
		key[2] = (float)(8 * (draw.key565 & 0x1F)) / 255.0f;
		key[3] = 0.0f;
	}

	block.setFloats(RemasteredLookConsts::kQuadColorKey, key, 4);
	call.hasPixelConstants = true;
	call.vertexConstants = block;
	call.hasVertexConstants = true;

	if (draw.isZ)
		queue(kPassVirtualDepth, call);
	else if (_passes[kPassAlbedo].empty())
		queue(kPassBitmapUnderlays, call);
	else
		queue(kPassBitmapOverlays, call);
}

void RemasteredLookPipeline::drawSprite(const LookSpriteDraw &draw) {
	if (!_open)
		return;

	if (!draw.texture) {
		warning("RemasteredLookPipeline::drawSprite(): no texture for a sprite %dx%d", draw.textureWidth, draw.textureHeight);
		return;
	}

	float targetWidth = (float)_targetWidth;
	float targetHeight = (float)_targetHeight;
	float ratioX = targetWidth / 640.0f;
	float ratioY = targetHeight / 480.0f;
	float scale = ratioX < ratioY ? ratioX : ratioY;
	float pixelX = draw.corner0[0] * scale;
	float pixelY = draw.corner0[1] * scale;
	float width = scale * (draw.corner1[0] - draw.corner0[0]);
	float height = scale * (draw.corner1[1] - draw.corner0[1]);
	float scaleX = (float)(2.0 / (double)targetWidth);
	float scaleY = (float)(-2.0 / (double)targetHeight);
	float ndcX = (float)((double)pixelX * (double)scaleX - 1.0);
	float ndcY = (float)((double)pixelY * (double)scaleY + 1.0);
	float ndcWidth = width * scaleX;
	float ndcHeight = height * scaleY;
	float inverseWidth = (float)(1.0 / (double)draw.textureWidth);
	float inverseHeight = (float)(1.0 / (double)draw.textureHeight);
	float depthValue = (float)(1.0 / (((double)draw.corner0[2] + (double)draw.corner1[2]) * 0.5 + 1.0));
	float data[6 * 5] = {
		ndcX, ndcY + ndcHeight, depthValue, 0.0f, 1.0f,
		ndcX + ndcWidth, ndcY + ndcHeight, depthValue, 1.0f, 1.0f,
		ndcX, ndcY, depthValue, 0.0f, 0.0f,
		ndcX + ndcWidth, ndcY + ndcHeight, depthValue, 1.0f, 1.0f,
		ndcX + ndcWidth, ndcY, depthValue, 1.0f, 0.0f,
		ndcX, ndcY, depthValue, 0.0f, 0.0f
	};

	RemasteredLookBuffer *quad = _remasteredLookDevice->createVertexBuffer(kLookVertexQuad, data, 6);

	_frameBuffers.push_back(quad);

	LookCall call;

	initCall(call, kLookProgramTexturedQuad, kLookDepthGreater, false);
	call.blendEnable = true;
	call.numTextures = 1;
	call.textures[0] = draw.texture;
	call.numSamplers = 1;
	call.samplers[0] = pointSampler;
	call.vertices = quad;

	LookConstBlock &block = call.pixelConstants;

	block.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutQuad));
	block.setFloats(RemasteredLookConsts::kQuadMultiplyColor, oneColor, 4);

	float texel[4] = { inverseWidth, inverseHeight, 0.0f, _scene->getGamma() };

	block.setFloats(RemasteredLookConsts::kQuadTexelDesaturateGamma, texel, 4);
	call.hasPixelConstants = true;
	call.vertexConstants = block;
	call.hasVertexConstants = true;
	queue(kPassBitmapOverlays, call);
}

bool RemasteredLookPipeline::drawShadowGeometry(const float *vertices, uint count, uint colour565, bool softwareEnabled) {
	(void)colour565;

	if (!_open || count < 3 || _scene->getCharacterLightCount() != 0)
		return false;

	RemasteredLookBuffer *buffer = _remasteredLookDevice->createVertexBuffer(kLookVertexShadow, vertices, count);

	_frameBuffers.push_back(buffer);

	LookCall call;

	initCall(call, kLookProgramShadow, kLookDepthGreater, true);
	call.primitive = 5;
	call.vertices = buffer;
	queue(kPassShadows, call);

	return !softwareEnabled;
}

void RemasteredLookPipeline::renderScene() {
	if (!_open || !_stageTarget)
		return;

	_remasteredLookDevice->beginFrame();
	_remasteredLookDevice->setCull(kLookCullNone);
	_remasteredLookDevice->setColorMask(true, true, true, true);

	setTargets(_stageTarget, nullptr);
	_remasteredLookDevice->setDepth(kLookDepthAlways, _stageForward);
	_remasteredLookDevice->clear(zeroColor, 0.0f);

	if (_forward->isActive()) {
		_forward->renderScene();
		return;
	}

	flushScene();
	virtualDepthPass();
	rasterizeLights();
	resolve();
	postProcess();

	setTargets(_final, _depth);
	issue(kPassBitmapOverlays, true, nullptr);
}

void RemasteredLookPipeline::flushScene() {
	setTargets(_stageTarget, nullptr);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	issue(kPassBitmapUnderlays, true, nullptr);

	RemasteredLookTargetSetup setup;

	setup.color[0] = _albedo;
	setup.color[1] = _position;
	setup.color[2] = _normal;
	setup.depth = _depth;
	_remasteredLookDevice->setTargets(setup);
	_remasteredLookDevice->setDepth(kLookDepthAlways, true);
	_remasteredLookDevice->clear(zeroColor, 0.0f);
	issue(kPassAlbedo, true, nullptr);

	if (_smoothing)
		smoothLightingPass();
}

void RemasteredLookPipeline::smoothLightingPass() {
	LookCall call;

	setTargets(_resolved, nullptr);
	_remasteredLookDevice->setCull(kLookCullNone);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	_remasteredLookDevice->clear(blackColor, 0.0f);

	initCall(call, kLookProgramCopy, kLookDepthAlways, false);
	call.numTextures = 1;
	call.textures[0] = getTexture(_normal);
	pointSamplers(call, 1);
	setFullQuad(call, false);
	issueCall(call);

	setTargets(_light, _depth);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	_remasteredLookDevice->clear(zeroColor, 0.0f);
	_remasteredLookDevice->setCull(kLookCullBack);
	issue(kPassSmoothLighting, true, nullptr);

	setTargets(_normal, _depth);
	_remasteredLookDevice->setCull(kLookCullNone);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);

	initCall(call, kLookProgramSmoothLightingResolve, kLookDepthAlways, false);
	call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutSmoothLightingResolve));

	float texel[2] = { (float)(1.0 / (double)_targetWidth), (float)(1.0 / (double)_targetHeight) };

	call.pixelConstants.setFloats(RemasteredLookConsts::kSmoothTexelSize, texel, 2);
	call.hasPixelConstants = true;
	call.vertexConstants = call.pixelConstants;
	call.hasVertexConstants = true;
	call.numTextures = 3;
	call.textures[0] = getTexture(_light);
	call.textures[1] = getTexture(_albedo);
	call.textures[2] = getTexture(_resolved);
	pointSamplers(call, 3);
	setFullQuad(call, false);
	issueCall(call);
}

void RemasteredLookPipeline::virtualDepthPass() {
	setTargets(_virtualDepth, nullptr);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	_remasteredLookDevice->clear(zeroColor, 0.0f);
	issue(kPassVirtualDepth, true, nullptr);
	renderZShapes();
}

void RemasteredLookPipeline::renderZShapes() {
	_scene->getCamera(_camera);
	_remasteredLookDevice->setCull(kLookCullBack);

	float view4[16];

	RemasteredLookMath::expand34(_camera.view, view4);

	for (int pass = 1; pass >= 0; pass--) {
		for (int type = 0; type < 2; type++) {
			for (uint i = 0; i < _scene->getZShapeCount(); i++) {
				float matrix[12];
				int tessellation = 0;

				if (!_scene->getZShapeInfo(i, type, pass, matrix, tessellation))
					continue;

				LookCall call;

				initCall(call, kLookProgramDeferredZShape, kLookDepthAlways, false);
				call.blendEnable = pass == 0;
				call.blendMode = pass == 0 ? kLookBlendMax : kLookBlendPremultiplied;
				call.vertexConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutZShapeVertex));
				call.vertexConstants.setFloat(RemasteredLookConsts::kProjectedViewDistance, _camera.viewDistance);

				float shape4[16];
				float localToCamera[16];

				RemasteredLookMath::expand34(matrix, shape4);
				RemasteredLookMath::multiply(shape4, view4, localToCamera);
				call.vertexConstants.setFloats(RemasteredLookConsts::kProjectedLocalToCamera, localToCamera, 16);
				call.hasVertexConstants = true;

				if (type == 0) {
					call.vertices = _shapeVertices[kShapeBoxSolid];
					call.indices = _shapeIndices[kShapeBoxSolid];
					call.count = 36;
				} else {
					int detail = tessellation > 5 ? 5 : tessellation;
					int n = detail <= 0 ? 5 : 10 * detail;
					int index = n == 5 ? 0 : n / 10;

					call.vertices = _planeVertices[index];
					call.indices = _planeIndices[index];
					call.count = (int)_planeCounts[index];
				}

				queue(kPassZShapes, call);
			}
		}

		issue(kPassZShapes, true, nullptr);
	}

	_remasteredLookDevice->setCull(kLookCullNone);
}

void RemasteredLookPipeline::rasterizeLights() {
	if (_scene->getCharacterLightCount() != 0) {
		rasterizeCharacterLights();
		return;
	}

	setTargets(_light, nullptr);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	_remasteredLookDevice->clear(zeroColor, 0.0f);

	if (_scene->getCutsceneLightCount() != 0)
		rasterizeCutsceneLights();
}

void RemasteredLookPipeline::shadowMapRender(const float *lightMatrix4, int type, float sizeX, float sizeY, float nearDistance, float farDistance,
		float *worldToShadowMap) {
	float viewProjection[16];

	RemasteredLookMath::buildShadowMatrices(lightMatrix4, type, sizeX, sizeY, nearDistance, farDistance, viewProjection, worldToShadowMap);

	setTargets(_shadowMap, _shadowDepth);
	_remasteredLookDevice->setDepth(kLookDepthAlways, true);
	_remasteredLookDevice->clear(oneColor, 1.0f);
	_remasteredLookDevice->setCull(kLookCullBack);
	issue(kPassShadowMeshes, false, viewProjection);

	uint casters = _scene->getShadowCasterCount();

	if (casters) {
		_remasteredLookDevice->setCull(kLookCullFront);
		_remasteredLookDevice->setColorMask(true, false, false, false);

		for (uint i = 0; i < casters; i++) {
			float matrix[12];

			if (!_scene->getShadowCasterInfo(i, matrix))
				continue;

			LookCall call;

			initCall(call, kLookProgramShadowMesh, kLookDepthLess, true);
			call.vertexConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutShadowMesh));

			float local4[16];
			float screen4[16];

			RemasteredLookMath::expand34(matrix, local4);
			RemasteredLookMath::multiply(local4, viewProjection, screen4);
			call.vertexConstants.setFloats(RemasteredLookConsts::kShadowMeshLocalToWorld, local4, 16);
			call.vertexConstants.setFloats(RemasteredLookConsts::kShadowMeshLocalToScreen, screen4, 16);
			call.hasVertexConstants = true;
			call.vertices = _shapeVertices[kShapeBoxLight];
			call.indices = _shapeIndices[kShapeBoxLight];
			call.count = 36;
			queue(kPassZShapes, call);
		}

		issue(kPassZShapes, true, nullptr);
	}
}

void RemasteredLookPipeline::rasterizeCharacterLights() {
	static const int plainPrograms[5] = {
		kLookProgramCharLightGradient, kLookProgramCharLightPoint, kLookProgramCharLightSpot, kLookProgramCharLightDirectional,
		kLookProgramCharLightDirectional
	};
	static const int shadowPrograms[5] = {
		kLookProgramCharLightGradient, kLookProgramCharLightPoint, kLookProgramCharLightSpotShadow,
		kLookProgramCharLightDirectionalShadow, kLookProgramCharLightDirectionalShadow
	};
	static const int environmentPrograms[5] = {
		kLookProgramCharLightGradient, kLookProgramCharLightPoint, kLookProgramCharLightSpotShadowEnv,
		kLookProgramCharLightDirectionalShadowEnv, kLookProgramCharLightDirectionalShadowEnv
	};

	_scene->getCamera(_camera);
	setTargets(_light, _depth);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	_remasteredLookDevice->clear(zeroColor, 0.0f);
	_remasteredLookDevice->setCull(kLookCullBack);

	float view4[16];
	float invView4[16];
	float vd = _camera.viewDistance;

	RemasteredLookMath::expand34(_camera.view, view4);
	RemasteredLookMath::expand34(_camera.invView, invView4);

	uint lightCount = _scene->getCharacterLightCount();
	bool needRender = _scene->getShadowCasterCount() != 0 || !_passes[kPassShadowMeshes].empty() || _framesSinceCameraChange < 2;

	for (int type = 0; type < 5; type++) {
		for (uint i = 0; i < lightCount; i++) {
			LookLightInfo info;

			if (!_scene->getCharacterLightInfo(i, type, info))
				continue;

			float lightMatrix[16];
			float localToCamera[16];

			RemasteredLookMath::expand34(info.matrix, lightMatrix);
			RemasteredLookMath::multiply(lightMatrix, view4, localToCamera);

			int inFront = 0;

			for (int corner = 0; corner < 8; corner++) {
				double depth = (double)boxCorners[corner][0] * localToCamera[1] + (double)boxCorners[corner][1] * localToCamera[5] +
						(double)boxCorners[corner][2] * localToCamera[9] + (double)localToCamera[13];

				if ((float)depth > 0.0f)
					inFront++;
			}

			if (inFront == 0)
				continue;

			bool box = inFront == 8;
			bool insideRadius = !((double)RemasteredLookMath::distanceSquared(_camera.invView + 9, info.matrix + 9) > (double)info.maxDistance * (double)info.maxDistance);
			bool noShadowVariant = !(info.shadowMode > 0 && insideRadius);

			LookCall call;

			initCall(call, plainPrograms[type], kLookDepthLessEqual, false);
			call.vertexConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutCharLightVertex));
			call.vertexConstants.setFloat(RemasteredLookConsts::kProjectedViewDistance, vd);
			call.vertexConstants.setFloat(RemasteredLookConsts::kProjectedViewDistance + 12, box ? 1.0f : 0.0f);
			call.vertexConstants.setFloats(RemasteredLookConsts::kProjectedLocalToCamera, localToCamera, 16);
			call.hasVertexConstants = true;

			LookConstBlock &block = call.pixelConstants;
			float worldToLocal[16];

			RemasteredLookMath::invert(lightMatrix, worldToLocal);
			block.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutCharLightFragment));
			block.setFloats(RemasteredLookConsts::kLightWorldToLocal, worldToLocal, 16);

			float inner[4] = { info.inner[0], info.inner[1], info.inner[2], info.innerRadius };
			float outer[4] = { info.outer[0], info.outer[1], info.outer[2], info.falloffPower };
			float position[4] = { info.position[0], info.position[1], info.position[2], RemasteredLookMath::inverseOneMinus(info.innerRadius) };
			float vertical[4] = { info.verticalInner, info.verticalPower, RemasteredLookMath::inverseOneMinus(info.verticalInner), type == 4 ? 1.0f : 0.0f };
			float shading[4] = { info.shadingBias, RemasteredLookMath::inverseDifference(info.shadingLimit, info.shadingBias), 0.0f, 0.0f };

			block.setFloats(RemasteredLookConsts::kLightInner, inner, 4);
			block.setFloats(RemasteredLookConsts::kLightOuter, outer, 4);
			block.setFloats(RemasteredLookConsts::kLightPosition, position, 4);
			block.setFloats(RemasteredLookConsts::kLightVertical, vertical, 4);
			block.setFloats(RemasteredLookConsts::kLightShading, shading, 4);

			if (!noShadowVariant && needRender) {
				float worldToShadowMap[16];

				issue(kPassLights, true, nullptr);
				shadowMapRender(lightMatrix, type, info.sizeX, info.sizeY, info.nearDistance, info.sizeZ, worldToShadowMap);
				setTargets(_light, _depth);
				_remasteredLookDevice->setColorMask(true, true, true, true);
				_remasteredLookDevice->setCull(kLookCullBack);
				block.setFloats(RemasteredLookConsts::kLightWorldToShadowMap, worldToShadowMap, 16);

				float shadow[4] = { info.shadowBias * 0.005f, info.shadowSoftness * (float)(1.0 / (double)kShadowMapResolution),
						(float)info.shadowQuality, 0.0f };

				block.setFloats(RemasteredLookConsts::kLightShadow, shadow, 4);
			}

			block.setFloats(RemasteredLookConsts::kLightCameraToWorld, invView4, 16);

			float viewDistance[4] = { vd, 0.0f, 0.0f, 0.0f };

			block.setFloats(RemasteredLookConsts::kLightViewDistance, viewDistance, 4);
			call.hasPixelConstants = true;

			if (!noShadowVariant) {
				call.program = info.shadowMode == 1 ? shadowPrograms[type] : environmentPrograms[type];
				call.depthFunc = info.shadowMode == 2 ? kLookDepthAlways : kLookDepthLessEqual;
			}

			call.blendEnable = true;
			call.blendMode = kLookBlendAdd;
			setLightTextures(call, false);

			if (box) {
				call.vertices = _shapeVertices[kShapeBoxLight];
				call.indices = _shapeIndices[kShapeBoxLight];
				call.count = 36;
			} else {
				call.vertices = _shapeVertices[kShapeQuad];
				call.count = -2;
			}

			queue(kPassLights, call);
		}
	}

	issue(kPassLights, true, nullptr);

	if (needRender)
		_passes[kPassShadowMeshes].clear();

	uint aoCount = _scene->getActorAoCount();

	if (aoCount) {
		_remasteredLookDevice->setCull(kLookCullBack);
		_remasteredLookDevice->setColorMask(false, false, false, true);

		for (uint i = 0; i < aoCount; i++) {
			float matrix[12];

			if (!_scene->getActorAoInfo(i, matrix))
				continue;

			float aoMatrix[16];
			float localToCamera[16];
			float worldToLocal[16];

			RemasteredLookMath::expand34(matrix, aoMatrix);
			RemasteredLookMath::multiply(aoMatrix, view4, localToCamera);
			RemasteredLookMath::invert(aoMatrix, worldToLocal);

			LookCall call;

			initCall(call, kLookProgramCharAo, kLookDepthAlways, false);
			call.vertexConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutCharAoVertex));
			call.vertexConstants.setFloat(RemasteredLookConsts::kProjectedViewDistance, vd);
			call.vertexConstants.setFloats(RemasteredLookConsts::kProjectedLocalToCamera, localToCamera, 16);
			call.hasVertexConstants = true;
			call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutCharAoFragment));
			call.pixelConstants.setFloat(RemasteredLookConsts::kAoViewDistance, vd);
			call.pixelConstants.setFloats(RemasteredLookConsts::kAoWorldToLocal, worldToLocal, 16);
			call.pixelConstants.setFloats(RemasteredLookConsts::kAoCameraToWorld, invView4, 16);
			call.hasPixelConstants = true;
			call.blendEnable = true;
			call.blendMode = kLookBlendAdd;
			call.numTextures = 0;
			call.numSamplers = 0;
			call.vertices = _shapeVertices[kShapeBoxLight];
			call.indices = _shapeIndices[kShapeBoxLight];
			call.count = 36;
			queue(kPassAo, call);
		}

		issue(kPassAo, true, nullptr);
		_remasteredLookDevice->setColorMask(true, true, true, true);
	}

	_remasteredLookDevice->setDepth(kLookDepthLessEqual, false);
	_remasteredLookDevice->setCull(kLookCullNone);
}

void RemasteredLookPipeline::rasterizeCutsceneLights() {
	_scene->getCamera(_camera);
	setTargets(_light, _depth);
	_remasteredLookDevice->setDepth(kLookDepthLess, false);
	_remasteredLookDevice->setCull(kLookCullNone);

	uint numSpheres = _scene->getRenderThingCount();

	if (numSpheres > 20)
		numSpheres = 20;

	float centers[20][3];
	float radii[20];

	for (uint i = 0; i < numSpheres; i++)
		_scene->getRenderThingSphere(i, centers[i], radii[i]);

	uint lightCount = _scene->getCutsceneLightCount();

	for (uint i = 0; i < lightCount; i++) {
		LookCutsceneLight light;

		if (!_scene->getCutsceneLightInfo(i, light))
			continue;

		if (light.type >= 2) {
			bool drawn = false;

			for (uint s = 0; s < numSpheres && !drawn; s++) {
				float dx = centers[s][0] - light.position[0];
				float dy = centers[s][1] - light.position[1];
				float dz = centers[s][2] - light.position[2];
				float distance = (float)(((double)dy * dy + (double)dx * dx) + (double)dz * dz);
				float reach = radii[s] + light.settings[1];

				if (reach * reach > distance)
					drawn = true;
			}

			if (!drawn)
				continue;
		}

		LookCall call;
		int program = kLookProgramDeferredLightDirect;
		float settings[4] = { light.settings[0], light.settings[1], 0.0f, 0.0f };
		float color[4] = { light.color[0], light.color[1], light.color[2], 0.0f };

		initCall(call, program, kLookDepthLess, false);

		if (light.type == 0) {
			call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutDeferredDirect));
			color[3] = 1.0f;

			float direction[4] = { light.direction[0], light.direction[1], light.direction[2], 0.0f };

			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneSettings, settings, 4);
			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneColor, color, 4);
			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneVector, direction, 4);
		} else if (light.type == 2) {
			call.program = kLookProgramDeferredLightOmni;
			call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutDeferredOmni));

			float point[4] = { light.position[0], light.position[1], light.position[2], 1.0f };

			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneSettings, settings, 4);
			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneColor, color, 4);
			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneVector, point, 4);
		} else if (light.type == 3) {
			call.program = kLookProgramDeferredLightSpot;
			call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutDeferredSpot));

			float point[4] = { light.position[0], light.position[1], light.position[2], 1.0f };
			float direction[4] = { light.direction[0], light.direction[1], light.direction[2], 1.0f };

			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneSettings, light.settings, 4);
			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneColor, color, 4);
			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneVector, point, 4);
			call.pixelConstants.setFloats(RemasteredLookConsts::kCutsceneDirection, direction, 4);
		}

		call.hasPixelConstants = true;
		call.blendEnable = true;
		call.blendMode = kLookBlendAdd;
		setLightTextures(call, false);
		setFullQuad(call, false);
		issueCall(call);
	}

	RemasteredLookTargetSetup setup;

	setup.color[0] = _albedo;
	setup.color[1] = _position;
	setup.color[2] = _light;
	setup.depth = _depth;
	_remasteredLookDevice->setTargets(setup);
	issue(kPassShadows, true, nullptr);
}

void RemasteredLookPipeline::resolve() {
	_scene->getCamera(_camera);

	LookCall call;
	bool characterPath = _scene->getCharacterLightCount() != 0 || _scene->getCutsceneLightCount() == 0;

	setTargets(_resolved, _depth);

	if (characterPath) {
		LookCompositorInfo compositor;

		_scene->getCompositor(compositor);
		initCall(call, kLookProgramCharResolve, kLookDepthAlways, false);
		call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutCharResolve));

		float inverseDelta = RemasteredLookMath::inverseDifference(compositor.maxLuminance, compositor.minLuminance);
		float envShadow[4] = { compositor.minLuminance, inverseDelta, compositor.power, 0.0f };
		float black[4] = { compositor.black[0], compositor.black[1], compositor.black[2], compositor.aoStrength };
		float white[4] = { compositor.white[0], compositor.white[1], compositor.white[2], 0.0f };
		float irisSize = 0.0f;
		float irisX = 0.0f;
		float irisY = 0.0f;

		_scene->getIris(irisSize, irisX, irisY);

		float irisScale = (float)(1.0 / ((1.0 - (double)irisSize) + 0.000009999999747378752) - 1.0);
		float iris[4] = { irisX, (float)(1.0 - (double)irisY), (float)(640.0 * (double)irisScale / 480.0), irisScale };

		call.pixelConstants.setFloats(RemasteredLookConsts::kResolveEnvShadow, envShadow, 4);
		call.pixelConstants.setFloats(RemasteredLookConsts::kResolveShadowBlack, black, 4);
		call.pixelConstants.setFloats(RemasteredLookConsts::kResolveShadowWhite, white, 4);
		call.pixelConstants.setFloats(RemasteredLookConsts::kResolveIris, iris, 4);
	} else {
		initCall(call, kLookProgramDeferredResolve, kLookDepthLess, false);
		call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutDeferredResolve));
		call.pixelConstants.setFloat(RemasteredLookConsts::kDeferredResolveViewDistance, _camera.viewDistance);
	}

	call.hasPixelConstants = true;
	setLightTextures(call, true);
	setFullQuad(call, false);
	issueCall(call);
}

void RemasteredLookPipeline::postProcess() {
	LookCall call;
	float width = (float)(int)_targetWidth;
	float height = (float)(int)_targetHeight;

	setTargets(_final, nullptr);
	initCall(call, kLookProgramPostProcess, kLookDepthAlways, false);
	call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutPostProcess));

	float first[4] = { -0.5f / width, -0.5f / height, 0.5f / width, 0.5f / height };
	float second[4] = { -2.0f / width, -2.0f / height, 2.0f / width, 2.0f / height };

	call.pixelConstants.setFloats(RemasteredLookConsts::kPostProcessPosOffset, first, 4);
	call.pixelConstants.setFloats(RemasteredLookConsts::kPostProcessOpt1, first, 4);
	call.pixelConstants.setFloats(RemasteredLookConsts::kPostProcessOpt2, second, 4);
	call.pixelConstants.setFloat(RemasteredLookConsts::kPostProcessSharpness, 8.0f);
	call.pixelConstants.setFloat(RemasteredLookConsts::kPostProcessThreshold, 0.125f);
	call.pixelConstants.setFloat(RemasteredLookConsts::kPostProcessThresholdMin, 0.05f);
	call.hasPixelConstants = true;
	call.numTextures = 1;
	call.textures[0] = getTexture(_resolved);
	call.numSamplers = 1;
	call.samplers[0] = linearSampler;
	setFullQuad(call, false);
	issueCall(call);
}

void RemasteredLookPipeline::presentSurface(RemasteredLookTexture *texture, float alpha, bool linear, bool widescreen, bool software) {
	float sourceWidth = (float)texture->getWidth();
	float sourceHeight = (float)texture->getHeight();
	float ratioX = (float)_hostWidth / sourceWidth;
	float ratioY = (float)_hostHeight / sourceHeight;
	float scaleX;
	float scaleY;
	float offsetX = 0.0f;
	float offsetY = 0.0f;

	if (widescreen) {
		scaleX = ratioX;
		scaleY = ratioY;
	} else {
		scaleX = ratioY < ratioX ? ratioY : ratioX;
		scaleY = scaleX;
		offsetX = (float)(((double)_hostWidth - (double)sourceWidth * (double)scaleX) * 0.5);
		offsetY = (float)(0.5 * ((double)_hostHeight - (double)scaleY * (double)sourceHeight));
	}

	float ndcScaleX = (float)(2.0 / (double)_hostWidth);
	float ndcScaleY = (float)(-2.0 / (double)_hostHeight);
	float ndcX = (float)((double)offsetX * (double)ndcScaleX - 1.0);
	float ndcY = (float)((double)offsetY * (double)ndcScaleY + 1.0);
	float ndcWidth = sourceWidth * scaleX * ndcScaleX;
	float ndcHeight = sourceHeight * scaleY * ndcScaleY;
	bool flip = !texture->isRenderTarget();
	float v0 = flip ? 1.0f : 0.0f;
	float v1 = flip ? 0.0f : 1.0f;
	RemasteredLookBuffer *quad = createFrameQuad(ndcX, ndcY, ndcWidth, ndcHeight, 0.0f, v0, 1.0f, v1);
	LookCall call;

	initCall(call, kLookProgramTexturedGammaQuad, kLookDepthAlways, false);
	call.blendEnable = alpha < 0.9999989867210388f;
	call.blendMode = kLookBlendPremultiplied;
	call.numTextures = 1;
	call.textures[0] = texture;
	call.numSamplers = 1;
	call.samplers[0].wrap = kLookWrapClamp;
	call.samplers[0].filter = linear ? 1 : 0;
	call.samplers[0].unused = call.samplers[0].filter;
	call.samplers[0].mip = 1;
	call.vertices = quad;

	float clamped = (alpha > 0.0f) ? (alpha < 1.0f ? alpha : 1.0f) : (alpha < 0.0f ? 0.0f : 1.0f);
	float multiply[4] = { clamped, clamped, clamped, clamped };
	float texel[4] = { (float)(1.0 / (double)sourceWidth), (float)(1.0 / (double)sourceHeight), 0.0f, _scene->getGamma() };

	call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutQuad));
	call.pixelConstants.setFloats(RemasteredLookConsts::kQuadMultiplyColor, multiply, 4);
	call.pixelConstants.setFloats(RemasteredLookConsts::kQuadTexelDesaturateGamma, texel, 4);
	call.hasPixelConstants = true;
	call.vertexConstants = call.pixelConstants;
	call.hasVertexConstants = true;
	(void)software;
	issueCall(call);
}

void RemasteredLookPipeline::present(RemasteredLookTexture *software, float softwareAlpha, bool deferredEnabled, bool softwareEnabled,
		RemasteredLookTexture *overlay, bool frontBufferVisible, bool widescreen, float resolutionScaling) {
	if (!_open)
		return;

	_remasteredLookDevice->beginFrame();
	setTargets(nullptr, nullptr);
	_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	_remasteredLookDevice->setCull(kLookCullNone);
	_remasteredLookDevice->setColorMask(true, true, true, true);

	if (deferredEnabled && _forward->isActive()) {
		_remasteredLookDevice->setDepth(kLookDepthAlways, true);
		_remasteredLookDevice->clear(zeroColor, 0.0f);
		_remasteredLookDevice->setDepth(kLookDepthAlways, false);
	}

	bool deferredLinear = resolutionScaling < 2.0f;

	if (deferredEnabled) {
		if (frontBufferVisible && software)
			presentSurface(software, 1.0f, true, widescreen, true);
		else
			presentSurface(getTexture(_final), 1.0f, deferredLinear, widescreen, false);
	}

	if (softwareEnabled && software)
		presentSurface(software, softwareAlpha, true, widescreen, true);

	if (overlay) {
		LookCall call;

		initCall(call, kLookProgramTexturedQuad, kLookDepthAlways, false);
		call.blendEnable = true;
		call.blendMode = kLookBlendPremultiplied;
		call.numTextures = 1;
		call.textures[0] = overlay;
		call.numSamplers = 1;
		call.samplers[0] = pointSampler;
		setFullQuad(call, true);
		call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutQuad));
		call.pixelConstants.setFloats(RemasteredLookConsts::kQuadMultiplyColor, oneColor, 4);

		float texel[4] = { 1.0f / (float)overlay->getWidth(), 1.0f / (float)overlay->getHeight(), 0.0f, 1.0f };

		call.pixelConstants.setFloats(RemasteredLookConsts::kQuadTexelDesaturateGamma, texel, 4);
		call.hasPixelConstants = true;
		call.vertexConstants = call.pixelConstants;
		call.hasVertexConstants = true;
		issueCall(call);
	}

	_remasteredLookDevice->endFrame();
}

} // End of namespace Grim
