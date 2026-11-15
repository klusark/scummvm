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

#include "engines/grim/emi/math/singlemath.h"

namespace Grim {

const float SingleMath::kMinLength = 9.99999968e-21f;
const float SingleMath::kDegreesToRadians = 0.0174532924f;
const float SingleMath::kRadiansToDegrees = 57.295784f;

static const float kPi = 3.14159274f;
static const float kHalfPi = 1.57079637f;
static const float kInverseTwoPi = 0.159154937f;
static const float kSineTerm3 = -0.166666672f;
static const float kSineTerm5 = 0.00833333377f;
static const float kSineTerm7 = -0.000198412701f;
static const float kSineTerm9 = 2.75573188e-06f;
static const float kAsinLimit = 0.999998987f;
static const float kAsinSplit = 0.707106769f;
static const float kAsinTiny = 0.00100000005f;
static const float kAsinTerm3 = 0.166666672f;
static const float kAsinTerm5 = 0.075000003f;
static const float kAsinTerm7 = 0.0446428582f;
static const float kAsinTerm9 = 0.0474460013f;
static const float kAngleScale = 57.2957764f;
static const float kAngleFlatLimit = 1.00000095f;

TruncFloat::TruncFloat(float value) {
	memcpy(&_value, &value, sizeof(_value));
}

TruncFloat TruncFloat::fromBits(uint32 bits) {
	TruncFloat result;

	result._value = bits;

	return result;
}

TruncFloat TruncFloat::fromInt(int value) {
	if (value == 0)
		return fromBits(0);

	bool negative = value < 0;
	uint64 magnitude = negative ? (uint64)(-(int64)value) : (uint64)value;

	return pack(negative, magnitude, 0);
}

float TruncFloat::toFloat() const {
	float result;

	memcpy(&result, &_value, sizeof(result));

	return result;
}

int TruncFloat::toInt() const {
	Operand operand = unpack(_value);

	if (operand.mantissa == 0)
		return 0;

	if (operand.exponent >= 8)
		return operand.negative ? (int)0x80000000 : 0x7FFFFFFF;

	uint64 magnitude;

	if (operand.exponent >= 0)
		magnitude = operand.mantissa << operand.exponent;
	else if (operand.exponent <= -24)
		magnitude = 0;
	else
		magnitude = operand.mantissa >> (-operand.exponent);

	return operand.negative ? -(int)magnitude : (int)magnitude;
}

int TruncFloat::topBit(uint64 value) {
	int position = 0;

	while (value > 1) {
		value >>= 1;
		position++;
	}

	return position;
}

uint64 TruncFloat::integerSquareRoot(uint64 value) {
	uint64 result = 0;
	uint64 bit = (uint64)1 << 62;

	while (bit > value)
		bit >>= 2;

	while (bit != 0) {
		if (value >= result + bit) {
			value -= result + bit;
			result = (result >> 1) + bit;
		} else {
			result >>= 1;
		}

		bit >>= 2;
	}

	return result;
}

TruncFloat::Operand TruncFloat::unpack(uint32 bits) {
	Operand operand;
	uint32 exponent = (bits >> 23) & 0xFF;
	uint32 fraction = bits & 0x7FFFFF;

	operand.negative = (bits >> 31) != 0;
	operand.exponent = 0;
	operand.mantissa = 0;

	if (exponent == 0)
		return operand;

	if (exponent == 255) {
		exponent = 254;
		fraction = 0x7FFFFF;
	}

	operand.mantissa = fraction | 0x800000;
	operand.exponent = (int)exponent - 150;

	return operand;
}

TruncFloat TruncFloat::pack(bool negative, uint64 mantissa, int exponent) {
	uint32 sign = negative ? 0x80000000 : 0;

	if (mantissa == 0)
		return fromBits(sign);

	int top = topBit(mantissa);
	uint64 kept;

	if (top >= 23)
		kept = mantissa >> (top - 23);
	else
		kept = mantissa << (23 - top);

	int biased = exponent + top + 127;

	if (biased <= 0)
		return fromBits(sign);

	if (biased >= 255)
		return fromBits(sign | 0x7F7FFFFF);

	return fromBits(sign | ((uint32)biased << 23) | ((uint32)kept & 0x7FFFFF));
}

TruncFloat TruncFloat::addOperands(Operand first, Operand second) {
	if (first.mantissa == 0 && second.mantissa == 0)
		return fromBits((first.negative && second.negative) ? 0x80000000 : 0);

	if (first.mantissa == 0)
		return pack(second.negative, second.mantissa, second.exponent);

	if (second.mantissa == 0)
		return pack(first.negative, first.mantissa, first.exponent);

	int firstTop = topBit(first.mantissa);
	int secondTop = topBit(second.mantissa);

	first.mantissa <<= (62 - firstTop);
	first.exponent -= (62 - firstTop);
	second.mantissa <<= (62 - secondTop);
	second.exponent -= (62 - secondTop);

	bool firstIsBigger = first.exponent > second.exponent || (first.exponent == second.exponent && first.mantissa >= second.mantissa);
	Operand big = firstIsBigger ? first : second;
	Operand small = firstIsBigger ? second : first;
	int distance = big.exponent - small.exponent;
	uint64 shifted = 0;
	bool sticky = false;

	if (distance >= 63) {
		sticky = true;
	} else if (distance > 0) {
		shifted = small.mantissa >> distance;
		sticky = (small.mantissa & (((uint64)1 << distance) - 1)) != 0;
	} else {
		shifted = small.mantissa;
	}

	uint64 result;

	if (big.negative == small.negative) {
		result = big.mantissa + shifted;
	} else {
		result = big.mantissa - shifted - (sticky ? 1 : 0);

		if (result == 0)
			return fromBits(0);
	}

	return pack(big.negative, result, big.exponent);
}

TruncFloat TruncFloat::squareRoot(TruncFloat value) {
	Operand operand = unpack(value._value);

	if (operand.mantissa == 0)
		return fromBits(0);

	uint64 mantissa = operand.mantissa;
	int exponent = operand.exponent;

	if ((exponent & 1) != 0) {
		mantissa <<= 1;
		exponent -= 1;
	}

	return pack(false, integerSquareRoot(mantissa << 38), exponent / 2 - 19);
}

TruncFloat TruncFloat::multiplyAdd(TruncFloat accumulator, TruncFloat first, TruncFloat second) {
	Operand left = unpack(first._value);
	Operand right = unpack(second._value);
	Operand product;

	product.negative = left.negative != right.negative;
	product.exponent = left.exponent + right.exponent;
	product.mantissa = (left.mantissa == 0 || right.mantissa == 0) ? 0 : left.mantissa * right.mantissa;

	return addOperands(unpack(accumulator._value), product);
}

TruncFloat TruncFloat::multiplySubtract(TruncFloat accumulator, TruncFloat first, TruncFloat second) {
	return multiplyAdd(accumulator, -first, second);
}

TruncFloat TruncFloat::operator-() const {
	return fromBits(_value ^ 0x80000000);
}

TruncFloat TruncFloat::operator+(TruncFloat other) const {
	return addOperands(unpack(_value), unpack(other._value));
}

TruncFloat TruncFloat::operator-(TruncFloat other) const {
	Operand second = unpack(other._value);

	second.negative = !second.negative;

	return addOperands(unpack(_value), second);
}

TruncFloat TruncFloat::operator*(TruncFloat other) const {
	Operand left = unpack(_value);
	Operand right = unpack(other._value);
	bool negative = left.negative != right.negative;

	if (left.mantissa == 0 || right.mantissa == 0)
		return fromBits(negative ? 0x80000000 : 0);

	return pack(negative, left.mantissa * right.mantissa, left.exponent + right.exponent);
}

TruncFloat TruncFloat::operator/(TruncFloat other) const {
	Operand left = unpack(_value);
	Operand right = unpack(other._value);
	bool negative = left.negative != right.negative;

	if (right.mantissa == 0)
		return fromBits((negative ? 0x80000000 : 0) | 0x7F7FFFFF);

	if (left.mantissa == 0)
		return fromBits(negative ? 0x80000000 : 0);

	uint64 quotient = (left.mantissa << 24) / right.mantissa;

	return pack(negative, quotient, left.exponent - right.exponent - 24);
}

TruncFloat &TruncFloat::operator+=(TruncFloat other) {
	*this = *this + other;

	return *this;
}

TruncFloat &TruncFloat::operator-=(TruncFloat other) {
	*this = *this - other;

	return *this;
}

TruncFloat &TruncFloat::operator*=(TruncFloat other) {
	*this = *this * other;

	return *this;
}

TruncFloat &TruncFloat::operator/=(TruncFloat other) {
	*this = *this / other;

	return *this;
}

int TruncFloat::compare(TruncFloat other) const {
	Operand left = unpack(_value);
	Operand right = unpack(other._value);
	int leftSign = left.mantissa == 0 ? 0 : (left.negative ? -1 : 1);
	int rightSign = right.mantissa == 0 ? 0 : (right.negative ? -1 : 1);

	if (leftSign != rightSign)
		return leftSign < rightSign ? -1 : 1;

	if (leftSign == 0)
		return 0;

	int magnitude = 0;

	if (left.exponent != right.exponent)
		magnitude = left.exponent < right.exponent ? -1 : 1;
	else if (left.mantissa != right.mantissa)
		magnitude = left.mantissa < right.mantissa ? -1 : 1;

	return leftSign < 0 ? -magnitude : magnitude;
}

bool TruncFloat::operator<(TruncFloat other) const {
	return compare(other) < 0;
}

bool TruncFloat::operator<=(TruncFloat other) const {
	return compare(other) <= 0;
}

bool TruncFloat::operator>(TruncFloat other) const {
	return compare(other) > 0;
}

bool TruncFloat::operator>=(TruncFloat other) const {
	return compare(other) >= 0;
}

bool TruncFloat::operator==(TruncFloat other) const {
	return compare(other) == 0;
}

bool TruncFloat::operator!=(TruncFloat other) const {
	return compare(other) != 0;
}

float SingleMath::reduceAngle(float radians) {
	TruncFloat pi(kPi);
	TruncFloat inverseTwoPi(kInverseTwoPi);
	TruncFloat value(radians);
	TruncFloat twoPi = pi + pi;

	if (value <= pi) {
		if (!(value < -pi))
			return radians;

		int turns = ((value - pi) * inverseTwoPi).toInt();

		return (value - TruncFloat::fromInt(turns) * twoPi).toFloat();
	}

	int turns = ((value + pi) * inverseTwoPi).toInt();

	return (value - TruncFloat::fromInt(turns) * twoPi).toFloat();
}

float SingleMath::sinePolynomial(float input) {
	TruncFloat value(input);
	TruncFloat square = value * value;
	TruncFloat cube = value * square;
	TruncFloat fifth = cube * square;
	TruncFloat seventh = fifth * square;
	TruncFloat ninth = seventh * square;
	TruncFloat accumulator = value + TruncFloat(0.0f);

	accumulator = TruncFloat::multiplyAdd(accumulator, cube, TruncFloat(kSineTerm3));
	accumulator = TruncFloat::multiplyAdd(accumulator, fifth, TruncFloat(kSineTerm5));
	accumulator = TruncFloat::multiplyAdd(accumulator, seventh, TruncFloat(kSineTerm7));

	return TruncFloat::multiplyAdd(accumulator, ninth, TruncFloat(kSineTerm9)).toFloat();
}

void SingleMath::sinCosDegrees(float degrees, float *sineOut, float *cosineOut) {
	TruncFloat pi(kPi);
	TruncFloat halfPi(kHalfPi);
	TruncFloat radians = (pi * TruncFloat(degrees)) / TruncFloat(180.0f);
	TruncFloat reduced(reduceAngle(radians.toFloat()));
	TruncFloat complement = -reduced.absolute() + halfPi;

	*cosineOut = sinePolynomial(complement.toFloat());

	reduced = TruncFloat(reduceAngle(radians.toFloat()));

	TruncFloat magnitude = reduced.absolute();

	if (!(magnitude <= halfPi))
		magnitude = pi - magnitude;

	TruncFloat sine(sinePolynomial(magnitude.toFloat()));

	if (reduced < TruncFloat(0.0f))
		sine = -sine;

	*sineOut = sine.toFloat();
}

float SingleMath::asinSeries(float input) {
	TruncFloat value(input);
	TruncFloat square = value * value;
	TruncFloat cube = value * square;
	TruncFloat termThree = TruncFloat(kAsinTerm3) * cube;
	TruncFloat fifth = cube * square;
	TruncFloat termFive = TruncFloat(kAsinTerm5) * fifth;
	TruncFloat seventh = fifth * square;
	TruncFloat ninth = seventh * square;
	TruncFloat partial = value + termThree;
	TruncFloat sum = termFive + partial;
	TruncFloat termSeven = TruncFloat(kAsinTerm7) * seventh;
	TruncFloat accumulator = termSeven + sum;

	return TruncFloat::multiplyAdd(accumulator, TruncFloat(kAsinTerm9), ninth).toFloat();
}

float SingleMath::asinDegrees(float input) {
	TruncFloat value(input);
	TruncFloat zero(0.0f);
	TruncFloat limit(kAsinLimit);

	if (!(value <= limit))
		return 90.0f;

	if (value < -limit)
		return -90.0f;

	TruncFloat reduced = value;
	TruncFloat split(kAsinSplit);
	bool isReduced = false;

	if (!(value < split) || value <= -split) {
		isReduced = true;

		TruncFloat root = TruncFloat::squareRoot(TruncFloat(1.0f) - value * value);

		if (value < zero)
			reduced = -root;
		else
			reduced = root;
	}

	TruncFloat radians = reduced;
	TruncFloat tiny(kAsinTiny);

	if (!(reduced < tiny) || reduced <= -tiny)
		radians = TruncFloat(asinSeries(reduced.toFloat()));

	TruncFloat result = (TruncFloat(180.0f) * radians) / TruncFloat(kPi);

	if (isReduced) {
		if (value < zero)
			result = TruncFloat(-90.0f) - result;
		else
			result = TruncFloat(90.0f) - result;
	}

	return result.toFloat();
}

float SingleMath::acosDegrees(float value) {
	return (TruncFloat(90.0f) - TruncFloat(asinDegrees(value))).toFloat();
}

float SingleMath::dot(const float *first, const float *second) {
	TruncFloat x = TruncFloat(first[0]) * TruncFloat(second[0]);
	TruncFloat y = TruncFloat(first[1]) * TruncFloat(second[1]);
	TruncFloat accumulator = x + y;

	return TruncFloat::multiplyAdd(accumulator, TruncFloat(first[2]), TruncFloat(second[2])).toFloat();
}

float SingleMath::length(const float *vector) {
	return TruncFloat::squareRoot(TruncFloat(dot(vector, vector))).toFloat();
}

void SingleMath::normalize(float *vector) {
	TruncFloat vectorLength(length(vector));

	if (vectorLength < TruncFloat(kMinLength))
		return;

	TruncFloat inverse = TruncFloat(1.0f) / vectorLength;

	vector[0] = (TruncFloat(vector[0]) * inverse).toFloat();
	vector[1] = (TruncFloat(vector[1]) * inverse).toFloat();
	vector[2] = (TruncFloat(vector[2]) * inverse).toFloat();
}

void SingleMath::rotate(const float *matrix, const float *vector, float *result) {
	TruncFloat first(vector[0]);
	TruncFloat second(vector[1]);
	TruncFloat third(vector[2]);
	TruncFloat x = TruncFloat::multiplyAdd(first * TruncFloat(matrix[0]) + second * TruncFloat(matrix[3]), third, TruncFloat(matrix[6]));
	TruncFloat y = TruncFloat::multiplyAdd(first * TruncFloat(matrix[1]) + second * TruncFloat(matrix[4]), third, TruncFloat(matrix[7]));
	TruncFloat z = TruncFloat::multiplyAdd(first * TruncFloat(matrix[2]) + second * TruncFloat(matrix[5]), third, TruncFloat(matrix[8]));

	result[0] = x.toFloat();
	result[1] = y.toFloat();
	result[2] = z.toFloat();
}

float SingleMath::angleBetween(float cosine) {
	if (!(cosine <= 1.0f) && cosine < kAngleFlatLimit)
		return 0.0f;

	return kAngleScale * (kDegreesToRadians * acosDegrees(cosine));
}

void SingleMath::matrixFromEuler(float x, float y, float z, float *matrix) {
	float quaternion[4];

	quaternionFromEuler(x, y, z, quaternion);
	matrixFromQuaternion(quaternion[0], quaternion[1], quaternion[2], quaternion[3], matrix);
}

float SingleMath::atan2Degrees(float y, float x) {
	TruncFloat zero(0.0f);
	TruncFloat tiny(9.99999975e-05f);
	TruncFloat valueY(y);
	TruncFloat valueX(x);
	TruncFloat result;

	if (valueX < tiny && !(valueX < -tiny)) {
		result = TruncFloat(90.0f);
	} else if (valueY < tiny && !(valueY < -tiny)) {
		result = zero;
	} else {
		TruncFloat absoluteY = valueY < zero ? -valueY : valueY;
		TruncFloat absoluteX = valueX < zero ? -valueX : valueX;
		bool swapped = false;
		TruncFloat ratio;

		if (absoluteY <= absoluteX) {
			ratio = absoluteY / absoluteX;
		} else {
			ratio = absoluteX / absoluteY;
			swapped = true;
		}

		if (ratio < tiny && !(ratio < -tiny)) {
			result = zero;
		} else {
			TruncFloat square = ratio * ratio;
			TruncFloat cube = ratio * square;
			TruncFloat fifth = cube * square;
			TruncFloat seventh = fifth * square;
			TruncFloat ninth = seventh * square;
			TruncFloat partial = ratio - TruncFloat(0.333333343f) * cube;
			TruncFloat sum = TruncFloat(0.200000003f) * fifth + partial;

			sum = sum - TruncFloat(0.142857149f) * seventh;
			sum = TruncFloat::multiplyAdd(sum, TruncFloat(0.0632349998f), ninth);
			result = (TruncFloat(180.0f) * sum) / TruncFloat(kPi);
		}

		if (swapped)
			result = TruncFloat(90.0f) - result;
	}

	if (valueX < -tiny)
		result = TruncFloat(180.0f) - result;

	if (valueY < -tiny)
		result = -result;

	return result.toFloat();
}

void SingleMath::matrixToEuler(const float *matrix, float *euler) {
	TruncFloat one(1.0f);
	TruncFloat negated = -TruncFloat(matrix[2]);

	if (!(negated <= one))
		negated = one;
	else if (negated < -one)
		negated = -one;

	TruncFloat toRadians(kDegreesToRadians);
	TruncFloat toDegrees(57.2957764f);
	TruncFloat pitch = toRadians * TruncFloat(asinDegrees(negated.toFloat()));
	float sineValue;
	float cosineValue;

	euler[1] = pitch.toFloat();
	sinCosDegrees((TruncFloat(kRadiansToDegrees) * pitch).toFloat(), &sineValue, &cosineValue);

	TruncFloat cosine(cosineValue);

	if (cosine < TruncFloat(0.0f))
		cosine = -cosine;

	if (cosine <= TruncFloat(1.1920929e-07f)) {
		euler[0] = (toRadians * TruncFloat(atan2Degrees(matrix[3], matrix[4]))).toFloat();
		euler[2] = 0.0f;
	} else {
		euler[0] = (toRadians * TruncFloat(atan2Degrees(matrix[5], matrix[8]))).toFloat();
		euler[2] = (toRadians * TruncFloat(atan2Degrees(matrix[1], matrix[0]))).toFloat();
	}

	euler[0] = (TruncFloat(euler[0]) * toDegrees).toFloat();
	euler[1] = (TruncFloat(euler[1]) * toDegrees).toFloat();
	euler[2] = (TruncFloat(euler[2]) * toDegrees).toFloat();
}

void SingleMath::quaternionFromEuler(float x, float y, float z, float *quaternion) {
	TruncFloat toRadians(kDegreesToRadians);
	TruncFloat toDegrees(kRadiansToDegrees);
	TruncFloat half(0.5f);
	float halfX = (toDegrees * (half * (toRadians * TruncFloat(x)))).toFloat();
	float halfY = (toDegrees * (half * (toRadians * TruncFloat(y)))).toFloat();
	float halfZ = (toDegrees * (half * (toRadians * TruncFloat(z)))).toFloat();
	float sineValue;
	float cosineValue;

	sinCosDegrees(halfX, &sineValue, &cosineValue);

	TruncFloat sinX(sineValue);
	TruncFloat cosX(cosineValue);

	sinCosDegrees(halfY, &sineValue, &cosineValue);

	TruncFloat sinY(sineValue);
	TruncFloat cosY(cosineValue);

	sinCosDegrees(halfZ, &sineValue, &cosineValue);

	TruncFloat sinZ(sineValue);
	TruncFloat cosZ(cosineValue);
	TruncFloat cosYZ = cosY * cosZ;
	TruncFloat sinYZ = sinY * sinZ;
	TruncFloat quatW = TruncFloat::multiplyAdd(cosX * cosYZ, sinX, sinYZ);
	TruncFloat quatX = TruncFloat::multiplySubtract(sinX * cosYZ, cosX, sinYZ);
	TruncFloat quatY = TruncFloat::multiplyAdd(cosZ * (cosX * sinY), sinZ, sinX * cosY);
	TruncFloat quatZ = TruncFloat::multiplySubtract(sinZ * (cosX * cosY), cosZ, sinX * sinY);

	quaternion[0] = quatX.toFloat();
	quaternion[1] = quatY.toFloat();
	quaternion[2] = quatZ.toFloat();
	quaternion[3] = quatW.toFloat();
}

void SingleMath::matrixFromQuaternion(float x, float y, float z, float w, float *matrix) {
	TruncFloat quatX(x);
	TruncFloat quatY(y);
	TruncFloat quatZ(z);
	TruncFloat quatW(w);
	TruncFloat doubleX = quatX + quatX;
	TruncFloat doubleY = quatY + quatY;
	TruncFloat doubleZ = quatZ + quatZ;
	TruncFloat yy = quatY * doubleY;
	TruncFloat zz = quatZ * doubleZ;
	TruncFloat xx = quatX * doubleX;
	TruncFloat wx = quatW * doubleX;
	TruncFloat xy = quatX * doubleY;
	TruncFloat xz = quatX * doubleZ;
	TruncFloat wy = quatW * doubleY;
	TruncFloat wz = quatW * doubleZ;
	TruncFloat yz = quatY * doubleZ;
	TruncFloat one(1.0f);

	matrix[0] = (one - (yy + zz)).toFloat();
	matrix[1] = (xy + wz).toFloat();
	matrix[2] = (xz - wy).toFloat();
	matrix[3] = (xy - wz).toFloat();
	matrix[4] = (one - (xx + zz)).toFloat();
	matrix[5] = (yz + wx).toFloat();
	matrix[6] = (xz + wy).toFloat();
	matrix[7] = (yz - wx).toFloat();
	matrix[8] = (one - (xx + yy)).toFloat();
}

void SingleMath::multiplyMatrices(const float *first, const float *second, float *result) {
	float product[9];

	for (int row = 0; row < 3; row++) {
		for (int column = 0; column < 3; column++) {
			TruncFloat sum = TruncFloat(first[3 * row]) * TruncFloat(second[column]) + TruncFloat(first[3 * row + 1]) * TruncFloat(second[3 + column]);

			product[3 * row + column] = TruncFloat::multiplyAdd(sum, TruncFloat(first[3 * row + 2]), TruncFloat(second[6 + column])).toFloat();
		}
	}

	for (int i = 0; i < 9; i++)
		result[i] = product[i];
}

void SingleMath::normalizeQuaternion(float *quaternion) {
	TruncFloat quatX(quaternion[0]);
	TruncFloat quatY(quaternion[1]);
	TruncFloat quatZ(quaternion[2]);
	TruncFloat quatW(quaternion[3]);
	TruncFloat squareZ = quatZ * quatZ;
	TruncFloat partial = TruncFloat::multiplyAdd(quatY * quatY, quatX, quatX);
	TruncFloat quatLength = TruncFloat::squareRoot(TruncFloat::multiplyAdd(squareZ + partial, quatW, quatW));

	if (quatLength < TruncFloat(kMinLength)) {
		quaternion[0] = 0.0f;
		quaternion[1] = 0.0f;
		quaternion[2] = 0.0f;
		quaternion[3] = 1.0f;

		return;
	}

	TruncFloat inverse = TruncFloat(1.0f) / quatLength;

	quaternion[0] = (quatX * inverse).toFloat();
	quaternion[1] = (quatY * inverse).toFloat();
	quaternion[2] = (quatZ * inverse).toFloat();
	quaternion[3] = (quatW * inverse).toFloat();
}

void SingleMath::quaternionFromMatrix(const float *matrix, float *quaternion) {
	TruncFloat zero(0.0f);
	TruncFloat one(1.0f);
	TruncFloat half(0.5f);
	TruncFloat trace = TruncFloat(matrix[8]) + (TruncFloat(matrix[0]) + TruncFloat(matrix[4]));

	if (!(trace < zero)) {
		TruncFloat root = TruncFloat::squareRoot(one + trace);
		TruncFloat scale = half / root;

		quaternion[3] = (half * root).toFloat();
		quaternion[0] = (scale * (TruncFloat(matrix[5]) - TruncFloat(matrix[7]))).toFloat();
		quaternion[1] = (scale * (TruncFloat(matrix[6]) - TruncFloat(matrix[2]))).toFloat();
		quaternion[2] = (scale * (TruncFloat(matrix[1]) - TruncFloat(matrix[3]))).toFloat();
	} else {
		int largest = 0;

		if (TruncFloat(matrix[4]) > TruncFloat(matrix[0]))
			largest = 1;

		if (TruncFloat(matrix[8]) > TruncFloat(matrix[4 * largest]))
			largest = 2;

		if (largest == 2) {
			TruncFloat root = TruncFloat::squareRoot(one + (TruncFloat(matrix[8]) - (TruncFloat(matrix[0]) + TruncFloat(matrix[4]))));
			TruncFloat scale = half / root;

			quaternion[2] = (half * root).toFloat();
			quaternion[0] = (scale * (TruncFloat(matrix[2]) + TruncFloat(matrix[6]))).toFloat();
			quaternion[1] = (scale * (TruncFloat(matrix[7]) + TruncFloat(matrix[5]))).toFloat();
			quaternion[3] = (scale * (TruncFloat(matrix[1]) - TruncFloat(matrix[3]))).toFloat();
		} else if (largest == 1) {
			TruncFloat root = TruncFloat::squareRoot(one + (TruncFloat(matrix[4]) - (TruncFloat(matrix[8]) + TruncFloat(matrix[0]))));
			TruncFloat scale = half / root;

			quaternion[1] = (half * root).toFloat();
			quaternion[2] = (scale * (TruncFloat(matrix[7]) + TruncFloat(matrix[5]))).toFloat();
			quaternion[0] = (scale * (TruncFloat(matrix[3]) + TruncFloat(matrix[1]))).toFloat();
			quaternion[3] = (scale * (TruncFloat(matrix[6]) - TruncFloat(matrix[2]))).toFloat();
		} else {
			TruncFloat root = TruncFloat::squareRoot(one + (TruncFloat(matrix[0]) - (TruncFloat(matrix[4]) + TruncFloat(matrix[8]))));
			TruncFloat scale = half / root;

			quaternion[0] = (half * root).toFloat();
			quaternion[1] = (scale * (TruncFloat(matrix[3]) + TruncFloat(matrix[1]))).toFloat();
			quaternion[2] = (scale * (TruncFloat(matrix[2]) + TruncFloat(matrix[6]))).toFloat();
			quaternion[3] = (scale * (TruncFloat(matrix[5]) - TruncFloat(matrix[7]))).toFloat();
		}
	}

	normalizeQuaternion(quaternion);
}

} // End of namespace Grim
