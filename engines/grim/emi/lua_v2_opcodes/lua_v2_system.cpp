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

#include "engines/grim/emi/resources/cbundle.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/files/fileutil.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/emi/input/sputcontrol_v2.h"

namespace Grim {

void Lua_V2::EnginePause() {
	_engine->setPause(true);
}

void Lua_V2::EngineResume() {
	_engine->setPause(false);
}

void Lua_V2::GetPhysicalBytes() {
	lua_pushnumber((float)(int)_engine->getTotalPhysMem());
}

void Lua_V2::GetMemoryUsage() {
	UnimplementedFunction();
}

void Lua_V2::SetMemoryUsage() {
	lua_Object limitObj = lua_getparam(1);

	lua_getparam(2);

	_engine->getMemoryServices()->setHardMemoryCap((uint)luaFtol(lua_getnumber(limitObj)));
}

void Lua_V2::SetActiveCD() {
	int disc = luaFtol(lua_getnumber(lua_getparam(1)));

	if (disc != 1 && disc != 2)
		return;

	int currentDisc = _engine->getDiscNumber();

	if (disc == currentDisc) {
		lua_pushnumber(1.0f);

		return;
	}

	if (currentDisc) {
		_engine->getSounds()->shutdownSounds();
		_engine->getBundleServices()->setDiscBundles(0);
	}

	if (_engine->findDisc(disc) && _engine->getBundleServices()->setDiscBundles(disc)) {
		_engine->getSounds()->startSounds(true);
		lua_pushnumber(1.0f);
	}
}

void Lua_V2::GetActiveCD() {
	int disc = _engine->getDiscNumber();

	_engine->getBundleServices()->setDiscBundles(disc);

	if (disc)
		lua_pushnumber((float)disc);
}

void Lua_V2::AreWeInternational() {
	if (_engine->getBundleServices()->isI9nBundleOpen())
		lua_pushnumber(1.0f);
}

void Lua_V2::FileFindFirst() {
	lua_Object extensionObj = lua_getparam(1);
	lua_Object directoryObj = lua_getparam(2);

	if (!lua_isstring(extensionObj)) {
		if (_engine->isDemo())
			logMessage(kConsoleWarning, "FileFindFirst: first argument must be a string\n");

		lua_pushnil();

		return;
	}

	const char *directory = "";

	if (!lua_isnil(directoryObj)) {
		directory = lua_getstring(directoryObj);

		if (!directory) {
			// The disasm passes a null pointer, the CRT prints it as "(null)"
			warning("Lua_V2::FileFindFirst(): the directory is not a string");

			directory = "(null)";
		}
	}

	const char *extension = lua_getstring(extensionObj);

	FileFindDispose();
	_find = FileUtilFind::newFind(directory, 4, extension);

	if (_find)
		FileFindNext();
	else
		lua_pushnil();
}

void Lua_V2::FileFindNext() {
	FileUtilEntry entry;

	if (_find) {
		while (_find->findNext(&entry)) {
			if (!entry.isDirectory) {
				lua_pushstring(entry.path.c_str());

				return;
			}
		}
	}

	lua_pushnil();
}

void Lua_V2::FileFindDispose() {
	if (_find)
		FileUtilFind::disposeFind(_find);
}

void Lua_V2::TextFileGetLineCount() {
	lua_Object nameObj = lua_getparam(1);
	char buffer[512];
	int lineCount = 0;

	if (lua_isnil(nameObj)) {
		if (_engine->isDemo())
			logMessage(kConsoleDebug, "TextFileGetLineCount: the argument must be a file name.\n");

		lua_pushnil();

		return;
	}

	const char *fileName = lua_getstring(nameObj);

	if (!fileName) {
		warning("Lua_V2::TextFileGetLineCount(): the file name is not a string");

		lua_pushnil();

		return;
	}

	LFile *file = _engine->getFileServices()->open(fileName, "rt");

	if (!file) {
		if (_engine->isDemo())
			logMessage(kConsoleDebug, "TextFileGetLineCount: cannot open %s.\n", fileName);

		lua_pushnil();

		return;
	}

	lua_Object linesTable = lua_createtable();

	while (!file->eof()) {
		lua_pushobject(linesTable);
		lua_pushnumber((float)lineCount);
		lua_pushnumber((float)file->tell());
		lua_settable();
		file->readLine(buffer, sizeof(buffer));
		lineCount++;
	}

	_engine->getFileServices()->close(file, true);

	lua_pushobject(linesTable);
	lua_pushstring("count");
	lua_pushnumber((float)lineCount);
	lua_settable();
	lua_pushobject(linesTable);
}

void Lua_V2::TextFileGetLine() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object positionObj = lua_getparam(2);
	char buffer[512];

	if (lua_isnil(nameObj) || lua_isnil(positionObj)) {
		if (_engine->isDemo())
			logMessage(kConsoleDebug, "TextFileGetLine: the arguments must be a file name and a position.\n");

		lua_pushnil();

		return;
	}

	const char *fileName = lua_getstring(nameObj);

	if (!fileName) {
		warning("Lua_V2::TextFileGetLine(): the file name is not a string");

		lua_pushnil();

		return;
	}

	LFile *file = _engine->getFileServices()->open(fileName, "rt");

	if (!file) {
		if (_engine->isDemo())
			logMessage(kConsoleDebug, "TextFileGetLine: cannot open %s.\n", fileName);

		lua_pushnil();

		return;
	}

	file->seek(luaFtol(lua_getnumber(positionObj)), SEEK_SET);

	buffer[0] = 0;
	file->readLine(buffer, sizeof(buffer));
	_engine->getFileServices()->close(file, true);

	lua_pushstring(buffer);
}

void Lua_V2::InputDialog() {
	if (disabledFunction(kConsoleError, "InputDialog: usb_getcmd failed.\n", LUA_NOOBJECT))
		return;

	lua_Object captionObj = lua_getparam(1);
	lua_Object promptObj = lua_getparam(2);
	lua_Object defaultObj = lua_getparam(3);
	char buffer[101];

	if (!lua_isstring(captionObj) || !lua_isstring(promptObj) || (!lua_isnil(defaultObj) && !lua_isstring(defaultObj))) {
		lua_pushnil();

		return;
	}

	const char *caption = lua_getstring(captionObj);
	const char *prompt = lua_getstring(promptObj);
	const char *defaultText = nullptr;

	if (lua_isstring(defaultObj))
		defaultText = lua_getstring(defaultObj);

	memset(buffer, 0, sizeof(buffer));

	if (_engine->inputBox(caption, prompt, buffer, sizeof(buffer), defaultText))
		lua_pushstring(buffer);
	else
		lua_pushnil();

	_engine->getControl()->clearKeyStates();
}

void Lua_V2::QueryDialog() {
	lua_Object captionObj = lua_getparam(1);
	lua_Object textObj = lua_getparam(2);
	lua_Object cancelObj = lua_getparam(3);

	if (lua_isstring(captionObj) && lua_isstring(textObj)) {
		const char *caption = lua_getstring(captionObj);
		const char *text = lua_getstring(textObj);
		uint flags = EmiEngine::kMessageBoxYesNoCancel;

		if (lua_isnil(cancelObj))
			flags = EmiEngine::kMessageBoxYesNo;

		int result = _engine->messageBox(caption, text, flags);

		if (result == EmiEngine::kMessageBoxCancel) {
			lua_pushnumber(0.0f);

			return;
		}

		if (result != EmiEngine::kMessageBoxNo) {
			lua_pushnumber(1.0f);

			return;
		}
	} else if (_engine->isDemo()) {
		logMessage(kConsoleWarning, "QueryDialog: the caption and the text must be strings.\n");
	}

	lua_pushnil();
}

void Lua_V2::ScreenShot() {
	_engine->saveScreenshot();
}

void Lua_V2::MakeScreenTextures() {
	lua_Object indexObj = lua_getparam(1);

	if (lua_isnil(indexObj) || !lua_isnumber(indexObj)) {
		logMessage(kConsoleError, "sputLua_MakeScreenTextures: First parameter must be a specialty texture index. Operation aborted.\n");
	} else if (_engine->buildSpecScreenTextures(luaFtol(lua_getnumber(indexObj)))) {
		lua_pushnumber(1.0f);

		return;
	}

	lua_pushnil();
}

void Lua_V2::SubmitSaveGameData() {
	lua_Object tableObj = lua_getparam(1);
	lua_Object entryObj;
	int index = 0;

	_saveDataSize = 0;

	for (;;) {
		lua_pushobject(tableObj);
		lua_pushnumber((float)index);
		index++;

		entryObj = lua_gettable();

		if (lua_isnil(entryObj))
			break;

		if (!lua_isstring(entryObj)) {
			logMessage(kConsoleError, "Savegame table must contain only strings!\n");

			return;
		}

		_saveDataSize += (int)strlen(lua_getstring(entryObj)) + 5;
	}

	if (!_saveDataSize)
		return;

	if (!_saveWriter) {
		warning("Lua_V2::SubmitSaveGameData(): no save in progress");

		return;
	}

	_saveWriter(&_saveDataSize, 4);

	index = 0;

	for (;;) {
		lua_pushobject(tableObj);
		lua_pushnumber((float)index);
		index++;

		entryObj = lua_gettable();

		if (lua_isnil(entryObj))
			break;

		const char *text = lua_getstring(entryObj);
		int length = (int)strlen(text) + 1;

		_saveWriter(&length, 4);
		_saveWriter((void *)text, length);
	}
}

void Lua_V2::GetSaveGameData() {
	lua_Object nameObj = lua_getparam(1);
	char text[128];

	if (!lua_isstring(nameObj))
		return;

	LFile *file = _engine->openSaveFile(lua_getstring(nameObj), "rb");

	if (file) {
		lua_Object linesTable = lua_createtable();
		int index = 0;
		int totalSize = 0;
		int length = 0;

		file->read(&totalSize, 4);

		while (totalSize > 0) {
			file->read(&length, 4);

			if (length < 1 || length > (int)sizeof(text)) {
				warning("Lua_V2::GetSaveGameData(): invalid string size %d", length);

				break;
			}

			if (file->read(text, length) != (uint)length) {
				warning("Lua_V2::GetSaveGameData(): unexpected end of file");

				break;
			}

			if (text[length - 1] != 0) {
				warning("Lua_V2::GetSaveGameData(): the string is not terminated");

				text[length - 1] = 0;
			}

			lua_pushobject(linesTable);
			lua_pushnumber((float)index);
			index++;
			lua_pushstring(text);
			lua_settable();

			totalSize += -4 - length;
		}

		lua_pushobject(linesTable);
	}

	_engine->closeSaveFile(file);
}

void Lua_V2::JustLoaded() {
	if (_engine->getGamePlatform() != Common::kPlatformPS2) {
		Lua_V1::JustLoaded();

		return;
	}

	// PS2 returns the loaded file name, and scripts keep it as the save slot
	const char *fileName = _engine->justLoadedFileName();

	if (fileName)
		lua_pushstring(fileName);
}

void Lua_V2::ScreenshotForSavegame() {
	if (_engine->grabScreen())
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::ThumbnailFromScreen() {
	lua_Object indexObj = lua_getparam(1);

	if (lua_isnil(indexObj) || !lua_isnumber(indexObj)) {
		logMessage(kConsoleError, "sputLua_ThumbnailFromScreen: First parameter must be a specialty texture index. Operation aborted.\n");
	} else if (_engine->makeThumbnailTexture(luaFtol(lua_getnumber(indexObj)), true)) {
		lua_pushnumber(1.0f);

		return;
	}

	lua_pushnil();
}

void Lua_V2::ThumbnailFromFile() {
	lua_Object indexObj = lua_getparam(1);
	lua_Object nameObj = lua_getparam(2);

	if (lua_isnil(indexObj) || !lua_isnumber(indexObj)) {
		logMessage(kConsoleError, "sputLua_ThumbnailFromFile: First parameter must be a specialty texture index. Operation aborted.\n");
	} else {
		if (lua_isnil(nameObj) || !lua_isstring(nameObj)) {
			logMessage(kConsoleError, "sputLua_ThumbnailFromFile: First parameter must be a filename. Operation aborted.\n");
			lua_pushnil();

			return;
		}

		if (_engine->loadThumbnailTexture(luaFtol(lua_getnumber(indexObj)), lua_getstring(nameObj))) {
			lua_pushnumber(1.0f);

			return;
		}
	}

	lua_pushnil();
}

void Lua_V2::ClearSpecialtyTexture() {
	lua_Object indexObj = lua_getparam(1);

	if (lua_isnil(indexObj) || !lua_isnumber(indexObj)) {
		logMessage(kConsoleError, "sputLua_ClearSpecialtyTexture: First parameter must be a specialty texture index. Operation aborted.\n");
	} else if (_engine->clearTextureSlot(luaFtol(lua_getnumber(indexObj)))) {
		lua_pushnumber(1.0f);

		return;
	}

	lua_pushnil();
}

} // end of namespace Grim
