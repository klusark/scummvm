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
#include "engines/grim/wuregistry.h"

namespace Grim {

enum {
	kImuseGroupCommentary = 5,
	kActorFlagBlackAndWhite = 0x40,
	kActorModeHkHack = 4
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

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_getnumber(modeObj) > 0.0f) {
		actor->flags |= kActorFlagBlackAndWhite;
	} else if (actor->flags & kActorFlagBlackAndWhite) {
		actor->flags &= ~kActorFlagBlackAndWhite;

		if (actor->extra && actor->extra->puck)
			actor->extra->puck->rotate(180.0f);
		else
			warning("Lua_V1::SetActorBWMode(): the actor has no puck to turn around");
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

	if (lua_getnumber(modeObj) > 0.0f)
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
	int enable = luaFtol(lua_getnumber(lua_getparam(1)));

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

	if (g_grim->getLibMode() == GrimEngine::kLibModeUser)
		return;

	_resolutionScaling = lua_getnumber(valueObj);
}

void Lua_V1::SetAdvancedLighting() {
	lua_Object valueObj = lua_getparam(1);

	if (g_grim->getLibMode() == GrimEngine::kLibModeUser)
		return;

	_advancedLighting = lua_getnumber(valueObj);
}

// The menu and the user modes pause the movie, and leaving them draws the scene again
void Lua_V1::SetGameRenderMode() {
	lua_Object modeObj = lua_getparam(1);

	if (!lua_isnumber(modeObj)) {
		warning("Lua_V1::SetGameRenderMode(): the mode is not a number");
		return;
	}

	int mode = luaFtol(lua_getnumber(modeObj));
	int oldMode = g_grim->getLibMode();
	bool wasPaused = oldMode == GrimEngine::kLibModeUser || oldMode == GrimEngine::kLibModeMenu;
	bool isPaused = mode == GrimEngine::kLibModeUser || mode == GrimEngine::kLibModeMenu;

	if (isPaused && !wasPaused) {
		g_grim->_sputSmush->pause(1);
	} else if (!isPaused && wasPaused) {
		g_grim->_sputSmush->pause(0);
		g_grim->_sputRender->forceRefresh();
	}

	g_grim->setLibMode(mode);
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

} // end of namespace Grim
