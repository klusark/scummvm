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

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

int RenderDroid::Primit3Module::getScreenCoord(RdVector3 pt, int *screen) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 transformed;
	RdVector3 projected;

	_rd->transformPoint34(transformed, pt, camera->viewMatrix);
	if (!_rd->_clip.point3(camera->clipFrustum, transformed))
		return 0;

	(_rd->_camera.*camera->project)(projected, transformed);

	if (screen) {
		screen[0] = (int)projected[0];
		screen[1] = (int)projected[1];
		memcpy(&screen[2], &projected[2], sizeof(float));
	}

	return 1;
}

int RenderDroid::Primit3Module::drawLine(RdVector3 p0, RdVector3 p1, int *screen0, int *screen1, int *clipped0, int *clipped1, uint16 color, uint pattern) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 transformed0;
	RdVector3 transformed1;
	RdVector3 projected0;
	RdVector3 projected1;

	_rd->transformPoint34(transformed0, p0, camera->viewMatrix);
	_rd->transformPoint34(transformed1, p1, camera->viewMatrix);

	if (!_rd->_clip.line3(camera->clipFrustum, transformed0, transformed1, clipped0, clipped1))
		return 0;

	(_rd->_camera.*camera->project)(projected0, transformed0);
	(_rd->_camera.*camera->project)(projected1, transformed1);

	int x0 = (int)projected0[0];
	int y0 = (int)projected0[1];

	if (screen0) {
		screen0[0] = x0;
		screen0[1] = y0;
		memcpy(&screen0[2], &projected0[2], sizeof(float));
	}

	int x1 = (int)projected1[0];
	int y1 = (int)projected1[1];

	if (screen1) {
		screen1[0] = x1;
		screen1[1] = y1;
		memcpy(&screen1[2], &projected1[2], sizeof(float));
	}

	_rd->_primit2.drawLine(camera->canvas, x0, y0, x1, y1, color, pattern);

	return 1;
}

void RenderDroid::Primit3Module::drawPoint(RdVector3 pt, float radius, float angleStep, uint16 color, uint pattern) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 transformed;
	RdVector3 shifted;
	RdVector3 projected;
	RdVector3 projectedShifted;

	_rd->transformPoint34(transformed, pt, camera->viewMatrix);
	shifted[1] = transformed[1];
	shifted[2] = transformed[2];
	shifted[0] = (float)((double)transformed[0] + radius);

	if (transformed[1] > 0.0) {
		(_rd->_camera.*camera->project)(projected, transformed);
		(_rd->_camera.*camera->project)(projectedShifted, shifted);

		float projectedRadius = (float)((double)projectedShifted[0] - projected[0]);

		_rd->_primit2.drawCircle(camera->canvas, (int)((double)projected[0] + 0.5), (int)((double)projected[1] + 0.5), projectedRadius, angleStep, color, pattern);
	}
}

int RenderDroid::Primit3Module::noClipFace(int geometryMode, int lightingMode, int textureMode, RdVertexLists *src, RdVertexLists *dst, RdVector2 uvDelta) {
	bool copyTexVertices = false;
	bool copyIntensities = false;

	switch (geometryMode) {
	case 0:
	case 1:
	case 2:
		break;
	case 3:
		if (lightingMode >= 0 && lightingMode <= 2)
			break;

		if (lightingMode != 3)
			return lightingMode;

		copyIntensities = true;
		break;
	case 4:
		if (lightingMode < 0 || lightingMode > 3)
			return lightingMode;

		copyTexVertices = true;
		if (lightingMode == 3)
			copyIntensities = true;
		break;
	default:
		return geometryMode;
	}

	for (uint i = 0; i < src->numVertices; i++) {
		dst->vertices[i][0] = src->vertices[src->vertIndices[i]][0];
		dst->vertices[i][1] = src->vertices[src->vertIndices[i]][1];
		dst->vertices[i][2] = src->vertices[src->vertIndices[i]][2];

		if (copyTexVertices) {
			dst->texVertices[i][0] = src->texVertices[src->texIndices[i]][0];
			dst->texVertices[i][1] = src->texVertices[src->texIndices[i]][1];
			dst->texVertices[i][0] = (float)((double)uvDelta[0] + dst->texVertices[i][0]);
			dst->texVertices[i][1] = (float)((double)dst->texVertices[i][1] + uvDelta[1]);
		}

		if (copyIntensities) {
			double intensity = src->intensities[src->vertIndices[i]];

			if (src->addedIntensities)
				intensity = intensity + src->addedIntensities[i];

			if (_rd->_remastered ? !(intensity >= 0.0) : (intensity < 0.0))
				intensity = 0.0;
			else if (intensity > 1.0)
				intensity = 1.0;

			dst->intensities[i] = (float)intensity;
		}
	}

	dst->numVertices = src->numVertices;

	return src->numVertices;
}

int RenderDroid::Primit3Module::clipFace(RdClipFrustum *clip, int geometryMode, int lightingMode, int textureMode, RdVertexLists *src, RdVertexLists *dst, RdVector2 uvDelta) {
	bool perspective = _rd->_camera.getCurrent()->projectType == 1;
	bool textured;
	bool intensities;
	bool wire = false;
	int result;

	switch (geometryMode) {
	case 0:
		textured = false;
		intensities = false;
		break;
	case 1:
	case 2:
		textured = false;
		intensities = false;
		wire = true;
		break;
	case 3:
		if (lightingMode >= 0 && lightingMode <= 2) {
			textured = false;
			intensities = false;
		} else if (lightingMode == 3) {
			textured = false;
			intensities = true;
		} else {
			return lightingMode;
		}
		break;
	case 4:
		if (lightingMode >= 0 && lightingMode <= 2) {
			textured = true;
			intensities = false;
		} else if (lightingMode == 3) {
			textured = true;
			intensities = true;
		} else {
			return lightingMode;
		}
		break;
	default:
		return geometryMode;
	}

	for (uint i = 0; i < src->numVertices; i++) {
		const float *srcVertex = src->vertices[src->vertIndices[i]];

		dst->vertices[i][0] = srcVertex[0];
		dst->vertices[i][1] = srcVertex[1];
		dst->vertices[i][2] = srcVertex[2];

		if (textured) {
			const float *srcTexVertex = src->texVertices[src->texIndices[i]];

			dst->texVertices[i][0] = srcTexVertex[0];
			dst->texVertices[i][1] = srcTexVertex[1];
			dst->texVertices[i][0] = (float)((double)uvDelta[0] + dst->texVertices[i][0]);
			dst->texVertices[i][1] = (float)((double)dst->texVertices[i][1] + uvDelta[1]);
		}

		if (intensities) {
			double intensity = src->intensities[src->vertIndices[i]];

			if (src->addedIntensities)
				intensity = intensity + src->addedIntensities[i];

			if (_rd->_remastered ? !(intensity >= 0.0) : (intensity < 0.0))
				intensity = 0.0;
			else if (intensity > 1.0)
				intensity = 1.0;

			dst->intensities[i] = (float)intensity;
		}
	}

	if (wire) {
		if (perspective)
			result = _rd->_clip.face3W(clip, dst->vertices, src->numVertices);
		else
			result = _rd->_clip.face3WOrtho(clip, dst->vertices, src->numVertices);
	} else if (textured && intensities) {
		if (perspective)
			result = _rd->_clip.face3GT(clip, dst->vertices, dst->texVertices, dst->intensities, src->numVertices);
		else
			result = _rd->_clip.face3GTOrtho(clip, dst->vertices, dst->texVertices, dst->intensities, src->numVertices);
	} else if (textured) {
		if (perspective)
			result = _rd->_clip.face3T(clip, dst->vertices, dst->texVertices, src->numVertices);
		else
			result = _rd->_clip.face3TOrtho(clip, dst->vertices, dst->texVertices, src->numVertices);
	} else if (intensities) {
		if (perspective)
			result = _rd->_clip.face3GS(clip, dst->vertices, dst->intensities, src->numVertices);
		else
			result = _rd->_clip.face3GSOrtho(clip, dst->vertices, dst->intensities, src->numVertices);
	} else {
		if (perspective)
			result = _rd->_clip.face3S(clip, dst->vertices, src->numVertices);
		else
			result = _rd->_clip.face3SOrtho(clip, dst->vertices, src->numVertices);
	}

	dst->numVertices = result;

	return result;
}

RdProcFace *RenderDroid::Primit3Module::drawPolygon(RdVector3 *verts, int numVerts, RdMaterial *material) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 transformed[32];
	RdVector3 screen[32];
	float sumX = 0.0f;
	float sumY = 0.0f;
	float sumZ = 0.0f;

	if (numVerts > 32) {
		warning("RenderDroid::Primit3Module::drawPolygon(): too many vertices (%d)", numVerts);
		return nullptr;
	}

	for (int i = 0; i < numVerts; i++) {
		_rd->transformPoint34(transformed[i], verts[i], camera->viewMatrix);
		sumX = (float)((double)sumX + transformed[i][0]);
		sumY = (float)((double)transformed[i][1] + sumY);
		sumZ = (float)((double)transformed[i][2] + sumZ);
	}

	double count = (double)numVerts;
	float centerX = (float)(sumX / count);
	float centerY = (float)(sumY / count);
	float centerZ = (float)(sumZ / count);

	int numClipped = _rd->_clip.face3S(camera->clipFrustum, transformed, numVerts);

	if (numClipped < 3)
		return nullptr;

	(_rd->_camera.*camera->projectLst)(screen, transformed, numClipped);

	// The demo draws the polygon twice, as back face and as front face
	if (_demo) {
		RdProcFace *last = nullptr;

		for (int side = 1; side >= 0; side--) {
			RdProcFace *demoFace = _rd->_cache.getProcEntry();

			if (!demoFace) {
				warning("RenderDroid::Primit3Module::drawPolygon(): no free cache entry");
				return last;
			}

			demoFace->flags = 0;
			demoFace->type = 0;
			demoFace->diffuseLight = 0.0f;
			demoFace->extraLight = 0.0f;
			demoFace->geometryMode = 3;
			demoFace->lightingMode = 1;
			demoFace->numVertices = numClipped;
			memcpy(demoFace->vertices, screen, sizeof(RdVector3) * numClipped);
			demoFace->curCel = -1;
			demoFace->material = material;
			demoFace->backface = side;
			_rd->_cache.addProcFace(0, numClipped, 1);
			last = demoFace;
		}

		return last;
	}

	RdProcFace *procFace = _rd->_cache.getProcEntry();

	if (!procFace) {
		warning("RenderDroid::Primit3Module::drawPolygon(): no free cache entry");
		return nullptr;
	}

	procFace->flags = 0;
	procFace->diffuseLight = 0.0f;
	procFace->extraLight = 0.0f;
	procFace->curCel = 0;
	procFace->type = 1;
	procFace->lightingMode = 1;
	procFace->geometryMode = 3;
	procFace->numVertices = numClipped;
	memcpy(procFace->vertices, screen, sizeof(RdVector3) * numClipped);
	procFace->curCel = -1;
	procFace->material = material;

	RdVector3 edge0;
	RdVector3 edge1;
	RdVector3 normal;

	edge0[0] = transformed[1][0] - transformed[0][0];
	edge0[1] = transformed[1][1] - transformed[0][1];
	edge0[2] = transformed[1][2] - transformed[0][2];
	edge1[0] = transformed[2][0] - transformed[1][0];
	edge1[1] = transformed[2][1] - transformed[1][1];
	edge1[2] = transformed[2][2] - transformed[1][2];
	_rd->cross3(normal, edge0, edge1);

	if (_rd->_remastered) {
		float facing = (float)(((double)normal[0] * centerX + (double)normal[1] * centerY) + (double)normal[2] * centerZ);

		procFace->backface = facing > 0.0f;
	} else {
		procFace->backface = (double)normal[0] * centerX + (double)normal[1] * centerY + (double)normal[2] * centerZ > 0.0;
	}
	_rd->_cache.addProcFace(0, numClipped, 1);

	return procFace;
}

int RenderDroid::Primit3Module::drawPolylineFace(RdThing *thing, RdFace3 *face, RdVector3 *verts, RdVertexLists *src) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdProcFace *procFace = _rd->_cache.getProcEntry();

	if (!procFace)
		return 0;

	RdVertexLists dst;

	memset(&dst, 0, sizeof(RdVertexLists));
	dst.vertices = _work;
	dst.screen = procFace->vertices;
	dst.texVertices = procFace->texVertices;
	dst.intensities = procFace->intensities;

	src->numVertices = face->numVertices;
	src->vertIndices = face->vertices;
	src->texIndices = face->texVertices;

	int geometryMode = _rd->_curGeometryMode;

	if (geometryMode >= face->geometryMode)
		geometryMode = face->geometryMode;

	if (geometryMode >= thing->curGeometryMode)
		geometryMode = thing->curGeometryMode;

	procFace->geometryMode = geometryMode;

	if ((_rd->_curRenderOptions & 2) && camera->ambientLight >= 1.0f) {
		procFace->lightingMode = 0;
	} else {
		int lightingMode = _rd->_curLightingMode;

		if (lightingMode >= face->lightingMode)
			lightingMode = face->lightingMode;

		if (lightingMode >= thing->curLightingMode)
			lightingMode = thing->curLightingMode;

		procFace->lightingMode = lightingMode;
	}

	int textureMode = _rd->_curTextureMode;

	if (textureMode >= face->textureMode)
		textureMode = face->textureMode;

	if (textureMode >= thing->curTextureMode)
		textureMode = thing->curTextureMode;

	procFace->textureMode = textureMode;

	clipFace(camera->clipFrustum, geometryMode, procFace->lightingMode, textureMode, src, &dst, face->uvDelta);

	if (dst.numVertices < 3)
		return dst.numVertices;

	(_rd->_camera.*camera->projectLst)(dst.screen, dst.vertices, dst.numVertices);

	if (_rd->_curRenderOptions & 2)
		procFace->ambientLight = camera->ambientLight;
	else
		procFace->ambientLight = 0.0f;

	RdColormap *curMap = _rd->_colormap.getCurrent();
	RdColormap *identityMap = _rd->_colormap.getIdentity();
	int lightingMode = procFace->lightingMode;

	if (lightingMode) {
		if (procFace->ambientLight < 1.0f) {
			if (lightingMode == 2) {
				if (procFace->diffuseLight >= 1.0f && curMap == identityMap)
					procFace->lightingMode = 0;
				else if (procFace->diffuseLight <= 0.0f)
					procFace->lightingMode = 1;
			} else if (lightingMode == 3) {
				float first = procFace->intensities[0];
				uint same = 1;

				while (same < dst.numVertices && procFace->intensities[same] == first)
					same++;

				if (same == dst.numVertices) {
					if (first == 1.0f) {
						if (curMap == identityMap) {
							procFace->lightingMode = 0;
						} else {
							procFace->lightingMode = 2;
							procFace->diffuseLight = 1.0f;
						}
					} else if (first == 0.0f) {
						procFace->lightingMode = 1;
						procFace->diffuseLight = 0.0f;
					} else {
						procFace->lightingMode = 2;
						procFace->diffuseLight = first;
					}
				}
			}
		} else if (curMap == identityMap) {
			procFace->lightingMode = 0;
		} else {
			procFace->lightingMode = 2;
			procFace->diffuseLight = 1.0f;
		}
	}

	char bufferFlags = 1;

	if (procFace->geometryMode >= 4)
		bufferFlags = 3;

	if (procFace->lightingMode >= 3)
		bufferFlags |= 4;

	procFace->curCel = thing->curCel;
	procFace->backface = 0;
	procFace->type = face->type;
	procFace->extraLight = face->extraLight;
	procFace->material = face->material;

	return _rd->_cache.addProcFace(0, dst.numVertices, bufferFlags);
}

void RenderDroid::Primit3Module::clearFrameCounters() {
}

} // End of namespace Grim
