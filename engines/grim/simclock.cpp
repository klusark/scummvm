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

#include "common/array.h"
#include "common/system.h"

#include "audio/mixer_intern.h"

#include "engines/grim/simclock.h"

namespace Grim {

bool g_simActive = false;
// (not from 0: some of the code takes a time of 0 as "not set")
static const uint32 kSimStart = 10000;
uint32 g_simMillis = kSimStart;

struct SimTimer {
	Common::TimerManager::TimerProc proc;
	int32 intervalUsec;
	void *refCon;
	uint64 nextUsec;
};

static Common::Array<SimTimer> s_simTimers;
static Audio::MixerImpl *s_simMixer = nullptr;

uint32 grimMillis() {
	return g_simActive ? g_simMillis : g_system->getMillis();
}

bool grimInstallTimer(Common::TimerManager::TimerProc proc, int32 intervalUsec, void *refCon, const char *id) {
	if (!g_simActive)
		return g_system->getTimerManager()->installTimerProc(proc, intervalUsec, refCon, id);

	grimRemoveTimer(proc);
	SimTimer t;
	t.proc = proc;
	t.intervalUsec = intervalUsec < 1 ? 1 : intervalUsec;
	t.refCon = refCon;
	t.nextUsec = (uint64)g_simMillis * 1000 + t.intervalUsec;
	s_simTimers.push_back(t);
	return true;
}

void grimRemoveTimer(Common::TimerManager::TimerProc proc) {
	if (!g_simActive) {
		g_system->getTimerManager()->removeTimerProc(proc);
		return;
	}
	for (uint i = 0; i < s_simTimers.size(); i++) {
		if (s_simTimers[i].proc == proc) {
			s_simTimers.remove_at(i);
			return;
		}
	}
}

void simStart(Audio::Mixer *mixer) {
	g_simActive = true;
	g_simMillis = kSimStart;
	s_simTimers.clear();
	s_simMixer = static_cast<Audio::MixerImpl *>(mixer);
	s_simMixer->setSimulated(true);
}

void simAdvance(uint32 ms) {
	const uint32 end = g_simMillis + ms;
	const uint64 endUsec = (uint64)end * 1000;
	for (;;) {
		// the timer that is due first (the one installed first of two at the same time)
		int best = -1;
		for (uint i = 0; i < s_simTimers.size(); i++) {
			if (s_simTimers[i].nextUsec <= endUsec && (best < 0 || s_simTimers[i].nextUsec < s_simTimers[best].nextUsec))
				best = i;
		}
		if (best < 0)
			break;
		SimTimer &t = s_simTimers[best];
		g_simMillis = (uint32)(t.nextUsec / 1000);
		t.nextUsec += t.intervalUsec;
		Common::TimerManager::TimerProc proc = t.proc;
		void *refCon = t.refCon;
		// the sound up to the time of the timer is mixed (and thrown away) first, as the audio device would have played it
		s_simMixer->simulateStep(g_simMillis - kSimStart);
		// (the timer may remove itself or install others: t is not used after the call)
		proc(refCon);
	}
	g_simMillis = end;
	s_simMixer->simulateStep(g_simMillis - kSimStart);
}

} // End of namespace Grim
