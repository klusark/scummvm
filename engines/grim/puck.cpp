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

#include "engines/grim/grim.h"
#include "engines/grim/puck.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/walkplane.h"

namespace Grim {

Puck::Puck(WalkSystem *system, float angle) {
	_system = system;

	_pos[0] = 0.0f;
	_pos[1] = 0.0f;
	_pos[2] = 0.0f;
	_system->_rd->build34(_orient, _pos, _pos);

	_constraint[1] = 1.0f;
	_reflectionAngle = angle;
	_plane = nullptr;
	_forwardDist = 0.0f;
	_constrained = 0;
	_constraint[0] = 0.0f;
	_constraint[2] = 0.0f;
	_next = nullptr;
	_prev = nullptr;

	_next = _system->_puckHead;

	if (_system->_puckHead)
		_system->_puckHead->_prev = this;

	_system->_puckHead = this;
}

Puck::~Puck() {
	if (this == _system->_puckHead)
		_system->_puckHead = _next;

	if (_prev)
		_prev->_next = _next;

	if (_next)
		_next->_prev = _prev;
}

void Puck::transformPoint34AccRemastered(RdVector3 vec, RdMatrix34 mat) {
	RdVector3 result;

	for (int i = 0; i < 3; i++) {
		float first = (float)((double)mat[0][i] * vec[0]);
		float second = (float)((double)mat[1][i] * vec[1]);
		float third = (float)((double)mat[2][i] * vec[2]);

		result[i] = (float)((((double)first + second) + third) + mat[3][i]);
	}

	vec[0] = result[0];
	vec[1] = result[1];
	vec[2] = result[2];
}

void Puck::transformVector34AccRemastered(RdVector3 vec, RdMatrix34 mat) {
	RdVector3 result;

	for (int i = 0; i < 3; i++) {
		float first = (float)((double)mat[0][i] * vec[0]);
		float second = (float)((double)mat[1][i] * vec[1]);
		float third = (float)((double)mat[2][i] * vec[2]);

		result[i] = (float)(((double)first + second) + third);
	}

	vec[0] = result[0];
	vec[1] = result[1];
	vec[2] = result[2];
}

void Puck::transformVectorOrtho34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat) {
	for (int i = 0; i < 3; i++) {
		float first = (float)((double)mat[i][0] * vec[0]);
		float second = (float)((double)mat[i][1] * vec[1]);
		float third = (float)((double)mat[i][2] * vec[2]);

		out[i] = (float)(((double)first + second) + third);
	}
}

void Puck::buildFromVectorAngle34Remastered(RdMatrix34 out, RdVector3 axis, float angle) {
	float sinAngle;
	float cosAngle;

	_system->_stdMath->sinCos(angle, &sinAngle, &cosAngle);

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

void Puck::extractAngles34Remastered(RdMatrix34 mat, RdVector3 pyr) {
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
		float angle = (float)(90.0 - _system->_stdMath->arcSin3(-negSideX));

		if ((negSideY > 0.0f && forwardZ > 0.0f) || (negSideY < 0.0f && forwardZ < 0.0f))
			angle = -angle;

		pyr[2] = angle;
		pyr[1] = 0.0f;
	} else {
		float angle = (float)(90.0 - _system->_stdMath->arcSin3((float)((double)forwardY / length)));

		if (forwardX > 0.0f)
			angle = -angle;

		pyr[1] = angle;
	}

	if (length < 0.001f) {
		pyr[0] = 90.0f;
	} else {
		float ratio = (float)(((forwardYSquared + forwardXSquared) + (double)forwardZ * 0.0) / length);

		if (ratio < 1.0f)
			pyr[0] = (float)(90.0 - _system->_stdMath->arcSin3(ratio));
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
			pyr[2] = (float)(90.0 - _system->_stdMath->arcSin3(cosValue));
		else
			pyr[2] = 180.0f;
	} else {
		pyr[2] = 0.0f;
	}

	if (negSideZ < 0.0f)
		pyr[2] = -pyr[2];
}

float *Puck::rot() {
	if (_system->_remastered)
		extractAngles34Remastered(_orient, _rot);
	else
		_system->_rd->extractAngles34(_orient, _rot);

	return _rot;
}

WalkPlane *Puck::validate() {
	_plane = _system->planeClosest(_pos, 4096);

	if (_plane) {
		if (!_plane->within(_pos, 1)) {
			_plane->closestPoint(_pos);
			setPos(_pos);
		}

		orientToPlane(_plane);
	}

	return _plane;
}

void Puck::setPos(float *pos) {
	_pos[0] = pos[0];
	_pos[1] = pos[1];
	_pos[2] = pos[2];

	_orient[3][0] = pos[0];
	_orient[3][1] = pos[1];
	_orient[3][2] = pos[2];
}

void Puck::xform(float *vec) {
	if (_system->_remastered) {
		transformPoint34AccRemastered(vec, _orient);

		return;
	}

	_system->_rd->transformPoint34Acc(vec, _orient);
}

void Puck::invXform(float *vec) {
	RdVector3 out;

	vec[0] = (float)((double)vec[0] - _pos[0]);
	vec[1] = (float)((double)vec[1] - _pos[1]);
	vec[2] = (float)((double)vec[2] - _pos[2]);

	if (_system->_remastered)
		transformVectorOrtho34Remastered(out, vec, _orient);
	else
		_system->_rd->transformVectorOrtho34(out, vec, _orient);

	vec[0] = out[0];
	vec[1] = out[1];
	vec[2] = out[2];
}

void Puck::rotate(float angle) {
	RdMatrix34 out;

	if (_system->_remastered) {
		buildFromVectorAngle34Remastered(out, _orient[2], angle);
		transformVector34AccRemastered(_orient[1], out);
	} else {
		_system->_rd->buildFromVectorAngle34(out, _orient[2], angle);
		_system->_rd->transformVector34Acc(_orient[1], out);
	}

	_orient[1][0] = (float)((double)_orient[1][0] + _orient[3][0]);
	_orient[1][1] = (float)((double)_orient[3][1] + _orient[1][1]);
	_orient[1][2] = (float)((double)_orient[3][2] + _orient[1][2]);

	if (_plane)
		_plane->snapTo(_orient[1]);

	_orient[1][0] = (float)((double)_orient[1][0] - _orient[3][0]);
	_orient[1][1] = (float)((double)_orient[1][1] - _orient[3][1]);
	_orient[1][2] = (float)((double)_orient[1][2] - _orient[3][2]);

	normalize();
}

float Puck::pointAt(float *target, float maxAngle) {
	RdVector3 vec;

	vec[0] = target[0];
	vec[1] = target[1];
	vec[2] = target[2];

	if (_plane)
		_plane->project(vec);

	invXform(vec);
	_system->_rd->normalize3Acc(vec);

	float angle = (float)(90.0 - _system->_stdMath->arcSin3(vec[1]));

	if (maxAngle < angle)
		angle = maxAngle;

	if (vec[0] > 0.0f)
		angle = -angle;

	rotate(angle);

	return angle;
}

double Puck::getYawToPoint(float *target) {
	RdVector3 vec;

	vec[0] = target[0];
	vec[1] = target[1];
	vec[2] = target[2];

	if (_plane)
		_plane->project(vec);

	invXform(vec);
	_system->_rd->normalize3Acc(vec);

	float angle = (float)(90.0 - _system->_stdMath->arcSin3(vec[1]));

	if (vec[0] > 0.0f)
		angle = -angle;

	if (_system->_remastered)
		return (double)(float)((double)rot()[1] + angle);

	return (double)rot()[1] + angle;
}

void Puck::constrain(int enable) {
	_constrained = 0;

	if (_plane && enable) {
		float *constraintPointer = _plane->getConstraintVector(this);

		if (constraintPointer) {
			_constraint[0] = constraintPointer[0];
			_constraint[1] = constraintPointer[1];
			_constraint[2] = constraintPointer[2];

			_orient[1][0] = constraintPointer[0];
			_orient[1][1] = constraintPointer[1];
			_orient[1][2] = constraintPointer[2];

			normalize();
			_constrained = 1;
		}
	}
}

int Puck::forwardDemo(float dist, int allowLeft, int allowRight) {
	float value = 0.0f;

	if (_system->_forwardCounter == 0)
		_forwardDist = dist;

	_system->_forwardCounter++;

	if (_constrained) {
		float dot = _system->_rd->dot3(_orient[0], _constraint);
		double angleDelta = (90.0 - _system->_stdMath->arcSin3(dot)) - 90.0;

		value = (float)angleDelta;

		if (fabs(angleDelta) > _reflectionAngle)
			return 0;

		if (_system->_rd->dot3(_orient[1], _constraint) <= 0.0f)
			rotate(-value);
		else
			rotate(value);
	}

	RdVector3 point;

	point[0] = 0.0f;
	point[1] = dist;
	point[2] = 0.0f;

	xform(point);

	if (!_plane || _plane->within(point, 1)) {
		setPos(point);
		_system->_forwardCounter = 0;

		return 1;
	}

	if (_system->_forwardCounter > 8) {
		_system->_forwardCounter = 0;

		return 0;
	}

	WalkPlane *oldPlane = _plane;
	RdVector3 out;

	value = (float)oldPlane->intersection(_pos, point, out);
	oldPlane->_enabled = 0;

	RdVector3 local;

	local[0] = out[0];
	local[1] = out[1];
	local[2] = out[2];

	invXform(local);

	dist = (float)((double)dist - local[1]);

	WalkPlane *newPlane = _system->planeAt(out, 4096);

	if (newPlane) {
		setPos(out);
		_plane = newPlane;
		orientToPlane(newPlane);

		if (dist >= 0.02f)
			dist = 0.02f;

		int result = forwardDemo(dist, 1, 1);

		oldPlane->_enabled = 1;

		return result;
	}

	setPos(out);

	if (fabs((double)value) >= _reflectionAngle) {
		_forwardDist = (float)((double)_forwardDist - dist);
	} else if ((value > 0.0f && allowLeft) || (value < 0.0f && allowRight)) {
		rotate(value);
		oldPlane->_enabled = 1;

		int result = forwardDemo(dist, 1, 1);

		oldPlane->_enabled = 1;

		return result;
	}

	oldPlane->_enabled = 1;

	return 0;
}

int Puck::forward(float dist, int allowLeft, int allowRight) {
	if (_system->_demo)
		return forwardDemo(dist, allowLeft, allowRight);

	if (_system->_remastered || g_grim->useRemasterFixes())
		return forwardRemastered(dist, nullptr, allowLeft, allowRight);

	int result = 0;
	float value = 0.0f;

	if (_system->_forwardCounter == 0)
		_forwardDist = dist;

	_system->_forwardCounter++;

	if (_constrained) {
		float dot = _system->_rd->dot3(_orient[0], _constraint);
		double angleDelta = (90.0 - _system->_stdMath->arcSin3(dot)) - 90.0;

		value = (float)angleDelta;

		if (fabs(angleDelta) > _reflectionAngle)
			return 0;

		if (_system->_rd->dot3(_orient[1], _constraint) <= 0.0f)
			rotate(-value);
		else
			rotate(value);
	}

	RdVector3 point;

	point[0] = 0.0f;
	point[1] = dist;
	point[2] = 0.0f;

	xform(point);

	if (!_plane || _plane->within(point, 1)) {
		setPos(point);
		_system->_forwardCounter = 0;

		return 1;
	}

	if (_system->_forwardCounter > 8) {
		_system->_forwardCounter = 0;

		return 0;
	}

	WalkPlane *oldPlane = _plane;
	RdVector3 out;

	value = (float)oldPlane->intersection(_pos, point, out);
	oldPlane->_enabled = 0;

	RdVector3 local;

	local[0] = out[0];
	local[1] = out[1];
	local[2] = out[2];

	invXform(local);

	dist = (float)((double)dist - local[1]);

	WalkPlane *newPlane = _system->planeAt(out, 4096);

	if (newPlane) {
		setPos(out);
		_plane = newPlane;
		orientToPlane(newPlane);

		if (dist >= 0.02f)
			dist = 0.02f;

		result = forward(dist, 1, 1);
		oldPlane->_enabled = 1;

		return result;
	}

	setPos(out);

	if ((_plane->_type & 0x100) != 0) {
		int quadAxis = _plane->_quadAxis;

		if ((quadAxis + 1) % _plane->_numVerts == _plane->_hitEdge)
			return 0;

		float *vert = &_plane->_verts[quadAxis]->x;
		RdVector3 vertLocal;

		vertLocal[0] = vert[0];
		vertLocal[1] = vert[1];
		vertLocal[2] = vert[2];

		invXform(vertLocal);

		if (vertLocal[0] <= 0.0f) {
			if (value < 3.0f)
				value = (float)(((double)value + 90.0f) + 90.2f);
		} else if (value > 3.0f) {
			value = (float)(-90.2f - (90.0f - (double)value));
		}

		rotate(value);
		result = forward(dist, 1, 1);
		oldPlane->_enabled = 1;

		return result;
	}

	if (fabs((double)value) >= _reflectionAngle || _constrained) {
		_forwardDist = (float)((double)_forwardDist - dist);
	} else if ((value > 0.0f && allowLeft) || (value < 0.0f && allowRight)) {
		rotate(value);
		oldPlane->_enabled = 1;
		result = forward(dist, 1, 1);
	}

	if (_constrained) {
		rotate(value);
		_constrained = 1;
		_constraint[0] = _orient[1][0];
		_constraint[1] = _orient[1][1];
		_constraint[2] = _orient[1][2];
	}

	oldPlane->_enabled = 1;

	return result;
}

int Puck::forwardRemastered(float dist, float *direction, int allowLeft, int allowRight) {
	int result = 0;
	float value = 0.0f;

	if (_system->_forwardCounter == 0)
		_forwardDist = dist;

	_system->_forwardCounter++;

	if (_constrained) {
		float dot = _system->_rd->dot3(_orient[0], _constraint);

		value = (float)((90.0 - _system->_stdMath->arcSin3(dot)) - 90.0);

		float absValue = value < 0.0f ? -value : value;

		if (_reflectionAngle < absValue)
			return 0;

		if (_system->_rd->dot3(_orient[1], _constraint) > 0.0f)
			rotate(value);
		else
			rotate(-value);
	}

	RdVector3 point;

	if (!direction) {
		point[0] = 0.0f;
		point[1] = dist;
		point[2] = 0.0f;

		xform(point);
	} else {
		point[0] = direction[0];
		point[1] = direction[1];
		point[2] = direction[2];

		_system->_rd->normalize3Acc(point);

		point[0] = (float)((double)point[0] * dist);
		point[1] = (float)((double)point[1] * dist);
		point[2] = (float)((double)point[2] * dist);

		point[0] = (float)((double)_pos[0] + point[0]);
		point[1] = (float)((double)_pos[1] + point[1]);
		point[2] = (float)((double)_pos[2] + point[2]);
	}

	if (!_plane || _plane->within(point, 1)) {
		setPos(point);
		_system->_forwardCounter = 0;

		return 1;
	}

	if (_system->_forwardCounter > 8) {
		_system->_forwardCounter = 0;

		return 0;
	}

	WalkPlane *oldPlane = _plane;
	RdVector3 out;
	RdVector3 edge;

	value = (float)oldPlane->intersection(_pos, point, out, edge);
	oldPlane->_enabled = 0;

	RdVector3 local;

	local[0] = out[0];
	local[1] = out[1];
	local[2] = out[2];

	invXform(local);

	dist = (float)((double)dist - local[1]);

	WalkPlane *newPlane = _system->planeAt(out, 4096);

	setPos(out);

	if (newPlane) {
		_plane = newPlane;
		orientToPlane(newPlane);

		if (!(0.02f > dist))
			dist = 0.02f;

		result = forwardRemastered(dist, nullptr, 1, 1);
		oldPlane->_enabled = 1;

		return result;
	}

	if ((_plane->_type & 0x100) != 0) {
		int quadAxis = _plane->_quadAxis;

		if (_plane->_numVerts <= 0) {
			warning("Puck::forwardRemastered(): plane with %d vertices", _plane->_numVerts);

			return 0;
		}

		if ((quadAxis + 1) % _plane->_numVerts == _plane->_hitEdge)
			return 0;

		float *vert = &_plane->_verts[quadAxis]->x;
		RdVector3 vertLocal;

		vertLocal[0] = vert[0];
		vertLocal[1] = vert[1];
		vertLocal[2] = vert[2];

		invXform(vertLocal);

		if (vertLocal[0] <= 0.0f) {
			if (value < 3.0f)
				value = (float)(((double)value + 90.0) + 90.2f);
		} else if (value > 3.0f) {
			value = (float)(-90.2f - (90.0 - (double)value));
		}

		rotate(value);
		result = forwardRemastered(dist, nullptr, 1, 1);
		oldPlane->_enabled = 1;

		return result;
	}

	float absValue = value < 0.0f ? -value : value;

	if (!(_reflectionAngle > absValue) || _constrained) {
		_forwardDist = (float)((double)_forwardDist - dist);
	} else if ((value > 0.0f && allowLeft) || (value < 0.0f && allowRight)) {
		oldPlane->_enabled = 1;

		float edgeDot = _system->_rd->dot3(_orient[1], edge);
		RdVector3 slide;

		slide[0] = (float)((double)edge[0] * edgeDot);
		slide[1] = (float)((double)edge[1] * edgeDot);
		slide[2] = (float)((double)edge[2] * edgeDot);

		_system->_rd->normalize3Acc(edge);

		float alongEdge = fabsf(_system->_rd->dot3(_orient[1], edge));

		result = forwardRemastered((float)((double)alongEdge * dist), slide, 1, 1);
	}

	if (_constrained) {
		rotate(value);
		_constrained = 1;
		_constraint[0] = _orient[1][0];
		_constraint[1] = _orient[1][1];
		_constraint[2] = _orient[1][2];
	}

	oldPlane->_enabled = 1;

	return result;
}

void Puck::orientToPlaneRemastered(WalkPlane *targetPlane) {
	_orient[2][0] = targetPlane->_normal[0];
	_orient[2][1] = targetPlane->_normal[1];
	_orient[2][2] = targetPlane->_normal[2];

	float normalZForwardY = (float)((double)_orient[2][2] * _orient[1][1]);
	float normalYForwardZ = (float)((double)_orient[2][1] * _orient[1][2]);
	float normalXForwardZ = (float)((double)_orient[2][0] * _orient[1][2]);
	float forwardXNormalZ = (float)((double)_orient[1][0] * _orient[2][2]);
	float forwardXNormalY = (float)((double)_orient[1][0] * _orient[2][1]);
	float normalXForwardY = (float)((double)_orient[2][0] * _orient[1][1]);

	_orient[0][0] = (float)((double)normalZForwardY - normalYForwardZ);
	_orient[0][1] = (float)((double)normalXForwardZ - forwardXNormalZ);
	_orient[0][2] = (float)((double)forwardXNormalY - normalXForwardY);

	_system->_rd->normalize3Acc(_orient[0]);

	float sideZNormalY = (float)((double)_orient[0][2] * _orient[2][1]);
	float sideYNormalZ = (float)((double)_orient[0][1] * _orient[2][2]);
	float normalZSideX = (float)((double)_orient[2][2] * _orient[0][0]);
	float sideZNormalX = (float)((double)_orient[0][2] * _orient[2][0]);
	float normalXSideY = (float)((double)_orient[2][0] * _orient[0][1]);
	float normalYSideX = (float)((double)_orient[2][1] * _orient[0][0]);

	_orient[1][0] = (float)((double)sideZNormalY - sideYNormalZ);
	_orient[1][1] = (float)((double)normalZSideX - sideZNormalX);
	_orient[1][2] = (float)((double)normalXSideY - normalYSideX);

	_system->_rd->normalize3Acc(_orient[1]);
}

void Puck::orientToPlane(WalkPlane *targetPlane) {
	if (_system->_remastered) {
		orientToPlaneRemastered(targetPlane);

		return;
	}

	double forwardX = _orient[1][0];
	double forwardY = _orient[1][1];
	double forwardZ = _orient[1][2];

	_orient[2][0] = targetPlane->_normal[0];
	_orient[2][1] = targetPlane->_normal[1];
	_orient[2][2] = targetPlane->_normal[2];

	_orient[0][0] = (float)(forwardY * _orient[2][2] - forwardZ * _orient[2][1]);
	_orient[0][1] = (float)((double)_orient[2][0] * forwardZ - forwardX * _orient[2][2]);
	_orient[0][2] = (float)(forwardX * _orient[2][1] - forwardY * _orient[2][0]);

	_system->_rd->normalize3Acc(_orient[0]);

	_orient[1][0] = (float)((double)_orient[0][2] * _orient[2][1] - (double)_orient[0][1] * _orient[2][2]);
	_orient[1][1] = (float)((double)_orient[2][2] * _orient[0][0] - (double)_orient[0][2] * _orient[2][0]);
	_orient[1][2] = (float)((double)_orient[0][1] * _orient[2][0] - (double)_orient[2][1] * _orient[0][0]);

	_system->_rd->normalize3Acc(_orient[1]);
}

void Puck::normalize() {
	_system->_rd->normalize3Acc(_orient[1]);

	if (_system->_remastered) {
		float normalZForwardY = (float)((double)_orient[2][2] * _orient[1][1]);
		float normalYForwardZ = (float)((double)_orient[2][1] * _orient[1][2]);
		float normalXForwardZ = (float)((double)_orient[2][0] * _orient[1][2]);
		float normalZForwardX = (float)((double)_orient[2][2] * _orient[1][0]);
		float normalYForwardX = (float)((double)_orient[2][1] * _orient[1][0]);
		float normalXForwardY = (float)((double)_orient[2][0] * _orient[1][1]);

		_orient[0][0] = (float)((double)normalZForwardY - normalYForwardZ);
		_orient[0][1] = (float)((double)normalXForwardZ - normalZForwardX);
		_orient[0][2] = (float)((double)normalYForwardX - normalXForwardY);

		_system->_rd->normalize3Acc(_orient[0]);

		return;
	}

	_orient[0][0] = (float)((double)_orient[2][2] * _orient[1][1] - (double)_orient[1][2] * _orient[2][1]);
	_orient[0][1] = (float)((double)_orient[1][2] * _orient[2][0] - (double)_orient[2][2] * _orient[1][0]);
	_orient[0][2] = (float)((double)_orient[1][0] * _orient[2][1] - (double)_orient[1][1] * _orient[2][0]);

	_system->_rd->normalize3Acc(_orient[0]);
}

int Puck::draw(uint16 color) {
	RdVector3 p0;
	RdVector3 p1;

	p0[0] = 0.0f;
	p0[1] = 0.0f;
	p0[2] = 0.0f;
	p1[0] = 0.0f;
	p1[1] = 0.1f;
	p1[2] = 0.0f;
	xform(p0);
	xform(p1);
	_system->_rd->_primit3.drawLine(p0, p1, nullptr, nullptr, nullptr, nullptr, color, 0xFFFFFFFF);

	p0[0] = 0.05f;
	p0[1] = 0.05f;
	p0[2] = 0.0f;
	p1[0] = 0.0f;
	p1[1] = 0.1f;
	p1[2] = 0.0f;
	xform(p0);
	xform(p1);
	_system->_rd->_primit3.drawLine(p0, p1, nullptr, nullptr, nullptr, nullptr, color, 0xFFFFFFFF);

	p0[0] = -0.05f;
	p0[1] = 0.05f;
	p0[2] = 0.0f;
	p1[0] = 0.0f;
	p1[1] = 0.1f;
	p1[2] = 0.0f;
	xform(p0);
	xform(p1);
	_system->_rd->_primit3.drawLine(p0, p1, nullptr, nullptr, nullptr, nullptr, color, 0xFFFFFFFF);

	p0[0] = 0.0f;
	p0[1] = 0.0f;
	p0[2] = 0.05f;
	p1[0] = 0.0f;
	p1[1] = 0.0f;
	p1[2] = 0.0f;
	xform(p0);
	xform(p1);

	return _system->_rd->_primit3.drawLine(p0, p1, nullptr, nullptr, nullptr, nullptr, color, 0xFFFFFFFF);
}

} // End of namespace Grim
