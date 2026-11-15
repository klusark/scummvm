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

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/actor/extra_v2.h"
#include "engines/grim/emi/core/lmemoryservices.h"

namespace Grim {

Extra_v2::Extra_v2(EmiEngine *engine) : _engine(engine) {
	_lookAtActive = false;
	_lookAtHasTarget = false;
	_lookTargetActorId = 0;

	for (int i = 0; i < kNumLookLimits; i++) {
		_lookLimits[i]._x = 0.0f;
		_lookLimits[i]._y = 0.0f;
		_lookLimits[i]._z = 1.0f;
	}

	_lookCosFov = 0.5f;
	_lookRate = Actor::kDegreesToRadians * 270.0f;
	_collisionScale = 1.0f;
	_timeScale = 1.0f;
	_walkRate = 0.4f;
	_turnRate = 135.0f;
	_walkDominate = false;
	_reflectionAngle = 60.0f;

	Color white;

	white._r = 255;
	white._g = 255;
	white._b = 255;
	_talkColor = white;

	_walking = false;
	_turnDir = 0;
	_tankTurn = false;
	_resting = false;
	_aligned = false;
	_snapTurn = false;
	_offsetYaw = 0.0f;
	_alignOff = 0.0f;
	_numPathPoints = 0;
	_pathIndex = 0;
	_puck = nullptr;
	_inBoxes = false;
	_talking = false;
	_soundHandle = 0;
	_talkTimerMs = 0;
	_sayLineId[0] = 0;
	_textEventId = 0;
	_lipsyncActive = false;
	_visemeCurrent = 0;
	_collisionMode = 0;
	_inCollision = false;

	// The disasm leaves visemeMs and the tail of sayLineId uninitialized
	_visemeMs = 0;

	for (int i = 1; i < kSayLineIdSize; i++)
		_sayLineId[i] = 0;
}

void *Extra_v2::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void Extra_v2::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

} // End of namespace Grim
