/*
** Lua API
** See Copyright Notice in lua.h
*/

#define FORBIDDEN_SYMBOL_EXCEPTION_setjmp
#define FORBIDDEN_SYMBOL_EXCEPTION_longjmp

#include "common/str.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/array.h"
#include "common/debug.h"
#include "common/util.h"

#include "engines/grim/lua/lapi.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/ldo.h"
#include "engines/grim/lua/lfunc.h"
#include "engines/grim/lua/lgc.h"
#include "engines/grim/lua/lmem.h"
#include "engines/grim/lua/lobject.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lstring.h"
#include "engines/grim/lua/ltable.h"
#include "engines/grim/lua/ltm.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/lua/luadebug.h"
#include "engines/grim/lua/lvm.h"


namespace Grim {

char lua_ident[] = "$Lua: " LUA_VERSION " " LUA_COPYRIGHT " $\n"
"$Autores:  " LUA_AUTHORS " $";

TObject *luaA_Address(lua_Object o) {
	return Address(o);
}

static int32 normalized_type(TObject *o) {
	int32 t = ttype(o);
	switch (t) {
	case LUA_T_PMARK:
		return LUA_T_PROTO;
	case LUA_T_CMARK:
		return LUA_T_CPROTO;
	case LUA_T_CLMARK:
		return LUA_T_CLOSURE;
	default:
		return t;
	}
}

void set_normalized(TObject *d, TObject *s) {
	d->value = s->value;
	d->ttype = (lua_Type)normalized_type(s);
}

static TObject *luaA_protovalue (TObject *o) {
	return (normalized_type(o) == LUA_T_CLOSURE) ?  protovalue(o) : o;
}

void luaA_packresults() {
	luaV_pack(lua_state->Cstack.lua2C, lua_state->Cstack.num, lua_state->stack.top);
	incr_top;
}

int32 luaA_passresults() {
	luaD_checkstack(lua_state->Cstack.num);
	memcpy(lua_state->stack.top, lua_state->Cstack.lua2C + lua_state->stack.stack, lua_state->Cstack.num * sizeof(TObject));
	lua_state->stack.top += lua_state->Cstack.num;
	return lua_state->Cstack.num;
}

static void checkCparams(int32 params) {
	if (lua_state->stack.top - lua_state->stack.stack < lua_state->Cstack.base + params)
		lua_error("API error - wrong number of arguments in C2lua stack");
}

static lua_Object put_luaObject(TObject *o) {
	luaD_openstack((lua_state->stack.top - lua_state->stack.stack) - lua_state->Cstack.base);
	lua_state->stack.stack[lua_state->Cstack.base++] = *o;

	return lua_state->Cstack.base;  // this is +1 real position (see Ref)
}

static lua_Object put_luaObjectonTop() {
	luaD_openstack((lua_state->stack.top - lua_state->stack.stack) - lua_state->Cstack.base);
	lua_state->stack.stack[lua_state->Cstack.base++] = *(--lua_state->stack.top);
	return lua_state->Cstack.base;  // this is +1 real position (see Ref)
}

lua_Object lua_pop() {
	checkCparams(1);
	return put_luaObjectonTop();
}

/*
** Get a parameter, returning the object handle or LUA_NOOBJECT on error.
** 'number' must be 1 to get the first parameter.
*/
lua_Object lua_lua2C(int32 number) {
	if (number <= 0 || number > lua_state->Cstack.num)
		return LUA_NOOBJECT;
	return lua_state->Cstack.lua2C + number;
}

int32 lua_callfunction(lua_Object function) {
	if (function == LUA_NOOBJECT) {
		return 1;
	} else {
		luaD_openstack((lua_state->stack.top - lua_state->stack.stack) - lua_state->Cstack.base);
		set_normalized(lua_state->stack.stack + lua_state->Cstack.base, Address(function));
		return luaD_protectedrun(MULT_RET);
	}
}

lua_Object lua_gettagmethod(int32 tag, const char *event) {
	return put_luaObject(luaT_gettagmethod(tag, event));
}

lua_Object lua_settagmethod(int32 tag, const char *event) {
	checkCparams(1);
	luaT_settagmethod(tag, event, lua_state->stack.top - 1);
	return put_luaObjectonTop();
}

lua_Object lua_seterrormethod() {
	TObject temp = errorim;
	checkCparams(1);
	errorim = *(--lua_state->stack.top);
	return put_luaObject(&temp);
}

lua_Object lua_gettable() {
	checkCparams(2);
	luaV_gettable();
	return put_luaObjectonTop();
}

lua_Object lua_rawgettable() {
	checkCparams(2);
	if (ttype(lua_state->stack.top-2) != LUA_T_ARRAY) {
		lua_error("indexed expression not a table in rawgettable");
	} else {
		TObject *h = luaH_get(avalue(lua_state->stack.top - 2), lua_state->stack.top - 1);
		--lua_state->stack.top;
		if (h) {
			*(lua_state->stack.top-1) = *h;
		} else {
			ttype(lua_state->stack.top-1) = LUA_T_NIL;
		}
	}
	return put_luaObjectonTop();
}

void lua_settable() {
	checkCparams(3);
	luaV_settable(lua_state->stack.top-3, 1);
}

void lua_rawsettable() {
	checkCparams(3);
	luaV_settable(lua_state->stack.top-3, 0);
}

lua_Object lua_createtable() {
	TObject o;
	luaC_checkGC();
	avalue(&o) = luaH_new(0);
	ttype(&o) = LUA_T_ARRAY;
	return put_luaObject(&o);
}

lua_Object lua_getglobal(const char *name) {
	luaD_checkstack(2);  // may need that to call T.M.
	luaV_getglobal(luaS_new(name));
	return put_luaObjectonTop();
}

lua_Object lua_rawgetglobal(const char *name) {
	TaggedString *ts = luaS_new(name);
	return put_luaObject(&ts->globalval);
}

void lua_setglobal(const char *name) {
	checkCparams(1);
	luaD_checkstack(2);  // may need that to call T.M.
	luaV_setglobal(luaS_new(name));
}

void lua_rawsetglobal(const char *name) {
	TaggedString *ts = luaS_new(name);
	checkCparams(1);
	luaS_rawsetglobal(ts, --lua_state->stack.top);
}

int32 lua_isnil(lua_Object o) {
	return (o == LUA_NOOBJECT) || (ttype(Address(o)) == LUA_T_NIL);
}

int32 lua_istable(lua_Object o) {
	return (o != LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_ARRAY);
}

int32 lua_isuserdata(lua_Object o) {
	return (o != LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_USERDATA);
}

int32 lua_iscfunction(lua_Object o) {
	return (lua_tag(o) == LUA_T_CPROTO);
}

int32 lua_isnumber(lua_Object o) {
	return (o!= LUA_NOOBJECT) && (tonumber(Address(o)) == 0);
}

int32 lua_isstring(lua_Object o) {
	int32 t = lua_tag(o);
	return (t == LUA_T_STRING) || (t == LUA_T_NUMBER);
}

int32 lua_isfunction(lua_Object o) {
	int32 t = lua_tag(o);
	return (t == LUA_T_PROTO) || (t == LUA_T_CPROTO);
}

float lua_getnumber(lua_Object object) {
	if (object == LUA_NOOBJECT)
		return 0.0f;
	if (tonumber(Address(object)))
		return 0.0f;
	else
		return (nvalue(Address(object)));
}

const char *lua_getstring (lua_Object object) {
	if (object == LUA_NOOBJECT || tostring(Address(object)))
		return nullptr;
	else
		return (svalue(Address(object)));
}

int32 lua_getuserdata(lua_Object object) {
	if (object == LUA_NOOBJECT || ttype(Address(object)) != LUA_T_USERDATA)
		return 0;
	else
		return (Address(object))->value.ud.id;
}

lua_CFunction lua_getcfunction(lua_Object object) {
	if (!lua_iscfunction(object))
		return nullptr;
	else
		return fvalue(luaA_protovalue(Address(object)));
}

void lua_pushnil() {
	ttype(lua_state->stack.top) = LUA_T_NIL;
	incr_top;
}

void lua_pushnumber(float n) {
	ttype(lua_state->stack.top) = LUA_T_NUMBER;
	nvalue(lua_state->stack.top) = n;
	incr_top;
}

void lua_pushstring(const char *s) {
	if (!s)
		ttype(lua_state->stack.top) = LUA_T_NIL;
	else {
		tsvalue(lua_state->stack.top) = luaS_new(s);
		ttype(lua_state->stack.top) = LUA_T_STRING;
	}
	incr_top;
	luaC_checkGC();
}

void lua_pushCclosure(lua_CFunction fn, int32 n) {
	if (!fn)
		lua_error("API error - attempt to push a NULL Cfunction");
	checkCparams(n);
	ttype(lua_state->stack.top) = LUA_T_CPROTO;
	fvalue(lua_state->stack.top) = fn;
	incr_top;
	luaV_closure(n);
}

void lua_pushusertag(int32 u, int32 tag) {
	if (tag < 0 && tag != LUA_ANYTAG)
		luaT_realtag(tag);  // error if tag is not valid
	lua_state->stack.top->value.ud.id = u;
	lua_state->stack.top->value.ud.tag = tag;
	ttype(lua_state->stack.top) = LUA_T_USERDATA;
	incr_top;
	luaC_checkGC();
}

void luaA_pushobject(TObject *o) {
	*lua_state->stack.top = *o;
	incr_top;
}

void lua_pushobject(lua_Object o) {
	if (o == LUA_NOOBJECT)
		lua_error("API error - attempt to push a NOOBJECT");
	else {
		set_normalized(lua_state->stack.top, Address(o));
		incr_top;
	}
}

int32 lua_tag(lua_Object lo) {
	if (lo == LUA_NOOBJECT)
		return LUA_T_NIL;
	else {
		TObject *o = Address(lo);
		int32 t;
		switch (t = ttype(o)) {
		case LUA_T_USERDATA:
			return o->value.ud.tag;
		case LUA_T_ARRAY:
			return o->value.a->htag;
		case LUA_T_PMARK:
			return LUA_T_PROTO;
		case LUA_T_CMARK:
			return LUA_T_CPROTO;
		case LUA_T_CLOSURE:
			// fall through
		case LUA_T_CLMARK:
			return o->value.cl->consts[0].ttype;
#ifdef LUA_DEBUG
		case LUA_T_LINE:
			LUA_INTERNALERROR("internal error");
#endif
		default:
			return t;
		}
	}
}

void lua_settag(int32 tag) {
	checkCparams(1);
	luaT_realtag(tag);
	switch (ttype(lua_state->stack.top - 1)) {
	case LUA_T_ARRAY:
		(lua_state->stack.top - 1)->value.a->htag = tag;
		break;
	case LUA_T_USERDATA:
		(lua_state->stack.top - 1)->value.ud.tag = tag;
		break;
	default:
		luaL_verror("cannot change the tag of a %.20s", luaO_typenames[-ttype((lua_state->stack.top - 1))]);
	}
	lua_state->stack.top--;
}

/*
** =======================================================
** Debug interface
** =======================================================
*/

// Hooks
lua_CHFunction lua_callhook = nullptr;
lua_LHFunction lua_linehook = nullptr;
lua_SaveRestoreUserHFunction lua_savehook = nullptr;
lua_SaveRestoreUserHFunction lua_restorehook = nullptr;

lua_Function lua_stackedfunction(int32 level) {
	StkId i;
	for (i = (lua_state->stack.top - 1) - lua_state->stack.stack; i >= 0; i--) {
		int32 t = lua_state->stack.stack[i].ttype;
		if (t == LUA_T_CLMARK || t == LUA_T_PMARK || t == LUA_T_CMARK)
			if (level-- == 0)
				return Ref(lua_state->stack.stack + i);
	}
	return LUA_NOOBJECT;
}

void lua_printstack(Common::WriteStream *f) {
	int32 level = 1;
	lua_Object func;
	Common::String text;

	while ((func = lua_stackedfunction(level++)) != LUA_NOOBJECT) {
		const char *name;
		int32 currentline;
		const char *filename;
		int32 linedefined;

		lua_funcinfo(func, &filename, &linedefined);
		text += (level == 2) ? "Active Stack:\n\t" : "\t";

		switch (*lua_getobjname(func, &name)) {
		case 'g':
			text += Common::String::format("function %s", name);
			break;
		case 't':
			text += Common::String::format("`%s' tag method", name);
			break;
		default:
			if (linedefined == 0)
				text += Common::String::format("main of %s", filename);
			else if (linedefined < 0)
				text += Common::String::format("%s", filename);
			else
				text += Common::String::format("function (%s:%d)", filename, (int)linedefined);

			filename = nullptr;
		}

		if ((currentline = lua_currentline(func)) > 0)
			text += Common::String::format(" at line %d", (int)currentline);

		if (filename)
			text += Common::String::format(" [in file %s]", filename);

		text += "\n";
	}

	if (f)
		f->writeString(text);
	else if (!text.empty())
		warning("%s", text.c_str());
}

int32 lua_currentline(lua_Function func) {
	TObject *f = Address(func);
	return (f + 1 < lua_state->stack.top && (f + 1)->ttype == LUA_T_LINE) ? (f + 1)->value.i : -1;
}

lua_Object lua_getlocal(lua_Function func, int32 local_number, char **name) {
	// check whether func is a Lua function
	if (lua_tag(func) != LUA_T_PROTO)
		return LUA_NOOBJECT;
	else {
		TObject *f = Address(func);
		TProtoFunc *fp = luaA_protovalue(f)->value.tf;
		*name = luaF_getlocalname(fp, local_number, lua_currentline(func));
		if (*name) {
			// if "*name", there must be a LUA_T_LINE
			// therefore, f + 2 points to function base
			return Ref((f + 2) + (local_number - 1));
		} else
			return LUA_NOOBJECT;
	}
}

int32 lua_setlocal(lua_Function func, int32 local_number) {
	// check whether func is a Lua function
	if (lua_tag(func) != LUA_T_PROTO)
		return 0;
	else {
		TObject *f = Address(func);
		TProtoFunc *fp = luaA_protovalue(f)->value.tf;
		char *name = luaF_getlocalname(fp, local_number, lua_currentline(func));
		checkCparams(1);
		--lua_state->stack.top;
		if (name) {
			// if "name", there must be a LUA_T_LINE
			// therefore, f+2 points to function base
			*((f + 2) + (local_number - 1)) = *lua_state->stack.top;
			return 1;
		} else
			return 0;
	}
}

void lua_funcinfo(lua_Object func, const char **filename, int32 *linedefined) {
	if (!lua_isfunction(func))
		lua_error("API - `funcinfo' called with a non-function value");
	else {
		TObject *f = luaA_protovalue(Address(func));
		if (normalized_type(f) == LUA_T_PROTO) {
			*filename = tfvalue(f)->fileName->str;
			*linedefined = tfvalue(f)->lineDefined;
		} else {
			*filename = "(C)";
			*linedefined = -1;
		}
	}
}

static int32 checkfunc (TObject *o) {
	return luaO_equalObj(o, lua_state->stack.top);
}

const char *lua_getobjname (lua_Object o, const char **name) {
	// try to find a name for given function
	set_normalized(lua_state->stack.top, Address(o)); // to be accessed by "checkfunc"
	*name = luaT_travtagmethods(checkfunc);
	if (*name)
		return "tag-method";
	*name = luaS_travsymbol(checkfunc);
	if (*name)
		return "global";
	else
		return "";
}

/*
** =======================================================
** BLOCK mechanism
** =======================================================
*/

void lua_beginblock() {
	if (lua_state->numCblocks >= MAX_C_BLOCKS)
		lua_error("too many nested blocks");
	lua_state->Cblocks[lua_state->numCblocks] = lua_state->Cstack;
	lua_state->numCblocks++;
}

void lua_endblock() {
	--lua_state->numCblocks;
	lua_state->Cstack = lua_state->Cblocks[lua_state->numCblocks];
	luaD_adjusttop(lua_state->Cstack.base);
}

int32 lua_ref(int32 lock) {
	int32 ref;
	checkCparams(1);
	ref = luaC_ref(lua_state->stack.top - 1, lock);
	lua_state->stack.top--;
	return ref;
}

lua_Object lua_getref(int32 r) {
	TObject *o = luaC_getref(r);
	return (o ? put_luaObject(o) : LUA_NOOBJECT);
}

#ifdef LUA_COMPAT2_5
/*
** API: set a function as a fallback
*/

static void do_unprotectedrun(lua_CFunction f, int32 params, int32 results) {
	StkId base = (lua_state->stack.top - lua_state->stack.stack) - params;
	luaD_openstack(params);
	lua_state->stack.stack[base].ttype = LUA_T_CPROTO;
	lua_state->stack.stack[base].value.f = f;
	lua_state->preventBreakCounter++;
	luaD_call(base + 1, results);
	lua_state->preventBreakCounter--;
}

lua_Object lua_setfallback(const char *name, lua_CFunction fallback) {
	lua_pushstring(name);
	lua_pushcfunction(fallback);
	do_unprotectedrun(luaT_setfallback, 2, 1);
	return put_luaObjectonTop();
}

#endif

enum {
	kListInternal,
	kListState,
	kListIo,
	kListIoTag,
	kListString,
	kListMath,
	kListMathPow,
	kListMain,
	kListExtra,
	kListFallback,
	kListCostume,
	kListText,
	kListPrimitives,
	kListHardware,
	kNumLists
};

static const char *const luaFuncIdNamesInternal[] = {
	"setfallback", "assert", "call", "collectgarbage", "dofile", "copytagmethods", "dostring", "error", "foreach", "foreachvar",
	"getglobal", "newtag", "next", "nextvar", "print", "rawgetglobal", "rawgettable", "rawsetglobal", "rawsettable", "seterrormethod",
	"setglobal", "settagmethod", "gettagmethod", "settag", "tonumber", "tostring", "tag", "type", "start_script", "stop_script",
	"next_script", "identify_script", "pause_scripts", "unpause_scripts", "find_script", "break_here"
};

static const char *const luaFuncIdNamesState[] = {
	"stderrorim"
};

static const char *const luaFuncIdNamesIo[] = {
	"setlocale", "execute", "remove", "rename", "tmpname", "getenv", "date", "exit", "debug", "print_stack"
};

static const char *const luaFuncIdNamesIoTag[] = {
	"readfrom", "writeto", "appendto", "read", "write"
};

static const char *const luaFuncIdNamesString[] = {
	"strlen", "strsub", "strlower", "strupper", "strrep", "ascii", "format", "strfind", "gsub"
};

static const char *const luaFuncIdNamesMath[] = {
	"abs", "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "ceil", "floor",
	"mod", "sqrt", "min", "max", "log", "log10", "exp", "deg", "rad", "random",
	"randomseed"
};

static const char *const luaFuncIdNamesMathPow[] = {
	"math_pow"
};

static const char *const luaFuncIdNamesMain[] = {
	"EnginePause", "EngineResume", "EngineDisplay", "SearchForFileOrSwapCDs", "CheckForFile", "Load", "Save", "SetActorColormap", "GetActorCostume", "SetActorCostume",
	"GetActorScale", "SetActorScale", "GetActorTimeScale", "SetActorTimeScale", "GetActorNodeLocation", "SetActorWalkChore", "SetActorTurnChores", "SetActorRestChore", "SetActorMumblechore", "SetActorTalkChore",
	"SetActorWalkRate", "GetActorWalkRate", "SetActorTurnRate", "GetActorTurnRate", "SetSelectedActor", "LoadActor", "GetActorPos", "GetActorRect", "GetActorPuckVector", "GetActorYawToPoint",
	"SetActorReflection", "SetActorOffsetYaw", "PutActorAtInterest", "PutActorAtOrigin", "PutActorAt", "PutActorInSet", "GetClippedPos", "WalkActorVector", "WalkActorForward", "DriveActorTo",
	"WalkActorTo", "WalkActorToAvoiding", "WalkActorToInterest", "ActorLookAt", "IsActorLooking", "SetActorLookRate", "GetActorLookRate", "CanActorSee", "GetVisibleThings", "GetCameraActor",
	"SetActorHead", "SetActorVisibility", "SetActorFollowBoxes", "ShutUpActor", "SetActorFrustrumCull", "IsActorInSector", "GetActorSector", "IsPointInSector", "GetPointSector", "TurnActor",
	"GetActorRot", "SetActorRot", "SetActorPitch", "SetActorRoll", "IsActorTurning", "PlayActorChore", "PlayActorChoreLooping", "StopActorChore", "CompleteActorChore", "IsActorMoving",
	"IsActorChoring", "IsActorResting", "SetActorChoreLooping", "GetActorChores", "GetActorLoopingChores", "GetActorCostumeDepth", "WorldToScreen", "ScreenToWorld", "exit", "FunctionName",
	"EnableDebugKeys", "LockFont", "UnLockFont", "LockSound", "UnlockSound", "LockCostume", "UnlockCostume", "EnableControl", "DisableControl", "GetControlState",
	"PrintMessage", "PrintError", "PrintWarning", "PrintDebug", "MakeCurrentSet", "LockSet", "UnLockSet", "MakeCurrentSetup", "GetCurrentSetup", "NextSetup",
	"PreviousSetup", "StartFullscreenMovie", "IsFullscreenMoviePlaying", "StartMovie", "StopMovie", "PauseMovie", "IsMoviePlaying", "PlaySound", "PlaySoundAt", "PlaySoundAttached",
	"IsSoundPlaying", "SetSoundPosition", "FileFindFirst", "FileFindNext", "FileFindDispose", "InputDialog", "QueryDialog", "WriteRegistryValue", "ReadRegistryValue", "GetSectorVertices",
	"GetSectorOppositeEdge", "IsSectorActive", "MakeSectorActive", "PreRender", "SpewStartup", "SpewShutdown", "GetCurrentScript", "PrintActorCostumes", "PushActorCostume", "PopActorCostume",
	"LoadCostume", "GetPhysicalBytes", "SetMemoryUsage", "RotateVector", "GetCameraPosition", "SetCameraPosition", "GetCameraInterest", "SetCameraInterest", "GetCameraFOV", "SetCameraFOV",
	"GetCameraRoll", "SetCameraRoll", "GetCameraLookVector", "PointActorAt", "TurnActorTo", "PerSecond", "SnapToPlane", "GetAngleBetweenVectors", "GetAngleBetweenActors", "SetAmbientLight",
	"FindLight", "TurnLightOn", "IsLightOn", "SetLightType", "GetLightType", "SetLightPosition", "GetLightPosition", "SetLightRotation", "GetLightRotation", "SetLightAngles",
	"GetLightAngles", "SetLightIntensity", "GetLightIntensity", "PointLightAt", "LightMgrStartup", "LightMgrShutdown", "LightMgrSetChange", "ImStartRecording", "ImStopRecording", "ImStartSound",
	"play", "ImStopSound", "ImStopAllSounds", "quiet", "ImGetParam", "ImSetParam", "ImFadeParam", "ImGetMasterVol", "ImSetMasterVol", "ImGetSfxVol",
	"ImSetSfxVol", "ImGetVoiceVol", "ImSetVoiceVol", "ImGetMusicVol", "ImSetMusicVol", "ImStartVoice", "ImStopVoice", "ImSetState", "ImSetSequence", "ImSetAttribute",
	"ImSetCuePoint", "ImPause", "ImResume", "vfx", "ImSetVoiceEffect", "ImGetMemoryFootprint", "ImGetSoundCacheSize", "LoadBundle", "UnloadBundle", "SetGamma",
	"SetActorWalkDominate", "SetActorConstrain", "RenderModeUser", "ForceRefresh", "DimScreen", "DimRegion", "CleanBuffer", "Display", "SetSpeechMode", "GetSpeechMode",
	"SetShadowColor", "ActivateActorShadow", "ActorShadow", "SetActorShadowPlane", "SetActorShadowPoint", "SetActiveShadow", "ActorDistToShadowPlane", "KillActorShadows", "AddShadowPlane", "SetActorShadowValid",
	"NewObjectState", "FreeObjectState", "SetObjectType", "SendObjectToBack", "SendObjectToFront", "ActorToClean", "FlushControls", "SetActorCollisionMode", "SetActorCollisionScale", "MarkDirtyRect",
	"CacheRect", "RestoreCacheRect", "ResReport", "HardwareAccelerated", "SetActorClipActive", "SetActorClipPlane", "FadeOutChore", "FadeInChore", "IrisDown", "IrisUp",
	"IrisComplete", "IrisClear", "TextFileGetLineCount", "TextFileGetLine", "ScreenShot", "SaveScreen", "GetSaveGameImage", "GetImage", "FreeImage", "BlastImage",
	"BlastRect", "SubmitSaveGameData", "GetSaveGameData", "SetTextSpeed", "GetTextSpeed", "FindFileOnAnyCD", "DetachFromResources", "AttachToResources", "SetSoundParameters", "GetSoundParameters",
	"TEST", "ActorPuckOrient", "GetTickCount", "ActorVoiceIs3D", "JustLoaded", "DumpMemoryLog", "ResetTextures", "BuildCache", "RestoreCache", "ShrinkBoxes",
	"UnShrinkBoxes", "GetShrinkPos", "NukeResources", "SetActorInvClipNode", "GetDiskFreeSpace", "SetWalkSystemFadeTime", "SaveIMuse", "RestoreIMuse", "GetMemoryUsage"
};

static const char *const luaFuncIdNamesExtra[] = {
	"  concatfallback", "  typeoverride", "  dfltcamera", "  dfltcontrol"
};

static const char *const luaFuncIdNamesFallback[] = {
	"  typeFB", "  errorFB", "  nilFB"
};

static const char *const luaFuncIdNamesCostume[] = {
	"GetKeyLength", "GetSmushLength"
};

static const char *const luaFuncIdNamesText[] = {
	"IsMessageGoing", "SetSayLineDefaults", "SetActorTalkColor", "GetActorTalkColor", "SayLine", "PrintLine", "MakeTextObject", "GetTextObjectDimensions", "GetFontDimensions", "ChangeTextObject",
	"KillTextObject", "BlastText", "ExpireText", "PurgeText", "MakeColor", "GetColorComponents", "SetTranslationMode", "GetTranslationMode", "GetTextCharPosition", "LocalizeString",
	"SetEmergencyFont", "SetOffscreenTextPos"
};

static const char *const luaFuncIdNamesPrimitives[] = {
	"DrawLine", "DrawPolygon", "DrawRectangle", "ChangePrimitive", "KillPrimitive", "PurgePrimitiveQueue"
};

static const char *const luaFuncIdNamesHardware[] = {
	"Is3DHardwareEnabled", "GetVideoDevices", "SetVideoDevices", "SetHardwareState", "Enumerate3DDevices", "EnumerateVideoDevices"
};

struct LuaFuncIdList {
	const char *title;
	const char *const *names;
	int count;
	int expectedCount;
	bool isEngine;
};

static const LuaFuncIdList luaFuncIdLists[kNumLists] = {
	{ "int_funcs", luaFuncIdNamesInternal, ARRAYSIZE(luaFuncIdNamesInternal), 36, false },
	{ "stateFuncs", luaFuncIdNamesState, ARRAYSIZE(luaFuncIdNamesState), 1, false },
	{ "iolib", luaFuncIdNamesIo, ARRAYSIZE(luaFuncIdNamesIo), 10, false },
	{ "iolibtag", luaFuncIdNamesIoTag, ARRAYSIZE(luaFuncIdNamesIoTag), 5, false },
	{ "strlib", luaFuncIdNamesString, ARRAYSIZE(luaFuncIdNamesString), 9, false },
	{ "mathlib", luaFuncIdNamesMath, ARRAYSIZE(luaFuncIdNamesMath), 21, false },
	{ "wrappermathlib", luaFuncIdNamesMathPow, ARRAYSIZE(luaFuncIdNamesMathPow), 1, false },
	{ "main", luaFuncIdNamesMain, ARRAYSIZE(luaFuncIdNamesMain), 279, true },
	{ "extra", luaFuncIdNamesExtra, ARRAYSIZE(luaFuncIdNamesExtra), 4, true },
	{ "tmFuncs", luaFuncIdNamesFallback, ARRAYSIZE(luaFuncIdNamesFallback), 3, false },
	{ "costume", luaFuncIdNamesCostume, ARRAYSIZE(luaFuncIdNamesCostume), 2, true },
	{ "text", luaFuncIdNamesText, ARRAYSIZE(luaFuncIdNamesText), 22, true },
	{ "primitives", luaFuncIdNamesPrimitives, ARRAYSIZE(luaFuncIdNamesPrimitives), 6, true },
	{ "hardware", luaFuncIdNamesHardware, ARRAYSIZE(luaFuncIdNamesHardware), 6, true }
};

enum {
	kListEmiPs2,
	kListEmiInternal,
	kListEmiState,
	kListEmiIo,
	kListEmiIoTag,
	kListEmiString,
	kListEmiMath,
	kListEmiMathPow,
	kListEmiMain,
	kListEmiExtra,
	kListEmiFallback,
	kListEmiPrimitives,
	kNumListsEmi
};

static const char *const luaFuncIdNamesInternalEmi[] = {
	"setfallback", "assert", "call", "collectgarbage", "dofile", "copytagmethods", "dostring", "error", "foreach", "foreachvar",
	"getglobal", "newtag", "next", "nextvar", "print", "rawgetglobal", "rawgettable", "rawsetglobal", "rawsettable", "seterrormethod",
	"setglobal", "settagmethod", "gettagmethod", "settag", "tonumber", "tostring", "tag", "type", "start_script", "stop_script",
	"next_script", "identify_script", "pause_scripts", "unpause_scripts", "pause_script", "unpause_script", "find_script", "break_here", "sleep_for"
};

static const char *const luaFuncIdNamesIoEmi[] = {
	"setlocale", "execute", "remove", "rename", "tmpname", "getenv", "date", "exit", "print_stack"
};

static const char *const luaFuncIdNamesMainEmi[] = {
	"EnginePause", "EngineResume", "EngineDisplay", "SpewStartup", "SpewShutdown", "GetCurrentScript", "GetPhysicalBytes", "GetMemoryUsage", "SetMemoryUsage", "GetDiskFreeSpace",
	"SetActiveCD", "GetActiveCD", "AreWeInternational", "FileFindFirst", "FileFindNext", "FileFindDispose", "TextFileGetLineCount", "TextFileGetLine", "InputDialog", "QueryDialog",
	"WriteRegistryValue", "ReadRegistryValue", "Load", "Save", "ScreenShot", "MakeScreenTextures", "SubmitSaveGameData", "GetSaveGameData", "JustLoaded", "ScreenshotForSavegame",
	"ThumbnailFromScreen", "ThumbnailFromFile", "ClearSpecialtyTexture", "LoadActor", "UnloadActor", "GetActorTimeScale", "SetActorTimeScale", "SetActorWalkDominate", "SetSelectedActor", "SetActorWalkChore",
	"SetActorTurnChores", "SetActorRestChore", "SetActorMumblechore", "SetActorTalkChore", "SetActorWalkRate", "GetActorWalkRate", "SetActorTurnRate", "GetActorTurnRate", "GetActorPos", "GetActorWorldPos",
	"GetActorPuckVector", "GetActorYawToPoint", "SetActorReflection", "PutActorInSet", "PutActorAt", "PutActorInOverworld", "RemoveActorFromOverworld", "ClearOverworld", "ToggleOverworld", "WalkActorVector",
	"WalkActorForward", "ActorStopMoving", "WalkActorTo", "WalkActorToAvoiding", "ActorLookAt", "IsActorLooking", "SetActorLookRate", "GetActorLookRate", "SetActorFOV", "GetActorFOV",
	"GetVisibleThings", "SetActorLighting", "SetActorHead", "SetActorHeadLimits", "SetActorVisibility", "SetActorFollowBoxes", "ActorActivateShadow", "ShutUpActor", "EnableActorPuck", "SetActorGlobalAlpha",
	"SetActorLocalAlpha", "SetActorCollisionMode", "SetActorCollisionScale", "SetActorSortOrder", "GetActorSortOrder", "AttachActor", "DetachActor", "IsActorInSector", "GetActorSector", "IsPointInSector",
	"GetPointSector", "TurnActor", "GetActorRot", "SetActorRot", "SetActorPitch", "SetActorRoll", "IsActorTurning", "PointActorAt", "TurnActorTo", "PlayActorChore",
	"PlayActorChoreLooping", "IsActorMoving", "IsActorResting", "GetActorChores", "StopActorChores", "IsChoreValid", "IsChorePlaying", "IsChoreLooping", "SetChoreLooping", "PlayChore",
	"FadeInChore", "FadeOutChore", "StopChore", "PauseChore", "AdvanceChore", "CompleteChore", "LockChore", "UnlockChore", "LockChoreSet", "UnlockChoreSet",
	"exit", "FunctionName", "EnableControl", "DisableControl", "GetControlState", "PrintMessage", "PrintError", "PrintWarning", "PrintDebug", "MakeCurrentSet",
	"LockSet", "UnLockSet", "LockBackground", "UnLockBackground", "MakeCurrentSetup", "GetCurrentSetup", "NextSetup", "PreviousSetup", "StartMovie", "StopMovie",
	"PauseMovie", "EscapeMovie", "IsMoviePlaying", "PlaySound", "StopSound", "StopAllSounds", "LoadSound", "FreeSound", "PlayLoadedSound", "IsSoundPlaying",
	"GetGroupVolume", "SetGroupVolume", "GetSoundVolume", "SetSoundVolume", "EnableAudioGroup", "EnableVoiceFX", "SetAudioMixAhead", "PlaySoundFrom", "PlayLoadedSoundFrom", "SetReverb",
	"UpdateSoundPosition", "ImSetState", "ImSelectSet", "ImStateHasLooped", "ImStateHasEnded", "ImPushState", "ImPopState", "ImFlushStack", "ImGetMillisecondPosition", "IsSectorActive",
	"MakeSectorActive", "GetSectorOppositeEdge", "GetSectorName", "WorldToScreen", "GetCameraPosition", "SetCameraPosition", "CameraLookAt", "GetCameraFOV", "SetCameraFOV", "GetCameraYaw",
	"YawCamera", "GetCameraPitch", "PitchCamera", "GetCameraRoll", "RollCamera", "GetCameraLookVector", "PerSecond", "SnapToPlane", "GetAngleBetweenVectors", "GetAngleBetweenActors",
	"SetAmbientLight", "TurnLightOn", "IsLightOn", "SetLightPosition", "GammaEnabled", "SetGamma", "SetPixelCenter", "LoadBundle", "RenderModeUser", "DimScreen",
	"DimRegion", "UndimAll", "UndimRegion", "Display", "SetSpeechMode", "GetSpeechMode", "GetCPUSpeed", "NewLayer", "FreeLayer", "GetLayerSortOrder",
	"SetLayerSortOrder", "GetLayerFrame", "SetLayerFrame", "AdvanceLayerFrame", "SetTextSpeed", "GetTextSpeed", "IsMessageGoing", "SetSayLineDefaults", "SetActorTalkColor", "GetActorTalkColor",
	"SayLine", "MakeTextObject", "KillTextObject", "ChangeTextObject", "GetTextObjectDimensions", "BlastText", "ExpireText", "PurgeText", "PushText", "PopText",
	"MakeColor", "GetColorComponents", "SetTranslationMode", "GetTranslationMode", "LocalizeString", "GetFontDimensions", "GetTextCharPosition", "SetOffscreenTextPos", "ReportAllResources", "ReportLRUResources",
	"ReportUsedResources", "ReportUnusedResources", "PurgeAmount", "PurgeAllResources", "PurgeUnusedResources", "PurgeUnlockedResources", "NukeAllScriptLocks", "LightMgrStartup", "LightMgrShutdown", "LightMgrSetChange",
	"ToggleDebugDraw", "ToggleDrawCameras", "ToggleDrawLights", "ToggleDrawSectors", "ToggleDrawBBoxes", "ToggleDrawFPS", "ToggleDrawPerformance", "ToggleDrawActorStats", "WriteSetToFile", "SectEditSelect",
	"SectEditPlace", "SectEditModify", "SectEditDelete", "SectEditInsert", "SectEditSortAdd", "SectEditForgetIt", "FRUTEY_Begin", "FRUTEY_End", "ImGetSfxVol", "ImSetSfxVol",
	"ImGetVoiceVol", "ImSetVoiceVol", "ImGetMusicVol", "ImSetMusicVol"
};

static const char *const luaFuncIdNamesExtraEmi[] = {
	"  concatfallback", "  typeoverride", "  dfltcontrol"
};

static const char *const luaFuncIdNamesPrimitivesEmi[] = {
	"Draw3DLine", "DrawLine", "DrawPolygon", "DrawRectangle", "ChangePrimitive", "KillPrimitive", "PurgePrimitiveQueue"
};

static const char *const luaFuncIdNamesPs2Emi[] = {
	"SetReplayMode", "GetReplayMode", "GetNextDemoNum", "NotifyGameState", "DeleteSavedGame", "CopySavedGame", "JustSaved", "GetMemoryCardId", "GetUniqueUint", "FormatMemoryCard",
	"SetActorTessellation", "IsActorChoring", "ApplyForceFeedback", "EnableSoakText", "SetStereoAudio", "pspu", "dsnd", "KillMusicAndWait", "OverWorldToScreen", "SetVScanOffsets",
	"MemDump", "AllocDump", "LuaMemDump", "GLReport"
};

static const LuaFuncIdList luaFuncIdListsEmi[kNumListsEmi] = {
	{ "ps2", luaFuncIdNamesPs2Emi, ARRAYSIZE(luaFuncIdNamesPs2Emi), 24, true },
	{ "int_funcs", luaFuncIdNamesInternalEmi, ARRAYSIZE(luaFuncIdNamesInternalEmi), 39, false },
	{ "stateFuncs", luaFuncIdNamesState, ARRAYSIZE(luaFuncIdNamesState), 1, false },
	{ "iolib", luaFuncIdNamesIoEmi, ARRAYSIZE(luaFuncIdNamesIoEmi), 9, false },
	{ "iolibtag", luaFuncIdNamesIoTag, ARRAYSIZE(luaFuncIdNamesIoTag), 5, false },
	{ "strlib", luaFuncIdNamesString, ARRAYSIZE(luaFuncIdNamesString), 9, false },
	{ "mathlib", luaFuncIdNamesMath, ARRAYSIZE(luaFuncIdNamesMath), 21, false },
	{ "wrappermathlib", luaFuncIdNamesMathPow, ARRAYSIZE(luaFuncIdNamesMathPow), 1, false },
	{ "main", luaFuncIdNamesMainEmi, ARRAYSIZE(luaFuncIdNamesMainEmi), 274, true },
	{ "extra", luaFuncIdNamesExtraEmi, ARRAYSIZE(luaFuncIdNamesExtraEmi), 3, true },
	{ "tmFuncs", luaFuncIdNamesFallback, ARRAYSIZE(luaFuncIdNamesFallback), 3, false },
	{ "primitives", luaFuncIdNamesPrimitivesEmi, ARRAYSIZE(luaFuncIdNamesPrimitivesEmi), 7, true }
};

// The demo has no sleep_for
static const char *const luaFuncIdNamesInternalEmiDemo[] = {
	"setfallback", "assert", "call", "collectgarbage", "dofile", "copytagmethods", "dostring", "error", "foreach", "foreachvar",
	"getglobal", "newtag", "next", "nextvar", "print", "rawgetglobal", "rawgettable", "rawsetglobal", "rawsettable", "seterrormethod",
	"setglobal", "settagmethod", "gettagmethod", "settag", "tonumber", "tostring", "tag", "type", "start_script", "stop_script",
	"next_script", "identify_script", "pause_scripts", "unpause_scripts", "pause_script", "unpause_script", "find_script", "break_here"
};

// The demo opcode table without its first three entries, in the order of the demo
static const char *const luaFuncIdNamesMainEmiDemo[] = {
	"EnginePause", "EngineResume", "EngineDisplay", "SpewStartup", "SpewShutdown", "GetCurrentScript", "GetPhysicalBytes", "GetMemoryUsage", "SetMemoryUsage", "GetDiskFreeSpace",
	"FindFileOrSwapCDs", "CheckForFile", "FindFileOnAnyCD", "FileFindFirst", "FileFindNext", "FileFindDispose", "TextFileGetLineCount", "TextFileGetLine", "InputDialog", "QueryDialog",
	"WriteRegistryValue", "ReadRegistryValue", "Load", "Save", "ScreenShot", "MakeScreenTextures", "SubmitSaveGameData", "GetSaveGameData", "JustLoaded", "ThumbnailFromScreen",
	"ThumbnailFromFile", "ClearSpecialtyTexture", "LoadActor", "UnloadActor", "GetActorTimeScale", "SetActorTimeScale", "SetActorWalkDominate", "SetSelectedActor", "SetActorWalkChore", "SetActorTurnChores",
	"SetActorRestChore", "SetActorMumblechore", "SetActorTalkChore", "SetActorWalkRate", "GetActorWalkRate", "SetActorTurnRate", "GetActorTurnRate", "GetActorPos", "GetActorWorldPos", "GetActorPuckVector",
	"GetActorYawToPoint", "SetActorReflection", "PutActorInSet", "PutActorAt", "PutActorInOverworld", "RemoveActorFromOverworld", "ClearOverworld", "ToggleOverworld", "WalkActorVector", "WalkActorForward",
	"ActorStopMoving", "WalkActorTo", "WalkActorToAvoiding", "ActorLookAt", "IsActorLooking", "SetActorLookRate", "GetActorLookRate", "SetActorFOV", "GetActorFOV", "GetVisibleThings",
	"SetActorLighting", "SetActorHead", "SetActorHeadLimits", "SetActorVisibility", "SetActorFollowBoxes", "ActorActivateShadow", "ShutUpActor", "EnableActorPuck", "SetActorGlobalAlpha", "SetActorLocalAlpha",
	"SetActorCollisionMode", "SetActorCollisionScale", "SetActorSortOrder", "GetActorSortOrder", "AttachActor", "DetachActor", "IsActorInSector", "GetActorSector", "IsPointInSector", "GetPointSector",
	"TurnActor", "GetActorRot", "SetActorRot", "SetActorPitch", "SetActorRoll", "IsActorTurning", "PointActorAt", "TurnActorTo", "PlayActorChore", "PlayActorChoreLooping",
	"IsActorMoving", "IsActorResting", "GetActorChores", "StopActorChores", "IsChoreValid", "IsChorePlaying", "IsChoreLooping", "SetChoreLooping", "PlayChore", "FadeInChore",
	"FadeOutChore", "StopChore", "PauseChore", "AdvanceChore", "CompleteChore", "LockChore", "UnlockChore", "LockChoreSet", "UnlockChoreSet", "exit",
	"FunctionName", "EnableControl", "DisableControl", "GetControlState", "PrintMessage", "PrintError", "PrintWarning", "PrintDebug", "MakeCurrentSet", "LockSet",
	"UnLockSet", "LockBackground", "UnLockBackground", "MakeCurrentSetup", "GetCurrentSetup", "NextSetup", "PreviousSetup", "StartMovie", "StopMovie", "PauseMovie",
	"IsMoviePlaying", "PlaySound", "StopAllSounds", "LoadSound", "FreeSound", "PlayLoadedSound", "IsSoundPlaying", "GetGroupVolume", "SetGroupVolume", "GetSoundVolume",
	"SetSoundVolume", "ImSetState", "ImPause", "ImResume", "ImSetVoiceEffect", "IsSectorActive", "MakeSectorActive", "GetSectorOppositeEdge", "WorldToScreen", "GetCameraPosition",
	"SetCameraPosition", "CameraLookAt", "GetCameraFOV", "SetCameraFOV", "GetCameraYaw", "YawCamera", "GetCameraPitch", "PitchCamera", "GetCameraRoll", "RollCamera",
	"GetCameraLookVector", "PerSecond", "SnapToPlane", "GetAngleBetweenVectors", "GetAngleBetweenActors", "SetAmbientLight", "TurnLightOn", "IsLightOn", "SetLightPosition", "GammaEnabled",
	"SetGamma", "SetPixelCenter", "RenderModeUser", "DimScreen", "DimRegion", "UndimAll", "UndimRegion", "Display", "SetSpeechMode", "GetSpeechMode",
	"NewLayer", "FreeLayer", "GetLayerSortOrder", "SetLayerSortOrder", "GetLayerFrame", "SetLayerFrame", "AdvanceLayerFrame", "SetTextSpeed", "GetTextSpeed", "IsMessageGoing",
	"SetSayLineDefaults", "SetActorTalkColor", "GetActorTalkColor", "SayLine", "MakeTextObject", "KillTextObject", "ChangeTextObject", "GetTextObjectDimensions", "BlastText", "ExpireText",
	"PurgeText", "MakeColor", "GetColorComponents", "SetTranslationMode", "GetTranslationMode", "LocalizeString", "GetFontDimensions", "GetTextCharPosition", "SetOffscreenTextPos", "ReportAllResources",
	"ReportLRUResources", "ReportUsedResources", "ReportUnusedResources", "PurgeAmount", "PurgeAllResources", "PurgeUnusedResources", "PurgeUnlockedResources", "LightMgrStartup", "LightMgrShutdown", "LightMgrSetChange",
	"ToggleDebugDraw", "ToggleDrawCameras", "ToggleDrawLights", "ToggleDrawSectors", "ToggleDrawBBoxes", "ToggleDrawFPS", "ToggleDrawPerformance", "GetSectorName", "WriteSetToFile", "SectEditSelect",
	"SectEditPlace", "SectEditModify", "SectEditDelete", "SectEditInsert", "SectEditSortAdd", "SectEditForgetIt", "FRUTEY_Begin", "FRUTEY_End", "ImGetSfxVol", "ImSetSfxVol",
	"ImGetVoiceVol", "ImSetVoiceVol", "ImGetMusicVol", "ImSetMusicVol"
};

static const LuaFuncIdList luaFuncIdListsEmiDemo[kNumListsEmi] = {
	{ "ps2", luaFuncIdNamesPs2Emi, ARRAYSIZE(luaFuncIdNamesPs2Emi), 24, true },
	{ "int_funcs", luaFuncIdNamesInternalEmiDemo, ARRAYSIZE(luaFuncIdNamesInternalEmiDemo), 38, false },
	{ "stateFuncs", luaFuncIdNamesState, ARRAYSIZE(luaFuncIdNamesState), 1, false },
	{ "iolib", luaFuncIdNamesIoEmi, ARRAYSIZE(luaFuncIdNamesIoEmi), 9, false },
	{ "iolibtag", luaFuncIdNamesIoTag, ARRAYSIZE(luaFuncIdNamesIoTag), 5, false },
	{ "strlib", luaFuncIdNamesString, ARRAYSIZE(luaFuncIdNamesString), 9, false },
	{ "mathlib", luaFuncIdNamesMath, ARRAYSIZE(luaFuncIdNamesMath), 21, false },
	{ "wrappermathlib", luaFuncIdNamesMathPow, ARRAYSIZE(luaFuncIdNamesMathPow), 1, false },
	{ "main", luaFuncIdNamesMainEmiDemo, ARRAYSIZE(luaFuncIdNamesMainEmiDemo), 254, true },
	{ "extra", luaFuncIdNamesExtraEmi, ARRAYSIZE(luaFuncIdNamesExtraEmi), 3, true },
	{ "tmFuncs", luaFuncIdNamesFallback, ARRAYSIZE(luaFuncIdNamesFallback), 3, false },
	{ "primitives", luaFuncIdNamesPrimitivesEmi, ARRAYSIZE(luaFuncIdNamesPrimitivesEmi), 7, true }
};

enum {
	kGroupNone,
	kGroupErrorIm,
	kGroupDoNothing,
	kGroupPrint,
	kGroupMovie,
	kGroupPlay,
	kGroupQuiet,
	kGroupVoiceEffect,
	kGroupEmpty,
	kNumGroups
};

struct LuaFuncIdAlias {
	int list;
	const char *name;
	int group;
};

static const LuaFuncIdAlias luaFuncIdAliases[] = {
	{ kListState, "stderrorim", kGroupErrorIm },
	{ kListFallback, "  nilFB", kGroupErrorIm },

	{ kListMain, "PutActorAtOrigin", kGroupDoNothing },
	{ kListMain, "GetClippedPos", kGroupDoNothing },
	{ kListMain, "DriveActorTo", kGroupDoNothing },
	{ kListMain, "WalkActorToInterest", kGroupDoNothing },
	{ kListMain, "UnLockFont", kGroupDoNothing },
	{ kListMain, "LockSound", kGroupDoNothing },
	{ kListMain, "UnlockSound", kGroupDoNothing },
	{ kListMain, "LockCostume", kGroupDoNothing },
	{ kListMain, "UnlockCostume", kGroupDoNothing },
	{ kListMain, "PlaySound", kGroupDoNothing },
	{ kListMain, "PlaySoundAt", kGroupDoNothing },
	{ kListMain, "PlaySoundAttached", kGroupDoNothing },
	{ kListMain, "IsSoundPlaying", kGroupDoNothing },
	{ kListMain, "GetSectorVertices", kGroupDoNothing },
	{ kListMain, "LoadBundle", kGroupDoNothing },
	{ kListMain, "UnloadBundle", kGroupDoNothing },
	{ kListExtra, "  dfltcamera", kGroupDoNothing },
	{ kListExtra, "  dfltcontrol", kGroupDoNothing },

	{ kListMain, "PrintError", kGroupPrint },
	{ kListMain, "PrintWarning", kGroupPrint },
	{ kListMain, "PrintDebug", kGroupPrint },

	{ kListMain, "IsFullscreenMoviePlaying", kGroupMovie },
	{ kListMain, "IsMoviePlaying", kGroupMovie },

	{ kListMain, "ImStartSound", kGroupPlay },
	{ kListMain, "play", kGroupPlay },

	{ kListMain, "ImStopAllSounds", kGroupQuiet },
	{ kListMain, "quiet", kGroupQuiet },

	{ kListMain, "ImSetVoiceEffect", kGroupVoiceEffect },
	{ kListMain, "vfx", kGroupVoiceEffect }
};

static const LuaFuncIdAlias luaFuncIdAliasesEmi[] = {
	{ kListEmiMain, "EngineDisplay", kGroupEmpty },
	{ kListEmiMain, "SpewStartup", kGroupEmpty },
	{ kListEmiMain, "SpewShutdown", kGroupEmpty },
	{ kListEmiMain, "SetAmbientLight", kGroupEmpty },
	{ kListEmiMain, "Display", kGroupEmpty },
	{ kListEmiIo, "rename", kGroupEmpty },
	{ kListEmiExtra, "  dfltcontrol", kGroupEmpty },
	{ kListEmiFallback, "  nilFB", kGroupEmpty }
};

// The demo has the empty function in InputDialog too, and its default control handler is a
// function of its own
static const LuaFuncIdAlias luaFuncIdAliasesEmiDemo[] = {
	{ kListEmiMain, "EngineDisplay", kGroupEmpty },
	{ kListEmiMain, "SpewStartup", kGroupEmpty },
	{ kListEmiMain, "SpewShutdown", kGroupEmpty },
	{ kListEmiMain, "InputDialog", kGroupEmpty },
	{ kListEmiMain, "SetAmbientLight", kGroupEmpty },
	{ kListEmiMain, "Display", kGroupEmpty },
	{ kListEmiIo, "rename", kGroupEmpty },
	{ kListEmiFallback, "  nilFB", kGroupEmpty }
};

enum {
	kListRemInternal,
	kListRemState,
	kListRemIo,
	kListRemIoTag,
	kListRemString,
	kListRemMath,
	kListRemMathPow,
	kListRemMain,
	kListRemExtra,
	kListRemFallback,
	kListRemCostume,
	kListRemCommentary,
	kListRemText,
	kListRemPrimitives,
	kListRemOverlay,
	kListRemMouse,
	kNumListsRemastered
};

static const char *const luaFuncIdNamesMainRemastered[] = {
	"EnginePause", "EngineResume", "EngineDisplay", "PrintStack", "GetPlatform", "SearchForFileOrSwapCDs", "CheckForFile", "New", "Load", "Save",
	"Erase", "GetSaveStatus", "StartCheckOfCrossSaveStatus", "GetCrossSaveStatus", "UnlockAchievement", "ResetAchievements", "AreAchievementsInstalled", "GlobalSaveResolved", "UnlockCutscene", "IsCutsceneUnlocked",
	"UnlockConcept", "IsConceptUnlocked", "SetActorColormap", "GetActorCostume", "SetActorCostume", "GetActorScale", "SetActorScale", "GetActorTimeScale", "SetActorTimeScale", "GetActorNodeLocation",
	"SetActorWalkChore", "SetActorTurnChores", "SetActorRestChore", "SetActorMumblechore", "SetActorTalkChore", "SetActorWalkRate", "GetActorWalkRate", "SetActorTurnRate", "GetActorTurnRate", "SetSelectedActor",
	"LoadActor", "GetActorPos", "GetActorRect", "GetActorPuckVector", "GetActorYawToPoint", "SetActorReflection", "SetActorOffsetYaw", "PutActorAtInterest", "PutActorAtOrigin", "PutActorAt",
	"PutActorInSet", "GetClippedPos", "WalkActorVector", "WalkActorForward", "DriveActorTo", "WalkActorTo", "WalkActorToAvoiding", "WalkActorToInterest", "WalkActorToMouseTarget", "ActorLookAt",
	"IsActorLooking", "SetActorLookRate", "GetActorLookRate", "CanActorSee", "GetVisibleThings", "GetCameraActor", "SetActorHead", "SetActorVisibility", "SetActorFollowBoxes", "ShutUpActor",
	"SetActorFrustrumCull", "SetActorBWMode", "SetActorHKHackMode", "PathfinderMode", "IsActorInSector", "GetActorSector", "IsPointInSector", "GetPointSector", "TurnActor", "GetActorRot",
	"SetActorRot", "SetActorPitch", "SetActorRoll", "IsActorTurning", "PlayActorChore", "PlayActorChoreLooping", "StopActorChore", "CompleteActorChore", "IsActorMoving", "IsActorChoring",
	"IsActorResting", "SetActorChoreLooping", "GetActorChores", "GetActorLoopingChores", "GetActorCostumeDepth", "WorldToScreen", "ScreenToWorld", "exit", "FunctionName", "EnableDebugKeys",
	"LockFont", "UnLockFont", "LockSound", "UnlockSound", "LockCostume", "UnlockCostume", "EnableControl", "DisableControl", "GetControlState", "PrintMessage",
	"PrintError", "PrintWarning", "PrintDebug", "MakeCurrentSet", "LockSet", "UnLockSet", "MakeCurrentSetup", "GetCurrentSetup", "NextSetup", "PreviousSetup",
	"StartFullscreenMovie", "IsFullscreenMoviePlaying", "StartMovie", "StopMovie", "PauseMovie", "IsMoviePlaying", "PlaySound", "PlaySoundAt", "PlaySoundAttached", "IsSoundPlaying",
	"SetSoundPosition", "FindSaveGames", "InitiateFindSaveGames", "GetFindSaveGameStatus", "FileFindFirst", "FileFindNext", "FileFindDispose", "InputDialog", "QueryDialog", "WriteRegistryValue",
	"ReadRegistryValue", "ReadRegistryIntValue", "SaveRegistryToDisk", "GetSectorVertices", "GetSectorOppositeEdge", "IsSectorActive", "MakeSectorActive", "PreRender", "SpewStartup", "SpewShutdown",
	"GetCurrentScript", "PrintActorCostumes", "PushActorCostume", "PopActorCostume", "LoadCostume", "GetPhysicalBytes", "SetMemoryUsage", "RotateVector", "GetCameraPosition", "SetCameraPosition",
	"GetCameraInterest", "SetCameraInterest", "GetCameraFOV", "SetCameraFOV", "GetCameraRoll", "SetCameraRoll", "GetCameraLookVector", "EnableBackgroundRender", "PointActorAt", "TurnActorTo",
	"PerSecond", "SnapToPlane", "GetAngleBetweenVectors", "GetAngleBetweenActors", "SetAmbientLight", "FindLight", "TurnLightOn", "IsLightOn", "SetLightType", "GetLightType",
	"SetLightPosition", "GetLightPosition", "SetLightRotation", "GetLightRotation", "SetLightAngles", "GetLightAngles", "SetLightIntensity", "GetLightIntensity", "PointLightAt", "LightMgrStartup",
	"LightMgrShutdown", "LightMgrSetChange", "ImStartRecording", "ImStopRecording", "ImStartSound", "play", "ImStopSound", "ImStopAllSounds", "quiet", "ImGetParam",
	"ImSetParam", "ImFadeParam", "ImGetMasterVol", "ImSetMasterVol", "ImGetSfxVol", "ImSetSfxVol", "ImGetVoiceVol", "ImSetVoiceVol", "ImGetMusicVol", "ImSetMusicVol",
	"ImGetCommentaryVol", "ImSetCommentaryVol", "ImStartVoice", "ImStopVoice", "ImSetState", "ImSetSequence", "ImSetAttribute", "ImSetCuePoint", "ImPause", "ImResume",
	"vfx", "ImSetVoiceEffect", "ImGetMemoryFootprint", "ImGetSoundCacheSize", "LoadBundle", "UnloadBundle", "SetGamma", "SetResolutionScaling", "SetAdvancedLighting", "SetActorWalkDominate",
	"SetActorConstrain", "RenderModeUser", "SetGameRenderMode", "GetGameRenderMode", "ForceRefresh", "DimScreen", "DimRegion", "CleanBuffer", "Display", "SetSpeechMode",
	"GetSpeechMode", "SetShadowColor", "ActivateActorShadow", "ActorShadow", "SetActorShadowPlane", "SetActorShadowPoint", "SetActiveShadow", "ActorDistToShadowPlane", "KillActorShadows", "AddShadowPlane",
	"SetActorShadowValid", "NewObjectState", "FreeObjectState", "SetObjectType", "SendObjectToBack", "SendObjectToFront", "ActorToClean", "FlushControls", "SetActorCollisionMode", "SetActorCollisionScale",
	"MarkDirtyRect", "CacheRect", "RestoreCacheRect", "ResReport", "HardwareAccelerated", "SetActorClipActive", "SetActorClipPlane", "FadeOutChore", "FadeInChore", "IrisDown",
	"IrisUp", "IrisComplete", "IrisClear", "DialogLogGetLineCount", "DialogLogGetLine", "DialogLogAddLine", "ScreenShot", "SaveScreen", "GetSaveGameImage", "GetImage",
	"FreeImage", "BlastImage", "BlastRect", "SubmitSaveGameData", "GetSaveGameData", "SetTextSpeed", "GetTextSpeed", "FindFileOnAnyCD", "DetachFromResources", "AttachToResources",
	"SetSoundParameters", "GetSoundParameters", "TEST", "ActorPuckOrient", "GetSystemTime", "GetTickCount", "ActorVoiceIs3D", "JustLoaded", "DumpMemoryLog", "ResetTextures",
	"BuildCache", "RestoreCache", "ShrinkBoxes", "UnShrinkBoxes", "GetShrinkPos", "NukeResources", "SetActorInvClipNode", "GetDiskFreeSpace", "SetWalkSystemFadeTime", "SaveIMuse",
	"RestoreIMuse", "GetMemoryUsage", "SetLanguage", "GetLanguage", "GetLanguageCount", "Is3DHardwareEnabled", "SetMouseSpeedScale", "SetKeyMappingMode", "ResetKeyMappingToDefault", "GetRemappedKeyName",
	"GetRemappedKeyHint", "LoadRemappedKeys", "SaveRemappedKeys"
};

static const char *const luaFuncIdNamesCommentaryRemastered[] = {
	"SetCommentary", "ClearCommentary", "PlayCurrentCommentary", "EnableCommentary", "StopCommentaryImmediately", "HasHeardCommentary", "IsPlayingCommentary"
};

static const char *const luaFuncIdNamesOverlayRemastered[] = {
	"OverlayCreate", "OverlayMove", "OverlaySetLayer", "OverlayDestroy", "OverlayDimensions", "OverlayClearCache", "OverlayFade", "OverlayGetScreenSize", "OverlayScreenBoxingMode", "OverlayShowLoadSpinner",
	"WidescreenCorrectionFactor"
};

static const char *const luaFuncIdNamesMouseRemastered[] = {
	"QueryActiveHotspots", "GetFloorWalkPos", "GetSelectedDialog", "SetCursor", "ProjectToAxis", "ShowCursor", "AddHotspot", "RemoveHotspot", "GetCursorPosition", "CursorAnimation",
	"UpdateHotspot", "LinkHotspot", "PreloadCursors", "CursorMovieEnabled"
};

static const LuaFuncIdList luaFuncIdListsRemastered[kNumListsRemastered] = {
	{ "int_funcs", luaFuncIdNamesInternal, ARRAYSIZE(luaFuncIdNamesInternal), 36, false },
	{ "stateFuncs", luaFuncIdNamesState, ARRAYSIZE(luaFuncIdNamesState), 1, false },
	{ "iolib", luaFuncIdNamesIo, ARRAYSIZE(luaFuncIdNamesIo), 10, false },
	{ "iolibtag", luaFuncIdNamesIoTag, ARRAYSIZE(luaFuncIdNamesIoTag), 5, false },
	{ "strlib", luaFuncIdNamesString, ARRAYSIZE(luaFuncIdNamesString), 9, false },
	{ "mathlib", luaFuncIdNamesMath, ARRAYSIZE(luaFuncIdNamesMath), 21, false },
	{ "wrappermathlib", luaFuncIdNamesMathPow, ARRAYSIZE(luaFuncIdNamesMathPow), 1, false },
	{ "main", luaFuncIdNamesMainRemastered, ARRAYSIZE(luaFuncIdNamesMainRemastered), 323, true },
	{ "extra", luaFuncIdNamesExtra, ARRAYSIZE(luaFuncIdNamesExtra), 4, true },
	{ "tmFuncs", luaFuncIdNamesFallback, ARRAYSIZE(luaFuncIdNamesFallback), 3, false },
	{ "costume", luaFuncIdNamesCostume, ARRAYSIZE(luaFuncIdNamesCostume), 2, true },
	{ "commentary", luaFuncIdNamesCommentaryRemastered, ARRAYSIZE(luaFuncIdNamesCommentaryRemastered), 7, true },
	{ "text", luaFuncIdNamesText, ARRAYSIZE(luaFuncIdNamesText), 22, true },
	{ "primitives", luaFuncIdNamesPrimitives, ARRAYSIZE(luaFuncIdNamesPrimitives), 6, true },
	{ "overlay", luaFuncIdNamesOverlayRemastered, ARRAYSIZE(luaFuncIdNamesOverlayRemastered), 11, true },
	{ "mouse", luaFuncIdNamesMouseRemastered, ARRAYSIZE(luaFuncIdNamesMouseRemastered), 14, true }
};

// Only these pairs share a function
static const LuaFuncIdAlias luaFuncIdAliasesRemastered[] = {
	{ kListRemMain, "ImStartSound", kGroupPlay },
	{ kListRemMain, "play", kGroupPlay },

	{ kListRemMain, "ImStopAllSounds", kGroupQuiet },
	{ kListRemMain, "quiet", kGroupQuiet },

	{ kListRemMain, "ImSetVoiceEffect", kGroupVoiceEffect },
	{ kListRemMain, "vfx", kGroupVoiceEffect }
};

enum {
	kMaxLists = kNumListsRemastered
};

static_assert((int)kNumLists <= (int)kMaxLists && (int)kNumListsEmi <= (int)kMaxLists, "the id models have more lists than kMaxLists");

struct LuaFuncIdEntry {
	lua_CFunction func;
	int group;
};

static bool isEmiDemoIdLists = false;
static bool isRemasteredIdLists = false;

static const LuaFuncIdList *lists = luaFuncIdLists;
static int activeListCount = kNumLists;
static const LuaFuncIdAlias *aliases = luaFuncIdAliases;
static int numAliases = ARRAYSIZE(luaFuncIdAliases);

static Common::Array<LuaFuncIdEntry> funcIdEntries[kMaxLists];
static Common::Array<lua_CFunction> groupFuncs[kNumGroups];
static int claimedLists[kMaxLists];
static int numLiveLists = 0;
static bool isBuilt = false;
static luaL_libList *builtHead = nullptr;
static int builtNumLists = 0;
static int builtNumEntries = 0;

static Common::Array<uint32> reportedIds;
static Common::Array<lua_CFunction> reportedFuncs;

static void getLiveSignature(luaL_libList **head, int *numLists, int *numEntries) {
	*head = list_of_libs;
	*numLists = 0;
	*numEntries = 0;

	for (luaL_libList *list = list_of_libs; list; list = list->next) {
		(*numLists)++;
		*numEntries += list->number;
	}
}

static int findLiveEngineList(const Common::Array<luaL_libList *> &live, const Common::Array<bool> &claimed, const LuaFuncIdList &model) {
	for (uint i = 0; i < live.size(); i++) {
		if (claimed[i] || live[i]->number != model.count)
			continue;

		const luaL_reg *regs = live[i]->list;

		if (strcmp(regs[0].name, model.names[0]) == 0 && strcmp(regs[model.count - 1].name, model.names[model.count - 1]) == 0)
			return i;
	}

	return -1;
}

static lua_CFunction findInLiveList(const luaL_libList *list, const char *name, int index) {
	if (index < list->number && strcmp(list->list[index].name, name) == 0)
		return list->list[index].func;

	for (int32 i = 0; i < list->number; i++) {
		if (strcmp(list->list[i].name, name) == 0)
			return list->list[i].func;
	}

	return nullptr;
}

static lua_CFunction findLibraryFunction(const Common::Array<luaL_libList *> &live, const Common::Array<bool> &claimed, const char *name) {
	for (int i = (int)live.size() - 1; i >= 0; i--) {
		if (claimed[i])
			continue;

		for (int32 j = 0; j < live[i]->number; j++) {
			if (strcmp(live[i]->list[j].name, name) == 0)
				return live[i]->list[j].func;
		}
	}

	return nullptr;
}

static void checkLiveListsOfModel(const Common::Array<luaL_libList *> &live) {
	bool hasUnmatched = false;
	int numEngineLists = 0;
	int numMatched = 0;

	for (int i = 0; i < activeListCount; i++) {
		const LuaFuncIdList &model = lists[i];

		if (!model.isEngine)
			continue;

		numEngineLists++;

		if (claimedLists[i] < 0) {
			hasUnmatched = true;
			warning("buildFuncIdLists(): no live list matches the list %s of the model (%d names, first '%s', last '%s')",
				model.title, model.count, model.names[0], model.names[model.count - 1]);
			continue;
		}

		const luaL_libList *liveList = live[claimedLists[i]];
		int numMissing = 0;
		int numMoved = 0;
		const char *firstMissing = "-";
		const char *firstMoved = "-";

		for (int j = 0; j < model.count; j++) {
			if (strcmp(liveList->list[j].name, model.names[j]) == 0)
				continue;

			if (findInLiveList(liveList, model.names[j], j)) {
				if (!numMoved)
					firstMoved = model.names[j];

				numMoved++;
			} else {
				if (!numMissing)
					firstMissing = model.names[j];

				numMissing++;
			}
		}

		if (numMissing || numMoved) {
			warning("buildFuncIdLists(): the live list %s differs from the model: %d names are missing (first '%s'), %d names are in another position (first '%s')",
				model.title, numMissing, firstMissing, numMoved, firstMoved);
		} else {
			numMatched++;
		}
	}

	if (hasUnmatched) {
		for (uint i = 0; i < live.size(); i++) {
			if (live[i]->number > 0)
				warning("buildFuncIdLists(): live list %d has %d names, first '%s', last '%s'", (int)i, (int)live[i]->number,
					live[i]->list[0].name, live[i]->list[live[i]->number - 1].name);
		}
	}

	debug(1, "buildFuncIdLists(): %d of the %d engine lists of the model match a live list exactly", numMatched, numEngineLists);
}

static void buildFuncIdLists() {
	Common::Array<luaL_libList *> live;
	Common::Array<bool> claimed;

	for (luaL_libList *list = list_of_libs; list; list = list->next) {
		live.push_back(list);
		claimed.push_back(false);
	}

	numLiveLists = live.size();

	if (!luaIsEmi && isRemasteredIdLists) {
		lists = luaFuncIdListsRemastered;
		activeListCount = kNumListsRemastered;
		aliases = luaFuncIdAliasesRemastered;
		numAliases = ARRAYSIZE(luaFuncIdAliasesRemastered);
	} else if (luaIsEmi && isEmiDemoIdLists) {
		lists = luaFuncIdListsEmiDemo;
		activeListCount = kNumListsEmi;
		aliases = luaFuncIdAliasesEmiDemo;
		numAliases = ARRAYSIZE(luaFuncIdAliasesEmiDemo);
	} else if (luaIsEmi) {
		lists = luaFuncIdListsEmi;
		activeListCount = kNumListsEmi;
		aliases = luaFuncIdAliasesEmi;
		numAliases = ARRAYSIZE(luaFuncIdAliasesEmi);
	} else {
		lists = luaFuncIdLists;
		activeListCount = kNumLists;
		aliases = luaFuncIdAliases;
		numAliases = ARRAYSIZE(luaFuncIdAliases);
	}

	for (int i = 0; i < activeListCount; i++) {
		claimedLists[i] = -1;

		if (!lists[i].isEngine)
			continue;

		int liveIndex = findLiveEngineList(live, claimed, lists[i]);

		if (liveIndex >= 0) {
			claimedLists[i] = liveIndex;
			claimed[liveIndex] = true;
		}
	}

	if (!luaIsEmi && isRemasteredIdLists)
		checkLiveListsOfModel(live);

	for (int i = 0; i < activeListCount; i++) {
		const LuaFuncIdList &model = lists[i];

		funcIdEntries[i].clear();

		for (int j = 0; j < model.count; j++) {
			LuaFuncIdEntry entry;
			entry.func = nullptr;
			entry.group = kGroupNone;

			if (!model.isEngine)
				entry.func = findLibraryFunction(live, claimed, model.names[j]);
			else if (claimedLists[i] >= 0)
				entry.func = findInLiveList(live[claimedLists[i]], model.names[j], j);

			funcIdEntries[i].push_back(entry);
		}
	}

	for (int i = 0; i < kNumGroups; i++)
		groupFuncs[i].clear();

	for (int i = 0; i < numAliases; i++) {
		const LuaFuncIdAlias &alias = aliases[i];
		const LuaFuncIdList &model = lists[alias.list];
		bool isFound = false;

		for (int j = 0; j < model.count; j++) {
			if (strcmp(model.names[j], alias.name) == 0) {
				funcIdEntries[alias.list][j].group = alias.group;
				isFound = true;
				break;
			}
		}

		if (!isFound)
			warning("buildFuncIdLists(): the alias '%s' is not in the list %s", alias.name, model.title);
	}

	for (int i = 0; i < activeListCount; i++) {
		for (uint j = 0; j < funcIdEntries[i].size(); j++) {
			const LuaFuncIdEntry &entry = funcIdEntries[i][j];

			if (entry.group == kGroupNone || !entry.func)
				continue;

			bool isKnown = false;

			for (uint k = 0; k < groupFuncs[entry.group].size(); k++) {
				if (groupFuncs[entry.group][k] == entry.func)
					isKnown = true;
			}

			if (!isKnown)
				groupFuncs[entry.group].push_back(entry.func);
		}
	}

	getLiveSignature(&builtHead, &builtNumLists, &builtNumEntries);
	isBuilt = true;
}

static void ensureFuncIdLists() {
	if (isBuilt) {
		luaL_libList *head;
		int numLists;
		int numEntries;

		getLiveSignature(&head, &numLists, &numEntries);

		if (head == builtHead && numLists == builtNumLists && numEntries == builtNumEntries)
			return;
	}

	buildFuncIdLists();
}

static bool entryMatches(const LuaFuncIdEntry &entry, lua_CFunction func) {
	if (entry.func == func)
		return true;

	if (entry.group == kGroupNone)
		return false;

	for (uint i = 0; i < groupFuncs[entry.group].size(); i++) {
		if (groupFuncs[entry.group][i] == func)
			return true;
	}

	return false;
}

static lua_CFunction getEntryFunction(const LuaFuncIdEntry &entry) {
	if (entry.func)
		return entry.func;

	if (entry.group != kGroupNone && !groupFuncs[entry.group].empty())
		return groupFuncs[entry.group][0];

	return nullptr;
}

static bool findFunctionId(lua_CFunction func, uint32 *id) {
	ensureFuncIdLists();

	if (!func)
		return false;

	for (int depth = 0; depth < activeListCount; depth++) {
		const Common::Array<LuaFuncIdEntry> &entries = funcIdEntries[activeListCount - 1 - depth];

		for (uint index = 0; index < entries.size(); index++) {
			if (entryMatches(entries[index], func)) {
				*id = ((uint32)depth << 16) | index;
				return true;
			}
		}
	}

	return false;
}

static const LuaFuncIdEntry *findEntryOfId(uint32 id, int *listIndexPointer) {
	ensureFuncIdLists();

	uint32 depth = id >> 16;
	uint32 index = id & 0xffff;

	if (depth >= (uint32)activeListCount)
		return nullptr;

	int listIndex = activeListCount - 1 - (int)depth;

	if (index >= funcIdEntries[listIndex].size())
		return nullptr;

	*listIndexPointer = listIndex;

	return &funcIdEntries[listIndex][index];
}

static const char *findLiveFunctionName(lua_CFunction func) {
	for (luaL_libList *list = list_of_libs; list; list = list->next) {
		for (int32 i = 0; i < list->number; i++) {
			if (list->list[i].func == func)
				return list->list[i].name;
		}
	}

	return nullptr;
}

uint32 lua_FuncPtrToID(lua_CFunction func) {
	uint32 id;

	if (findFunctionId(func, &id))
		return id;

	for (uint i = 0; i < reportedFuncs.size(); i++) {
		if (reportedFuncs[i] == func)
			return 0;
	}

	reportedFuncs.push_back(func);

	const char *name = findLiveFunctionName(func);
	warning("lua_FuncPtrToID(): the function '%s' has no id, 0 is returned", name ? name : "?");

	return 0;
}

lua_CFunction lua_IDToFuncPtr(uint32 id) {
	int listIndex = 0;
	const LuaFuncIdEntry *entry = findEntryOfId(id, &listIndex);
	lua_CFunction func = entry ? getEntryFunction(*entry) : nullptr;

	if (func)
		return func;

	for (uint i = 0; i < reportedIds.size(); i++) {
		if (reportedIds[i] == id)
			return nullptr;
	}

	reportedIds.push_back(id);

	if (!entry)
		warning("lua_IDToFuncPtr(): no function with the id %08x", id);
	else
		warning("lua_IDToFuncPtr(): the function '%s' of the list %s (id %08x) is not in the running Lua",
			lists[listIndex].names[id & 0xffff], lists[listIndex].title, id);

	return nullptr;
}

bool lua_FuncPtrHasID(lua_CFunction func) {
	uint32 id;

	return findFunctionId(func, &id);
}

void lua_FuncIDsInvalidate() {
	isBuilt = false;
	reportedIds.clear();
	reportedFuncs.clear();
}

void lua_FuncIDsSetEmiDemo(bool isDemo) {
	isEmiDemoIdLists = isDemo;
	lua_FuncIDsInvalidate();
}

void lua_FuncIDsSetRemastered(bool isRemastered) {
	isRemasteredIdLists = isRemastered;
	lua_FuncIDsInvalidate();
}

} // end of namespace Grim

