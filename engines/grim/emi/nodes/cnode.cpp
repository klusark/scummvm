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

#include "engines/grim/emi/nodes/cnode.h"
#include "engines/grim/emi/nodes/cnoderef.h"
#include "engines/grim/emi/core/lmemoryservices.h"

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

void CNode::updateWorldTransform() {
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

	if (_parent) {
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

	float yaw = (float)(atan2(unit._x, unit._z) * factor);

	_eulerDeg._x = (float)(-asin(unit._y) * factor);
	_eulerDeg._y = yaw;
	_nodeFlags = (_nodeFlags & ~(kEulerValid | kQuaternionValid | kLocalSet)) | kEulerValid | kLocalSet;
	updateLocalState();
}

} // End of namespace Grim
