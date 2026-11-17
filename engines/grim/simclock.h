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

#ifndef GRIM_SIMCLOCK_H
#define GRIM_SIMCLOCK_H

#include "common/scummsys.h"
#include "common/timer.h"

namespace Audio {
class Mixer;
}

namespace Grim {

// The clock of the game. In a scripted test run (--sim-fps) it is a simulated one that moves on by a fixed time per
// frame, and the timers of iMUSE and SMUSH and the mixer are stepped with it, so that a run does not depend on the
// speed of the machine. Otherwise it is the clock of the system.

/** Whether the clock is simulated */
extern bool g_simActive;
/** The simulated clock in ms */
extern uint32 g_simMillis;

/** The clock of the game in ms: the simulated one in a simulated run, the system's otherwise */
uint32 grimMillis();

/** As the timer manager's installTimerProc/removeTimerProc; in a simulated run the timer fires by the simulated clock */
bool grimInstallTimer(Common::TimerManager::TimerProc proc, int32 intervalUsec, void *refCon, const char *id);
void grimRemoveTimer(Common::TimerManager::TimerProc proc);

/** Starts the simulated clock; the mixer is then stepped by simAdvance() */
void simStart(Audio::Mixer *mixer);
/** Moves the simulated clock on by ms: the timers that fall due fire in order, the mixer mixes the sound of that time */
void simAdvance(uint32 ms);

} // End of namespace Grim

#endif
