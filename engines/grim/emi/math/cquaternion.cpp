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

#include "engines/grim/emi/math/cquaternion.h"

namespace Grim {

static const float kDegreesToRadians = 3.14159274f * 0.00555555569f;
static const float kMinLength = 1e-20f;
static const double kSlerpLinearLimit = 0.000001;

CQuaternion::CQuaternion() {
	_w = 0.0f;
	_z = 0.0f;
	_y = 0.0f;
	_x = 0.0f;
}

CQuaternion::CQuaternion(float newX, float newY, float newZ, float newW) {
	_x = newX;
	_y = newY;
	_z = newZ;
	_w = newW;
}

CQuaternion::CQuaternion(const CVector &eulerDegrees) {
	setEuler(eulerDegrees._x, eulerDegrees._y, eulerDegrees._z);
}

CQuaternion::CQuaternion(const CVector &axis, float angle) {
	setAxisAngle(angle, axis);
}

void CQuaternion::setEuler(float eulerX, float eulerY, float eulerZ) {
	double halfX = (double)kDegreesToRadians * eulerX * 0.5;
	float cosX = (float)cos(halfX);
	double halfY = (double)kDegreesToRadians * eulerY * 0.5;
	float halfYStored = (float)halfY;
	float cosY = (float)cos(halfY);
	double halfZ = (double)kDegreesToRadians * eulerZ * 0.5;
	float halfZStored = (float)halfZ;
	float cosZ = (float)cos(halfZ);
	double sinX = sin(halfX);
	double sinY = sin(halfYStored);
	double sinZ = sin(halfZStored);
	double cosZY = (double)cosZ * cosY;

	_w = (float)(sinZ * sinY * sinX + cosZY * cosX);
	_x = (float)(cosZY * sinX - sinZ * sinY * cosX);
	_y = (float)(sinZ * sinX * cosY + sinY * cosZ * cosX);
	_z = (float)(sinZ * cosY * cosX - sinY * sinX * cosZ);
}

CQuaternion *CQuaternion::slerp(const CQuaternion &other, float t, CQuaternion *out) const {
	double dotProduct = (double)other._z * _z + (double)other._y * _y + (double)other._x * _x + (double)_w * other._w;
	float cosine = (float)dotProduct;
	float otherX;
	float otherY;
	float otherZ;
	float otherW;

	if (dotProduct >= 0.0) {
		otherX = other._x;
		otherY = other._y;
		otherZ = other._z;
		otherW = other._w;
	} else {
		cosine = -cosine;
		otherX = -other._x;
		otherY = -other._y;
		otherZ = -other._z;
		otherW = -other._w;
	}

	double scaleThis;
	double scaleOther;

	if (1.0 - cosine <= kSlerpLinearLimit) {
		scaleThis = 1.0 - t;
		scaleOther = t;
	} else {
		double angleWide = acos((double)cosine);
		float angle = (float)angleWide;
		float sine = (float)sin(angleWide);

		scaleThis = sin((1.0 - t) * angle) / sine;
		scaleOther = sin(angle * t) / sine;
	}

	out->_x = (float)(scaleThis * _x + scaleOther * otherX);
	out->_y = (float)(scaleOther * otherY + scaleThis * _y);
	out->_z = (float)(scaleOther * otherZ + scaleThis * _z);
	out->_w = (float)(scaleThis * _w + scaleOther * otherW);

	return out;
}

void CQuaternion::setAxisAngle(float angle, const CVector &axis) {
	double half = angle * 0.5;
	double sine = sin(half);

	_x = (float)(sine * axis._x);
	_y = (float)(sine * axis._y);
	_z = (float)(sine * axis._z);
	_w = (float)cos(half);

	normalize();
}

void CQuaternion::normalize() {
	double length = sqrt((double)_x * _x + (double)_y * _y + (double)_z * _z + (double)_w * _w);

	if (length >= (double)kMinLength) {
		double inverse = 1.0 / length;

		_x = (float)(inverse * _x);
		_y = (float)(inverse * _y);
		_z = (float)(inverse * _z);
		_w = (float)(inverse * _w);
	} else {
		_w = 1.0f;
		_z = 0.0f;
		_y = 0.0f;
		_x = 0.0f;
	}
}

void CQuaternion::invert() {
	double squaredLength = (double)_x * _x + (double)_y * _y + (double)_z * _z + (double)_w * _w;

	if (squaredLength == 0.0) {
		// The original could divide by zero here, let's avoid that...
		return;
	}

	double scale = 1.0 / squaredLength;

	_x = (float)-(scale * _x);
	_y = (float)-(scale * _y);
	_z = (float)-(scale * _z);
	_w = (float)(scale * _w);
}

float CQuaternion::dot(const CQuaternion &other) const {
	return (float)((double)other._w * _w + (double)other._z * _z + (double)other._y * _y + (double)other._x * _x);
}

CQuaternion CQuaternion::operator*(float scale) const {
	float scaledW = scale * _w;
	float scaledZ = scale * _z;
	float scaledY = scale * _y;
	float scaledX = scale * _x;

	return CQuaternion(scaledX, scaledY, scaledZ, scaledW);
}

CQuaternion &CQuaternion::operator+=(const CQuaternion &other) {
	_x = other._x + _x;
	_y = other._y + _y;
	_z = other._z + _z;
	_w = other._w + _w;

	return *this;
}

} // End of namespace Grim
