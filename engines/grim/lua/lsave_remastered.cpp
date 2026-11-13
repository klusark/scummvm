#define FORBIDDEN_SYMBOL_EXCEPTION_setjmp
#define FORBIDDEN_SYMBOL_EXCEPTION_longjmp

#include "common/array.h"
#include "common/endian.h"
#include "common/hashmap.h"
#include "common/textconsole.h"

#include "engines/grim/lua/lopcodes.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lstring.h"
#include "engines/grim/lua/ltask.h"
#include "engines/grim/lua/ltm.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

// The Lua state in the format of the remastered: the layout of the original, with every serial
// and every value of an object in a slot of 8 bytes

// Userdata is inline in the ScummVM objects: the first pass collects it
static bool collectUserdata;
static Common::HashMap<uint64, uint32> userdataIndices;
static Common::Array<UserData> userdataEntries;

static int nullWriter(void *buffer, int size) {
	return 0;
}

static uint32 getSerial(const void *ptr) {
	return (uint32)(uintptr)ptr;
}

static void saveInt32(SaveRestoreAccess *saveFunc, int32 value) {
	uint32 data = TO_LE_32((uint32)value);

	saveFunc(&data, 4);
}

// A serial or a pointer value: the high half is zero
static void saveSerial(SaveRestoreAccess *saveFunc, uint32 value) {
	saveInt32(saveFunc, (int32)value);
	saveInt32(saveFunc, 0);
}

// A value with a sign, like the one that the hook gives to a userdata
static void saveSignedValue(SaveRestoreAccess *saveFunc, int32 value) {
	saveInt32(saveFunc, value);
	saveInt32(saveFunc, value < 0 ? -1 : 0);
}

static uint32 getUserdataSerial(const UserData *userdata) {
	uint64 key = ((uint64)(uint32)userdata->tag << 32) | (uint32)userdata->id;
	uint32 index;

	if (!userdataIndices.tryGetVal(key, index)) {
		index = userdataEntries.size();
		userdataIndices.setVal(key, index);
		userdataEntries.push_back(*userdata);
	}

	if (collectUserdata)
		return 0;

	return getSerial(&userdataEntries[index]);
}

static void SaveObject(const TObject *object, SaveRestoreAccess *saveFunc) {
	int32 ttype = object->ttype;
	uint32 serialNum = 0;

	switch (ttype) {
	case LUA_T_CPROTO:
		if (lua_FuncPtrHasID(object->value.f)) {
			serialNum = lua_FuncPtrToID(object->value.f);
		} else {
			ttype = LUA_T_NIL;
		}
		break;

	case LUA_T_USERDATA:
		serialNum = getUserdataSerial(&object->value.ud);
		break;

	default:
		memcpy(&serialNum, &object->value, 4);
	}

	saveInt32(saveFunc, ttype);
	saveSerial(saveFunc, serialNum);
}

static void luaS_writestrings(SaveRestoreAccess *saveFunc) {
	for (int32 i = 0; i < NUM_HASHS; i++) {
		stringtable *tb = &string_root[i];

		for (int32 j = 0; j < tb->size; j++) {
			if (tb->hash[j] != nullptr && tb->hash[j] != &EMPTY) {
				TaggedString *tString = tb->hash[j];

				saveSerial(saveFunc, getSerial(tString));
				saveInt32(saveFunc, tString->constindex);
				SaveObject(&tString->globalval, saveFunc);

				int32 length = strlen(tString->str);

				saveInt32(saveFunc, length);
				saveFunc(tString->str, length);
			}
		}
	}
}

static void luaS_writeuserdata(SaveRestoreAccess *saveFunc) {
	for (uint i = 0; i < userdataEntries.size(); i++) {
		UserData *userdata = &userdataEntries[i];
		int32 value = userdata->id;

		saveSerial(saveFunc, getSerial(userdata));
		saveInt32(saveFunc, -1);

		if (lua_savehook)
			value = lua_savehook(userdata->tag, value, saveFunc);

		saveInt32(saveFunc, userdata->tag);
		saveSignedValue(saveFunc, value);
	}
}

static uint32 luaS_count(uint32 *maxstrlen) {
	uint32 count = 0;

	*maxstrlen = 0;

	for (int32 i = 0; i < NUM_HASHS; i++) {
		stringtable *tb = &string_root[i];

		for (int32 j = 0; j < tb->size; j++) {
			if (tb->hash[j] != nullptr && tb->hash[j] != &EMPTY) {
				count++;

				uint32 length = strlen(tb->hash[j]->str);

				if (length > *maxstrlen)
					*maxstrlen = length;
			}
		}
	}

	return count;
}

static void luaH_save(SaveRestoreAccess *saveFunc, Hash *hash) {
	int32 numItems = 0;

	saveInt32(saveFunc, hash->nhash);

	for (int32 i = 0; i < hash->nhash; i++) {
		Node *n = &hash->node[i];

		if (n->ref.ttype != LUA_T_NIL && n->val.ttype != LUA_T_NIL)
			numItems++;
	}

	saveInt32(saveFunc, numItems);
	saveInt32(saveFunc, hash->htag);

	for (int32 i = 0; i < hash->nhash; i++) {
		Node *n = &hash->node[i];

		if (n->ref.ttype != LUA_T_NIL && n->val.ttype != LUA_T_NIL) {
			SaveObject(&n->ref, saveFunc);
			SaveObject(&n->val, saveFunc);
		}
	}
}

static void SaveGlobals(SaveRestoreAccess *saveFunc) {
	uint32 maxstrlen, globalCount = 0, protoCount = 0, clCount = 0, tableCount = 0, stringCount = 0;

	stringCount = luaS_count(&maxstrlen) + userdataEntries.size();

	for (GCnode *node = rootcl.next; node; node = node->next)
		clCount++;

	for (GCnode *node = roottable.next; node; node = node->next)
		tableCount++;

	for (GCnode *node = rootproto.next; node; node = node->next)
		protoCount++;

	for (GCnode *node = rootglobal.next; node; node = node->next)
		globalCount++;

	saveInt32(saveFunc, stringCount);
	saveInt32(saveFunc, clCount);
	saveInt32(saveFunc, tableCount);
	saveInt32(saveFunc, protoCount);
	saveInt32(saveFunc, globalCount);
	saveInt32(saveFunc, maxstrlen);

	luaS_writestrings(saveFunc);
	luaS_writeuserdata(saveFunc);

	for (Closure *closure = (Closure *)rootcl.next; closure; closure = (Closure *)closure->head.next) {
		saveSerial(saveFunc, getSerial(closure));
		saveInt32(saveFunc, closure->nelems);

		for (int32 i = 0; i <= closure->nelems; i++)
			SaveObject(&closure->consts[i], saveFunc);
	}

	for (Hash *hash = (Hash *)roottable.next; hash; hash = (Hash *)hash->head.next) {
		saveSerial(saveFunc, getSerial(hash));
		luaH_save(saveFunc, hash);
	}

	for (TProtoFunc *proto = (TProtoFunc *)rootproto.next; proto; proto = (TProtoFunc *)proto->head.next) {
		int32 numVars = 0;

		saveSerial(saveFunc, getSerial(proto));
		saveSerial(saveFunc, getSerial(proto->fileName));
		saveInt32(saveFunc, proto->lineDefined);
		saveInt32(saveFunc, proto->nconsts);

		for (int32 i = 0; i < proto->nconsts; i++)
			SaveObject(&proto->consts[i], saveFunc);

		if (proto->locvars) {
			while (proto->locvars[numVars++].line != -1) {
			}
		}

		saveInt32(saveFunc, numVars);

		for (int32 i = 0; i < numVars; i++) {
			saveSerial(saveFunc, getSerial(proto->locvars[i].varname));
			saveInt32(saveFunc, proto->locvars[i].line);
		}

		int32 numBytes = luaExeCodeSize(proto->code, 0x7fffffff);

		saveInt32(saveFunc, numBytes);
		saveFunc(proto->code, numBytes);
	}

	for (GCnode *tString = rootglobal.next; tString; tString = tString->next)
		saveSerial(saveFunc, getSerial(tString));

	saveInt32(saveFunc, 1);

	SaveObject(&errorim, saveFunc);

	saveInt32(saveFunc, IMtable_size);

	for (int32 i = 0; i < IMtable_size; i++) {
		for (int32 j = 0; j < IM_N; j++)
			SaveObject(&IMtable[i].int_method[j], saveFunc);
	}

	saveInt32(saveFunc, last_tag);
	saveInt32(saveFunc, refSize);

	for (int32 i = 0; i < refSize; i++) {
		SaveObject(&refArray[i].o, saveFunc);
		saveInt32(saveFunc, (int32)refArray[i].status);
	}

	saveInt32(saveFunc, GCthreshold);
	saveInt32(saveFunc, nblocks);

	saveInt32(saveFunc, Mbuffsize);
	saveFunc(Mbuffer, Mbuffsize);

	int32 Mbasepos = Mbuffbase - Mbuffer;

	saveInt32(saveFunc, Mbasepos);
	saveInt32(saveFunc, Mbuffnext);

	saveInt32(saveFunc, globalTaskSerialId);
}

static void SaveExecutionState(lua_Task *execState, SaveRestoreAccess *saveFunc) {
	if (execState->next)
		SaveExecutionState(execState->next, saveFunc);

	saveSerial(saveFunc, getSerial(execState->cl));
	saveSerial(saveFunc, getSerial(execState->tf));
	saveInt32(saveFunc, execState->base);
	saveInt32(saveFunc, execState->initBase);
	saveInt32(saveFunc, execState->initResults);
	saveInt32(saveFunc, execState->executed ? 1 : 0);
	saveInt32(saveFunc, execState->pc - execState->tf->code);
	saveInt32(saveFunc, execState->aux);
}

static void SaveState(LState *state, SaveRestoreAccess *saveFunc) {
	uint32 numExecStates = 0;
	int32 previousExecState = -1;

	for (lua_Task *execState = state->task; execState; execState = execState->next) {
		if (state->prevTask && state->prevTask == execState)
			previousExecState = numExecStates;

		numExecStates++;
	}

	saveInt32(saveFunc, numExecStates);

	if (state->task)
		SaveExecutionState(state->task, saveFunc);

	saveInt32(saveFunc, previousExecState);

	saveInt32(saveFunc, state->preventBreakCounter);
	saveInt32(saveFunc, state == lua_rootState ? 1 : 0);
	saveInt32(saveFunc, state->updated ? 1 : 0);
	saveInt32(saveFunc, (state->all_paused || state->paused) ? 1 : 0);
	saveInt32(saveFunc, state->callLevelCounter);

	int32 stackSize = (state->stack.last - state->stack.stack) + 1;

	saveInt32(saveFunc, stackSize);

	stackSize = state->stack.top - state->stack.stack;
	saveInt32(saveFunc, stackSize);

	for (int32 i = 0; i < stackSize; i++)
		SaveObject(&state->stack.stack[i], saveFunc);

	saveInt32(saveFunc, state->Cstack.base);
	saveInt32(saveFunc, state->Cstack.lua2C);
	saveInt32(saveFunc, state->Cstack.num);

	saveInt32(saveFunc, state->numCblocks);

	for (int32 j = 0; j < state->numCblocks; j++) {
		saveInt32(saveFunc, state->Cblocks[j].base);
		saveInt32(saveFunc, state->Cblocks[j].lua2C);
		saveInt32(saveFunc, state->Cblocks[j].num);
	}

	saveInt32(saveFunc, state->id);
	SaveObject(&state->taskFunc, saveFunc);
}

static void saveLuaState(SaveRestoreAccess *saveFunc) {
	uint32 numTasks = 0, currentTaskNumber = 0;

	SaveGlobals(saveFunc);

	for (LState *state = lua_rootState; state; state = state->next) {
		if (state == lua_state)
			currentTaskNumber = numTasks;

		numTasks++;
	}

	saveInt32(saveFunc, numTasks);
	saveInt32(saveFunc, currentTaskNumber);

	for (LState *state = lua_rootState; state; state = state->next)
		SaveState(state, saveFunc);
}

int lua_SaveRemastered(SaveRestoreAccess *saveFunc) {
	lua_collectgarbage(0);

	collectUserdata = true;
	userdataIndices.clear();
	userdataEntries.clear();
	saveLuaState(nullWriter);

	collectUserdata = false;
	saveLuaState(saveFunc);

	userdataIndices.clear();
	userdataEntries.clear();

	return 0;
}

} // end of namespace Grim
