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

#ifndef GRIM_EMI_CNODE_H
#define GRIM_EMI_CNODE_H

#include "engines/grim/emi/math/cmatrix.h"
#include "engines/grim/emi/math/cquaternion.h"
#include "engines/grim/emi/math/ctransform.h"
#include "engines/grim/emi/math/cvector.h"
#include "engines/grim/emi/files/lfile.h"

namespace Grim {

class CNodeRef;

class CNode {
public:
	enum {
		kNameSize = 32
	};

	enum {
		kWorldValid = 0x01,
		kEulerValid = 0x02,
		kQuaternionValid = 0x04,
		kPositionValid = 0x08,
		kLocalSet = 0x10
	};

	CNode();
	virtual ~CNode();

	void *operator new(size_t size);
	void operator delete(void *block);

	virtual void setLocalTransform(const CVector &position, const CQuaternion &rotation);
	virtual void setLocalPosition(const CVector &position);

	CQuaternion getWorldOrientation();
	void updateWorldTransform();
	void updateWorldTransformPs2();
	bool usesSingleMath() const;
	void computeParentRelativeMatrixPs2(float *matrix);
	void computeLocalMatrixFromWorldPs2();
	void updateLocalQuaternionPs2();
	CQuaternion getWorldOrientationPs2();
	void localToWorldPs2(const CVector *point, CVector *out);
	void worldToLocalPs2(const float *worldX, const float *worldY, const float *worldZ, float *localX, float *localY, float *localZ);
	void worldToLocalPs2(const CVector *point, CVector *out);
	void localToWorldDirectionPs2(const CVector *direction, CVector *out);
	void lookAtPs2(const CVector &target);
	void localToWorld(CVector *point);
	CVector *localToWorld(const CVector *point, CVector *out);
	float *worldToLocal(const float *worldX, const float *worldY, const float *worldZ, float *localX, float *localY, float *localZ);
	CVector *worldToLocal(CVector *point);
	CVector *worldToLocal(const CVector *point, CVector *out);
	CVector *localToWorldDirection(const CVector *direction, CVector *out);
	void computeLocalMatrixFromWorld();
	void updateLocalQuaternion();
	void computeLocalPositionFromWorld();
	void updateChildrenLocalTransforms();
	void setParent(CNode *newParent);
	void detachFromParent();
	CNode *findNode(const char *nodeName);
	CNode *findNode(const LString &nodeName);
	void lookAt(const CVector &target);

	void updateLocalState();

	CMatrix _bindRot;
	CVector _bindPos;
	CMatrix _skinRot;
	CVector _skinPos;
	char _name[kNameSize];
	CVector _scale;
	uint _nodeFlags;
	CVector _pos;
	CQuaternion _quat;
	CVector _eulerDeg;
	CTransform _world;
	int _observerCount;
	CNodeRef *_observerHead;
	CNodeRef *_observerTail;
	int _childCount;
	CNode *_parent;
	CNode *_firstChild;
	CNode *_nextSibling;
	CNode *_prevSibling;
};

} // End of namespace Grim

#endif
