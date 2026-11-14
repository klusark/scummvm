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

void RenderDroid::calcSurfaceNormal(RdVector3 out, RdVector3 p0, RdVector3 p1, RdVector3 p2) {
	RdVector3 first;
	RdVector3 second;

	first[0] = (float)((double)p1[0] - p0[0]);
	first[1] = (float)((double)p1[1] - p0[1]);
	first[2] = (float)((double)p1[2] - p0[2]);
	second[0] = (float)((double)p2[0] - p0[0]);
	second[1] = (float)((double)p2[1] - p0[1]);
	second[2] = (float)((double)p2[2] - p0[2]);

	normalize3Acc(first);
	normalize3Acc(second);

	out[0] = (float)((double)second[2] * first[1] - (double)second[1] * first[2]);
	out[1] = (float)((double)second[0] * first[2] - (double)second[2] * first[0]);
	out[2] = (float)((double)second[1] * first[0] - (double)second[0] * first[1]);

	normalize3Acc(out);
	clampVector(out, 0.000001f);
}

void RenderDroid::clampVector(RdVector3 vec, float eps) {
	for (int i = 0; i < 3; i++) {
		double magnitude = (vec[i] < 0.0) ? -(double)vec[i] : (double)vec[i];
		if (magnitude < eps)
			vec[i] = 0.0f;
	}
}

int RenderDroid::pointsCollinearRemastered(RdVector3 a, RdVector3 b, RdVector3 c) {
	RdVector3 first;
	RdVector3 second;

	first[0] = (float)((double)b[0] - a[0]);
	first[1] = (float)((double)b[1] - a[1]);
	first[2] = (float)((double)b[2] - a[2]);
	second[0] = (float)((double)c[0] - a[0]);
	second[1] = (float)((double)c[1] - a[1]);
	second[2] = (float)((double)c[2] - a[2]);

	normalize3Acc(first);
	normalize3Acc(second);

	float dot = (float)fabs(((double)second[1] * first[1] + (double)second[0] * first[0]) + (double)second[2] * first[2]);

	return dot >= 0.999f && dot <= 1.001f;
}

int RenderDroid::pointsCollinear(RdVector3 a, RdVector3 b, RdVector3 c) {
	if (_remastered)
		return pointsCollinearRemastered(a, b, c);

	RdVector3 first;
	RdVector3 second;

	first[0] = (float)((double)b[0] - a[0]);
	first[1] = (float)((double)b[1] - a[1]);
	first[2] = (float)((double)b[2] - a[2]);
	second[0] = (float)((double)c[0] - a[0]);
	second[1] = (float)((double)c[1] - a[1]);
	second[2] = (float)((double)c[2] - a[2]);

	normalize3Acc(first);
	normalize3Acc(second);

	double dot = (double)second[0] * first[0] + (double)second[1] * first[1] + (double)second[2] * first[2];
	if (dot < 0.0)
		dot = -dot;

	return dot >= 0.999f && dot <= 1.001f;
}

} // End of namespace Grim
