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

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/math/singlemath.h"
#include "engines/grim/emi/actor/puck_v2.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

void Actor::followPath(float deltaTime) {
	const float maxAngle = 3.4028235e38f;

	if (!_extra) {
		warning("Actor::followPath(): missing extra data");

		return;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	double deltaX = (double)_world._pos._x - _extra->_walkTarget._x;
	double deltaY = (double)_world._pos._y - _extra->_walkTarget._y;
	double deltaZ = (double)_world._pos._z - _extra->_walkTarget._z;
	double distance = sqrt((deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX);

	float rate = _extra->_walkRate;

	if (rate < 0.0f)
		rate = -rate;

	float limit = (float)((double)rate * _extra->_timeScale * deltaTime * 1.25);

	// The disasm takes the branches below also when the comparisons are unordered
	if (!(distance >= limit)) {
		do {
			if (_extra->_pathIndex >= _extra->_numPathPoints)
				break;

			_extra->_pathIndex++;
			setActorPosition(&_extra->_walkTarget._x);

			if (_extra->_pathIndex < Extra_v2::kMaxPathPoints)
				_extra->_walkTarget = _extra->_path[_extra->_pathIndex];
			else
				warning("Actor::followPath(): path index out of range");

			if ((_nodeFlags & kWorldValid) == 0)
				updateWorldTransform();

			deltaY = (double)_world._pos._y - _extra->_walkTarget._y;
			deltaZ = (double)_world._pos._z - _extra->_walkTarget._z;
			deltaX = (double)_world._pos._x - _extra->_walkTarget._x;
			distance = sqrt((deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX);
		} while (!(distance >= limit));

		if (_extra->_pathIndex == _extra->_numPathPoints) {
			_mode &= ~1U;

			if (ChoreInstance::isInstanceValid(_engine, _choreSlotIds[1]))
				ChoreInstance::getInstance(_engine, _choreSlotIds[1])->stop();

			_choreSlotIds[1] = 0;

			return;
		}
	}

	_mode |= 1U;

	if (_extra->_puck)
		_extra->_puck->pointAt(&_extra->_walkTarget._x, maxAngle);
	else
		warning("Actor::followPath(): missing puck");

	double cosLimit = cos((double)kDegreesToRadians * 80.0);
	double cosAlign = cos((double)kDegreesToRadians * _extra->_alignOff);

	if (!(cosLimit >= cosAlign))
		forward();
}

void Actor::updateWalk(float deltaTime) {
	Puck_v2 *puck = _extra->_puck;
	int turnDirection = _extra->_turnDir;

	if (turnDirection != 0 && (_mode & 1) == 0)
		puck->rotate((float)-((double)deltaTime * _extra->_timeScale * _extra->_turnRate * (double)turnDirection));

	if (_extra->_walking) {
		float distance = (float)((double)_extra->_timeScale * _extra->_walkRate * deltaTime);
		int allowLeft = 1;
		int allowRight = 1;

		if (!_extra->_walkDominate) {
			allowLeft = _extra->_turnDir != -1;
			allowRight = _extra->_turnDir != 1;
		}

		_extra->_walking = puck->forward(distance, allowLeft, allowRight);
	}

	_extra->_aligned = alignToPuck(puck);

	if (_extra->_turnDir == 0 && !_extra->_aligned) {
		if (_extra->_alignOff > 180.0f)
			_extra->_turnDir = 1;
		else
			_extra->_turnDir = -1;
	}

	if ((puck->_nodeFlags & kPositionValid) == 0)
		puck->computeLocalPositionFromWorld();

	setLocalPosition(puck->_pos);
}

bool Actor::alignToPuck(Puck_v2 *puck) {
	if ((_nodeFlags & kEulerValid) == 0)
		computeLocalMatrixFromWorld();

	float savedX = _eulerDeg._x;
	float savedY = _eulerDeg._y;
	float savedZ = _eulerDeg._z;

	if ((puck->_nodeFlags & kEulerValid) == 0)
		puck->computeLocalMatrixFromWorld();

	double difference = puck->_eulerDeg._y;

	if (difference - difference != 0.0 || (double)savedY - savedY != 0.0) {
		warning("Actor::alignToPuck(): invalid yaw");
		difference = 0.0;
		savedY = 0.0f;
	}

	while (difference < savedY)
		difference += 360.0;

	difference = difference - savedY;

	while (difference >= 360.0)
		difference -= 360.0;

	double sum = difference + _extra->_offsetYaw;
	float alignAngle = (float)sum;

	// The disasm compares the sum before it is rounded to float
	if (!(sum >= 0.5) || alignAngle > 359.5f) {
		_extra->_snapTurn = false;
		_extra->_alignOff = 0.0f;

		return true;
	}

	double turnStep = 360.0;

	_extra->_alignOff = alignAngle;

	if (!_extra->_snapTurn) {
		float frameTime = _engine->getFrameTime();

		turnStep = (double)frameTime * _extra->_timeScale * _extra->_turnRate;
	}

	bool reversed = false;

	if (alignAngle > 180.0f) {
		reversed = true;
		alignAngle = 360.0f - alignAngle;
	}

	if (!(turnStep + 0.01f >= alignAngle))
		alignAngle = (float)(((double)alignAngle - turnStep) * 0.1f + turnStep);

	double direction = reversed ? -1.0 : 1.0;

	_eulerDeg._x = savedX;
	_eulerDeg._z = savedZ;
	_eulerDeg._y = (float)(direction * alignAngle + savedY);

	_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
	updateLocalState();

	// The disasm returns the previous value of snapTurn
	bool previousSnapTurn = _extra->_snapTurn;

	_extra->_snapTurn = false;

	return previousSnapTurn;
}

void Actor::followPathPs2(float deltaTime) {
	const float maxAngle = 3.4028235e38f;
	const float limitAngle = 80.0000076f;

	if (!_extra) {
		warning("Actor::followPathPs2(): missing extra data");

		return;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	float offset[3];

	offset[0] = (TruncFloat(_world._pos._x) - TruncFloat(_extra->_walkTarget._x)).toFloat();
	offset[1] = (TruncFloat(_world._pos._y) - TruncFloat(_extra->_walkTarget._y)).toFloat();
	offset[2] = (TruncFloat(_world._pos._z) - TruncFloat(_extra->_walkTarget._z)).toFloat();

	TruncFloat distance(SingleMath::length(offset));
	TruncFloat rate(_extra->_walkRate);

	if (rate < TruncFloat(0.0f))
		rate = -rate;

	TruncFloat scaled = TruncFloat(deltaTime) * rate;

	scaled = TruncFloat(_extra->_timeScale) * scaled;

	TruncFloat limit = TruncFloat(1.25f) * scaled;

	if (distance < limit) {
		while (distance < limit && _extra->_pathIndex < _extra->_numPathPoints) {
			_extra->_pathIndex++;
			setActorPosition(&_extra->_walkTarget._x);

			if (_extra->_pathIndex < Extra_v2::kMaxPathPoints)
				_extra->_walkTarget = _extra->_path[_extra->_pathIndex];
			else
				warning("Actor::followPathPs2(): path index out of range");

			if ((_nodeFlags & kWorldValid) == 0)
				updateWorldTransform();

			offset[0] = (TruncFloat(_world._pos._x) - TruncFloat(_extra->_walkTarget._x)).toFloat();
			offset[1] = (TruncFloat(_world._pos._y) - TruncFloat(_extra->_walkTarget._y)).toFloat();
			offset[2] = (TruncFloat(_world._pos._z) - TruncFloat(_extra->_walkTarget._z)).toFloat();
			distance = TruncFloat(SingleMath::length(offset));
		}

		if (_extra->_pathIndex == _extra->_numPathPoints) {
			_mode &= ~1U;

			if (ChoreInstance::isInstanceValid(_engine, _choreSlotIds[1]))
				ChoreInstance::getInstance(_engine, _choreSlotIds[1])->stop();

			_choreSlotIds[1] = 0;

			return;
		}
	}

	_mode |= 1U;

	if (_extra->_puck)
		_extra->_puck->pointAtPs2(&_extra->_walkTarget._x, TruncFloat(maxAngle));
	else
		warning("Actor::followPathPs2(): missing puck");

	float sine;
	float cosineLimit;
	float cosineAlign;

	SingleMath::sinCosDegrees(limitAngle, &sine, &cosineLimit);

	TruncFloat alignAngle = TruncFloat(SingleMath::kDegreesToRadians) * TruncFloat(_extra->_alignOff);

	alignAngle = TruncFloat(SingleMath::kRadiansToDegrees) * alignAngle;
	SingleMath::sinCosDegrees(alignAngle.toFloat(), &sine, &cosineAlign);

	if (!(TruncFloat(cosineAlign) <= TruncFloat(cosineLimit)))
		forward();
}

void Actor::updateWalkPs2(float deltaTime) {
	Puck_v2 *puck = _extra->_puck;
	int turnDirection = _extra->_turnDir;

	if (turnDirection != 0 && (_mode & 1) == 0) {
		TruncFloat turn = TruncFloat(deltaTime) * TruncFloat(_extra->_timeScale);

		turn = turn * TruncFloat(_extra->_turnRate);
		turn = turn * TruncFloat::fromInt(turnDirection);
		puck->rotatePs2(-turn);
	}

	if (_extra->_walking) {
		TruncFloat distance = TruncFloat(_extra->_walkRate) * TruncFloat(_extra->_timeScale);

		distance = TruncFloat(deltaTime) * distance;

		puck->forwardPs2(distance);
		_extra->_walking = true;
	}

	_extra->_aligned = alignToPuckPs2(puck);

	if (_extra->_turnDir == 0 && !_extra->_aligned) {
		if (TruncFloat(_extra->_alignOff) > TruncFloat(180.0f))
			_extra->_turnDir = 1;
		else
			_extra->_turnDir = -1;
	}

	if ((puck->_nodeFlags & kPositionValid) == 0)
		puck->computeLocalPositionFromWorld();

	setLocalPosition(puck->_pos);
}

bool Actor::alignToPuckPs2(Puck_v2 *puck) {
	if ((_nodeFlags & kEulerValid) == 0)
		computeLocalMatrixFromWorld();

	float savedX = _eulerDeg._x;
	TruncFloat savedY(_eulerDeg._y);
	float savedZ = _eulerDeg._z;
	TruncFloat full(360.0f);

	if ((puck->_nodeFlags & kEulerValid) == 0)
		puck->computeLocalMatrixFromWorld();

	TruncFloat difference(puck->_eulerDeg._y);

	while (difference < savedY)
		difference = difference + full;

	difference = difference - savedY;

	while (!(difference < full))
		difference = difference - full;

	TruncFloat sum = difference + TruncFloat(_extra->_offsetYaw);

	if (sum < TruncFloat(0.05f) || !(sum <= TruncFloat(359.95f))) {
		_extra->_snapTurn = false;
		_extra->_alignOff = 0.0f;

		return true;
	}

	_extra->_alignOff = sum.toFloat();

	TruncFloat turnStep = full;

	if (!_extra->_snapTurn) {
		turnStep = TruncFloat(_extra->_turnRate) * TruncFloat(_extra->_timeScale);
		turnStep = turnStep * TruncFloat(_engine->getFrameTime());
	}

	bool reversed = false;
	TruncFloat angle = sum;

	if (!(angle <= TruncFloat(180.0f))) {
		reversed = true;
		angle = full - angle;
	}

	if (!(angle <= turnStep + TruncFloat(0.01f))) {
		TruncFloat remaining = angle - turnStep;

		angle = turnStep + TruncFloat(0.1f) * remaining;
	}

	TruncFloat step = reversed ? angle * TruncFloat(-1.0f) : angle * TruncFloat(1.0f);

	_eulerDeg._x = savedX;
	_eulerDeg._y = (savedY + step).toFloat();
	_eulerDeg._z = savedZ;

	_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
	updateLocalState();

	bool previousSnapTurn = _extra->_snapTurn;

	_extra->_snapTurn = false;

	return previousSnapTurn;
}

void Actor::turn(float degrees) {
	if (!_extra) {
		warning("Actor::turn(): missing extra data");

		return;
	}

	if (!_extra->_puck)
		enablePuck(true);

	if (degrees > 0.0f)
		_extra->_turnDir = 1;
	else
		_extra->_turnDir = -1;
}

void Actor::forward() {
	if (!_extra) {
		warning("Actor::forward(): missing extra data");

		return;
	}

	if (!_extra->_puck)
		enablePuck(true);

	_extra->_walking = true;
}

void Actor::enableWalkSystem(bool enable) {
	if (!_extra)
		allocateActorExtra();

	if (enable) {
		_extra->_inBoxes = true;
		enablePuck(true);

		if (_room._resource) {
			Room_v2 *lockedRoom = nullptr;

			if (_room.lock())
				lockedRoom = _room._typed;

			WalkPlaneCollection *collection = nullptr;

			if (lockedRoom)
				collection = lockedRoom->_walkPlanes;
			else
				warning("Actor::enableWalkSystem(): the room could not be locked");

			_extra->_puck->validate(collection);

			if (lockedRoom)
				_room.unlock();
		}
	} else {
		_extra->_inBoxes = false;

		if (_extra->_puck)
			_extra->_puck->_plane = nullptr;
	}
}

void Actor::enablePuck(bool enable) {
	if (!_extra)
		allocateActorExtra();

	if (enable) {
		if (_extra->_puck) {
			_extra->_puck->reset();
		} else {
			const float maxAngle = 3.4028235e38f;
			float target[3];

			Puck_v2 *newPuck = new Puck_v2(_engine, _extra->_reflectionAngle);

			newPuck->_actorName = _actorName;

			if ((_nodeFlags & kWorldValid) == 0)
				updateWorldTransform();

			newPuck->setLocalPosition(_world._pos);

			if ((_nodeFlags & kWorldValid) == 0)
				updateWorldTransform();

			target[0] = _world._rot._m[6];
			target[1] = _world._rot._m[7];
			target[2] = _world._rot._m[8];

			if ((_nodeFlags & kWorldValid) == 0)
				updateWorldTransform();

			target[0] = target[0] + _world._pos._x;
			target[1] = target[1] + _world._pos._y;
			target[2] = target[2] + _world._pos._z;

			newPuck->pointAt(target, maxAngle);
			_extra->_puck = newPuck;
		}
	} else if (_extra->_puck) {
		delete _extra->_puck;
		_extra->_puck = nullptr;
	}
}

void Actor::setActorPosition(const float *position) {
	if (_extra && _extra->_puck) {
		Puck_v2 *puck = _extra->_puck;
		CVector newPosition;

		puck->reset();

		newPosition._x = position[0];
		newPosition._y = position[1];
		newPosition._z = position[2];
		puck->setLocalPosition(newPosition);

		if (_extra->_inBoxes) {
			Room_v2 *lockedRoom = nullptr;

			if (_room.lock())
				lockedRoom = _room._typed;

			WalkPlaneCollection *collection = nullptr;

			if (lockedRoom) {
				collection = lockedRoom->_walkPlanes;
				_room.unlock();
			} else {
				warning("Actor::setActorPosition(): the room could not be locked");
			}

			puck->validate(collection);
		}

		if ((puck->_nodeFlags & kPositionValid) == 0)
			puck->computeLocalPositionFromWorld();

		setLocalPosition(puck->_pos);
	} else {
		CVector newPosition;

		newPosition._x = position[0];
		newPosition._y = position[1];
		newPosition._z = position[2];
		setLocalPosition(newPosition);
	}
}

void Actor::setActorRotation(const float *rotation, bool snap) {
	_mode &= ~1U;

	if (_extra && _extra->_puck) {
		Puck_v2 *puck = _extra->_puck;

		puck->_eulerDeg._y = rotation[1];
		puck->_eulerDeg._x = 0.0f;
		puck->_eulerDeg._z = 0.0f;
		puck->_nodeFlags = (puck->_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
		puck->updateLocalState();

		_extra->_snapTurn = snap;

		if ((_nodeFlags & kEulerValid) == 0)
			computeLocalMatrixFromWorld();

		if (_engine->getGamePlatform() == Common::kPlatformPS2)
			_extra->_alignOff = (TruncFloat(rotation[1]) - TruncFloat(_eulerDeg._y)).toFloat();
		else
			_extra->_alignOff = rotation[1] - _eulerDeg._y;

		if ((_nodeFlags & kEulerValid) == 0)
			computeLocalMatrixFromWorld();

		float currentYaw = _eulerDeg._y;

		_eulerDeg._x = rotation[0];
		_eulerDeg._y = currentYaw;
		_eulerDeg._z = rotation[2];
		_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
		updateLocalState();
	} else {
		_eulerDeg._x = rotation[0];
		_eulerDeg._y = rotation[1];
		_eulerDeg._z = rotation[2];
		_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
		updateLocalState();
	}
}

void Actor::setReflection(float angle) {
	if (!_extra) {
		warning("Actor::setReflection(): missing extra data");

		return;
	}

	if (_extra->_puck)
		_extra->_puck->_reflectionAngle = angle;
}

void Actor::setWalkDominate(bool dominate) {
	if (!_extra) {
		warning("Actor::setWalkDominate(): missing extra data");

		return;
	}

	_extra->_walkDominate = dominate;
}

void Actor::leaveSector(WalkPlane_v2 *plane) {
	if (!plane)
		return;

	if (!_extra) {
		warning("Actor::leaveSector(): missing extra data");

		return;
	}

	Puck_v2 *puck = _extra->_puck;

	if (puck) {
		plane->jumpOut(puck);

		if ((puck->_nodeFlags & kPositionValid) == 0)
			puck->computeLocalPositionFromWorld();

		setActorPosition(&puck->_pos._x);
	}
}

void Actor::actorWalkTo(float *target, const float *goal, Actor *other) {
	// The disasm's output buffer holds only 30 points but the avoiding path can grow past that, let's give it room
	enum {
		kOutputPathCapacity = 100
	};

	float inputPath[Extra_v2::kMaxPathPoints][3];
	float outputPath[kOutputPathCapacity][3];
	float start[3];

	memset(inputPath, 0, sizeof(inputPath));
	memset(outputPath, 0, sizeof(outputPath));

	_mode &= ~1U;

	if (!_extra) {
		warning("Actor::actorWalkTo(): missing extra data");

		return;
	}

	if (_extra->_puck) {
		Room_v2 *lockedRoom = nullptr;

		if (_room.lock())
			lockedRoom = _room._typed;

		WalkPlaneCollection *collection = nullptr;

		if (lockedRoom) {
			collection = lockedRoom->_walkPlanes;
			_room.unlock();
		} else {
			warning("Actor::actorWalkTo(): the room could not be locked");
		}

		if (!collection) {
			warning("Actor::actorWalkTo(): the room has no walk planes");

			return;
		}

		Puck_v2 *puck = _extra->_puck;
		WalkPlane_v2 *plane = collection->planeClosest(target, 0x1000);

		if (plane)
			plane->closestPoint(target);
		else if (_engine->isDemo())
			warning("Actor::actorWalkTo(): could not find a walk plane close to the destination");

		if ((puck->_nodeFlags & kPositionValid) == 0)
			puck->computeLocalPositionFromWorld();

		start[0] = puck->_pos._x;
		start[1] = puck->_pos._y;
		start[2] = puck->_pos._z;

		if (collection->buildPath(start, target, true)) {
			float (*pathPoints)[3];
			int pointCount = collection->getPath(&pathPoints);

			if (pointCount <= 1 && _engine->isDemo())
				warning("Actor::actorWalkTo(): the path has no waypoints");

			for (int i = 0; i < pointCount; i++) {
				inputPath[i][0] = pathPoints[i][0];
				inputPath[i][1] = pathPoints[i][1];
				inputPath[i][2] = pathPoints[i][2];
			}

			int newCount = buildAvoidingPath(other, &inputPath[0][0], pointCount, &outputPath[0][0]);

			if (newCount > Extra_v2::kMaxPathPoints) {
				warning("Actor::actorWalkTo(): too many path points");
				newCount = Extra_v2::kMaxPathPoints;
			}

			_extra->_numPathPoints = newCount;
			_extra->_pathIndex = 1;

			for (int i = 0; i < newCount; i++) {
				_extra->_path[i]._x = outputPath[i][0];
				_extra->_path[i]._y = outputPath[i][1];
				_extra->_path[i]._z = outputPath[i][2];
			}

			_extra->_walkTarget = _extra->_path[_extra->_pathIndex];

			if (newCount > 1)
				_mode |= 1U;
		}

		_extra->_goal._x = goal[0];
		_extra->_goal._y = goal[1];
		_extra->_goal._z = goal[2];
	} else if (_engine->isDemo()) {
		warning("Actor::actorWalkTo(): the actor is not using the walk system");
	}
}

} // End of namespace Grim
