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

#include "engines/grim/emi/actor/actor_ps2.h"
#include "engines/grim/emi/actor/puck_ps2.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/math/singlemath.h"
#include "engines/grim/emi/nodes/rmesh.h"
#include "engines/grim/emi/nodes/rprimitiveinstance.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

static const float kCollisionFloatMax = 3.4028235e+38f;
static const int kCollisionAnyPlane = -1;
static const float kTankTurnRate = 140.0f;

ActorPs2::ActorPs2(EmiEngine *engine, const char *newActorName) : Actor(engine, newActorName) {
}

Puck_v2 *ActorPs2::createPuck(float newReflectionAngle) {
	return new Puck_v2Ps2(_engine, newReflectionAngle);
}

void ActorPs2::updateMovement(float frameDelta, float scaledDelta) {
	if ((_mode & 1) != 0)
		followPath(frameDelta);

	if (_extra && _extra->_puck)
		updateWalk(frameDelta);
}

float ActorPs2::collisionRadius(const RPrimitive *prim, float scale) {
	TruncFloat two(2.0f);
	TruncFloat extentX = TruncFloat(prim->_box._max._x) - TruncFloat(prim->_box._min._x);
	TruncFloat extentZ = TruncFloat(prim->_box._max._z) - TruncFloat(prim->_box._min._z);
	TruncFloat quarterX = (extentX / two) * extentX / two;
	TruncFloat quarterZ = (extentZ / two) * extentZ / two;

	return (TruncFloat(scale) * TruncFloat::squareRoot(quarterX + quarterZ)).toFloat();
}

void ActorPs2::followPath(float deltaTime) {
	const float maxAngle = 3.4028235e38f;
	const float limitAngle = 80.0000076f;

	if (!_extra) {
		warning("ActorPs2::followPath(): missing extra data");

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
				warning("ActorPs2::followPath(): path index out of range");

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
		static_cast<Puck_v2Ps2 *>(_extra->_puck)->pointAtTrunc(&_extra->_walkTarget._x, TruncFloat(maxAngle));
	else
		warning("ActorPs2::followPath(): missing puck");

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

void ActorPs2::updateWalk(float deltaTime) {
	Puck_v2Ps2 *puck = static_cast<Puck_v2Ps2 *>(_extra->_puck);
	int turnDirection = _extra->_turnDir;

	if (turnDirection != 0 && (_mode & 1) == 0) {
		TruncFloat turn = TruncFloat(deltaTime) * TruncFloat(_extra->_timeScale);

		turn = turn * TruncFloat(getTurnRate());
		turn = turn * TruncFloat::fromInt(turnDirection);
		puck->rotateTrunc(-turn);
	}

	if (_extra->_walking) {
		TruncFloat distance = TruncFloat(_extra->_walkRate) * TruncFloat(_extra->_timeScale);

		distance = TruncFloat(deltaTime) * distance;

		puck->forwardTrunc(distance);
		_extra->_walking = true;
	}

	_extra->_aligned = alignToPuck(puck);

	if (_extra->_aligned && _extra->_turnDir == 0)
		_extra->_tankTurn = false;

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

float ActorPs2::getTurnRate() {
	// The slow turn only belongs to the character-relative movement, so the camera-relative one drops it
	if (_extra->_tankTurn && _engine->getLua()->isCameraRelativePs2())
		_extra->_tankTurn = false;

	// A lower rate set by the scripts wins
	if (_extra->_tankTurn && _extra->_turnRate > kTankTurnRate)
		return kTankTurnRate;

	return _extra->_turnRate;
}

bool ActorPs2::alignToPuck(Puck_v2 *puck) {
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
		turnStep = TruncFloat(getTurnRate()) * TruncFloat(_extra->_timeScale);
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

int ActorPs2::findClosestCandidate(float startX, float startY, float startZ, float goalX, float goalY, float goalZ, const ObjectCorners_v2 *corners) {
	int found = -1;
	TruncFloat best(kCollisionFloatMax);

	for (int i = 0; i < ObjectCorners_v2::kNumCorners; i++) {
		if (!corners->valid[i])
			continue;

		TruncFloat cornerX(corners->corner[i][0]);
		TruncFloat cornerY(corners->corner[i][1]);
		TruncFloat cornerZ(corners->corner[i][2]);
		TruncFloat startDeltaX = TruncFloat(startX) - cornerX;
		TruncFloat startDeltaY = TruncFloat(startY) - cornerY;
		TruncFloat startDeltaZ = TruncFloat(startZ) - cornerZ;
		TruncFloat goalDeltaX = cornerX - TruncFloat(goalX);
		TruncFloat goalDeltaY = cornerY - TruncFloat(goalY);
		TruncFloat goalDeltaZ = cornerZ - TruncFloat(goalZ);
		TruncFloat first = TruncFloat::multiplyAdd(startDeltaX * startDeltaX + startDeltaY * startDeltaY, startDeltaZ, startDeltaZ);
		TruncFloat second = TruncFloat::multiplyAdd(goalDeltaX * goalDeltaX + goalDeltaY * goalDeltaY, goalDeltaZ, goalDeltaZ);
		TruncFloat total = first + second;

		if (total < best) {
			found = i;
			best = total;
		}
	}

	return found;
}

void ActorPs2::getOuttaTheBox(float *point, const float *box, const float *transform) {
	TruncFloat zero(0.0f);
	TruncFloat margin = TruncFloat::fromBits(0x3C23D70A);
	TruncFloat deltaX = TruncFloat(point[0]) - TruncFloat(transform[9]);
	TruncFloat deltaY = TruncFloat(point[1]) - TruncFloat(transform[10]);
	TruncFloat deltaZ = TruncFloat(point[2]) - TruncFloat(transform[11]);
	TruncFloat localX = TruncFloat::multiplyAdd(deltaX * TruncFloat(transform[0]) + deltaY * TruncFloat(transform[1]), deltaZ, TruncFloat(transform[2]));
	TruncFloat localY = TruncFloat::multiplyAdd(deltaX * TruncFloat(transform[3]) + deltaY * TruncFloat(transform[4]), deltaZ, TruncFloat(transform[5]));
	TruncFloat localZ = TruncFloat::multiplyAdd(deltaX * TruncFloat(transform[6]) + deltaY * TruncFloat(transform[7]), deltaZ, TruncFloat(transform[8]));

	if (!(localX <= TruncFloat(box[3]) && localX >= TruncFloat(box[0]) && localY <= TruncFloat(box[4]) && localY >= TruncFloat(box[1]) && localZ <= TruncFloat(box[5]) && localZ >= TruncFloat(box[2])))
		return;

	TruncFloat outX = localX;
	TruncFloat outY = localY;
	TruncFloat outZ = localZ;
	TruncFloat best = TruncFloat::fromBits(0x7F7FFFFF);
	TruncFloat limit(box[5]);

	if (limit > localZ) {
		TruncFloat distance = limit - localZ;

		if (distance < zero)
			distance = -distance;

		if (distance < best) {
			outX = localX;
			outY = localY;
			outZ = margin + limit;
			best = distance;
		}
	}

	limit = TruncFloat(box[2]);

	if (limit < localZ) {
		TruncFloat distance = limit - localZ;

		if (distance < zero)
			distance = -distance;

		if (distance < best) {
			outX = localX;
			outY = localY;
			outZ = limit - margin;
			best = distance;
		}
	}

	limit = TruncFloat(box[3]);

	if (limit > localX) {
		TruncFloat distance = limit - localX;

		if (distance < zero)
			distance = -distance;

		if (distance < best) {
			outX = margin + limit;
			outY = localY;
			outZ = localZ;
			best = distance;
		}
	}

	limit = TruncFloat(box[0]);

	if (limit < localX) {
		TruncFloat distance = limit - localX;

		if (distance < zero)
			distance = -distance;

		if (distance < best) {
			outX = limit - margin;
			outY = localY;
			outZ = localZ;
		}
	}

	float local[3];
	float rotated[3];

	local[0] = outX.toFloat();
	local[1] = outY.toFloat();
	local[2] = outZ.toFloat();

	SingleMath::rotate(transform, local, rotated);

	point[0] = (TruncFloat(rotated[0]) + TruncFloat(transform[9])).toFloat();
	point[1] = (TruncFloat(rotated[1]) + TruncFloat(transform[10])).toFloat();
	point[2] = (TruncFloat(rotated[2]) + TruncFloat(transform[11])).toFloat();
}

bool ActorPs2::testCollision(EmiEngine *engine, float startX, float startY, float startZ, float endX, float endY, float endZ, float originX, float originY, float originZ, const float *box, const float *transform) {
	TruncFloat best(1.0f);
	TruncFloat startDeltaX = (TruncFloat(startX) - TruncFloat(originX)) - TruncFloat(transform[9]);
	TruncFloat startDeltaY = (TruncFloat(startY) - TruncFloat(originY)) - TruncFloat(transform[10]);
	TruncFloat startDeltaZ = (TruncFloat(startZ) - TruncFloat(originZ)) - TruncFloat(transform[11]);
	TruncFloat localStartX = TruncFloat::multiplyAdd(startDeltaX * TruncFloat(transform[0]) + startDeltaY * TruncFloat(transform[1]), startDeltaZ, TruncFloat(transform[2]));
	TruncFloat localStartY = TruncFloat::multiplyAdd(startDeltaX * TruncFloat(transform[3]) + startDeltaY * TruncFloat(transform[4]), startDeltaZ, TruncFloat(transform[5]));
	TruncFloat localStartZ = TruncFloat::multiplyAdd(startDeltaX * TruncFloat(transform[6]) + startDeltaY * TruncFloat(transform[7]), startDeltaZ, TruncFloat(transform[8]));
	TruncFloat endDeltaX = (TruncFloat(endX) - TruncFloat(originX)) - TruncFloat(transform[9]);
	TruncFloat endDeltaY = (TruncFloat(endY) - TruncFloat(originY)) - TruncFloat(transform[10]);
	TruncFloat endDeltaZ = (TruncFloat(endZ) - TruncFloat(originZ)) - TruncFloat(transform[11]);
	TruncFloat localEndX = TruncFloat::multiplyAdd(endDeltaX * TruncFloat(transform[0]) + endDeltaY * TruncFloat(transform[1]), endDeltaZ, TruncFloat(transform[2]));
	TruncFloat localEndY = TruncFloat::multiplyAdd(endDeltaX * TruncFloat(transform[3]) + endDeltaY * TruncFloat(transform[4]), endDeltaZ, TruncFloat(transform[5]));
	TruncFloat localEndZ = TruncFloat::multiplyAdd(endDeltaX * TruncFloat(transform[6]) + endDeltaY * TruncFloat(transform[7]), endDeltaZ, TruncFloat(transform[8]));
	TruncFloat directionX = localEndX - localStartX;
	TruncFloat directionY = localEndY - localStartY;
	TruncFloat directionZ = localEndZ - localStartZ;
	TruncFloat limit(box[5]);

	if (localStartZ > limit && localEndZ < limit) {
		TruncFloat fraction = (limit - localStartZ) / directionZ;

		if (fraction < best) {
			TruncFloat hitX = localStartX + directionX * fraction;
			TruncFloat hitY = localStartY + directionY * fraction;
			TruncFloat hitZ = localStartZ + directionZ * fraction;

			if (hitX > TruncFloat(box[0]) && hitX < TruncFloat(box[3])) {
				engine->_hitPoint._x = hitX.toFloat();
				engine->_hitPoint._y = hitY.toFloat();
				engine->_hitPoint._z = hitZ.toFloat();
				best = fraction;
			}
		}
	}

	limit = TruncFloat(box[2]);

	if (localStartZ < limit && localEndZ > limit) {
		TruncFloat fraction = (limit - localStartZ) / directionZ;

		if (fraction < best) {
			TruncFloat hitX = localStartX + directionX * fraction;
			TruncFloat hitY = localStartY + directionY * fraction;
			TruncFloat hitZ = localStartZ + directionZ * fraction;

			if (hitX > TruncFloat(box[0]) && hitX < TruncFloat(box[3])) {
				engine->_hitPoint._x = hitX.toFloat();
				engine->_hitPoint._y = hitY.toFloat();
				engine->_hitPoint._z = hitZ.toFloat();
				best = fraction;
			}
		}
	}

	limit = TruncFloat(box[3]);

	if (localStartX > limit && localEndX < limit) {
		TruncFloat fraction = (limit - localStartX) / directionX;

		if (fraction < best) {
			TruncFloat hitX = localStartX + directionX * fraction;
			TruncFloat hitY = localStartY + directionY * fraction;
			TruncFloat hitZ = localStartZ + directionZ * fraction;

			if (hitZ > TruncFloat(box[2]) && hitZ < TruncFloat(box[5])) {
				engine->_hitPoint._x = hitX.toFloat();
				engine->_hitPoint._y = hitY.toFloat();
				engine->_hitPoint._z = hitZ.toFloat();
				best = fraction;
			}
		}
	}

	limit = TruncFloat(box[0]);

	if (localStartX < limit && localEndX > limit) {
		TruncFloat fraction = (limit - localStartX) / directionX;

		if (fraction < best) {
			TruncFloat hitX = localStartX + directionX * fraction;
			TruncFloat hitY = localStartY + directionY * fraction;
			TruncFloat hitZ = localStartZ + directionZ * fraction;

			if (hitZ > TruncFloat(box[2]) && hitZ < TruncFloat(box[5])) {
				engine->_hitPoint._x = hitX.toFloat();
				engine->_hitPoint._y = hitY.toFloat();
				engine->_hitPoint._z = hitZ.toFloat();
				best = fraction;
			}
		}
	}

	return best != TruncFloat(1.0f);
}

int ActorPs2::buildAvoidingPath(Actor *other, float *path, int count, float *outPath) {
	int outCount = 0;
	float startX = 0.0f;
	float startY = 0.0f;
	float startZ = 0.0f;
	float endX = 0.0f;
	float endY = 0.0f;
	float endZ = 0.0f;
	float goal[3];

	goal[0] = 0.0f;
	goal[1] = 0.0f;
	goal[2] = 0.0f;

	if (!other || !other->_extra || !_extra) {
		if (other && other->_extra)
			warning("ActorPs2::buildAvoidingPath(): the actor has no extra data");

		for (int i = 0; i < count; i++) {
			outPath[3 * i] = path[3 * i];
			outPath[3 * i + 1] = path[3 * i + 1];
			outPath[3 * i + 2] = path[3 * i + 2];
			outCount++;
		}

		return outCount;
	}

	if (count <= 0) {
		warning("ActorPs2::buildAvoidingPath(): the path has no points");

		return 0;
	}

	TruncFloat thisRadius = TruncFloat(0.5f) * TruncFloat(_extra->_collisionScale);

	if (!_instances.empty() && _instances[0]) {
		if (_instances[0]->_prim)
			thisRadius = TruncFloat(collisionRadius(_instances[0]->_prim, _extra->_collisionScale));
		else
			warning("ActorPs2::buildAvoidingPath(): primitive instance without primitive");
	}

	TruncFloat otherRadius = TruncFloat(0.5f) * TruncFloat(other->_extra->_collisionScale);

	if (!other->_instances.empty() && other->_instances[0]) {
		if (other->_instances[0]->_prim)
			otherRadius = TruncFloat(collisionRadius(other->_instances[0]->_prim, other->_extra->_collisionScale));
		else
			warning("ActorPs2::buildAvoidingPath(): primitive instance without primitive");
	}

	TruncFloat radius = thisRadius + otherRadius;
	float radiusFloat = radius.toFloat();
	float negativeRadius = (-radius).toFloat();
	float box[6];

	box[0] = negativeRadius;
	box[1] = negativeRadius;
	box[2] = negativeRadius;
	box[3] = radiusFloat;
	box[4] = radiusFloat;
	box[5] = radiusFloat;

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	if ((other->_nodeFlags & kPositionValid) == 0)
		other->computeLocalPositionFromWorld();

	float *lastPoint = &path[3 * count - 3];

	getOuttaTheBox(lastPoint, box, other->_world._rot._m);

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	if ((other->_nodeFlags & kPositionValid) == 0)
		other->computeLocalPositionFromWorld();

	getOuttaTheBox(path, box, other->_world._rot._m);

	goal[0] = lastPoint[0];
	goal[1] = lastPoint[1];
	goal[2] = lastPoint[2];

	bool locked = _room.lock();
	Room_v2 *currentRoom = locked ? _room._typed : nullptr;
	WalkPlaneCollection *planes = nullptr;

	if (currentRoom)
		planes = currentRoom->_walkPlanes;
	else
		warning("ActorPs2::buildAvoidingPath(): the room could not be locked");

	if (locked)
		_room.unlock();

	if (!planes) {
		warning("ActorPs2::buildAvoidingPath(): the room has no walk planes");

		return 0;
	}

	if (!planes->planeAt(goal, kCollisionAnyPlane)) {
		_engine->getConsoleServices()->logWarning("Actor::CheckPathForCollisions -- Destination not valid after collision adjustment.\n");

		return 0;
	}

	int index = 0;
	bool collided = false;

	for (index = 0; index < count - 1; index++) {
		startX = path[3 * index];
		startY = path[3 * index + 1];
		startZ = path[3 * index + 2];
		endX = path[3 * index + 3];
		endY = path[3 * index + 4];
		endZ = path[3 * index + 5];

		outPath[3 * outCount] = path[3 * index];
		outPath[3 * outCount + 1] = path[3 * index + 1];
		outPath[3 * outCount + 2] = path[3 * index + 2];
		outCount++;

		if ((other->_nodeFlags & kWorldValid) == 0)
			other->updateWorldTransform();

		if (testCollision(_engine, startX, startY, startZ, endX, endY, endZ, 0.0f, 0.0f, 0.0f, box, other->_world._rot._m)) {
			collided = true;

			break;
		}
	}

	if (!collided) {
		outPath[3 * outCount] = path[3 * index];
		outPath[3 * outCount + 1] = path[3 * index + 1];
		outPath[3 * outCount + 2] = path[3 * index + 2];

		return outCount + 1;
	}

	ObjectCorners_v2 corners;

	for (int i = 0; i < ObjectCorners_v2::kNumCorners; i++) {
		corners.corner[i][0] = 0.0f;
		corners.corner[i][1] = 0.0f;
		corners.corner[i][2] = 0.0f;
	}

	float hit[3];
	float rotated[3];

	hit[0] = _engine->_hitPoint._x;
	hit[1] = _engine->_hitPoint._y;
	hit[2] = _engine->_hitPoint._z;

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	SingleMath::rotate(other->_world._rot._m, hit, rotated);

	hit[0] = (TruncFloat(rotated[0]) + TruncFloat(other->_world._pos._x)).toFloat();
	hit[1] = (TruncFloat(rotated[1]) + TruncFloat(other->_world._pos._y)).toFloat();
	hit[2] = (TruncFloat(rotated[2]) + TruncFloat(other->_world._pos._z)).toFloat();

	WalkPlane_v2 *closestPlane = planes->planeClosest(hit, kCollisionAnyPlane);

	if (closestPlane)
		closestPlane->project(hit);
	else
		warning("ActorPs2::buildAvoidingPath(): no walk plane near the hit point");

	corners.corner[0][0] = negativeRadius;
	corners.corner[0][1] = 0.0f;
	corners.corner[0][2] = negativeRadius;
	corners.corner[1][0] = radiusFloat;
	corners.corner[1][1] = 0.0f;
	corners.corner[1][2] = negativeRadius;
	corners.corner[2][0] = radiusFloat;
	corners.corner[2][1] = 0.0f;
	corners.corner[2][2] = radiusFloat;
	corners.corner[3][0] = negativeRadius;
	corners.corner[3][1] = 0.0f;
	corners.corner[3][2] = radiusFloat;

	for (int k = 0; k < ObjectCorners_v2::kNumCorners; k++) {
		float *corner = corners.corner[k];

		if ((other->_nodeFlags & kWorldValid) == 0)
			other->updateWorldTransform();

		SingleMath::rotate(other->_world._rot._m, corner, rotated);

		corner[0] = (TruncFloat(rotated[0]) + TruncFloat(other->_world._pos._x)).toFloat();
		corner[1] = hit[1];
		corner[2] = (TruncFloat(rotated[2]) + TruncFloat(other->_world._pos._z)).toFloat();

		corners.valid[k] = 0;
		endX = corner[0];
		endY = corner[1];
		endZ = corner[2];

		if ((other->_nodeFlags & kWorldValid) == 0)
			other->updateWorldTransform();

		if (!testCollision(_engine, startX, startY, startZ, endX, endY, endZ, 0.0f, 0.0f, 0.0f, box, other->_world._rot._m)
			&& planes->buildPath(&path[3 * index], corner, false))
			corners.valid[k] = 1;
	}

	int closest = findClosestCandidate(startX, startY, startZ, goal[0], goal[1], goal[2], &corners);

	if (closest == -1) {
		_engine->getConsoleServices()->logWarning("Actor::CheckPathForCollisions -- Cannot get around blocking object. Stopping.\n");

		return outCount;
	}

	if (!planes->buildPath(&path[3 * index], corners.corner[closest], true)) {
		_engine->getConsoleServices()->logWarning("Actor::CheckPathForCollisions -- Cannot get to best corner. Stopping.\n");

		return outCount;
	}

	float (*pathPoints)[3];
	int pathCount = planes->getPath(&pathPoints);

	for (int j = 1; j < pathCount; j++) {
		outPath[3 * outCount] = pathPoints[j][0];
		outPath[3 * outCount + 1] = pathPoints[j][1];
		outPath[3 * outCount + 2] = pathPoints[j][2];
		outCount++;
	}

	corners.valid[closest] = 0;

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	float *lastOutPoint = &outPath[3 * outCount - 3];
	float *nextOutPoint = &outPath[3 * outCount];

	if (!testCollision(_engine, lastOutPoint[0], lastOutPoint[1], lastOutPoint[2], goal[0], goal[1], goal[2], 0.0f, 0.0f, 0.0f, box, other->_world._rot._m)
		&& planes->buildPath(lastOutPoint, goal, true)) {
		pathCount = planes->getPath(&pathPoints);

		for (int j = 1; j < pathCount; j++) {
			outPath[3 * outCount] = pathPoints[j][0];
			outPath[3 * outCount + 1] = pathPoints[j][1];
			outPath[3 * outCount + 2] = pathPoints[j][2];
			outCount++;
		}

		return outCount;
	}

	int previousCorner = (closest + 3) % 4;
	int nextCorner = (closest + 5) % 4;
	const float *previousPoint = corners.corner[previousCorner];
	bool previousReachable = planes->buildPath(lastOutPoint, previousPoint, false);
	const float *nextPoint = corners.corner[nextCorner];
	bool nextReachable = planes->buildPath(lastOutPoint, nextPoint, false);
	int current = nextCorner;
	int direction = 1;

	if (nextReachable) {
		if (previousReachable) {
			TruncFloat previousGoalX = TruncFloat(previousPoint[0]) - TruncFloat(goal[0]);
			TruncFloat previousGoalY = TruncFloat(previousPoint[1]) - TruncFloat(goal[1]);
			TruncFloat previousGoalZ = TruncFloat(previousPoint[2]) - TruncFloat(goal[2]);
			TruncFloat previousLastX = TruncFloat(lastOutPoint[0]) - TruncFloat(previousPoint[0]);
			TruncFloat previousLastY = TruncFloat(lastOutPoint[1]) - TruncFloat(previousPoint[1]);
			TruncFloat previousLastZ = TruncFloat(lastOutPoint[2]) - TruncFloat(previousPoint[2]);
			TruncFloat nextGoalX = TruncFloat(nextPoint[0]) - TruncFloat(goal[0]);
			TruncFloat nextGoalY = TruncFloat(nextPoint[1]) - TruncFloat(goal[1]);
			TruncFloat nextGoalZ = TruncFloat(nextPoint[2]) - TruncFloat(goal[2]);
			TruncFloat nextLastX = TruncFloat(lastOutPoint[0]) - TruncFloat(nextPoint[0]);
			TruncFloat nextLastY = TruncFloat(lastOutPoint[1]) - TruncFloat(nextPoint[1]);
			TruncFloat nextLastZ = TruncFloat(lastOutPoint[2]) - TruncFloat(nextPoint[2]);
			TruncFloat previousToGoal = TruncFloat::multiplyAdd(previousGoalX * previousGoalX + previousGoalY * previousGoalY, previousGoalZ, previousGoalZ);
			TruncFloat previousToLast = TruncFloat::multiplyAdd(previousLastX * previousLastX + previousLastY * previousLastY, previousLastZ, previousLastZ);
			TruncFloat nextToGoal = TruncFloat::multiplyAdd(nextGoalX * nextGoalX + nextGoalY * nextGoalY, nextGoalZ, nextGoalZ);
			TruncFloat nextToLast = TruncFloat::multiplyAdd(nextLastX * nextLastX + nextLastY * nextLastY, nextLastZ, nextLastZ);
			TruncFloat previousDistance = previousToLast + previousToGoal;
			TruncFloat nextDistance = nextToLast + nextToGoal;

			if (previousDistance < nextDistance) {
				current = previousCorner;
				direction = -1;
			}
		}
	} else if (!previousReachable) {
		_engine->getConsoleServices()->logWarning("Actor::CheckPathForCollisions -- Left and right points are OOB! Stopping.\n");
		_engine->getConsoleServices()->logWarning("Actor::CheckPathForCollisions -- to fix this, expand the boxes or place the actor you are trying to walk around in a less congested area.\n");

		return outCount;
	} else {
		current = previousCorner;
		direction = -1;
	}

	for (;;) {
		if ((other->_nodeFlags & kWorldValid) == 0)
			other->updateWorldTransform();

		bool blocked = testCollision(_engine, nextOutPoint[-3], nextOutPoint[-2], nextOutPoint[-1], goal[0], goal[1], goal[2], 0.0f, 0.0f, 0.0f, box, other->_world._rot._m);

		if (!blocked || current == closest)
			break;

		outCount++;
		nextOutPoint[0] = corners.corner[current][0];
		nextOutPoint[1] = corners.corner[current][1];
		nextOutPoint[2] = corners.corner[current][2];
		nextOutPoint += 3;
		current = (current + direction + 4) % 4;
	}

	outPath[3 * outCount] = goal[0];
	outPath[3 * outCount + 1] = goal[1];
	outPath[3 * outCount + 2] = goal[2];

	return outCount + 1;
}

} // End of namespace Grim
