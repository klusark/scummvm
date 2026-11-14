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

#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/renderdroid/rd.h"

namespace Grim {

RdThing *RenderDroid::ThingModule::create(void *userArg) {
	RdThing *thing = new RdThing;

	if (!newEntry(thing, userArg))
		return nullptr;

	return thing;
}

int RenderDroid::ThingModule::newEntry(RdThing *thing, void *userArg) {
	thing->curGeoSet = -1;
	thing->curCel = -1;
	thing->data.model3 = nullptr;
	thing->textureMode = 2;
	thing->curTextureMode = 2;
	thing->type = 0;
	thing->puppet = nullptr;
	thing->jointsLocked = 0;
	thing->jointsFrame = 0;
	thing->geometryMode = 4;
	thing->lightingMode = 3;
	thing->curGeometryMode = 4;
	thing->curLightingMode = 3;
	thing->frustrumCull = 1;
	thing->jointMatrices = nullptr;
	thing->abJointAmputate = nullptr;
	thing->userArg = userArg;
	thing->abMeshVisible = nullptr;

	return 1;
}

void RenderDroid::ThingModule::destroy(RdThing *&thing) {
	if (thing) {
		freeEntry(thing);
		delete thing;
		thing = nullptr;
	}
}

void RenderDroid::ThingModule::freeEntry(RdThing *thing) {
	if (thing->type == 1) {
		if (thing->jointMatrices) {
			delete[] thing->jointMatrices;
			thing->jointMatrices = nullptr;
		}
		if (thing->jointPyr) {
			delete[] thing->jointPyr;
			thing->jointPyr = nullptr;
		}
		if (thing->abMeshVisible) {
			delete[] thing->abMeshVisible;
			thing->abMeshVisible = nullptr;
		}
		if (thing->abJointAmputate) {
			delete[] thing->abJointAmputate;
			thing->abJointAmputate = nullptr;
		}
		// The demo frees the puppet without removing the tracks
		if (thing->puppet && !_rd->isDemo()) {
			for (int i = 0; i < _rd->_puppet.getNumTracks(); i++) {
				if (thing->puppet->tracks[i].status)
					_rd->_puppet.removeTrack(thing->puppet, i);
			}
		}
	}

	if (thing->puppet) {
		delete thing->puppet;
		thing->puppet = nullptr;
	}
}

int RenderDroid::ThingModule::setModel3(RdThing *thing, RdModel3 *model3) {
	thing->type = 1;
	thing->data.model3 = model3;
	thing->curGeoSet = -1;
	thing->jointMatrices = new RdMatrix34[model3->numHNodes];
	thing->jointPyr = new RdVector3[model3->numHNodes];
	memset(thing->jointPyr, 0, 12 * model3->numHNodes);

	thing->abJointAmputate = new int[model3->numHNodes];
	memset(thing->abJointAmputate, 0, 4 * model3->numHNodes);

	thing->abMeshVisible = new int[model3->numHNodes];
	for (uint i = 0; i < model3->numHNodes; i++)
		thing->abMeshVisible[i] = 1;

	for (uint i = 0; i < model3->numHNodes; i++) {
		RdModel3HNode *hNode = &model3->hNodes[i];
		_rd->build34(hNode->mat, hNode->pyr, hNode->xyz);
	}

	return 1;
}

int RenderDroid::ThingModule::setCamera(RdThing *thing, RdCamera *camera) {
	thing->type = 2;
	thing->data.camera = camera;
	return 1;
}

int RenderDroid::ThingModule::setSprite3(RdThing *thing, RdSprite *sprite) {
	thing->type = 4;
	thing->data.sprite = sprite;
	thing->curCel = -1;
	return 1;
}

void RenderDroid::ThingModule::accumulateMatrices(RdThing *thing, RdModel3HNode *hNode, RdMatrix34 parent) {
	RdMatrix34 out;

	_rd->buildTranslate34(out, hNode->pivot);
	_rd->postMultiply34(out, thing->jointMatrices[hNode->num]);

	if (hNode->parentNode) {
		RdVector3 negatedPivot;
		negatedPivot[0] = -hNode->parentNode->pivot[0];
		negatedPivot[1] = -hNode->parentNode->pivot[1];
		negatedPivot[2] = -hNode->parentNode->pivot[2];
		_rd->postTranslate34(out, negatedPivot);
	}

	_rd->multiply34(thing->jointMatrices[hNode->num], parent, out);

	if (hNode->numChildren) {
		RdModel3HNode *child = hNode->firstChildNode;

		for (uint i = 0; i < hNode->numChildren; i++) {
			if (!thing->abJointAmputate[child->num])
				accumulateMatrices(thing, child, thing->jointMatrices[hNode->num]);

			child = child->siblingNode;
		}
	}
}

int RenderDroid::ThingModule::draw(RdThing *thing, RdMatrix34 mat, RdThing *parentThing) {
	if (!_rd->_curGeometryMode)
		return 0;

	switch (thing->type) {
	case 1:
		return _rd->_model3.draw(thing, mat, parentThing);
	case 4:
		if (_rd->_remasteredLookRenderer)
			_rd->_remasteredLookRenderer->drawSprite(thing, mat);

		return _rd->_sprite.draw(thing, mat);
	case 5:
		return _rd->_model3.drawParticle(thing, mat);
	case 6:
		return _rd->_model3.drawPolyline(thing, mat);
	case 0:
	case 2:
	case 3:
		return 0;
	default:
		return thing->type;
	}
}

} // End of namespace Grim
