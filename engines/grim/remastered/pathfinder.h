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

#ifndef GRIM_PATHFINDER_H
#define GRIM_PATHFINDER_H

#include "common/scummsys.h"

namespace Grim {

class SputActorModule;
class WalkPlane;
class WalkSystem;
struct SputActor;

// Paths over the walkable planes of the set. The planes are merged into polygons and the boxes of
// the actors in the way are cut out of them. The reflex corners of the outline are the nodes of a
// visibility graph, the shortest route is found with A* and its corners are rounded.
class PathFinder {
public:
	enum {
		kMaxBoxPoints = 256,
		kMaxPathPoints = 128
	};

	explicit PathFinder(WalkSystem *walkSystem);
	~PathFinder();

	void markDirty() { _dirty = 1; }
	void setDirect(bool direct) { _direct = direct ? 1 : 0; }

	// Writes the points from the start to the goal and returns how many, -1 when the polygons can
	// not be made. The goal and the start are moved into the polygons.
	int buildPath(SputActorModule *actorModule, SputActor *actor, float *goal, float *start, float (*outPath)[3], int maxPoints, SputActor *avoid);

private:
	enum {
		kMaxPolygons = 256,
		kMaxSegments = 256,
		kMaxVertices = 256,
		kMaxGraphNodes = 128,
		kMaxVertexPolygons = 6,
		kMaxCircleOut = 128,
		kModeDifference = 0,
		kModeUnion = 1
	};

	struct Segment {
		int v0;
		int v1;
		int flag;
	};

	struct Polygon {
		Segment segments[kMaxSegments];
		float boxMin[3];
		float boxMax[3];
		int flag;
		int numSegments;
	};

	struct Vertex {
		float pos[3];
		int next;
		int prev;
		int numPolygons;
		int polygons[kMaxVertexPolygons];
	};

	static float sseMin(float a, float b);
	static float sseMax(float a, float b);
	static float sqrtFloat(float value);

	Polygon *newPolygon() const;
	int vertIndex(const float *pos);
	void computeBoundingBox(Polygon *poly) const;
	Polygon *fromWalkPlane(const WalkPlane *plane);
	int addSegment(Polygon *poly, const Segment &segment) const;
	int splitSegment(Polygon *poly, int index, float t, unsigned *fail);
	bool linearSplit(Polygon *poly, int index, const float *first, const float *second, unsigned *fail);
	Polygon *combinePolygons(Polygon *a, Polygon *b, unsigned mode);
	void alignSegmentDirection(Polygon *poly) const;
	int separatePolygons(Polygon **polys, int *count);
	int compactVertices(Polygon **polys, int count);
	int getCCWverts(Vertex *graph, Polygon **polys, int count, int first);
	bool pointInPoly2D(const Polygon *poly, const float *point) const;
	bool onEdge(const Polygon *poly, const float *point) const;
	bool segmentInPoly2D(const Polygon *poly, const Segment &segment) const;
	float getSectorHeight(const Polygon *poly, const float *point) const;
	bool sharesVertexWithOther(Polygon **polys, int count, int index) const;
	int projectIntoPolygons(float *point, Polygon **polys, int count, int startIndex) const;
	int rebuild();
	void visibilityGraphNaive(int from, int to);
	int findPath(int from, int to, float (*outPath)[3], int maxPoints) const;
	void smoothenPath(float (*path)[3], int *numPoints, int maxPoints) const;
	int addCircleSegments(float (*path)[3], int index, float (*out)[3], const float *nextPoint, const float *prevPoint) const;
	float getCircleAngle(const float *corner, const float *other, bool positive) const;

	WalkSystem *_walkSystem;

	Polygon *_polygons[kMaxPolygons];
	int _numPolygons;
	int _dirty;
	Vertex _vertices[kMaxVertices];
	int _numVertices;
	Vertex _graph[kMaxGraphNodes];
	float _adjacency[kMaxGraphNodes][kMaxGraphNodes];
	int _graphCount;
	int _direct;
	float _boxPoints[kMaxBoxPoints][3];
	int _numBoxPoints;
};

} // End of namespace Grim

#endif
