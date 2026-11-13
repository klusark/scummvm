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
#include "engines/grim/puck.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/std/std_math.h"
#include "engines/grim/std/std_sort.h"
#include "engines/grim/walkplane.h"

namespace Grim {

static const LipSyncEntry lipSyncAnimTable[46] = {
	{ 95, 0 }, { 593, 1 }, { 97, 1 }, { 230, 1 }, { 652, 8 }, { 596, 1 }, { 601, 1 }, { 98, 6 },
	{ 679, 2 }, { 100, 2 }, { 240, 5 }, { 603, 8 }, { 616, 8 }, { 602, 9 }, { 605, 9 }, { 101, 1 },
	{ 102, 4 }, { 103, 8 }, { 609, 8 }, { 104, 8 }, { 618, 8 }, { 105, 3 }, { 676, 2 }, { 107, 2 },
	{ 108, 5 }, { 619, 5 }, { 109, 6 }, { 110, 8 }, { 331, 8 }, { 111, 7 }, { 112, 6 }, { 114, 2 },
	{ 635, 2 }, { 633, 2 }, { 115, 2 }, { 643, 2 }, { 116, 2 }, { 638, 2 }, { 952, 5 }, { 650, 9 },
	{ 117, 9 }, { 118, 4 }, { 119, 9 }, { 106, 8 }, { 122, 2 }, { 658, 2 }
};

SputActorModule::SputActorModule(Res *res, RenderDroid *rd, StdMath *stdMath, StdDisplay *stdDisplay, WalkSystem *walkSystem,
		SputResource *sputResource, SputRoom *sputRoom, SputRender *sputRender, SputCostume *sputCostume, SputText *sputText,
		SputTime *sputTime) :
		_res(res), _rd(rd), _stdMath(stdMath), _stdDisplay(stdDisplay), _walkSystem(walkSystem), _sputResource(sputResource),
		_sputRoom(sputRoom), _sputRender(sputRender), _sputCostume(sputCostume), _sputText(sputText), _sputTime(sputTime),
		_grim(nullptr), _cos(sputCostume->getCos()),
		_hs(nullptr), _initialized(0), _started(0), _actors(nullptr), _maxActors(1024), _numActors(0), _selectedActor(nullptr),
		_numActorsTalking(0), _numCollisionActors(0), _speechMode(2), _textRate(6), _walkSystemFadeTime(130), _numLipSyncAnims(46),
		_shadowMaterial(nullptr) {
	memset(_talkingActors, 0, sizeof(_talkingActors));
	memset(_collisionActors, 0, sizeof(_collisionActors));
	memset(_collisionImpact, 0, sizeof(_collisionImpact));
	memset(_collisionOut, 0, sizeof(_collisionOut));
	memset(_collisionIn, 0, sizeof(_collisionIn));
	memcpy(_lipSyncAnims, lipSyncAnimTable, sizeof(_lipSyncAnims));

	// The demo has no shadow material, its shadows are one solid cel
	memset(&_demoShadowMaterial, 0, sizeof(_demoShadowMaterial));
	memset(&_demoShadowCel, 0, sizeof(_demoShadowCel));
	Common::strlcpy(_demoShadowMaterial.name, "Default shadow material", 32);
	_demoShadowMaterial.numCels = 1;
	_demoShadowMaterial.cels[0] = &_demoShadowCel;
}

void SputActorModule::publishActors() {
	_sputRoom->setActors(_actors);
	_sputRender->setActors(_actors);
	_sputCostume->setActors(_actors);
}

void SputActorModule::nukeCostumeStacks() {
	for (int i = 0; i < _maxActors; i++) {
		if (!_actors[i].inUse)
			continue;

		if (_actors[i].costumeStack)
			_sputCostume->printCostumeInfo(_actors[i].costumeStack);

		_sputCostume->freeCostumeNodes(_actors[i].costumeStack, 1);
		_actors[i].costumeStack = nullptr;
	}
}

int SputActorModule::initialize(SysStruct *hs) {
	_hs = hs;

	for (int i = 0; i < 4; i++)
		_talkingActors[i] = nullptr;

	_initialized = 1;

	return 0;
}

int SputActorModule::terminate() {
	_hs = nullptr;

	if (_initialized)
		_initialized = 0;

	return 0;
}

int SputActorModule::startup() {
	if (_started)
		return 1;

	_actors = new SputActor[_maxActors]();
	publishActors();

	Grim::sort(_lipSyncAnims, _numLipSyncAnims, sizeof(LipSyncEntry), compareAnimEntry);

	_started = 1;

	return 0;
}

void SputActorModule::shutdown() {
	for (int i = 0; i < _maxActors; i++)
		freeActor(&_actors[i]);

	delete[] _actors;
	_actors = nullptr;
	publishActors();

	_started = 0;
}

// No actor is left, not even the camera: the render module allocates it again
void SputActorModule::reset() {
	for (int i = 0; i < 4; i++)
		_talkingActors[i] = nullptr;

	_numActorsTalking = 0;

	for (int i = 0; i < _maxActors; i++)
		freeActor(&_actors[i]);

	_numActors = 0;
	_selectedActor = nullptr;
}

int SputActorModule::getNumActorsTalking() {
	return _numActorsTalking;
}

int SputActorModule::setCostume(SputActor *actor, const char *costumeName, int instance) {
	CostumeNode *node;

	if (!costumeName || instance) {
		node = _sputCostume->setCostume(actor->costumeStack, 0, instance);
	} else {
		int costume = (int)(intptr)_sputResource->alloc(kSputTagCost, costumeName);

		if (!costume)
			return 0;

		node = _sputCostume->setCostume(actor->costumeStack, costume, 0);
	}

	actor->costumeStack = node;
	_cos->instSetUserData(actor->costumeStack->costumeInstance, (int)(actor - _actors));
	setFrustrumCull(actor, ((uint)~actor->flags >> 2) & 1);

	return 1;
}

void SputActorModule::setActorHead(SputActor *actor, uint joint1, uint joint2, uint joint3, float maxAngle, float yawLimit, float pitchLimit) {
	CostumeNode *node = actor->costumeStack;

	if (!node || !node->headThing)
		return;

	uint numHNodes = node->headThing->data.model3->numHNodes;

	if (joint1 > numHNodes || joint2 > numHNodes || joint3 > numHNodes)
		return;

	node->headJoint[0] = joint1;
	node->headJoint[1] = joint2;
	node->headYawLimit = yawLimit;
	node->headJoint[2] = joint3;
	node->headPitchLimit = pitchLimit;

	if (maxAngle < 0.0f)
		maxAngle = -maxAngle;

	float halfAngle = (float)((double)maxAngle * 0.5);
	float sinValue;

	_stdMath->sinCos(halfAngle, &sinValue, &node->headLookRef);
}

int SputActorModule::setSelectedActor(int handle) {
	int result = -1;

	if (_selectedActor)
		result = getActorHandle(_selectedActor);

	if (handle >= 0 && handle < _maxActors)
		_selectedActor = &_actors[handle];

	return result;
}

SputActor *SputActorModule::actorDeref(int handle) {
	if (handle < 0 || handle >= _maxActors) {
		warning("SputActorModule::actorDeref(): invalid actor handle %d", handle);
		return nullptr;
	}

	return &_actors[handle];
}

int SputActorModule::getActorHandle(SputActor *actor) {
	return (int)(actor - _actors);
}

int SputActorModule::allocateActor() {
	SputActor *actor = nullptr;

	for (int i = 0; i < _maxActors; i++) {
		if (!_actors[i].inUse) {
			actor = &_actors[i];
			break;
		}
	}

	if (!actor) {
		int talkingIndex[4];
		int selectedIndex = -1;

		for (int i = 0; i < 4; i++) {
			talkingIndex[i] = -1;

			if (_talkingActors[i])
				talkingIndex[i] = (int)(_talkingActors[i] - _actors);
		}

		if (_selectedActor)
			selectedIndex = (int)(_selectedActor - _actors);

		SputActor *newActors = new SputActor[_maxActors + 32]();

		memcpy(newActors, _actors, _maxActors * sizeof(SputActor));
		delete[] _actors;
		_actors = newActors;
		_maxActors += 32;
		actor = &_actors[_maxActors - 32];

		for (int i = 0; i < 4; i++) {
			if (_talkingActors[i])
				_talkingActors[i] = &_actors[talkingIndex[i]];
		}

		if (_selectedActor)
			_selectedActor = &_actors[selectedIndex];

		for (int i = 0; i < _maxActors - 32; i++) {
			if (_actors[i].inUse && _actors[i].extra && _actors[i].extra->sayText)
				_actors[i].extra->sayText->actor = &_actors[i];
		}

		publishActors();
	}

	_numActors++;

	memset(actor, 0, sizeof(SputActor));
	actor->matrixValid = 1;
	_rd->identity34(actor->orientMatrix);
	actor->scale = 1.0f;
	actor->costumeStack = nullptr;
	actor->colorMap = new ResRef();
	actor->inUse = 1;

	return (int)(actor - _actors);
}

void SputActorModule::allocateActorExtra(SputActor *actor) {
	if (!actor || actor->extra) {
		warning("SputActorModule::allocateActorExtra(): no actor or the actor already has an extra");
		return;
	}

	actor->extra = new Extra();
	actor->extra->walkRate = 0.40000001f;
	actor->extra->turnRate = 135.0f;
	actor->extra->walkDominate = 0;
	actor->extra->reflectionAngle = 60.0f;
	actor->extra->collisionScale = 1.0f;
	actor->extra->timeScale = 1.0f;
	actor->extra->talkColor = 0xFFFFFF;
	actor->extra->subClipNode = -1;
	actor->extra->clipPlaneNorm[0] = 0.0f;
	actor->extra->clipPlaneNorm[1] = 0.0f;
	actor->extra->clipPlaneNorm[2] = 1.0f;
}

void SputActorModule::enableWalkSystem(SputActor *actor, int enable) {
	if (!actor->extra)
		allocateActorExtra(actor);

	if (enable) {
		actor->extra->inBoxes = 1;
		enablePuck(actor, 1);
		actor->extra->puck->validate();
	} else {
		actor->extra->inBoxes = 0;

		if (actor->extra->puck)
			actor->extra->puck->_plane = nullptr;
	}
}

void SputActorModule::enablePuck(SputActor *actor, int enable) {
	Extra *extra = actor->extra;

	if (enable || !extra->puck) {
		if (!extra->puck) {
			Puck *puck = new Puck(_walkSystem, extra->reflectionAngle);

			puck->setPos(actor->actorPos);

			float *forward = getOrientMatrix(actor)[1];
			RdVector3 demoForward;
			RdVector3 target;

			// The demo reads the three values after the matrix (walking, turn direction, resting) as floats
			if (_grim->getGameFlags() & ADGF_DEMO) {
				memcpy(&demoForward[0], &extra->walking, sizeof(float));
				memcpy(&demoForward[1], &extra->turnDir, sizeof(float));
				memcpy(&demoForward[2], &extra->resting, sizeof(float));
				forward = demoForward;
			}

			target[0] = forward[0] + actor->actorPos[0];
			target[1] = actor->actorPos[1] + forward[1];
			target[2] = actor->actorPos[2] + forward[2];
			puck->pointAt(target, 3.4028235e38f);
			extra->puck = puck;
		}
	} else {
		delete extra->puck;
		extra->puck = nullptr;
	}
}

void SputActorModule::setActorPosition(SputActor *actor, float *pos) {
	Puck *puck = nullptr;

	if (actor->extra)
		puck = actor->extra->puck;

	actor->mode &= ~1;

	if (puck) {
		puck->setPos(pos);

		if (actor->extra->inBoxes)
			puck->validate();

		actor->actorPos[0] = puck->_pos[0];
		actor->actorPos[1] = puck->_pos[1];
		actor->actorPos[2] = puck->_pos[2];
	} else {
		actor->actorPos[0] = pos[0];
		actor->actorPos[1] = pos[1];
		actor->actorPos[2] = pos[2];
	}

	actor->matrixValid = 0;
}

void SputActorModule::setActorRotation(SputActor *actor, float *rot, int flipTurn) {
	Extra *extra = actor->extra;

	actor->mode &= ~1;

	if (extra && extra->puck) {
		Puck *puck = extra->puck;
		RdMatrix34 matrix;

		_rd->buildRotate34(matrix, rot);
		matrix[1][0] = matrix[1][0] + puck->_pos[0];
		matrix[1][1] = puck->_pos[1] + matrix[1][1];
		matrix[1][2] = puck->_pos[2] + matrix[1][2];
		puck->invXform(matrix[1]);
		matrix[1][2] = 0.0f;
		_rd->normalize3Acc(matrix[1]);

		float angle = (float)(90.0 - _stdMath->arcSin3(matrix[1][1]));

		if (!(matrix[1][0] > 0.0f))
			puck->rotate(angle);
		else
			puck->rotate(-angle);

		if (actor->extra->turnRate > 0.0f) {
			if (flipTurn)
				actor->extra->turnRate = actor->extra->turnRate * -1.0f;
		}

		actor->extra->alignOff = angle;

		// The demo leaves the rotation of the actor alone here
		if (!(_grim->getGameFlags() & ADGF_DEMO)) {
			actor->actorRot[2] = rot[2];
			actor->actorRot[0] = rot[0];
			actor->matrixValid = 0;
		}
	} else {
		actor->actorRot[0] = rot[0];
		actor->actorRot[1] = rot[1];
		actor->actorRot[2] = rot[2];
		actor->matrixValid = 0;
	}
}

void SputActorModule::setReflection(SputActor *actor, float angle) {
	Puck *puck = actor->extra->puck;

	if (puck)
		puck->_reflectionAngle = angle;
}

void SputActorModule::setWalkChore(SputActor *actor, int choreIndex, CostumeNode *node) {
	CostumeNode *current = actor->costumeStack;

	if (!current)
		return;

	bool demo = (_grim->getGameFlags() & ADGF_DEMO) != 0;

	// The demo has no costume parameter, the chore stays in the node where it was found
	CostumeNode *target = demo ? nullptr : node;

	if (!target)
		target = actor->costumeStack;

	RdThing *headThing = current->headThing;

	do {
		if (current->headThing != headThing)
			break;

		if (current->walkChore >= 0)
			break;

		current = current->next;
	} while (current);

	if (current && current->headThing == headThing) {
		if (choreIndex != current->walkChore) {
			int status = _cos->instChoreStatus(current->costumeInstance, current->walkChore);

			if (status) {
				fadeOutChore(current->costumeInstance, current->walkChore, _walkSystemFadeTime);

				if (choreIndex != -1) {
					_cos->instPlayChore(current->costumeInstance, choreIndex, status & 2, ((uint)~status >> 2) & 1);
					_cos->instUpdateChore(current->costumeInstance, choreIndex, 100);
					_sputCostume->fadeKeysIn(current->costumeInstance, choreIndex, _walkSystemFadeTime);
				}
			}

			if (demo) {
				current->walkChore = choreIndex;
			} else {
				current->walkChore = -1;
				target->walkChore = choreIndex;
			}
		}
	} else {
		target->walkChore = choreIndex;
	}
}

void SputActorModule::setTurnChores(SputActor *actor, int leftChore, int rightChore, CostumeNode *node) {
	CostumeNode *current = actor->costumeStack;

	if (!current)
		return;

	bool demo = (_grim->getGameFlags() & ADGF_DEMO) != 0;

	// The demo has no costume parameter, the chores stay in the node where they were found
	CostumeNode *target = demo ? nullptr : node;

	if (!target)
		target = actor->costumeStack;

	RdThing *headThing = current->headThing;

	do {
		if (current->headThing != headThing)
			break;

		if (current->turnLeftChore >= 0)
			break;

		current = current->next;
	} while (current);

	if (current && current->headThing == headThing) {
		if (leftChore != current->turnLeftChore) {
			int status = _cos->instChoreStatus(current->costumeInstance, current->turnLeftChore);

			if (status) {
				_cos->instStopChore(current->costumeInstance, current->turnLeftChore);

				if (leftChore != -1)
					_cos->instPlayChore(current->costumeInstance, leftChore, status & 2, ((uint)~status >> 2) & 1);
			}

			if (demo) {
				current->turnLeftChore = leftChore;
			} else {
				current->turnLeftChore = -1;
				target->turnLeftChore = leftChore;
			}
		}

		if (rightChore != current->turnRightChore) {
			int status = _cos->instChoreStatus(current->costumeInstance, current->turnRightChore);

			if (status) {
				_cos->instStopChore(current->costumeInstance, current->turnRightChore);

				if (leftChore != -1)
					_cos->instPlayChore(current->costumeInstance, rightChore, status & 2, ((uint)~status >> 2) & 1);
			}

			if (demo) {
				current->turnRightChore = rightChore;
			} else {
				current->turnRightChore = -1;
				target->turnRightChore = rightChore;
			}
		}
	} else {
		target->turnLeftChore = leftChore;
		target->turnRightChore = rightChore;
	}
}

void SputActorModule::setRestChore(SputActor *actor, int choreIndex, CostumeNode *node) {
	CostumeNode *current = actor->costumeStack;

	if (!current)
		return;

	bool demo = (_grim->getGameFlags() & ADGF_DEMO) != 0;

	// The demo has no costume parameter, the chore stays in the node where it was found
	CostumeNode *target = demo ? nullptr : node;

	if (!target)
		target = actor->costumeStack;

	RdThing *headThing = current->headThing;

	do {
		if (current->headThing != headThing)
			break;

		// The demo looks for the node by the left turn chore
		if (demo ? current->turnLeftChore >= 0 : current->restChore >= 0)
			break;

		current = current->next;
	} while (current);

	if (current && current->headThing == headThing) {
		if (choreIndex != current->restChore) {
			int status = _cos->instChoreStatus(current->costumeInstance, current->restChore);

			if (status) {
				_cos->instStopChore(current->costumeInstance, current->restChore);

				if (choreIndex != -1)
					_cos->instPlayChore(current->costumeInstance, choreIndex, status & 2, ((uint)~status >> 2) & 1);
			}

			if (demo) {
				current->restChore = choreIndex;
			} else {
				current->restChore = -1;
				target->restChore = choreIndex;
			}
		}
	} else {
		target->restChore = choreIndex;
	}
}

int SputActorModule::pushCostume(SputActor *actor, const char *costumeName, int instance) {
	CostumeNode *node;

	if (!costumeName || instance) {
		node = _sputCostume->pushCostume(actor->costumeStack, 0, instance);
	} else {
		int costume = (int)(intptr)_sputResource->alloc(kSputTagCost, costumeName);

		if (!costume)
			return 0;

		node = _sputCostume->pushCostume(actor->costumeStack, costume, 0);
	}

	actor->costumeStack = node;
	_cos->instSetUserData(actor->costumeStack->costumeInstance, (int)(actor - _actors));
	setFrustrumCull(actor, ((uint)~actor->flags >> 2) & 1);

	return 1;
}

void SputActorModule::popCostume(SputActor *actor, int freeInstance) {
	actor->costumeStack = _sputCostume->popCostume(actor->costumeStack, freeInstance);
}

void SputActorModule::fadeInChore(int inst, int choreIndex, int fadeMs) {
	_cos->instPlayChore(inst, choreIndex, 1, 0);
	_cos->instUpdateChore(inst, choreIndex, 70);
	_sputCostume->fadeKeysIn(inst, choreIndex, fadeMs);
}

void SputActorModule::fadeOutChore(int inst, int choreIndex, int fadeMs) {
	if (choreIndex == -1) {
		int numChores = _cos->instCountChores(inst);

		for (int i = 0; i < numChores; i++)
			fadeOutChore(inst, i, fadeMs);
	} else {
		_sputCostume->fadeKeysOut(inst, choreIndex, fadeMs);
		_cos->instStopChore(inst, choreIndex);
	}
}

void SputActorModule::setSpeechMode(int mode) {
	if (mode != _speechMode && (mode & 3) != 0)
		_speechMode = mode;
}

int SputActorModule::getSpeechMode() {
	return _speechMode;
}

void SputActorModule::setWalkDominate(SputActor *actor, int dominate) {
	actor->extra->walkDominate = dominate;
}

int SputActorModule::countCostumes(SputActor *actor) {
	int count = 0;

	for (CostumeNode *node = actor->costumeStack; node; node = node->next)
		count++;

	return count;
}

void SputActorModule::setClipPlane(SputActor *actor, float *normal, float *point) {
	if (!actor->extra)
		allocateActorExtra(actor);

	actor->extra->clipPlaneNorm[0] = normal[0];
	actor->extra->clipPlaneNorm[1] = normal[1];
	actor->extra->clipPlaneNorm[2] = normal[2];
	actor->extra->clipPlanePt[0] = point[0];
	actor->extra->clipPlanePt[1] = point[1];
	actor->extra->clipPlanePt[2] = point[2];
}

void SputActorModule::setClipPlaneActive(SputActor *actor, int active) {
	if (!actor->extra)
		allocateActorExtra(actor);

	actor->extra->clipPlaneActive = active;
}

int SputActorModule::getActorTextRate() {
	return _textRate;
}

void SputActorModule::setActorTextRate(int rate) {
	if (rate >= 1) {
		if (rate > 10)
			rate = 10;

		_textRate = rate;
	} else {
		_textRate = 1;
	}
}

void SputActorModule::setFrustrumCull(SputActor *actor, int cull) {
	if (!actor)
		return;

	if (cull)
		actor->flags |= 4;
	else
		actor->flags &= ~4;

	if (actor->costumeStack) {
		for (ModelInst *modelInst = actor->costumeStack->headList; modelInst; modelInst = modelInst->next)
			modelInst->thing->frustrumCull = cull;
	}
}

void SputActorModule::freeActor(SputActor *actor) {
	if (actor->curSetName[0])
		_sputRoom->removeActor(actor->curSetName, actor);

	resetShadows(actor);
	_sputCostume->freeCostumeNodes(actor->costumeStack, 0);
	actor->costumeStack = nullptr;

	if (actor->extra) {
		delete actor->extra->puck;
		actor->extra->puck = nullptr;
		delete actor->extra;
		actor->extra = nullptr;
	}

	if (actor->colorMap) {
		_res->removeReference(actor->colorMap);
		delete actor->colorMap;
		actor->colorMap = nullptr;
	}

	actor->inUse = 0;
	_numActors--;
}

void SputActorModule::setWalkSystemFadeTime(int fadeMs) {
	_walkSystemFadeTime = fadeMs;
}

CostumeNode *SputActorModule::allocCostumeNode() {
	CostumeNode *node = new CostumeNode();

	initCostumeNode(node);

	return node;
}

void SputActorModule::initCostumeNode(CostumeNode *node) {
	node->costumeInstance = 0;
	node->headJoint[0] = -1;
	node->headJoint[1] = -1;
	node->headJoint[2] = -1;
	node->headYawLimit = 30.0f;
	node->headPitchLimit = 90.0f;
	node->headLookRef = -0.35f;
	node->headTurnRate = 360.0f;
	node->walkChore = -1;
	node->turnLeftChore = -1;
	node->turnRightChore = -1;
	node->restChore = -1;
	node->mumbleChore = -1;
	memset(node->chore, 0xFF, sizeof(node->chore));
	memset(&node->rCostume, 0, sizeof(node->rCostume));
	node->headList = nullptr;
	node->next = nullptr;
}

} // End of namespace Grim
