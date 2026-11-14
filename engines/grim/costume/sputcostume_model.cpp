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
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/grim.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/sputresource.h"

namespace Grim {

static const int mainThingKeyValues[2] = { 0, 4 };
static const char *const mainThingKeyNames[2] = { "Hide", "Show" };
static const CosComponentNames mainThingKeyDesc = { 2, mainThingKeyValues, mainThingKeyNames, nullptr, nullptr };

static const int thingMeshKeyValues[2] = { 0, 4 };
static const char *const thingMeshKeyNames[2] = { "Hide", "Show" };
static const CosComponentNames thingMeshKeyDesc = { 2, thingMeshKeyValues, thingMeshKeyNames, nullptr, nullptr };

static const int colormapKeyValues[1] = { 0 };
static const char *const colormapKeyNames[1] = { "Exist" };
static const CosComponentNames colormapKeyDesc = { 1, colormapKeyValues, colormapKeyNames, nullptr, nullptr };

void ThingHandler::change(int fourcc, void *instData, int op, int keyValue, int timeOffset, int nextKeyValue, int arg7, int instHandle) {
	ModelInst *inst = static_cast<ModelInst *>(instData);

	if (keyValue < 0 || keyValue >= 2) {
		warning("ThingHandler::change(): invalid key %d", keyValue);
		return;
	}

	inst->visible = mainThingKeyValues[keyValue] != 0;
}

int ThingHandler::request(int costume, int fourcc, void *parentId, int parentFourcc, CosComponent *comp) {
	char name[32];
	int resolveParentOffset = 1;

	memset(name, 0, sizeof(name));

	if (parentId && parentFourcc != kSputCostumeTagMesh)
		return 0;

	// TODO: grimPlat_SelectFilesDialog branch (args == NULL)
	const char *args = comp->name;

	Common::strlcpy(name, args, 32);

	char *modelName = strtok(name, ",");
	char *flag = strtok(nullptr, ",");

	if (!modelName) {
		warning("ThingHandler::request(): empty model name");
		return 0;
	}

	void *model = _sputCostume->_sputResource->alloc(kSputTagModl, modelName);

	if (flag)
		resolveParentOffset = atoi(flag);

	if (!model)
		return 0;

	ThingCompRes *res = new ThingCompRes();

	_sputCostume->_res->setReference(&res->ref, model);
	res->resolveParentOffset = resolveParentOffset;

	return _sputCostume->_cos.registerComponent(costume, args, res, fourcc, parentId, parentFourcc, &mainThingKeyDesc) != 0;
}

void ThingHandler::freeComponent(int costume, int fourcc, void *compRes) {
	ThingCompRes *res = static_cast<ThingCompRes *>(compRes);

	_sputCostume->_res->removeReference(&res->ref);
	delete res;
}

void ThingHandler::stop(void *instData) {
	ModelInst *inst = static_cast<ModelInst *>(instData);

	inst->visible = inst->parentModel == nullptr;
}

int ThingHandler::adjust(void **compRes, char *args, int argsSize) {
	ThingCompRes *res = static_cast<ThingCompRes *>(*compRes);
	int value = res->resolveParentOffset != 0;
	int accepted = 0; // TODO: grimPlat_ControlPanel("Resolve parent offset (0 = off)")

	if (!accepted)
		return 0;

	char *token = strtok(args, ",");

	if (!token) {
		warning("ThingHandler::adjust(): empty component name");
		return 0;
	}

	res->resolveParentOffset = value;

	char text[32];

	snprintf(text, sizeof(text), "%s,%d", token, value);
	Common::strlcpy(token, text, 32);

	return 1;
}

void *ThingHandler::instance(int instHandle, int compIndex, int create) {
	Cos *cos = &_sputCostume->_cos;

	if (!create) {
		ModelInst *modelInst = static_cast<ModelInst *>(cos->instGetInstanceData(instHandle, compIndex, nullptr));

		if (!modelInst) {
			warning("ThingHandler::instance(): no instance data for component %d", compIndex);
			return nullptr;
		}

		modelInst->refCount--;

		if (!modelInst->refCount) {
			_sputCostume->_res->removeReference(&modelInst->modelRef);
			_sputCostume->_rd->_thing.destroy(modelInst->thing);
			delete modelInst;
		}

		return nullptr;
	}

	int fourcc = 0;
	void *id = nullptr;
	int parentIndex = -1;
	CosComponent *comp = cos->instComponentInfo(instHandle, compIndex, &fourcc, &id, &parentIndex);

	if (!comp) {
		warning("ThingHandler::instance(): invalid component %d", compIndex);
		return nullptr;
	}

	int parentInst = cos->instGetUserData(instHandle);

	if (parentInst) {
		int numComponents = cos->instCountComponents(parentInst);

		for (int i = 0; i < numComponents; i++) {
			int parentFourcc = 0;
			CosComponent *parentComp = cos->instComponentInfo(parentInst, i, &parentFourcc, nullptr, nullptr);

			if (parentFourcc == fourcc && parentComp && !scumm_stricmp(parentComp->name, comp->name)) {
				ModelInst *shared = static_cast<ModelInst *>(cos->instGetInstanceData(parentInst, i, nullptr));

				shared->refCount++;

				return shared;
			}
		}
	}

	ThingCompRes *res = static_cast<ThingCompRes *>(id);

	if (!res->ref.valid) {
		warning("ThingHandler::instance(): Bad model Reference");
		return nullptr;
	}

	RdModel3 *model3 = static_cast<RdModel3 *>(res->ref.handle);
	ModelInst *modelInst = new ModelInst();

	_sputCostume->_res->setReference(&modelInst->modelRef, model3);

	RdThing *thing = _sputCostume->_rd->_thing.create(0);

	_sputCostume->_rd->_thing.setModel3(thing, model3);
	thing->puppet = _sputCostume->_rd->_puppet.create(thing);
	modelInst->thing = thing;
	modelInst->resolveParentOffset = res->resolveParentOffset;

	if (parentIndex < 0) {
		modelInst->parentModel = nullptr;
		modelInst->parentMeshIdx = -1;
		modelInst->visible = 1;
	} else {
		MeshInstData *meshInst = static_cast<MeshInstData *>(cos->instGetInstanceData(instHandle, parentIndex, nullptr));

		modelInst->parentModel = meshInst->modelInst;
		modelInst->parentMeshIdx = meshInst->meshIndex;
		modelInst->visible = 0;
	}

	modelInst->refCount++;

	return modelInst;
}

void MeshHandler::change(int fourcc, void *instData, int op, int keyValue, int timeOffset, int nextKeyValue, int arg7, int instHandle) {
	MeshInstData *inst = static_cast<MeshInstData *>(instData);

	if (keyValue < 0 || keyValue >= 2) {
		warning("MeshHandler::change(): invalid key %d", keyValue);
		return;
	}

	RdThing *thing = inst->modelInst->thing;
	int visible = thingMeshKeyValues[keyValue] != 0;

	inst->visible = visible;
	thing->abMeshVisible[inst->meshIndex] = visible;
}

int MeshHandler::request(int costume, int fourcc, void *parentId, int parentFourcc, CosComponent *comp) {
	if (!parentId || (parentFourcc != kSputCostumeTagThing && parentFourcc != kSputCostumeTagMainThing))
		return 0;

	ThingCompRes *parentRes = static_cast<ThingCompRes *>(parentId);

	if (!parentRes->ref.valid) {
		warning("MeshHandler::request(): Bad model Reference");
		return 0;
	}

	RdModel3 *model3 = static_cast<RdModel3 *>(parentRes->ref.handle);

	// TODO: grimPlat_TreeSelectDialog branch (args == NULL)
	const char *args = comp->name;
	const char *digit = args;

	while (*digit && !Common::isDigit(*digit))
		digit++;

	if (!Common::isDigit(*digit))
		return 0;

	int meshIndex = atoi(digit);

	if (meshIndex < 0 || meshIndex >= (int)model3->numHNodes)
		warning("Mesh number out of range for thing!");

	char name[45];

	Common::strlcpy(name, args, sizeof(name));

	uint id = ((uint)(uintptr)parentId & 0xffff) | ((uint)meshIndex << 16);

	return _sputCostume->_cos.registerComponent(costume, name, (void *)(uintptr)id, fourcc, parentId, parentFourcc, &thingMeshKeyDesc) != 0;
}

void MeshHandler::freeComponent(int costume, int fourcc, void *compRes) {
	_sputCostume->stub();
}

void MeshHandler::stop(void *instData) {
	MeshInstData *inst = static_cast<MeshInstData *>(instData);

	inst->visible = 1;
}

int MeshHandler::adjust(void **compRes, char *args, int argsSize) {
	return strchr(args, ')') == nullptr;
}

void *MeshHandler::instance(int instHandle, int compIndex, int create) {
	Cos *cos = &_sputCostume->_cos;

	if (!create) {
		MeshInstData *meshInst = static_cast<MeshInstData *>(cos->instGetInstanceData(instHandle, compIndex, nullptr));

		delete meshInst;

		return nullptr;
	}

	int fourcc = 0;
	void *id = nullptr;
	int parentIndex = -1;

	cos->instComponentInfo(instHandle, compIndex, &fourcc, &id, &parentIndex);

	MeshInstData *meshInst = new MeshInstData();

	meshInst->meshIndex = (int)(intptr)id >> 16;
	meshInst->modelInst = static_cast<ModelInst *>(cos->instGetInstanceData(instHandle, parentIndex, nullptr));
	meshInst->visible = 1;

	return meshInst;
}

void MaterialHandler::change(int fourcc, void *instData, int op, int keyValue, int timeOffset, int nextKeyValue, int arg7, int instHandle) {
	MaterialInst *inst = static_cast<MaterialInst *>(instData);

	inst->key = keyValue;
}

int MaterialHandler::request(int costume, int fourcc, void *parentId, int parentFourcc, CosComponent *comp) {
	if (parentFourcc == kSputCostumeTagSprite) {
		warning("MaterialHandler::request(): a material cannot be attached to a sprite");
		return 0;
	}

	if (parentFourcc != kSputCostumeTagThing && parentFourcc != kSputCostumeTagMainThing)
		return 0;

	if (!parentId) {
		warning("MaterialHandler::request(): no parent component");
		return 0;
	}

	ThingCompRes *parentRes = static_cast<ThingCompRes *>(parentId);

	if (!parentRes->ref.valid) {
		warning("MaterialHandler::request(): Bad model Reference");
		return 0;
	}

	RdModel3 *model3 = static_cast<RdModel3 *>(parentRes->ref.handle);

	// TODO: grimPlat_SelectFilesDialog branch (args == NULL)
	const char *args = comp->name;
	uint materialIndex = 0;

	if (model3->numMaterials) {
		do {
			RdMaterial *material = model3->materials[materialIndex];

			if (material && !scumm_stricmp(args, material->name))
				break;

			materialIndex++;
		} while (materialIndex < model3->numMaterials);
	}

	if (materialIndex == model3->numMaterials)
		return 0;

	RdMaterial *material = model3->materials[materialIndex];

	if (_sputCostume->_lecBitmaps)
		delete[] _sputCostume->_lecBitmaps;

	_sputCostume->_lecBitmaps = new CosStateBitmapDesc[material->numCels]();

	for (uint i = 0; i < material->numCels; i++) {
		RdMatCel *cel = material->cels[i];

		if (!cel || !cel->texture || !cel->texture->mips[0]) {
			warning("MaterialHandler::request(): cel %u of %s has no texture", i, material->name);
			continue;
		}

		StdVBuffer *frame = cel->texture->mips[0];

		_sputCostume->_lecBitmaps[i].pixels = frame->pixels;
		_sputCostume->_lecBitmaps[i].width = frame->tex.width;
		_sputCostume->_lecBitmaps[i].height = frame->tex.height;
		_sputCostume->_lecBitmaps[i].reserved12 = 1;
	}

	CosComponentNames keyDesc = { (int)material->numCels, nullptr, nullptr, _sputCostume->_lecBitmaps, nullptr };

	return _sputCostume->_cos.registerComponent(costume, args, (void *)(uintptr)materialIndex, fourcc, parentId, parentFourcc, &keyDesc) != 0;
}

void MaterialHandler::freeComponent(int costume, int fourcc, void *compRes) {
	_sputCostume->stub();
}

void *MaterialHandler::instance(int instHandle, int compIndex, int create) {
	Cos *cos = &_sputCostume->_cos;

	if (!create) {
		MaterialInst *inst = static_cast<MaterialInst *>(cos->instGetInstanceData(instHandle, compIndex, nullptr));

		delete inst;

		return nullptr;
	}

	MaterialInst *inst = new MaterialInst();
	int fourcc = 0;
	int parentIndex = -1;

	cos->instComponentInfo(instHandle, compIndex, &fourcc, &inst->compRes, &parentIndex);
	inst->modelInst = static_cast<ModelInst *>(cos->instGetInstanceData(instHandle, parentIndex, nullptr));
	inst->key = 0;

	return inst;
}

void MaterialHandler::saveLoad(int instHandle, int compIndex, int save, void *instData, Common::WriteStream *write, Common::ReadStream *read) {
	MaterialInst *inst = static_cast<MaterialInst *>(instData);

	if (save)
		write->writeSint32LE(inst->key);
	else
		inst->key = read->readSint32LE();
}

void ColormapHandler::change(int fourcc, void *instData, int op, int keyValue, int timeOffset, int nextKeyValue, int arg7, int instHandle) {
	_sputCostume->nullSub1();
}

int ColormapHandler::request(int costume, int fourcc, void *parentId, int parentFourcc, CosComponent *comp) {
	if (parentFourcc != kSputCostumeTagThing && parentFourcc != kSputCostumeTagMainThing && parentFourcc != kSputCostumeTagSprite)
		return 0;

	// TODO: grimPlat_SelectFilesDialog branch (args == NULL)
	const char *args = comp->name;
	void *colormap = _sputCostume->_sputResource->alloc(kSputTagCmap, args);

	if (!colormap)
		return 0;

	ResRef *res = new ResRef();

	_sputCostume->_res->setReference(res, colormap);

	return _sputCostume->_cos.registerComponent(costume, args, res, fourcc, parentId, parentFourcc, &colormapKeyDesc) != 0;
}

void ColormapHandler::freeComponent(int costume, int fourcc, void *compRes) {
	ResRef *res = static_cast<ResRef *>(compRes);

	_sputCostume->_res->removeReference(res);
	delete res;
}

void *ColormapHandler::instance(int instHandle, int compIndex, int create) {
	Cos *cos = &_sputCostume->_cos;

	if (!create) {
		ColormapInst *inst = static_cast<ColormapInst *>(cos->instGetInstanceData(instHandle, compIndex, nullptr));

		_sputCostume->_res->removeReference(&inst->ref);
		delete inst;

		return nullptr;
	}

	ColormapInst *inst = new ColormapInst();
	int fourcc = 0;
	void *id = nullptr;
	int parentIndex = -1;

	cos->instComponentInfo(instHandle, compIndex, &fourcc, &id, &parentIndex);

	ResRef *res = static_cast<ResRef *>(id);

	if (!res->valid)
		warning("ColormapHandler::instance(): Bad Colormap Reference");

	_sputCostume->_res->setReference(&inst->ref, res->handle);
	inst->modelInst = static_cast<ModelInst *>(cos->instGetInstanceData(instHandle, parentIndex, nullptr));

	return inst;
}

void SpriteHandler::change(int fourcc, void *instData, int op, int keyValue, int timeOffset, int nextKeyValue, int arg7, int instHandle) {
	ModelInst *inst = static_cast<ModelInst *>(instData);

	if (keyValue) {
		inst->visible = 1;
		inst->sprite->face.material->curCel = keyValue - 1;
	} else {
		inst->visible = 0;
	}
}

int SpriteHandler::request(int costume, int fourcc, void *parentId, int parentFourcc, CosComponent *comp) {
	char name[32];
	float width = 0.0f;
	float height = 0.0f;
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float offsetZ = 0.0f;

	memset(name, 0, sizeof(name));

	if (!parentId || parentFourcc != kSputCostumeTagMesh)
		return 0;

	// TODO: grimPlat_SelectFilesDialog branch (args == NULL)
	const char *args = comp->name;

	Common::strlcpy(name, args, 32);
	strtok(name, ",");

	char *token = strtok(nullptr, ",");

	if (token) {
		bool remastered = g_grim->isRemastered();

		if (remastered)
			width = (float)((double)atoi(token) / 100.0);
		else
			width = (float)((double)atoi(token) * (double)0.01f);

		const char *tokens[4];

		for (int i = 0; i < 4; i++) {
			tokens[i] = strtok(nullptr, ",");

			if (!tokens[i])
				warning("SpriteHandler::request(): missing sprite parameter in %s", args);
		}

		float *values[4] = { &height, &offsetX, &offsetY, &offsetZ };

		for (int i = 0; i < 4; i++) {
			if (!tokens[i])
				continue;

			if (remastered)
				*values[i] = (float)((double)atoi(tokens[i]) / 100.0);
			else
				*values[i] = (float)((double)atoi(tokens[i]) * (double)0.01f);
		}
	}

	SpriteCompRes *res = new SpriteCompRes();
	RdVector3 offset;

	offset[0] = offsetX;
	offset[1] = offsetY;
	offset[2] = offsetZ;

	res->sprite = _sputCostume->_rd->_sprite.create(0, name, name, width, height, 5, 0, 1, 0.0f, &offset);
	Common::strlcpy(res->name, name, 32);

	if (!res->sprite || !res->sprite->face.material) {
		warning("SpriteHandler::request(): sprite %s has no material", name);
		return 0;
	}

	CosComponentNames keyDesc = { (int)res->sprite->face.material->numCels + 1, nullptr, nullptr, nullptr, nullptr };

	return _sputCostume->_cos.registerComponent(costume, args, res, fourcc, parentId, kSputCostumeTagMesh, &keyDesc) != 0;
}

void SpriteHandler::freeComponent(int costume, int fourcc, void *compRes) {
	SpriteCompRes *res = static_cast<SpriteCompRes *>(compRes);

	_sputCostume->_rd->_material.destroy(res->sprite->face.material);
	_sputCostume->_rd->_sprite.destroy(res->sprite);
	delete res;
}

int SpriteHandler::adjust(void **compRes, char *args, int argsSize) {
	SpriteCompRes *res = static_cast<SpriteCompRes *>(*compRes);
	RdSprite *sprite = res->sprite;
	int values[5];

	values[0] = (int)(int64)((double)sprite->width * 100.0 - (double)-0.2f);
	values[1] = (int)(int64)((double)sprite->height * 100.0 - (double)-0.2f);
	values[2] = (int)(int64)((double)sprite->offset[0] * 100.0 - (double)-0.2f);
	values[3] = (int)(int64)((double)sprite->offset[1] * 100.0 - (double)-0.2f);
	values[4] = (int)(int64)((double)sprite->offset[2] * 100.0 - (double)-0.2f);

	int accepted = 0; // TODO: grimPlat_ControlPanel("Width", "Height", "X", "Y", "Z")

	if (!accepted)
		return 0;

	sprite->width = (float)((double)values[0] * (double)0.01f);
	sprite->height = (float)((double)values[1] * (double)0.01f);
	sprite->halfWidth = (float)((double)sprite->width * (double)0.5f);
	sprite->halfHeight = (float)((double)sprite->height * (double)0.5f);
	sprite->offset[0] = (float)((double)values[2] * (double)0.01f);
	sprite->offset[1] = (float)((double)values[3] * (double)0.01f);
	sprite->offset[2] = (float)((double)values[4] * (double)0.01f);

	char *token = strtok(args, ",");

	if (!token) {
		warning("SpriteHandler::adjust(): empty component name");
		return 0;
	}

	char text[32];

	snprintf(text, sizeof(text), "%s,%d,%d,%d,%d,%d", token, values[0], values[1], values[2], values[3], values[4]);
	Common::strlcpy(token, text, 32);

	return 1;
}

void *SpriteHandler::instance(int instHandle, int compIndex, int create) {
	Cos *cos = &_sputCostume->_cos;

	if (!create) {
		ModelInst *modelInst = static_cast<ModelInst *>(cos->instGetInstanceData(instHandle, compIndex, nullptr));

		modelInst->refCount--;

		if (!modelInst->refCount) {
			_sputCostume->_rd->_thing.destroy(modelInst->thing);
			delete modelInst;
		}

		return nullptr;
	}

	int fourcc = 0;
	void *id = nullptr;
	int parentIndex = -1;

	cos->instComponentInfo(instHandle, compIndex, &fourcc, &id, &parentIndex);

	SpriteCompRes *res = static_cast<SpriteCompRes *>(id);
	ModelInst *modelInst = new ModelInst();

	modelInst->sprite = res->sprite;

	RdThing *thing = _sputCostume->_rd->_thing.create(0);

	modelInst->thing = thing;
	thing->curCel = 0;
	_sputCostume->_rd->_thing.setSprite3(thing, res->sprite);

	if (parentIndex < 0) {
		modelInst->parentModel = nullptr;
		modelInst->refCount++;
		modelInst->parentMeshIdx = -1;
		modelInst->visible = 1;
	} else {
		MeshInstData *meshInst = static_cast<MeshInstData *>(cos->instGetInstanceData(instHandle, parentIndex, nullptr));

		modelInst->parentModel = meshInst->modelInst;
		modelInst->parentMeshIdx = meshInst->meshIndex;
		modelInst->refCount++;
		modelInst->visible = 0;
	}

	return modelInst;
}

} // End of namespace Grim
