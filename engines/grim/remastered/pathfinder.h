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

class WalkPlane;
class WalkSystem;

// Shortest path over the walkable planes of the set. The graph has the vertices of the
// planes as nodes, two nodes are connected when the straight segment between them stays on
// the planes. The graph is rebuilt when the planes change.
class PathFinder {
public:
	enum {
		kMaxPlanes = 256,
		kMaxNodes = 400
	};

	explicit PathFinder(WalkSystem *walkSystem);

	// Writes the points from the start to the goal and returns how many, or 0 if there is no path
	int buildPath(const float *start, const float *goal, float (*outPath)[3], int maxPoints);

private:
	static bool isWalkable(const WalkPlane *plane);
	static uint32 hashBytes(uint32 hash, const void *data, int size);
	static bool clipRange(double value0, double value1, double minValue, double maxValue, double *low, double *high);

	bool update();
	bool rebuild();
	bool clipSegmentToPlane(int planeIndex, const float *from, const float *to, double *low, double *high) const;
	bool isSegmentWalkable(const float *from, const float *to) const;
	bool projectPoint(const float *point, float *out) const;
	bool isConnected(int a, int b) const;
	double distance(int a, int b) const;
	int search(int startNode, int goalNode, float (*outPath)[3], int maxPoints);

	WalkSystem *_walkSystem;

	WalkPlane *_planes[kMaxPlanes];
	double _planeMin[kMaxPlanes][3];
	double _planeMax[kMaxPlanes][3];
	int _numPlanes;
	uint32 _signature;
	bool _built;
	bool _valid;

	float _nodes[kMaxNodes + 2][3];
	int _numNodes;
	byte _adjacent[kMaxNodes][kMaxNodes];
	byte _tempAdjacent[2][kMaxNodes];
};

} // End of namespace Grim

#endif
