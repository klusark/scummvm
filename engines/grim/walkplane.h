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

#ifndef GRIM_WALKSYSTEM_H
#define GRIM_WALKSYSTEM_H

#include "common/scummsys.h"

namespace Grim {

class PathFinder;
class Puck;
class RenderDroid;
class StdConffile;
class StdMath;
class WalkPlane;
class WalkSystem;

struct WpVertex {
	int refCount;
	float x;
	float y;
	float z;
};

struct WpCollection {
	WalkPlane *head;
	char name[36];
};

struct WpCachePlane {
	WalkPlane *plane;
	int numVerts;
	int vertIdx[10];
	int active;
};

struct WpCacheVert {
	int index;
	WpVertex v;
};

class WalkPlane {
public:
	enum {
		kMaxTriangles = 10
	};

	WalkPlane(WalkSystem *system, float (*verts)[3], int numPoints, const char *collection);
	~WalkPlane();

	void setName(const char *name);
	void setType(int newType);
	int within(float *point, uint flags);
	int closestPoint(float *point);
	double intersection(float *a, float *b, float *out);
	double intersection(float *a, float *b, float *out, float *edge);
	void snapTo(float *point);
	void project(float *point);
	void draw(uint16 color);
	void jumpOut(Puck *puck);
	float *getConstraintVector(Puck *puck);
	int clipPoly(const float *poly, int numPoly, float *out);
	double distToEdge(int edge, float *point);
	int removeShortEdge();

	char _name[32];
	int _number;
	int _type;
	int _active;
	int _enabled;
	float _tolerance;
	int _numVerts;
	WpVertex *_verts[10];
	float _normal[3];
	WalkPlane *_prev;
	WalkPlane *_next;
	int _hitEdge;
	int _quadAxis;
	int _axisU;
	int _axisV;
	int _alwaysMerge;
	// Triangles of the Remastered sector files: three indices of the vertices of the plane each
	int _numTriangles;
	int _triangles[kMaxTriangles][3];

private:
	int closestPointRemastered(float *point);
	int withinRemastered(float *point, uint flags);
	double intersectionRemastered(float *a, float *b, float *out, float *edge);

	WalkSystem *_system;
};

class WalkSystem {
public:
	WalkSystem(RenderDroid *rd, StdMath *stdMath, StdConffile *stdConffile, bool isDemo, bool isRemastered = false);
	~WalkSystem();

	void buildCache();
	void destroyCache();
	void restoreCache();
	void killCollection(const char *collection);
	void setCollection(const char *name);
	WalkPlane *planeAt(float *point, int flags);
	WalkPlane *planeNamed(const char *name);
	WalkPlane *planeNumbered(int index);
	WalkPlane *planeClosest(float *point, int flags);
	void allActive();
	WpVertex *addVertex(WalkPlane *plane, float *pos);
	void removeVertex(WpVertex *vert);
	const char *parseCollection(const char *collection);
	int closestVertWithin(WalkPlane *a, WalkPlane *b, float *target, float *out);
	WalkPlane *planeContaining(float *a, float *b);
	int getPath(float **path);
	int buildPath(float *from, float *to, int tighten);
	int findPath(float *from, float *to, WalkPlane *exclude);
	int findExitPoints(WalkPlane *plane, float *from, float *out);
	void tightenPath();
	void shrinkBoxes(float amount);
	PathFinder *getPathFinder();
	void markPathFinderDirty();

	// Walk box path of the remastered: the working path holds 128 points
	int getPathRemastered(float **path);
	float (*getWorkingPath())[3] { return _workingPath; }
	int getNumWorkingPathPoints() const { return _numWorkingPathPoints; }
	void setNumWorkingPathPoints(int count) { _numWorkingPathPoints = count; }
	int findPathRemastered(float *from, float *to, WalkPlane *plane, unsigned int mode);
	int findExitPointsRemastered(WalkPlane *plane, float *to, float *out);
	int closestVertWithinRemastered(WalkPlane *a, WalkPlane *b, float *target, float *out);
	void tightenPathRemastered();

	WalkPlane *getHead() const { return _wpHead; }
	Puck *getPuckHead() const { return _puckHead; }
	const char *getCurCollection() const { return _curCollection; }

private:
	friend class WalkPlane;
	friend class Puck;

	WalkPlane *newPlane(float (*verts)[3], int numPoints, const char *collection, const char *sectorName, int id, int visible, int type, int alwaysMerge,
			int numTriangles, int (*triangles)[3]);
	WpVertex *addVertexDemo(float *pos);

	RenderDroid *_rd;
	StdMath *_stdMath;
	StdConffile *_stdConffile;

	WpVertex _vertDb[2048];
	int _totVerts;
	int _numDemoVerts;
	WalkPlane *_wpHead;
	char *_curCollection;
	WpCollection _collection[140];
	Puck *_puckHead;
	int _forwardCounter;
	bool _demo;
	bool _remastered;

	float _path[30][3];
	int _numPathPoints;
	float _workingPath[128][3];
	int _numWorkingPathPoints;
	int _totIterations;
	int _pathFindFail;
	float _constraintVector[3];
	char _sectionName[48];

	WpCachePlane *_cache;
	WpCacheVert *_vertCache;
	int _numPlanes;
	int _numCacheVerts;
	int _cacheBuilt;

	PathFinder *_pathFinder;
};

} // End of namespace Grim

#endif
