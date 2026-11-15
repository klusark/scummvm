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

#include "common/endian.h"
#include "common/list.h"
#include "common/textconsole.h"

#include "engines/grim/emi/scene/clightnode.h"
#include "engines/grim/emi/nodes/cweightmap.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/resources/lresource.h"
#include "engines/grim/emi/nodes/rmesh.h"
#include "engines/grim/emi/nodes/rmeshinstance.h"
#include "engines/grim/emi/nodes/rmeshparser.h"

namespace Grim {

static const int kBoundsSize = 40;
static const int kDefaultDrawFlags = 772;
static const int kDefaultAmbient = 128;
static const double kHalf = 0.5;
static const char kSourceExtension[] = "PCX";
static const CVector kZeroVector;
static const float kCubeDirections[RMesh::kNumCubeFaces][3] = {
	{ 1.0f, 0.0f, 0.0f },
	{ -1.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f },
	{ 0.0f, -1.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, -1.0f }
};

static bool readBytes(LFile *file, void *buffer, uint size) {
	return file->read(buffer, size) == size;
}

static bool readInt(LFile *file, int *value) {
	byte bytes[4];

	if (!readBytes(file, bytes, sizeof(bytes)))
		return false;

	*value = READ_LE_INT32(bytes);

	return true;
}

static bool readString(LFile *file, int fileSize, LString *result) {
	int length;

	if (!readInt(file, &length))
		return false;

	if (length < 0 || (fileSize > 0 && length > fileSize)) {
		warning("RMesh::loadBinary(): invalid string length %d", length);

		return false;
	}

	Common::Array<char> buffer(length + 1);

	if (length > 0 && !readBytes(file, &buffer[0], length))
		return false;

	buffer[length] = 0;
	*result = &buffer[0];

	return true;
}

static bool readFloats(LFile *file, float *dest, int count) {
	if (count <= 0)
		return true;

	Common::Array<byte> bytes(count * sizeof(float));

	if (!readBytes(file, &bytes[0], bytes.size()))
		return false;

	for (int i = 0; i < count; i++)
		dest[i] = READ_LE_FLOAT32(&bytes[i * sizeof(float)]);

	return true;
}

static bool readInts(LFile *file, int *dest, int count) {
	if (count <= 0)
		return true;

	Common::Array<byte> bytes(count * sizeof(int));

	if (!readBytes(file, &bytes[0], bytes.size()))
		return false;

	for (int i = 0; i < count; i++)
		dest[i] = READ_LE_INT32(&bytes[i * sizeof(int)]);

	return true;
}

static bool isCountValid(int count, int elementSize, int fileSize) {
	return count >= 0 && (fileSize <= 0 || (int64)count * elementSize <= fileSize);
}

static void writeBytes(LFile *file, const void *buffer, uint size) {
	file->write(buffer, size);
}

static void writeInt(LFile *file, int value) {
	byte bytes[4];

	WRITE_LE_INT32(bytes, value);
	writeBytes(file, bytes, sizeof(bytes));
}

static void writeFloat(LFile *file, float value) {
	byte bytes[4];

	WRITE_LE_FLOAT32(bytes, value);
	writeBytes(file, bytes, sizeof(bytes));
}

static void writeString(LFile *file, const LString &text) {
	writeInt(file, text.size() + 1);
	writeBytes(file, text.c_str(), text.size() + 1);
}

RMesh::RMesh(EmiEngine *engine) : _engine(engine) {
	_vertFlags = 0;
	_numTextureSets = 0;
	_numVerts = 0;
	_pos = nullptr;
	_normals = nullptr;
	_vertIndex = nullptr;
	_colors = nullptr;
	_uv = nullptr;
	_weightMap = nullptr;
}

RMesh::~RMesh() {
	freeVertexData();

	for (uint i = 0; i < _matHandles.size(); i++)
		_matHandles[i].unlock();

	_matHandles.clear();
	_materials.clear();
	_texFlags.clear();

	for (uint i = 0; i < _trisets.size(); i++) {
		delete _trisets[i];
		_trisets[i] = nullptr;
	}

	_trisets.clear();

	delete _weightMap;
	_weightMap = nullptr;
}

EmiEngine *RMesh::getEngine() const {
	return _engine;
}

RPrimitiveInstance *RMesh::createInstance() {
	return new RMeshInstance(this);
}

void RMesh::allocVertexData(int flags, int count) {
	if (count < 0) {
		warning("RMesh::allocVertexData(): invalid vertex count %d", count);
		count = 0;
	}

	_vertFlags = flags;
	_numVerts = count;

	_pos = (float *)_engine->getMemoryServices()->mallocImpl(count * 3 * sizeof(float));

	if (_pos)
		memset(_pos, 0, count * 3 * sizeof(float));

	if (flags & 1) {
		_normals = (float *)_engine->getMemoryServices()->mallocImpl(_numVerts * 3 * sizeof(float));

		if (_normals)
			memset(_normals, 0, _numVerts * 3 * sizeof(float));
	}

	if (flags & 2) {
		_colors = (PackedColor *)_engine->getMemoryServices()->mallocImpl(_numVerts * sizeof(PackedColor));

		if (_colors)
			memset(_colors, 0, _numVerts * sizeof(PackedColor));
	}

	if (flags & 4)
		_vertIndex = (int *)_engine->getMemoryServices()->mallocImpl(_numVerts * sizeof(int));

	if (flags & 0x10) {
		_uv = (float *)_engine->getMemoryServices()->mallocImpl(_numVerts * 2 * sizeof(float));

		if (_uv)
			memset(_uv, 0, _numVerts * 2 * sizeof(float));
	}
}

void RMesh::freeVertexData() {
	_numVerts = 0;

	if (_pos) {
		_engine->getMemoryServices()->freeImpl(_pos);
		_pos = nullptr;
	}

	if (_normals) {
		_engine->getMemoryServices()->freeImpl(_normals);
		_normals = nullptr;
	}

	if (_vertIndex) {
		_engine->getMemoryServices()->freeImpl(_vertIndex);
		_vertIndex = nullptr;
	}

	if (_colors) {
		_engine->getMemoryServices()->freeImpl(_colors);
		_colors = nullptr;
	}

	if (_uv) {
		_engine->getMemoryServices()->freeImpl(_uv);
		_uv = nullptr;
	}
}

void RMesh::lightVertices(const CTransform *transform, int count, CDCVertex *vertices, const float *vertexNormals, int maxLights) {
	_engine->_rmeshLightingTimer.start();

	if (maxLights > kMaxLights) {
		warning("RMesh::lightVertices(): too many lights requested (%d)", maxLights);
		maxLights = kMaxLights;
	}

	int numLights = 0;
	float lightDistances[kMaxLights] = { 0.0f };
	CLight *lightList[kMaxLights];
	float lightTransforms[kMaxLights][12];

	CLight *ambientLight = nullptr;
	const Common::List<CLightNode *> &activeLights = _engine->getActiveLights();

	for (Common::List<CLightNode *>::const_iterator it = activeLights.begin(); it != activeLights.end(); ++it) {
		CLightNode *lightNode = *it;

		if (!lightNode)
			continue;

		CLight *light = lightNode->_light;

		if (!light) {
			warning("RMesh::lightVertices(): light node without a light");
			continue;
		}

		if (light->_type == CLight::kTypeAmbient) {
			ambientLight = light;
			continue;
		}

		if ((lightNode->_nodeFlags & CNode::kWorldValid) == 0)
			lightNode->updateWorldTransform();

		const float *r = transform->_rot._m;
		double deltaX = (double)lightNode->_world._pos._x - transform->_pos._x;
		double deltaY = (double)lightNode->_world._pos._y - transform->_pos._y;
		double deltaZ = (double)lightNode->_world._pos._z - transform->_pos._z;
		float localX = (float)(deltaY * r[1] + deltaZ * r[2] + deltaX * r[0]);
		float localY = (float)(deltaY * r[4] + deltaZ * r[5] + deltaX * r[3]);
		float localZ = (float)(deltaY * r[7] + deltaZ * r[8] + deltaX * r[6]);
		CMatrix transposed;
		CMatrix product;
		float lightTransform[12];

		transform->_rot.transpose(&transposed);
		lightNode->_world._rot.multiply(&product, &transposed);

		for (int i = 0; i < 9; i++)
			lightTransform[i] = product._m[i];

		lightTransform[9] = localX;
		lightTransform[10] = localY;
		lightTransform[11] = localZ;

		if (light->isOutOfRange(lightTransform, &_box))
			continue;

		double sumX = (double)_box._max._x + _box._min._x;
		float sumY = (float)((double)_box._max._y + _box._min._y);
		float sumZ = (float)((double)_box._max._z + _box._min._z);
		double centerX = sumX * kHalf;
		float centerY = (float)((double)sumY * kHalf);
		float centerZ = (float)((double)sumZ * kHalf);
		double distanceX = centerX - lightTransform[9];
		double distanceY = (double)centerY - lightTransform[10];
		double distanceZ = (double)centerZ - lightTransform[11];
		double distance = distanceZ * distanceZ + distanceY * distanceY + distanceX * distanceX;
		int slot = numLights;

		if (numLights == maxLights) {
			slot = 0;

			for (int i = 1; i < numLights; i++) {
				if (lightDistances[i] > lightDistances[slot])
					slot = i;
			}

			if (!(distance < lightDistances[slot]))
				continue;
		} else {
			numLights++;
		}

		lightDistances[slot] = (float)distance;
		lightList[slot] = light;

		for (int i = 0; i < 12; i++)
			lightTransforms[slot][i] = lightTransform[i];
	}

	float actorTransform[12];

	for (int i = 0; i < 9; i++)
		actorTransform[i] = transform->_rot._m[i];

	actorTransform[9] = transform->_pos._x;
	actorTransform[10] = transform->_pos._y;
	actorTransform[11] = transform->_pos._z;

	Color ambient;

	ambient.setRGB(0, 0, 0);

	if (ambientLight)
		ambientLight->addLighting(&ambient, actorTransform, &kZeroVector._x, &kZeroVector._x);
	else
		ambient.setRGB(kDefaultAmbient, kDefaultAmbient, kDefaultAmbient);

	Color color;
	PackedColor lit;

	lit._b = 0;
	lit._g = 0;
	lit._r = 0;
	lit._a = 0;

	for (int i = 0; i < count; i++) {
		color = ambient;

		for (int j = 0; j < numLights; j++)
			lightList[j]->addLighting(&color, lightTransforms[j], &vertices[i].x, vertexNormals + i * 3);

		color.clamp();

		lit._r = 255;

		if ((uint)color._r <= 0xFF)
			lit._r = color._r;

		lit._g = 255;

		if ((uint)color._g <= 0xFF)
			lit._g = color._g;

		lit._b = 255;

		if ((uint)color._b <= 0xFF)
			lit._b = color._b;

		lit._a = 255;

		if (_colors) {
			PackedColor modulated;

			_colors[i].modulate(&modulated, &lit);
			vertices[i].color = modulated.pack();
		} else {
			vertices[i].color = lit.pack();
		}
	}

	_engine->_rmeshLightingTimer.stop();
}

void RMesh::lightVerticesAmbientCube(const CTransform *transform, int count, CDCVertex *vertices, const float *vertexNormals, int maxLights) {
	_engine->_rmeshLightingTimer.start();

	if (maxLights > kMaxLights) {
		warning("RMesh::lightVerticesAmbientCube(): too many lights requested (%d)", maxLights);
		maxLights = kMaxLights;
	}

	int numLights = 0;
	float lightDistances[kMaxLights] = { 0.0f };
	CLight *lightList[kMaxLights];
	float lightTransforms[kMaxLights][12];

	CLight *ambientLight = nullptr;
	const Common::List<CLightNode *> &activeLights = _engine->getActiveLights();

	for (Common::List<CLightNode *>::const_iterator it = activeLights.begin(); it != activeLights.end(); ++it) {
		CLightNode *lightNode = *it;

		if (!lightNode)
			continue;

		CLight *light = lightNode->_light;

		if (!light) {
			warning("RMesh::lightVerticesAmbientCube(): light node without a light");
			continue;
		}

		if (light->_type == CLight::kTypeAmbient) {
			ambientLight = light;
			continue;
		}

		if ((lightNode->_nodeFlags & CNode::kWorldValid) == 0)
			lightNode->updateWorldTransform();

		const float *r = transform->_rot._m;
		double deltaX = (double)lightNode->_world._pos._x - transform->_pos._x;
		double deltaY = (double)lightNode->_world._pos._y - transform->_pos._y;
		double deltaZ = (double)lightNode->_world._pos._z - transform->_pos._z;
		float localX = (float)(deltaY * r[1] + deltaZ * r[2] + deltaX * r[0]);
		float localY = (float)(deltaY * r[4] + deltaZ * r[5] + deltaX * r[3]);
		float localZ = (float)(deltaY * r[7] + deltaZ * r[8] + deltaX * r[6]);
		CMatrix transposed;
		CMatrix product;
		float lightTransform[12];

		transform->_rot.transpose(&transposed);
		lightNode->_world._rot.multiply(&product, &transposed);

		for (int i = 0; i < 9; i++)
			lightTransform[i] = product._m[i];

		lightTransform[9] = localX;
		lightTransform[10] = localY;
		lightTransform[11] = localZ;

		if (light->isOutOfRange(lightTransform, &_box))
			continue;

		double sumX = (double)_box._min._x + _box._max._x;
		float sumY = (float)((double)_box._max._y + _box._min._y);
		float sumZ = (float)((double)_box._max._z + _box._min._z);
		double centerX = sumX * kHalf;
		float centerY = (float)((double)sumY * kHalf);
		float centerZ = (float)((double)sumZ * kHalf);
		double distanceX = centerX - lightTransform[9];
		double distanceY = (double)centerY - lightTransform[10];
		double distanceZ = (double)centerZ - lightTransform[11];
		double distance = distanceX * distanceX + distanceZ * distanceZ + distanceY * distanceY;
		int slot = numLights;

		if (numLights == maxLights) {
			slot = 0;

			for (int i = 1; i < numLights; i++) {
				if (lightDistances[i] > lightDistances[slot])
					slot = i;
			}

			if (!(distance < lightDistances[slot]))
				continue;
		} else {
			numLights++;
		}

		lightDistances[slot] = (float)distance;
		lightList[slot] = light;

		for (int i = 0; i < 12; i++)
			lightTransforms[slot][i] = lightTransform[i];
	}

	float actorTransform[12];

	for (int i = 0; i < 9; i++)
		actorTransform[i] = transform->_rot._m[i];

	actorTransform[9] = transform->_pos._x;
	actorTransform[10] = transform->_pos._y;
	actorTransform[11] = transform->_pos._z;

	Color ambient;

	ambient.setRGB(0, 0, 0);

	if (ambientLight)
		ambientLight->addLighting(&ambient, actorTransform, &kZeroVector._x, &kZeroVector._x);
	else
		ambient.setRGB(kDefaultAmbient, kDefaultAmbient, kDefaultAmbient);

	Color cube[kNumCubeFaces];

	for (int face = 0; face < kNumCubeFaces; face++) {
		cube[face] = ambient;

		for (int j = 0; j < numLights; j++) {
			float factor = lightList[j]->evaluate(lightTransforms[j], &_center._x, kCubeDirections[face]);

			if (factor > 0.0f) {
				const Color &lightColor = lightList[j]->_color;

				// The disasm rounds the products to the nearest integer here
				float addedR = (float)((double)(uint)lightColor._r * factor);
				cube[face]._r += (int)lrint(addedR);

				float addedG = (float)((double)(uint)lightColor._g * factor);
				cube[face]._g += (int)lrint(addedG);

				float addedB = (float)((double)(uint)lightColor._b * factor);
				cube[face]._b += (int)lrint(addedB);
			}
		}

		cube[face].clamp();
	}

	PackedColor lit;

	lit._b = 0;
	lit._g = 0;
	lit._r = 0;
	lit._a = 0;

	for (int i = 0; i < count; i++) {
		float normalX = vertexNormals[i * 3];
		float normalY = vertexNormals[i * 3 + 1];
		float normalZ = vertexNormals[i * 3 + 2];
		float weightX = (float)((double)normalX * normalX);
		float weightY = (float)((double)normalY * normalY);
		float weightZ = (float)((double)normalZ * normalZ);
		uint32 bitsX;
		uint32 bitsY;
		uint32 bitsZ;

		memcpy(&bitsX, &normalX, sizeof(bitsX));
		memcpy(&bitsY, &normalY, sizeof(bitsY));
		memcpy(&bitsZ, &normalZ, sizeof(bitsZ));

		Color sum;
		const Color &faceX = cube[bitsX >> 31];
		const Color &faceY = cube[2 + (bitsY >> 31)];
		const Color &faceZ = cube[4 + (bitsZ >> 31)];

		sum._r += (int)lrint((float)((double)(uint)faceX._r * weightX));
		sum._g += (int)lrint((float)((double)(uint)faceX._g * weightX));
		sum._b += (int)lrint((float)((double)(uint)faceX._b * weightX));
		sum._r += (int)lrint((float)((double)(uint)faceY._r * weightY));
		sum._g += (int)lrint((float)((double)(uint)faceY._g * weightY));
		sum._b += (int)lrint((float)((double)(uint)faceY._b * weightY));
		sum._r += (int)lrint((float)((double)(uint)faceZ._r * weightZ));
		sum._g += (int)lrint((float)((double)(uint)faceZ._g * weightZ));
		sum._b += (int)lrint((float)((double)(uint)faceZ._b * weightZ));

		lit._r = 255;

		if ((uint)sum._r <= 0xFF)
			lit._r = sum._r;

		lit._g = 255;

		if ((uint)sum._g <= 0xFF)
			lit._g = sum._g;

		lit._b = 255;

		if ((uint)sum._b <= 0xFF)
			lit._b = sum._b;

		lit._a = 255;

		if (_colors) {
			PackedColor modulated;

			_colors[i].modulate(&modulated, &lit);
			vertices[i].color = modulated.pack();
		} else {
			vertices[i].color = lit.pack();
		}
	}

	_engine->_rmeshLightingTimer.stop();
}

void RMesh::computeBounds() {
	if (_numVerts) {
		_box.init(_pos);

		for (int i = 1; i < _numVerts; i++)
			_box.expand(&_pos[i * 3]);

		double sumX = (double)_box._max._x + _box._min._x;
		float sumY = (float)((double)_box._min._y + _box._max._y);
		float sumZ = (float)((double)_box._min._z + _box._max._z);

		_center._y = (float)((double)sumY * kHalf);
		_center._x = (float)(sumX * kHalf);
		_center._z = (float)((double)sumZ * kHalf);

		double sizeZ = (double)_box._max._z - _box._min._z;
		double sizeY = (double)_box._max._y - _box._min._y;
		double sizeX = (double)_box._max._x - _box._min._x;

		_radius = (float)(sqrt(sizeX * sizeX + sizeY * sizeY + sizeZ * sizeZ) * kHalf);
	}
}

bool RMesh::save(LFile *file) {
	CPrimitive::save(file);

	RMeshParser parser(_engine);

	parser.setFile(file);
	parser.writeVersion(2, 1);
	parser.writeComment("RMesh primitive");
	parser.writeEntry(RMeshParser::kKeywordBasePrim);
	parser.writeComment("Texture sets");
	parser.writeEntry(RMeshParser::kKeywordTotTxSets, _numTextureSets);

	if (_numTextureSets) {
		parser.writeVersion(2, 0);
		parser.writeComment("Material Set");
		parser.writeEntry(RMeshParser::kKeywordSetType, 0);
		parser.writeEntry(RMeshParser::kKeywordNumTextures, (int)_matHandles.size());

		for (uint i = 0; i < _matHandles.size(); i++) {
			LString materialName;

			if (_matHandles[i]._resource)
				materialName = _matHandles[i]._resource->_name;
			else
				warning("RMesh::save(): material %u has no resource", i);

			if (!materialName.contains(".sur") && !materialName.contains(".SUR")) {
				uint dot = materialName.find('.');

				if (dot != LString::npos) {
					materialName.erase(dot + 1);
					materialName += kSourceExtension;
				}
			}

			int flags = 0;

			if (i < _texFlags.size())
				flags = _texFlags[i];
			else
				warning("RMesh::save(): material %u has no texture flags", i);

			parser.writeEntry(RMeshParser::kKeywordTextureName, materialName.c_str(), flags);
		}

		parser.writeEndOfSection();
	}

	parser.writeComment("Faces");
	parser.writeEntry(RMeshParser::kKeywordNumFaces, 0);
	parser.writeEntry(RMeshParser::kKeywordNumFaceNormals, 0);
	parser.writeComment("Vertex arrays");
	parser.writeEntry(RMeshParser::kKeywordVertFlags, _vertFlags);
	parser.writeEntry(RMeshParser::kKeywordNumVerts, _numVerts);
	parser.writeComment("Vert x y z");

	for (int i = 0; i < _numVerts; i++)
		parser.writeEntry(RMeshParser::kKeywordVector, _pos[i * 3], _pos[i * 3 + 1], _pos[i * 3 + 2]);

	if (_vertFlags & 1) {
		parser.writeComment("Normal x y z");

		for (int i = 0; i < _numVerts; i++)
			parser.writeEntry(RMeshParser::kKeywordNormal, _normals[i * 3], _normals[i * 3 + 1], _normals[i * 3 + 2]);
	}

	if (_vertFlags & 2) {
		parser.writeComment("Color r g b");

		for (int i = 0; i < _numVerts; i++)
			parser.writeEntry(RMeshParser::kKeywordColor, _colors[i]._r, _colors[i]._g, _colors[i]._b, _colors[i]._a);
	}

	if (_vertFlags & 4) {
		parser.writeComment("VertIndex");

		for (int i = 0; i < _numVerts; i++)
			parser.writeEntry(RMeshParser::kKeywordVertIndex, _vertIndex[i]);
	}

	if (_vertFlags & 0x10) {
		parser.writeComment("TextCoord u v");

		for (int i = 0; i < _numVerts; i++)
			parser.writeEntry(RMeshParser::kKeywordTexCoord, _uv[i * 2], _uv[i * 2 + 1]);
	}

	parser.writeComment("Triangle sets");
	parser.writeEntry(RMeshParser::kKeywordNumTriSets, (int)_trisets.size());

	for (uint i = 0; i < _trisets.size(); i++) {
		TriSet *triset = _trisets[i];
		int numIndices = (int)triset->indices.size();

		parser.writeEntry(RMeshParser::kKeywordTsDrawFlags, triset->drawFlags);
		parser.writeEntry(RMeshParser::kKeywordTsMaterialIdx, triset->materialIdx);
		parser.writeEntry(RMeshParser::kKeywordTsNumTris, numIndices / 3);

		for (int j = 0; j + 2 < numIndices; j += 3)
			parser.writeEntry(RMeshParser::kKeywordTsTriangle, triset->indices[j], triset->indices[j + 1], triset->indices[j + 2]);
	}

	if (_weightMap) {
		parser.writeEntry(RMeshParser::kKeywordDeformPrim);
		_weightMap->save(file);
	}

	parser.writeEndOfSection();

	return true;
}

bool RMesh::restore(LFile *file) {
	CPrimitive::restore(file);

	RMeshParser parser(_engine);

	parser.setFile(file);
	parser._mesh = this;

	bool result = parser.parseRemaining(2, 1);

	if (result && _weightMap && _vertIndex) {
		_weightMap->remapVertexLists(_numVerts, _vertIndex);
		_weightMap->flattenWeights();
	}

	if (_vertIndex) {
		_engine->getMemoryServices()->freeImpl(_vertIndex);
		_vertIndex = nullptr;
		_vertFlags &= ~4;
	}

	return result;
}

bool RMesh::loadBinary(const char *path) {
	LFile *file = _engine->getFileServices()->openAndLoad(path, "rb");

	if (!file)
		return false;

	int fileSize = _engine->getFileServices()->getSize(path);
	byte bounds[kBoundsSize];
	bool valid = readString(file, fileSize, &_name);

	valid = valid && readBytes(file, bounds, sizeof(bounds));

	if (valid) {
		_radius = READ_LE_FLOAT32(bounds);
		_center._x = READ_LE_FLOAT32(bounds + 4);
		_center._y = READ_LE_FLOAT32(bounds + 8);
		_center._z = READ_LE_FLOAT32(bounds + 12);
		_box._min._x = READ_LE_FLOAT32(bounds + 16);
		_box._min._y = READ_LE_FLOAT32(bounds + 20);
		_box._min._z = READ_LE_FLOAT32(bounds + 24);
		_box._max._x = READ_LE_FLOAT32(bounds + 28);
		_box._max._y = READ_LE_FLOAT32(bounds + 32);
		_box._max._z = READ_LE_FLOAT32(bounds + 36);
	}

	valid = valid && readInt(file, &_numTextureSets);

	if (valid && _numTextureSets > 0) {
		int setType = 0;
		int numTextures = 0;

		valid = readInt(file, &setType) && readInt(file, &numTextures);

		if (valid && !isCountValid(numTextures, 8, fileSize)) {
			warning("RMesh::loadBinary(): invalid texture count %d", numTextures);
			valid = false;
		}

		if (valid) {
			_matHandles.resize(numTextures, LResourceHandle<CMaterial>(_engine));
			_texFlags.resize(numTextures, 0);
			_materials.resize(numTextures, nullptr);
		}

		for (int i = 0; valid && i < numTextures; i++) {
			LString materialName;

			valid = readString(file, fileSize, &materialName) && readInt(file, &_texFlags[i]);

			if (!valid)
				break;

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
			_matHandles[i].setResource(foundMaterial._resource);

			if (_matHandles[i].lock() && _matHandles[i]._typed) {
				_materials[i] = _matHandles[i]._typed;
				_materials[i]->addFlags(_texFlags[i]);
			} else {
				_materials[i] = nullptr;
			}
		}
	}

	int vertexCount = 0;

	valid = valid && readInt(file, &_vertFlags) && readInt(file, &vertexCount);

	if (valid && !isCountValid(vertexCount, 12, fileSize)) {
		warning("RMesh::loadBinary(): invalid vertex count %d", vertexCount);
		valid = false;
	}

	if (valid) {
		allocVertexData(_vertFlags, vertexCount);
		valid = readFloats(file, _pos, 3 * _numVerts);
	}

	if (valid && (_vertFlags & 1))
		valid = readFloats(file, _normals, 3 * _numVerts);

	if (valid && (_vertFlags & 2) && _numVerts > 0) {
		Common::Array<byte> rgba(_numVerts * sizeof(PackedColor));

		valid = readBytes(file, &rgba[0], rgba.size());

		for (int i = 0; valid && i < _numVerts; i++) {
			_colors[i]._r = rgba[i * 4];
			_colors[i]._g = rgba[i * 4 + 1];
			_colors[i]._b = rgba[i * 4 + 2];
			_colors[i]._a = rgba[i * 4 + 3];
		}
	}

	if (valid && (_vertFlags & 4))
		valid = readInts(file, _vertIndex, _numVerts);

	if (valid && (_vertFlags & 0x10))
		valid = readFloats(file, _uv, 2 * _numVerts);

	int numTriSets = 0;
	bool isPS2 = _engine->getGamePlatform() == Common::kPlatformPS2;
	int indexSize = isPS2 ? (int)sizeof(uint32) : (int)sizeof(uint16);

	valid = valid && readInt(file, &numTriSets);

	if (valid && !isCountValid(numTriSets, 12, fileSize)) {
		warning("RMesh::loadBinary(): invalid triangle set count %d", numTriSets);
		valid = false;
	}

	if (valid)
		_trisets.resize(numTriSets, nullptr);

	for (int i = 0; valid && i < numTriSets; i++) {
		TriSet *triset = new TriSet();
		int numIndices = 0;

		_trisets[i] = triset;
		valid = readInt(file, &triset->drawFlags);

		if (valid && !triset->drawFlags)
			triset->drawFlags = kDefaultDrawFlags;

		valid = valid && readInt(file, &triset->numMaterials);

		if (valid && triset->numMaterials > 0)
			valid = readInt(file, &triset->materialIdx);

		valid = valid && readInt(file, &numIndices);

		if (valid && !isCountValid(numIndices, indexSize, fileSize)) {
			warning("RMesh::loadBinary(): invalid index count %d", numIndices);
			valid = false;
		}

		if (valid && numIndices > 0) {
			Common::Array<byte> bytes(numIndices * indexSize);

			valid = readBytes(file, &bytes[0], bytes.size());

			if (valid) {
				triset->indices.resize(numIndices);

				for (int j = 0; j < numIndices; j++) {
					if (isPS2) {
						uint32 index = READ_LE_UINT32(&bytes[j * indexSize]);

						if (index > 0xFFFF)
							warning("RMesh::loadBinary(): index %u does not fit in 16 bits in %s", index, path);

						triset->indices[j] = (uint16)index;
					} else {
						triset->indices[j] = READ_LE_UINT16(&bytes[j * indexSize]);
					}
				}
			}
		}
	}

	int hasWeightMap = 0;

	valid = valid && readInt(file, &hasWeightMap);

	if (valid && hasWeightMap) {
		int numNodes = 0;
		int numWeights = 0;

		_weightMap = new CWeightMap(_engine);
		valid = readInt(file, &numNodes);

		if (valid && !isCountValid(numNodes, 4, fileSize)) {
			warning("RMesh::loadBinary(): invalid node count %d", numNodes);
			valid = false;
		}

		if (valid)
			_weightMap->_nodeNames.resize(numNodes);

		for (int i = 0; valid && i < numNodes; i++)
			valid = readString(file, fileSize, &_weightMap->_nodeNames[i]);

		valid = valid && readInt(file, &numWeights);

		if (valid && !isCountValid(numWeights, 12, fileSize)) {
			warning("RMesh::loadBinary(): invalid weight count %d", numWeights);
			valid = false;
		}

		if (valid && numWeights > 0) {
			Common::Array<byte> bytes(numWeights * 12);

			valid = readBytes(file, &bytes[0], bytes.size());

			if (valid) {
				_weightMap->_weights.resize(numWeights);

				for (int i = 0; i < numWeights; i++) {
					_weightMap->_weights[i].first = (byte)READ_LE_UINT32(&bytes[i * 12]);
					_weightMap->_weights[i].node = READ_LE_INT32(&bytes[i * 12 + 4]);
					_weightMap->_weights[i].weight = READ_LE_FLOAT32(&bytes[i * 12 + 8]);
				}
			}
		}
	}

	if (!valid)
		warning("RMesh::loadBinary(): unexpected end of file in %s", path);

	_engine->getFileServices()->close(file, true);

	return valid;
}

bool RMesh::saveBinary(const char *path) {
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
	writeInt(file, _numTextureSets);

	if (_numTextureSets > 0) {
		// The disasm writes an uninitialised stack value here
		writeInt(file, 0);
		writeInt(file, (int)_matHandles.size());

		for (uint i = 0; i < _matHandles.size(); i++) {
			LString materialName;

			if (_matHandles[i]._resource)
				materialName = _matHandles[i]._resource->_name;
			else
				warning("RMesh::saveBinary(): material %u has no resource", i);

			writeString(file, materialName);

			if (i < _texFlags.size()) {
				writeInt(file, _texFlags[i]);
			} else {
				warning("RMesh::saveBinary(): material %u has no texture flags", i);
				writeInt(file, 0);
			}
		}
	}

	writeInt(file, _vertFlags);
	writeInt(file, _numVerts);

	for (int i = 0; i < 3 * _numVerts; i++)
		writeFloat(file, _pos[i]);

	if (_vertFlags & 1) {
		for (int i = 0; i < 3 * _numVerts; i++)
			writeFloat(file, _normals[i]);
	}

	if (_vertFlags & 2) {
		Common::Array<byte> rgba(_numVerts * sizeof(PackedColor));

		for (int i = 0; i < _numVerts; i++) {
			rgba[i * 4] = _colors[i]._r;
			rgba[i * 4 + 1] = _colors[i]._g;
			rgba[i * 4 + 2] = _colors[i]._b;
			rgba[i * 4 + 3] = _colors[i]._a;
		}

		if (_numVerts > 0)
			writeBytes(file, &rgba[0], rgba.size());
	}

	if (_vertFlags & 4) {
		for (int i = 0; i < _numVerts; i++)
			writeInt(file, _vertIndex[i]);
	}

	if (_vertFlags & 0x10) {
		for (int i = 0; i < 2 * _numVerts; i++)
			writeFloat(file, _uv[i]);
	}

	writeInt(file, (int)_trisets.size());

	for (uint i = 0; i < _trisets.size(); i++) {
		TriSet *triset = _trisets[i];

		writeInt(file, triset->drawFlags);
		writeInt(file, triset->numMaterials);

		if (triset->numMaterials > 0)
			writeInt(file, triset->materialIdx);

		writeInt(file, (int)triset->indices.size());

		for (uint j = 0; j < triset->indices.size(); j++) {
			byte bytes[2];

			WRITE_LE_UINT16(bytes, triset->indices[j]);
			writeBytes(file, bytes, sizeof(bytes));
		}
	}

	if (_weightMap) {
		writeInt(file, 1);
		writeInt(file, (int)_weightMap->_nodeNames.size());

		for (uint i = 0; i < _weightMap->_nodeNames.size(); i++)
			writeString(file, _weightMap->_nodeNames[i]);

		writeInt(file, (int)_weightMap->_weights.size());

		for (uint i = 0; i < _weightMap->_weights.size(); i++) {
			writeInt(file, _weightMap->_weights[i].first);
			writeInt(file, _weightMap->_weights[i].node);
			writeFloat(file, _weightMap->_weights[i].weight);
		}
	} else {
		writeInt(file, 0);
	}

	_engine->getFileServices()->close(file, true);

	return true;
}

} // End of namespace Grim
