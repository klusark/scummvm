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

#include "common/savefile.h"

#include "engines/grim/debug.h"
#include "engines/grim/grim.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/lua/lauxlib.h"

namespace Grim {

// Like the original, a sound is identified either by its name or by the number ImStartSound returned
static const char *getSoundKey(lua_Object nameObj) {
	// The demo always uses the name, ImStartSound doesn't return a number there
	if (!(g_grim->getGameFlags() & ADGF_DEMO) && lua_isnumber(nameObj))
		return (const char *)(intptr)luaFtolSse(lua_getnumber(nameObj));

	return lua_getstring(nameObj);
}

void Lua_V1::ImStartSound() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object priorityObj = lua_getparam(2);
	lua_Object groupObj = lua_getparam(3);

	if (!lua_isstring(nameObj) && !lua_isnil(nameObj))
		return;
	if (!lua_isnumber(priorityObj) || !lua_isnumber(groupObj))
		return;

	const char *soundName = lua_getstring(nameObj);
	int priority = luaFtolSse(lua_getnumber(priorityObj));
	int group = luaFtolSse(lua_getnumber(groupObj));

	int result = g_grim->getImuse()->soundsLoadStartSound(soundName, priority, group);

	// The demo doesn't return anything
	if (!(g_grim->getGameFlags() & ADGF_DEMO))
		lua_pushnumber(result);
}

void Lua_V1::ImStopSound() {
	lua_Object nameObj = lua_getparam(1);

	if ((g_grim->getGameFlags() & ADGF_DEMO) && !lua_isstring(nameObj) && !lua_isnil(nameObj)) {
		warning("Lua_V1::ImStopSound(): invalid filename");
		return;
	}

	g_grim->getImuse()->soundsStopSound(getSoundKey(nameObj));
}

void Lua_V1::ImStopAllSounds() {
	g_grim->getImuse()->imStopAllSounds();
}

void Lua_V1::ImPause() {
	g_grim->getImuse()->imPause();
}

void Lua_V1::ImResume() {
	g_grim->getImuse()->imResume();
}

void Lua_V1::ImSetVoiceEffect() {
	lua_Object effectObj = lua_getparam(1);

	if (!lua_isstring(effectObj) && !lua_isnil(effectObj))
		return;

	const char *effectName = lua_getstring(effectObj);
	if (!effectName) {
		warning("Lua_V1::ImSetVoiceEffect(): no effect name");
		return;
	}

	g_grim->getImuse()->iMUSESetVoiceEffect(effectName);
}

void Lua_V1::ImSetMusicVol() {
	lua_Object volObj = lua_getparam(1);
	if (!lua_isnumber(volObj))
		return;
	g_grim->getImuse()->setMusicGroupVol(luaFtolSse(lua_getnumber(volObj)));
}

void Lua_V1::ImGetMusicVol() {
	lua_pushnumber(g_grim->getImuse()->getMusicGroupVol());
}

void Lua_V1::ImSetVoiceVol() {
	lua_Object volObj = lua_getparam(1);
	if (!lua_isnumber(volObj))
		return;
	g_grim->getImuse()->setVoiceGroupVol(luaFtolSse(lua_getnumber(volObj)));
}

void Lua_V1::ImGetVoiceVol() {
	lua_pushnumber(g_grim->getImuse()->getVoiceGroupVol());
}

void Lua_V1::ImSetSfxVol() {
	lua_Object volObj = lua_getparam(1);
	if (!lua_isnumber(volObj))
		return;
	g_grim->getImuse()->setSFXGroupVol(luaFtolSse(lua_getnumber(volObj)));
}

void Lua_V1::ImGetSfxVol() {
	lua_pushnumber(g_grim->getImuse()->getSFXGroupVol());
}

void Lua_V1::ImSetParam() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object paramObj = lua_getparam(2);
	lua_Object valueObj = lua_getparam(3);

	if (g_grim->getGameFlags() & ADGF_DEMO) {
		if (!lua_isstring(nameObj) && !lua_isnil(nameObj)) {
			warning("Lua_V1::ImSetParam(): invalid filename");
			return;
		}
	} else if (!lua_isnumber(nameObj) && !lua_isstring(nameObj)) {
		return;
	}

	if (!lua_isnumber(paramObj) || !lua_isnumber(valueObj))
		return;

	int param = luaFtolSse(lua_getnumber(paramObj));
	int value = luaFtolSse(lua_getnumber(valueObj));
	if (value < 0 && !(g_grim->getGameFlags() & ADGF_DEMO))
		value = 0;

	g_grim->getImuse()->soundsSetParam(getSoundKey(nameObj), param, value);
}

void Lua_V1::ImGetParam() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object paramObj = lua_getparam(2);

	if (g_grim->getGameFlags() & ADGF_DEMO) {
		if ((!lua_isstring(nameObj) && !lua_isnil(nameObj)) || !lua_isnumber(paramObj)) {
			warning("Lua_V1::ImGetParam(): invalid filename or param");
			lua_pushnumber(-1.0);
			return;
		}
	} else if (!lua_isnumber(nameObj) && !lua_isstring(paramObj)) {
		lua_pushnumber(-1.0);
		return;
	}

	int param = luaFtolSse(lua_getnumber(paramObj));
	lua_pushnumber(g_grim->getImuse()->soundsGetParam(getSoundKey(nameObj), param));
}

void Lua_V1::ImFadeParam() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object opcodeObj = lua_getparam(2);
	lua_Object valueObj = lua_getparam(3);
	lua_Object durationObj = lua_getparam(4);

	bool demo = g_grim->getGameFlags() & ADGF_DEMO;

	if (demo) {
		if (!lua_isstring(nameObj) && !lua_isnil(nameObj)) {
			warning("Lua_V1::ImFadeParam(): invalid filename");
			return;
		}
	} else if (!lua_isstring(nameObj) && !lua_isnumber(nameObj)) {
		lua_pushnumber(0);
		return;
	}
	if (!lua_isnumber(opcodeObj) || !lua_isnumber(valueObj) || !lua_isnumber(durationObj))
		return;

	int opcode = luaFtolSse(lua_getnumber(opcodeObj));
	int value = luaFtolSse(lua_getnumber(valueObj));
	if (value < 0 && !demo)
		value = 0;
	int duration = luaFtolSse(lua_getnumber(durationObj));

	g_grim->getImuse()->soundsFadeParam(getSoundKey(nameObj), opcode, value, duration);
}

void Lua_V1::ImSetState() {
	lua_Object stateObj = lua_getparam(1);
	if (!lua_isnumber(stateObj))
		return;

	// The demo sets the state right away and returns the new one
	if (g_grim->getGameFlags() & ADGF_DEMO) {
		lua_pushnumber(g_grim->getImuse()->imSetState(luaFtolSse(lua_getnumber(stateObj))));
		return;
	}

	g_grim->setImuseState(luaFtolSse(lua_getnumber(stateObj)));
}

void Lua_V1::ImSetSequence() {
	lua_Object stateObj = lua_getparam(1);
	if (!lua_isnumber(stateObj))
		return;

	int state = luaFtolSse(lua_getnumber(stateObj));
	lua_pushnumber(g_grim->getImuse()->imSetSequence(state));
}

void Lua_V1::SaveIMuse() {
	g_grim->imuseSave();
}

void Lua_V1::RestoreIMuse() {
	g_grim->imuseRestore();
}

void Lua_V1::SetSoundPosition() {
	RdVector3 pos = { 0.0f, 0.0f, 0.0f };
	byte minVol = 10;
	byte maxVol = 127;
	float maxDist = 8.0f;
	float panFactor = 1.5f;
	float maxRadius = 1.0f;
	int argId = 3;
	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (room)
		g_grim->_sputRoom->getSoundParameters(room, &maxDist, &minVol, &maxVol, &panFactor, &maxRadius);

	lua_Object nameObj = lua_getparam(1);
	if (!lua_isnumber(nameObj) && !lua_isstring(nameObj))
		return;

	lua_Object actorObj = lua_getparam(2);
	if (isSputActor(actorObj)) {
		SputActor *actor = getSputActor(actorObj);
		if (!actor)
			return;

		pos[0] = actor->actorPos[0];
		pos[1] = actor->actorPos[1];
		pos[2] = actor->actorPos[2];
	} else if (lua_isnumber(actorObj)) {
		argId = 5;
		pos[0] = lua_getnumber(actorObj);
		pos[1] = lua_getnumber(lua_getparam(3));
		pos[2] = lua_getnumber(lua_getparam(4));
	}

	lua_Object minVolObj = lua_getparam(argId);
	lua_Object maxVolObj = lua_getparam(argId + 1);
	lua_Object maxDistObj = lua_getparam(argId + 2);

	if (lua_isnumber(minVolObj)) {
		minVol = (byte)luaFtolSse(lua_getnumber(minVolObj));
		if (minVol > 127)
			minVol = 127;
	}

	if (lua_isnumber(maxVolObj)) {
		maxVol = (byte)luaFtolSse(lua_getnumber(maxVolObj));
		if (maxVol > 127)
			maxVol = 127;
		else if (maxVol < minVol)
			maxVol = minVol;
	}

	if (lua_isnumber(maxDistObj)) {
		maxDist = lua_getnumber(maxDistObj);
		if (maxDist < 0.0f)
			maxDist = 0.0f;
	}

	g_grim->_sputRoom->setSoundPosition(getSoundKey(nameObj), pos, minVol, maxVol, maxDist, 0.8f);
}

void Lua_V1::SetSoundPositionDemo() {
	RdVector3 pos = { 0.0f, 0.0f, 0.0f };
	RdVector3 delta;
	int screen[3];
	int argId = 3;
	int minVol = 10;
	int maxVol = 127;
	int pan = 64;
	float maxDist = 8.0f;

	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj)) {
		warning("SetSoundPosition: first argument (filename) must be a string.");
		return;
	}

	lua_Object actorObj = lua_getparam(2);

	if (isSputActor(actorObj)) {
		SputActor *actor = getSputActor(actorObj);

		if (!actor) {
			warning("SetSoundPosition: invalid actor handle.");
			return;
		}

		pos[0] = actor->actorPos[0];
		pos[1] = actor->actorPos[1];
		pos[2] = actor->actorPos[2];
	} else if (lua_isnumber(actorObj)) {
		argId = 5;
		pos[0] = lua_getnumber(actorObj);
		pos[1] = lua_getnumber(lua_getparam(3));
		pos[2] = lua_getnumber(lua_getparam(4));
	}

	lua_Object minVolObj = lua_getparam(argId);
	lua_Object maxVolObj = lua_getparam(argId + 1);
	lua_Object maxDistObj = lua_getparam(argId + 2);

	if (lua_isnumber(minVolObj)) {
		minVol = luaFtolSse(lua_getnumber(minVolObj));
		if (minVol > 127)
			minVol = 127;
	}

	if (lua_isnumber(maxVolObj)) {
		int value = luaFtolSse(lua_getnumber(maxVolObj));

		if ((uint)value > 127) {
			maxVol = 127;
		} else {
			maxVol = value < minVol ? minVol : value;
		}
	}

	if (lua_isnumber(maxDistObj)) {
		maxDist = lua_getnumber(maxDistObj);
		if (maxDist < 0.0f)
			maxDist = 0.0f;
	}

	Room *room = g_grim->_sputRoom->getCurrentRoom();
	int volume = maxVol;

	if (room) {
		if (g_grim->_rd->_primit3.getScreenCoord(pos, screen)) {
			pan = screen[0] / 5;
			if ((uint)pan > 127)
				pan = 127;
		}

		const float *cameraPos = room->setups[room->curSetup].camera.position;

		delta[0] = pos[0] - cameraPos[0];
		delta[1] = pos[1] - cameraPos[1];
		delta[2] = pos[2] - cameraPos[2];

		float distance = g_grim->_rd->normalize3Acc(delta);
		double minVolume = (double)minVol;
		double scaled = minVolume;

		// With maxDist 0 the EXE divides by zero and ends up on the minimum
		if (maxDist != 0.0f)
			scaled = (double)(maxVol - minVol) / maxDist * (maxDist - distance) + minVolume;

		if (scaled >= minVolume) {
			if (scaled > (double)maxVol)
				scaled = (double)maxVol;

			volume = (int)scaled;
		} else {
			volume = minVol;
		}
	}

	const char *name = lua_getstring(nameObj);

	g_grim->getImuse()->soundsSetParam(name, IMUSE_P_VOLUME, volume);
	g_grim->getImuse()->soundsSetParam(name, IMUSE_P_PAN, pan);
}

void Lua_V1::IsSoundPlaying() {
	// dummy
}

void Lua_V1::PlaySoundAt() {
	// dummy
}

void Lua_V1::LoadBundle() {
	// loading grimdemo.mus is already handled
}

void Lua_V1::LoadBundleDemo() {
	lua_Object nameObj = lua_getparam(1);
	char path[128];

	// The EXE lets nil through and then reads it as a string
	if (!lua_isstring(nameObj)) {
		warning("LUA LoadBundle: invalid filename.");
		return;
	}

	if (g_grim->_res->findExternalFile(lua_getstring(nameObj), path, sizeof(path)) == 1) {
		lua_pushnumber((float)g_grim->_res->fileOpen(path));
	} else {
		warning("Unable to find music bundle '%s'!", lua_getstring(nameObj));
		lua_pushnil();
	}
}

void Lua_V1::UnloadBundleDemo() {
	lua_Object nameObj = lua_getparam(1);
	char path[128];

	// The EXE lets nil through and then reads it as a string
	if (!lua_isstring(nameObj)) {
		warning("LUA UnLoadBundle: invalid filename.");
		return;
	}

	if (g_grim->_res->findExternalFile(lua_getstring(nameObj), path, sizeof(path)) == 1) {
		lua_pushnumber((float)g_grim->_res->fileClose(path));
	} else {
		warning("Unable to unload music bundle '%s'!", lua_getstring(nameObj));
		lua_pushnil();
	}
}

void Lua_V1::PlaySound() {
	// dummy
}

} // end of namespace Grim
