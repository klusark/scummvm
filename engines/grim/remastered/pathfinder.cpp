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

#include "engines/grim/actor/sputactor.h"
#include "engines/grim/remastered/pathfinder.h"
#include "engines/grim/walkplane.h"

namespace Grim {

enum {
	kPlaneTypeWalk = 0x1000
};

static const float kFarValue = 1.0e10f;
static const float kVertexMergeSquared = 1.0e-7f;
static const float kBoxGrowth = 0.001f;
static const float kSplitLow = 0.001f;
static const float kSplitHigh = 0.999f;
static const float kCombineHigh = 1.001f;
static const float kCrossLimit = 1.0e-5f;
static const float kParallelDistanceSquared = 1.0e-6f;
static const float kEdgeSquared = 1.0e-8f;
static const float kMidpointStart = 0.499f;
static const float kMidpointEnd = 0.501f;
static const float kLinearLow = 0.01f;
static const float kLinearHigh = 0.99f;
static const float kMinWeight = 1.0e-6f;
static const float kCircleRadius = 0.02f;
static const float kArcStep = 0.3490658700466156f;
static const double kTwoPi = 6.2831854820251465;
static const double kArcHeightTolerance = 0.0001;
static const double kArcRadiusDouble = (double)0.02f;

PathFinder::PathFinder(WalkSystem *walkSystem) : _walkSystem(walkSystem), _numPolygons(0), _dirty(1), _numVertices(0),
		_graphCount(0), _direct(0), _numBoxPoints(0) {
	memset(_polygons, 0, sizeof(_polygons));
	memset(_vertices, 0, sizeof(_vertices));
	memset(_graph, 0, sizeof(_graph));
	memset(_adjacency, 0, sizeof(_adjacency));
	memset(_boxPoints, 0, sizeof(_boxPoints));
}

PathFinder::~PathFinder() {
	freePolygons();
}

void PathFinder::freePolygons() {
	for (int i = 0; i < _numPolygons; i++) {
		delete _polygons[i];
		_polygons[i] = nullptr;
	}

	_numPolygons = 0;
}

// The second operand is returned when the first is NaN
float PathFinder::sseMin(float a, float b) {
	return (a < b) ? a : b;
}

float PathFinder::sseMax(float a, float b) {
	return (a > b) ? a : b;
}

PathFinder::Polygon *PathFinder::newPolygon() const {
	Polygon *poly = new Polygon();

	poly->flag = 0;
	poly->numSegments = 0;

	for (int i = 0; i < 3; i++) {
		poly->boxMin[i] = kFarValue;
		poly->boxMax[i] = -kFarValue;
	}

	return poly;
}

int PathFinder::vertIndex(const float *pos) {
	if (_numVertices > 0) {
		for (int i = 0; i < _numVertices; i++) {
			float dx = pos[0] - _vertices[i].pos[0];
			float dy = pos[1] - _vertices[i].pos[1];
			float dz = pos[2] - _vertices[i].pos[2];
			float xx = dx * dx;
			float yy = dy * dy;
			float zz = dz * dz;
			double sum = ((double)xx + yy) + zz;

			if ((double)kVertexMergeSquared > sum)
				return i;
		}

		if (_numVertices > 255)
			return -1;
	}

	int index = _numVertices;

	_vertices[index].pos[0] = pos[0];
	_vertices[index].pos[1] = pos[1];
	_vertices[index].pos[2] = pos[2];
	_numVertices++;

	return index;
}

void PathFinder::computeBoundingBox(Polygon *poly) const {
	for (int i = 0; i < 3; i++) {
		poly->boxMin[i] = kFarValue;
		poly->boxMax[i] = -kFarValue;
	}

	for (int i = 0; i < poly->numSegments; i++) {
		const float *pos = _vertices[poly->segments[i].v0].pos;

		for (int j = 0; j < 3; j++) {
			poly->boxMin[j] = sseMin(poly->boxMin[j], pos[j] + -kBoxGrowth);
			poly->boxMax[j] = sseMax(poly->boxMax[j], pos[j] + kBoxGrowth);
		}
	}
}

PathFinder::Polygon *PathFinder::fromWalkPlane(const WalkPlane *plane) {
	Polygon *poly = newPolygon();
	int numVerts = plane->_numVerts;

	for (int i = 0; i < numVerts; i++) {
		const float *a = &plane->_verts[i]->x;
		const float *b = &plane->_verts[(i + 1 == numVerts) ? 0 : i + 1]->x;
		int first = vertIndex(a);
		int second = vertIndex(b);

		if ((first | second) < 0) {
			delete poly;
			return nullptr;
		}

		if (first != second) {
			poly->segments[poly->numSegments].v0 = first;
			poly->segments[poly->numSegments].v1 = second;
			poly->segments[poly->numSegments].flag = 1;
			poly->numSegments++;

			if ((unsigned)(poly->numSegments - 1) > 0xFE) {
				delete poly;
				return nullptr;
			}
		}
	}

	poly->flag = plane->_alwaysMerge != 0 ? 1 : 0;
	computeBoundingBox(poly);

	return poly;
}

int PathFinder::addSegment(Polygon *poly, const Segment &segment) const {
	if (poly->numSegments >= kMaxSegments) {
		warning("PathFinder::addSegment(): the polygon has no room for another segment");
		return -1;
	}

	poly->segments[poly->numSegments] = segment;
	poly->numSegments++;

	return poly->numSegments <= 255 ? poly->numSegments - 1 : -1;
}

bool PathFinder::pointInPoly2D(const Polygon *poly, const float *point) const {
	int winding = 0;

	for (int i = 0; i < poly->numSegments; i++) {
		const float *p0 = _vertices[poly->segments[i].v0].pos;
		const float *p1 = _vertices[poly->segments[i].v1].pos;

		float deltaX = p1[0] - p0[0];
		float deltaY = p1[1] - p0[1];
		float pointX = point[0] - p0[0];
		float pointY = point[1] - p0[1];
		float cross = (float)((double)pointY * deltaX - (double)pointX * deltaY);

		if (point[1] >= p0[1]) {
			if (p1[1] > point[1] && cross > 0.0f)
				winding++;
		} else if (point[1] >= p1[1] && cross < 0.0f) {
			winding--;
		}
	}

	return winding != 0;
}

bool PathFinder::onEdge(const Polygon *poly, const float *point) const {
	for (int i = 0; i < poly->numSegments; i++) {
		const float *p0 = _vertices[poly->segments[i].v0].pos;
		const float *p1 = _vertices[poly->segments[i].v1].pos;
		float wx = point[0] - p0[0];
		float wy = point[1] - p0[1];
		float dx = p1[0] - p0[0];
		float dy = p1[1] - p0[1];
		double numerator = (double)(float)((double)dy * wy) + (float)((double)dx * wx);
		double denominator = (double)(float)((double)dx * dx) + (float)((double)dy * dy);
		float t = (float)(numerator / denominator);

		// A NaN becomes 1
		if (!(t < 1.0f))
			t = 1.0f;
		else if (!(0.0f < t))
			t = 0.0f;

		float rx = (float)((double)wx - (float)((double)dx * t));
		float ry = (float)((double)wy - (float)((double)dy * t));
		float squareX = (float)((double)rx * rx);
		float squareY = (float)((double)ry * ry);
		float distance = (float)((double)squareX + squareY);

		if (distance < kEdgeSquared)
			return true;
	}

	return false;
}

bool PathFinder::segmentInPoly2D(const Polygon *poly, const Segment &segment) const {
	if (poly->numSegments <= 0)
		return false;

	const float *p0 = _vertices[segment.v0].pos;
	const float *p1 = _vertices[segment.v1].pos;
	float middle[3];

	for (int i = 0; i < 2; i++) {
		float endPart = p1[i] * kMidpointEnd;
		float startPart = p0[i] * kMidpointStart;

		middle[i] = endPart + startPart;
	}

	middle[2] = 0.0f;

	if (!pointInPoly2D(poly, middle))
		return false;

	return !onEdge(poly, middle);
}

float PathFinder::getSectorHeight(const Polygon *poly, const float *point) const {
	float bestDistance = kFarValue;
	float height = kFarValue;

	for (int i = 0; i < poly->numSegments; i++) {
		const float *pos = _vertices[poly->segments[i].v0].pos;
		double dx = (double)point[0] - pos[0];
		double dy = (double)point[1] - pos[1];
		double dz = (double)point[2] - pos[2];
		float xx = (float)(dx * dx);
		float yy = (float)(dy * dy);
		float zz = (float)(dz * dz);
		float distance = (float)sqrt(((double)xx + yy) + zz);

		if (bestDistance > distance) {
			height = pos[2];
			bestDistance = distance;
		}
	}

	return height;
}

bool PathFinder::sharesVertexWithOther(Polygon **polys, int count, int index) const {
	bool starts[kMaxVertices];
	const Polygon *poly = polys[index];

	memset(starts, 0, sizeof(starts));

	for (int i = 0; i < count; i++) {
		const Polygon *other = polys[i];

		if (other->numSegments > 0 && other != poly) {
			for (int j = 0; j < other->numSegments; j++)
				starts[other->segments[j].v0] = true;
		}
	}

	for (int i = 0; i < poly->numSegments; i++) {
		if (starts[poly->segments[i].v0])
			return true;
	}

	return false;
}

int PathFinder::projectIntoPolygons(float *point, Polygon **polys, int count, int startIndex) const {
	if (count <= 0) {
		point[0] = kFarValue;
		point[1] = kFarValue;
		point[2] = kFarValue;

		return -1;
	}

	int best = -1;
	float bestHeight = kFarValue;

	for (int i = 0; i < count; i++) {
		const Polygon *poly = polys[i];

		if (!(startIndex < 0 || i == startIndex) && !sharesVertexWithOther(polys, count, i))
			continue;

		if (poly->numSegments <= 0)
			continue;

		if (!pointInPoly2D(poly, point) && !onEdge(poly, point))
			continue;

		float height = getSectorHeight(poly, point);
		double heightDifference = (double)height - point[2];
		float difference = (float)(heightDifference < 0.0 ? -heightDifference : heightDifference);

		if (bestHeight > difference) {
			best = i;
			bestHeight = difference;
		}
	}

	if (best >= 0)
		return best;

	int result = -1;
	float bestDistance = kFarValue;
	float snapped[3] = { kFarValue, kFarValue, kFarValue };

	for (int i = 0; i < count; i++) {
		const Polygon *poly = polys[i];

		if (!(startIndex < 0 || i == startIndex) && !sharesVertexWithOther(polys, count, i))
			continue;

		for (int j = 0; j < poly->numSegments; j++) {
			const float *a = _vertices[poly->segments[j].v0].pos;
			const float *b = _vertices[poly->segments[j].v1].pos;
			float dx = b[0] - a[0];
			float dy = b[1] - a[1];
			float dz = b[2] - a[2];
			float wx = point[0] - a[0];
			float wy = point[1] - a[1];
			float wz = point[2] - a[2];
			float productY = (float)((double)dy * wy);
			float productX = (float)((double)dx * wx);
			float productZ = (float)((double)wz * dz);
			double numerator = ((double)productY + productX) + productZ;
			float squareX = (float)((double)dx * dx);
			float squareY = (float)((double)dy * dy);
			float squareZ = (float)((double)dz * dz);
			double denominator = ((double)squareX + squareY) + squareZ;
			float t = (float)(numerator / denominator);

			if (t < 0.0f)
				t = 0.0f;
			else if (1.0f < t)
				t = 1.0f;

			float stepX = t * dx;
			float stepY = t * dy;
			float stepZ = t * dz;
			float ux = a[0] + stepX;
			float uy = a[1] + stepY;
			float uz = stepZ + a[2];
			double ex = (double)ux - point[0];
			double ey = (double)uy - point[1];
			double ez = (double)uz - point[2];
			float exSquared = (float)(ex * ex);
			float eySquared = (float)(ey * ey);
			float ezSquared = (float)(ez * ez);
			float distance = (float)sqrt(((double)exSquared + eySquared) + ezSquared);

			if (bestDistance > distance) {
				result = i;
				snapped[0] = ux;
				snapped[1] = uy;
				snapped[2] = uz;
				bestDistance = distance;
			}
		}
	}

	point[0] = snapped[0];
	point[1] = snapped[1];
	point[2] = snapped[2];

	return result;
}

int PathFinder::rebuild() {
	for (int i = 0; i < _numPolygons; i++) {
		if (_polygons[i]) {
			delete _polygons[i];
			_polygons[i] = nullptr;
		}
	}

	_numPolygons = 0;
	_numVertices = 0;

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if ((plane->_type & kPlaneTypeWalk) && plane->_active) {
			Polygon *poly = fromWalkPlane(plane);

			_polygons[_numPolygons++] = poly;

			if (!poly || _numPolygons > 255)
				return 0;
		}
	}

	if (_numPolygons > 1) {
		for (int i = 0; i < _numPolygons / 2; i++) {
			Polygon *temp = _polygons[i];

			_polygons[i] = _polygons[_numPolygons - 1 - i];
			_polygons[_numPolygons - 1 - i] = temp;
		}
	}

	bool changed;

	do {
		if (_numPolygons <= 0)
			break;

		changed = false;

		for (int i = 0; i < _numPolygons; i++) {
			for (int j = i + 1; j < _numPolygons; j++) {
				Polygon *merged = combinePolygons(_polygons[i], _polygons[j], kModeUnion);

				if (merged) {
					delete _polygons[i];
					delete _polygons[j];
					_polygons[i] = merged;
					_numPolygons--;
					_polygons[j] = _polygons[_numPolygons];
					_polygons[_numPolygons] = nullptr;
					j--;
					changed = true;
				}
			}
		}
	} while (changed);

	if (_numBoxPoints > 3) {
		for (int box = 0; box < _numBoxPoints / 4; box++) {
			if (_numPolygons >= kMaxPolygons) {
				warning("PathFinder::rebuild(): too many polygons for the boxes");
				return 0;
			}

			Polygon *boxPoly = newPolygon();

			_polygons[_numPolygons] = boxPoly;

			for (int corner = 1; corner <= 4; corner++) {
				int first = vertIndex(_boxPoints[4 * box + corner - 1]);
				int second = vertIndex(_boxPoints[4 * box + corner % 4]);
				Segment segment;

				segment.v0 = second;
				segment.v1 = first;
				segment.flag = 0;

				if ((first | second) < 0 || addSegment(boxPoly, segment) < 0) {
					delete boxPoly;
					_polygons[_numPolygons] = nullptr;
					return 0;
				}
			}

			computeBoundingBox(boxPoly);
			_numPolygons++;

			int last = _numPolygons;

			for (int i = 0; i < last - 1; i++) {
				Polygon *cut = combinePolygons(_polygons[i], _polygons[last - 1], kModeDifference);

				if (cut) {
					delete _polygons[i];
					_polygons[i] = cut;
				}
			}

			delete _polygons[last - 1];
			_polygons[last - 1] = nullptr;
			_numPolygons--;

			for (int i = 0; i < _numPolygons; i++)
				alignSegmentDirection(_polygons[i]);
		}
	}

	if (!separatePolygons(_polygons, &_numPolygons))
		return 0;

	if (compactVertices(_polygons, _numPolygons) > 200)
		return 0;

	int numNodes = getCCWverts(_graph, _polygons, _numPolygons, 2);

	_graphCount = numNodes;

	if ((unsigned)numNodes > 127)
		return 0;

	visibilityGraphNaive(2, numNodes);
	_dirty = 0;

	return 1;
}

int PathFinder::splitSegment(Polygon *poly, int index, float t, unsigned *fail) {
	if (!(t > kSplitLow && t < kSplitHigh))
		return 0;

	const float *p0 = _vertices[poly->segments[index].v0].pos;
	const float *p1 = _vertices[poly->segments[index].v1].pos;
	float point[3];

	for (int i = 0; i < 3; i++) {
		float delta = p1[i] - p0[i];

		point[i] = delta * t + p0[i];
	}

	int vertex = vertIndex(point);

	if (vertex < 0) {
		*fail = 1;
		return 1;
	}

	if (poly->numSegments >= kMaxSegments) {
		warning("PathFinder::splitSegment(): the polygon has no room for another segment");
		*fail = 1;
		return 1;
	}

	int oldEnd = poly->segments[index].v1;
	int flag = poly->segments[index].flag;

	poly->segments[index].v1 = vertex;
	poly->segments[poly->numSegments].v0 = vertex;
	poly->segments[poly->numSegments].v1 = oldEnd;
	poly->segments[poly->numSegments].flag = flag;
	poly->numSegments++;

	if ((unsigned)(poly->numSegments - 1) >= 0xFF) {
		*fail = 1;
		return 1;
	}

	return 1;
}

bool PathFinder::linearSplit(Polygon *poly, int index, const float *first, const float *second, unsigned *fail) {
	const float *p0 = _vertices[poly->segments[index].v0].pos;
	const float *p1 = _vertices[poly->segments[index].v1].pos;
	float ex = p1[0] - p0[0];
	float ey = p1[1] - p0[1];
	float ez = p1[2] - p0[2];
	float exSquared = (float)((double)ex * ex);
	float eySquared = (float)((double)ey * ey);
	float ezSquared = (float)((double)ez * ez);
	float length = (float)(((double)exSquared + eySquared) + ezSquared);
	float firstX = first[0] - p0[0];
	float firstY = first[1] - p0[1];
	float firstZ = first[2] - p0[2];
	float secondX = second[0] - p0[0];
	float secondY = second[1] - p0[1];
	float secondZ = second[2] - p0[2];
	float firstProductY = (float)((double)firstY * ey);
	float firstProductX = (float)((double)firstX * ex);
	float firstProductZ = (float)((double)firstZ * ez);
	float tFirst = (float)((((double)firstProductY + firstProductX) + firstProductZ) / length);
	float secondProductY = (float)((double)secondY * ey);
	float secondProductX = (float)((double)secondX * ex);
	float secondProductZ = (float)((double)secondZ * ez);
	float tSecond = (float)((((double)secondProductY + secondProductX) + secondProductZ) / length);
	float tMin = sseMin(tSecond, tFirst);
	int current = index;

	if (tMin > kLinearLow && tMin < kLinearHigh) {
		float point[3];

		point[0] = ex * tMin + p0[0];
		point[1] = ey * tMin + p0[1];
		point[2] = tMin * ez + p0[2];

		int vertex = vertIndex(point);

		if (vertex < 0) {
			*fail = 1;
			return false;
		}

		if (poly->numSegments >= kMaxSegments) {
			warning("PathFinder::linearSplit(): the polygon has no room for another segment");
			*fail = 1;
			return false;
		}

		int oldEnd = poly->segments[index].v1;

		poly->segments[index].v1 = vertex;
		poly->segments[poly->numSegments].v0 = vertex;
		poly->segments[poly->numSegments].v1 = oldEnd;
		poly->segments[poly->numSegments].flag = poly->segments[index].flag;
		poly->numSegments++;
		current = poly->numSegments - 1;

		if ((unsigned)current >= 0xFF) {
			*fail = 1;
			return false;
		}
	}

	float tMax = sseMax(tFirst, tSecond);

	if (tMax > kLinearLow && tMax < kLinearHigh) {
		float point[3];

		point[0] = p0[0] + ex * tMax;
		point[1] = p0[1] + ey * tMax;
		point[2] = p0[2] + ez * tMax;

		int vertex = vertIndex(point);

		if (vertex < 0) {
			*fail = 1;
			return false;
		}

		if (poly->numSegments >= kMaxSegments) {
			warning("PathFinder::linearSplit(): the polygon has no room for another segment");
			*fail = 1;
			return false;
		}

		int oldEnd = poly->segments[current].v1;

		poly->segments[current].v1 = vertex;
		poly->segments[poly->numSegments].v0 = vertex;
		poly->segments[poly->numSegments].v1 = oldEnd;
		poly->segments[poly->numSegments].flag = poly->segments[current].flag;
		poly->numSegments++;
		current = poly->numSegments - 1;

		if ((unsigned)current >= 0xFF) {
			*fail = 1;
			return false;
		}
	}

	return current != index;
}

bool PathFinder::crossSegments(const float *a0, const float *a1, const float *b0, const float *b1, float *tA, float *tB) const {
	float ex = a1[0] - a0[0];
	float ey = a1[1] - a0[1];
	float dx = b1[0] - b0[0];
	float dy = b1[1] - b0[1];
	float wx = a0[0] - b0[0];
	float wy = a0[1] - b0[1];
	float den = (float)((double)dy * ex - (double)dx * ey);
	float absDen = den < 0.0f ? -den : den;

	if (!(kCrossLimit > absDen)) {
		float inverse = (float)(1.0 / (double)den);

		*tA = (float)(((double)dx * wy - (double)dy * wx) * inverse);
		*tB = (float)(((double)ex * wy - (double)ey * wx) * inverse);

		return -kSplitLow < *tB && -kSplitLow < *tA && *tA < kCombineHigh && *tB < kCombineHigh;
	}

	float cross = (float)((double)wy * ex - (double)wx * ey);
	float eySquared = (float)((double)ey * ey);
	float exSquared = (float)((double)ex * ex);
	double distanceSquared = ((double)cross * cross) / ((double)eySquared + exSquared);

	if (distanceSquared > (double)kParallelDistanceSquared)
		return false;

	float dxSquared = (float)((double)dx * dx);
	float dySquared = (float)((double)dy * dy);
	float length = (float)((double)dxSquared + dySquared);
	float startPart = (float)(((double)(float)((double)wy * dy) + (float)((double)wx * dx)) / length);
	float vx = a1[0] - b0[0];
	float vy = a1[1] - b0[1];
	float endPart = (float)(((double)(float)((double)dy * vy) + (float)((double)dx * vx)) / length);

	*tA = 0.0f;
	*tB = startPart;

	if (-kSplitLow > startPart && -kSplitLow > endPart)
		return false;

	if (startPart > kCombineHigh && endPart > kCombineHigh)
		return false;

	return true;
}

PathFinder::Polygon *PathFinder::combinePolygons(Polygon *a, Polygon *b, unsigned mode) {
	if (!(b->boxMax[0] > a->boxMin[0]))
		return nullptr;

	if (!(a->boxMax[0] > b->boxMin[0]))
		return nullptr;

	if (!(b->boxMax[1] > a->boxMin[1]))
		return nullptr;

	if (!(a->boxMax[1] > b->boxMin[1]))
		return nullptr;

	if (a->flag == 0 && b->flag == 0 && mode != 0) {
		if (!(b->boxMax[2] > a->boxMin[2]))
			return nullptr;

		if (!(a->boxMax[2] > b->boxMin[2]))
			return nullptr;
	}

	if (a->numSegments > 0) {
		double tolerance = (mode != 0) ? (double)0.0001f : (double)0.01f;

		for (int i = 0; i < a->numSegments; i++) {
			int j = 0;

			while (j < b->numSegments) {
				const float *a0 = _vertices[a->segments[i].v0].pos;
				const float *a1 = _vertices[a->segments[i].v1].pos;
				const float *b0 = _vertices[b->segments[j].v0].pos;
				const float *b1 = _vertices[b->segments[j].v1].pos;
				float tA = 0.0f;
				float tB = 0.0f;
				bool hit = crossSegments(a0, a1, b0, b1, &tA, &tB);

				if (!hit) {
					j++;
					continue;
				}

				double heightDeltaA = (double)a1[2] - a0[2];
				double heightDeltaB = (double)b1[2] - b0[2];
				float heightA = (float)((double)a0[2] + (double)tA * heightDeltaA);
				float heightB = (float)((double)b0[2] + (double)tB * heightDeltaB);
				float heightDifference = fabsf(heightA - heightB);

				if ((double)heightDifference > tolerance && a->flag == 0 && b->flag == 0 && mode != 0)
					return nullptr;

				float e3x = a1[0] - a0[0];
				float e3y = a1[1] - a0[1];
				float e3z = (float)heightDeltaA;
				float d3x = b1[0] - b0[0];
				float d3y = b1[1] - b0[1];
				float d3z = (float)heightDeltaB;
				float productY = (float)((double)e3y * d3y);
				float productX = (float)((double)e3x * d3x);
				float productZ = (float)((double)e3z * d3z);
				double dot = fabs(((double)productY + productX) + productZ);
				float squareEX = (float)((double)e3x * e3x);
				float squareEY = (float)((double)e3y * e3y);
				float squareEZ = (float)((double)e3z * e3z);
				float lengthE = (float)sqrt(((double)squareEX + squareEY) + squareEZ);
				double lengthEScaled = (double)lengthE * kSplitHigh;
				float squareDX = (float)((double)d3x * d3x);
				float squareDY = (float)((double)d3y * d3y);
				float squareDZ = (float)((double)d3z * d3z);
				float lengthD = (float)sqrt(((double)squareDX + squareDY) + squareDZ);
				unsigned fail = 0;
				int firstResult;
				int secondResult;

				if (!((double)lengthD * lengthEScaled < dot)) {
					firstResult = splitSegment(a, i, tA, &fail);
					secondResult = splitSegment(b, j, tB, &fail);
				} else {
					float pointB0[3] = { b0[0], b0[1], b0[2] };
					float pointB1[3] = { b1[0], b1[1], b1[2] };
					float pointA0[3] = { a0[0], a0[1], a0[2] };
					float pointA1[3] = { a1[0], a1[1], a1[2] };

					firstResult = linearSplit(a, i, pointB0, pointB1, &fail) ? 1 : 0;
					secondResult = linearSplit(b, j, pointA0, pointA1, &fail) ? 1 : 0;
				}

				if (fail)
					return nullptr;

				if ((firstResult | secondResult) == 0)
					j++;
			}
		}
	}

	Polygon *result = newPolygon();

	if (a->flag && b->flag)
		result->flag = 1;

	bool changed = false;
	int numA = a->numSegments;
	int numB = b->numSegments;

	for (int s = 0; s < numA; s++) {
		Segment segment = a->segments[s];
		bool inside = segmentInPoly2D(b, segment);
		bool shared = false;

		for (int t = 0; t < numB; t++) {
			if (b->segments[t].v0 == segment.v1 && b->segments[t].v1 == segment.v0) {
				shared = true;
				break;
			}
		}

		bool drop = (mode != 0) ? (inside || shared) : inside;

		if (drop) {
			changed = true;
			continue;
		}

		result->segments[result->numSegments] = segment;
		result->numSegments++;

		if (result->numSegments > 255) {
			delete result;
			return nullptr;
		}
	}

	for (int s = 0; s < numB; s++) {
		Segment segment = b->segments[s];

		if (mode != 0) {
			bool inside = segmentInPoly2D(a, segment);
			bool shared = false;

			for (int t = 0; t < numA; t++) {
				if ((a->segments[t].v0 == segment.v1 && a->segments[t].v1 == segment.v0) ||
						(a->segments[t].v0 == segment.v0 && a->segments[t].v1 == segment.v1)) {
					shared = true;
					break;
				}
			}

			if (inside || shared) {
				changed = true;
				continue;
			}

			result->segments[result->numSegments] = segment;
			result->numSegments++;

			if (result->numSegments > 255) {
				delete result;
				return nullptr;
			}
		} else if (segmentInPoly2D(a, segment)) {
			result->segments[result->numSegments] = segment;
			result->numSegments++;
			changed = true;

			if (result->numSegments > 255) {
				delete result;
				return nullptr;
			}
		}
	}

	if (!changed) {
		delete result;
		return nullptr;
	}

	computeBoundingBox(result);

	return result;
}

void PathFinder::alignSegmentDirection(Polygon *poly) const {
	int numSegments = poly->numSegments;
	int changed;

	do {
		changed = 0;

		if (numSegments <= 0)
			return;

		for (int k = 0; k < numSegments; k++) {
			if (poly->segments[k].flag != 0)
				continue;

			int start = poly->segments[k].v0;

			for (int o = 0; o < numSegments; o++) {
				if (k == o || poly->segments[o].flag == 0)
					continue;

				if (poly->segments[o].v0 == start) {
					poly->segments[k].v0 = poly->segments[k].v1;
					poly->segments[k].v1 = start;
					poly->segments[k].flag = 1;
					numSegments = poly->numSegments;
					changed = 1;
					break;
				}

				if (poly->segments[o].v1 == start) {
					poly->segments[k].flag = 1;
					numSegments = poly->numSegments;
					changed = 1;
					break;
				}
			}
		}
	} while (changed);
}

int PathFinder::separatePolygons(Polygon **polys, int *count) {
	int originalCount = *count;

	if (originalCount <= 0)
		return 1;

	for (int p = 0; p < originalCount; p++) {
		Polygon *poly = polys[p];
		int numSegments = poly->numSegments;

		if (numSegments > 0) {
			float sum[kMaxSegments];
			int label[kMaxSegments];

			memset(sum, 0, numSegments * sizeof(float));

			for (int k = 0; k < numSegments; k++) {
				label[k] = k;
				poly->segments[k].flag = 1;
			}

			bool relabeled;

			do {
				relabeled = false;

				for (int k = 0; k < numSegments; k++) {
					for (int i = 0; i < numSegments; i++) {
						if (poly->segments[k].v0 == poly->segments[i].v1) {
							int labelK = label[k];
							int labelI = label[i];

							if (labelK != labelI) {
								int smaller = labelI;

								if (labelK <= labelI)
									smaller = labelK;

								label[i] = smaller;
								label[k] = smaller;
								relabeled = true;
							}
						}
					}
				}
			} while (relabeled);

			for (int j = 0; j < numSegments; j++) {
				const float *start = _vertices[poly->segments[j].v0].pos;
				const float *end = _vertices[poly->segments[j].v1].pos;
				double span = (double)end[0] - start[0];
				double product = (double)start[1] * end[1];

				sum[label[j]] = (float)(span * product + sum[label[j]]);
			}

			bool anyNegative = false;

			for (int l = 0; l < numSegments; l++) {
				if (0.0f > sum[l])
					anyNegative = true;
			}

			if (!anyNegative) {
				if (numSegments > 1) {
					for (int c = 1; c < numSegments; c++) {
						if (sum[c] != 0.0f) {
							Polygon *created = nullptr;

							for (int k = 0; k < numSegments; k++) {
								if (label[k] != c)
									continue;

								poly->segments[k].flag = 0;

								if (!created) {
									if (*count >= kMaxPolygons) {
										warning("PathFinder::separatePolygons(): too many polygons");
										return 0;
									}

									created = newPolygon();
									polys[*count] = created;
									(*count)++;
								}

								created->segments[created->numSegments].v0 = poly->segments[k].v0;
								created->segments[created->numSegments].v1 = poly->segments[k].v1;
								created->segments[created->numSegments].flag = poly->segments[k].flag;
								created->numSegments++;

								if ((unsigned)(created->numSegments - 1) > 0xFE)
									return 0;
							}
						}
					}
				}

				for (int k = 0; k < poly->numSegments; k++) {
					bool finished = false;

					while (poly->segments[k].flag == 0) {
						poly->numSegments--;
						poly->segments[k] = poly->segments[poly->numSegments];

						if (k >= poly->numSegments) {
							finished = true;
							break;
						}
					}

					if (finished)
						break;
				}
			}
		}
	}

	for (int q = 0; q < *count; q++)
		computeBoundingBox(polys[q]);

	return 1;
}

int PathFinder::compactVertices(Polygon **polys, int count) {
	int vertices = _numVertices;

	for (int k = 0; k < vertices; k++)
		_vertices[k].next = -1;

	for (int p = 0; p < count; p++) {
		for (int j = 0; j < polys[p]->numSegments; j++)
			_vertices[polys[p]->segments[j].v0].next = polys[p]->segments[j].v0;
	}

	for (int k = 0; k < vertices; k++) {
		if (_vertices[k].next < 0) {
			do {
				if (k >= vertices)
					break;

				vertices--;
				_numVertices = vertices;
				_vertices[k] = _vertices[vertices];
				_vertices[vertices].next = k;
			} while (_vertices[k].next < 0);
		}
	}

	for (int p = 0; p < count; p++) {
		for (int j = 0; j < polys[p]->numSegments; j++) {
			Segment *segment = &polys[p]->segments[j];

			if (segment->v0 >= _numVertices)
				segment->v0 = _vertices[segment->v0].next;

			if (segment->v1 >= _numVertices)
				segment->v1 = _vertices[segment->v1].next;
		}
	}

	return _numVertices;
}

int PathFinder::getCCWverts(Vertex *graph, Polygon **polys, int count, int first) {
	int numVerts = _numVertices;

	for (int k = 0; k < numVerts; k++) {
		_vertices[k].next = -1;
		_vertices[k].prev = -1;
		_vertices[k].numPolygons = 0;
	}

	for (int i = 0; i < count; i++) {
		for (int j = 0; j < polys[i]->numSegments; j++) {
			int a = polys[i]->segments[j].v0;
			int b = polys[i]->segments[j].v1;

			for (int side = 0; side < 2; side++) {
				Vertex *vertex = &_vertices[side == 0 ? a : b];
				int members = vertex->numPolygons;
				bool add = false;

				if (members <= 0) {
					add = true;
				} else {
					int m = 0;

					while (vertex->polygons[m] != i) {
						m++;

						if (m >= members) {
							if (members <= 5)
								add = true;

							break;
						}
					}
				}

				if (add) {
					vertex->numPolygons = members + 1;
					vertex->polygons[members] = i;
				}
			}

			_vertices[a].next = (_vertices[a].next < 0) ? b : -2;
			_vertices[b].prev = (_vertices[b].prev < 0) ? a : -2;
		}
	}

	int out = first;

	for (int k = 0; k < numVerts; k++) {
		const Vertex &vertex = _vertices[k];

		if (vertex.next == -1 || vertex.prev == -1)
			return -1;

		bool keep = true;

		if (vertex.next != -2 && vertex.prev != -2) {
			const Vertex &next = _vertices[vertex.next];
			const Vertex &prev = _vertices[vertex.prev];
			float previousY = prev.pos[1] - next.pos[1];
			float previousX = prev.pos[0] - next.pos[0];
			float vertexX = vertex.pos[0] - next.pos[0];
			float vertexY = vertex.pos[1] - next.pos[1];
			float cross = (float)((double)previousY * vertexX - (double)previousX * vertexY);

			keep = cross > 0.0f;
		}

		if (keep) {
			if (out < kMaxGraphNodes) {
				graph[out] = vertex;
			} else if (out == kMaxGraphNodes) {
				warning("PathFinder::getCCWverts(): too many corners for the graph");
			}

			out++;
		}
	}

	return out;
}

void PathFinder::visibilityGraphNaive(int from, int to) {
	for (int i = from; i < to; i++) {
		_adjacency[i][i] = 0.0f;

		for (int j = i + 1; j < _graphCount; j++) {
			const Vertex *vi = &_graph[i];
			const Vertex *vj = &_graph[j];
			float weight = 0.0f;

			if (vi->numPolygons > 0) {
				bool visible = false;
				bool blocked = false;

				for (int a = 0; a < vi->numPolygons && !blocked; a++) {
					int polyId = vi->polygons[a];
					bool shared = false;

					for (int b = 0; b < vj->numPolygons; b++) {
						if (vj->polygons[b] == polyId) {
							shared = true;
							break;
						}
					}

					if (!shared)
						continue;

					const Polygon *poly = _polygons[polyId];

					if (poly->numSegments > 0) {
						float ex = vj->pos[0] - vi->pos[0];
						float ey = vj->pos[1] - vi->pos[1];

						for (int k = 0; k < poly->numSegments; k++) {
							const float *s0 = _vertices[poly->segments[k].v0].pos;
							const float *s1 = _vertices[poly->segments[k].v1].pos;
							float wx = vi->pos[0] - s0[0];
							float wy = vi->pos[1] - s0[1];
							float dx = s1[0] - s0[0];
							float dy = s1[1] - s0[1];
							float den = (float)((double)dy * ex - (double)dx * ey);
							float absDen = den;

							if (0.0f > den)
								absDen = -den;

							if (!(kCrossLimit > absDen)) {
								float inverse = (float)(1.0 / (double)den);
								float tE = (float)(((double)dx * wy - (double)dy * wx) * inverse);
								float tS = (float)(((double)wy * ex - (double)wx * ey) * inverse);

								if (tS > kSplitLow) {
									if (tE > kSplitLow && kSplitHigh > tE && kSplitHigh > tS) {
										blocked = true;
										break;
									}
								}
							}
						}

						if (blocked)
							break;
					}

					float middle[3];
					float endX = vj->pos[0] * kMidpointEnd;
					float startX = vi->pos[0] * kMidpointStart;
					float endY = vj->pos[1] * kMidpointEnd;
					float startY = vi->pos[1] * kMidpointStart;

					middle[0] = endX + startX;
					middle[1] = endY + startY;
					middle[2] = 0.0f;

					if (pointInPoly2D(poly, middle) || onEdge(poly, middle))
						visible = true;
				}

				if (!blocked && visible) {
					double dz = (double)vi->pos[2] - vj->pos[2];
					double dy = (double)vi->pos[1] - vj->pos[1];
					double dx = (double)vi->pos[0] - vj->pos[0];
					float xx = (float)(dx * dx);
					float yy = (float)(dy * dy);
					float zz = (float)(dz * dz);
					float distance = (float)sqrt(((double)yy + xx) + zz);

					weight = (kMinWeight > distance) ? kMinWeight : distance;
				}
			}

			_adjacency[i][j] = weight;
			_adjacency[j][i] = weight;
		}
	}
}

int PathFinder::findPath(int from, int to, float (*outPath)[3], int maxPoints) const {
	float targetX = _graph[to].pos[0];
	float targetY = _graph[to].pos[1];
	float targetZ = _graph[to].pos[2];
	int state[kMaxGraphNodes];
	float cost[kMaxGraphNodes];
	float estimate[kMaxGraphNodes];
	int parent[kMaxGraphNodes];
	int open[kMaxGraphNodes];

	memset(state, 0, sizeof(state));
	memset(parent, 0, sizeof(parent));
	memset(open, 0, sizeof(open));

	for (int i = 0; i < kMaxGraphNodes; i++) {
		cost[i] = kFarValue;
		estimate[i] = kFarValue;
	}

	cost[from] = 0.0f;

	{
		double dz = (double)_graph[from].pos[2] - targetZ;
		double dy = (double)_graph[from].pos[1] - targetY;
		double dx = (double)_graph[from].pos[0] - targetX;
		float xx = (float)(dx * dx);
		float yy = (float)(dy * dy);
		float zz = (float)(dz * dz);

		estimate[from] = (float)sqrt(((double)xx + yy) + zz);
	}

	open[0] = from;
	state[from] = 1;

	int openCount = 1;
	int current = from;

	for (;;) {
		int index = 0;

		if (openCount >= 2) {
			float best = estimate[current];

			for (int i = 1; i < openCount; i++) {
				float value = estimate[open[i]];

				if (best > value) {
					index = i;
					current = open[i];
				}

				best = sseMin(value, best);
			}
		}

		if (current == to)
			break;

		open[index] = open[openCount - 1];
		openCount--;
		state[current] = 2;

		for (int j = 0; j < _graphCount; j++) {
			float weight = _adjacency[j][current];

			if (weight != 0.0f && state[j] != 2) {
				float newCost = weight + cost[current];

				if (state[j] != 1 || cost[j] > newCost) {
					int oldState = state[j];
					double dz = (double)_graph[j].pos[2] - targetZ;
					double dy = (double)_graph[j].pos[1] - targetY;
					double dx = (double)_graph[j].pos[0] - targetX;
					float xx = (float)(dx * dx);
					float yy = (float)(dy * dy);
					float zz = (float)(dz * dz);

					parent[j] = current;
					cost[j] = newCost;
					estimate[j] = (float)sqrt(((double)xx + yy) + zz) + newCost;

					if (oldState != 1) {
						state[j] = 1;

						if (openCount < kMaxGraphNodes) {
							open[openCount] = j;
							openCount++;
						}
					}
				}
			}
		}

		if (openCount <= 0)
			return 0;

		current = open[0];
	}

	if (maxPoints <= 0)
		return 0;

	int count = 1;
	int node = to;

	for (;;) {
		outPath[count - 1][0] = _graph[node].pos[0];
		outPath[count - 1][1] = _graph[node].pos[1];
		outPath[count - 1][2] = _graph[node].pos[2];

		if (node == from)
			return count;

		node = parent[node];

		if (count >= maxPoints)
			return 0;

		count++;
	}
}

void PathFinder::smoothenPath(float (*path)[3], int *numPoints, int maxPoints) const {
	float copy[kMaxPathPoints][3];
	float out[kMaxCircleOut][3];
	int count = *numPoints;

	for (int i = 0; i < count; i++) {
		copy[i][0] = path[i][0];
		copy[i][1] = path[i][1];
		copy[i][2] = path[i][2];
	}

	if (count - 1 >= 2) {
		for (int i = 1; i < count - 1; i++) {
			int steps = addCircleSegments(path, i, out, path[i + 1], path[i - 1]);

			if (steps > 0) {
				copy[i][0] = out[steps + 1][0];
				copy[i][1] = out[steps + 1][1];
				copy[i][2] = out[steps + 1][2];
			}
		}
	}

	float prev[3] = { path[0][0], path[0][1], path[0][2] };

	if (*numPoints - 1 >= 2) {
		int index = 1;
		int original = 1;

		do {
			int steps = addCircleSegments(path, index, out, copy[original + 1], prev);

			prev[0] = path[index][0];
			prev[1] = path[index][1];
			prev[2] = path[index][2];

			if (steps != 0) {
				int total = *numPoints;

				if (total + steps > maxPoints) {
					warning("PathFinder::smoothenPath(): no room for the arc of the corner %d", index);
				} else {
					for (int s = total - 1; s >= index; s--) {
						path[s + steps][0] = path[s][0];
						path[s + steps][1] = path[s][1];
						path[s + steps][2] = path[s][2];
					}

					for (int s = 0; s <= steps; s++) {
						path[index + s][0] = out[1 + s][0];
						path[index + s][1] = out[1 + s][1];
						path[index + s][2] = out[1 + s][2];
					}

					*numPoints = total + steps;
					index += steps;
				}
			}

			index++;
			original++;
		} while (index < *numPoints - 1);
	}
}

float PathFinder::getCircleAngle(const float *corner, const float *other, bool positive) const {
	float deltaX = (float)((double)other[0] - corner[0]);
	float deltaY = (float)((double)other[1] - corner[1]);
	float angle = (float)atan2((double)deltaY, (double)deltaX);
	float squareX = (float)((double)deltaX * deltaX);
	float squareY = (float)((double)deltaY * deltaY);
	float distance = (float)sqrt((double)squareX + squareY);
	float quotient = (float)((double)distance / kCircleRadius);
	float arc = (float)atan((double)quotient);

	if (positive)
		return (float)((double)arc + angle);

	return (float)((double)angle - arc);
}

int PathFinder::addCircleSegments(float (*path)[3], int index, float (*out)[3], const float *nextPoint, const float *prevPoint) const {
	const float *corner = path[index];
	const float *following = path[index + 1];
	const float *before = path[index - 1];

	float prevDeltaX = (float)((double)corner[0] - prevPoint[0]);
	float prevDeltaY = (float)((double)corner[1] - prevPoint[1]);
	float followDeltaX = (float)((double)following[0] - prevPoint[0]);
	float followDeltaY = (float)((double)following[1] - prevPoint[1]);
	float turn = (float)((double)followDeltaY * prevDeltaX - (double)followDeltaX * prevDeltaY);
	bool positive = turn > 0.0f;

	double followX = (double)corner[0] - following[0];
	double followY = (double)corner[1] - following[1];
	float followSquareX = (float)(followX * followX);
	float followSquareY = (float)(followY * followY);
	float followDistance = (float)sqrt((double)followSquareX + followSquareY);

	if (followDistance <= kCircleRadius)
		return 0;

	double prevX = (double)corner[0] - prevPoint[0];
	double prevY = (double)corner[1] - prevPoint[1];
	float prevSquareX = (float)(prevX * prevX);
	float prevSquareY = (float)(prevY * prevY);
	float prevDistance = (float)sqrt((double)prevSquareX + prevSquareY);

	if (prevDistance <= kCircleRadius)
		return 0;

	float start = getCircleAngle(corner, before, positive);
	float end = getCircleAngle(corner, nextPoint, !positive);
	float step = positive ? kArcStep : -kArcStep;

	if (end < start && 0.0f < step) {
		do
			end = (float)((double)end + kTwoPi);
		while (end < start);
	}

	if (start < end && step < 0.0f) {
		do
			end = (float)((double)end - kTwoPi);
		while (end > start);
	}

	float absStep = step < 0.0f ? -step : step;
	double delta = (double)end - start;
	double stepsValue = fabs(delta) / absStep + 1.0;

	if (!(stepsValue >= 0.0 && stepsValue < (double)(kMaxCircleOut - 3))) {
		warning("PathFinder::addCircleSegments(): the arc has no valid number of steps");
		return 0;
	}

	int steps = (int)stepsValue;

	out[0][0] = before[0];
	out[0][1] = before[1];
	out[0][2] = before[2];

	for (int i = 0; i <= steps; i++) {
		float ratio = (float)((double)i / (float)steps);
		float angle = (float)((double)start + delta * ratio);
		float cosine = (float)cos((double)angle);
		float cx = (float)((double)cosine * kArcRadiusDouble);
		float sine = (float)sin((double)angle);
		float cy = (float)((double)sine * kArcRadiusDouble);

		out[i + 1][0] = (float)((double)cx + corner[0]);
		out[i + 1][1] = (float)((double)cy + corner[1]);
		out[i + 1][2] = (float)((double)corner[2] + 0.0);
	}

	out[steps + 2][0] = following[0];
	out[steps + 2][1] = following[1];
	out[steps + 2][2] = following[2];

	for (int s = 0; s < steps + 2; s++) {
		const float *e0 = out[s];
		const float *e1 = out[s + 1];

		for (int p = 0; p < _numPolygons; p++) {
			const Polygon *poly = _polygons[p];

			for (int g = 0; g < poly->numSegments; g++) {
				const float *s0 = _vertices[poly->segments[g].v0].pos;
				const float *s1 = _vertices[poly->segments[g].v1].pos;
				float ex = (float)((double)e1[0] - e0[0]);
				float ey = (float)((double)e1[1] - e0[1]);
				float dx = (float)((double)s1[0] - s0[0]);
				float dy = (float)((double)s1[1] - s0[1]);
				float wx = (float)((double)s0[0] - e0[0]);
				float wy = (float)((double)s0[1] - e0[1]);
				float den = (float)((double)ey * dx - (double)ex * dy);
				float absDen = den;

				if (0.0f > den)
					absDen = -den;

				if (kCrossLimit > absDen)
					continue;

				float inverse = (float)(1.0 / (double)den);
				float u = (float)(((double)dx * wy - (double)dy * wx) * inverse);

				if (!(u > kSplitLow))
					continue;

				float v = (float)(((double)ex * wy - (double)wx * ey) * inverse);

				if (!(v > kSplitLow && kSplitHigh > v && kSplitHigh > u))
					continue;

				const Vertex *low = &_vertices[poly->segments[g].v1];
				const Vertex *high = &_vertices[poly->segments[g].v1];

				if (s1[2] > s0[2])
					low = &_vertices[poly->segments[g].v0];

				if (s0[2] > s1[2])
					high = &_vertices[poly->segments[g].v0];

				float zMax = (float)((double)high->pos[2] + kArcHeightTolerance);

				if (!(e0[2] > zMax) || !(e1[2] > zMax)) {
					float zMin = (float)((double)low->pos[2] + -kArcHeightTolerance);

					if (!(zMin > e0[2]) || !(zMax > e1[2]))
						return 0;
				}
			}
		}
	}

	return steps;
}

int PathFinder::buildPath(SputActorModule *actorModule, SputActor *actor, float *goal, float *start, float (*outPath)[3], int maxPoints, SputActor *avoid) {
	float points[kMaxBoxPoints][3];
	int numPoints = 0;

	actorModule->getCollisionBoxes(actor, &numPoints, kMaxBoxPoints, points, avoid);

	if (numPoints != _numBoxPoints) {
		_dirty = 1;
		_numBoxPoints = numPoints;
	}

	for (int i = 0; i < numPoints; i++) {
		double dz = (double)points[i][2] - _boxPoints[i][2];
		double dy = (double)points[i][1] - _boxPoints[i][1];
		double dx = (double)points[i][0] - _boxPoints[i][0];
		float yy = (float)(dy * dy);
		float xx = (float)(dx * dx);
		float zz = (float)(dz * dz);
		float distance = (float)sqrt(((double)yy + xx) + zz);

		if ((double)distance > 0.001)
			_dirty = 1;

		_boxPoints[i][0] = points[i][0];
		_boxPoints[i][1] = points[i][1];
		_boxPoints[i][2] = points[i][2];
	}

	if (_dirty && !rebuild())
		return -1;

	int startPolygon = projectIntoPolygons(start, _polygons, _numPolygons, -1);

	projectIntoPolygons(goal, _polygons, _numPolygons, startPolygon);

	if (_direct == 1) {
		outPath[0][0] = start[0];
		outPath[0][1] = start[1];
		outPath[0][2] = start[2];
		outPath[1][0] = goal[0];
		outPath[1][1] = goal[1];
		outPath[1][2] = goal[2];

		return 2;
	}

	for (int i = 0; i < 3; i++) {
		_graph[0].pos[i] = goal[i];
		_graph[1].pos[i] = start[i];
	}

	_graph[0].numPolygons = 1;

	if (_numPolygons <= 0) {
		_graph[1].numPolygons = 1;
	} else {
		float best = kFarValue;

		for (int i = 0; i < _numPolygons; i++) {
			if (pointInPoly2D(_polygons[i], _graph[0].pos) || onEdge(_polygons[i], _graph[0].pos)) {
				double heightDifference = (double)getSectorHeight(_polygons[i], _graph[0].pos) - _graph[0].pos[2];
				float difference = (float)(heightDifference < 0.0 ? -heightDifference : heightDifference);

				if (best > difference) {
					_graph[0].polygons[0] = i;
					best = difference;
				}
			}
		}

		_graph[1].numPolygons = 1;
		best = kFarValue;

		for (int i = 0; i < _numPolygons; i++) {
			if (pointInPoly2D(_polygons[i], _graph[1].pos) || onEdge(_polygons[i], _graph[1].pos)) {
				double heightDifference = (double)getSectorHeight(_polygons[i], _graph[1].pos) - _graph[1].pos[2];
				float difference = (float)(heightDifference < 0.0 ? -heightDifference : heightDifference);

				if (best > difference) {
					_graph[1].polygons[0] = i;
					best = difference;
				}
			}
		}
	}

	visibilityGraphNaive(0, 2);

	int result = findPath(0, 1, outPath, maxPoints);

	if (result >= 3) {
		smoothenPath(outPath, &result, maxPoints);
		return result;
	}

	return result;
}

} // End of namespace Grim
