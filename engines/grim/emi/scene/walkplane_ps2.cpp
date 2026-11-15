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

#include "engines/grim/emi/actor/puck_ps2.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/math/singlemath.h"
#include "engines/grim/emi/scene/walkplane_ps2.h"

namespace Grim {

static const float kEdgeEpsilonPs2 = 9.99999975e-06f;
static const float kMinLengthPs2 = 9.99999968e-21f;
static const float kRatioLimitPs2 = 1.00999999f;
static const float kProbeDistancePs2 = 0.100000001f;
static const float kJumpOutDistancePs2 = -10.0f;

WalkPlane_v2Ps2::WalkPlane_v2Ps2(EmiEngine *engine, const float *vertices, int vertexCount, WalkPlaneCollection *owner) : WalkPlane_v2(engine, vertices, vertexCount, owner) {
	if (_numVerts >= 2)
		computeEquation();
}

bool WalkPlane_v2Ps2::closestSharedVertex(WalkPlane_v2 *planeB, float x, float y, float z, float *out) {
	TruncFloat best(3.40282347e+38f);
	TruncFloat pointY(y);
	TruncFloat pointZ(z);
	WalkPlane_v2 *planes[2] = { this, planeB };
	WalkPlane_v2 *others[2] = { planeB, this };

	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < planes[pass]->_numVerts; i++) {
			wpVertex *vertex = planes[pass]->_verts[i];

			if (!others[pass]->within(&vertex->x, false))
				continue;

			TruncFloat deltaX = TruncFloat(vertex->x) - TruncFloat(x);
			TruncFloat deltaY = TruncFloat(vertex->y) - pointY;
			TruncFloat deltaZ = TruncFloat(vertex->z) - pointZ;
			TruncFloat distance = TruncFloat::multiplyAdd(deltaX * deltaX + deltaY * deltaY, deltaZ, deltaZ);

			if (distance < best) {
				best = distance;
				out[0] = vertex->x;
				out[1] = vertex->y;
				out[2] = vertex->z;
			}
		}
	}

	return best != TruncFloat(3.40282347e+38f);
}

void WalkPlane_v2Ps2::computeEquation() {
	const wpVertex *first = _verts[0];
	const wpVertex *second = _verts[1];
	const wpVertex *last = _verts[_numVerts - 1];
	TruncFloat lastX = TruncFloat(last->x) - TruncFloat(first->x);
	TruncFloat lastY = TruncFloat(last->y) - TruncFloat(first->y);
	TruncFloat lastZ = TruncFloat(last->z) - TruncFloat(first->z);
	TruncFloat secondX = TruncFloat(second->x) - TruncFloat(first->x);
	TruncFloat secondY = TruncFloat(second->y) - TruncFloat(first->y);
	TruncFloat secondZ = TruncFloat(second->z) - TruncFloat(first->z);
	TruncFloat normalZ = TruncFloat::multiplySubtract(lastX * secondY, lastY, secondX);
	TruncFloat normalY = TruncFloat::multiplySubtract(lastZ * secondX, lastX, secondZ);
	TruncFloat normalX = TruncFloat::multiplySubtract(lastY * secondZ, lastZ, secondY);
	TruncFloat length = TruncFloat::squareRoot(TruncFloat::multiplyAdd(normalX * normalX + normalY * normalY, normalZ, normalZ));

	if (!(length < TruncFloat(kMinLengthPs2))) {
		TruncFloat inverse = TruncFloat(1.0f) / length;

		normalX = normalX * inverse;
		normalY = normalY * inverse;
		normalZ = normalZ * inverse;
	}

	_normal[0] = normalX.toFloat();
	_normal[1] = normalY.toFloat();
	_normal[2] = normalZ.toFloat();
	_d = (-TruncFloat::multiplyAdd(normalX * TruncFloat(first->x) + normalY * TruncFloat(first->y), normalZ, TruncFloat(first->z))).toFloat();
}

void WalkPlane_v2Ps2::project(float *point) {
	TruncFloat product = TruncFloat(point[0]) * TruncFloat(_normal[0]);
	TruncFloat accumulator = -TruncFloat(_d) - product;

	accumulator = TruncFloat::multiplySubtract(accumulator, TruncFloat(point[2]), TruncFloat(_normal[2]));
	point[1] = (accumulator / TruncFloat(_normal[1])).toFloat();
}

bool WalkPlane_v2Ps2::within(const float *point, bool checkActive) {
	if (checkActive && (!_enabled || !_active))
		return false;

	TruncFloat zero(0.0f);
	TruncFloat epsilon(kEdgeEpsilonPs2);
	TruncFloat pointX(point[0]);
	TruncFloat pointZ(point[2]);
	TruncFloat product = pointX * TruncFloat(_normal[0]);
	TruncFloat accumulator = -TruncFloat(_d) - product;

	accumulator = TruncFloat::multiplySubtract(accumulator, pointZ, TruncFloat(_normal[2]));

	TruncFloat height = TruncFloat(point[1]) - accumulator / TruncFloat(_normal[1]);

	if (height < zero)
		height = -height;

	if (!(height <= TruncFloat(_tolerance)))
		return false;

	if (_numVerts <= 0)
		return true;

	const float *previous = &_verts[_numVerts - 1]->x;

	for (int i = 0; i < _numVerts; i++) {
		const float *current = &_verts[i]->x;
		TruncFloat edgeX = TruncFloat(previous[0]) - TruncFloat(current[0]);
		TruncFloat edgeZ = TruncFloat(previous[2]) - TruncFloat(current[2]);
		TruncFloat offsetZ = TruncFloat(current[2]) - pointZ;
		TruncFloat offsetX = TruncFloat(current[0]) - pointX;
		TruncFloat cross = TruncFloat::multiplySubtract(offsetZ * edgeX, offsetX, edgeZ);
		TruncFloat lengthSquared = TruncFloat::multiplyAdd(edgeX * edgeX, edgeZ, edgeZ);
		TruncFloat side = cross / lengthSquared;
		TruncFloat magnitude = side;

		if (side < zero)
			magnitude = -side;

		if (magnitude <= epsilon)
			side = zero;

		if (side < zero)
			return false;

		previous = current;
	}

	return true;
}

void WalkPlane_v2Ps2::closestPointOnEdge(float *point, const float *current, const float *previous) {
	TruncFloat zero(0.0f);
	TruncFloat edgeX = TruncFloat(current[0]) - TruncFloat(previous[0]);
	TruncFloat edgeZ = TruncFloat(current[2]) - TruncFloat(previous[2]);
	TruncFloat normalX = -edgeZ;
	TruncFloat normalZ = edgeX;
	TruncFloat length = TruncFloat::squareRoot(TruncFloat::multiplyAdd(normalX * normalX, normalZ, normalZ));

	if (!(length < TruncFloat(kMinLengthPs2))) {
		TruncFloat inverse = TruncFloat(1.0f) / length;

		normalX = normalX * inverse;
		normalZ = normalZ * inverse;
	}

	TruncFloat previousDot = TruncFloat::multiplyAdd(normalX * TruncFloat(previous[0]), normalZ, TruncFloat(previous[2]));
	TruncFloat pointDot = TruncFloat::multiplyAdd(normalX * TruncFloat(point[0]), normalZ, TruncFloat(point[2]));
	TruncFloat distance = -previousDot + pointDot;

	if (!(distance < zero))
		return;

	point[0] = (TruncFloat(point[0]) - distance * normalX).toFloat();
	point[2] = (TruncFloat(point[2]) - distance * normalZ).toFloat();

	TruncFloat absoluteX = edgeX;
	TruncFloat absoluteZ = edgeZ;

	if (absoluteX < zero)
		absoluteX = -absoluteX;

	if (absoluteZ < zero)
		absoluteZ = -absoluteZ;

	TruncFloat ratio;

	if (absoluteX <= absoluteZ)
		ratio = (TruncFloat(point[2]) - TruncFloat(previous[2])) / edgeZ;
	else
		ratio = (TruncFloat(point[0]) - TruncFloat(previous[0])) / edgeX;

	if (ratio < zero) {
		point[0] = previous[0];
		point[1] = previous[1];
		point[2] = previous[2];
	}

	if (!(ratio <= TruncFloat(1.0f))) {
		point[0] = current[0];
		point[1] = current[1];
		point[2] = current[2];
	}
}

void WalkPlane_v2Ps2::clampToEdges(float *point) {
	if (_numVerts <= 0)
		return;

	const float *previous = &_verts[_numVerts - 1]->x;

	for (int i = 0; i < _numVerts; i++) {
		const float *current = &_verts[i]->x;

		closestPointOnEdge(point, current, previous);
		previous = current;
	}
}

void WalkPlane_v2Ps2::closestPoint(float *point) {
	TruncFloat zero(0.0f);
	TruncFloat epsilon(kEdgeEpsilonPs2);

	project(point);

	if (_numVerts <= 0)
		return;

	const float *previous = &_verts[_numVerts - 1]->x;

	for (int i = 0; i < _numVerts; i++) {
		const float *current = &_verts[i]->x;
		TruncFloat pointX(point[0]);
		TruncFloat pointZ(point[2]);
		TruncFloat edgeX = TruncFloat(current[0]) - TruncFloat(previous[0]);
		TruncFloat edgeZ = TruncFloat(current[2]) - TruncFloat(previous[2]);
		TruncFloat negativeZ = -edgeZ;
		TruncFloat offsetX = pointX - TruncFloat(previous[0]);
		TruncFloat offsetZ = pointZ - TruncFloat(previous[2]);
		TruncFloat numerator = TruncFloat::multiplyAdd(negativeZ * offsetX, edgeX, offsetZ);

		if (numerator < zero) {
			TruncFloat denominator = -(negativeZ * negativeZ) - edgeX * edgeX;
			TruncFloat magnitude = denominator;

			if (denominator < zero)
				magnitude = -denominator;

			TruncFloat test = denominator;

			if (magnitude <= epsilon)
				test = zero;

			if (!(zero == test)) {
				TruncFloat ratio = numerator / denominator;
				TruncFloat newX = pointX + ratio * negativeZ;
				TruncFloat newZ = pointZ + ratio * edgeX;

				point[0] = newX.toFloat();
				point[2] = newZ.toFloat();

				TruncFloat absoluteZ = edgeZ;
				TruncFloat absoluteX = edgeX;
				TruncFloat position;

				if (absoluteZ < zero)
					absoluteZ = -absoluteZ;

				if (absoluteX < zero)
					absoluteX = -absoluteX;

				if (!(absoluteX <= absoluteZ))
					position = (newX - TruncFloat(previous[0])) / edgeX;
				else
					position = (newZ - TruncFloat(previous[2])) / edgeZ;

				if (i == _numVerts - 1 && !(position <= TruncFloat(1.0f))) {
					point[0] = current[0];
					point[1] = current[1];
					point[2] = current[2];
				} else if (position < zero) {
					point[0] = previous[0];
					point[1] = previous[1];
					point[2] = previous[2];
				} else {
					project(point);
				}
			}
		}

		previous = current;
	}
}

bool WalkPlane_v2Ps2::segmentCrosses(const float *startA, const float *endA, const float *startB, const float *endB, float *ratio) {
	TruncFloat zero(0.0f);
	TruncFloat epsilon(kEdgeEpsilonPs2);
	TruncFloat limit(kRatioLimitPs2);
	TruncFloat edgeDeltaZ = TruncFloat(endB[1]) - TruncFloat(startB[1]);
	TruncFloat edgeDeltaX = TruncFloat(startB[0]) - TruncFloat(endB[0]);
	TruncFloat sideEnd = TruncFloat::multiplyAdd(edgeDeltaZ * TruncFloat(endA[0]), edgeDeltaX, TruncFloat(endA[1]));
	TruncFloat sideLine = TruncFloat::multiplyAdd(edgeDeltaZ * TruncFloat(endB[0]), edgeDeltaX, TruncFloat(endB[1]));

	if (!((sideEnd - sideLine) < zero))
		return false;

	TruncFloat deltaBZ = TruncFloat(endB[1]) - TruncFloat(startB[1]);
	TruncFloat deltaBX = TruncFloat(endB[0]) - TruncFloat(startB[0]);
	TruncFloat deltaAX = TruncFloat(endA[0]) - TruncFloat(startA[0]);
	TruncFloat deltaAZ = TruncFloat(endA[1]) - TruncFloat(startA[1]);
	TruncFloat offsetZ = TruncFloat(startA[1]) - TruncFloat(startB[1]);
	TruncFloat offsetX = TruncFloat(startA[0]) - TruncFloat(startB[0]);
	TruncFloat denominator = TruncFloat::multiplySubtract(deltaAX * deltaBZ, deltaAZ, deltaBX);
	TruncFloat first = TruncFloat::multiplySubtract(offsetZ * deltaBX, offsetX, deltaBZ) / denominator;
	TruncFloat magnitude = first;

	if (first < zero)
		magnitude = -first;

	if (magnitude <= epsilon)
		first = zero;

	if (first < zero)
		return false;

	if (!(first <= limit))
		return false;

	TruncFloat second = TruncFloat::multiplySubtract(offsetZ * deltaAX, offsetX, deltaAZ) / denominator;

	magnitude = second;

	if (second < zero)
		magnitude = -second;

	if (magnitude <= epsilon)
		second = zero;

	if (second < zero)
		return false;

	if (!(second <= limit))
		return false;

	*ratio = first.toFloat();

	return true;
}

void WalkPlane_v2Ps2::findExit(const float *ahead, const float *back, float *out) {
	TruncFloat zero(0.0f);
	float startA[2];
	float endA[2];

	_hitEdge = -1;
	startA[0] = ahead[0];
	startA[1] = ahead[2];
	endA[0] = back[0];
	endA[1] = back[2];

	TruncFloat best(-1.0f);
	const float *previous = &_verts[_numVerts - 1]->x;

	for (int i = 0; i < _numVerts; i++) {
		const float *current = &_verts[i]->x;
		float edgeStart[2];
		float edgeEnd[2];
		float ratio = 0.0f;

		edgeStart[0] = current[0];
		edgeStart[1] = current[2];
		edgeEnd[0] = previous[0];
		edgeEnd[1] = previous[2];

		if (segmentCrosses(startA, endA, edgeStart, edgeEnd, &ratio) && !(TruncFloat(ratio) <= best)) {
			_hitEdge = i;
			best = TruncFloat(ratio);
		}

		previous = current;
	}

	if (best < zero) {
		out[0] = startA[0];
		out[1] = 0.0f;
		out[2] = startA[1];
	} else {
		TruncFloat deltaX = TruncFloat(endA[0]) - TruncFloat(startA[0]);
		TruncFloat deltaZ = TruncFloat(endA[1]) - TruncFloat(startA[1]);

		out[0] = (TruncFloat(startA[0]) + best * deltaX).toFloat();
		out[1] = 0.0f;
		out[2] = (TruncFloat(startA[1]) + best * deltaZ).toFloat();
	}

	project(out);
}

float *WalkPlane_v2Ps2::getConstraintVector(Puck_v2 *basePuck) {
	Puck_v2Ps2 *puck = static_cast<Puck_v2Ps2 *>(basePuck);
	TruncFloat zero(0.0f);
	TruncFloat epsilon(kEdgeEpsilonPs2);

	puck->updateWorld();

	TruncFloat probe(kProbeDistancePs2);
	TruncFloat forwardX(puck->_world._rot._m[6]);
	TruncFloat forwardY(puck->_world._rot._m[7]);
	TruncFloat forwardZ(puck->_world._rot._m[8]);
	TruncFloat stepX = forwardX * probe;
	TruncFloat stepY = forwardY * probe;
	TruncFloat stepZ = forwardZ * probe;

	if ((puck->_nodeFlags & CNode::kPositionValid) == 0)
		puck->computeLocalPositionFromWorld();

	float ahead[3];

	ahead[0] = (TruncFloat(puck->_pos._x) + stepX).toFloat();
	ahead[1] = (TruncFloat(puck->_pos._y) + stepY).toFloat();
	ahead[2] = (TruncFloat(puck->_pos._z) + stepZ).toFloat();

	puck->updateWorld();

	TruncFloat backward(-1.0f);

	stepX = TruncFloat(puck->_world._rot._m[6]) * backward;
	stepY = TruncFloat(puck->_world._rot._m[7]) * backward;
	stepZ = TruncFloat(puck->_world._rot._m[8]) * backward;

	if ((puck->_nodeFlags & CNode::kPositionValid) == 0)
		puck->computeLocalPositionFromWorld();

	float back[3];
	float exitPoint[3];

	back[0] = (TruncFloat(puck->_pos._x) + stepX).toFloat();
	back[1] = (TruncFloat(puck->_pos._y) + stepY).toFloat();
	back[2] = (TruncFloat(puck->_pos._z) + stepZ).toFloat();

	findExit(ahead, back, exitPoint);

	if (_hitEdge == -1) {
		logConsoleStatus("GetConstraintVector confused.\n");

		return nullptr;
	}

	const float *current = &_verts[_hitEdge]->x;
	const float *previous = &_verts[_hitEdge > 0 ? _hitEdge - 1 : _numVerts - 1]->x;
	TruncFloat edgeX = TruncFloat(previous[0]) - TruncFloat(current[0]);
	TruncFloat testX = edgeX;
	TruncFloat ratio;

	if (testX < zero)
		testX = -testX;

	if (testX <= epsilon)
		testX = zero;
	else
		testX = edgeX;

	if (testX == zero)
		ratio = (TruncFloat(exitPoint[2]) - TruncFloat(current[2])) / (TruncFloat(previous[2]) - TruncFloat(current[2]));
	else
		ratio = (TruncFloat(exitPoint[0]) - TruncFloat(current[0])) / edgeX;

	const float *next = &_verts[(_hitEdge + 1) % _numVerts]->x;
	const float *other = &_verts[(_hitEdge - 2 + _numVerts) % _numVerts]->x;
	TruncFloat directionX = TruncFloat(other[0]) - TruncFloat(next[0]);
	TruncFloat directionZ = TruncFloat(other[2]) - TruncFloat(next[2]);
	TruncFloat lineX = TruncFloat(next[0]) + ratio * directionX;
	TruncFloat lineZ = TruncFloat(next[2]) + ratio * directionZ;

	puck->_lineA[0] = lineX.toFloat();
	puck->_lineA[1] = 0.0f;
	puck->_lineA[2] = lineZ.toFloat();
	puck->_lineB[0] = exitPoint[0];
	puck->_lineB[1] = 0.0f;
	puck->_lineB[2] = exitPoint[2];

	_constraintVector[0] = (lineX - TruncFloat(exitPoint[0])).toFloat();
	_constraintVector[1] = 0.0f;
	_constraintVector[2] = (lineZ - TruncFloat(exitPoint[2])).toFloat();

	return _constraintVector;
}

void WalkPlane_v2Ps2::jumpOut(Puck_v2 *puck) {
	float position[3] = { 0.0f, 0.0f, 0.0f };
	float ahead[3] = { 0.0f, 0.0f, 0.0f };
	float hit[3] = { 0.0f, 0.0f, 0.0f };

	if (_numVerts != 4) {
		logConsoleStatus("WalkPlane::JumpOut requires that the plane be a square!\n");

		return;
	}

	if ((puck->_nodeFlags & CNode::kPositionValid) == 0)
		puck->computeLocalPositionFromWorld();

	position[0] = puck->_pos._x;
	position[1] = puck->_pos._y;
	position[2] = puck->_pos._z;

	if ((puck->_nodeFlags & CNode::kWorldValid) == 0)
		puck->updateWorldTransform();

	TruncFloat distance(kJumpOutDistancePs2);

	ahead[0] = (TruncFloat(puck->_world._rot._m[6]) * distance + TruncFloat(position[0])).toFloat();
	ahead[1] = (TruncFloat(puck->_world._rot._m[7]) * distance + TruncFloat(position[1])).toFloat();
	ahead[2] = (TruncFloat(puck->_world._rot._m[8]) * distance + TruncFloat(position[2])).toFloat();

	findExit(position, ahead, hit);

	int edge = _hitEdge;

	if (edge == -1) {
		logConsoleStatus("Leave sector confused.\n");

		return;
	}

	const float *current = &_verts[edge]->x;
	const float *previous = &_verts[edge > 0 ? edge - 1 : _numVerts - 1]->x;
	TruncFloat zero(0.0f);
	TruncFloat deltaX = TruncFloat(previous[0]) - TruncFloat(current[0]);
	TruncFloat magnitude = deltaX;
	TruncFloat ratio;

	if (deltaX < zero)
		magnitude = -deltaX;

	if (magnitude <= TruncFloat(kEdgeEpsilonPs2))
		deltaX = zero;

	if (zero == deltaX)
		ratio = (TruncFloat(hit[2]) - TruncFloat(current[2])) / (TruncFloat(previous[2]) - TruncFloat(current[2]));
	else
		ratio = (TruncFloat(hit[0]) - TruncFloat(current[0])) / deltaX;

	TruncFloat remaining = TruncFloat(1.0f) - ratio;

	_hitEdge = (edge + 2) % _numVerts;

	const float *newCurrent = &_verts[_hitEdge]->x;
	const float *newPrevious = &_verts[_hitEdge > 0 ? _hitEdge - 1 : _numVerts - 1]->x;
	CVector newPosition;

	newPosition._x = (TruncFloat(newCurrent[0]) + remaining * (TruncFloat(newPrevious[0]) - TruncFloat(newCurrent[0]))).toFloat();
	newPosition._y = (TruncFloat(newCurrent[1]) + remaining * (TruncFloat(newPrevious[1]) - TruncFloat(newCurrent[1]))).toFloat();
	newPosition._z = (TruncFloat(newCurrent[2]) + remaining * (TruncFloat(newPrevious[2]) - TruncFloat(newCurrent[2]))).toFloat();

	puck->setLocalPosition(newPosition);
}

} // End of namespace Grim
