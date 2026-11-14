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

void RenderDroid::cross3(RdVector3 out, RdVector3 a, RdVector3 b) {
	if (_remastered) {
		cross3Remastered(out, a, b);
		return;
	}

	out[0] = (float)((double)b[2] * a[1] - (double)a[2] * b[1]);
	out[1] = (float)((double)a[2] * b[0] - (double)b[2] * a[0]);
	out[2] = (float)((double)b[1] * a[0] - (double)a[1] * b[0]);
}

float RenderDroid::dot2(RdVector2 a, RdVector2 b) {
	if (_remastered)
		return dot2Remastered(a, b);

	return (float)((double)b[1] * a[1] + (double)a[0] * b[0]);
}

float RenderDroid::dot3(RdVector3 a, RdVector3 b) {
	if (_remastered)
		return dot3Remastered(a, b);

	return (float)((double)b[1] * a[1] + (double)b[2] * a[2] + (double)a[0] * b[0]);
}

void RenderDroid::cross3Remastered(RdVector3 out, RdVector3 a, RdVector3 b) {
	float first = (float)((double)a[1] * b[2]);
	float second = (float)((double)a[2] * b[1]);

	out[0] = (float)((double)first - second);
	first = (float)((double)a[2] * b[0]);
	second = (float)((double)a[0] * b[2]);
	out[1] = (float)((double)first - second);
	first = (float)((double)b[1] * a[0]);
	second = (float)((double)b[0] * a[1]);
	out[2] = (float)((double)first - second);
}

float RenderDroid::dot2Remastered(RdVector2 a, RdVector2 b) {
	float productY = (float)((double)a[1] * b[1]);
	float productX = (float)((double)a[0] * b[0]);

	return (float)((double)productY + productX);
}

float RenderDroid::dot3Remastered(RdVector3 a, RdVector3 b) {
	float productY = (float)((double)a[1] * b[1]);
	float productX = (float)((double)a[0] * b[0]);
	float productZ = (float)((double)a[2] * b[2]);

	return (float)(((double)productY + productX) + productZ);
}

void RenderDroid::extractAngles(RdVector3 vec, RdVector3 pyr) {
	pyr[2] = 0.0f;
	pyr[0] = _stdMath->arcSin3(vec[2]);
	pyr[1] = _stdMath->arcTan4(vec[1], vec[0]);
}

float RenderDroid::normalize3(RdVector3 out, RdVector3 in) {
	if (_remastered)
		return normalize3Remastered(out, in);

	double length = sqrt((double)in[1] * in[1] + (double)in[2] * in[2] + (double)in[0] * in[0]);

	if (length == 0.0) {
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	} else {
		out[0] = (float)(in[0] / length);
		out[1] = (float)(in[1] / length);
		out[2] = (float)(in[2] / length);
	}

	return (float)length;
}

float RenderDroid::normalize3Remastered(RdVector3 out, RdVector3 in) {
	float squareX = (float)((double)in[0] * in[0]);
	float squareY = (float)((double)in[1] * in[1]);
	float squareZ = (float)((double)in[2] * in[2]);
	float length = (float)sqrt(((double)squareX + squareY) + squareZ);

	if (length == 0.0f) {
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	} else {
		out[0] = (float)((double)in[0] / length);
		out[1] = (float)((double)in[1] / length);
		out[2] = (float)((double)in[2] / length);
	}

	return length;
}

float RenderDroid::normalize2Remastered(float *vec) {
	float length = (float)sqrt((double)vec[0] * vec[0] + (double)vec[1] * vec[1]);

	if (length != 0.0f) {
		vec[0] = (float)((double)vec[0] / length);
		vec[1] = (float)((double)vec[1] / length);
	}

	return length;
}

float RenderDroid::normalize3AccRemastered(RdVector3 vec) {
	float length = (float)sqrt(((double)vec[1] * vec[1] + (double)vec[0] * vec[0]) + (double)vec[2] * vec[2]);

	if (length != 0.0f) {
		float inverse = (float)(1.0 / length);

		vec[0] = (float)(vec[0] * inverse);
		vec[1] = (float)(inverse * vec[1]);
		vec[2] = (float)(inverse * vec[2]);
	}

	return length;
}

float RenderDroid::normalize3Acc(RdVector3 vec) {
	if (_remastered)
		return normalize3AccRemastered(vec);

	double length = sqrt((double)vec[1] * vec[1] + (double)vec[2] * vec[2] + (double)vec[0] * vec[0]);

	if (length != 0.0) {
		double inverse = 1.0 / length;
		float y = (float)(vec[1] * inverse);
		float z = (float)(vec[2] * inverse);
		vec[0] = (float)(inverse * vec[0]);
		vec[1] = y;
		vec[2] = z;
	}

	return (float)length;
}

float RenderDroid::normalize3QuickAcc(RdVector3 vec) {
	float x = (vec[0] >= 0.0) ? vec[0] : -vec[0];
	float y = (vec[1] >= 0.0) ? vec[1] : -vec[1];
	float z = (vec[2] >= 0.0) ? vec[2] : -vec[2];
	float largest;
	float middle;
	float smallest;

	if (z <= y) {
		if (x >= y) {
			largest = x;
			middle = y;
			smallest = z;
		} else if (z <= x) {
			largest = y;
			middle = x;
			smallest = z;
		} else {
			largest = y;
			middle = z;
			smallest = x;
		}
	} else if (z <= x) {
		largest = x;
		middle = z;
		smallest = y;
	} else {
		largest = z;
		if (y < x) {
			middle = x;
			smallest = y;
		} else {
			middle = y;
			smallest = x;
		}
	}

	float length = (float)(0.34375 * middle + 0.25 * smallest + largest);
	float inverse = (float)(1.0 / length);

	vec[0] = (float)((double)vec[0] * inverse);
	vec[1] = (float)((double)vec[1] * inverse);
	vec[2] = (float)((double)vec[2] * inverse);

	return length;
}

void RenderDroid::rotate3Acc(RdVector3 vec, RdVector3 pyr) {
	RdMatrix34 rotation;

	buildRotate34(rotation, pyr);
	transformVector34Acc(vec, rotation);
}

} // End of namespace Grim
