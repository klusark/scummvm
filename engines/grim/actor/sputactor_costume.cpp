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

#include "engines/grim/chore/cos.h"
#include "engines/grim/grim.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/costume/sputcostume_components.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"
#include "engines/grim/std/std_display.h"

namespace Grim {

float (*SputActorModule::getOrientMatrix(SputActor *actor))[3] {
	if (!actor->matrixValid) {
		_rd->build34(actor->orientMatrix, actor->actorRot, actor->actorPos);
		actor->matrixValid = 1;
	}

	return actor->orientMatrix;
}

void SputActorModule::drawToClean(SputActor *actor) {
	Room *room = _sputRoom->getCurrentRoom();
	RoomSetup *setup = &room->setups[room->curSetup];
	RdCanvas *canvas = setup->camera.prdCamera->canvas;

	if (!canvas)
		return;

	StdVBuffer *vBuffer = canvas->vBuffer;
	StdVBuffer *zBuffer = canvas->zBuffer;

	canvas->vBuffer = _sputRender->cleanArtBuffer();
	canvas->zBuffer = _sputRender->cleanZBuffer();

	RdColormap *colormap;

	if (actor->colorMap->valid) {
		colormap = static_cast<RdColormap *>(actor->colorMap->handle);
	} else {
		ResRef *colormapRef = &room->colormaps[room->curColormap];

		if (!colormapRef->valid)
			error("SputActorModule::drawToClean(): Bad Colormap Reference");

		colormap = static_cast<RdColormap *>(colormapRef->handle);
	}

	_rd->_colormap.setCurrent(colormap);

	SputRenderPaletteFunc paletteFunc = _sputRender->getPaletteFunc();

	if (paletteFunc)
		paletteFunc(colormap->colors, 0);

	if (actor->scale != 1.0f) {
		RdVector3 scale;

		scale[0] = actor->scale;
		scale[1] = actor->scale;
		scale[2] = actor->scale;
		_rd->preScale34(getOrientMatrix(actor), scale);
	}

	drawCostume(actor->costumeStack, getOrientMatrix(actor));
	actor->matrixValid = 0;
	_rd->_cache.flush();
	canvas->vBuffer = vBuffer;
	canvas->zBuffer = zBuffer;
}

void SputActorModule::drawCostume(CostumeNode *node, RdMatrix34 matrix) {
	drawCostumeImpl(node, matrix);
}

void SputActorModule::drawCostumeImpl(CostumeNode *node, RdMatrix34 matrix) {
	int b3DAcceleration = _sputRender->is3DHardware();
	bool demo = _rd->isDemo();
	RdThing *headThing = node->headThing;

	for (CostumeNode *current = node; current; current = current->next) {
		if (current->headThing != headThing)
			break;

		syncCostumeComponents(current->costumeInstance);
	}

	SputActor *actor = &_actors[_cos->instGetUserData(node->costumeInstance)];

	if (actor->extra && actor->extra->clipPlaneActive) {
		_rd->_model3.activateClipPlane(1);
		_rd->_model3.setClipPlane(actor->extra->clipPlaneNorm, actor->extra->clipPlanePt);
	} else {
		_rd->_model3.activateClipPlane(0);
	}

	applyPendingScale(actor, node->headList);

	for (ModelInst *modelInst = node->headList; modelInst; modelInst = modelInst->next) {
		if (!modelInst->visible)
			continue;

		int *viewport = _rd->_cache.getViewportRect();

		viewport[0] = 640;
		viewport[1] = 480;
		viewport[2] = 0;
		viewport[3] = 0;

		if (modelInst->parentModel) {
			RdColormap *previousColormap = _rd->_colormap.getCurrent();
			ModelInst *colormapModel = modelInst;

			// The demo only uses the colormap of the model itself
			if (!demo) {
				do {
					if (colormapModel->colormap)
						break;

					colormapModel = colormapModel->parentModel;
				} while (colormapModel);
			}

			if (colormapModel && colormapModel->colormap) {
				_rd->_colormap.setCurrent(colormapModel->colormap);

#if defined(GRIM_STD3D)
				if (b3DAcceleration)
					_std3D->setCurrentPalette(colormapModel->colormap->colors, 0);
#endif
			}

			if (!demo) {
				buildJointMatricesRec(modelInst->parentModel, matrix);
			} else if (modelInst->parentModel->thing->jointsFrame != _rd->getFrameTrue()) {
				// The demo builds only the parent, and only if it is not up to date
				_rd->_puppet.buildJointMatrices(modelInst->parentModel->thing, matrix);
			}

			RdThing *parentThing = modelInst->parentModel->thing;
			RdModel3HNode *parentNode = &parentThing->data.model3->hNodes[modelInst->parentMeshIdx];
			RdModel3HNode *childNode = modelInst->thing->data.model3->hNodes;
			float insertOffsetArray[3] = { 0.0f, 0.0f, 0.0f };

			if (!b3DAcceleration)
				_stdDisplay->vBufferLock(_sputRender->drawBuffer());

			if (modelInst->resolveParentOffset && modelInst->thing->type != 4) {
				float *insertOffset = modelInst->thing->data.model3->insertOffset;

				insertOffsetArray[0] = insertOffset[0];
				insertOffsetArray[1] = insertOffset[1];
				insertOffsetArray[2] = insertOffset[2];
				insertOffset[0] = 0.0f;
				insertOffset[1] = 0.0f;
				insertOffset[2] = 0.0f;
				childNode->pivot[0] = childNode->pivot[0] - parentNode->pivot[0];
				childNode->pivot[1] = childNode->pivot[1] - parentNode->pivot[1];
				childNode->pivot[2] = childNode->pivot[2] - parentNode->pivot[2];
			}

			if (!demo)
				actor->onScreen = 0;

			if (actor->extra && actor->extra->subClipNode == modelInst->parentMeshIdx) {
				_rd->_model3.activateClipPlane(1);
				_rd->_model3.setClipPlane(actor->orientMatrix[1], actor->actorPos);
			}

			if (_rd->_thing.draw(modelInst->thing, parentThing->jointMatrices[modelInst->parentMeshIdx], parentThing))
				actor->onScreen = 1;

			if (actor->extra && actor->extra->subClipNode == modelInst->parentMeshIdx)
				_rd->_model3.activateClipPlane(0);

			if (modelInst->resolveParentOffset && modelInst->thing->type != 4) {
				float *insertOffset = modelInst->thing->data.model3->insertOffset;

				childNode->pivot[0] = parentNode->pivot[0] + childNode->pivot[0];
				childNode->pivot[1] = parentNode->pivot[1] + childNode->pivot[1];
				childNode->pivot[2] = parentNode->pivot[2] + childNode->pivot[2];
				insertOffset[0] = insertOffsetArray[0];
				insertOffset[1] = insertOffsetArray[1];
				insertOffset[2] = insertOffsetArray[2];
			}

			if (!b3DAcceleration)
				_stdDisplay->vBufferUnlock(_sputRender->drawBuffer());

			if (!demo && g_grim->useRemasterFixes())
				_rd->_colormap.setCurrent(previousColormap);
		} else {
			if (modelInst->colormap) {
				_rd->_colormap.setCurrent(modelInst->colormap);

#if defined(GRIM_STD3D)
				if (b3DAcceleration)
					_std3D->setCurrentPalette(modelInst->colormap->colors, 0);
#endif
			}

			if (!b3DAcceleration)
				_stdDisplay->vBufferLock(_sputRender->drawBuffer());

			int drawn = _rd->_thing.draw(modelInst->thing, matrix) != 0;

			// The demo only sets the flag, the sorting clears it
			if (drawn || !demo)
				actor->onScreen = drawn;

			if (!b3DAcceleration)
				_stdDisplay->vBufferUnlock(_sputRender->drawBuffer());
		}

		if (actor->colorMap->valid) {
			RdColormap *actorColormap = static_cast<RdColormap *>(actor->colorMap->handle);

			_rd->_colormap.setCurrent(actorColormap);

#if defined(GRIM_STD3D)
			if (b3DAcceleration)
				_std3D->setCurrentPalette(actorColormap->colors, 0);
#endif
		}

		StdRect rect;

		rect.x = viewport[0];
		rect.y = viewport[1];
		rect.width = viewport[2] - viewport[0];
		rect.height = viewport[3] - viewport[1];
		_sputRender->markDirtyRect(&rect, 1);

		if (modelInst->thing == node->headThing)
			actor->drawnRect = rect;
	}

	loadShadowResources(actor);

	for (CostumeNode *current = node; current; current = current->next) {
		if (current->headThing != headThing)
			break;

		syncCostumeComponents(current->costumeInstance);
	}

	applyPendingScale(actor, node->headList);
}

void SputActorModule::applyPendingScale(SputActor *actor, ModelInst *list) {
	for (ModelInst *modelInst = list; modelInst; modelInst = modelInst->next) {
		if (actor->scale == 1.0f)
			continue;

		RdThing *thing = modelInst->thing;
		RdModel3 *model3 = thing->data.model3;

		model3->radius = model3->radius * actor->scale;

		if (thing->type == 1) {
			for (uint i = 0; i < model3->geo[model3->curGeoSet].numMeshes; i++) {
				RdModel3Mesh *mesh = &model3->geo[model3->curGeoSet].meshes[i];

				mesh->radius = actor->scale * mesh->radius;
			}
		}

		double difference = 1.0 - 1.0 / (double)actor->scale;

		actor->scale = (float)(1.0 / (double)actor->scale);

		double absDifference = difference;

		if (difference < 0.0)
			absDifference = -difference;

		if (absDifference <= 0.0000099999997)
			difference = 0.0;

		if (difference == 0.0)
			actor->scale = 1.0f;
	}
}

void SputActorModule::syncCostumeComponents(int instHandle) {
	int numComponents = _cos->instCountComponents(instHandle);

	for (int i = 0; i < numComponents; i++) {
		int fourcc = 0;
		void *instData = _cos->instGetInstanceData(instHandle, i, &fourcc);

		if (fourcc == kSputCostumeTagMaterial) {
			MaterialInst *inst = static_cast<MaterialInst *>(instData);
			RdModel3 *model3 = inst->modelInst->thing->data.model3;
			RdMaterial *material = model3->materials[(intptr)inst->compRes];
			int curCel = material->curCel;

			material->curCel = inst->key;
			inst->key = curCel;
		} else if (fourcc == kSputCostumeTagColormap) {
			ColormapInst *inst = static_cast<ColormapInst *>(instData);

			if (!inst->ref.valid)
				error("SputActorModule::syncCostumeComponents(): Bad Colormap Reference");

			inst->modelInst->colormap = static_cast<RdColormap *>(inst->ref.handle);
		}
	}
}

void SputActorModule::buildJointMatricesRec(ModelInst *inst, RdMatrix34 matrix) {
	ModelInst *parentModel = inst->parentModel;
	RdThing *thing = inst->thing;

	if (parentModel) {
		buildJointMatricesRec(parentModel, matrix);
		_rd->_puppet.buildJointMatrices(thing, parentModel->thing->jointMatrices[inst->parentMeshIdx]);
	} else {
		_rd->_puppet.buildJointMatrices(thing, matrix);
	}
}

void SputActorModule::enumCostumeComponents(int inst) {
	_sputRoom->getCurrentRoom();

	int numComponents = _cos->instCountComponents(inst);

	for (int i = 0; i < numComponents; i++) {
		int fourcc = 0;

		_cos->instGetInstanceData(inst, i, &fourcc);
	}
}

} // End of namespace Grim
