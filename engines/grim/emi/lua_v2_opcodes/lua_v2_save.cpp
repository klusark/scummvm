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

#include "engines/grim/emi/graphics/background.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/costume/choreset.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/resources/resourcemanager.h"
#include "engines/grim/emi/scene/room_v2.h"

namespace Grim {

static const int kSavedStringBufferSize = 512;
static const int kMaxSavedLocks = 65536;

static void writeSavedString(SaveRestoreAccess *writer, const LString &text) {
	int length = (int)text.size() + 1;

	writer(&length, sizeof(length));
	writer((void *)text.c_str(), length);
}

static bool readSavedString(SaveRestoreAccess *reader, LString &text) {
	char buffer[kSavedStringBufferSize + 1];
	int length = 0;

	reader(&length, sizeof(length));

	if (length < 0 || length > kSavedStringBufferSize) {
		warning("Lua_V2::restoreLocks(): invalid string length %d", length);

		return false;
	}

	reader(buffer, length);
	buffer[length] = 0;
	text = buffer;

	return true;
}

static bool readSavedLockCount(SaveRestoreAccess *reader, int *count) {
	reader(count, sizeof(*count));

	if (*count < 0 || *count > kMaxSavedLocks) {
		warning("Lua_V2::restoreLocks(): invalid lock count %d", *count);

		return false;
	}

	return true;
}

void Lua_V2::Load() {
	lua_Object fileNameObj = lua_getparam(1);

	if (!lua_isnil(fileNameObj) && !lua_isstring(fileNameObj))
		logMessage(kConsoleWarning, "Argument to Load() must be a string!\n");

	Lua_V1::Load();
}

void Lua_V2::Save() {
	lua_Object fileNameObj = lua_getparam(1);

	if (!lua_isnil(fileNameObj) && !lua_isstring(fileNameObj))
		logMessage(kConsoleWarning, "Save: the argument must be a file name or nil.\n");

	Lua_V1::Save();
}

void Lua_V2::saveGameCallback(SaveRestoreAccess *writer) {
	_saveWriter = writer;
	_saveDataSize = 0;

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("saveGameCallback");

	lua_Object handler = lua_gettable();

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("saveGameCallback");

		lua_Object function = lua_gettable();

		if (lua_isfunction(function)) {
			lua_pushobject(handler);
			lua_callfunction(function);
		} else {
			logMessage(kConsoleError, "Installed savegame handler has no member func '%s'\n", "saveGameCallback");
		}
	} else if (lua_isfunction(handler)) {
		lua_callfunction(handler);
	} else if (!lua_isnil(handler)) {
		logMessage(kConsoleError, "Installed save game callback is invalid.\n");
	}

	if (_saveDataSize == 0)
		writer(&_saveDataSize, sizeof(_saveDataSize));

	lua_endblock();
}

void Lua_V2::getVersion(LString &version) {
	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("version");

	lua_Object versionObject = lua_gettable();

	if (lua_isstring(versionObject)) {
		version = lua_getstring(versionObject);
	} else {
		warning("Lua_V2::getVersion(): the system table has no version string");
		version.clear();
	}

	lua_endblock();
}

void Lua_V2::saveLocks(SaveRestoreAccess *writer) {
	int count = (int)_lockedChores.size();

	writer(&count, sizeof(count));

	for (int i = 0; i < count; i++) {
		writeSavedString(writer, _lockedChores[i].choreSetName);
		writeSavedString(writer, _lockedChores[i].choreName);
	}

	count = (int)_lockedChoreSets.size();
	writer(&count, sizeof(count));

	for (int i = 0; i < count; i++)
		writeSavedString(writer, _lockedChoreSets[i]);

	count = (int)_lockedRooms.size();
	writer(&count, sizeof(count));

	for (int i = 0; i < count; i++)
		writeSavedString(writer, _lockedRooms[i]);

	count = (int)_lockedBackgrounds.size();
	writer(&count, sizeof(count));

	for (int i = 0; i < count; i++)
		writeSavedString(writer, _lockedBackgrounds[i]);

	count = (int)_lockedSounds.size();
	writer(&count, sizeof(count));

	for (int i = 0; i < count; i++)
		writer(&_lockedSounds[i], sizeof(uint));
}

void Lua_V2::restoreLocks(SaveRestoreAccess *reader) {
	int count = 0;

	if (!readSavedLockCount(reader, &count))
		return;

	_lockedChores.resize(count);

	for (int i = 0; i < count; i++) {
		if (!readSavedString(reader, _lockedChores[i].choreSetName))
			return;

		if (!readSavedString(reader, _lockedChores[i].choreName))
			return;

		LResourceHandle<ChoreSet> choreSetHandle(_engine, _lockedChores[i].choreSetName);

		if (choreSetHandle.lock() && choreSetHandle._typed) {
			Chore *chore = choreSetHandle._typed->findChore(_lockedChores[i].choreName);

			if (chore)
				chore->lockComponents();
		}

		choreSetHandle.unlock();
	}

	if (!readSavedLockCount(reader, &count))
		return;

	_lockedChoreSets.resize(count);

	for (int i = 0; i < count; i++) {
		if (!readSavedString(reader, _lockedChoreSets[i]))
			return;

		LResourceHandle<ChoreSet> choreSetHandle(_engine, _lockedChoreSets[i]);

		if (choreSetHandle.lock() && choreSetHandle._typed) {
			for (uint j = 0; j < choreSetHandle._typed->_chores.size(); j++) {
				Chore *chore = choreSetHandle._typed->_chores[j];

				if (chore)
					chore->lockComponents();
			}
		}

		choreSetHandle.unlock();
	}

	if (!readSavedLockCount(reader, &count))
		return;

	_lockedRooms.resize(count);

	for (int i = 0; i < count; i++) {
		if (!readSavedString(reader, _lockedRooms[i]))
			return;

		LResourceHandle<Room_v2> roomHandle(_engine, _lockedRooms[i]);

		if (roomHandle.lock() && roomHandle._typed) {
			roomHandle._typed->_lockCount++;
			_engine->getResourceManager()->lockResource(roomHandle._typed);
		}

		roomHandle.unlock();
	}

	if (!readSavedLockCount(reader, &count))
		return;

	_lockedBackgrounds.resize(count);

	for (int i = 0; i < count; i++) {
		if (!readSavedString(reader, _lockedBackgrounds[i]))
			return;

		if (_engine->getGamePlatform() == Common::kPlatformPS2) {
			lockBackgroundWithScene(_lockedBackgrounds[i], false);

			continue;
		}

		LResourceHandle<Background> backgroundHandle(_engine, _lockedBackgrounds[i]);

		if (backgroundHandle.lock() && backgroundHandle._typed) {
			backgroundHandle._typed->_lockCount++;
			_engine->getResourceManager()->lockResource(backgroundHandle._typed);
		}

		backgroundHandle.unlock();
	}

	if (!readSavedLockCount(reader, &count))
		return;

	_lockedSounds.resize(count);

	for (int i = 0; i < count; i++)
		reader(&_lockedSounds[i], sizeof(uint));
}

} // end of namespace Grim
