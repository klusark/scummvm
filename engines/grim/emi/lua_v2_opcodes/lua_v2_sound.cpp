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

#include "common/endian.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lstring.h"
#include "engines/grim/emi/input/demoreplay.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/sound/sounds.h"

namespace Grim {

static const int32 kSoundTag = (int32)MKTAG(' ', 'D', 'N', 'S');
static const int kSfxSoundMode = 1;
static const int kDefaultSoundVolume = 100;

void Lua_V2::PlaySound() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object volumeObj = lua_getparam(2);

	if (!lua_isstring(nameObj)) {
		logMessage(kConsoleStatus, "PlaySound: 1st argument must be a string.");

		return;
	}

	LString soundName = lua_getstring(nameObj);

	// The demo keeps the name as the script gave it
	if (!_engine->isDemo())
		LStringUtil::stringToLower(soundName);

	if (lua_isnumber(volumeObj))
		_engine->getSounds()->playSound(soundName.c_str(), luaFtol(lua_getnumber(volumeObj)));
	else
		_engine->getSounds()->playSound(soundName.c_str(), kDefaultSoundVolume);
}

void Lua_V2::StopSound() {
	lua_Object soundObj = lua_getparam(1);

	if (lua_isuserdata(soundObj) && lua_tag(soundObj) == kSoundTag)
		_engine->getSounds()->stopSoundPlayback((uint)lua_getuserdata(soundObj));
	else
		logMessage(kConsoleWarning, "StopSound: 1st argument must be hSound!\n");
}

void Lua_V2::StopAllSounds() {
	// The demo has no script-locked sound list
	if (!_engine->isDemo()) {
		for (uint i = 0; i < _lockedSounds.size(); i++)
			_engine->getSounds()->stopSound(_lockedSounds[i]);

		_lockedSounds.clear();
	}

	_engine->getSounds()->stopAllSounds();
}

void Lua_V2::LoadSound() {
	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj)) {
		logMessage(kConsoleStatus, "LoadSound: 1st argument must be a string.");

		return;
	}

	LString soundName = lua_getstring(nameObj);

	if (_engine->isDemo()) {
		// The demo keeps the name as given and always pushes a sound tag, even for handle 0
		uint demoHandle = _engine->getSounds()->loadSound(soundName.c_str(), kSfxSoundMode);

		lua_pushusertag((int32)demoHandle, kSoundTag);

		return;
	}

	LStringUtil::stringToLower(soundName);

	uint soundHandle = _engine->getSounds()->loadSound(soundName.c_str(), kSfxSoundMode);

	if (soundHandle != _engine->getSounds()->getLoadFailure()) {
		_lockedSounds.push_back(soundHandle);
		lua_pushusertag((int32)soundHandle, kSoundTag);
	} else {
		lua_pushnil();
	}
}

void Lua_V2::FreeSound() {
	lua_Object soundObj = lua_getparam(1);

	if (!lua_isuserdata(soundObj) || lua_tag(soundObj) != kSoundTag) {
		logMessage(kConsoleWarning, "FreeSound: 1st argument must be hSound!\n");

		return;
	}

	uint soundHandle = (uint)lua_getuserdata(soundObj);

	for (uint i = 0; i < _lockedSounds.size(); i++) {
		if (_lockedSounds[i] == soundHandle) {
			_lockedSounds.remove_at(i);

			break;
		}
	}

	_engine->getSounds()->stopSound(soundHandle);
}

void Lua_V2::PlayLoadedSound() {
	lua_Object soundObj = lua_getparam(1);
	lua_Object loopObj = lua_getparam(2);
	lua_Object volumeObj = lua_getparam(3);
	lua_Object releaseObj = lua_getparam(4);

	if (!lua_isuserdata(soundObj) || lua_tag(soundObj) != kSoundTag) {
		logMessage(kConsoleWarning, "PlayLoadedSound: 1st argument must be hSound!\n");

		return;
	}

	uint soundHandle = (uint)lua_getuserdata(soundObj);
	bool loop = lua_isnil(loopObj) == 0;
	bool release = lua_isnil(releaseObj) != 0;
	int volume = kDefaultSoundVolume;

	if (lua_isnumber(volumeObj))
		volume = luaFtol(lua_getnumber(volumeObj));

	_engine->getSounds()->startSound(soundHandle, release ? 1 : 0, loop, volume);

	if (release && !loop) {
		if (_engine->isDemo())
			_engine->getSounds()->releaseSoundWhenDone(soundHandle);
		else
			_engine->getSounds()->unregisterSound(soundHandle);
	}
}

void Lua_V2::IsSoundPlaying() {
	lua_Object soundObj = lua_getparam(1);

	if (!lua_isuserdata(soundObj) || lua_tag(soundObj) != kSoundTag) {
		logMessage(kConsoleWarning, "IsSoundPlaying: 1st argument must be hSound!\n");

		return;
	}

	uint soundHandle = (uint)lua_getuserdata(soundObj);
	bool isPlaying;

	// The demo channels have no finite loop count test
	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		isPlaying = _engine->getSounds()->isSoundPlayingPs2(soundHandle, "IsSoundPlaying");
	else if (_engine->isDemo())
		isPlaying = _engine->getSounds()->isSoundPlaying(soundHandle);
	else
		isPlaying = _engine->getSounds()->isSoundPlayingNoFiniteLoop(soundHandle);

	if (isPlaying)
		lua_pushnumber(1.0f);
}

void Lua_V2::GetGroupVolume() {
	lua_Object groupObj = lua_getparam(1);

	if (lua_isnumber(groupObj)) {
		int volume = _engine->getSounds()->getCategoryVolume(luaFtol(lua_getnumber(groupObj)));

		lua_pushnumber((float)volume);
	} else {
		logMessage(kConsoleWarning, "GetGroupVolume: 1st argument must be a number!\n");
	}
}

void Lua_V2::SetGroupVolume() {
	lua_Object groupObj = lua_getparam(1);
	lua_Object volumeObj = lua_getparam(2);

	if (!lua_isnumber(groupObj)) {
		logMessage(kConsoleWarning, "SetGroupVolume: 1st argument must be a number!\n");

		return;
	}

	int group = luaFtol(lua_getnumber(groupObj));
	int volume = kDefaultSoundVolume;

	if (lua_isnumber(volumeObj))
		volume = luaFtol(lua_getnumber(volumeObj));

	_engine->getSounds()->setCategoryVolume(group, volume);
}

void Lua_V2::GetSoundVolume() {
	lua_Object soundObj = lua_getparam(1);

	if (!lua_isuserdata(soundObj) || lua_tag(soundObj) != kSoundTag) {
		logMessage(kConsoleWarning, "GetSoundVolume: 1st argument must be hSound!\n");

		return;
	}

	int volume = 0;

	if (_engine->getSounds()->getSoundVolume((uint)lua_getuserdata(soundObj), &volume))
		lua_pushnumber((float)volume);
}

void Lua_V2::SetSoundVolume() {
	lua_Object soundObj = lua_getparam(1);
	lua_Object volumeObj = lua_getparam(2);

	if (!lua_isuserdata(soundObj) || lua_tag(soundObj) != kSoundTag) {
		logMessage(kConsoleWarning, "SetSoundVolume: 1st argument must be hSound!\n");

		return;
	}

	uint soundHandle = (uint)lua_getuserdata(soundObj);
	int volume = kDefaultSoundVolume;

	if (lua_isnumber(volumeObj))
		volume = luaFtol(lua_getnumber(volumeObj));

	_engine->getSounds()->setSoundVolume(soundHandle, volume);
}

void Lua_V2::EnableAudioGroup() {
	lua_Object groupObj = lua_getparam(1);
	lua_Object enabledObj = lua_getparam(2);

	if (!lua_isnumber(groupObj)) {
		logMessage(kConsoleWarning, "EnableAudioGroup: 1st argument must be a number!\n");

		return;
	}

	int group = luaFtol(lua_getnumber(groupObj));
	bool enabled = lua_isnil(enabledObj) == 0;

	_engine->getSounds()->setCategoryEnabled(group, enabled);
}

void Lua_V2::EnableVoiceFX() {
	lua_Object enabledObj = lua_getparam(1);

	// The disasm passes the stack slot with only its low byte written
	_engine->getSounds()->setVoiceFxEnabled(lua_isnil(enabledObj) ? 0 : 1);
}

void Lua_V2::SetAudioMixAhead() {
	lua_Object mixAheadObj = lua_getparam(1);

	if (lua_isnumber(mixAheadObj)) {
		int16 mixAhead = (int16)luaFtol(lua_getnumber(mixAheadObj));

		_engine->getSounds()->setAudioMixAhead(&mixAhead, true);
	} else {
		logMessage(kConsoleStatus, "SetAudioMixAhead: argument must be a number.");
	}
}

void Lua_V2::PlaySoundFrom() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	lua_Object volumeObj = lua_getparam(5);

	if (!lua_isstring(nameObj)) {
		logMessage(kConsoleStatus, "PlaySoundFrom: 1st argument must be a string.");

		return;
	}

	LString soundName = lua_getstring(nameObj);

	LStringUtil::stringToLower(soundName);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		logMessage(kConsoleWarning, "PlaySoundFrom: Must supply all three coordinates.\n");

		return;
	}

	float x = lua_getnumber(xObj);
	float y = lua_getnumber(yObj);
	float z = lua_getnumber(zObj);
	int volume = kDefaultSoundVolume;

	if (lua_isnumber(volumeObj))
		volume = luaFtol(lua_getnumber(volumeObj));

	_engine->getSounds()->playSoundFrom(soundName.c_str(), x, y, z, (int16)volume);
}

void Lua_V2::PlayLoadedSoundFrom() {
	lua_Object soundObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	lua_Object loopObj = lua_getparam(5);
	lua_Object volumeObj = lua_getparam(6);

	if (!lua_isuserdata(soundObj) || lua_tag(soundObj) != kSoundTag) {
		logMessage(kConsoleWarning, "PlayLoadedSoundFrom: 1st argument must be hSound!\n");

		return;
	}

	uint soundHandle = (uint)lua_getuserdata(soundObj);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		logMessage(kConsoleWarning, "PlayLoadedSoundFrom: Must supply all three coordinates.\n");

		return;
	}

	float x = lua_getnumber(xObj);
	float y = lua_getnumber(yObj);
	float z = lua_getnumber(zObj);
	bool loop = lua_isnil(loopObj) == 0;
	int volume = kDefaultSoundVolume;

	if (lua_isnumber(volumeObj))
		volume = luaFtol(lua_getnumber(volumeObj));

	_engine->getSounds()->playLoadedSoundFrom(soundHandle, x, y, z, loop, (int16)volume);
}

void Lua_V2::SetReverb() {
	static const int reverbPresets[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
		13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
	};

	// PS2 maps the script parameters to the SPU2 room type, depth (mix), delay (predelay) and feedback (decay)
	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		lua_Object typeObj = lua_getparam(1);
		lua_Object depthObj = lua_getparam(3);
		lua_Object feedbackObj = lua_getparam(2);
		lua_Object delayObj = lua_getparam(4);
		int type = 0;
		int depth = 0;
		int delay = 0;
		int feedback = 0;

		if (lua_isnumber(typeObj))
			type = luaFtol(lua_getnumber(typeObj));

		if (lua_isnumber(depthObj))
			depth = luaFtol(lua_getnumber(depthObj));

		if (lua_isnumber(delayObj))
			delay = luaFtol(lua_getnumber(delayObj));

		if (lua_isnumber(feedbackObj))
			feedback = luaFtol(lua_getnumber(feedbackObj));

		_engine->getSounds()->setSonyReverb(type, depth, delay, feedback);

		return;
	}

	lua_Object environmentObj = lua_getparam(1);
	lua_Object paramObjs[5];
	float params[5];

	for (int i = 0; i < 5; i++) {
		paramObjs[i] = lua_getparam(i + 2);
		params[i] = -1.0f;
	}

	if (!lua_isnumber(environmentObj))
		return;

	int environment = luaFtol(lua_getnumber(environmentObj));
	int preset;

	if (environment == 60)
		preset = 26;
	else if (environment == 70)
		preset = 27;
	else if (environment < 0 || environment > 25)
		return;
	else
		preset = reverbPresets[environment];

	for (int i = 0; i < 5; i++) {
		if (lua_isnumber(paramObjs[i]))
			params[i] = lua_getnumber(paramObjs[i]);
	}

	_engine->getSounds()->setReverb(preset, &params[0], &params[1], &params[2], &params[3], &params[4]);
}

void Lua_V2::UpdateSoundPosition() {
	lua_Object soundObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);

	if (!lua_isuserdata(soundObj) || lua_tag(soundObj) != kSoundTag) {
		logMessage(kConsoleWarning, "SetSoundVolume: 1st argument must be hSound!\n");

		return;
	}

	uint soundHandle = (uint)lua_getuserdata(soundObj);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		logMessage(kConsoleWarning, "UpdateSoundPosition: Must supply all three coordinates.\n");

		return;
	}

	float x = lua_getnumber(xObj);
	float y = lua_getnumber(yObj);
	float z = lua_getnumber(zObj);

	_engine->getSounds()->setSoundPosition(soundHandle, x, y, z);
}

void Lua_V2::ImSetState() {
	lua_Object stateObj = lua_getparam(1);

	if (lua_isnumber(stateObj))
		_engine->setPendingMusicState(luaFtol(lua_getnumber(stateObj)));
	else
		logMessage(kConsoleDebug, "LUA ImSetState: invalid param.\n");
}

void Lua_V2::ImPause() {
	_engine->getSounds()->pauseMusicStreams();
}

void Lua_V2::ImResume() {
	_engine->getSounds()->resumeMusicStreams();
}

void Lua_V2::ImSelectSet() {
	// PS2 has no music sets
	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return;

	lua_Object setObj = lua_getparam(1);

	if (lua_isnumber(setObj))
		_engine->getSounds()->selectMusicSet((int16)luaFtol(lua_getnumber(setObj)));
}

void Lua_V2::ImStateHasLooped() {
	lua_Object musicObj = lua_getparam(1);

	if (!lua_isnumber(musicObj)) {
		logMessage(kConsoleDebug, "LUA ImStateHasLooped: invalid param.\n");

		return;
	}

	uint musicId = (uint)luaFtol(lua_getnumber(musicObj));
	bool looped = false;
	bool ended = false;

	// The disasm leaves both flags uninitialised when the music is not playing
	_engine->getSounds()->consumeMusicFlags(musicId, looped, ended);

	if (looped)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::ImStateHasEnded() {
	lua_Object musicObj = lua_getparam(1);

	if (!lua_isnumber(musicObj)) {
		logMessage(kConsoleDebug, "LUA ImStateHasEnded: invalid param.\n");

		return;
	}

	uint musicId = (uint)luaFtol(lua_getnumber(musicObj));
	bool looped = false;
	bool ended = false;

	// The disasm leaves both flags uninitialised when the music is not playing
	_engine->getSounds()->consumeMusicFlags(musicId, looped, ended);

	if (ended)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::ImPushState() {
	lua_Object musicObj = lua_getparam(1);
	lua_Object flagObj = lua_getparam(2);

	if (!lua_isnumber(musicObj)) {
		logMessage(kConsoleDebug, "ImPushState: invalid param.\n");

		return;
	}

	bool flag = lua_isnil(flagObj) == 0;

	_engine->getSounds()->pushMusicState(luaFtol(lua_getnumber(musicObj)), flag);
}

void Lua_V2::ImPopState() {
	DemoReplay *replay = _engine->getDemoReplay();

	if (replay && replay->_mode == DemoReplay::kModePlayback) {
		_engine->setPendingMusicState(_engine->getSounds()->takePushedMusicState());

		return;
	}

	lua_Object flagObj = lua_getparam(1);
	bool flag = lua_isnil(flagObj) == 0;

	_engine->getSounds()->popMusicState(flag);
}

void Lua_V2::ImFlushStack() {
	_engine->getSounds()->flushMusicStateStack();
}

void Lua_V2::ImGetMillisecondPosition() {
	lua_Object musicObj = lua_getparam(1);

	if (!lua_isnumber(musicObj)) {
		logMessage(kConsoleDebug, "ImGetMillisecondPosition: invalid param.\n");

		return;
	}

	uint musicId = (uint)luaFtol(lua_getnumber(musicObj));
	int position = _engine->getSounds()->getMusicPositionMs(musicId);

	lua_pushnumber((float)position);
}

} // end of namespace Grim
