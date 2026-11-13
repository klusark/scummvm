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

#include "common/platform.h"
#include "common/system.h"

#include "engines/grim/grim.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/puck.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/stdcontrol.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

enum {
	kImuseGroupCommentary = 5,
	kActorFlagBlackAndWhite = 0x40,
	kActorModeHkHack = 4,
	kTextCanvasWidthPlay = 1440,
	kTextCanvasWidthMenu = 1920
};

// The kinds of input device the scripts are told about
enum {
	kInputModePad = 1,
	kInputModeKeys = 2,
	kInputModeMouse = 3
};

// Opcodes of the main table that the remastered has and the original does not.
// The ones that belong to another module are in remastered/lua_remastered.cpp.

void Lua_V1::GetPlatform() {
	int platform = 1;

	switch (g_grim->getGamePlatform()) {
	case Common::kPlatformMacintosh:
		platform = 2;
		break;
	case Common::kPlatformLinux:
		platform = 3;
		break;
	default:
		break;
	}

	lua_pushnumber((float)platform);
}

// The desktop builds of the original do nothing for the achievements
void Lua_V1::UnlockAchievement() {
}

void Lua_V1::ResetAchievements() {
}

void Lua_V1::AreAchievementsInstalled() {
	lua_pushnumber(1.0f);
}

void Lua_V1::GlobalSaveResolved() {
	lua_pushnumber(1.0f);
}

// The cross-save check exists on the consoles only
void Lua_V1::StartCheckOfCrossSaveStatus() {
}

void Lua_V1::GetCrossSaveStatus() {
	lua_pushnumber(1.0f);
}

void Lua_V1::UnlockCutscene() {
	lua_Object indexObj = lua_getparam(1);

	if (lua_isnumber(indexObj))
		g_grim->_wuRegistry->unlockCutscene(luaFtol(lua_getnumber(indexObj)));
}

void Lua_V1::IsCutsceneUnlocked() {
	lua_Object indexObj = lua_getparam(1);

	if (lua_isnumber(indexObj) && g_grim->_wuRegistry->isCutsceneUnlocked(luaFtol(lua_getnumber(indexObj))))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::UnlockConcept() {
	lua_Object indexObj = lua_getparam(1);

	if (lua_isnumber(indexObj))
		g_grim->_wuRegistry->unlockConcept(luaFtol(lua_getnumber(indexObj)));
}

void Lua_V1::IsConceptUnlocked() {
	lua_Object indexObj = lua_getparam(1);

	if (lua_isnumber(indexObj) && g_grim->_wuRegistry->isConceptUnlocked(luaFtol(lua_getnumber(indexObj))))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::SetActorBWMode() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object modeObj = lua_getparam(2);

	if (!isSputActor(actorObj) || !lua_isnumber(modeObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_getnumber(modeObj) <= 0.0f) {
		if (actor->flags & kActorFlagBlackAndWhite) {
			actor->flags &= ~kActorFlagBlackAndWhite;

			if (actor->extra && actor->extra->puck)
				actor->extra->puck->rotate(180.0f);
			else
				warning("Lua_V1::SetActorBWMode(): the actor has no puck to turn around");
		}
	} else {
		actor->flags |= kActorFlagBlackAndWhite;
	}
}

void Lua_V1::SetActorHKHackMode() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object modeObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isnumber(modeObj) && lua_getnumber(modeObj) > 0.0f)
		actor->mode |= kActorModeHkHack;
	else
		actor->mode &= ~kActorModeHkHack;
}

void Lua_V1::ReadRegistryIntValue() {
	lua_Object keyObj = lua_getparam(1);

	if (!lua_isstring(keyObj)) {
		lua_pushnil();
		return;
	}

	const char *key = lua_getstring(keyObj);
	WuRegistry *wuRegistry = g_grim->_wuRegistry;

	if (wuRegistry->queryType(key) == WuRegistry::kValueBinary)
		lua_pushnumber((float)wuRegistry->getInt(key, 0));
	else
		lua_pushnil();
}

void Lua_V1::SaveRegistryToDisk() {
	g_grim->_wuRegistry->saveRegistry();
}

void Lua_V1::EnableBackgroundRender() {
	lua_Object enableObj = lua_getparam(1);

	if (!lua_isnumber(enableObj))
		return;

	int enable = luaFtol(lua_getnumber(enableObj));

	g_grim->_sputRender->preRendered(enable, enable);
}

void Lua_V1::ImGetCommentaryVol() {
	lua_pushnumber((float)g_grim->getImuse()->imSetGroupVol(kImuseGroupCommentary, -1));
}

void Lua_V1::ImSetCommentaryVol() {
	lua_Object volObj = lua_getparam(1);

	if (!lua_isnumber(volObj))
		return;

	g_grim->getImuse()->imSetGroupVol(kImuseGroupCommentary, luaFtol(lua_getnumber(volObj)));
}

// Only the high resolution renderer uses these two values
void Lua_V1::SetResolutionScaling() {
	lua_Object valueObj = lua_getparam(1);

	if (!lua_isnumber(valueObj) || g_grim->getLibMode() == GrimEngine::kLibModeUser)
		return;

	_resolutionScaling = lua_getnumber(valueObj);
}

void Lua_V1::SetAdvancedLighting() {
	lua_Object valueObj = lua_getparam(1);

	if (!lua_isnumber(valueObj) || g_grim->getLibMode() == GrimEngine::kLibModeUser)
		return;

	_advancedLighting = (uint32)luaFtol(lua_getnumber(valueObj));
}

// The menu and the user modes pause the movie, and leaving them draws the scene again unless
// the second argument is set. A mode that is not a number is mode 0, as in the original.
void Lua_V1::SetGameRenderMode() {
	lua_Object modeObj = lua_getparam(1);
	lua_Object keepObj = lua_getparam(2);
	int mode = luaFtol(lua_getnumber(modeObj));
	int oldMode = g_grim->getLibMode();

	if (mode == oldMode)
		return;

	bool wasPaused = oldMode == GrimEngine::kLibModeUser || oldMode == GrimEngine::kLibModeMenu;
	bool isPaused = mode == GrimEngine::kLibModeUser || mode == GrimEngine::kLibModeMenu;

	if (isPaused && !wasPaused) {
		g_grim->_sputSmush->pause(1);
	} else if (!isPaused && wasPaused) {
		g_grim->_sputSmush->pause(0);

		if (lua_isnil(keepObj) || luaFtol(lua_getnumber(keepObj)) == 0)
			g_grim->_sputRender->forceRefresh();
	}

	g_grim->setLibMode(mode);
	g_grim->setTextCanvasWidth(mode == GrimEngine::kLibModeMenu ? kTextCanvasWidthMenu : kTextCanvasWidthPlay);
}

void Lua_V1::GetGameRenderMode() {
	lua_pushnumber((float)g_grim->getLibMode());
}

void Lua_V1::GetSystemTime() {
	lua_pushnumber((float)g_system->getMillis());
}

void Lua_V1::SetLanguage() {
	lua_Object languageObj = lua_getparam(1);

	if (lua_isnumber(languageObj))
		g_grim->setLanguage(luaFtol(lua_getnumber(languageObj)));
}

void Lua_V1::GetLanguage() {
	lua_pushnumber((float)g_grim->getLanguage());
}

void Lua_V1::GetLanguageCount() {
	lua_pushnumber((float)GrimEngine::getLanguageCount());
}

void Lua_V1::LoadRemappedKeys() {
	for (int i = 0; i < StdControl::kNumRemappedKeys; i++) {
		Common::String name = Common::String::format("Remap%d", i);
		int sdlKey = g_grim->_wuRegistry->getInt(name.c_str(), 0);

		if (sdlKey)
			g_grim->_stdControl->setRemappedKey(i, sdlKey);
	}
}

void Lua_V1::SaveRemappedKeys() {
	for (int i = 0; i < StdControl::kNumRemappedKeys; i++) {
		Common::String name = Common::String::format("Remap%d", i);

		g_grim->_wuRegistry->saveInt(name.c_str(), g_grim->_stdControl->getRemappedKey(i));
	}
}

bool Lua_V1::getSaveSlotTime(int slot, Common::String *time) {
	return g_grim->_wuRegistry->getSlotTime(slot, time);
}

// Tells the scripts that the player changed the kind of device, and gives the new one and the
// old one. The handler is called whatever it is, a missing one is an error of the scripts.
void Lua_V1::changeInputMode(int mode, lua_Object func) {
	if (mode == _lastInputMode)
		return;

	lua_pushnumber((float)mode);
	lua_pushnumber((float)_lastInputMode);
	lua_callfunction(func);

	_lastInputMode = mode;
}

// The control update of the remastered. Besides the buttons and the axes it tells the scripts
// which kind of device is used, pings them every fifth call and reports a long press of the
// mouse button.
int Lua_V1::controlCallbackRemastered(float frameTime) {
	StdControl *stdControl = g_grim->_stdControl;
	ControlSymbol *symbols = g_grim->_sputControl->getSymbols();
	int numSymbols = g_grim->_sputControl->getNumSymbols();
	lua_Object buttonFunc;
	lua_Object buttonSelf;
	lua_Object axisFunc;
	lua_Object axisSelf;

	lua_beginblock();

	bool buttonHandler = getSystemHandler("buttonHandler", true, &buttonFunc, &buttonSelf);
	bool axisHandler = getSystemHandler("axisHandler", true, &axisFunc, &axisSelf);

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("inputModeHandler");
	lua_Object modeFunc = lua_gettable();

	_controlEventCount = (_controlEventCount + 1) % 5;

	for (int i = 0; i < numSymbols; i++) {
		ControlSymbol *symbol = &symbols[i];

		if (!(symbol->flags & 1))
			continue;

		if (symbol->flags & 4) {
			int pressCount = 0;
			int doubleClick = 0;
			int longPress = 0;
			int value = stdControl->readKey(symbol->code, &pressCount, &doubleClick, &longPress);

			if (symbol->code == StdControl::kKeyMousePing) {
				if (_controlEventCount == 0 && buttonHandler) {
					if (buttonSelf != LUA_NOOBJECT)
						lua_pushobject(buttonSelf);

					lua_pushnumber((float)i);
					lua_pushnumber(1);
					lua_pushnumber((float)(uint)pressCount);
					lua_pushnumber(0);
					lua_callfunction(buttonFunc);
				}
			} else if (symbol->code == StdControl::kKeyMouseLong) {
				if (value && buttonHandler && longPress) {
					if (buttonSelf != LUA_NOOBJECT)
						lua_pushobject(buttonSelf);

					lua_pushnumber((float)i);
					lua_pushnil();
					lua_pushnumber((float)(uint)pressCount);
					lua_pushnumber(0);
					lua_callfunction(buttonFunc);
				}
			} else if (value != symbol->lastValue && buttonHandler) {
				int mode = kInputModeKeys;

				if (symbol->code >= 256 && symbol->code <= 283)
					mode = kInputModePad;
				else if (symbol->code >= 284 && symbol->code <= 289)
					mode = kInputModeMouse;

				if (value)
					changeInputMode(mode, modeFunc);

				if (buttonSelf != LUA_NOOBJECT)
					lua_pushobject(buttonSelf);

				lua_pushnumber((float)i);

				if (value)
					lua_pushnumber(1);
				else
					lua_pushnil();

				lua_pushnumber((float)(uint)pressCount);
				lua_pushnumber((float)doubleClick);
				lua_callfunction(buttonFunc);
				symbol->lastValue = value;
			}
		} else {
			float value = stdControl->readAxis(symbol->code);
			float lastValue;

			if (symbol->flags & 8)
				value = -value;

			memcpy(&lastValue, &symbol->lastValue, sizeof(lastValue));

			if (value != lastValue && axisHandler) {
				changeInputMode(symbol->code <= 11 ? kInputModePad : kInputModeMouse, modeFunc);

				if (axisSelf != LUA_NOOBJECT)
					lua_pushobject(axisSelf);

				lua_pushnumber((float)i);
				lua_pushnumber(value);
				lua_callfunction(axisFunc);
				memcpy(&symbol->lastValue, &value, sizeof(value));
			}
		}
	}

	lua_endblock();

	return 0;
}

// The key mapping asks the scripts to take the result: 0 the key was taken, 1 it cannot be used,
// 2 the slot does not exist, 3 the key is in use, 4 the player cancelled
void Lua_V1::notifyKeyMapped(int result) {
	lua_beginblock();

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("NotifyKeyMapped");

	lua_Object func = lua_gettable();

	lua_pushnumber((float)result);
	lua_callfunction(func);

	lua_endblock();
}

} // end of namespace Grim
