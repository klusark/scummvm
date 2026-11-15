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

#include "common/system.h"
#include "common/textconsole.h"

#include "engines/grim/debug.h"
#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/math/cvector.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/input/demoreplay.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/nodes/rnode.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/lua/luadebug.h"

namespace Grim {

static const float kOverworldNearClip = 0.05f;
static const float kOverworldFarClip = 20.0f;
static const float kPs2ScreenWidth = 640.0f;
static const float kPs2ScreenHeight = 480.0f;
static const float kPs2HalfScreenWidth = 320.0f;
static const float kPs2HalfScreenHeight = 240.0f;

void Lua_V2::SetReplayMode() {
	lua_Object modeObj = lua_getparam(1);
	lua_Object nameObj = lua_getparam(2);

	if (!lua_isnumber(modeObj))
		return;

	int mode = luaFtol(lua_getnumber(modeObj));
	const char *name = nullptr;

	if (lua_isstring(nameObj))
		name = lua_getstring(nameObj);

	_engine->setReplayRequest(mode, name);
}

void Lua_V2::GetReplayMode() {
	if (_engine->getDemoReplay())
		lua_pushnumber((float)_engine->getDemoReplay()->_mode);
}

void Lua_V2::GetNextDemoNum() {
	int demoNumber = _engine->getNextDemoNum();

	if (demoNumber >= 0)
		lua_pushnumber((float)demoNumber);
}

void Lua_V2::NotifyGameState() {
	// The menu flag only blocks the PS2 reboot to the disc launcher
}

void Lua_V2::DeleteSavedGame() {
	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj))
		return;

	// When the last save is deleted the card id goes back to 2, like a removed folder
	if (_engine->deleteSaveFile(lua_getstring(nameObj)))
		lua_pushnumber(1.0f);
}

void Lua_V2::CopySavedGame() {
	lua_Object sourceObj = lua_getparam(1);
	lua_Object targetObj = lua_getparam(2);

	if (!lua_isstring(sourceObj) || !lua_isstring(targetObj))
		return;

	if (_engine->copySaveFile(lua_getstring(sourceObj), lua_getstring(targetObj)))
		lua_pushnumber(1.0f);
}

void Lua_V2::JustSaved() {
	if (_engine->justSaved())
		lua_pushnumber(1.0f);
}

void Lua_V2::GetMemoryCardId() {
	lua_pushnumber((float)_engine->getMemoryCardId());
}

void Lua_V2::GetUniqueUint() {
	lua_pushnumber((float)getClockUniqueUint());
}

void Lua_V2::FormatMemoryCard() {
	// The virtual card is always formatted, so formatting fails
}

void Lua_V2::SetActorTessellation() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object modeObj = lua_getparam(2);

	if (!isActor(actorObj)) {
		logMessage(kConsoleDebug, "SetActorTessellation: Invalid parameter.\n");

		return;
	}

	Actor *actor = Actor::actorDeref(_engine, lua_getuserdata(actorObj));

	if (!actor) {
		logMessage(kConsoleDebug, "SetActorTessellation: Invalid Actor handle.\n");

		return;
	}

	// Only the GS renderer reads it
	if (lua_isnil(modeObj)) {
		actor->_drawFlags &= ~(uint)RNode::kTessellation;
	} else if (lua_isnumber(modeObj)) {
		if (luaFtol(lua_getnumber(modeObj)) != 0)
			actor->_drawFlags |= RNode::kTessellation;
		else
			actor->_drawFlags &= ~(uint)RNode::kTessellation;
	} else {
		logMessage(kConsoleDebug, "SetActorTessellation: Invalid parameter.\n");
	}
}

void Lua_V2::IsActorChoring() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object skipLoopingObj = lua_getparam(2);
	Actor *actor = nullptr;

	if (isActor(actorObj))
		actor = Actor::actorDeref(_engine, lua_getuserdata(actorObj));

	if (!actor) {
		logMessage(kConsoleDebug, "GetActorChores: Invalid actor handle.\n");
		lua_pushnil();

		return;
	}

	bool skipLooping = !lua_isnil(skipLoopingObj);
	bool choring = false;
	uint foundId;
	uint choreId = 0;

	while (!choring) {
		choreId = *actor->findChoreInstance(&foundId, choreId);

		if (!choreId)
			break;

		if (!ChoreInstance::isInstanceValid(_engine, choreId) || !ChoreInstance::getInstance(_engine, choreId)->isPlaying())
			continue;

		if (!ChoreInstance::isInstanceValid(_engine, choreId) || !ChoreInstance::getInstance(_engine, choreId)->isLooping() || !skipLooping)
			choring = true;
	}

	if (choring)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::ApplyForceFeedback() {
	lua_Object controllerObj = lua_getparam(1);

	if (!lua_isnumber(controllerObj)) {
		logMessage(kConsoleDebug, "GetControlState: Invalid control (non-numerical).\n");

		return;
	}

	// No pad vibration here
}

void Lua_V2::EnableSoakText() {
	// The soak test overlay is not ported
}

void Lua_V2::SetStereoAudio() {
	lua_Object stereoObj = lua_getparam(1);
	bool stereo = true;

	if (!lua_isnil(stereoObj))
		stereo = lua_getnumber(stereoObj) != 0.0f;

	_engine->getSounds()->setStereoAudio(stereo);
}

void Lua_V2::pspu() {
	// This dumps the SPU profile to the IOP console
}

void Lua_V2::dsnd() {
	logMessage(kConsoleDebug, "Debug LECAudio:  0->off, 1->Level 1, 2->Level 2 (streams), 3->Level 3\n");

	// The IOP debug level is not ported
}

void Lua_V2::KillMusicAndWait() {
	_engine->getSounds()->killMusicAndWait(lua_isnil(lua_getparam(1)));
}

void Lua_V2::OverWorldToScreen() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object zObj = lua_getparam(3);
	Camera camera(_engine->getDrawContext());

	camera.setLocalPosition(CVector());
	camera._nearClip = kOverworldNearClip;
	camera._farClip = kOverworldFarClip;

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		logMessage(kConsoleDebug, "OverworldWorldToScreen: Parameters must be x, y, z coords.\n");

		return;
	}

	float z = lua_getnumber(zObj);
	float y = lua_getnumber(yObj);
	float x = lua_getnumber(xObj);

	camera.updateViewMatrix();

	const float *m = camera._view._rot._m;
	float pointX = ((x * m[0] + y * m[3]) + z * m[6]) + camera._view._pos._x;
	float pointY = ((x * m[1] + y * m[4]) + z * m[7]) + camera._view._pos._y;
	float depth = ((x * m[2] + y * m[5]) + z * m[8]) + camera._view._pos._z;

	if (depth == 0.0f) {
		warning("Lua_V2::OverWorldToScreen(): point is on the camera plane");
		lua_pushnil();
		lua_pushnil();

		return;
	}

	float inverseDepth = 1.0f / depth;
	float screenX = camera._halfWidth + inverseDepth * (pointX * camera._focalX);
	float screenY = camera._halfHeight + inverseDepth * (-pointY * camera._focalY);

	if (!_engine->getDrawContext() || screenX < 0.0f || !(screenX <= kPs2ScreenWidth) || screenY < 0.0f || !(screenY <= kPs2ScreenHeight)) {
		lua_pushnil();
		lua_pushnil();

		return;
	}

	lua_pushnumber((screenX - kPs2HalfScreenWidth) / kPs2HalfScreenWidth);
	lua_pushnumber((kPs2HalfScreenHeight - screenY) / kPs2HalfScreenHeight);
}

void Lua_V2::SetVScanOffsets() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj)) {
		logMessage(kConsoleWarning, "SetVScanOffsets() - invalid params");

		return;
	}

	// This is the TV display offset; there is nothing to move here
}

void Lua_V2::ToggleDrawFPS() {
	toggleStatsOverlay(false);
}

void Lua_V2::UnimplementedFunctionPs2() {
	const char *name;

	lua_getobjname(lua_stackedfunction(0), &name);

	if (!name) {
		warning("Lua_V2::UnimplementedFunctionPs2(): function without name");
		name = "";
	}

	Debug::debugImpl(Debug::Scripts, "UNIMPLEMENTED FUNCTION: %s\n%s", name, lua_getstacktext().c_str());
}

void Lua_V2::ObsoleteFunctionPs2() {
	const char *name;

	lua_getobjname(lua_stackedfunction(0), &name);

	if (!name) {
		warning("Lua_V2::ObsoleteFunctionPs2(): function without name");
		name = "";
	}

	Debug::debugImpl(Debug::Scripts, "OBSOLETE FUNCTION: %s\n%s", name, lua_getstacktext().c_str());
}

uint Lua_V2::getClockUniqueUint() {
	TimeDate now;

	g_system->getTimeAndDate(now);

	uint second = (uint)(now.tm_sec / 10 * 16 + now.tm_sec % 10);
	uint minute = (uint)(now.tm_min / 10 * 16 + now.tm_min % 10);
	uint hour = (uint)(now.tm_hour / 10 * 16 + now.tm_hour % 10);
	uint day = (uint)(now.tm_mday / 10 * 16 + now.tm_mday % 10);
	uint month = (uint)((now.tm_mon + 1) / 10 * 16 + (now.tm_mon + 1) % 10);

	// Clock fields stay BCD, like the original
	return month * 100000000 + day * 1000000 + hour * 10000 + minute * 100 + second;
}

} // End of namespace Grim
