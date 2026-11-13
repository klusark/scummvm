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

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

void RenderDroid::build34(RdMatrix34 out, RdVector3 pyr, RdVector3 pos) {
	float sinP, cosP, sinY, cosY, sinR, cosR;

	_stdMath->sinCos(pyr[0], &sinP, &cosP);
	_stdMath->sinCos(pyr[1], &sinY, &cosY);
	_stdMath->sinCos(pyr[2], &sinR, &cosR);

	if (_remastered) {
		float sinRsinYStored = (float)((double)sinR * sinY);
		float cosRcosYStored = (float)((double)cosR * cosY);
		float sinRcosYStored = (float)((double)sinR * cosY);
		float cosRsinYStored = (float)((double)cosR * sinY);

		out[0][0] = (float)(-(double)sinRsinYStored * sinP + cosRcosYStored);
		out[0][1] = (float)((double)sinRcosYStored * sinP + cosRsinYStored);
		out[0][2] = (float)(-(double)sinR * cosP);
		out[1][0] = (float)(-(double)sinY * cosP);
		out[1][1] = (float)((double)cosY * cosP);
		out[1][2] = sinP;
		out[2][0] = (float)((double)cosRsinYStored * sinP + sinRcosYStored);
		out[2][1] = (float)(-(double)sinP * cosRcosYStored + sinRsinYStored);
		out[2][2] = (float)((double)cosR * cosP);
		out[3][0] = pos[0];
		out[3][1] = pos[1];
		out[3][2] = pos[2];
		return;
	}

	double sinRsinY = (double)sinR * sinY;
	double sinRcosY = (double)sinR * cosY;
	double cosRcosY = (double)cosR * cosY;
	float sinRcosYStored = (float)sinRcosY;
	double cosRsinY = (double)cosR * sinY;
	float cosRsinYStored = (float)cosRsinY;

	out[0][0] = (float)(-sinRsinY * sinP + cosRcosY);
	out[0][1] = (float)(sinRcosY * sinP + cosRsinYStored);
	out[0][2] = (float)(-(double)sinR * cosP);
	out[1][0] = (float)(-(double)sinY * cosP);
	out[1][1] = (float)((double)cosY * cosP);
	out[1][2] = sinP;
	out[2][0] = (float)(cosRsinY * sinP + sinRcosYStored);
	out[2][1] = (float)(-(double)sinP * cosRcosY + sinRsinY);
	out[2][2] = (float)((double)cosR * cosP);
	out[3][0] = pos[0];
	out[3][1] = pos[1];
	out[3][2] = pos[2];
}

void RenderDroid::buildFromVectorAngle34(RdMatrix34 out, RdVector3 axis, float angle) {
	if (_remastered) {
		buildFromVectorAngle34Remastered(out, axis, angle);
		return;
	}

	float sinAngle, cosAngle;

	_stdMath->sinCos(angle, &sinAngle, &cosAngle);

	double towardsPlusZ = 1.0 - axis[2];
	if (fabs(towardsPlusZ) <= 1e-5f)
		towardsPlusZ = 0.0;

	if (towardsPlusZ == 0.0) {
		out[0][0] = cosAngle;
		out[1][1] = cosAngle;
		out[1][0] = -sinAngle;
		out[0][1] = sinAngle;
		out[0][2] = 0.0f;
		out[1][2] = 0.0f;
		out[2][0] = 0.0f;
		out[2][1] = 0.0f;
		out[2][2] = 1.0f;
		out[3][0] = 0.0f;
		out[3][1] = 0.0f;
		out[3][2] = 0.0f;
		return;
	}

	double towardsMinusZ = (double)axis[2] - -1.0;
	if (fabs(towardsMinusZ) <= 1e-5f)
		towardsMinusZ = 0.0;

	if (towardsMinusZ == 0.0) {
		out[0][0] = cosAngle;
		out[1][1] = cosAngle;
		out[0][1] = -sinAngle;
		out[1][0] = sinAngle;
		out[0][2] = 0.0f;
		out[1][2] = 0.0f;
		out[2][0] = 0.0f;
		out[2][1] = 0.0f;
		out[2][2] = 1.0f;
		out[3][0] = 0.0f;
		out[3][1] = 0.0f;
		out[3][2] = 0.0f;
		return;
	}

	double ax = axis[0];
	double ay = axis[1];
	double az = axis[2];

	float xx = (float)(ax * ax);
	float yy = (float)(ay * ay);
	// The demo multiplies the cosine by xx already rounded to float
	double xxCos = isDemo() ? (double)xx * cosAngle : ax * ax * cosAngle;
	float xxCosStored = (float)xxCos;
	float z = (float)((1.0 - xx) - yy);
	double yyCos = (double)yy * cosAngle;
	float yyCosStored = (float)yyCos;
	float denominator = (float)(1.0 - z);
	double quotient = (xxCos * z + yyCosStored) / denominator;
	float oneMinusCos = (float)(1.0 - cosAngle);
	float xy = (float)(ay * ax);
	float xSin = (float)(sinAngle * ax);
	float ySin = (float)(ay * sinAngle);
	float zSin = (float)(az * sinAngle);
	float zxOneMinusCos = (float)(az * ax * oneMinusCos);
	float yzOneMinusCos = (float)(ay * az * oneMinusCos);
	double xyOneMinusCos = (double)xy * oneMinusCos;

	out[0][0] = (float)(quotient + xx);
	out[1][1] = (float)(((double)yyCosStored * z + xxCosStored) / denominator + yy);
	out[2][2] = (float)(((double)yyCosStored + xxCosStored) + z);
	out[0][1] = (float)(zSin + xyOneMinusCos);
	out[1][0] = (float)(xyOneMinusCos - zSin);
	out[0][2] = (float)(zxOneMinusCos - ySin);
	out[2][0] = (float)(zxOneMinusCos + ySin);
	out[1][2] = (float)(yzOneMinusCos + xSin);
	out[2][1] = (float)(yzOneMinusCos - xSin);
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
}

void RenderDroid::buildRotate34(RdMatrix34 out, RdVector3 pyr) {
	float sinP, cosP, sinY, cosY, sinR, cosR;

	_stdMath->sinCos(pyr[0], &sinP, &cosP);
	_stdMath->sinCos(pyr[1], &sinY, &cosY);
	_stdMath->sinCos(pyr[2], &sinR, &cosR);

	double sinRsinY = (double)sinR * sinY;
	double sinRcosY = (double)sinR * cosY;
	double cosRcosY = (double)cosR * cosY;
	float sinRcosYStored = (float)sinRcosY;
	double cosRsinY = (double)cosR * sinY;
	float cosRsinYStored = (float)cosRsinY;

	out[0][0] = (float)(-sinRsinY * sinP + cosRcosY);
	out[0][1] = (float)(sinRcosY * sinP + cosRsinYStored);
	out[0][2] = (float)(-(double)sinR * cosP);
	out[1][0] = (float)(-(double)sinY * cosP);
	out[1][1] = (float)((double)cosY * cosP);
	out[1][2] = sinP;
	out[2][0] = (float)(cosRsinY * sinP + sinRcosYStored);
	out[2][1] = (float)(-(double)sinP * cosRcosY + sinRsinY);
	out[2][2] = (float)((double)cosR * cosP);
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
}

void RenderDroid::buildScale34(RdMatrix34 out, RdVector3 scale) {
	out[0][0] = scale[0];
	out[0][1] = 0.0f;
	out[0][2] = 0.0f;
	out[1][0] = 0.0f;
	out[1][1] = scale[1];
	out[1][2] = 0.0f;
	out[2][0] = 0.0f;
	out[2][1] = 0.0f;
	out[2][2] = scale[2];
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
}

void RenderDroid::buildTranslate34(RdMatrix34 out, RdVector3 pos) {
	memcpy(out, _identMatrix34, sizeof(RdMatrix34));
	out[3][0] = pos[0];
	out[3][1] = pos[1];
	out[3][2] = pos[2];
}

void RenderDroid::extractAngles34(RdMatrix34 mat, RdVector3 pyr) {
	if (_remastered) {
		extractAngles34Remastered(mat, pyr);
		return;
	}

	float m3Squared = (float)((double)mat[1][0] * mat[1][0]);
	double sumOfSquares = (double)mat[1][1] * mat[1][1] + m3Squared;
	float sumOfSquaresStored = (float)sumOfSquares;
	double lengthExact = sqrt(sumOfSquares);
	float length = (float)lengthExact;
	float negM0 = -mat[0][0];
	float negM1 = -mat[0][1];
	float negM2 = -mat[0][2];
	float m5 = mat[1][2];

	if (lengthExact < 0.001f) {
		double angle = 90.0 - _stdMath->arcSin3(mat[0][0]);
		if ((negM1 > 0.0 && m5 > 0.0) || (negM1 < 0.0 && m5 < 0.0))
			angle = -angle;
		pyr[2] = (float)angle;
		pyr[1] = 0.0f;
	} else {
		double angle = 90.0 - _stdMath->arcSin3((float)((double)mat[1][1] / length));
		if (mat[1][0] > 0.0)
			angle = -angle;
		pyr[1] = (float)angle;
	}

	if (length < 0.001f) {
		pyr[0] = 90.0f;
	} else {
		float ratio = (float)((double)sumOfSquaresStored / length);
		if (ratio < 1.0)
			pyr[0] = (float)(90.0 - _stdMath->arcSin3(ratio));
		else
			pyr[0] = 0.0f;
	}

	if (m5 < 0.0)
		pyr[0] = -pyr[0];

	if (length < 0.001f)
		return;

	double numerator = -(double)mat[1][1] * negM0 + (double)negM1 * mat[1][0];
	float cosValue = (float)(numerator / lengthExact);

	if (cosValue < 1.0) {
		if (cosValue > -1.0)
			pyr[2] = (float)(90.0 - _stdMath->arcSin3(cosValue));
		else
			pyr[2] = 180.0f;
	} else {
		pyr[2] = 0.0f;
	}

	if (negM2 < 0.0)
		pyr[2] = -pyr[2];
}

void RenderDroid::identity34(RdMatrix34 out) {
	memcpy(out, _identMatrix34, sizeof(RdMatrix34));
}

void RenderDroid::invertOrtho34(RdMatrix34 out, RdMatrix34 in) {
	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][2] = in[2][1];
	out[1][0] = in[0][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
	out[0][0] = in[0][0];
	out[1][1] = in[1][1];
	out[2][2] = in[2][2];

	double y = in[3][1];
	double z = in[3][2];
	double x = in[3][0];

	out[3][0] = (float)-((double)in[0][1] * y + (double)in[0][2] * z + x * in[0][0]);
	if (isDemo())
		out[3][1] = (float)-((double)in[1][1] * y + (double)in[1][2] * z + (double)in[1][0] * x);
	else
		out[3][1] = (float)-((double)in[1][1] * y + (double)in[1][0] * x + (double)in[1][2] * z);
	out[3][2] = (float)-((double)in[2][2] * z + (double)in[2][0] * x + (double)in[2][1] * y);
}

void RenderDroid::lookAt(RdMatrix34 out, RdVector3 pos, RdVector3 target, float roll) {
	RdVector3 worldUp = { 0.0f, 0.0f, 1.0f };
	RdVector3 lookingDownUp = { 0.0f, 1.0f, 0.0f };
	RdVector3 lookingUpUp = { -0.0f, -1.0f, -0.0f };
	RdMatrix34 rollMatrix;
	RdVector3 up;

	out[1][0] = (float)((double)target[0] - pos[0]);
	out[1][1] = (float)((double)target[1] - pos[1]);
	out[1][2] = (float)((double)target[2] - pos[2]);
	normalize3Acc(out[1]);

	buildFromVectorAngle34(rollMatrix, out[1], roll);

	double facing = (double)out[1][2] * worldUp[2] + (double)out[1][1] * worldUp[1] + (double)out[1][0] * worldUp[0];
	if (facing < 0.0)
		facing = -facing;

	if (facing <= 0.999)
		transformVector34(up, worldUp, rollMatrix);
	else if (out[1][2] <= 0.0)
		transformVector34(up, lookingDownUp, rollMatrix);
	else
		transformVector34(up, lookingUpUp, rollMatrix);

	out[0][0] = (float)((double)out[1][1] * up[2] - (double)out[1][2] * up[1]);
	out[0][1] = (float)((double)out[1][2] * up[0] - (double)out[1][0] * up[2]);
	out[0][2] = (float)((double)out[1][0] * up[1] - (double)out[1][1] * up[0]);
	normalize3Acc(out[0]);

	out[2][0] = (float)((double)out[0][1] * out[1][2] - (double)out[0][2] * out[1][1]);
	out[2][1] = (float)((double)out[0][2] * out[1][0] - (double)out[1][2] * out[0][0]);
	out[2][2] = (float)((double)out[1][1] * out[0][0] - (double)out[0][1] * out[1][0]);
	normalize3Acc(out[2]);

	out[3][0] = pos[0];
	out[3][1] = pos[1];
	out[3][2] = pos[2];
}

void RenderDroid::multiply34(RdMatrix34 out, RdMatrix34 a, RdMatrix34 b) {
	out[0][0] = (float)((double)a[2][0] * b[0][2] + (double)a[1][0] * b[0][1] + (double)a[0][0] * b[0][0]);
	out[0][1] = (float)((double)a[0][1] * b[0][0] + (double)a[1][1] * b[0][1] + (double)a[2][1] * b[0][2]);
	out[0][2] = (float)((double)a[0][2] * b[0][0] + (double)a[2][2] * b[0][2] + (double)a[1][2] * b[0][1]);
	if (isDemo())
		out[1][0] = (float)((double)b[1][0] * a[0][0] + (double)b[1][2] * a[2][0] + (double)b[1][1] * a[1][0]);
	else
		out[1][0] = (float)((double)b[1][0] * a[0][0] + (double)b[1][1] * a[1][0] + (double)b[1][2] * a[2][0]);

	out[1][1] = (float)((double)b[1][2] * a[2][1] + (double)b[1][1] * a[1][1] + (double)b[1][0] * a[0][1]);
	out[1][2] = (float)((double)b[1][2] * a[2][2] + (double)a[1][2] * b[1][1] + (double)b[1][0] * a[0][2]);
	if (isDemo())
		out[2][0] = (float)((double)b[2][0] * a[0][0] + (double)b[2][1] * a[1][0] + (double)b[2][2] * a[2][0]);
	else
		out[2][0] = (float)((double)b[2][0] * a[0][0] + (double)b[2][2] * a[2][0] + (double)b[2][1] * a[1][0]);

	out[2][1] = (float)((double)b[2][0] * a[0][1] + (double)b[2][1] * a[1][1] + (double)b[2][2] * a[2][1]);
	out[2][2] = (float)((double)b[2][2] * a[2][2] + (double)b[2][0] * a[0][2] + (double)b[2][1] * a[1][2]);
	out[3][0] = (float)((double)b[3][0] * a[0][0] + (double)b[3][2] * a[2][0] + (double)b[3][1] * a[1][0] + a[3][0]);
	out[3][1] = (float)((double)b[3][0] * a[0][1] + (double)b[3][1] * a[1][1] + (double)b[3][2] * a[2][1] + a[3][1]);
	out[3][2] = (float)((double)b[3][1] * a[1][2] + (double)b[3][0] * a[0][2] + (double)b[3][2] * a[2][2] + a[3][2]);
}

void RenderDroid::postMultiply34(RdMatrix34 dst, RdMatrix34 src) {
	RdMatrix34 orig;

	memcpy(orig, dst, sizeof(RdMatrix34));

	if (isDemo()) {
		for (int r = 0; r < 4; r++) {
			float add0 = (r == 3) ? src[3][0] : 0.0f;
			float add1 = (r == 3) ? src[3][1] : 0.0f;
			float add2 = (r == 3) ? src[3][2] : 0.0f;
			double x = (double)src[2][0] * orig[r][2] + (double)src[1][0] * orig[r][1] + (double)orig[r][0] * src[0][0];
			double y = (double)src[2][1] * orig[r][2] + (double)src[1][1] * orig[r][1] + (double)src[0][1] * orig[r][0];
			double z = (double)src[1][2] * orig[r][1] + (double)src[2][2] * orig[r][2] + (double)src[0][2] * orig[r][0];

			if (r == 3) {
				x += add0;
				y += add1;
				z += add2;
			}

			dst[r][0] = (float)x;
			dst[r][1] = (float)y;
			dst[r][2] = (float)z;
		}

		return;
	}

	dst[0][0] = (float)((double)src[1][0] * orig[0][1] + (double)src[2][0] * orig[0][2] + (double)orig[0][0] * src[0][0]);
	dst[0][1] = (float)((double)src[2][1] * orig[0][2] + (double)src[0][1] * orig[0][0] + (double)src[1][1] * orig[0][1]);
	dst[0][2] = (float)((double)src[1][2] * orig[0][1] + (double)src[0][2] * orig[0][0] + (double)src[2][2] * orig[0][2]);
	dst[1][0] = (float)((double)src[1][0] * orig[1][1] + (double)src[2][0] * orig[1][2] + (double)orig[1][0] * src[0][0]);
	dst[1][1] = (float)((double)src[2][1] * orig[1][2] + (double)src[0][1] * orig[1][0] + (double)src[1][1] * orig[1][1]);
	dst[1][2] = (float)((double)src[1][2] * orig[1][1] + (double)src[0][2] * orig[1][0] + (double)src[2][2] * orig[1][2]);
	dst[2][0] = (float)((double)src[1][0] * orig[2][1] + (double)src[2][0] * orig[2][2] + (double)orig[2][0] * src[0][0]);
	dst[2][1] = (float)((double)src[2][1] * orig[2][2] + (double)src[0][1] * orig[2][0] + (double)src[1][1] * orig[2][1]);
	dst[2][2] = (float)((double)src[1][2] * orig[2][1] + (double)src[0][2] * orig[2][0] + (double)src[2][2] * orig[2][2]);
	dst[3][0] = (float)((double)src[1][0] * orig[3][1] + (double)src[2][0] * orig[3][2] + (double)orig[3][0] * src[0][0] + src[3][0]);
	dst[3][1] = (float)((double)src[2][1] * orig[3][2] + (double)src[0][1] * orig[3][0] + (double)src[1][1] * orig[3][1] + src[3][1]);
	dst[3][2] = (float)((double)src[1][2] * orig[3][1] + (double)src[0][2] * orig[3][0] + (double)src[2][2] * orig[3][2] + src[3][2]);
}

void RenderDroid::postTranslate34(RdMatrix34 mat, RdVector3 vec) {
	mat[3][0] = (float)((double)vec[0] + mat[3][0]);
	mat[3][1] = (float)((double)vec[1] + mat[3][1]);
	mat[3][2] = (float)((double)vec[2] + mat[3][2]);
}

void RenderDroid::preMultiply34(RdMatrix34 dst, RdMatrix34 src) {
	RdMatrix34 orig;

	memcpy(orig, dst, sizeof(RdMatrix34));

	if (isDemo()) {
		for (int c = 0; c < 3; c++)
			dst[0][c] = (float)((double)src[0][1] * orig[1][c] + (double)src[0][2] * orig[2][c] + (double)orig[0][c] * src[0][0]);

		for (int r = 1; r < 3; r++) {
			for (int c = 0; c < 3; c++)
				dst[r][c] = (float)((double)src[r][2] * orig[2][c] + (double)src[r][1] * orig[1][c] + (double)src[r][0] * orig[0][c]);
		}

		for (int c = 0; c < 3; c++)
			dst[3][c] = (float)((double)src[3][0] * orig[0][c] + (double)src[3][2] * orig[2][c] + (double)src[3][1] * orig[1][c] + orig[3][c]);

		return;
	}

	dst[0][0] = (float)((double)src[0][1] * orig[1][0] + (double)src[0][2] * orig[2][0] + (double)orig[0][0] * src[0][0]);
	dst[0][1] = (float)((double)src[0][1] * orig[1][1] + (double)src[0][2] * orig[2][1] + (double)orig[0][1] * src[0][0]);
	dst[0][2] = (float)((double)src[0][1] * orig[1][2] + (double)src[0][2] * orig[2][2] + (double)orig[0][2] * src[0][0]);
	dst[1][0] = (float)((double)src[1][1] * orig[1][0] + (double)src[1][0] * orig[0][0] + (double)src[1][2] * orig[2][0]);
	dst[1][1] = (float)((double)src[1][1] * orig[1][1] + (double)src[1][0] * orig[0][1] + (double)src[1][2] * orig[2][1]);
	dst[1][2] = (float)((double)src[1][1] * orig[1][2] + (double)src[1][0] * orig[0][2] + (double)src[1][2] * orig[2][2]);
	dst[2][0] = (float)((double)src[2][1] * orig[1][0] + (double)src[2][0] * orig[0][0] + (double)src[2][2] * orig[2][0]);
	dst[2][1] = (float)((double)src[2][1] * orig[1][1] + (double)src[2][0] * orig[0][1] + (double)src[2][2] * orig[2][1]);
	dst[2][2] = (float)((double)src[2][1] * orig[1][2] + (double)src[2][0] * orig[0][2] + (double)src[2][2] * orig[2][2]);
	dst[3][0] = (float)((double)src[3][1] * orig[1][0] + (double)src[3][0] * orig[0][0] + (double)src[3][2] * orig[2][0] + orig[3][0]);
	dst[3][1] = (float)((double)src[3][1] * orig[1][1] + (double)src[3][0] * orig[0][1] + (double)src[3][2] * orig[2][1] + orig[3][1]);
	dst[3][2] = (float)((double)src[3][1] * orig[1][2] + (double)src[3][0] * orig[0][2] + (double)src[3][2] * orig[2][2] + orig[3][2]);
}

void RenderDroid::preRotate34(RdMatrix34 mat, RdVector3 pyr) {
	RdMatrix34 rotation;

	buildRotate34(rotation, pyr);
	preMultiply34(mat, rotation);
}

void RenderDroid::preScale34(RdMatrix34 mat, RdVector3 scale) {
	RdMatrix34 scaling;

	buildScale34(scaling, scale);
	preMultiply34(mat, scaling);
}

void RenderDroid::transformPoint34(RdVector3 out, RdVector3 pt, RdMatrix34 mat) {
	if (_remastered) {
		transformPoint34Remastered(out, pt, mat);
		return;
	}

	out[0] = (float)((double)mat[2][0] * pt[2] + (double)mat[1][0] * pt[1] + (double)pt[0] * mat[0][0] + mat[3][0]);
	out[1] = (float)((double)mat[2][1] * pt[2] + (double)mat[1][1] * pt[1] + (double)mat[0][1] * pt[0] + mat[3][1]);
	out[2] = (float)((double)mat[1][2] * pt[1] + (double)mat[2][2] * pt[2] + (double)mat[0][2] * pt[0] + mat[3][2]);
}

void RenderDroid::transformPoint34Acc(RdVector3 pt, RdMatrix34 mat) {
	if (_remastered) {
		transformPoint34AccRemastered(pt, mat);
		return;
	}

	float x = (float)((double)mat[2][0] * pt[2] + (double)mat[1][0] * pt[1] + (double)pt[0] * mat[0][0] + mat[3][0]);
	float y = (float)((double)mat[2][1] * pt[2] + (double)mat[1][1] * pt[1] + (double)mat[0][1] * pt[0] + mat[3][1]);
	float z = (float)((double)mat[1][2] * pt[1] + (double)mat[2][2] * pt[2] + (double)mat[0][2] * pt[0] + mat[3][2]);

	pt[0] = x;
	pt[1] = y;
	pt[2] = z;
}

void RenderDroid::transformPointLst34(RdMatrix34 mat, RdVector3 *in, RdVector3 *out, uint num) {
	for (uint i = 0; i < num; i++) {
		out[i][0] = (float)((double)in[i][1] * mat[1][0] + (double)mat[2][0] * in[i][2] + (double)mat[0][0] * in[i][0] + mat[3][0]);
		if (isDemo())
			out[i][1] = (float)((double)mat[0][1] * in[i][0] + (double)mat[1][1] * in[i][1] + (double)mat[2][1] * in[i][2] + mat[3][1]);
		else
			out[i][1] = (float)((double)mat[0][1] * in[i][0] + (double)mat[2][1] * in[i][2] + (double)mat[1][1] * in[i][1] + mat[3][1]);

		out[i][2] = (float)((double)mat[0][2] * in[i][0] + (double)mat[1][2] * in[i][1] + (double)mat[2][2] * in[i][2] + mat[3][2]);
	}
}

void RenderDroid::transformVector34(RdVector3 out, RdVector3 vec, RdMatrix34 mat) {
	if (_remastered) {
		transformVector34Remastered(out, vec, mat);
		return;
	}

	out[0] = (float)((double)mat[2][0] * vec[2] + (double)mat[1][0] * vec[1] + (double)vec[0] * mat[0][0]);
	out[1] = (float)((double)mat[2][1] * vec[2] + (double)mat[1][1] * vec[1] + (double)mat[0][1] * vec[0]);
	out[2] = (float)((double)mat[1][2] * vec[1] + (double)mat[2][2] * vec[2] + (double)mat[0][2] * vec[0]);
}

void RenderDroid::transformVector34Acc(RdVector3 vec, RdMatrix34 mat) {
	if (_remastered) {
		transformVector34AccRemastered(vec, mat);
		return;
	}

	float x = (float)((double)mat[2][0] * vec[2] + (double)mat[1][0] * vec[1] + (double)vec[0] * mat[0][0]);
	float y = (float)((double)mat[2][1] * vec[2] + (double)mat[1][1] * vec[1] + (double)mat[0][1] * vec[0]);
	float z = (float)((double)mat[1][2] * vec[1] + (double)mat[2][2] * vec[2] + (double)mat[0][2] * vec[0]);

	vec[0] = x;
	vec[1] = y;
	vec[2] = z;
}

void RenderDroid::transformVectorOrtho34(RdVector3 out, RdVector3 vec, RdMatrix34 mat) {
	if (_remastered) {
		transformVectorOrtho34Remastered(out, vec, mat);
		return;
	}

	out[0] = (float)((double)mat[0][2] * vec[2] + (double)mat[0][1] * vec[1] + (double)vec[0] * mat[0][0]);
	out[1] = (float)((double)mat[1][2] * vec[2] + (double)mat[1][1] * vec[1] + (double)mat[1][0] * vec[0]);
	out[2] = (float)((double)mat[2][1] * vec[1] + (double)mat[2][2] * vec[2] + (double)mat[2][0] * vec[0]);
}

void RenderDroid::transformPoint34Remastered(RdVector3 out, RdVector3 pt, RdMatrix34 mat) {
	for (int i = 0; i < 3; i++) {
		float first = (float)((double)mat[0][i] * pt[0]);
		float second = (float)((double)mat[1][i] * pt[1]);
		float third = (float)((double)mat[2][i] * pt[2]);

		out[i] = (float)((((double)first + second) + third) + mat[3][i]);
	}
}

void RenderDroid::transformPoint34AccRemastered(RdVector3 pt, RdMatrix34 mat) {
	RdVector3 result;

	transformPoint34Remastered(result, pt, mat);

	pt[0] = result[0];
	pt[1] = result[1];
	pt[2] = result[2];
}

void RenderDroid::transformVector34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat) {
	for (int i = 0; i < 3; i++) {
		float first = (float)((double)mat[0][i] * vec[0]);
		float second = (float)((double)mat[1][i] * vec[1]);
		float third = (float)((double)mat[2][i] * vec[2]);

		out[i] = (float)(((double)first + second) + third);
	}
}

void RenderDroid::transformVector34AccRemastered(RdVector3 vec, RdMatrix34 mat) {
	RdVector3 result;

	transformVector34Remastered(result, vec, mat);

	vec[0] = result[0];
	vec[1] = result[1];
	vec[2] = result[2];
}

void RenderDroid::transformVectorOrtho34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat) {
	for (int i = 0; i < 3; i++) {
		float first = (float)((double)mat[i][0] * vec[0]);
		float second = (float)((double)mat[i][1] * vec[1]);
		float third = (float)((double)mat[i][2] * vec[2]);

		out[i] = (float)(((double)first + second) + third);
	}
}

void RenderDroid::buildFromVectorAngle34Remastered(RdMatrix34 out, RdVector3 axis, float angle) {
	float sinAngle;
	float cosAngle;

	_stdMath->sinCos(angle, &sinAngle, &cosAngle);

	double towardsPlusZ = 1.0 - axis[2];
	double absTowardsPlusZ = towardsPlusZ < 0.0 ? -towardsPlusZ : towardsPlusZ;

	if (!(absTowardsPlusZ > 0.00001f)) {
		out[0][0] = cosAngle;
		out[1][1] = cosAngle;
		out[0][1] = sinAngle;
		out[1][0] = -sinAngle;
		out[0][2] = 0.0f;
		out[1][2] = 0.0f;
		out[2][0] = 0.0f;
		out[2][1] = 0.0f;
		out[2][2] = 1.0f;
		out[3][0] = 0.0f;
		out[3][1] = 0.0f;
		out[3][2] = 0.0f;

		return;
	}

	double towardsMinusZ = (double)axis[2] + 1.0;
	double absTowardsMinusZ = towardsMinusZ < 0.0 ? -towardsMinusZ : towardsMinusZ;

	if (!(absTowardsMinusZ > 0.00001f)) {
		out[0][0] = cosAngle;
		out[1][1] = cosAngle;
		out[0][1] = -sinAngle;
		out[1][0] = sinAngle;
		out[0][2] = 0.0f;
		out[1][2] = 0.0f;
		out[2][0] = 0.0f;
		out[2][1] = 0.0f;
		out[2][2] = 1.0f;
		out[3][0] = 0.0f;
		out[3][1] = 0.0f;
		out[3][2] = 0.0f;

		return;
	}

	float oneMinusCos = (float)(1.0 - cosAngle);
	float xy = (float)((double)axis[1] * axis[0]);
	float xSin = (float)((double)axis[0] * sinAngle);
	float ySin = (float)((double)axis[1] * sinAngle);
	float zSin = (float)((double)sinAngle * axis[2]);
	float xx = (float)((double)axis[0] * axis[0]);
	float yy = (float)((double)axis[1] * axis[1]);
	float z = (float)((1.0 - xx) - yy);
	float denominator = (float)(1.0 - z);
	float xxCos = (float)((double)xx * cosAngle);
	float yyCos = (float)((double)yy * cosAngle);
	float zx = (float)((double)axis[0] * axis[2]);
	float zxOneMinusCos = (float)((double)zx * oneMinusCos);
	float yz = (float)((double)axis[1] * axis[2]);
	float yzOneMinusCos = (float)((double)yz * oneMinusCos);
	double xyOneMinusCos = (double)oneMinusCos * xy;

	out[0][0] = (float)((double)xx + ((double)xxCos * z + yyCos) / denominator);
	out[1][1] = (float)((double)yy + ((double)yyCos * z + xxCos) / denominator);
	out[2][2] = (float)(((double)z + xxCos) + yyCos);
	out[0][1] = (float)((double)zSin + xyOneMinusCos);
	out[1][0] = (float)(xyOneMinusCos - zSin);
	out[0][2] = (float)((double)zxOneMinusCos - ySin);
	out[1][2] = (float)((double)yzOneMinusCos + xSin);
	out[2][0] = (float)((double)ySin + zxOneMinusCos);
	out[2][1] = (float)((double)yzOneMinusCos - xSin);
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
}

void RenderDroid::extractAngles34Remastered(RdMatrix34 mat, RdVector3 pyr) {
	float forwardX = mat[1][0];
	float forwardY = mat[1][1];
	float forwardZ = mat[1][2];
	float negSideX = -mat[0][0];
	float negSideY = -mat[0][1];
	float negSideZ = -mat[0][2];
	double forwardXSquared = (double)forwardX * forwardX;
	double forwardYSquared = (double)forwardY * forwardY;
	float length = (float)sqrt((forwardXSquared + forwardYSquared) + 0.0);

	if (length < 0.001f) {
		float angle = (float)(90.0 - _stdMath->arcSin3(-negSideX));

		if ((negSideY > 0.0f && forwardZ > 0.0f) || (negSideY < 0.0f && forwardZ < 0.0f))
			angle = -angle;

		pyr[2] = angle;
		pyr[1] = 0.0f;
	} else {
		float angle = (float)(90.0 - _stdMath->arcSin3((float)((double)forwardY / length)));

		if (forwardX > 0.0f)
			angle = -angle;

		pyr[1] = angle;
	}

	if (length < 0.001f) {
		pyr[0] = 90.0f;
	} else {
		float ratio = (float)(((forwardYSquared + forwardXSquared) + (double)forwardZ * 0.0) / length);

		if (ratio < 1.0f)
			pyr[0] = (float)(90.0 - _stdMath->arcSin3(ratio));
		else
			pyr[0] = 0.0f;
	}

	if (forwardZ < 0.0f)
		pyr[0] = -pyr[0];

	if (!(length >= 0.001f))
		return;

	double numerator = ((double)forwardX * negSideY + (double)(-forwardY) * negSideX) + (double)negSideZ * 0.0;
	float cosValue = (float)(numerator / length);

	if (cosValue < 1.0f) {
		if (-1.0f < cosValue)
			pyr[2] = (float)(90.0 - _stdMath->arcSin3(cosValue));
		else
			pyr[2] = 180.0f;
	} else {
		pyr[2] = 0.0f;
	}

	if (negSideZ < 0.0f)
		pyr[2] = -pyr[2];
}

} // End of namespace Grim
