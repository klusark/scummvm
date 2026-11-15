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

#ifndef GRIM_EMI_SINGLEMATH_H
#define GRIM_EMI_SINGLEMATH_H

#include "common/scummsys.h"

namespace Grim {

class TruncFloat {
public:
	TruncFloat() : _value(0) {}
	TruncFloat(float value);

	static TruncFloat fromBits(uint32 bits);
	static TruncFloat fromInt(int value);
	static TruncFloat squareRoot(TruncFloat value);
	static TruncFloat multiplyAdd(TruncFloat accumulator, TruncFloat first, TruncFloat second);
	static TruncFloat multiplySubtract(TruncFloat accumulator, TruncFloat first, TruncFloat second);

	float toFloat() const;
	int toInt() const;
	uint32 bits() const { return _value; }
	TruncFloat absolute() const { return fromBits(_value & 0x7FFFFFFF); }

	TruncFloat operator-() const;
	TruncFloat operator+(TruncFloat other) const;
	TruncFloat operator-(TruncFloat other) const;
	TruncFloat operator*(TruncFloat other) const;
	TruncFloat operator/(TruncFloat other) const;
	TruncFloat &operator+=(TruncFloat other);
	TruncFloat &operator-=(TruncFloat other);
	TruncFloat &operator*=(TruncFloat other);
	TruncFloat &operator/=(TruncFloat other);

	bool operator<(TruncFloat other) const;
	bool operator<=(TruncFloat other) const;
	bool operator>(TruncFloat other) const;
	bool operator>=(TruncFloat other) const;
	bool operator==(TruncFloat other) const;
	bool operator!=(TruncFloat other) const;

private:
	struct Operand {
		bool negative;
		int exponent;
		uint64 mantissa;
	};

	static Operand unpack(uint32 bits);
	static TruncFloat pack(bool negative, uint64 mantissa, int exponent);
	static TruncFloat addOperands(Operand first, Operand second);
	static int topBit(uint64 value);
	static uint64 integerSquareRoot(uint64 value);
	int compare(TruncFloat other) const;

	uint32 _value;
};

class SingleMath {
public:
	static const float kMinLength;
	static const float kDegreesToRadians;
	static const float kRadiansToDegrees;

	static float reduceAngle(float radians);
	static float sinePolynomial(float value);
	static void sinCosDegrees(float degrees, float *sineOut, float *cosineOut);
	static float asinDegrees(float value);
	static float acosDegrees(float value);
	static float dot(const float *first, const float *second);
	static float length(const float *vector);
	static void normalize(float *vector);
	static void rotate(const float *matrix, const float *vector, float *result);
	static void matrixFromEuler(float x, float y, float z, float *matrix);
	static void matrixFromQuaternion(float x, float y, float z, float w, float *matrix);
	static void quaternionFromEuler(float x, float y, float z, float *quaternion);
	static void matrixToEuler(const float *matrix, float *euler);
	static void multiplyMatrices(const float *first, const float *second, float *result);
	static void normalizeQuaternion(float *quaternion);
	static void quaternionFromMatrix(const float *matrix, float *quaternion);
	static float atan2Degrees(float y, float x);
	static float angleBetween(float cosine);

private:
	static float asinSeries(float value);
};

} // End of namespace Grim

#endif
