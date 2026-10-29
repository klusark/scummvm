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

#include "common/util.h"

#include "engines/grim/debug.h"
#include "engines/grim/grim.h"
#include "engines/grim/sector.h"
#include "engines/grim/textsplit.h"
#include "engines/grim/savegame.h"
#include "engines/grim/set.h"
#include "engines/grim/sincos.h"

namespace Grim {

Sector::Sector() :
		_vertices(nullptr), _origVertices(nullptr), _sortplanes(nullptr),_invalid(false),
		_shrinkRadius(0.f), _numVertices(0), _id(0), _numSortplanes(0),
		_type(NoneType), _visible(false), _height(0.f), _grimGeo(false), _grimFree(true), _grimExitEdge(-1), _grimFunnelEdge(0), _grimShrunk(false), _origNumVertices(0), _origVisible(false), _axisA(1), _axisB(0), _gHeight(0.005f) {
	_gNormal[0] = _gNormal[1] = _gNormal[2] = 0.f;
 }

Sector::Sector(const Sector &other) :
		_vertices(nullptr), _origVertices(nullptr), _sortplanes(nullptr),
		_numSortplanes(0), _grimGeo(false), _grimFree(true), _grimExitEdge(-1), _grimFunnelEdge(0), _grimShrunk(false), _origNumVertices(0), _origVisible(false), _axisA(1), _axisB(0), _gHeight(0.005f) {
	*this = other;
}

Sector::~Sector() {
	delete[] _vertices;
	delete[] _origVertices;
	delete[] _sortplanes;
}

void Sector::saveState(SaveGame *savedState) const {
	// A sector that the shrinking of the original has taken vertices from or switched off is saved as it was before: the format has one count
	const bool asOriginal = _grimShrunk && _origVertices && (_origNumVertices != _numVertices || _origVisible != _visible);
	const Math::Vector3d *saved = asOriginal ? _origVertices : _vertices;
	const int count = asOriginal ? _origNumVertices : _numVertices;
	savedState->writeLESint32(count);
	savedState->writeLESint32(_id);
	savedState->writeLESint32(_type);
	savedState->writeBool(asOriginal ? _origVisible : _visible);
	savedState->writeFloat(_height);

	savedState->writeString(_name);

	for (int i = 0; i < count + 1; ++i) {
		savedState->writeVector3d(saved[i]);
	}

	savedState->writeVector3d(_normal);

	savedState->writeFloat(asOriginal ? 0.f : _shrinkRadius);
	savedState->writeBool(_invalid);
	if (!asOriginal && _shrinkRadius != 0.f && !_invalid) {
		for (int i = 0; i < _numVertices + 1; ++i) {
			savedState->writeVector3d(_origVertices[i]);
		}
	}

	if (savedState->saveMinorVersion() > 8 && g_grim->getGameType() == GType_MONKEY4) {
		savedState->writeLEUint32(_numSortplanes);
		for (int i = 0; i < _numSortplanes; ++i) {
			savedState->writeLEUint32(_sortplanes[i]);
		}
	}
}

bool Sector::restoreState(SaveGame *savedState) {
	_numVertices = savedState->readLESint32();
	_id          = savedState->readLESint32();
	_type        = (SectorType)savedState->readLESint32();
	_visible     = savedState->readBool();
	_height      = savedState->readFloat();

	_name        = savedState->readString();

	_vertices = new Math::Vector3d[_numVertices + 1];
	for (int i = 0; i < _numVertices + 1; ++i) {
		_vertices[i] = savedState->readVector3d();
	}

	_normal = savedState->readVector3d();

	_shrinkRadius = savedState->readFloat();
	_invalid = savedState->readBool();
	if (_shrinkRadius != 0.f && !_invalid) {
		_origVertices = new Math::Vector3d[_numVertices + 1];
		for (int i = 0; i < _numVertices + 1; ++i) {
			_origVertices[i] = savedState->readVector3d();
		}
		// (what the shrinking of the original keeps to give back: the count and the visibility are the saved ones)
		_grimShrunk = g_grim->getGameType() == GType_GRIM;
		_origNumVertices = _numVertices;
		_origVisible = _visible;
	} else {
		_origVertices = nullptr;
	}
	if (savedState->saveMinorVersion() > 8 && g_grim->getGameType() == GType_MONKEY4) {
		_numSortplanes = savedState->readLEUint32();
		_sortplanes = new int[_numSortplanes];
		for (int i = 0; i < _numSortplanes; ++i) {
			_sortplanes[i] = savedState->readLEUint32();
		}
	}
	prepareGrimGeometry();
	return true;
}

void Sector::load(TextSplitter &ts) {
	char buf[256];
	int ident = 0, i = 0;
	Math::Vector3d tempVert;

	// Sector NAMES can be null, but ts isn't flexible enough
	if (strlen(ts.getCurrentLine()) > strlen(" sector"))
		ts.scanString(" sector %256s", 1, buf);
	else {
		ts.nextLine();
		buf[0] = '\0';
	}

	ts.scanString(" id %d", 1, &ident);

	_name = buf;
	_id = ident;
	ts.scanString(" type %256s", 1, buf);

	if (strstr(buf, "walk"))
		_type = WalkType;

	else if (strstr(buf, "funnel"))
		_type = FunnelType;
	else if (strstr(buf, "camera"))
		_type = CameraType;
	else if (strstr(buf, "special"))
		_type = SpecialType;
	else if (strstr(buf, "chernobyl"))
		_type = HotType;
	else
		Debug::error(Debug::Sets, "Unknown sector type '%s' in room setup", buf);

	ts.scanString(" default visibility %256s", 1, buf);
	if (strcmp(buf, "visible") == 0)
		_visible = true;
	else if (strcmp(buf, "invisible") == 0)
		_visible = false;
	else
		error("Invalid visibility spec: %s", buf);
	ts.scanString(" height %f", 1, &_height);
	ts.scanString(" numvertices %d", 1, &_numVertices);
	_vertices = new Math::Vector3d[_numVertices + 1];

	ts.scanString(" vertices: %f %f %f", 3, &_vertices[0].x(), &_vertices[0].y(), &_vertices[0].z());
	for (i = 1; i < _numVertices; i++)
		ts.scanString(" %f %f %f", 3, &_vertices[i].x(), &_vertices[i].y(), &_vertices[i].z());

	// Repeat the last vertex for convenience
	_vertices[_numVertices] = _vertices[0];

	_normal = Math::Vector3d::crossProduct(_vertices[1] - _vertices[0], _vertices[_numVertices - 1] - _vertices[0]);
	float length = _normal.getMagnitude();
	if (length > 0)
		_normal /= length;
	prepareGrimGeometry();

	// Remastered
	if (!ts.checkString("numtris")) {
		return;
	}
	int _numTris;
	ts.scanString(" numtris %d", 1, &_numTris);
	//_vertices = new Math::Vector3d[_numVertices + 1];
	int a,b,c;
	if (_numTris > 0) {
		ts.scanString(" triangles: %f %f %f", 3, &a, &b, &c);
		for (i = 1; i < _numTris; i++)
			ts.scanString(" %f %f %f", 3, &a, &b, &c);
	}
}

void Sector::loadBinary(Common::SeekableReadStream *data) {
	_numVertices = data->readUint32LE();
	_vertices = new Math::Vector3d[_numVertices + 1];
	for (int i = 0; i < _numVertices; i++) {
		_vertices[i].readFromStream(data);
	}

	// Repeat the last vertex for convenience
	_vertices[_numVertices] = _vertices[0];

	_normal = Math::Vector3d::crossProduct(_vertices[1] - _vertices[0], _vertices[_numVertices - 1] - _vertices[0]);
	float length = _normal.getMagnitude();
	if (length > 0)
		_normal /= length;

	char name[128];
	int nameLength = data->readUint32LE();

	data->read(name, nameLength);
	_name = name;

	_id = data->readUint32LE();

	_visible = data->readByte();

	_type = (SectorType)data->readUint32LE();

	_numSortplanes = data->readUint32LE();
	_sortplanes = new int[_numSortplanes];
	for (int i = 0; i < _numSortplanes; ++i) {
		_sortplanes[i] = data->readUint32LE();
	}

	_height = data->readFloatLE();
}

void Sector::setVisible(bool vis) {
	_visible = vis;
}

void Sector::shrink(float radius) {
	if ((getType() & WalkType) == 0 || _shrinkRadius == radius)
		return;

	_shrinkRadius = radius;
	if (!_origVertices) {
		_origVertices = _vertices;
		_vertices = new Math::Vector3d[_numVertices + 1];
	}

	// Move each vertex inwards by the given amount.
	for (int j = 0; j < _numVertices; j++) {
		Math::Vector3d shrinkDir;

		for (int k = 0; k < g_grim->getCurrSet()->getSectorCount(); k++) {
			Sector *other = g_grim->getCurrSet()->getSectorBase(k);
			if ((other->getType() & WalkType) == 0)
				continue;

			for (int l = 0; l < other->_numVertices; l++) {
				Math::Vector3d *otherVerts = other->_vertices;
				if (other->_origVertices)
					otherVerts = other->_origVertices;
				if ((otherVerts[l] - _origVertices[j]).getMagnitude() < 0.01f) {
					Math::Vector3d e1 = otherVerts[l + 1] - otherVerts[l];
					Math::Vector3d e2;
					if (l - 1 >= 0)
						e2 = otherVerts[l] - otherVerts[l - 1];
					else
						e2 = otherVerts[l] - otherVerts[other->_numVertices - 1];
					e1.normalize();
					e2.normalize();
					Math::Vector3d bisector = (e1 - e2);
					bisector.normalize();
					shrinkDir += bisector;
				}
			}
		}

		if (shrinkDir.getMagnitude() > 0.1f) {
			shrinkDir.normalize();
			_vertices[j] = _origVertices[j] + shrinkDir * radius;
		} else {
			_vertices[j] = _origVertices[j];
		}
	}

	_vertices[_numVertices] = _vertices[0];

	// Make sure the sector is still convex.
	for (int j = 0; j < _numVertices; j++) {
		Math::Vector3d e1 = _vertices[j + 1] - _vertices[j];
		Math::Vector3d e2;
		if (j - 1 >= 0)
			e2 = _vertices[j] - _vertices[j - 1];
		else
			e2 = _vertices[j] - _vertices[_numVertices - 1];

		if (e1.x() * e2.y() > e1.y() * e2.x()) {
			// Not convex, so mark the sector invalid.
			_invalid = true;
			delete[] _vertices;
			_vertices = _origVertices;
			_origVertices = nullptr;
			break;
		}
	}
}

void Sector::unshrink() {
	if (_shrinkRadius != 0.f) {
		_shrinkRadius = 0.f;
		_invalid = false;
		if (_origVertices) {
			delete[] _vertices;
			_vertices = _origVertices;
			_origVertices = nullptr;
		}
		if (_grimShrunk) {
			// (the shrinking of the original may have taken vertices away and switched the sector off)
			_grimShrunk = false;
			_numVertices = _origNumVertices;
			_visible = _origVisible;
		}
	}
}

void Sector::grimPlane(float *normal, float *origin, int &axisA, int &axisB) const {
	for (int i = 0; i < 3; i++)
		normal[i] = _gNormal[i];
	origin[0] = _vertices[0].x();
	origin[1] = _vertices[0].y();
	origin[2] = _vertices[0].z();
	axisA = _axisA;
	axisB = _axisB;
}

// FUN_004a7000: the coordinate of a point on the plane of the sector that is not one of the two axes of the sector
void Sector::grimFixPlaneCoordinate(float *p) const {
	const int a = _axisA, b = _axisB;
	int k;
	if (a == 1 && b == 0)
		k = 2;
	else if (a == 0 && b == 2)
		k = 1;
	else
		k = 0;
	const Math::Vector3d &v0 = _vertices[0];
	const long double plane = (long double)_gNormal[2] * v0.z() + (long double)_gNormal[0] * v0.x() + (long double)_gNormal[1] * v0.y();
	p[k] = (float)(((long double)_gNormal[a] * p[a] + (long double)_gNormal[b] * p[b] + -plane) / -(long double)_gNormal[k]);
}

bool Sector::grimContains(const float *p, bool checkActive) const {
	if (checkActive && (!_grimFree || !isVisible()))
		return false;
	return isPointInSector(Math::Vector3d(p[0], p[1], p[2]), true);
}

// Sector_FindExitPoint (FUN_004a6d10): the target moves along the line to the start onto each edge it is outside of, then onto the plane. The angle is the
// one between that edge and the line, taken from an arcsine series, 90.2 degrees off the right angle.
float Sector::grimFindExitPoint(const float *from, const float *target, float *out) const {
	typedef long double LD;
	_grimExitEdge = -1;
	if (grimContains(target, true))
		return 0.f;
	out[0] = target[0];
	out[1] = target[1];
	out[2] = target[2];
	const int a = _axisA, b = _axisB;
	const Math::Vector3d *prev = &_vertices[_numVertices - 1];
	const Math::Vector3d *hitPrev = nullptr, *hitCur = nullptr;
	for (int i = 0; i < _numVertices; ++i) {
		const Math::Vector3d *cur = &_vertices[i];
		const float pb = prev->getValue(b), pa = prev->getValue(a);
		const float l14 = cur->getValue(b) - pb;
		const float l18 = -(cur->getValue(a) - pa);
		float f5 = (float)(((LD)out[b] - pb) * l18 + ((LD)out[a] - pa) * l14);
		float f2 = fabsf(f5) <= 1e-05f ? 0.f : f5;
		if (f2 < 0.f) {
			const float o1 = out[a], o2 = from[a];
			// (the value stays on the x87 stack unrounded; the difference of the a axis is stored as a float first, and that one is used for the new a)
			const LD B = (LD)out[b] - from[b];
			const float A = (float)((LD)o1 - o2);
			const LD f8 = -(B * l18) - ((LD)o1 - o2) * l14;
			const float f6 = fabsl(f8) <= (LD)1e-05f ? 0.f : (float)f8;
			if (f6 != 0.f) {
				const LD ratio = (LD)f5 / f8;
				out[b] = (float)(B * ratio + out[b]);
				out[a] = (float)((LD)A * ratio + out[a]);
				hitPrev = prev;
				hitCur = cur;
				_grimExitEdge = i;
			}
		}
		prev = cur;
	}
	grimFixPlaneCoordinate(out);
	if (hitPrev) {
		float e[3] = { hitPrev->x() - hitCur->x(), hitPrev->y() - hitCur->y(), hitPrev->z() - hitCur->z() };
		float d[3] = { from[0] - target[0], from[1] - target[1], from[2] - target[2] };
		for (int pass = 0; pass < 2; ++pass) {
			float *v = pass == 0 ? e : d;
			const LD len = sqrtl((LD)v[0] * v[0] + (LD)v[2] * v[2] + (LD)v[1] * v[1]);
			if (len != 0.L) {
				const LD inv = 1.L / len;
				v[0] = (float)(inv * v[0]);
				v[1] = (float)(v[1] * inv);
				v[2] = (float)(inv * v[2]);
			}
		}
		const LD dot = (LD)e[0] * d[0] + (LD)d[2] * e[2] + (LD)e[1] * d[1];
		const LD asinDeg = (LD)originalAsinDeg((float)dot);
		// (the constants are the floats 90.2f at 0x4d892c and -90.2f at 0x4d8930, not 90.2)
		const LD angle = asinDeg <= 0.L ? (LD)-90.2f - asinDeg : (LD)90.2f - asinDeg;
		return (float)angle;
	}
	// FUN_004a6fa0: onto the plane of the sector along its normal
	const Math::Vector3d &v0 = _vertices[0];
	const float dd = (float)(((LD)out[1] - v0.y()) * _gNormal[1] + ((LD)out[2] - v0.z()) * _gNormal[2] + ((LD)out[0] - v0.x()) * _gNormal[0]);
	out[0] = (float)((LD)out[0] - (LD)dd * _gNormal[0]);
	out[1] = (float)((LD)out[1] - (LD)dd * _gNormal[1]);
	out[2] = (float)((LD)out[2] - (LD)dd * _gNormal[2]);
	return 0.f;
}

// FUN_004a9070: how far a point is from the line of an edge
float Sector::grimEdgeDistance(int edge, const float *p) const {
	typedef long double LD;
	const Math::Vector3d &v0 = _vertices[edge];
	const Math::Vector3d &v1 = _vertices[(edge + 1) % _numVertices];
	float e[3] = { v1.x() - v0.x(), v1.y() - v0.y(), v1.z() - v0.z() };
	float q[3] = { p[0] - v0.x(), p[1] - v0.y(), p[2] - v0.z() };
	float en[3], qn[3];
	const float elen = (float)sqrtl((LD)e[0] * e[0] + (LD)e[2] * e[2] + (LD)e[1] * e[1]);
	for (int i = 0; i < 3; ++i)
		en[i] = elen != 0.f ? e[i] / elen : e[i];
	const LD qlen = sqrtl((LD)q[0] * q[0] + (LD)q[2] * q[2] + (LD)q[1] * q[1]);
	const float qlenf = (float)qlen;
	for (int i = 0; i < 3; ++i)
		qn[i] = qlenf != 0.f ? q[i] / qlenf : q[i];
	const LD dot = (LD)qn[2] * en[2] + (LD)qn[1] * en[1] + (LD)qn[0] * en[0];
	const float angle = (float)(90.0L - (LD)originalAsinDeg((float)dot));
	float s, c;
	originalSinCos(angle, s, c);
	return (float)((LD)s * qlen);
}

// FUN_004a6b30: a point outside of the sector is moved onto the edges it is outside of (the plane coordinate first). The x87 keeps the differences, the sums of
// products and the length unrounded: floats are stored for the edge vector (ex, ey), for the side the point is on and for the new place only.
void Sector::grimClampPoint(float *p) const {
	typedef long double LD;
	grimFixPlaneCoordinate(p);
	const int n = _numVertices;
	Math::Vector3d prev = _vertices[n - 1];
	for (int i = 0; i < n; ++i) {
		const Math::Vector3d cur = _vertices[i];
		const float ey = (float)-((LD)cur.y() - prev.y());
		const float ex = (float)((LD)cur.x() - prev.x());
		const LD sideExt = ((LD)p[1] - prev.y()) * ex + ((LD)p[0] - prev.x()) * ey;
		const float side = (float)sideExt;
		if (sideExt < 0.L) {
			const LD len2 = -((LD)ey * ey) - (LD)ex * ex;
			if (fabsl(len2) > (LD)1e-05f) {
				const LD t = (LD)side / len2;
				const float nx = (float)(t * ey + p[0]);
				const float ny = (float)(t * ex + p[1]);
				p[0] = nx;
				p[1] = ny;
				const LD dx = (LD)cur.x() - prev.x();
				const float dy = (float)((LD)cur.y() - prev.y());
				LD along;
				if (fabsl(dx) <= fabsf(dy))
					along = ((LD)ny - prev.y()) / dy;
				else
					along = ((LD)nx - prev.x()) / dx;
				if (i != n - 1 || along <= 1.0L) {
					if (0.L <= along) {
						grimFixPlaneCoordinate(p);
					} else {
						p[0] = prev.x();
						p[1] = prev.y();
						p[2] = prev.z();
					}
				} else {
					p[0] = cur.x();
					p[1] = cur.y();
					p[2] = cur.z();
				}
			}
		}
		prev = cur;
	}
}

void Sector::grimSaveForShrink(float radius) {
	if (_origVertices)
		return;
	_origVertices = new Math::Vector3d[_numVertices + 1];
	for (int i = 0; i < _numVertices + 1; ++i)
		_origVertices[i] = _vertices[i];
	_origNumVertices = _numVertices;
	_origVisible = _visible;
	_grimShrunk = true;
	_shrinkRadius = radius;
}

void Sector::grimSetVertex(int i, const Math::Vector3d &v) {
	_vertices[i] = v;
	if (i == 0)
		_vertices[_numVertices] = v;
}

// The vector divided by its length, as FUN_004469a0 does it
static long double shrinkNormalize(float *v) {
	typedef long double LD;
	const LD len = sqrtl((LD)v[0] * v[0] + (LD)v[2] * v[2] + (LD)v[1] * v[1]);
	if (len != 0.L) {
		const LD inv = 1.L / len;
		v[0] = (float)(inv * v[0]);
		v[1] = (float)(v[1] * inv);
		v[2] = (float)(v[2] * inv);
	}
	return len;
}

bool Sector::grimShrinkOffsets(float radius, float *offsets) const {
	typedef long double LD;
	const int n = _numVertices;
	for (int i = 0; i < n; ++i) {
		const Math::Vector3d &prev = _vertices[(i + n - 1) % n];
		const Math::Vector3d &cur = _vertices[i];
		const Math::Vector3d &next = _vertices[(i + 1) % n];
		// the two directions along the edges that meet in the vertex, and the bisector of them
		float a[3] = { prev.x() - cur.x(), prev.y() - cur.y(), prev.z() - cur.z() };
		shrinkNormalize(a);
		float b[3] = { next.x() - cur.x(), next.y() - cur.y(), next.z() - cur.z() };
		shrinkNormalize(b);
		float bis[3] = { a[0] + b[0], a[1] + b[1], a[2] + b[2] };
		shrinkNormalize(bis);
		// the vertex moves along the bisector until it is the radius away from both edges: radius / sin(half the angle)
		const LD dot = (LD)b[0] * bis[0] + (LD)b[1] * bis[1] + (LD)b[2] * bis[2];
		const float half = (float)(90.0L - (LD)originalAsinDeg((float)dot));
		float sn, cs;
		originalSinCos(half, sn, cs);
		const float scale = (float)((LD)radius / sn);
		const float base[3] = { cur.x(), cur.y(), cur.z() };
		float moved[3];
		for (int c = 0; c < 3; ++c)
			moved[c] = (float)((LD)scale * bis[c] + base[c]);
		// the new place has to be in the sector and the radius (less a millimetre) from the line of each of its edges
		if (!isPointInSector(Math::Vector3d(moved[0], moved[1], moved[2]), true))
			return false;
		for (int k = 0; k < n; ++k) {
			if ((LD)grimEdgeDistance(k, moved) < (LD)radius - 0.001L)
				return false;
		}
		for (int c = 0; c < 3; ++c)
			offsets[i * 3 + c] = (float)((LD)scale * bis[c]);
	}
	return true;
}

bool Sector::grimCollapseShortestEdge(Common::Array<Math::Vector3d> &pool) {
	const int n = _numVertices;
	if (n < 4)
		return false;
	// the shortest edge a - (a + 1)
	float best = 3.4028235e+38f;
	int a = 0;
	for (int i = 0; i < n; ++i) {
		const Math::Vector3d &p0 = _vertices[(i + n - 1) % n];
		const Math::Vector3d &p1 = _vertices[i];
		const float dx = p0.x() - p1.x();
		const float dy = p0.y() - p1.y();
		const float dz = p0.z() - p1.z();
		const float d2 = dx * dx + dz * dz + dy * dy;
		if (d2 < best) {
			a = (n - 1 + i) % n;
			best = d2;
		}
	}
	const Math::Vector3d A = _vertices[a];
	const Math::Vector3d P = _vertices[(a + n - 1) % n];
	const Math::Vector3d B = _vertices[(a + 1) % n];
	const Math::Vector3d Q = _vertices[(a + 2) % n];
	const int ax = _axisA, ay = _axisB;
	float q[3] = { Q.x(), Q.y(), Q.z() };
	const float n0 = -(A.getValue(ax) - P.getValue(ax));
	const float n1 = A.getValue(ay) - P.getValue(ay);
	// where the line of the edge before a meets the line of the edge after a + 1
	const float c1 = (q[ay] - P.getValue(ay)) * n0 + (q[ax] - P.getValue(ax)) * n1;
	if (fabsf(c1) <= 1e-05f)
		return false;
	const float dy = q[ay] - B.getValue(ay);
	const float dx = q[ax] - B.getValue(ax);
	const float c2 = -(dy * n0) - dx * n1;
	if (fabsf(c2) <= 1e-05f)
		return false;
	q[ay] = dy * (c1 / c2) + q[ay];
	q[ax] = dx * (c1 / c2) + q[ax];
	grimFixPlaneCoordinate(q);
	Math::Vector3d r(q[0], q[1], q[2]);
	bool found = false;
	for (uint k = 0; k < pool.size() && !found; ++k) {
		const float ddx = q[0] - pool[k].x(), ddy = q[1] - pool[k].y(), ddz = q[2] - pool[k].z();
		if (ddz * ddz + ddy * ddy + ddx * ddx < 1e-08f) {
			r = pool[k];
			found = true;
		}
	}
	if (!found)
		pool.push_back(r);
	Math::Vector3d *verts = new Math::Vector3d[n];
	verts[0] = r;
	for (int k = 1; k < n - 1; ++k)
		verts[k] = _vertices[(a + 2 + k - 1) % n];
	delete[] _vertices;
	_numVertices = n - 1;
	_vertices = new Math::Vector3d[_numVertices + 1];
	for (int k = 0; k < _numVertices; ++k)
		_vertices[k] = verts[k];
	_vertices[_numVertices] = _vertices[0];
	delete[] verts;
	return true;
}

float Sector::distanceToPoint(const Math::Vector3d &point) const {
	// The plane has equation ax + by + cz + d = 0
	float a = _normal.x();
	float b = _normal.y();
	float c = _normal.z();
	float d = -_vertices[0].x() * a - _vertices[0].y() * b - _vertices[0].z() * c;

	// dist is positive if it is above the plain, negative if it is
	// below and 0 if it is on the plane.
	float dist = (a * point.x() + b * point.y() + c * point.z() + d);
	dist /= sqrt(a * a + b * b + c * c);
	return dist;
}

// The height of the plane of the sector at the xz of the point, when that xz lies inside the polygon seen from above (ignoring the heights)
bool Sector::containsXZ(const Math::Vector3d &point, float *planeY, float margin) const {
	if (fabsf(_normal.y()) < 1e-4f)
		return false;
	double winding = _normal.y() > 0 ? 1.0 : -1.0;
	for (int i = 0; i < _numVertices; i++) {
		const double ex = (double)_vertices[i + 1].x() - _vertices[i].x();
		const double ez = (double)_vertices[i + 1].z() - _vertices[i].z();
		const double dx = (double)point.x() - _vertices[i].x();
		const double dz = (double)point.z() - _vertices[i].z();
		const double len = sqrt(ex * ex + ez * ez);
		if (len <= 0.0)
			continue;
		if (winding * (ez * dx - ex * dz) / len < margin - 0.00001)
			return false;
	}
	if (planeY)
		*planeY = _vertices[0].y() - (_normal.x() * (point.x() - _vertices[0].x()) + _normal.z() * (point.z() - _vertices[0].z())) / _normal.y();
	return true;
}

// The sector of the original (FUN_004a6490) works out its own normal with Newell's method, normalizes it and takes the two axes that are not the one
// where the normal is largest (compared as it is, not as an absolute value); its margin from the plane is 0.005 for a walk sector, for a sector with no
// height and for one with the height 0 (FUN_004a7bc0), the height of the file otherwise.
void Sector::prepareGrimGeometry() {
	_grimGeo = false;
	if (g_grim->getGameType() != GType_GRIM || _numVertices < 3)
		return;
	typedef long double LD;
	// (every line is one x87 expression: the sums and differences of the vertices and their product stay unrounded, a float is stored per line; the
	// normalizing is FUN_004469a0 with the length and its inverse in extended precision)
	float nx = 0.f, ny = 0.f, nz = 0.f;
	Math::Vector3d prev = _vertices[_numVertices - 1];
	for (int i = 0; i < _numVertices; ++i) {
		const Math::Vector3d &cur = _vertices[i];
		nx = (float)(((LD)prev.y() - cur.y()) * ((LD)prev.z() + cur.z()) + nx);
		ny = (float)(((LD)prev.z() - cur.z()) * ((LD)cur.x() + prev.x()) + ny);
		nz = (float)(((LD)prev.y() + cur.y()) * ((LD)prev.x() - cur.x()) + nz);
		prev = cur;
	}
	const LD len = sqrtl((LD)nx * nx + (LD)nz * nz + (LD)ny * ny);
	if (len != 0.L) {
		const LD inv = 1.L / len;
		nx = (float)(inv * nx);
		ny = (float)(ny * inv);
		nz = (float)(nz * inv);
	}
	_gNormal[0] = nx;
	_gNormal[1] = ny;
	_gNormal[2] = nz;
	float m = nx <= ny ? ny : nx;
	m = m <= nz ? nz : m;
	if (m == nx) {
		_axisA = 2;
		_axisB = 1;
	} else if (m == ny) {
		_axisA = 0;
		_axisB = 2;
	} else {
		_axisA = 1;
		_axisB = 0;
	}
	_gHeight = (_type == WalkType || _height == 0.f) ? 0.005f : _height;
	_grimGeo = true;
	prepareGrimFunnel();
}

// FUN_004a6790 for a funnel: a funnel has four vertices (any other is a walk sector), and the edge the walk goes along is the shorter one of the pair of
// opposite edges that is not the more parallel: of the edges 0-1 and 2-3 when those are the more parallel pair, else of 1-2 and 3-0.
void Sector::prepareGrimFunnel() {
	typedef long double LD;
	_grimFunnelEdge = 0;
	if (_type != FunnelType)
		return;
	if (_numVertices != 4) {
		_type = WalkType;
		return;
	}
	// (each vector is normalized as FUN_004469a0 does: the length in extended precision, the inverse stored to the floats)
	auto edge = [&](int from, int to, float *out, float &length) {
		out[0] = (float)((LD)_vertices[to].x() - _vertices[from].x());
		out[1] = (float)((LD)_vertices[to].y() - _vertices[from].y());
		out[2] = (float)((LD)_vertices[to].z() - _vertices[from].z());
		const LD len = sqrtl((LD)out[0] * out[0] + (LD)out[2] * out[2] + (LD)out[1] * out[1]);
		if (len != 0.L) {
			const LD inv = 1.L / len;
			out[0] = (float)(inv * out[0]);
			out[1] = (float)(out[1] * inv);
			out[2] = (float)(out[2] * inv);
		}
		length = (float)len;
	};
	auto parallel = [](const float *a, const float *b) {
		float d = (float)((LD)b[2] * a[2] + (LD)b[1] * a[1] + (LD)b[0] * a[0]);
		return d < 0.f ? -d : d;
	};
	float a[3], b[3], c[3], d[3], la, lb, lc, ld;
	edge(0, 1, a, la);
	edge(2, 3, b, lb);
	const float pAB = parallel(a, b);
	edge(1, 2, c, lc);
	edge(3, 0, d, ld);
	const float pCD = parallel(c, d);
	if (pCD < pAB)
		_grimFunnelEdge = la < lb ? 0 : 2;
	else
		_grimFunnelEdge = lc < ld ? 1 : 3;
}

void Sector::weldVertices(Common::Array<Math::Vector3d> &pool) {
	if (g_grim->getGameType() != GType_GRIM)
		return;
	for (int i = 0; i < _numVertices; ++i) {
		bool found = false;
		for (uint k = 0; k < pool.size(); ++k) {
			float dx = _vertices[i].x() - pool[k].x();
			float dy = _vertices[i].y() - pool[k].y();
			float dz = _vertices[i].z() - pool[k].z();
			if (dz * dz + dy * dy + dx * dx < 1e-08f) {
				_vertices[i] = pool[k];
				found = true;
				break;
			}
		}
		if (!found)
			pool.push_back(_vertices[i]);
	}
	_vertices[_numVertices] = _vertices[0];
	_normal = Math::Vector3d::crossProduct(_vertices[1] - _vertices[0], _vertices[_numVertices - 1] - _vertices[0]);
	float length = _normal.getMagnitude();
	if (length > 0)
		_normal /= length;
	prepareGrimGeometry();
}

bool Sector::isPointInSector(const Math::Vector3d &point, bool exact) const {
	if (exact && _grimGeo) {
		// FUN_004a6980: the distance from the plane (a distance up to 1e-5 is none), then, edge by edge, the cross product on the two axes with 1e-5 added: a
		// point up to 2e-5 outside an edge (in units of the cross product, that is 2e-5 / the length of the edge) is inside
		const Math::Vector3d &v1 = _vertices[1];
		float dist = (point.y() - v1.y()) * _gNormal[1] + (point.z() - v1.z()) * _gNormal[2] + (point.x() - v1.x()) * _gNormal[0];
		if (fabsf(dist) <= 1e-05f)
			dist = 0.f;
		if (fabsf(dist) > _gHeight)
			return false;
		const int a = _axisA, b = _axisB;
		Math::Vector3d prev = _vertices[_numVertices - 1];
		for (int i = 0; i < _numVertices; ++i) {
			const Math::Vector3d &cur = _vertices[i];
			float c = ((point.getValue(a) - prev.getValue(a)) * (cur.getValue(b) - prev.getValue(b)) +
			           (point.getValue(b) - prev.getValue(b)) * -(cur.getValue(a) - prev.getValue(a))) - -1e-05f;
			if (fabsf(c) <= 1e-05f)
				c = 0.f;
			if (c < 0.f)
				return false;
			prev = cur;
		}
		return true;
	}
	// Calculate the distance of the point from the plane of the sector.
	// Return false if it isn't within a margin.
	if (_height < 9000.f) { // No need to check when height is 9999.
		float dist = distanceToPoint(point);

		if (fabsf(dist) > _height + 0.01) // Add an error margin
			return false;
	}

	// On the plane, so check if it is inside the polygon.
	// A point on an edge has to count as inside: the closest point of a sector to a place (where a walk is to end, say) is found on
	// its edge, and in single precision it is a little off the edge, by about a unit in the last place of its coordinates. The old
	// test (the cross product with the normal, in single precision, against -0.000001) rejected such points, more so the longer the edge,
	// and a walk to them was never found ("No path found"). The distance from the edge is computed in double precision here and may be
	// negative by what single precision can have lost: a tenth of a millimetre at the most for coordinates of a few hundred.
	// (Not "< 0" without a margin: a value like -7.45058e-09 would make the actors stop for ever, issue #610, infinite loop in
	// de.forklift_actor.dismount.)
	const double reach = fabs((double)point.x()) + fabs((double)point.y()) + fabs((double)point.z());
	const double margin = 0.00001 + 0.0000003 * reach;
	for (int i = 0; i < _numVertices; i++) {
		const double ex = (double)_vertices[i + 1].x() - _vertices[i].x();
		const double ey = (double)_vertices[i + 1].y() - _vertices[i].y();
		const double ez = (double)_vertices[i + 1].z() - _vertices[i].z();
		const double dx = (double)point.x() - _vertices[i].x();
		const double dy = (double)point.y() - _vertices[i].y();
		const double dz = (double)point.z() - _vertices[i].z();
		const double cross = (ey * dz - ez * dy) * _normal.x() + (ez * dx - ex * dz) * _normal.y() + (ex * dy - ey * dx) * _normal.z();
		const double len2 = ex * ex + ey * ey + ez * ez;
		if (len2 > 0.0 && cross < -margin * sqrt(len2))
			return false;
		if (len2 <= 0.0 && cross < -0.000001)
			return false;
	}
	return true;
}

Common::List<Math::Line3d> Sector::getBridgesTo(Sector *sector) const {
	// This returns a list of "bridges", which are edges that can be travelled
	// through to get to another sector. 0 bridges mean the sectors aren't
	// connected.

	// The algorithm starts by considering all the edges of sector A
	// bridges. It then narrows them down by cutting the bridges against
	// sector B, so we end up with a list of lines which are at the border
	// of sector A and inside sector B.

	Common::List<Math::Line3d> bridges;
	Common::List<Math::Line3d>::iterator it;

	for (int i = 0; i < _numVertices; i++) {
		bridges.push_back(Math::Line3d(_vertices[i], _vertices[i + 1]));
	}

	Math::Vector3d *sectorVertices = sector->getVertices();
	for (int i = 0; i < sector->getNumVertices(); i++) {
		Math::Vector3d pos, edge, delta_b1, delta_b2;
		Math::Line3d line(sectorVertices[i], sectorVertices[i + 1]);
		it = bridges.begin();
		while (it != bridges.end()) {
			Math::Line3d &bridge = (*it);
			edge = line.end() - line.begin();
			delta_b1 = bridge.begin() - line.begin();
			delta_b2 = bridge.end() - line.begin();
			Math::Vector3d cross_b1 = Math::Vector3d::crossProduct(edge, delta_b1);
			Math::Vector3d cross_b2 = Math::Vector3d::crossProduct(edge, delta_b2);
			bool b1_out = cross_b1.dotProduct(_normal) < -1e-7;
			bool b2_out = cross_b2.dotProduct(_normal) < -1e-7;

			bool useXZ = (g_grim->getGameType() == GType_MONKEY4);

			if (b1_out && b2_out) {
				// Both points are outside.
				it = bridges.erase(it);
				continue;
			} else if (b1_out) {
				if (bridge.intersectLine2d(line, &pos, useXZ)) {
					bridge = Math::Line3d(pos, bridge.end());
				}
			} else if (b2_out) {
				if (bridge.intersectLine2d(line, &pos, useXZ)) {
					bridge = Math::Line3d(bridge.begin(), pos);
				}
			}

			++it;
		}
	}

	// All the bridges should be at the same height on both sectors.
	it = bridges.begin();
	while (it != bridges.end()) {
		if (g_grim->getGameType() == GType_MONKEY4) {
			// Set pac contains sectors which are not parallel to any
			// other sector or share any edge. Since one sector isn't
			// a plane, finding the intersections in 3D would be complicated.
			//
			// Checking for bridges using a projection in 2D and having a height
			// threshold to avoid that characters jump from lower to higher floors
			// seems to be a good compromise.
			//
			// The value of at least 0.1 was chosen to fix a path finding issue
			// in set pac when guybrush tried to reach the pile of rocks.
			if (fabs(getProjectionToPlane(it->begin()).y() - sector->getProjectionToPlane(it->begin()).y()) > 0.1f ||
			    fabs(getProjectionToPlane(it->end()).y() - sector->getProjectionToPlane(it->end()).y()) > 0.1f) {
				it = bridges.erase(it);
				continue;
			}
		} else {
			if (fabs(getProjectionToPlane(it->begin()).z() - sector->getProjectionToPlane(it->begin()).z()) > 0.01f ||
			    fabs(getProjectionToPlane(it->end()).z() - sector->getProjectionToPlane(it->end()).z()) > 0.01f) {
				it = bridges.erase(it);
				continue;
			}
		}
		++it;
	}
	return bridges;
}

Math::Vector3d Sector::getProjectionToPlane(const Math::Vector3d &point) const {
	if (_normal.getMagnitude() == 0)
		error("Sector normal is (0,0,0)");

	// Formula: return p - n * (n . (p - v_0))
	Math::Vector3d result = point;
	result -= _normal * _normal.dotProduct(point - _vertices[0]);
	return result;
}

Math::Vector3d Sector::getProjectionToPuckVector(const Math::Vector3d &v) const {
	if (_normal.getMagnitude() == 0)
		error("Sector normal is (0,0,0)");

	Math::Vector3d result = v;
	result -= _normal * _normal.dotProduct(v);
	return result;
}

// Find the closest point on the walkplane to the given point
Math::Vector3d Sector::getClosestPoint(const Math::Vector3d &point) const {
	// First try to project to the plane
	Math::Vector3d p2 = getProjectionToPlane(point);
	if (isPointInSector(p2))
		return p2;

	// Now try to project to some edge
	for (int i = 0; i < _numVertices; i++) {
		Math::Vector3d edge = _vertices[i + 1] - _vertices[i];
		Math::Vector3d delta = point - _vertices[i];
		float scalar = Math::Vector3d::dotProduct(delta, edge) / Math::Vector3d::dotProduct(edge, edge);
		Math::Vector3d cross = Math::Vector3d::crossProduct(delta, edge);
		if (scalar >= 0 && scalar <= 1 && cross.dotProduct(_normal) > 0)
			// That last test is just whether the z-component
			// of delta cross edge is positive; we don't
			// want to return opposite edges.
			return _vertices[i] + scalar * edge;
	}

	// Otherwise, just find the closest vertex
	float minDist = (point - _vertices[0]).getMagnitude();
	int index = 0;
	for (int i = 1; i < _numVertices; i++) {
		float currDist = (point - _vertices[i]).getMagnitude();
		if (currDist < minDist) {
			minDist = currDist;
			index = i;
		}
	}
	return _vertices[index];
}

void Sector::getExitInfo(const Math::Vector3d &s, const Math::Vector3d &dirVec, struct ExitInfo *result) const {
	Math::Vector3d start = getProjectionToPlane(s);
	Math::Vector3d dir = getProjectionToPuckVector(dirVec);

	// First find the edge the ray exits through: this is where
	// the z-component of (v_i - start) x dir changes sign from
	// positive to negative.

	// First find a vertex such that the cross product has
	// positive z-component.
	int i;
	for (i = 0; i < _numVertices; i++) {
		Math::Vector3d delta = _vertices[i] - start;
		Math::Vector3d cross = Math::Vector3d::crossProduct(delta, dir);
		if (cross.dotProduct(_normal) > 0)
			break;
	}

	// Now continue until the cross product has negative
	// z-component.
	while (i < _numVertices) {
		i++;
		Math::Vector3d delta = _vertices[i] - start;
		Math::Vector3d cross = Math::Vector3d::crossProduct(delta, dir);
		if (cross.dotProduct(_normal) <= 0)
			break;
	}

	result->edgeDir = _vertices[i] - _vertices[i - 1];
	result->angleWithEdge = Math::Vector3d::angle(dir, result->edgeDir);
	result->edgeVertex = i - 1;

	Math::Vector3d edgeNormal = Math::Vector3d::crossProduct(result->edgeDir, _normal);
	float d = Math::Vector3d::dotProduct(dir, edgeNormal);
	// This is 0 for the albinizod monster in the at set
	if (!d)
		d = 1.f;
	result->exitPoint = start + (Math::Vector3d::dotProduct(_vertices[i] - start, edgeNormal) / d) * dir;
}

int Sector::getSortplane(int setup) {
	// An unkown sortplane is 0
	if (setup < 0 || setup >= _numSortplanes) {
		return 0;
	}
	return _sortplanes[setup];
}

Sector &Sector::operator=(const Sector &other) {
	_numVertices = other._numVertices;
	_id = other._id;
	_name = other._name;
	_type = other._type;
	_visible = other._visible;
	_vertices = new Math::Vector3d[_numVertices + 1];
	for (int i = 0; i < _numVertices + 1; ++i) {
		_vertices[i] = other._vertices[i];
	}
	if (other._origVertices) {
		_origVertices = new Math::Vector3d[_numVertices + 1];
		for (int i = 0; i < _numVertices + 1; ++i) {
			_origVertices[i] = other._origVertices[i];
		}
	} else {
		_origVertices = nullptr;
	}
	_height = other._height;
	_normal = other._normal;
	_shrinkRadius = other._shrinkRadius;
	_invalid = other._invalid;
	_grimGeo = other._grimGeo;
	_grimFunnelEdge = other._grimFunnelEdge;
	_grimShrunk = other._grimShrunk;
	_origNumVertices = other._origNumVertices;
	_origVisible = other._origVisible;
	for (int i = 0; i < 3; ++i)
		_gNormal[i] = other._gNormal[i];
	_axisA = other._axisA;
	_axisB = other._axisB;
	_gHeight = other._gHeight;

	return *this;
}

bool Sector::operator==(const Sector &other) const {
	bool ok = _numVertices == other._numVertices &&
	          _id == other._id &&
	          _name == other._name &&
	          _type == other._type &&
	          _visible == other._visible;
	for (int i = 0; i < _numVertices + 1; ++i) {
		ok = ok && _vertices[i] == other._vertices[i];
	}
	ok = ok && _height == other._height && _normal == other._normal;

	return ok;
}

} // end of namespace Grim
