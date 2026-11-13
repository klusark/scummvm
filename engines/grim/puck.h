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

#ifndef GRIM_PUCK_H
#define GRIM_PUCK_H

#include "common/scummsys.h"

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

class WalkPlane;
class WalkSystem;

class Puck {
public:
	Puck(WalkSystem *system, float angle);
	~Puck();

	float *rot();
	WalkPlane *validate();
	void setPos(float *pos);
	void xform(float *vec);
	void invXform(float *vec);
	void rotate(float angle);
	float pointAt(float *target, float maxAngle);
	double getYawToPoint(float *target);
	void constrain(int enable);
	int forward(float dist, int allowLeft, int allowRight);
	int forwardDemo(float dist, int allowLeft, int allowRight);
	void orientToPlane(WalkPlane *targetPlane);
	void normalize();
	int draw(uint16 color);

	RdVector3 _pos;
	RdMatrix34 _orient;
	float _reflectionAngle;
	float _forwardDist;
	RdVector3 _constraint;
	int _constrained;
	Puck *_prev;
	Puck *_next;
	WalkPlane *_plane;

private:
	int forwardRemastered(float dist, float *direction, int allowLeft, int allowRight);
	void orientToPlaneRemastered(WalkPlane *targetPlane);

	WalkSystem *_system;
	RdVector3 _rot;
};

} // End of namespace Grim

#endif
