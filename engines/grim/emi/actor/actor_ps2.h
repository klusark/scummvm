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

#ifndef GRIM_EMI_ACTOR_PS2_H
#define GRIM_EMI_ACTOR_PS2_H

#include "engines/grim/emi/actor/actor.h"

namespace Grim {

class RPrimitive;

class ActorPs2 : public Actor {
public:
	ActorPs2(EmiEngine *engine, const char *newActorName);

	static float collisionRadius(const RPrimitive *prim, float scale);

	Puck_v2 *createPuck(float newReflectionAngle) override;
	void updateMovement(float frameDelta, float scaledDelta) override;
	int buildAvoidingPath(Actor *other, float *path, int count, float *outPath) override;
	int findClosestCandidate(float startX, float startY, float startZ, float goalX, float goalY, float goalZ, const ObjectCorners_v2 *corners) override;

private:
	void followPath(float deltaTime);
	void updateWalk(float deltaTime);
	bool alignToPuck(Puck_v2 *puck);

	static void getOuttaTheBox(float *point, const float *box, const float *transform);
	static bool testCollision(EmiEngine *engine, float startX, float startY, float startZ, float endX, float endY, float endZ, float originX, float originY, float originZ, const float *box, const float *transform);
};

} // End of namespace Grim

#endif
