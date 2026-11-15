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
#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/math/singlemath.h"
#include "engines/grim/emi/nodes/rmesh.h"
#include "engines/grim/emi/nodes/rprimitiveinstance.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

static const float kCollisionFloatMax = 3.4028235e+38f;
static const float kCollisionMargin = 0.01f;
static const float kCollisionMinLength = 1e-20f;
static const int kCollisionAnyPlane = -1;

static float collisionRadiusPs2(const RPrimitive *prim, float scale) {
	TruncFloat two(2.0f);
	TruncFloat extentX = TruncFloat(prim->_box._max._x) - TruncFloat(prim->_box._min._x);
	TruncFloat extentZ = TruncFloat(prim->_box._max._z) - TruncFloat(prim->_box._min._z);
	TruncFloat quarterX = (extentX / two) * extentX / two;
	TruncFloat quarterZ = (extentZ / two) * extentZ / two;

	return (TruncFloat(scale) * TruncFloat::squareRoot(quarterX + quarterZ)).toFloat();
}

static double calcCollisionRadius(const RPrimitive *prim) {
	double extentX = (double)prim->_box._max._x - prim->_box._min._x;
	double extentZ = (double)prim->_box._max._z - prim->_box._min._z;

	return sqrt((extentZ * extentZ) * 0.25 + (extentX * extentX) * 0.25);
}

void Actor::getOuttaTheBox(float *point, const float *box, const float *transform) {
	double deltaX = (double)point[0] - transform[9];
	double deltaY = (double)point[1] - transform[10];
	double deltaZ = (double)point[2] - transform[11];

	float localX = (float)((deltaX * transform[0] + deltaZ * transform[2]) + deltaY * transform[1]);
	float localY = (float)((deltaZ * transform[5] + deltaY * transform[4]) + deltaX * transform[3]);
	float localZ = (float)((deltaX * transform[6] + deltaZ * transform[8]) + deltaY * transform[7]);

	if (localX <= box[3] && localX >= box[0] && localY <= box[4] && localY >= box[1] && localZ <= box[5] && localZ >= box[2]) {
		float outX = localX;
		float outY = localY;
		float outZ = localZ;
		double best = kCollisionFloatMax;

		if (box[5] > localZ) {
			double distance = (double)box[5] - localZ;

			if (distance < 0.0)
				distance = -distance;

			if (distance < best) {
				outX = localX;
				outY = localY;
				outZ = (float)((double)box[5] + kCollisionMargin);
				best = (float)distance;
			}
		}

		if (box[2] < localZ) {
			double distance = (double)box[2] - localZ;

			if (distance < 0.0)
				distance = -distance;

			if (distance < best) {
				outX = localX;
				outY = localY;
				outZ = (float)((double)box[2] - kCollisionMargin);
				best = (float)distance;
			}
		}

		if (box[3] > localX) {
			double distance = (double)box[3] - localX;

			if (distance < 0.0)
				distance = -distance;

			if (distance < best) {
				outX = (float)((double)box[3] + kCollisionMargin);
				outY = localY;
				outZ = localZ;
				best = (float)distance;
			}
		}

		if (box[0] < localX) {
			double distance = (double)box[0] - localX;

			if (distance < 0.0)
				distance = -distance;

			if (distance < best) {
				outX = (float)((double)box[0] - kCollisionMargin);
				outY = localY;
				outZ = localZ;
			}
		}

		double worldX = ((double)outZ * transform[6] + (double)outX * transform[0]) + (double)outY * transform[3];
		float worldY = (float)(((double)outY * transform[4] + (double)outZ * transform[7]) + (double)outX * transform[1]);
		float worldZ = (float)(((double)outY * transform[5] + (double)outZ * transform[8]) + (double)outX * transform[2]);

		point[0] = (float)(worldX + transform[9]);
		point[1] = (float)((double)worldY + transform[10]);
		point[2] = (float)((double)worldZ + transform[11]);
	}
}

bool Actor::testCollision(EmiEngine *engine, float startX, float startY, float startZ, float endX, float endY, float endZ, float originX, float originY, float originZ, const float *box, const float *transform) {
	float best = 1.0f;

	double startDeltaX = ((double)startX - originX) - transform[9];
	float startRelativeY = (float)((double)startY - originY);
	float startRelativeZ = (float)((double)startZ - originZ);
	double startDeltaY = (double)startRelativeY - transform[10];
	double startDeltaZ = (double)startRelativeZ - transform[11];

	float localStartX = (float)((startDeltaZ * transform[2] + startDeltaY * transform[1]) + startDeltaX * transform[0]);
	float localStartY = (float)((startDeltaZ * transform[5] + startDeltaY * transform[4]) + startDeltaX * transform[3]);
	float localStartZ = (float)((startDeltaX * transform[6] + startDeltaZ * transform[8]) + startDeltaY * transform[7]);

	double endDeltaX = ((double)endX - originX) - transform[9];
	float endRelativeY = (float)((double)endY - originY);
	float endRelativeZ = (float)((double)endZ - originZ);
	double endDeltaY = (double)endRelativeY - transform[10];
	double endDeltaZ = (double)endRelativeZ - transform[11];

	float localEndX = (float)((endDeltaZ * transform[2] + endDeltaY * transform[1]) + endDeltaX * transform[0]);
	float localEndY = (float)((endDeltaZ * transform[5] + endDeltaY * transform[4]) + endDeltaX * transform[3]);
	float localEndZ = (float)((endDeltaX * transform[6] + endDeltaZ * transform[8]) + endDeltaY * transform[7]);

	float directionX = (float)((double)localEndX - localStartX);
	float directionY = (float)((double)localEndY - localStartY);
	float directionZ = (float)((double)localEndZ - localStartZ);

	if (localStartZ > box[5] && localEndZ < box[5]) {
		double fraction = ((double)box[5] - localStartZ) / directionZ;

		if (fraction < 1.0) {
			float fractionFloat = (float)fraction;
			float hitX = (float)((double)directionX * fractionFloat + localStartX);
			float hitDeltaY = (float)((double)directionY * fractionFloat);
			float hitDeltaZ = (float)((double)directionZ * fractionFloat);
			float hitY = (float)((double)hitDeltaY + localStartY);
			float hitZ = (float)((double)hitDeltaZ + localStartZ);

			if (hitX > box[0] && hitX < box[3]) {
				engine->_hitPoint._x = hitX;
				engine->_hitPoint._y = hitY;
				engine->_hitPoint._z = hitZ;
				best = fractionFloat;
			}
		}
	}

	if (localStartZ < box[2] && localEndZ > box[2]) {
		double fraction = ((double)box[2] - localStartZ) / directionZ;

		if (fraction < best) {
			float fractionFloat = (float)fraction;
			float hitX = (float)((double)directionX * fractionFloat + localStartX);
			float hitDeltaY = (float)((double)directionY * fractionFloat);
			float hitDeltaZ = (float)((double)directionZ * fractionFloat);
			float hitY = (float)((double)hitDeltaY + localStartY);
			float hitZ = (float)((double)hitDeltaZ + localStartZ);

			if (hitX > box[0] && hitX < box[3]) {
				engine->_hitPoint._x = hitX;
				engine->_hitPoint._y = hitY;
				engine->_hitPoint._z = hitZ;
				best = fractionFloat;
			}
		}
	}

	if (localStartX > box[3] && localEndX < box[3]) {
		double fraction = ((double)box[3] - localStartX) / directionX;

		if (fraction < best) {
			float fractionFloat = (float)fraction;
			float hitDeltaY = (float)((double)directionY * fractionFloat);
			float hitDeltaZ = (float)((double)directionZ * fractionFloat);
			float hitX = (float)((double)directionX * fractionFloat + localStartX);
			float hitY = (float)((double)hitDeltaY + localStartY);
			double hitZExact = (double)hitDeltaZ + localStartZ;
			float hitZ = (float)hitZExact;

			if (hitZExact > box[2] && hitZ < box[5]) {
				engine->_hitPoint._x = hitX;
				engine->_hitPoint._y = hitY;
				engine->_hitPoint._z = hitZ;
				best = fractionFloat;
			}
		}
	}

	if (localStartX < box[0] && localEndX > box[0]) {
		double fraction = ((double)box[0] - localStartX) / directionX;

		if (fraction < best) {
			float fractionFloat = (float)fraction;
			float hitDeltaY = (float)((double)directionY * fractionFloat);
			float hitDeltaZ = (float)((double)directionZ * fractionFloat);
			float hitX = (float)((double)directionX * fractionFloat + localStartX);
			float hitY = (float)((double)hitDeltaY + localStartY);
			double hitZExact = (double)hitDeltaZ + localStartZ;
			float hitZ = (float)hitZExact;

			if (hitZExact > box[2] && hitZ < box[5]) {
				engine->_hitPoint._x = hitX;
				engine->_hitPoint._y = hitY;
				engine->_hitPoint._z = hitZ;
				best = fractionFloat;
			}
		}
	}

	return best != 1.0f;
}

int Actor::findClosestCandidate(float startX, float startY, float startZ, float goalX, float goalY, float goalZ, const ObjectCorners_v2 *corners) {
	int found = -1;
	float best = kCollisionFloatMax;

	if (g_grim->getGamePlatform() == Common::kPlatformPS2) {
		TruncFloat bestPs2(kCollisionFloatMax);

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

			if (total < bestPs2) {
				found = i;
				bestPs2 = total;
			}
		}

		return found;
	}

	for (int i = 0; i < ObjectCorners_v2::kNumCorners; i++) {
		if (!corners->valid[i])
			continue;

		double startDeltaZ = (double)startZ - corners->corner[i][2];
		double startDeltaY = (double)startY - corners->corner[i][1];
		double startDeltaX = (double)startX - corners->corner[i][0];
		double goalDeltaZ = (double)corners->corner[i][2] - goalZ;
		double goalDeltaY = (double)corners->corner[i][1] - goalY;
		float goalDeltaX = (float)((double)corners->corner[i][0] - goalX);

		float total = (float)(((startDeltaX * startDeltaX + startDeltaZ * startDeltaZ) + startDeltaY * startDeltaY)
			+ (((double)goalDeltaX * goalDeltaX + goalDeltaZ * goalDeltaZ) + goalDeltaY * goalDeltaY));

		if (total < best) {
			found = i;
			best = total;
		}
	}

	return found;
}

int Actor::buildAvoidingPath(Actor *other, float *path, int count, float *outPath) {
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
			warning("Actor::buildAvoidingPath(): the actor has no extra data");

		for (int i = 0; i < count; i++) {
			outPath[3 * i] = path[3 * i];
			outPath[3 * i + 1] = path[3 * i + 1];
			outPath[3 * i + 2] = path[3 * i + 2];
			outCount++;
		}

		return outCount;
	}

	if (count <= 0) {
		warning("Actor::buildAvoidingPath(): the path has no points");

		return 0;
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return buildAvoidingPathPs2(other, path, count, outPath);

	double thisRadius = 0.5;

	if (!_instances.empty() && _instances[0]) {
		if (_instances[0]->_prim)
			thisRadius = calcCollisionRadius(_instances[0]->_prim);
		else
			warning("Actor::buildAvoidingPath(): primitive instance without primitive");
	}

	thisRadius = thisRadius * _extra->_collisionScale;

	double otherRadius = 0.5;

	if (!other->_instances.empty() && other->_instances[0]) {
		if (other->_instances[0]->_prim)
			otherRadius = calcCollisionRadius(other->_instances[0]->_prim);
		else
			warning("Actor::buildAvoidingPath(): primitive instance without primitive");
	}

	double radius = thisRadius + otherRadius * other->_extra->_collisionScale;
	float radiusFloat = (float)radius;
	float negativeRadius = (float)-radius;
	float box[6];

	box[0] = negativeRadius;
	box[1] = negativeRadius;
	box[2] = negativeRadius;
	box[3] = radiusFloat;
	box[4] = radiusFloat;
	box[5] = radiusFloat;

	if ((other->_nodeFlags & kPositionValid) == 0)
		other->computeLocalPositionFromWorld();

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	float *lastPoint = &path[3 * count - 3];

	getOuttaTheBox(lastPoint, box, other->_world._rot._m);

	if ((other->_nodeFlags & kPositionValid) == 0)
		other->computeLocalPositionFromWorld();

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

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
		warning("Actor::buildAvoidingPath(): the room could not be locked");

	if (locked)
		_room.unlock();

	if (!planes) {
		warning("Actor::buildAvoidingPath(): the room has no walk planes");

		return 0;
	}

	if (!planes->planeAt(goal, kCollisionAnyPlane)) {
		if (_engine->isDemo())
			warning("Actor::buildAvoidingPath(): the destination is not valid after the collision adjustment");

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

	hit[0] = _engine->_hitPoint._x;
	hit[1] = _engine->_hitPoint._y;
	hit[2] = _engine->_hitPoint._z;

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	const float *matrix = other->_world._rot._m;
	double hitWorldX = ((double)hit[1] * matrix[3] + (double)hit[2] * matrix[6]) + (double)hit[0] * matrix[0];
	float hitOffsetY = (float)(((double)hit[1] * matrix[4] + (double)hit[2] * matrix[7]) + (double)hit[0] * matrix[1]);
	float hitOffsetZ = (float)(((double)hit[1] * matrix[5] + (double)hit[2] * matrix[8]) + (double)hit[0] * matrix[2]);

	hit[0] = (float)(hitWorldX + other->_world._pos._x);
	hit[1] = (float)((double)hitOffsetY + other->_world._pos._y);
	hit[2] = (float)((double)hitOffsetZ + other->_world._pos._z);

	WalkPlane_v2 *closestPlane = planes->planeClosest(hit, kCollisionAnyPlane);

	if (closestPlane)
		closestPlane->project(hit);
	else
		warning("Actor::buildAvoidingPath(): no walk plane near the hit point");

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

		matrix = other->_world._rot._m;

		double cornerWorldX = ((double)matrix[6] * corner[2] + (double)matrix[3] * corner[1]) + (double)corner[0] * matrix[0];
		float cornerOffsetZ = (float)(((double)matrix[8] * corner[2] + (double)matrix[5] * corner[1]) + (double)matrix[2] * corner[0]);

		corner[0] = (float)(cornerWorldX + other->_world._pos._x);
		corner[1] = hit[1];
		corner[2] = (float)((double)cornerOffsetZ + other->_world._pos._z);

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
		if (_engine->isDemo())
			warning("Actor::buildAvoidingPath(): cannot get around the blocking object, stopping");

		return outCount;
	}

	if (!planes->buildPath(&path[3 * index], corners.corner[closest], true)) {
		if (_engine->isDemo())
			warning("Actor::buildAvoidingPath(): cannot get to the best corner, stopping");

		return outCount;
	}

	float (*pathPoints)[3];
	int pathCount = planes->getPath(&pathPoints);

	if (pathCount > 1) {
		for (int j = 1; j < pathCount; j++) {
			outPath[3 * outCount] = pathPoints[j][0];
			outPath[3 * outCount + 1] = pathPoints[j][1];
			outPath[3 * outCount + 2] = pathPoints[j][2];
			outCount++;
		}
	}

	corners.valid[closest] = 0;

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	float *lastOutPoint = &outPath[3 * outCount - 3];
	float *nextOutPoint = &outPath[3 * outCount];

	if (!testCollision(_engine, lastOutPoint[0], lastOutPoint[1], lastOutPoint[2], goal[0], goal[1], goal[2], 0.0f, 0.0f, 0.0f, box, other->_world._rot._m)
		&& planes->buildPath(lastOutPoint, goal, true)) {
		pathCount = planes->getPath(&pathPoints);

		if (pathCount > 1) {
			for (int j = 1; j < pathCount; j++) {
				outPath[3 * outCount] = pathPoints[j][0];
				outPath[3 * outCount + 1] = pathPoints[j][1];
				outPath[3 * outCount + 2] = pathPoints[j][2];
				outCount++;
			}

			return outCount;
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
			double previousToLastZ = (double)lastOutPoint[2] - previousPoint[2];
			double previousToLastY = (double)lastOutPoint[1] - previousPoint[1];
			double previousToLastX = (double)lastOutPoint[0] - previousPoint[0];
			double previousToGoalZ = (double)previousPoint[2] - goal[2];
			float previousToGoalY = (float)((double)previousPoint[1] - goal[1]);
			float previousToGoalX = (float)((double)previousPoint[0] - goal[0]);
			float nextToLastZ = (float)((double)lastOutPoint[2] - nextPoint[2]);
			float nextToLastY = (float)((double)lastOutPoint[1] - nextPoint[1]);
			float nextToLastX = (float)((double)lastOutPoint[0] - nextPoint[0]);
			float nextToGoalZ = (float)((double)nextPoint[2] - goal[2]);
			float nextToGoalY = (float)((double)nextPoint[1] - goal[1]);
			float nextToGoalX = (float)((double)nextPoint[0] - goal[0]);

			double previousDistance = ((previousToLastX * previousToLastX + previousToLastY * previousToLastY) + previousToLastZ * previousToLastZ)
				+ (((double)previousToGoalX * previousToGoalX + (double)previousToGoalY * previousToGoalY) + previousToGoalZ * previousToGoalZ);
			double nextDistance = (((double)nextToGoalX * nextToGoalX + (double)nextToGoalY * nextToGoalY) + (double)nextToGoalZ * nextToGoalZ)
				+ (((double)nextToLastX * nextToLastX + (double)nextToLastY * nextToLastY) + (double)nextToLastZ * nextToLastZ);

			if (nextDistance > previousDistance) {
				current = previousCorner;
				direction = -1;
			}
		}
	} else if (!previousReachable) {
		if (_engine->isDemo()) {
			warning("Actor::buildAvoidingPath(): both side points are out of bounds, stopping");
			warning("To fix this, expand the boxes or place the actor you are trying to walk around in a less congested area");
		}

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

bool Actor::resolveCollisions() {
	if (_instances.empty())
		return true;

	RPrimitiveInstance *instance = _instances[0];

	if (!instance) {
		warning("Actor::resolveCollisions(): missing primitive instance");

		return true;
	}

	RPrimitive *prim = instance->_prim;

	if (!prim)
		return true;

	if (!_extra) {
		warning("Actor::resolveCollisions(): the actor has no extra data");

		return true;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	bool ps2 = _engine->getGamePlatform() == Common::kPlatformPS2;
	const float *matrix = _world._rot._m;
	float offsetX;
	float offsetY;
	float offsetZ;
	float centerX;
	float centerY;
	float centerZ;
	float radius;

	if (ps2) {
		float centerInput[3] = { prim->_center._x, prim->_center._y, prim->_center._z };
		float rotated[3];

		SingleMath::rotate(matrix, centerInput, rotated);
		offsetX = rotated[0];
		offsetY = rotated[1];
		offsetZ = rotated[2];
		centerX = (TruncFloat(offsetX) + TruncFloat(_world._pos._x)).toFloat();
		centerY = (TruncFloat(offsetY) + TruncFloat(_world._pos._y)).toFloat();
		centerZ = (TruncFloat(offsetZ) + TruncFloat(_world._pos._z)).toFloat();
		radius = collisionRadiusPs2(prim, _extra->_collisionScale);
	} else {
		offsetX = (float)(((double)matrix[3] * prim->_center._y + (double)matrix[6] * prim->_center._z) + (double)matrix[0] * prim->_center._x);
		offsetY = (float)(((double)matrix[4] * prim->_center._y + (double)matrix[7] * prim->_center._z) + (double)matrix[1] * prim->_center._x);
		offsetZ = (float)(((double)matrix[5] * prim->_center._y + (double)matrix[8] * prim->_center._z) + (double)matrix[2] * prim->_center._x);

		if ((_nodeFlags & kWorldValid) == 0)
			updateWorldTransform();

		centerX = (float)((double)offsetX + _world._pos._x);
		centerY = (float)((double)offsetY + _world._pos._y);
		centerZ = (float)((double)offsetZ + _world._pos._z);
		radius = (float)(calcCollisionRadius(prim) * _extra->_collisionScale);
	}

	// The disasm handles the collision modes 1 and 2 of this actor in the same way
	if (ps2 && _extra->_collisionMode == 1)
		_engine->getConsoleServices()->logError("WARNING: No code for box->box or box->sphere collisions, only sphere->box, sphere->sphere! -- RTS\n");

	if (_extra->_collisionMode == 0)
		return true;

	for (int i = 0; i < _engine->_collisionCount; i++) {
		Actor *other = _engine->_collisionActors[i];

		if (!other || other == this)
			continue;

		Extra_v2 *otherExtra = other->_extra;

		if (!otherExtra)
			continue;

		if (otherExtra->_collisionMode == 0)
			continue;

		if (other->_instances.empty())
			continue;

		RPrimitiveInstance *otherInstance = other->_instances[0];

		if (!otherInstance) {
			warning("Actor::resolveCollisions(): missing primitive instance");

			continue;
		}

		RPrimitive *otherPrim = otherInstance->_prim;

		if (!otherPrim)
			continue;

		if ((other->_nodeFlags & kWorldValid) == 0)
			other->updateWorldTransform();

		const float *otherMatrix = other->_world._rot._m;
		float otherOffsetX = (float)(((double)otherMatrix[6] * otherPrim->_center._z + (double)otherMatrix[3] * otherPrim->_center._y) + (double)otherPrim->_center._x * otherMatrix[0]);
		float otherOffsetY = (float)(((double)otherMatrix[7] * otherPrim->_center._z + (double)otherMatrix[1] * otherPrim->_center._x) + (double)otherMatrix[4] * otherPrim->_center._y);
		float otherOffsetZ = (float)(((double)otherMatrix[8] * otherPrim->_center._z + (double)otherMatrix[2] * otherPrim->_center._x) + (double)otherMatrix[5] * otherPrim->_center._y);

		if ((other->_nodeFlags & kWorldValid) == 0)
			other->updateWorldTransform();

		double otherCenterX = (double)otherOffsetX + other->_world._pos._x;
		double otherCenterY = (double)otherOffsetY + other->_world._pos._y;
		double otherCenterZ = (double)otherOffsetZ + other->_world._pos._z;
		float otherRadius = (float)(calcCollisionRadius(otherPrim) * otherExtra->_collisionScale);

		float newPosition[3];

		if (ps2) {
			float otherCenterInput[3] = { otherPrim->_center._x, otherPrim->_center._y, otherPrim->_center._z };
			float otherOffset[3];

			SingleMath::rotate(otherMatrix, otherCenterInput, otherOffset);

			TruncFloat ps2OtherX = TruncFloat(otherOffset[0]) + TruncFloat(other->_world._pos._x);
			TruncFloat ps2OtherY = TruncFloat(otherOffset[1]) + TruncFloat(other->_world._pos._y);
			TruncFloat ps2OtherZ = TruncFloat(otherOffset[2]) + TruncFloat(other->_world._pos._z);
			TruncFloat ps2OtherRadius(collisionRadiusPs2(otherPrim, otherExtra->_collisionScale));
			TruncFloat ps2DeltaX = TruncFloat(centerX) - ps2OtherX;
			TruncFloat ps2DeltaY = TruncFloat(centerY) - ps2OtherY;
			TruncFloat ps2DeltaZ = TruncFloat(centerZ) - ps2OtherZ;
			TruncFloat ps2Distance = TruncFloat::squareRoot(TruncFloat::multiplyAdd(ps2DeltaX * ps2DeltaX + ps2DeltaY * ps2DeltaY, ps2DeltaZ, ps2DeltaZ));

			if (!(ps2Distance < ps2OtherRadius + TruncFloat(radius)))
				continue;

			if (otherExtra->_collisionMode != 2)
				continue;

			_extra->_inCollision = true;
			otherExtra->_inCollision = true;

			if ((_nodeFlags & kPositionValid) == 0)
				computeLocalPositionFromWorld();

			_engine->_collisionOldPosition._x = _pos._x;
			_engine->_collisionOldPosition._y = _pos._y;
			_engine->_collisionOldPosition._z = _pos._z;

			TruncFloat ps2Length = TruncFloat::squareRoot(TruncFloat::multiplyAdd(ps2DeltaX * ps2DeltaX + ps2DeltaY * ps2DeltaY, ps2DeltaZ, ps2DeltaZ));
			TruncFloat ps2DirectionX = ps2DeltaX;
			TruncFloat ps2DirectionY = ps2DeltaY;
			TruncFloat ps2DirectionZ = ps2DeltaZ;

			if (!(ps2Length < TruncFloat(kCollisionMinLength))) {
				TruncFloat ps2Inverse = TruncFloat(1.0f) / ps2Length;

				ps2DirectionX = ps2DeltaX * ps2Inverse;
				ps2DirectionY = ps2DeltaY * ps2Inverse;
				ps2DirectionZ = ps2DeltaZ * ps2Inverse;
			}

			TruncFloat ps2Radius(radius);
			TruncFloat ps2Push = ps2Radius * (TruncFloat(1.0f) - (ps2Distance - ps2OtherRadius) / ps2Radius);

			newPosition[0] = (TruncFloat(centerX) + ps2DirectionX * ps2Push - TruncFloat(offsetX)).toFloat();
			newPosition[1] = (TruncFloat(centerY) + ps2DirectionY * ps2Push - TruncFloat(offsetY)).toFloat();
			newPosition[2] = (TruncFloat(centerZ) + ps2DirectionZ * ps2Push - TruncFloat(offsetZ)).toFloat();
		} else {
		float deltaZ = (float)((double)centerZ - otherCenterZ);
		float deltaY = (float)((double)centerY - otherCenterY);
		float deltaX = (float)((double)centerX - otherCenterX);
		float distance = (float)sqrt(((double)deltaX * deltaX + (double)deltaY * deltaY) + (double)deltaZ * deltaZ);

		if (!((double)otherRadius + radius > distance))
			continue;

		if (otherExtra->_collisionMode != 2)
			continue;

		if (radius == 0.0f) {
			// The original could divide by zero here, let's avoid that...
			warning("Actor::resolveCollisions(): the collision radius is zero");

			continue;
		}

		_extra->_inCollision = true;
		otherExtra->_inCollision = true;

		if ((_nodeFlags & kPositionValid) == 0)
			computeLocalPositionFromWorld();

		_engine->_collisionOldPosition._x = _pos._x;
		_engine->_collisionOldPosition._y = _pos._y;
		_engine->_collisionOldPosition._z = _pos._z;

		double directionLength = sqrt(((double)deltaX * deltaX + (double)deltaZ * deltaZ) + (double)deltaY * deltaY);
		double directionX = deltaX;
		float directionY = deltaY;
		float directionZ = deltaZ;

		if (directionLength >= kCollisionMinLength) {
			float inverseLength = (float)(1.0 / (float)directionLength);

			directionX = (double)deltaX * inverseLength;
			directionY = (float)((double)deltaY * inverseLength);
			directionZ = (float)((double)deltaZ * inverseLength);
		}

		double push = (1.0 - ((double)distance - otherRadius) / radius) * radius;
		float pushFloat = (float)push;
		float pushY = (float)((double)directionY * pushFloat);
		float pushZ = (float)((double)directionZ * pushFloat);
		double pushedX = directionX * push + centerX;
		float pushedY = (float)((double)pushY + centerY);
		float pushedZ = (float)((double)pushZ + centerZ);

		newPosition[0] = (float)(pushedX - offsetX);
		newPosition[1] = (float)((double)pushedY - offsetY);
		newPosition[2] = (float)((double)pushedZ - offsetZ);
		}

		setActorPosition(newPosition);

		if ((_nodeFlags & kPositionValid) == 0)
			computeLocalPositionFromWorld();

		_engine->_collisionNewPosition._x = _pos._x;
		_engine->_collisionNewPosition._y = _pos._y;
		_engine->_collisionNewPosition._z = _pos._z;
		_engine->_hitPoint._x = _engine->_collisionNewPosition._x;
		_engine->_hitPoint._y = _engine->_collisionNewPosition._y;
		_engine->_hitPoint._z = _engine->_collisionNewPosition._z;

		_engine->getLua()->callCollisionHandler(this, other, _engine->_collisionOldPosition._x, _engine->_collisionOldPosition._y, _engine->_collisionOldPosition._z,
			_engine->_hitPoint._x, _engine->_hitPoint._y, _engine->_hitPoint._z, _engine->_collisionNewPosition._x, _engine->_collisionNewPosition._y, _engine->_collisionNewPosition._z);
	}

	if ((_nodeFlags & kPositionValid) == 0)
		computeLocalPositionFromWorld();

	_prevPos._x = _pos._x;
	_prevPos._y = _pos._y;
	_prevPos._z = _pos._z;

	return true;
}

void Actor::getOuttaTheBoxPs2(float *point, const float *box, const float *transform) {
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

bool Actor::testCollisionPs2(EmiEngine *engine, float startX, float startY, float startZ, float endX, float endY, float endZ, float originX, float originY, float originZ, const float *box, const float *transform) {
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

int Actor::buildAvoidingPathPs2(Actor *other, float *path, int count, float *outPath) {
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

	TruncFloat thisRadius = TruncFloat(0.5f) * TruncFloat(_extra->_collisionScale);

	if (!_instances.empty() && _instances[0]) {
		if (_instances[0]->_prim)
			thisRadius = TruncFloat(collisionRadiusPs2(_instances[0]->_prim, _extra->_collisionScale));
		else
			warning("Actor::buildAvoidingPathPs2(): primitive instance without primitive");
	}

	TruncFloat otherRadius = TruncFloat(0.5f) * TruncFloat(other->_extra->_collisionScale);

	if (!other->_instances.empty() && other->_instances[0]) {
		if (other->_instances[0]->_prim)
			otherRadius = TruncFloat(collisionRadiusPs2(other->_instances[0]->_prim, other->_extra->_collisionScale));
		else
			warning("Actor::buildAvoidingPathPs2(): primitive instance without primitive");
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

	getOuttaTheBoxPs2(lastPoint, box, other->_world._rot._m);

	if ((other->_nodeFlags & kWorldValid) == 0)
		other->updateWorldTransform();

	if ((other->_nodeFlags & kPositionValid) == 0)
		other->computeLocalPositionFromWorld();

	getOuttaTheBoxPs2(path, box, other->_world._rot._m);

	goal[0] = lastPoint[0];
	goal[1] = lastPoint[1];
	goal[2] = lastPoint[2];

	bool locked = _room.lock();
	Room_v2 *currentRoom = locked ? _room._typed : nullptr;
	WalkPlaneCollection *planes = nullptr;

	if (currentRoom)
		planes = currentRoom->_walkPlanes;
	else
		warning("Actor::buildAvoidingPathPs2(): the room could not be locked");

	if (locked)
		_room.unlock();

	if (!planes) {
		warning("Actor::buildAvoidingPathPs2(): the room has no walk planes");

		return 0;
	}

	if (!planes->planeAt(goal, kCollisionAnyPlane)) {
		_engine->getConsoleServices()->logError("Actor::CheckPathForCollisions -- Destination not valid after collision adjustment.\n");

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

		if (testCollisionPs2(_engine, startX, startY, startZ, endX, endY, endZ, 0.0f, 0.0f, 0.0f, box, other->_world._rot._m)) {
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
		warning("Actor::buildAvoidingPathPs2(): no walk plane near the hit point");

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

		if (!testCollisionPs2(_engine, startX, startY, startZ, endX, endY, endZ, 0.0f, 0.0f, 0.0f, box, other->_world._rot._m)
			&& planes->buildPath(&path[3 * index], corner, false))
			corners.valid[k] = 1;
	}

	int closest = findClosestCandidate(startX, startY, startZ, goal[0], goal[1], goal[2], &corners);

	if (closest == -1) {
		_engine->getConsoleServices()->logError("Actor::CheckPathForCollisions -- Cannot get around blocking object. Stopping.\n");

		return outCount;
	}

	if (!planes->buildPath(&path[3 * index], corners.corner[closest], true)) {
		_engine->getConsoleServices()->logError("Actor::CheckPathForCollisions -- Cannot get to best corner. Stopping.\n");

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

	if (!testCollisionPs2(_engine, lastOutPoint[0], lastOutPoint[1], lastOutPoint[2], goal[0], goal[1], goal[2], 0.0f, 0.0f, 0.0f, box, other->_world._rot._m)
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
		_engine->getConsoleServices()->logError("Actor::CheckPathForCollisions -- Left and right points are OOB! Stopping.\n");
		_engine->getConsoleServices()->logError("Actor::CheckPathForCollisions -- to fix this, expand the boxes or place the actor you are trying to walk around in a less congested area.\n");

		return outCount;
	} else {
		current = previousCorner;
		direction = -1;
	}

	for (;;) {
		if ((other->_nodeFlags & kWorldValid) == 0)
			other->updateWorldTransform();

		bool blocked = testCollisionPs2(_engine, nextOutPoint[-3], nextOutPoint[-2], nextOutPoint[-1], goal[0], goal[1], goal[2], 0.0f, 0.0f, 0.0f, box, other->_world._rot._m);

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
