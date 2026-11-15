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

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/input/demoreplay.h"
#include "engines/grim/emi/input/inputrecorder.h"
#include "engines/grim/emi/input/joystick.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/input/sputcontrol_v2.h"
#include "engines/grim/grim.h"

namespace Grim {

void Lua_V2::makeControlsTable(int ref) {
	lua_Object controlsTable = lua_createtable();

	lua_pushobject(lua_getref(ref));
	lua_pushstring("controls");
	lua_pushobject(controlsTable);
	lua_settable();

	for (int i = 0; i < _engine->getControl()->getNumControls(); i++) {
		lua_pushobject(controlsTable);
		lua_pushstring(_engine->getControl()->_controlTable[i].name);
		lua_pushnumber((float)i);
		lua_settable();
	}

	lua_pushobject(controlsTable);
	lua_pushstring("joy_selected");
	lua_pushnumber((float)(int)_engine->getJoystick()->_joyId);
	lua_settable();
}

void Lua_V2::EnableControl() {
	lua_Object controlObj = lua_getparam(1);

	if (!lua_isnumber(controlObj)) {
		logMessage(kConsoleDebug, "Invalid control (non-numerical); will not be enabled.\n");
		lua_pushnil();

		return;
	}

	_engine->getControl()->enableControl((uint)luaFtol(lua_getnumber(controlObj)));
}

void Lua_V2::DisableControl() {
	lua_Object controlObj = lua_getparam(1);

	if (!lua_isnumber(controlObj)) {
		logMessage(kConsoleDebug, "Invalid control (non-numerical); will not be disabled.\n");
		lua_pushnil();

		return;
	}

	_engine->getControl()->disableControl((uint)luaFtol(lua_getnumber(controlObj)));
}

void Lua_V2::GetControlState() {
	lua_Object controlObj = lua_getparam(1);

	if (!lua_isnumber(controlObj)) {
		logMessage(kConsoleDebug, "GetControlState: Invalid control (non-numerical).\n");

		return;
	}

	int index = luaFtol(lua_getnumber(controlObj));

	if ((uint)index >= (uint)_engine->getControl()->getNumControls()) {
		logMessage(kConsoleError, "GetControlState: Invalid control %d.\n", index);

		return;
	}

	ControlEntry *control = &_engine->getControl()->_controlTable[index];

	if (control->flags & ControlEntry::kAxis)
		lua_pushnumber(control->axisValue);
	else if (control->state)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

bool Lua_V2::buttonHandler(int vk, bool down) {
	bool pushSelf = false;
	bool callHandler;
	bool handled = false;
	lua_Object function;

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("buttonHandler");

	lua_Object handler = lua_gettable();

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("buttonHandler");

		function = lua_gettable();

		if (lua_isfunction(function)) {
			pushSelf = true;
			callHandler = true;
		} else {
			logMessage(kConsoleError, "Installed button handler table has no member func '%s'!\n", "buttonHandler");

			callHandler = down;
		}
	} else if (lua_isfunction(handler)) {
		function = handler;
		callHandler = true;
	} else {
		logMessage(kConsoleError, "Installed button handler is invalid!\n");

		function = LUA_NOOBJECT;
		callHandler = false;
	}

	uint index = 0;
	uint numControls = (uint)_engine->getControl()->getNumControls();

	while (index < numControls && vk != (int)_engine->getControl()->_controlTable[index].code)
		index++;

	if (index < numControls) {
		ControlEntry *control = &_engine->getControl()->_controlTable[index];

		if (_engine->getControl()->isModifierKey(vk) && control->state == (byte)down) {
			index = numControls;
		} else {
			control->state = down;

			if (_engine->getDemoReplay())
				_engine->getDemoReplay()->consumeKey(control, down);
			else if (_engine->getInputRecorder())
				_engine->getInputRecorder()->recordKey(control, down);
			else if (_engine->getGamePlatform() != Common::kPlatformPS2)
				warning("Lua_V2::buttonHandler(): there is no input recorder");
		}
	}

	if (callHandler && index < numControls) {
		if (pushSelf)
			lua_pushobject(handler);

		lua_pushnumber((float)index);

		if (down)
			lua_pushnumber(1.0f);
		else
			lua_pushnil();

		lua_callfunction(function);

		if (!_engine->getControl()->isModifierKey(vk))
			handled = true;
	}

	lua_endblock();

	return handled;
}

void Lua_V2::callAxisHandler(int index, float value) {
	bool pushSelf = false;
	bool callHandler = false;
	lua_Object function = LUA_NOOBJECT;

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("axisHandler");

	lua_Object handler = lua_gettable();

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("axisHandler");

		function = lua_gettable();

		if (lua_isfunction(function)) {
			pushSelf = true;
			callHandler = true;
		} else {
			logMessage(kConsoleError, "Installed axis handler table has no member func '%s'!\n", "axisHandler");
		}
	} else if (lua_isfunction(handler)) {
		function = handler;
		callHandler = true;
	} else {
		logMessage(kConsoleError, "Installed axis handler is invalid!\n");
	}

	if (callHandler) {
		if (pushSelf)
			lua_pushobject(handler);

		lua_pushnumber((float)index);
		lua_pushnumber(value);
		lua_callfunction(function);

		if (_engine->getDemoReplay())
			_engine->getDemoReplay()->consumeAxis(index, value);
	}

	lua_endblock();
}

void Lua_V2::axisHandler() {
	bool pushSelf = false;
	bool callHandler = false;
	bool buttonHandled = false;
	lua_Object function = LUA_NOOBJECT;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		for (int i = 0; i < _engine->getControl()->getNumControls(); i++) {
			ControlEntry *control = &_engine->getControl()->_controlTable[i];

			if (!(control->flags & ControlEntry::kEnabled))
				continue;

			if (control->flags & ControlEntry::kAxis) {
				float value = _engine->getJoystick()->getAxis((int)control->code);

				callAxisHandler(i, value);
				control->axisValue = value;
			} else if (control->flags & ControlEntry::kJoystickButton) {
				bool state = _engine->getJoystick()->getControlState((int)control->code);

				if ((byte)state != control->state && !buttonHandled)
					buttonHandled = buttonHandler((int)control->code, state);
			}
		}

		return;
	}

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("axisHandler");

	lua_Object handler = lua_gettable();

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("axisHandler");

		function = lua_gettable();

		if (lua_isfunction(function)) {
			pushSelf = true;
			callHandler = true;
		} else {
			// The disasm leaves the flag uninitialized here
			logMessage(kConsoleError, "Installed axis handler table has no member func '%s'!\n", "axisHandler");
		}
	} else if (lua_isfunction(handler)) {
		function = handler;
		callHandler = true;
	} else {
		logMessage(kConsoleError, "Installed axis handler is invalid!\n");
	}

	for (int i = 0; i < _engine->getControl()->getNumControls(); i++) {
		ControlEntry *control = &_engine->getControl()->_controlTable[i];

		if (!(control->flags & ControlEntry::kEnabled))
			continue;

		if (control->flags & ControlEntry::kAxis) {
			float value = _engine->getJoystick()->getAxis((int)control->code);

			bool callNow = callHandler && value != control->axisValue;

			if (callNow) {
				if (pushSelf)
					lua_pushobject(handler);

				lua_pushnumber((float)i);
				lua_pushnumber(value);
				lua_callfunction(function);
				control->axisValue = value;
			}
		} else if (control->flags & ControlEntry::kJoystickButton) {
			bool state = _engine->getJoystick()->getControlState((int)control->code);

			if ((byte)state != control->state && !buttonHandled)
				buttonHandled = buttonHandler((int)control->code, state);
		}
	}

	lua_endblock();
}

void Lua_V2::updateJoySelected() {
	lua_pushobject(lua_getglobal("system"));
	lua_pushstring("controls");

	lua_Object controlsTable = lua_gettable();

	lua_pushobject(controlsTable);
	lua_pushstring("joy_selected");
	lua_pushnumber((float)(int)_engine->getJoystick()->_joyId);
	lua_settable();
}

} // End of namespace Grim
