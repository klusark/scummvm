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

#include "engines/grim/grim.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/costume/sputcostume_components.h"
#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"
#include "engines/grim/walkplane.h"

namespace Grim {

int SputActorModule::actorHasShadow(SputActor *actor) {
	for (int i = 0; i < 6; i++) {
		if (actor->extra->shadows[i].active)
			return 1;
	}

	return 0;
}

void SputActorModule::activateActorShadow(SputActor *actor, int shadowIdx, int active) {
	if (shadowIdx < 0 || shadowIdx >= 6) {
		warning("SputActorModule::activateActorShadow(): invalid shadow index %d", shadowIdx);
		return;
	}

	actor->extra->shadows[shadowIdx].active = active;
}

void SputActorModule::setShadowPlane(SputActor *actor, const char *planeName) {
	int shadowIdx = actor->extra->activeShadow;

	if (shadowIdx < 0 || shadowIdx >= 6) {
		warning("SputActorModule::setShadowPlane(): invalid active shadow %d", shadowIdx);
		return;
	}

	ActorShadow *shadow = &actor->extra->shadows[shadowIdx];
	WalkPlane *plane = _walkSystem->planeNamed(planeName);

	if (!plane)
		return;

	shadow->normal[0] = plane->_normal[0];
	shadow->normal[1] = plane->_normal[1];
	shadow->normal[2] = plane->_normal[2];

	WpVertex *vertex = plane->_verts[0];
	double zTerm = (double)vertex->z * shadow->normal[2];
	double yTerm = (double)vertex->y * shadow->normal[1];
	double xTerm = (double)vertex->x * shadow->normal[0];

	shadow->active = 1;
	shadow->side = 0;
	shadow->planeD = (float)-(xTerm + (yTerm + zTerm));
}

void SputActorModule::setShadowPoint(SputActor *actor, float x, float y, float z) {
	int shadowIdx = actor->extra->activeShadow;

	if (shadowIdx < 0 || shadowIdx >= 6) {
		warning("SputActorModule::setShadowPoint(): invalid active shadow %d", shadowIdx);
		return;
	}

	ActorShadow *shadow = &actor->extra->shadows[shadowIdx];

	shadow->lightPos[0] = x;
	shadow->lightPos[1] = y;
	shadow->lightPos[2] = z;
}

void SputActorModule::setShadowValid(SputActor *actor, int shadowIdx, int side) {
	if (shadowIdx < 0 || shadowIdx >= 6) {
		warning("SputActorModule::setShadowValid(): invalid shadow index %d", shadowIdx);
		return;
	}

	actor->extra->shadows[shadowIdx].side = side;
}

void SputActorModule::addShadowClipPlane(SputActor *actor, int shadowIdx, const char *planeName) {
	WalkPlane *plane = _walkSystem->planeNamed(planeName);

	if (!plane)
		return;

	if (shadowIdx < 0 || shadowIdx >= 6) {
		warning("SputActorModule::addShadowClipPlane(): invalid shadow index %d", shadowIdx);
		return;
	}

	ActorShadow *shadow = &actor->extra->shadows[shadowIdx];

	for (int i = 0; i < 12; i++) {
		if (!shadow->clipPlane[i]) {
			shadow->clipPlane[i] = plane;
			return;
		}
	}
}

void SputActorModule::resetShadows(SputActor *actor) {
	if (actor && actor->extra)
		memset(actor->extra->shadows, 0, sizeof(actor->extra->shadows));
}

void SputActorModule::loadShadowResources(SputActor *actor) {
	if (_rd->getRemasteredLookRenderer() && !_rd->getRemasteredLookRenderer()->isSoftwareEnabled())
		return;

	if (!actor->extra)
		return;

	_grim->setPerfCounter(11);

	for (int i = 0; i < 6; i++) {
		ActorShadow *shadow = &actor->extra->shadows[i];

		if (!shadow->active)
			continue;

		int side = shadow->side;
		bool init = true;

		if (side) {
			double distance = (double)actor->actorPos[0] * shadow->normal[0] + (double)actor->actorPos[2] * shadow->normal[2];

			distance = distance + (double)actor->actorPos[1] * shadow->normal[1];
			distance = distance + shadow->planeD;

			if (side == -1 && distance > 0.0)
				init = false;
			else if (side == 1 && distance < 0.0)
				init = false;
		}

		if (init)
			initShadow(actor->costumeStack, shadow);
	}

	_grim->queryPerfCounterDiff(11);
}

void SputActorModule::initShadow(CostumeNode *nodePointer, ActorShadow *shadow) {
	if (!nodePointer)
		return;

	float minArray[2] = { 640.0f, 480.0f };
	float maxArray[2] = { 0.0f, 0.0f };
	bool demo = (_grim->getGameFlags() & ADGF_DEMO) != 0;

	for (ModelInst *modelInst = nodePointer->headList; modelInst; modelInst = modelInst->next) {
		RdThing *thing = modelInst->thing;

		if (!modelInst->visible || modelInst->sprite)
			continue;

		RdModel3 *model3 = thing->data.model3;

		for (uint node = 0; node < model3->numHNodes; node++) {
			int meshNodeIndex = model3->hNodes[node].meshNodeIndex;

			if (meshNodeIndex == -1)
				continue;

			RdModel3Mesh *mesh = &model3->geo[0].meshes[meshNodeIndex];

			if (!mesh->castShadow || !thing->abMeshVisible[meshNodeIndex])
				continue;

			RdVector3 quads[16];
			int vertex = 0;

			while (vertex < 16) {
				// The shadows of the demo have three quads, the second one is missing
				if (demo && vertex == 4) {
					vertex = 8;
					continue;
				}

				_rd->transformPoint34(quads[vertex], mesh->shadowVerts[vertex], thing->jointMatrices[node]);

				// The demo goes on when the projection fails
				if (!_sputRender->buildShadowBitmap(quads[vertex], shadow->lightPos, quads[vertex], shadow->normal[0], shadow->normal[1], shadow->normal[2], shadow->planeD) && !demo)
					break;

				vertex++;
			}

			if (vertex < 16)
				continue;

			if (!_shadowMaterial) {
				if (demo) {
					_shadowMaterial = &_demoShadowMaterial;
				} else {
					char materialName[] = "shadow.mat";

					_shadowMaterial = _rd->_material.load(materialName, 0, 0);
				}
			}

			RdColormap *colormap = _sputCostume->getShadowColormap();

			if (!colormap && demo) {
				_sputCostume->setShadowColor(0, 0, 0);
				colormap = _sputCostume->getShadowColormap();
			} else if (!colormap) {
				char colormapName[] = "shadow.cmp";

				colormap = _rd->_colormap.load(colormapName);
				_sputCostume->setShadowColormap(colormap);
			}

			if (_shadowMaterial) {
				_rd->_colormap.setCurrent(colormap);

#if defined(GRIM_STD3D)
				if (_sputRender->is3DHardware()) {
					if (colormap)
						_std3D->setCurrentPalette(colormap->colors, 0);
					else
						warning("SputActorModule::initShadow(): the shadow colormap could not be loaded");
				}
#endif

				drawShadowPolygon(quads[0], 4, minArray, maxArray, shadow);

				if (!demo)
					drawShadowPolygon(quads[4], 4, minArray, maxArray, shadow);

				drawShadowPolygon(quads[8], 4, minArray, maxArray, shadow);
				drawShadowPolygon(quads[12], 4, minArray, maxArray, shadow);
			}
		}
	}

	if (!(maxArray[0] < minArray[0])) {
		StdRect rect;

		rect.x = (int)minArray[0];
		rect.y = (int)minArray[1];
		rect.width = (int)(maxArray[0] - minArray[0]);
		rect.height = (int)(maxArray[1] - minArray[1]);
		_sputRender->markDirtyRect(&rect, 1);
	}
}

void SputActorModule::drawShadowPolygon(const float *poly, int numPoly, float *minPointer, float *maxPointer, ActorShadow *shadow) {
	RdVector3 clipped[12];

	for (int i = 0; i < 12; i++) {
		WalkPlane *plane = shadow->clipPlane[i];

		if (!plane)
			break;

		int numClipped = plane->clipPoly(poly, numPoly, clipped[0]);

		if (numClipped > 2) {
			RdProcFace *face = nullptr;
			RemasteredLookRenderer *remasteredLookRenderer = _rd->getRemasteredLookRenderer();

			if (!remasteredLookRenderer || !remasteredLookRenderer->drawShadowGeometry(clipped[0], numClipped, 0))
				face = _rd->_primit3.drawPolygon(clipped, numClipped, _shadowMaterial);

			if (face) {
				double xMin = (double)(uint)face->xMin;

				if (xMin > minPointer[0])
					xMin = minPointer[0];

				minPointer[0] = (float)xMin;

				double yMin = (double)(uint)face->yMin;

				if (minPointer[1] < yMin)
					yMin = minPointer[1];

				minPointer[1] = (float)yMin;

				double xMax = (double)(uint)face->xMax;

				if (xMax < maxPointer[0])
					xMax = maxPointer[0];

				maxPointer[0] = (float)xMax;

				double yMax = (double)(uint)face->yMax;

				if (maxPointer[1] > yMax)
					yMax = maxPointer[1];

				maxPointer[1] = (float)yMax;
			}
		}
	}
}

} // End of namespace Grim
