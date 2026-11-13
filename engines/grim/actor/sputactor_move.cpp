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

#include <math.h>

#include "common/textconsole.h"

#include "engines/grim/chore/cos.h"
#include "engines/grim/grim.h"
#include "engines/grim/puck.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_math.h"
#include "engines/grim/walkplane.h"

namespace Grim {

int SputActorModule::canActorSeePoint(SputActor *actor, float *point, float *distance) {
	CostumeNode *node = actor->costumeStack;

	if (!node)
		return 0;

	if (node->headJoint[0] < 0)
		return 0;

	if (!node->headThing)
		return 0;

	RdMatrix34 meshMatrix;

	_rd->_model3.getMeshMatrix(node->headThing, getOrientMatrix(actor), node->headJoint[0], meshMatrix);

	RdVector3 direction;

	direction[0] = point[0] - meshMatrix[3][0];
	direction[1] = point[1] - meshMatrix[3][1];
	direction[2] = point[2] - meshMatrix[3][2];
	*distance = _rd->normalize3Acc(direction);

	RdVector3 flat;

	flat[0] = direction[0];
	flat[1] = direction[1];
	flat[2] = 0.0f;
	_rd->normalize3Acc(flat);

	float xTerm = (float)((double)getOrientMatrix(actor)[1][0] * flat[0]);
	float xyTerm = (float)((double)getOrientMatrix(actor)[1][1] * flat[1] + xTerm);
	double dot = (double)getOrientMatrix(actor)[1][2] * flat[2] + xyTerm;

	return !(dot < node->headLookRef);
}

int SputActorModule::canActorSeeActor(SputActor *actor, SputActor *target, float *distance) {
	if (!target->curSetName[0] || !actor->curSetName[0] || scumm_stricmp(target->curSetName, actor->curSetName))
		return 0;

	return canActorSeePoint(actor, getOrientMatrix(target)[3], distance);
}

void SputActorModule::turn(SputActor *actor, float dir) {
	Extra *extra = actor->extra;
	float frameTime = _sputTime->getFrameTimeFlex();
	float scaledTime = (float)((double)frameTime * extra->timeScale);
	float turnAmount = (float)(((double)extra->turnRate * scaledTime) * dir);

	if (!extra->puck)
		enablePuck(actor, 1);

	extra = actor->extra;

	double alignAbs = extra->alignOff;

	if (alignAbs < 0.0)
		alignAbs = -alignAbs;

	double doubleTurn = (double)turnAmount + turnAmount;

	if (doubleTurn < 0.0)
		doubleTurn = -doubleTurn;

	if (!(alignAbs > doubleTurn)) {
		if (!(dir > 0.0f))
			extra->turnDir = -1;
		else
			extra->turnDir = 1;
	}
}

void SputActorModule::forward(SputActor *actor) {
	if (!actor->extra->puck)
		enablePuck(actor, 1);

	actor->extra->walking = 1;
}

void SputActorModule::updateActors(float delta) {
	_numCollisionActors = 0;

	Room *room = _sputRoom->getCurrentRoom();

	if (!room)
		return;

	updateSpeechOnly(delta);

	RoomActorLink *link = room->actorNode->actors;

	if (!link)
		return;

	do {
		SputActor *actor = &_actors[link->actorIndex];

		if (actor->extra)
			actor->extra->collided = 0;

		if (actor->costumeStack && actor->costumeStack->headThing) {
			if (_numCollisionActors < 256) {
				_collisionActors[_numCollisionActors] = actor;
				_numCollisionActors++;
			} else {
				warning("SputActorModule::updateActors(): too many actors to collide");
			}
		}

		updateCostumesMain(actor, delta);
		link = link->next;
	} while (link);

	_hadDynamicCollision = false;
	updatePositions();
	_dynamicCollisionFrames = _hadDynamicCollision ? _dynamicCollisionFrames + 1 : 0;
}

void SputActorModule::updateActorsDemo(float delta) {
	_numCollisionActors = 0;

	for (int i = 0; i < _numActors; i++) {
		SputActor *actor = &_actors[i];

		if (!actor->inUse)
			continue;

		if (!scumm_stricmp(actor->curSetName, _sputRoom->getCurrentRoomName()) || (actor->extra && actor->extra->talking))
			updateCostumesMainDemo(actor, delta);
		else
			stopActorDemo(actor);
	}
}

void SputActorModule::updateCostumesMainDemo(SputActor *actor, float delta) {
	CostumeNode *node = actor->costumeStack;

	if (node && node->rCostume.valid)
		_cos->instRescue(node->costumeInstance, node->rCostume.serial);

	if (actor->mode & 1)
		updatePuckMovement(actor);

	if (actor->extra && actor->extra->puck)
		updatePuck(actor);

	updateCostumes4(actor);
	_sputCostume->update(actor->costumeStack, delta);

	if (actor->mode & 2)
		updateCostumes2(actor, delta);

	Extra *extra = actor->extra;

	if (!extra || !extra->talking)
		return;

	// The demo has no lip sync data, only the timer and the voice end the line
	extra->talkTimerMs += (int)(int64)((double)delta * -1000.0);

	if (extra->voiceName[0] && !isVoicePlayingDemo(actor))
		shutUpDemo(actor);

	// Both checks can run, the count of talking actors is lowered twice then, as in the EXE
	if (!extra->voiceName[0] && extra->talkTimerMs < 0)
		shutUpDemo(actor);
}

void SputActorModule::updateCostumesMain(SputActor *actor, float delta) {
	if (actor->extra)
		delta = (float)((double)actor->extra->timeScale * delta);

	CostumeNode *node = actor->costumeStack;

	actor->prevPosition[0] = actor->actorPos[0];
	actor->prevPosition[1] = actor->actorPos[1];
	actor->prevPosition[2] = actor->actorPos[2];

	if (node && node->rCostume.valid)
		_cos->instRescue(node->costumeInstance, node->rCostume.serial);

	if (actor->mode & 1)
		updatePuckMovement(actor);

	if (actor->extra && actor->extra->puck)
		updatePuck(actor);

	updateCostumes4(actor);
	_sputCostume->update(actor->costumeStack, delta);

	if (actor->mode & 2)
		updateCostumes2(actor, delta);

	if (actor->flags & 0x10)
		updateCostumes3(actor);
}

void SputActorModule::updateCostumes2(SputActor *actor, float delta) {
	CostumeNode *node = actor->costumeStack;

	if (!actor->extra) {
		warning("SputActorModule::updateCostumes2(): the actor has no extra");
		return;
	}

	if (!node || node->headJoint[0] < 0 || !node->headThing)
		return;

	int lookAhead = 0;
	RdVector3 up;

	up[0] = 0.0f;
	up[1] = 1.0f;
	up[2] = 0.0f;

	// The demo uses the joint matrices of the last frame
	if (!actor->onScreen && !(_grim->getGameFlags() & ADGF_DEMO))
		_rd->_puppet.buildJointMatrices(node->headThing, getOrientMatrix(actor));

	RdMatrix34 headMatrix;
	RdMatrix34 inverseMatrix;

	_rd->_model3.getMeshMatrix(node->headThing, getOrientMatrix(actor), node->headJoint[2], headMatrix);
	_rd->invertOrtho34(inverseMatrix, headMatrix);

	Extra *extra = actor->extra;
	RdVector3 target;

	if (extra->lookCarrot[0] == 0.0f && extra->lookCarrot[1] == 0.0f && extra->lookCarrot[2] == 0.0f) {
		lookAhead = 1;
		_rd->transformVector34(target, getOrientMatrix(actor)[1], inverseMatrix);

		if (g_grim->useRemasterFixes())
			_rd->normalize3Acc(target);
	} else {
		_rd->transformPoint34(target, extra->lookCarrot, inverseMatrix);
		_rd->normalize3Acc(target);
	}

	RdVector3 axis;

	_rd->cross3(axis, up, target);
	_rd->normalize3Acc(axis);

	float turnLimit = (float)((double)node->headTurnRate * delta);
	float dotValue = _rd->dot2(up, target);
	double angle = 90.0 - _stdMath->arcSin2(dotValue);
	double absAngle = angle;

	if (absAngle < 0.0)
		absAngle = -absAngle;

	if (absAngle < turnLimit) {
		if (lookAhead)
			actor->mode &= ~2;

		if (angle <= 0.0)
			angle = -angle;

		turnLimit = (float)angle;
	}

	RdMatrix34 turnMatrix;
	RdVector3 pitchYawRoll;

	_rd->buildFromVectorAngle34(turnMatrix, axis, turnLimit);
	_rd->transformPoint34Acc(up, turnMatrix);
	_rd->extractAngles(up, pitchYawRoll);

	RdThing *headThing = node->headThing;

	if (actor->mode & 2) {
		pitchYawRoll[1] = (float)(pitchYawRoll[1] * 0.3333333432674408);
		pitchYawRoll[0] = (float)(pitchYawRoll[0] * 0.3333333432674408);

		double pitchLimit = node->headPitchLimit * 0.3333333432674408;
		double yawLimit = node->headYawLimit * 0.3333333432674408;

		for (int i = 0; i < 3; i++) {
			float *jointAngles = headThing->jointPyr[node->headJoint[i]];

			jointAngles[1] = (float)(pitchYawRoll[1] + jointAngles[1]);
			jointAngles[0] = (float)(pitchYawRoll[0] + jointAngles[0]);
			jointAngles[2] = 0.0f;

			double value = jointAngles[1];

			if (value < -pitchLimit)
				value = -pitchLimit;
			else if (value > pitchLimit)
				value = pitchLimit;

			jointAngles[1] = (float)value;

			value = jointAngles[0];

			if (value < -yawLimit)
				value = -yawLimit;
			else if (value > yawLimit)
				value = yawLimit;

			jointAngles[0] = (float)value;
		}

		headThing->jointsFrame = 0;
	} else {
		for (int i = 0; i < 3; i++) {
			float *jointAngles = headThing->jointPyr[node->headJoint[i]];

			jointAngles[1] = 0.0f;
			jointAngles[0] = 0.0f;
			jointAngles[2] = 0.0f;
		}

		headThing->jointsFrame = 0;
	}
}

void SputActorModule::updateCostumes3(SputActor *actor) {
	if (!actor || !actor->extra || !actor->extra->puck)
		return;

	Puck *puck = actor->extra->puck;

	_rd->build34(actor->orientMatrix, actor->actorRot, actor->actorPos);

	RdVector3 actorForward;
	RdVector3 puckForward;

	actorForward[0] = actor->orientMatrix[2][0];
	actorForward[1] = actor->orientMatrix[2][1];
	actorForward[2] = actor->orientMatrix[2][2];
	puckForward[0] = puck->_orient[2][0];
	puckForward[1] = puck->_orient[2][1];
	puckForward[2] = puck->_orient[2][2];

	float dotValue = _rd->dot3(actorForward, puckForward);
	double angle = 90.0 - _stdMath->arcSin3(dotValue);
	float angleF = (float)angle;
	double absAngle = angle;

	if (absAngle < 0.0)
		absAngle = -absAngle;

	if (!(absAngle > 2.0))
		return;

	float frameTime = _sputTime->getFrameTimeFlex();
	double rate = (double)frameTime * actor->extra->turnRate;

	if (angleF > 0.0f && rate < angleF)
		angleF = (float)rate;

	if (angleF < 0.0f) {
		double negRate = -rate;

		if (negRate > angleF)
			angleF = (float)negRate;
	}

	RdVector3 axis;

	axis[0] = (float)((double)puckForward[2] * actorForward[1] - (double)puckForward[1] * actorForward[2]);
	axis[1] = (float)((double)puckForward[0] * actorForward[2] - (double)puckForward[2] * actorForward[0]);
	axis[2] = (float)((double)puckForward[1] * actorForward[0] - (double)puckForward[0] * actorForward[1]);
	_rd->normalize3Acc(axis);

	RdMatrix34 turnMatrix;

	_rd->buildFromVectorAngle34(turnMatrix, axis, angleF);
	_rd->postMultiply34(actor->orientMatrix, turnMatrix);
	_rd->extractAngles34(actor->orientMatrix, actor->actorRot);
	_rd->build34(actor->orientMatrix, actor->actorRot, actor->actorPos);
}

void SputActorModule::updatePuck(SputActor *actor) {
	Extra *extra = actor->extra;
	Puck *puck = extra->puck;

	if (extra->turnDir && !(actor->mode & 1)) {
		float frameTime = _sputTime->getFrameTimeFlex();
		float scaledTime = (float)((double)frameTime * extra->timeScale);
		float turnAmount = (float)(((double)extra->turnRate * scaledTime) * (double)extra->turnDir);

		extra->puck->rotate(turnAmount);
	}

	if (actor->extra->walking) {
		float frameTime = _sputTime->getFrameTimeFlex();
		Extra *walkExtra = actor->extra;
		float distance = (float)(((double)frameTime * walkExtra->timeScale) * walkExtra->walkRate);
		int walking;

		if (walkExtra->walkDominate)
			walking = walkExtra->puck->forward(distance, 1, 1);
		else
			walking = walkExtra->puck->forward(distance, walkExtra->turnDir != -1, walkExtra->turnDir != 1);

		actor->extra->walking = walking;
	}

	actor->extra->aligned = alignToPuck(actor, puck);

	Extra *lookExtra = actor->extra;

	if (!lookExtra->turnDir && !lookExtra->aligned) {
		if (!(lookExtra->alignOff > 0.0f))
			lookExtra->turnDir = 1;
		else
			lookExtra->turnDir = -1;
	}

	// The demo clears the pitch and the roll of the actor
	if (_grim->getGameFlags() & ADGF_DEMO) {
		actor->actorRot[0] = 0.0f;
		actor->actorRot[2] = 0.0f;
	}

	actor->actorPos[0] = puck->_pos[0];
	actor->actorPos[1] = puck->_pos[1];
	actor->actorPos[2] = puck->_pos[2];
	actor->matrixValid = 0;
}

int SputActorModule::alignToPuck(SputActor *actor, Puck *puck) {
	float *forward = getOrientMatrix(actor)[1];
	RdVector3 vec;

	vec[0] = forward[0];
	vec[1] = forward[1];
	vec[2] = forward[2];

	if (actor->extra->offsetYaw != 0.0f) {
		RdMatrix34 turnMatrix;

		_rd->buildFromVectorAngle34(turnMatrix, getOrientMatrix(actor)[2], actor->extra->offsetYaw);
		_rd->transformVector34Acc(vec, turnMatrix);
	}

	vec[0] = vec[0] + puck->_pos[0];
	vec[1] = puck->_pos[1] + vec[1];
	vec[2] = puck->_pos[2] + vec[2];
	puck->invXform(vec);
	vec[2] = 0.0f;
	_rd->normalize3Acc(vec);

	double angle = 90.0 - _stdMath->arcSin3(vec[1]);
	float angleF = (float)angle;

	if (angle < 0.5) {
		actor->extra->alignOff = 0.0f;
		return 1;
	}

	actor->extra->alignOff = angleF;

	Extra *extra = actor->extra;
	double limit;

	if (extra->turnRate <= 0.0f) {
		limit = 3.4028234663852886e38;
		extra->turnRate = extra->turnRate * -1.0f;
	} else {
		float frameTime = _sputTime->getFrameTimeFlex();

		limit = (double)frameTime * actor->extra->timeScale * actor->extra->turnRate;
	}

	if (angleF > limit)
		angleF = (float)((angleF - limit) * 0.10000000149011612 + limit);

	float rotX = actor->actorRot[0];
	float rotY = actor->actorRot[1];
	float rotZ = actor->actorRot[2];

	if (!(vec[0] > 0.0f))
		rotY = (float)((double)rotY - angleF);
	else
		rotY = (float)((double)rotY + angleF);

	actor->actorRot[0] = rotX;
	actor->actorRot[1] = rotY;
	actor->actorRot[2] = rotZ;
	actor->matrixValid = 0;

	return 0;
}

void SputActorModule::updateCostumes4(SputActor *actor) {
	if (!actor->extra)
		return;

	CostumeNode *node = actor->costumeStack;

	if (!node)
		return;

	RdThing *headThing = node->headThing;

	do {
		if (node->headThing != headThing)
			break;

		if (node->walkChore >= 0)
			break;

		node = node->next;
	} while (node);

	if (node && node->headThing == headThing) {
		int status = _cos->instChoreStatus(node->costumeInstance, node->walkChore);

		if (actor->extra->walking) {
			if (!status) {
				actor->extra->resting = 0;
				fadeInChore(node->costumeInstance, node->walkChore, _walkSystemFadeTime);
			}
		} else if ((status & 1) && (!(actor->mode & 1) || (_grim->getGameFlags() & ADGF_DEMO))) {
			fadeOutChore(node->costumeInstance, node->walkChore, _walkSystemFadeTime);
		}
	}

	if (actor->extra->walking)
		actor->extra->turnDir = 0;

	node = actor->costumeStack;
	headThing = node->headThing;

	do {
		if (node->headThing != headThing)
			break;

		if (node->turnLeftChore >= 0)
			break;

		node = node->next;
	} while (node);

	if (node) {
		if (node->headThing == headThing && node->turnLeftChore >= 0) {
			int status = _cos->instChoreStatus(node->costumeInstance, node->turnLeftChore);

			if (actor->extra->turnDir <= 0) {
				if (status & 1)
					fadeOutChore(node->costumeInstance, node->turnLeftChore, _walkSystemFadeTime);
			} else if (!status) {
				actor->extra->resting = 0;
				fadeInChore(node->costumeInstance, node->turnLeftChore, _walkSystemFadeTime);
			}
		}

		if (node->headThing == headThing && node->turnRightChore >= 0) {
			int status = _cos->instChoreStatus(node->costumeInstance, node->turnRightChore);

			if (actor->extra->turnDir >= 0) {
				if (status & 1)
					fadeOutChore(node->costumeInstance, node->turnRightChore, _walkSystemFadeTime);
			} else if (!status) {
				actor->extra->resting = 0;
				fadeInChore(node->costumeInstance, node->turnRightChore, _walkSystemFadeTime);
			}
		}
	}

	if (!actor->extra->walking && !actor->extra->turnDir)
		actor->extra->resting = 1;

	node = actor->costumeStack;
	headThing = node->headThing;

	do {
		if (node->headThing != headThing)
			break;

		if (node->restChore >= 0)
			break;

		node = node->next;
	} while (node);

	if (node && node->headThing == headThing && node->restChore >= 0) {
		int status = _cos->instChoreStatus(node->costumeInstance, node->restChore);

		if (actor->extra->resting) {
			if (!status)
				fadeInChore(node->costumeInstance, node->restChore, _walkSystemFadeTime);
		} else if (status) {
			fadeOutChore(node->costumeInstance, node->restChore, _walkSystemFadeTime);
		}
	}

	actor->extra->walking = 0;
	actor->extra->turnDir = 0;
}

void SputActorModule::updatePuckMovement(SputActor *actor) {
	Extra *extra = actor->extra;
	double deltaY = actor->actorPos[1] - extra->walkTarget[1];
	double deltaX = actor->actorPos[0] - extra->walkTarget[0];
	double deltaZ = actor->actorPos[2] - extra->walkTarget[2];
	float deltaXF = (float)deltaX;
	float deltaYF = (float)deltaY;
	float distance = (float)sqrt(deltaX * deltaXF + deltaY * deltaYF + deltaZ * deltaZ);
	float walkRate = extra->walkRate;

	if (walkRate < 0.0f)
		walkRate = -walkRate;

	extra = actor->extra;

	float frameTime = _sputTime->getFrameTimeFlex();
	float step = (float)((double)frameTime * extra->timeScale * walkRate * 1.25);

	if (distance < step) {
		double currentDistance;

		do {
			if (extra->pathIndex >= extra->numPathPoints)
				break;

			extra->pathIndex++;
			setActorPosition(actor, actor->extra->walkTarget);

			if (actor->extra->pathIndex < 0 || actor->extra->pathIndex >= pathCapacity()) {
				warning("SputActorModule::updatePuckMovement(): path index %d out of range", actor->extra->pathIndex);
				break;
			}

			float *nextPoint = actor->extra->path[actor->extra->pathIndex];

			actor->extra->walkTarget[0] = nextPoint[0];
			actor->extra->walkTarget[1] = nextPoint[1];
			actor->extra->walkTarget[2] = nextPoint[2];

			extra = actor->extra;
			deltaY = actor->actorPos[1] - extra->walkTarget[1];
			deltaX = actor->actorPos[0] - extra->walkTarget[0];
			deltaZ = actor->actorPos[2] - extra->walkTarget[2];
			deltaXF = (float)deltaX;
			deltaYF = (float)deltaY;
			currentDistance = sqrt(deltaX * deltaXF + deltaY * deltaYF + deltaZ * deltaZ);
		} while (currentDistance < step);

		extra = actor->extra;

		if (extra->pathIndex == extra->numPathPoints) {
			actor->mode &= ~1;
			return;
		}
	}

	actor->mode |= 1;
	extra->puck->pointAt(extra->walkTarget, 3.4028235e38f);
	forward(actor);
}

void SputActorModule::leaveSector(SputActor *actor, WalkPlane *sector) {
	if (!sector || !actor)
		return;

	Puck *puck = actor->extra->puck;

	if (puck) {
		sector->jumpOut(puck);
		setActorPosition(actor, puck->_pos);
	}
}

void SputActorModule::setActorConstrain(SputActor *actor, int constrain) {
	if (!(actor->mode & 1) || !constrain) {
		Puck *puck = actor->extra->puck;

		if (puck)
			puck->constrain(constrain);
	}
}

} // End of namespace Grim
