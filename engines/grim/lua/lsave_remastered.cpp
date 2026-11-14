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

static void saveSerial(SaveRestoreAccess *saveFunc, uint32 value) {
	saveInt32(saveFunc, (int32)value);
	saveInt32(saveFunc, 0);
}

class RemasteredSaver {
public:
	RemasteredSaver() : _collectUserdata(false) {}

	void save(SaveRestoreAccess *saveFunc);

private:
	bool _collectUserdata;
	Common::HashMap<uint64, uint32> _userdataIndices;
	Common::Array<UserData> _userdataEntries;

	uint32 getUserdataSerial(const UserData *userdata);
	void saveObject(const TObject *object, SaveRestoreAccess *saveFunc);
	void writeStrings(SaveRestoreAccess *saveFunc);
	void writeUserdata(SaveRestoreAccess *saveFunc);
	void saveHash(SaveRestoreAccess *saveFunc, Hash *hash);
	void saveGlobals(SaveRestoreAccess *saveFunc);
	void saveState(LState *state, SaveRestoreAccess *saveFunc);
	void saveLuaState(SaveRestoreAccess *saveFunc);
};

uint32 RemasteredSaver::getUserdataSerial(const UserData *userdata) {
	uint64 key = ((uint64)(uint32)userdata->tag << 32) | (uint32)userdata->id;
	uint32 index;

	if (!_userdataIndices.tryGetVal(key, index)) {
		index = _userdataEntries.size();
		_userdataIndices.setVal(key, index);
		_userdataEntries.push_back(*userdata);
	}

	if (_collectUserdata)
		return 0;

	return getSerial(&_userdataEntries[index]);
}

void RemasteredSaver::saveObject(const TObject *object, SaveRestoreAccess *saveFunc) {
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

void RemasteredSaver::writeStrings(SaveRestoreAccess *saveFunc) {
	for (int32 i = 0; i < NUM_HASHS; i++) {
		stringtable *tb = &string_root[i];

		for (int32 j = 0; j < tb->size; j++) {
			if (tb->hash[j] != nullptr && tb->hash[j] != &EMPTY) {
				TaggedString *tString = tb->hash[j];

				saveSerial(saveFunc, getSerial(tString));
				saveInt32(saveFunc, tString->constindex);
				saveObject(&tString->globalval, saveFunc);

				int32 length = strlen(tString->str);

				saveInt32(saveFunc, length);
				saveFunc(tString->str, length);
			}
		}
	}
}

void RemasteredSaver::writeUserdata(SaveRestoreAccess *saveFunc) {
	for (uint i = 0; i < _userdataEntries.size(); i++) {
		UserData *userdata = &_userdataEntries[i];
		int32 value = userdata->id;

		saveSerial(saveFunc, getSerial(userdata));
		saveInt32(saveFunc, -1);

		if (lua_savehook)
			value = lua_savehook(userdata->tag, value, saveFunc);

		saveInt32(saveFunc, userdata->tag);
		// A value of -1 is saved as 0xFFFFFFFF with a zero high half
		saveSerial(saveFunc, (uint32)value);
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

void RemasteredSaver::saveHash(SaveRestoreAccess *saveFunc, Hash *hash) {
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
			saveObject(&n->ref, saveFunc);
			saveObject(&n->val, saveFunc);
		}
	}
}

void RemasteredSaver::saveGlobals(SaveRestoreAccess *saveFunc) {
	uint32 maxstrlen, globalCount = 0, protoCount = 0, clCount = 0, tableCount = 0, stringCount = 0;

	stringCount = luaS_count(&maxstrlen) + _userdataEntries.size();

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

	writeStrings(saveFunc);
	writeUserdata(saveFunc);

	for (Closure *closure = (Closure *)rootcl.next; closure; closure = (Closure *)closure->head.next) {
		saveSerial(saveFunc, getSerial(closure));
		saveInt32(saveFunc, closure->nelems);

		for (int32 i = 0; i <= closure->nelems; i++)
			saveObject(&closure->consts[i], saveFunc);
	}

	for (Hash *hash = (Hash *)roottable.next; hash; hash = (Hash *)hash->head.next) {
		saveSerial(saveFunc, getSerial(hash));
		saveHash(saveFunc, hash);
	}

	for (TProtoFunc *proto = (TProtoFunc *)rootproto.next; proto; proto = (TProtoFunc *)proto->head.next) {
		int32 numVars = 0;

		saveSerial(saveFunc, getSerial(proto));
		saveSerial(saveFunc, getSerial(proto->fileName));
		saveInt32(saveFunc, proto->lineDefined);
		saveInt32(saveFunc, proto->nconsts);

		for (int32 i = 0; i < proto->nconsts; i++)
			saveObject(&proto->consts[i], saveFunc);

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

	saveObject(&errorim, saveFunc);

	saveInt32(saveFunc, IMtable_size);

	for (int32 i = 0; i < IMtable_size; i++) {
		for (int32 j = 0; j < IM_N; j++)
			saveObject(&IMtable[i].int_method[j], saveFunc);
	}

	saveInt32(saveFunc, last_tag);
	saveInt32(saveFunc, refSize);

	for (int32 i = 0; i < refSize; i++) {
		saveObject(&refArray[i].o, saveFunc);
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

void RemasteredSaver::saveState(LState *state, SaveRestoreAccess *saveFunc) {
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
		saveObject(&state->stack.stack[i], saveFunc);

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
	saveObject(&state->taskFunc, saveFunc);
}

void RemasteredSaver::saveLuaState(SaveRestoreAccess *saveFunc) {
	uint32 numTasks = 0, currentTaskNumber = 0;

	saveGlobals(saveFunc);

	for (LState *state = lua_rootState; state; state = state->next) {
		if (state == lua_state)
			currentTaskNumber = numTasks;

		numTasks++;
	}

	saveInt32(saveFunc, numTasks);
	saveInt32(saveFunc, currentTaskNumber);

	for (LState *state = lua_rootState; state; state = state->next)
		saveState(state, saveFunc);
}

void RemasteredSaver::save(SaveRestoreAccess *saveFunc) {
	_collectUserdata = true;
	_userdataIndices.clear();
	_userdataEntries.clear();
	saveLuaState(nullWriter);

	_collectUserdata = false;
	saveLuaState(saveFunc);
}

int lua_SaveRemastered(SaveRestoreAccess *saveFunc) {
	lua_collectgarbage(0);

	RemasteredSaver saver;

	saver.save(saveFunc);

	return 0;
}

} // end of namespace Grim
