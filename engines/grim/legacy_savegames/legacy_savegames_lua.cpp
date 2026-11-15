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

#include "common/array.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/legacy_savegames/legacy_savegames_lua.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

// The lists of C functions of the legacy engine, in the order of the ids stored in a savegame.
// An id is (depth << 16) | index, depth 0 being the list registered last.

static const char *const legacyLuaNamesTmFB[] = {
	"  typeFB", "  errorFB", "  nilFB"
};

// Entries 2 and 3 are the same function, the ids always use 2
static const char *const legacyLuaNamesBase[] = {
	"  concatfallback", "  typeoverride", "  dfltcamera", "  dfltcontrol"
};

static const char *const legacyLuaNamesHardware[] = {
	"Is3DHardwareEnabled", "GetVideoDevices", "SetVideoDevices", "SetHardwareState", "Enumerate3DDevices", "EnumerateVideoDevices"
};

static const char *const legacyLuaNamesPrimitives[] = {
	"DrawLine", "DrawPolygon", "DrawRectangle", "ChangePrimitive", "KillPrimitive", "PurgePrimitiveQueue"
};

static const char *const legacyLuaNamesText[] = {
	"IsMessageGoing", "SetSayLineDefaults", "SetActorTalkColor", "GetActorTalkColor", "SayLine", "PrintLine",
	"MakeTextObject", "GetTextObjectDimensions", "GetFontDimensions", "ChangeTextObject", "KillTextObject", "BlastText",
	"ExpireText", "PurgeText", "MakeColor", "GetColorComponents", "SetTranslationMode", "GetTranslationMode",
	"GetTextCharPosition", "LocalizeString", "SetEmergencyFont", "SetOffscreenTextPos"
};

static const char *const legacyLuaNamesMain[] = {
	"EngineDisplay", "CheckForFile", "Load", "Save", "remove", "SetActorColormap",
	"GetActorCostume", "SetActorCostume", "SetActorScale", "GetActorTimeScale", "SetActorTimeScale", "GetActorNodeLocation",
	"SetActorWalkChore", "SetActorTurnChores", "SetActorRestChore", "SetActorMumblechore", "SetActorTalkChore", "SetActorWalkRate",
	"GetActorWalkRate", "SetActorTurnRate", "SetSelectedActor", "LoadActor", "GetActorPos", "GetActorRect",
	"GetActorPuckVector", "GetActorYawToPoint", "SetActorReflection", "PutActorAtInterest", "PutActorAt", "PutActorInSet",
	"WalkActorVector", "WalkActorForward", "DriveActorTo", "WalkActorTo", "WalkActorToAvoiding", "ActorLookAt",
	"SetActorLookRate", "GetActorLookRate", "GetVisibleThings", "GetCameraActor", "SetActorHead", "SetActorVisibility",
	"SetActorFollowBoxes", "ShutUpActor", "SetActorFrustrumCull", "IsActorInSector", "GetActorSector", "IsPointInSector",
	"GetPointSector", "TurnActor", "GetActorRot", "SetActorRot", "SetActorPitch", "SetActorRoll",
	"IsActorTurning", "PlayActorChore", "PlayActorChoreLooping", "StopActorChore", "CompleteActorChore", "IsActorMoving",
	"IsActorChoring", "IsActorResting", "SetActorChoreLooping", "GetActorChores", "GetActorCostumeDepth", "WorldToScreen",
	"exit", "FunctionName", "EnableDebugKeys", "LockFont", "EnableControl", "DisableControl",
	"GetControlState", "PrintError", "PrintWarning", "PrintDebug", "MakeCurrentSet", "LockSet",
	"UnLockSet", "MakeCurrentSetup", "GetCurrentSetup", "NextSetup", "PreviousSetup", "StartFullscreenMovie",
	"IsFullscreenMoviePlaying", "StartMovie", "StopMovie", "PauseMovie", "IsMoviePlaying", "PlaySound",
	"PlaySoundAt", "IsSoundPlaying", "SetSoundPosition", "FileFindFirst", "FileFindNext", "FileFindDispose",
	"InputDialog", "WriteRegistryValue", "ReadRegistryValue", "GetSectorOppositeEdge", "MakeSectorActive", "PreRender",
	"SpewStartup", "GetCurrentScript", "PrintActorCostumes", "PushActorCostume", "PopActorCostume", "LoadCostume",
	"RotateVector", "GetCameraPosition", "SetCameraPosition", "SetCameraInterest", "GetCameraFOV", "SetCameraFOV",
	"GetCameraRoll", "SetCameraRoll", "GetCameraLookVector", "PointActorAt", "TurnActorTo", "PerSecond",
	"GetAngleBetweenVectors", "GetAngleBetweenActors", "SetAmbientLight", "TurnLightOn", "SetLightPosition", "SetLightIntensity",
	"LightMgrSetChange", "LightMgrStartup", "ImStartSound", "ImStopSound", "ImStopAllSounds", "ImGetParam",
	"ImSetParam", "ImFadeParam", "ImGetSfxVol", "ImSetSfxVol", "ImGetVoiceVol", "ImSetVoiceVol",
	"ImGetMusicVol", "ImSetMusicVol", "ImSetState", "ImSetSequence", "ImPause", "ImResume",
	"ImSetVoiceEffect", "LoadBundle", "SetGamma", "SetActorWalkDominate", "SetActorConstrain", "RenderModeUser",
	"ForceRefresh", "DimScreen", "DimRegion", "CleanBuffer", "Display", "SetSpeechMode",
	"GetSpeechMode", "SetShadowColor", "ActivateActorShadow", "SetActorShadowPlane", "SetActorShadowPoint", "SetActiveShadow",
	"KillActorShadows", "AddShadowPlane", "SetActorShadowValid", "FreeObjectState", "NewObjectState", "SetObjectType",
	"SendObjectToBack", "SendObjectToFront", "ActorToClean", "FlushControls", "SetActorCollisionMode", "SetActorCollisionScale",
	"SetActorClipActive", "SetActorClipPlane", "FadeOutChore", "FadeInChore", "IrisDown", "IrisUp",
	"TextFileGetLineCount", "TextFileGetLine", "ScreenShot", "GetSaveGameImage", "GetImage", "FreeImage",
	"BlastImage", "BlastRect", "SubmitSaveGameData", "GetSaveGameData", "SetTextSpeed", "GetTextSpeed",
	"DetachFromResources", "AttachToResources", "ActorPuckOrient", "JustLoaded", "ResetTextures", "ShrinkBoxes",
	"UnShrinkBoxes", "GetShrinkPos", "NukeResources", "SetActorInvClipNode", "GetDiskFreeSpace", "SaveIMuse",
	"RestoreIMuse", "GetMemoryUsage", "dofile"
};

static const char *const legacyLuaNamesPow[] = {
	"math_pow"
};

static const char *const legacyLuaNamesMath[] = {
	"abs", "sin", "cos", "tan", "asin", "acos", "atan", "ceil", "floor",
	"mod", "sqrt", "min", "max", "deg", "rad", "random", "randomseed"
};

static const char *const legacyLuaNamesStr[] = {
	"strlen", "strsub", "strlower", "strupper", "strrep", "format", "strfind", "gsub"
};

static const char *const legacyLuaNamesIoTag[] = {
	"readfrom", "writeto", "appendto", "read", "write"
};

static const char *const legacyLuaNamesIo[] = {
	"date", "exit", "print_stack"
};

static const char *const legacyLuaNamesStdError[] = {
	"stderrorim"
};

static const char *const legacyLuaNamesInternal[] = {
	"setfallback", "assert", "call", "collectgarbage", "copytagmethods", "dostring", "error", "foreach", "foreachvar", "getglobal",
	"newtag", "next", "nextvar", "print", "rawgetglobal", "rawgettable", "rawsetglobal", "rawsettable", "seterrormethod", "setglobal",
	"settagmethod", "gettagmethod", "settag", "tonumber", "tostring", "tag", "type", "start_script", "stop_script", "next_script",
	"identify_script", "pause_scripts", "unpause_scripts", "find_script", "sleep_for", "break_here", "pause_script", "unpause_script"
};

struct LegacyLuaList {
	const char *const *names;
	int count;
	// The library lists were registered before the engine ones
	bool isLibrary;
};

// Index = depth
static const LegacyLuaList legacyLuaLists[kNumLegacyLuaLists] = {
	{ legacyLuaNamesTmFB, ARRAYSIZE(legacyLuaNamesTmFB), true },
	{ legacyLuaNamesBase, ARRAYSIZE(legacyLuaNamesBase), false },
	{ legacyLuaNamesHardware, ARRAYSIZE(legacyLuaNamesHardware), false },
	{ legacyLuaNamesPrimitives, ARRAYSIZE(legacyLuaNamesPrimitives), false },
	{ legacyLuaNamesText, ARRAYSIZE(legacyLuaNamesText), false },
	{ legacyLuaNamesMain, ARRAYSIZE(legacyLuaNamesMain), false },
	{ legacyLuaNamesPow, ARRAYSIZE(legacyLuaNamesPow), true },
	{ legacyLuaNamesMath, ARRAYSIZE(legacyLuaNamesMath), true },
	{ legacyLuaNamesStr, ARRAYSIZE(legacyLuaNamesStr), true },
	{ legacyLuaNamesIoTag, ARRAYSIZE(legacyLuaNamesIoTag), true },
	{ legacyLuaNamesIo, ARRAYSIZE(legacyLuaNamesIo), true },
	{ legacyLuaNamesStdError, ARRAYSIZE(legacyLuaNamesStdError), true },
	{ legacyLuaNamesInternal, ARRAYSIZE(legacyLuaNamesInternal), true }
};

static const char *const legacyLuaTextRefNames[kNumLegacyLuaTextRefs] = {
	"x", "y", "font", "width", "height", "fgcolor", "bgcolor", "fxcolor",
	"hicolor", "duration", "center", "ljustify", "rjustify", "volume", "pan", "background"
};

// Numbers of the refs made by LuaBase::registerLua() of the legacy engine
static const int kLegacyLuaRefSystemTable = 0;
static const int kLegacyLuaRefTypeOverride = 1;
static const int kLegacyLuaRefOldConcatFallback = 2;
static const int kLegacyLuaRefFirstTextObject = 3;

// A name that cannot be mapped is reported once
static Common::Array<int32> reportedFunctions;
static LegacyLuaMapper *legacyMapper = nullptr;
static int numUnmappedUserdata = 0;
static int32 firstUnmappedTag = 0;
static int32 firstUnmappedId = 0;

const char *legacyLuaFunctionName(int depth, int index) {
	if (depth < 0 || depth >= kNumLegacyLuaLists)
		return nullptr;

	if (index < 0 || index >= legacyLuaLists[depth].count)
		return nullptr;

	return legacyLuaLists[depth].names[index];
}

// The newest entry wins, or the oldest one for the library functions
static lua_CFunction findLiveFunction(const char *name, bool oldestFirst) {
	lua_CFunction found = nullptr;

	for (luaL_libList *list = list_of_libs; list; list = list->next) {
		for (int32 i = 0; i < list->number; i++) {
			if (strcmp(list->list[i].name, name) != 0)
				continue;

			if (!oldestFirst)
				return list->list[i].func;

			found = list->list[i].func;
		}
	}

	return found;
}

static bool isFunctionReported(int32 depth, int32 index) {
	int32 key = (depth << 16) | index;

	for (uint i = 0; i < reportedFunctions.size(); i++) {
		if (reportedFunctions[i] == key)
			return true;
	}

	reportedFunctions.push_back(key);

	return false;
}

lua_CFunction legacyLuaResolveFunction(int32 depth, int32 index) {
	const char *name = legacyLuaFunctionName(depth, index);

	if (!name) {
		if (!isFunctionReported(depth, index))
			warning("legacyLuaResolveFunction(): the legacy lists have no function %d/%d", depth, index);

		return nullptr;
	}

	lua_CFunction func = findLiveFunction(name, legacyLuaLists[depth].isLibrary);

	if (!func && !isFunctionReported(depth, index))
		warning("legacyLuaResolveFunction(): the function %d/%d '%s' is not in the running Lua", depth, index, name);

	return func;
}

static bool mapLegacyUserdata(int32 oldTag, int32 oldId, int32 *newTag, int32 *newId) {
	if (legacyMapper->mapUserdata(oldTag, oldId, newTag, newId))
		return true;

	if (numUnmappedUserdata == 0) {
		firstUnmappedTag = oldTag;
		firstUnmappedId = oldId;
	}

	numUnmappedUserdata++;

	return false;
}

bool legacyRestoreLua(Common::SeekableReadStream *stream, int minorVersion, LegacyLuaMapper *mapper) {
	reportedFunctions.clear();
	legacyMapper = mapper;
	numUnmappedUserdata = 0;

	int result = lua_RestoreLegacy(stream, minorVersion, legacyLuaResolveFunction, mapper ? mapLegacyUserdata : nullptr);

	legacyMapper = nullptr;

	if (result != 0) {
		warning("legacyRestoreLua(): the Lua state cannot be read");
		return false;
	}

	if (stream->pos() != stream->size()) {
		warning("legacyRestoreLua(): %d bytes of the Lua section are left", (int)(stream->size() - stream->pos()));
		return false;
	}

	if (numUnmappedUserdata > 0)
		warning("legacyRestoreLua(): %d userdata not mapped, the first has tag %08x and id %d", numUnmappedUserdata, (uint)firstUnmappedTag, firstUnmappedId);

	if (minorVersion >= 7 && minorVersion <= 28)
		lua_PatchGrimSave();

	lua_PatchGrimNaSave();

	return true;
}

void legacyLuaGetRefs(LegacyLuaRefs *refs) {
	refs->systemTable = kLegacyLuaRefSystemTable;
	refs->typeOverride = kLegacyLuaRefTypeOverride;
	refs->oldConcatFallback = kLegacyLuaRefOldConcatFallback;

	for (int i = 0; i < kNumLegacyLuaTextRefs; i++)
		refs->textObject[i] = kLegacyLuaRefFirstTextObject + i;
}

static bool checkLegacyLuaRef(const char *what, int legacyRef, int engineRef) {
	if (legacyRef == engineRef)
		return true;

	warning("legacyLuaCheckRefs(): %s is ref %d in the savegame and %d in the engine", what, legacyRef, engineRef);

	return false;
}

bool legacyLuaCheckRefs(const LegacyLuaRefs &engineRefs) {
	LegacyLuaRefs legacyRefs;
	legacyLuaGetRefs(&legacyRefs);

	bool isOk = true;

	lua_beginblock();

	if (!checkLegacyLuaRef("the system table", legacyRefs.systemTable, engineRefs.systemTable))
		isOk = false;

	if (!checkLegacyLuaRef("the type override", legacyRefs.typeOverride, engineRefs.typeOverride))
		isOk = false;

	if (!checkLegacyLuaRef("the old concat fallback", legacyRefs.oldConcatFallback, engineRefs.oldConcatFallback))
		isOk = false;

	if (!lua_istable(lua_getref(legacyRefs.systemTable))) {
		warning("legacyLuaCheckRefs(): ref %d is not the system table", legacyRefs.systemTable);
		isOk = false;
	}

	if (!lua_iscfunction(lua_getref(legacyRefs.typeOverride))) {
		warning("legacyLuaCheckRefs(): ref %d is not the original type function", legacyRefs.typeOverride);
		isOk = false;
	}

	if (!lua_iscfunction(lua_getref(legacyRefs.oldConcatFallback))) {
		warning("legacyLuaCheckRefs(): ref %d is not the old concat fallback", legacyRefs.oldConcatFallback);
		isOk = false;
	}

	for (int i = 0; i < kNumLegacyLuaTextRefs; i++) {
		if (!checkLegacyLuaRef(legacyLuaTextRefNames[i], legacyRefs.textObject[i], engineRefs.textObject[i]))
			isOk = false;

		lua_Object obj = lua_getref(legacyRefs.textObject[i]);
		if (lua_tag(obj) != LUA_T_STRING || strcmp(lua_getstring(obj), legacyLuaTextRefNames[i]) != 0) {
			warning("legacyLuaCheckRefs(): ref %d is not the string '%s'", legacyRefs.textObject[i], legacyLuaTextRefNames[i]);
			isOk = false;
		}
	}

	lua_endblock();

	return isOk;
}

static bool isLiveEntry(const char *name, lua_CFunction func) {
	for (luaL_libList *list = list_of_libs; list; list = list->next) {
		for (int32 i = 0; i < list->number; i++) {
			if (list->list[i].func == func && strcmp(list->list[i].name, name) == 0)
				return true;
		}
	}

	return false;
}

static const char *findLiveName(lua_CFunction func) {
	for (luaL_libList *list = list_of_libs; list; list = list->next) {
		for (int32 i = 0; i < list->number; i++) {
			if (list->list[i].func == func)
				return list->list[i].name;
		}
	}

	return nullptr;
}

Common::String legacyLuaSelfCheck() {
	const int kMaxListed = 20;

	int numOk = 0;
	int numRenamed = 0;
	int numUnlisted = 0;
	Common::String renamed;
	Common::String unlisted;

	for (GCnode *node = rootglobal.next; node; node = node->next) {
		TaggedString *global = (TaggedString *)node;

		if (global->globalval.ttype != LUA_T_CPROTO)
			continue;

		lua_CFunction func = global->globalval.value.f;

		if (isLiveEntry(global->str, func)) {
			numOk++;
			continue;
		}

		const char *liveName = findLiveName(func);

		if (liveName) {
			if (numRenamed < kMaxListed)
				renamed += Common::String::format(" %s=%s", global->str, liveName);
			numRenamed++;
		} else {
			if (numUnlisted < kMaxListed)
				unlisted += Common::String::format(" %s", global->str);
			numUnlisted++;
		}
	}

	return Common::String::format("Lua C functions: %d ok, %d under another name:%s, %d not in the lists:%s",
		numOk, numRenamed, renamed.c_str(), numUnlisted, unlisted.c_str());
}

} // End of namespace Grim
