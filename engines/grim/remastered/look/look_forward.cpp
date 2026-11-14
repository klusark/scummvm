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
#include "engines/grim/remastered/look/look_forward.h"
#include "engines/grim/remastered/look/look_math.h"
#include "engines/grim/remastered/look/look_pipeline.h"

#include <string.h>

namespace Grim {

enum {
	kForwardProbeCenter = 144,
	kForwardProbeSize = 160,
	kForwardIrradiance = 176,
	kForwardIrradianceStride = 112
};

static const float forwardZeroColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static const float forwardOneColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

static const LookSampler forwardMeshSampler = { kLookWrapRepeat, 1, 1, 2 };
static const LookSampler forwardShadowSampler = { kLookWrapClamp, 1, 1, 0 };

RemasteredLookForward::RemasteredLookForward(RemasteredLookPipeline *pipeline) : _pipeline(pipeline), _scene(nullptr), _active(false),
		_missingLatticeReported(false) {
}

void RemasteredLookForward::setScene(RemasteredLookScene *scene) {
	_scene = scene;
	_lightprobe.setScene(scene);
}

void RemasteredLookForward::updateFlag() {
	if (!_scene)
		return;

	_active = _scene->getAdvancedLighting() == 0;
	_scene->setLightLodType(_active ? 2 : 1);

	if (_active)
		_lightprobe.beginFrame();
}

void RemasteredLookForward::beginFrame() {
	updateFlag();
}

void RemasteredLookForward::postCameraChange() {
	_pipeline->_framesSinceCameraChange = 0;
	updateFlag();
}

void RemasteredLookForward::drawSubmesh(RemasteredLookBuffer *vertices, RemasteredLookTexture *texture, const float *matrix12, float red, float green, float blue) {
	RemasteredLookPipeline *pipeline = _pipeline;

	pipeline->_scene->getCamera(pipeline->_camera);

	RemasteredLookLightprobe::Lattice *lattice = _lightprobe.getCurrentLattice();

	if (!lattice) {
		if (!_missingLatticeReported) {
			warning("RemasteredLookForward::drawSubmesh(): no light probe lattice is selected");
			_missingLatticeReported = true;
		}

		return;
	}

	LookConstBlock vertexBlock;

	vertexBlock.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutForwardMeshVertex));

	float viewDistance[4] = { pipeline->_camera.viewDistance, 1.0f, 1.0f, 1.0f };

	if (red < 0.0f || !texture) {
		viewDistance[1] = red;
		viewDistance[2] = green;
		viewDistance[3] = blue;
	}

	float model4[16];
	float view4[16];

	RemasteredLookMath::expand34(matrix12, model4);
	RemasteredLookMath::expand34(pipeline->_camera.view, view4);
	vertexBlock.setFloats(RemasteredLookConsts::kLitMeshViewDistance, viewDistance, 4);
	vertexBlock.setFloats(RemasteredLookConsts::kLitMeshModel, model4, 16);
	vertexBlock.setFloats(RemasteredLookConsts::kLitMeshView, view4, 16);

	float probeCenter[4] = { lattice->center[0], lattice->center[1], lattice->center[2], 0.0f };
	float probeSize[4] = { lattice->step10[0], lattice->step10[1], lattice->step10[2], 0.0f };

	vertexBlock.setFloats(kForwardProbeCenter, probeCenter, 4);
	vertexBlock.setFloats(kForwardProbeSize, probeSize, 4);

	for (int face = 0; face < RemasteredLookLightprobe::kNumFaces; face++) {
		for (int probe = 0; probe < RemasteredLookLightprobe::kNumProbes; probe++) {
			float irradiance[4] = { lattice->probe[probe][face][0], lattice->probe[probe][face][1], lattice->probe[probe][face][2], 0.0f };

			vertexBlock.setFloats(kForwardIrradiance + kForwardIrradianceStride * face + 16 * probe, irradiance, 4);
		}
	}

	if (!lattice->constantsBuilt) {
		lattice->fragmentConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutLitMeshFragment));
		lattice->keylightKind = (uint32)_lightprobe.getKeylightAttributes(lattice->fragmentConstants, 0);
		lattice->constantsBuilt = true;
	}

	int variant = 0;

	if (lattice->inKeylight) {
		if (lattice->keylightKind == RemasteredLookLightprobe::kKindSpot)
			variant = 1;
		else if (lattice->keylightKind == RemasteredLookLightprobe::kKindDirectional)
			variant = 2;
	}

	int program = kLookProgramForwardTextured + (texture ? 0 : 3) + variant + ((lattice->flags & RemasteredLookLightprobe::kFlagSingleProbe) ? 6 : 0);
	LookCall call;

	pipeline->initCall(call, program, kLookDepthGreater, true);
	call.setVertexConstants(vertexBlock);
	call.setPixelConstants(lattice->fragmentConstants);
	call.numTextures = 2;
	call.textures[0] = texture;
	call.textures[1] = pipeline->getTexture(pipeline->_shadowMap);
	call.numSamplers = 2;
	call.samplers[0] = forwardMeshSampler;
	call.samplers[1] = forwardShadowSampler;
	call.vertices = vertices;
	pipeline->queue(RemasteredLookPipeline::kPassAlbedo, call);

	if (lattice->inKeylight && !(lattice->flags & RemasteredLookLightprobe::kFlagNoShadowMeshes) && pipeline->_scene->getShadowCharacterLightCount()) {
		LookCall shadow;

		pipeline->initCall(shadow, kLookProgramShadowMesh, kLookDepthLess, true);
		shadow.vertexConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutShadowMesh));
		shadow.vertexConstants.setFloats(RemasteredLookConsts::kShadowMeshLocalToWorld, model4, 16);
		shadow.hasVertexConstants = true;
		shadow.vertices = vertices;
		pipeline->queue(RemasteredLookPipeline::kPassShadowMeshes, shadow);
	}
}

bool RemasteredLookForward::sceneShadows() {
	RemasteredLookPipeline *pipeline = _pipeline;
	RemasteredLookLightprobe::Info &info = _lightprobe.getInfo();
	bool needRender = pipeline->_scene->getShadowCasterCount() != 0 || !pipeline->_passes[RemasteredLookPipeline::kPassShadowMeshes].empty() ||
			pipeline->_framesSinceCameraChange < 2;

	if (!(info.keylightIndex < pipeline->_scene->getCharacterLightCount()))
		return false;

	LookLightInfo light;

	if (!pipeline->_scene->getCharacterLightInfo(info.keylightIndex, info.keylightType, light))
		return false;

	if (needRender) {
		float lightMatrix[16];

		RemasteredLookMath::expand34(light.matrix, lightMatrix);
		pipeline->shadowMapRender(lightMatrix, (int)info.keylightType, light.sizeX, light.sizeY, light.nearDistance, light.sizeZ,
				info.worldToShadowMap);
		pipeline->_remasteredLookDevice->setColorMask(true, true, true, true);
	}

	info.bias = 0.0049999999f * light.shadowBias;
	info.softness = light.shadowSoftness * 0.0009765625f;
	info.quality = (float)light.shadowQuality;
	info.enabled = needRender ? 1.0f : 0.0f;

	return needRender;
}

void RemasteredLookForward::flush() {
	RemasteredLookPipeline *pipeline = _pipeline;
	RemasteredLookDevice *device = pipeline->_remasteredLookDevice;

	pipeline->setTargets(pipeline->_stageTarget, nullptr);
	device->setDepth(kLookDepthAlways, true);
	pipeline->issue(RemasteredLookPipeline::kPassBitmapUnderlays, true, nullptr);

	if (!sceneShadows()) {
		pipeline->setTargets(pipeline->_shadowMap, pipeline->_shadowDepth);
		device->setDepth(kLookDepthAlways, true);
		device->clear(forwardOneColor, 1.0f);
	}

	device->setCull(kLookCullNone);
	pipeline->setTargets(pipeline->_light, pipeline->_depth);
	device->setDepth(kLookDepthAlways, true);
	device->clear(forwardZeroColor, 0.0f);
	pipeline->issue(RemasteredLookPipeline::kPassAlbedo, true, nullptr);
}

void RemasteredLookForward::resolve(RemasteredLookTarget *target, RemasteredLookTarget *depth) {
	RemasteredLookPipeline *pipeline = _pipeline;
	RemasteredLookScene *scene = pipeline->_scene;
	RemasteredLookLightprobe::Info &info = _lightprobe.getInfo();

	scene->getCamera(pipeline->_camera);

	LookCall call;

	pipeline->initCall(call, kLookProgramCharResolveShadows, kLookDepthAlways, false);

	LookConstBlock &block = call.pixelConstants;

	block.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutCharForwardResolve));

	uint aoCount = scene->getActorAoCount();
	LookCompositorInfo compositor;

	if (!scene->getCompositor(compositor))
		compositor = LookCompositorInfo();

	float aoStrength = aoCount ? compositor.aoStrength : 0.0f;
	float difference = compositor.maxLuminance - compositor.minLuminance;
	float inverseDelta = difference == 0.0f ? 0.0f : (float)(1.0 / (double)difference);
	float irisSize = 0.0f;
	float irisX = 0.0f;
	float irisY = 0.0f;

	scene->getIris(irisSize, irisX, irisY);

	float irisScale = (float)(1.0 / ((1.0 - (double)irisSize) + 0.000009999999747378752) - 1.0);
	float irisCenterY = (float)(1.0 - (double)irisY);
	float irisWidth = (float)(640.0 * (double)irisScale / 480.0);

	float envShadow[4] = { compositor.minLuminance, inverseDelta, compositor.power, 0.0f };
	float black[4] = { compositor.black[0], compositor.black[1], compositor.black[2], aoStrength };
	float white[4] = { compositor.white[0], compositor.white[1], compositor.white[2], (float)(info.keylightType != 2) };
	float iris[4] = { irisX, irisCenterY, irisWidth, irisScale };

	block.setFloats(RemasteredLookConsts::kResolveEnvShadow, envShadow, 4);
	block.setFloats(RemasteredLookConsts::kResolveShadowBlack, black, 4);
	block.setFloats(RemasteredLookConsts::kResolveShadowWhite, white, 4);
	block.setFloats(RemasteredLookConsts::kResolveIris, iris, 4);

	float invView4[16];

	RemasteredLookMath::expand34(pipeline->_camera.invView, invView4);
	block.setFloats(RemasteredLookConsts::kForwardResolveCameraToWorld, invView4, 16);

	float aoPosition[4] = { 0.0f, 0.0f, 0.0f, pipeline->_camera.viewDistance };
	float aoMatrix[12];

	if (aoCount && scene->getActorAoInfo(0, aoMatrix)) {
		aoPosition[0] = aoMatrix[9];
		aoPosition[1] = aoMatrix[10];
		aoPosition[2] = aoMatrix[11];
	}

	block.setFloats(RemasteredLookConsts::kForwardResolveAoPosition, aoPosition, 4);
	_lightprobe.getKeylightAttributes(block, RemasteredLookConsts::kForwardResolveWorldToLocal);

	float *shadow = block.getFloats(RemasteredLookConsts::kForwardResolveShadow);

	shadow[0] = (float)((double)shadow[0] * 0.1000000014901161);
	shadow[3] = (float)((info.latticesInKeylight != 0 ? 1.0 : 0.0) * (double)shadow[3]);

	call.hasPixelConstants = true;
	pipeline->setLightTextures(call, true);
	pipeline->setFullQuad(call, false);
	pipeline->setTargets(target, depth);
	pipeline->issueCall(call);
}

void RemasteredLookForward::renderScene() {
	RemasteredLookPipeline *pipeline = _pipeline;
	RemasteredLookDevice *device = pipeline->_remasteredLookDevice;

	flush();

	pipeline->setTargets(pipeline->_virtualDepth, nullptr);
	device->setDepth(kLookDepthAlways, false);

	if (pipeline->_passes[RemasteredLookPipeline::kPassVirtualDepth].empty())
		device->clear(forwardZeroColor, 0.0f);

	pipeline->issue(RemasteredLookPipeline::kPassVirtualDepth, true, nullptr);
	pipeline->renderZShapes();
	resolve(pipeline->_final, pipeline->_depth);
	pipeline->setTargets(pipeline->_final, pipeline->_depth);
	pipeline->issue(RemasteredLookPipeline::kPassBitmapOverlays, true, nullptr);
}

} // End of namespace Grim
