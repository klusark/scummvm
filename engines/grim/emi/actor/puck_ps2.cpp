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

#include "engines/grim/emi/actor/puck_v2.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/math/singlemath.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

enum {
	kPlaneTypeWalk = 0x1000,
	kPlaneTypeConstraint = 0x1200
};

static const float kMinLengthPs2 = 9.99999968e-21f;
static const float kAngleScalePs2 = 57.2957764f;
static const float kNeighborDistancePs2 = 0.00999999978f;

void Puck_v2::updateWorldPs2() {
	if ((_nodeFlags & kWorldValid) != 0)
		return;

	if (_parent || (_nodeFlags & kQuaternionValid) != 0 || (_nodeFlags & kEulerValid) == 0) {
		updateWorldTransform();

		return;
	}

	SingleMath::matrixFromEuler(_eulerDeg._x, _eulerDeg._y, _eulerDeg._z, _world._rot._m);

	_world._pos._x = _pos._x;
	_world._pos._y = _pos._y;
	_world._pos._z = _pos._z;
	_nodeFlags |= kWorldValid;
}

void Puck_v2::rotatePs2(TruncFloat angle) {
	if ((_nodeFlags & kEulerValid) == 0)
		computeLocalMatrixFromWorld();

	float savedX = _eulerDeg._x;
	float savedZ = _eulerDeg._z;

	_eulerDeg._x = savedX;
	_eulerDeg._y = (TruncFloat(_eulerDeg._y) + angle).toFloat();
	_eulerDeg._z = savedZ;
	_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
	updateLocalState();
}

TruncFloat Puck_v2::getYawToPointPs2(const float *target) {
	TruncFloat zero(0.0f);
	TruncFloat full(360.0f);

	if ((_nodeFlags & kEulerValid) == 0)
		computeLocalMatrixFromWorld();

	TruncFloat yaw(_eulerDeg._y);

	while (yaw < zero)
		yaw = yaw + full;

	while (!(yaw < full))
		yaw = yaw - full;

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldPs2();

	TruncFloat directionX = TruncFloat(target[0]) - TruncFloat(_world._pos._x);
	TruncFloat directionZ = TruncFloat(target[2]) - TruncFloat(_world._pos._z);
	TruncFloat length = TruncFloat::squareRoot(TruncFloat::multiplyAdd(directionX * directionX + zero * zero, directionZ, directionZ));

	if (!(length < TruncFloat(kMinLengthPs2))) {
		TruncFloat inverse = TruncFloat(1.0f) / length;

		directionX = directionX * inverse;
		directionZ = directionZ * inverse;
	}

	TruncFloat angle = TruncFloat(SingleMath::kDegreesToRadians) * TruncFloat(SingleMath::acosDegrees(directionZ.toFloat()));

	angle = TruncFloat(kAngleScalePs2) * angle;

	if (directionX < zero)
		angle = full - angle;

	TruncFloat difference = angle - yaw;
	bool reversed = false;

	if (!(difference <= TruncFloat(180.0f))) {
		reversed = true;
		difference = full - difference;
	}

	return difference * TruncFloat(reversed ? -1.0f : 1.0f);
}

TruncFloat Puck_v2::pointAtPs2(const float *target, TruncFloat maxAngle) {
	TruncFloat angle = maxAngle;

	if (!(maxAngle < getYawToPointPs2(target)))
		angle = getYawToPointPs2(target);

	rotatePs2(angle);

	return angle;
}

bool Puck_v2::constrainPs2(const float *vector) {
	TruncFloat zero(0.0f);

	if (vector) {
		_constraint[0] = vector[0];
		_constraint[1] = vector[1];
		_constraint[2] = vector[2];
		_constraint[1] = 0.0f;

		TruncFloat length = TruncFloat::squareRoot(TruncFloat::multiplyAdd(TruncFloat(_constraint[0]) * TruncFloat(_constraint[0]) + TruncFloat(_constraint[1]) * TruncFloat(_constraint[1]), TruncFloat(_constraint[2]), TruncFloat(_constraint[2])));

		if (!(length < TruncFloat(kMinLengthPs2))) {
			TruncFloat inverse = TruncFloat(1.0f) / length;

			_constraint[0] = (TruncFloat(_constraint[0]) * inverse).toFloat();
			_constraint[1] = (TruncFloat(_constraint[1]) * inverse).toFloat();
			_constraint[2] = (TruncFloat(_constraint[2]) * inverse).toFloat();
		}

		_constrained = true;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldPs2();

	TruncFloat dot = TruncFloat::multiplyAdd(TruncFloat(_world._rot._m[6]) * TruncFloat(_constraint[0]) + TruncFloat(_world._rot._m[7]) * TruncFloat(_constraint[1]), TruncFloat(_world._rot._m[8]), TruncFloat(_constraint[2]));
	TruncFloat magnitude = dot;

	if (dot < zero)
		magnitude = -dot;

	if (magnitude < TruncFloat(0.5f))
		return false;

	TruncFloat angle = TruncFloat(SingleMath::kDegreesToRadians) * TruncFloat(SingleMath::acosDegrees(_constraint[2]));

	angle = TruncFloat(kAngleScalePs2) * angle;

	if (TruncFloat(_constraint[0]) < zero)
		angle = TruncFloat(360.0f) - angle;

	float yaw;

	if (dot <= zero)
		yaw = (TruncFloat(180.0f) + angle).toFloat();
	else
		yaw = angle.toFloat();

	_eulerDeg._x = 0.0f;
	_eulerDeg._y = yaw;
	_eulerDeg._z = 0.0f;
	_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
	updateLocalState();

	return true;
}

void Puck_v2::closestPointOnConstraintLinePs2(float *out, const float *point) {
	TruncFloat zero(0.0f);
	TruncFloat pointX = TruncFloat(point[0]) - TruncFloat(_lineA[0]);
	TruncFloat pointY = zero - TruncFloat(_lineA[1]);
	TruncFloat pointZ = TruncFloat(point[2]) - TruncFloat(_lineA[2]);
	TruncFloat lineX = TruncFloat(_lineB[0]) - TruncFloat(_lineA[0]);
	TruncFloat lineY = TruncFloat(_lineB[1]) - TruncFloat(_lineA[1]);
	TruncFloat lineZ = TruncFloat(_lineB[2]) - TruncFloat(_lineA[2]);
	TruncFloat numerator = TruncFloat::multiplyAdd(pointX * lineX + pointY * lineY, pointZ, lineZ);
	TruncFloat denominator = TruncFloat::multiplyAdd(lineX * lineX + lineY * lineY, lineZ, lineZ);
	TruncFloat ratio = numerator / denominator;

	out[0] = (TruncFloat(_lineA[0]) + lineX * ratio).toFloat();
	out[1] = (TruncFloat(_lineA[1]) + lineY * ratio).toFloat();
	out[2] = (TruncFloat(_lineA[2]) + lineZ * ratio).toFloat();
}

bool Puck_v2::setPlanePs2(WalkPlane_v2 *plane, const float *point) {
	bool changed = _plane != plane;
	float position[3];

	_plane = plane;
	position[0] = point[0];
	position[1] = point[1];
	position[2] = point[2];
	_plane->projectPs2(position);

	CVector newPosition;

	newPosition._x = position[0];
	newPosition._y = position[1];
	newPosition._z = position[2];
	setLocalPosition(newPosition);

	if (!changed || (plane->_type & kPlaneTypeConstraint) != kPlaneTypeConstraint) {
		_constrained = false;

		return false;
	}

	float *vector = plane->getConstraintVectorPs2(this);

	if (vector) {
		constrainPs2(vector);
		_constrained = true;
	}

	return true;
}

bool Puck_v2::tryNeighborPlanesPs2() {
	for (uint i = 0; i < _plane->_neighbors.size(); i++) {
		WalkPlane_v2 *neighbor = _planes->planeNumbered(_plane->_neighbors[i]);
		float probe[3];

		probe[0] = _pos._x;
		probe[1] = _pos._y;
		probe[2] = _pos._z;

		if (!neighbor || !neighbor->_enabled)
			continue;

		if (!neighbor->_active || (neighbor->_type & kPlaneTypeWalk) == 0)
			continue;

		neighbor->clampToEdgesPs2(probe);

		TruncFloat deltaY = TruncFloat(probe[1]) - TruncFloat(_pos._y);
		TruncFloat deltaX = TruncFloat(probe[0]) - TruncFloat(_pos._x);
		TruncFloat deltaZ = TruncFloat(probe[2]) - TruncFloat(_pos._z);
		TruncFloat distance = TruncFloat::multiplyAdd(deltaX * deltaX + deltaY * deltaY, deltaZ, deltaZ);

		if (distance < TruncFloat(kNeighborDistancePs2)) {
			setPlanePs2(neighbor, probe);

			return true;
		}
	}

	float position[3];

	position[0] = _pos._x;
	position[1] = _pos._y;
	position[2] = _pos._z;

	WalkPlane_v2 *found = _planes->planeAt(position, kPlaneTypeWalk);

	if (!found)
		return false;

	setPlanePs2(found, position);

	return true;
}

void Puck_v2::clampToPlanePs2(const float *target) {
	float position[3];

	position[0] = target[0];
	position[1] = target[1];
	position[2] = target[2];
	_plane->clampToEdgesPs2(position);
	_plane->projectPs2(position);
	setPlanePs2(_plane, position);
}

bool Puck_v2::finishMovePs2(const float *target) {
	float position[3];

	position[0] = target[0];
	position[1] = target[1];
	position[2] = target[2];

	if (_plane->withinPs2(position, true)) {
		CVector newPosition;

		newPosition._x = position[0];
		newPosition._y = position[1];
		newPosition._z = position[2];
		setLocalPosition(newPosition);

		return true;
	}

	_plane->_enabled = false;

	WalkPlane_v2 *found = _planes->planeAt(position, kPlaneTypeWalk);

	_plane->_enabled = true;

	if (found && (found->_type & kPlaneTypeConstraint) == kPlaneTypeConstraint) {
		setPlanePs2(found, position);

		return true;
	}

	clampToPlanePs2(position);
	_plane->_enabled = false;

	while (tryNeighborPlanesPs2() && !_constrained) {
		clampToPlanePs2(position);
		_plane->_enabled = false;
	}

	_planes->allActive();

	return true;
}

bool Puck_v2::forwardPs2(TruncFloat distance) {
	float target[3] = { 0.0f, 0.0f, 0.0f };

	if (_constrained && !constrainPs2(nullptr))
		return false;

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldPs2();

	if (_plane) {
		float ahead[3];

		ahead[0] = (TruncFloat(_world._pos._x) + TruncFloat(_world._rot._m[6])).toFloat();
		ahead[1] = (TruncFloat(_world._pos._y) + TruncFloat(_world._rot._m[7])).toFloat();
		ahead[2] = (TruncFloat(_world._pos._z) + TruncFloat(_world._rot._m[8])).toFloat();
		_plane->projectPs2(ahead);

		if ((_nodeFlags & kWorldValid) == 0)
			updateWorldPs2();

		TruncFloat directionX = TruncFloat(ahead[0]) - TruncFloat(_world._pos._x);
		TruncFloat directionY = TruncFloat(ahead[1]) - TruncFloat(_world._pos._y);
		TruncFloat directionZ = TruncFloat(ahead[2]) - TruncFloat(_world._pos._z);
		TruncFloat length = TruncFloat::squareRoot(TruncFloat::multiplyAdd(directionX * directionX + directionY * directionY, directionZ, directionZ));

		if (!(length < TruncFloat(kMinLengthPs2))) {
			TruncFloat inverse = TruncFloat(1.0f) / length;

			directionX = directionX * inverse;
			directionY = directionY * inverse;
			directionZ = directionZ * inverse;
		}

		TruncFloat stepX = directionX * distance;
		TruncFloat stepY = directionY * distance;
		TruncFloat stepZ = directionZ * distance;

		if ((_nodeFlags & kWorldValid) == 0)
			updateWorldPs2();

		target[0] = (TruncFloat(_world._pos._x) + stepX).toFloat();
		target[1] = (TruncFloat(_world._pos._y) + stepY).toFloat();
		target[2] = (TruncFloat(_world._pos._z) + stepZ).toFloat();

		if (_constrained) {
			float closest[3];

			closestPointOnConstraintLinePs2(closest, target);
			target[0] = closest[0];
			target[1] = closest[1];
			target[2] = closest[2];
			_plane->projectPs2(target);
		}

		return finishMovePs2(target);
	}

	TruncFloat stepX = TruncFloat(_world._rot._m[6]) * distance;
	TruncFloat stepY = TruncFloat(_world._rot._m[7]) * distance;
	TruncFloat stepZ = TruncFloat(_world._rot._m[8]) * distance;

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldPs2();

	CVector newPosition;

	newPosition._x = (TruncFloat(_world._pos._x) + stepX).toFloat();
	newPosition._y = (TruncFloat(_world._pos._y) + stepY).toFloat();
	newPosition._z = (TruncFloat(_world._pos._z) + stepZ).toFloat();
	setLocalPosition(newPosition);

	return true;
}

} // End of namespace Grim
