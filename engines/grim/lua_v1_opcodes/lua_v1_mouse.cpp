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

#include "engines/grim/grim.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/remastered/dialoglog.h"
#include "engines/grim/remastered/hotspot.h"
#include "engines/grim/remastered/sputmouse.h"

namespace Grim {

enum {
	kMaxActiveHotspots = 10,
	kLogLineLength = 512,
	kLogBufferSize = 520
};

// Opcodes of the mouse, the hotspots, the path finder and the dialog log of the remastered

lua_Object Lua_V1::makeVectorTable(const float *vector) {
	lua_Object table = lua_createtable();

	lua_pushobject(table);
	lua_pushstring("x");
	lua_pushnumber(vector[0]);
	lua_settable();
	lua_pushobject(table);
	lua_pushstring("y");
	lua_pushnumber(vector[1]);
	lua_settable();
	lua_pushobject(table);
	lua_pushstring("z");
	lua_pushnumber(vector[2]);
	lua_settable();

	return table;
}

void Lua_V1::WalkActorToMouseTarget() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "WalkActorToMouseTarget"))
		return;

	g_grim->_sputMouse->walkActorToMouseTarget(actor, g_grim->_sputRender);
}

void Lua_V1::PathfinderMode() {
	g_grim->_sputActor->setPathfinderMode(luaFtol(lua_getnumber(lua_getparam(1))));
}

// Table with a key for each line of the log, from 0, that holds the slot of the line. The number of
// lines is in count.
void Lua_V1::DialogLogGetLineCount() {
	DialogLog *dialogLog = g_grim->_dialogLog;
	lua_Object result = lua_createtable();
	int cursor = -1;
	const char *line = dialogLog->getNextLine(&cursor);
	float count = 0.0f;

	lua_pushobject(result);

	if (line) {
		int numLines = 0;

		do {
			lua_pushnumber((float)numLines);
			lua_pushnumber((float)cursor);
			lua_settable();
			numLines++;
			line = dialogLog->getNextLine(&cursor);
			lua_pushobject(result);
		} while (line);

		count = (float)numLines;
	}

	lua_pushstring("count");
	lua_pushnumber(count);
	lua_settable();
	lua_pushobject(result);
}

// The first parameter is the table of the line count, the second one is a slot of the log
void Lua_V1::DialogLogGetLine() {
	lua_Object tableObj = lua_getparam(1);
	lua_Object slotObj = lua_getparam(2);

	if (lua_isnil(tableObj) || lua_isnil(slotObj)) {
		lua_pushnil();
		return;
	}

	char buffer[kLogBufferSize];

	if (!g_grim->_dialogLog->copyLine(luaFtol(lua_getnumber(slotObj)), buffer, kLogBufferSize, kLogLineLength)) {
		warning("Lua_V1::DialogLogGetLine(): there is no line in that slot");
		lua_pushnil();
		return;
	}

	lua_pushstring(buffer);
}

void Lua_V1::DialogLogAddLine() {
	lua_Object lineObj = lua_getparam(1);

	if (lua_isnil(lineObj))
		return;

	const char *text = lua_getstring(lineObj);

	if (!text) {
		warning("Lua_V1::DialogLogAddLine(): the line is not a text");
		return;
	}

	g_grim->_dialogLog->addLine(text);
}

// A table for each hotspot under the pointer, the first one has the priority. The ones that walk
// have the points to walk to in path.
void Lua_V1::QueryActiveHotspots() {
	int mode = luaFtol(lua_getnumber(lua_getparam(1)));
	SputMouse *mouse = g_grim->_sputMouse;
	const Hotspot *hotspots[kMaxActiveHotspots];
	int x;
	int y;

	mouse->getCanvasPosition(&x, &y);

	int numHotspots = mouse->getHotspots()->getActiveHotspots(x, y, hotspots, kMaxActiveHotspots, mode);
	lua_Object result = lua_createtable();

	lua_pushobject(result);

	for (int i = 0; i < numHotspots; i++) {
		const Hotspot *hotspot = hotspots[i];

		lua_pushnumber((float)i);

		lua_Object entry = lua_createtable();

		// The points of a walk hotspot are the first key of the table
		if (hotspot->type == HotspotManager::kTypeWalk) {
			lua_pushobject(entry);
			lua_pushstring("path");

			lua_Object path = lua_createtable();

			lua_pushobject(path);

			for (int j = 0; j < hotspot->pathLen; j++) {
				lua_pushnumber((float)j);
				lua_pushobject(makeVectorTable(hotspot->path[j]));
				lua_settable();
				lua_pushobject(path);
			}

			lua_settable();
		}

		lua_pushobject(entry);
		lua_pushstring("wait");
		lua_pushnumber(hotspot->wait);
		lua_settable();
		lua_pushobject(entry);
		lua_pushstring("id");
		lua_pushstring(hotspot->id);
		lua_settable();
		lua_pushobject(entry);
		lua_pushstring("handle");
		lua_pushnumber((float)hotspot->handle);
		lua_settable();
		lua_pushobject(entry);
		lua_pushstring("type");

		if (hotspot->type == HotspotManager::kTypeWalk)
			lua_pushstring("walk");
		else if (hotspot->type == HotspotManager::kTypeOverlay)
			lua_pushstring("overlay");
		else if (hotspot->type == HotspotManager::kTypeSpecial)
			lua_pushstring("special");
		else
			lua_pushstring("normal");

		lua_settable();
		lua_pushobject(entry);
		lua_pushstring("cursor");
		lua_pushnumber((float)hotspot->cursor);
		lua_settable();
		lua_pushobject(entry);
		lua_settable();
		lua_pushobject(result);
	}
}

void Lua_V1::GetFloorWalkPos() {
	lua_Object flagObj = lua_getparam(1);
	bool onGround = lua_isnumber(flagObj) && lua_getnumber(flagObj) > 0.0f;
	RdVector3 pos;

	if (g_grim->_sputMouse->getFloorWalkPos(onGround, g_grim->_sputRender, pos))
		lua_pushobject(makeVectorTable(pos));
	else
		lua_pushnil();
}

void Lua_V1::GetSelectedDialog() {
	int x = luaFtol(lua_getnumber(lua_getparam(1)));
	int y = luaFtol(lua_getnumber(lua_getparam(2)));
	int height = luaFtol(lua_getnumber(lua_getparam(3)));
	int numLines = luaFtol(lua_getnumber(lua_getparam(4)));

	lua_pushnumber((float)g_grim->_sputMouse->getSelectedDialog(x, y, height, numLines));
}

void Lua_V1::SetCursor() {
	g_grim->_sputMouse->setCursor(luaFtol(lua_getnumber(lua_getparam(1))));
}

// Position of the actor along an axis that the pointer points to, and which side of the actor it is.
// The actor is the one of handle 0 when the first parameter is not an actor.
void Lua_V1::ProjectToAxis() {
	SputActor *actor = getSputActor(lua_getparam(1));
	RdVector3 axis;
	float scalar = 0.0f;
	float parameter = 0.0f;

	axis[0] = lua_getnumber(lua_getparam(2));
	axis[1] = lua_getnumber(lua_getparam(3));
	axis[2] = lua_getnumber(lua_getparam(4));

	if (actor)
		g_grim->_sputMouse->projectToAxis(g_grim->_sputRender, actor->actorPos, axis, lua_getnumber(lua_getparam(5)), &scalar, &parameter);

	lua_pushnumber(scalar);
	lua_pushnumber(parameter);
}

void Lua_V1::ShowCursor() {
	g_grim->_sputMouse->showCursor(luaFtol(lua_getnumber(lua_getparam(1))) != 0);
}

// AddHotspot(id, x, y, width, height, cursor, boost, menu type, hover image, hover layer, coords)
void Lua_V1::AddHotspot() {
	const char *id = lua_getstring(lua_getparam(1));

	if (!id) {
		warning("Lua_V1::AddHotspot(): the id is not a text");
		lua_pushnil();
		return;
	}

	lua_Object menuObj = lua_getparam(8);
	lua_Object hoverObj = lua_getparam(9);
	lua_Object layerObj = lua_getparam(10);
	int x = luaFtol(lua_getnumber(lua_getparam(2)));
	int y = luaFtol(lua_getnumber(lua_getparam(3)));
	int width = luaFtol(lua_getnumber(lua_getparam(4)));
	int height = luaFtol(lua_getnumber(lua_getparam(5)));
	int cursor = luaFtol(lua_getnumber(lua_getparam(6)));
	int boost = luaFtol(lua_getnumber(lua_getparam(7)));
	int menuType = lua_isnumber(menuObj) ? luaFtol(lua_getnumber(menuObj)) : 0;
	const char *hoverImage = lua_isstring(hoverObj) ? lua_getstring(hoverObj) : "";
	int hoverLayer = lua_isnumber(layerObj) ? luaFtol(lua_getnumber(layerObj)) : 2;
	int coords = luaFtol(lua_getnumber(lua_getparam(11)));

	lua_pushnumber((float)g_grim->_sputMouse->getHotspots()->addHotspot(id, x, y, width, height, cursor, boost, menuType,
			hoverImage, hoverLayer, coords));
}

void Lua_V1::RemoveHotspot() {
	g_grim->_sputMouse->getHotspots()->removeHotspot(luaFtol(lua_getnumber(lua_getparam(1))));
}

void Lua_V1::GetCursorPosition() {
	int x;
	int y;

	g_grim->_sputMouse->getCanvasPosition(&x, &y);
	lua_pushnumber((float)x);
	lua_pushnumber((float)y);
}

void Lua_V1::CursorAnimation() {
	int first = luaFtol(lua_getnumber(lua_getparam(1)));
	int last = luaFtol(lua_getnumber(lua_getparam(2)));

	g_grim->_sputMouse->cursorAnimation(first, last);
}

// UpdateHotspot(id, x, y, coords)
void Lua_V1::UpdateHotspot() {
	const char *id = lua_getstring(lua_getparam(1));
	int coords = luaFtol(lua_getnumber(lua_getparam(4)));
	int x = luaFtol(lua_getnumber(lua_getparam(2)));
	int y = luaFtol(lua_getnumber(lua_getparam(3)));

	if (!id) {
		warning("Lua_V1::UpdateHotspot(): the id is not a text");
		return;
	}

	g_grim->_sputMouse->getHotspots()->updateHotspot(id, x, y, coords);
}

// LinkHotspot(id, actor): the hotspot follows the actor, anything else unlinks it
void Lua_V1::LinkHotspot() {
	const char *id = lua_getstring(lua_getparam(1));
	lua_Object actorObj = lua_getparam(2);
	int actorHandle = -1;

	if (isSputActor(actorObj))
		actorHandle = lua_getuserdata(actorObj);

	if (!id) {
		warning("Lua_V1::LinkHotspot(): the id is not a text");
		return;
	}

	g_grim->_sputMouse->getHotspots()->linkHotspot(id, actorHandle);
}

void Lua_V1::PreloadCursors() {
	g_grim->_sputMouse->preloadCursors();
}

void Lua_V1::CursorMovieEnabled() {
	g_grim->_sputMouse->setVisibleInMovie(luaFtol(lua_getnumber(lua_getparam(1))));
}

} // end of namespace Grim
