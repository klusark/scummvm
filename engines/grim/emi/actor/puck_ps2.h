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

#ifndef GRIM_EMI_PUCK_PS2_H
#define GRIM_EMI_PUCK_PS2_H

#include "engines/grim/emi/actor/puck_v2.h"

namespace Grim {

class Puck_v2Ps2 : public Puck_v2 {
public:
	Puck_v2Ps2(EmiEngine *engine, float newReflectionAngle);

	void rotate(float angle) override;
	float pointAt(const float *target, float maxAngle) override;
	float getYawToPoint(const float *target) override;
	bool forward(float distance, int allowLeft, int allowRight) override;
	bool constrain(const float *vector) override;

	void updateWorld();
	void rotateTrunc(TruncFloat angle);
	TruncFloat getYawToPointTrunc(const float *target);
	TruncFloat pointAtTrunc(const float *target, TruncFloat maxAngle);
	bool forwardTrunc(TruncFloat distance);

private:
	void closestPointOnConstraintLine(float *out, const float *point);
	bool finishMove(const float *target);
	bool setPlane(WalkPlane_v2 *plane, const float *point);
	bool tryNeighborPlanes();
	void clampToPlane(const float *target);
};

} // End of namespace Grim

#endif
