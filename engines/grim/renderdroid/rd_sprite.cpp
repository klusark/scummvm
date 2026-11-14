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

#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

RdSprite *RenderDroid::SpriteModule::create(int type, char *name, char *matFilename, float width, float height, int geometryMode, int lightingMode, int textureMode, float extraLight, RdVector3 *offset) {
	RdSprite *sprite = new RdSprite;
	memset(sprite, 0, sizeof(RdSprite));

	newEntry(sprite, name, type, matFilename, width, height, geometryMode, lightingMode, textureMode, extraLight, offset);

	return sprite;
}

int RenderDroid::SpriteModule::newEntry(RdSprite *sprite, char *name, int type, char *matFilename, float width, float height, int geometryMode, int lightingMode, int textureMode, float extraLight, RdVector3 *offset) {
	if (name)
		Common::strlcpy(sprite->name, name, 32);

	sprite->width = width;
	sprite->type = type;
	sprite->height = height;
	sprite->offset[0] = (*offset)[0];
	sprite->offset[1] = (*offset)[1];
	sprite->offset[2] = (*offset)[2];
	sprite->face.lightingMode = lightingMode;
	sprite->face.geometryMode = geometryMode;
	sprite->face.type = 1;
	sprite->face.textureMode = textureMode;
	sprite->face.extraLight = extraLight;

	sprite->face.material = _rd->_material.load(matFilename, 0, 0);

	if (!sprite->face.material)
		return 0;

	sprite->face.numVertices = 4;
	sprite->face.vertices = new int[4];

	if (sprite->face.geometryMode <= 3) {
		for (uint i = 0; i < sprite->face.numVertices; i++)
			sprite->face.vertices[i] = i;
	} else {
		sprite->face.texVertices = new int[sprite->face.numVertices];

		for (uint i = 0; i < sprite->face.numVertices; i++) {
			sprite->face.vertices[i] = i;
			sprite->face.texVertices[i] = i;
		}

		if (!sprite->face.material->cels[0] || !sprite->face.material->cels[0]->texture || !sprite->face.material->cels[0]->texture->mips[0]) {
			warning("RenderDroid::SpriteModule::newEntry(): material %s has no texture", sprite->face.material->name);
			return 0;
		}

		sprite->texVerts = new float[2 * sprite->face.numVertices];

		StdVBuffer *texture = sprite->face.material->cels[0]->texture->mips[0];
		double texHeight = (double)(uint)texture->tex.height;
		double texWidth = (double)(uint)texture->tex.width;

		sprite->texVerts[0] = 0.5f;
		sprite->texVerts[1] = (float)(texHeight - 0.5);
		sprite->texVerts[2] = (float)(texWidth - 0.5);
		sprite->texVerts[3] = (float)(texHeight - 0.5);
		sprite->texVerts[4] = (float)(texWidth - 0.5);
		sprite->texVerts[5] = 0.5f;
		sprite->texVerts[6] = 0.5f;
		sprite->texVerts[7] = 0.5f;
	}

	sprite->halfWidth = (float)(sprite->width * 0.5);
	sprite->halfHeight = (float)(sprite->height * 0.5);
	sprite->face.uvDelta[0] = 0.0f;
	sprite->face.uvDelta[1] = 0.0f;
	sprite->radius = (float)sqrt((double)sprite->halfWidth * sprite->halfWidth + (double)sprite->halfHeight * sprite->halfHeight);

	return 1;
}

int RenderDroid::SpriteModule::draw(RdThing *thing, RdMatrix34 mat) {
	RdSprite *sprite = thing->data.sprite;
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 center;
	RdVector3 offset;

	_rd->transformPoint34(center, mat[3], camera->viewMatrix);
	_rd->transformVector34(offset, sprite->offset, mat);
	_rd->transformVector34Acc(offset, camera->viewMatrix);

	float halfWidth;

	if (_rd->_remastered ? !(offset[0] < 0.0) : (offset[0] >= 0.0))
		halfWidth = -sprite->halfWidth;
	else
		halfWidth = sprite->halfWidth;

	int frustrumCull;

	if (_rd->_curCullFlags & 2)
		frustrumCull = _rd->_clip.sphereInFrustrum(camera->clipFrustum, center, sprite->radius);
	else
		frustrumCull = thing->frustrumCull;

	if (frustrumCull == 2)
		return 0;

	RdProcFace *procFace = _rd->_cache.getProcEntry();

	if (!procFace)
		return 0;

	float left = (float)((double)offset[0] - halfWidth);
	float right = (float)((double)halfWidth + offset[0]);
	float y = (float)((double)offset[1] + center[1]);
	float bottom = (float)((double)offset[2] + center[2]);
	float top = (float)(((double)center[2] + sprite->halfHeight + sprite->halfHeight) + offset[2]);
	float topSecond = (float)((double)center[2] + (((double)sprite->halfHeight + sprite->halfHeight) + offset[2]));

	if (_rd->_remastered) {
		float heightPart = (float)((double)sprite->halfHeight * 2.0 + offset[2]);

		top = (float)((double)center[2] + heightPart);
		topSecond = top;
	}

	_spriteVerts[0] = (float)((double)center[0] + left);
	_spriteVerts[1] = y;
	_spriteVerts[2] = bottom;
	_spriteVerts[3] = (float)((double)center[0] + right);
	_spriteVerts[4] = y;
	_spriteVerts[5] = bottom;
	_spriteVerts[6] = (float)((double)center[0] + right);
	_spriteVerts[7] = y;
	_spriteVerts[8] = top;
	_spriteVerts[9] = (float)((double)center[0] + left);
	_spriteVerts[10] = y;
	_spriteVerts[11] = topSecond;

	RdVertexLists src;
	RdVertexLists dst;

	memset(&src, 0, sizeof(RdVertexLists));
	memset(&dst, 0, sizeof(RdVertexLists));

	src.numVertices = sprite->face.numVertices;
	src.vertIndices = sprite->face.vertices;
	src.texIndices = sprite->face.texVertices;
	src.vertices = (RdVector3 *)_spriteVerts;
	src.intensities = nullptr;
	src.texVertices = (RdVector2 *)sprite->texVerts;
	src.addedIntensities = nullptr;

	dst.vertices = _spriteWork;
	dst.screen = procFace->vertices;
	dst.texVertices = procFace->texVertices;
	dst.intensities = procFace->intensities;

	int geometryMode = _rd->_curGeometryMode;

	if (geometryMode >= sprite->face.geometryMode)
		geometryMode = sprite->face.geometryMode;

	if (geometryMode >= thing->curGeometryMode)
		geometryMode = thing->curGeometryMode;

	procFace->geometryMode = geometryMode;

	if ((_rd->_curRenderOptions & 2) && camera->ambientLight >= 1.0f) {
		procFace->lightingMode = 0;
	} else {
		int lightingMode = _rd->_curLightingMode;

		if (lightingMode >= sprite->face.lightingMode)
			lightingMode = sprite->face.lightingMode;

		if (lightingMode >= thing->curLightingMode)
			lightingMode = thing->curLightingMode;

		procFace->lightingMode = lightingMode;
	}

	int textureMode = _rd->_curTextureMode;

	if (textureMode >= sprite->face.textureMode)
		textureMode = sprite->face.textureMode;

	if (textureMode >= thing->curTextureMode)
		textureMode = thing->curTextureMode;

	procFace->textureMode = textureMode;

	_rd->_primit3.clipFace(camera->clipFrustum, geometryMode, procFace->lightingMode, textureMode, &src, &dst, sprite->face.uvDelta);

	if (dst.numVertices < 3)
		return 0;

	(_rd->_camera.*camera->projectLst)(dst.screen, dst.vertices, dst.numVertices);

	if (_rd->_curRenderOptions & 2)
		procFace->ambientLight = camera->ambientLight;
	else
		procFace->ambientLight = 0.0f;

	RdColormap *curMap = _rd->_colormap.getCurrent();
	RdColormap *identityMap = _rd->_colormap.getIdentity();
	int lightingMode = procFace->lightingMode;

	if (lightingMode) {
		if (_rd->_remastered ? !(procFace->ambientLight >= 1.0f) : (procFace->ambientLight < 1.0f)) {
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
		} else {
			procFace->lightingMode = curMap == identityMap ? 0 : 2;
		}
	}

	char bufferFlags = 1;

	if (procFace->geometryMode >= 4)
		bufferFlags = 3;

	if (procFace->lightingMode >= 3)
		bufferFlags |= 4;

	procFace->curCel = thing->curCel;
	procFace->backface = halfWidth < 0.0f;
	procFace->type = sprite->face.type;
	procFace->extraLight = sprite->face.extraLight;
	procFace->material = sprite->face.material;
	_rd->_cache.addProcFace(0, dst.numVertices, bufferFlags);

	return 1;
}

void RenderDroid::SpriteModule::destroy(RdSprite *&sprite) {
	if (sprite) {
		freeEntry(sprite);
		delete sprite;
		sprite = nullptr;
	}
}

void RenderDroid::SpriteModule::freeEntry(RdSprite *sprite) {
	if (sprite->texVerts) {
		delete[] sprite->texVerts;
		sprite->texVerts = nullptr;
	}

	if (sprite->face.vertices) {
		delete[] sprite->face.vertices;
		sprite->face.vertices = nullptr;
	}

	if (sprite->face.texVertices) {
		delete[] sprite->face.texVertices;
		sprite->face.texVertices = nullptr;
	}
}

} // End of namespace Grim
