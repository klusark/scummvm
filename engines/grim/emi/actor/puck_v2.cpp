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

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/actor/puck_v2.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

enum {
	kTypeWalk = 0x1000,
	kTypeFunnel = 0x100,
	kTypeConstraint = 0x1200,
	kMaxRecursionDepth = 8
};

static const float kMinLength = 1e-20f;
static const float kFullTurnDegrees = 360.0f;
static const float kHalfTurnDegrees = 180.0f;
static const float kQuarterTurnDegrees = 90.0f;
static const float kMinConstraintAlignment = 0.5f;
static const float kFunnelAngleLimit = 3.0f;
static const double kDemoMinRotation = 0.00001;
static const double kDemoFunnelOffset = 90.2;
static const double kDemoMinMovement = 0.01;
static const double kDemoParallelEpsilon = 0.00001;

Puck_v2::Puck_v2(EmiEngine *engine, float newReflectionAngle) : _engine(engine) {
	_constraint[0] = 0.0f;
	_constraint[1] = 0.0f;
	_constraint[2] = 0.0f;
	_lineA[0] = 0.0f;
	_lineA[1] = 0.0f;
	_lineA[2] = 0.0f;
	_lineB[0] = 0.0f;
	_lineB[1] = 0.0f;
	_lineB[2] = 0.0f;
	_planes = nullptr;
	_startStep = 0.0f;
	reset();
	_reflectionAngle = newReflectionAngle;
}

Puck_v2::~Puck_v2() {
}

void Puck_v2::reset() {
	_plane = nullptr;
	_constrained = false;
	_constraint[0] = 0.0f;
	_constraint[1] = 1.0f;
	_constraint[2] = 0.0f;
}

void Puck_v2::validate(WalkPlaneCollection *collection) {
	_planes = collection;

	if (!_plane) {
		if (!collection) {
			warning("Puck_v2::validate(): missing collection");

			return;
		}

		_plane = collection->planeClosest(&_pos._x, kTypeWalk);
	}

	if (_plane && !_plane->within(&_pos._x, true)) {
		_plane->closestPoint(&_pos._x);

		CVector newPosition = _pos;

		setLocalPosition(newPosition);
	}
}

void Puck_v2::rotate(float angle) {
	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		rotatePs2(TruncFloat(angle));

		return;
	}

	if (_engine->isDemo() && fabs((double)angle) <= kDemoMinRotation)
		return;

	if ((_nodeFlags & kEulerValid) == 0)
		computeLocalMatrixFromWorld();

	_eulerDeg._y = (float)((double)_eulerDeg._y + angle);
	_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
	updateLocalState();
}

float Puck_v2::pointAt(const float *target, float maxAngle) {
	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return pointAtPs2(target, TruncFloat(maxAngle)).toFloat();

	float angle;

	if (getYawToPoint(target) > maxAngle)
		angle = maxAngle;
	else
		angle = getYawToPoint(target);

	rotate(angle);

	return angle;
}

float Puck_v2::getYawToPoint(const float *target) {
	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return getYawToPointPs2(target).toFloat();

	if ((_nodeFlags & kEulerValid) == 0)
		computeLocalMatrixFromWorld();

	double yaw = _eulerDeg._y;
	float storedYaw = _eulerDeg._y;

	if (yaw - yaw != 0.0) {
		warning("Puck_v2::getYawToPoint(): invalid yaw");
		yaw = 0.0;
		storedYaw = 0.0f;
	}

	if (yaw < 0.0) {
		do {
			yaw += kFullTurnDegrees;
		} while (yaw < 0.0);

		storedYaw = (float)yaw;
	}

	if (!(yaw < kFullTurnDegrees)) {
		do {
			yaw -= kFullTurnDegrees;
		} while (yaw >= kFullTurnDegrees);

		storedYaw = (float)yaw;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	float deltaX = (float)((double)target[0] - _world._pos._x);
	float deltaZ = (float)((double)target[2] - _world._pos._z);
	float normalizedX = deltaX;
	float normalizedZ = deltaZ;
	double length = sqrt((double)deltaX * deltaX + (double)deltaZ * deltaZ);

	if (length >= kMinLength) {
		double inverse = 1.0 / length;

		normalizedX = (float)(deltaX * inverse);
		normalizedZ = (float)(inverse * deltaZ);
	}

	double angle = acos((double)normalizedZ) * WalkPlane_v2::kRadiansToDegrees;

	if (normalizedX < 0.0f)
		angle = kFullTurnDegrees - angle;

	angle -= storedYaw;

	if (angle > kHalfTurnDegrees)
		return (float)-(kFullTurnDegrees - angle);

	return (float)angle;
}

float *Puck_v2::closestPointOnConstraintLine(float *out, const float *point) {
	float pointX = point[0];
	float pointZ = point[2];
	double pointDeltaX = (double)pointX - _lineA[0];
	double negativeLineAY = -(double)_lineA[1];
	double pointDeltaZ = (double)pointZ - _lineA[2];
	float lineDeltaX = (float)((double)_lineB[0] - _lineA[0]);
	float lineDeltaY = (float)((double)_lineB[1] - _lineA[1]);
	double lineDeltaZExact = (double)_lineB[2] - _lineA[2];
	float lineDeltaZ = (float)lineDeltaZExact;
	double dot = (lineDeltaZExact * pointDeltaZ + (double)lineDeltaY * negativeLineAY) + (double)lineDeltaX * pointDeltaX;
	double lengthSquared = ((double)lineDeltaZ * lineDeltaZ + (double)lineDeltaY * lineDeltaY) + (double)lineDeltaX * lineDeltaX;
	double ratio;

	if (lengthSquared == 0.0) {
		// The original could divide by zero here, let's avoid that...
		warning("Puck_v2::closestPointOnConstraintLine(): degenerate constraint line");
		ratio = 0.0;
	} else {
		ratio = dot / lengthSquared;
	}

	float scaledX = (float)(lineDeltaX * ratio);
	float scaledY = (float)(lineDeltaY * ratio);
	double scaledZ = ratio * lineDeltaZ;

	out[0] = (float)((double)scaledX + _lineA[0]);
	out[1] = (float)((double)scaledY + _lineA[1]);
	out[2] = (float)(scaledZ + _lineA[2]);

	return out;
}

bool Puck_v2::forward(float distance, int allowLeft, int allowRight) {
	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return forwardPs2(TruncFloat(distance));

	float target[3] = { 0.0f, 0.0f, 0.0f };
	float hit[3] = { 0.0f, 0.0f, 0.0f };

	// The turn flags are only used by the demo wall rule

	if (_engine->isDemo() && _engine->_puckRecursionDepth == 0)
		_startStep = distance;

	_engine->_puckRecursionDepth++;

	if (_constrained) {
		if (!constrain(nullptr))
			return false;
	}

	if (!_plane) {
		if ((_nodeFlags & kWorldValid) == 0)
			updateWorldTransform();

		float forwardX = _world._rot._m[6];
		float forwardY = _world._rot._m[7];
		float forwardZ = _world._rot._m[8];
		float step[3];

		step[0] = (float)((double)forwardX * distance);
		step[1] = (float)((double)forwardY * distance);
		step[2] = (float)((double)forwardZ * distance);

		if ((_nodeFlags & kWorldValid) == 0)
			updateWorldTransform();

		CVector newPosition;

		newPosition._y = (float)((double)step[1] + _world._pos._y);
		newPosition._x = (float)((double)step[0] + _world._pos._x);
		newPosition._z = (float)((double)step[2] + _world._pos._z);
		setLocalPosition(newPosition);
		_engine->_puckRecursionDepth = 0;

		return true;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	float forwardX = _world._rot._m[6];
	float forwardY = _world._rot._m[7];
	float forwardZ = _world._rot._m[8];

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	float ahead[3];
	float step[3];

	ahead[0] = (float)((double)forwardX + _world._pos._x);
	ahead[1] = (float)((double)forwardY + _world._pos._y);
	ahead[2] = (float)((double)forwardZ + _world._pos._z);

	if (_engine->isDemo()) {
		double alignment = (double)forwardX * _plane->_normal[0] + (double)forwardY * _plane->_normal[1] + (double)forwardZ * _plane->_normal[2];

		if (fabs(alignment) != 1.0)
			_plane->project(ahead);
	} else {
		_plane->project(ahead);
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	ahead[0] = (float)((double)ahead[0] - _world._pos._x);
	ahead[1] = (float)((double)ahead[1] - _world._pos._y);
	ahead[2] = (float)((double)ahead[2] - _world._pos._z);

	double length = sqrt(((double)ahead[0] * ahead[0] + (double)ahead[2] * ahead[2]) + (double)ahead[1] * ahead[1]);

	if (length >= kMinLength) {
		double inverse = 1.0 / length;

		ahead[0] = (float)(inverse * ahead[0]);
		ahead[1] = (float)(ahead[1] * inverse);
		ahead[2] = (float)(ahead[2] * inverse);
	}

	step[0] = (float)((double)ahead[0] * distance);
	step[1] = (float)((double)ahead[1] * distance);
	step[2] = (float)((double)ahead[2] * distance);

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	target[0] = (float)((double)step[0] + _world._pos._x);
	target[1] = (float)((double)step[1] + _world._pos._y);
	target[2] = (float)((double)step[2] + _world._pos._z);

	if (_constrained && !_engine->isDemo()) {
		float *closest = closestPointOnConstraintLine(step, target);

		target[0] = closest[0];
		target[1] = closest[1];
		target[2] = closest[2];
		_plane->project(target);
	}

	if (_plane->within(target, true)) {
		_plane->project(target);

		CVector newPosition;

		newPosition._x = target[0];
		newPosition._y = target[1];
		newPosition._z = target[2];
		setLocalPosition(newPosition);
		_engine->_puckRecursionDepth = 0;

		return true;
	}

	if (_engine->_puckRecursionDepth > kMaxRecursionDepth) {
		_engine->_puckRecursionDepth = 0;

		return false;
	}

	WalkPlane_v2 *oldPlane = _plane;
	float angle = oldPlane->intersection(&_pos._x, target, hit);

	oldPlane->_enabled = false;

	CVector hitLocal;

	hitLocal._x = hit[0];
	hitLocal._y = hit[1];
	hitLocal._z = hit[2];
	worldToLocal(&hitLocal);

	float remaining = (float)((double)distance - hitLocal._z);
	WalkPlane_v2 *newPlane = nullptr;

	if (_planes)
		newPlane = _planes->planeAt(hit, kTypeWalk);
	else
		warning("Puck_v2::forward(): missing collection");

	CVector hitPosition;

	hitPosition._x = hit[0];
	hitPosition._y = hit[1];
	hitPosition._z = hit[2];

	if (newPlane) {
		setLocalPosition(hitPosition);
		_plane = newPlane;

		if ((newPlane->_type & kTypeConstraint) == kTypeConstraint) {
			float *constraintVector = newPlane->getConstraintVector(this);

			if (_engine->isDemo()) {
				constrain(constraintVector);
				_constrained = true;
			} else if (constraintVector) {
				constrain(constraintVector);
				_constrained = true;
			}

			oldPlane->_enabled = true;

			return false;
		}

		_constrained = false;

		bool result = forward(remaining, 1, 1);

		oldPlane->_enabled = true;

		return result;
	}

	if ((_nodeFlags & kPositionValid) == 0)
		computeLocalPositionFromWorld();

	bool wallResult = false;

	if (_engine->isDemo()) {
		double movedX = (double)_pos._x - hitPosition._x;
		double movedY = (double)_pos._y - hitPosition._y;
		double movedZ = (double)_pos._z - hitPosition._z;

		if (sqrt(movedX * movedX + movedY * movedY + movedZ * movedZ) > kDemoMinMovement) {
			_engine->_puckRecursionDepth = 0;
			wallResult = true;
		}
	}

	setLocalPosition(hitPosition);

	if ((_plane->_type & kTypeFunnel) == 0) {
		if (_engine->isDemo()) {
			if (fabs((double)angle) >= _reflectionAngle) {
				_startStep = (float)((double)_startStep - remaining);
			} else if ((angle > 0.0f && allowLeft) || (angle < 0.0f && allowRight)) {
				rotate(angle);
				oldPlane->_enabled = true;
				wallResult = forward(remaining, 1, 1);
			}

			oldPlane->_enabled = true;

			return wallResult;
		}

		rotate(angle);
		oldPlane->_enabled = true;
	} else {
		int axis = _plane->_quadAxis;

		if (_plane->_numVerts <= 0 || axis < 0 || axis >= _plane->_numVerts) {
			warning("Puck_v2::forward(): invalid funnel (axis %d of %d)", axis, _plane->_numVerts);

			return false;
		}

		if ((axis + 1) % _plane->_numVerts == _plane->_hitEdge)
			return false;

		CVector corner;

		corner._x = _plane->_verts[axis]->x;
		corner._y = _plane->_verts[axis]->y;
		corner._z = _plane->_verts[axis]->z;
		worldToLocal(&corner);

		if (_engine->isDemo()) {
			if (!(corner._x > 0.0f)) {
				if (angle < kFunnelAngleLimit)
					angle = (float)((double)angle + kQuarterTurnDegrees + kDemoFunnelOffset);
			} else if (angle > kFunnelAngleLimit) {
				angle = (float)-(kDemoFunnelOffset + ((double)kQuarterTurnDegrees - angle));
			}
		} else if (!(corner._x > 0.0f)) {
			if (angle < kFunnelAngleLimit)
				angle = (float)((double)angle + kQuarterTurnDegrees + kQuarterTurnDegrees);
		} else if (angle > kFunnelAngleLimit) {
			angle = (float)(-(double)kQuarterTurnDegrees - ((double)kQuarterTurnDegrees - angle));
		}

		rotate(angle);
	}

	bool result = forward(remaining, 1, 1);

	oldPlane->_enabled = true;

	return result;
}

bool Puck_v2::constrain(const float *vector) {
	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return constrainPs2(vector);

	if (vector) {
		_constraint[0] = vector[0];
		_constraint[1] = vector[1];
		_constraint[2] = vector[2];
		_constraint[1] = 0.0f;

		double length = sqrt(((double)_constraint[0] * _constraint[0] + (double)_constraint[1] * _constraint[1]) + (double)_constraint[2] * _constraint[2]);

		if (length >= kMinLength) {
			double inverse = 1.0 / length;

			_constraint[0] = (float)(inverse * _constraint[0]);
			_constraint[1] = (float)(inverse * _constraint[1]);
			_constraint[2] = (float)(inverse * _constraint[2]);
		}

		_constrained = true;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	double dot = ((double)_world._rot._m[8] * _constraint[2] + (double)_world._rot._m[7] * _constraint[1]) + (double)_world._rot._m[6] * _constraint[0];
	float storedDot = (float)dot;

	if (dot < 0.0)
		dot = -dot;

	if (dot < kMinConstraintAlignment)
		return false;

	// The demo keeps the yaw when the actor already faces along the constraint
	if (_engine->isDemo() && fabs(dot - 1.0) <= kDemoParallelEpsilon)
		return true;

	double angle = acos((double)_constraint[2]) * WalkPlane_v2::kRadiansToDegrees;

	if (_constraint[0] < 0.0f)
		angle = kFullTurnDegrees - angle;

	if (!(storedDot > 0.0f))
		angle += kHalfTurnDegrees;

	_eulerDeg._y = (float)angle;
	_eulerDeg._x = 0.0f;
	_eulerDeg._z = 0.0f;
	_nodeFlags = (_nodeFlags & ~kQuaternionValid) | kEulerValid | kLocalSet;
	updateLocalState();

	return true;
}

} // End of namespace Grim
