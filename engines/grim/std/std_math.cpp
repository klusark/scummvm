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

#include "engines/grim/std/std_math.h"

namespace Grim {

int StdMath::tableIndex(float value, int quadrant) {
	int lowest = quadrant * 4096;

	if (value > -2147483648.0f && value < 2147483648.0f) {
		int index = (int)value;

		if (index >= lowest && index < lowest + 4096)
			return index;

		warning("StdMath::tableIndex(): index %d out of range", index);

		return index < lowest ? lowest : lowest + 4095;
	}

	warning("StdMath::tableIndex(): invalid index value %f", value);

	return lowest;
}

float StdMath::flexPower(float base, int exponent) {
	float result = base;
	for (int i = 0; i < exponent - 1; i++)
		result = (float)((double)result * base);
	return result;
}

float StdMath::rint(float value) {
	return (float)floor((double)value);
}

float StdMath::normalizeAngle(float angle) {
	float result;

	if (_remastered) {
		if (!(angle < 0.0f)) {
			if (angle < 360.0f)
				return angle;

			float turns = (float)floor((double)angle / 360.0);

			result = (float)((double)angle - (double)turns * 360.0);
		} else {
			float positive = -angle;

			if (!(positive < 360.0f)) {
				float turns = (float)floor((double)positive / 360.0);

				result = (float)(360.0 - ((double)positive - (double)turns * 360.0));
			} else {
				result = (float)(360.0 - (double)positive);
			}
		}

		if (result == 360.0)
			return 0.0f;

		return result;
	}

	if (angle >= 0.0) {
		if (angle < 360.0)
			return angle;
		float turns = (float)((double)angle / 360.0);
		result = (float)((double)angle - (double)rint(turns) * 360.0);
	} else {
		float positive = (float)(-(double)angle);
		if (positive >= 360.0) {
			float turns = (float)((double)positive / 360.0);
			result = (float)(360.0 - ((double)positive - (double)rint(turns) * 360.0));
		} else {
			result = (float)(360.0 - (double)positive);
		}
	}

	if (result == 360.0)
		return 0.0f;
	return result;
}

float StdMath::normalizeAngleAcute(float angle) {
	float result = normalizeAngle(angle);

	if (result > 180.0)
		return (float)(-(360.0 - (double)result));
	return result;
}

void StdMath::sinCos(float angle, float *sin, float *cos) {
	float normalized = normalizeAngle(angle);
	int quadrant;

	if (normalized >= 90.0) {
		if (normalized >= 180.0)
			quadrant = (normalized >= 270.0) ? 3 : 2;
		else
			quadrant = 1;
	} else {
		quadrant = 0;
	}

	float value = (float)((double)normalized * 45.511112f);
	float fraction = (float)((double)value - rint(value));
	int index = tableIndex(value, quadrant);
	int next = index + 1;
	const float *table = kSinCosTable;
	float nextSin;
	float nextCos;
	float curSin;
	float curCos;

	switch (quadrant) {
	case 0:
		nextSin = (next < 4096) ? table[next] : table[4095 - (index - 4095)];
		*sin = (float)(((double)nextSin - table[index]) * fraction + table[index]);
		nextCos = (next < 4096) ? table[4095 - next] : -table[next - 4096];
		*cos = (float)(((double)nextCos - table[4095 - index]) * fraction + table[4095 - index]);
		break;
	case 1:
		nextSin = (next < 0x2000) ? table[4095 - (index - 4095)] : -table[next - 8192];
		curSin = table[4095 - (index - 4096)];
		*sin = (float)(((double)nextSin - curSin) * fraction + curSin);
		nextCos = (next < 0x2000) ? -table[next - 4096] : -table[4095 - (index - 0x1FFF)];
		curCos = -table[index - 4096];
		*cos = (float)(((double)nextCos - curCos) * fraction + curCos);
		break;
	case 2:
		nextSin = (next < 12288) ? -table[next - 8192] : -table[4095 - (index - 12287)];
		curSin = -table[index - 8192];
		*sin = (float)(((double)nextSin - curSin) * fraction + curSin);
		nextCos = (next < 12288) ? -table[4095 - (index - 0x1FFF)] : table[next - 12288];
		curCos = -table[4095 - (index - 0x2000)];
		*cos = (float)(((double)nextCos - curCos) * fraction + curCos);
		break;
	case 3:
		nextSin = (next < 0x4000) ? -table[4095 - (index - 12287)] : table[next - 16384];
		curSin = -table[4095 - (index - 12288)];
		*sin = (float)(((double)nextSin - curSin) * fraction + curSin);
		nextCos = (next < 0x4000) ? table[next - 12288] : table[4095 - (index - 0x3FFF)];
		curCos = table[index - 12288];
		*cos = (float)(((double)nextCos - curCos) * fraction + curCos);
		break;
	default:
		break;
	}
}

float StdMath::tan(float angle) {
	float normalized = normalizeAngle(angle);
	int quadrant;

	if (normalized >= 90.0) {
		if (normalized >= 180.0)
			quadrant = (normalized >= 270.0) ? 3 : 2;
		else
			quadrant = 1;
	} else {
		quadrant = 0;
	}

	float scaled = (float)((double)normalized / 360.0 * 16384.0);
	float fraction = (float)((double)scaled - rint(scaled));
	int index = tableIndex(scaled, quadrant);
	int next = index + 1;
	const float *table = kTanTable;
	float nextValue;
	float curValue;
	float result = 0.0f;

	switch (quadrant) {
	case 0:
		nextValue = (next < 4096) ? table[next] : -table[4095 - (index - 4095)];
		result = (float)(((double)nextValue - table[index]) * fraction + table[index]);
		break;
	case 1:
		nextValue = (next < 0x2000) ? -table[4095 - (index - 4095)] : table[next - 8192];
		curValue = -table[4095 - (index - 4096)];
		result = (float)(((double)nextValue - curValue) * fraction + curValue);
		break;
	case 2:
		nextValue = (next < 12288) ? table[next - 8192] : -table[4095 - (index - 12287)];
		result = (float)(((double)nextValue - table[index - 8192]) * fraction + table[index - 8192]);
		break;
	case 3:
		nextValue = (next < 0x4000) ? -table[4095 - (index - 12287)] : table[next - 16384];
		curValue = -table[4095 - (index - 12288)];
		result = (float)(((double)nextValue - curValue) * fraction + curValue);
		break;
	default:
		break;
	}

	return result;
}

float StdMath::arcSin2Remastered(float value) {
	bool negative = value < 0.0;
	float absValue = negative ? (float)(-(double)value) : value;
	float result;

	if (!(absValue > 0.70710677f)) {
		result = (float)(((double)flexPower(absValue, 3) * 0.212749f + absValue) * 57.295784f);
	} else {
		float root = (float)sqrt(1.0 - (double)absValue * absValue);

		result = (float)(90.0 - ((double)flexPower(root, 3) * 0.212749f + root) * 57.295784f);
	}

	return negative ? -result : result;
}

float StdMath::arcSin2(float value) {
	if (_remastered)
		return arcSin2Remastered(value);

	bool negative = value < 0.0;
	float absValue = (value >= 0.0) ? value : (float)(-(double)value);
	float result;

	if (absValue <= 0.70710677f) {
		result = (float)(((double)flexPower(absValue, 3) * 0.212749f + absValue) * 57.295784f);
	} else {
		float root = sqrtImpl((float)(1.0 - (double)absValue * absValue));
		result = (float)(90.0 - ((double)flexPower(root, 3) * 0.212749f + root) * 57.295784f);
	}

	return negative ? -result : result;
}

float StdMath::sqrtImpl(float value) {
	return (float)sqrt((double)value);
}

float StdMath::arcSin3Remastered(float value) {
	bool negative = value < 0.0;
	float absValue = negative ? (float)(-(double)value) : value;
	float result;

	if (!(absValue > 0.70710677f)) {
		double first = (double)flexPower(absValue, 3) / 6.0 + absValue;
		double second = (double)flexPower(absValue, 5) * 0.075f + first;

		result = (float)(((double)flexPower(absValue, 7) * 0.066797f + second) * 57.295784f);
	} else {
		float root = (float)sqrt(1.0 - (double)absValue * absValue);
		double first = (double)flexPower(root, 3) / 6.0 + root;
		double second = (double)flexPower(root, 5) * 0.075f + first;

		result = (float)(90.0 - ((double)flexPower(root, 7) * 0.066797f + second) * 57.295784f);
	}

	return negative ? -result : result;
}

float StdMath::arcSin3(float value) {
	if (_remastered)
		return arcSin3Remastered(value);

	bool negative = value < 0.0;
	float absValue = (value >= 0.0) ? value : (float)(-(double)value);
	float result;

	if (absValue <= 0.70710677f) {
		float first = (float)((double)flexPower(absValue, 3) / 6.0 + absValue);
		float second = (float)((double)flexPower(absValue, 5) * 0.075f + first);
		result = (float)(((double)flexPower(absValue, 7) * 0.066797f + second) * 57.295784f);
	} else {
		float root = sqrtImpl((float)(1.0 - (double)absValue * absValue));
		float first = (float)((double)flexPower(root, 3) / 6.0 + root);
		float second = (float)((double)flexPower(root, 5) * 0.075f + first);
		result = (float)(90.0 - ((double)flexPower(root, 7) * 0.066797f + second) * 57.295784f);
	}

	return negative ? -result : result;
}

float StdMath::arcTan4Remastered(float y, float x) {
	if (x == 0.0f && y == 0.0f)
		return 0.0f;

	bool yNegative = y < 0.0f;
	bool xNegative = x < 0.0f;
	float absY = yNegative ? -y : y;
	float absX = xNegative ? -x : x;
	float ratio = (absY < absX) ? (float)((double)absY / absX) : (float)((double)absX / absY);
	float absRatio = (ratio < 0.0f) ? -ratio : ratio;

	double first = (double)absRatio - (double)flexPower(absRatio, 3) / 3.0;
	double second = (double)flexPower(absRatio, 5) / 5.0 + first;
	double third = second - (double)flexPower(absRatio, 7) / 7.0;
	float angle = (float)(((double)flexPower(absRatio, 9) * 0.063235f + third) * 57.295784f);

	if (absX <= absY)
		angle = (float)(90.0 - angle);

	float result = (float)(90.0 - angle);
	if (yNegative)
		result = (float)(180.0 - result);

	if (!xNegative)
		return -result;
	return result;
}

float StdMath::arcTan4(float y, float x) {
	if (_remastered)
		return arcTan4Remastered(y, x);

	if (x == 0.0 && y == 0.0)
		return 0.0f;

	float absY = (y >= 0.0) ? y : (float)(-(double)y);
	float absX = (x >= 0.0) ? x : (float)(-(double)x);
	double ratio = (absX <= absY) ? (double)absX / absY : (double)absY / absX;
	float ratioValue = (float)ratio;
	float absRatio = (ratioValue >= 0.0) ? ratioValue : (float)(-(double)ratioValue);

	float first = (float)((double)absRatio - (double)flexPower(absRatio, 3) / 3.0);
	float second = (float)((double)flexPower(absRatio, 5) / 5.0 + first);
	float third = (float)((double)second - (double)flexPower(absRatio, 7) / 7.0);
	float angle = (float)(((double)flexPower(absRatio, 9) * 0.063235f + third) * 57.295784f);

	if (absY >= absX)
		angle = (float)(90.0 - angle);

	float result = (float)(90.0 - angle);
	if (y < 0.0)
		result = (float)(180.0 - result);

	if (x >= 0.0)
		return -result;
	return result;
}

} // End of namespace Grim
