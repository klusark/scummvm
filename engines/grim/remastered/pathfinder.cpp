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

#include "engines/grim/remastered/pathfinder.h"
#include "engines/grim/walkplane.h"

namespace Grim {

enum {
	kPlaneTypeWalk = 0x1000
};

static const uint32 kHashSeed = 2166136261u;
static const uint32 kHashPrime = 16777619u;
static const double kGapDistance = 0.005;
static const double kEdgeEpsilon = 0.00001;
static const double kNodeMergeDistance = 0.0005;
static const double kBigValue = 1.0e30;
static const double kBoxMargin = 0.01;

PathFinder::PathFinder(WalkSystem *walkSystem) : _walkSystem(walkSystem), _numPlanes(0), _signature(0), _built(false), _valid(false), _numNodes(0) {
	memset(_planes, 0, sizeof(_planes));
	memset(_planeMin, 0, sizeof(_planeMin));
	memset(_planeMax, 0, sizeof(_planeMax));
	memset(_nodes, 0, sizeof(_nodes));
	memset(_adjacent, 0, sizeof(_adjacent));
	memset(_tempAdjacent, 0, sizeof(_tempAdjacent));
}

bool PathFinder::isWalkable(const WalkPlane *plane) {
	return (plane->_type & kPlaneTypeWalk) != 0 && plane->_active && plane->_numVerts >= 3;
}

uint32 PathFinder::hashBytes(uint32 hash, const void *data, int size) {
	const byte *bytes = static_cast<const byte *>(data);

	for (int i = 0; i < size; i++) {
		hash ^= bytes[i];
		hash *= kHashPrime;
	}

	return hash;
}

// Restricts the range of the parameter t so that value0 + t * (value1 - value0) is in [minValue, maxValue]
bool PathFinder::clipRange(double value0, double value1, double minValue, double maxValue, double *low, double *high) {
	double delta = value1 - value0;

	if (fabs(delta) < 1.0e-12)
		return value0 >= minValue && value0 <= maxValue;

	double first = (minValue - value0) / delta;
	double second = (maxValue - value0) / delta;

	if (first > second) {
		double temp = first;

		first = second;
		second = temp;
	}

	if (first > *low)
		*low = first;

	if (second < *high)
		*high = second;

	return *low <= *high;
}

// The graph is rebuilt when the walkable planes, their state or their vertices change
bool PathFinder::update() {
	WalkPlane *current[kMaxPlanes];
	int count = 0;
	bool overflow = false;
	uint32 hash = kHashSeed;

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if (!isWalkable(plane))
			continue;

		if (count >= kMaxPlanes) {
			overflow = true;
			break;
		}

		current[count++] = plane;
		hash = hashBytes(hash, &plane, sizeof(plane));
		hash = hashBytes(hash, &plane->_numVerts, sizeof(int));

		for (int i = 0; i < plane->_numVerts; i++)
			hash = hashBytes(hash, &plane->_verts[i]->x, 3 * sizeof(float));
	}

	if (_built && hash == _signature && count == _numPlanes)
		return _valid;

	_built = true;
	_signature = hash;
	_numPlanes = count;

	if (overflow) {
		warning("PathFinder::update(): more than %d walk planes", (int)kMaxPlanes);
		_valid = false;
		return false;
	}

	for (int i = 0; i < count; i++)
		_planes[i] = current[i];

	_valid = rebuild();

	return _valid;
}

bool PathFinder::rebuild() {
	_numNodes = 0;

	if (_numPlanes == 0)
		return false;

	for (int i = 0; i < _numPlanes; i++) {
		const WalkPlane *plane = _planes[i];
		double margin = (double)plane->_tolerance + kBoxMargin;

		for (int j = 0; j < 3; j++) {
			_planeMin[i][j] = kBigValue;
			_planeMax[i][j] = -kBigValue;
		}

		for (int v = 0; v < plane->_numVerts; v++) {
			const WpVertex *vertex = plane->_verts[v];
			double pos[3];

			pos[0] = vertex->x;
			pos[1] = vertex->y;
			pos[2] = vertex->z;

			for (int j = 0; j < 3; j++) {
				if (pos[j] < _planeMin[i][j])
					_planeMin[i][j] = pos[j];

				if (pos[j] > _planeMax[i][j])
					_planeMax[i][j] = pos[j];
			}

			int node;

			for (node = 0; node < _numNodes; node++) {
				double squared = 0.0;

				for (int j = 0; j < 3; j++)
					squared += (pos[j] - _nodes[node][j]) * (pos[j] - _nodes[node][j]);

				if (squared < kNodeMergeDistance * kNodeMergeDistance)
					break;
			}

			if (node == _numNodes) {
				if (_numNodes >= kMaxNodes) {
					warning("PathFinder::rebuild(): more than %d vertices, the walk boxes are used instead", (int)kMaxNodes);
					_numNodes = 0;
					return false;
				}

				_nodes[_numNodes][0] = vertex->x;
				_nodes[_numNodes][1] = vertex->y;
				_nodes[_numNodes][2] = vertex->z;
				_numNodes++;
			}
		}

		for (int j = 0; j < 3; j++) {
			_planeMin[i][j] -= margin;
			_planeMax[i][j] += margin;
		}
	}

	for (int a = 0; a < _numNodes; a++) {
		_adjacent[a][a] = 0;

		for (int b = a + 1; b < _numNodes; b++) {
			byte connected = isSegmentWalkable(_nodes[a], _nodes[b]) ? 1 : 0;

			_adjacent[a][b] = connected;
			_adjacent[b][a] = connected;
		}
	}

	return true;
}

// The part of the segment that is on the plane, as a range of the parameter from 0 to 1
bool PathFinder::clipSegmentToPlane(int planeIndex, const float *from, const float *to, double *low, double *high) const {
	const WalkPlane *plane = _planes[planeIndex];
	const WpVertex *origin = plane->_verts[0];
	double originPos[3];
	double height0 = 0.0;
	double height1 = 0.0;

	originPos[0] = origin->x;
	originPos[1] = origin->y;
	originPos[2] = origin->z;

	for (int i = 0; i < 3; i++) {
		height0 += (double)plane->_normal[i] * ((double)from[i] - originPos[i]);
		height1 += (double)plane->_normal[i] * ((double)to[i] - originPos[i]);
	}

	*low = 0.0;
	*high = 1.0;

	double tolerance = plane->_tolerance;

	if (!clipRange(height0, height1, -tolerance, tolerance, low, high))
		return false;

	int axisU = plane->_axisU;
	int axisV = plane->_axisV;
	const WpVertex *prev = plane->_verts[plane->_numVerts - 1];

	for (int i = 0; i < plane->_numVerts; i++) {
		const WpVertex *vertex = plane->_verts[i];
		const float *prevPos = &prev->x;
		const float *vertexPos = &vertex->x;
		double edgeU = (double)vertexPos[axisU] - prevPos[axisU];
		double edgeV = (double)vertexPos[axisV] - prevPos[axisV];
		double side0 = -edgeU * ((double)from[axisV] - prevPos[axisV]) + edgeV * ((double)from[axisU] - prevPos[axisU]);
		double side1 = -edgeU * ((double)to[axisV] - prevPos[axisV]) + edgeV * ((double)to[axisU] - prevPos[axisU]);

		if (!clipRange(side0, side1, -kEdgeEpsilon, kBigValue, low, high))
			return false;

		prev = vertex;
	}

	return true;
}

// True when the planes cover the whole segment, apart from gaps that are too small to matter
bool PathFinder::isSegmentWalkable(const float *from, const float *to) const {
	double low[kMaxPlanes];
	double high[kMaxPlanes];
	double minPos[3];
	double maxPos[3];
	double length = 0.0;
	int num = 0;

	for (int i = 0; i < 3; i++) {
		minPos[i] = from[i] < to[i] ? from[i] : to[i];
		maxPos[i] = from[i] < to[i] ? to[i] : from[i];
		length += ((double)to[i] - from[i]) * ((double)to[i] - from[i]);
	}

	length = sqrt(length);

	if (length < 0.000001)
		length = 0.000001;

	for (int p = 0; p < _numPlanes; p++) {
		bool apart = false;

		for (int i = 0; i < 3; i++) {
			if (maxPos[i] < _planeMin[p][i] || minPos[i] > _planeMax[p][i])
				apart = true;
		}

		if (apart)
			continue;

		double first;
		double last;

		if (!clipSegmentToPlane(p, from, to, &first, &last))
			continue;

		int index = num;

		while (index > 0 && low[index - 1] > first) {
			low[index] = low[index - 1];
			high[index] = high[index - 1];
			index--;
		}

		low[index] = first;
		high[index] = last;
		num++;
	}

	if (num == 0)
		return false;

	double epsilon = kGapDistance / length;
	double covered = 0.0;

	for (int i = 0; i < num; i++) {
		if (low[i] > covered + epsilon)
			return false;

		if (high[i] > covered)
			covered = high[i];
	}

	return covered >= 1.0 - epsilon;
}

// A point outside the planes is moved to the closest point of the closest plane
bool PathFinder::projectPoint(const float *point, float *out) const {
	out[0] = point[0];
	out[1] = point[1];
	out[2] = point[2];

	for (int i = 0; i < _numPlanes; i++) {
		if (_planes[i]->within(out, 0))
			return true;
	}

	double best = kBigValue;
	bool found = false;

	for (int i = 0; i < _numPlanes; i++) {
		float candidate[3];

		candidate[0] = point[0];
		candidate[1] = point[1];
		candidate[2] = point[2];
		_planes[i]->closestPoint(candidate);

		double squared = 0.0;

		for (int j = 0; j < 3; j++)
			squared += ((double)candidate[j] - point[j]) * ((double)candidate[j] - point[j]);

		if (squared < best) {
			best = squared;
			out[0] = candidate[0];
			out[1] = candidate[1];
			out[2] = candidate[2];
			found = true;
		}
	}

	return found;
}

// The nodes after the vertices are the start and the goal: they are only connected to the vertices
bool PathFinder::isConnected(int a, int b) const {
	if (a == b)
		return false;

	if (a < _numNodes && b < _numNodes)
		return _adjacent[a][b] != 0;

	int temp = a >= _numNodes ? a : b;
	int other = a >= _numNodes ? b : a;

	if (other >= _numNodes)
		return false;

	return _tempAdjacent[temp - _numNodes][other] != 0;
}

double PathFinder::distance(int a, int b) const {
	double squared = 0.0;

	for (int i = 0; i < 3; i++)
		squared += ((double)_nodes[a][i] - _nodes[b][i]) * ((double)_nodes[a][i] - _nodes[b][i]);

	return sqrt(squared);
}

// A* over the graph, the distance to the goal is the estimate
int PathFinder::search(int startNode, int goalNode, float (*outPath)[3], int maxPoints) {
	int total = _numNodes + 2;
	double cost[kMaxNodes + 2];
	int parent[kMaxNodes + 2];
	byte state[kMaxNodes + 2];

	for (int i = 0; i < total; i++) {
		cost[i] = kBigValue;
		parent[i] = -1;
		state[i] = 0;
	}

	cost[startNode] = 0.0;
	state[startNode] = 1;

	for (;;) {
		int current = -1;
		double bestEstimate = kBigValue;

		for (int i = 0; i < total; i++) {
			if (state[i] != 1)
				continue;

			double estimate = cost[i] + distance(i, goalNode);

			if (estimate < bestEstimate) {
				bestEstimate = estimate;
				current = i;
			}
		}

		if (current < 0)
			return 0;

		if (current == goalNode)
			break;

		state[current] = 2;

		for (int next = 0; next < total; next++) {
			if (state[next] == 2 || !isConnected(current, next))
				continue;

			double newCost = cost[current] + distance(current, next);

			if (newCost < cost[next]) {
				cost[next] = newCost;
				parent[next] = current;
				state[next] = 1;
			}
		}
	}

	int num = 0;

	for (int node = goalNode; node >= 0; node = parent[node])
		num++;

	if (num > maxPoints)
		return 0;

	int index = num - 1;

	for (int node = goalNode; node >= 0; node = parent[node]) {
		outPath[index][0] = _nodes[node][0];
		outPath[index][1] = _nodes[node][1];
		outPath[index][2] = _nodes[node][2];
		index--;
	}

	return num;
}

int PathFinder::buildPath(const float *start, const float *goal, float (*outPath)[3], int maxPoints) {
	if (maxPoints < 2 || !update())
		return 0;

	float from[3];
	float to[3];

	if (!projectPoint(start, from) || !projectPoint(goal, to))
		return 0;

	if (isSegmentWalkable(from, to)) {
		for (int i = 0; i < 3; i++) {
			outPath[0][i] = from[i];
			outPath[1][i] = to[i];
		}

		return 2;
	}

	int startNode = _numNodes;
	int goalNode = _numNodes + 1;

	for (int i = 0; i < 3; i++) {
		_nodes[startNode][i] = from[i];
		_nodes[goalNode][i] = to[i];
	}

	for (int i = 0; i < _numNodes; i++) {
		_tempAdjacent[0][i] = isSegmentWalkable(from, _nodes[i]) ? 1 : 0;
		_tempAdjacent[1][i] = isSegmentWalkable(to, _nodes[i]) ? 1 : 0;
	}

	int num = search(startNode, goalNode, outPath, maxPoints);

	if (num < 2)
		return 0;

	// Points that the neighbours can reach directly are not needed
	int index = 1;

	while (index < num - 1) {
		if (isSegmentWalkable(outPath[index - 1], outPath[index + 1])) {
			for (int i = index; i < num - 1; i++) {
				outPath[i][0] = outPath[i + 1][0];
				outPath[i][1] = outPath[i + 1][1];
				outPath[i][2] = outPath[i + 1][2];
			}

			num--;
		} else {
			index++;
		}
	}

	return num;
}

} // End of namespace Grim
