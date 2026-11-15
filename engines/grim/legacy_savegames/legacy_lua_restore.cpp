#include "common/endian.h"
#include "common/debug.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/legacy_savegames/legacy_savegames_lua.h"

#include "engines/grim/lua/ltask.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/lmem.h"
#include "engines/grim/lua/ldo.h"
#include "engines/grim/lua/ltm.h"
#include "engines/grim/lua/ltable.h"
#include "engines/grim/lua/lvm.h"
#include "engines/grim/lua/lopcodes.h"
#include "engines/grim/lua/lstring.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

static SaveRestoreAccess *restoreFunc;
static uint restoreMinorVersion;

// Set only while a legacy savegame is read
static bool restoreIsLegacy = false;
static bool restoreFailed = false;
static Common::SeekableReadStream *legacyStream = nullptr;
static lua_LegacyFunctionHook legacyFunctionHook = nullptr;
static lua_LegacyUserdataHook legacyUserdataHook = nullptr;

// After a failure every value read is zero, so the loops end quickly
static int legacyStreamReader(void *data, int size) {
	if (size < 0) {
		restoreFailed = true;
		return 1;
	}

	if (restoreFailed || legacyStream->read(data, size) != (uint32)size) {
		restoreFailed = true;
		memset(data, 0, size);
		return 1;
	}

	return 0;
}

// Fixed sane limits, a damaged legacy savegame must not ask for giant allocations
static const int32 kMaxLegacyObjects = 16 * 1024 * 1024;
static const int32 kMaxLegacyStringLength = 64 * 1024 * 1024;

static int32 checkLegacyCount(int32 count, int32 limit) {
	if (count < 0 || count > limit) {
		restoreFailed = true;
		return 0;
	}

	return count;
}

// Called when a legacy savegame mentions a C function that is not available
static void missingLegacyFunction() {
	lua_error("A C function missing when the legacy savegame was loaded has been called");
}

static void readBytes(void *data, int size) {
	restoreFunc(data, size);
}

static uint64 readLEUint64() {
	uint64 data;
	restoreFunc(&data, 8);

	return FROM_LE_64(data);
}

static uint32 readLEUint32() {
	uint32 data;
	restoreFunc(&data, 4);

	return FROM_LE_32(data);
}

static int32 readLESint32() {
	return (int32)readLEUint32();
}

static byte readByte() {
	byte data;
	restoreFunc(&data, 1);

	return data;
}

static bool readBool() {
	return readByte() != 0;
}

static float readFloat() {
	float f;
	uint32 v = readLEUint32();
	memcpy(&f, &v, 4);

	return f;
}

static void restoreObjectValue(TObject *object) {
	object->ttype = (lua_Type)readLESint32();

	switch (object->ttype) {
		case LUA_T_NUMBER:
		case LUA_T_TASK:
			{
				object->value.n = readFloat();
			}
			break;
		case LUA_T_NIL:
			{
				object->value.ts = nullptr;
			}
			break;
		case LUA_T_ARRAY:
			{
				PointerId ptr;
				ptr.id = readLEUint64();
				object->value.a = (Hash *)makePointerFromId(ptr);
			}
			break;
		case LUA_T_USERDATA:
			{
				object->value.ud.id = readLESint32();
				object->value.ud.tag = readLESint32();
				if (restoreMinorVersion == 0) {
					readLEUint32();
					readLEUint32();
				}
				if (restoreIsLegacy) {
					int32 newTag;
					int32 newId;
					if (!restoreFailed && legacyUserdataHook &&
						legacyUserdataHook(object->value.ud.tag, object->value.ud.id, &newTag, &newId)) {
						object->value.ud.tag = newTag;
						object->value.ud.id = newId;
					}
				} else if (lua_restorehook) {
					object->value.ud.id = lua_restorehook(object->value.ud.tag, object->value.ud.id, restoreFunc);
				}
			}
			break;
		case LUA_T_STRING:
			{
				PointerId ptr;
				ptr.id = readLEUint64();
				object->value.ts = (TaggedString *)makePointerFromId(ptr);
			}
			break;
		case LUA_T_PROTO:
		case LUA_T_PMARK:
			{
				PointerId ptr;
				ptr.id = readLEUint64();
				object->value.tf = (TProtoFunc *)makePointerFromId(ptr);
			}
			break;
		case LUA_T_CPROTO:
		case LUA_T_CMARK:
			{
				PointerId ptr;
				ptr.id = readLEUint64();

				// WORKAROUND: C++ forbids casting from a pointer-to-function to a
				// pointer-to-object. We use a union to work around that.
				union {
					void *objPtr;
					lua_CFunction funcPtr;
				} ptrUnion;

				ptrUnion.objPtr = makePointerFromId(ptr);
				object->value.f = ptrUnion.funcPtr;
			}
			break;
		case LUA_T_CLOSURE:
		case LUA_T_CLMARK:
			{
				PointerId ptr;
				ptr.id = readLEUint64();
				object->value.cl = (Closure *)makePointerFromId(ptr);
			}
			break;
		case LUA_T_LINE:
			{
				object->value.i = readLESint32();
			}
			break;
		default:
			PointerId ptr;
			ptr.id = readLEUint64();
			object->value.ts = (TaggedString *)makePointerFromId(ptr);
	}
}

struct ArrayIDObj {
	void *object;
	PointerId idObj;
};

static int sortCallback(const void *id1, const void *id2) {
	if (((const ArrayIDObj *)id1)->idObj.id > ((const ArrayIDObj *)id2)->idObj.id) {
		return 1;
	} else if (((const ArrayIDObj *)id1)->idObj.id < ((const ArrayIDObj *)id2)->idObj.id) {
		return -1;
	} else {
		return 0;
	}
}

int32 arrayHashTablesCount = 0;
int32 arrayProtoFuncsCount = 0;
int32 arrayClosuresCount = 0;
int32 arrayStringsCount = 0;
ArrayIDObj *arrayStrings = nullptr;
ArrayIDObj *arrayHashTables = nullptr;
ArrayIDObj *arrayClosures = nullptr;
ArrayIDObj *arrayProtoFuncs = nullptr;
static bool arraysAllreadySort = false;

static void recreateObj(TObject *obj) {
	if (obj->ttype == LUA_T_CPROTO && restoreIsLegacy) {
		uintptr id = (uintptr)(obj->value.f);
		obj->value.f = legacyFunctionHook((int32)(id >> 16), (int32)(id & 0xffff));
		if (!obj->value.f)
			obj->value.f = missingLegacyFunction;
	} else if (obj->ttype == LUA_T_CPROTO) {
		uintptr id = ((uintptr)(obj->value.f)) >> 16;
		luaL_libList *list = list_of_libs;
		while (list) {
			if (id == 0)
				break;
			id--;
			list = list->next;
		}

		int32 numberFunc = (uintptr)(obj->value.f) & 0xffff;
		if (list && id == 0 && numberFunc < list->number) {
			obj->value.f = list->list[numberFunc].func;
		} else {
			obj->value.f = nullptr;
			assert(obj->value.f);
		}
	} else if (obj->ttype == LUA_T_NIL || obj->ttype == LUA_T_LINE || obj->ttype == LUA_T_NUMBER ||
		   obj->ttype == LUA_T_TASK || obj->ttype == LUA_T_USERDATA) {
		return;
	} else {
		if (obj->value.i == 0)
			return;

		if (!arraysAllreadySort) {
			arraysAllreadySort = true;
			qsort(arrayHashTables, arrayHashTablesCount, sizeof(ArrayIDObj), sortCallback);
			qsort(arrayProtoFuncs, arrayProtoFuncsCount, sizeof(ArrayIDObj), sortCallback);
			qsort(arrayClosures, arrayClosuresCount, sizeof(ArrayIDObj), sortCallback);
			qsort(arrayStrings, arrayStringsCount, sizeof(ArrayIDObj), sortCallback);
		}

		ArrayIDObj *found;
		ArrayIDObj tmpId;
		tmpId.object = nullptr;

		switch (obj->ttype) {
		case LUA_T_PMARK:
			tmpId.idObj = makeIdFromPointer(obj->value.tf);
			found = (ArrayIDObj *)bsearch(&tmpId, arrayProtoFuncs, arrayProtoFuncsCount, sizeof(ArrayIDObj), sortCallback);
			assert(found);
			obj->value.tf = (TProtoFunc *)found->object;
			break;
		case LUA_T_PROTO:
			tmpId.idObj = makeIdFromPointer(obj->value.tf);
			found = (ArrayIDObj *)bsearch(&tmpId, arrayProtoFuncs, arrayProtoFuncsCount, sizeof(ArrayIDObj), sortCallback);
			assert(found);
			obj->value.tf = (TProtoFunc *)found->object;
			break;
		case LUA_T_CLOSURE:
			tmpId.idObj = makeIdFromPointer(obj->value.cl);
			found = (ArrayIDObj *)bsearch(&tmpId, arrayClosures, arrayClosuresCount, sizeof(ArrayIDObj), sortCallback);
			assert(found);
			obj->value.cl = (Closure *)found->object;
			break;
		case LUA_T_ARRAY:
			tmpId.idObj = makeIdFromPointer(obj->value.a);
			found = (ArrayIDObj *)bsearch(&tmpId, arrayHashTables, arrayHashTablesCount, sizeof(ArrayIDObj), sortCallback);
			assert(found);
			obj->value.a = (Hash *)found->object;
			break;
		case LUA_T_STRING:
			tmpId.idObj = makeIdFromPointer(obj->value.ts);
			found = (ArrayIDObj *)bsearch(&tmpId, arrayStrings, arrayStringsCount, sizeof(ArrayIDObj), sortCallback);
			assert(found);
			obj->value.ts = (TaggedString *)found->object;
			break;
		default:
			obj->value.i = 0;
			obj->value.ts = nullptr;
			return;
		}
	}
}

static void restoreState() {
	lua_close();
	lua_rootState = lua_state = luaM_new(LState);
	lua_stateinit(lua_state);
	lua_resetglobals();

	arrayStringsCount = readLESint32();
	arrayClosuresCount = readLESint32();
	arrayHashTablesCount = readLESint32();
	arrayProtoFuncsCount = readLESint32();
	int32 rootGlobalCount = readLESint32();

	if (restoreIsLegacy) {
		arrayStringsCount = checkLegacyCount(arrayStringsCount, kMaxLegacyObjects);
		arrayClosuresCount = checkLegacyCount(arrayClosuresCount, kMaxLegacyObjects);
		arrayHashTablesCount = checkLegacyCount(arrayHashTablesCount, kMaxLegacyObjects);
		arrayProtoFuncsCount = checkLegacyCount(arrayProtoFuncsCount, kMaxLegacyObjects);
		rootGlobalCount = checkLegacyCount(rootGlobalCount, kMaxLegacyObjects);
	}

	arrayStrings = (ArrayIDObj *)luaM_malloc(sizeof(ArrayIDObj) * arrayStringsCount);
	ArrayIDObj *arraysObj = arrayStrings;
	int32 maxStringsLength;
	maxStringsLength = readLESint32();

	if (restoreIsLegacy)
		maxStringsLength = checkLegacyCount(maxStringsLength, kMaxLegacyStringLength);

	char *tempStringBuffer = (char *)luaM_malloc(maxStringsLength + 1); // add extra char for 0 terminate string

	//printf("1: %d\n", g_grim->_savedState->getBufferPos());

	int32 i;
	for (i = 0; i < arrayStringsCount; i++) {
		arraysObj->idObj.id = readLEUint64();
		int32 constIndex = readLESint32();

		TaggedString *tempString = nullptr;
		if (constIndex != -1) {
			TObject obj;
			restoreObjectValue(&obj);
			int32 length = readLESint32();
			if (restoreIsLegacy && (length < 0 || length > maxStringsLength)) {
				restoreFailed = true;
				length = 0;
			}
			readBytes(tempStringBuffer, length);
			tempStringBuffer[length] = '\0';
			tempString = luaS_new(tempStringBuffer);
			tempString->globalval = obj;
		}
		assert(tempString);
		tempString->constindex = constIndex;
		arraysObj->object = tempString;
		arraysObj++;
	}
	luaM_free(tempStringBuffer);

	//printf("2: %d\n", g_grim->_savedState->getBufferPos());

	int32 l;
	Closure *tempClosure;
	GCnode *prevClosure = &rootcl;
	arraysObj = (ArrayIDObj *)luaM_malloc(sizeof(ArrayIDObj) * arrayClosuresCount);
	arrayClosures = arraysObj;
	for (i = 0; i < arrayClosuresCount; i++) {
		arraysObj->idObj.id = readLEUint64();
		int32 countElements = readLESint32();
		tempClosure = (Closure *)luaM_malloc((countElements * sizeof(TObject)) + sizeof(Closure));
		luaO_insertlist(prevClosure, (GCnode *)tempClosure);
		prevClosure = (GCnode *)tempClosure;

		tempClosure->nelems = countElements;
		for (l = 0; l <= tempClosure->nelems; l++) {
			restoreObjectValue(&tempClosure->consts[l]);
		}
		arraysObj->object = tempClosure;
		arraysObj++;
	}

	Hash *tempHash;
	GCnode *prevHash = &roottable;
	arraysObj = (ArrayIDObj *)luaM_malloc(sizeof(ArrayIDObj) * arrayHashTablesCount);
	arrayHashTables = arraysObj;
	for (i = 0; i < arrayHashTablesCount; i++) {
		arraysObj->idObj.id = readLEUint64();
		tempHash = luaM_new(Hash);
		tempHash->nhash = readLESint32();
		tempHash->nuse = readLESint32();
		tempHash->htag = readLESint32();
		tempHash->node = hashnodecreate(tempHash->nhash);
		luaO_insertlist(prevHash, (GCnode *)tempHash);
		prevHash = (GCnode *)tempHash;

		for (l = 0; l < tempHash->nuse; l++) {
			restoreObjectValue(&tempHash->node[l].ref);
			restoreObjectValue(&tempHash->node[l].val);
		}
		arraysObj->object = tempHash;
		arraysObj++;
	}

	TProtoFunc *tempProtoFunc;
	GCnode *oldProto = &rootproto;
	arrayProtoFuncs = (ArrayIDObj *)luaM_malloc(sizeof(ArrayIDObj) * arrayProtoFuncsCount);
	arraysObj = arrayProtoFuncs;
	for (i = 0; i < arrayProtoFuncsCount; i++) {
		arraysObj->idObj.id = readLEUint64();
		tempProtoFunc = luaM_new(TProtoFunc);
		luaO_insertlist(oldProto, (GCnode *)tempProtoFunc);
		oldProto = (GCnode *)tempProtoFunc;
		PointerId ptr;
		ptr.id = readLEUint64();
		tempProtoFunc->fileName = (TaggedString *)makePointerFromId(ptr);
		tempProtoFunc->lineDefined = readLESint32();
		tempProtoFunc->nconsts = readLESint32();
		if (tempProtoFunc->nconsts > 0) {
			tempProtoFunc->consts = (TObject *)luaM_malloc(tempProtoFunc->nconsts * sizeof(TObject));
		} else {
			tempProtoFunc->consts = nullptr;
		}

		for (l = 0; l < tempProtoFunc->nconsts; l++) {
			restoreObjectValue(&tempProtoFunc->consts[l]);
		}

		int32 countVariables = readLESint32();
		if (countVariables) {
			tempProtoFunc->locvars = (LocVar *)luaM_malloc(countVariables * sizeof(LocVar));
		} else {
			tempProtoFunc->locvars = nullptr;
		}

		for (l = 0; l < countVariables; l++) {
			ptr.id = readLEUint64();
			tempProtoFunc->locvars[l].varname = (TaggedString *)makePointerFromId(ptr);
			tempProtoFunc->locvars[l].line = readLESint32();
		}

		int32 codeSize = readLESint32();
		tempProtoFunc->code = (byte *)luaM_malloc(codeSize);
		readBytes(tempProtoFunc->code, codeSize);
		arraysObj->object = tempProtoFunc;
		arraysObj++;
	}

	for (i = 0; i < NUM_HASHS; i++) {
		stringtable *tempStringTable = &string_root[i];
		for (l = 0; l < tempStringTable->size; l++) {
			TaggedString *tempString = tempStringTable->hash[l];
			if (tempString && tempString->constindex != -1 && tempString != &EMPTY) {
				recreateObj(&tempString->globalval);
			}
		}
	}

	tempProtoFunc = (TProtoFunc *)rootproto.next;
	while (tempProtoFunc) {
		TObject tempObj;
		tempObj.value.ts = (TaggedString *)tempProtoFunc->fileName;
		tempObj.ttype = LUA_T_STRING;
		recreateObj(&tempObj);
		tempProtoFunc->fileName = (TaggedString *)tempObj.value.ts;
		for (i = 0; i < tempProtoFunc->nconsts; i++) {
			recreateObj(&tempProtoFunc->consts[i]);
		}

		if (tempProtoFunc->locvars) {
			for (i = 0; tempProtoFunc->locvars[i].line != -1; i++) {
				TObject tempObj2;
				tempObj2.value.ts = tempProtoFunc->locvars[i].varname;
				tempObj2.ttype = LUA_T_STRING;
				recreateObj(&tempObj2);
				tempProtoFunc->locvars[i].varname = (TaggedString *)tempObj2.value.ts;
			}
		}
		tempProtoFunc = (TProtoFunc *)tempProtoFunc->head.next;
	}

	tempHash = (Hash *)roottable.next;
	while (tempHash) {
		for (i = 0; i < tempHash->nuse; i++) {
			recreateObj(&tempHash->node[i].ref);
			recreateObj(&tempHash->node[i].val);
		}
		Node *oldNode = tempHash->node;
		tempHash->node = hashnodecreate(tempHash->nhash);
		for (i = 0; i < tempHash->nuse; i++) {
			Node *newNode = oldNode + i;
			if (newNode->ref.ttype != LUA_T_NIL && newNode->val.ttype != LUA_T_NIL) {
				*node(tempHash, present(tempHash, &newNode->ref)) = *newNode;
			}
		}
		luaM_free(oldNode);
		tempHash = (Hash *)tempHash->head.next;
	}

	tempClosure = (Closure *)rootcl.next;
	while (tempClosure) {
		for (i = 0; i <= tempClosure->nelems; i++) {
			recreateObj(&tempClosure->consts[i]);
		}
		tempClosure = (Closure *)tempClosure->head.next;
	}

	TaggedString *tempListString = (TaggedString *)&(rootglobal);
	for (i = 0; i < rootGlobalCount; i++) {
		TObject tempObj;
		TaggedString *tempString = nullptr;
		tempObj.ttype = LUA_T_STRING;
		PointerId ptr;
		ptr.id = readLEUint64();
		tempObj.value.ts = (TaggedString *)makePointerFromId(ptr);
		recreateObj(&tempObj);
		tempString = (TaggedString *)tempObj.value.ts;
		assert(tempString);
		tempListString->head.next = (GCnode *)tempString;
		tempListString = tempString;
	}
	tempListString->head.next = nullptr;

	restoreObjectValue(&errorim);
	recreateObj(&errorim);

	IMtable_size = readLESint32();
	if (IMtable_size > 0) {
		IMtable = (IM *)luaM_malloc(IMtable_size * sizeof(IM));
		for (i = 0; i < IMtable_size; i++) {
			IM *im = &IMtable[i];
			for (l = 0; l < IM_N; l++) {
				restoreObjectValue(&im->int_method[l]);
				recreateObj(&im->int_method[l]);
			}
		}
	} else {
		IMtable = nullptr;
	}

	last_tag = readLESint32();
	refSize = readLESint32();
	if (refSize > 0) {
		refArray = (ref *)luaM_malloc(refSize * sizeof(ref));
		for (i = 0; i < refSize; i++) {
			restoreObjectValue(&refArray[i].o);
			recreateObj(&refArray[i].o);
			refArray[i].status = (Status)readLESint32();
		}
	} else {
		refArray = nullptr;
	}

	GCthreshold = readLESint32();
	nblocks = readLESint32();

	Mbuffsize = readLESint32();
	Mbuffer = (char *)luaM_malloc(Mbuffsize);
	readBytes(Mbuffer, Mbuffsize);
	int32 MbaseOffset = readLESint32();
	Mbuffbase = MbaseOffset + Mbuffer;
	Mbuffnext = readLESint32();

	globalTaskSerialId = readLESint32();

	int32 countStates = readLESint32();
	int32 currentState = readLESint32();

	LState *state = lua_rootState;
	for (l = 0; l < countStates; l++) {
		if (l == 0)
			state = lua_rootState;
		else {
			LState *s = luaM_new(LState);
			lua_stateinit(s);
			state->next = s;
			s->prev = state;
			state = s;
		}
		int32 countTasks = readLESint32();
		if (countTasks) {
			lua_Task *task = nullptr;
			for (i = 0; i < countTasks; i++) {
				if (i == 0) {
					task = state->task = luaM_new(lua_Task);
					lua_taskinit(task, nullptr, 0, 0);
				} else {
					lua_Task *t = luaM_new(lua_Task);
					lua_taskinit(t, nullptr, 0, 0);
					task->next = t;
					task = t;
				}

				task->S = &state->stack;

				TObject tempObj;
				tempObj.ttype = LUA_T_CLOSURE;
				PointerId ptr;
				ptr.id = readLEUint64();
				tempObj.value.cl = (Closure *)makePointerFromId(ptr);
				recreateObj(&tempObj);
				task->cl = (Closure *)tempObj.value.cl;
				tempObj.ttype = LUA_T_PROTO;
				ptr.id = readLEUint64();
				tempObj.value.tf = (TProtoFunc *)makePointerFromId(ptr);
				recreateObj(&tempObj);
				task->tf = (TProtoFunc *)tempObj.value.tf;

				task->base = readLESint32();
				task->initBase = readLESint32();
				task->initResults = readLESint32();
				task->executed = readBool();
				int32 pcOffset = readLESint32();
				task->aux = readLESint32();
				if (restoreFailed) {
					task->pc = nullptr;
					task->consts = nullptr;
				} else {
					task->pc = task->tf->code + pcOffset;
					task->consts = task->tf->consts;
				}
			}
		} else {
			state->task = nullptr;
		}
		int32 n = readLESint32();
		if (n < 0) {
			state->prevTask = nullptr;
		} else {
			state->prevTask = state->task;
			for (; n; n--)
				state->prevTask = state->prevTask->next;
		}

		state->updated = readBool();

		byte pauseState = readByte();
		state->all_paused = pauseState & LUA_SG_ALL_PAUSED;
		state->paused = (pauseState & LUA_SG_PAUSED) ? true : false;

		state->preventBreakCounter = readLESint32();
		state->callLevelCounter = readLESint32();

		int32 stackLastSize = readLESint32();
		if (state->stack.stack)
			luaM_free(state->stack.stack);

		state->stack.stack = (TObject *)luaM_malloc(stackLastSize * sizeof(TObject));
		state->stack.last = state->stack.stack + stackLastSize - 1;

		int32 stackTopSize = readLESint32();
		state->stack.top = state->stack.stack + stackTopSize;
		for (i = 0; i < stackTopSize; i++) {
			restoreObjectValue(&state->stack.stack[i]);
			recreateObj(&state->stack.stack[i]);
		}

		state->Cstack.base = readLESint32();
		state->Cstack.lua2C = readLESint32();
		state->Cstack.num = readLESint32();

		state->numCblocks = readLESint32();
		for (i = 0; i < state->numCblocks; i++) {
			state->Cblocks[i].base = readLESint32();
			state->Cblocks[i].lua2C = readLESint32();
			state->Cblocks[i].num = readLESint32();
		}

		if (restoreMinorVersion >= 3) {
			state->sleepFor = readLEUint32();
		}
		state->id = readLEUint32();
		restoreObjectValue(&state->taskFunc);
		if (state->taskFunc.ttype == LUA_T_PROTO || state->taskFunc.ttype == LUA_T_CPROTO)
			recreateObj(&state->taskFunc);
	}

	for (; currentState; currentState--)
		lua_state = lua_state->next;

	arraysAllreadySort = false;
	arrayStringsCount = 0;
	arrayHashTablesCount = 0;
	arrayClosuresCount = 0;
	arrayProtoFuncsCount = 0;
	luaM_free(arrayClosures);
	luaM_free(arrayStrings);
	luaM_free(arrayHashTables);
	luaM_free(arrayProtoFuncs);
	arrayHashTables = nullptr;
	arrayClosures = nullptr;
	arrayProtoFuncs = nullptr;
	arrayStrings = nullptr;
}

int lua_RestoreLegacy(Common::SeekableReadStream *stream, int minorVersion, lua_LegacyFunctionHook functionHook, lua_LegacyUserdataHook userdataHook) {
	if (!functionHook) {
		warning("lua_RestoreLegacy(): no function hook");
		return 1;
	}

	legacyStream = stream;
	legacyFunctionHook = functionHook;
	legacyUserdataHook = userdataHook;
	restoreFunc = legacyStreamReader;
	restoreMinorVersion = minorVersion;
	restoreIsLegacy = true;
	restoreFailed = false;

	restoreState();

	bool failed = restoreFailed;

	restoreIsLegacy = false;
	restoreFailed = false;
	legacyStream = nullptr;
	legacyFunctionHook = nullptr;
	legacyUserdataHook = nullptr;

	if (failed) {
		warning("lua_RestoreLegacy(): the Lua state is damaged or truncated");
		return 1;
	}

	return 0;
}

} // end of namespace Grim
