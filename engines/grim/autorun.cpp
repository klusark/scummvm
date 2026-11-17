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

// Scripted test runs (grim_tools/autorun): --autorun, --sim-fps and the remote Lua command file.

#define FORBIDDEN_SYMBOL_EXCEPTION_getenv
#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_fopen
#define FORBIDDEN_SYMBOL_EXCEPTION_fputs
#define FORBIDDEN_SYMBOL_EXCEPTION_fclose
#define FORBIDDEN_SYMBOL_EXCEPTION_remove
#define FORBIDDEN_SYMBOL_EXCEPTION_rename

#include "common/config-manager.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/simclock.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/lua/ldo.h"

namespace Grim {

void GrimEngine::autorunInit() {
	_autorunPending = ConfMan.hasKey("autorun");
	if (ConfMan.hasKey("sim_fps"))
		_simFps = CLIP(ConfMan.getInt("sim_fps"), 1, 1000);
	// sim_frame_ms: every simulated frame is exactly that many milliseconds (60 fps gives 16 and 17 ms alternately)
	if (ConfMan.hasKey("sim_frame_ms")) {
		_simMs = CLIP(ConfMan.getInt("sim_frame_ms"), 1, 1000);
		if (!_simFps)
			_simFps = 1000 / _simMs;
	}
	if (ConfMan.hasKey("sim_render_every"))
		_simRenderEvery = CLIP(ConfMan.getInt("sim_render_every"), 1, 1000);
	g_luaBusyLimit = 0;
	if (_simFps) {
		// the sound, iMUSE and the movies run in steps of the simulated time too (simclock.h)
		simStart(_mixer);
		// sim_busy_calls: a script that makes that many calls of C functions in one frame without yielding moves the
		// clock on by a frame (0 = never), see autorunBusyFrame
		g_luaBusyLimit = ConfMan.hasKey("sim_busy_calls") ? (uint32)MAX(ConfMan.getInt("sim_busy_calls"), 0) : 1000000;
		g_luaBusyHook = &GrimEngine::autorunBusyFrame;
	}
}

uint32 GrimEngine::simFrameMs() const {
	if (_simMs)
		return _simMs;
	return (uint32)((uint64)(_simFrame + 1) * 1000 / _simFps) - (uint32)((uint64)_simFrame * 1000 / _simFps);
}

// In the original the clock runs on while a script loops without yielding: a loop that waits for a sound to start
// (Tumbler.make_noise of the safe in vd.lua starts sounds until one gets a track) ends when the sounds that hold the
// tracks are over. A simulated run gives such a script a frame of time every g_luaBusyLimit calls of C functions.
void GrimEngine::autorunBusyFrame() {
	simAdvance(g_grim->simFrameMs());
	warning("A script runs on without yielding: the clock is moved on by a frame (simulated clock %u ms)", g_simMillis);
}

// Before a frame: the simulated clock moves on by the time of a frame, and it is decided whether the frame is drawn
void GrimEngine::autorunBeginFrame() {
	_simSkipRender = false;
	if (!_simFps)
		return;
	simAdvance(simFrameMs());
	_simFrame++;
	g_luaBusyCalls = 0;
	// A simulated run goes through thousands of frames a second, drawing them all is most of what it costs: one frame
	// in _simRenderEvery is drawn (by the count of frames, so that a run stays the same every time), and every frame
	// that is not of the normal mode (movies, menus) or in which the mode or the room has changed
	if (_simRenderEvery > 1) {
		void *room = _sputRoom ? (void *)_sputRoom->getCurrentRoom() : nullptr;
		if (_libMode != kLibModeNormal || _libMode != _simLastMode || room != _simLastRoom || _simFrame % _simRenderEvery == 0) {
			_simLastMode = _libMode;
			_simLastRoom = room;
		} else {
			_simSkipRender = true;
		}
	}
}

// After a frame: the script given with --autorun is started once the game is running in a room (which is also the
// case after a game was loaded), and the remote command file is looked for
void GrimEngine::autorunEndFrame() {
	if (_autorunPending && _sputRoom && _sputRoom->getCurrentRoom() && (_libMode == kLibModeNormal || _libMode == 3))
		runAutorun();
	pollRemoteLua();
}

// An autorun script that loaded a game has to run again on the loaded state
void GrimEngine::autorunGameLoaded() {
	if (ConfMan.hasKey("autorun")) {
		_autorunPending = true;
		_autorunLoaded = true;
	}
}

void GrimEngine::runAutorun() {
	_autorunPending = false;

	const Common::Path path = ConfMan.getPath("autorun");
	Common::FSNode node(path);
	Common::SeekableReadStream *stream = node.createReadStream();
	if (!stream) {
		warning("Autorun: could not open '%s'", path.toString().c_str());
		return;
	}
	Common::String script = stream->readString(0, stream->size());
	delete stream;

	// As a task the script can wait, for example for an actor to arrive
	warning("Autorun: running '%s'", path.toString().c_str());
	// A save made while the script was running holds that task, so stop an old one first
	Common::String task = Common::String::format("%sif __autorun__ then stop_script(__autorun__) end\n__autorun__ = function()\n%s\nend\nstart_script(__autorun__)", _autorunLoaded ? "AR_LOADED = TRUE\n" : "", script.c_str());
	if (lua_dostring(task.c_str()) != 0) {
		// Nothing would ever run, so don't leave the game sitting there
		warning("Autorun: '%s' failed to load", path.toString().c_str());
		quitGame();
	}
}

// Debug aid: with SCUMMVM_LUA_REMOTE=<dir> a file <dir>/cmd.lua is run as Lua when it appears (and
// deleted), the global string R is then written to <dir>/cmd.out
void GrimEngine::pollRemoteLua() {
	static const char *dir = getenv("SCUMMVM_LUA_REMOTE");
	if (!dir)
		return;
	const uint32 now = g_system->getMillis();
	if (now - _remotePoll < 100)
		return;
	_remotePoll = now;
	Common::FSNode cmd(Common::Path(Common::String(dir) + "/cmd.lua"));
	if (!cmd.exists())
		return;
	Common::SeekableReadStream *in = cmd.createReadStream();
	if (!in)
		return;
	Common::String code = in->readString(0, in->size());
	delete in;
	remove((Common::String(dir) + "/cmd.lua").c_str());
	lua_dostring("R = \"\"");
	int status = lua_dostring(code.c_str());
	lua_Object r = lua_getglobal("R");
	Common::String out = lua_isstring(r) ? lua_getstring(r) : "";
	if (status != 0)
		out += " (error)";
	Common::String tmp = Common::String(dir) + "/cmd.tmp";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (f) {
		fputs(out.c_str(), f);
		fclose(f);
		rename(tmp.c_str(), (Common::String(dir) + "/cmd.out").c_str());
	}
}

} // End of namespace Grim
