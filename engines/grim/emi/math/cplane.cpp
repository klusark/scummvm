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

#include "common/scummsys.h"

#include "engines/grim/emi/math/cplane.h"

namespace Grim {

static const float kMinLength = 1e-20f;
static const float kPlaneTolerance = 1e-4f;

void CPlane::setFromNormalAndPoint(const CVector &normal, const CVector &point) {
	_n._x = normal._x;
	_n._y = normal._y;
	_n._z = normal._z;

	double length = sqrt((double)_n._x * _n._x + (double)_n._y * _n._y + (double)normal._z * normal._z);

	if (length >= (double)kMinLength) {
		double inverse = 1.0 / length;

		_n._x = (float)(inverse * _n._x);
		_n._y = (float)(inverse * _n._y);
		_n._z = (float)(inverse * _n._z);
	}

	_d = (float)-((double)point._z * _n._z + (double)point._y * _n._y + (double)point._x * _n._x);
}

void CPlane::projectPoint(CVector *point) {
	double distance = (double)point->_y * _n._y + (double)point->_x * _n._x + (double)_n._z * point->_z + _d;
	float offsetX = (float)(distance * _n._x);
	float offsetY = (float)(distance * _n._y);
	double offsetZ = distance * _n._z;

	point->_x = (float)((double)point->_x - offsetX);
	point->_y = (float)((double)point->_y - offsetY);
	point->_z = (float)((double)point->_z - offsetZ);
}

int CPlane::classifyPoint(const CVector &point) {
	double distance = (double)point._z * _n._z + (double)point._y * _n._y + (double)point._x * _n._x + _d;

	if (distance <= (double)kPlaneTolerance)
		return distance < -(double)kPlaneTolerance ? kPointBehind : kPointOnPlane;

	return kPointInFront;
}

void CPlane::transformToLocal(const CTransform *transform) {
	double negD = -(double)_d;
	float centerX = (float)(negD * _n._x);
	float centerY = (float)(negD * _n._y);
	float localX = (float)((double)centerX - transform->_pos._x);
	float localY = (float)((double)centerY - transform->_pos._y);
	double localZ = negD * _n._z - transform->_pos._z;
	float rotatedX = (float)((double)localX * transform->_rot._m[0] + localZ * transform->_rot._m[2] + (double)localY * transform->_rot._m[1]);
	float rotatedY = (float)((double)localX * transform->_rot._m[3] + localZ * transform->_rot._m[5] + (double)localY * transform->_rot._m[4]);
	double rotatedZ = (double)localX * transform->_rot._m[6] + localZ * transform->_rot._m[8] + (double)localY * transform->_rot._m[7];
	double normalX = _n._x;
	double normalY = _n._y;
	double normalZ = _n._z;

	_n._x = (float)(normalX * transform->_rot._m[0] + normalZ * transform->_rot._m[2] + normalY * transform->_rot._m[1]);
	_n._y = (float)(normalX * transform->_rot._m[3] + normalZ * transform->_rot._m[5] + normalY * transform->_rot._m[4]);
	_n._z = (float)(normalX * transform->_rot._m[6] + normalZ * transform->_rot._m[8] + normalY * transform->_rot._m[7]);
	_d = (float)-(rotatedZ * _n._z + (double)rotatedX * _n._x + (double)rotatedY * _n._y);
}

} // End of namespace Grim
