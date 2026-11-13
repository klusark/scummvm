#define FORBIDDEN_SYMBOL_EXCEPTION_setjmp
#define FORBIDDEN_SYMBOL_EXCEPTION_longjmp

#include "common/endian.h"
#include "common/textconsole.h"

#include "engines/grim/std/std_sort.h"

#include "engines/grim/lua/lmem.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lstring.h"
#include "engines/grim/lua/ltable.h"
#include "engines/grim/lua/ltask.h"
#include "engines/grim/lua/ltm.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

// The Lua state of the remastered: the layout of the original, with every serial and every value
// of an object in a slot of 8 bytes of which only the low half is used

struct RemasteredPtrID {
	uint32 serialNumber;
	void *item;
};

static bool ptrsSorted = false;

static RemasteredPtrID *stringPtrs;
static uint32 stringPtrTotal;

static RemasteredPtrID *closurePtrs;
static uint32 closurePtrTotal;

static RemasteredPtrID *tablePtrs;
static uint32 tablePtrTotal;

static RemasteredPtrID *protoPtrs;
static uint32 protoPtrTotal;

// Userdata is inline in the ScummVM objects: its string table entries point here
static UserData *userdata;

static int32 loadInt32(SaveRestoreAccess *restoreFunc) {
	uint32 data = 0;

	restoreFunc(&data, 4);

	return (int32)FROM_LE_32(data);
}

static uint32 loadSlot(SaveRestoreAccess *restoreFunc) {
	uint32 value = (uint32)loadInt32(restoreFunc);

	loadInt32(restoreFunc);

	return value;
}

static int comparePtrIDs(const void *elem1, const void *elem2) {
	const RemasteredPtrID *elem1Pointer = (const RemasteredPtrID *)elem1;
	const RemasteredPtrID *elem2Pointer = (const RemasteredPtrID *)elem2;

	if (elem1Pointer->serialNumber < elem2Pointer->serialNumber)
		return -1;
	else if (elem1Pointer->serialNumber > elem2Pointer->serialNumber)
		return 1;

	return 0;
}

static void SerialToPtr(TObject *object) {
	RemasteredPtrID *ptrID, ptrid;
	uint32 serialNum = (uint32)object->value.i;

	if (!ptrsSorted) {
		sort(stringPtrs, stringPtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);
		sort(closurePtrs, closurePtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);
		sort(tablePtrs, tablePtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);
		sort(protoPtrs, protoPtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);

		ptrsSorted = true;
	}

	memset(&object->value, 0, sizeof(Value));

	if (serialNum == 0)
		return;

	ptrid.serialNumber = serialNum;
	ptrid.item = nullptr;

	switch (object->ttype) {
	case LUA_T_USERDATA:
	case LUA_T_STRING:
		ptrID = (RemasteredPtrID *)binarySearch(&ptrid, stringPtrs, stringPtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);
		break;

	case LUA_T_CLOSURE:
		ptrID = (RemasteredPtrID *)binarySearch(&ptrid, closurePtrs, closurePtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);
		break;

	case LUA_T_ARRAY:
		ptrID = (RemasteredPtrID *)binarySearch(&ptrid, tablePtrs, tablePtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);
		break;

	case LUA_T_PROTO:
	case LUA_T_PMARK:
		ptrID = (RemasteredPtrID *)binarySearch(&ptrid, protoPtrs, protoPtrTotal, sizeof(RemasteredPtrID), comparePtrIDs);
		break;

	default:
		ptrID = nullptr;
	}

	if (!ptrID)
		return;

	switch (object->ttype) {
	case LUA_T_USERDATA:
		object->value.ud = *(UserData *)ptrID->item;
		break;

	case LUA_T_STRING:
		object->value.ts = (TaggedString *)ptrID->item;
		break;

	case LUA_T_CLOSURE:
		object->value.cl = (Closure *)ptrID->item;
		break;

	case LUA_T_ARRAY:
		object->value.a = (Hash *)ptrID->item;
		break;

	default:
		object->value.tf = (TProtoFunc *)ptrID->item;
	}
}

static void LoadObject(TObject *object, SaveRestoreAccess *restoreFunc) {
	object->ttype = (lua_Type)loadInt32(restoreFunc);

	uint32 raw = loadSlot(restoreFunc);

	memset(&object->value, 0, sizeof(Value));
	memcpy(&object->value, &raw, 4);
}

static void ResolveObject(TObject *object) {
	switch (object->ttype) {
	case LUA_T_CPROTO:
		{
			uint32 id = (uint32)object->value.i;

			memset(&object->value, 0, sizeof(Value));
			object->value.f = lua_IDToFuncPtr(id);
		}
		break;

	case LUA_T_NUMBER:
	case LUA_T_LINE:
	case LUA_T_TASK:
	case LUA_T_NIL:
		break;

	default:
		SerialToPtr(object);
	}
}

static void LoadResolvedObject(TObject *object, SaveRestoreAccess *restoreFunc) {
	LoadObject(object, restoreFunc);
	ResolveObject(object);
}

static void LoadSerial(TObject *object, lua_Type ttype, SaveRestoreAccess *restoreFunc) {
	uint32 raw = loadSlot(restoreFunc);

	object->ttype = ttype;
	memset(&object->value, 0, sizeof(Value));
	memcpy(&object->value, &raw, 4);
}

static void luaH_forcerehash(Hash *t) {
	Node *vold = t->node;

	t->node = hashnodecreate(t->nhash);

	for (int32 i = 0; i < t->nuse; i++) {
		Node *n = vold + i;

		if (n->ref.ttype != LUA_T_NIL && n->val.ttype != LUA_T_NIL)
			*node(t, present(t, &n->ref)) = *n;
	}

	luaM_free(vold);
}

static Hash *luaH_load(SaveRestoreAccess *restoreFunc) {
	Hash *hash = luaM_new(Hash);

	hash->nhash = loadInt32(restoreFunc);
	hash->nuse = loadInt32(restoreFunc);
	hash->htag = loadInt32(restoreFunc);
	hash->node = hashnodecreate(hash->nhash);
	luaO_insertlist(&roottable, (GCnode *)hash);

	for (int32 i = 0; i < hash->nuse; i++) {
		LoadObject(&hash->node[i].ref, restoreFunc);
		LoadObject(&hash->node[i].val, restoreFunc);
	}

	return hash;
}

static void luaS_resolvestrings() {
	for (int32 i = 0; i < NUM_HASHS; i++) {
		stringtable *tb = &string_root[i];

		for (int32 j = 0; j < tb->size; j++) {
			TaggedString *tString = tb->hash[j];

			if (tString != nullptr && tString != &EMPTY && tString->constindex != -1)
				ResolveObject(&tString->globalval);
		}
	}
}

static void GlobalEndRestore() {
	if (stringPtrs) {
		luaM_free(stringPtrs);
		stringPtrTotal = 0;
		stringPtrs = nullptr;
	}

	if (userdata) {
		luaM_free(userdata);
		userdata = nullptr;
	}

	if (closurePtrs) {
		luaM_free(closurePtrs);
		closurePtrTotal = 0;
		closurePtrs = nullptr;
	}

	if (tablePtrs) {
		luaM_free(tablePtrs);
		tablePtrTotal = 0;
		tablePtrs = nullptr;
	}

	if (protoPtrs) {
		luaM_free(protoPtrs);
		protoPtrTotal = 0;
		protoPtrs = nullptr;
	}

	ptrsSorted = false;
}

static void GlobalBeginRestore(SaveRestoreAccess *restoreFunc) {
	uint32 maxstrlen, globalCount;
	uint32 numUserdata = 0;
	RemasteredPtrID *ptr;
	char *buffer;

	stringPtrTotal = loadInt32(restoreFunc);
	closurePtrTotal = loadInt32(restoreFunc);
	tablePtrTotal = loadInt32(restoreFunc);
	protoPtrTotal = loadInt32(restoreFunc);
	globalCount = loadInt32(restoreFunc);

	ptr = stringPtrs = luaM_newvector(stringPtrTotal, RemasteredPtrID);
	userdata = luaM_newvector(stringPtrTotal, UserData);

	maxstrlen = loadInt32(restoreFunc);
	buffer = (char *)luaM_malloc(maxstrlen + 5);

	for (uint32 i = 0; i < stringPtrTotal; i++) {
		ptr->serialNumber = loadSlot(restoreFunc);

		int32 constindex = loadInt32(restoreFunc);
		int32 tag = loadInt32(restoreFunc);
		int32 v = (int32)loadSlot(restoreFunc);

		if (constindex != -1) {
			uint32 length = loadInt32(restoreFunc);

			restoreFunc(buffer, length);
			buffer[length] = '\0';

			TaggedString *tString = luaS_new(buffer);

			memset(&tString->globalval.value, 0, sizeof(Value));
			tString->globalval.value.i = v;
			tString->globalval.ttype = (lua_Type)tag;
			tString->constindex = constindex;

			ptr->item = tString;
		} else {
			UserData *userdataEntry = &userdata[numUserdata++];

			userdataEntry->tag = tag;
			userdataEntry->id = v;

			if (lua_restorehook)
				userdataEntry->id = lua_restorehook(tag, v, restoreFunc);

			ptr->item = userdataEntry;
		}

		ptr++;
	}

	luaM_free(buffer);

	ptr = closurePtrs = luaM_newvector(closurePtrTotal, RemasteredPtrID);

	for (uint32 i = 0; i < closurePtrTotal; i++) {
		ptr->serialNumber = loadSlot(restoreFunc);

		uint32 nelems = loadInt32(restoreFunc);
		Closure *closure = (Closure *)luaM_malloc(sizeof(Closure) + nelems * sizeof(TObject));

		luaO_insertlist(&rootcl, (GCnode *)closure);
		closure->nelems = nelems;

		for (int32 j = 0; j <= closure->nelems; j++)
			LoadObject(&closure->consts[j], restoreFunc);

		ptr->item = closure;
		ptr++;
	}

	ptr = tablePtrs = luaM_newvector(tablePtrTotal, RemasteredPtrID);

	for (uint32 i = 0; i < tablePtrTotal; i++) {
		ptr->serialNumber = loadSlot(restoreFunc);
		ptr->item = luaH_load(restoreFunc);
		ptr++;
	}

	ptr = protoPtrs = luaM_newvector(protoPtrTotal, RemasteredPtrID);

	for (uint32 i = 0; i < protoPtrTotal; i++) {
		ptr->serialNumber = loadSlot(restoreFunc);

		TProtoFunc *protoFunc = luaM_new(TProtoFunc);

		luaO_insertlist(&rootproto, (GCnode *)protoFunc);
		protoFunc->fileName = (TaggedString *)(uintptr)loadSlot(restoreFunc);
		protoFunc->lineDefined = loadInt32(restoreFunc);
		protoFunc->nconsts = loadInt32(restoreFunc);
		protoFunc->consts = luaM_newvector(protoFunc->nconsts, TObject);

		for (int32 j = 0; j < protoFunc->nconsts; j++)
			LoadObject(&protoFunc->consts[j], restoreFunc);

		uint32 numVars = loadInt32(restoreFunc);

		if (numVars)
			protoFunc->locvars = luaM_newvector(numVars, LocVar);
		else
			protoFunc->locvars = nullptr;

		for (uint32 j = 0; j < numVars; j++) {
			protoFunc->locvars[j].varname = (TaggedString *)(uintptr)loadSlot(restoreFunc);
			protoFunc->locvars[j].line = loadInt32(restoreFunc);
		}

		uint32 numBytes = loadInt32(restoreFunc);

		protoFunc->code = (byte *)luaM_malloc(numBytes);
		restoreFunc(protoFunc->code, numBytes);
		ptr->item = protoFunc;
		ptr++;
	}

	luaS_resolvestrings();

	for (TProtoFunc *protoFunc = (TProtoFunc *)rootproto.next; protoFunc; protoFunc = (TProtoFunc *)protoFunc->head.next) {
		TObject tempObject;

		tempObject.ttype = LUA_T_STRING;
		memset(&tempObject.value, 0, sizeof(Value));
		tempObject.value.i = (int32)(uintptr)protoFunc->fileName;

		ResolveObject(&tempObject);

		protoFunc->fileName = tempObject.value.ts;

		for (int32 i = 0; i < protoFunc->nconsts; i++)
			ResolveObject(&protoFunc->consts[i]);

		if (protoFunc->locvars) {
			int32 i = 0;

			while (protoFunc->locvars[i].line != -1) {
				tempObject.ttype = LUA_T_STRING;
				memset(&tempObject.value, 0, sizeof(Value));
				tempObject.value.i = (int32)(uintptr)protoFunc->locvars[i].varname;

				ResolveObject(&tempObject);

				protoFunc->locvars[i].varname = tempObject.value.ts;
				i++;
			}
		}
	}

	for (Hash *hash = (Hash *)roottable.next; hash; hash = (Hash *)hash->head.next) {
		for (int32 i = 0; i < hash->nuse; i++) {
			ResolveObject(&hash->node[i].ref);
			ResolveObject(&hash->node[i].val);
		}

		luaH_forcerehash(hash);
	}

	for (Closure *closure = (Closure *)rootcl.next; closure; closure = (Closure *)closure->head.next) {
		for (int32 i = 0; i <= closure->nelems; i++)
			ResolveObject(&closure->consts[i]);
	}

	GCnode *prevString = &rootglobal;

	for (uint32 i = 0; i < globalCount; i++) {
		TObject tempObject;

		LoadSerial(&tempObject, LUA_T_STRING, restoreFunc);
		ResolveObject(&tempObject);

		TaggedString *tString = tempObject.value.ts;

		if (!tString)
			error("LState::GlobalBeginRestore(): a global string is missing");

		prevString->next = &tString->head;
		prevString = &tString->head;
	}

	prevString->next = nullptr;

	loadInt32(restoreFunc);

	LoadResolvedObject(&errorim, restoreFunc);

	IMtable_size = loadInt32(restoreFunc);
	IMtable = luaM_newvector(IMtable_size, IM);

	for (int32 i = 0; i < IMtable_size; i++) {
		IM *im = &IMtable[i];

		for (int32 j = 0; j < IM_N; j++)
			LoadResolvedObject(&im->int_method[j], restoreFunc);
	}

	last_tag = loadInt32(restoreFunc);
	refSize = loadInt32(restoreFunc);
	refArray = luaM_newvector(refSize, struct ref);

	for (int32 i = 0; i < refSize; i++) {
		LoadResolvedObject(&refArray[i].o, restoreFunc);
		refArray[i].status = (Status)loadInt32(restoreFunc);
	}

	GCthreshold = loadInt32(restoreFunc);
	nblocks = loadInt32(restoreFunc);

	Mbuffsize = loadInt32(restoreFunc);
	Mbuffer = (char *)luaM_malloc(Mbuffsize);
	restoreFunc(Mbuffer, Mbuffsize);

	int32 Mbasepos = loadInt32(restoreFunc);

	Mbuffbase = Mbasepos + Mbuffer;
	Mbuffnext = loadInt32(restoreFunc);

	globalTaskSerialId = loadInt32(restoreFunc);
}

static void RestoreExecutionState(lua_Task *execState, Stack *stack, uint32 numChildren, SaveRestoreAccess *restoreFunc) {
	TObject tempObject;

	if (numChildren) {
		execState->next = luaM_new(lua_Task);
		lua_taskinit(execState->next, nullptr, 0, 0);
		RestoreExecutionState(execState->next, stack, numChildren - 1, restoreFunc);
	}

	execState->S = stack;

	LoadSerial(&tempObject, LUA_T_CLOSURE, restoreFunc);
	ResolveObject(&tempObject);
	execState->cl = tempObject.value.cl;

	LoadSerial(&tempObject, LUA_T_PROTO, restoreFunc);
	ResolveObject(&tempObject);
	execState->tf = tempObject.value.tf;

	execState->base = loadInt32(restoreFunc);
	execState->initBase = loadInt32(restoreFunc);
	execState->initResults = loadInt32(restoreFunc);
	execState->executed = loadInt32(restoreFunc) != 0;

	int32 pcOffset = loadInt32(restoreFunc);

	execState->pc = execState->tf->code + pcOffset;
	execState->aux = loadInt32(restoreFunc);
	execState->consts = execState->tf->consts;
}

static void RestoreState(LState *state, SaveRestoreAccess *restoreFunc) {
	uint32 numExecStates = loadInt32(restoreFunc);

	if (numExecStates) {
		state->task = luaM_new(lua_Task);
		lua_taskinit(state->task, nullptr, 0, 0);
		RestoreExecutionState(state->task, &state->stack, numExecStates - 1, restoreFunc);
	} else {
		state->task = nullptr;
	}

	int32 previousExecState = loadInt32(restoreFunc);

	if (previousExecState >= 0) {
		lua_Task *execState = state->task;

		while (previousExecState--)
			execState = execState->next;

		state->prevTask = execState;
	} else {
		state->prevTask = nullptr;
	}

	state->preventBreakCounter = loadInt32(restoreFunc);
	loadInt32(restoreFunc);
	state->updated = loadInt32(restoreFunc) != 0;
	state->all_paused = loadInt32(restoreFunc) ? 1 : 0;
	state->paused = false;
	state->callLevelCounter = loadInt32(restoreFunc);

	int32 stackSize = loadInt32(restoreFunc);

	luaM_free(state->stack.stack);
	state->stack.stack = luaM_newvector(stackSize, TObject);
	state->stack.last = state->stack.stack + stackSize - 1;

	int32 stackTop = loadInt32(restoreFunc);

	state->stack.top = state->stack.stack + stackTop;

	for (int32 i = 0; i < stackTop; i++)
		LoadResolvedObject(&state->stack.stack[i], restoreFunc);

	state->Cstack.base = loadInt32(restoreFunc);
	state->Cstack.lua2C = loadInt32(restoreFunc);
	state->Cstack.num = loadInt32(restoreFunc);

	state->numCblocks = loadInt32(restoreFunc);

	for (int32 j = 0; j < state->numCblocks; j++) {
		state->Cblocks[j].base = loadInt32(restoreFunc);
		state->Cblocks[j].lua2C = loadInt32(restoreFunc);
		state->Cblocks[j].num = loadInt32(restoreFunc);
	}

	state->id = loadInt32(restoreFunc);
	LoadResolvedObject(&state->taskFunc, restoreFunc);
}

int lua_RestoreRemastered(SaveRestoreAccess *restoreFunc) {
	if (lua_isopen())
		lua_close();

	lua_rootState = lua_state = luaM_new(LState);
	lua_stateinit(lua_state);
	lua_resetglobals();

	GlobalBeginRestore(restoreFunc);

	int32 numThreads = globalTaskSerialId;

	uint32 numTasks = loadInt32(restoreFunc);
	uint32 currentTaskNumber = loadInt32(restoreFunc);

	RestoreState(lua_rootState, restoreFunc);

	LState *lState = lua_rootState;

	for (uint32 i = 1; i < numTasks; i++) {
		LState *newState = luaM_new(LState);

		lua_stateinit(newState);
		RestoreState(newState, restoreFunc);

		newState->prev = lState;
		lState->next = newState;
		lState = newState;
	}

	lua_state = lua_rootState;

	while (currentTaskNumber--)
		lua_state = lua_state->next;

	GlobalEndRestore();

	globalTaskSerialId = numThreads;

	return 0;
}

} // end of namespace Grim
