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

#ifndef GRIM_EMI_PUCK_V2_H
#define GRIM_EMI_PUCK_V2_H

#include "engines/grim/emi/nodes/cnode.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/math/singlemath.h"

namespace Grim {

class EmiEngine;
class WalkPlane_v2;
class WalkPlaneCollection;

class Puck_v2 : public CNode {
public:
	Puck_v2(EmiEngine *engine, float newReflectionAngle);
	virtual ~Puck_v2();

	void reset();
	void validate(WalkPlaneCollection *collection);
	virtual void rotate(float angle);
	virtual float pointAt(const float *target, float maxAngle);
	virtual float getYawToPoint(const float *target);
	float *closestPointOnConstraintLine(float *out, const float *point);
	virtual bool forward(float distance, int allowLeft, int allowRight);
	virtual bool constrain(const float *vector);

	float _reflectionAngle;
	float _startStep;
	bool _constrained;
	float _constraint[3];
	float _lineA[3];
	float _lineB[3];
	LString _actorName;
	WalkPlane_v2 *_plane;
	WalkPlaneCollection *_planes;

private:
	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
