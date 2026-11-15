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

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"

namespace Grim {

int Lua_V2::doExitCallback() {
	bool called = false;

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("exitHandler");

	lua_Object handler = lua_gettable();

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("exitHandler");

		lua_Object function = lua_gettable();

		if (lua_isfunction(function)) {
			called = true;
			lua_pushobject(handler);
			lua_callfunction(function);
		} else {
			logMessage(kConsoleError, "Installed exitHandler has no member func '%s'\n", "exitHandler");
		}
	} else if (lua_isfunction(handler)) {
		called = true;
		lua_callfunction(handler);
	} else if (!lua_isnil(handler)) {
		logMessage(kConsoleError, "Installed exitHandler is invalid.\n");
	}

	lua_endblock();

	return called;
}

int Lua_V2::doPauseCallback() {
	bool called = false;

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("pauseHandler");

	lua_Object handler = lua_gettable();

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("pauseHandler");

		lua_Object function = lua_gettable();

		if (lua_isfunction(function)) {
			called = true;
			lua_pushobject(handler);
			lua_callfunction(function);
		} else {
			logMessage(kConsoleError, "Installed pauseHandler has no member func '%s'\n", "pauseHandler");
		}
	} else if (lua_isfunction(handler)) {
		called = true;
		lua_callfunction(handler);
	} else if (!lua_isnil(handler)) {
		logMessage(kConsoleError, "Installed pauseHandler is invalid.\n");
	}

	lua_endblock();

	return called;
}

int Lua_V2::doCharacterCallback(int character) {
	bool called = false;
	lua_Object function = LUA_NOOBJECT;
	bool callHandler = false;

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("characterHandler");

	lua_Object handler = lua_gettable();

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("characterHandler");

		function = lua_gettable();

		if (lua_isfunction(function)) {
			called = true;
			callHandler = true;
			lua_pushobject(handler);
		} else {
			logMessage(kConsoleError, "Installed characterHandler has no member func '%s'\n", "characterHandler");
		}
	} else if (lua_isfunction(handler)) {
		function = handler;
		called = true;
		callHandler = true;
	} else if (!lua_isnil(handler)) {
		logMessage(kConsoleError, "Installed character handler is invalid.\n");
	}

	if (callHandler) {
		char text[2];

		text[0] = (char)(byte)character;
		text[1] = 0;

		lua_pushstring(text);
		lua_callfunction(function);

		// The disasm ignores the result of the handler
		lua_isnil(lua_lua2C(1));
	}

	lua_endblock();

	return called;
}

bool Lua_V2::callPrefsInit() {
	bool called = false;

	lua_beginblock();

	lua_Object prefs = lua_getglobal("system_prefs");

	if (lua_istable(prefs)) {
		lua_pushobject(prefs);
		lua_pushstring("init");

		lua_Object function = lua_gettable();

		if (lua_isfunction(function)) {
			called = true;
			lua_pushobject(prefs);
			lua_callfunction(function);
		}
	}

	lua_endblock();

	return called;
}

} // end of namespace Grim
