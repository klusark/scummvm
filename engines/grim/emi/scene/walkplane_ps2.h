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

#ifndef GRIM_EMI_WALKPLANE_PS2_H
#define GRIM_EMI_WALKPLANE_PS2_H

#include "engines/grim/emi/scene/walkplane_v2.h"

namespace Grim {

class WalkPlane_v2Ps2 : public WalkPlane_v2 {
public:
	WalkPlane_v2Ps2(EmiEngine *engine, const float *vertices, int vertexCount, WalkPlaneCollection *owner);

	bool within(const float *point, bool checkActive) override;
	void closestPoint(float *point) override;
	void findExit(const float *ahead, const float *back, float *out) override;
	void project(float *point) override;
	float *getConstraintVector(Puck_v2 *puck) override;
	void jumpOut(Puck_v2 *puck) override;
	bool closestSharedVertex(WalkPlane_v2 *planeB, float x, float y, float z, float *out) override;

	void clampToEdges(float *point);

private:
	void computeEquation();
	void closestPointOnEdge(float *point, const float *current, const float *previous);
	static bool segmentCrosses(const float *startA, const float *endA, const float *startB, const float *endB, float *ratio);
};

} // End of namespace Grim

#endif
