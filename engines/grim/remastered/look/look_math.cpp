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

#include "engines/grim/remastered/look/look_math.h"

#include <math.h>
#include <string.h>

namespace Grim {

void RemasteredLookMath::expand34(const float *matrix12, float *out16) {
	for (int row = 0; row < 4; row++) {
		out16[row * 4] = matrix12[row * 3];
		out16[row * 4 + 1] = matrix12[row * 3 + 1];
		out16[row * 4 + 2] = matrix12[row * 3 + 2];
		out16[row * 4 + 3] = row == 3 ? 1.0f : 0.0f;
	}
}

void RemasteredLookMath::multiply(const float *first, const float *second, float *out16) {
	float result[16];

	for (int row = 0; row < 4; row++) {
		for (int column = 0; column < 4; column++) {
			double sum = (double)first[row * 4] * (double)second[column];

			sum += (double)first[row * 4 + 1] * (double)second[4 + column];
			sum += (double)first[row * 4 + 2] * (double)second[8 + column];
			sum += (double)first[row * 4 + 3] * (double)second[12 + column];

			result[row * 4 + column] = (float)sum;
		}
	}

	memcpy(out16, result, sizeof(result));
}

bool RemasteredLookMath::invert(const float *a, float *out) {
	const double m0 = a[0], m1 = a[1], m2 = a[2], m3 = a[3], m4 = a[4], m5 = a[5], m6 = a[6], m7 = a[7];
	const double m8 = a[8], m9 = a[9], m10 = a[10], m11 = a[11], m12 = a[12], m13 = a[13], m14 = a[14], m15 = a[15];

	float dm0 = (float)((((((m5 * m10) * m15) + ((m6 * m11) * m13)) + ((m7 * m9) * m14)) - (((m6 * (m9 * m15)) + (m5 * (m11 * m14))) + ((m13 * m10) * m7))));
	float dm1 = (float)((((((m4 * m10) * m15) + ((m6 * m11) * m12)) + ((m7 * m8) * m14)) - (((m6 * (m15 * m8)) + ((m10 * m12) * m7)) + (m4 * (m11 * m14)))));
	float dm2 = (float)((((((m5 * m11) * m12) + ((m4 * m9) * m15)) + ((m7 * m8) * m13)) - (((m5 * (m8 * m15)) + ((m11 * m13) * m4)) + ((m12 * m9) * m7))));
	float dm3 = (float)((((((m5 * m10) * m12) + ((m4 * m9) * m14)) + ((m6 * m8) * m13)) - (((m5 * (m8 * m14)) + ((m10 * m13) * m4)) + ((m12 * m9) * m6))));
	float det = (float)((((double)dm0 * m0 - (double)dm1 * m1) + (double)dm2 * m2) - (double)dm3 * m3);
	float absDet = fabsf(det);

	if (!(9.99999974737875e-06f < absDet) && absDet == absDet) {
		warning("RemasteredLookMath::invert(): the matrix is singular");
		memset(out, 0, 16 * sizeof(float));
		return false;
	}

	float c[16];

	c[0] = (float)((((((m6 * m11) * m13) + ((m5 * m10) * m15)) + ((m7 * m9) * m14)) - (((m6 * (m15 * m9)) + ((m10 * m13) * m7)) + (m5 * (m11 * m14)))));
	c[1] = -(float)((((((m4 * m10) * m15) + ((m6 * m11) * m12)) + ((m7 * m8) * m14)) - (((m6 * (m8 * m15)) + (m4 * (m11 * m14))) + ((m12 * m10) * m7))));
	c[2] = (float)((((((m5 * m11) * m12) + ((m4 * m9) * m15)) + ((m7 * m8) * m13)) - (((m5 * (m8 * m15)) + ((m11 * m13) * m4)) + ((m12 * m9) * m7))));
	c[3] = -(float)((((((m5 * m10) * m12) + ((m4 * m9) * m14)) + ((m6 * m8) * m13)) - (((m5 * (m8 * m14)) + ((m10 * m13) * m4)) + ((m12 * m9) * m6))));
	c[4] = -(float)((((((m2 * m11) * m13) + ((m1 * m10) * m15)) + ((m3 * m9) * m14)) - (((m2 * (m9 * m15)) + ((m11 * m14) * m1)) + ((m13 * m10) * m3))));
	c[5] = (float)((((((m2 * m11) * m12) + ((m0 * m10) * m15)) + ((m3 * m8) * m14)) - (((m2 * (m8 * m15)) + ((m11 * m14) * m0)) + ((m12 * m10) * m3))));
	c[6] = -(float)((((((m1 * m11) * m12) + ((m0 * m9) * m15)) + ((m3 * m8) * m13)) - (((m1 * (m15 * m8)) + ((m11 * m13) * m0)) + ((m12 * m9) * m3))));
	c[7] = (float)((((((m1 * m10) * m12) + ((m0 * m9) * m14)) + ((m2 * m8) * m13)) - (((m1 * (m8 * m14)) + ((m10 * m13) * m0)) + ((m12 * m9) * m2))));
	c[8] = (float)((((((m2 * m7) * m13) + ((m1 * m6) * m15)) + ((m3 * m5) * m14)) - (((m2 * (m5 * m15)) + ((m7 * m14) * m1)) + ((m13 * m6) * m3))));
	c[9] = -(float)((((((m2 * m7) * m12) + ((m0 * m6) * m15)) + ((m3 * m4) * m14)) - (((m2 * (m4 * m15)) + ((m7 * m14) * m0)) + ((m12 * m6) * m3))));
	c[10] = (float)((((((m1 * m7) * m12) + ((m0 * m5) * m15)) + ((m3 * m4) * m13)) - (((m1 * (m4 * m15)) + ((m7 * m13) * m0)) + ((m12 * m5) * m3))));
	c[11] = -(float)((((((m0 * m5) * m14) + ((m1 * m6) * m12)) + ((m2 * m4) * m13)) - (((m1 * (m14 * m4)) + ((m5 * m12) * m2)) + (m0 * (m6 * m13)))));
	c[12] = -(float)((((((m2 * m7) * m9) + ((m1 * m6) * m11)) + ((m3 * m5) * m10)) - (((m2 * (m5 * m11)) + ((m7 * m10) * m1)) + ((m9 * m6) * m3))));
	c[13] = (float)((((((m2 * m7) * m8) + ((m0 * m6) * m11)) + ((m3 * m4) * m10)) - (((m2 * (m4 * m11)) + ((m7 * m10) * m0)) + ((m8 * m6) * m3))));
	c[14] = -(float)((((((m0 * m5) * m11) + ((m1 * m7) * m8)) + ((m3 * m4) * m9)) - (((m1 * (m11 * m4)) + ((m5 * m8) * m3)) + (m0 * (m7 * m9)))));
	c[15] = (float)((((((m0 * m5) * m10) + ((m1 * m6) * m8)) + ((m2 * m4) * m9)) - (((m1 * (m4 * m10)) + (m0 * (m6 * m9))) + ((m8 * m5) * m2))));

	float inverseDet = (float)(1.0 / (double)det);
	float result[16];

	for (int row = 0; row < 4; row++) {
		for (int column = 0; column < 4; column++)
			result[row * 4 + column] = (float)((double)c[row + 4 * column] * (double)inverseDet);
	}

	memcpy(out, result, sizeof(result));

	return true;
}

void RemasteredLookMath::normalize3(const float *in, float *out) {
	float x = in[0];
	float y = in[1];
	float z = in[2];
	float sum = 0.0f;

	sum = (float)((double)x * (double)x + (double)sum);
	sum = (float)((double)y * (double)y + (double)sum);
	sum = (float)((double)z * (double)z + (double)sum);

	double inverse = 0.0;

	if (!(sum <= 0.0f)) {
		float root = (float)sqrt((double)sum);

		inverse = 1.0 / (double)root;
	}

	float scale = (float)inverse;

	out[0] = x * scale;
	out[1] = y * scale;
	out[2] = z * scale;
}

float RemasteredLookMath::inverseOneMinus(float value) {
	float divisor = (float)(1.0 - (double)value);

	if (divisor == 0.0f)
		return 0.0f;

	return (float)(1.0 / (double)divisor);
}

float RemasteredLookMath::inverseDifference(float high, float low) {
	float divisor = (float)((double)high - (double)low);

	if (divisor == 0.0f)
		return 0.0f;

	return (float)(1.0 / (double)divisor);
}

float RemasteredLookMath::distanceSquared(const float *first, const float *second) {
	float sum = 0.0f;

	for (int i = 0; i < 3; i++) {
		float difference = (float)((double)first[i] - (double)second[i]);

		sum = (float)((double)difference * (double)difference + (double)sum);
	}

	return sum;
}

void RemasteredLookMath::buildShadowMatrices(const float *lightMatrix, int type, float sizeX, float sizeY, float nearDistance,
		float farDistance, float *viewProjection, float *worldToShadowMap) {
	float apex[3];

	for (int i = 0; i < 3; i++)
		apex[i] = lightMatrix[12 + i] + lightMatrix[8 + i];

	float axisX[3];
	float axisY[3];
	float axisZ[3];

	normalize3(lightMatrix, axisX);
	normalize3(lightMatrix + 4, axisY);
	normalize3(lightMatrix + 8, axisZ);

	float lightBasis[16];

	lightBasis[0] = axisX[0];
	lightBasis[1] = axisX[1];
	lightBasis[2] = axisX[2];
	lightBasis[3] = 0.0f;
	lightBasis[4] = axisY[0];
	lightBasis[5] = axisY[1];
	lightBasis[6] = axisY[2];
	lightBasis[7] = 0.0f;
	lightBasis[8] = axisZ[0];
	lightBasis[9] = axisZ[1];
	lightBasis[10] = axisZ[2];
	lightBasis[11] = 0.0f;
	lightBasis[12] = apex[0];
	lightBasis[13] = apex[1];
	lightBasis[14] = apex[2];
	lightBasis[15] = 1.0f;

	float view[16];

	invert(lightBasis, view);

	float projection[16];

	memset(projection, 0, sizeof(projection));

	if (type == 2) {
		float tangent = (float)tan(0.3926990926265717);
		float scale = (float)(1.0 / (double)tangent);
		float depthScale = (float)(1.0 / ((double)nearDistance - (double)farDistance));

		depthScale = farDistance * depthScale;

		projection[0] = scale;
		projection[5] = scale;
		projection[10] = depthScale;
		projection[11] = -1.0f;
		projection[14] = nearDistance * depthScale;
		projection[15] = 0.0f;
	} else {
		float depthScale = (float)(1.0 / ((double)nearDistance - (double)farDistance));

		projection[0] = (float)(2.0 / (double)sizeX);
		projection[5] = (float)(2.0 / (double)sizeY);
		projection[10] = depthScale;
		projection[11] = 0.0f;
		projection[14] = nearDistance * depthScale;
		projection[15] = 1.0f;
	}

	multiply(view, projection, viewProjection);

	static const float bias[16] = {
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	};

	multiply(viewProjection, bias, worldToShadowMap);
}

} // End of namespace Grim
