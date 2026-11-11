#ifndef GRIM_LTASK_H
#define GRIM_LTASK_H

#include "engines/grim/lua/lua.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lbuiltin.h"

namespace Grim {

struct lua_Task {
	lua_Task *next;
	struct Stack *S;
	Closure *cl;
	TProtoFunc *tf;
	StkId base;
	byte *pc;
	TObject *consts;
	int32 aux;
	bool executed;
	StkId initBase;
	int32 initResults;
	// The instructions this activation has run in the current update: the original counts them for each activation (GRIM LUA.DLL
	// ExecutionState +0x28, Monkey4.exe the same, reset when the activation starts and for every task at the start of each update,
	// see runtasks) and ends the script after 10 million (see luaV_execute)
	int32 instrCount;
};

void lua_taskinit(lua_Task *task, lua_Task *next, StkId tbase, int results);
void lua_taskresume(lua_Task *task, Closure *closure, TProtoFunc *protofunc, StkId tbase);
StkId luaV_execute(lua_Task *task);

void pause_script();
void unpause_script();

void runtasks(LState *const rootState);

} // end of namespace Grim

#endif
