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
static const int kMaxWalkPointsRemastered = 128;
static const int kRetailPathPoints = 30;

// Actors that the scripts allocate as not being the main character walk on the walk boxes only
static const int kActorFlagNotMain = 0x80;
static const int kPlaneTypeWalk = 0x1000;

static int clampPoints(int numOut, int count, int capacity) {
	if (numOut + count > capacity) {
		warning("SputActorModule::actorWalkToImpl(): the path is too long");
		return capacity - numOut;
	}

	return count;
}

int SputActorModule::pathCapacity() const {
	return _grim->isRemastered() ? (int)kExtraPathPoints : kRetailPathPoints;
}

void SputActorModule::setPathfinderMode(int mode) {
	_walkSystem->getPathFinder()->setDirect(mode != 0);
}

// The global path building of the remastered: the path finder first, the walk boxes when it is not
// allowed or fails. The points stay in the working path.
bool SputActorModule::buildPath(SputActor *actor, float *start, float *goal, int tighten, unsigned *failFlag, SputActor *avoid) {
	unsigned notMain = actor->flags & kActorFlagNotMain;
	bool skip = failFlag ? ((notMain | *failFlag) != 0) : (notMain != 0);

	bool built = false;

	if (!skip) {
		int numPoints = _walkSystem->getPathFinder()->buildPath(this, actor, goal, start, _walkSystem->getWorkingPath(), kExtraPathPoints, avoid);

		_walkSystem->setNumWorkingPathPoints(numPoints);
		built = numPoints > 0;

		if (built) {
			_walkSystem->allActive();
			return true;
		}

		if (failFlag)
			*failFlag = 1;
	}

	_walkSystem->allActive();
	_walkSystem->setNumWorkingPathPoints(0);

	if (!_walkSystem->planeAt(start, kPlaneTypeWalk) || !_walkSystem->planeAt(goal, kPlaneTypeWalk))
		return false;

	if (_walkSystem->findPathRemastered(start, goal, nullptr, 0)) {
		int numPoints = _walkSystem->getNumWorkingPathPoints();
		float (*workingPath)[3] = _walkSystem->getWorkingPath();

		workingPath[numPoints][0] = goal[0];
		workingPath[numPoints][1] = goal[1];
		workingPath[numPoints][2] = goal[2];
		_walkSystem->setNumWorkingPathPoints(numPoints + 1);
		built = true;

		if (tighten)
			_walkSystem->tightenPathRemastered();
	} else if (_walkSystem->findPathRemastered(start, goal, nullptr, 1)) {
		built = true;

		if (tighten)
			_walkSystem->tightenPathRemastered();
	}

	_walkSystem->allActive();

	return built;
}

void SputActorModule::actorWalkTo(SputActor *actor, float *pos, float *goal, SputActor *target) {
	actor->mode &= ~1;

	Puck *puck = actor->extra->puck;

	if (!puck || !_walkSystem->getHead())
		return;

	bool remastered = _grim->isRemastered();

	if (_grim->useRemasterFixes() && !(_grim->getGameFlags() & ADGF_DEMO)) {
		// The actor is moved onto the walk plane when it is just off it
		if (!_walkSystem->planeAt(actor->actorPos, kPlaneTypeWalk)) {
			WalkPlane *closest = _walkSystem->planeClosest(actor->actorPos, kPlaneTypeWalk);
			RdVector3 snapped;

			snapped[0] = actor->actorPos[0];
			snapped[1] = actor->actorPos[1];
			snapped[2] = actor->actorPos[2];

			if (closest) {
				closest->closestPoint(snapped);

				double dx = (double)snapped[0] - actor->actorPos[0];
				double dy = (double)snapped[1] - actor->actorPos[1];
				double dz = (double)snapped[2] - actor->actorPos[2];
				float squareX = (float)(dx * dx);
				float squareY = (float)(dy * dy);
				float squareZ = (float)(dz * dz);
				float distance = (float)sqrt(((double)squareX + squareY) + squareZ);

				if (distance < 0.001f) {
					actor->mode &= ~1;
					puck->setPos(snapped);

					if (actor->extra->inBoxes)
						puck->validate();

					actor->actorPos[0] = puck->_pos[0];
					actor->actorPos[1] = puck->_pos[1];
					actor->actorPos[2] = puck->_pos[2];
					actor->matrixValid = 0;
				}
			} else {
				warning("SputActorModule::actorWalkTo(): no walk plane near the actor");
			}

			_walkSystem->planeAt(actor->actorPos, kPlaneTypeWalk);
		}
	}

	WalkPlane *plane = _walkSystem->planeClosest(pos, 4096);

	if (plane)
		plane->closestPoint(pos);

	float *builtPath = nullptr;
	int numBuilt = 0;
	bool hasPath = false;
	unsigned failFlag = 0;

	if (remastered) {
		failFlag = actor->flags & kActorFlagNotMain;
		hasPath = buildPath(actor, puck->_pos, pos, 1, &failFlag, target);

		if (hasPath) {
			numBuilt = _walkSystem->getPathRemastered(&builtPath);
		}
	} else if (_walkSystem->buildPath(puck->_pos, pos, 1)) {
		numBuilt = _walkSystem->getPath(&builtPath);
		hasPath = true;
	}

	if (hasPath) {
		int capacity = pathCapacity();
		RdVector3 built[kExtraPathPoints];
		RdVector3 walkPath[kExtraPathPoints];

		if (numBuilt > capacity) {
			warning("SputActorModule::actorWalkTo(): the built path has too many points");
			numBuilt = capacity;
		}

		if (numBuilt > 0)
			memcpy(built, builtPath, numBuilt * sizeof(RdVector3));

		int numPath;

		if (remastered && failFlag == 0) {
			numPath = numBuilt;

			if (numPath > 0)
				memcpy(walkPath, built, numPath * sizeof(RdVector3));
		} else if (remastered) {
			numPath = actorWalkToImplRemastered(actor, target, built[0], numBuilt, walkPath[0]);
		} else {
			numPath = actorWalkToImpl(actor, target, built[0], numBuilt, walkPath[0]);
		}

		if (numPath > capacity) {
			warning("SputActorModule::actorWalkTo(): the walk path has too many points");
			numPath = capacity;
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
	int walkCapacity = kMaxWalkPoints;

	if (!target) {
		if (numPoints > 0) {
			numPoints = clampPoints(0, numPoints, walkCapacity);
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
		numBuilt = clampPoints(numOut, numBuilt, walkCapacity);
		memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
		numOut += numBuilt;
	}

	corners.valid[closest] = 0;

	float *lastOut = &outPath[3 * (numOut - 1)];

	if (checkCollisions(lastOut, lastPoint, targetCenterWorld, targetBox, getOrientMatrix(target), targetCenterOffset)) {
		numBuilt = _walkSystem->getPath(&built);

		if (numBuilt > 0) {
			numBuilt = clampPoints(numOut, numBuilt, walkCapacity);
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
			numBuilt = clampPoints(numOut, numBuilt, walkCapacity);
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
		numBuilt = clampPoints(numOut, numBuilt, walkCapacity);
		memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
		numOut += numBuilt;
	}

	return numOut;
}

int SputActorModule::actorWalkToImplRemastered(SputActor *actor, SputActor *target, float *path, int numPoints, float *outPath) {
	int numOut = 0;
	unsigned int failFlag = 1;

	if (!target) {
		if (numPoints > 0) {
			numPoints = clampPoints(0, numPoints, kMaxWalkPointsRemastered);
			memcpy(outPath, path, numPoints * sizeof(RdVector3));
			numOut = numPoints;
		}

		return numOut;
	}

	if (numPoints <= 0 || !actor->costumeStack || !target->costumeStack) {
		warning("SputActorModule::actorWalkToImplRemastered(): no path or no costume to walk around");
		return 0;
	}

	RdModel3 *model = actor->costumeStack->headThing->data.model3;
	float myRadius = (float)((double)actor->extra->collisionScale * model->radius);
	RdVector3 myCenterOffset;
	RdVector3 targetCenterWorld;
	RdVector3 targetCenterOffset;
	RdModel3 *targetModel = target->costumeStack->headThing->data.model3;

	transformVector34Remastered(myCenterOffset, model->geomCenter, getOrientMatrix(actor));
	transformPoint34Remastered(targetCenterWorld, targetModel->geomCenter, getOrientMatrix(target));
	transformVector34Remastered(targetCenterOffset, targetModel->geomCenter, getOrientMatrix(target));

	float targetScale = target->extra->collisionScale;
	RdBound3 targetBox;

	targetBox.min[0] = (float)((double)targetModel->bound.min[0] * targetScale - myRadius);
	targetBox.min[1] = (float)((double)targetModel->bound.min[1] * targetScale - myRadius);
	targetBox.min[2] = (float)((double)targetModel->bound.min[2] * targetScale - myRadius);
	targetBox.max[0] = (float)((double)targetModel->bound.max[0] * targetScale + myRadius);
	targetBox.max[1] = (float)((double)targetModel->bound.max[1] * targetScale + myRadius);
	targetBox.max[2] = (float)((double)targetScale * targetModel->bound.max[2] + myRadius);

	float *lastInput = &path[3 * numPoints - 3];

	getOuttaTheBoxRemastered(lastInput, targetBox, getOrientMatrix(target), target->actorPos);
	getOuttaTheBoxRemastered(path, targetBox, getOrientMatrix(target), target->actorPos);

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
		segFrom[0] = (float)((double)current[0] + targetCenterOffset[0]);
		segFrom[1] = (float)((double)current[1] + targetCenterOffset[1]);
		segFrom[2] = (float)((double)current[2] + targetCenterOffset[2]);
		segTo[0] = (float)((double)next[0] + targetCenterOffset[0]);
		segTo[1] = (float)((double)next[1] + targetCenterOffset[1]);
		segTo[2] = (float)((double)next[2] + targetCenterOffset[2]);
		outPath[3 * (numOut - 1)] = current[0];
		outPath[3 * (numOut - 1) + 1] = current[1];
		outPath[3 * (numOut - 1) + 2] = current[2];

		if (testCollisionRemastered(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target))) {
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

	transformPoint34Remastered(snapped, _collisionImpact, getOrientMatrix(target));

	WalkPlane *plane = _walkSystem->planeClosest(snapped, -1);

	if (!plane) {
		warning("SputActorModule::actorWalkToImplRemastered(): no walk plane near the collision point");
		return numOut;
	}

	plane->snapTo(snapped);

	ObjectCorners corners;

	memset(&corners, 0, sizeof(corners));

	float margin = (float)((double)myRadius + 0.01999999955296516);

	findObjectCornersRemastered(targetModel, margin, getOrientMatrix(target), snapped[2], &corners);

	if (!corners.valid[0])
		warning("SputActorModule::actorWalkToImplRemastered(): the actor to walk around is not upright");

	for (int i = 0; i < 4; i++) {
		corners.valid[i] = 0;
		segTo[0] = (float)((double)corners.corner[i][0] + targetCenterOffset[0]);
		segTo[1] = (float)((double)corners.corner[i][1] + targetCenterOffset[1]);
		segTo[2] = (float)((double)corners.corner[i][2] + targetCenterOffset[2]);

		if (!testCollisionRemastered(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target)) && buildPath(actor, &path[3 * idx], corners.corner[i], 0, &failFlag, nullptr))
			corners.valid[i] = 1;
	}

	int closest = findClosestCandidateRemastered(segFrom, lastPoint, &corners);

	if (closest == -1)
		return numOut;

	float *built;

	buildPath(actor, &path[3 * idx], corners.corner[closest], 0, &failFlag, nullptr);

	int numBuilt = _walkSystem->getPathRemastered(&built);

	if (numBuilt > 0) {
		numBuilt = clampPoints(numOut, numBuilt, kMaxWalkPointsRemastered);
		memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
		numOut += numBuilt;
	}

	corners.valid[closest] = 0;

	float *lastOut = &outPath[3 * (numOut - 1)];

	if (checkCollisionsRemastered(actor, lastOut, lastPoint, targetCenterWorld, targetBox, getOrientMatrix(target), targetCenterOffset)) {
		numBuilt = _walkSystem->getPathRemastered(&built);

		if (numBuilt > 0) {
			numBuilt = clampPoints(numOut, numBuilt, kMaxWalkPointsRemastered);
			memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
			numOut += numBuilt;
		}

		return numOut;
	}

	int ccwCorner = (closest + 3) % 4;
	int cwCorner = (closest + 5) % 4;
	int ccwValid = buildPath(actor, lastOut, corners.corner[ccwCorner], 0, &failFlag, nullptr);
	int cwValid = buildPath(actor, lastOut, corners.corner[cwCorner], 0, &failFlag, nullptr);

	if (cwValid && ccwValid) {
		float *ccw = corners.corner[ccwCorner];
		float *cw = corners.corner[cwCorner];

		double ccwFromY = (double)lastOut[1] - ccw[1];
		double ccwFromX = (double)lastOut[0] - ccw[0];
		double ccwFromZ = (double)lastOut[2] - ccw[2];
		double ccwToY = (double)ccw[1] - lastPoint[1];
		double ccwToX = (double)ccw[0] - lastPoint[0];
		double ccwToZ = (double)ccw[2] - lastPoint[2];
		float ccwFromSquareX = (float)(ccwFromX * ccwFromX);
		float ccwFromSquareY = (float)(ccwFromY * ccwFromY);
		float ccwFromSquareZ = (float)(ccwFromZ * ccwFromZ);
		float ccwToSquareX = (float)(ccwToX * ccwToX);
		float ccwToSquareY = (float)(ccwToY * ccwToY);
		float ccwToSquareZ = (float)(ccwToZ * ccwToZ);
		float ccwCost = (float)((((double)ccwFromSquareX + ccwFromSquareY) + ccwFromSquareZ) + (((double)ccwToSquareX + ccwToSquareY) + ccwToSquareZ));

		double cwFromY = (double)lastOut[1] - cw[1];
		double cwFromX = (double)lastOut[0] - cw[0];
		double cwFromZ = (double)lastOut[2] - cw[2];
		double cwToY = (double)cw[1] - lastPoint[1];
		double cwToX = (double)cw[0] - lastPoint[0];
		double cwToZ = (double)cw[2] - lastPoint[2];
		float cwFromSquareX = (float)(cwFromX * cwFromX);
		float cwFromSquareY = (float)(cwFromY * cwFromY);
		float cwFromSquareZ = (float)(cwFromZ * cwFromZ);
		float cwToSquareX = (float)(cwToX * cwToX);
		float cwToSquareY = (float)(cwToY * cwToY);
		float cwToSquareZ = (float)(cwToZ * cwToZ);
		float cwCost = (float)((((double)cwToSquareX + cwToSquareY) + cwToSquareZ) + (((double)cwFromSquareX + cwFromSquareY) + cwFromSquareZ));

		if (cwCost > ccwCost)
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
		return numOut;
	}

	while (cornerIdx != closest) {
		buildPath(actor, lastOut, corners.corner[cornerIdx], 0, &failFlag, nullptr);
		numBuilt = _walkSystem->getPathRemastered(&built);

		for (int i = 0; i < numBuilt - 1; i++) {
			segFrom[0] = (float)((double)built[3 * i] + targetCenterOffset[0]);
			segFrom[1] = (float)((double)built[3 * i + 1] + targetCenterOffset[1]);
			segFrom[2] = (float)((double)built[3 * i + 2] + targetCenterOffset[2]);
			segTo[0] = (float)((double)built[3 * i + 3] + targetCenterOffset[0]);
			segTo[1] = (float)((double)built[3 * i + 4] + targetCenterOffset[1]);
			segTo[2] = (float)((double)built[3 * i + 5] + targetCenterOffset[2]);

			if (testCollisionRemastered(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target)))
				return 0;
		}

		if (numBuilt > 0) {
			numBuilt = clampPoints(numOut, numBuilt, kMaxWalkPointsRemastered);
			memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
			numOut += numBuilt;
		}

		lastOut = &outPath[3 * (numOut - 1)];

		if (checkCollisionsRemastered(actor, lastOut, lastPoint, targetCenterWorld, targetBox, getOrientMatrix(target), targetCenterOffset)) {
			numBuilt = _walkSystem->getPathRemastered(&built);

			for (int i = 0; i < numBuilt - 1; i++) {
				segFrom[0] = (float)((double)built[3 * i] + targetCenterOffset[0]);
				segFrom[1] = (float)((double)built[3 * i + 1] + targetCenterOffset[1]);
				segFrom[2] = (float)((double)built[3 * i + 2] + targetCenterOffset[2]);
				segTo[0] = (float)((double)built[3 * i + 3] + targetCenterOffset[0]);
				segTo[1] = (float)((double)built[3 * i + 4] + targetCenterOffset[1]);
				segTo[2] = (float)((double)built[3 * i + 5] + targetCenterOffset[2]);

				if (testCollisionRemastered(segFrom, segTo, targetCenterWorld, targetBox, getOrientMatrix(target)))
					return 0;
			}

			if (numBuilt > 0) {
				numBuilt = clampPoints(numOut, numBuilt, kMaxWalkPointsRemastered);
				memcpy(&outPath[3 * numOut], built, numBuilt * sizeof(RdVector3));
				numOut += numBuilt;
			}

			return numOut;
		}

		cornerIdx = (step + cornerIdx + 4) % 4;
	}

	return numOut;
}

// The corners of the boxes of the actors that are near the actor, or are the one to walk around.
// Each box has four points. Actors that lie down have no box.
void SputActorModule::getCollisionBoxes(SputActor *actor, int *count, int maxPoints, float (*points)[3], SputActor *avoid) {
	*count = 0;

	if (!actor->costumeStack || !actor->costumeStack->headThing || !actor->extra) {
		warning("SputActorModule::getCollisionBoxes(): the actor has no model");
		return;
	}

	RdModel3 *model = actor->costumeStack->headThing->data.model3;

	if (!model) {
		warning("SputActorModule::getCollisionBoxes(): the actor has no model");
		return;
	}

	RdVector3 offset;

	transformVector34Remastered(offset, model->geomCenter, getOrientMatrix(actor));

	float centerX = (float)((double)actor->actorPos[0] + offset[0]);
	float centerY = (float)((double)actor->actorPos[1] + offset[1]);
	float centerZ = (float)((double)actor->actorPos[2] + offset[2]);
	float radius = model->radius * actor->extra->collisionScale;

	if (_dynamicCollisionFrames > 9 || _numCollisionActors <= 0)
		return;

	float margin = (float)((double)radius + 0.01999999955296516);

	for (int i = 0; i < _numCollisionActors; i++) {
		SputActor *other = _collisionActors[i];

		if (!other || !other->extra || other == actor)
			continue;

		int mode = other->extra->collisionMode;

		if (mode == 0 || !other->costumeStack || mode == 2)
			continue;

		if (!other->costumeStack->headThing || !other->costumeStack->headThing->data.model3) {
			warning("SputActorModule::getCollisionBoxes(): an actor has no model");
			continue;
		}

		RdModel3 *otherModel = other->costumeStack->headThing->data.model3;
		RdVector3 otherCenter;

		transformPoint34Remastered(otherCenter, otherModel->geomCenter, getOrientMatrix(other));

		float otherRadius = otherModel->radius * other->extra->collisionScale;
		double dx = (double)centerX - otherCenter[0];
		double dy = (double)centerY - otherCenter[1];
		double dz = (double)centerZ - otherCenter[2];
		float squareX = (float)(dx * dx);
		float squareY = (float)(dy * dy);
		float squareZ = (float)(dz * dz);
		float distance = (float)sqrt(((double)squareX + squareY) + squareZ);

		if (!(((double)otherRadius + radius) > distance) && other != avoid)
			continue;

		ObjectCorners corners;

		memset(&corners, 0, sizeof(corners));
		findObjectCornersRemastered(otherModel, margin, getOrientMatrix(other), other->actorPos[2], &corners);

		if (!corners.valid[0]) {
			warning("SputActorModule::getCollisionBoxes(): an actor that is not upright has no box");
			continue;
		}

		for (int j = 0; j < 4; j++) {
			points[*count][0] = corners.corner[j][0];
			points[*count][1] = corners.corner[j][1];
			points[*count][2] = corners.corner[j][2];
			(*count)++;
		}

		if (*count >= maxPoints) {
			warning("SputActorModule::getCollisionBoxes(): max number of bounding boxes exceeded");
			break;
		}
	}
}

} // End of namespace Grim
