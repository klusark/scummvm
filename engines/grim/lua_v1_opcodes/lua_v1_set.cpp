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

#include "math/matrix3.h"

#include "engines/grim/debug.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/grim.h"
#include "engines/grim/res.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/sputobjectstate.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/walkplane.h"
#include "engines/grim/lua/lauxlib.h"

namespace Grim {

// Sector functions
/* Find the sector (of any type) which contains
 * the requested coordinate (x,y,z).
 */
void Lua_V1::GetPointSector() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object zObj = lua_getparam(3);
	lua_Object typeObj = lua_getparam(4);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		lua_pushnil();
		return;
	}

	int sectorType = 4096;

	if (!lua_isnil(typeObj))
		sectorType = luaFtol(lua_getnumber(typeObj));

	float point[3];
	point[0] = lua_getnumber(xObj);
	point[1] = lua_getnumber(yObj);
	point[2] = lua_getnumber(zObj);

	WalkPlane *plane = g_grim->_walkSystem->planeAt(point, sectorType);

	if (plane) {
		lua_pushnumber((float)plane->_number);
		lua_pushstring(plane->_name);
		lua_pushnumber((float)plane->_type);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::GetActorSector() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object typeObj = lua_getparam(2);

	if (!isSputActor(actorObj) || !lua_isnumber(typeObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	WalkPlane *plane = g_grim->_walkSystem->planeAt(actor->actorPos, luaFtol(lua_getnumber(typeObj)));

	if (plane) {
		lua_pushnumber((float)plane->_number);
		lua_pushstring(plane->_name);
		lua_pushnumber((float)plane->_type);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::IsActorInSector() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object nameObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = nullptr;
	const char *name = nullptr;
	WalkPlane *plane = nullptr;

	if (lua_isstring(nameObj)) {
		actor = getSputActor(actorObj);
		name = lua_getstring(nameObj);

		if (actor) {
			plane = g_grim->_walkSystem->getHead();

			while (plane && (!strstr(plane->_name, name) || !plane->within(actor->actorPos, 1)))
				plane = plane->_next;
		}
	}

	if (plane) {
		lua_pushnumber((float)plane->_number);
		lua_pushstring(plane->_name);
		lua_pushnumber((float)plane->_type);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::IsPointInSector() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object zObj = lua_getparam(3);
	lua_Object nameObj = lua_getparam(4);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj) || !lua_isstring(nameObj)) {
		lua_pushnil();
		return;
	}

	float point[3];
	point[0] = lua_getnumber(xObj);
	point[1] = lua_getnumber(yObj);
	point[2] = lua_getnumber(zObj);
	const char *name = lua_getstring(nameObj);
	WalkPlane *plane = g_grim->_walkSystem->getHead();

	while (plane && (!strstr(plane->_name, name) || !plane->within(point, 1)))
		plane = plane->_next;

	if (plane) {
		lua_pushnumber((float)plane->_number);
		lua_pushstring(plane->_name);
		lua_pushnumber((float)plane->_type);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::GetSectorOppositeEdge() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);
	lua_Object nameObj = lua_getparam(2);
	WalkPlane *plane = nullptr;

	if (lua_isstring(nameObj))
		plane = g_grim->_walkSystem->planeNamed(lua_getstring(nameObj));

	if (!plane) {
		lua_pushnil();
		return;
	}

	if (actor && hasActorExtra(actor, "GetSectorOppositeEdge"))
		g_grim->_sputActor->leaveSector(actor, plane);
}

void Lua_V1::MakeSectorActive() {
	lua_Object sectorObj = lua_getparam(1);

	if (!lua_isnumber(sectorObj) && !lua_isstring(sectorObj))
		return;

	lua_Object visibleObj = lua_getparam(2);
	WalkPlane *plane;

	if (lua_isstring(sectorObj))
		plane = g_grim->_walkSystem->planeNamed(lua_getstring(sectorObj));
	else
		plane = g_grim->_walkSystem->planeNumbered(luaFtol(lua_getnumber(sectorObj)));

	if (plane) {
		plane->_active = lua_isnil(visibleObj) ? 0 : 1;
		g_grim->_walkSystem->markPathFinderDirty();
	}
}

void Lua_V1::IsSectorActive() {
	lua_Object sectorObj = lua_getparam(1);

	if ((lua_isnumber(sectorObj) || lua_isstring(sectorObj)) && g_grim->_sputRoom->getCurrentRoom()) {
		WalkPlane *plane;

		if (lua_isstring(sectorObj))
			plane = g_grim->_walkSystem->planeNamed(lua_getstring(sectorObj));
		else
			plane = g_grim->_walkSystem->planeNumbered(luaFtol(lua_getnumber(sectorObj)));

		if (plane && plane->_active)
			lua_pushnumber(1.0f);
	}

	// The original always pushes the nil after the result
	lua_pushnil();
}

// Set functions
void Lua_V1::LockSet() {
	lua_Object nameObj = lua_getparam(1);
	if (!lua_isstring(nameObj))
		return;

	Room *room = static_cast<Room *>(g_grim->_sputResource->alloc(kSputTagSet, lua_getstring(nameObj)));

	if (room && !room->roomRef.valid)
		g_grim->_res->setReference(&room->roomRef, room);
}

void Lua_V1::UnLockSet() {
	lua_Object nameObj = lua_getparam(1);
	if (!lua_isstring(nameObj))
		return;

	Room *room = static_cast<Room *>(g_grim->_sputResource->alloc(kSputTagSet, lua_getstring(nameObj)));

	if (room && room->roomRef.valid)
		g_grim->_res->removeReference(&room->roomRef);
}

void Lua_V1::LockSetDemo() {
	warning("LockSet: Set handles no longer allowed, use filename!");
}

void Lua_V1::UnlockSetDemo() {
	warning("UnlockSet: Set handles no longer allowed.");
}

void Lua_V1::MakeCurrentSet() {
	lua_Object nameObj = lua_getparam(1);

	if (lua_isstring(nameObj))
		g_grim->makeCurrentSet(lua_getstring(nameObj));
	else
		g_grim->makeCurrentSet(nullptr);
}

void Lua_V1::MakeCurrentSetup() {
	lua_Object setupObj = lua_getparam(1);
	if (!lua_isnumber(setupObj))
		return;

	g_grim->makeCurrentSetupLib((uint)luaFtol(lua_getnumber(setupObj)));
}

void Lua_V1::GetCurrentSetup() {
	lua_Object nameObj = lua_getparam(1);
	if (!lua_isstring(nameObj))
		return;

	const char *name = lua_getstring(nameObj);

	if (scumm_stricmp(name, g_grim->_sputRoom->getCurrentRoomName()) != 0) {
		if (g_grim->_res->loaded(kSputTagSet, name)) {
			Room *room = static_cast<Room *>(g_grim->_sputResource->alloc(kSputTagSet, name));

			if (room)
				lua_pushnumber((float)room->curSetup);
		}
	} else if (g_grim->_sputRoom->getCurrentRoom()) {
		lua_pushnumber((float)g_grim->_sputRoom->getCurrentRoom()->curSetup);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::PreviousSetup() {
	g_grim->previousSetup();
}

void Lua_V1::NextSetup() {
	g_grim->nextSetup();
}

void Lua_V1::GetCameraLookVector() {
	lua_Object setupObj = lua_getparam(1);
	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (!room) {
		lua_pushnil();
		return;
	}

	int setupIndex;

	if (lua_isnumber(setupObj))
		setupIndex = luaFtol(lua_getnumber(setupObj));
	else
		setupIndex = room->curSetup;

	if (setupIndex < 0 || (uint)setupIndex >= room->numSetups) {
		warning("GetCameraLookVector(): invalid setup %d", setupIndex);
		lua_pushnil();
		return;
	}

	RoomCamera *camera = &room->setups[setupIndex].camera;
	RdVector3 lookVector;

	lookVector[0] = camera->position[0];
	lookVector[1] = camera->position[1];
	lookVector[2] = camera->position[2];
	lookVector[0] = (float)((double)lookVector[0] - camera->interest[0]);
	lookVector[1] = (float)((double)lookVector[1] - camera->interest[1]);
	lookVector[2] = (float)((double)lookVector[2] - camera->interest[2]);
	g_grim->_rd->normalize3Acc(lookVector);

	lua_Object result = lua_createtable();
	lua_pushobject(result);
	lua_pushstring("x");
	lua_pushnumber(lookVector[0]);
	lua_settable();
	lua_pushobject(result);
	lua_pushstring("y");
	lua_pushnumber(lookVector[1]);
	lua_settable();
	lua_pushobject(result);
	lua_pushstring("z");
	lua_pushnumber(lookVector[2]);
	lua_settable();
	lua_pushobject(result);
}

/* This function makes the walkplane sectors smaller by the
 * given size. This is used when manny is holding some big
 * thing, like his scythe, that is likely to clip with the
 * things around him. The sectors are still connected, but they
 * have a margin which prevents manny to go too close to the
 * sectors edges.
 */
void Lua_V1::ShrinkBoxes() {
	lua_Object sizeObj = lua_getparam(1);

	if (lua_isnil(sizeObj))
		return;

	UnShrinkBoxes();
	g_grim->_walkSystem->buildCache();
	g_grim->_walkSystem->shrinkBoxes(lua_getnumber(sizeObj));
}

void Lua_V1::UnShrinkBoxes() {
	g_grim->_walkSystem->restoreCache();
	g_grim->_walkSystem->destroyCache();
}

/* Given a position and a size this function calculates and pushes
 * the nearest point to that which will be valid if the boxes are
 * shrunk by the amount specified.
 */
void Lua_V1::GetShrinkPos() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object zObj = lua_getparam(3);
	lua_Object rObj = lua_getparam(4);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj) || !lua_isnumber(rObj))
		return;

	RdVector3 pos;
	float radius;

	pos[0] = lua_getnumber(xObj);
	pos[1] = lua_getnumber(yObj);
	pos[2] = lua_getnumber(zObj);
	radius = lua_getnumber(rObj);

	g_grim->_walkSystem->buildCache();
	g_grim->_walkSystem->shrinkBoxes(radius);

	WalkPlane *plane = g_grim->_walkSystem->planeClosest(pos, 4096);

	if (plane)
		plane->closestPoint(pos);

	UnShrinkBoxes();

	if (plane) {
		lua_pushnumber(pos[0]);
		lua_pushnumber(pos[1]);
		lua_pushnumber(pos[2]);
	} else {
		lua_pushnil();
	}
}

int Lua_V1::getObjectStateHandle(SputObjectState *state) {
	Lua_V1 *lua = g_grim->_luaV1;

	if (!state->luaHandle)
		state->luaHandle = lua->_nextObjectStateHandle++;

	return state->luaHandle;
}

SputObjectState *Lua_V1::findObjectState(int handle, int *setupPointer, int *indexPointer) {
	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (!room || !handle)
		return nullptr;

	for (uint setup = 0; setup < room->numSetups; setup++) {
		int index = 0;

		for (SputObjectState *state = room->setups[setup].ObjectStates; state; state = state->next) {
			if (state->luaHandle == handle) {
				if (setupPointer)
					*setupPointer = (int)setup;

				if (indexPointer)
					*indexPointer = index;

				return state;
			}

			index++;
		}
	}

	return nullptr;
}

void Lua_V1::NewObjectState() {
	lua_Object setupObj = lua_getparam(1);
	lua_Object typeObj = lua_getparam(2);
	lua_Object bitmapObj = lua_getparam(3);
	lua_Object zBitmapObj = lua_getparam(4);
	lua_Object copyModeObj = lua_getparam(5);
	int copyMode = -1;

	if (!lua_isnil(copyModeObj))
		copyMode = luaFtol(lua_getnumber(copyModeObj));

	const char *zBitmap = lua_getstring(zBitmapObj);
	const char *bitmap = lua_getstring(bitmapObj);
	int type = luaFtol(lua_getnumber(typeObj));
	int setup = luaFtol(lua_getnumber(setupObj));

	SputObjectState *state = g_grim->_sputRoom->addObjectState(setup, type, bitmap, zBitmap, copyMode);

	if (state)
		lua_pushusertag(getObjectStateHandle(state), MKTAG('T','A','T','S'));
	else
		lua_pushnil();
}

void Lua_V1::FreeObjectState() {
	lua_Object stateObj = lua_getparam(1);

	if (!lua_isuserdata(stateObj) || lua_tag(stateObj) != MKTAG('T','A','T','S'))
		return;

	SputObjectState *state = findObjectState(lua_getuserdata(stateObj), nullptr, nullptr);

	if (state)
		g_grim->_sputRoom->removeObjectState(state);
}

void Lua_V1::SendObjectToBack() {
	lua_Object stateObj = lua_getparam(1);

	if (!lua_isuserdata(stateObj) || lua_tag(stateObj) != MKTAG('T','A','T','S'))
		return;

	int setup;
	SputObjectState *state = findObjectState(lua_getuserdata(stateObj), &setup, nullptr);

	if (state) {
		RoomSetup *setupPointer = &g_grim->_sputRoom->getCurrentRoom()->setups[setup];

		setupPointer->ObjectStates = state->moveToBack(setupPointer->ObjectStates);
	}
}

void Lua_V1::SendObjectToFront() {
	lua_Object stateObj = lua_getparam(1);

	if (!lua_isuserdata(stateObj) || lua_tag(stateObj) != MKTAG('T','A','T','S'))
		return;

	int setup;
	SputObjectState *state = findObjectState(lua_getuserdata(stateObj), &setup, nullptr);

	if (state) {
		RoomSetup *setupPointer = &g_grim->_sputRoom->getCurrentRoom()->setups[setup];

		setupPointer->ObjectStates = state->moveToFront(setupPointer->ObjectStates);
	}
}

void Lua_V1::SetObjectType() {
	lua_Object stateObj = lua_getparam(1);
	lua_Object typeObj = lua_getparam(2);

	if (!lua_isuserdata(stateObj) || lua_tag(stateObj) != MKTAG('T','A','T','S'))
		return;

	SputObjectState *state = findObjectState(lua_getuserdata(stateObj), nullptr, nullptr);

	if (state)
		state->type = luaFtol(lua_getnumber(typeObj));
}

void Lua_V1::SetAmbientLight() {
	lua_Object ambientObj = lua_getparam(1);

	if (!lua_isnumber(ambientObj))
		return;

	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (room)
		g_grim->_rd->_camera.setAmbientLight(room->setups[room->curSetup].camera.prdCamera, lua_getnumber(ambientObj));
}

void Lua_V1::SetLightIntensity() {
	lua_Object lightObj = lua_getparam(1);
	lua_Object intensityObj = lua_getparam(2);
	Room *room = g_grim->_sputRoom->getCurrentRoom();
	int lightIndex = -1;

	if (lua_isnumber(lightObj)) {
		lightIndex = luaFtol(lua_getnumber(lightObj));
	} else if (lua_isstring(lightObj)) {
		lightIndex = g_grim->_sputRoom->findLightByName(nullptr, lua_getstring(lightObj));
	} else {
		return;
	}

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (!light || !lua_isnumber(intensityObj))
		return;

	float intensity = lua_getnumber(intensityObj);
	light->light.intensity = intensity;

	for (uint s = 0; s < room->numSetups; s++) {
		RdCamera *camera = room->setups[s].camera.prdCamera;

		if (!camera || !camera->lights[lightIndex]) {
			warning("SetLightIntensity(): setup %u has no camera light %d", s, lightIndex);
			continue;
		}

		camera->lights[lightIndex]->intensity = intensity;
	}
}

void Lua_V1::SetLightPosition() {
	lua_Object lightObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	int lightIndex = -1;

	if (lua_isnumber(lightObj)) {
		lightIndex = luaFtol(lua_getnumber(lightObj));
	} else if (lua_isstring(lightObj)) {
		lightIndex = g_grim->_sputRoom->findLightByName(nullptr, lua_getstring(lightObj));
	} else {
		return;
	}

	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (!room)
		return;

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (!light || !lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj))
		return;

	float x = lua_getnumber(xObj);
	float y = lua_getnumber(yObj);
	float z = lua_getnumber(zObj);

	light->position[0] = x;
	light->position[1] = y;
	light->position[2] = z;

	for (uint s = 0; s < room->numSetups; s++) {
		RdCamera *camera = room->setups[s].camera.prdCamera;

		if (!camera) {
			warning("SetLightPosition(): setup %u has no camera", s);
			continue;
		}

		camera->lightPos[lightIndex][0] = x;
		camera->lightPos[lightIndex][1] = y;
		camera->lightPos[lightIndex][2] = z;
	}
}

void Lua_V1::TurnLightOn() {
	lua_Object lightObj = lua_getparam(1);
	lua_Object onObj = lua_getparam(2);
	int lightIndex = -1;

	if (lua_isnumber(lightObj)) {
		lightIndex = luaFtol(lua_getnumber(lightObj));
	} else if (lua_isstring(lightObj)) {
		lightIndex = g_grim->_sputRoom->findLightByName(nullptr, lua_getstring(lightObj));
	} else {
		return;
	}

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (light)
		light->light.active = !lua_isnil(onObj);
}

void Lua_V1::GetCameraPosition() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("GetCameraPosition(): no camera");
		lua_pushnil();
		return;
	}

	lua_pushnumber(camera->position[0]);
	lua_pushnumber(camera->position[1]);
	lua_pushnumber(camera->position[2]);
}

void Lua_V1::SetCameraPosition() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("SetCameraPosition(): no camera");
		lua_pushnil();
		return;
	}

	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(1);
	lua_Object zObj = lua_getparam(1);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj))
		lua_pushnil();

	camera->position[0] = lua_getnumber(xObj);
	camera->position[1] = lua_getnumber(yObj);
	camera->position[2] = lua_getnumber(zObj);
	g_grim->_rd->lookAt(camera->posMatrix, camera->position, camera->interest, camera->roll);
	g_grim->_rd->_camera.update(camera->posMatrix);
}

void Lua_V1::SetCameraInterest() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("SetCameraInterest(): no camera");
		lua_pushnil();
		return;
	}

	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(1);
	lua_Object zObj = lua_getparam(1);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj))
		lua_pushnil();

	camera->interest[0] = lua_getnumber(xObj);
	camera->interest[1] = lua_getnumber(yObj);
	camera->interest[2] = lua_getnumber(zObj);
	g_grim->_rd->lookAt(camera->posMatrix, camera->position, camera->interest, camera->roll);
	g_grim->_rd->_camera.update(camera->posMatrix);
}

void Lua_V1::GetCameraFOV() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("GetCameraFOV(): no camera");
		lua_pushnil();
		return;
	}

	lua_pushnumber(camera->fov);
}

void Lua_V1::SetCameraFOV() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("SetCameraFOV(): no camera");
		lua_pushnil();
		return;
	}

	lua_Object fovObj = lua_getparam(1);

	if (!lua_isnumber(fovObj))
		lua_pushnil();

	camera->fov = lua_getnumber(fovObj);
	camera->prdCamera->fov = camera->fov;
}

void Lua_V1::GetCameraRoll() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("GetCameraRoll(): no camera");
		lua_pushnil();
		return;
	}

	lua_pushnumber(camera->roll);
}

void Lua_V1::SetCameraRoll() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("SetCameraRoll(): no camera");
		lua_pushnil();
		return;
	}

	lua_Object rollObj = lua_getparam(1);

	if (!lua_isnumber(rollObj))
		lua_pushnil();

	camera->roll = lua_getnumber(rollObj);
	g_grim->_rd->lookAt(camera->posMatrix, camera->position, camera->interest, camera->roll);
	g_grim->_rd->_camera.update(camera->posMatrix);
}

} // end of namespace Grim
