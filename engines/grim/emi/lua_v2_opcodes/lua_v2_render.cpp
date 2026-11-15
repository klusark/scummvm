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

#include "common/textconsole.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/scene/clightnode.h"
#include "engines/grim/emi/graphics/dimregion.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/math/frect.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/movie/movie.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/sound/sounds.h"

namespace Grim {

static const float kDefaultDimAlpha = 0.7f;

CLightNode *Lua_V2::getLightArg(int paramNumber, const char *opcodeName) {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logMessage(kConsoleWarning, "%s: no current room!\n", opcodeName);

		return nullptr;
	}

	lua_Object nameObj = lua_getparam(paramNumber);

	if (!lua_isstring(nameObj)) {
		logMessage(kConsoleWarning, "%s: light name argument must be a string.\n", opcodeName);

		return nullptr;
	}

	CLightNode *light = currentRoom->findLight(lua_getstring(nameObj));

	if (!light)
		logMessage(kConsoleWarning, "%s: Invalid light name.\n", opcodeName);

	return light;
}

void Lua_V2::TurnLightOn() {
	CLightNode *light = getLightArg(1, "TurnLightOn");

	if (light) {
		lua_Object activeObj = lua_getparam(2);

		light->setActive(lua_isnil(activeObj) == 0);
	}
}

void Lua_V2::IsLightOn() {
	CLightNode *light = getLightArg(1, "IsLightOn");

	if (light && light->_active)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::SetLightPosition() {
	CLightNode *light = getLightArg(1, "SetLightPosition");

	if (!light) {
		logMessage(kConsoleDebug, "SetLightPosition: first param must be a light.\n");

		return;
	}

	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		logMessage(kConsoleDebug, "SetLightPosition: must provide x,y, and z coordinates.\n");

		return;
	}

	CVector position;

	position._z = lua_getnumber(zObj);
	position._y = lua_getnumber(yObj);
	position._x = lua_getnumber(xObj);
	light->setLocalPosition(position);
}

void Lua_V2::GammaEnabled() {
	if (_engine->hasGammaControl())
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::SetGamma() {
	lua_Object gammaObj = lua_getparam(1);

	if (lua_isnumber(gammaObj))
		_engine->setGamma(lua_getnumber(gammaObj));
}

void Lua_V2::SetPixelCenter() {
	lua_Object centerObj = lua_getparam(1);

	if (!lua_isnumber(centerObj))
		return;

	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom)
		return;

	if (!currentRoom->getSetupCamera(-1))
		return;

	CDrawContext *drawContext = _engine->getDrawContext();

	if (!drawContext)
		return;

	float value = lua_getnumber(centerObj);

	drawContext->setPixelCenter(value > 0.0f || value < 0.0f);
}

void Lua_V2::LoadBundle() {
	lua_Object islandObj = lua_getparam(1);

	if (!lua_isstring(islandObj) && !lua_isnil(islandObj))
		return;

	const char *islandName = lua_getstring(islandObj);

	if (!islandName) {
		warning("Lua_V2::LoadBundle(): the island name is nil");
		lua_pushnil();

		return;
	}

	if (_engine->setIsland(islandName))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::RenderModeUser() {
	lua_Object modeObj = lua_getparam(1);
	lua_Object soundObj = lua_getparam(2);
	int mode = _engine->getMode();

	if (lua_isnil(modeObj) && mode == EmiEngine::kModeOverworld) {
		Movie::resumeCurrent(_engine);

		if (lua_isnil(soundObj))
			_engine->getSounds()->resumeAll();

		_engine->setMode(_engine->getSavedMode());
		logMessage(kConsoleStatus, "RENDERMODE USER nil - Set mode to %d\n", _engine->getMode());
	} else if (!lua_isnil(modeObj) && mode != EmiEngine::kModeOverworld) {
		logMessage(kConsoleStatus, "RENDERMODE USER TRUE - Set mode to %d\n", mode);
		_engine->setSavedMode(mode);
		Movie::pauseCurrent(_engine);

		if (lua_isnil(soundObj))
			_engine->getSounds()->pauseAll();

		_engine->setMode(EmiEngine::kModeOverworld);
		toggleStatsOverlay(true);
	} else {
		logMessage(kConsoleStatus, "RENDERMODE USER %s - NO ACTION\n", lua_isnil(modeObj) ? "nil" : "TRUE");
	}
}

void Lua_V2::toggleStatsOverlay(bool hide) {
	if (hide || (_statsOverlayFlags & kStatsOverlayFlag) != 0)
		_statsOverlayFlags &= ~kStatsOverlayFlag;
	else
		_statsOverlayFlags |= kStatsOverlayFlag;
}

void Lua_V2::DimScreen() {
	lua_Object alphaObj = lua_getparam(1);
	int regionId;

	if (lua_isnumber(alphaObj))
		regionId = Grim::DimRegion::dimScreen(_engine, 0, lua_getnumber(alphaObj));
	else
		regionId = Grim::DimRegion::dimScreen(_engine, 0, kDefaultDimAlpha);

	lua_pushnumber((float)(uint)regionId);
}

void Lua_V2::DimRegion() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object widthObj = lua_getparam(3);
	lua_Object heightObj = lua_getparam(4);
	lua_Object alphaObj = lua_getparam(5);
	FRect region;

	region._x = lua_getnumber(xObj);
	region._y = lua_getnumber(yObj);
	region._w = lua_getnumber(widthObj);
	region._h = lua_getnumber(heightObj);

	float alpha = lua_getnumber(alphaObj);

	lua_pushnumber((float)(uint)Grim::DimRegion::dimRegion(_engine, &region, alpha));
}

void Lua_V2::UndimAll() {
	Grim::DimRegion::undimAll(_engine);
}

void Lua_V2::UndimRegion() {
	lua_Object regionObj = lua_getparam(1);

	if (lua_isnumber(regionObj)) {
		bool found = Grim::DimRegion::undimRegion(_engine, luaFtol(lua_getnumber(regionObj)));

		if (!found)
			logMessage(kConsoleWarning, "UndimRegion() - could not find the given dimregion id");
	} else {
		logMessage(kConsoleError, "UndimRegion() - argument is not a dimregion id");

		lua_pushnil();
	}
}

void Lua_V2::SetSpeechMode() {
	lua_Object modeObj = lua_getparam(1);

	if (!lua_isnumber(modeObj)) {
		logMessage(kConsoleDebug, "SetSpeechMode: argument must be a number!\n");

		return;
	}

	int mode = luaFtol(lua_getnumber(modeObj));

	if (mode >= 1 && mode <= 3) {
		Actor::setSpeechMode(_engine, mode);

		if (mode == 1)
			logMessage(kConsoleDebug, "SetSpeechMode: mode set to Text Only.\n");
		else if (mode == 2)
			logMessage(kConsoleDebug, "SetSpeechMode: mode set to Voice Only.\n");
		else
			logMessage(kConsoleDebug, "SetSpeechMode: mode set to Voice and Text.\n");
	} else {
		logMessage(kConsoleDebug, "SetSpeechMode: invalid mode argument.\n");
	}
}

void Lua_V2::GetSpeechMode() {
	int mode = Actor::getSpeechMode(_engine);

	if ((mode & 2) != 0) {
		if ((mode & 1) != 0) {
			lua_pushnumber(3.0f);

			return;
		}
	} else if ((mode & 1) != 0) {
		lua_pushnumber(1.0f);

		return;
	}

	lua_pushnumber(2.0f);
}

void Lua_V2::GetCPUSpeed() {
	lua_pushnumber((float)_engine->getCpuSpeed());
}

} // end of namespace Grim
