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

#ifndef GRIM_EMI_WALKPLANECOLLECTION_H
#define GRIM_EMI_WALKPLANECOLLECTION_H

#include "common/array.h"
#include "common/list.h"

#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/wpvertex.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class EmiEngine;

class WalkPlaneCollection {
public:
	enum {
		kMaxPathPoints = 30
	};

	explicit WalkPlaneCollection(EmiEngine *engine);
	~WalkPlaneCollection();

	void *operator new(size_t size);
	void operator delete(void *block);

	void buildNeighbors();
	bool buildPath(const float *destination, const float *source, bool tighten);
	bool findPlanePath(WalkPlane_v2 *goal, WalkPlane_v2 *start);
	bool tightenPath();
	bool canWalkStraight(const float *from, const float *to);
	int getSize();
	void save(SaveRestoreAccess *accessFunc);
	void restore(SaveRestoreAccess *accessFunc);
	wpVertex *addVertex(const float *position);
	void removeVertex(wpVertex *vertex);
	WalkPlane_v2 *planeAt(const float *point, int typeMask);
	WalkPlane_v2 *planeNamed(const LString &planeName);
	WalkPlane_v2 *planeNumbered(int planeNumber);
	WalkPlane_v2 *planeClosest(const float *point, int typeMask);
	void allActive();
	char *parseCollection(LFile *file);

	int getPath(float (**path)[3]);

	Common::List<wpVertex *> _vertices;
	Common::List<WalkPlane_v2 *> _planes;

private:
	enum {
		kSectionNameSize = 48,
		kLineBufferSize = 256
	};

	void addParsedPlane(const float *planeVertices, int vertexCount, const char *planeName, int planeNumber, int planeType, bool visible, int sortOrderCount, const int *sortOrders, float height);
	void logCollectionError(const char *message);

	EmiEngine *_engine;
	float _rawPoints[kMaxPathPoints][3];
	int _rawCount;
	float _tightPoints[kMaxPathPoints][3];
	int _tightCount;
	Common::Array<WalkPlane_v2 *> _planePath;
	char _sectionName[kSectionNameSize];
};

} // End of namespace Grim

#endif
