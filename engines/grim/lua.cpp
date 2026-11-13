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
#include "common/system.h"
#include "common/events.h"

#include "math/matrix3.h"

#include "engines/grim/debug.h"
#include "engines/grim/lua.h"
#include "engines/grim/grim.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/luadebug.h"
#include "engines/grim/lua/lualib.h"

namespace Grim {

void LuaObjects::add(float number) {
	Obj obj;
	obj._type = Obj::Number;
	obj._value.number = number;
	_objects.push_back(obj);
}

void LuaObjects::add(int number) {
	Obj obj;
	obj._type = Obj::Number;
	obj._value.number = number;
	_objects.push_back(obj);
}

void LuaObjects::add(const char *str) {
	Obj obj;
	obj._type = Obj::String;
	obj._value.string = str;
	_objects.push_back(obj);
}

void LuaObjects::addNil() {
	Obj obj;
	obj._type = Obj::Nil;
	// Just set a value to avoid having uninitialized fields.
	obj._value.string = nullptr;
	_objects.push_back(obj);
}

void LuaObjects::pushObjects() const {
	for (Common::List<Obj>::const_iterator i = _objects.begin(); i != _objects.end(); ++i) {
		const Obj &o = *i;
		switch (o._type) {
			case Obj::Nil:
				lua_pushnil();
				break;
			case Obj::Number:
				lua_pushnumber(o._value.number);
				break;
			case Obj::String:
				lua_pushstring(o._value.string);
				break;
		}
	}
}

LuaBase *LuaBase::_instance = nullptr;

LuaBase::LuaBase(bool isEmi, bool openState) :
		_frameTimeCollection(0) {
	_instance = this;

	if (!openState)
		return;

	lua_open(isEmi);
	lua_iolibopen();
	lua_strlibopen();
	lua_mathlibopen();
}

LuaBase::~LuaBase() {
	_instance = nullptr;

	if (lua_isopen()) {
		lua_removelibslists();
		lua_close();
		lua_iolibclose();
	}

	lua_callhook = nullptr;
	lua_linehook = nullptr;
	lua_savehook = nullptr;
	lua_restorehook = nullptr;
}

void LuaBase::registerTypeOverride() {
	lua_pushobject(lua_getglobal("type"));
	_refTypeOverride = lua_ref(true);
	lua_pushCclosure(LUA_OPCODE(LuaBase, typeOverride), 0);
	lua_setglobal("type");
}

void LuaBase::registerSectorConstants() {
	// Register constants for box types
	lua_pushnumber(0);
	lua_setglobal("NONE");
	lua_pushnumber(0x1000);
	lua_setglobal("WALK");
	lua_pushnumber(0x2000);
	lua_setglobal("CAMERA");
	lua_pushnumber(0x4000);
	lua_setglobal("SPECIAL");
	lua_pushnumber(0x8000);
	lua_setglobal("HOT");
}

void LuaBase::registerConcatFallback() {
	lua_pushobject(lua_setfallback("concat", LUA_OPCODE(LuaBase, concatFallback)));
	_refOldConcatFallback = lua_ref(1);
}

void LuaBase::forceDemo() {
	lua_pushnumber(1);
	lua_setglobal("DEMO");
}

bool LuaBase::logError(const char *message) {
	return false;
}

Common::SeekableReadStream *LuaBase::openBundleStream(const char *path) {
	return nullptr;
}

Common::SeekableReadStream *LuaBase::openPrefsForReading(Common::SeekableReadStream *realFile) {
	return realFile;
}

Common::WriteStream *LuaBase::openPrefsForWriting(Common::WriteStream *realFile) {
	return realFile;
}

struct luaL_reg baseOpcodes[] = {
	{ "  concatfallback", LUA_OPCODE(LuaBase, concatFallback) },
	{ "  typeoverride", LUA_OPCODE(LuaBase, typeOverride) },
	{ "  dfltcamera", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "  dfltcontrol", LUA_OPCODE(LuaBase, dummyHandler) },
};

void LuaBase::recognizeOpcodes() {
	luaL_addlibtolist(baseOpcodes, ARRAYSIZE(baseOpcodes));
}

void LuaBase::update(int frameTime, int movieTime) {
	_frameTimeCollection += frameTime;
	if (_frameTimeCollection > 10000) {
		_frameTimeCollection = 0;
		lua_collectgarbage(0);
	}

	lua_beginblock();
	setFrameTime(frameTime);
	lua_endblock();

	lua_beginblock();
	setMovieTime(movieTime);
	lua_endblock();

	// Run asynchronous tasks
	lua_runtasks(frameTime);
}

void LuaBase::setFrameTime(float frameTime) {
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("frameTime");
	lua_pushnumber(frameTime);
	lua_settable();
}

void LuaBase::setMovieTime(float movieTime) {
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("movieTime");
	lua_pushnumber(movieTime);
	lua_settable();
}

int LuaBase::dofile(const char *filename) {
	Common::SeekableReadStream *stream;
	stream = g_grim->openStreamFile(filename);
	if (!stream) {
		Debug::warningImpl(Debug::Engine, "Cannot find script %s", filename);
		return 2;
	}

	int32 size = stream->size();
	char *buffer = new char[size];
	stream->read(buffer, size);
	int result = lua_dobuffer(const_cast<char *>(buffer), size, const_cast<char *>(filename));
	delete stream;
	delete[] buffer;
	return result;
}

bool LuaBase::callback(const char *name) {
	LuaObjects o;
	return callback(name, o);
}

bool LuaBase::callback(const char *name, const LuaObjects &objects) {
	lua_beginblock();

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring(name);
	lua_Object table = lua_gettable();

	if (lua_istable(table)) {
		lua_pushobject(table);
		lua_pushstring(name);
		lua_Object func = lua_gettable();
		if (lua_isfunction(func)) {
			lua_pushobject(table);
			objects.pushObjects();
			lua_callfunction(func);
		} else {
			lua_endblock();
			return false;
		}
	} else if (lua_isfunction(table)) {
		objects.pushObjects();
		lua_callfunction(table);
	} else if (!lua_isnil(table)) {
		lua_endblock();
		return false;
	}

	lua_endblock();
	return true;
}

bool LuaBase::getbool(int num) {
	return !lua_isnil(lua_getparam(num));
}

void LuaBase::pushbool(bool val) {
	if (val)
		lua_pushnumber(1);
	else
		lua_pushnil();
}

void LuaBase::dummyHandler() {
}

void LuaBase::typeOverride() {
	lua_Object data = lua_getparam(1);

	if (lua_isuserdata(data)) {
		switch (lua_tag(data)) {
			case MKTAG('A','C','T','R'):
			case MKTAG('R','T','C','A'):
				lua_pushstring("actor");
				lua_pushnumber(lua_tag(data));
				return;
			case MKTAG('C','O','S','T'):
				lua_pushstring("costume");
				lua_pushnumber(lua_tag(data));
				return;
			case MKTAG('S','E','T',' '):
				lua_pushstring("set");
				lua_pushnumber(lua_tag(data));
				return;
			case MKTAG('K','E','Y','F'):
				lua_pushstring("keyframe");
				lua_pushnumber(lua_tag(data));
				return;
			default:
				break;
		}
	}

	lua_pushobject(data);
	lua_callfunction(lua_getref(_refTypeOverride));
	lua_Object param1 = lua_getresult(1);
	lua_Object param2 = lua_getresult(2);
	lua_pushobject(param1);
	lua_pushobject(param2);
}

void LuaBase::concatFallback() {
	lua_Object params[2];
	char result[200];
	char *ptr;

	params[0] = lua_getparam(1);
	params[1] = lua_getparam(2);
	result[0] = 0;

	for (int i = 0; i < 2; i++) {
		if (!lua_isnil(params[i]) && !lua_isuserdata(params[i]) && !lua_isstring(params[i])) {
			lua_pushobject(params[0]);
			lua_pushobject(params[1]);
			lua_callfunction(lua_getref(_refOldConcatFallback));
			lua_pushobject(lua_getresult(1));
			return;
		}

		int pos = strlen(result);
		ptr = &result[pos];

		if (lua_isnil(params[i]))
			Common::sprintf_s(ptr, sizeof(result) - pos, "(nil)");
		else if (lua_isstring(params[i]))
			Common::sprintf_s(ptr, sizeof(result) - pos, "%s", lua_getstring(params[i]));
		else if (lua_tag(params[i]) == MKTAG('R','T','C','A')) {
			int handle = lua_getuserdata(params[i]);
			SputActor *actor = g_grim->_sputActor->actorDeref(handle);
			const char *modelName = "";

			if (actor && actor->costumeStack && actor->costumeStack->headThing && actor->costumeStack->headThing->data.model3)
				modelName = actor->costumeStack->headThing->data.model3->name;

			Common::sprintf_s(ptr, sizeof(result) - pos, "(actor%d:%s)", handle, modelName);
		} else {
			lua_pushobject(params[0]);
			lua_pushobject(params[1]);
			lua_callfunction(lua_getref(_refOldConcatFallback));
			lua_pushobject(lua_getresult(1));
			return;
		}
	}

	lua_pushstring(result);
}

}
