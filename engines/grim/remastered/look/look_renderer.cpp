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

#include "common/system.h"
#include "common/textconsole.h"

#include "engines/grim/actor/sputactor.h"
#include "engines/grim/remastered/look/look_forward.h"
#include "engines/grim/remastered/look/look_lightprobe.h"
#include "engines/grim/remastered/look/look_math.h"
#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/wuregistry.h"

#include <math.h>
#include <string.h>

namespace Grim {

RemasteredLookRenderer::ExtraLightSpec::ExtraLightSpec() : type(0), shadingBias(0.0f), shadingLimit(0.0f), innerRadius(0.0f),
		falloffPower(0.0f), verticalInner(0.0f), verticalPower(0.0f), animationMode(0), animationFrequency(0.0f),
		shadowMode(0), shadowQuality(0), shadowBias(0.0f), shadowSoftness(0.0f), shadowNear(0.0f), shadowMaxDistance(0.0f),
		intensityA(0.0f), intensityB(0.0f) {
	for (int i = 0; i < 3; i++) {
		position[i] = 0.0f;
		rotation[i] = 0.0f;
		scale[i] = 0.0f;
		innerA[i] = 0.0f;
		outerA[i] = 0.0f;
		innerB[i] = 0.0f;
		outerB[i] = 0.0f;
	}
}

RemasteredLookRenderer::RemasteredLookRenderer(RenderDroid *rd, SputRoom *room, SputActorModule *actors, SputRender *render, SputTime *time, WuRegistry *registry) :
		_rd(rd), _room(room), _actors(actors), _render(render), _time(time), _registry(registry), _remasteredLookDevice(nullptr),
		_remasteredLookPipeline(nullptr), _remasteredLookTextureCache(nullptr), _softwareTexture(nullptr), _overlayTexture(nullptr), _open(false), _hostWidth(0),
		_hostHeight(0), _drawBuffer(nullptr), _zBuffer(nullptr), _frontBuffer(nullptr), _frontBufferVisible(false),
		_previousFrameTime(0.0), _hasPreviousFrame(false), _softwareAlpha(0.0f), _softwareAlphaEnd(0.0f),
		_softwareAlphaStart(0.0f), _softwareAlphaBlendTime(0.75f), _deferredEnabled(false), _softwareEnabled(true),
		_graphicsChanged(false), _widescreen(false), _resolutionScaling(2.0f), _random("lookLights"), _extraCount(0),
		_lightLodType(CharacterLight::kLodDeferred) {
}

RemasteredLookRenderer::~RemasteredLookRenderer() {
	close();
}

bool RemasteredLookRenderer::open(int hostWidth, int hostHeight) {
	if (_open)
		return true;

	_remasteredLookDevice = createRemasteredLookDevice();

	if (!_remasteredLookDevice)
		return false;

	if (!_remasteredLookDevice->open(hostWidth, hostHeight)) {
		delete _remasteredLookDevice;
		_remasteredLookDevice = nullptr;
		return false;
	}

	_hostWidth = hostWidth;
	_hostHeight = hostHeight;
	_resolutionScaling = _registry->getFloat("ResolutionScaling", 2.0f);
	_widescreen = (int)_registry->getFloat("Widescreen", 0.0f) == 2;
	_remasteredLookPipeline = new RemasteredLookPipeline(_remasteredLookDevice);

	if (!_remasteredLookPipeline->open(hostWidth, hostHeight, _resolutionScaling)) {
		delete _remasteredLookPipeline;
		_remasteredLookPipeline = nullptr;
		_remasteredLookDevice->close();
		delete _remasteredLookDevice;
		_remasteredLookDevice = nullptr;
		return false;
	}

	_remasteredLookPipeline->setScene(this);
	_remasteredLookTextureCache = new RemasteredLookTextureCache(_remasteredLookDevice);

	Common::Array<byte> blank;
	LookImage image;

	blank.resize(640 * 480 * 2);
	memset(&blank[0], 0, blank.size());
	image.format = kLookFormatRgb565;
	image.width = 640;
	image.height = 480;
	image.numLevels = 1;
	image.levels[0].data = &blank[0];
	image.levels[0].size = blank.size();
	image.levels[0].width = 640;
	image.levels[0].height = 480;
	_softwareTexture = _remasteredLookDevice->createTexture(image);

	Common::Array<byte> overlay;

	overlay.resize((uint)hostWidth * hostHeight * 4);
	memset(&overlay[0], 0, overlay.size());
	image.format = kLookFormatRgba8;
	image.width = hostWidth;
	image.height = hostHeight;
	image.levels[0].data = &overlay[0];
	image.levels[0].size = overlay.size();
	image.levels[0].width = hostWidth;
	image.levels[0].height = hostHeight;
	_overlayTexture = _remasteredLookDevice->createTexture(image);

	if (!_softwareTexture || !_overlayTexture) {
		warning("RemasteredLookRenderer::open(): the composition textures could not be created");
		_open = true;
		close();
		return false;
	}

	_open = true;
	_extraCount = 0;

	return true;
}

void RemasteredLookRenderer::close() {
	if (!_open && !_remasteredLookDevice)
		return;

	releaseAllVertexBuffers();

	if (_remasteredLookTextureCache) {
		_remasteredLookTextureCache->clear();
		delete _remasteredLookTextureCache;
		_remasteredLookTextureCache = nullptr;
	}

	if (_softwareTexture)
		_remasteredLookDevice->destroyTexture(_softwareTexture);

	if (_overlayTexture)
		_remasteredLookDevice->destroyTexture(_overlayTexture);

	delete _remasteredLookPipeline;
	_remasteredLookPipeline = nullptr;

	if (_remasteredLookDevice) {
		_remasteredLookDevice->close();
		delete _remasteredLookDevice;
		_remasteredLookDevice = nullptr;
	}

	_setx.clear();
	_loadedRoom.clear();
	_open = false;
	_deferredEnabled = false;
	_softwareEnabled = true;
}

void RemasteredLookRenderer::setBuffers(const StdVBuffer *drawBuffer, const StdVBuffer *zBuffer, const StdVBuffer *frontBuffer) {
	_drawBuffer = drawBuffer;
	_zBuffer = zBuffer;
	_frontBuffer = frontBuffer;
}

void RemasteredLookRenderer::updateAlpha() {
	double now = (double)g_system->getMillis();

	if (!_hasPreviousFrame) {
		_previousFrameTime = now;
		_hasPreviousFrame = true;
	}

	double previous = _previousFrameTime;

	_previousFrameTime = now;

	float mode = _registry->getFloat("RenderingMode", 0.0f);
	float previousEnd = _softwareAlphaEnd;

	_softwareAlphaEnd = (int)mode == 0 ? 0.0f : 1.0f;

	if ((double)fabsf(_softwareAlphaEnd - previousEnd) > 0.001) {
		_softwareAlphaStart = _softwareAlpha;
		_softwareAlphaBlendTime = 0.0f;
	}

	if ((double)fabsf(_softwareAlphaEnd - _softwareAlpha) <= 0.001) {
		_softwareAlpha = _softwareAlphaEnd;
	} else {
		float step = (float)((now - previous) / 1000.0);
		float blend = step + _softwareAlphaBlendTime;

		_softwareAlphaBlendTime = blend < 0.75f ? blend : 0.75f;

		float t = _softwareAlphaBlendTime / 0.75f;
		float value = (float)(((double)t * (double)_softwareAlphaEnd) + ((1.0 - (double)t) * (double)_softwareAlphaStart));
		float clamped = 0.0f;

		if (!(0.0f > value))
			clamped = 1.0f < value ? 1.0f : value;

		_softwareAlpha = (float)((3.0 - (double)(clamped + clamped)) * ((double)clamped * (double)clamped));
	}

	_deferredEnabled = (double)_softwareAlpha < 0.9999989867210388;
	_softwareEnabled = (double)_softwareAlpha > 0.000001;
}

void RemasteredLookRenderer::getCurrentSetupName(Common::String &name) const {
	name.clear();

	Room *room = _room->getCurrentRoom();

	if (room && room->setups && room->curSetup >= 0 && (uint)room->curSetup < room->numSetups)
		name = room->setups[room->curSetup].name;
}

bool RemasteredLookRenderer::getLightStatus(const char *name, int &value) const {
	Room *room = _room->getCurrentRoom();

	if (!room)
		return false;

	for (uint i = 0; i < room->numLights; i++) {
		if (!scumm_stricmp(name, room->lights[i].name)) {
			value = room->lights[i].light.active;
			return true;
		}
	}

	return false;
}

void RemasteredLookRenderer::updateRoom() {
	Room *room = _room->getCurrentRoom();
	Common::String name;

	if (room)
		name = room->name;

	if (name == _loadedRoom)
		return;

	_loadedRoom = name;
	_setx.clear();
	_extraCount = 0;

	for (int i = 0; i < kMaxExtraLights; i++)
		_extraLights[i] = CharacterLight();

	if (!room)
		return;

	if (!_setx.loadForSet(_rd->getHs(), name.c_str()))
		_setx.clear();

	Common::String setup;

	getCurrentSetupName(setup);
	_setx.resetKeylightStates(setup.c_str());
}

void RemasteredLookRenderer::updateExtraLights(float step) {
	for (uint i = 0; i < _extraCount; i++)
		_extraLights[i].updateAnimation(step, _random);

	_extraCount = 0;
}

void RemasteredLookRenderer::recoverContext() {
	if (!_open || !_remasteredLookDevice->isContextLost())
		return;

	warning("RemasteredLookRenderer::recoverContext(): the OpenGL context was recreated, rebuilding the look");

	int width = _hostWidth;
	int height = _hostHeight;

	_remasteredLookDevice->abandon();
	close();

	if (!open(width, height))
		warning("RemasteredLookRenderer::recoverContext(): the look could not be restarted");
}

void RemasteredLookRenderer::beginFrame() {
	recoverContext();

	if (!_open)
		return;

	updateAlpha();

	if (!_deferredEnabled)
		return;

	if (_graphicsChanged) {
		_graphicsChanged = false;
		_remasteredLookPipeline->reallocate(_resolutionScaling);

		if (!_remasteredLookPipeline->isOpen()) {
			warning("RemasteredLookRenderer::beginFrame(): the render targets could not be recreated, the look is disabled");
			_deferredEnabled = false;
			_softwareAlpha = 1.0f;
			_softwareEnabled = true;
			return;
		}
	}

	updateRoom();

	float step = _time->getFrameTimeFlex() * 10.0f;

	if (_setx.isLoaded())
		_setx.update(_time->getFrameTimeFlex(), _random);

	updateExtraLights(step);

	Room *room = _room->getCurrentRoom();
	bool smoothing = (int)_registry->getFloat("ClassicLighting", 0.0f) == 0 && !(room && strstr(room->name, "_inv"));

	_frontBufferVisible = false;
	_remasteredLookPipeline->beginFrame(smoothing);
}

void RemasteredLookRenderer::postCameraChange() {
	if (_open && _remasteredLookPipeline)
		_remasteredLookPipeline->postCameraChange();
}

void RemasteredLookRenderer::notifyChange(const void *key) {
	if (_open)
		_remasteredLookTextureCache->invalidate(key);
}

void RemasteredLookRenderer::notifyBufferWritten(const StdVBuffer *buffer) {
	if (!_open || !buffer)
		return;

	_remasteredLookTextureCache->invalidate(buffer->pixels);

	if (buffer == _frontBuffer)
		_frontBufferVisible = true;
}

void RemasteredLookRenderer::notifyFree(const void *key) {
	if (!_open)
		return;

	_remasteredLookTextureCache->release(key);
	releaseVertexBuffers(key);
}

void RemasteredLookRenderer::notifyMaterialLoad(const void *pixels, const char *materialName, int cellIndex) {
	if (_open)
		_remasteredLookTextureCache->loadReplacement(_rd->getHs(), pixels, materialName, cellIndex);
}

void RemasteredLookRenderer::releaseVertexBuffers(const void *key) {
	for (int i = (int)_vertexEntries.size() - 1; i >= 0; i--) {
		if (_vertexEntries[i].vertices == key || _vertexEntries[i].set == key) {
			_remasteredLookDevice->destroyBuffer(_vertexEntries[i].buffer);
			_vertexEntries.remove_at(i);
		}
	}
}

void RemasteredLookRenderer::releaseAllVertexBuffers() {
	for (uint i = 0; i < _vertexEntries.size(); i++)
		_remasteredLookDevice->destroyBuffer(_vertexEntries[i].buffer);

	_vertexEntries.clear();
}

RemasteredLookBuffer *RemasteredLookRenderer::getVertexBuffer(RdModel3Mesh *mesh, const void *set, int index, float scaleU, float scaleV, int &count) {
	for (uint i = 0; i < _vertexEntries.size(); i++) {
		if (_vertexEntries[i].vertices == mesh->vertices && _vertexEntries[i].set == set && _vertexEntries[i].index == index) {
			count = (int)_vertexEntries[i].buffer->getNumElements();
			return _vertexEntries[i].buffer;
		}
	}

	Common::Array<float> data;

	for (uint f = 0; f < mesh->numFaces; f++) {
		RdFace3 *face = &mesh->faces[f];

		if (face->material != set || face->curCel != index || face->numVertices < 3)
			continue;

		for (uint k = 0; k + 2 < face->numVertices; k++) {
			uint corners[3] = { 0, k + 1, k + 2 };

			for (int c = 0; c < 3; c++) {
				uint16 positionIndex = (uint16)face->vertices[corners[c]];
				uint16 uvIndex = (uint16)face->texVertices[corners[c]];
				const float *position = mesh->vertices[positionIndex];
				const float *normal = mesh->vertexNormals ? mesh->vertexNormals[positionIndex] : nullptr;
				const float *uv = mesh->texVertices ? mesh->texVertices[uvIndex] : nullptr;

				data.push_back(position[0]);
				data.push_back(position[1]);
				data.push_back(position[2]);
				data.push_back(normal ? normal[0] : 0.0f);
				data.push_back(normal ? normal[1] : 0.0f);
				data.push_back(normal ? normal[2] : 0.0f);
				data.push_back(uv ? uv[0] * scaleU : 0.0f);
				data.push_back(uv ? uv[1] * scaleV : 0.0f);
			}
		}
	}

	count = (int)(data.size() / 8);

	if (count == 0)
		return nullptr;

	VertexEntry entry;

	entry.vertices = mesh->vertices;
	entry.set = set;
	entry.index = index;
	entry.buffer = _remasteredLookDevice->createVertexBuffer(kLookVertexTextured, data.data(), count);
	_vertexEntries.push_back(entry);

	return entry.buffer;
}

void RemasteredLookRenderer::drawModel3Mesh(RdModel3Mesh *mesh, const float *matrix12) {
	if (!isLookEnabled() || !mesh || !mesh->vertices)
		return;

	Common::Array<const void *> sets;
	Common::Array<int> indices;

	for (uint f = 0; f < mesh->numFaces; f++) {
		RdFace3 *face = &mesh->faces[f];
		bool known = false;

		for (uint i = 0; i < sets.size(); i++) {
			if (sets[i] == face->material && indices[i] == face->curCel) {
				known = true;
				break;
			}
		}

		if (!known) {
			sets.push_back(face->material);
			indices.push_back(face->curCel);
		}
	}

	RdColormap *colormap = _rd->_colormap.getCurrent();

	for (uint i = 0; i < sets.size(); i++) {
		RdMaterial *set = (RdMaterial *)sets[i];
		int index = indices[i];
		int celIndex = index;
		StdVBuffer *buffer = nullptr;
		bool keyFlag = false;
		int keyValue = 0;
		int colorIndex = 0;
		bool negate = false;

		if (set) {
			if (celIndex < 0)
				celIndex = set->curCel;

			RdMatCel *cel = (celIndex >= 0 && (uint)celIndex < set->numCels) ? set->cels[celIndex] : nullptr;

			colorIndex = cel ? cel->colorIndex : 0;
			negate = (set->flags & 4) != 0;

			if ((set->flags & 2) && cel && (cel->flags & 8) && cel->texture) {
				buffer = cel->texture->mips[0];
				keyFlag = (cel->texture->flags & 1) != 0;
				keyValue = cel->texture->colorKey;
			}
		}

		const byte *palette = nullptr;
		int width = 0;
		int height = 0;

		if (buffer) {
			width = buffer->tex.width;
			height = buffer->tex.height;

			if (buffer->tex.format.colorMode == 0)
				palette = buffer->paletteRGB ? buffer->paletteRGB : (colormap ? colormap->colors : nullptr);
		} else {
			palette = colormap ? colormap->colors : nullptr;
		}

		float scaleU = 1.0f;
		float scaleV = 1.0f;

		if (width != 0 && height != 0) {
			scaleU = 1.0f / (float)width;
			scaleV = 1.0f / (float)height;
		}

		int count = 0;
		RemasteredLookBuffer *vertices = getVertexBuffer(mesh, set, index, scaleU, scaleV, count);

		if (!vertices)
			continue;

		RemasteredLookTexture *texture = nullptr;

		if (buffer && buffer->pixels)
			texture = _remasteredLookTextureCache->getTexture(buffer->pixels, width, height, buffer->tex.format.bpp, palette, keyFlag, keyValue, false, false);

		if (buffer && !texture)
			warning("RemasteredLookRenderer::drawModel3Mesh(): mesh %s has no texture for %p (%dx%d, bpp %d, key %d/%d, palette %p)", mesh->name,
					(const void *)buffer->pixels, width, height, buffer->tex.format.bpp, keyFlag, keyValue, (const void *)palette);

		float red = 0.0f;
		float green = 0.0f;
		float blue = 0.0f;

		if (palette) {
			const byte *color = palette + 3 * colorIndex;

			red = (float)color[0] / 255.0f;
			green = (float)color[1] / 255.0f;
			blue = (float)color[2] / 255.0f;
		}

		if (negate) {
			red = -red;
			green = -green;
			blue = -blue;
		}

		_remasteredLookPipeline->drawSubmesh(vertices, texture, matrix12, red, green, blue);
	}
}

bool RemasteredLookRenderer::getCameraRelativeValue(const float *center, float radius, float &value) const {
	float step[3] = { 0.25f * radius, 0.25f * radius, radius * 0.5f };
	RdMatrix34 camera;

	_rd->_camera.getMatrix(camera);

	float distance = (float)sqrt((double)RemasteredLookMath::distanceSquared(camera[3], center));
	float inverse = (float)(1.0 / ((double)distance + 1.0));
	float length = 0.0f;

	length = (float)((double)step[0] * step[0] + (double)length);
	length = (float)((double)step[1] * step[1] + (double)length);
	length = (float)((double)step[2] * step[2] + (double)length);
	value = (float)sqrt((double)length) * inverse;

	return true;
}

void RemasteredLookRenderer::addExtraLight(const ExtraLightSpec &spec) {
	if (_extraCount > 15)
		return;

	uint found = kMaxExtraLights;

	for (uint k = _extraCount; k < (uint)kMaxExtraLights; k++) {
		if (_extraLights[k].name == spec.name) {
			found = k;
			break;
		}
	}

	if (found == (uint)kMaxExtraLights) {
		CharacterLight &light = _extraLights[_extraCount];

		light = CharacterLight();
		light.name = spec.name;
		light.type = (byte)spec.type;
		light.shadingBias = spec.shadingBias;
		light.shadingLimit = spec.shadingLimit;
		light.innerRadius = spec.innerRadius;
		light.falloffPower = spec.falloffPower;
		light.verticalInnerRadius = spec.verticalInner;
		light.verticalFalloffPower = spec.verticalPower;
		light.animationMode = (byte)spec.animationMode;
		light.animationFrequency = spec.animationFrequency;
		light.shadowMode = (byte)spec.shadowMode;
		light.shadowQuality = (byte)spec.shadowQuality;
		light.shadowBias = spec.shadowBias;
		light.shadowSoftness = spec.shadowSoftness;
		light.shadowNear = spec.shadowNear;
		light.shadowMaxDistance = spec.shadowMaxDistance;
		light.lodType = 0;

		for (int i = 0; i < 3; i++) {
			light.colorsA.innerColor[i] = spec.innerA[i];
			light.colorsA.outerColor[i] = spec.outerA[i];
			light.colorsB.innerColor[i] = spec.innerB[i];
			light.colorsB.outerColor[i] = spec.outerB[i];
		}

		light.colorsA.intensity = spec.intensityA;
		light.colorsB.intensity = spec.intensityB;
		light.resetAnimationState();
	} else if (found != _extraCount) {
		_extraLights[_extraCount] = _extraLights[found];
		_extraLights[found] = CharacterLight();
	}

	CharacterLight &slot = _extraLights[_extraCount];

	for (int i = 0; i < 3; i++) {
		slot.position[i] = spec.position[i];
		slot.rotation[i] = spec.rotation[i];
		slot.scale[i] = spec.scale[i];
	}

	_extraCount++;
}

void RemasteredLookRenderer::drawBeginModel3(RdModel3 *model, const float *worldCenter, float radius, const float *matrix12, const char *parentName) {
	if (!isLookEnabled())
		return;

	_remasteredLookPipeline->setModelCastsShadows(true);

	if (_remasteredLookPipeline->isForwardActive()) {
		_remasteredLookPipeline->getForward()->getLightprobe()->beginLattice(model->name, worldCenter, radius, parentName);
		return;
	}

	bool casts = true;

	if (!(radius != radius)) {
		float value = 0.0f;

		getCameraRelativeValue(worldCenter, radius, value);
		casts = value > 0.0099999998f;

		if (value <= 0.0099999998f && strstr(model->name, "manny"))
			casts = true;
	}

	_remasteredLookPipeline->setModelCastsShadows(casts);

	Common::String setup;

	getCurrentSetupName(setup);

	if (strstr(model->name, "cigarette")) {
		float z = worldCenter[2];

		if (!strstr(setup.c_str(), "ga_") || z > 0.25f) {
			ExtraLightSpec spec;

			spec.name = model->name;
			spec.type = 1;
			spec.position[0] = worldCenter[0];
			spec.position[1] = worldCenter[1];
			spec.position[2] = z;

			float aoMatrix[12];

			if (getActorAoCount() && getActorAoInfo(0, aoMatrix)) {
				float difference[3] = { spec.position[0] - aoMatrix[9], spec.position[1] - aoMatrix[10], 0.0f };
				float direction[3];

				RemasteredLookMath::normalize3(difference, direction);

				float x = spec.position[0];
				float y = spec.position[1];

				spec.position[0] = (float)((double)direction[0] * 0.05000000074505806 + (double)x);
				spec.position[1] = (float)(0.05000000074505806 * (double)direction[1] + (double)y);
			}

			for (int i = 0; i < 3; i++)
				spec.scale[i] = 0.25f;

			spec.shadingBias = 0.0f;
			spec.shadingLimit = 1.0f;
			spec.innerRadius = 0.25f;
			spec.falloffPower = 2.0f;
			spec.verticalInner = 1.0f;
			spec.verticalPower = 1.0f;
			spec.animationMode = 2;
			spec.animationFrequency = 2.0f;
			spec.innerA[0] = 1.0f;
			spec.innerA[1] = 0.5f;
			spec.innerA[2] = 0.0f;
			spec.outerA[0] = 1.0f;
			memcpy(spec.innerB, spec.innerA, sizeof(spec.innerB));
			memcpy(spec.outerB, spec.outerA, sizeof(spec.outerB));
			spec.intensityA = 0.5f;
			spec.intensityB = 0.4f;
			spec.shadowMode = 0;
			spec.shadowQuality = 0;
			spec.shadowBias = 1.0f;
			spec.shadowSoftness = 4.0f;
			spec.shadowNear = 0.1f;
			spec.shadowMaxDistance = 1.0f;
			addExtraLight(spec);
		}
	}

	if (strstr(model->name, "chepito")) {
		bool inSu = strstr(setup.c_str(), "su_") != nullptr;
		float scaleFactor = inSu ? 0.699999988079071f : 1.0f;
		ExtraLightSpec spec;

		spec.name = model->name;
		spec.type = 1;
		spec.position[0] = worldCenter[0];
		spec.position[1] = worldCenter[1];
		spec.position[2] = (float)((double)worldCenter[2] + 0.5);

		for (int i = 0; i < 3; i++) {
			spec.scale[i] = 2.5f;
			spec.innerA[i] = i == 0 ? 1.0f : (i == 1 ? 0.9529411792755127f : 0.6470588445663452f);
			spec.outerA[i] = spec.innerA[i];
			spec.innerB[i] = spec.innerA[i];
			spec.outerB[i] = spec.innerA[i];
		}

		spec.shadingBias = -0.25f;
		spec.shadingLimit = 0.75f;
		spec.innerRadius = 0.25f;
		spec.falloffPower = 1.0f;
		spec.verticalInner = 1.0f;
		spec.verticalPower = 1.0f;
		spec.animationMode = 2;
		spec.animationFrequency = 0.5f;
		spec.intensityA = scaleFactor;
		spec.intensityB = 0.5f * scaleFactor;
		spec.shadowBias = 1.0f;
		spec.shadowSoftness = 4.0f;
		spec.shadowNear = 0.1f;
		spec.shadowMaxDistance = 1.0f;
		addExtraLight(spec);

		if (inSu) {
			float outward[3] = { -spec.position[0], -spec.position[1], 0.0f };
			float inward[3];

			RemasteredLookMath::normalize3(outward, inward);

			float angle = (float)atan2((double)inward[0], (double)inward[1]);
			ExtraLightSpec spot = spec;

			spot.name = "su_spot";
			spot.type = 2;
			spot.position[0] = (float)((double)inward[0] * 1.5 + (double)worldCenter[0]);
			spot.position[1] = (float)(1.5 * (double)inward[1] + (double)worldCenter[1]);
			spot.position[2] = 0.40000001f;
			spot.rotation[0] = 55.0f;
			spot.rotation[1] = 0.0f;
			spot.rotation[2] = (float)((double)angle * 180.0 / 3.141592025756836);
			spot.scale[0] = 2.0f;
			spot.scale[1] = 1.5f;
			spot.scale[2] = 2.0f;
			spot.shadingBias = 0.0f;
			spot.shadingLimit = 1.0f;
			spot.innerRadius = 0.75f;
			spot.falloffPower = 1.0f;
			spot.verticalInner = 0.8500000238418579f;
			spot.verticalPower = 1.0f;
			spot.animationMode = 0;
			spot.animationFrequency = 0.0f;
			spot.shadowMode = 2;
			spot.shadowQuality = 1;
			spot.shadowBias = 1.0f;
			spot.shadowSoftness = 4.0f;
			spot.shadowNear = 0.1f;
			spot.shadowMaxDistance = 10.0f;
			addExtraLight(spot);
		}
	}

	if (strstr(model->name, "lola") && strstr(setup.c_str(), "bi_")) {
		int status = 0;

		if (getLightStatus("flash", status) && (status & 0xFF)) {
			ExtraLightSpec spec;

			spec.name = "bi_flash";
			spec.type = 1;
			spec.position[0] = 0.0f;
			spec.position[1] = -1.75f;
			spec.position[2] = 0.5f;

			for (int i = 0; i < 3; i++) {
				spec.scale[i] = 2.0f;
				spec.innerA[i] = 1.0f;
				spec.outerA[i] = 1.0f;
				spec.innerB[i] = 1.0f;
				spec.outerB[i] = 1.0f;
			}

			spec.shadingBias = 0.0f;
			spec.shadingLimit = 1.0f;
			spec.innerRadius = 0.75f;
			spec.falloffPower = 2.0f;
			spec.verticalInner = 1.0f;
			spec.verticalPower = 1.0f;
			spec.animationMode = 0;
			spec.animationFrequency = 1.0f;
			spec.intensityA = 3.0f;
			spec.intensityB = 1.5f;
			spec.shadowBias = 1.0f;
			spec.shadowSoftness = 4.0f;
			spec.shadowNear = 0.1f;
			spec.shadowMaxDistance = 1.0f;
			addExtraLight(spec);
		}
	}

	if (strstr(model->name, "bonewagon") && strstr(setup.c_str(), "at_")) {
		float row[3] = { matrix12[3], matrix12[4], matrix12[5] };
		float forward[3];

		RemasteredLookMath::normalize3(row, forward);

		ExtraLightSpec spec;

		spec.name = model->name;
		spec.type = 1;

		for (int i = 0; i < 3; i++) {
			spec.position[i] = (float)((double)forward[i] * 2.0 + (double)worldCenter[i]);
			spec.scale[i] = 2.0f;
			spec.innerA[i] = i == 0 ? 1.0f : (i == 1 ? 0.9529411792755127f : 0.6470588445663452f);
			spec.outerA[i] = spec.innerA[i];
			spec.innerB[i] = spec.innerA[i];
			spec.outerB[i] = spec.innerA[i];
		}

		spec.shadingBias = 0.0f;
		spec.shadingLimit = 1.0f;
		spec.innerRadius = 0.25f;
		spec.falloffPower = 1.0f;
		spec.verticalInner = 1.0f;
		spec.verticalPower = 1.0f;
		spec.animationMode = 0;
		spec.animationFrequency = 1.0f;
		spec.intensityA = 0.699999988079071f;
		spec.intensityB = 0.3499999940395355f;
		spec.shadowBias = 1.0f;
		spec.shadowSoftness = 4.0f;
		spec.shadowNear = 0.1f;
		spec.shadowMaxDistance = 1.0f;
		addExtraLight(spec);

		ExtraLightSpec spot = spec;

		spot.name = "at_spot";
		spot.type = 2;

		for (int i = 0; i < 3; i++)
			spot.position[i] = (float)((double)forward[i] * 3.0 + (double)worldCenter[i]);

		spot.rotation[0] = forward[1] < -0.5f ? -60.0f : 60.0f;
		spot.rotation[1] = 0.0f;
		spot.rotation[2] = 0.0f;
		spot.scale[0] = 1.0f;
		spot.scale[1] = 1.0f;
		spot.scale[2] = 2.0f;
		spot.innerRadius = 0.75f;
		spot.verticalInner = 0.75f;
		spot.animationFrequency = 0.0f;
		spot.shadowMode = 2;
		spot.shadowQuality = 1;
		spot.shadowMaxDistance = 10.0f;
		addExtraLight(spot);
	}
}

void RemasteredLookRenderer::drawEndModel3() {
	if (isLookEnabled() && _remasteredLookPipeline->isForwardActive())
		_remasteredLookPipeline->getForward()->getLightprobe()->endLattice();
}

void RemasteredLookRenderer::drawSprite(RdThing *thing, float (*matrix)[3]) {
	if (!isLookEnabled())
		return;

	RdSprite *sprite = thing->data.sprite;
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 point;
	RdVector3 offset;

	_rd->transformPoint34(point, matrix[3], camera->viewMatrix);
	_rd->transformVector34(offset, sprite->offset, matrix);
	_rd->transformVector34Acc(offset, camera->viewMatrix);

	RdVector3 input;
	RdVector3 first;
	RdVector3 second;

	input[0] = (offset[0] - sprite->halfWidth) + point[0];
	input[1] = (offset[1] + 0.0f) + point[1];
	input[2] = ((sprite->halfHeight + sprite->halfHeight) + offset[2]) + point[2];
	(_rd->_camera.*camera->project)(first, input);
	input[0] = (offset[0] + sprite->halfWidth) + point[0];
	input[1] = (offset[1] + 0.0f) + point[1];
	input[2] = (offset[2] + 0.0f) + point[2];
	(_rd->_camera.*camera->project)(second, input);

	RdMaterial *set = sprite->face.material;
	int celIndex = thing->curCel;

	if (!set)
		return;

	if (celIndex < 0)
		celIndex = set->curCel;

	RdMatCel *cel = (celIndex >= 0 && (uint)celIndex < set->numCels) ? set->cels[celIndex] : nullptr;

	if (!(set->flags & 2) || !cel || !(cel->flags & 8) || !cel->texture)
		return;

	StdVBuffer *buffer = cel->texture->mips[0];

	if (!buffer || !buffer->pixels)
		return;

	const byte *palette = nullptr;
	RdColormap *colormap = _rd->_colormap.getCurrent();

	if (buffer->tex.format.colorMode == 0)
		palette = buffer->paletteRGB ? buffer->paletteRGB : (colormap ? colormap->colors : nullptr);

	LookSpriteDraw draw;

	draw.texture = _remasteredLookTextureCache->getTexture(buffer->pixels, buffer->tex.width, buffer->tex.height, buffer->tex.format.bpp, palette,
			(cel->texture->flags & 1) != 0, cel->texture->colorKey, false, false);
	memcpy(draw.corner0, first, sizeof(draw.corner0));
	memcpy(draw.corner1, second, sizeof(draw.corner1));
	draw.textureWidth = buffer->tex.width;
	draw.textureHeight = buffer->tex.height;
	_remasteredLookPipeline->drawSprite(draw);
}

bool RemasteredLookRenderer::drawShadowGeometry(const float *vertices, uint count, uint colour565) {
	if (!isLookEnabled())
		return false;

	return _remasteredLookPipeline->drawShadowGeometry(vertices, count, colour565, _softwareEnabled);
}

void RemasteredLookRenderer::bufferCopy(const StdVBuffer *destination, const StdVBuffer *source, int x, int y, const StdRect *rect, bool colorKeyFlag) {
	if (!_open || !source || !source->pixels)
		return;

	bool isZ = destination == _zBuffer;
	bool isDraw = destination == _drawBuffer;

	if (!isZ && !isDraw) {
		if (destination == _frontBuffer)
			_frontBufferVisible = source != _drawBuffer;
		else
			_remasteredLookTextureCache->invalidate(destination->pixels);

		return;
	}

	if (!_deferredEnabled)
		return;

	_frontBufferVisible = false;

	int bpp = isZ ? 16 : source->tex.format.bpp;
	const byte *palette = nullptr;

	if (source->tex.format.colorMode == 0) {
		RdColormap *colormap = _rd->_colormap.getCurrent();

		palette = source->paletteRGB ? source->paletteRGB : (colormap ? colormap->colors : nullptr);
	}

	LookBitmapDraw draw;

	draw.texture = _remasteredLookTextureCache->getTexture(source->pixels, source->tex.width, source->tex.height, bpp, palette, colorKeyFlag, source->colorKey, false, isZ);
	draw.isZ = isZ;
	draw.is565 = bpp == 16;
	draw.destinationX = x;
	draw.destinationY = y;
	draw.hasRect = rect != nullptr;

	if (rect) {
		draw.rectX = rect->x;
		draw.rectY = rect->y;
		draw.rectWidth = rect->width;
		draw.rectHeight = rect->height;
	}

	draw.textureWidth = source->tex.width;
	draw.textureHeight = source->tex.height;
	draw.sourceWidth = source->tex.width;
	draw.sourceHeight = source->tex.height;
	draw.keyFlag = colorKeyFlag;
	draw.key565 = (uint)source->colorKey;
	_remasteredLookPipeline->drawBitmap(draw);
}

void RemasteredLookRenderer::present(const byte *software565, int softwarePitch, const byte *overlayRgba, bool overlayChanged) {
	recoverContext();

	if (!_open)
		return;

	if (software565) {
		if (softwarePitch == 640 * 2) {
			_remasteredLookDevice->updateTexture(_softwareTexture, software565, 640, 480);
		} else {
			Common::Array<byte> packed;

			packed.resize(640 * 480 * 2);

			for (int row = 0; row < 480; row++)
				memcpy(&packed[row * 640 * 2], software565 + row * softwarePitch, 640 * 2);

			_remasteredLookDevice->updateTexture(_softwareTexture, &packed[0], 640, 480);
		}
	}

	if (overlayRgba && overlayChanged)
		_remasteredLookDevice->updateTexture(_overlayTexture, overlayRgba, _hostWidth, _hostHeight);

	if (_deferredEnabled)
		_remasteredLookPipeline->renderScene();

	_remasteredLookPipeline->present(_softwareTexture, _softwareAlpha, _deferredEnabled, _softwareEnabled, _overlayTexture, _frontBufferVisible,
			_widescreen, _resolutionScaling);

	bool widescreen = (int)_registry->getFloat("Widescreen", 0.0f) == 2;
	float scaling = _registry->getFloat("ResolutionScaling", 2.0f);

	if (widescreen != _widescreen || (float)(uint)scaling != (float)(uint)_resolutionScaling) {
		_widescreen = widescreen;
		_resolutionScaling = (float)(uint)scaling;
		_graphicsChanged = true;
	}
}

uint RemasteredLookRenderer::getCharacterLightCount() const {
	return _setx.getLightCount() + _extraCount;
}

uint RemasteredLookRenderer::getShadowCharacterLightCount() const {
	return _setx.getShadowLightCount();
}

void RemasteredLookRenderer::fillMatrix(const float *position, const float *rotation, const float *scale, float matrix[12]) const {
	RdMatrix34 rotationMatrix;
	float angles[3] = { rotation[0], rotation[1], rotation[2] };

	_rd->buildRotate34(rotationMatrix, angles);

	for (int i = 0; i < 3; i++) {
		matrix[i] = rotationMatrix[0][i] * scale[0];
		matrix[3 + i] = rotationMatrix[1][i] * scale[1];
		matrix[6 + i] = rotationMatrix[2][i] * scale[2];
		matrix[9 + i] = position[i];
	}
}

bool RemasteredLookRenderer::getCharacterLightInfo(uint index, int type, LookLightInfo &info) const {
	uint fileCount = _setx.getLightCount();
	const CharacterLight *light = nullptr;

	if (index < fileCount)
		light = &_setx.getLight(index);
	else if (index - fileCount < _extraCount)
		light = &_extraLights[index - fileCount];

	if (!light || light->type != type || !light->isVisible(_lightLodType))
		return false;

	for (int i = 0; i < 3; i++)
		info.position[i] = light->position[i];

	fillMatrix(light->position, light->rotation, light->scale, info.matrix);
	light->getInnerColor(info.inner);
	light->getOuterColor(info.outer);
	info.shadingBias = light->shadingBias;
	info.shadingLimit = light->shadingLimit;
	info.innerRadius = light->innerRadius;
	info.falloffPower = light->falloffPower;
	info.verticalInner = light->verticalInnerRadius;
	info.verticalPower = light->verticalFalloffPower;
	info.shadowMode = light->shadowMode;
	info.shadowQuality = light->shadowQuality;
	info.shadowBias = light->getTypeShadowBias();
	info.shadowSoftness = light->shadowSoftness;
	info.sizeX = light->scale[0] * 2.0f;
	info.sizeY = light->scale[1] * 2.0f;
	info.sizeZ = light->scale[2] * 2.0f;
	info.nearDistance = light->shadowNear;
	info.maxDistance = light->shadowMaxDistance;
	info.keylight = light->keylightState;

	return light->hasIntensity();
}

bool RemasteredLookRenderer::getCutsceneLightInfo(uint index, LookCutsceneLight &light) const {
	(void)index;
	(void)light;

	return false;
}

uint RemasteredLookRenderer::getShadowCasterCount() const {
	return _setx.getShadowCasterCount();
}

bool RemasteredLookRenderer::getShadowCasterInfo(uint index, float matrix[12]) const {
	const ShadowCaster &caster = _setx.getShadowCaster(index);

	if (caster.type != 0)
		return false;

	fillMatrix(caster.position, caster.currentRotation, caster.scale, matrix);

	return true;
}

uint RemasteredLookRenderer::getZShapeCount() const {
	return _setx.getZShapeCount();
}

bool RemasteredLookRenderer::getZShapeInfo(uint index, int type, int drawPass, float matrix[12], int &tessellation) const {
	const ZShape &shape = _setx.getZShape(index);
	Common::String setup;

	getCurrentSetupName(setup);

	if (shape.type != type || shape.drawPass != drawPass || !shape.matchesSetup(setup.c_str()))
		return false;

	fillMatrix(shape.position, shape.rotation, shape.scale, matrix);
	tessellation = shape.planeTessellation;

	return true;
}

bool RemasteredLookRenderer::getCompositor(LookCompositorInfo &info) const {
	RdMatrix34 camera;

	_rd->_camera.getMatrix(camera);

	const LightCompositor *compositor = _setx.findClosestCompositor(camera[3]);

	if (!compositor)
		return false;

	for (int i = 0; i < 3; i++) {
		info.white[i] = compositor->shadowWhite[i];
		info.black[i] = compositor->shadowBlack[i];
	}

	info.minLuminance = compositor->minLuminance;
	info.maxLuminance = compositor->maxLuminance;
	info.power = compositor->power;
	info.aoStrength = compositor->aoStrength;

	return true;
}

uint RemasteredLookRenderer::getActorAoCount() const {
	uint count = 0;
	SputActor *actors = _actors->getActors();

	for (int i = 0; i < _actors->getMaxActors(); i++) {
		if (actors[i].inUse && (actors[i].flags & 0x20))
			count++;
	}

	return count;
}

bool RemasteredLookRenderer::getActorAoInfo(uint index, float matrix[12]) const {
	SputActor *actors = _actors->getActors();
	uint seen = 0;

	for (int i = 0; i < _actors->getMaxActors(); i++) {
		if (!actors[i].inUse || !(actors[i].flags & 0x20))
			continue;

		if (seen++ != index)
			continue;

		RdMatrix34 rotationMatrix;
		RdVector3 angles = { actors[i].actorRot[0], actors[i].actorRot[1], actors[i].actorRot[2] };
		float scale = actors[i].scale * 0.34999999f;

		_rd->buildRotate34(rotationMatrix, angles);

		for (int row = 0; row < 3; row++) {
			for (int column = 0; column < 3; column++)
				matrix[row * 3 + column] = scale * rotationMatrix[row][column];
		}

		matrix[9] = actors[i].actorPos[0];
		matrix[10] = actors[i].actorPos[1];
		matrix[11] = actors[i].actorPos[2];

		return true;
	}

	return false;
}

void RemasteredLookRenderer::getRenderThingSphere(uint index, float center[3], float &radius) const {
	(void)index;
	center[0] = center[1] = center[2] = 0.0f;
	radius = 0.0f;
}

void RemasteredLookRenderer::getIris(float &size, float &centerX, float &centerY) const {
	_render->getIrisParameters(&size, &centerX, &centerY);
}

float RemasteredLookRenderer::getGamma() const {
	return _render->getGamma();
}

void RemasteredLookRenderer::getCamera(LookCamera &camera) const {
	RdCamera *current = _rd->_camera.getCurrent();
	RdMatrix34 inverse;

	if (!current)
		return;

	memcpy(camera.view, current->viewMatrix, sizeof(camera.view));
	_rd->_camera.getMatrix(inverse);
	memcpy(camera.invView, inverse, sizeof(camera.invView));
	camera.viewDistance = current->focalLen;
}

uint RemasteredLookRenderer::getAdvancedLighting() const {
	return _render->getAdvancedLighting();
}

const void *RemasteredLookRenderer::getCameraIdentity() const {
	return _rd->_camera.getCurrent();
}

void RemasteredLookRenderer::resetCharacterLightKeylightStates() {
	Common::String setup;

	getCurrentSetupName(setup);
	_setx.resetKeylightStates(setup.c_str());

	for (uint i = 0; i < _extraCount; i++)
		_extraLights[i].resetKeylightState(setup.c_str());
}

void RemasteredLookRenderer::setCharacterLightKeylightState(uint index, int value) {
	uint fileCount = _setx.getLightCount();

	if (index < fileCount)
		_setx.getLight(index).keylightState = value;
	else if (index - fileCount < _extraCount)
		_extraLights[index - fileCount].keylightState = value;
}

} // End of namespace Grim
