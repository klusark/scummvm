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
#include "common/textconsole.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/graphics/layer.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/scene/setup.h"

namespace Grim {

void Lua_V2::NewLayer() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object sortOrderObj = lua_getparam(2);
	// The disasm reads the first parameter again where the frame should be
	lua_Object frameObj = lua_getparam(1);
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logMessage(kConsoleWarning, "NewLayer: No current Set!\n");
		lua_pushnil();

		return;
	}

	if (!currentRoom->_setups || currentRoom->_currentSetup < 0 || currentRoom->_currentSetup >= currentRoom->_numSetups || !lua_isstring(nameObj)) {
		warning("Lua_V2::NewLayer(): invalid setup or layer name");
		lua_pushnil();

		return;
	}

	Setup *setup = &currentRoom->_setups[currentRoom->_currentSetup];
	int frame = luaFtol(lua_getnumber(frameObj));
	int sortOrder = luaFtol(lua_getnumber(sortOrderObj));
	const char *layerName = lua_getstring(nameObj);
	int layerId = setup->addLayer(layerName, sortOrder, frame, -1);

	lua_pushusertag(layerId, MKTAG('R', 'Y', 'A', 'L'));
}

void Lua_V2::FreeLayer() {
	lua_Object layerObj = lua_getparam(1);

	if (lua_isuserdata(layerObj) && lua_tag(layerObj) == MKTAG('R', 'Y', 'A', 'L')) {
		int layerId = lua_getuserdata(layerObj);
		Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

		if (currentRoom) {
			if (!currentRoom->_setups || currentRoom->_currentSetup < 0 || currentRoom->_currentSetup >= currentRoom->_numSetups) {
				warning("Lua_V2::FreeLayer(): invalid setup");

				return;
			}

			currentRoom->_setups[currentRoom->_currentSetup].removeLayerById(layerId);
		}
	} else {
		logMessage(kConsoleError, "FreeLayer first parm must be a layer");
	}
}

void Lua_V2::SetLayerSortOrder() {
	lua_Object layerObj = lua_getparam(1);

	if (lua_isuserdata(layerObj) && lua_tag(layerObj) == MKTAG('R', 'Y', 'A', 'L')) {
		Layer *layer = Room_v2::findLayerById(_engine, lua_getuserdata(layerObj));

		if (layer) {
			lua_Object sortOrderObj = lua_getparam(2);

			layer->_sortOrder = luaFtol(lua_getnumber(sortOrderObj));
		} else {
			logMessage(kConsoleError, "SetLayerSortOrder layer not found.\n");
		}
	} else {
		logMessage(kConsoleError, "SetLayerSortOrder first parm must be a layer");
	}
}

void Lua_V2::GetLayerSortOrder() {
	lua_Object layerObj = lua_getparam(1);

	if (lua_isuserdata(layerObj) && lua_tag(layerObj) == MKTAG('R', 'Y', 'A', 'L')) {
		Layer *layer = Room_v2::findLayerById(_engine, lua_getuserdata(layerObj));

		if (layer) {
			lua_pushnumber((float)layer->_sortOrder);
		} else {
			logMessage(kConsoleError, "GetLayerSortOrder: layer not found.\n");
		}
	} else {
		logMessage(kConsoleError, "GetLayerSortOrder first parm must be a layer");
	}
}

void Lua_V2::GetLayerFrame() {
	lua_Object layerObj = lua_getparam(1);

	if (lua_isuserdata(layerObj) && lua_tag(layerObj) == MKTAG('R', 'Y', 'A', 'L')) {
		Layer *layer = Room_v2::findLayerById(_engine, lua_getuserdata(layerObj));

		if (layer) {
			lua_pushnumber((float)layer->_frame);
		} else {
			logMessage(kConsoleError, "GetLayerCurFrame: layer not found.\n");
		}
	} else {
		logMessage(kConsoleError, "GetLayerCurFrame first parm must be a layer");
	}
}

void Lua_V2::SetLayerFrame() {
	lua_Object layerObj = lua_getparam(1);

	if (lua_isuserdata(layerObj) && lua_tag(layerObj) == MKTAG('R', 'Y', 'A', 'L')) {
		Layer *layer = Room_v2::findLayerById(_engine, lua_getuserdata(layerObj));

		if (layer) {
			lua_Object frameObj = lua_getparam(2);

			layer->_frame = luaFtol(lua_getnumber(frameObj));
		} else {
			logMessage(kConsoleError, "SetLayerCurFrame: layer not found.\n");
		}
	} else {
		logMessage(kConsoleError, "SetLayerCurFrame first parm must be a layer");
	}
}

void Lua_V2::AdvanceLayerFrame() {
	lua_Object layerObj = lua_getparam(1);

	if (lua_isuserdata(layerObj) && lua_tag(layerObj) == MKTAG('R', 'Y', 'A', 'L')) {
		Layer *layer = Room_v2::findLayerById(_engine, lua_getuserdata(layerObj));

		if (layer) {
			lua_Object frameObj = lua_getparam(2);

			layer->_frame += luaFtol(lua_getnumber(frameObj));
		} else {
			logMessage(kConsoleError, "AdvanceLayerCurFrame: layer not found.\n");
		}
	} else {
		logMessage(kConsoleError, "AdvanceLayerCurFrame: first parm must be a layer");
	}
}

} // end of namespace Grim
