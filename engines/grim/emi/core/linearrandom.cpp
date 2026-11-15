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

#include "engines/grim/emi/core/linearrandom.h"

namespace Grim {

static const uint32 kMultiplier = 0x41C64E6D;
static const uint32 kIncrement = 0x3039;
static const uint32 kInitialState = 1;
static const uint32 kResultMask = 0x7FFFFFFF;

LinearRandom::LinearRandom() : _state(kInitialState) {
}

void LinearRandom::seed(uint32 newSeed) {
	_state = newSeed;
}

int32 LinearRandom::next() {
	_state = _state * kMultiplier + kIncrement;

	return (int32)(_state & kResultMask);
}

} // End of namespace Grim
