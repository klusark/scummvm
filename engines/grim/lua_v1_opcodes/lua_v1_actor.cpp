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

#include "engines/grim/grim.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/chore/cos.h"
#include "engines/grim/puck.h"
#include "engines/grim/res.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_math.h"
#include "engines/grim/walkplane.h"
#include "engines/grim/lua/lauxlib.h"

namespace Grim {

bool Lua_V1::isSputActor(lua_Object obj) {
	return lua_isuserdata(obj) && lua_tag(obj) == MKTAG('R','T','C','A');
}

SputActor *Lua_V1::getSputActor(lua_Object obj) {
	return g_grim->_sputActor->actorDeref(lua_getuserdata(obj));
}

void Lua_V1::pushSputActor(int handle) {
	lua_pushusertag(handle, MKTAG('R','T','C','A'));
}

bool Lua_V1::hasActorExtra(SputActor *actor, const char *opcode) {
	if (actor->extra)
		return true;

	warning("%s(): the actor has no extra data", opcode);
	return false;
}

bool Lua_V1::findCostumeInStack(lua_Object obj, CostumeNode *stack, CostumeNode **found) {
	*found = stack;

	if (lua_isnil(obj))
		return true;

	if (lua_isnumber(obj)) {
		int depth = luaFtol(lua_getnumber(obj));

		if (*found) {
			while (depth > 0) {
				depth--;
				*found = (*found)->next;

				if (!*found)
					return false;
			}
		}

		return *found != nullptr;
	}

	if (lua_isstring(obj)) {
		const char *costumeName = lua_getstring(obj);
		bool demo = (g_grim->getGameFlags() & ADGF_DEMO) != 0;

		if (*found) {
			do {
				CosCostume *costume = g_grim->_sputCostume->getCos()->instGetCostume((*found)->costumeInstance, nullptr);

				if (costume && !(demo ? strcmp(costume->name, costumeName) : scumm_stricmp(costume->name, costumeName)))
					break;

				*found = (*found)->next;
			} while (*found);
		}

		return *found != nullptr;
	}

	return false;
}

void Lua_V1::LoadActor() {
	lua_Object flag20Obj = lua_getparam(2);
	lua_Object flag80Obj = lua_getparam(3);
	int flag20 = lua_isnumber(flag20Obj) && lua_getnumber(flag20Obj) != 0.0f;
	int flag80 = lua_isnumber(flag80Obj) && lua_getnumber(flag80Obj) != 0.0f;
	int handle = g_grim->_sputActor->allocateActor(flag20, flag80);

	if (handle < 0)
		lua_pushnil();
	else
		lua_pushusertag(handle, MKTAG('R','T','C','A'));
}

void Lua_V1::GetActorTimeScale() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	lua_pushnumber(actor->extra->timeScale);
}

void Lua_V1::SetSelectedActor() {
	lua_Object actorObj = lua_getparam(1);
	if (!lua_isuserdata(actorObj) || lua_tag(actorObj) != MKTAG('R','T','C','A'))
		return;

	int handle = g_grim->_sputActor->setSelectedActor(lua_getuserdata(actorObj));

	if (handle < 0)
		lua_pushnil();
	else
		lua_pushusertag(handle, MKTAG('R','T','C','A'));
}

void Lua_V1::GetCameraActor() {
	pushSputActor(g_grim->_sputRender->getCamActorIndex());
}

void Lua_V1::SetActorTalkColor() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object colorObj = lua_getparam(2);

	if (lua_isuserdata(actorObj) && lua_tag(actorObj) == MKTAG('R','T','C','A')) {
		SputActor *actor = g_grim->_sputActor->actorDeref(lua_getuserdata(actorObj));

		if (!actor)
			return;

		if (!actor->extra)
			g_grim->_sputActor->allocateActorExtra(actor);

		if (lua_isuserdata(colorObj) && lua_tag(colorObj) == MKTAG('R','L','O','C'))
			actor->extra->talkColor = lua_getuserdata(colorObj);
	}
}

void Lua_V1::GetActorTalkColor() {
	lua_Object actorObj = lua_getparam(1);
	if (lua_isuserdata(actorObj) && lua_tag(actorObj) == MKTAG('R','T','C','A')) {
		SputActor *actor = g_grim->_sputActor->actorDeref(lua_getuserdata(actorObj));

		if (actor && actor->extra) {
			lua_pushusertag(actor->extra->talkColor, MKTAG('R','L','O','C'));
			return;
		}
	}
	lua_pushnil();
}

void Lua_V1::SetActorRestChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object costumeObj = lua_getparam(3);
	CostumeNode *node;
	int chore;

	if (!isSputActor(actorObj) || (!lua_isnumber(choreObj) && !lua_isnil(choreObj)))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	// The demo has no costume parameter
	if (g_grim->getGameFlags() & ADGF_DEMO)
		node = nullptr;
	else if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (lua_isnil(choreObj))
		chore = -1;
	else
		chore = luaFtol(lua_getnumber(choreObj));

	g_grim->_sputActor->setRestChore(actor, chore, node);
}

void Lua_V1::SetActorWalkChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object costumeObj = lua_getparam(3);
	CostumeNode *node;
	int chore;

	bool demo = (g_grim->getGameFlags() & ADGF_DEMO) != 0;

	// The demo does not accept nil and has no costume parameter
	if (!isSputActor(actorObj) || (!lua_isnumber(choreObj) && (demo || !lua_isnil(choreObj))))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (demo)
		node = nullptr;
	else if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (lua_isnumber(choreObj))
		chore = luaFtol(lua_getnumber(choreObj));
	else
		chore = -1;

	g_grim->_sputActor->setWalkChore(actor, chore, node);
}

void Lua_V1::SetActorTurnChores() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object leftChoreObj = lua_getparam(2);
	lua_Object rightChoreObj = lua_getparam(3);
	lua_Object costumeObj = lua_getparam(4);
	CostumeNode *node;

	if (!isSputActor(actorObj) || !lua_isnumber(leftChoreObj) || !lua_isnumber(rightChoreObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	// The demo has no costume parameter
	if (g_grim->getGameFlags() & ADGF_DEMO)
		node = nullptr;
	else if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	int rightChore = luaFtol(lua_getnumber(rightChoreObj));
	int leftChore = luaFtol(lua_getnumber(leftChoreObj));

	g_grim->_sputActor->setTurnChores(actor, leftChore, rightChore, node);
}

void Lua_V1::SetActorTalkChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object indexObj = lua_getparam(2);
	lua_Object choreObj = lua_getparam(3);
	lua_Object costumeObj = lua_getparam(4);
	CostumeNode *node;
	int chore;

	if (!isSputActor(actorObj) || !lua_isnumber(indexObj) || (!lua_isnumber(choreObj) && !lua_isnil(choreObj)))
		return;

	int index = luaFtol(lua_getnumber(indexObj));

	if (index < 1 || index > 16)
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	// The demo has no costume parameter
	if (g_grim->getGameFlags() & ADGF_DEMO)
		node = actor->costumeStack;
	else if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (!node)
		return;

	if (lua_isnumber(choreObj))
		chore = luaFtol(lua_getnumber(choreObj));
	else
		chore = -1;

	node->chore[index - 1] = chore;
}

void Lua_V1::SetActorMumblechore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object costumeObj = lua_getparam(3);
	CostumeNode *node;

	if (!isSputActor(actorObj) || !lua_isnumber(choreObj))
		return;

	int chore = luaFtol(lua_getnumber(choreObj));
	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	// The demo has no costume parameter
	if (g_grim->getGameFlags() & ADGF_DEMO)
		node = actor->costumeStack;
	else if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (!node)
		return;

	node->mumbleChore = chore;
}

void Lua_V1::SetActorVisibility() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object visibleObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isnil(visibleObj))
		actor->flags |= 2;
	else
		actor->flags &= ~2;
}

void Lua_V1::SetActorScale() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object scaleObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isnumber(scaleObj))
		actor->scale = lua_getnumber(scaleObj);
	else
		actor->scale = 1.0f;
}

void Lua_V1::GetActorScale() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj)) {
		if (g_grim->getGameFlags() & ADGF_DEMO)
			warning("GetActorScale: Invalid actor handle!");

		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	lua_pushnumber(actor->scale);
}

void Lua_V1::SetActorTimeScale() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object scaleObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	if (lua_isnumber(scaleObj))
		actor->extra->timeScale = lua_getnumber(scaleObj);
	else
		actor->extra->timeScale = 1.0f;

	if (!(actor->extra->timeScale >= 0.0f))
		actor->extra->timeScale = -actor->extra->timeScale;
}

void Lua_V1::SetActorCollisionMode() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object modeObj  = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	actor->extra->collisionMode = luaFtol(lua_getnumber(modeObj));
}

void Lua_V1::SetActorCollisionScale() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object scaleObj  = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	actor->extra->collisionScale = lua_getnumber(scaleObj);
}

void Lua_V1::PutActorAt() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);

	if (!isSputActor(actorObj) || !lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	RdVector3 pos;
	pos[0] = lua_getnumber(xObj);
	pos[1] = lua_getnumber(yObj);
	pos[2] = lua_getnumber(zObj);

	bool clearWalk = true;

	if (g_grim->isRemastered()) {
		lua_Object keepObj = lua_getparam(5);

		if (lua_isnumber(keepObj))
			clearWalk = lua_getnumber(keepObj) <= 0.0f;
	}

	g_grim->_sputActor->setActorPosition(actor, pos, clearWalk);
}

void Lua_V1::GetActorPos() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	lua_pushnumber(actor->actorPos[0]);
	lua_pushnumber(actor->actorPos[1]);
	lua_pushnumber(actor->actorPos[2]);
}

void Lua_V1::SetActorRot() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object pitchObj = lua_getparam(2);
	lua_Object yawObj = lua_getparam(3);
	lua_Object rollObj = lua_getparam(4);
	lua_Object flipObj = lua_getparam(5);

	if (!isSputActor(actorObj) || !lua_isnumber(pitchObj) || !lua_isnumber(yawObj) || !lua_isnumber(rollObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	RdVector3 rot;
	rot[0] = lua_getnumber(pitchObj);
	rot[1] = lua_getnumber(yawObj);
	rot[2] = lua_getnumber(rollObj);
	g_grim->_sputActor->setActorRotation(actor, rot, lua_isnil(flipObj));
}

void Lua_V1::GetActorRot() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	lua_pushnumber(actor->actorRot[0]);
	lua_pushnumber(actor->actorRot[1]);
	lua_pushnumber(actor->actorRot[2]);
}

void Lua_V1::IsActorTurning() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	if (actor->extra->alignOff != 0.0f)
		lua_pushnumber(1.0f);

	lua_pushnil();
}

void Lua_V1::GetAngleBetweenActors() {
	lua_Object actor1Obj = lua_getparam(1);
	lua_Object actor2Obj = lua_getparam(2);

	if (!isSputActor(actor1Obj) || !isSputActor(actor2Obj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor1 = getSputActor(actor1Obj);
	SputActor *actor2 = getSputActor(actor2Obj);

	if (!actor1 || !actor2 || !hasActorExtra(actor1, "GetAngleBetweenActors") || !actor1->extra->puck) {
		lua_pushnil();
		return;
	}

	float *look = actor1->extra->puck->_orient[1];
	RdVector3 lookVec;
	RdVector3 targetVec;

	lookVec[0] = look[0];
	lookVec[1] = look[1];
	lookVec[2] = look[2];
	targetVec[0] = actor2->actorPos[0];
	targetVec[1] = actor2->actorPos[1];
	targetVec[2] = lookVec[2];
	targetVec[0] = (float)((double)targetVec[0] - actor1->actorPos[0]);
	targetVec[1] = (float)((double)targetVec[1] - actor1->actorPos[1]);
	targetVec[2] = (float)((double)lookVec[2] - actor1->actorPos[2]);

	g_grim->_rd->normalize3Acc(lookVec);
	g_grim->_rd->normalize3Acc(targetVec);

	float dot = (float)(((double)targetVec[0] * lookVec[0] + (double)targetVec[1] * lookVec[1]) + (double)targetVec[2] * lookVec[2]);

	lua_pushnumber((float)(90.0f - (double)g_grim->_stdMath->arcSin3(dot)));
}

void Lua_V1::GetActorYawToPoint() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object pointObj = lua_getparam(2);
	lua_Object xObj, yObj, zObj;

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	if (lua_istable(pointObj)) {
		lua_pushobject(pointObj);
		lua_pushstring("x");
		xObj = lua_gettable();
		lua_pushobject(pointObj);
		lua_pushstring("y");
		yObj = lua_gettable();
		lua_pushobject(pointObj);
		lua_pushstring("z");
		zObj = lua_gettable();
	} else {
		xObj = pointObj;
		yObj = lua_getparam(3);
		zObj = lua_getparam(4);
	}

	RdVector3 target;

	target[0] = lua_getnumber(xObj);
	target[1] = lua_getnumber(yObj);
	target[2] = lua_getnumber(zObj);

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	if (!actor->extra->puck)
		g_grim->_sputActor->enablePuck(actor, 1);

	lua_pushnumber((float)actor->extra->puck->getYawToPoint(target));
}

/* Changes the set that an actor is associated with,
 * by changing the set to "nil" an actor is disabled
 * but should still not be destroyed.
 */
void Lua_V1::PutActorInSet() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object setObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!lua_isstring(setObj) && !lua_isnil(setObj))
		return;

	// The demo only stores the name of the set
	if (g_grim->getGameFlags() & ADGF_DEMO) {
		if (lua_isstring(setObj))
			Common::strlcpy(actor->curSetName, lua_getstring(setObj), sizeof(actor->curSetName));
		else
			actor->curSetName[0] = 0;

		return;
	}

	g_grim->_sputRoom->addActor(lua_getstring(setObj), actor);

	if (actor->extra && lua_isstring(setObj) && actor->extra->puck && actor->extra->inBoxes) {
		if (g_grim->_res->loaded(kSputTagSet, lua_getstring(setObj)))
			actor->extra->puck->validate();
	}
}

void Lua_V1::SetActorWalkRate() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object rateObj = lua_getparam(2);

	if (!isSputActor(actorObj) || !lua_isnumber(rateObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	actor->extra->walkRate = lua_getnumber(rateObj);
}

void Lua_V1::GetActorWalkRate() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	lua_pushnumber(actor->extra->walkRate);
}

void Lua_V1::SetActorTurnRate() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object rateObj = lua_getparam(2);

	if (!isSputActor(actorObj) || !lua_isnumber(rateObj))
		return;

	float rate = lua_getnumber(rateObj);
	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	// The demo stores the rate as it is
	if (!(actor->extra->turnRate >= 0.0f) && !(g_grim->getGameFlags() & ADGF_DEMO))
		rate = rate * -1.0f;

	actor->extra->turnRate = rate;
}

void Lua_V1::WalkActorForward() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "WalkActorForward"))
		return;

	actor->mode &= ~1;
	g_grim->_sputActor->forward(actor);
}

void Lua_V1::SetActorReflection() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object angleObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	g_grim->_sputActor->setReflection(actor, lua_getnumber(angleObj));
}

void Lua_V1::GetActorPuckVector() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object addObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	int addToPos = !lua_isnil(addObj);
	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	Puck *puck = actor->extra->puck;

	if (!puck) {
		lua_pushnil();
		return;
	}

	float x = puck->_orient[1][0];
	float y = puck->_orient[1][1];
	float z = puck->_orient[1][2];

	if (addToPos) {
		x = x + puck->_pos[0];
		y = puck->_pos[1] + y;
		z = puck->_pos[2] + z;
	}

	lua_pushnumber(x);
	lua_pushnumber(y);
	lua_pushnumber(z);
}

void Lua_V1::ActorPuckOrient() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object orientObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isnil(orientObj) || !luaFtol(lua_getnumber(orientObj)))
		actor->flags &= ~0x10;
	else
		actor->flags |= 0x10;
}

void Lua_V1::WalkActorTo() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	lua_Object goalObj = lua_getparam(5);
	RdVector3 pos;
	RdVector3 goal;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isnumber(xObj)) {
		pos[0] = lua_getnumber(xObj);
		pos[1] = lua_getnumber(yObj);
		pos[2] = lua_getnumber(zObj);
	} else if (isSputActor(xObj)) {
		SputActor *destActor = getSputActor(xObj);

		if (!destActor)
			return;

		pos[0] = destActor->actorPos[0];
		pos[1] = destActor->actorPos[1];
		pos[2] = destActor->actorPos[2];
	} else {
		return;
	}

	goal[0] = lua_getnumber(goalObj);
	goal[1] = lua_getnumber(goalObj);
	goal[2] = lua_getnumber(goalObj);

	if (!hasActorExtra(actor, "WalkActorTo"))
		return;

	g_grim->_sputActor->actorWalkTo(actor, pos, goal, nullptr);
}

void Lua_V1::WalkActorToAvoiding() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object avoidObj = lua_getparam(2);
	lua_Object xObj = lua_getparam(3);
	lua_Object yObj = lua_getparam(4);
	lua_Object zObj = lua_getparam(5);
	lua_Object goalObj = lua_getparam(6);
	RdVector3 pos;
	RdVector3 goal;

	if (!isSputActor(actorObj) || !isSputActor(avoidObj))
		return;

	SputActor *actor = getSputActor(actorObj);
	SputActor *avoidActor = getSputActor(avoidObj);

	if (!actor || !avoidActor)
		return;

	if (lua_isnumber(xObj)) {
		pos[0] = lua_getnumber(xObj);
		pos[1] = lua_getnumber(yObj);
		pos[2] = lua_getnumber(zObj);
	} else if (isSputActor(xObj)) {
		SputActor *destActor = getSputActor(xObj);

		if (!destActor)
			return;

		pos[0] = destActor->actorPos[0];
		pos[1] = destActor->actorPos[1];
		pos[2] = destActor->actorPos[2];
	} else {
		return;
	}

	goal[0] = lua_getnumber(goalObj);
	goal[1] = lua_getnumber(goalObj);
	goal[2] = lua_getnumber(goalObj);

	if (!hasActorExtra(actor, "WalkActorToAvoiding"))
		return;

	g_grim->_sputActor->actorWalkTo(actor, pos, goal, avoidActor);
}

/* This draw an actor to an offscreen buffer.
 * The actors' buffers blit to the global buffer which then gets blitted to screen.
 * This double layer system is necessary for set at, where Glottis and the Albinizod
 * are drawn to the clean buffer but at different times.
 *
 * If you change the clean system check with the sets dd, at and with the trail of flowers
 * in sh or zw.
 */
void Lua_V1::ActorToClean() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	g_grim->_sputActor->drawToClean(actor);
}

void Lua_V1::IsActorMoving() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	pushbool((actor->mode & 1) != 0);
}

void Lua_V1::IsActorResting() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	pushbool(actor->extra->resting != 0);
}

/* Get the location of one of the actor's nodes, this is
 * used by Glottis to watch where Manny is located in
 * order to hand him the work order.  This function is
 * also important for when Velasco hands Manny the logbook
 * in Rubacava. It is also used for calculating many positions
 * passed to ActorLookAt.
 */
void Lua_V1::GetActorNodeLocation() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object nodeObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	if (!lua_isnumber(nodeObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	CostumeNode *node = actor->costumeStack;

	if (!node || !node->headThing)
		return;

	bool demo = (g_grim->getGameFlags() & ADGF_DEMO) != 0;

	// The demo uses the joint matrices of the last frame
	if (!actor->onScreen && !demo)
		g_grim->_rd->_puppet.buildJointMatrices(node->headThing, g_grim->_sputActor->getOrientMatrix(actor));

	uint nodeNum = (uint)luaFtol(lua_getnumber(nodeObj));
	RdMatrix34 meshMatrix;

	if (!g_grim->_rd->_model3.getMeshMatrix(node->headThing, g_grim->_sputActor->getOrientMatrix(actor), nodeNum, meshMatrix)) {
		warning("GetActorNodeLocation(): invalid node %u", nodeNum);
		return;
	}

	// The demo returns the position of the mesh matrix as it is
	if (demo) {
		lua_pushnumber(meshMatrix[3][0]);
		lua_pushnumber(meshMatrix[3][1]);
		lua_pushnumber(meshMatrix[3][2]);
		return;
	}

	RdVector3 location;

	location[0] = node->headThing->data.model3->hNodes[nodeNum].pivot[0];
	location[1] = node->headThing->data.model3->hNodes[nodeNum].pivot[1];
	location[2] = node->headThing->data.model3->hNodes[nodeNum].pivot[2];
	g_grim->_rd->transformVector34Acc(location, meshMatrix);

	location[0] = (float)((double)location[0] + meshMatrix[3][0]);
	location[1] = (float)((double)location[1] + meshMatrix[3][1]);
	location[2] = (float)((double)location[2] + meshMatrix[3][2]);

	lua_pushnumber(location[0]);
	lua_pushnumber(location[1]);
	lua_pushnumber(location[2]);
}

void Lua_V1::SetActorWalkDominate() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object modeObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	int dominate = lua_isnil(modeObj);
	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra) {
		warning("SetActorWalkDominate(): the actor has no extra data");
		return;
	}

	g_grim->_sputActor->setWalkDominate(actor, dominate);
}

void Lua_V1::SetActorColormap() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object nameObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->colorMap) {
		warning("SetActorColormap(): the actor has no colormap reference");
		return;
	}

	if (lua_isstring(nameObj)) {
		void *colormap = g_grim->_sputResource->alloc(kSputTagCmap, lua_getstring(nameObj));

		if (!actor->colorMap->valid || actor->colorMap->serial != (int)(intptr)colormap) {
			g_grim->_res->removeReference(actor->colorMap);
			g_grim->_res->setReference(actor->colorMap, colormap);
		}
	} else if (lua_isnil(nameObj)) {
		g_grim->_res->removeReference(actor->colorMap);
	}
}

void Lua_V1::TurnActor() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object dirObj = lua_getparam(2);

	if (!isSputActor(actorObj) || !lua_isnumber(dirObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "TurnActor"))
		return;

	actor->mode &= ~1;
	g_grim->_sputActor->turn(actor, lua_getnumber(dirObj));
}

void Lua_V1::PushActorCostume() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object nameObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isstring(nameObj))
		g_grim->_sputActor->pushCostume(actor, lua_getstring(nameObj), 0);
}

void Lua_V1::SetActorCostume() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object costumeObj = lua_getparam(2);

	int result = 0;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isstring(costumeObj)) {
		result = g_grim->_sputActor->setCostume(actor, lua_getstring(costumeObj), 0);
	} else if (lua_isnil(costumeObj)) {
		while (actor->costumeStack)
			g_grim->_sputActor->popCostume(actor, 1);

		result = 1;
	}

	if (result)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::GetActorCostume() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object costumeObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	CostumeNode *node = actor->costumeStack;

	if (!lua_isnil(costumeObj)) {
		if (!lua_isnumber(costumeObj))
			return;

		int depth = luaFtol(lua_getnumber(costumeObj));

		while (node && depth > 0) {
			depth--;
			node = node->next;
		}
	}

	if (node) {
		CosCostume *costume = g_grim->_sputCostume->getCos()->instGetCostume(node->costumeInstance, nullptr);

		if (costume) {
			lua_pushstring(costume->name);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::PopActorCostume() {
	lua_Object actorObj = lua_getparam(1);
	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	CosCostume *costume = nullptr;

	if (actor->costumeStack)
		costume = g_grim->_sputCostume->getCos()->instGetCostume(actor->costumeStack->costumeInstance, nullptr);

	if (costume)
		lua_pushstring(costume->name);
	else
		lua_pushnil();

	g_grim->_sputActor->popCostume(actor, 1);
}

void Lua_V1::GetActorCostumeDepth() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	lua_pushnumber(g_grim->_sputActor->countCostumes(actor));
}

void Lua_V1::PrintActorCostumes() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	g_grim->_sputCostume->printCostumeInfo(actor->costumeStack);
}

void Lua_V1::LoadCostume() {
	lua_Object nameObj = lua_getparam(1);

	if (lua_isstring(nameObj))
		g_grim->_sputResource->alloc(kSputTagCost, lua_getstring(nameObj));
	else
		lua_pushnil();
}

void Lua_V1::PlayActorChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object costumeObj = lua_getparam(3);

	CostumeNode *node;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (node && lua_isnumber(choreObj) && g_grim->_sputCostume->getCos()->instPlayChore(node->costumeInstance, luaFtol(lua_getnumber(choreObj)), 0, 0))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::CompleteActorChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object costumeObj = lua_getparam(3);

	CostumeNode *node;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (node && lua_isnumber(choreObj) && g_grim->_sputCostume->getCos()->instCompleteChore(node->costumeInstance, luaFtol(lua_getnumber(choreObj))))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::PlayActorChoreLooping() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object costumeObj = lua_getparam(3);

	CostumeNode *node;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (node && lua_isnumber(choreObj) && g_grim->_sputCostume->getCos()->instPlayChore(node->costumeInstance, luaFtol(lua_getnumber(choreObj)), 1, 0))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::SetActorChoreLooping() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object loopObj = lua_getparam(3);
	lua_Object costumeObj = lua_getparam(4);
	CostumeNode *node;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node) || !node)
		return;

	Cos *cos = g_grim->_sputCostume->getCos();

	if (lua_isnumber(choreObj)) {
		cos->instSetChoreLoop(node->costumeInstance, luaFtol(lua_getnumber(choreObj)), !lua_isnil(loopObj));
	} else if (lua_isnil(choreObj)) {
		int numChores = cos->instCountChores(node->costumeInstance);

		for (int i = 0; i < numChores; i++)
			cos->instSetChoreLoop(node->costumeInstance, i, !lua_isnil(loopObj));
	}
}

void Lua_V1::StopActorChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object costumeObj = lua_getparam(3);

	CostumeNode *node;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node) || !node)
		return;

	if (lua_isnumber(choreObj))
		g_grim->_sputCostume->getCos()->instStopChore(node->costumeInstance, luaFtol(lua_getnumber(choreObj)));
	else if (lua_isnil(choreObj))
		g_grim->_sputCostume->getCos()->instStopChores(node->costumeInstance);
}

void Lua_V1::FadeOutChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object costumeObj = lua_getparam(2);
	lua_Object choreObj = lua_getparam(3);
	lua_Object timeObj = lua_getparam(4);

	CostumeNode *node;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	int choreIndex = luaFtol(lua_getnumber(choreObj));
	int timeMs = luaFtol(lua_getnumber(timeObj));

	if ((lua_isstring(costumeObj) || lua_isnumber(costumeObj)) && findCostumeInStack(costumeObj, actor->costumeStack, &node))
		g_grim->_sputCostume->fadeKeysOut(node->costumeInstance, choreIndex, timeMs);
}

void Lua_V1::FadeInChore() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object costumeObj = lua_getparam(2);
	lua_Object choreObj = lua_getparam(3);
	lua_Object timeObj = lua_getparam(4);

	CostumeNode *node;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	int choreIndex = luaFtol(lua_getnumber(choreObj));
	int timeMs = luaFtol(lua_getnumber(timeObj));

	if ((lua_isstring(costumeObj) || lua_isnumber(costumeObj)) && findCostumeInStack(costumeObj, actor->costumeStack, &node)) {
		g_grim->_sputCostume->getCos()->instPlayChore(node->costumeInstance, choreIndex, 0, 0);
		g_grim->_sputCostume->getCos()->instUpdateChore(node->costumeInstance, choreIndex, 100);
		g_grim->_sputCostume->fadeKeysIn(node->costumeInstance, choreIndex, timeMs);
	}
}

void Lua_V1::IsActorChoring() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object choreObj = lua_getparam(2);
	lua_Object ignoreLoopingObj = lua_getparam(3);
	lua_Object costumeObj = lua_getparam(4);
	CostumeNode *node;
	int foundChore = -1;

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	bool ignoreLooping = !lua_isnil(ignoreLoopingObj);

	if (!node) {
		lua_pushnil();
		return;
	}

	Cos *cos = g_grim->_sputCostume->getCos();

	if (lua_isnumber(choreObj)) {
		int status = cos->instChoreStatus(node->costumeInstance, luaFtol(lua_getnumber(choreObj)));

		if (!(status & kCosChoreStatePlaying)) {
			lua_pushnil();
		} else if (!ignoreLooping || !(status & kCosChoreStateLooping)) {
			lua_pushobject(choreObj);

			if (status & kCosChoreStateLooping)
				lua_pushnumber(1.0f);
			else
				lua_pushnil();
		}

		return;
	}

	if (lua_isnil(choreObj)) {
		int numChores = cos->instCountChores(node->costumeInstance);

		for (int i = 0; i < numChores; i++) {
			int status = cos->instChoreStatus(node->costumeInstance, i);

			if ((status & kCosChoreStatePlaying) && (!ignoreLooping || !(status & kCosChoreStateLooping))) {
				foundChore = i;

				if (status & kCosChoreStateLooping) {
					lua_pushnumber((float)i);
					lua_pushnumber(1.0f);
					return;
				}
			}
		}

		if (foundChore >= 0)
			lua_pushnumber((float)foundChore);
		else
			lua_pushnil();

		return;
	}

	lua_pushnil();
}

void Lua_V1::GetActorChores() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object costumeObj = lua_getparam(2);
	CostumeNode *node;

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (!node) {
		lua_pushnil();
		return;
	}

	Cos *cos = g_grim->_sputCostume->getCos();
	lua_Object choreTable = lua_createtable();
	int numChores = cos->instCountChores(node->costumeInstance);
	int playingCount = 0;

	for (int i = 0; i < numChores; i++) {
		if (cos->instChoreStatus(node->costumeInstance, i) & kCosChoreStatePlaying) {
			lua_pushobject(choreTable);
			lua_pushnumber((float)i);
			lua_pushnumber((float)i);
			lua_settable();
			playingCount++;
		}
	}

	lua_pushobject(choreTable);
	lua_pushstring("count");
	lua_pushnumber((float)playingCount);
	lua_settable();

	lua_pushobject(choreTable);
}

void Lua_V1::ActorLookAt() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	lua_Object rateObj = lua_getparam(5);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	if (!actor->costumeStack)
		return;

	Extra *extra = actor->extra;

	if (lua_isnumber(rateObj))
		actor->costumeStack->headTurnRate = lua_getnumber(rateObj);

	if (lua_isnil(xObj)) {
		if (extra->lookCarrot[0] != 0.0f || extra->lookCarrot[1] != 0.0f || extra->lookCarrot[2] != 0.0f) {
			extra->lookCarrot[0] = 0.0f;
			extra->lookCarrot[1] = 0.0f;
			extra->lookCarrot[2] = 0.0f;

			if (lua_isnumber(yObj))
				actor->costumeStack->headTurnRate = lua_getnumber(yObj);

			actor->mode |= 2;
		}
	} else if (lua_isnumber(xObj)) {
		extra->lookCarrot[0] = lua_getnumber(xObj);

		if (lua_isnumber(yObj))
			extra->lookCarrot[1] = lua_getnumber(yObj);
		else
			extra->lookCarrot[1] = 0.0f;

		if (lua_isnumber(zObj))
			extra->lookCarrot[2] = lua_getnumber(zObj);
		else
			extra->lookCarrot[2] = 0.0f;

		actor->mode |= 2;
	} else if (isSputActor(xObj)) {
		SputActor *lookedActor = getSputActor(xObj);

		if (!lookedActor)
			return;

		extra->lookCarrot[0] = lookedActor->actorPos[0];
		extra->lookCarrot[1] = lookedActor->actorPos[1];
		extra->lookCarrot[2] = lookedActor->actorPos[2];

		if (lua_isnumber(yObj))
			actor->costumeStack->headTurnRate = lua_getnumber(yObj);

		actor->mode |= 2;
	}
}

/* Turn the actor to a point specified in the 3D space,
 * this should not have the actor look toward the point
 * but should rotate the entire actor toward it.
 * It does not however make the actor slowly turn the next
 * times update() is called on it. It does make it turn
 * slightly, by the amount given by its turn rate, and its
 * effect is immediate.
 */
void Lua_V1::TurnActorTo() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	RdVector3 target;

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "TurnActorTo")) {
		lua_pushnil();
		return;
	}

	if (isSputActor(xObj)) {
		SputActor *targetActor = getSputActor(xObj);

		if (!targetActor) {
			lua_pushnil();
			return;
		}

		target[0] = targetActor->actorPos[0];
		target[1] = targetActor->actorPos[1];
		target[2] = targetActor->actorPos[2];
	} else {
		target[0] = lua_getnumber(xObj);
		target[1] = lua_getnumber(yObj);
		target[2] = lua_getnumber(zObj);
	}

	if (!actor->extra->puck)
		g_grim->_sputActor->enablePuck(actor, 1);

	float turnRate = actor->extra->turnRate;

	if (!(turnRate >= 0.0f))
		turnRate = -turnRate;

	float maxAngle = (float)((double)g_grim->_sputTime->getFrameTimeFlex() * turnRate);
	double turned = actor->extra->puck->pointAt(target, maxAngle);

	if (turned < 0.0)
		turned = -turned;

	if (turned >= maxAngle)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::PointActorAt() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	RdVector3 target;

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "PointActorAt"))
		return;

	if (isSputActor(xObj)) {
		SputActor *targetActor = getSputActor(xObj);

		if (!targetActor)
			return;

		target[0] = targetActor->actorPos[0];
		target[1] = targetActor->actorPos[1];
		target[2] = targetActor->actorPos[2];
	} else {
		target[0] = lua_getnumber(xObj);
		target[1] = lua_getnumber(yObj);
		target[2] = lua_getnumber(zObj);
	}

	if (!actor->extra->puck)
		g_grim->_sputActor->enablePuck(actor, 1);

	actor->extra->puck->pointAt(target, 3.4028235e38f);

	if (actor->extra->turnRate > 0.0f)
		actor->extra->turnRate = actor->extra->turnRate * -1.0f;
}

void Lua_V1::WalkActorVector() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object refObj = lua_getparam(2);
	lua_Object xObj = lua_getparam(3);
	lua_Object yObj = lua_getparam(4);
	lua_Object zObj = lua_getparam(5);
	lua_Object adjustObj = lua_getparam(6);

	if (!isSputActor(actorObj) || !isSputActor(refObj))
		return;

	SputActor *actor = getSputActor(actorObj);
	SputActor *refActor = getSputActor(refObj);

	if (!actor || !refActor || !hasActorExtra(actor, "WalkActorVector"))
		return;

	if (!actor->extra->puck) {
		warning("WalkActorVector(): the actor has no puck");
		return;
	}

	RdVector3 target;
	RdMatrix34 refMatrix;

	target[0] = lua_getnumber(xObj);
	target[1] = lua_getnumber(yObj);
	target[2] = lua_getnumber(zObj);
	memcpy(refMatrix, g_grim->_sputActor->getOrientMatrix(refActor), sizeof(RdMatrix34));

	if (lua_isnumber(adjustObj)) {
		RdVector3 adjust;
		RdVector3 savedPos;

		adjust[0] = 0.0f;
		adjust[1] = lua_getnumber(adjustObj);
		adjust[2] = 0.0f;
		savedPos[0] = refMatrix[3][0];
		savedPos[1] = refMatrix[3][1];
		savedPos[2] = refMatrix[3][2];
		refMatrix[3][0] = 0.0f;
		refMatrix[3][1] = 0.0f;
		refMatrix[3][2] = 0.0f;
		g_grim->_rd->preRotate34(refMatrix, adjust);
		refMatrix[3][0] = savedPos[0];
		refMatrix[3][1] = savedPos[1];
		refMatrix[3][2] = savedPos[2];
	}

	g_grim->_rd->transformVector34Acc(target, refMatrix);

	for (int i = 0; i < 3; i++)
		target[i] = (float)((double)actor->actorPos[i] + target[i]);

	RdVector3 localTarget;

	localTarget[0] = target[0];
	localTarget[1] = target[1];
	localTarget[2] = target[2];
	actor->extra->puck->invXform(localTarget);
	g_grim->_rd->normalize3Acc(localTarget);

	float turnAngle = (float)(90.0f - (double)g_grim->_stdMath->arcSin3(localTarget[1]));
	float turnRate = actor->extra->turnRate;

	if (!(turnRate >= 0.0f))
		turnRate = -turnRate;

	double turnStep = (double)g_grim->_sputTime->getFrameTimeFlex() * turnRate;

	actor->extra->puck->pointAt(target, (float)(turnStep + turnStep));

	if (!(turnAngle >= 90.0f)) {
		actor->mode &= ~1;
		g_grim->_sputActor->forward(actor);
	}
}

/* Set the pitch of the actor to the requested value,
 * this will rotate an actor toward/away from the ground.
 * This is used when Glottis runs over the signpost in
 * the Petrified Forest
 */
void Lua_V1::SetActorPitch() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object pitchObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	actor->actorRot[0] = lua_getnumber(pitchObj);
	actor->matrixValid = 0;
}

void Lua_V1::SetActorRoll() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object rollObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	actor->actorRot[2] = lua_getnumber(rollObj);
	actor->matrixValid = 0;
}

void Lua_V1::SetActorLookRate() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object rateObj = lua_getparam(2);

	if (!isSputActor(actorObj) || !lua_isnumber(rateObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (actor->costumeStack)
		actor->costumeStack->headTurnRate = lua_getnumber(rateObj);
}

void Lua_V1::GetActorLookRate() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (actor && actor->costumeStack)
		lua_pushnumber(actor->costumeStack->headTurnRate);
	else
		lua_pushnil();
}

void Lua_V1::SetActorHead() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object joint1Obj = lua_getparam(2);
	lua_Object joint2Obj = lua_getparam(3);
	lua_Object joint3Obj = lua_getparam(4);
	lua_Object maxRollObj = lua_getparam(5);
	lua_Object maxPitchObj = lua_getparam(6);
	lua_Object maxYawObj = lua_getparam(7);

	if (!isSputActor(actorObj))
		return;
	if (!lua_isnumber(joint1Obj) || !lua_isnumber(joint2Obj) || !lua_isnumber(joint3Obj) ||
			!lua_isnumber(maxRollObj) || !lua_isnumber(maxPitchObj) || !lua_isnumber(maxYawObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	float maxYaw = lua_getnumber(maxYawObj);
	float maxPitch = lua_getnumber(maxPitchObj);
	float maxRoll = lua_getnumber(maxRollObj);
	uint joint3 = (uint)luaFtol(lua_getnumber(joint3Obj));
	uint joint2 = (uint)luaFtol(lua_getnumber(joint2Obj));
	uint joint1 = (uint)luaFtol(lua_getnumber(joint1Obj));

	g_grim->_sputActor->setActorHead(actor, joint1, joint2, joint3, maxRoll, maxPitch, maxYaw);
}

void Lua_V1::PutActorAtInterest() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (room) {
		RdVector3 interest;
		interest[0] = room->setups[room->curSetup].camera.interest[0];
		interest[1] = room->setups[room->curSetup].camera.interest[1];
		interest[2] = room->setups[room->curSetup].camera.interest[2];
		g_grim->_sputActor->setActorPosition(actor, interest);
	}
}

void Lua_V1::SetActorFollowBoxes() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object followObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	int follow = 1;

	if (lua_isnil(followObj))
		follow = 0;

	g_grim->_sputActor->enableWalkSystem(actor, follow);
}

void Lua_V1::SetActorConstrain() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object constrainObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	int constrain = !lua_isnil(constrainObj);
	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra) {
		warning("SetActorConstrain(): the actor has no extra data");
		return;
	}

	g_grim->_sputActor->setActorConstrain(actor, constrain);
}

void Lua_V1::GetVisibleThings() {
	lua_Object actorObj = lua_getparam(1);
	SputActor *actor = nullptr;

	if (lua_isnil(actorObj)) {
		actor = g_grim->_sputActor->getSelectedActor();

		if (!actor)
			return;
	} else if (isSputActor(actorObj)) {
		actor = getSputActor(actorObj);
	}

	if (!actor) {
		warning("GetVisibleThings(): the parameter is not an actor");
		return;
	}

	lua_Object result = lua_createtable();

	// The demo checks all the actors, not the ones linked to the set
	if (g_grim->getGameFlags() & ADGF_DEMO) {
		int numActors = g_grim->_sputActor->getNumActors();

		for (int i = 0; i < numActors; i++) {
			SputActor *testActor = &g_grim->_sputActor->getActors()[i];
			float distance;

			if (testActor != actor && !(actor->flags & 1) && g_grim->_sputActor->canActorSeeActor(actor, testActor, &distance)) {
				lua_pushobject(result);
				pushSputActor(g_grim->_sputActor->getActorHandle(testActor));
				lua_pushnumber(1.0f);
				lua_settable();
			}
		}

		lua_pushobject(result);
		return;
	}

	if (!actor->curSetName[0])
		lua_pushobject(result);

	RoomActorLink *link = g_grim->_sputRoom->getRoomActors(actor->curSetName);

	while (link) {
		SputActor *testActor = &g_grim->_sputActor->getActors()[link->actorIndex];

		if (testActor != actor && !(actor->flags & 1)) {
			float distance;

			if (g_grim->_sputActor->canActorSeeActor(actor, testActor, &distance)) {
				lua_pushobject(result);
				pushSputActor(g_grim->_sputActor->getActorHandle(testActor));
				lua_pushnumber(1.0f);
				lua_settable();
			}
		}

		link = link->next;
	}

	lua_pushobject(result);
}

void Lua_V1::SetShadowColor() {
	int r = luaFtol(lua_getnumber(lua_getparam(1)));
	int g = luaFtol(lua_getnumber(lua_getparam(2)));
	int b = luaFtol(lua_getnumber(lua_getparam(3)));

	g_grim->_sputCostume->setShadowColor(r, g, b);
}

void Lua_V1::SetShadowColorDemo() {
	// The demo stores the color and never reads it
}

void Lua_V1::KillActorShadows() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	g_grim->_sputActor->resetShadows(actor);
}

void Lua_V1::SetActiveShadow() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object shadowIdObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	actor->extra->activeShadow = luaFtol(lua_getnumber(shadowIdObj));
}

void Lua_V1::SetActorShadowPoint() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "SetActorShadowPoint"))
		return;

	float z = lua_getnumber(zObj);
	float y = lua_getnumber(yObj);
	float x = lua_getnumber(xObj);

	g_grim->_sputActor->setShadowPoint(actor, x, y, z);
}

void Lua_V1::SetActorShadowPlane() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object nameObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "SetActorShadowPlane"))
		return;

	const char *name = lua_getstring(nameObj);

	if (!name) {
		warning("SetActorShadowPlane(): the plane name is not a string");
		return;
	}

	g_grim->_sputActor->setShadowPlane(actor, name);
}

void Lua_V1::AddShadowPlane() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object nameObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	const char *name = lua_getstring(nameObj);

	if (!name) {
		warning("AddShadowPlane(): the plane name is not a string");
		return;
	}

	g_grim->_sputActor->addShadowClipPlane(actor, actor->extra->activeShadow, name);
}

void Lua_V1::ActivateActorShadow() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object shadowIdObj = lua_getparam(2);
	lua_Object stateObj = lua_getparam(3);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor || !hasActorExtra(actor, "ActivateActorShadow"))
		return;

	int active = !lua_isnil(stateObj);

	g_grim->_sputActor->activateActorShadow(actor, luaFtol(lua_getnumber(shadowIdObj)), active);
}

void Lua_V1::SetActorShadowValid() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object numObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	g_grim->_sputActor->setShadowValid(actor, actor->extra->activeShadow, luaFtol(lua_getnumber(numObj)));
}

void Lua_V1::GetActorRect() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (actor && actor->onScreen) {
		lua_pushnumber((float)actor->drawnRect.x);
		lua_pushnumber((float)actor->drawnRect.y);
		lua_pushnumber((float)actor->drawnRect.width);
		lua_pushnumber((float)actor->drawnRect.height);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::SetActorInvClipNode() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object nodeObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	int nodeId;

	if (lua_isnil(nodeObj))
		nodeId = -1;
	else
		nodeId = luaFtol(lua_getnumber(nodeObj));

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	actor->extra->subClipNode = nodeId;
}

void Lua_V1::SetActorClipPlane() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object normalXObj = lua_getparam(2);
	lua_Object normalYObj = lua_getparam(3);
	lua_Object normalZObj = lua_getparam(4);
	lua_Object pointXObj = lua_getparam(5);
	lua_Object pointYObj = lua_getparam(6);
	lua_Object pointZObj = lua_getparam(7);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!lua_isnumber(normalXObj) || !lua_isnumber(normalYObj) || !lua_isnumber(normalZObj) ||
	    !lua_isnumber(pointXObj) || !lua_isnumber(pointYObj) || !lua_isnumber(pointZObj))
		return;

	RdVector3 normal;
	RdVector3 point;

	normal[0] = lua_getnumber(normalXObj);
	normal[1] = lua_getnumber(normalYObj);
	normal[2] = lua_getnumber(normalZObj);
	g_grim->_rd->normalize3Acc(normal);

	point[0] = lua_getnumber(pointXObj);
	point[1] = lua_getnumber(pointYObj);
	point[2] = lua_getnumber(pointZObj);

	g_grim->_sputActor->setClipPlane(actor, normal, point);
}

void Lua_V1::SetActorClipActive() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object stateObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	g_grim->_sputActor->setClipPlaneActive(actor, luaFtol(lua_getnumber(stateObj)));
}

void Lua_V1::SetActorFrustrumCull() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object stateObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	g_grim->_sputActor->setFrustrumCull(actor, luaFtol(lua_getnumber(stateObj)));
}

void Lua_V1::DriveActorTo() {
	// nothing to implement
	// dummy opcode in original
}

} // end of namespace Grim
