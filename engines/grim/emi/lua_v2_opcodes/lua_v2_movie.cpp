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
#include "engines/grim/emi/movie/movie.h"

namespace Grim {

void Lua_V2::StartMovie() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object subtitlesObj = lua_getparam(2);
	lua_Object flag6194Obj = lua_getparam(3);
	lua_Object flag6195Obj = lua_getparam(4);
	lua_Object frameObj = lua_getparam(5);
	bool subtitles = false;
	bool flag6194 = false;
	bool flag6195 = true;
	int startFrame = 1;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		if (!lua_isstring(nameObj)) {
			logMessage(kConsoleStatus, "StartMovie: Argument must be a string.");

			lua_pushnil();

			return;
		}

		// PS2 reads the other arguments without using them and returns nothing
		Movie::requestMoviePs2(_engine, lua_getstring(nameObj));

		return;
	}

	if (_engine->isDemo()) {
		// The demo takes only the movie name and always asks for subtitles
		if (!lua_isstring(nameObj)) {
			logError("StartMovie: the argument must be a string.\n");
			lua_pushnil();

			return;
		}

		const char *movieName = lua_getstring(nameObj);
		bool isStarted = _engine->playMovie(movieName, true, false, true, 1);

		logMessage(kConsoleStatus, "StartMovie: %s %s\n", movieName, isStarted ? "OK" : "FAIL");

		if (isStarted)
			lua_pushnumber(1.0f);
		else
			lua_pushnil();

		return;
	}

	if (!lua_isstring(nameObj)) {
		lua_pushnil();

		return;
	}

	if (lua_isnumber(subtitlesObj)) {
		float value = lua_getnumber(subtitlesObj);

		subtitles = value > 0.0f || value < 0.0f;
	}

	if (lua_isnumber(flag6194Obj)) {
		float value = lua_getnumber(flag6194Obj);

		flag6194 = value > 0.0f || value < 0.0f;
	}

	// The disasm clears this flag for any number, whatever its value
	if (lua_isnumber(flag6195Obj))
		flag6195 = false;

	if (lua_isnumber(frameObj))
		startFrame = luaFtol(lua_getnumber(frameObj));

	if (_engine->playMovie(lua_getstring(nameObj), subtitles, flag6194, flag6195, startFrame))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::StopMovie() {
	Movie::stopMovie(_engine);
}

void Lua_V2::PauseMovie() {
	lua_Object pauseObj = lua_getparam(1);

	if (lua_isnil(pauseObj))
		Movie::resumeCurrent(_engine);
	else
		Movie::pauseCurrent(_engine);
}

void Lua_V2::EscapeMovie() {
	Movie::skipCurrent(_engine);
}

void Lua_V2::IsMoviePlaying() {
	if (_engine->getMode() == EmiEngine::kModeMovie)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

} // End of namespace Grim
