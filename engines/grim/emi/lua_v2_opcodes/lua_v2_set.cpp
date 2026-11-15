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

#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/emi/graphics/background.h"
#include "engines/grim/emi/graphics/tiledscene.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/lua/luadebug.h"

namespace Grim {

void Lua_V2::Exit() {
	lua_Object codeObj = lua_getparam(1);
	int code = 0;

	if (lua_isnumber(codeObj))
		code = luaFtol(lua_getnumber(codeObj));

	logMessage(kConsoleDebug, "Exit called from scripts with code %d!  Shutting down.\n", code);

	_engine->requestShutdown(code);
}

void Lua_V2::FunctionName() {
	const char *name = nullptr;
	const char *filename = nullptr;
	int32 line = 0;
	char buffer[256];
	lua_Object functionObj = lua_getparam(1);

	if (!lua_isfunction(functionObj)) {
		logMessage(kConsoleStatus, "FunctionName: Parameter must be a function.\n");

		return;
	}

	lua_funcinfo(functionObj, &filename, &line);

	char functionType = *lua_getobjname(functionObj, &name);
	const char *typeName = "unnamed function";
	const char *functionName = "<unknown>";

	if (functionType == 'g') {
		typeName = "function";
		functionName = name;
	} else if (functionType == 't') {
		typeName = "tag method";
		functionName = name;
	}

	Common::sprintf_s(buffer, "%s '%s' (%s:%d)", typeName, functionName, filename, (int)line);
	lua_pushstring(buffer);
}

void Lua_V2::PrintMessage() {
	lua_Object messageObj = lua_getparam(1);

	if (!lua_isnil(messageObj) && lua_isstring(messageObj)) {
		logMessage(kConsoleStatus, "%s\n", lua_getstring(messageObj));
	} else if (!lua_isnil(messageObj)) {
		logMessage(kConsoleStatus, "MessagePrint takes only a single string argument!");
	} else {
		logMessage(kConsoleStatus, "(nil)");
	}
}

void Lua_V2::PrintError() {
	lua_Object messageObj = lua_getparam(1);

	if (lua_isnil(messageObj))
		logMessage(kConsoleError, "(nil)");
	else if (lua_isstring(messageObj))
		logMessage(kConsoleError, "%s\n", lua_getstring(messageObj));
	else
		logMessage(kConsoleError, "ErrorPrint takes only a single string argument!");
}

void Lua_V2::PrintWarning() {
	lua_Object messageObj = lua_getparam(1);

	if (!lua_isnil(messageObj) && lua_isstring(messageObj)) {
		logMessage(kConsoleWarning, "%s\n", lua_getstring(messageObj));
	} else if (!lua_isnil(messageObj)) {
		logMessage(kConsoleWarning, "WarningPrint takes only a single string argument!");
	} else {
		logMessage(kConsoleWarning, "(nil)");
	}
}

void Lua_V2::PrintDebug() {
	lua_Object messageObj = lua_getparam(1);

	if (!lua_isnil(messageObj) && lua_isstring(messageObj)) {
		logMessage(kConsoleDebug, "%s\n", lua_getstring(messageObj));
	} else if (!lua_isnil(messageObj)) {
		logMessage(kConsoleDebug, "DebugPrint takes only a single string argument!");
	} else {
		logMessage(kConsoleDebug, "(nil)");
	}
}

void Lua_V2::MakeCurrentSet() {
	lua_Object roomObj = lua_getparam(1);

	if (lua_isstring(roomObj)) {
		// The demo logs the set name and leaves the loaded sounds alone
		if (_engine->isDemo()) {
			logMessage(kConsoleStatus, "Making the current set %s.\n", lua_getstring(roomObj));
		} else {
			for (uint i = 0; i < _lockedSounds.size(); i++)
				_engine->getSounds()->stopSound(_lockedSounds[i]);

			_lockedSounds.clear();
		}

		Room_v2::loadRoom(_engine, lua_getstring(roomObj));
	} else {
		logMessage(kConsoleWarning, "Must pass a string to MakeCurrentSet!\n");
		Room_v2::loadRoom(_engine, nullptr);
	}
}

void Lua_V2::LockSet() {
	lua_Object roomObj = lua_getparam(1);

	if (!lua_isstring(roomObj))
		return;

	LString roomName = lua_getstring(roomObj);
	LResourceHandle<Room_v2> roomHandle(_engine, roomName);

	if (roomHandle.lock() && roomHandle._typed) {
		roomHandle._typed->_lockCount++;
		_engine->getResourceManager()->lockResource(roomHandle._typed);

		// The demo keeps no list of locked names
		if (!_engine->isDemo())
			_lockedRooms.push_back(roomName);
	}

	roomHandle.unlock();
}

void Lua_V2::UnLockSet() {
	lua_Object roomObj = lua_getparam(1);

	if (!lua_isstring(roomObj))
		return;

	LString roomName = lua_getstring(roomObj);
	LResourceHandle<Room_v2> roomHandle(_engine, roomName);

	if (roomHandle.lock() && roomHandle._typed) {
		Room_v2 *room = roomHandle._typed;

		if (_engine->getGamePlatform() == Common::kPlatformPS2 && room->_lockCount <= 0) {
			logMessage(kConsoleError, "Attempted to unlock a non-locked resource (%s) !\n", roomName.c_str());
		} else {
			room->_lockCount--;
			_engine->getResourceManager()->unlockResource(room);

			if (room->_lockCount == 0 && room->_purgeOnFree)
				_engine->getResourceManager()->purgeResource(room, true);
		}

		for (uint i = 0; !_engine->isDemo() && i < _lockedRooms.size(); i++) {
			if (_lockedRooms[i] == roomName) {
				_lockedRooms.remove_at(i);

				break;
			}
		}
	} else {
		logMessage(kConsoleWarning, "Attempted to unlock non-existent set %s!\n", roomName.c_str());
	}

	roomHandle.unlock();
}

void Lua_V2::LockBackground() {
	lua_Object backgroundObj = lua_getparam(1);

	if (!lua_isstring(backgroundObj))
		return;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		lockBackgroundWithScene(lua_getstring(backgroundObj), true);

		return;
	}

	LString backgroundName = "BG";

	backgroundName += lua_getstring(backgroundObj);

	LResourceHandle<Background> backgroundHandle(_engine, backgroundName);

	if (backgroundHandle.lock() && backgroundHandle._typed) {
		backgroundHandle._typed->_lockCount++;
		_engine->getResourceManager()->lockResource(backgroundHandle._typed);

		if (!_engine->isDemo())
			_lockedBackgrounds.push_back(backgroundName);
	}

	backgroundHandle.unlock();
}

void Lua_V2::UnLockBackground() {
	lua_Object backgroundObj = lua_getparam(1);

	if (!lua_isstring(backgroundObj))
		return;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		unlockBackgroundWithScene(lua_getstring(backgroundObj), true);

		return;
	}

	LString backgroundName = "BG";

	backgroundName += lua_getstring(backgroundObj);

	LResourceHandle<Background> backgroundHandle(_engine, backgroundName);

	if (backgroundHandle.lock() && backgroundHandle._typed) {
		Background *background = backgroundHandle._typed;

		background->_lockCount--;
		_engine->getResourceManager()->unlockResource(background);

		if (background->_lockCount == 0 && background->_purgeOnFree)
			_engine->getResourceManager()->purgeResource(background, true);

		for (uint i = 0; !_engine->isDemo() && i < _lockedBackgrounds.size(); i++) {
			if (_lockedBackgrounds[i] == backgroundName) {
				_lockedBackgrounds.remove_at(i);

				break;
			}
		}
	} else if (_engine->isDemo()) {
		logMessage(kConsoleWarning, "UnLockBackground: the background %s does not exist.\n", backgroundName.c_str());
	}

	backgroundHandle.unlock();
}

void Lua_V2::lockBackgroundWithScene(const LString &name, bool recordName) {
	LString backgroundName = "BG";

	backgroundName += name;

	LResourceHandle<Background> backgroundHandle(_engine, backgroundName);

	if (backgroundHandle.lock() && backgroundHandle._typed) {
		backgroundHandle._typed->_lockCount++;
		_engine->getResourceManager()->lockResource(backgroundHandle._typed);

		if (recordName)
			_lockedBackgrounds.push_back(name);
	}

	backgroundHandle.unlock();

	LString sceneName = "TS";

	sceneName += name;

	LResourceHandle<TiledScene> sceneHandle(_engine, sceneName);

	if (sceneHandle.lock() && sceneHandle._typed) {
		sceneHandle._typed->_lockCount++;
		_engine->getResourceManager()->lockResource(sceneHandle._typed);
	}

	sceneHandle.unlock();
}

void Lua_V2::unlockBackgroundWithScene(const LString &name, bool forgetName) {
	LString backgroundName = "BG";

	backgroundName += name;

	LResourceHandle<Background> backgroundHandle(_engine, backgroundName);

	if (backgroundHandle.lock() && backgroundHandle._typed) {
		Background *background = backgroundHandle._typed;

		if (background->_lockCount <= 0) {
			logMessage(kConsoleError, "Attempted to unlock a non-locked resource (%s) !\n", backgroundName.c_str());
		} else {
			background->_lockCount--;
			_engine->getResourceManager()->unlockResource(background);

			if (background->_lockCount == 0 && background->_purgeOnFree)
				_engine->getResourceManager()->purgeResource(background, true);
		}

		for (uint i = 0; forgetName && i < _lockedBackgrounds.size(); i++) {
			if (_lockedBackgrounds[i] == name) {
				_lockedBackgrounds.remove_at(i);

				break;
			}
		}
	}

	backgroundHandle.unlock();

	LString sceneName = "TS";

	sceneName += name;

	LResourceHandle<TiledScene> sceneHandle(_engine, sceneName);

	if (sceneHandle.lock() && sceneHandle._typed) {
		TiledScene *scene = sceneHandle._typed;

		if (scene->_lockCount <= 0) {
			logMessage(kConsoleError, "Attempted to unlock a non-locked resource (%s) !\n", sceneName.c_str());
		} else {
			scene->_lockCount--;
			_engine->getResourceManager()->unlockResource(scene);

			if (scene->_lockCount == 0 && scene->_purgeOnFree)
				_engine->getResourceManager()->purgeResource(scene, true);
		}
	}

	sceneHandle.unlock();
}

void Lua_V2::MakeCurrentSetup() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logMessage(kConsoleStatus, "MakeCurrentSetup: No current Set!\n");

		return;
	}

	lua_Object setupObj = lua_getparam(1);

	if (lua_isnumber(setupObj)) {
		currentRoom->setSetup(luaFtol(lua_getnumber(setupObj)));
	} else if (!_engine->isDemo() && lua_isstring(setupObj)) {
		// The disasm passes the address of the string as the setup index
		warning("Lua_V2::MakeCurrentSetup(): a setup name is not a valid setup index");
	} else {
		logMessage(kConsoleStatus, "MakeCurrentSetup: Invalid params!\n");
	}
}

void Lua_V2::GetCurrentSetup() {
	lua_Object roomObj = lua_getparam(1);

	if (!lua_isstring(roomObj)) {
		logMessage(kConsoleDebug, "Bad set string for GetCurrentSetup!");

		return;
	}

	const char *currentRoomName = Room_v2::getCurrentRoomName(_engine);

	if (!currentRoomName)
		warning("Lua_V2::GetCurrentSetup(): there is no current room");

	if (currentRoomName && scumm_stricmp(lua_getstring(roomObj), currentRoomName) == 0) {
		Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

		if (currentRoom)
			lua_pushnumber((float)currentRoom->_currentSetup);
		else
			lua_pushnil();

		return;
	}

	LString roomName = lua_getstring(roomObj);
	LResourceHandle<Room_v2> roomHandle(_engine, roomName);

	if (roomHandle.lock() && roomHandle._typed) {
		logMessage(kConsoleDebug, "GetCurrentSetup: requested setup for a room that isn't the current one. Was this intentional?\n");
		lua_pushnumber((float)roomHandle._typed->_currentSetup);
	}

	roomHandle.unlock();
}

void Lua_V2::NextSetup() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logMessage(kConsoleStatus, "No current Set!\n");

		return;
	}

	// The original could divide by zero here, let's avoid that...
	if (currentRoom->_numSetups == 0) {
		warning("Lua_V2::NextSetup(): the room has no setups");

		return;
	}

	currentRoom->setSetup((currentRoom->_currentSetup + 1) % currentRoom->_numSetups);
}

void Lua_V2::PreviousSetup() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logMessage(kConsoleStatus, "No current Set!\n");

		return;
	}

	// The original could divide by zero here, let's avoid that...
	if (currentRoom->_numSetups == 0) {
		warning("Lua_V2::PreviousSetup(): the room has no setups");

		return;
	}

	currentRoom->setSetup((currentRoom->_currentSetup - 1) % currentRoom->_numSetups);
}

} // end of namespace Grim
