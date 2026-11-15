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

#include "engines/grim/emi/math/cmatrix.h"

namespace Grim {

static const float kPi = 3.14159274f;
static const float kDegreesToRadians = 3.14159274f * 0.00555555569f;
static const float kHalfTurnDegrees = 180.0f;
static const double kGimbalLimit = 1.1920928955078125e-7;

void CMatrix::setIdentity() {
	_m[0] = 1.0f;
	_m[1] = 0.0f;
	_m[2] = 0.0f;
	_m[3] = 0.0f;
	_m[4] = 1.0f;
	_m[5] = 0.0f;
	_m[6] = 0.0f;
	_m[7] = 0.0f;
	_m[8] = 1.0f;
}

CMatrix *CMatrix::setFromEuler(const CVector &eulerDegrees) {
	_m[0] = 1.0f;
	_m[4] = 1.0f;
	_m[8] = 1.0f;
	_m[1] = 0.0f;
	_m[2] = 0.0f;
	_m[3] = 0.0f;
	_m[5] = 0.0f;
	_m[6] = 0.0f;
	_m[7] = 0.0f;

	double halfX = (double)kDegreesToRadians * eulerDegrees._x * 0.5;
	float cosX = (float)cos(halfX);
	double halfY = (double)kDegreesToRadians * eulerDegrees._y * 0.5;
	float halfYStored = (float)halfY;
	float cosY = (float)cos(halfY);
	double halfZ = (double)kDegreesToRadians * eulerDegrees._z * 0.5;
	float halfZStored = (float)halfZ;
	float cosZ = (float)cos(halfZ);
	double sinX = sin(halfX);
	double sinY = sin(halfYStored);
	double sinZ = sin(halfZStored);
	float cosZY = (float)((double)cosZ * cosY);
	float sinZY = (float)(sinZ * sinY);

	float quatW = (float)(sinZ * sinY * sinX + (double)cosZY * cosX);
	float quatZ = (float)(sinZ * cosY * cosX - sinY * sinX * cosZ);
	float quatY = (float)(sinZ * sinX * cosY + sinY * cosZ * cosX);
	float quatX = (float)((double)cosZY * sinX - (double)sinZY * cosX);

	CQuaternion quaternion(quatX, quatY, quatZ, quatW);

	setFromQuaternion(quaternion);

	return this;
}

void CMatrix::toEuler(CVector *eulerDegrees) {
	float sine = -_m[2];

	if (sine <= 1.0f) {
		if (sine < -1.0f)
			sine = -1.0f;
	} else {
		sine = 1.0f;
	}

	double pitch = asin(sine);

	eulerDegrees->_y = (float)pitch;

	if (fabs(cos(pitch)) <= kGimbalLimit) {
		eulerDegrees->_x = (float)atan2(_m[3], _m[4]);
		eulerDegrees->_z = 0.0f;
	} else {
		eulerDegrees->_x = (float)atan2(_m[5], _m[8]);
		eulerDegrees->_z = (float)atan2(_m[1], _m[0]);
	}

	double factor = (double)kHalfTurnDegrees / kPi;

	eulerDegrees->_x = (float)(factor * eulerDegrees->_x);
	eulerDegrees->_y = (float)(pitch * factor);
	eulerDegrees->_z = (float)(factor * eulerDegrees->_z);
}

void CMatrix::toQuaternion(CQuaternion *quaternion) {
	double sumYZ = (double)_m[4] + _m[8];
	float sumYZStored = (float)sumYZ;
	double trace = sumYZ + _m[0];

	// The original could divide by zero below, let's avoid that...
	if (trace >= 0.0) {
		double root = sqrt(trace + 1.0);

		quaternion->_w = (float)(root * 0.5);

		double scale = root != 0.0 ? 0.5 / root : 0.0;

		quaternion->_x = (float)(((double)_m[5] - _m[7]) * scale);
		quaternion->_y = (float)(((double)_m[6] - _m[2]) * scale);
		quaternion->_z = (float)(((double)_m[1] - _m[3]) * scale);
		quaternion->normalize();

		return;
	}

	int largest = _m[4] > (double)_m[0] ? 1 : 0;

	if (_m[8] > (double)_m[4 * largest])
		largest = 2;

	if (largest == 1) {
		double root = sqrt((double)_m[4] - ((double)_m[0] + _m[8]) + 1.0);

		quaternion->_y = (float)(root * 0.5);

		double scale = root != 0.0 ? 0.5 / root : 0.0;

		quaternion->_z = (float)(((double)_m[7] + _m[5]) * scale);
		quaternion->_x = (float)(((double)_m[3] + _m[1]) * scale);
		quaternion->_w = (float)(((double)_m[6] - _m[2]) * scale);
	} else if (largest == 2) {
		double root = sqrt((double)_m[8] - ((double)_m[4] + _m[0]) + 1.0);

		quaternion->_z = (float)(root * 0.5);

		double scale = root != 0.0 ? 0.5 / root : 0.0;

		quaternion->_x = (float)(((double)_m[6] + _m[2]) * scale);
		quaternion->_y = (float)(((double)_m[7] + _m[5]) * scale);
		quaternion->_w = (float)(((double)_m[1] - _m[3]) * scale);
	} else {
		double root = sqrt((double)_m[0] - sumYZStored + 1.0);

		quaternion->_x = (float)(root * 0.5);

		double scale = root != 0.0 ? 0.5 / root : 0.0;

		quaternion->_y = (float)(((double)_m[3] + _m[1]) * scale);
		quaternion->_z = (float)(((double)_m[6] + _m[2]) * scale);
		quaternion->_w = (float)(((double)_m[5] - _m[7]) * scale);
	}

	quaternion->normalize();
}

CMatrix *CMatrix::setFromQuaternion(const CQuaternion &quaternion) {
	double twoX = (double)quaternion._x + quaternion._x;
	float twoY = (float)((double)quaternion._y + quaternion._y);
	float twoZ = (float)((double)quaternion._z + quaternion._z);
	float xx = (float)(twoX * quaternion._x);
	float xy = (float)((double)twoY * quaternion._x);
	float xz = (float)((double)twoZ * quaternion._x);
	float yy = (float)((double)twoY * quaternion._y);
	float yz = (float)((double)twoZ * quaternion._y);
	float zz = (float)((double)twoZ * quaternion._z);
	double xw = twoX * quaternion._w;
	double yw = (double)twoY * quaternion._w;
	double zw = (double)twoZ * quaternion._w;

	_m[0] = (float)(1.0 - ((double)zz + yy));
	_m[1] = (float)(zw + xy);
	_m[2] = (float)(xz - yw);
	_m[3] = (float)(xy - zw);
	_m[4] = (float)(1.0 - ((double)zz + xx));
	_m[5] = (float)(xw + yz);
	_m[6] = (float)(yw + xz);
	_m[7] = (float)(yz - xw);
	_m[8] = (float)(1.0 - ((double)yy + xx));

	return this;
}

CMatrix *CMatrix::transpose(CMatrix *out) const {
	float result[9];

	result[0] = _m[0];
	result[1] = _m[3];
	result[2] = _m[6];
	result[3] = _m[1];
	result[4] = _m[4];
	result[5] = _m[7];
	result[6] = _m[2];
	result[7] = _m[5];
	result[8] = _m[8];

	for (int i = 0; i < 9; i++)
		out->_m[i] = result[i];

	return out;
}

CMatrix *CMatrix::multiply(CMatrix *out, const CMatrix *other) const {
	float result[9];

	result[0] = (float)((double)_m[2] * other->_m[6] + (double)other->_m[3] * _m[1] + (double)other->_m[0] * _m[0]);
	result[1] = (float)((double)_m[2] * other->_m[7] + (double)other->_m[4] * _m[1] + (double)other->_m[1] * _m[0]);
	result[2] = (float)((double)other->_m[2] * _m[0] + (double)_m[2] * other->_m[8] + (double)_m[1] * other->_m[5]);
	result[3] = (float)((double)other->_m[3] * _m[4] + (double)_m[3] * other->_m[0] + (double)other->_m[6] * _m[5]);
	result[4] = (float)((double)other->_m[7] * _m[5] + (double)other->_m[4] * _m[4] + (double)other->_m[1] * _m[3]);
	result[5] = (float)((double)other->_m[8] * _m[5] + (double)other->_m[5] * _m[4] + (double)other->_m[2] * _m[3]);
	result[6] = (float)((double)_m[8] * other->_m[6] + (double)_m[7] * other->_m[3] + (double)other->_m[0] * _m[6]);
	result[7] = (float)((double)_m[8] * other->_m[7] + (double)_m[7] * other->_m[4] + (double)other->_m[1] * _m[6]);
	result[8] = (float)((double)other->_m[2] * _m[6] + (double)_m[8] * other->_m[8] + (double)_m[7] * other->_m[5]);

	for (int i = 0; i < 9; i++)
		out->_m[i] = result[i];

	return out;
}

} // End of namespace Grim
