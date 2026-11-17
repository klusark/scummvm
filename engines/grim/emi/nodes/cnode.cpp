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
#include "common/util.h"

#include "engines/grim/grim.h"
#include "engines/grim/emi/nodes/cnode.h"
#include "engines/grim/emi/nodes/cnoderef.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/math/singlemath.h"

namespace Grim {

static const float kMinLength = 1e-20f;
static const float kPi = 3.14159274f;
static const float kHalfTurnDegrees = 180.0f;

CNode::CNode() {
	_bindRot.setIdentity();
	_bindPos._x = 0.0f;
	_bindPos._y = 0.0f;
	_bindPos._z = 0.0f;
	_skinRot.setIdentity();
	_skinPos._x = 0.0f;
	_skinPos._y = 0.0f;
	_skinPos._z = 0.0f;
	_scale._z = 0.0f;
	_scale._y = 0.0f;
	_scale._x = 0.0f;
	_pos._z = 0.0f;
	_pos._y = 0.0f;
	_pos._x = 0.0f;
	_eulerDeg._z = 0.0f;
	_eulerDeg._y = 0.0f;
	_eulerDeg._x = 0.0f;
	_world._rot.setIdentity();
	_world._pos._x = 0.0f;
	_world._pos._y = 0.0f;
	_world._pos._z = 0.0f;
	_observerCount = 0;
	_observerHead = nullptr;
	_observerTail = nullptr;
	_pos._x = 0.0f;
	_pos._y = 0.0f;
	_pos._z = 0.0f;
	_quat.setEuler(0.0f, 0.0f, 0.0f);
	_eulerDeg._x = 0.0f;
	_eulerDeg._y = 0.0f;
	_eulerDeg._z = 0.0f;
	_nodeFlags = 15;
	_childCount = 0;
	_prevSibling = nullptr;
	_nextSibling = nullptr;
	_firstChild = nullptr;
	_parent = nullptr;
	_name[0] = 0;
	_scale._x = 1.0f;
	_scale._y = 1.0f;
	_scale._z = 1.0f;
}

CNode::~CNode() {
	CNodeRef *observer = _observerHead;

	while (observer) {
		CNodeRef *current = observer;

		observer = observer->_next;
		current->onNodeDestroyed();
		current->_node = nullptr;
	}

	detachFromParent();
}

void *CNode::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void CNode::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

void CNode::updateLocalState() {
	if ((_nodeFlags & kWorldValid) != 0)
		updateChildrenLocalTransforms();

	if ((_nodeFlags & kPositionValid) == 0)
		computeLocalPositionFromWorld();

	if ((_nodeFlags & (kEulerValid | kQuaternionValid)) == 0)
		updateLocalQuaternion();

	_nodeFlags = (_nodeFlags & ~(kWorldValid | kLocalSet)) | kLocalSet;
}

CQuaternion CNode::getWorldOrientation() {
	if (usesSingleMath())
		return getWorldOrientationPs2();

	CQuaternion orientation;

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	_world._rot.toQuaternion(&orientation);

	return orientation;
}

void CNode::setLocalPosition(const CVector &position) {
	_pos = position;
	_nodeFlags |= kPositionValid | kLocalSet;
	updateLocalState();
}

void CNode::setLocalTransform(const CVector &position, const CQuaternion &rotation) {
	_pos = position;
	_nodeFlags |= kPositionValid | kLocalSet;
	updateLocalState();

	_quat._x = rotation._x;
	_nodeFlags = (_nodeFlags & ~(kEulerValid | kQuaternionValid | kLocalSet)) | kQuaternionValid | kLocalSet;
	_quat._y = rotation._y;
	_quat._z = rotation._z;
	_quat._w = rotation._w;
	updateLocalState();
}

void CNode::localToWorld(CVector *point) {
	if (usesSingleMath()) {
		localToWorldPs2(point, point);

		return;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;
	float newY = (float)((double)m[7] * point->_z + (double)m[1] * point->_x + (double)m[4] * point->_y);
	float newZ = (float)((double)m[8] * point->_z + (double)m[2] * point->_x + (double)m[5] * point->_y);
	double newX = (double)m[6] * point->_z + (double)m[3] * point->_y + (double)m[0] * point->_x + _world._pos._x;
	float storedY = (float)((double)newY + _world._pos._y);
	float storedZ = (float)((double)newZ + _world._pos._z);

	point->_y = storedY;
	point->_x = (float)newX;
	point->_z = storedZ;
}

CVector *CNode::localToWorld(const CVector *point, CVector *out) {
	if (usesSingleMath()) {
		localToWorldPs2(point, out);

		return out;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;
	float newY = (float)((double)m[1] * point->_x + (double)m[7] * point->_z + (double)m[4] * point->_y);
	float newZ = (float)((double)m[2] * point->_x + (double)m[8] * point->_z + (double)m[5] * point->_y);
	double sumY = (double)newY + _world._pos._y;
	float storedZ = (float)((double)newZ + _world._pos._z);

	out->_x = (float)((double)m[6] * point->_z + (double)m[3] * point->_y + (double)m[0] * point->_x + _world._pos._x);
	out->_z = storedZ;
	out->_y = (float)sumY;

	return out;
}

float *CNode::worldToLocal(const float *worldX, const float *worldY, const float *worldZ, float *localX, float *localY, float *localZ) {
	if (usesSingleMath()) {
		worldToLocalPs2(worldX, worldY, worldZ, localX, localY, localZ);

		return localX;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;

	*localX = *worldX - _world._pos._x;
	*localY = *worldY - _world._pos._y;

	double deltaZ = (double)*worldZ - _world._pos._z;

	*localZ = (float)deltaZ;

	float storedX = *localX;
	float storedY = *localY;

	*localX = (float)(deltaZ * m[2] + (double)*localY * m[1] + (double)storedX * m[0]);
	*localY = (float)(deltaZ * m[5] + (double)storedY * m[4] + (double)storedX * m[3]);
	*localZ = (float)(deltaZ * m[8] + (double)storedY * m[7] + (double)storedX * m[6]);

	return localX;
}

CVector *CNode::worldToLocal(CVector *point) {
	if (usesSingleMath()) {
		worldToLocalPs2(point, point);

		return point;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;
	double deltaX = (double)point->_x - _world._pos._x;
	float deltaY = (float)((double)point->_y - _world._pos._y);
	double deltaZ = (double)point->_z - _world._pos._z;
	float deltaZStored = (float)deltaZ;
	float newY = (float)(deltaX * m[3] + (double)deltaZStored * m[5] + (double)deltaY * m[4]);
	double newX = deltaZ * m[2] + (double)deltaY * m[1] + deltaX * m[0];
	double partialZ = deltaX * m[6] + (double)deltaZStored * m[8];
	double partialY = (double)deltaY * m[7];

	point->_y = newY;
	float newZ = (float)(partialZ + partialY);
	point->_x = (float)newX;
	point->_z = newZ;

	return point;
}

CVector *CNode::worldToLocal(const CVector *point, CVector *out) {
	if (usesSingleMath()) {
		worldToLocalPs2(point, out);

		return out;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;
	double deltaX = (double)point->_x - _world._pos._x;
	float deltaY = (float)((double)point->_y - _world._pos._y);
	double deltaZ = (double)point->_z - _world._pos._z;
	float deltaZStored = (float)deltaZ;
	float newX = (float)(deltaZ * m[2] + (double)deltaY * m[1] + deltaX * m[0]);
	double newY = deltaX * m[3] + (double)deltaZStored * m[5] + (double)deltaY * m[4];
	float newZ = (float)(deltaX * m[6] + (double)deltaZStored * m[8] + (double)deltaY * m[7]);

	out->_x = newX;
	out->_z = newZ;
	out->_y = (float)newY;

	return out;
}

CVector *CNode::localToWorldDirection(const CVector *direction, CVector *out) {
	if (usesSingleMath()) {
		localToWorldDirectionPs2(direction, out);

		return out;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;
	double newY = (double)m[1] * direction->_x + (double)m[7] * direction->_z + (double)m[4] * direction->_y;
	float newZ = (float)((double)m[2] * direction->_x + (double)m[8] * direction->_z + (double)m[5] * direction->_y);

	out->_x = (float)((double)m[6] * direction->_z + (double)m[3] * direction->_y + (double)m[0] * direction->_x);
	out->_z = newZ;
	out->_y = (float)newY;

	return out;
}

bool CNode::usesSingleMath() const {
	return g_grim && g_grim->getGamePlatform() == Common::kPlatformPS2;
}

void CNode::updateWorldTransformPs2() {
	if ((_nodeFlags & kQuaternionValid) != 0) {
		SingleMath::matrixFromQuaternion(_quat._x, _quat._y, _quat._z, _quat._w, _world._rot._m);
		_world._pos = _pos;
	} else if ((_nodeFlags & kEulerValid) != 0) {
		SingleMath::matrixFromEuler(_eulerDeg._x, _eulerDeg._y, _eulerDeg._z, _world._rot._m);
		_world._pos = _pos;
	}

	if (_parent) {
		if ((_parent->_nodeFlags & kWorldValid) == 0)
			_parent->updateWorldTransform();

		const float *parentMatrix = _parent->_world._rot._m;
		float localPosition[3] = { _world._pos._x, _world._pos._y, _world._pos._z };
		float rotatedPosition[3];
		float product[9];

		SingleMath::rotate(parentMatrix, localPosition, rotatedPosition);

		TruncFloat worldX = TruncFloat(rotatedPosition[0]) + TruncFloat(_parent->_world._pos._x);
		TruncFloat worldY = TruncFloat(rotatedPosition[1]) + TruncFloat(_parent->_world._pos._y);
		TruncFloat worldZ = TruncFloat(rotatedPosition[2]) + TruncFloat(_parent->_world._pos._z);

		SingleMath::multiplyMatrices(_world._rot._m, parentMatrix, product);

		for (int i = 0; i < 9; i++)
			_world._rot._m[i] = product[i];

		_world._pos._x = worldX.toFloat();
		_world._pos._y = worldY.toFloat();
		_world._pos._z = worldZ.toFloat();
	}

	_nodeFlags |= kWorldValid;
}

void CNode::computeParentRelativeMatrixPs2(float *matrix) {
	const float *parentMatrix = _parent->_world._rot._m;
	float transposed[9];

	transposed[0] = parentMatrix[0];
	transposed[1] = parentMatrix[3];
	transposed[2] = parentMatrix[6];
	transposed[3] = parentMatrix[1];
	transposed[4] = parentMatrix[4];
	transposed[5] = parentMatrix[7];
	transposed[6] = parentMatrix[2];
	transposed[7] = parentMatrix[5];
	transposed[8] = parentMatrix[8];

	SingleMath::multiplyMatrices(_world._rot._m, transposed, matrix);
}

void CNode::computeLocalMatrixFromWorldPs2() {
	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	float euler[3];

	if (_parent) {
		float local[9];

		computeParentRelativeMatrixPs2(local);
		SingleMath::matrixToEuler(local, euler);
	} else {
		SingleMath::matrixToEuler(_world._rot._m, euler);
	}

	_eulerDeg._x = euler[0];
	_eulerDeg._y = euler[1];
	_eulerDeg._z = euler[2];
	_nodeFlags |= kEulerValid;
}

void CNode::updateLocalQuaternionPs2() {
	float quaternion[4];

	if ((_nodeFlags & kEulerValid) != 0) {
		SingleMath::quaternionFromEuler(_eulerDeg._x, _eulerDeg._y, _eulerDeg._z, quaternion);
	} else {
		if ((_nodeFlags & kWorldValid) == 0)
			updateWorldTransform();

		if (_parent) {
			float local[9];

			computeParentRelativeMatrixPs2(local);
			SingleMath::quaternionFromMatrix(local, quaternion);
		} else {
			SingleMath::quaternionFromMatrix(_world._rot._m, quaternion);
		}
	}

	_quat._x = quaternion[0];
	_quat._y = quaternion[1];
	_quat._z = quaternion[2];
	_quat._w = quaternion[3];
	_nodeFlags |= kQuaternionValid;
}

CQuaternion CNode::getWorldOrientationPs2() {
	CQuaternion orientation;
	float quaternion[4];

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	SingleMath::quaternionFromMatrix(_world._rot._m, quaternion);

	orientation._x = quaternion[0];
	orientation._y = quaternion[1];
	orientation._z = quaternion[2];
	orientation._w = quaternion[3];

	return orientation;
}

void CNode::localToWorldPs2(const CVector *point, CVector *out) {
	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	float source[3] = { point->_x, point->_y, point->_z };
	float rotated[3];

	SingleMath::rotate(_world._rot._m, source, rotated);

	out->_x = (TruncFloat(rotated[0]) + TruncFloat(_world._pos._x)).toFloat();
	out->_y = (TruncFloat(rotated[1]) + TruncFloat(_world._pos._y)).toFloat();
	out->_z = (TruncFloat(rotated[2]) + TruncFloat(_world._pos._z)).toFloat();
}

void CNode::worldToLocalPs2(const float *worldX, const float *worldY, const float *worldZ, float *localX, float *localY, float *localZ) {
	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;

	*localX = (TruncFloat(*worldX) - TruncFloat(_world._pos._x)).toFloat();
	*localY = (TruncFloat(*worldY) - TruncFloat(_world._pos._y)).toFloat();
	*localZ = (TruncFloat(*worldZ) - TruncFloat(_world._pos._z)).toFloat();

	TruncFloat deltaX(*localX);
	TruncFloat deltaY(*localY);
	TruncFloat deltaZ(*localZ);

	*localX = TruncFloat::multiplyAdd(deltaX * TruncFloat(m[0]) + deltaY * TruncFloat(m[1]), deltaZ, TruncFloat(m[2])).toFloat();
	*localY = TruncFloat::multiplyAdd(deltaX * TruncFloat(m[3]) + deltaY * TruncFloat(m[4]), deltaZ, TruncFloat(m[5])).toFloat();
	*localZ = TruncFloat::multiplyAdd(deltaX * TruncFloat(m[6]) + deltaY * TruncFloat(m[7]), deltaZ, TruncFloat(m[8])).toFloat();
}

void CNode::worldToLocalPs2(const CVector *point, CVector *out) {
	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;
	TruncFloat deltaX = TruncFloat(point->_x) - TruncFloat(_world._pos._x);
	TruncFloat deltaY = TruncFloat(point->_y) - TruncFloat(_world._pos._y);
	TruncFloat deltaZ = TruncFloat(point->_z) - TruncFloat(_world._pos._z);
	TruncFloat newX = TruncFloat::multiplyAdd(deltaX * TruncFloat(m[0]) + deltaY * TruncFloat(m[1]), deltaZ, TruncFloat(m[2]));
	TruncFloat newY = TruncFloat::multiplyAdd(deltaX * TruncFloat(m[3]) + deltaY * TruncFloat(m[4]), deltaZ, TruncFloat(m[5]));
	TruncFloat newZ = TruncFloat::multiplyAdd(deltaX * TruncFloat(m[6]) + deltaY * TruncFloat(m[7]), deltaZ, TruncFloat(m[8]));

	out->_x = newX.toFloat();
	out->_y = newY.toFloat();
	out->_z = newZ.toFloat();
}

void CNode::localToWorldDirectionPs2(const CVector *direction, CVector *out) {
	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	float source[3] = { direction->_x, direction->_y, direction->_z };
	float rotated[3];

	SingleMath::rotate(_world._rot._m, source, rotated);

	out->_x = rotated[0];
	out->_y = rotated[1];
	out->_z = rotated[2];
}

void CNode::lookAtPs2(const CVector &target) {
	float localTarget[3] = { target._x, target._y, target._z };

	if (_parent) {
		if ((_parent->_nodeFlags & kWorldValid) == 0)
			_parent->updateWorldTransform();

		const float *p = _parent->_world._rot._m;
		TruncFloat deltaX = TruncFloat(localTarget[0]) - TruncFloat(_parent->_world._pos._x);
		TruncFloat deltaY = TruncFloat(localTarget[1]) - TruncFloat(_parent->_world._pos._y);
		TruncFloat deltaZ = TruncFloat(localTarget[2]) - TruncFloat(_parent->_world._pos._z);
		TruncFloat newX = TruncFloat::multiplyAdd(deltaX * TruncFloat(p[0]) + deltaY * TruncFloat(p[1]), deltaZ, TruncFloat(p[2]));
		TruncFloat newY = TruncFloat::multiplyAdd(deltaX * TruncFloat(p[3]) + deltaY * TruncFloat(p[4]), deltaZ, TruncFloat(p[5]));
		TruncFloat newZ = TruncFloat::multiplyAdd(deltaX * TruncFloat(p[6]) + deltaY * TruncFloat(p[7]), deltaZ, TruncFloat(p[8]));

		localTarget[0] = newX.toFloat();
		localTarget[1] = newY.toFloat();
		localTarget[2] = newZ.toFloat();
	}

	if ((_nodeFlags & kPositionValid) == 0)
		computeLocalPositionFromWorld();

	TruncFloat directionX = TruncFloat(localTarget[0]) - TruncFloat(_pos._x);
	TruncFloat directionY = TruncFloat(localTarget[1]) - TruncFloat(_pos._y);
	TruncFloat directionZ = TruncFloat(localTarget[2]) - TruncFloat(_pos._z);
	TruncFloat directionLength = TruncFloat::squareRoot(TruncFloat::multiplyAdd(directionX * directionX + directionY * directionY, directionZ, directionZ));

	if (!(directionLength < TruncFloat(SingleMath::kMinLength))) {
		TruncFloat inverse = TruncFloat(1.0f) / directionLength;

		directionX = directionX * inverse;
		directionY = directionY * inverse;
		directionZ = directionZ * inverse;
	}

	TruncFloat toRadians(SingleMath::kDegreesToRadians);
	TruncFloat toDegrees = TruncFloat::fromBits(0x42652EE0);
	TruncFloat yaw = toDegrees * (toRadians * TruncFloat(SingleMath::atan2Degrees(directionX.toFloat(), directionZ.toFloat())));
	TruncFloat pitch = toDegrees * -(toRadians * TruncFloat(SingleMath::asinDegrees(directionY.toFloat())));

	_eulerDeg._x = pitch.toFloat();
	_eulerDeg._y = yaw.toFloat();
	_eulerDeg._z = 0.0f;
	_nodeFlags = (_nodeFlags | kEulerValid | kLocalSet) & ~kQuaternionValid;
	updateLocalState();
}

void CNode::updateWorldTransform() {
	if (usesSingleMath()) {
		updateWorldTransformPs2();

		return;
	}

	CMatrix rotation;

	if ((_nodeFlags & kQuaternionValid) != 0) {
		rotation.setFromQuaternion(_quat);
		_world._rot = rotation;
		_world._pos._x = _pos._x;
		_world._pos._y = _pos._y;
		_world._pos._z = _pos._z;
	} else if ((_nodeFlags & kEulerValid) != 0) {
		rotation = *rotation.setFromEuler(_eulerDeg);
		_world._rot = rotation;
		_world._pos._x = _pos._x;
		_world._pos._y = _pos._y;
		_world._pos._z = _pos._z;
	}

	if (_parent) {
		if ((_parent->_nodeFlags & kWorldValid) == 0)
			_parent->updateWorldTransform();

		const float *p = _parent->_world._rot._m;
		const float *w = _world._rot._m;
		const CVector &wp = _world._pos;
		const CVector &pp = _parent->_world._pos;
		float posY = (float)((double)p[1] * wp._x + (double)wp._z * p[7] + (double)wp._y * p[4]);
		float posZ = (float)((double)wp._z * p[8] + (double)wp._y * p[5] + (double)wp._x * p[2]);
		double posX = (double)wp._z * p[6] + (double)p[0] * wp._x + (double)wp._y * p[3] + pp._x;
		float worldY = (float)((double)posY + pp._y);
		float worldZ = (float)((double)posZ + pp._z);

		rotation._m[0] = (float)((double)p[0] * w[0] + (double)p[6] * w[2] + (double)p[3] * w[1]);
		rotation._m[1] = (float)((double)p[1] * w[0] + (double)p[7] * w[2] + (double)p[4] * w[1]);
		rotation._m[2] = (float)((double)p[8] * w[2] + (double)p[5] * w[1] + (double)w[0] * p[2]);
		rotation._m[3] = (float)((double)p[6] * w[5] + (double)w[4] * p[3] + (double)w[3] * p[0]);
		rotation._m[4] = (float)((double)w[3] * p[1] + (double)w[5] * p[7] + (double)w[4] * p[4]);
		rotation._m[5] = (float)((double)w[5] * p[8] + (double)w[4] * p[5] + (double)w[3] * p[2]);
		rotation._m[6] = (float)((double)w[7] * p[3] + (double)p[0] * w[6] + (double)p[6] * w[8]);
		rotation._m[7] = (float)((double)p[7] * w[8] + (double)p[4] * w[7] + (double)w[6] * p[1]);
		rotation._m[8] = (float)((double)w[8] * p[8] + (double)w[7] * p[5] + (double)w[6] * p[2]);

		_world._rot = rotation;
		_world._pos._x = (float)posX;
		_world._pos._y = worldY;
		_world._pos._z = worldZ;
		_nodeFlags |= kWorldValid;
	} else {
		_nodeFlags |= kWorldValid;
	}
}

void CNode::computeLocalMatrixFromWorld() {
	if (usesSingleMath()) {
		computeLocalMatrixFromWorldPs2();

		return;
	}

	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	if (_parent) {
		const float *p = _parent->_world._rot._m;
		const float *w = _world._rot._m;
		CMatrix local;

		local._m[0] = (float)((double)p[2] * w[2] + (double)p[1] * w[1] + (double)p[0] * w[0]);
		local._m[1] = (float)((double)p[5] * w[2] + (double)p[4] * w[1] + (double)p[3] * w[0]);
		local._m[2] = (float)((double)p[8] * w[2] + (double)p[7] * w[1] + (double)p[6] * w[0]);
		local._m[3] = (float)((double)p[0] * w[3] + (double)p[2] * w[5] + (double)p[1] * w[4]);
		local._m[4] = (float)((double)p[3] * w[3] + (double)p[5] * w[5] + (double)p[4] * w[4]);
		local._m[5] = (float)((double)p[6] * w[3] + (double)p[8] * w[5] + (double)p[7] * w[4]);
		local._m[6] = (float)((double)p[0] * w[6] + (double)p[2] * w[8] + (double)p[1] * w[7]);
		local._m[7] = (float)((double)p[3] * w[6] + (double)p[5] * w[8] + (double)p[4] * w[7]);
		local._m[8] = (float)((double)p[6] * w[6] + (double)p[8] * w[8] + (double)p[7] * w[7]);
		local.toEuler(&_eulerDeg);
	} else {
		_world._rot.toEuler(&_eulerDeg);
	}

	_nodeFlags |= kEulerValid;
}

void CNode::updateLocalQuaternion() {
	if (usesSingleMath()) {
		updateLocalQuaternionPs2();

		return;
	}

	if ((_nodeFlags & kEulerValid) != 0) {
		_quat = CQuaternion(_eulerDeg);
		_nodeFlags |= kQuaternionValid;
	} else {
		if ((_nodeFlags & kWorldValid) == 0)
			updateWorldTransform();

		if (_parent) {
			const float *p = _parent->_world._rot._m;
			const float *w = _world._rot._m;
			CMatrix local;

			local._m[0] = (float)((double)p[0] * w[0] + (double)p[2] * w[2] + (double)p[1] * w[1]);
			local._m[1] = (float)((double)p[3] * w[0] + (double)p[5] * w[2] + (double)p[4] * w[1]);
			local._m[2] = (float)((double)p[6] * w[0] + (double)p[8] * w[2] + (double)p[7] * w[1]);
			local._m[3] = (float)((double)p[0] * w[3] + (double)p[2] * w[5] + (double)p[1] * w[4]);
			local._m[4] = (float)((double)p[3] * w[3] + (double)p[5] * w[5] + (double)p[4] * w[4]);
			local._m[5] = (float)((double)p[6] * w[3] + (double)p[8] * w[5] + (double)p[7] * w[4]);
			local._m[6] = (float)((double)p[2] * w[8] + (double)p[1] * w[7] + (double)p[0] * w[6]);
			local._m[7] = (float)((double)p[5] * w[8] + (double)p[4] * w[7] + (double)p[3] * w[6]);
			local._m[8] = (float)((double)p[8] * w[8] + (double)p[7] * w[7] + (double)p[6] * w[6]);
			local.toQuaternion(&_quat);
		} else {
			_world._rot.toQuaternion(&_quat);
		}

		_nodeFlags |= kQuaternionValid;
	}
}

void CNode::computeLocalPositionFromWorld() {
	_pos = _world._pos;

	if (_parent && usesSingleMath()) {
		if ((_parent->_nodeFlags & kWorldValid) == 0)
			_parent->updateWorldTransform();

		TruncFloat deltaX = TruncFloat(_pos._x) - TruncFloat(_parent->_world._pos._x);
		TruncFloat deltaY = TruncFloat(_pos._y) - TruncFloat(_parent->_world._pos._y);
		TruncFloat deltaZ = TruncFloat(_pos._z) - TruncFloat(_parent->_world._pos._z);
		const float *p = _parent->_world._rot._m;

		_pos._x = TruncFloat::multiplyAdd(deltaX * TruncFloat(p[0]) + deltaY * TruncFloat(p[1]), deltaZ, TruncFloat(p[2])).toFloat();
		_pos._y = TruncFloat::multiplyAdd(deltaX * TruncFloat(p[3]) + deltaY * TruncFloat(p[4]), deltaZ, TruncFloat(p[5])).toFloat();
		_pos._z = TruncFloat::multiplyAdd(deltaX * TruncFloat(p[6]) + deltaY * TruncFloat(p[7]), deltaZ, TruncFloat(p[8])).toFloat();
	} else if (_parent) {
		if ((_parent->_nodeFlags & kWorldValid) == 0)
			_parent->updateWorldTransform();

		const float *p = _parent->_world._rot._m;
		double deltaX = (double)_pos._x - _parent->_world._pos._x;
		float deltaY = (float)((double)_pos._y - _parent->_world._pos._y);
		double deltaZ = (double)_pos._z - _parent->_world._pos._z;
		float deltaZStored = (float)deltaZ;
		double newX = deltaZ * p[2] + (double)deltaY * p[1] + deltaX * p[0];
		float newY = (float)((double)deltaZStored * p[5] + deltaX * p[3] + (double)deltaY * p[4]);
		double partialZ = (double)deltaZStored * p[8] + deltaX * p[6];
		double partialY = (double)deltaY * p[7];

		_pos._y = newY;
		float newZ = (float)(partialZ + partialY);
		_pos._x = (float)newX;
		_pos._z = newZ;
	}

	_nodeFlags |= kPositionValid;
}

void CNode::updateChildrenLocalTransforms() {
	for (CNode *child = _firstChild; child; child = child->_nextSibling)
		child->updateLocalState();
}

void CNode::setParent(CNode *newParent) {
	if (!newParent) {
		warning("CNode::setParent(): null parent");

		return;
	}

	if (_parent)
		detachFromParent();

	_parent = newParent;
	_prevSibling = nullptr;
	_nextSibling = newParent->_firstChild;

	if (_nextSibling)
		_nextSibling->_prevSibling = this;

	_parent->_childCount++;
	_parent->_firstChild = this;
	updateLocalState();
}

void CNode::detachFromParent() {
	if (!_parent)
		return;

	_parent->_childCount--;

	if (!_prevSibling)
		_parent->_firstChild = _nextSibling;

	if (_prevSibling)
		_prevSibling->_nextSibling = _nextSibling;

	if (_nextSibling)
		_nextSibling->_prevSibling = _prevSibling;

	_nextSibling = nullptr;
	_prevSibling = nullptr;
	_parent = nullptr;
	updateLocalState();
}

CNode *CNode::findNode(const char *nodeName) {
	CNode *node = this;

	if (!scumm_strnicmp(_name, nodeName, kNameSize))
		return node;

	while (true) {
		if (node->_firstChild) {
			CNode *result = node->_firstChild->findNode(nodeName);

			if (result)
				return result;
		}

		node = node->_nextSibling;

		if (!node)
			return nullptr;

		if (!scumm_strnicmp(node->_name, nodeName, kNameSize))
			return node;
	}
}

CNode *CNode::findNode(const LString &nodeName) {
	return findNode(nodeName.c_str());
}

void CNode::lookAt(const CVector &target) {
	if (usesSingleMath()) {
		lookAtPs2(target);

		return;
	}

	CVector localTarget = target;

	if (_parent) {
		if ((_parent->_nodeFlags & kWorldValid) == 0)
			_parent->updateWorldTransform();

		const float *p = _parent->_world._rot._m;
		double deltaX = (double)localTarget._x - _parent->_world._pos._x;
		float deltaY = (float)((double)localTarget._y - _parent->_world._pos._y);
		double deltaZ = (double)localTarget._z - _parent->_world._pos._z;
		float deltaZStored = (float)deltaZ;
		float newY = (float)((double)deltaZStored * p[5] + (double)deltaY * p[4] + deltaX * p[3]);
		float newZ = (float)((double)deltaZStored * p[8] + (double)deltaY * p[7] + deltaX * p[6]);

		localTarget._y = newY;
		localTarget._x = (float)(deltaZ * p[2] + (double)deltaY * p[1] + deltaX * p[0]);
		localTarget._z = newZ;
	}

	if ((_nodeFlags & kPositionValid) == 0)
		computeLocalPositionFromWorld();

	CVector direction;

	direction._x = localTarget._x - _pos._x;
	direction._y = localTarget._y - _pos._y;
	direction._z = localTarget._z - _pos._z;

	CVector unit = direction;
	double length = sqrt((double)direction._z * direction._z + (double)direction._y * direction._y + (double)direction._x * direction._x);

	if (length >= (double)kMinLength) {
		double inverse = 1.0 / length;

		unit._x = (float)(direction._x * inverse);
		unit._y = (float)(inverse * direction._y);
		unit._z = (float)(inverse * direction._z);
	}

	float factor = (float)((double)kHalfTurnDegrees / kPi);

	_eulerDeg._z = 0.0f;

	float yaw = (float)(atan2((double)unit._x, (double)unit._z) * factor);

	_eulerDeg._x = (float)(-asin((double)unit._y) * factor);
	_eulerDeg._y = yaw;
	_nodeFlags = (_nodeFlags & ~(kEulerValid | kQuaternionValid | kLocalSet)) | kEulerValid | kLocalSet;
	updateLocalState();
}

} // End of namespace Grim
