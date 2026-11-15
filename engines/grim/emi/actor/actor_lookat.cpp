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
#include "engines/grim/emi/math/cplane.h"
#include "engines/grim/emi/math/cquaternion.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"

namespace Grim {

static const float kLookAtMinLength = 1e-20f;
static const float kLookAtDotLimit = 0.9999f;

static void setJointQuaternion(CNode *joint, const CQuaternion &rotation) {
	joint->_quat = rotation;
	joint->_nodeFlags = (joint->_nodeFlags & ~(uint)CNode::kEulerValid) | CNode::kQuaternionValid | CNode::kLocalSet;

	if (joint->_nodeFlags & CNode::kWorldValid)
		joint->updateChildrenLocalTransforms();

	if ((joint->_nodeFlags & CNode::kPositionValid) == 0)
		joint->computeLocalPositionFromWorld();

	if ((joint->_nodeFlags & (CNode::kEulerValid | CNode::kQuaternionValid)) == 0)
		joint->updateLocalQuaternion();

	joint->_nodeFlags = (joint->_nodeFlags & ~(uint)CNode::kWorldValid) | CNode::kLocalSet;
}

bool Actor::setActorHead(LString jointName, float *params) {
	if (!_extra)
		allocateActorExtra();

	_extra->_lookAtActive = false;
	_extra->_lookAtHasTarget = false;
	_extra->_lookJointName = "";
	_extra->_lookOffset._x = 0.0f;
	_extra->_lookOffset._y = 0.0f;
	_extra->_lookOffset._z = 0.0f;

	if (!jointName.empty()) {
		CNode *joint = findNode(jointName);

		if (!joint)
			return false;

		if ((joint->_nodeFlags & CNode::kQuaternionValid) == 0)
			joint->updateLocalQuaternion();

		_extra->_lookGoalQuat = joint->_quat;
		_extra->_lookCurQuat = _extra->_lookGoalQuat;
		_extra->_lookAtActive = true;
		_extra->_lookJointName = jointName;

		if ((_nodeFlags & CNode::kWorldValid) == 0)
			updateWorldTransform();

		float forwardX = _world._rot._m[6];
		float forwardY = _world._rot._m[7];
		float forwardZ = _world._rot._m[8];

		if ((joint->_nodeFlags & CNode::kWorldValid) == 0)
			joint->updateWorldTransform();

		_extra->_lookDirection._x = (float)((double)forwardX + joint->_world._pos._x);
		_extra->_lookDirection._y = (float)((double)forwardY + joint->_world._pos._y);
		_extra->_lookDirection._z = (float)((double)forwardZ + joint->_world._pos._z);
		joint->worldToLocal(&_extra->_lookDirection);

		CVector &direction = _extra->_lookDirection;
		double length = sqrt(((double)direction._x * direction._x + (double)direction._y * direction._y) + (double)direction._z * direction._z);

		if (length >= (double)kLookAtMinLength) {
			double inverse = 1.0 / length;

			direction._x = (float)(inverse * direction._x);
			direction._y = (float)(inverse * direction._y);
			direction._z = (float)(inverse * direction._z);
		}

		if (!params) {
			warning("Actor::setActorHead(): no offset given");

			return true;
		}

		_extra->_lookOffset._x = params[0];
		_extra->_lookOffset._y = params[1];
		_extra->_lookOffset._z = params[2];
	}

	return true;
}

bool Actor::setHeadLimits(float horizontalLimit, float upLimit, float downLimit) {
	double halfAngle = (double)horizontalLimit * 0.5;
	float halfCos = (float)cos(halfAngle);
	float halfSin = (float)sin(halfAngle);

	if (!_extra)
		allocateActorExtra();

	_extra->_lookLimits[0]._x = halfCos;
	_extra->_lookLimits[0]._y = 0.0f;
	_extra->_lookLimits[0]._z = halfSin;
	_extra->_lookLimits[1]._x = -halfCos;
	_extra->_lookLimits[1]._y = 0.0f;
	_extra->_lookLimits[1]._z = halfSin;
	_extra->_lookLimits[2]._x = 0.0f;
	_extra->_lookLimits[2]._y = -(float)cos((double)upLimit);
	_extra->_lookLimits[2]._z = (float)sin((double)upLimit);
	_extra->_lookLimits[3]._x = 0.0f;
	_extra->_lookLimits[3]._y = (float)cos((double)downLimit);
	_extra->_lookLimits[3]._z = (float)sin((double)downLimit);

	return true;
}

bool Actor::canActorSeePoint(const float *point, float *out) {
	if (!_extra)
		return false;

	if (!point || !out) {
		warning("Actor::canActorSeePoint(): missing point or output");

		return false;
	}

	if ((_nodeFlags & CNode::kWorldValid) == 0)
		updateWorldTransform();

	float forwardX = _world._rot._m[6];
	float forwardZ = _world._rot._m[8];
	double forwardLength = sqrt((double)_world._rot._m[8] * _world._rot._m[8] + (double)_world._rot._m[6] * _world._rot._m[6]);

	if (forwardLength >= (double)kLookAtMinLength) {
		double inverse = 1.0 / forwardLength;

		forwardX = (float)((double)_world._rot._m[6] * inverse);
		forwardZ = (float)((double)_world._rot._m[8] * inverse);
	}

	if ((_nodeFlags & CNode::kWorldValid) == 0)
		updateWorldTransform();

	float deltaX = (float)((double)point[0] - _world._pos._x);
	double deltaY = (double)point[1] - _world._pos._y;
	float deltaZ = (float)((double)point[2] - _world._pos._z);
	double distance = sqrt(((double)deltaZ * deltaZ + deltaY * deltaY) + (double)deltaX * deltaX);
	float directionX = deltaX;
	float directionZ = deltaZ;

	if (distance >= (double)kLookAtMinLength) {
		double inverse = 1.0 / distance;

		directionX = (float)((double)deltaX * inverse);
		directionZ = (float)((double)deltaZ * inverse);
		*out = (float)distance;
	} else {
		*out = 0.0f;
	}

	double horizontal = sqrt((double)directionZ * directionZ + (double)directionX * directionX);
	float horizontalStored = (float)horizontal;
	double normalizedX = directionX;
	float normalizedZ = directionZ;

	if (horizontal >= (double)kLookAtMinLength) {
		double inverse = 1.0 / (double)horizontalStored;
		float inverseStored = (float)inverse;

		normalizedX = inverse * (double)directionX;
		normalizedZ = (float)((double)directionZ * inverseStored);
	}

	double dot = ((double)forwardX * normalizedX + (double)normalizedZ * (double)forwardZ) + 0.0 * 0.0;

	return dot >= (double)_extra->_lookCosFov;
}

bool Actor::canActorSeeActor(Actor *other, float *out) {
	if (!other) {
		warning("Actor::canActorSeeActor(): no actor");

		return false;
	}

	if (!other->_room._resource)
		return false;

	if (!_room._resource)
		return false;

	if (_room._resource->_name != other->_room._resource->_name)
		return false;

	if ((other->_nodeFlags & CNode::kWorldValid) == 0)
		other->updateWorldTransform();

	return canActorSeePoint(&other->_world._pos._x, out);
}

void Actor::setLookAtJoints(float deltaTime) {
	CQuaternion slerped;
	CQuaternion target;

	if (!_extra)
		return;

	if (_extra->_lookTargetActorId != 0) {
		Actor *targetActor = actorDeref(_engine, _extra->_lookTargetActorId);

		if (targetActor) {
			if ((targetActor->_nodeFlags & CNode::kWorldValid) == 0)
				targetActor->updateWorldTransform();

			_extra->_lookTarget._x = targetActor->_world._pos._x;
			_extra->_lookTarget._y = targetActor->_world._pos._y;
			_extra->_lookTarget._z = targetActor->_world._pos._z;
		} else {
			LConsoleServices *consoleServices = _engine->getConsoleServices();

			if (consoleServices)
				consoleServices->logError("SetLookAtJoints trying to look at deleted actor!!");
			else
				warning("SetLookAtJoints trying to look at deleted actor!!");

			_extra->_lookTargetActorId = 0;
			_extra->_lookAtHasTarget = false;
		}
	}

	CNode *joint = findNode(_extra->_lookJointName);

	if (!joint)
		return;

	if ((joint->_nodeFlags & CNode::kQuaternionValid) == 0)
		joint->updateLocalQuaternion();

	CQuaternion jointQuat = joint->_quat;
	CVector direction = _extra->_lookDirection;

	if (_extra->_lookAtHasTarget) {
		CVector aim;
		aim._x = (float)((double)_extra->_lookOffset._x + _extra->_lookTarget._x);
		aim._y = (float)((double)_extra->_lookOffset._y + _extra->_lookTarget._y);
		aim._z = (float)((double)_extra->_lookOffset._z + _extra->_lookTarget._z);

		CVector toTarget = aim;
		joint->worldToLocal(&toTarget);

		CPlane plane;
		plane._n._x = 0.0f;
		plane._n._y = 0.0f;
		plane._n._z = 0.0f;
		plane._d = 0.0f;

		CVector worldLimit;

		for (int i = 0; i < Extra_v2::kNumLookLimits; i++) {
			localToWorldDirection(&_extra->_lookLimits[i], &worldLimit);

			if ((joint->_nodeFlags & CNode::kWorldValid) == 0)
				joint->updateWorldTransform();

			plane.setFromNormalAndPoint(worldLimit, joint->_world._pos);

			if ((joint->_nodeFlags & CNode::kWorldValid) == 0)
				joint->updateWorldTransform();

			plane.transformToLocal(&joint->_world);

			if (plane.classifyPoint(toTarget) == CPlane::kPointBehind)
				plane.projectPoint(&toTarget);
		}

		double targetLength = sqrt(((double)toTarget._x * toTarget._x + (double)toTarget._y * toTarget._y) + (double)toTarget._z * toTarget._z);

		if (targetLength >= (double)kLookAtMinLength) {
			double inverse = 1.0 / targetLength;

			toTarget._x = (float)(inverse * toTarget._x);
			toTarget._y = (float)(inverse * toTarget._y);
			toTarget._z = (float)(inverse * toTarget._z);
		}

		float cosine = (float)(((double)toTarget._z * direction._z + (double)toTarget._y * direction._y) + (double)direction._x * toTarget._x);
		CVector cross;
		cross._x = (float)((double)toTarget._z * direction._y - (double)toTarget._y * direction._z);
		cross._y = (float)((double)direction._z * toTarget._x - (double)toTarget._z * direction._x);
		cross._z = (float)((double)toTarget._y * direction._x - (double)direction._y * toTarget._x);

		CVector axis = cross;
		double crossLength = sqrt(((double)cross._z * cross._z + (double)cross._y * cross._y) + (double)cross._x * cross._x);

		if (crossLength >= (double)kLookAtMinLength) {
			double inverse = 1.0 / crossLength;

			axis._x = (float)((double)cross._x * inverse);
			axis._y = (float)((double)axis._y * inverse);
			axis._z = (float)((double)axis._z * inverse);
		}

		float angle = (float)acos((double)cosine);
		CQuaternion rotation(axis, angle);
		const CQuaternion &goal = _extra->_lookGoalQuat;
		float resultW = (float)((((double)rotation._w * goal._w - (double)goal._x * rotation._x) - (double)goal._y * rotation._y) - (double)goal._z * rotation._z);
		float resultZ = (float)((((double)goal._x * rotation._y + (double)rotation._w * goal._z) + (double)rotation._z * goal._w) - (double)goal._y * rotation._x);
		float resultY = (float)((((double)goal._z * rotation._x + (double)rotation._w * goal._y) + (double)rotation._y * goal._w) - (double)goal._x * rotation._z);
		float resultX = (float)((((double)rotation._w * goal._x + (double)goal._y * rotation._z) + (double)rotation._x * goal._w) - (double)goal._z * rotation._y);

		target = CQuaternion(resultX, resultY, resultZ, resultW);
	} else {
		target = jointQuat;
	}

	float step = (float)((double)deltaTime * _extra->_lookRate);
	float dot = _extra->_lookCurQuat.dot(target);

	// The demo does not clamp the dot product
	if (!_engine->isDemo()) {
		if (dot > kLookAtDotLimit)
			dot = kLookAtDotLimit;
		else if (!(dot >= -kLookAtDotLimit))
			dot = -kLookAtDotLimit;
	}

	double span = acos((double)dot);
	span = span + span;

	// An unordered or zero span snaps to the target, avoiding a division by zero
	if (span >= (double)step && (!_engine->isDemo() || span > 0.0)) {
		float fraction = (float)((double)step / span);

		_extra->_lookCurQuat.slerp(target, fraction, &slerped);
		slerped.normalize();
		setJointQuaternion(joint, slerped);
		_extra->_lookCurQuat = slerped;
	} else {
		setJointQuaternion(joint, target);
		_extra->_lookCurQuat = target;
	}
}

} // End of namespace Grim
