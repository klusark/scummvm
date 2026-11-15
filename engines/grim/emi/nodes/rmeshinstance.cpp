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

#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/graphics/cmaterial.h"
#include "engines/grim/emi/nodes/cweightmap.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/nodes/rmesh.h"
#include "engines/grim/emi/nodes/rmeshinstance.h"
#include "engines/grim/emi/nodes/rnode.h"

namespace Grim {

static const int kStaticLights = 32;
static const int kDynamicLights = 4;
static const int kStaticLightingMask = RNode::kDynamicLighting | RNode::kNoLighting;
static const int kBlendFlag = 0x10000;
static const float kAlphaScale = 255.0f;
static const int kAlphaShift = 24;
static const uint32 kAlphaMask = 0x00FFFFFF;

RMeshInstance::RMeshInstance(RMesh *meshPrimitive) : RPrimitiveInstance(meshPrimitive), _engine(meshPrimitive->getEngine()) {
	CDrawContext *ctx = _engine->getDrawContext();
	Common::Array<uint32> packedColors;
	const uint32 *colorData = nullptr;

	_vertexArray = nullptr;
	_ownsVertexBuf = false;
	_vertexBuf = nullptr;
	_boneNodes = nullptr;
	_staticLightingDone = false;
	_lastFrame = 0;

	if (meshPrimitive->_colors && meshPrimitive->_numVerts > 0) {
		packedColors.resize(meshPrimitive->_numVerts);

		for (int i = 0; i < meshPrimitive->_numVerts; i++)
			packedColors[i] = meshPrimitive->_colors[i].pack();

		colorData = &packedColors[0];
	}

	if (ctx)
		_vertexArray = ctx->createVertexArray(meshPrimitive->_numVerts, meshPrimitive->_pos, colorData, meshPrimitive->_uv);
	else
		warning("RMeshInstance::RMeshInstance(): no draw context");

	for (uint i = 0; i < meshPrimitive->_matHandles.size(); i++)
		_texIndex.push_back(0);

	if (!meshPrimitive->_weightMap) {
		_ownsVertexBuf = false;
		_vertexBuf = meshPrimitive->_normals;
	} else {
		int numVerts = _vertexArray ? _vertexArray->_numVerts : 0;

		_vertexBuf = (float *)_engine->getMemoryServices()->mallocImpl(numVerts * 3 * sizeof(float));

		if (_vertexBuf)
			memset(_vertexBuf, 0, numVerts * 3 * sizeof(float));

		_ownsVertexBuf = true;
	}
}

RMeshInstance::~RMeshInstance() {
	delete _vertexArray;
	_vertexArray = nullptr;

	if (_ownsVertexBuf && _vertexBuf) {
		_engine->getMemoryServices()->freeImpl(_vertexBuf);
		_vertexBuf = nullptr;
	}

	if (_boneNodes) {
		_engine->getMemoryServices()->freeImpl(_boneNodes);
		_boneNodes = nullptr;
	}

	_texIndex.clear();
}

void RMeshInstance::setMaterialTextureIndex(int materialIndex, int textureIndex) {
	if (materialIndex < 0 || materialIndex >= (int)_texIndex.size()) {
		warning("RMeshInstance::setMaterialTextureIndex(): invalid material index %d", materialIndex);

		return;
	}

	_texIndex[materialIndex] = textureIndex;
}

void RMeshInstance::update(RNode *node, Camera *camera) {
	_engine->_rmeshUpdateTimer.start();

	RMesh *mesh = static_cast<RMesh *>(_prim);
	CDrawContext *ctx = camera->getDrawContext();

	if (!ctx || !_vertexArray) {
		warning("RMeshInstance::update(): no draw context or vertex array");
		_engine->_rmeshUpdateTimer.stop();

		return;
	}

	int frameCounter = ctx->_frameCounter;
	CDCVertex *vertices = static_cast<CDCVertex *>(_vertexArray->lock());

	if (!_staticLightingDone) {
		if (mesh->_normals && (node->_drawFlags & kStaticLightingMask) == 0) {
			if ((node->_nodeFlags & CNode::kWorldValid) == 0)
				node->updateWorldTransform();

			mesh->lightVertices(&node->_world, _vertexArray->_numVerts, vertices, _vertexBuf, kStaticLights);
		}

		_staticLightingDone = true;
	}

	if (_lastFrame != frameCounter) {
		_lastFrame = frameCounter;

		if (mesh->_weightMap) {
			_engine->_rmeshComputeTransformsTimer.start();

			CTransform identity;

			identity._rot.setIdentity();
			mesh->_weightMap->computeTransforms(node, &identity);
			_engine->_rmeshComputeTransformsTimer.stop();

			VertexSource source;

			source.count = mesh->_numVerts;
			source.positions = mesh->_pos;
			source.normals = mesh->_normals;

			if (_boneNodes)
				mesh->_weightMap->deformVerts(&source, &vertices->x, _vertexBuf, _boneNodes);
			else
				warning("RMeshInstance::update(): the bone nodes are not bound");
		}

		if (mesh->_normals) {
			uint flags = node->_drawFlags;

			if ((flags & RNode::kNoLighting) == 0 && (flags & RNode::kDynamicLighting) != 0) {
				if ((node->_nodeFlags & CNode::kWorldValid) == 0)
					node->updateWorldTransform();

				if ((flags & RNode::kAmbientCube) == 0)
					mesh->lightVertices(&node->_world, _vertexArray->_numVerts, vertices, _vertexBuf, kDynamicLights);
				else
					mesh->lightVerticesAmbientCube(&node->_world, _vertexArray->_numVerts, vertices, _vertexBuf, kDynamicLights);
			}
		}

		if (_globalAlphaMode == kAlphaSet) {
			for (int i = 0; i < _vertexArray->_numVerts; i++) {
				uint32 alpha = (uint32)(int)((double)_globalAlpha * kAlphaScale) & 0xFF;

				vertices[i].color = (vertices[i].color & kAlphaMask) | (alpha << kAlphaShift);
			}
		} else if (_globalAlphaMode == kAlphaModulate) {
			for (int i = 0; i < _vertexArray->_numVerts; i++) {
				int scale = (int)((double)_globalAlpha * kAlphaScale);
				int vertexAlpha = vertices[i].color >> kAlphaShift;
				int product = (int)((uint32)scale * (uint32)vertexAlpha);
				uint32 alpha = (uint32)(product / 255) & 0xFF;

				vertices[i].color = (vertices[i].color & kAlphaMask) | (alpha << kAlphaShift);
			}
		}

		if (_matAlphaMode == kAlphaSet || _matAlphaMode == kAlphaModulate) {
			for (Common::HashMap<int, float>::const_iterator it = _matAlpha.begin(); it != _matAlpha.end(); ++it) {
				if (it->_key < 0 || it->_key >= _vertexArray->_numVerts) {
					warning("RMeshInstance::update(): invalid vertex index %d", it->_key);
					continue;
				}

				CDCVertex *vertex = &vertices[it->_key];
				int scale = (int)((double)it->_value * kAlphaScale);
				uint32 alpha;

				if (_matAlphaMode == kAlphaSet) {
					alpha = (uint32)scale & 0xFF;
				} else {
					int vertexAlpha = vertex->color >> kAlphaShift;
					int product = (int)((uint32)scale * (uint32)vertexAlpha);

					alpha = (uint32)(product / 255) & 0xFF;
				}

				vertex->color = (vertex->color & kAlphaMask) | (alpha << kAlphaShift);
			}
		}
	}

	_vertexArray->unlock();
	_engine->_rmeshUpdateTimer.stop();
}

void RMeshInstance::output(RNode *node, Camera *camera) {
	_engine->_rmeshOutputTimer.start();

	if (!_visible) {
		_engine->_rmeshOutputTimer.stop();

		return;
	}

	CDrawContext *ctx = camera->getDrawContext();
	RMesh *mesh = static_cast<RMesh *>(_prim);

	if (!ctx || !_vertexArray) {
		warning("RMeshInstance::output(): no draw context or vertex array");
		_engine->_rmeshOutputTimer.stop();

		return;
	}

	CTexture *texture = nullptr;

	if ((node->_nodeFlags & CNode::kWorldValid) == 0)
		node->updateWorldTransform();

	const float *v = camera->_view._rot._m;
	const CVector &nodePos = node->_world._pos;
	const CVector &viewPos = camera->_view._pos;

	double pendingZ = (double)v[2] * nodePos._x + (double)v[8] * nodePos._z + (double)v[5] * nodePos._y;
	float rotatedY = (float)((double)v[1] * nodePos._x + (double)v[7] * nodePos._z + (double)v[4] * nodePos._y);
	float rotatedX = (float)((double)v[6] * nodePos._z + (double)v[3] * nodePos._y + (double)nodePos._x * v[0]);
	float viewX = (float)((double)rotatedX + viewPos._x);
	float viewY = (float)((double)rotatedY + viewPos._y);
	float viewZ = (float)(pendingZ + viewPos._z);
	CMatrix rotation;

	node->_world._rot.multiply(&rotation, &camera->_view._rot);

	float matrix[12];

	for (int i = 0; i < 9; i++)
		matrix[i] = rotation._m[i];

	matrix[9] = viewX;
	matrix[10] = viewY;
	matrix[11] = viewZ;

	ctx->setCameraMatrix(matrix);
	update(node, camera);
	ctx->submitVertices(_vertexArray);

	for (uint i = 0; i < mesh->_trisets.size(); i++) {
		_engine->_rmeshFutzingTimer.start();

		TriSet *triset = mesh->_trisets[i];
		int flags = triset->drawFlags;

		if (_globalAlphaMode || _matAlphaMode)
			flags |= kBlendFlag;

		// The disasm keeps the texture of the previous triangle set when the material index is -1
		if (!mesh->_matHandles.empty() && triset->materialIdx != -1) {
			int materialIdx = triset->materialIdx;

			if (materialIdx < 0 || materialIdx >= (int)mesh->_materials.size() || materialIdx >= (int)_texIndex.size() || !mesh->_materials[materialIdx])
				warning("RMeshInstance::output(): invalid material index %d", materialIdx);
			else
				texture = mesh->_materials[materialIdx]->getTexture(_texIndex[materialIdx]);
		}

		_engine->_rmeshFutzingTimer.stop();
		ctx->drawTriangles(flags, texture, (int)triset->indices.size(), triset->indices.data());
	}

	ctx->releaseVertices();
	ctx->resetCameraMatrix();
	_engine->_rmeshOutputTimer.stop();
}

int RMeshInstance::getSaveSize() {
	return RPrimitiveInstance::getSaveSize() + (int)sizeof(int) + (int)(_texIndex.size() * sizeof(int));
}

bool RMeshInstance::saveState(SaveRestoreAccess *accessFunc) {
	RPrimitiveInstance::saveState(accessFunc);

	int count = (int)_texIndex.size();

	accessFunc(&count, sizeof(count));

	for (int i = 0; i < count; i++)
		accessFunc(&_texIndex[i], sizeof(int));

	return true;
}

bool RMeshInstance::restoreState(SaveRestoreAccess *accessFunc) {
	RPrimitiveInstance::restoreState(accessFunc);

	int count = 0;

	accessFunc(&count, sizeof(count));

	if (count < 0) {
		warning("RMeshInstance::restoreState(): invalid texture index count %d", count);
		count = 0;
	}

	_texIndex.resize(count, 0);

	for (int i = 0; i < count; i++)
		accessFunc(&_texIndex[i], sizeof(int));

	return true;
}

} // End of namespace Grim
