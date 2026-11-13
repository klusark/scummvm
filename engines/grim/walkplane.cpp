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

#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/puck.h"
#include "engines/grim/remastered/pathfinder.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/walkplane.h"

namespace Grim {

WalkPlane::WalkPlane(WalkSystem *system, float (*verts)[3], int numPoints, const char *collection) {
	_system = system;

	if (!collection)
		collection = "";

	_active = 1;
	_enabled = 1;
	_prev = nullptr;
	_next = nullptr;
	_number = 0;
	_type = 0;
	_hitEdge = -1;
	_quadAxis = 0;
	_alwaysMerge = 0;
	_numTriangles = 0;
	memset(_triangles, 0, sizeof(_triangles));
	_numVerts = numPoints;
	_name[0] = 0;
	_tolerance = _system->_demo ? 0.1f : 0.005f;

	if (_numVerts > 10) {
		warning("WalkPlane::WalkPlane(): %d vertices requested, only 10 are supported", _numVerts);
		_numVerts = 10;
	}

	for (int i = 0; i < 10; i++)
		_verts[i] = nullptr;

	_normal[0] = 0.0f;
	_normal[1] = 0.0f;
	_normal[2] = 0.0f;

	for (int i = 0; i < _numVerts; i++) {
		_verts[i] = _system->addVertex(this, verts[i]);

		if (!_verts[i]) {
			for (int j = 0; j < i; j++)
				_system->removeVertex(_verts[j]);

			_numVerts = 0;
			_axisU = 2;
			_axisV = 1;
			return;
		}
	}

	if (_numVerts > 0) {
		WpVertex *prevVert = _verts[_numVerts - 1];

		for (int i = 0; i < _numVerts; i++) {
			WpVertex *vert = _verts[i];

			_normal[0] = (float)(((double)prevVert->y - vert->y) * ((double)prevVert->z + vert->z) + _normal[0]);
			_normal[1] = (float)(((double)prevVert->z - vert->z) * ((double)vert->x + prevVert->x) + _normal[1]);
			_normal[2] = (float)(((double)prevVert->x - vert->x) * ((double)prevVert->y + vert->y) + _normal[2]);

			prevVert = vert;
		}
	}

	_system->_rd->normalize3Acc(_normal);

	float largest = _normal[1];
	if (_normal[1] < _normal[0])
		largest = _normal[0];

	if (largest <= _normal[2])
		largest = _normal[2];

	if (largest == _normal[0]) {
		_axisU = 2;
		_axisV = 1;
	} else if (largest == _normal[1]) {
		_axisU = 0;
		_axisV = 2;
	} else {
		_axisU = 1;
		_axisV = 0;
	}

	int index;
	for (index = 0; index < 140; index++) {
		WpCollection *collectionPointer = &_system->_collection[index];

		if (!strcmp(collectionPointer->name, collection) || collectionPointer->name[0] == 0)
			break;
	}

	if (index == 140) {
		warning("WalkPlane::WalkPlane(): too many collections, plane not added to %s", collection);
		return;
	}

	WpCollection *collectionPointer = &_system->_collection[index];

	if (collectionPointer->name[0] == 0) {
		if (strlen(collection) >= sizeof(collectionPointer->name))
			warning("WalkPlane::WalkPlane(): collection name %s is too long", collection);

		Common::strlcpy(collectionPointer->name, collection, sizeof(collectionPointer->name));
	}

	_next = collectionPointer->head;

	if (collectionPointer->head)
		collectionPointer->head->_prev = this;

	collectionPointer->head = this;
	_system->_wpHead = this;
}

WalkPlane::~WalkPlane() {
	if (this == _system->_wpHead)
		_system->_wpHead = _next;

	for (int i = 0; i < 140; i++) {
		if (_system->_collection[i].head == this) {
			_system->_collection[i].head = _next;
			break;
		}
	}

	for (int i = 0; i < _numVerts; i++)
		_system->removeVertex(_verts[i]);

	if (_prev)
		_prev->_next = _next;

	if (_next)
		_next->_prev = _prev;
}

void WalkPlane::setName(const char *name) {
	if (name) {
		if (strlen(name) >= sizeof(_name))
			warning("WalkPlane::setName(): name %s is too long", name);

		Common::strlcpy(_name, name, sizeof(_name));
	}
}

void WalkPlane::setType(int newType) {
	_type = newType;

	if (_system->_demo) {
		// The demo has no quadrilateral axis
		_tolerance = 10000.0f;

		if ((newType & 0x1000) != 0)
			_tolerance = 0.1f;

		return;
	}

	_tolerance = 9999.0f;

	if ((newType & 0x1000) != 0)
		_tolerance = 0.005f;

	if ((newType & 0x100) != 0) {
		if (_numVerts == 4) {
			RdVector3 edgeA;
			RdVector3 edgeB;

			edgeA[0] = (float)((double)_verts[1]->x - _verts[0]->x);
			edgeA[1] = (float)((double)_verts[1]->y - _verts[0]->y);
			edgeA[2] = (float)((double)_verts[1]->z - _verts[0]->z);
			edgeB[0] = (float)((double)_verts[3]->x - _verts[2]->x);
			edgeB[1] = (float)((double)_verts[3]->y - _verts[2]->y);
			edgeB[2] = (float)((double)_verts[3]->z - _verts[2]->z);

			float lengthA = _system->_rd->normalize3Acc(edgeA);
			float lengthB = _system->_rd->normalize3Acc(edgeB);
			double dot = (double)edgeB[0] * edgeA[0] + (double)edgeB[1] * edgeA[1] + (double)edgeB[2] * edgeA[2];
			float dotAbs = (float)fabs(dot);

			edgeA[0] = (float)((double)_verts[2]->x - _verts[1]->x);
			edgeA[1] = (float)((double)_verts[2]->y - _verts[1]->y);
			edgeA[2] = (float)((double)_verts[2]->z - _verts[1]->z);
			edgeB[0] = (float)((double)_verts[0]->x - _verts[3]->x);
			edgeB[1] = (float)((double)_verts[0]->y - _verts[3]->y);
			edgeB[2] = (float)((double)_verts[0]->z - _verts[3]->z);

			float lengthC = _system->_rd->normalize3Acc(edgeA);
			float lengthD = _system->_rd->normalize3Acc(edgeB);
			dot = (double)edgeB[0] * edgeA[0] + (double)edgeB[1] * edgeA[1] + (double)edgeB[2] * edgeA[2];

			if (dotAbs <= fabs(dot)) {
				if (lengthC >= lengthD)
					_quadAxis = 3;
				else
					_quadAxis = 1;
			} else if (lengthA >= lengthB) {
				_quadAxis = 2;
			} else {
				_quadAxis = 0;
			}
		} else {
			_type = 4096;
		}
	}
}

int WalkPlane::withinRemastered(float *point, uint flags) {
	if (flags) {
		if (!_enabled)
			return 0;

		if (!_active)
			return 0;
	}

	if (_numVerts < 2) {
		warning("WalkPlane::withinRemastered(): plane with %d vertices", _numVerts);
		return 0;
	}

	float planeDistance = (float)((((double)point[1] - _verts[1]->y) * _normal[1] + ((double)point[0] - _verts[1]->x) * _normal[0]) + ((double)point[2] - _verts[1]->z) * _normal[2]);
	float absDistance = planeDistance < 0.0f ? -planeDistance : planeDistance;
	float distance = (0.00001f < absDistance) ? absDistance : 0.0f;

	if (_tolerance < distance)
		return 0;

	if (_numVerts > 0) {
		float *prevVert = &_verts[_numVerts - 1]->x;

		for (int i = 0; i < _numVerts; i++) {
			float *vert = &_verts[i]->x;
			float edgeU = (float)(-((double)vert[_axisU] - prevVert[_axisU]));
			float edgeV = (float)((double)vert[_axisV] - prevVert[_axisV]);
			float side = (float)(((double)point[_axisU] - prevVert[_axisU]) * edgeV + ((double)point[_axisV] - prevVert[_axisV]) * edgeU);

			side = (float)((double)side + 0.00001f);

			float absSide = side < 0.0f ? -side : side;

			if (0.00001f < absSide && side < 0.0f)
				return 0;

			prevVert = vert;
		}
	}

	return 1;
}

int WalkPlane::within(float *point, uint flags) {
	if (_system->_remastered)
		return withinRemastered(point, flags);

	if (flags) {
		if (!_enabled)
			return 0;

		if (!_active)
			return 0;
	}

	double distance = ((double)point[0] - _verts[1]->x) * _normal[0] + ((double)point[2] - _verts[1]->z) * _normal[2] + ((double)point[1] - _verts[1]->y) * _normal[1];

	distance = fabs(distance);
	if (distance <= 0.00001f)
		distance = 0.0;

	if (distance > _tolerance)
		return 0;

	if (_numVerts > 0) {
		float *prevVert = &_verts[_numVerts - 1]->x;

		for (int i = 0; i < _numVerts; i++) {
			float *vert = &_verts[i]->x;

			double side = -((double)vert[_axisU] - prevVert[_axisU]) * ((double)point[_axisV] - prevVert[_axisV]) + ((double)vert[_axisV] - prevVert[_axisV]) * ((double)point[_axisU] - prevVert[_axisU]);

			if (!_system->_demo)
				side += 0.00001f;

			if (fabs(side) <= 0.00001f)
				side = 0.0;

			if (side < 0.0)
				return 0;

			prevVert = vert;
		}
	}

	return 1;
}

int WalkPlane::closestPointRemastered(float *point) {
	project(point);

	if (_numVerts <= 0)
		return _numVerts;

	float *prevVert = &_verts[_numVerts - 1]->x;

	for (int i = 0; i < _numVerts; i++) {
		float *vert = &_verts[i]->x;
		float edgeY = (float)(-((double)vert[1] - prevVert[1]));
		float edgeX = (float)((double)vert[0] - prevVert[0]);
		float side = (float)(((double)point[0] - prevVert[0]) * edgeY + ((double)point[1] - prevVert[1]) * edgeX);

		if (side < 0.0f) {
			float denominator = (float)(-((double)edgeY * edgeY) - (double)edgeX * edgeX);
			float absDenominator = denominator < 0.0f ? -denominator : denominator;

			if (0.00001f < absDenominator && denominator != 0.0f) {
				float t = (float)((double)side / denominator);
				float newX = (float)((double)edgeY * t + point[0]);

				point[0] = newX;

				float newY = (float)((double)edgeX * t + point[1]);

				point[1] = newY;

				float deltaX = (float)((double)vert[0] - prevVert[0]);
				float deltaY = (float)((double)vert[1] - prevVert[1]);
				float absDeltaX = deltaX < 0.0f ? -deltaX : deltaX;
				float absDeltaY = deltaY < 0.0f ? -deltaY : deltaY;
				float param;

				if (absDeltaY < absDeltaX)
					param = (float)(((double)newX - prevVert[0]) / deltaX);
				else
					param = (float)(((double)newY - prevVert[1]) / deltaY);

				if (i == _numVerts - 1 && 1.0f < param) {
					point[0] = vert[0];
					point[1] = vert[1];
					point[2] = vert[2];
				} else if (param < 0.0f) {
					point[0] = prevVert[0];
					point[1] = prevVert[1];
					point[2] = prevVert[2];
				} else {
					project(point);
				}
			}
		}

		prevVert = vert;
	}

	return _numVerts;
}

int WalkPlane::closestPoint(float *point) {
	if (_system->_remastered)
		return closestPointRemastered(point);

	project(point);

	if (_numVerts > 0) {
		float *prevVert = &_verts[_numVerts - 1]->x;

		for (int i = 0; i < _numVerts; i++) {
			float *vert = &_verts[i]->x;

			float edgeY = (float)(-((double)vert[1] - prevVert[1]));
			float edgeX = (float)((double)vert[0] - prevVert[0]);
			double side = ((double)point[0] - prevVert[0]) * edgeY + ((double)point[1] - prevVert[1]) * edgeX;
			float sideStored = (float)side;

			if (side < 0.0) {
				double denominator = -((double)edgeY * edgeY) - (double)edgeX * edgeX;

				if (fabs(denominator) > 0.00001f) {
					double t = (double)sideStored / denominator;
					double newX = t * edgeY + point[0];
					float newY = (float)(t * edgeX + point[1]);

					point[0] = (float)newX;
					point[1] = newY;

					double deltaX = (double)vert[0] - prevVert[0];
					float deltaY = (float)((double)vert[1] - prevVert[1]);
					double param;

					if (fabs(deltaX) <= fabs((double)deltaY))
						param = ((double)newY - prevVert[1]) / deltaY;
					else
						param = (newX - prevVert[0]) / deltaX;

					if (i == _numVerts - 1 && param > 1.0) {
						point[0] = vert[0];
						point[1] = vert[1];
						point[2] = vert[2];
					} else if (param < 0.0) {
						point[0] = prevVert[0];
						point[1] = prevVert[1];
						point[2] = prevVert[2];
					} else {
						project(point);
					}
				}
			}

			prevVert = vert;
		}
	}

	return _numVerts;
}

double WalkPlane::intersection(float *a, float *b, float *out) {
	RdVector3 edge;

	return intersection(a, b, out, edge);
}

double WalkPlane::intersectionRemastered(float *a, float *b, float *out, float *edge) {
	float *hitVert = nullptr;
	RdVector3 hitVec = { 0.0f, 0.0f, 0.0f };

	_hitEdge = -1;

	if (within(b, 1))
		return 0.0;

	edge[0] = 0.0f;
	edge[1] = 0.0f;
	edge[2] = 0.0f;
	out[0] = b[0];
	out[1] = b[1];
	out[2] = b[2];

	if (_numVerts > 0) {
		float *prevVert = &_verts[_numVerts - 1]->x;

		for (int i = 0; i < _numVerts; i++) {
			float *vert = &_verts[i]->x;
			float edgeNormal[2];

			edgeNormal[0] = (float)(-((double)vert[_axisU] - prevVert[_axisU]));
			edgeNormal[1] = (float)((double)vert[_axisV] - prevVert[_axisV]);

			float side = (float)(((double)out[_axisU] - prevVert[_axisU]) * edgeNormal[1] + ((double)out[_axisV] - prevVert[_axisV]) * edgeNormal[0]);
			float absSide = side < 0.0f ? -side : side;

			if (0.00001f < absSide && side < 0.0f) {
				float deltaV = (float)((double)out[_axisV] - a[_axisV]);
				float deltaU = (float)((double)out[_axisU] - a[_axisU]);
				float denominator = (float)(-((double)edgeNormal[0] * deltaV) - (double)deltaU * edgeNormal[1]);
				float absDenominator = denominator < 0.0f ? -denominator : denominator;

				if (0.00001f < absDenominator && denominator != 0.0f) {
					float t = (float)((double)side / denominator);

					hitVert = prevVert;
					out[_axisV] = (float)((double)deltaV * t + out[_axisV]);
					out[_axisU] = (float)((double)deltaU * t + out[_axisU]);

					hitVec[0] = (float)((double)prevVert[0] - vert[0]);
					hitVec[1] = (float)((double)prevVert[1] - vert[1]);
					hitVec[2] = (float)((double)prevVert[2] - vert[2]);
					edge[0] = (float)((double)vert[0] - prevVert[0]);
					edge[1] = (float)((double)vert[1] - prevVert[1]);
					edge[2] = (float)((double)vert[2] - prevVert[2]);
					_hitEdge = i;
				}
			}

			prevVert = vert;
		}
	}

	project(out);

	if (!hitVert) {
		snapTo(out);
		return 0.0;
	}

	RdVector3 toA;
	toA[0] = (float)((double)a[0] - b[0]);
	toA[1] = (float)((double)a[1] - b[1]);
	toA[2] = (float)((double)a[2] - b[2]);

	_system->_rd->normalize3Acc(hitVec);
	_system->_rd->normalize3Acc(toA);

	float value = _system->_rd->dot3(hitVec, toA);
	float angle = _system->_stdMath->arcSin3(value);

	if (angle > 0.0f)
		return (float)((double)90.2f - angle);

	return (float)((double)-90.2f - angle);
}

double WalkPlane::intersection(float *a, float *b, float *out, float *edge) {
	if (_system->_remastered)
		return intersectionRemastered(a, b, out, edge);

	float *hitVert = nullptr;
	RdVector3 hitVec = { 0.0f, 0.0f, 0.0f };

	_hitEdge = -1;

	if (within(b, 1))
		return 0.0;

	edge[0] = 0.0f;
	edge[1] = 0.0f;
	edge[2] = 0.0f;
	out[0] = b[0];
	out[1] = b[1];
	out[2] = b[2];

	if (_numVerts > 0) {
		float *prevVert = &_verts[_numVerts - 1]->x;

		for (int i = 0; i < _numVerts; i++) {
			float *vert = &_verts[i]->x;
			float edgeNormal[2];

			edgeNormal[0] = (float)(-((double)vert[_axisU] - prevVert[_axisU]));
			edgeNormal[1] = (float)((double)vert[_axisV] - prevVert[_axisV]);

			float side = (float)(((double)out[_axisU] - prevVert[_axisU]) * edgeNormal[1] + ((double)out[_axisV] - prevVert[_axisV]) * edgeNormal[0]);

			if (side < -0.00001f) {
				double deltaV = (double)out[_axisV] - a[_axisV];
				double deltaU = (double)out[_axisU] - a[_axisU];
				float deltaUStored = (float)deltaU;
				double denominator = -(deltaV * edgeNormal[0]) - deltaU * edgeNormal[1];

				if (fabs(denominator) > 0.00001f) {
					double t = (double)side / denominator;

					hitVert = prevVert;
					out[_axisV] = (float)(deltaV * t + out[_axisV]);
					out[_axisU] = (float)(deltaUStored * t + out[_axisU]);

					hitVec[0] = (float)((double)prevVert[0] - vert[0]);
					hitVec[1] = (float)((double)prevVert[1] - vert[1]);
					hitVec[2] = (float)((double)prevVert[2] - vert[2]);
					edge[0] = (float)((double)vert[0] - prevVert[0]);
					edge[1] = (float)((double)vert[1] - prevVert[1]);
					edge[2] = (float)((double)vert[2] - prevVert[2]);
					_hitEdge = i;
				}
			}

			prevVert = vert;
		}
	}

	project(out);

	if (!hitVert) {
		snapTo(out);
		return 0.0;
	}

	RdVector3 toA;
	toA[0] = (float)((double)a[0] - b[0]);
	toA[1] = (float)((double)a[1] - b[1]);
	toA[2] = (float)((double)a[2] - b[2]);

	_system->_rd->normalize3Acc(hitVec);
	_system->_rd->normalize3Acc(toA);

	float value = _system->_rd->dot3(hitVec, toA);
	double angle = _system->_stdMath->arcSin3(value);

	if (angle <= 0.0)
		return -90.2f - angle;

	return 90.2f - angle;
}

void WalkPlane::snapTo(float *point) {
	if (_system->_remastered) {
		float planeDistance = (float)((((double)point[1] - _verts[0]->y) * _normal[1] + ((double)point[0] - _verts[0]->x) * _normal[0]) + ((double)point[2] - _verts[0]->z) * _normal[2]);

		point[0] = (float)((double)point[0] - (double)planeDistance * _normal[0]);
		point[1] = (float)((double)point[1] - _normal[1] * (double)planeDistance);
		point[2] = (float)((double)point[2] - _normal[2] * (double)planeDistance);
		return;
	}

	double distance = ((double)point[0] - _verts[0]->x) * _normal[0] + ((double)point[2] - _verts[0]->z) * _normal[2] + ((double)point[1] - _verts[0]->y) * _normal[1];

	point[0] = (float)(point[0] - distance * _normal[0]);
	point[1] = (float)(point[1] - distance * _normal[1]);
	point[2] = (float)(point[2] - distance * _normal[2]);
}

void WalkPlane::project(float *point) {
	int axisW;

	if (_axisU == 1 && _axisV == 0) {
		axisW = 2;
	} else if (_axisU == 0 && _axisV == 2) {
		axisW = 1;
	} else if (_axisU == 2 && _axisV == 1) {
		// The demo does not project on these planes
		if (_system->_demo)
			return;

		axisW = 0;
	} else {
		warning("WalkPlane::project(): invalid projection axes %d, %d", _axisU, _axisV);
		return;
	}

	if (_system->_remastered) {
		float negatedDistance = (float)-(((double)_verts[0]->y * _normal[1] + (double)_verts[0]->x * _normal[0]) + (double)_verts[0]->z * _normal[2]);

		point[axisW] = (float)((((double)_normal[_axisU] * point[_axisU] + (double)_normal[_axisV] * point[_axisV]) + negatedDistance) / -(double)_normal[axisW]);
		return;
	}

	double planeDistance = (double)_normal[0] * _verts[0]->x + (double)_normal[1] * _verts[0]->y + (double)_normal[2] * _verts[0]->z;

	point[axisW] =(float)((((double)_normal[_axisV] * point[_axisV] + (double)_normal[_axisU] * point[_axisU]) - planeDistance) / -(double)_normal[axisW]);
}

void WalkPlane::draw(uint16 color) {
	if (_active && _numVerts > 0) {
		float *prevVert = &_verts[_numVerts - 1]->x;

		for (int i = 0; i < _numVerts; i++) {
			float *vert = &_verts[i]->x;

			_system->_rd->_primit3.drawLine(prevVert, vert, nullptr, nullptr, nullptr, nullptr, color, 0xFFFFFFFF);
			prevVert = vert;
		}
	}
}

void WalkPlane::jumpOut(Puck *puck) {
	if (_numVerts == 4) {
		RdVector3 a;
		RdVector3 b;
		RdVector3 out;

		a[0] = puck->_pos[0];
		a[1] = puck->_pos[1];
		a[2] = puck->_pos[2];

		b[0] = (float)((double)puck->_orient[1][0] * -10.0f + a[0]);
		b[1] = (float)((double)puck->_orient[1][1] * -10.0f + a[1]);
		b[2] = (float)((double)puck->_orient[1][2] * -10.0f + a[2]);

		intersection(a, b, out);

		int edge = _hitEdge;

		if (edge != -1) {
			float *vert = &_verts[edge]->x;
			int prevEdge = edge - 1;

			if (prevEdge <= -1)
				prevEdge = _numVerts - 1;

			double deltaX = (double)_verts[prevEdge]->x - vert[0];
			double t;

			if (fabs(deltaX) > 0.00001f)
				t = ((double)out[0] - vert[0]) / deltaX;
			else
				t = ((double)out[1] - vert[1]) / ((double)_verts[prevEdge]->y - vert[1]);

			double inverseT = 1.0 - t;
			int nextEdge = (edge + 2) % _numVerts;

			_hitEdge = nextEdge;

			float *to = &_verts[nextEdge]->x;
			int beforeNext = nextEdge - 1;

			if (beforeNext <= -1)
				beforeNext = _numVerts - 1;

			float *from = &_verts[beforeNext]->x;

			out[0] = (float)(((double)from[0] - to[0]) * inverseT + to[0]);
			out[1] = (float)(((double)from[1] - to[1]) * inverseT + to[1]);
			out[2] = (float)(((double)from[2] - to[2]) * inverseT + to[2]);

			puck->setPos(out);
		}
	}
}

float *WalkPlane::getConstraintVector(Puck *puck) {
	float *constraint = _system->_constraintVector;
	RdVector3 a;
	RdVector3 b;

	a[0] = puck->_pos[0];
	a[1] = puck->_pos[1];
	a[2] = puck->_pos[2];

	float reach = 100.0f;
	if (puck->_forwardDist > 0.0f)
		reach = -100.0f;

	b[0] = (float)((double)reach * puck->_orient[1][0] + a[0]);
	b[1] = (float)((double)reach * puck->_orient[1][1] + a[1]);
	b[2] = (float)((double)reach * puck->_orient[1][2] + a[2]);

	intersection(a, b, constraint);

	if (_hitEdge == -1)
		return nullptr;

	WpVertex *hitVert = _verts[_hitEdge];

	double deltaX = (double)hitVert->x - constraint[0];
	double deltaY = (double)hitVert->y - constraint[1];
	double deltaZ = (double)hitVert->z - constraint[2];
	float deltaXStored = (float)deltaX;
	float deltaZStored = (float)deltaZ;

	// The demo uses a larger distance to the corners
	double cornerDist = _system->_demo ? 0.01 : 0.0025;

	if (deltaX * deltaXStored + deltaY * deltaY + deltaZ * deltaZStored >= cornerDist) {
		int prevEdge = _hitEdge - 1;

		if (prevEdge <= -1)
			prevEdge = _numVerts - 1;

		WpVertex *prevVert = _verts[prevEdge];

		double probeX = (double)prevVert->x - constraint[0];
		double probeY = (double)prevVert->y - constraint[1];
		double probeZ = (double)prevVert->z - constraint[2];

		if (probeX * probeX + probeY * probeY + probeZ * probeZ < cornerDist) {
			RdVector3 vec;

			vec[0] = (float)((double)prevVert->x - hitVert->x);
			vec[1] = (float)((double)prevVert->y - _verts[_hitEdge]->y);
			vec[2] = (float)((double)prevVert->z - _verts[_hitEdge]->z);

			_system->_rd->normalize3Acc(vec);

			float value = _system->_rd->dot3(vec, puck->_orient[1]);
			double angle = 90.0 - _system->_stdMath->arcSin3(value);

			if (angle < 1.0 || angle > 179.0) {
				int newEdge = _hitEdge - 1;

				if (newEdge <= -1)
					newEdge = _numVerts - 1;

				_hitEdge = newEdge;
			}
		}
	} else {
		int prevEdge = _hitEdge - 1;

		if (prevEdge <= -1)
			prevEdge = _numVerts - 1;

		RdVector3 vec;

		vec[0] = (float)((double)_verts[prevEdge]->x - hitVert->x);
		vec[1] = (float)((double)_verts[prevEdge]->y - _verts[_hitEdge]->y);
		vec[2] = (float)((double)_verts[prevEdge]->z - _verts[_hitEdge]->z);

		_system->_rd->normalize3Acc(vec);

		float value = _system->_rd->dot3(vec, puck->_orient[1]);
		double angle = 90.0 - _system->_stdMath->arcSin3(value);

		if (angle < 1.0 || angle > 179.0)
			_hitEdge = (_hitEdge + 1) % _numVerts;
	}

	float *edgeVert = &_verts[_hitEdge]->x;
	int edgeBefore = _hitEdge - 1;

	if (edgeBefore <= -1)
		edgeBefore = _numVerts - 1;

	float *edgePrev = &_verts[edgeBefore]->x;
	double edgeDeltaX = (double)edgePrev[0] - edgeVert[0];
	double t;

	if (fabs(edgeDeltaX) > 0.00001f)
		t = ((double)constraint[0] - edgeVert[0]) / edgeDeltaX;
	else
		t = ((double)constraint[1] - edgeVert[1]) / ((double)edgePrev[1] - edgeVert[1]);

	if (0.15f > t)
		t = 0.15f;

	if (0.85f < t)
		t = 0.85f;

	float *nextVert = &_verts[(_hitEdge + 1) % _numVerts]->x;
	float *otherVert = &_verts[(_numVerts + _hitEdge - 2) % _numVerts]->x;

	double targetX = ((double)otherVert[0] - nextVert[0]) * t + nextVert[0];
	double targetY = ((double)otherVert[1] - nextVert[1]) * t + nextVert[1];
	double targetZ = ((double)otherVert[2] - nextVert[2]) * t + nextVert[2];

	constraint[0] = (float)(targetX - constraint[0]);
	constraint[1] = (float)(targetY - constraint[1]);
	constraint[2] = (float)(targetZ - constraint[2]);

	_system->_rd->normalize3Acc(constraint);

	if (puck->_forwardDist < 0.0f) {
		constraint[0] = (float)(constraint[0] * -1.0f);
		constraint[1] = (float)(constraint[1] * -1.0f);
		constraint[2] = (float)(constraint[2] * -1.0f);
	}

	return constraint;
}

int WalkPlane::clipPoly(const float *poly, int numPoly, float *out) {
	float bufferA[36] = { 0.0f };
	float bufferB[36] = { 0.0f };
	float *in = bufferA;
	float *newObject = bufferB;
	int count = numPoly;

	if (numPoly > 12) {
		warning("WalkPlane::clipPoly(): polygon has %d vertices, only 12 are supported", numPoly);
		return 0;
	}

	if (numPoly > 0)
		memcpy(bufferA, poly, sizeof(float) * 3 * numPoly);

	if (_numVerts > 0) {
		float *prevVert = &_verts[_numVerts - 1]->x;

		for (int i = 0; i < _numVerts; i++) {
			float *vert = &_verts[i]->x;
			float edgeU = (float)(-((double)vert[_axisU] - prevVert[_axisU]));
			float edgeV = (float)((double)vert[_axisV] - prevVert[_axisV]);

			if (count <= 0)
				return 0;

			float *prevIn = &in[3 * (count - 1)];
			float prevDist = (float)(((double)prevIn[_axisV] - prevVert[_axisV]) * edgeU + ((double)prevIn[_axisU] - prevVert[_axisU]) * edgeV);
			int outCount = 0;

			for (int j = 0; j < count; j++) {
				float *curIn = &in[3 * j];
				double dist = ((double)curIn[_axisV] - prevVert[_axisV]) * edgeU + ((double)curIn[_axisU] - prevVert[_axisU]) * edgeV;

				bool addIntersection = (prevDist > 0.0f && dist < 0.0) || (prevDist < 0.0f && dist > 0.0);
				bool addVertex = dist > 0.0 && prevDist != 0.0f;

				if (addIntersection) {
					double deltaV = (double)curIn[_axisV] - prevIn[_axisV];
					double deltaU = (double)curIn[_axisU] - prevIn[_axisU];
					float deltaUStored = (float)deltaU;
					double denominator = -(deltaV * edgeU) - deltaU * edgeV;

					if (fabs(denominator) > 0.00001f) {
						if (outCount >= 12) {
							warning("WalkPlane::clipPoly(): clipped polygon has more than 12 vertices");
							return 0;
						}

						double t = dist / denominator;
						float *newVert = &newObject[3 * outCount];

						newVert[_axisV] = (float)(deltaV * t + curIn[_axisV]);
						newVert[_axisU] = (float)(deltaUStored * t + curIn[_axisU]);
						outCount++;
					}
				}

				if (addVertex) {
					if (outCount >= 12) {
						warning("WalkPlane::clipPoly(): clipped polygon has more than 12 vertices");
						return 0;
					}

					memcpy(&newObject[3 * outCount], curIn, sizeof(float) * 3);
					outCount++;
				}

				prevDist = (float)dist;
				prevIn = curIn;
			}

			if (outCount < 2)
				return 0;

			float *swap = in;
			in = newObject;
			newObject = swap;
			count = outCount;
			prevVert = vert;
		}
	}

	for (int i = 0; i < count; i++) {
		memcpy(&out[3 * i], &in[3 * i], sizeof(float) * 3);
		project(&out[3 * i]);
	}

	return count;
}

double WalkPlane::distToEdge(int edge, float *point) {
	if (edge < 0 || edge >= _numVerts) {
		warning("WalkPlane::distToEdge(): edge %d out of range", edge);
		return 0.0;
	}

	float *start = &_verts[edge]->x;
	float *end = &_verts[(edge + 1) % _numVerts]->x;
	RdVector3 edgeVec;
	RdVector3 pointVec;
	RdVector3 edgeDir;
	RdVector3 pointDir;

	edgeVec[0] = (float)((double)end[0] - start[0]);
	edgeVec[1] = (float)((double)end[1] - start[1]);
	edgeVec[2] = (float)((double)end[2] - start[2]);
	pointVec[0] = (float)((double)point[0] - start[0]);
	pointVec[1] = (float)((double)point[1] - start[1]);
	pointVec[2] = (float)((double)point[2] - start[2]);

	_system->_rd->normalize3(edgeDir, edgeVec);
	float pointLength = _system->_rd->normalize3(pointDir, pointVec);

	float value = (float)((double)pointDir[0] * edgeDir[0] + (double)pointDir[1] * edgeDir[1] + (double)pointDir[2] * edgeDir[2]);
	float angle = (float)(90.0 - _system->_stdMath->arcSin3(value));
	float sinValue;
	float cosValue;

	_system->_stdMath->sinCos(angle, &sinValue, &cosValue);

	return (double)sinValue * pointLength;
}

int WalkPlane::removeShortEdge() {
	float minLength = 3.4028235e38f;
	int shortest = 0;

	if (_numVerts < 4)
		return 0;

	float *prevVert = &_verts[_numVerts - 1]->x;

	for (int i = 0; i < _numVerts; i++) {
		float *vert = &_verts[i]->x;
		double deltaY = (double)prevVert[1] - vert[1];
		double deltaZ = (double)prevVert[2] - vert[2];
		double deltaX = (double)prevVert[0] - vert[0];
		float deltaYStored = (float)deltaY;
		float deltaXStored = (float)deltaX;
		double lengthSq = deltaY * deltaYStored + deltaZ * deltaZ + deltaX * deltaXStored;

		if (lengthSq < minLength) {
			minLength = (float)lengthSq;
			shortest = (_numVerts + i - 1) % _numVerts;
		}

		prevVert = vert;
	}

	float *edgeStart = &_verts[shortest]->x;
	float *before = &_verts[(_numVerts + shortest - 1) % _numVerts]->x;
	float *after = &_verts[(shortest + 1) % _numVerts]->x;
	float *farVert = &_verts[(shortest + 2) % _numVerts]->x;
	float point[3];

	point[0] = farVert[0];
	point[1] = farVert[1];
	point[2] = farVert[2];

	float edgeU = (float)(-((double)edgeStart[_axisU] - before[_axisU]));
	float edgeV = (float)((double)edgeStart[_axisV] - before[_axisV]);
	float cross = (float)(((double)point[_axisU] - before[_axisU]) * edgeV + ((double)point[_axisV] - before[_axisV]) * edgeU);

	if (fabs((double)cross) <= 0.00001f)
		return 0;

	double deltaV = (double)point[_axisV] - after[_axisV];
	double deltaU = (double)point[_axisU] - after[_axisU];
	double denominator = -(deltaV * edgeU) - deltaU * edgeV;

	if (fabs(denominator) <= 0.00001f)
		return 0;

	double t = (double)cross / denominator;

	point[_axisV] = (float)(deltaV * t + point[_axisV]);
	point[_axisU] = (float)(deltaU * t + point[_axisU]);

	project(point);

	WpVertex *newVert = _system->addVertex(nullptr, point);

	if (!newVert)
		return 0;

	WpVertex *newArray[10];

	newArray[0] = newVert;
	for (int i = 0; i < _numVerts - 2; i++)
		newArray[1 + i] = _verts[(shortest + 2 + i) % _numVerts];

	_system->removeVertex(_verts[shortest]);
	_system->removeVertex(_verts[(shortest + 1) % _numVerts]);

	_numVerts--;

	for (int i = 0; i < _numVerts; i++)
		_verts[i] = newArray[i];

	return 1;
}

WalkSystem::WalkSystem(RenderDroid *rd, StdMath *stdMath, StdConffile *stdConffile, bool isDemo, bool isRemastered) {
	_rd = rd;
	_stdMath = stdMath;
	_stdConffile = stdConffile;

	memset(_vertDb, 0, sizeof(_vertDb));
	_totVerts = 0;
	_wpHead = nullptr;
	_curCollection = nullptr;
	memset(_collection, 0, sizeof(_collection));
	_puckHead = nullptr;
	_forwardCounter = 0;
	_demo = isDemo;
	_numDemoVerts = 0;
	_remastered = isRemastered;

	memset(_path, 0, sizeof(_path));
	_numPathPoints = 0;
	memset(_workingPath, 0, sizeof(_workingPath));
	_numWorkingPathPoints = 0;
	_totIterations = 0;
	_pathFindFail = 0;
	memset(_constraintVector, 0, sizeof(_constraintVector));
	_sectionName[0] = 0;

	_cache = nullptr;
	_vertCache = nullptr;
	_numPlanes = 0;
	_numCacheVerts = 0;
	_cacheBuilt = 0;
	_pathFinder = nullptr;
}

WalkSystem::~WalkSystem() {
	delete _pathFinder;
	_pathFinder = nullptr;
	destroyCache();

	for (int i = 0; i < 140; i++) {
		while (_collection[i].head) {
			WalkPlane *plane = _collection[i].head;
			delete plane;
		}
	}
}

PathFinder *WalkSystem::getPathFinder() {
	if (!_pathFinder)
		_pathFinder = new PathFinder(this);

	return _pathFinder;
}

void WalkSystem::markPathFinderDirty() {
	if (_pathFinder)
		_pathFinder->markDirty();
}

void WalkSystem::buildCache() {
	destroyCache();

	_numCacheVerts = 0;
	_numPlanes = 0;

	for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
		_numCacheVerts += plane->_numVerts;
		_numPlanes++;
	}

	_vertCache = new WpCacheVert[_numCacheVerts];

	int cursor = 0;
	for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
		for (int i = 0; i < plane->_numVerts; i++) {
			WpVertex *vert = plane->_verts[i];

			_vertCache[cursor].v.refCount = vert->refCount;
			_vertCache[cursor].v.x = vert->x;
			_vertCache[cursor].v.y = vert->y;
			_vertCache[cursor].v.z = vert->z;
			_vertCache[cursor].index = (int)(vert - _vertDb);
			cursor++;
		}
	}

	_cache = new WpCachePlane[_numPlanes];

	int planeIndex = 0;
	for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
		_cache[planeIndex].plane = plane;
		_cache[planeIndex].numVerts = plane->_numVerts;
		_cache[planeIndex].active = plane->_active;

		for (int i = 0; i < plane->_numVerts; i++)
			_cache[planeIndex].vertIdx[i] = (int)(plane->_verts[i] - _vertDb);

		planeIndex++;
	}

	_cacheBuilt = 1;
}

void WalkSystem::destroyCache() {
	_cacheBuilt = 0;

	delete[] _vertCache;
	delete[] _cache;

	_vertCache = nullptr;
	_cache = nullptr;
	_numPlanes = 0;
	_numCacheVerts = 0;
}

void WalkSystem::restoreCache() {
	if (_cacheBuilt) {
		for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
			for (int i = 0; i < plane->_numVerts; i++)
				removeVertex(plane->_verts[i]);
		}

		for (int i = 0; i < _numCacheVerts; i++) {
			WpVertex *vert = &_vertDb[_vertCache[i].index];

			vert->refCount = _vertCache[i].v.refCount;
			vert->x = _vertCache[i].v.x;
			vert->y = _vertCache[i].v.y;
			vert->z = _vertCache[i].v.z;
		}

		for (int i = 0; i < _numPlanes; i++) {
			WalkPlane *plane = _cache[i].plane;

			plane->_numVerts = _cache[i].numVerts;

			for (int j = 0; j < plane->_numVerts; j++)
				plane->_verts[j] = &_vertDb[_cache[i].vertIdx[j]];

			plane->_active = _cache[i].active;
		}

		_totVerts = 0;

		for (int i = 0; i < 1024; i++) {
			if (_vertDb[i].refCount > 0)
				_totVerts++;
		}

		markPathFinderDirty();
	}
}

void WalkSystem::killCollection(const char *collection) {
	WalkPlane *oldHead = _wpHead;

	setCollection(collection);

	if (oldHead == _wpHead)
		oldHead = nullptr;

	while (_wpHead) {
		WalkPlane *plane = _wpHead;
		delete plane;
	}

	_wpHead = oldHead;
}

void WalkSystem::setCollection(const char *name) {
	int index;

	for (index = 0; index < 140; index++) {
		if (!strcmp(_collection[index].name, name))
			break;
	}

	if (index >= 140) {
		_wpHead = nullptr;
		_curCollection = nullptr;
	} else {
		_wpHead = _collection[index].head;
		_curCollection = _collection[index].name;
	}
}

WalkPlane *WalkSystem::planeAt(float *point, int flags) {
	for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
		if ((flags & plane->_type) != 0 && plane->within(point, 1))
			return plane;
	}

	return nullptr;
}

WalkPlane *WalkSystem::planeNamed(const char *name) {
	WalkPlane *plane;

	for (plane = _wpHead; plane; plane = plane->_next) {
		if (!strcmp(plane->_name, name))
			break;
	}

	return plane;
}

WalkPlane *WalkSystem::planeNumbered(int index) {
	WalkPlane *plane;

	for (plane = _wpHead; plane; plane = plane->_next) {
		if (plane->_number == index)
			break;
	}

	return plane;
}

WalkPlane *WalkSystem::planeClosest(float *point, int flags) {
	WalkPlane *closest = nullptr;
	float minDistance = 3.4028235e38f;

	WalkPlane *found = planeAt(point, flags);
	if (found)
		return found;

	if (_remastered) {
		for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
			if ((flags & plane->_type) != 0 && plane->_enabled && plane->_active) {
				float pointArray[3];

				pointArray[0] = point[0];
				pointArray[1] = point[1];
				pointArray[2] = point[2];

				plane->closestPoint(pointArray);

				double deltaY = (double)pointArray[1] - point[1];
				float squareX = (float)(((double)pointArray[0] - point[0]) * ((double)pointArray[0] - point[0]));
				float squareY = (float)(deltaY * deltaY);
				double deltaZ = (double)pointArray[2] - point[2];
				float squareZ = (float)(deltaZ * deltaZ);
				double distance = ((double)squareY + squareX) + squareZ;

				if ((double)minDistance > distance) {
					minDistance = (float)(((double)squareX + squareY) + squareZ);
					closest = plane;
				}
			}
		}

		return closest;
	}

	for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
		if ((flags & plane->_type) != 0 && plane->_enabled && plane->_active) {
			float pointArray[3];

			pointArray[0] = point[0];
			pointArray[1] = point[1];
			pointArray[2] = point[2];

			plane->closestPoint(pointArray);

			double deltaZ = (double)pointArray[2] - point[2];
			double deltaY = (double)pointArray[1] - point[1];
			double deltaX = (double)pointArray[0] - point[0];
			float deltaYStored = (float)deltaY;
			float deltaXStored = (float)deltaX;
			double distance = deltaY * deltaYStored + deltaZ * deltaZ + deltaX * deltaXStored;

			if (distance < minDistance) {
				minDistance = (float)distance;
				closest = plane;
			}
		}
	}

	return closest;
}

void WalkSystem::allActive() {
	for (WalkPlane *plane = _wpHead; plane; plane = plane->_next)
		plane->_enabled = 1;
}

WpVertex *WalkSystem::addVertexDemo(float *pos) {
	WpVertex *free = nullptr;

	if (_numDemoVerts >= 2046) {
		warning("WalkSystem::addVertexDemo(): vertex database overflow");
		return nullptr;
	}

	for (int i = 0; i < _numDemoVerts; i++) {
		double deltaY = (double)pos[1] - _vertDb[i].y;
		double deltaX = (double)pos[0] - _vertDb[i].x;
		double deltaZ = (double)pos[2] - _vertDb[i].z;
		float deltaXStored = (float)deltaX;
		float deltaZStored = (float)deltaZ;

		if (deltaX * deltaXStored + deltaY * deltaY + deltaZ * deltaZStored < 0.00000001) {
			_vertDb[i].refCount++;
			return &_vertDb[i];
		}

		if (_vertDb[i].refCount == 0)
			free = &_vertDb[i];
	}

	if (!free) {
		free = &_vertDb[_numDemoVerts];
		_numDemoVerts++;
	}

	free->refCount = 1;
	free->x = pos[0];
	free->y = pos[1];
	free->z = pos[2];

	return free;
}

WpVertex *WalkSystem::addVertex(WalkPlane *plane, float *pos) {
	if (_demo)
		return addVertexDemo(pos);

	if (_totVerts >= 1022)
		warning("WalkSystem::addVertex(): vertex database is almost full (%d vertices)", _totVerts);

	for (int i = 0; i < 1024; i++) {
		if (_vertDb[i].refCount != 0) {
			double deltaY = (double)pos[1] - _vertDb[i].y;
			double deltaX = (double)pos[0] - _vertDb[i].x;
			double deltaZ = (double)pos[2] - _vertDb[i].z;
			float deltaXStored = (float)deltaX;
			float deltaZStored = (float)deltaZ;

			if (deltaX * deltaXStored + deltaY * deltaY + deltaZ * deltaZStored < 0.00000001) {
				_vertDb[i].refCount++;
				return &_vertDb[i];
			}
		}
	}

	WpVertex *vert = nullptr;

	for (int i = 0; i < 1024; i++) {
		if (_vertDb[i].refCount <= 0) {
			vert = &_vertDb[i];
			break;
		}
	}

	if (!vert) {
		warning("WalkSystem::addVertex(): vertex database is full");
		return nullptr;
	}

	vert->refCount = 1;
	_totVerts++;
	vert->x = pos[0];
	vert->y = pos[1];
	vert->z = pos[2];

	return vert;
}

void WalkSystem::removeVertex(WpVertex *vert) {
	if (_demo) {
		vert->refCount--;
		return;
	}

	if (vert->refCount > 0) {
		vert->refCount--;

		if (vert->refCount == 0)
			_totVerts--;
	}
}

const char *WalkSystem::parseCollection(const char *collection) {
	int numVerts = 0;
	int id = 0;
	int type = 0;
	int visible = 0;
	int numVertsRead = 0;
	int vertsParsed = 0;
	float height = 12345.0f;
	int alwaysMerge = 0;
	int numTriangles = 0;
	int trianglesParsed = 0;
	char sectorName[48];
	char word[48];
	char line[256];
	float verts[10][3];
	float overflow[3];
	int triangles[WalkPlane::kMaxTriangles][3];
	int overflowTriangle[3];
	int value;

	memset(verts, 0, sizeof(verts));
	memset(triangles, 0, sizeof(triangles));
	sectorName[0] = 0;
	_sectionName[0] = 0;

	while (_stdConffile->readLine()) {
		Common::strlcpy(line, _stdConffile->getLine(), sizeof(line));

		if (sscanf(line, " section: %47s", _sectionName) == 1) {
			if (numVerts > 2)
				newPlane(verts, numVerts, collection, sectorName, id, visible, type, alwaysMerge, numTriangles, triangles);

			return _sectionName;
		}

		if (!strstr(line, " numsectors ")) {
			if (sscanf(line, " sector %47s", word) == 1) {
				if (numVerts > 2) {
					WalkPlane *plane = newPlane(verts, numVerts, collection, sectorName, id, visible, type, alwaysMerge, numTriangles, triangles);

					if (height != 12345.0f) {
						if (type == 4096 && !_demo)
							height = 0.005f;

						if (height == 0.0f)
							plane->_tolerance = _demo ? 0.1f : 0.005f;
						else
							plane->_tolerance = height;
					}
				}

				sectorName[0] = 0;
				height = 12345.0f;
				Common::strlcpy(sectorName, word, sizeof(sectorName));
				id = 0;
				type = 0;
				visible = 0;
				numVertsRead = 0;
				vertsParsed = 0;
				numTriangles = 0;
				trianglesParsed = 0;
			} else if (sscanf(line, " id %i", &value) == 1) {
				id = value;
			} else if (sscanf(line, " type %47s", word) == 1) {
				if (!strcmp(word, "walk"))
					type = 4096;
				else if (!strcmp(word, "funnel"))
					type = 4352;
				else if (!strcmp(word, "camera"))
					type = 0x2000;
				else if (!strcmp(word, "special"))
					type = 0x4000;
				else if (!strcmp(word, "chernobyl"))
					type = 0x8000;
			} else if (sscanf(line, " default visibility %47s", word) == 1) {
				visible = strcmp(word, "invisible") != 0;
			} else if (sscanf(line, " always_merge %d", &alwaysMerge) != 1 && sscanf(line, " height %f", &height) != 1) {
				if (sscanf(line, " numvertices %d", &value) == 1) {
					numVertsRead = value;
				} else if (sscanf(line, " numtris %d", &value) == 1) {
					numTriangles = value;
				} else if (sscanf(line, " numgates %47s", word) != 1) {
					float *vert = overflow;
					if (vertsParsed < 10)
						vert = verts[vertsParsed];

					int *triangle = overflowTriangle;
					if (trianglesParsed < WalkPlane::kMaxTriangles)
						triangle = triangles[trianglesParsed];

					if (sscanf(line, " vertices: %f %f %f", &vert[0], &vert[1], &vert[2]) == 3 || (vertsParsed < numVertsRead && sscanf(line, " %f %f %f", &vert[0], &vert[1], &vert[2]) == 3)) {
						if (vertsParsed >= 10)
							warning("WalkSystem::parseCollection(): sector %s has more than 10 vertices", sectorName);

						vertsParsed++;
					} else if (sscanf(line, " triangles: %d %d %d", &triangle[0], &triangle[1], &triangle[2]) == 3) {
						if (trianglesParsed >= WalkPlane::kMaxTriangles)
							warning("WalkSystem::parseCollection(): sector %s has more than %d triangles", sectorName, (int)WalkPlane::kMaxTriangles);

						trianglesParsed++;
					} else if (trianglesParsed < numTriangles && sscanf(line, " %d %d %d", &triangle[0], &triangle[1], &triangle[2]) == 3) {
						if (trianglesParsed >= WalkPlane::kMaxTriangles)
							warning("WalkSystem::parseCollection(): sector %s has more than %d triangles", sectorName, (int)WalkPlane::kMaxTriangles);

						trianglesParsed++;
					}
				}
			}
		}

		numVerts = numVertsRead;
	}

	if (numVerts > 2)
		newPlane(verts, numVerts, collection, sectorName, id, visible, type, alwaysMerge, numTriangles, triangles);

	return nullptr;
}

WalkPlane *WalkSystem::newPlane(float (*verts)[3], int numPoints, const char *collection, const char *sectorName, int id, int visible, int type, int alwaysMerge,
		int numTriangles, int (*triangles)[3]) {
	WalkPlane *plane = new WalkPlane(this, verts, numPoints, collection);

	if (numTriangles > WalkPlane::kMaxTriangles) {
		warning("WalkSystem::newPlane(): sector %s has %d triangles, only %d are supported", sectorName, numTriangles, (int)WalkPlane::kMaxTriangles);
		numTriangles = WalkPlane::kMaxTriangles;
	}

	if (numTriangles < 0)
		numTriangles = 0;

	plane->_numTriangles = numTriangles;
	memcpy(plane->_triangles, triangles, numTriangles * sizeof(plane->_triangles[0]));
	plane->setName(sectorName);
	plane->_active = visible;
	plane->_number = id;
	markPathFinderDirty();
	plane->setType(type);
	plane->_alwaysMerge = alwaysMerge;

	return plane;
}

int WalkSystem::closestVertWithin(WalkPlane *a, WalkPlane *b, float *target, float *out) {
	float minDistance = 3.4028235e38f;

	for (int i = 0; i < a->_numVerts; i++) {
		WpVertex *vert = a->_verts[i];

		if (b->within(&vert->x, 0)) {
			double deltaZ = (double)vert->z - target[2];
			double deltaY = (double)vert->y - target[1];
			double deltaX = (double)vert->x - target[0];
			float deltaYStored = (float)deltaY;
			float deltaXStored = (float)deltaX;
			double distance = deltaY * deltaYStored + deltaZ * deltaZ + deltaX * deltaXStored;

			if (distance < minDistance) {
				minDistance = (float)distance;
				out[0] = vert->x;
				out[1] = vert->y;
				out[2] = vert->z;
			}
		}
	}

	for (int i = 0; i < b->_numVerts; i++) {
		WpVertex *vert = b->_verts[i];

		if (a->within(&vert->x, 0)) {
			double deltaZ = (double)vert->z - target[2];
			double deltaY = (double)vert->y - target[1];
			double deltaX = (double)vert->x - target[0];
			float deltaYStored = (float)deltaY;
			float deltaXStored = (float)deltaX;
			double distance = deltaY * deltaYStored + deltaZ * deltaZ + deltaX * deltaXStored;

			if (distance < minDistance) {
				minDistance = (float)distance;
				out[0] = vert->x;
				out[1] = vert->y;
				out[2] = vert->z;
			}
		}
	}

	return minDistance != 3.4028235e38f;
}

WalkPlane *WalkSystem::planeContaining(float *a, float *b) {
	WalkPlane *plane = planeAt(a, 4096);

	while (plane) {
		if (plane->within(b, 1))
			break;

		plane->_enabled = 0;
		plane = planeAt(a, 4096);
	}

	allActive();

	return plane;
}

int WalkSystem::getPath(float **path) {
	*path = &_path[0][0];

	return _numPathPoints;
}

int WalkSystem::buildPath(float *from, float *to, int tighten) {
	int result = 0;

	allActive();
	_numPathPoints = 0;

	if (!planeAt(from, 4096))
		return 0;

	// The demo does not check the destination
	if (!_demo && !planeAt(to, 4096))
		return 0;

	if (!_demo && g_grim->useRemasterFixes())
		return buildPathWithFixes(from, to, tighten);

	if (findPath(from, to, nullptr)) {
		if (_numPathPoints >= 30) {
			warning("WalkSystem::buildPath(): path has too many points");
		} else {
			_path[_numPathPoints][0] = to[0];
			_path[_numPathPoints][1] = to[1];
			_path[_numPathPoints][2] = to[2];
			_numPathPoints++;

			if (tighten)
				tightenPath();
		}

		result = 1;
	}

	allActive();

	return result;
}

int WalkSystem::buildPathWithFixes(float *from, float *to, int tighten) {
	int result = 0;

	if (findPathRemastered(from, to, nullptr, 0)) {
		_workingPath[_numWorkingPathPoints][0] = to[0];
		_workingPath[_numWorkingPathPoints][1] = to[1];
		_workingPath[_numWorkingPathPoints][2] = to[2];
		_numWorkingPathPoints++;
		result = 1;
	} else if (findPathRemastered(from, to, nullptr, 1)) {
		result = 1;
	}

	if (result && tighten)
		tightenPathRemastered();

	_numPathPoints = _numWorkingPathPoints;

	if (_numPathPoints > 30) {
		if (result)
			warning("WalkSystem::buildPathWithFixes(): path has too many points");

		_numPathPoints = 30;
	}

	memcpy(_path, _workingPath, _numPathPoints * sizeof(_path[0]));

	allActive();

	return result;
}

int WalkSystem::findPath(float *from, float *to, WalkPlane *exclude) {
	if (_numPathPoints + 1 >= 30) {
		if (exclude)
			exclude->_enabled = 0;

		return 1;
	}

	WalkPlane *plane = exclude;
	if (!plane)
		plane = planeAt(from, 4096);

	if (!plane)
		return 0;

	_path[_numPathPoints][0] = from[0];
	_path[_numPathPoints][1] = from[1];
	_path[_numPathPoints][2] = from[2];
	_numPathPoints++;

	if (plane->within(to, 1))
		return 1;

	int savedPathPoints = _numPathPoints;
	float hit[3];

	plane->intersection(from, to, hit);
	plane->_enabled = 0;

	double deltaZ = (double)hit[2] - from[2];
	double deltaY = (double)hit[1] - from[1];
	double deltaX = (double)hit[0] - from[0];
	float deltaYStored = (float)deltaY;
	float deltaZStored = (float)deltaZ;
	float deltaXStored = (float)deltaX;
	WalkPlane *nextPlane = nullptr;

	// The demo always looks for the next plane
	if (_demo || deltaY * deltaYStored + deltaZ * deltaZStored + deltaX * deltaXStored > 0.0001f)
		nextPlane = planeAt(hit, 4096);

	if (nextPlane) {
		if (findPath(hit, to, nextPlane))
			return 1;

		nextPlane->_enabled = 0;
	}

	float exits[120];
	int numExits = findExitPoints(plane, to, exits);

	for (int i = 0; i < numExits; i++) {
		WalkPlane *exit = planeAt(&exits[4 * i], 4096);

		if (exit) {
			_numPathPoints = savedPathPoints;

			if (closestVertWithin(plane, exit, to, hit)) {
				if (findPath(hit, to, exit))
					return 1;
			}
		}
	}

	plane->_enabled = 1;

	if (nextPlane)
		nextPlane->_enabled = 1;

	return 0;
}

int WalkSystem::findExitPoints(WalkPlane *plane, float *from, float *out) {
	float exits[30][4];
	int count = 0;

	for (WalkPlane *other = _wpHead; other; other = other->_next) {
		if (other != plane && other->_enabled && (other->_type & 0x1000) != 0) {
			for (int i = 0; i < other->_numVerts; i++) {
				WpVertex *vert = other->_verts[i];

				if (plane->within(&vert->x, 0)) {
					int j;

					for (j = 0; j < count; j++) {
						if (!memcmp(exits[j], &vert->x, 12))
							break;
					}

					if (j == count) {
						if (count >= 30) {
							warning("WalkSystem::findExitPoints(): more than 30 exit points");
							continue;
						}

						exits[count][0] = vert->x;
						exits[count][1] = vert->y;
						exits[count][2] = vert->z;

						double deltaX = (double)from[0] - exits[count][0];
						double deltaY = (double)from[1] - exits[count][1];
						double deltaZ = (double)from[2] - exits[count][2];
						float deltaXStored = (float)deltaX;
						float deltaZStored = (float)deltaZ;

						exits[count][3] = (float)(deltaX * deltaXStored + deltaY * deltaY + deltaZ * deltaZStored);
						count++;
					}
				}
			}
		}
	}

	if (count > 0) {
		memcpy(&out[0], exits[0], sizeof(float) * 4);

		for (int i = 1; i < count; i++) {
			float distance = exits[i][3];
			int j = i - 1;

			while (j > -1) {
				if (out[4 * j + 3] <= (double)distance)
					break;

				memcpy(&out[4 * (j + 1)], &out[4 * j], sizeof(float) * 4);
				j--;
			}

			memcpy(&out[4 * (j + 1)], exits[i], sizeof(float) * 4);
		}
	}

	return count;
}

void WalkSystem::tightenPath() {
	int changed = 0;

	if (_numPathPoints - 3 > 0) {
		int last = 2;
		int i = 0;

		do {
			float *first = _path[last - 2];
			float *mid = _path[last - 1];
			float *lastPointer = _path[last];

			WalkPlane *planeA = planeContaining(first, mid);
			WalkPlane *planeB = planeContaining(mid, lastPointer);

			if (!planeA || !planeB)
				return;

			float point[3];

			planeA->intersection(first, lastPointer, point);

			WalkPlane *planeC = planeContaining(point, first);

			if (planeContaining(point, lastPointer)) {
				if (planeC) {
					double deltaY = (double)point[1] - mid[1];
					double deltaX = (double)point[0] - mid[0];
					double deltaZ = (double)point[2] - mid[2];
					float deltaXStored = (float)deltaX;
					float deltaZStored = (float)deltaZ;

					if (deltaY * deltaY + deltaX * deltaXStored + deltaZ * deltaZStored > 0.0025f) {
						mid[0] = point[0];
						mid[1] = point[1];
						mid[2] = point[2];
						changed = 1;
					}
				}
			}

			last++;
			i++;
		} while (i < _numPathPoints - 3);
	}

	if (_numPathPoints - 2 > 0) {
		int j = 1;

		do {
			float *prevPt = _path[j - 1];
			float *pt = _path[j];
			double deltaY = (double)prevPt[1] - pt[1];
			double deltaX = (double)prevPt[0] - pt[0];
			double deltaZ = (double)prevPt[2] - pt[2];
			float deltaXStored = (float)deltaX;
			float deltaZStored = (float)deltaZ;

			if (deltaY * deltaY + deltaX * deltaXStored + deltaZ * deltaZStored < 0.0001f) {
				if (planeContaining(prevPt, pt)) {
					changed = 1;
					_numPathPoints--;

					for (int k = j; k < _numPathPoints; k++) {
						_path[k][0] = _path[k + 1][0];
						_path[k][1] = _path[k + 1][1];
						_path[k][2] = _path[k + 1][2];
					}
				}
			}

			j++;
		} while (j - 1 < _numPathPoints - 2);
	}

	if (changed)
		tightenPath();
}

int WalkSystem::getPathRemastered(float **path) {
	*path = &_workingPath[0][0];

	return _numWorkingPathPoints;
}

int WalkSystem::findPathRemastered(float *from, float *to, WalkPlane *plane, unsigned int mode) {
	if (!plane) {
		allActive();
		_pathFindFail = 0;
		_numWorkingPathPoints = 0;
		_totIterations = 0;
		plane = planeAt(from, 4096);
	}

	_totIterations++;

	if (_numWorkingPathPoints + 1 >= 128 || _totIterations > 2000) {
		if (plane)
			plane->_enabled = 0;
		else
			warning("WalkSystem::findPathRemastered(): no walk plane when the path is too long");

		_pathFindFail = 1;

		return 0;
	}

	if (!plane)
		return 0;

	_workingPath[_numWorkingPathPoints][0] = from[0];
	_workingPath[_numWorkingPathPoints][1] = from[1];
	_workingPath[_numWorkingPathPoints][2] = from[2];
	_numWorkingPathPoints++;

	if (plane->within(to, 1))
		return 1;

	int savedPathPoints = _numWorkingPathPoints;
	float hit[3];
	float edge[3];

	plane->intersection(from, to, hit, edge);
	plane->_enabled = 0;

	double deltaY = (double)hit[1] - from[1];
	double deltaX = (double)hit[0] - from[0];
	double deltaZ = (double)hit[2] - from[2];
	float squareX = (float)(deltaX * deltaX);
	float squareY = (float)(deltaY * deltaY);
	float squareZ = (float)(deltaZ * deltaZ);
	WalkPlane *nextPlane = nullptr;

	if (((double)squareX + squareY) + squareZ > 0.00009999999747378752) {
		nextPlane = planeAt(hit, 4096);

		if (nextPlane && findPathRemastered(hit, to, nextPlane, mode))
			return 1;
	}

	if (_pathFindFail)
		return 0;

	if (nextPlane)
		nextPlane->_enabled = 0;
	else if (mode)
		return 1;

	float exits[128 * 4];
	int numExits = findExitPointsRemastered(plane, to, exits);

	for (int i = 0; i < numExits; i++) {
		WalkPlane *exit = planeAt(&exits[4 * i], 4096);

		if (exit) {
			_numWorkingPathPoints = savedPathPoints;

			if (closestVertWithinRemastered(plane, exit, to, hit) && findPathRemastered(hit, to, exit, mode))
				return 1;

			if (_pathFindFail)
				return 0;
		}
	}

	plane->_enabled = 1;

	if (nextPlane)
		nextPlane->_enabled = 1;

	return 0;
}

int WalkSystem::findExitPointsRemastered(WalkPlane *plane, float *to, float *out) {
	float exits[128][4];
	int count = 0;

	for (WalkPlane *other = _wpHead; other; other = other->_next) {
		if (other == plane || !other->_enabled || (other->_type & 0x1000) == 0 || other->_numVerts <= 0)
			continue;

		for (int i = 0; i < other->_numVerts; i++) {
			WpVertex *vert = other->_verts[i];

			if (!plane->within(&vert->x, 0))
				continue;

			int j;

			for (j = 0; j < count; j++) {
				if (!memcmp(exits[j], &vert->x, 12))
					break;
			}

			if (j != count)
				continue;

			if (count >= 128) {
				warning("WalkSystem::findExitPointsRemastered(): more than 128 exit points");
				continue;
			}

			exits[count][0] = vert->x;
			exits[count][1] = vert->y;
			exits[count][2] = vert->z;

			double deltaY = (double)to[1] - exits[count][1];
			double deltaX = (double)to[0] - exits[count][0];
			double deltaZ = (double)to[2] - exits[count][2];
			float squareX = (float)(deltaX * deltaX);
			float squareY = (float)(deltaY * deltaY);
			float squareZ = (float)(deltaZ * deltaZ);

			exits[count][3] = (float)(((double)squareX + squareY) + squareZ);
			count++;
		}
	}

	if (count > 0) {
		memcpy(&out[0], exits[0], sizeof(float) * 4);

		for (int i = 1; i < count; i++) {
			float distance = exits[i][3];
			int j = i - 1;

			while (j > -1) {
				if (!(out[4 * j + 3] > distance))
					break;

				memcpy(&out[4 * (j + 1)], &out[4 * j], sizeof(float) * 4);
				j--;
			}

			memcpy(&out[4 * (j + 1)], exits[i], sizeof(float) * 4);
		}
	}

	return count;
}

int WalkSystem::closestVertWithinRemastered(WalkPlane *a, WalkPlane *b, float *target, float *out) {
	float minDistance = 3.4028235e38f;

	for (int i = 0; i < a->_numVerts; i++) {
		WpVertex *vert = a->_verts[i];

		if (b->within(&vert->x, 0)) {
			double deltaX = (double)vert->x - target[0];
			double deltaZ = (double)vert->z - target[2];
			float squareX = (float)(deltaX * deltaX);
			double deltaY = (double)vert->y - target[1];
			float squareY = (float)(deltaY * deltaY);
			float squareZ = (float)(deltaZ * deltaZ);
			float distance = (float)(((double)squareX + squareY) + squareZ);

			if ((double)minDistance > distance) {
				minDistance = distance;
				out[0] = vert->x;
				out[1] = vert->y;
				out[2] = vert->z;
			}
		}
	}

	for (int i = 0; i < b->_numVerts; i++) {
		WpVertex *vert = b->_verts[i];

		if (a->within(&vert->x, 0)) {
			double deltaX = (double)vert->x - target[0];
			double deltaZ = (double)vert->z - target[2];
			float squareX = (float)(deltaX * deltaX);
			double deltaY = (double)vert->y - target[1];
			float squareY = (float)(deltaY * deltaY);
			float squareZ = (float)(deltaZ * deltaZ);
			float distance = (float)(((double)squareX + squareY) + squareZ);

			if ((double)minDistance > distance) {
				minDistance = distance;
				out[0] = vert->x;
				out[1] = vert->y;
				out[2] = vert->z;
			}
		}
	}

	return minDistance != 3.4028235e38f;
}

void WalkSystem::tightenPathRemastered() {
	int changed = 0;

	if (_numWorkingPathPoints - 3 > 0) {
		int last = 2;
		int i = 0;

		do {
			float *first = _workingPath[last - 2];
			float *mid = _workingPath[last - 1];
			float *lastPointer = _workingPath[last];

			WalkPlane *planeA = planeContaining(first, mid);
			WalkPlane *planeB = planeContaining(mid, lastPointer);

			if (!planeA || !planeB)
				return;

			float point[3];
			float edge[3];

			planeA->intersection(first, lastPointer, point, edge);

			WalkPlane *planeC = planeContaining(point, first);
			WalkPlane *planeD = planeContaining(point, lastPointer);

			if (planeD && planeC) {
				double deltaY = (double)point[1] - mid[1];
				double deltaX = (double)point[0] - mid[0];
				double deltaZ = (double)point[2] - mid[2];
				float squareX = (float)(deltaX * deltaX);
				float squareY = (float)(deltaY * deltaY);
				float squareZ = (float)(deltaZ * deltaZ);

				if (((double)squareX + squareY) + squareZ > 0.002500000176951289) {
					mid[0] = point[0];
					mid[1] = point[1];
					mid[2] = point[2];
					changed = 1;
				}
			}

			last++;
			i++;
		} while (i < _numWorkingPathPoints - 3);
	}

	if (_numWorkingPathPoints - 2 > 0) {
		int j = 1;
		int limit = _numWorkingPathPoints - 2;

		do {
			float *prevPoint = _workingPath[j - 1];
			float *point = _workingPath[j];
			double deltaY = (double)prevPoint[1] - point[1];
			double deltaX = (double)prevPoint[0] - point[0];
			double deltaZ = (double)prevPoint[2] - point[2];
			float squareY = (float)(deltaY * deltaY);
			float squareX = (float)(deltaX * deltaX);
			float squareZ = (float)(deltaZ * deltaZ);

			if (((double)squareY + squareX) + squareZ < 0.00009999999747378752) {
				if (planeContaining(prevPoint, point)) {
					changed = 1;
					limit--;
					_numWorkingPathPoints--;

					for (int k = j; k < _numWorkingPathPoints; k++) {
						_workingPath[k][0] = _workingPath[k + 1][0];
						_workingPath[k][1] = _workingPath[k + 1][1];
						_workingPath[k][2] = _workingPath[k + 1][2];
					}
				}
			}

			j++;
		} while (j - 1 < limit);
	}

	if (changed)
		tightenPathRemastered();
}

void WalkSystem::shrinkBoxes(float amount) {
	int vertexUseCount[1024];
	float vertexOffsetSum[3072];
	float offsets[30];

	if (amount < 0.0f)
		return;

	memset(vertexOffsetSum, 0, sizeof(vertexOffsetSum));
	memset(vertexUseCount, 0, sizeof(vertexUseCount));

	for (WalkPlane *plane = _wpHead; plane; plane = plane->_next) {
		if ((plane->_type & 0x1000) == 0 || !plane->_active)
			continue;

		bool restart;

		do {
			restart = false;
			memset(offsets, 0, sizeof(offsets));

			float *vert = &plane->_verts[0]->x;
			float *prevVert = &plane->_verts[plane->_numVerts - 1]->x;
			int corner = 0;
			bool valid = true;

			while (valid && corner < plane->_numVerts) {
				float *nextVert = &plane->_verts[(corner + 1) % plane->_numVerts]->x;
				RdVector3 toPrev;
				RdVector3 toNext;
				RdVector3 bisector;
				RdVector3 point;

				toPrev[0] = (float)((double)prevVert[0] - vert[0]);
				toPrev[1] = (float)((double)prevVert[1] - vert[1]);
				toPrev[2] = (float)((double)prevVert[2] - vert[2]);
				_rd->normalize3Acc(toPrev);

				toNext[0] = (float)((double)nextVert[0] - vert[0]);
				toNext[1] = (float)((double)nextVert[1] - vert[1]);
				toNext[2] = (float)((double)nextVert[2] - vert[2]);
				_rd->normalize3Acc(toNext);

				bisector[0] = (float)((double)toNext[0] + toPrev[0]);
				bisector[1] = (float)((double)toNext[1] + toPrev[1]);
				bisector[2] = (float)((double)toNext[2] + toPrev[2]);
				_rd->normalize3Acc(bisector);

				float value = (float)((double)toNext[0] * bisector[0] + (double)toNext[1] * bisector[1] + (double)toNext[2] * bisector[2]);
				float angle = (float)(90.0 - _stdMath->arcSin3(value));
				float sinValue;
				float cosValue;

				_stdMath->sinCos(angle, &sinValue, &cosValue);

				double offsetLength = (double)amount / sinValue;
				float offsetLengthStored = (float)offsetLength;

				point[0] = (float)(offsetLength * bisector[0] + vert[0]);
				point[1] = (float)((double)offsetLengthStored * bisector[1] + vert[1]);
				point[2] = (float)((double)offsetLengthStored * bisector[2] + vert[2]);

				if (plane->within(point, 1)) {
					double limit = (double)amount - 0.001;

					for (int edge = 0; edge < plane->_numVerts; edge++) {
						if (plane->distToEdge(edge, point) < limit) {
							valid = false;
							break;
						}
					}
				} else {
					valid = false;
				}

				if (valid) {
					offsets[3 * corner] = (float)((double)offsetLengthStored * bisector[0]);
					offsets[3 * corner + 1] = (float)((double)offsetLengthStored * bisector[1]);
					offsets[3 * corner + 2] = (float)((double)offsetLengthStored * bisector[2]);

					corner++;
					prevVert = vert;
					vert = nextVert;
				}
			}

			if (valid) {
				for (int i = 0; i < plane->_numVerts; i++) {
					int index = (int)(plane->_verts[i] - _vertDb);

					vertexOffsetSum[3 * index] = (float)(offsets[3 * i] + vertexOffsetSum[3 * index]);
					vertexOffsetSum[3 * index + 1] = (float)(vertexOffsetSum[3 * index + 1] + offsets[3 * i + 1]);
					vertexOffsetSum[3 * index + 2] = (float)(offsets[3 * i + 2] + vertexOffsetSum[3 * index + 2]);
					vertexUseCount[index]++;
				}
			} else if (plane->removeShortEdge()) {
				restart = true;
			} else {
				plane->_active = 0;
				markPathFinderDirty();
			}
		} while (restart);
	}

	for (int i = 0; i < 1024; i++) {
		if (vertexUseCount[i] != 0) {
			double inverse = 1.0 / (double)vertexUseCount[i];

			_vertDb[i].x = (float)(vertexOffsetSum[3 * i] * inverse + _vertDb[i].x);
			_vertDb[i].y = (float)(vertexOffsetSum[3 * i + 1] * inverse + _vertDb[i].y);
			_vertDb[i].z = (float)(vertexOffsetSum[3 * i + 2] * inverse + _vertDb[i].z);
		}
	}

	markPathFinderDirty();
}

} // End of namespace Grim
