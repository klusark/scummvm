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
	kMaxActiveHotspots = 10
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

	float axisX;
	float axisY;

	g_grim->_sputMouse->getAxes(&axisX, &axisY);
	g_grim->_sputActor->walkActorToMouseTarget(actor, axisX, axisY);
}

void Lua_V1::PathfinderMode() {
	lua_Object modeObj = lua_getparam(1);

	g_grim->_sputActor->setPathfinderMode(lua_isnumber(modeObj) && lua_getnumber(modeObj) != 0.0f);
}

// Table with the line numbers of the log (from 0) and the number of lines in count
void Lua_V1::DialogLogGetLineCount() {
	DialogLog *dialogLog = g_grim->_dialogLog;
	int numLines = dialogLog->getLineCount();
	lua_Object result = lua_createtable();

	for (int i = 0; i < numLines; i++) {
		lua_pushobject(result);
		lua_pushnumber((float)i);
		lua_pushnumber((float)dialogLog->getLineIndex(i));
		lua_settable();
	}

	lua_pushobject(result);
	lua_pushstring("count");
	lua_pushnumber((float)numLines);
	lua_settable();
	lua_pushobject(result);
}

// The first parameter is the table of the line count and is not used
void Lua_V1::DialogLogGetLine() {
	lua_Object indexObj = lua_getparam(2);

	if (!lua_isnumber(indexObj)) {
		lua_pushnil();
		return;
	}

	const char *line = g_grim->_dialogLog->getLine(luaFtol(lua_getnumber(indexObj)));

	if (line)
		lua_pushstring(line);
	else
		lua_pushnil();
}

void Lua_V1::DialogLogAddLine() {
	lua_Object lineObj = lua_getparam(1);

	if (lua_isstring(lineObj))
		g_grim->_dialogLog->addLine(lua_getstring(lineObj));
}

// A table for each hotspot under the pointer, the first one has the priority. The ones that walk
// have the points to walk to in path.
void Lua_V1::QueryActiveHotspots() {
	lua_Object modeObj = lua_getparam(1);
	int mode = lua_isnumber(modeObj) ? luaFtol(lua_getnumber(modeObj)) : 0;
	SputMouse *mouse = g_grim->_sputMouse;
	const Hotspot *hotspots[kMaxActiveHotspots];
	int x;
	int y;

	mouse->getCanvasPosition(&x, &y);

	int numHotspots = mouse->getHotspots()->getActiveHotspots(x, y, hotspots, kMaxActiveHotspots, mode);
	lua_Object result = lua_createtable();

	for (int i = 0; i < numHotspots; i++) {
		const Hotspot *hotspot = hotspots[i];
		lua_Object entry = lua_createtable();
		const char *typeName;

		if (hotspot->type == HotspotManager::kTypeWalk)
			typeName = "walk";
		else if (hotspot->type == HotspotManager::kTypeOverlay)
			typeName = "overlay";
		else if (hotspot->type == HotspotManager::kTypeSpecial)
			typeName = "special";
		else
			typeName = "normal";

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
		lua_pushstring(typeName);
		lua_settable();
		lua_pushobject(entry);
		lua_pushstring("cursor");
		lua_pushnumber((float)hotspot->cursor);
		lua_settable();

		if (hotspot->type == HotspotManager::kTypeWalk) {
			lua_Object path = lua_createtable();

			for (int j = 0; j < hotspot->pathLen; j++) {
				lua_Object point = makeVectorTable(hotspot->path[j]);

				lua_pushobject(path);
				lua_pushnumber((float)j);
				lua_pushobject(point);
				lua_settable();
			}

			lua_pushobject(entry);
			lua_pushstring("path");
			lua_pushobject(path);
			lua_settable();
		}

		lua_pushobject(result);
		lua_pushnumber((float)i);
		lua_pushobject(entry);
		lua_settable();
	}

	lua_pushobject(result);
}

void Lua_V1::GetFloorWalkPos() {
	lua_Object flagObj = lua_getparam(1);
	bool onGround = lua_isnumber(flagObj) && lua_getnumber(flagObj) > 0.0f;
	RdVector3 pos;

	if (g_grim->_sputMouse->getFloorWalkPos(onGround, pos))
		lua_pushobject(makeVectorTable(pos));
	else
		lua_pushnil();
}

void Lua_V1::GetSelectedDialog() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object heightObj = lua_getparam(3);
	lua_Object countObj = lua_getparam(4);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(heightObj) || !lua_isnumber(countObj)) {
		lua_pushnumber(0.0f);
		return;
	}

	int selected = g_grim->_sputMouse->getSelectedDialog(luaFtol(lua_getnumber(xObj)), luaFtol(lua_getnumber(yObj)),
			luaFtol(lua_getnumber(heightObj)), luaFtol(lua_getnumber(countObj)));

	lua_pushnumber((float)selected);
}

void Lua_V1::SetCursor() {
	lua_Object cursorObj = lua_getparam(1);

	if (lua_isnumber(cursorObj))
		g_grim->_sputMouse->setCursor(luaFtol(lua_getnumber(cursorObj)));
}

// Position of the actor along an axis that the pointer points to, and which side of the actor it is
void Lua_V1::ProjectToAxis() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object axisXObj = lua_getparam(2);
	lua_Object axisYObj = lua_getparam(3);
	lua_Object axisZObj = lua_getparam(4);
	lua_Object offsetObj = lua_getparam(5);

	if (!isSputActor(actorObj) || !lua_isnumber(axisXObj) || !lua_isnumber(axisYObj) || !lua_isnumber(axisZObj) || !lua_isnumber(offsetObj)) {
		warning("Lua_V1::ProjectToAxis(): invalid arguments");
		lua_pushnumber(0.0f);
		lua_pushnumber(0.0f);
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnumber(0.0f);
		lua_pushnumber(0.0f);
		return;
	}

	RdVector3 axis;
	float scalar;
	float parameter;

	axis[0] = lua_getnumber(axisXObj);
	axis[1] = lua_getnumber(axisYObj);
	axis[2] = lua_getnumber(axisZObj);
	g_grim->_sputMouse->projectToAxis(actor->actorPos, axis, lua_getnumber(offsetObj), &scalar, &parameter);
	lua_pushnumber(scalar);
	lua_pushnumber(parameter);
}

void Lua_V1::ShowCursor() {
	lua_Object showObj = lua_getparam(1);

	g_grim->_sputMouse->showCursor(lua_isnumber(showObj) && lua_getnumber(showObj) != 0.0f);
}

// AddHotspot(id, x, y, width, height, cursor, boost, menu type, hover image, hover layer, coords)
void Lua_V1::AddHotspot() {
	lua_Object idObj = lua_getparam(1);

	if (!lua_isstring(idObj)) {
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
	const char *hoverImage = lua_isstring(hoverObj) ? lua_getstring(hoverObj) : nullptr;
	int hoverLayer = lua_isnumber(layerObj) ? luaFtol(lua_getnumber(layerObj)) : 1;
	int coords = luaFtol(lua_getnumber(lua_getparam(11)));
	int handle = g_grim->_sputMouse->getHotspots()->addHotspot(lua_getstring(idObj), x, y, width, height, cursor, boost, menuType,
			hoverImage, hoverLayer, coords);

	if (handle >= 0)
		lua_pushnumber((float)handle);
	else
		lua_pushnil();
}

void Lua_V1::RemoveHotspot() {
	lua_Object handleObj = lua_getparam(1);

	if (lua_isnumber(handleObj))
		g_grim->_sputMouse->getHotspots()->removeHotspot(luaFtol(lua_getnumber(handleObj)));
}

void Lua_V1::GetCursorPosition() {
	int x;
	int y;

	g_grim->_sputMouse->getCanvasPosition(&x, &y);
	lua_pushnumber((float)x);
	lua_pushnumber((float)y);
}

void Lua_V1::CursorAnimation() {
	lua_Object firstObj = lua_getparam(1);
	lua_Object lastObj = lua_getparam(2);

	if (lua_isnumber(firstObj) && lua_isnumber(lastObj))
		g_grim->_sputMouse->cursorAnimation(luaFtol(lua_getnumber(firstObj)), luaFtol(lua_getnumber(lastObj)));
}

// UpdateHotspot(id, x, y, coords)
void Lua_V1::UpdateHotspot() {
	lua_Object idObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object coordsObj = lua_getparam(4);

	if (!lua_isstring(idObj) || !lua_isnumber(xObj) || !lua_isnumber(yObj))
		return;

	int coords = lua_isnumber(coordsObj) ? luaFtol(lua_getnumber(coordsObj)) : 0;

	g_grim->_sputMouse->getHotspots()->updateHotspot(lua_getstring(idObj), luaFtol(lua_getnumber(xObj)), luaFtol(lua_getnumber(yObj)), coords);
}

// LinkHotspot(id, actor): the hotspot follows the actor, nil unlinks it
void Lua_V1::LinkHotspot() {
	lua_Object idObj = lua_getparam(1);
	lua_Object actorObj = lua_getparam(2);

	if (!lua_isstring(idObj))
		return;

	int actorIndex = -1;

	if (isSputActor(actorObj)) {
		SputActor *actor = getSputActor(actorObj);

		if (actor)
			actorIndex = g_grim->_sputActor->getActorHandle(actor);
	}

	g_grim->_sputMouse->getHotspots()->linkHotspot(lua_getstring(idObj), actorIndex);
}

void Lua_V1::PreloadCursors() {
	g_grim->_sputMouse->preloadCursors();
}

void Lua_V1::CursorMovieEnabled() {
	lua_Object enableObj = lua_getparam(1);

	g_grim->_sputMouse->setVisibleInMovie(lua_isnumber(enableObj) && lua_getnumber(enableObj) != 0.0f);
}

} // end of namespace Grim
