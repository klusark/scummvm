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

#ifndef GRIM_WALKPLANE_H
#define GRIM_WALKPLANE_H

#include "common/str.h"
#include "common/array.h"
#include "common/list.h"

#include "math/vector3d.h"
#include "math/line3d.h"

namespace Common {
	class SeekableReadStream;
}

namespace Grim {

class SaveGame;
class TextSplitter;

class Sector {
public:
	enum SectorType {
		NoneType = 0,
		WalkType = 0x1000,
		FunnelType = 0x1100,
		CameraType = 0x2000,
		SpecialType = 0x4000,
		HotType = 0x8000
	};

	Sector();
	Sector(const Sector &other);
	virtual ~Sector();

	void saveState(SaveGame *savedState) const;
	bool restoreState(SaveGame *savedState);

	void load(TextSplitter &ts);
	void loadBinary(Common::SeekableReadStream *data);
	void setVisible(bool visible);
	void shrink(float radius);
	void unshrink();

	// The shrinking of the boxes of the original (FUN_004a9d00), which Set::shrinkBoxes() does for all the sectors together
	void grimSaveForShrink(float radius);
	/** The offsets (three for each vertex) that move the vertices inward by the radius, false when one of them does not fit in the sector */
	bool grimShrinkOffsets(float radius, float *offsets) const;
	/** Joins the two ends of the shortest edge in the point where the lines of the edges next to it meet (FUN_004a9180) */
	bool grimCollapseShortestEdge(Common::Array<Math::Vector3d> &pool);
	void grimSwitchOff() { _visible = false; }
	bool isActive() const { return _visible && !_invalid; }
	void grimSetVertex(int i, const Math::Vector3d &v);
	/** The mark the search of the planner of the original puts on a sector it has been through (+0x2c): a sector that is marked contains nothing for FUN_004a6980 with checkActive */
	void grimSetFree(bool free) { _grimFree = free; }
	bool grimFree() const { return _grimFree; }
	/** Sector_ContainsPoint (FUN_004a6980); with checkActive a sector that is switched off or marked contains nothing */
	bool grimContains(const float *p, bool checkActive) const;
	/** Sector_FindExitPoint (FUN_004a6d10): where the line from the start to the target leaves the sector (the target when it is inside), and the angle
	 * of the line with the edge it leaves by, 90.2 degrees off it (0 when it leaves through no edge or is inside) */
	float grimFindExitPoint(const float *from, const float *target, float *out) const;
	/** The edge (the index of its second vertex) where the last grimFindExitPoint left the sector, -1 when it did not (+0x74 of the sector) */
	int grimExitEdge() const { return _grimExitEdge; }
	/** The edge of a funnel sector that the walk goes along (+0x78 of the sector, set with the type: FUN_004a6790) */
	int grimFunnelEdge() const { return _grimFunnelEdge; }
	/** The nearest place of the sector for a point (FUN_004a6b30): the height made right, and a point outside an edge moved onto it */
	void grimClampPoint(float *p) const;

	Common::String getName() const { return _name; }
	int getSectorId() const { return _id; }
	SectorType getType() const { return _type; } // FIXME: Implement type de-masking
	bool isVisible() const { return _visible && !_invalid; }
	/** The sector has been shrunk (a held item makes the walk boxes smaller) */
	bool isShrunk() const { return _shrinkRadius != 0.f || _origVertices != nullptr; }
	/** With exact the test of the original (FUN_004a6980) as the scripts see it; the walk of the actors keeps the stricter test of ScummVM */
	bool isPointInSector(const Math::Vector3d &point, bool exact = false) const;
	/** The original joins the vertices of the sectors of a set that are closer than 1e-4 (FUN_004a7ab0): the first one that was loaded is used */
	void weldVertices(Common::Array<Math::Vector3d> &pool);
	bool containsXZ(const Math::Vector3d &point, float *planeY, float margin = 0.f) const;
	float distanceToPoint(const Math::Vector3d &point) const;
	Common::List<Math::Line3d> getBridgesTo(Sector *sector) const;

	Math::Vector3d getProjectionToPlane(const Math::Vector3d &point) const;
	Math::Vector3d getProjectionToPuckVector(const Math::Vector3d &v) const;

	Math::Vector3d getClosestPoint(const Math::Vector3d &point) const;

	// Interface to trace a ray to its exit from the polygon
	struct ExitInfo {
		Math::Vector3d exitPoint;
		Math::Angle angleWithEdge;
		Math::Vector3d edgeDir;
		int edgeVertex;
	};
	void getExitInfo(const Math::Vector3d &start, const Math::Vector3d &dir, struct ExitInfo *result) const;

	int getNumSortplanes() { return _numSortplanes; }
	int getSortplane(int setup);
	int getNumVertices() const { return _numVertices; }
	Math::Vector3d *getVertices() const { return _vertices; }
	Math::Vector3d getNormal() const { return _normal; }
	/** The plane of the sector as the original has it: the normal, the first vertex and the two axes of the plane (FUN_004a7000) */
	void grimPlane(float *normal, float *origin, int &axisA, int &axisB) const;

	Sector &operator=(const Sector &other);
	bool operator==(const Sector &other) const;

private:
	int _numVertices;
	int _id;
	int _numSortplanes;
	int *_sortplanes;

	Common::String _name;
	SectorType _type;
	bool _visible;
	bool _invalid;
	Math::Vector3d *_vertices;
	Math::Vector3d *_origVertices;
	float _height;
	float _shrinkRadius;

	Math::Vector3d _normal;

	/** The containment test of the original (FUN_004a6980) works on its own normal (Newell's), the two axes with the largest part of it and a margin from the plane */
	void prepareGrimFunnel();
	void prepareGrimGeometry();
	bool _grimGeo;
	bool _grimFree;
	mutable int _grimExitEdge;
	int _grimFunnelEdge;
	bool _grimShrunk;
	int _origNumVertices;
	bool _origVisible;
	float grimEdgeDistance(int edge, const float *p) const;
	void grimFixPlaneCoordinate(float *p) const;
	float _gNormal[3];
	int _axisA, _axisB;
	float _gHeight;
};

} // end of namespace Grim

#endif
