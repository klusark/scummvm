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

#include <math.h>

#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/puck.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/std/std_math.h"
#include "engines/grim/walkplane.h"

namespace Grim {

static const int kActorFlagNotMain = 0x80;

void SputActorModule::getOuttaTheBox(float *point, const RdBound3 &box, RdMatrix34 matrix, float *origin) {
	RdVector3 local;

	local[0] = (float)((double)point[0] - origin[0]);
	local[1] = (float)((double)point[1] - origin[1]);
	local[2] = (float)((double)point[2] - origin[2]);

	RdVector3 out;

	_rd->transformVectorOrtho34(out, local, matrix);

	if (!(out[2] > box.min[2] && out[1] > box.min[1] && out[1] < box.max[1] && out[0] > box.min[0] && out[0] < box.max[0] && out[2] < box.max[2]))
		return;

	double best = 3.4028234663852886e38;

	if (box.max[1] > out[1]) {
		double distance = (double)box.max[1] - out[1];

		if (distance < 0.0)
			distance = -distance;

		if (distance < best) {
			best = (float)distance;
			local[0] = out[0];
			local[2] = out[2];
			local[1] = (float)((double)box.max[1] - -0.009999999776482582);
		}
	}

	if (box.min[1] < out[1]) {
		double distance = (double)box.min[1] - out[1];

		if (distance < 0.0)
			distance = -distance;

		if (distance < best) {
			best = (float)distance;
			local[0] = out[0];
			local[2] = out[2];
			local[1] = (float)((double)box.min[1] - 0.009999999776482582);
		}
	}

	if (box.max[0] > out[0]) {
		double distance = (double)box.max[0] - out[0];

		if (distance < 0.0)
			distance = -distance;

		if (distance < best) {
			best = (float)distance;
			local[1] = out[1];
			local[2] = out[2];
			local[0] = (float)((double)box.max[0] - -0.009999999776482582);
		}
	}

	if (box.min[0] < out[0]) {
		double distance = (double)box.min[0] - out[0];

		if (distance < 0.0)
			distance = -distance;

		if (distance < best) {
			local[1] = out[1];
			local[2] = out[2];
			local[0] = (float)((double)box.min[0] - 0.009999999776482582);
		}
	}

	_rd->transformPoint34(point, local, matrix);
}

int SputActorModule::checkCollisions(float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix, float *offset) {
	RdVector3 offsetFrom;
	RdVector3 offsetTo;

	offsetFrom[0] = (float)((double)offset[0] + from[0]);
	offsetFrom[1] = (float)((double)from[1] + offset[1]);
	offsetFrom[2] = (float)((double)from[2] + offset[2]);
	offsetTo[0] = (float)((double)offset[0] + to[0]);
	offsetTo[1] = (float)((double)to[1] + offset[1]);
	offsetTo[2] = (float)((double)to[2] + offset[2]);

	if (testCollision(offsetFrom, offsetTo, center, box, matrix))
		return 0;

	return _walkSystem->buildPath(from, to, 0);
}

void SputActorModule::findObjectCorners(RdModel3 *model, float margin, RdMatrix34 mat, float z, ObjectCorners *out) {
	if (mat[2][2] < 0.999)
		return;

	out->corner[0][0] = model->bound.min[0] - margin;
	out->corner[0][1] = model->bound.min[1] - margin;
	out->corner[0][2] = model->bound.min[2];
	out->corner[1][0] = margin + model->bound.max[0];
	out->corner[1][1] = model->bound.min[1] - margin;
	out->corner[1][2] = model->bound.min[2];
	out->corner[2][0] = margin + model->bound.max[0];
	out->corner[2][1] = margin + model->bound.max[1];
	out->corner[2][2] = model->bound.min[2];
	out->corner[3][0] = model->bound.min[0] - margin;
	out->corner[3][1] = margin + model->bound.max[1];
	out->corner[3][2] = model->bound.min[2];

	for (int i = 0; i < 4; i++) {
		_rd->transformPoint34Acc(out->corner[i], mat);
		out->corner[i][2] = z;
		out->valid[i] = 1;
	}
}

int SputActorModule::testCollision(float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix) {
	RdVector3 local;
	RdVector3 start;
	RdVector3 end;

	local[0] = (float)((double)from[0] - center[0]);
	local[1] = (float)((double)from[1] - center[1]);
	local[2] = (float)((double)from[2] - center[2]);
	_rd->transformVectorOrtho34(start, local, matrix);

	local[0] = (float)((double)to[0] - center[0]);
	local[1] = (float)((double)to[1] - center[1]);
	local[2] = (float)((double)to[2] - center[2]);
	_rd->transformVectorOrtho34(end, local, matrix);

	float dir[3];

	dir[0] = (float)((double)end[0] - start[0]);
	dir[1] = (float)((double)end[1] - start[1]);
	dir[2] = (float)((double)end[2] - start[2]);

	float best = 1.0f;

	if (start[1] > box.max[1] && end[1] < box.max[1]) {
		double t = ((double)box.max[1] - start[1]) / dir[1];
		float tF = (float)t;

		if (t < 1.0) {
			double xD = (double)tF * dir[0] + start[0];
			float x = (float)xD;
			float y = (float)((double)tF * dir[1] + start[1]);
			float z = (float)((double)tF * dir[2] + start[2]);

			if (xD > box.min[0] && x < box.max[0]) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = tF;
			}
		}
	}

	if (start[1] < box.min[1] && end[1] > box.min[1]) {
		double t = ((double)box.min[1] - start[1]) / dir[1];
		float tF = (float)t;

		if (t < best) {
			double xD = (double)tF * dir[0] + start[0];
			float x = (float)xD;
			float y = (float)((double)tF * dir[1] + start[1]);
			float z = (float)((double)tF * dir[2] + start[2]);

			if (xD > box.min[0] && x < box.max[0]) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = tF;
			}
		}
	}

	if (start[0] > box.max[0] && end[0] < box.max[0]) {
		double t = ((double)box.max[0] - start[0]) / dir[0];
		float tF = (float)t;

		if (t < best) {
			double yD = (double)tF * dir[1] + start[1];
			float y = (float)yD;
			float x = (float)((double)tF * dir[0] + start[0]);
			float z = (float)((double)tF * dir[2] + start[2]);

			if (yD > box.min[1] && y < box.max[1]) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = tF;
			}
		}
	}

	if (start[0] < box.min[0] && end[0] > box.min[0]) {
		double t = ((double)box.min[0] - start[0]) / dir[0];
		float tF = (float)t;

		if (t < best) {
			double yD = (double)tF * dir[1] + start[1];
			float y = (float)yD;
			float x = (float)((double)tF * dir[0] + start[0]);
			float z = (float)((double)tF * dir[2] + start[2]);

			if (yD > box.min[1] && y < box.max[1]) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = tF;
			}
		}
	}

	return best != 1.0f;
}

int SputActorModule::findClosestCandidate(float *from, float *to, ObjectCorners *corners) {
	int closest = -1;
	float best = 3.4028235e38f;

	for (int i = 0; i < 4; i++) {
		if (!corners->valid[i])
			continue;

		float *corner = corners->corner[i];
		double dx1 = (double)from[0] - corner[0];
		double dy1 = (double)from[1] - corner[1];
		double dy2 = (double)corner[1] - to[1];
		double dz2 = (double)corner[2] - to[2];
		double dz1 = (double)from[2] - corner[2];
		double dx2 = (double)corner[0] - to[0];
		float dx1F = (float)dx1;
		float dy1F = (float)dy1;
		float dz1F = (float)dz1;
		float dy2F = (float)dy2;
		float dz2F = (float)dz2;

		double distance = ((dx1 * dx1F + dy1 * dy1F) + dz1 * dz1F) + ((dy2 * dy2F + dz2 * dz2F) + dx2 * dx2);

		if (distance < best) {
			best = (float)distance;
			closest = i;
		}
	}

	return closest;
}

void SputActorModule::getOuttaTheBoxRemastered(float *point, const RdBound3 &box, RdMatrix34 matrix, float *origin) {
	RdVector3 local;
	RdVector3 out;

	local[0] = (float)((double)point[0] - origin[0]);
	local[1] = (float)((double)point[1] - origin[1]);
	local[2] = (float)((double)point[2] - origin[2]);

	_rd->transformVectorOrtho34Remastered(out, local, matrix);

	if (!(box.min[2] < out[2] && box.min[1] < out[1] && box.max[1] > out[1] && box.min[0] < out[0] && box.max[0] > out[0] && box.max[2] > out[2]))
		return;

	float best = 3.4028235e38f;

	if (box.max[1] > out[1]) {
		float distance = (float)fabs((double)box.max[1] - out[1]);

		if (distance < 3.4028234663852886e38) {
			local[0] = out[0];
			local[1] = (float)((double)box.max[1] + 0.009999999776482582);
			local[2] = out[2];
			best = distance;
		}
	}

	if (box.min[1] < out[1]) {
		float distance = (float)fabs((double)box.min[1] - out[1]);

		if ((double)best > distance) {
			local[0] = out[0];
			local[1] = (float)((double)box.min[1] - 0.009999999776482582);
			local[2] = out[2];
			best = distance;
		}
	}

	if (box.max[0] > out[0]) {
		float distance = (float)fabs((double)box.max[0] - out[0]);

		if ((double)best > distance) {
			local[0] = (float)((double)box.max[0] + 0.009999999776482582);
			local[1] = out[1];
			local[2] = out[2];
			best = distance;
		}
	}

	if (box.min[0] < out[0]) {
		float distance = (float)fabs((double)box.min[0] - out[0]);

		if ((double)best > distance) {
			local[0] = (float)((double)box.min[0] - 0.009999999776482582);
			local[1] = out[1];
			local[2] = out[2];
		}
	}

	_rd->transformPoint34Remastered(point, local, matrix);
}

int SputActorModule::checkCollisionsRemastered(SputActor *actor, float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix, float *offset) {
	RdVector3 offsetFrom;
	RdVector3 offsetTo;
	unsigned int failFlag = 1;

	offsetFrom[0] = (float)((double)from[0] + offset[0]);
	offsetFrom[1] = (float)((double)from[1] + offset[1]);
	offsetFrom[2] = (float)((double)from[2] + offset[2]);
	offsetTo[0] = (float)((double)to[0] + offset[0]);
	offsetTo[1] = (float)((double)to[1] + offset[1]);
	offsetTo[2] = (float)((double)to[2] + offset[2]);

	if (testCollisionRemastered(offsetFrom, offsetTo, center, box, matrix))
		return 0;

	return buildPath(actor, from, to, 0, &failFlag, nullptr);
}

void SputActorModule::findObjectCornersRemastered(RdModel3 *model, float margin, RdMatrix34 mat, float z, ObjectCorners *out) {
	if (mat[2][2] < 0.999)
		return;

	out->corner[0][0] = (float)((double)model->bound.min[0] - margin);
	out->corner[0][1] = (float)((double)model->bound.min[1] - margin);
	out->corner[0][2] = model->bound.min[2];
	out->corner[1][0] = (float)((double)model->bound.max[0] + margin);
	out->corner[1][1] = (float)((double)model->bound.min[1] - margin);
	out->corner[1][2] = model->bound.min[2];
	out->corner[2][0] = (float)((double)model->bound.max[0] + margin);
	out->corner[2][1] = (float)((double)margin + model->bound.max[1]);
	out->corner[2][2] = model->bound.min[2];
	out->corner[3][0] = (float)((double)model->bound.min[0] - margin);
	out->corner[3][1] = (float)((double)margin + model->bound.max[1]);
	out->corner[3][2] = model->bound.min[2];

	for (int i = 0; i < 4; i++) {
		_rd->transformPoint34AccRemastered(out->corner[i], mat);
		out->corner[i][2] = z;
		out->valid[i] = 1;
	}
}

int SputActorModule::testCollisionRemastered(float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix) {
	RdVector3 local;
	RdVector3 start;
	RdVector3 end;

	local[0] = (float)((double)from[0] - center[0]);
	local[1] = (float)((double)from[1] - center[1]);
	local[2] = (float)((double)from[2] - center[2]);
	_rd->transformVectorOrtho34Remastered(start, local, matrix);

	local[0] = (float)((double)to[0] - center[0]);
	local[1] = (float)((double)to[1] - center[1]);
	local[2] = (float)((double)to[2] - center[2]);
	_rd->transformVectorOrtho34Remastered(end, local, matrix);

	float dirX = (float)((double)end[0] - start[0]);
	float dirY = (float)((double)end[1] - start[1]);
	float dirZ = (float)((double)end[2] - start[2]);
	float best = 1.0f;

	if (box.max[1] < start[1] && end[1] < box.max[1]) {
		float t = (float)(((double)box.max[1] - start[1]) / dirY);

		if (t < 1.0f) {
			float x = (float)((double)(float)((double)t * dirX) + start[0]);
			float y = (float)((double)(float)((double)t * dirY) + start[1]);
			float z = (float)((double)(float)((double)t * dirZ) + start[2]);

			if (box.min[0] < x && box.max[0] > x) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = t;
			}
		}
	}

	if (box.min[1] > start[1] && end[1] > box.min[1]) {
		float t = (float)(((double)box.min[1] - start[1]) / dirY);

		if ((double)best > t) {
			float x = (float)((double)(float)((double)t * dirX) + start[0]);
			float y = (float)((double)(float)((double)t * dirY) + start[1]);
			float z = (float)((double)(float)((double)t * dirZ) + start[2]);

			if (box.min[0] < x && box.max[0] > x) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = t;
			}
		}
	}

	if (box.max[0] < start[0] && end[0] < box.max[0]) {
		float t = (float)(((double)box.max[0] - start[0]) / dirX);

		if ((double)best > t) {
			float x = (float)((double)(float)((double)t * dirX) + start[0]);
			float y = (float)((double)(float)((double)t * dirY) + start[1]);
			float z = (float)((double)(float)((double)t * dirZ) + start[2]);

			if (box.min[1] < y && box.max[1] > y) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = t;
			}
		}
	}

	if (box.min[0] > start[0] && end[0] > box.min[0]) {
		float t = (float)(((double)box.min[0] - start[0]) / dirX);

		if ((double)best > t) {
			float x = (float)((double)(float)((double)t * dirX) + start[0]);
			float y = (float)((double)(float)((double)t * dirY) + start[1]);
			float z = (float)((double)(float)((double)t * dirZ) + start[2]);

			if (box.min[1] < y && box.max[1] > y) {
				_collisionImpact[0] = x;
				_collisionImpact[1] = y;
				_collisionImpact[2] = z;
				best = t;
			}
		}
	}

	return best != 1.0f;
}

int SputActorModule::findClosestCandidateRemastered(float *from, float *to, ObjectCorners *corners) {
	int closest = -1;
	float best = 3.4028235e38f;

	for (int i = 0; i < 4; i++) {
		if (!corners->valid[i])
			continue;

		float *corner = corners->corner[i];
		double fromDeltaY = (double)from[1] - corner[1];
		double fromDeltaX = (double)from[0] - corner[0];
		double fromDeltaZ = (double)from[2] - corner[2];
		double toDeltaY = (double)corner[1] - to[1];
		double toDeltaX = (double)corner[0] - to[0];
		double toDeltaZ = (double)corner[2] - to[2];
		float toSquareX = (float)(toDeltaX * toDeltaX);
		float toSquareY = (float)(toDeltaY * toDeltaY);
		double toPart = ((double)toSquareX + toSquareY) + (float)(toDeltaZ * toDeltaZ);
		float fromSquareX = (float)(fromDeltaX * fromDeltaX);
		float fromSquareY = (float)(fromDeltaY * fromDeltaY);
		double fromPart = ((double)fromSquareX + fromSquareY) + (float)(fromDeltaZ * fromDeltaZ);
		float distance = (float)(fromPart + toPart);

		if ((double)best > distance) {
			best = distance;
			closest = i;
		}
	}

	return closest;
}

int SputActorModule::sweepIntoBox(float *from, float *to, const RdBound3 &box, float *outPos) {
	int face = 0;
	double dx = (double)from[0] - to[0];
	float dxF = (float)dx;
	double dy = (double)from[1] - to[1];
	float dyF = (float)dy;
	double dz = (double)from[2] - to[2];
	float toX = (float)(dx * -10.0 + to[0]);
	float toY = (float)(dy * -10.0 + to[1]);
	float toZ = (float)(dz * -10.0 + to[2]);

	to[0] = toX;
	to[1] = toY;
	to[2] = toZ;

	double best = 3.4028234663852886e38;
	RdVector3 normal = { 0.0f, 0.0f, 0.0f };

	if ((double)box.max[0] - 0.002 < toX) {
		double denominator = dxF < 0.0f ? -dxF : dxF;
		double t = ((double)box.max[0] - from[0]) / denominator;
		float tF = (float)t;
		double tAbs = t < 0.0 ? -t : t;

		if (tAbs < 3.4028234663852886e38) {
			best = -tF;
			face = 1;
			normal[0] = 1.0f;
			normal[1] = 0.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.min[0] - -0.002 > toX) {
		double denominator = dxF < 0.0f ? -dxF : dxF;
		double t = ((double)box.min[0] - from[0]) / denominator;
		float tF = (float)t;
		double tAbs = t < 0.0 ? -t : t;
		double bestAbs = best < 0.0 ? -best : best;

		if (tAbs < bestAbs) {
			best = tF;
			face = 2;
			normal[0] = -1.0f;
			normal[1] = 0.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.max[1] - 0.002 < toY) {
		double denominator = dyF < 0.0f ? -dyF : dyF;
		double t = ((double)box.max[1] - from[1]) / denominator;
		float tF = (float)t;
		double tAbs = t < 0.0 ? -t : t;
		double bestAbs = best < 0.0 ? -best : best;

		if (tAbs < bestAbs) {
			best = -tF;
			face = 3;
			normal[0] = 0.0f;
			normal[1] = 1.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.min[1] - -0.002 > toY) {
		double denominator = dyF < 0.0f ? -dyF : dyF;
		double t = ((double)box.min[1] - from[1]) / denominator;
		float tF = (float)t;
		double tAbs = t < 0.0 ? -t : t;
		double bestAbs = best < 0.0 ? -best : best;

		if (tAbs < bestAbs) {
			best = tF;
			face = 4;
			normal[0] = 0.0f;
			normal[1] = -1.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.max[2] - 0.002 < toZ) {
		double denominator = dz < 0.0 ? -dz : dz;
		double t = ((double)box.max[2] - from[2]) / denominator;
		float tF = (float)t;
		double tAbs = t < 0.0 ? -t : t;
		double bestAbs = best < 0.0 ? -best : best;

		if (tAbs < bestAbs) {
			best = -tF;
			face = 5;
			normal[0] = 0.0f;
			normal[1] = 0.0f;
			normal[2] = 1.0f;
		}
	}

	if ((double)box.min[2] - -0.002 > toZ) {
		double denominator = dz < 0.0 ? -dz : dz;
		double t = ((double)box.min[2] - from[2]) / denominator;
		float tF = (float)t;
		double tAbs = t < 0.0 ? -t : t;
		double bestAbs = best < 0.0 ? -best : best;

		if (tAbs < bestAbs) {
			best = tF;
			face = 6;
			normal[0] = 0.0f;
			normal[1] = 0.0f;
			normal[2] = -1.0f;
		}
	}

	if (!face)
		return 0;

	outPos[0] = (float)(best * dxF + from[0]);
	outPos[1] = (float)(best * dyF + from[1]);
	outPos[2] = (float)(best * dz + from[2]);
	_collisionImpact[0] = outPos[0];
	_collisionImpact[1] = outPos[1];
	_collisionImpact[2] = outPos[2];

	double bestAbs = best < 0.0 ? -best : best;
	RdVector3 sweep;

	sweep[0] = (float)(dxF * bestAbs);
	sweep[1] = (float)(dyF * bestAbs);
	sweep[2] = (float)(dz * bestAbs);

	float length = _rd->normalize3Acc(sweep);
	float dotValue = _rd->dot3(sweep, normal);
	double angle = 90.0 - _stdMath->arcSin3(dotValue);
	float angleF = (float)angle;

	if (angle < 90.0)
		return 0;

	RdVector3 axis;

	axis[0] = (float)((double)normal[2] * sweep[1] - (double)normal[1] * sweep[2]);
	axis[1] = (float)((double)normal[0] * sweep[2] - (double)normal[2] * sweep[0]);
	axis[2] = (float)((double)normal[1] * sweep[0] - (double)normal[0] * sweep[1]);
	_rd->normalize3Acc(axis);

	float turnAngle = (float)((double)angleF - 90.0);
	RdMatrix34 turnMatrix;

	_rd->buildFromVectorAngle34(turnMatrix, axis, turnAngle);
	_rd->transformVector34Acc(sweep, turnMatrix);

	outPos[0] = (float)((double)length * sweep[0] + outPos[0]);
	outPos[1] = (float)((double)length * sweep[1] + outPos[1]);
	outPos[2] = (float)((double)length * sweep[2] + outPos[2]);

	return 1;
}

int SputActorModule::sweepIntoBoxRemastered(float *from, float *to, const RdBound3 &box, float *outPos) {
	int face = 0;
	float best = 3.4028235e38f;
	RdVector3 delta;
	RdVector3 normal = { 0.0f, 0.0f, 0.0f };

	for (int i = 0; i < 3; i++)
		delta[i] = (float)((double)from[i] - to[i]);

	for (int i = 0; i < 3; i++) {
		float step = (float)((double)delta[i] * -10.0);

		to[i] = (float)((double)step + to[i]);
	}

	if ((double)box.max[0] - 0.002 < to[0]) {
		float denominator = delta[0] < 0.0f ? -delta[0] : delta[0];
		float t = (float)(((double)box.max[0] - from[0]) / denominator);
		float tAbs = t < 0.0f ? -t : t;

		if (tAbs < 3.4028234663852886e38) {
			face = 1;
			best = -t;
			normal[0] = 1.0f;
			normal[1] = 0.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.min[0] + 0.002 > to[0]) {
		float denominator = delta[0] < 0.0f ? -delta[0] : delta[0];
		float t = (float)(((double)box.min[0] - from[0]) / denominator);
		float tAbs = t < 0.0f ? -t : t;
		float bestAbs = best < 0.0f ? -best : best;

		if (bestAbs > tAbs) {
			face = 2;
			best = t;
			normal[0] = -1.0f;
			normal[1] = 0.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.max[1] - 0.002 < to[1]) {
		float denominator = delta[1] < 0.0f ? -delta[1] : delta[1];
		float t = (float)(((double)box.max[1] - from[1]) / denominator);
		float tAbs = t < 0.0f ? -t : t;
		float bestAbs = best < 0.0f ? -best : best;

		if (bestAbs > tAbs) {
			face = 3;
			best = -t;
			normal[0] = 0.0f;
			normal[1] = 1.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.min[1] + 0.002 > to[1]) {
		float denominator = delta[1] < 0.0f ? -delta[1] : delta[1];
		float t = (float)(((double)box.min[1] - from[1]) / denominator);
		float tAbs = t < 0.0f ? -t : t;
		float bestAbs = best < 0.0f ? -best : best;

		if (bestAbs > tAbs) {
			face = 4;
			best = t;
			normal[0] = 0.0f;
			normal[1] = -1.0f;
			normal[2] = 0.0f;
		}
	}

	if ((double)box.max[2] - 0.002 < to[2]) {
		float denominator = delta[2] < 0.0f ? -delta[2] : delta[2];
		float t = (float)(((double)box.max[2] - from[2]) / denominator);
		float tAbs = t < 0.0f ? -t : t;
		float bestAbs = best < 0.0f ? -best : best;

		if (bestAbs > tAbs) {
			face = 5;
			best = -t;
			normal[0] = 0.0f;
			normal[1] = 0.0f;
			normal[2] = 1.0f;
		}
	}

	if ((double)box.min[2] + 0.002 > to[2]) {
		float denominator = delta[2] < 0.0f ? -delta[2] : delta[2];
		float t = (float)(((double)box.min[2] - from[2]) / denominator);
		float tAbs = t < 0.0f ? -t : t;
		float bestAbs = best < 0.0f ? -best : best;

		if (bestAbs > tAbs) {
			face = 6;
			best = t;
			normal[0] = 0.0f;
			normal[1] = 0.0f;
			normal[2] = -1.0f;
		}
	}

	if (!face)
		return 0;

	for (int i = 0; i < 3; i++) {
		float step = (float)((double)delta[i] * best);

		outPos[i] = (float)((double)step + from[i]);
	}

	_collisionImpact[0] = outPos[0];
	_collisionImpact[1] = outPos[1];
	_collisionImpact[2] = outPos[2];

	float bestAbs = best < 0.0f ? -best : best;
	RdVector3 sweep;

	for (int i = 0; i < 3; i++)
		sweep[i] = (float)((double)delta[i] * bestAbs);

	float length = _rd->normalize3AccRemastered(sweep);
	float dotValue = _rd->dot3Remastered(sweep, normal);
	float angle = (float)(90.0 - _stdMath->arcSin3(dotValue));

	if (angle < 90.0f)
		return 0;

	RdVector3 axis;
	float firstProduct = (float)((double)normal[2] * sweep[1]);
	float secondProduct = (float)((double)normal[1] * sweep[2]);

	axis[0] = (float)((double)firstProduct - secondProduct);
	firstProduct = (float)((double)sweep[2] * normal[0]);
	secondProduct = (float)((double)normal[2] * sweep[0]);
	axis[1] = (float)((double)firstProduct - secondProduct);
	firstProduct = (float)((double)sweep[0] * normal[1]);
	secondProduct = (float)((double)sweep[1] * normal[0]);
	axis[2] = (float)((double)firstProduct - secondProduct);
	_rd->normalize3AccRemastered(axis);

	float turnAngle = (float)((double)angle - 90.0);
	RdMatrix34 turnMatrix;

	_rd->buildFromVectorAngle34Remastered(turnMatrix, axis, turnAngle);
	_rd->transformVector34AccRemastered(sweep, turnMatrix);

	for (int i = 0; i < 3; i++) {
		float step = (float)((double)length * sweep[i]);

		outPos[i] = (float)((double)step + outPos[i]);
	}

	return 1;
}

int SputActorModule::findSlideCandidates(float *prevCenter, float *otherCenter, float radius, float moved, float *firstPoint, float *secondPoint) {
	double deltaY = (double)otherCenter[1] - prevCenter[1];
	double deltaX = (double)otherCenter[0] - prevCenter[0];
	float squareX = (float)(deltaX * deltaX);
	float squareY = (float)(deltaY * deltaY);
	float distance = (float)sqrt((double)squareX + squareY);
	float distanceSquared = (float)((double)distance * distance);
	float radiusSquared = (float)((double)radius * radius);
	float movedSquared = (float)((double)moved * moved);
	float sum = (float)(((double)distanceSquared - movedSquared) + radiusSquared);
	double twice = (double)distance + distance;
	float sumSquared = (float)((double)sum * sum);
	float discriminant = (float)((double)radiusSquared * ((double)distanceSquared * 4.0) - sumSquared);

	if (discriminant < 0.0f)
		return 0;

	if (twice == 0.0) {
		warning("SputActorModule::findSlideCandidates(): the previous position is on the center of the actor");
		return 0;
	}

	float centerDistance = (float)((double)sum / twice);
	float root = (float)sqrt((double)discriminant);
	float halfChord = (float)((double)root / twice);
	float direction[2];

	direction[0] = (float)deltaX;
	direction[1] = (float)deltaY;
	_rd->normalize2Remastered(direction);

	float negativeDirectionY = -direction[1];
	double along = (double)distance - centerDistance;
	RdVector3 base;
	float step = (float)((double)direction[0] * along);

	base[0] = (float)((double)step + prevCenter[0]);
	step = (float)((double)direction[1] * along);
	base[1] = (float)((double)step + prevCenter[1]);
	step = (float)(along * 0.0);
	base[2] = (float)((double)step + prevCenter[2]);

	float offsetX = (float)((double)negativeDirectionY * halfChord);
	float offsetY = (float)((double)direction[0] * halfChord);
	float offsetZ = (float)((double)halfChord * 0.0);

	firstPoint[0] = (float)((double)offsetX + base[0]);
	firstPoint[1] = (float)((double)offsetY + base[1]);
	firstPoint[2] = (float)((double)offsetZ + base[2]);

	double backStep = (double)halfChord * -2.0;
	float backX = (float)((double)negativeDirectionY * backStep);
	float backY = (float)((double)direction[0] * backStep);
	float backZ = (float)(backStep * 0.0);

	secondPoint[0] = (float)((double)backX + firstPoint[0]);
	secondPoint[1] = (float)((double)backY + firstPoint[1]);
	secondPoint[2] = (float)((double)backZ + firstPoint[2]);

	return 1;
}

int SputActorModule::findSlidePoint(float *otherCenter, float *prevCenter, float *center, float reach) {
	float deltaZ = (float)((double)otherCenter[2] - center[2]);
	float reachSquared = (float)((double)reach * reach);
	float deltaZSquared = (float)((double)deltaZ * deltaZ);
	float radiusSquared = (float)((double)reachSquared - deltaZSquared);

	if (radiusSquared < 0.0f)
		return 0;

	double prevDeltaY = (double)prevCenter[1] - center[1];
	double prevDeltaX = (double)prevCenter[0] - center[0];
	double prevDeltaZ = (double)prevCenter[2] - center[2];
	float prevSquareX = (float)(prevDeltaX * prevDeltaX);
	float prevSquareY = (float)(prevDeltaY * prevDeltaY);
	float prevSquareZ = (float)(prevDeltaZ * prevDeltaZ);
	float moved = (float)sqrt(((double)prevSquareX + prevSquareY) + prevSquareZ);
	float radius = (float)sqrt((double)radiusSquared);
	RdVector3 firstPoint;
	RdVector3 secondPoint;

	if (!findSlideCandidates(prevCenter, otherCenter, radius, moved, firstPoint, secondPoint))
		return 0;

	double firstDeltaY = (double)center[1] - firstPoint[1];
	double firstDeltaX = (double)center[0] - firstPoint[0];
	double firstDeltaZ = (double)center[2] - firstPoint[2];
	float secondDeltaY = (float)((double)center[1] - secondPoint[1]);
	double secondDeltaX = (double)center[0] - secondPoint[0];
	float secondDeltaZ = (float)((double)center[2] - secondPoint[2]);
	float firstSquareX = (float)(firstDeltaX * firstDeltaX);
	float firstSquareY = (float)(firstDeltaY * firstDeltaY);
	float firstSquareZ = (float)(firstDeltaZ * firstDeltaZ);
	float firstDistance = (float)sqrt(((double)firstSquareX + firstSquareY) + firstSquareZ);
	float secondSquareX = (float)(secondDeltaX * secondDeltaX);
	float secondSquareY = (float)((double)secondDeltaY * secondDeltaY);
	float secondSquareZ = (float)((double)secondDeltaZ * secondDeltaZ);
	float secondDistance = (float)sqrt(((double)secondSquareX + secondSquareY) + secondSquareZ);
	float *chosen = firstDistance < secondDistance ? firstPoint : secondPoint;

	center[0] = chosen[0];
	center[1] = chosen[1];
	center[2] = chosen[2];

	return 1;
}

void SputActorModule::sphereCollisionRemastered(SputActor *actor, SputActor *other, float *myCenterOffset, float *myCenterWorld, float *prevCenterWorld,
		float *otherCenterWorld, float myRadius, float otherRadius) {
	Extra *extra = actor->extra;

	extra->collided = 1;
	other->extra->collided = 1;
	_collisionIn[0] = actor->actorPos[0];
	_collisionIn[1] = actor->actorPos[1];
	_collisionIn[2] = actor->actorPos[2];

	RdVector3 center;

	center[0] = myCenterWorld[0];
	center[1] = myCenterWorld[1];
	center[2] = myCenterWorld[2];

	int walking = (actor->mode & 1) != 0;
	int notMain = actor->flags & kActorFlagNotMain;
	double reach = (double)otherRadius + myRadius;

	if (walking && !notMain && findSlidePoint(otherCenterWorld, prevCenterWorld, center, (float)reach)) {
		if (extra->pathIndex < extra->numPathPoints) {
			if (extra->pathIndex < 0 || extra->pathIndex >= pathCapacity()) {
				warning("SputActorModule::sphereCollisionRemastered(): path index %d out of range", extra->pathIndex);
			} else {
				float *pathPoint = extra->path[extra->pathIndex];
				RdVector3 nextPoint;

				nextPoint[0] = (float)((double)pathPoint[0] + myCenterOffset[0]);
				nextPoint[1] = (float)((double)pathPoint[1] + myCenterOffset[1]);
				nextPoint[2] = (float)((double)pathPoint[2] + myCenterOffset[2]);

				double nextDeltaY = (double)nextPoint[1] - otherCenterWorld[1];
				double nextDeltaX = (double)nextPoint[0] - otherCenterWorld[0];
				double nextDeltaZ = (double)nextPoint[2] - otherCenterWorld[2];
				float nextSquareX = (float)(nextDeltaX * nextDeltaX);
				float nextSquareY = (float)(nextDeltaY * nextDeltaY);
				float nextSquareZ = (float)(nextDeltaZ * nextDeltaZ);
				float nextDistance = (float)sqrt(((double)nextSquareX + nextSquareY) + nextSquareZ);

				if (nextDistance < reach) {
					walking = 0;
				} else {
					Puck *puck = extra->puck;
					RdVector3 aim;

					aim[0] = (float)((double)center[0] - prevCenterWorld[0]);
					aim[1] = (float)((double)center[1] - prevCenterWorld[1]);
					aim[2] = (float)((double)center[2] - prevCenterWorld[2]);
					_rd->normalize3AccRemastered(aim);

					if (puck) {
						aim[0] = (float)((double)aim[0] + puck->_pos[0]);
						aim[1] = (float)((double)puck->_pos[1] + aim[1]);
						aim[2] = (float)((double)puck->_pos[2] + aim[2]);
						puck->pointAt(aim, 3.4028235e38f);
						alignToPuck(actor, puck, 0);
					} else {
						warning("SputActorModule::sphereCollisionRemastered(): the actor has no puck to turn");
					}
				}
			}
		}
	} else {
		double deltaX = (double)myCenterWorld[0] - otherCenterWorld[0];
		double deltaY = (double)myCenterWorld[1] - otherCenterWorld[1];
		double deltaZ = (double)myCenterWorld[2] - otherCenterWorld[2];
		float squareX = (float)(deltaX * deltaX);
		float squareY = (float)(deltaY * deltaY);
		float squareZ = (float)(deltaZ * deltaZ);
		float distance = (float)sqrt(((double)squareX + squareY) + squareZ);
		RdVector3 direction;

		direction[0] = (float)((double)myCenterWorld[0] - otherCenterWorld[0]);
		direction[1] = (float)((double)myCenterWorld[1] - otherCenterWorld[1]);
		direction[2] = (float)((double)myCenterWorld[2] - otherCenterWorld[2]);
		_rd->normalize3AccRemastered(direction);

		float ratio = (float)(((double)distance - otherRadius) / myRadius);
		double pushOut = (double)myRadius * (1.0 - ratio);
		float pushX = (float)((double)direction[0] * pushOut);
		float pushY = (float)((double)direction[1] * pushOut);
		float pushZ = (float)(pushOut * direction[2]);

		center[0] = (float)((double)pushX + myCenterWorld[0]);
		center[1] = (float)((double)pushY + myCenterWorld[1]);
		center[2] = (float)((double)pushZ + myCenterWorld[2]);
	}

	center[0] = (float)((double)center[0] - myCenterOffset[0]);
	center[1] = (float)((double)center[1] - myCenterOffset[1]);
	center[2] = (float)((double)center[2] - myCenterOffset[2]);
	setActorPosition(actor, center);

	double movedDeltaX = (double)actor->actorPos[0] - actor->prevPosition[0];
	double movedDeltaY = (double)actor->actorPos[1] - actor->prevPosition[1];
	double movedDeltaZ = (double)actor->actorPos[2] - actor->prevPosition[2];
	float movedSquareX = (float)(movedDeltaX * movedDeltaX);
	float movedSquareY = (float)(movedDeltaY * movedDeltaY);
	float movedSquareZ = (float)(movedDeltaZ * movedDeltaZ);
	float moved = (float)sqrt(((double)movedSquareX + movedSquareY) + movedSquareZ);

	if (moved < 0.001 && !notMain)
		walking = 0;

	_collisionOut[0] = actor->actorPos[0];
	_collisionOut[1] = actor->actorPos[1];
	_collisionOut[2] = actor->actorPos[2];
	_collisionImpact[0] = actor->actorPos[0];
	_collisionImpact[1] = actor->actorPos[1];
	_collisionImpact[2] = actor->actorPos[2];

	if (walking)
		actor->mode |= 1;
}

void SputActorModule::updatePositions() {
	for (int i = 0; i < _numCollisionActors; i++) {
		SputActor *actor = _collisionActors[i];

		if (memcmp(actor->actorPos, actor->prevPosition, sizeof(RdVector3))) {
			if (_grim->isRemastered())
				updatePositionRemastered(actor);
			else
				updatePosition(actor);
		}
	}
}

void SputActorModule::updatePosition(SputActor *actor) {
	RdModel3 *model3 = actor->costumeStack->headThing->data.model3;
	RdVector3 myCenterOffset;

	_rd->transformVector34(myCenterOffset, model3->geomCenter, getOrientMatrix(actor));

	RdVector3 myCenterWorld;

	myCenterWorld[0] = (float)((double)myCenterOffset[0] + actor->actorPos[0]);
	myCenterWorld[1] = (float)((double)actor->actorPos[1] + myCenterOffset[1]);
	myCenterWorld[2] = (float)((double)actor->actorPos[2] + myCenterOffset[2]);

	RdVector3 prevCenterWorld;

	prevCenterWorld[0] = (float)((double)myCenterOffset[0] + actor->prevPosition[0]);
	prevCenterWorld[1] = (float)((double)myCenterOffset[1] + actor->prevPosition[1]);
	prevCenterWorld[2] = (float)((double)myCenterOffset[2] + actor->prevPosition[2]);

	float myRadius = (float)((double)model3->radius * actor->extra->collisionScale);

	for (int i = 0; i < _numCollisionActors; i++) {
		SputActor *other = _collisionActors[i];

		if (other == actor)
			continue;

		if (!other->extra || !other->extra->collisionMode)
			continue;

		RdModel3 *otherModel3 = other->costumeStack->headThing->data.model3;
		RdVector3 otherCenterWorld;

		_rd->transformPoint34(otherCenterWorld, otherModel3->geomCenter, getOrientMatrix(other));

		double deltaX = (double)myCenterWorld[0] - otherCenterWorld[0];
		double deltaY = (double)myCenterWorld[1] - otherCenterWorld[1];
		double deltaZ = (double)myCenterWorld[2] - otherCenterWorld[2];
		float deltaXF = (float)deltaX;
		float deltaYF = (float)deltaY;
		float deltaZF = (float)deltaZ;
		double distance = sqrt((deltaX * deltaXF + deltaY * deltaY) + deltaZ * deltaZF);
		double otherRadius = (double)otherModel3->radius * other->extra->collisionScale;
		float otherRadiusF = (float)otherRadius;
		float distanceF = (float)distance;

		if (!(distance < otherRadius + myRadius))
			continue;

		if (other->extra->collisionMode == 2 && g_grim->useRemasterFixes()) {
			sphereCollisionRemastered(actor, other, myCenterOffset, myCenterWorld, prevCenterWorld, otherCenterWorld, myRadius, otherRadiusF);
		} else if (other->extra->collisionMode == 2) {
			actor->extra->collided = 1;
			other->extra->collided = 1;

			RdVector3 direction;

			direction[0] = (float)((double)myCenterWorld[0] - otherCenterWorld[0]);
			direction[1] = (float)((double)myCenterWorld[1] - otherCenterWorld[1]);
			direction[2] = (float)((double)myCenterWorld[2] - otherCenterWorld[2]);
			_collisionIn[0] = actor->actorPos[0];
			_collisionIn[1] = actor->actorPos[1];
			_collisionIn[2] = actor->actorPos[2];
			_rd->normalize3Acc(direction);

			float ratio = (float)(((double)distanceF - otherRadiusF) / myRadius);
			float pushOut = (float)((1.0 - ratio) * myRadius);
			RdVector3 newPos;

			newPos[0] = (float)(((double)direction[0] * pushOut + myCenterWorld[0]) - myCenterOffset[0]);
			newPos[1] = (float)(((double)direction[1] * pushOut + myCenterWorld[1]) - myCenterOffset[1]);
			newPos[2] = (float)(((double)direction[2] * pushOut + myCenterWorld[2]) - myCenterOffset[2]);

			int walking = (actor->mode & 1) != 0;

			setActorPosition(actor, newPos);
			_collisionOut[0] = actor->actorPos[0];
			_collisionOut[1] = actor->actorPos[1];
			_collisionOut[2] = actor->actorPos[2];
			_collisionImpact[0] = actor->actorPos[0];
			_collisionImpact[1] = actor->actorPos[1];
			_collisionImpact[2] = actor->actorPos[2];

			if (walking)
				actor->mode |= 1;
		} else {
			RdVector3 delta;

			delta[0] = deltaXF;
			delta[1] = deltaYF;
			delta[2] = deltaZF;

			RdVector3 localPrev;

			_rd->transformVectorOrtho34(localPrev, delta, getOrientMatrix(other));
			localPrev[0] = (float)((double)localPrev[0] + otherModel3->geomCenter[0]);
			localPrev[1] = (float)((double)localPrev[1] + otherModel3->geomCenter[1]);
			localPrev[2] = (float)((double)localPrev[2] + otherModel3->geomCenter[2]);

			RdBound3 otherBox;
			float otherScale = other->extra->collisionScale;

			otherBox.min[0] = (float)((double)otherModel3->bound.min[0] * otherScale - myRadius);
			otherBox.min[1] = (float)((double)otherModel3->bound.min[1] * otherScale - myRadius);
			otherBox.min[2] = (float)((double)otherModel3->bound.min[2] * otherScale - myRadius);
			otherBox.max[0] = (float)((double)otherModel3->bound.max[0] * otherScale + myRadius);
			otherBox.max[1] = (float)((double)otherModel3->bound.max[1] * otherScale + myRadius);
			otherBox.max[2] = (float)((double)otherModel3->bound.max[2] * otherScale + myRadius);

			if (!(localPrev[2] > otherBox.min[2] && localPrev[1] > otherBox.min[1] && localPrev[1] < otherBox.max[1] && localPrev[0] > otherBox.min[0] &&
					localPrev[0] < otherBox.max[0] && localPrev[2] < otherBox.max[2]))
				continue;

			actor->extra->collided = 1;
			_collisionIn[0] = actor->actorPos[0];
			_collisionIn[1] = actor->actorPos[1];
			_collisionIn[2] = actor->actorPos[2];

			RdVector3 prevDelta;

			prevDelta[0] = (float)(((double)myCenterOffset[0] + actor->prevPosition[0]) - otherCenterWorld[0]);
			prevDelta[1] = (float)((double)actor->prevPosition[1] + myCenterOffset[1]);
			prevDelta[2] = (float)((double)actor->prevPosition[2] + myCenterOffset[2]);
			prevDelta[1] = (float)((double)prevDelta[1] - otherCenterWorld[1]);
			prevDelta[2] = (float)((double)prevDelta[2] - otherCenterWorld[2]);

			RdVector3 localCur;

			_rd->transformVectorOrtho34(localCur, prevDelta, getOrientMatrix(other));
			localCur[0] = (float)((double)localCur[0] + otherModel3->geomCenter[0]);
			localCur[1] = (float)((double)localCur[1] + otherModel3->geomCenter[1]);
			localCur[2] = (float)((double)localCur[2] + otherModel3->geomCenter[2]);

			RdVector3 hitPos;

			if (!sweepIntoBox(localPrev, localCur, otherBox, hitPos)) {
				getOuttaTheBox(localPrev, otherBox, getOrientMatrix(other), other->actorPos);
				hitPos[0] = localPrev[0];
				hitPos[1] = localPrev[1];
				hitPos[2] = localPrev[2];
			}

			_rd->transformPoint34Acc(hitPos, getOrientMatrix(other));
			hitPos[0] = (float)((double)hitPos[0] - myCenterOffset[0]);
			hitPos[1] = (float)((double)hitPos[1] - myCenterOffset[1]);
			hitPos[2] = (float)((double)hitPos[2] - myCenterOffset[2]);
			actor->prevPosition[0] = actor->actorPos[0];
			actor->prevPosition[1] = actor->actorPos[1];
			actor->prevPosition[2] = actor->actorPos[2];

			_rd->transformPoint34Acc(_collisionImpact, getOrientMatrix(other));
			_collisionImpact[0] = (float)((double)_collisionImpact[0] - myCenterOffset[0]);
			_collisionImpact[1] = (float)((double)_collisionImpact[1] - myCenterOffset[1]);
			_collisionImpact[2] = (float)((double)_collisionImpact[2] - myCenterOffset[2]);

			int walking = (actor->mode & 1) != 0;

			setActorPosition(actor, hitPos);
			_collisionOut[0] = actor->actorPos[0];
			_collisionOut[1] = actor->actorPos[1];
			_collisionOut[2] = actor->actorPos[2];

			if (walking) {
				actor->mode |= 1;

				if (actor->extra->numPathPoints > 0 && actor->extra->numPathPoints <= pathCapacity()) {
					RdVector3 goalPoint;
					float *lastPathPoint = actor->extra->path[actor->extra->numPathPoints - 1];

					goalPoint[0] = lastPathPoint[0];
					goalPoint[1] = lastPathPoint[1];
					goalPoint[2] = lastPathPoint[2];
					actorWalkTo(actor, goalPoint, actor->extra->goal, other);
				} else {
					warning("SputActorModule::updatePosition(): the actor has no path to rebuild");
				}

				_hadDynamicCollision = true;
			}
		}

		_lua->collisionHandler(actor, other, _collisionIn, _collisionImpact, _collisionOut);
	}
}

void SputActorModule::updatePositionRemastered(SputActor *actor) {
	if (!actor->extra) {
		warning("SputActorModule::updatePositionRemastered(): the moving actor has no extra data");
		return;
	}

	RdModel3 *model3 = actor->costumeStack->headThing->data.model3;
	RdVector3 myCenterOffset;

	_rd->transformVector34Remastered(myCenterOffset, model3->geomCenter, getOrientMatrix(actor));

	RdVector3 myCenterWorld;

	myCenterWorld[0] = (float)((double)actor->actorPos[0] + myCenterOffset[0]);
	myCenterWorld[1] = (float)((double)actor->actorPos[1] + myCenterOffset[1]);
	myCenterWorld[2] = (float)((double)actor->actorPos[2] + myCenterOffset[2]);

	RdVector3 prevCenterWorld;

	prevCenterWorld[0] = (float)((double)myCenterOffset[0] + actor->prevPosition[0]);
	prevCenterWorld[1] = (float)((double)myCenterOffset[1] + actor->prevPosition[1]);
	prevCenterWorld[2] = (float)((double)myCenterOffset[2] + actor->prevPosition[2]);

	float myRadius = (float)((double)actor->extra->collisionScale * model3->radius);

	for (int i = 0; i < _numCollisionActors; i++) {
		SputActor *other = _collisionActors[i];

		if (other == actor)
			continue;

		if (!other->extra || !other->extra->collisionMode)
			continue;

		RdModel3 *otherModel3 = other->costumeStack->headThing->data.model3;
		RdVector3 otherCenterWorld;

		_rd->transformPoint34Remastered(otherCenterWorld, otherModel3->geomCenter, getOrientMatrix(other));

		double deltaY = (double)myCenterWorld[1] - otherCenterWorld[1];
		double deltaX = (double)myCenterWorld[0] - otherCenterWorld[0];
		double deltaZ = (double)myCenterWorld[2] - otherCenterWorld[2];
		float squareX = (float)(deltaX * deltaX);
		float squareY = (float)(deltaY * deltaY);
		float squareZ = (float)(deltaZ * deltaZ);
		float distance = (float)sqrt(((double)squareX + squareY) + squareZ);
		float otherRadius = (float)((double)other->extra->collisionScale * otherModel3->radius);

		if (!(distance < (double)otherRadius + myRadius))
			continue;

		if (other->extra->collisionMode == 2) {
			sphereCollisionRemastered(actor, other, myCenterOffset, myCenterWorld, prevCenterWorld, otherCenterWorld, myRadius, otherRadius);
		} else {
			RdVector3 delta;

			delta[0] = (float)deltaX;
			delta[1] = (float)deltaY;
			delta[2] = (float)deltaZ;

			RdVector3 localCurrent;

			_rd->transformVectorOrtho34Remastered(localCurrent, delta, getOrientMatrix(other));
			localCurrent[0] = (float)((double)otherModel3->geomCenter[0] + localCurrent[0]);
			localCurrent[1] = (float)((double)otherModel3->geomCenter[1] + localCurrent[1]);
			localCurrent[2] = (float)((double)otherModel3->geomCenter[2] + localCurrent[2]);

			RdBound3 otherBox;
			float otherScale = other->extra->collisionScale;

			otherBox.min[0] = (float)((double)otherModel3->bound.min[0] * otherScale - myRadius);
			otherBox.min[1] = (float)((double)otherModel3->bound.min[1] * otherScale - myRadius);
			otherBox.min[2] = (float)((double)otherModel3->bound.min[2] * otherScale - myRadius);
			otherBox.max[0] = (float)((double)otherModel3->bound.max[0] * otherScale + myRadius);
			otherBox.max[1] = (float)((double)otherModel3->bound.max[1] * otherScale + myRadius);
			otherBox.max[2] = (float)((double)otherModel3->bound.max[2] * otherScale + myRadius);

			if (!(otherBox.min[2] < localCurrent[2] && otherBox.min[1] < localCurrent[1] && otherBox.max[1] > localCurrent[1] && otherBox.min[0] < localCurrent[0] &&
					otherBox.max[0] > localCurrent[0] && otherBox.max[2] > localCurrent[2]))
				continue;

			actor->extra->collided = 1;
			_collisionIn[0] = actor->actorPos[0];
			_collisionIn[1] = actor->actorPos[1];
			_collisionIn[2] = actor->actorPos[2];

			RdVector3 prevDelta;

			prevDelta[0] = (float)((double)actor->prevPosition[0] + myCenterOffset[0]);
			prevDelta[1] = (float)((double)actor->prevPosition[1] + myCenterOffset[1]);
			prevDelta[2] = (float)((double)actor->prevPosition[2] + myCenterOffset[2]);
			prevDelta[0] = (float)((double)prevDelta[0] - otherCenterWorld[0]);
			prevDelta[1] = (float)((double)prevDelta[1] - otherCenterWorld[1]);
			prevDelta[2] = (float)((double)prevDelta[2] - otherCenterWorld[2]);

			RdVector3 localPrevious;

			_rd->transformVectorOrtho34Remastered(localPrevious, prevDelta, getOrientMatrix(other));
			localPrevious[0] = (float)((double)otherModel3->geomCenter[0] + localPrevious[0]);
			localPrevious[1] = (float)((double)otherModel3->geomCenter[1] + localPrevious[1]);
			localPrevious[2] = (float)((double)otherModel3->geomCenter[2] + localPrevious[2]);

			RdVector3 hitPos;

			if (!sweepIntoBoxRemastered(localCurrent, localPrevious, otherBox, hitPos)) {
				RdMatrix34 identity;
				RdVector3 origin = { 0.0f, 0.0f, 0.0f };

				_rd->identity34(identity);
				getOuttaTheBoxRemastered(localCurrent, otherBox, identity, origin);
				hitPos[0] = localCurrent[0];
				hitPos[1] = localCurrent[1];
				hitPos[2] = localCurrent[2];
			}

			_rd->transformPoint34AccRemastered(hitPos, getOrientMatrix(other));
			hitPos[0] = (float)((double)hitPos[0] - myCenterOffset[0]);
			hitPos[1] = (float)((double)hitPos[1] - myCenterOffset[1]);
			hitPos[2] = (float)((double)hitPos[2] - myCenterOffset[2]);
			actor->prevPosition[0] = actor->actorPos[0];
			actor->prevPosition[1] = actor->actorPos[1];
			actor->prevPosition[2] = actor->actorPos[2];

			_rd->transformPoint34AccRemastered(_collisionImpact, getOrientMatrix(other));
			_collisionImpact[0] = (float)((double)_collisionImpact[0] - myCenterOffset[0]);
			_collisionImpact[1] = (float)((double)_collisionImpact[1] - myCenterOffset[1]);
			_collisionImpact[2] = (float)((double)_collisionImpact[2] - myCenterOffset[2]);

			int walking = (actor->mode & 1) != 0;

			setActorPosition(actor, hitPos);
			_collisionOut[0] = actor->actorPos[0];
			_collisionOut[1] = actor->actorPos[1];
			_collisionOut[2] = actor->actorPos[2];

			if (walking) {
				actor->mode |= 1;

				if (actor->extra->numPathPoints > 0 && actor->extra->numPathPoints <= pathCapacity()) {
					RdVector3 goalPoint;
					float *lastPathPoint = actor->extra->path[actor->extra->numPathPoints - 1];

					goalPoint[0] = lastPathPoint[0];
					goalPoint[1] = lastPathPoint[1];
					goalPoint[2] = lastPathPoint[2];
					actorWalkTo(actor, goalPoint, actor->extra->goal, other);
				} else {
					warning("SputActorModule::updatePositionRemastered(): the actor has no path to rebuild");
				}

				_hadDynamicCollision = true;
			}
		}

		_lua->collisionHandler(actor, other, _collisionIn, _collisionImpact, _collisionOut);
	}
}

} // End of namespace Grim
