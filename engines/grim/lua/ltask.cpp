#define FORBIDDEN_SYMBOL_EXCEPTION_setjmp
#define FORBIDDEN_SYMBOL_EXCEPTION_longjmp

#include "engines/grim/lua/ltask.h"
#include "engines/grim/lua/lapi.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/lmem.h"
#include "engines/grim/lua/ldo.h"
#include "engines/grim/lua/lvm.h"

#include "engines/grim/lua.h"

#include "common/textconsole.h"

namespace Grim {

static bool lua_istask(lua_Object object) {
	return object != LUA_NOOBJECT && ttype(Address(object)) == LUA_T_TASK;
}

static int lua_gettask(lua_Object object) {
	if (!lua_istask(object))
		return -1;

	return (int)nvalue(Address(object));
}

static void lua_pushtask(int handle) {
	ttype(lua_state->stack.top) = LUA_T_TASK;
	nvalue(lua_state->stack.top) = (float)handle;
	incr_top;
}

static bool isSameFunction(const TObject *a, const TObject *b) {
	return a->ttype == b->ttype && memcmp(&a->value, &b->value, sizeof(Value)) == 0;
}

void lua_taskinit(lua_Task *task, lua_Task *next, StkId tbase, int results) {
	task->executed = false;
	task->next = next;
	task->initBase = tbase;
	task->initResults = results;
	task->opcodeCount = 0;
}

void lua_taskresume(lua_Task *task, Closure *closure, TProtoFunc *protofunc, StkId tbase) {
	task->cl = closure;
	task->tf = protofunc;
	task->base = tbase;
	task->pc = task->tf->code;
	task->consts = task->tf->consts;
	task->S = &lua_state->stack;
}

void start_script() {
	lua_Object paramObj = lua_getparam(1);
	lua_Type type = paramObj == LUA_NOOBJECT ? LUA_T_NIL : ttype(Address(paramObj));

	if (luaIsRemastered) {
		if (!lua_isfunction(paramObj))
			return;
	} else if (luaIsEmi) {
		if (!lua_isfunction(paramObj)) {
			LuaBase *lua = LuaBase::instance();
			if (lua)
				lua->logError("startscript: First parameter must be a function!\n");

			return;
		}
	} else if (paramObj == LUA_NOOBJECT || (type != LUA_T_CPROTO && type != LUA_T_PROTO)) {
		return;
	}

	LState *state = luaM_new(LState);
	lua_stateinit(state);

	state->next = lua_state->next;
	state->prev = lua_state;
	if (state->next)
		state->next->prev = state;
	lua_state->next = state;

	state->taskFunc.ttype = type;
	state->taskFunc.value = Address(paramObj)->value;

	int l = 2;
	for (lua_Object object = lua_getparam(l++); object != LUA_NOOBJECT; object = lua_getparam(l++)) {
		TObject ptr;
		ptr.ttype = Address(object)->ttype;
		ptr.value = Address(object)->value;
		LState *tmpState = lua_state;
		lua_state = state;
		luaA_pushobject(&ptr);
		lua_state = tmpState;
	}

	ttype(lua_state->stack.top) = LUA_T_TASK;
	nvalue(lua_state->stack.top) = (float)state->id;
	incr_top;
}

void next_script() {
	lua_Object paramObj = lua_getparam(1);
	lua_Type type = paramObj == LUA_NOOBJECT ? LUA_T_NIL : ttype(Address(paramObj));

	if (luaIsEmi || luaIsRemastered) {
		if (paramObj == LUA_NOOBJECT)
			lua_error("Too few arguments to nextscript! Specify nil or a task handle.");

		if (!lua_isnil(paramObj) && !lua_istask(paramObj))
			lua_error("Argument to nextscript must be nil or a task handle!");
	} else if (paramObj == LUA_NOOBJECT || (type != LUA_T_TASK && type != LUA_T_NIL)) {
		lua_error("Bad argument to next_script");
	}

	if (type == LUA_T_TASK) {
		uint32 task = (uint32)nvalue(Address(paramObj));
		LState *state;
		for (state = lua_rootState->next; state != nullptr; state = state->next) {
			if (state->id == task) {
				if (state->next) {
					ttype(lua_state->stack.top) = LUA_T_TASK;
					nvalue(lua_state->stack.top) = (float)state->next->id;
					incr_top;
				} else
					lua_pushnil();
				return;
			}
		}
	}

	if (lua_rootState->next) {
		ttype(lua_state->stack.top) = LUA_T_TASK;
		nvalue(lua_state->stack.top) = (float)lua_rootState->next->id;
		incr_top;
	} else
		lua_pushnil();
}

static void stop_script_v2() {
	lua_Object paramObj = lua_getparam(1);

	if (paramObj == LUA_NOOBJECT)
		lua_error("Too few arguments to stopscript! Specify the name of a function or a task handle.");

	if (!lua_isfunction(paramObj) && !lua_istask(paramObj))
		lua_error("Argument to stopscript must be a task handle or function!");

	LState *state;

	if (!lua_istask(paramObj)) {
		TObject function = *Address(paramObj);

		for (state = lua_rootState->next; state != nullptr;) {
			if (!isSameFunction(&state->taskFunc, &function)) {
				state = state->next;
			} else if (state == lua_state) {
				LuaBase *lua = LuaBase::instance();
				if (lua)
					lua->logError("stopscript: Batch stop can't stop active task, skipping.\n");

				state = state->next;
			} else {
				LState *nextState = state->next;

				lua_statedeinit(state);
				luaM_free(state);
				state = nextState;
			}
		}

		return;
	}

	int task = lua_gettask(paramObj);

	for (state = lua_rootState->next; state != nullptr; state = state->next) {
		if (state->id == (uint32)task)
			break;
	}

	if (!state) {
		LuaBase *lua = LuaBase::instance();
		if (lua)
			lua->logError("stopscript: Task is no longer active.\n");

		lua_pushnumber(1.0f);
	} else if (state == lua_state) {
		lua_error("Current task is committing suicide!\n");
	} else {
		lua_statedeinit(state);
		luaM_free(state);
	}
}

static void stop_script_remastered() {
	lua_Object paramObj = lua_getparam(1);

	if (paramObj == LUA_NOOBJECT)
		lua_error("Too few arguments to stopscript! Specify the name of a function or a task handle.");

	if (!lua_isfunction(paramObj) && !lua_istask(paramObj))
		lua_error("Argument to stopscript must be a task handle or function!");

	LState *state;

	if (lua_istask(paramObj)) {
		uint32 task = (uint32)lua_gettask(paramObj);

		for (state = lua_rootState->next; state != nullptr; state = state->next) {
			if (state->id == task)
				break;
		}

		if (!state)
			return;

		if (state == lua_state)
			lua_error("Current task is committing suicide!\n");

		lua_statedeinit(state);
		luaM_free(state);
		return;
	}

	TObject function = *Address(paramObj);

	for (state = lua_rootState->next; state != nullptr;) {
		if (isSameFunction(&state->taskFunc, &function) && state != lua_state) {
			LState *nextState = state->next;

			lua_statedeinit(state);
			luaM_free(state);
			state = nextState;
		} else {
			state = state->next;
		}
	}
}

void stop_script() {
	if (luaIsRemastered) {
		stop_script_remastered();
		return;
	}

	if (luaIsEmi) {
		stop_script_v2();
		return;
	}

	lua_Object paramObj = lua_getparam(1);
	lua_Type type = paramObj == LUA_NOOBJECT ? LUA_T_NIL : ttype(Address(paramObj));
	LState *state;

	if (paramObj == LUA_NOOBJECT || (type != LUA_T_CPROTO && type != LUA_T_PROTO && type != LUA_T_TASK))
		lua_error("Bad argument to stop_script");

	if (type == LUA_T_TASK) {
		uint32 task = (uint32)nvalue(Address(paramObj));
		for (state = lua_rootState->next; state != nullptr; state = state->next) {
			if (state->id == task)
				break;
		}
		if (state) {
			if (state != lua_state) {
				lua_statedeinit(state);
				luaM_free(state);
			}
		}
	} else if (type == LUA_T_PROTO || type == LUA_T_CPROTO) {
		for (state = lua_rootState->next; state != nullptr;) {
			bool match;
			if (type == LUA_T_PROTO) {
				match = (state->taskFunc.ttype == type && tfvalue(&state->taskFunc) == tfvalue(Address(paramObj)));
			} else {
				match = (state->taskFunc.ttype == type && fvalue(&state->taskFunc) == fvalue(Address(paramObj)));
			}
			if (match && state != lua_state) {
				LState *tmp = state->next;
				lua_statedeinit(state);
				luaM_free(state);
				state = tmp;
			} else {
				state = state->next;
			}
		}
	}
}

void identify_script() {
	lua_Object paramObj = lua_getparam(1);
	lua_Type type = paramObj == LUA_NOOBJECT ? LUA_T_NIL : ttype(Address(paramObj));

	if (luaIsEmi || luaIsRemastered) {
		if (paramObj == LUA_NOOBJECT)
			lua_error("Too few arguments to identifyscript! Specify nil or a task handle.");

		if (!lua_istask(paramObj))
			lua_error("Argument to identifyscript must be a task handle!");
	} else if (paramObj == LUA_NOOBJECT || type != LUA_T_TASK) {
		lua_error("Bad argument to identify_script");
	}

	uint32 task = (uint32)nvalue(Address(paramObj));
	LState *state;
	for (state = lua_rootState->next; state != nullptr; state = state->next) {
		if (state->id == task) {
			luaA_pushobject(&state->taskFunc);
			return;
		}
	}

	lua_pushnil();
}

static void find_script_v2() {
	lua_Object paramObj = lua_getparam(1);
	lua_Object afterObj = lua_getparam(2);

	if (!lua_isfunction(paramObj) && !lua_istask(paramObj) && !lua_isnil(paramObj))
		lua_error("Argument to findscript must be a function or task handle or nil!");

	if (lua_isnil(paramObj)) {
		lua_pushtask(lua_state->id);
		lua_pushnumber(1.0f);
		return;
	}

	LState *state = lua_rootState->next;

	if (!lua_istask(paramObj)) {
		if (lua_isfunction(paramObj)) {
			TObject function = *Address(paramObj);
			int count = 0;
			uint32 handle = 0;

			if (lua_istask(afterObj)) {
				uint32 after = (uint32)lua_gettask(afterObj);
				bool isAfterFound = false;

				while (state && !isAfterFound) {
					isAfterFound = state->id == after;
					state = state->next;
				}
			}

			for (; state != nullptr; state = state->next) {
				if (isSameFunction(&state->taskFunc, &function)) {
					count++;

					if (!handle)
						handle = state->id;
				}
			}

			if (count) {
				lua_pushtask(handle);
				lua_pushnumber((float)count);
				return;
			}
		}

		lua_pushnil();
		lua_pushnumber(0.0f);
		return;
	}

	uint32 task = (uint32)lua_gettask(paramObj);

	while (state && state->id != task)
		state = state->next;

	if (!state) {
		lua_pushnil();
		lua_pushnumber(0.0f);
		return;
	}

	lua_pushobject(paramObj);
	lua_pushnumber(1.0f);
	lua_pushnumber(state->paused ? 1.0f : 0.0f);
}

static void find_script_remastered() {
	lua_Object paramObj = lua_getparam(1);

	if (paramObj == LUA_NOOBJECT)
		lua_error("Too few arguments to findscript! Specify a function or a task handle.");

	if (!lua_isfunction(paramObj) && !lua_istask(paramObj))
		lua_error("Argument to findscript must be a function or task handle!");

	LState *state;

	if (lua_istask(paramObj)) {
		uint32 task = (uint32)lua_gettask(paramObj);

		for (state = lua_rootState->next; state != nullptr; state = state->next) {
			if (state->id == task) {
				lua_pushobject(paramObj);
				lua_pushnumber(1.0f);
				return;
			}
		}
	} else {
		TObject function = *Address(paramObj);
		int count = 0;
		uint32 handle = 0;

		for (state = lua_rootState->next; state != nullptr; state = state->next) {
			if (isSameFunction(&state->taskFunc, &function)) {
				count++;
				handle = state->id;
			}
		}

		if (count) {
			lua_pushtask(handle);
			lua_pushnumber((float)count);
			return;
		}
	}

	lua_pushnil();
	lua_pushnumber(0.0f);
}

void find_script() {
	if (luaIsRemastered) {
		find_script_remastered();
		return;
	}

	if (luaIsEmi) {
		find_script_v2();
		return;
	}

	lua_Object paramObj = lua_getparam(1);
	lua_Type type = paramObj == LUA_NOOBJECT ? LUA_T_NIL : ttype(Address(paramObj));

	if (type != LUA_T_CPROTO && type != LUA_T_PROTO && type != LUA_T_TASK && type != LUA_T_NIL)
		lua_error("Bad argument to find_script");

	if (type == LUA_T_NIL) {
		ttype(lua_state->stack.top) = LUA_T_TASK;
		nvalue(lua_state->stack.top) = lua_state->id;
		incr_top;
		lua_pushnumber(1.0f);
		return;
	}

	if (type == LUA_T_TASK) {
		uint32 task = (uint32)nvalue(Address(paramObj));
		LState *state;
		for (state = lua_rootState->next; state != nullptr; state = state->next) {
			if (state->id == task) {
				lua_pushobject(paramObj);
				lua_pushnumber(1.0f);
				return;
			}
		}
	} else if (type == LUA_T_PROTO || type == LUA_T_CPROTO) {
		int task = -1, countTasks = 0;
		bool match;
		LState *state;
		for (state = lua_rootState->next; state != nullptr; state = state->next) {
			if (type == LUA_T_PROTO) {
				match = (state->taskFunc.ttype == type && tfvalue(&state->taskFunc) == tfvalue(Address(paramObj)));
			} else {
				match = (state->taskFunc.ttype == type && fvalue(&state->taskFunc) == fvalue(Address(paramObj)));
			}
			if (match) {
				task = state->id;
				countTasks++;
			}
		}
		if (countTasks) {
			assert(task != -1);
			ttype(lua_state->stack.top) = LUA_T_TASK;
			nvalue(lua_state->stack.top) = (float)task;
			incr_top;
			lua_pushnumber((float)countTasks);
			return;
		}
	}

	lua_pushnil();
	lua_pushnumber(0.0f);
}

void pause_script() {
	lua_Object taskObj = lua_getparam(1);
	lua_Type type = taskObj == LUA_NOOBJECT ? LUA_T_NIL : ttype(Address(taskObj));

	if (luaIsEmi) {
		if (taskObj == LUA_NOOBJECT)
			lua_error("Too few arguments to pausescript! Specify a task handle.");

		if (!lua_istask(taskObj))
			lua_error("Argument to pausescript must be a task handle!");

		uint32 handle = (uint32)lua_gettask(taskObj);
		LState *taskState;

		for (taskState = lua_rootState->next; taskState != nullptr; taskState = taskState->next) {
			if (taskState->id == handle)
				break;
		}

		if (taskState) {
			taskState->pauseSticky = true;
			taskState->paused = true;
		} else {
			LuaBase *lua = LuaBase::instance();
			if (lua)
				lua->logError("pausescript: Task is no longer active.\n");
		}

		return;
	}

	if (type != LUA_T_TASK) {
		lua_error("Bad argument to pause_script");
		return;
	}

	uint32 task = (uint32)nvalue(Address(taskObj));
	LState *state;
	for (state = lua_rootState->next; state != nullptr; state = state->next) {
		if (state->id == task) {
			state->paused = true;
			return;
		}
	}
}

void pause_scripts() {
	LState *t;
	lua_Object boolObj = lua_getparam(1);

	if (luaIsRemastered) {
		for (t = lua_rootState->next; t != nullptr; t = t->next) {
			if (t != lua_state)
				t->all_paused = 1;
		}

		return;
	}

	if (luaIsEmi) {
		for (t = lua_rootState->next; t != nullptr; t = t->next) {
			if (t != lua_state) {
				t->paused = true;

				if (lua_isnil(boolObj))
					t->pauseSticky = true;
			}
		}

		return;
	}

	bool p = false;
	if (!lua_isnil(boolObj))
		p = true;

	for (t = lua_rootState->next; t != nullptr; t = t->next) {
		if (lua_state != t) {
			if (p) {
				t->all_paused++;
			} else {
				t->all_paused = 1;
			}
		}

	}
}

void unpause_script() {
	lua_Object taskObj = lua_getparam(1);
	lua_Type type = taskObj == LUA_NOOBJECT ? LUA_T_NIL : ttype(Address(taskObj));

	if (luaIsEmi) {
		if (taskObj == LUA_NOOBJECT)
			lua_error("Too few arguments to unpausescript! Specify a task handle.");

		if (!lua_istask(taskObj))
			lua_error("Argument to unpausescript must be a task handle!");

		uint32 handle = (uint32)lua_gettask(taskObj);
		LState *taskState;

		for (taskState = lua_rootState->next; taskState != nullptr; taskState = taskState->next) {
			if (taskState->id == handle)
				break;
		}

		if (taskState) {
			taskState->pauseSticky = false;
			taskState->paused = false;
		} else {
			LuaBase *lua = LuaBase::instance();
			if (lua)
				lua->logError("unpausescript: Task is no longer active.\n");
		}

		return;
	}

	if (type != LUA_T_TASK) {
		lua_error("Bad argument to unpause_script");
		return;
	}

	uint32 task = (uint32)nvalue(Address(taskObj));
	LState *state;
	for (state = lua_rootState->next; state != nullptr; state = state->next) {
		if (state->id == task) {
			state->paused = false;
			return;
		}
	}
}

void unpause_scripts() {
	LState *t;
	lua_Object boolObj = lua_getparam(1);

	if (luaIsRemastered) {
		for (t = lua_rootState->next; t != nullptr; t = t->next)
			t->all_paused = 0;

		return;
	}

	if (luaIsEmi) {
		for (t = lua_rootState->next; t != nullptr; t = t->next) {
			if (lua_isnil(boolObj))
				t->pauseSticky = false;

			if (!t->pauseSticky)
				t->paused = false;
		}

		return;
	}

	bool p = false;
	if (!lua_isnil(boolObj))
		p = true;

	for (t = lua_rootState->next; t != nullptr; t = t->next) {
		if (lua_state != t) {
			if (p) {
				if (t->all_paused > 0)
					t->all_paused--;
			} else {
				t->all_paused = 0;
			}
		}
	}
}

void current_script() {
	ttype(lua_state->stack.top) = LUA_T_TASK;
	nvalue(lua_state->stack.top) = (float)lua_state->id;
	incr_top;
}

void break_here() {}

void sleep_for() {
	lua_Object msObj = lua_getparam(1);

	if (lua_isnumber(msObj)) {
		int ms = (int)lua_getnumber(msObj);
		lua_state->sleepFor = ms;
	} else if (luaIsEmi) {
		LuaBase *lua = LuaBase::instance();
		if (lua)
			lua->logError("sleep_for: not passed a valid time in ms");
	}
}

void lua_runtasks(int frameTime) {
	if (!lua_state || !lua_state->next) {
		return;
	}

	// Mark all the states to be updated
	LState *state = lua_state->next;
	do {
		for (lua_Task *task = state->task; task != nullptr; task = task->next)
			task->opcodeCount = 0;

		if (luaIsEmi) {
			state->updated = false;

			if (state->sleepFor >= 0 && !state->paused)
				state->sleepFor -= frameTime;
		} else if (state->sleepFor > 0) {
			state->sleepFor -= frameTime;
		} else {
			state->updated = false;
		}
		state = state->next;
	} while	(state);

	// And run them
	runtasks(lua_state);
}

void runtasks(LState *const rootState) {
	lua_state = lua_state->next;
	while (lua_state) {
		LState *nextState = nullptr;
		bool stillRunning;
		if (luaIsEmi && lua_state->sleepFor >= 0) {
			lua_state->updated = true;
			nextState = lua_state->next;
		} else if (!lua_state->all_paused && !lua_state->updated && !lua_state->paused) {
			jmp_buf errorJmp;
			lua_state->errorJmp = &errorJmp;
			if (setjmp(errorJmp)) {
				lua_Task *t, *m;
				for (t = lua_state->task; t != nullptr;) {
					m = t->next;
					luaM_free(t);
					t = m;
				}
				stillRunning = false;
				lua_state->task = nullptr;
			} else {
				if (lua_state->task) {
					stillRunning = luaD_call(lua_state->task->initBase, lua_state->task->initResults);
				} else {
					StkId base = lua_state->Cstack.base;
					luaD_openstack((lua_state->stack.top - lua_state->stack.stack) - base);
					set_normalized(lua_state->stack.stack + lua_state->Cstack.base, &lua_state->taskFunc);
					stillRunning = luaD_call(base + 1, 255);
				}
			}
			nextState = lua_state->next;
			// The state returned. Delete it
			if (!stillRunning) {
				lua_statedeinit(lua_state);
				luaM_free(lua_state);
			} else {
				lua_state->updated = true;
			}
		} else {
			nextState = lua_state->next;
		}
		lua_state = nextState;
	}

	// Restore the value of lua_state to the main script
	lua_state = rootState;
	// Check for states that may have been created in this run.
	LState *state = lua_state->next;
	while (state) {
		if (!state->all_paused && !state->paused && !state->updated && (!luaIsEmi || lua_state->sleepFor < 0)) {
			// New state! Run a new pass.
			runtasks(rootState);
			return;
		}
		state = state->next;
	}
}

} // end of namespace Grim
