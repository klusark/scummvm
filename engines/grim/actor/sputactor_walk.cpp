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
#include "engines/grim/puck.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/remastered/pathfinder.h"
#include "engines/grim/walkplane.h"

namespace Grim {

static const int kMaxWalkPoints = 64;

// Actors that the scripts allocate as not being the main character walk on the walk boxes only
static const int kActorFlagNotMain = 0x80;
static const int kPlaneTypeWalk = 0x1000;
static const float kMouseRayLength = 100.0f;

SputActorModule::~SputActorModule() {
	delete _pathFinder;
}

static int clampPoints(int numOut, int count) {
	if (numOut + count > kMaxWalkPoints) {
		warning("SputActorModule::actorWalkToImpl(): the path is too long");
		return kMaxWalkPoints - numOut;
	}

	return count;
}

void SputActorModule::actorWalkTo(SputActor *actor, float *pos, float *goal, SputActor *target) {
	actor->mode &= ~1;

	Puck *puck = actor->extra->puck;

	if (!puck || !_walkSystem->getHead())
		return;

	WalkPlane *plane = _walkSystem->planeClosest(pos, 4096);

	if (plane)
		plane->closestPoint(pos);

	float *builtPath = nullptr;
	int numBuilt = 0;
	bool hasPath = false;

	// The remastered tries the path finder before the walk boxes. It does not know about the
	// actor to walk around yet, so those walks keep the walk boxes.
	if (_grim->isRemastered() && !target && !(actor->flags & kActorFlagNotMain)) {
		numBuilt = buildPathFinderPath(puck->_pos, pos, &builtPath);
		hasPath = numBuilt > 0;
	}

	if (!hasPath && _walkSystem->buildPath(puck->_pos, pos, 1)) {
		numBuilt = _walkSystem->getPath(&builtPath);
		hasPath = true;
	}

	if (hasPath) {
		RdVector3 built[30];
		RdVector3 walkPath[kMaxWalkPoints];

		if (numBuilt > 30) {
			warning("SputActorModule::actorWalkTo(): the built path has too many points");
			numBuilt = 30;
		}

		if (numBuilt > 0)
			memcpy(built, builtPath, numBuilt * sizeof(RdVector3));

		int numPath = actorWalkToImpl(actor, target, built[0], numBuilt, walkPath[0]);

		if (numPath > 30) {
			warning("SputActorModule::actorWalkTo(): the walk path has too many points");
			numPath = 30;
		}

		actor->extra->numPathPoints = numPath;
		actor->extra->pathIndex = 1;

		if (numPath > 0)
			memcpy(actor->extra->path, walkPath, numPath * sizeof(RdVector3));

		float *firstTarget = actor->extra->path[actor->extra->pathIndex];

		actor->extra->walkTarget[0] = firstTarget[0];
		actor->extra->walkTarget[1] = firstTarget[1];
		actor->extra->walkTarget[2] = firstTarget[2];

		if (numPath > 1)
			actor->mode |= 1;
	}

	actor->extra->goal[0] = goal[0];
	actor->extra->goal[1] = goal[1];
	actor->extra->goal[2] = goal[2];
}

int SputActorModule::actorWalkToImpl(SputActor *actor, SputActor *target, float *path, int numPoints, float *outPath) {
	int numOut = 0;

	if (!target) {
		if (numPoints > 0) {
			numPoints = clampPoints(0, numPoints);
			memcpy(outPath, path, numPoints * sizeof(RdVector3));
			numOut = numPoints;
		}

		return numOut;
	}

	if (numPoints <= 0 || !actor->costumeStack || !target->costumeStack) {
		warning("SputActorModule::actorWalkToImpl(): no path or no costume to walk around");
		return 0;
	}

	RdModel3 *model = actor->costumeStack->headThing->data.model3;
	float myRadius = (float)((double)actor->extra->collisionScale * model->radius);
	RdModel3 *targetModel = target->costumeStack->headThing->data.model3;
	RdVector3 targetCenterWorld;
	RdVector3 targetCenterOffset;

	_rd->transformPoint34(targetCenterWorld, targetModel->geomCenter, getOrientMatrix(target));
	_rd->transformVector34(targetCenterOffset, targetModel->geomCenter, getOrientMatrix(target));

	float targetScale = target->extra->collisionScale;
	RdBound3 targetBox;

	targetBox.min[0] = (float)((double)targetModel->bound.min[0] * targetScale - myRadius);
	targetBox.min[1] = (float)((double)targetModel->bound.min[1] * targetScale - myRadius);
	targetBox.min[2] = (float)((double)targetModel->bound.min[2] * targetScale - myRadius);
	targetBox.max[0] = (float)((double)targetModel->bound.max[0] * targetScale + myRadius);
	targetBox.max[1] = (float)((double)targetModel->bound.max[1] * targetScale + myRadius);
	targetBox.max[2] = (float)((double)targetModel->bound.max[2] * targetScale + myRadius);

	float *lastInput = &path[3 * numPoints - 3];

	getOuttaTheBox(lastInput, targetBox, getOrientMatrix(target), target->actorPos);
	getOuttaTheBox(path, targetBox, getOrientMatrix(target), target->actorPos);

	RdVector3 lastPoint;

	lastPoint[0] = lastInput[0];
	lastPoint[1] = lastInput[1];
	lastPoint[2] = lastInput[2];

	if (!_walkSystem->planeAt(lastPoint, -1))
		return 0;

	int numSegments = numPoints - 1;
	int idx = 0;
	RdVector3 segFrom;
	RdVector3 segTo;
	bool collided = false;

	while (idx < numSegments) {
		float *current = &path[3 * idx];
		float *next = &path[3 * idx + 3];

		numOut++;
		segFrom[0] = (float)((double)targetCenterOffset[0] + current[0]);
		segFrom[1] = (float)((double)current[1] + targetCenterOffset[1]);
		segFrom[2] = (float)((double)targetCenterOffset[2] + current[2]);
		segTo[0] = (float)((double)next[0] + targetCenterOffset[0]);
		segTo[1] = (float)((double)next[1] + targetCenterOffset[1]);
		segTo[2] = (float)((double)next[2] + targetCenterOffset[2]);
		outPath[3 * (numOut - 1)] = current[0];
		outPath[3 * (numOut - 1) + 1] = current[1];
		outPath[3 * (numOut - 1) + 2] = current[2];

		if (testCollision(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target))) {
			collided = true;
			break;
		}

		idx++;
	}

	if (!collided) {
		memcpy(&outPath[3 * numOut], &path[3 * idx], sizeof(RdVector3));

		return numOut + 1;
	}

	RdVector3 snapped;

	_rd->transformPoint34(snapped, _collisionImpact, getOrientMatrix(target));

	WalkPlane *plane = _walkSystem->planeClosest(snapped, -1);

	if (!plane) {
		warning("SputActorModule::actorWalkToImpl(): no walk plane near the collision point");
		return numOut;
	}

	plane->snapTo(snapped);

	ObjectCorners corners;

	memset(&corners, 0, sizeof(corners));

	float margin = (float)((double)myRadius - -0.019999999552965164);

	findObjectCorners(targetModel, margin, getOrientMatrix(target), snapped[2], &corners);

	for (int i = 0; i < 4; i++) {
		corners.valid[i] = 0;
		segTo[0] = (float)((double)targetCenterOffset[0] + corners.corner[i][0]);
		segTo[1] = (float)((double)corners.corner[i][1] + targetCenterOffset[1]);
		segTo[2] = (float)((double)targetCenterOffset[2] + corners.corner[i][2]);

		if (!testCollision(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target)) && _walkSystem->buildPath(&path[3 * idx], corners.corner[i], 0))
			corners.valid[i] = 1;
	}

	int closest = findClosestCandidate(segFrom, lastPoint, &corners);

	if (closest == -1)
		return numOut;

	float *built;

	_walkSystem->buildPath(&path[3 * idx], corners.corner[closest], 0);

	int numBuilt = _walkSystem->getPath(&built);

	if (numBuilt > 0) {
		numBuilt = clampPoints(numOut, numBuilt);
		memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
		numOut += numBuilt;
	}

	corners.valid[closest] = 0;

	float *lastOut = &outPath[3 * (numOut - 1)];

	if (checkCollisions(lastOut, lastPoint, targetCenterWorld, targetBox, getOrientMatrix(target), targetCenterOffset)) {
		numBuilt = _walkSystem->getPath(&built);

		if (numBuilt > 0) {
			numBuilt = clampPoints(numOut, numBuilt);
			memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
			numOut += numBuilt;
		}

		return numOut;
	}

	int ccwCorner = (closest + 3) % 4;
	int cwCorner = (closest + 5) % 4;
	int ccwValid = _walkSystem->buildPath(lastOut, corners.corner[ccwCorner], 0);
	int cwValid = _walkSystem->buildPath(lastOut, corners.corner[cwCorner], 0);

	if (cwValid && ccwValid) {
		float *ccw = corners.corner[ccwCorner];
		float *cw = corners.corner[cwCorner];

		double ccwDx = (double)lastOut[0] - ccw[0];
		float ccwDxF = (float)ccwDx;
		float ccwDy = (float)((double)lastOut[1] - ccw[1]);
		float ccwDz = (float)((double)lastOut[2] - ccw[2]);
		double ccwEndX = (double)ccw[0] - lastPoint[0];
		float ccwEndXF = (float)ccwEndX;
		float ccwEndY = (float)((double)ccw[1] - lastPoint[1]);
		float ccwEndZ = (float)((double)ccw[2] - lastPoint[2]);
		double cwDx = (double)lastOut[0] - cw[0];
		float cwDxF = (float)cwDx;
		float cwDy = (float)((double)lastOut[1] - cw[1]);
		float cwDz = (float)((double)lastOut[2] - cw[2]);
		double cwEndX = (double)cw[0] - lastPoint[0];
		float cwEndXF = (float)cwEndX;
		float cwEndY = (float)((double)cw[1] - lastPoint[1]);
		double cwEndZ = (double)cw[2] - lastPoint[2];

		double ccwCost = ccwDx * ccwDxF + (double)ccwDy * ccwDy + (double)ccwDz * ccwDz + ccwEndX * ccwEndXF + (double)ccwEndY * ccwEndY + (double)ccwEndZ * ccwEndZ;
		double cwCost = cwDx * cwDxF + (double)cwDy * cwDy + (double)cwDz * cwDz + cwEndX * cwEndXF + (double)cwEndY * cwEndY + cwEndZ * cwEndZ;

		if (cwCost <= ccwCost)
			ccwValid = 0;
		else
			cwValid = 0;
	}

	int cornerIdx;
	int step;

	if (cwValid) {
		step = 1;
		cornerIdx = cwCorner;
	} else if (ccwValid) {
		step = -1;
		cornerIdx = ccwCorner;
	} else {
		warning("SputActorModule::actorWalkToImpl(): no corner to walk around the target");
		return numOut;
	}

	if (cornerIdx == closest)
		return numOut;

	for (;;) {
		_walkSystem->buildPath(lastOut, corners.corner[cornerIdx], 0);
		numBuilt = _walkSystem->getPath(&built);

		for (int i = 0; i < numBuilt - 1; i++) {
			segFrom[0] = (float)((double)targetCenterOffset[0] + built[3 * i]);
			segFrom[1] = (float)((double)built[3 * i + 1] + targetCenterOffset[1]);
			segFrom[2] = (float)((double)built[3 * i + 2] + targetCenterOffset[2]);
			segTo[0] = (float)((double)built[3 * i + 3] + targetCenterOffset[0]);
			segTo[1] = (float)((double)built[3 * i + 4] + targetCenterOffset[1]);
			segTo[2] = (float)((double)built[3 * i + 5] + targetCenterOffset[2]);

			if (testCollision(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target)))
				return 0;
		}

		if (numBuilt > 0) {
			numBuilt = clampPoints(numOut, numBuilt);
			memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
			numOut += numBuilt;
		}

		lastOut = &outPath[3 * (numOut - 1)];

		if (checkCollisions(lastOut, lastPoint, targetCenterWorld, targetBox, getOrientMatrix(target), targetCenterOffset))
			break;

		cornerIdx = (step + cornerIdx + 4) % 4;

		if (cornerIdx == closest)
			return numOut;
	}

	numBuilt = _walkSystem->getPath(&built);

	for (int i = 0; i < numBuilt - 1; i++) {
		segFrom[0] = (float)((double)targetCenterOffset[0] + built[3 * i]);
		segFrom[1] = (float)((double)built[3 * i + 1] + targetCenterOffset[1]);
		segFrom[2] = (float)((double)built[3 * i + 2] + targetCenterOffset[2]);
		segTo[0] = (float)((double)built[3 * i + 3] + targetCenterOffset[0]);
		segTo[1] = (float)((double)built[3 * i + 4] + targetCenterOffset[1]);
		segTo[2] = (float)((double)built[3 * i + 5] + targetCenterOffset[2]);

		if (testCollision(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target)))
			return 0;
	}

	if (numBuilt > 0) {
		numBuilt = clampPoints(numOut, numBuilt);
		memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
		numOut += numBuilt;
	}

	return numOut;
}

int SputActorModule::buildPathFinderPath(float *from, float *to, float **path) {
	if (!_pathFinder)
		_pathFinder = new PathFinder(_walkSystem);

	int numPoints = _pathFinder->buildPath(from, to, _pathFinderPoints, kPathFinderMaxPoints);

	if (numPoints <= 0)
		return 0;

	*path = &_pathFinderPoints[0][0];

	return numPoints;
}

// The pointer axes give a ray from the camera. The actor walks to the nearest walk plane that the ray
// crosses, or to the closest point of that plane when the ray misses its polygon.
void SputActorModule::walkActorToMouseTarget(SputActor *actor, float axisX, float axisY) {
	RdCamera *camera = _rd->_camera.getCurrent();

	if (!camera || !camera->canvas || camera->focalLen == 0.0f)
		return;

	RdVector3 direction;
	RdVector3 world;
	RdVector3 start;
	RdVector3 end;
	RdMatrix34 cameraMatrix;
	float screenX = axisX * camera->canvas->xCenter + camera->canvas->xCenter;
	float screenY = axisY * camera->canvas->yCenter + camera->canvas->yCenter;

	direction[0] = (screenX - camera->canvas->xCenter) / camera->focalLen;
	direction[1] = 1.0f;
	direction[2] = -(screenY - camera->canvas->yCenter) / camera->focalLen;
	_rd->normalize3Acc(direction);

	_rd->invertOrtho34(cameraMatrix, camera->viewMatrix);
	_rd->transformVector34(world, direction, cameraMatrix);
	_rd->_camera.getPosition(start);

	for (int i = 0; i < 3; i++)
		end[i] = start[i] + world[i] * kMouseRayLength;

	RdVector3 point;
	RdVector3 nearHit;
	WalkPlane *nearPlane = nullptr;
	double nearT = 2.0;
	double hitT = 2.0;
	bool found = false;

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if ((plane->_type & kPlaneTypeWalk) == 0 || !plane->_active || plane->_numVerts <= 0)
			continue;

		const WpVertex *vertex = plane->_verts[0];
		double vertexPos[3];
		double denominator = 0.0;
		double numerator = 0.0;

		vertexPos[0] = vertex->x;
		vertexPos[1] = vertex->y;
		vertexPos[2] = vertex->z;

		for (int i = 0; i < 3; i++) {
			denominator += (double)plane->_normal[i] * ((double)end[i] - start[i]);
			numerator += (double)plane->_normal[i] * (vertexPos[i] - start[i]);
		}

		if (fabs(denominator) < 0.000001)
			continue;

		double t = numerator / denominator;

		if (t < 0.0 || t > 1.0)
			continue;

		RdVector3 hit;

		for (int i = 0; i < 3; i++)
			hit[i] = (float)(start[i] + t * ((double)end[i] - start[i]));

		if (t < nearT) {
			nearT = t;
			nearPlane = plane;
			nearHit[0] = hit[0];
			nearHit[1] = hit[1];
			nearHit[2] = hit[2];
		}

		if (t < hitT && plane->within(hit, 0)) {
			hitT = t;
			point[0] = hit[0];
			point[1] = hit[1];
			point[2] = hit[2];
			found = true;
		}
	}

	if (!found) {
		if (!nearPlane)
			return;

		point[0] = nearHit[0];
		point[1] = nearHit[1];
		point[2] = nearHit[2];
		nearPlane->closestPoint(point);
	}

	RdVector3 goal = { 0.0f, 0.0f, 0.0f };

	actorWalkTo(actor, point, goal, nullptr);
}

} // End of namespace Grim
