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

#include "common/array.h"
#include "common/endian.h"
#include "common/textconsole.h"

#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/nodes/csprite.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lformatparser.h"
#include "engines/grim/emi/resources/lresource.h"
#include "engines/grim/emi/nodes/rnode.h"

namespace Grim {

enum {
	kKeywordTextureName = 0,
	kKeywordDrawFlags = 1,
	kKeywordDimensions = 2,
	kKeywordOffset = 3,
	kKeywordColor = 4,
	kKeywordTexCoord = 5
};

static const int kTokenSize = 256;
static const int kBinaryHeaderSize = 40;
static const int kBinaryTailSize = 120;
static const float kCornerX[CSprite::kNumCorners] = { -1.0f, 1.0f, 1.0f, -1.0f };
static const float kCornerY[CSprite::kNumCorners] = { 1.0f, 1.0f, -1.0f, -1.0f };
static const float kAlphaScale = 255.0f;

class CSpriteParser : public LFormatParser {
public:
	explicit CSpriteParser(EmiEngine *engine) : LFormatParser(engine) {
		addKeyword("TextureName", kKeywordTextureName, 7);
		addKeyword("Drawflags", kKeywordDrawFlags, 1);
		addKeyword("Dimensions", kKeywordDimensions, 10);
		addKeyword("Offset", kKeywordOffset, 42);
		addKeyword("Color", kKeywordColor, 85);
		addKeyword("TxCoord", kKeywordTexCoord, 10);
	}
};

static bool readBytes(LFile *file, void *buffer, uint size) {
	return file->read(buffer, size) == size;
}

static bool readString(LFile *file, int fileSize, LString *result) {
	byte lengthBytes[4];

	if (!readBytes(file, lengthBytes, sizeof(lengthBytes)))
		return false;

	int length = READ_LE_INT32(lengthBytes);

	if (length < 0 || (fileSize > 0 && length > fileSize)) {
		warning("CSprite::loadBinary(): invalid string length %d", length);
		return false;
	}

	Common::Array<char> buffer(length + 1);

	if (length > 0 && !readBytes(file, &buffer[0], length))
		return false;

	buffer[length] = 0;
	*result = &buffer[0];

	return true;
}

static void writeInt(LFile *file, int value) {
	byte bytes[4];

	WRITE_LE_INT32(bytes, value);
	file->write(bytes, sizeof(bytes));
}

static void writeFloat(LFile *file, float value) {
	byte bytes[4];

	WRITE_LE_FLOAT32(bytes, value);
	file->write(bytes, sizeof(bytes));
}

static void writeString(LFile *file, const LString &text) {
	writeInt(file, text.size() + 1);
	file->write(text.c_str(), text.size() + 1);
}

CSprite::CSprite(EmiEngine *engine) : _material(engine), _engine(engine) {
	_width = 1.0f;
	_height = 1.0f;
	_drawFlags = 0;

	for (int i = 0; i < kNumCorners; i++) {
		_corners[i] = Color::kWhite;
		_uv[i * 2] = (i == 0 || i == 3) ? 0.0f : 1.0f;
		_uv[i * 2 + 1] = (i == 2 || i == 3) ? 0.0f : 1.0f;
	}
}

CSprite::~CSprite() {
	_material.unlock();
	_material.setResource(nullptr);
}

EmiEngine *CSprite::getEngine() const {
	return _engine;
}

RPrimitiveInstance *CSprite::createInstance() {
	return new CSpriteInstance(this);
}

bool CSprite::restore(LFile *file) {
	CPrimitive::restore(file);

	CSpriteParser parser(_engine);
	char textureName[kTokenSize];
	int textureFlags = 0;

	parser.setFile(file);
	parser.readVersion(1, 0);

	if (!parser.readEntry(kKeywordTextureName, textureName, &textureFlags))
		return false;

	_material._resource = _engine->getResourceManager()->findResource(textureName);

	if (!_material._resource) {
		_material._resource = _engine->getResourceManager()->createResource(textureName);

		if (!_material._resource)
			_material.allocateResource();

		if (_material._resource) {
			_material._resource->resSetName(textureName);
			_engine->getResourceManager()->addResource(_material._resource);
		}
	}

	_material.setResource(_material._resource);

	if (!parser.readEntry(kKeywordDrawFlags, &_drawFlags))
		return false;

	if (!parser.readEntry(kKeywordDimensions, &_width, &_height))
		return false;

	// The disasm reads the offset line with the id of the dimensions keyword
	if (!parser.readEntry(kKeywordDimensions, &_offset._x, &_offset._y, &_offset._z))
		return false;

	for (int i = 0; i < kNumCorners; i++) {
		if (!parser.readEntry(kKeywordColor, &_corners[i]._r, &_corners[i]._g, &_corners[i]._b, &_corners[i]._a))
			return false;
	}

	for (int i = 0; i < kNumCorners; i++) {
		if (!parser.readEntry(kKeywordTexCoord, &_uv[i * 2], &_uv[i * 2 + 1]))
			return false;
	}

	parser.readEndOfSection();

	if (_material._resource && _material.lock() && _material._typed)
		_material._typed->addFlags(textureFlags);

	return true;
}

bool CSprite::loadBinary(const char *path) {
	LFile *file = _engine->getFileServices()->openAndLoad(path, "rb");

	if (!file)
		return false;

	int fileSize = _engine->getFileServices()->getSize(path);
	LString spriteName;
	LString materialName;
	byte header[kBinaryHeaderSize];
	byte flagBytes[4];
	byte tail[kBinaryTailSize];
	bool valid = readString(file, fileSize, &spriteName);

	valid = valid && readBytes(file, header, sizeof(header));
	valid = valid && readString(file, fileSize, &materialName);
	valid = valid && readBytes(file, flagBytes, sizeof(flagBytes));
	valid = valid && readBytes(file, tail, sizeof(tail));

	if (!valid) {
		warning("CSprite::loadBinary(): unexpected end of file");
		_engine->getFileServices()->close(file, true);

		return false;
	}

	_name = spriteName;
	_radius = READ_LE_FLOAT32(header);
	_center._x = READ_LE_FLOAT32(header + 4);
	_center._y = READ_LE_FLOAT32(header + 8);
	_center._z = READ_LE_FLOAT32(header + 12);
	_box._min._x = READ_LE_FLOAT32(header + 16);
	_box._min._y = READ_LE_FLOAT32(header + 20);
	_box._min._z = READ_LE_FLOAT32(header + 24);
	_box._max._x = READ_LE_FLOAT32(header + 28);
	_box._max._y = READ_LE_FLOAT32(header + 32);
	_box._max._z = READ_LE_FLOAT32(header + 36);

	LResourceHandle<CMaterial> foundMaterial(_engine);

	foundMaterial._resource = _engine->getResourceManager()->findResource(materialName);

	if (!foundMaterial._resource) {
		foundMaterial._resource = _engine->getResourceManager()->createResource(materialName);

		if (!foundMaterial._resource)
			foundMaterial.allocateResource();

		if (foundMaterial._resource) {
			foundMaterial._resource->resSetName(materialName.c_str());
			_engine->getResourceManager()->addResource(foundMaterial._resource);
		}
	}

	foundMaterial.setResource(foundMaterial._resource);
	_material.setResource(foundMaterial._resource);

	CMaterial *typedMaterial = nullptr;

	if (_material._resource && _material.lock())
		typedMaterial = _material._typed;

	if (typedMaterial)
		typedMaterial->setFlags(READ_LE_INT32(flagBytes));
	else
		warning("CSprite::loadBinary(): cannot lock the material");

	_width = READ_LE_FLOAT32(tail);
	_height = READ_LE_FLOAT32(tail + 4);
	_offset._x = READ_LE_FLOAT32(tail + 8);
	_offset._y = READ_LE_FLOAT32(tail + 12);
	_offset._z = READ_LE_FLOAT32(tail + 16);

	for (int i = 0; i < kNumCorners; i++) {
		_corners[i]._a = READ_LE_INT32(tail + 20 + i * 16);
		_corners[i]._r = READ_LE_INT32(tail + 24 + i * 16);
		_corners[i]._g = READ_LE_INT32(tail + 28 + i * 16);
		_corners[i]._b = READ_LE_INT32(tail + 32 + i * 16);
	}

	for (int i = 0; i < kNumCorners * 2; i++)
		_uv[i] = READ_LE_FLOAT32(tail + 84 + i * 4);

	_drawFlags = READ_LE_INT32(tail + 116);

	_engine->getFileServices()->close(file, true);

	return true;
}

bool CSprite::saveBinary(const char *path) {
	LFile *file = _engine->getFileServices()->open(path, "wb");

	if (!file)
		return false;

	writeString(file, _name);
	writeFloat(file, _radius);
	writeFloat(file, _center._x);
	writeFloat(file, _center._y);
	writeFloat(file, _center._z);
	writeFloat(file, _box._min._x);
	writeFloat(file, _box._min._y);
	writeFloat(file, _box._min._z);
	writeFloat(file, _box._max._x);
	writeFloat(file, _box._max._y);
	writeFloat(file, _box._max._z);

	LString materialName;

	if (_material._resource)
		materialName = _material._resource->_name;
	else
		warning("CSprite::saveBinary(): the sprite has no material");

	writeString(file, materialName);

	CMaterial *typedMaterial = nullptr;

	if (_material._resource && _material.lock())
		typedMaterial = _material._typed;

	writeInt(file, typedMaterial ? typedMaterial->_flags : 0);
	_material.unlock();

	writeFloat(file, _width);
	writeFloat(file, _height);
	writeFloat(file, _offset._x);
	writeFloat(file, _offset._y);
	writeFloat(file, _offset._z);

	for (int i = 0; i < kNumCorners; i++) {
		writeInt(file, _corners[i]._a);
		writeInt(file, _corners[i]._r);
		writeInt(file, _corners[i]._g);
		writeInt(file, _corners[i]._b);
	}

	for (int i = 0; i < kNumCorners * 2; i++)
		writeFloat(file, _uv[i]);

	writeInt(file, _drawFlags);

	_engine->getFileServices()->close(file, true);

	return true;
}

CSpriteInstance::CSpriteInstance(CSprite *spritePrimitive) : RPrimitiveInstance(spritePrimitive), _engine(spritePrimitive->getEngine()) {
	_textureIndex = 0;
}

CSpriteInstance::~CSpriteInstance() {
}

int CSpriteInstance::getSaveSize() {
	return RPrimitiveInstance::getSaveSize() + (int)sizeof(_textureIndex);
}

bool CSpriteInstance::saveState(SaveRestoreAccess *accessFunc) {
	RPrimitiveInstance::saveState(accessFunc);
	accessFunc(&_textureIndex, sizeof(_textureIndex));

	return true;
}

bool CSpriteInstance::restoreState(SaveRestoreAccess *accessFunc) {
	RPrimitiveInstance::restoreState(accessFunc);
	accessFunc(&_textureIndex, sizeof(_textureIndex));

	return true;
}

void CSpriteInstance::output(RNode *node, Camera *camera) {
	CDrawContext *ctx = _engine->getDrawContext();

	if (!_visible)
		return;

	if (!ctx) {
		warning("CSpriteInstance::output(): no draw context");
		return;
	}

	CSprite *sprite = static_cast<CSprite *>(_prim);

	update(node, camera);
	ctx->resetCameraMatrix();

	if ((node->_nodeFlags & CNode::kWorldValid) == 0)
		node->updateWorldTransform();

	const float *v = camera->_view._rot._m;
	const CVector &nodePos = node->_world._pos;
	const CVector &viewPos = camera->_view._pos;

	double pendingZ = (double)v[8] * nodePos._z + (double)v[2] * nodePos._x + (double)v[5] * nodePos._y;
	float rotatedY = (float)((double)v[7] * nodePos._z + (double)v[1] * nodePos._x + (double)v[4] * nodePos._y);
	float rotatedX = (float)((double)v[6] * nodePos._z + (double)v[3] * nodePos._y + (double)v[0] * nodePos._x);
	float nodeViewZ = (float)(pendingZ + viewPos._z);
	float nodeViewY = (float)((double)rotatedY + viewPos._y);
	float nodeViewX = (float)((double)rotatedX + viewPos._x);

	if ((node->_nodeFlags & CNode::kEulerValid) == 0)
		node->computeLocalMatrixFromWorld();

	CVector billboardEuler;

	billboardEuler._x = 0.0f;
	billboardEuler._y = 0.0f;
	billboardEuler._z = node->_eulerDeg._y;

	CMatrix rotation;

	rotation.setFromEuler(billboardEuler);

	CDrawList *drawList = ctx->getDrawList(0);
	DrawPoly *poly = drawList->newPoly(4);

	poly->flags = sprite->_drawFlags;

	if (sprite->_material._resource) {
		CMaterial *spriteMaterial = nullptr;

		if (sprite->_material.lock())
			spriteMaterial = sprite->_material._typed;

		if (spriteMaterial) {
			poly->tex = spriteMaterial->getTexture(_textureIndex);
			sprite->_material.unlock();
		} else {
			warning("CSpriteInstance::output(): cannot lock the material");
			poly->tex = nullptr;
		}
	} else {
		poly->tex = nullptr;
	}

	const float *m = rotation._m;

	for (int i = 0; i < CSprite::kNumCorners; i++) {
		double halfWidth = (double)kCornerX[i] * sprite->_width * 0.5;
		float halfHeight = (float)((double)kCornerY[i] * sprite->_height * 0.5);
		double localX = halfWidth + sprite->_offset._x;
		double localY = (double)halfHeight + sprite->_offset._y;
		double localZ = sprite->_offset._z;
		float spriteX = (float)((double)m[0] * localX + (double)m[6] * localZ + (double)m[3] * localY);
		float spriteY = (float)((double)m[1] * localX + (double)m[7] * localZ + (double)m[4] * localY);
		double pendingSpriteZ = (double)m[2] * localX + (double)m[8] * localZ + (double)m[5] * localY;
		DrawVertex *vertex = poly->verts[i];

		vertex->x = (float)((double)spriteX + nodeViewX);
		vertex->y = (float)((double)spriteY + nodeViewY);
		vertex->z = (float)(pendingSpriteZ + nodeViewZ);
		vertex->u = sprite->_uv[i * 2];
		vertex->v = sprite->_uv[i * 2 + 1];
		vertex->color = ((uint32)(sprite->_corners[i]._a & 0xFF) << 24) | ((uint32)(sprite->_corners[i]._r & 0xFF) << 16) |
				((uint32)(sprite->_corners[i]._g & 0xFF) << 8) | (uint32)(sprite->_corners[i]._b & 0xFF);
	}

	if (_globalAlphaMode == kAlphaSet) {
		for (int i = 0; i < CSprite::kNumCorners; i++) {
			uint32 alpha = (uint32)(int)((double)_globalAlpha * kAlphaScale) & 0xFF;

			poly->verts[i]->color = (poly->verts[i]->color & 0x00FFFFFF) | (alpha << 24);
		}
	} else if (_globalAlphaMode == kAlphaModulate) {
		for (int i = 0; i < CSprite::kNumCorners; i++) {
			int scale = (int)((double)_globalAlpha * kAlphaScale);
			int vertexAlpha = poly->verts[i]->color >> 24;
			int product = (int)((uint32)scale * (uint32)vertexAlpha);
			uint32 alpha = (uint32)(product / 255) & 0xFF;

			poly->verts[i]->color = (poly->verts[i]->color & 0x00FFFFFF) | (alpha << 24);
		}
	}

	if (_matAlphaMode == kAlphaSet || _matAlphaMode == kAlphaModulate) {
		for (Common::HashMap<int, float>::const_iterator it = _matAlpha.begin(); it != _matAlpha.end(); ++it) {
			if (it->_key < 0 || it->_key >= poly->numVerts) {
				warning("CSpriteInstance::output(): invalid vertex index %d", it->_key);
				continue;
			}

			DrawVertex *vertex = poly->verts[it->_key];
			int scale = (int)((double)it->_value * kAlphaScale);
			uint32 alpha;

			if (_matAlphaMode == kAlphaSet) {
				alpha = (uint32)scale & 0xFF;
			} else {
				int vertexAlpha = vertex->color >> 24;
				int product = (int)((uint32)scale * (uint32)vertexAlpha);

				alpha = (uint32)(product / 255) & 0xFF;
			}

			vertex->color = (vertex->color & 0x00FFFFFF) | (alpha << 24);
		}
	}

	drawList->addPoly(poly);

	if (!_engine->isDemo())
		ctx->flushDrawLists();
}

} // End of namespace Grim
