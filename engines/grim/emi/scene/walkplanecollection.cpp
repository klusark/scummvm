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
#include "common/util.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/math/singlemath.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

static const int kGroundPlaneMask = 0x1000;
static const int kNameBufferSize = 48;
static const float kMaxFloat = 3.4028235e38f;
static const float kDefaultTolerance = 0.005f;
static const float kUnsetHeight = 12345.0f;
static const double kSameVertexDistance = 1e-8;

WalkPlaneCollection::WalkPlaneCollection(EmiEngine *engine) {
	_engine = engine;
	_rawCount = 0;
	_tightCount = 0;

	memset(_rawPoints, 0, sizeof(_rawPoints));
	memset(_tightPoints, 0, sizeof(_tightPoints));
	memset(_sectionName, 0, sizeof(_sectionName));
}

void WalkPlaneCollection::logCollectionError(const char *message) {
	LConsoleServices *console = _engine->getConsoleServices();

	if (console)
		console->logError("%s", message);
	else
		warning("%s", message);
}

WalkPlaneCollection::~WalkPlaneCollection() {
	Common::List<WalkPlane_v2 *>::iterator planeIt;

	for (planeIt = _planes.begin(); planeIt != _planes.end(); ++planeIt) {
		if (*planeIt) {
			delete *planeIt;
			*planeIt = nullptr;
		}
	}

	_planes.clear();

	Common::List<wpVertex *>::iterator vertexIt;

	for (vertexIt = _vertices.begin(); vertexIt != _vertices.end(); ++vertexIt) {
		_engine->getMemoryServices()->freeImpl(*vertexIt);
		*vertexIt = nullptr;
	}

	_vertices.clear();
}

void *WalkPlaneCollection::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void WalkPlaneCollection::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

int WalkPlaneCollection::getPath(float (**path)[3]) {
	*path = _tightPoints;

	return _tightCount;
}

void WalkPlaneCollection::buildNeighbors() {
	Common::List<WalkPlane_v2 *>::iterator planeIt;
	Common::List<WalkPlane_v2 *>::iterator otherIt;

	allActive();

	for (planeIt = _planes.begin(); planeIt != _planes.end(); ++planeIt)
		(*planeIt)->_neighbors.clear();

	for (planeIt = _planes.begin(); planeIt != _planes.end(); ++planeIt) {
		WalkPlane_v2 *plane = *planeIt;

		if ((plane->_type & kGroundPlaneMask) != 0) {
			for (otherIt = _planes.begin(); otherIt != _planes.end(); ++otherIt) {
				WalkPlane_v2 *other = *otherIt;

				if ((other->_type & kGroundPlaneMask) != 0 && other->_enabled && plane != other && plane->touches(other, true) && plane->addNeighbor(other->_number))
					other->addNeighbor(plane->_number);
			}
		}

		plane->_enabled = false;
	}

	allActive();
}

bool WalkPlaneCollection::buildPath(const float *destination, const float *source, bool tighten) {
	allActive();

	WalkPlane_v2 *destinationPlane = planeAt(destination, kGroundPlaneMask);

	if (!destinationPlane) {
		logCollectionError("WalkPlaneCollection::BuildPath -- Destination not in any walkplane.\n");

		return false;
	}

	WalkPlane_v2 *sourcePlane = planeAt(source, kGroundPlaneMask);

	if (!sourcePlane) {
		logCollectionError("WalkPlaneCollection::BuildPath -- Source not in any walkplane.\n");

		return false;
	}

	if (findPlanePath(destinationPlane, sourcePlane)) {
		_rawCount = 1;
		_rawPoints[0][0] = destination[0];
		_rawPoints[0][1] = destination[1];
		_rawPoints[0][2] = destination[2];

		if (_planePath.size() > 1) {
			WalkPlane_v2 *previousPlane = destinationPlane;
			float from[3];
			float gate[3];

			from[0] = destination[0];
			from[1] = destination[1];
			from[2] = destination[2];

			for (uint i = 1; i < _planePath.size(); i++) {
				WalkPlane_v2 *nextPlane = _planePath[i];

				previousPlane->getGatePoint(gate, nextPlane, from, source);

				from[0] = gate[0];
				from[1] = gate[1];
				from[2] = gate[2];

				if (_rawCount < kMaxPathPoints) {
					_rawPoints[_rawCount][0] = from[0];
					_rawPoints[_rawCount][1] = from[1];
					_rawPoints[_rawCount][2] = from[2];
					_rawCount++;
				} else {
					warning("WalkPlaneCollection::buildPath(): too many path points");
				}

				previousPlane = nextPlane;
			}
		}

		if (_rawCount < kMaxPathPoints) {
			_rawPoints[_rawCount][0] = source[0];
			_rawPoints[_rawCount][1] = source[1];
			_rawPoints[_rawCount][2] = source[2];
			_rawCount++;
		} else {
			warning("WalkPlaneCollection::buildPath(): too many path points");
		}

		if (tighten) {
			if (!tightenPath()) {
				logCollectionError("WalkPlaneCollection::BuildPath -- TightenPath failed. Check for stranded walkplanes.\n");

				return false;
			}
		} else {
			_tightCount = _rawCount;

			for (int i = 0; i < _rawCount; i++) {
				_tightPoints[i][0] = _rawPoints[i][0];
				_tightPoints[i][1] = _rawPoints[i][1];
				_tightPoints[i][2] = _rawPoints[i][2];
			}
		}
	}

	return true;
}

bool WalkPlaneCollection::findPlanePath(WalkPlane_v2 *goal, WalkPlane_v2 *start) {
	Common::Array<WalkPlane_v2 *> queue;
	Common::List<WalkPlane_v2 *>::iterator planeIt;

	for (planeIt = _planes.begin(); planeIt != _planes.end(); ++planeIt) {
		(*planeIt)->_bfsState = 0;
		(*planeIt)->_bfsParent = nullptr;
	}

	queue.push_back(start);

	while (!queue.empty()) {
		WalkPlane_v2 *current = queue[0];

		for (uint i = 0; i < current->_neighbors.size(); i++) {
			WalkPlane_v2 *neighbor = planeNumbered(current->_neighbors[i]);

			if (!neighbor) {
				warning("WalkPlaneCollection::findPlanePath(): missing neighbor plane %d", current->_neighbors[i]);
				continue;
			}

			if (neighbor->_active && neighbor->_bfsState == 0) {
				neighbor->_bfsState = 1;
				neighbor->_bfsParent = current;
				queue.push_back(neighbor);
			}
		}

		queue.remove_at(0);
		current->_bfsState = 2;

		if (current == goal)
			break;
	}

	_planePath.clear();

	for (WalkPlane_v2 *plane = goal; plane; plane = plane->_bfsParent)
		_planePath.push_back(plane);

	// The disasm succeeds even if the goal was never reached
	return true;
}

bool WalkPlaneCollection::tightenPath() {
	if (_rawCount < 2) {
		warning("WalkPlaneCollection::tightenPath(): the path has less than two points");

		return false;
	}

	_tightPoints[0][0] = _rawPoints[0][0];
	_tightPoints[0][1] = _rawPoints[0][1];
	_tightPoints[0][2] = _rawPoints[0][2];
	_tightCount = 1;

	int fromIndex = 0;
	int candidate = 1;
	int lastIndex = _rawCount - 1;

	while (true) {
		if (canWalkStraight(_tightPoints[fromIndex], _rawPoints[candidate])) {
			_tightPoints[_tightCount][0] = _rawPoints[candidate][0];
			_tightPoints[_tightCount][1] = _rawPoints[candidate][1];
			_tightPoints[_tightCount][2] = _rawPoints[candidate][2];

			if (candidate == lastIndex) {
				_tightCount++;

				return true;
			}

			candidate++;
		} else {
			fromIndex = _tightCount;
			_tightCount++;

			if (_tightCount > lastIndex)
				return false;
		}
	}
}

bool WalkPlaneCollection::canWalkStraight(const float *from, const float *to) {
	float start[3] = { 0.0f, 0.0f, 0.0f };
	float exitPoint[3] = { 0.0f, 0.0f, 0.0f };
	WalkPlane_v2 *plane = planeAt(from, kGroundPlaneMask);

	start[0] = from[0];
	start[1] = from[1];
	start[2] = from[2];

	if (!plane) {
		warning("WalkPlaneCollection::canWalkStraight(): the start point is not in any walkplane");
		allActive();

		return false;
	}

	if (plane->within(to, true)) {
		allActive();

		return true;
	}

	while (true) {
		if (_engine->getGamePlatform() == Common::kPlatformPS2)
			plane->findExitPs2(start, to, exitPoint);
		else
			plane->intersection(start, to, exitPoint);

		plane->_enabled = false;
		plane = planeAt(exitPoint, kGroundPlaneMask);

		if (!plane)
			break;

		start[0] = exitPoint[0];
		start[1] = exitPoint[1];
		start[2] = exitPoint[2];

		if (plane->within(to, true)) {
			allActive();

			return true;
		}
	}

	allActive();

	return false;
}

int WalkPlaneCollection::getSize() {
	int size = (int)(sizeof(WalkPlaneCollection) + sizeof(void *) * (_planes.size() + 5 * _vertices.size()));
	Common::List<WalkPlane_v2 *>::iterator planeIt;

	for (planeIt = _planes.begin(); planeIt != _planes.end(); ++planeIt)
		size += (*planeIt)->getSize();

	return size;
}

void WalkPlaneCollection::save(SaveRestoreAccess *accessFunc) {
	int count = (int)_planes.size();
	Common::List<WalkPlane_v2 *>::iterator planeIt;

	accessFunc(&count, sizeof(count));

	for (planeIt = _planes.begin(); planeIt != _planes.end(); ++planeIt) {
		int planeNumber = (*planeIt)->_number;

		accessFunc(&planeNumber, sizeof(planeNumber));
		(*planeIt)->serialize(accessFunc);
	}
}

void WalkPlaneCollection::restore(SaveRestoreAccess *accessFunc) {
	int count = 0;

	accessFunc(&count, sizeof(count));

	for (int i = 0; i < count; i++) {
		int planeNumber = 0;

		accessFunc(&planeNumber, sizeof(planeNumber));

		WalkPlane_v2 *plane = planeNumbered(planeNumber);

		if (!plane) {
			warning("WalkPlaneCollection::restore(): missing plane %d", planeNumber);

			return;
		}

		plane->serialize(accessFunc);
	}
}

wpVertex *WalkPlaneCollection::addVertex(const float *position) {
	Common::List<wpVertex *>::iterator it;

	for (it = _vertices.begin(); it != _vertices.end(); ++it) {
		wpVertex *vertex = *it;
		double deltaX = (double)position[0] - (double)vertex->x;
		double deltaY = (double)position[1] - (double)vertex->y;
		double deltaZ = (double)position[2] - (double)vertex->z;

		if (deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ < kSameVertexDistance) {
			vertex->refCount++;

			return vertex;
		}
	}

	wpVertex *newVertex = (wpVertex *)_engine->getMemoryServices()->mallocImpl(sizeof(wpVertex));

	newVertex->x = 0.0f;
	newVertex->y = 0.0f;
	newVertex->z = 0.0f;
	_vertices.push_back(newVertex);
	newVertex->refCount = 1;
	newVertex->x = position[0];
	newVertex->y = position[1];
	newVertex->z = position[2];

	return newVertex;
}

void WalkPlaneCollection::removeVertex(wpVertex *vertex) {
	if (vertex->refCount > 0) {
		vertex->refCount--;

		if (vertex->refCount == 0) {
			Common::List<wpVertex *>::iterator it = _vertices.begin();

			while (it != _vertices.end()) {
				if (*it == vertex)
					it = _vertices.erase(it);
				else
					++it;
			}

			_engine->getMemoryServices()->freeImpl(vertex);
		}
	}
}

WalkPlane_v2 *WalkPlaneCollection::planeAt(const float *point, int typeMask) {
	Common::List<WalkPlane_v2 *>::iterator it;

	for (it = _planes.begin(); it != _planes.end(); ++it) {
		WalkPlane_v2 *plane = *it;

		if ((typeMask & plane->_type) != 0 && plane->within(point, true))
			return plane;
	}

	return nullptr;
}

WalkPlane_v2 *WalkPlaneCollection::planeNamed(const LString &planeName) {
	Common::List<WalkPlane_v2 *>::iterator it;

	for (it = _planes.begin(); it != _planes.end(); ++it) {
		if ((*it)->_name == planeName)
			return *it;
	}

	return nullptr;
}

WalkPlane_v2 *WalkPlaneCollection::planeNumbered(int planeNumber) {
	Common::List<WalkPlane_v2 *>::iterator it;

	for (it = _planes.begin(); it != _planes.end(); ++it) {
		if ((*it)->_number == planeNumber)
			return *it;
	}

	return nullptr;
}

WalkPlane_v2 *WalkPlaneCollection::planeClosest(const float *point, int typeMask) {
	WalkPlane_v2 *found = planeAt(point, typeMask);

	if (found)
		return found;

	WalkPlane_v2 *closest = nullptr;
	float closestDistance = kMaxFloat;
	Common::List<WalkPlane_v2 *>::iterator it;

	for (it = _planes.begin(); it != _planes.end(); ++it) {
		WalkPlane_v2 *plane = *it;

		if ((typeMask & plane->_type) != 0 && plane->_enabled && plane->_active) {
			float projected[3];

			projected[0] = point[0];
			projected[1] = point[1];
			projected[2] = point[2];
			plane->closestPoint(projected);

			if (_engine->getGamePlatform() == Common::kPlatformPS2) {
				TruncFloat deltaY = TruncFloat(projected[1]) - TruncFloat(point[1]);
				TruncFloat deltaX = TruncFloat(projected[0]) - TruncFloat(point[0]);
				TruncFloat deltaZ = TruncFloat(projected[2]) - TruncFloat(point[2]);
				TruncFloat distance = TruncFloat::multiplyAdd(deltaX * deltaX + deltaY * deltaY, deltaZ, deltaZ);

				if (distance < TruncFloat(closestDistance)) {
					closestDistance = distance.toFloat();
					closest = plane;
				}

				continue;
			}

			double deltaX = (double)projected[0] - (double)point[0];
			double deltaY = (double)projected[1] - (double)point[1];
			double deltaZ = (double)projected[2] - (double)point[2];
			double distance = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

			if (distance < closestDistance) {
				closestDistance = (float)distance;
				closest = plane;
			}
		}
	}

	return closest;
}

void WalkPlaneCollection::allActive() {
	Common::List<WalkPlane_v2 *>::iterator it;

	for (it = _planes.begin(); it != _planes.end(); ++it)
		(*it)->_enabled = true;
}

void WalkPlaneCollection::addParsedPlane(const float *planeVertices, int vertexCount, const char *planeName, int planeNumber, int planeType, bool visible, int sortOrderCount, const int *sortOrders, float height) {
	int count = vertexCount;

	if (count > WalkPlane_v2::kMaxVertices) {
		warning("WalkPlaneCollection::addParsedPlane(): too many vertices in sector %s", planeName);
		count = WalkPlane_v2::kMaxVertices;
	}

	WalkPlane_v2 *plane = new WalkPlane_v2(_engine, planeVertices, count, this);

	_planes.push_back(plane);
	plane->_name = planeName;
	plane->_number = planeNumber;
	plane->_active = visible;
	plane->setType(planeType);
	plane->_numSortOrders = sortOrderCount;

	for (int i = 0; i < sortOrderCount; i++)
		plane->_sortOrder[i % WalkPlane_v2::kMaxSortOrders] = sortOrders[i];

	if (height == 0.0f)
		plane->_tolerance = kDefaultTolerance;
	else
		plane->_tolerance = height;
}

char *WalkPlaneCollection::parseCollection(LFile *file) {
	float planeVertices[WalkPlane_v2::kMaxVertices][3];
	float discardedVertex[3];
	int sortOrders[WalkPlane_v2::kMaxSortOrders];
	char buffer[kLineBufferSize];
	char token[kNameBufferSize];
	char planeName[kNameBufferSize];
	int planeNumber = 0;
	int planeType = 0;
	int visible = 0;
	int declaredVertices = 0;
	int vertexCount = 0;
	int sortOrderCount = 0;
	int value = 0;
	float height = kUnsetHeight;

	memset(planeVertices, 0, sizeof(planeVertices));
	memset(sortOrders, 0, sizeof(sortOrders));
	_sectionName[0] = 0;
	planeName[0] = 0;

	if (!file->readLine(buffer, kLineBufferSize))
		return nullptr;

	while (sscanf(buffer, " section: %47s", _sectionName) != 1) {
		if (!strstr(buffer, " numsectors ")) {
			if (sscanf(buffer, " sector %47s", token) == 1) {
				if (declaredVertices > 2) {
					addParsedPlane(&planeVertices[0][0], declaredVertices, planeName, planeNumber, planeType, visible != 0, sortOrderCount, sortOrders, height);
					sortOrderCount = 0;
				}

				planeName[0] = 0;
				height = kUnsetHeight;
				Common::strlcpy(planeName, token, kNameBufferSize);
				planeNumber = 0;
				planeType = 0;
				visible = 0;
				declaredVertices = 0;
				vertexCount = 0;
			} else if (sscanf(buffer, " id %i", &value) == 1) {
				planeNumber = value;
			} else if (sscanf(buffer, " type %47s", token) == 1) {
				if (strcmp(token, "walk") == 0)
					planeType = 0x1000;
				else if (strcmp(token, "constrain") == 0)
					planeType = 0x1200;
				else if (strcmp(token, "funnel") == 0)
					planeType = 0x1100;
				else if (strcmp(token, "camera") == 0)
					planeType = 0x2000;
				else if (strcmp(token, "special") == 0)
					planeType = 0x4000;
				else if (strcmp(token, "chernobyl") == 0)
					planeType = 0x8000;
			} else if (sscanf(buffer, " default visibility %47s", token) == 1) {
				visible = strcmp(token, "invisible") != 0;
			} else if (sscanf(buffer, " height %f", &height) != 1) {
				if (sscanf(buffer, " numvertices %d", &value) == 1) {
					declaredVertices = value;
				} else if (sscanf(buffer, " numgates %47s", token) != 1) {
					float *target = vertexCount < WalkPlane_v2::kMaxVertices ? planeVertices[vertexCount] : discardedVertex;

					if (sscanf(buffer, " vertices: %f %f %f", &target[0], &target[1], &target[2]) == 3) {
						if (vertexCount < WalkPlane_v2::kMaxVertices)
							vertexCount++;
						else
							warning("WalkPlaneCollection::parseCollection(): too many vertices in sector %s", planeName);
					} else if (vertexCount < declaredVertices && sscanf(buffer, " %f %f %f", &target[0], &target[1], &target[2]) == 3) {
						if (vertexCount < WalkPlane_v2::kMaxVertices)
							vertexCount++;
						else
							warning("WalkPlaneCollection::parseCollection(): too many vertices in sector %s", planeName);
					} else {
						char *keyword = strtok(buffer, " ,\t");

						if (keyword && scumm_stricmp(keyword, "sortplanes") == 0) {
							char *order = strtok(nullptr, " ,\t");

							while (order) {
								if (sortOrderCount < WalkPlane_v2::kMaxSortOrders)
									sortOrders[sortOrderCount++] = atoi(order);
								else
									warning("WalkPlaneCollection::parseCollection(): too many sort planes in sector %s", planeName);

								order = strtok(nullptr, " ,\t");
							}
						}
					}
				}
			}
		}

		if (!file->readLine(buffer, kLineBufferSize)) {
			if (declaredVertices > 2)
				addParsedPlane(&planeVertices[0][0], declaredVertices, planeName, planeNumber, planeType, visible != 0, sortOrderCount, sortOrders, height);

			return nullptr;
		}
	}

	if (declaredVertices > 2)
		addParsedPlane(&planeVertices[0][0], declaredVertices, planeName, planeNumber, planeType, visible != 0, sortOrderCount, sortOrders, height);

	return _sectionName;
}

} // End of namespace Grim
