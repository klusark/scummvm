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

#include "common/algorithm.h"
#include "common/textconsole.h"

#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"
#include "engines/grim/std/std_sort.h"

namespace Grim {

RenderDroid::CacheModule::CacheModule(RenderDroid *rd) : Module(rd), _numProcFaces(0), _numUsedVertices(0), _numUsedTexVertices(0), _numUsedIntensities(0), _drawnFaces(0),
		_totalNormalTris(0), _totalSolidTris(0), _totalVerts(0) {
	memset(_viewportRect, 0, sizeof(_viewportRect));
	_hwVertices = new Std3DVertex[32768]();
	_hwNormalTris = new Std3DTri[1024]();
	_hwSolidTris = new Std3DTri[1024]();
}

RenderDroid::CacheModule::~CacheModule() {
	delete[] _hwVertices;
	delete[] _hwNormalTris;
	delete[] _hwSolidTris;
}

int RenderDroid::CacheModule::startup() {
	return 1;
}

int RenderDroid::CacheModule::finishFrame() {
	int result = _rd->_curAcceleration;

#if defined(GRIM_STD3D)
	if (result > 0)
		return _rd->_std3D->endScene();
#endif

	return result;
}

int RenderDroid::CacheModule::reset() {
	_numProcFaces = 0;
	_numUsedVertices = 0;
	_numUsedTexVertices = 0;
	_numUsedIntensities = 0;

	// Only the demo resets the viewport rectangle with the cache
	if (_rd->isDemo()) {
		_viewportRect[0] = 0x7FFFFFFF;
		_viewportRect[1] = 0x7FFFFFFF;
		_viewportRect[2] = 0;
		_viewportRect[3] = 0;
	}

	return 0;
}

void RenderDroid::CacheModule::clearFrameCounters() {
	_drawnFaces = 0;
}

RdProcFace *RenderDroid::CacheModule::getProcEntry() {
	size_t numProcFaces = _numProcFaces;
	size_t maxProcFaces = _rd->_remastered ? 0x2000 : 0x400;
	uint maxEntries = _rd->_remastered ? 0x40000 : 0x8000;

	if (numProcFaces >= maxProcFaces) {
		flush();
		numProcFaces = _numProcFaces;
	}

	if (maxEntries - _numUsedVertices < 0x20)
		return nullptr;

	if (maxEntries - _numUsedTexVertices < 0x20)
		return nullptr;

	if (maxEntries - _numUsedIntensities < 0x20)
		return nullptr;

	RdProcFace *procFace = &_procFaces[numProcFaces];

	procFace->vertices = (RdVector3 *)&_vertices[3 * _numUsedVertices];
	procFace->texVertices = (RdVector2 *)&_texVertices[2 * _numUsedTexVertices];
	procFace->intensities = &_intensities[_numUsedIntensities];
	procFace->redIntensities = &_intensities[_numUsedIntensities];
	procFace->greenIntensities = &_greenIntensities[_numUsedIntensities];
	procFace->blueIntensities = &_blueIntensities[_numUsedIntensities];

	return procFace;
}

int RenderDroid::CacheModule::flush() {
	int result = (int)_numProcFaces;

	if (!_numProcFaces)
		return result;

	if (_rd->_curSortingMethod == 2) {
		sort(_procFaces, _numProcFaces, sizeof(RdProcFace), procFaceCompare);
	}

	if (_rd->_curAcceleration <= 0) {
		StdVBuffer *lockedBuffer = nullptr;
		RdCamera *camera = _rd->_camera.getCurrent();

		if (camera && camera->canvas && camera->canvas->vBuffer && camera->canvas->vBuffer->type == 1) {
			if (_rd->_stdDisplay->vBufferLock(camera->canvas->vBuffer))
				lockedBuffer = camera->canvas->vBuffer;
		}

		if (_rd->_curOcclusionMethod) {
			if (_rd->_curOcclusionMethod == 1) {
				_rd->_active.advanceFrame();
				_rd->_active.drawScene();
			}
		} else if (_rd->_curZBufferMethod) {
			if (_rd->_curZBufferMethod == 2) {
				for (size_t i = 0; i < _numProcFaces; i++) {
					if (_procFaces[i].flags & 1)
						drawFaceUser(&_procFaces[i]);
					else
						drawFaceZ(&_procFaces[i]);
				}
			}
		} else {
			for (size_t i = 0; i < _numProcFaces; i++) {
				if (_procFaces[i].flags & 1)
					drawFaceUser(&_procFaces[i]);
				else
					drawFaceN(&_procFaces[i]);
			}
		}

		if (lockedBuffer)
			_rd->_stdDisplay->vBufferUnlock(lockedBuffer);
	} else {
#if defined(GRIM_STD3D)
		sendFaceListToHardware();
#endif
	}

	_drawnFaces += (uint)_numProcFaces;

	return reset();
}

int RenderDroid::CacheModule::addProcFace(uint flags, uint numVertices, char bufferFlags) {
	if (_numProcFaces >= (_rd->_remastered ? 0x2000 : 0x400))
		return 0;

	RdProcFace *procFace = &_procFaces[_numProcFaces];
	const float *verts = (const float *)procFace->vertices;

	procFace->flags = flags;
	procFace->numVertices = numVertices;
	procFace->userData = _rd->_curProcFaceUserData;

	float xMax = verts[0];
	float xMin = xMax;
	double yMin = verts[1];
	float yMax = verts[1];

	procFace->yMaxIdx = 0;
	procFace->yMinIdx = 0;
	procFace->zMax = verts[2];
	procFace->zMin = verts[2];

	for (uint i = 1; i < numVertices; i++) {
		float x = verts[3 * i];
		float y = verts[3 * i + 1];
		float z = verts[3 * i + 2];

		if (x >= xMin) {
			if (x > xMax)
				xMax = x;
		} else {
			xMin = x;
		}

		if (y >= yMin) {
			if (y > (double)yMax) {
				procFace->yMaxIdx = i;
				yMax = y;
			}
		} else {
			yMin = y;
			procFace->yMinIdx = i;
		}

		if (z >= (double)procFace->zMin) {
			if (z > (double)procFace->zMax)
				procFace->zMax = z;
		} else {
			procFace->zMin = z;
		}
	}

	if (xMin >= (double)xMax)
		return 0;

	if (yMin >= yMax)
		return 0;

	procFace->xMin = (int)(float)ceil((double)xMin);
	procFace->xMax = (int)(float)ceil((double)xMax);
	procFace->yMin = (int)(float)ceil((double)(float)yMin);
	procFace->yMax = (int)(float)ceil((double)yMax);

	if (bufferFlags & 1)
		_numUsedVertices += numVertices;

	if (bufferFlags & 2)
		_numUsedTexVertices += numVertices;

	if (bufferFlags & 4)
		_numUsedIntensities += numVertices;

	procFace->colormap = _rd->_colormap.getCurrent();
	_numProcFaces++;

	if ((uint)procFace->xMin < (uint)_viewportRect[0])
		_viewportRect[0] = procFace->xMin;

	if ((uint)procFace->xMax > (uint)_viewportRect[2])
		_viewportRect[2] = procFace->xMax;

	if ((uint)procFace->yMin < (uint)_viewportRect[1])
		_viewportRect[1] = procFace->yMin;

	if ((uint)procFace->yMax > (uint)_viewportRect[3])
		_viewportRect[3] = procFace->yMax;

	return 1;
}

int RenderDroid::CacheModule::procFaceCompare(const void *a, const void *b) {
	const RdProcFace *faceA = (const RdProcFace *)a;
	const RdProcFace *faceB = (const RdProcFace *)b;

	if (faceA->zMin == faceB->zMin)
		return 0;

	if (faceA->zMin >= (double)faceB->zMin)
		return -1;

	return 1;
}

RdMatCel *RenderDroid::CacheModule::selectCel(const RdProcFace *procFace) {
	RdMaterial *material = procFace->material;

	if (!material)
		return nullptr;

	if (!material->numCels) {
		warning("RenderDroid::CacheModule::selectCel(): material %s has no cels", material->name);
		return nullptr;
	}

	int curCel = procFace->curCel;

	if (curCel == -1)
		curCel = material->curCel;

	if (curCel < 0)
		curCel = 0;
	else if (curCel > (int)(material->numCels - 1))
		curCel = material->numCels - 1;

	return material->cels[curCel];
}

float RenderDroid::CacheModule::calcFaceLight(const RdProcFace *procFace, int lightingMode, float ambientLight) {
	if (_rd->_remastered) {
		double sum = procFace->extraLight;

		if (lightingMode == 2)
			sum = (double)procFace->extraLight + procFace->diffuseLight;

		double remasteredClamped = 0.0;

		if (sum >= 0.0)
			remasteredClamped = sum > 1.0 ? 1.0 : sum;

		if (!(remasteredClamped > ambientLight))
			remasteredClamped = ambientLight;

		return (float)(remasteredClamped * 63.0);
	}

	float light = procFace->extraLight;

	if (lightingMode == 2)
		light = (float)((double)procFace->extraLight + procFace->diffuseLight);

	double clamped = light;

	if (light < 0.0)
		clamped = 0.0;
	else if (light > 1.0)
		clamped = 1.0;

	if (clamped <= ambientLight)
		clamped = ambientLight;

	return (float)(clamped * 63.0);
}

void RenderDroid::CacheModule::calcVertexLights(RdProcFace *procFace, float ambientLight) {
	if (_rd->_remastered) {
		for (uint i = 0; i < procFace->numVertices; i++) {
			double sum = (double)procFace->extraLight + procFace->intensities[i];
			double remasteredClamped = 0.0;

			if (sum >= 0.0)
				remasteredClamped = sum > 1.0 ? 1.0 : sum;

			if (!(remasteredClamped > ambientLight))
				remasteredClamped = ambientLight;

			procFace->intensities[i] = (float)(remasteredClamped * 63.0);
		}

		return;
	}

	for (uint i = 0; i < procFace->numVertices; i++) {
		float light = (float)((double)procFace->extraLight + procFace->intensities[i]);
		double clamped = light;

		if (light < 0.0)
			clamped = 0.0;
		else if (light > 1.0)
			clamped = 1.0;

		if (clamped <= ambientLight)
			clamped = ambientLight;

		procFace->intensities[i] = (float)(clamped * 63.0);
	}
}

int RenderDroid::CacheModule::drawFaceUser(RdProcFace *procFace) {
	int geometryMode = procFace->geometryMode;

	if (geometryMode >= _rd->_curGeometryMode)
		geometryMode = _rd->_curGeometryMode;

	int lightingMode = procFace->lightingMode;

	if (lightingMode >= _rd->_curLightingMode)
		lightingMode = _rd->_curLightingMode;

	int textureMode = procFace->textureMode;

	if (textureMode >= _rd->_curTextureMode)
		textureMode = _rd->_curTextureMode;

	if (!geometryMode)
		return 0;

	RdMatCel *cel = selectCel(procFace);

	if (cel && geometryMode == 4 && !(cel->flags & 8))
		geometryMode = 3;

	float ambientLight = 0.0f;

	if (_rd->_curRenderOptions & 2)
		ambientLight = procFace->ambientLight;

	float light;

	switch (geometryMode) {
	case 1:
	case 2:
		if ((uint)lightingMode <= 4)
			procFace->userDraw(procFace, cel, 0.0f);
		break;
	case 3:
	case 4:
		// The demo draws textured faces only with the perspective mode
		if (geometryMode == 4 && ((_rd->isDemo() && textureMode != 1) || (textureMode != 0 && textureMode != 1)))
			break;

		switch (lightingMode) {
		case 0:
			procFace->userDraw(procFace, cel, 0.0f);
			break;
		case 1:
		case 2:
			light = calcFaceLight(procFace, lightingMode, ambientLight);
			procFace->diffuseLight = light;
			procFace->userDraw(procFace, cel, light);
			break;
		case 3:
			calcVertexLights(procFace, ambientLight);
			procFace->userDraw(procFace, cel, 0.0f);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	// The demo draws the wireframe over the face when the option is set
	if (_rd->isDemo() && (_rd->_curRenderOptions & 0x40))
		_rd->_nRaster.drawNGonLW(procFace, cel);

	return 1;
}

int RenderDroid::CacheModule::drawFaceN(RdProcFace *procFace) {
	int geometryMode = procFace->geometryMode;

	if (geometryMode >= _rd->_curGeometryMode)
		geometryMode = _rd->_curGeometryMode;

	int lightingMode = procFace->lightingMode;

	if (lightingMode >= _rd->_curLightingMode)
		lightingMode = _rd->_curLightingMode;

	int textureMode = procFace->textureMode;

	if (textureMode >= _rd->_curTextureMode)
		textureMode = _rd->_curTextureMode;

	if (!geometryMode)
		return 0;

	RdMatCel *cel = selectCel(procFace);

	if (cel && geometryMode == 4 && !(cel->flags & 8))
		geometryMode = 3;

	if (!cel) {
		warning("RenderDroid::CacheModule::drawFaceN(): face without a material cel");
		return 1;
	}

	float ambientLight = 0.0f;

	if (_rd->_curRenderOptions & 2)
		ambientLight = procFace->ambientLight;

	float light;

	switch (geometryMode) {
	case 1:
		if ((uint)lightingMode <= 4)
			_rd->_nRaster.drawNGonLV(procFace, cel);
		break;
	case 2:
		if ((uint)lightingMode <= 4)
			_rd->_nRaster.drawNGonLW(procFace, cel);
		break;
	case 3:
		switch (lightingMode) {
		case 0:
			if (procFace->type & 2)
				_rd->_nRaster.drawNGonTLS(procFace, cel);
			else
				_rd->_nRaster.drawNGonLS(procFace, cel);
			break;
		case 1:
		case 2:
			light = calcFaceLight(procFace, lightingMode, ambientLight);
			procFace->diffuseLight = light;

			if (procFace->type & 2)
				_rd->_nRaster.drawNGonTFS(procFace, cel, light);
			else
				_rd->_nRaster.drawNGonFS(procFace, cel, light);
			break;
		case 3:
			calcVertexLights(procFace, ambientLight);

			if (procFace->type & 2)
				_rd->_nRaster.drawNGonTGS(procFace, cel);
			else
				_rd->_nRaster.drawNGonGS(procFace, cel);
			break;
		default:
			break;
		}
		break;
	case 4:
		if (textureMode == 0) {
			switch (lightingMode) {
			case 0:
				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonMTLAT(procFace, cel);
					else
						_rd->_nRaster.drawNGonMLAT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonTLAT(procFace, cel);
					else
						_rd->_nRaster.drawNGonLAT(procFace, cel);
				}
				break;
			case 1:
			case 2:
				light = calcFaceLight(procFace, lightingMode, ambientLight);
				procFace->diffuseLight = light;

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonMTFAT(procFace, cel, light);
					else
						_rd->_nRaster.drawNGonMFAT(procFace, cel, light);
				} else {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonTFAT(procFace, cel, light);
					else
						_rd->_nRaster.drawNGonFAT(procFace, cel, light);
				}
				break;
			case 3:
				calcVertexLights(procFace, ambientLight);

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonMTGAT(procFace, cel);
					else
						_rd->_nRaster.drawNGonMGAT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonTGAT(procFace, cel);
					else
						_rd->_nRaster.drawNGonGAT(procFace, cel);
				}
				break;
			default:
				break;
			}
		} else if (textureMode == 1) {
			switch (lightingMode) {
			case 0:
				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonMTLIT(procFace, cel);
					else
						_rd->_nRaster.drawNGonMLIT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonTLIT(procFace, cel);
					else
						_rd->_nRaster.drawNGonLIT(procFace, cel);
				}
				break;
			case 1:
			case 2:
				light = calcFaceLight(procFace, lightingMode, ambientLight);
				procFace->diffuseLight = light;

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonMTFIT(procFace, cel, light);
					else
						_rd->_nRaster.drawNGonMFIT(procFace, cel, light);
				} else {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonTFIT(procFace, cel, light);
					else
						_rd->_nRaster.drawNGonFIT(procFace, cel, light);
				}
				break;
			case 3:
				calcVertexLights(procFace, ambientLight);

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonMTGIT(procFace, cel);
					else
						_rd->_nRaster.drawNGonMGIT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_nRaster.drawNGonTGIT(procFace, cel);
					else
						_rd->_nRaster.drawNGonGIT(procFace, cel);
				}
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	// The demo draws the wireframe over the face when the option is set
	if (_rd->isDemo() && (_rd->_curRenderOptions & 0x40))
		_rd->_nRaster.drawNGonLW(procFace, cel);

	return 1;
}

int RenderDroid::CacheModule::drawFaceZ(RdProcFace *procFace) {
	int geometryMode = procFace->geometryMode;

	if (geometryMode >= _rd->_curGeometryMode)
		geometryMode = _rd->_curGeometryMode;

	int lightingMode = procFace->lightingMode;

	if (lightingMode >= _rd->_curLightingMode)
		lightingMode = _rd->_curLightingMode;

	int textureMode = procFace->textureMode;

	if (textureMode >= _rd->_curTextureMode)
		textureMode = _rd->_curTextureMode;

	if (!geometryMode)
		return 0;

	RdMatCel *cel = selectCel(procFace);

	if (cel && geometryMode == 4 && !(cel->flags & 8))
		geometryMode = 3;

	if (!cel) {
		warning("RenderDroid::CacheModule::drawFaceZ(): face without a material cel");
		return 1;
	}

	float ambientLight = 0.0f;

	if (_rd->_curRenderOptions & 2)
		ambientLight = procFace->ambientLight;

	float light;

	switch (geometryMode) {
	case 1:
		if ((uint)lightingMode <= 4)
			_rd->_zRaster.drawNGonLV(procFace, cel);
		break;
	case 2:
		if ((uint)lightingMode <= 4)
			_rd->_zRaster.drawNGonLW(procFace, cel);
		break;
	case 3:
		switch (lightingMode) {
		case 0:
			if (procFace->type & 2)
				_rd->_zRaster.drawNGonTLS(procFace, cel);
			else
				_rd->_zRaster.drawNGonLS(procFace, cel);
			break;
		case 1:
			procFace->diffuseLight = 0.0f;

			if (procFace->type & 2)
				_rd->_zRaster.drawNGonTFS(procFace, cel, 0.0f);
			else
				_rd->_zRaster.drawNGonFS(procFace, cel, 0.0f);
			break;
		case 2:
			light = calcFaceLight(procFace, lightingMode, ambientLight);
			procFace->diffuseLight = light;

			if (procFace->type & 2)
				_rd->_zRaster.drawNGonTFS(procFace, cel, light);
			else
				_rd->_zRaster.drawNGonFS(procFace, cel, light);
			break;
		case 3:
			calcVertexLights(procFace, ambientLight);

			if (procFace->type & 2)
				_rd->_zRaster.drawNGonTGS(procFace, cel);
			else
				_rd->_zRaster.drawNGonGS(procFace, cel);
			break;
		default:
			break;
		}
		break;
	case 4:
		if (textureMode == 0) {
			switch (lightingMode) {
			case 0:
				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonMTLAT(procFace, cel);
					else
						_rd->_zRaster.drawNGonMLAT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonTLAT(procFace, cel);
					else
						_rd->_zRaster.drawNGonLAT(procFace, cel);
				}
				break;
			case 1:
			case 2:
				light = calcFaceLight(procFace, lightingMode, ambientLight);
				procFace->diffuseLight = light;

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonMTFAT(procFace, cel, light);
					else
						_rd->_zRaster.drawNGonMFAT(procFace, cel, light);
				} else {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonTFAT(procFace, cel, light);
					else
						_rd->_zRaster.drawNGonFAT(procFace, cel, light);
				}
				break;
			case 3:
				calcVertexLights(procFace, ambientLight);

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonMTGAT(procFace, cel);
					else
						_rd->_zRaster.drawNGonMGAT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonTGAT(procFace, cel);
					else
						_rd->_zRaster.drawNGonGAT(procFace, cel);
				}
				break;
			default:
				break;
			}
		} else if (textureMode == 1) {
			switch (lightingMode) {
			case 0:
				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonMTLIT(procFace, cel);
					else
						_rd->_zRaster.drawNGonMLIT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonTLIT(procFace, cel);
					else
						_rd->_zRaster.drawNGonLIT(procFace, cel);
				}
				break;
			case 1:
			case 2:
				light = calcFaceLight(procFace, lightingMode, ambientLight);
				procFace->diffuseLight = light;

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonMTFIT(procFace, cel, light);
					else
						_rd->_zRaster.drawNGonMFIT(procFace, cel, light);
				} else {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonTFIT(procFace, cel, light);
					else
						_rd->_zRaster.drawNGonFIT(procFace, cel, light);
				}
				break;
			case 3:
				calcVertexLights(procFace, ambientLight);

				if (cel->texture->flags & 1) {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonMTGIT(procFace, cel);
					else
						_rd->_zRaster.drawNGonMGIT(procFace, cel);
				} else {
					if (procFace->type & 2)
						_rd->_zRaster.drawNGonTGIT(procFace, cel);
					else
						_rd->_zRaster.drawNGonGIT(procFace, cel);
				}
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	// The demo draws the wireframe over the face when the option is set
	if (_rd->isDemo() && (_rd->_curRenderOptions & 0x40))
		_rd->_zRaster.drawNGonLW(procFace, cel);

	return 1;
}

} // End of namespace Grim
