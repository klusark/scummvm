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

#include "common/endian.h"
#include "common/memstream.h"
#include "common/path.h"
#include "common/str.h"
#include "common/textconsole.h"
#include "common/translation.h"
#include "common/ustr.h"
#include "common/util.h"

#include "gui/message.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/emi_registry.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/files/fileutil.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/grim.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/luadebug.h"
#include "engines/grim/patchr.h"

namespace Grim {

static const char *const fieldNames[] = {
	"x", "y", "font", "width", "height", "hicolor", "fgcolor", "bgcolor",
	"fxcolor", "duration", "layer", "center", "ljustify", "rjustify", "volume", "pan"
};

bool Lua_V2::isSoundLocked(int soundHandle) {
	// The demo keeps no list of script-locked sounds
	if (_engine->isDemo())
		return false;

	for (uint i = 0; i < _lockedSounds.size(); i++) {
		if ((int)_lockedSounds[i] == soundHandle)
			return true;
	}

	return false;
}

static const struct luaL_reg monkeyMainOpcodes[] = {
	{ "EnginePause", LUA_OPCODE(Lua_V2, EnginePause) },
	{ "EngineResume", LUA_OPCODE(Lua_V2, EngineResume) },
	{ "EngineDisplay", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "SpewStartup", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "SpewShutdown", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "GetCurrentScript", LUA_OPCODE(Lua_V1, GetCurrentScript) },
	{ "GetPhysicalBytes", LUA_OPCODE(Lua_V2, GetPhysicalBytes) },
	{ "GetMemoryUsage", LUA_OPCODE(Lua_V2, GetMemoryUsage) },
	{ "SetMemoryUsage", LUA_OPCODE(Lua_V2, SetMemoryUsage) },
	{ "GetDiskFreeSpace", LUA_OPCODE(Lua_V1, GetDiskFreeSpace) },
	{ "SetActiveCD", LUA_OPCODE(Lua_V2, SetActiveCD) },
	{ "GetActiveCD", LUA_OPCODE(Lua_V2, GetActiveCD) },
	{ "AreWeInternational", LUA_OPCODE(Lua_V2, AreWeInternational) },
	{ "FileFindFirst", LUA_OPCODE(Lua_V2, FileFindFirst) },
	{ "FileFindNext", LUA_OPCODE(Lua_V2, FileFindNext) },
	{ "FileFindDispose", LUA_OPCODE(Lua_V2, FileFindDispose) },
	{ "TextFileGetLineCount", LUA_OPCODE(Lua_V2, TextFileGetLineCount) },
	{ "TextFileGetLine", LUA_OPCODE(Lua_V2, TextFileGetLine) },
	{ "InputDialog", LUA_OPCODE(Lua_V2, InputDialog) },
	{ "QueryDialog", LUA_OPCODE(Lua_V2, QueryDialog) },
	{ "WriteRegistryValue", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ReadRegistryValue", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "Load", LUA_OPCODE(Lua_V1, Load) },
	{ "Save", LUA_OPCODE(Lua_V1, Save) },
	{ "ScreenShot", LUA_OPCODE(Lua_V2, ScreenShot) },
	{ "MakeScreenTextures", LUA_OPCODE(Lua_V2, MakeScreenTextures) },
	{ "SubmitSaveGameData", LUA_OPCODE(Lua_V2, SubmitSaveGameData) },
	{ "GetSaveGameData", LUA_OPCODE(Lua_V2, GetSaveGameData) },
	{ "JustLoaded", LUA_OPCODE(Lua_V2, JustLoaded) },
	{ "ScreenshotForSavegame", LUA_OPCODE(Lua_V2, ScreenshotForSavegame) },
	{ "ThumbnailFromScreen", LUA_OPCODE(Lua_V2, ThumbnailFromScreen) },
	{ "ThumbnailFromFile", LUA_OPCODE(Lua_V2, ThumbnailFromFile) },
	{ "ClearSpecialtyTexture", LUA_OPCODE(Lua_V2, ClearSpecialtyTexture) },
	{ "LoadActor", LUA_OPCODE(Lua_V2, LoadActor) },
	{ "UnloadActor", LUA_OPCODE(Lua_V2, UnloadActor) },
	{ "GetActorTimeScale", LUA_OPCODE(Lua_V2, GetActorTimeScale) },
	{ "SetActorTimeScale", LUA_OPCODE(Lua_V2, SetActorTimeScale) },
	{ "SetActorWalkDominate", LUA_OPCODE(Lua_V2, SetActorWalkDominate) },
	{ "SetSelectedActor", LUA_OPCODE(Lua_V2, SetSelectedActor) },
	{ "SetActorWalkChore", LUA_OPCODE(Lua_V2, SetActorWalkChore) },
	{ "SetActorTurnChores", LUA_OPCODE(Lua_V2, SetActorTurnChores) },
	{ "SetActorRestChore", LUA_OPCODE(Lua_V2, SetActorRestChore) },
	{ "SetActorMumblechore", LUA_OPCODE(Lua_V2, SetActorMumblechore) },
	{ "SetActorTalkChore", LUA_OPCODE(Lua_V2, SetActorTalkChore) },
	{ "SetActorWalkRate", LUA_OPCODE(Lua_V2, SetActorWalkRate) },
	{ "GetActorWalkRate", LUA_OPCODE(Lua_V2, GetActorWalkRate) },
	{ "SetActorTurnRate", LUA_OPCODE(Lua_V2, SetActorTurnRate) },
	{ "GetActorTurnRate", LUA_OPCODE(Lua_V2, GetActorTurnRate) },
	{ "GetActorPos", LUA_OPCODE(Lua_V2, GetActorPos) },
	{ "GetActorWorldPos", LUA_OPCODE(Lua_V2, GetActorWorldPos) },
	{ "GetActorPuckVector", LUA_OPCODE(Lua_V2, GetActorPuckVector) },
	{ "GetActorYawToPoint", LUA_OPCODE(Lua_V2, GetActorYawToPoint) },
	{ "SetActorReflection", LUA_OPCODE(Lua_V2, SetActorReflection) },
	{ "PutActorInSet", LUA_OPCODE(Lua_V2, PutActorInSet) },
	{ "PutActorAt", LUA_OPCODE(Lua_V2, PutActorAt) },
	{ "PutActorInOverworld", LUA_OPCODE(Lua_V2, PutActorInOverworld) },
	{ "RemoveActorFromOverworld", LUA_OPCODE(Lua_V2, RemoveActorFromOverworld) },
	{ "ClearOverworld", LUA_OPCODE(Lua_V2, ClearOverworld) },
	{ "ToggleOverworld", LUA_OPCODE(Lua_V2, ToggleOverworld) },
	{ "WalkActorVector", LUA_OPCODE(Lua_V2, WalkActorVector) },
	{ "WalkActorForward", LUA_OPCODE(Lua_V2, WalkActorForward) },
	{ "ActorStopMoving", LUA_OPCODE(Lua_V2, ActorStopMoving) },
	{ "WalkActorTo", LUA_OPCODE(Lua_V2, WalkActorTo) },
	{ "WalkActorToAvoiding", LUA_OPCODE(Lua_V2, WalkActorToAvoiding) },
	{ "ActorLookAt", LUA_OPCODE(Lua_V2, ActorLookAt) },
	{ "IsActorLooking", LUA_OPCODE(Lua_V2, IsActorLooking) },
	{ "SetActorLookRate", LUA_OPCODE(Lua_V2, SetActorLookRate) },
	{ "GetActorLookRate", LUA_OPCODE(Lua_V2, GetActorLookRate) },
	{ "SetActorFOV", LUA_OPCODE(Lua_V2, SetActorFOV) },
	{ "GetActorFOV", LUA_OPCODE(Lua_V2, GetActorFOV) },
	{ "GetVisibleThings", LUA_OPCODE(Lua_V2, GetVisibleThings) },
	{ "SetActorLighting", LUA_OPCODE(Lua_V2, SetActorLighting) },
	{ "SetActorHead", LUA_OPCODE(Lua_V2, SetActorHead) },
	{ "SetActorHeadLimits", LUA_OPCODE(Lua_V2, SetActorHeadLimits) },
	{ "SetActorVisibility", LUA_OPCODE(Lua_V2, SetActorVisibility) },
	{ "SetActorFollowBoxes", LUA_OPCODE(Lua_V2, SetActorFollowBoxes) },
	{ "ActorActivateShadow", LUA_OPCODE(Lua_V2, ActorActivateShadow) },
	{ "ShutUpActor", LUA_OPCODE(Lua_V2, ShutUpActor) },
	{ "EnableActorPuck", LUA_OPCODE(Lua_V2, EnableActorPuck) },
	{ "SetActorGlobalAlpha", LUA_OPCODE(Lua_V2, SetActorGlobalAlpha) },
	{ "SetActorLocalAlpha", LUA_OPCODE(Lua_V2, SetActorLocalAlpha) },
	{ "SetActorCollisionMode", LUA_OPCODE(Lua_V2, SetActorCollisionMode) },
	{ "SetActorCollisionScale", LUA_OPCODE(Lua_V2, SetActorCollisionScale) },
	{ "SetActorSortOrder", LUA_OPCODE(Lua_V2, SetActorSortOrder) },
	{ "GetActorSortOrder", LUA_OPCODE(Lua_V2, GetActorSortOrder) },
	{ "AttachActor", LUA_OPCODE(Lua_V2, AttachActor) },
	{ "DetachActor", LUA_OPCODE(Lua_V2, DetachActor) },
	{ "IsActorInSector", LUA_OPCODE(Lua_V2, IsActorInSector) },
	{ "GetActorSector", LUA_OPCODE(Lua_V2, GetActorSector) },
	{ "IsPointInSector", LUA_OPCODE(Lua_V2, IsPointInSector) },
	{ "GetPointSector", LUA_OPCODE(Lua_V2, GetPointSector) },
	{ "TurnActor", LUA_OPCODE(Lua_V2, TurnActor) },
	{ "GetActorRot", LUA_OPCODE(Lua_V2, GetActorRot) },
	{ "SetActorRot", LUA_OPCODE(Lua_V2, SetActorRot) },
	{ "SetActorPitch", LUA_OPCODE(Lua_V2, SetActorPitch) },
	{ "SetActorRoll", LUA_OPCODE(Lua_V2, SetActorRoll) },
	{ "IsActorTurning", LUA_OPCODE(Lua_V2, IsActorTurning) },
	{ "PointActorAt", LUA_OPCODE(Lua_V2, PointActorAt) },
	{ "TurnActorTo", LUA_OPCODE(Lua_V2, TurnActorTo) },
	{ "PlayActorChore", LUA_OPCODE(Lua_V2, PlayActorChore) },
	{ "PlayActorChoreLooping", LUA_OPCODE(Lua_V2, PlayActorChoreLooping) },
	{ "IsActorMoving", LUA_OPCODE(Lua_V2, IsActorMoving) },
	{ "IsActorResting", LUA_OPCODE(Lua_V2, IsActorResting) },
	{ "GetActorChores", LUA_OPCODE(Lua_V2, GetActorChores) },
	{ "StopActorChores", LUA_OPCODE(Lua_V2, StopActorChores) },
	{ "IsChoreValid", LUA_OPCODE(Lua_V2, IsChoreValid) },
	{ "IsChorePlaying", LUA_OPCODE(Lua_V2, IsChorePlaying) },
	{ "IsChoreLooping", LUA_OPCODE(Lua_V2, IsChoreLooping) },
	{ "SetChoreLooping", LUA_OPCODE(Lua_V2, SetChoreLooping) },
	{ "PlayChore", LUA_OPCODE(Lua_V2, PlayChore) },
	{ "FadeInChore", LUA_OPCODE(Lua_V2, FadeInChore) },
	{ "FadeOutChore", LUA_OPCODE(Lua_V2, FadeOutChore) },
	{ "StopChore", LUA_OPCODE(Lua_V2, StopChore) },
	{ "PauseChore", LUA_OPCODE(Lua_V2, PauseChore) },
	{ "AdvanceChore", LUA_OPCODE(Lua_V2, AdvanceChore) },
	{ "CompleteChore", LUA_OPCODE(Lua_V2, CompleteChore) },
	{ "LockChore", LUA_OPCODE(Lua_V2, LockChore) },
	{ "UnlockChore", LUA_OPCODE(Lua_V2, UnlockChore) },
	{ "LockChoreSet", LUA_OPCODE(Lua_V2, LockChoreSet) },
	{ "UnlockChoreSet", LUA_OPCODE(Lua_V2, UnlockChoreSet) },
	{ "exit", LUA_OPCODE(Lua_V2, Exit) },
	{ "FunctionName", LUA_OPCODE(Lua_V2, FunctionName) },
	{ "EnableControl", LUA_OPCODE(Lua_V2, EnableControl) },
	{ "DisableControl", LUA_OPCODE(Lua_V2, DisableControl) },
	{ "GetControlState", LUA_OPCODE(Lua_V2, GetControlState) },
	{ "PrintMessage", LUA_OPCODE(Lua_V2, PrintMessage) },
	{ "PrintError", LUA_OPCODE(Lua_V2, PrintError) },
	{ "PrintWarning", LUA_OPCODE(Lua_V2, PrintWarning) },
	{ "PrintDebug", LUA_OPCODE(Lua_V2, PrintDebug) },
	{ "MakeCurrentSet", LUA_OPCODE(Lua_V2, MakeCurrentSet) },
	{ "LockSet", LUA_OPCODE(Lua_V2, LockSet) },
	{ "UnLockSet", LUA_OPCODE(Lua_V2, UnLockSet) },
	{ "LockBackground", LUA_OPCODE(Lua_V2, LockBackground) },
	{ "UnLockBackground", LUA_OPCODE(Lua_V2, UnLockBackground) },
	{ "MakeCurrentSetup", LUA_OPCODE(Lua_V2, MakeCurrentSetup) },
	{ "GetCurrentSetup", LUA_OPCODE(Lua_V2, GetCurrentSetup) },
	{ "NextSetup", LUA_OPCODE(Lua_V2, NextSetup) },
	{ "PreviousSetup", LUA_OPCODE(Lua_V2, PreviousSetup) },
	{ "StartMovie", LUA_OPCODE(Lua_V2, StartMovie) },
	{ "StopMovie", LUA_OPCODE(Lua_V2, StopMovie) },
	{ "PauseMovie", LUA_OPCODE(Lua_V2, PauseMovie) },
	{ "EscapeMovie", LUA_OPCODE(Lua_V2, EscapeMovie) },
	{ "IsMoviePlaying", LUA_OPCODE(Lua_V2, IsMoviePlaying) },
	{ "PlaySound", LUA_OPCODE(Lua_V2, PlaySound) },
	{ "StopSound", LUA_OPCODE(Lua_V2, StopSound) },
	{ "StopAllSounds", LUA_OPCODE(Lua_V2, StopAllSounds) },
	{ "LoadSound", LUA_OPCODE(Lua_V2, LoadSound) },
	{ "FreeSound", LUA_OPCODE(Lua_V2, FreeSound) },
	{ "PlayLoadedSound", LUA_OPCODE(Lua_V2, PlayLoadedSound) },
	{ "IsSoundPlaying", LUA_OPCODE(Lua_V2, IsSoundPlaying) },
	{ "GetGroupVolume", LUA_OPCODE(Lua_V2, GetGroupVolume) },
	{ "SetGroupVolume", LUA_OPCODE(Lua_V2, SetGroupVolume) },
	{ "GetSoundVolume", LUA_OPCODE(Lua_V2, GetSoundVolume) },
	{ "SetSoundVolume", LUA_OPCODE(Lua_V2, SetSoundVolume) },
	{ "EnableAudioGroup", LUA_OPCODE(Lua_V2, EnableAudioGroup) },
	{ "EnableVoiceFX", LUA_OPCODE(Lua_V2, EnableVoiceFX) },
	{ "SetAudioMixAhead", LUA_OPCODE(Lua_V2, SetAudioMixAhead) },
	{ "PlaySoundFrom", LUA_OPCODE(Lua_V2, PlaySoundFrom) },
	{ "PlayLoadedSoundFrom", LUA_OPCODE(Lua_V2, PlayLoadedSoundFrom) },
	{ "SetReverb", LUA_OPCODE(Lua_V2, SetReverb) },
	{ "UpdateSoundPosition", LUA_OPCODE(Lua_V2, UpdateSoundPosition) },
	{ "ImSetState", LUA_OPCODE(Lua_V2, ImSetState) },
	{ "ImSelectSet", LUA_OPCODE(Lua_V2, ImSelectSet) },
	{ "ImStateHasLooped", LUA_OPCODE(Lua_V2, ImStateHasLooped) },
	{ "ImStateHasEnded", LUA_OPCODE(Lua_V2, ImStateHasEnded) },
	{ "ImPushState", LUA_OPCODE(Lua_V2, ImPushState) },
	{ "ImPopState", LUA_OPCODE(Lua_V2, ImPopState) },
	{ "ImFlushStack", LUA_OPCODE(Lua_V2, ImFlushStack) },
	{ "ImGetMillisecondPosition", LUA_OPCODE(Lua_V2, ImGetMillisecondPosition) },
	{ "IsSectorActive", LUA_OPCODE(Lua_V2, IsSectorActive) },
	{ "MakeSectorActive", LUA_OPCODE(Lua_V2, MakeSectorActive) },
	{ "GetSectorOppositeEdge", LUA_OPCODE(Lua_V2, GetSectorOppositeEdge) },
	{ "GetSectorName", LUA_OPCODE(Lua_V2, GetSectorName) },
	{ "WorldToScreen", LUA_OPCODE(Lua_V2, WorldToScreen) },
	{ "GetCameraPosition", LUA_OPCODE(Lua_V2, GetCameraPosition) },
	{ "SetCameraPosition", LUA_OPCODE(Lua_V2, SetCameraPosition) },
	{ "CameraLookAt", LUA_OPCODE(Lua_V2, CameraLookAt) },
	{ "GetCameraFOV", LUA_OPCODE(Lua_V2, GetCameraFOV) },
	{ "SetCameraFOV", LUA_OPCODE(Lua_V2, SetCameraFOV) },
	{ "GetCameraYaw", LUA_OPCODE(Lua_V2, GetCameraYaw) },
	{ "YawCamera", LUA_OPCODE(Lua_V2, YawCamera) },
	{ "GetCameraPitch", LUA_OPCODE(Lua_V2, GetCameraPitch) },
	{ "PitchCamera", LUA_OPCODE(Lua_V2, PitchCamera) },
	{ "GetCameraRoll", LUA_OPCODE(Lua_V2, GetCameraRoll) },
	{ "RollCamera", LUA_OPCODE(Lua_V2, RollCamera) },
	{ "GetCameraLookVector", LUA_OPCODE(Lua_V2, GetCameraLookVector) },
	{ "PerSecond", LUA_OPCODE(Lua_V2, PerSecond) },
	{ "SnapToPlane", LUA_OPCODE(Lua_V2, SnapToPlane) },
	{ "GetAngleBetweenVectors", LUA_OPCODE(Lua_V2, GetAngleBetweenVectors) },
	{ "GetAngleBetweenActors", LUA_OPCODE(Lua_V2, GetAngleBetweenActors) },
	{ "SetAmbientLight", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "TurnLightOn", LUA_OPCODE(Lua_V2, TurnLightOn) },
	{ "IsLightOn", LUA_OPCODE(Lua_V2, IsLightOn) },
	{ "SetLightPosition", LUA_OPCODE(Lua_V2, SetLightPosition) },
	{ "GammaEnabled", LUA_OPCODE(Lua_V2, GammaEnabled) },
	{ "SetGamma", LUA_OPCODE(Lua_V2, SetGamma) },
	{ "SetPixelCenter", LUA_OPCODE(Lua_V2, SetPixelCenter) },
	{ "LoadBundle", LUA_OPCODE(Lua_V2, LoadBundle) },
	{ "RenderModeUser", LUA_OPCODE(Lua_V2, RenderModeUser) },
	{ "DimScreen", LUA_OPCODE(Lua_V2, DimScreen) },
	{ "DimRegion", LUA_OPCODE(Lua_V2, DimRegion) },
	{ "UndimAll", LUA_OPCODE(Lua_V2, UndimAll) },
	{ "UndimRegion", LUA_OPCODE(Lua_V2, UndimRegion) },
	{ "Display", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "SetSpeechMode", LUA_OPCODE(Lua_V2, SetSpeechMode) },
	{ "GetSpeechMode", LUA_OPCODE(Lua_V2, GetSpeechMode) },
	{ "GetCPUSpeed", LUA_OPCODE(Lua_V2, GetCPUSpeed) },
	{ "NewLayer", LUA_OPCODE(Lua_V2, NewLayer) },
	{ "FreeLayer", LUA_OPCODE(Lua_V2, FreeLayer) },
	{ "GetLayerSortOrder", LUA_OPCODE(Lua_V2, GetLayerSortOrder) },
	{ "SetLayerSortOrder", LUA_OPCODE(Lua_V2, SetLayerSortOrder) },
	{ "GetLayerFrame", LUA_OPCODE(Lua_V2, GetLayerFrame) },
	{ "SetLayerFrame", LUA_OPCODE(Lua_V2, SetLayerFrame) },
	{ "AdvanceLayerFrame", LUA_OPCODE(Lua_V2, AdvanceLayerFrame) },
	{ "SetTextSpeed", LUA_OPCODE(Lua_V2, SetTextSpeed) },
	{ "GetTextSpeed", LUA_OPCODE(Lua_V2, GetTextSpeed) },
	{ "IsMessageGoing", LUA_OPCODE(Lua_V2, IsMessageGoing) },
	{ "SetSayLineDefaults", LUA_OPCODE(Lua_V2, SetSayLineDefaults) },
	{ "SetActorTalkColor", LUA_OPCODE(Lua_V2, SetActorTalkColor) },
	{ "GetActorTalkColor", LUA_OPCODE(Lua_V2, GetActorTalkColor) },
	{ "SayLine", LUA_OPCODE(Lua_V2, SayLine) },
	{ "MakeTextObject", LUA_OPCODE(Lua_V2, MakeTextObject) },
	{ "KillTextObject", LUA_OPCODE(Lua_V2, KillTextObject) },
	{ "ChangeTextObject", LUA_OPCODE(Lua_V2, ChangeTextObject) },
	{ "GetTextObjectDimensions", LUA_OPCODE(Lua_V2, GetTextObjectDimensions) },
	{ "BlastText", LUA_OPCODE(Lua_V2, BlastText) },
	{ "ExpireText", LUA_OPCODE(Lua_V2, ExpireText) },
	{ "PurgeText", LUA_OPCODE(Lua_V2, PurgeText) },
	{ "PushText", LUA_OPCODE(Lua_V2, PushText) },
	{ "PopText", LUA_OPCODE(Lua_V2, PopText) },
	{ "MakeColor", LUA_OPCODE(Lua_V2, MakeColor) },
	{ "GetColorComponents", LUA_OPCODE(Lua_V2, GetColorComponents) },
	{ "SetTranslationMode", LUA_OPCODE(Lua_V2, SetTranslationMode) },
	{ "GetTranslationMode", LUA_OPCODE(Lua_V2, GetTranslationMode) },
	{ "LocalizeString", LUA_OPCODE(Lua_V2, LocalizeString) },
	{ "GetFontDimensions", LUA_OPCODE(Lua_V2, GetFontDimensions) },
	{ "GetTextCharPosition", LUA_OPCODE(Lua_V2, GetTextCharPosition) },
	{ "SetOffscreenTextPos", LUA_OPCODE(Lua_V2, SetOffscreenTextPos) },
	{ "ReportAllResources", LUA_OPCODE(Lua_V2, ReportAllResources) },
	{ "ReportLRUResources", LUA_OPCODE(Lua_V2, ReportLRUResources) },
	{ "ReportUsedResources", LUA_OPCODE(Lua_V2, ReportUsedResources) },
	{ "ReportUnusedResources", LUA_OPCODE(Lua_V2, ReportUnusedResources) },
	{ "PurgeAmount", LUA_OPCODE(Lua_V2, PurgeAmount) },
	{ "PurgeAllResources", LUA_OPCODE(Lua_V2, PurgeAllResources) },
	{ "PurgeUnusedResources", LUA_OPCODE(Lua_V2, PurgeUnusedResources) },
	{ "PurgeUnlockedResources", LUA_OPCODE(Lua_V2, PurgeUnlockedResources) },
	{ "NukeAllScriptLocks", LUA_OPCODE(Lua_V2, NukeAllScriptLocks) },
	{ "LightMgrStartup", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "LightMgrShutdown", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "LightMgrSetChange", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDebugDraw", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawCameras", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawLights", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawSectors", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawBBoxes", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawFPS", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawPerformance", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawActorStats", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "WriteSetToFile", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditSelect", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditPlace", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditModify", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditDelete", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditInsert", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditSortAdd", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditForgetIt", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "FRUTEY_Begin", LUA_OPCODE(Lua_V2, FRUTEY_Begin) },
	{ "FRUTEY_End", LUA_OPCODE(Lua_V2, FRUTEY_End) },
	{ "ImGetSfxVol", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ImSetSfxVol", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ImGetVoiceVol", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ImSetVoiceVol", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ImGetMusicVol", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ImSetMusicVol", LUA_OPCODE(Lua_V2, UnimplementedFunction) }
};

// Saves store the C functions by position, so the demo needs the order of its own table
static const struct luaL_reg monkeyDemoMainOpcodes[] = {
	{ "EnginePause", LUA_OPCODE(Lua_V2, EnginePause) },
	{ "EngineResume", LUA_OPCODE(Lua_V2, EngineResume) },
	{ "EngineDisplay", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "SpewStartup", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "SpewShutdown", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "GetCurrentScript", LUA_OPCODE(Lua_V1, GetCurrentScript) },
	{ "GetPhysicalBytes", LUA_OPCODE(Lua_V2, GetPhysicalBytes) },
	{ "GetMemoryUsage", LUA_OPCODE(Lua_V2, GetMemoryUsage) },
	{ "SetMemoryUsage", LUA_OPCODE(Lua_V2, SetMemoryUsage) },
	{ "GetDiskFreeSpace", LUA_OPCODE(Lua_V1, GetDiskFreeSpace) },
	{ "FindFileOrSwapCDs", LUA_OPCODE(Lua_V2, FindFileOrSwapCDs) },
	{ "CheckForFile", LUA_OPCODE(Lua_V2, CheckForFile) },
	{ "FindFileOnAnyCD", LUA_OPCODE(Lua_V2, FindFileOnAnyCD) },
	{ "FileFindFirst", LUA_OPCODE(Lua_V2, FileFindFirst) },
	{ "FileFindNext", LUA_OPCODE(Lua_V2, FileFindNext) },
	{ "FileFindDispose", LUA_OPCODE(Lua_V2, FileFindDispose) },
	{ "TextFileGetLineCount", LUA_OPCODE(Lua_V2, TextFileGetLineCount) },
	{ "TextFileGetLine", LUA_OPCODE(Lua_V2, TextFileGetLine) },
	{ "InputDialog", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "QueryDialog", LUA_OPCODE(Lua_V2, QueryDialog) },
	{ "WriteRegistryValue", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ReadRegistryValue", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "Load", LUA_OPCODE(Lua_V1, Load) },
	{ "Save", LUA_OPCODE(Lua_V1, Save) },
	{ "ScreenShot", LUA_OPCODE(Lua_V2, ScreenShot) },
	{ "MakeScreenTextures", LUA_OPCODE(Lua_V2, MakeScreenTextures) },
	{ "SubmitSaveGameData", LUA_OPCODE(Lua_V2, SubmitSaveGameData) },
	{ "GetSaveGameData", LUA_OPCODE(Lua_V2, GetSaveGameData) },
	{ "JustLoaded", LUA_OPCODE(Lua_V2, JustLoaded) },
	{ "ThumbnailFromScreen", LUA_OPCODE(Lua_V2, ThumbnailFromScreen) },
	{ "ThumbnailFromFile", LUA_OPCODE(Lua_V2, ThumbnailFromFile) },
	{ "ClearSpecialtyTexture", LUA_OPCODE(Lua_V2, ClearSpecialtyTexture) },
	{ "LoadActor", LUA_OPCODE(Lua_V2, LoadActor) },
	{ "UnloadActor", LUA_OPCODE(Lua_V2, UnloadActor) },
	{ "GetActorTimeScale", LUA_OPCODE(Lua_V2, GetActorTimeScale) },
	{ "SetActorTimeScale", LUA_OPCODE(Lua_V2, SetActorTimeScale) },
	{ "SetActorWalkDominate", LUA_OPCODE(Lua_V2, SetActorWalkDominate) },
	{ "SetSelectedActor", LUA_OPCODE(Lua_V2, SetSelectedActor) },
	{ "SetActorWalkChore", LUA_OPCODE(Lua_V2, SetActorWalkChore) },
	{ "SetActorTurnChores", LUA_OPCODE(Lua_V2, SetActorTurnChores) },
	{ "SetActorRestChore", LUA_OPCODE(Lua_V2, SetActorRestChore) },
	{ "SetActorMumblechore", LUA_OPCODE(Lua_V2, SetActorMumblechore) },
	{ "SetActorTalkChore", LUA_OPCODE(Lua_V2, SetActorTalkChore) },
	{ "SetActorWalkRate", LUA_OPCODE(Lua_V2, SetActorWalkRate) },
	{ "GetActorWalkRate", LUA_OPCODE(Lua_V2, GetActorWalkRate) },
	{ "SetActorTurnRate", LUA_OPCODE(Lua_V2, SetActorTurnRate) },
	{ "GetActorTurnRate", LUA_OPCODE(Lua_V2, GetActorTurnRate) },
	{ "GetActorPos", LUA_OPCODE(Lua_V2, GetActorPos) },
	{ "GetActorWorldPos", LUA_OPCODE(Lua_V2, GetActorWorldPos) },
	{ "GetActorPuckVector", LUA_OPCODE(Lua_V2, GetActorPuckVector) },
	{ "GetActorYawToPoint", LUA_OPCODE(Lua_V2, GetActorYawToPoint) },
	{ "SetActorReflection", LUA_OPCODE(Lua_V2, SetActorReflection) },
	{ "PutActorInSet", LUA_OPCODE(Lua_V2, PutActorInSet) },
	{ "PutActorAt", LUA_OPCODE(Lua_V2, PutActorAt) },
	{ "PutActorInOverworld", LUA_OPCODE(Lua_V2, PutActorInOverworld) },
	{ "RemoveActorFromOverworld", LUA_OPCODE(Lua_V2, RemoveActorFromOverworld) },
	{ "ClearOverworld", LUA_OPCODE(Lua_V2, ClearOverworld) },
	{ "ToggleOverworld", LUA_OPCODE(Lua_V2, ToggleOverworld) },
	{ "WalkActorVector", LUA_OPCODE(Lua_V2, WalkActorVector) },
	{ "WalkActorForward", LUA_OPCODE(Lua_V2, WalkActorForward) },
	{ "ActorStopMoving", LUA_OPCODE(Lua_V2, ActorStopMoving) },
	{ "WalkActorTo", LUA_OPCODE(Lua_V2, WalkActorTo) },
	{ "WalkActorToAvoiding", LUA_OPCODE(Lua_V2, WalkActorToAvoiding) },
	{ "ActorLookAt", LUA_OPCODE(Lua_V2, ActorLookAt) },
	{ "IsActorLooking", LUA_OPCODE(Lua_V2, IsActorLooking) },
	{ "SetActorLookRate", LUA_OPCODE(Lua_V2, SetActorLookRate) },
	{ "GetActorLookRate", LUA_OPCODE(Lua_V2, GetActorLookRate) },
	{ "SetActorFOV", LUA_OPCODE(Lua_V2, SetActorFOV) },
	{ "GetActorFOV", LUA_OPCODE(Lua_V2, GetActorFOV) },
	{ "GetVisibleThings", LUA_OPCODE(Lua_V2, GetVisibleThings) },
	{ "SetActorLighting", LUA_OPCODE(Lua_V2, SetActorLighting) },
	{ "SetActorHead", LUA_OPCODE(Lua_V2, SetActorHead) },
	{ "SetActorHeadLimits", LUA_OPCODE(Lua_V2, SetActorHeadLimits) },
	{ "SetActorVisibility", LUA_OPCODE(Lua_V2, SetActorVisibility) },
	{ "SetActorFollowBoxes", LUA_OPCODE(Lua_V2, SetActorFollowBoxes) },
	{ "ActorActivateShadow", LUA_OPCODE(Lua_V2, ActorActivateShadow) },
	{ "ShutUpActor", LUA_OPCODE(Lua_V2, ShutUpActor) },
	{ "EnableActorPuck", LUA_OPCODE(Lua_V2, EnableActorPuck) },
	{ "SetActorGlobalAlpha", LUA_OPCODE(Lua_V2, SetActorGlobalAlpha) },
	{ "SetActorLocalAlpha", LUA_OPCODE(Lua_V2, SetActorLocalAlpha) },
	{ "SetActorCollisionMode", LUA_OPCODE(Lua_V2, SetActorCollisionMode) },
	{ "SetActorCollisionScale", LUA_OPCODE(Lua_V2, SetActorCollisionScale) },
	{ "SetActorSortOrder", LUA_OPCODE(Lua_V2, SetActorSortOrder) },
	{ "GetActorSortOrder", LUA_OPCODE(Lua_V2, GetActorSortOrder) },
	{ "AttachActor", LUA_OPCODE(Lua_V2, AttachActor) },
	{ "DetachActor", LUA_OPCODE(Lua_V2, DetachActor) },
	{ "IsActorInSector", LUA_OPCODE(Lua_V2, IsActorInSector) },
	{ "GetActorSector", LUA_OPCODE(Lua_V2, GetActorSector) },
	{ "IsPointInSector", LUA_OPCODE(Lua_V2, IsPointInSector) },
	{ "GetPointSector", LUA_OPCODE(Lua_V2, GetPointSector) },
	{ "TurnActor", LUA_OPCODE(Lua_V2, TurnActor) },
	{ "GetActorRot", LUA_OPCODE(Lua_V2, GetActorRot) },
	{ "SetActorRot", LUA_OPCODE(Lua_V2, SetActorRot) },
	{ "SetActorPitch", LUA_OPCODE(Lua_V2, SetActorPitch) },
	{ "SetActorRoll", LUA_OPCODE(Lua_V2, SetActorRoll) },
	{ "IsActorTurning", LUA_OPCODE(Lua_V2, IsActorTurning) },
	{ "PointActorAt", LUA_OPCODE(Lua_V2, PointActorAt) },
	{ "TurnActorTo", LUA_OPCODE(Lua_V2, TurnActorTo) },
	{ "PlayActorChore", LUA_OPCODE(Lua_V2, PlayActorChore) },
	{ "PlayActorChoreLooping", LUA_OPCODE(Lua_V2, PlayActorChoreLooping) },
	{ "IsActorMoving", LUA_OPCODE(Lua_V2, IsActorMoving) },
	{ "IsActorResting", LUA_OPCODE(Lua_V2, IsActorResting) },
	{ "GetActorChores", LUA_OPCODE(Lua_V2, GetActorChores) },
	{ "StopActorChores", LUA_OPCODE(Lua_V2, StopActorChores) },
	{ "IsChoreValid", LUA_OPCODE(Lua_V2, IsChoreValid) },
	{ "IsChorePlaying", LUA_OPCODE(Lua_V2, IsChorePlaying) },
	{ "IsChoreLooping", LUA_OPCODE(Lua_V2, IsChoreLooping) },
	{ "SetChoreLooping", LUA_OPCODE(Lua_V2, SetChoreLooping) },
	{ "PlayChore", LUA_OPCODE(Lua_V2, PlayChore) },
	{ "FadeInChore", LUA_OPCODE(Lua_V2, FadeInChore) },
	{ "FadeOutChore", LUA_OPCODE(Lua_V2, FadeOutChore) },
	{ "StopChore", LUA_OPCODE(Lua_V2, StopChore) },
	{ "PauseChore", LUA_OPCODE(Lua_V2, PauseChore) },
	{ "AdvanceChore", LUA_OPCODE(Lua_V2, AdvanceChore) },
	{ "CompleteChore", LUA_OPCODE(Lua_V2, CompleteChore) },
	{ "LockChore", LUA_OPCODE(Lua_V2, LockChore) },
	{ "UnlockChore", LUA_OPCODE(Lua_V2, UnlockChore) },
	{ "LockChoreSet", LUA_OPCODE(Lua_V2, LockChoreSet) },
	{ "UnlockChoreSet", LUA_OPCODE(Lua_V2, UnlockChoreSet) },
	{ "exit", LUA_OPCODE(Lua_V2, Exit) },
	{ "FunctionName", LUA_OPCODE(Lua_V2, FunctionName) },
	{ "EnableControl", LUA_OPCODE(Lua_V2, EnableControl) },
	{ "DisableControl", LUA_OPCODE(Lua_V2, DisableControl) },
	{ "GetControlState", LUA_OPCODE(Lua_V2, GetControlState) },
	{ "PrintMessage", LUA_OPCODE(Lua_V2, PrintMessage) },
	{ "PrintError", LUA_OPCODE(Lua_V2, PrintError) },
	{ "PrintWarning", LUA_OPCODE(Lua_V2, PrintWarning) },
	{ "PrintDebug", LUA_OPCODE(Lua_V2, PrintDebug) },
	{ "MakeCurrentSet", LUA_OPCODE(Lua_V2, MakeCurrentSet) },
	{ "LockSet", LUA_OPCODE(Lua_V2, LockSet) },
	{ "UnLockSet", LUA_OPCODE(Lua_V2, UnLockSet) },
	{ "LockBackground", LUA_OPCODE(Lua_V2, LockBackground) },
	{ "UnLockBackground", LUA_OPCODE(Lua_V2, UnLockBackground) },
	{ "MakeCurrentSetup", LUA_OPCODE(Lua_V2, MakeCurrentSetup) },
	{ "GetCurrentSetup", LUA_OPCODE(Lua_V2, GetCurrentSetup) },
	{ "NextSetup", LUA_OPCODE(Lua_V2, NextSetup) },
	{ "PreviousSetup", LUA_OPCODE(Lua_V2, PreviousSetup) },
	{ "StartMovie", LUA_OPCODE(Lua_V2, StartMovie) },
	{ "StopMovie", LUA_OPCODE(Lua_V2, StopMovie) },
	{ "PauseMovie", LUA_OPCODE(Lua_V2, PauseMovie) },
	{ "IsMoviePlaying", LUA_OPCODE(Lua_V2, IsMoviePlaying) },
	{ "PlaySound", LUA_OPCODE(Lua_V2, PlaySound) },
	{ "StopAllSounds", LUA_OPCODE(Lua_V2, StopAllSounds) },
	{ "LoadSound", LUA_OPCODE(Lua_V2, LoadSound) },
	{ "FreeSound", LUA_OPCODE(Lua_V2, FreeSound) },
	{ "PlayLoadedSound", LUA_OPCODE(Lua_V2, PlayLoadedSound) },
	{ "IsSoundPlaying", LUA_OPCODE(Lua_V2, IsSoundPlaying) },
	{ "GetGroupVolume", LUA_OPCODE(Lua_V2, GetGroupVolume) },
	{ "SetGroupVolume", LUA_OPCODE(Lua_V2, SetGroupVolume) },
	{ "GetSoundVolume", LUA_OPCODE(Lua_V2, GetSoundVolume) },
	{ "SetSoundVolume", LUA_OPCODE(Lua_V2, SetSoundVolume) },
	{ "ImSetState", LUA_OPCODE(Lua_V2, ImSetState) },
	{ "ImPause", LUA_OPCODE(Lua_V2, ImPause) },
	{ "ImResume", LUA_OPCODE(Lua_V2, ImResume) },
	{ "ImSetVoiceEffect", LUA_OPCODE(Lua_V2, ObsoleteFunction) },
	{ "IsSectorActive", LUA_OPCODE(Lua_V2, IsSectorActive) },
	{ "MakeSectorActive", LUA_OPCODE(Lua_V2, MakeSectorActive) },
	{ "GetSectorOppositeEdge", LUA_OPCODE(Lua_V2, GetSectorOppositeEdge) },
	{ "WorldToScreen", LUA_OPCODE(Lua_V2, WorldToScreen) },
	{ "GetCameraPosition", LUA_OPCODE(Lua_V2, GetCameraPosition) },
	{ "SetCameraPosition", LUA_OPCODE(Lua_V2, SetCameraPosition) },
	{ "CameraLookAt", LUA_OPCODE(Lua_V2, CameraLookAt) },
	{ "GetCameraFOV", LUA_OPCODE(Lua_V2, GetCameraFOV) },
	{ "SetCameraFOV", LUA_OPCODE(Lua_V2, SetCameraFOV) },
	{ "GetCameraYaw", LUA_OPCODE(Lua_V2, GetCameraYaw) },
	{ "YawCamera", LUA_OPCODE(Lua_V2, YawCamera) },
	{ "GetCameraPitch", LUA_OPCODE(Lua_V2, GetCameraPitch) },
	{ "PitchCamera", LUA_OPCODE(Lua_V2, PitchCamera) },
	{ "GetCameraRoll", LUA_OPCODE(Lua_V2, GetCameraRoll) },
	{ "RollCamera", LUA_OPCODE(Lua_V2, RollCamera) },
	{ "GetCameraLookVector", LUA_OPCODE(Lua_V2, GetCameraLookVector) },
	{ "PerSecond", LUA_OPCODE(Lua_V2, PerSecond) },
	{ "SnapToPlane", LUA_OPCODE(Lua_V2, SnapToPlane) },
	{ "GetAngleBetweenVectors", LUA_OPCODE(Lua_V2, GetAngleBetweenVectors) },
	{ "GetAngleBetweenActors", LUA_OPCODE(Lua_V2, GetAngleBetweenActors) },
	{ "SetAmbientLight", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "TurnLightOn", LUA_OPCODE(Lua_V2, TurnLightOn) },
	{ "IsLightOn", LUA_OPCODE(Lua_V2, IsLightOn) },
	{ "SetLightPosition", LUA_OPCODE(Lua_V2, SetLightPosition) },
	{ "GammaEnabled", LUA_OPCODE(Lua_V2, GammaEnabled) },
	{ "SetGamma", LUA_OPCODE(Lua_V2, SetGamma) },
	{ "SetPixelCenter", LUA_OPCODE(Lua_V2, SetPixelCenter) },
	{ "RenderModeUser", LUA_OPCODE(Lua_V2, RenderModeUser) },
	{ "DimScreen", LUA_OPCODE(Lua_V2, DimScreen) },
	{ "DimRegion", LUA_OPCODE(Lua_V2, DimRegion) },
	{ "UndimAll", LUA_OPCODE(Lua_V2, UndimAll) },
	{ "UndimRegion", LUA_OPCODE(Lua_V2, UndimRegion) },
	{ "Display", LUA_OPCODE(Lua_V2, EmptyFunction) },
	{ "SetSpeechMode", LUA_OPCODE(Lua_V2, SetSpeechMode) },
	{ "GetSpeechMode", LUA_OPCODE(Lua_V2, GetSpeechMode) },
	{ "NewLayer", LUA_OPCODE(Lua_V2, NewLayer) },
	{ "FreeLayer", LUA_OPCODE(Lua_V2, FreeLayer) },
	{ "GetLayerSortOrder", LUA_OPCODE(Lua_V2, GetLayerSortOrder) },
	{ "SetLayerSortOrder", LUA_OPCODE(Lua_V2, SetLayerSortOrder) },
	{ "GetLayerFrame", LUA_OPCODE(Lua_V2, GetLayerFrame) },
	{ "SetLayerFrame", LUA_OPCODE(Lua_V2, SetLayerFrame) },
	{ "AdvanceLayerFrame", LUA_OPCODE(Lua_V2, AdvanceLayerFrame) },
	{ "SetTextSpeed", LUA_OPCODE(Lua_V2, SetTextSpeed) },
	{ "GetTextSpeed", LUA_OPCODE(Lua_V2, GetTextSpeed) },
	{ "IsMessageGoing", LUA_OPCODE(Lua_V2, IsMessageGoing) },
	{ "SetSayLineDefaults", LUA_OPCODE(Lua_V2, SetSayLineDefaults) },
	{ "SetActorTalkColor", LUA_OPCODE(Lua_V2, SetActorTalkColor) },
	{ "GetActorTalkColor", LUA_OPCODE(Lua_V2, GetActorTalkColor) },
	{ "SayLine", LUA_OPCODE(Lua_V2, SayLine) },
	{ "MakeTextObject", LUA_OPCODE(Lua_V2, MakeTextObject) },
	{ "KillTextObject", LUA_OPCODE(Lua_V2, KillTextObject) },
	{ "ChangeTextObject", LUA_OPCODE(Lua_V2, ChangeTextObject) },
	{ "GetTextObjectDimensions", LUA_OPCODE(Lua_V2, GetTextObjectDimensions) },
	{ "BlastText", LUA_OPCODE(Lua_V2, BlastText) },
	{ "ExpireText", LUA_OPCODE(Lua_V2, ExpireText) },
	{ "PurgeText", LUA_OPCODE(Lua_V2, PurgeText) },
	{ "MakeColor", LUA_OPCODE(Lua_V2, MakeColor) },
	{ "GetColorComponents", LUA_OPCODE(Lua_V2, GetColorComponents) },
	{ "SetTranslationMode", LUA_OPCODE(Lua_V2, SetTranslationMode) },
	{ "GetTranslationMode", LUA_OPCODE(Lua_V2, GetTranslationMode) },
	{ "LocalizeString", LUA_OPCODE(Lua_V2, LocalizeString) },
	{ "GetFontDimensions", LUA_OPCODE(Lua_V2, GetFontDimensions) },
	{ "GetTextCharPosition", LUA_OPCODE(Lua_V2, GetTextCharPosition) },
	{ "SetOffscreenTextPos", LUA_OPCODE(Lua_V2, SetOffscreenTextPos) },
	{ "ReportAllResources", LUA_OPCODE(Lua_V2, ReportAllResources) },
	{ "ReportLRUResources", LUA_OPCODE(Lua_V2, ReportLRUResources) },
	{ "ReportUsedResources", LUA_OPCODE(Lua_V2, ReportUsedResources) },
	{ "ReportUnusedResources", LUA_OPCODE(Lua_V2, ReportUnusedResources) },
	{ "PurgeAmount", LUA_OPCODE(Lua_V2, PurgeAmount) },
	{ "PurgeAllResources", LUA_OPCODE(Lua_V2, PurgeAllResources) },
	{ "PurgeUnusedResources", LUA_OPCODE(Lua_V2, PurgeUnusedResources) },
	{ "PurgeUnlockedResources", LUA_OPCODE(Lua_V2, PurgeUnlockedResources) },
	{ "LightMgrStartup", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "LightMgrShutdown", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "LightMgrSetChange", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDebugDraw", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawCameras", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawLights", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawSectors", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawBBoxes", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawFPS", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "ToggleDrawPerformance", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "GetSectorName", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "WriteSetToFile", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditSelect", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditPlace", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditModify", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditDelete", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditInsert", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditSortAdd", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "SectEditForgetIt", LUA_OPCODE(Lua_V2, UnimplementedFunction) },
	{ "FRUTEY_Begin", LUA_OPCODE(Lua_V2, FRUTEY_Begin) },
	{ "FRUTEY_End", LUA_OPCODE(Lua_V2, FRUTEY_End) },
	{ "ImGetSfxVol", LUA_OPCODE(Lua_V2, ObsoleteFunction) },
	{ "ImSetSfxVol", LUA_OPCODE(Lua_V2, ObsoleteFunction) },
	{ "ImGetVoiceVol", LUA_OPCODE(Lua_V2, ObsoleteFunction) },
	{ "ImSetVoiceVol", LUA_OPCODE(Lua_V2, ObsoleteFunction) },
	{ "ImGetMusicVol", LUA_OPCODE(Lua_V2, ObsoleteFunction) },
	{ "ImSetMusicVol", LUA_OPCODE(Lua_V2, ObsoleteFunction) }
};

static const struct luaL_reg monkeyPs2MainOpcodes[] = {
	{ "InputDialog", LUA_OPCODE(Lua_V2, InputDialog) },
	{ "GetCurrentScript", LUA_OPCODE(Lua_V1, GetCurrentScript) },
	{ "SetReplayMode", LUA_OPCODE(Lua_V2, SetReplayMode) },
	{ "GetReplayMode", LUA_OPCODE(Lua_V2, GetReplayMode) },
	{ "GetNextDemoNum", LUA_OPCODE(Lua_V2, GetNextDemoNum) },
	{ "NotifyGameState", LUA_OPCODE(Lua_V2, NotifyGameState) },
	{ "exit", LUA_OPCODE(Lua_V2, Exit) },
	{ "FunctionName", LUA_OPCODE(Lua_V2, FunctionName) },
	{ "GetDiskFreeSpace", LUA_OPCODE(Lua_V1, GetDiskFreeSpace) },
	{ "SetActiveCD", LUA_OPCODE(Lua_V2, SetActiveCD) },
	{ "GetActiveCD", LUA_OPCODE(Lua_V2, GetActiveCD) },
	{ "AreWeInternational", LUA_OPCODE(Lua_V2, AreWeInternational) },
	{ "NukeAllScriptLocks", LUA_OPCODE(Lua_V2, NukeAllScriptLocks) },
	{ "PurgeAllResources", LUA_OPCODE(Lua_V2, PurgeAllResources) },
	{ "LoadBundle", LUA_OPCODE(Lua_V2, LoadBundle) },
	{ "Load", LUA_OPCODE(Lua_V1, Load) },
	{ "Save", LUA_OPCODE(Lua_V1, Save) },
	{ "DeleteSavedGame", LUA_OPCODE(Lua_V2, DeleteSavedGame) },
	{ "CopySavedGame", LUA_OPCODE(Lua_V2, CopySavedGame) },
	{ "SubmitSaveGameData", LUA_OPCODE(Lua_V2, SubmitSaveGameData) },
	{ "GetSaveGameData", LUA_OPCODE(Lua_V2, GetSaveGameData) },
	{ "JustLoaded", LUA_OPCODE(Lua_V2, JustLoaded) },
	{ "JustSaved", LUA_OPCODE(Lua_V2, JustSaved) },
	{ "MakeScreenTextures", LUA_OPCODE(Lua_V2, MakeScreenTextures) },
	{ "ThumbnailFromFile", LUA_OPCODE(Lua_V2, ThumbnailFromFile) },
	{ "ScreenshotForSavegame", LUA_OPCODE(Lua_V2, ScreenshotForSavegame) },
	{ "ClearSpecialtyTexture", LUA_OPCODE(Lua_V2, ClearSpecialtyTexture) },
	{ "GetMemoryCardId", LUA_OPCODE(Lua_V2, GetMemoryCardId) },
	{ "GetUniqueUint", LUA_OPCODE(Lua_V2, GetUniqueUint) },
	{ "FormatMemoryCard", LUA_OPCODE(Lua_V2, FormatMemoryCard) },
	{ "LoadActor", LUA_OPCODE(Lua_V2, LoadActor) },
	{ "UnloadActor", LUA_OPCODE(Lua_V2, UnloadActor) },
	{ "GetActorTimeScale", LUA_OPCODE(Lua_V2, GetActorTimeScale) },
	{ "SetActorTimeScale", LUA_OPCODE(Lua_V2, SetActorTimeScale) },
	{ "SetActorWalkDominate", LUA_OPCODE(Lua_V2, SetActorWalkDominate) },
	{ "SetSelectedActor", LUA_OPCODE(Lua_V2, SetSelectedActor) },
	{ "SetActorWalkChore", LUA_OPCODE(Lua_V2, SetActorWalkChore) },
	{ "SetActorTurnChores", LUA_OPCODE(Lua_V2, SetActorTurnChores) },
	{ "SetActorRestChore", LUA_OPCODE(Lua_V2, SetActorRestChore) },
	{ "SetActorMumblechore", LUA_OPCODE(Lua_V2, SetActorMumblechore) },
	{ "SetActorTalkChore", LUA_OPCODE(Lua_V2, SetActorTalkChore) },
	{ "SetActorWalkRate", LUA_OPCODE(Lua_V2, SetActorWalkRate) },
	{ "GetActorWalkRate", LUA_OPCODE(Lua_V2, GetActorWalkRate) },
	{ "SetActorTurnRate", LUA_OPCODE(Lua_V2, SetActorTurnRate) },
	{ "GetActorTurnRate", LUA_OPCODE(Lua_V2, GetActorTurnRate) },
	{ "GetActorPos", LUA_OPCODE(Lua_V2, GetActorPos) },
	{ "GetActorWorldPos", LUA_OPCODE(Lua_V2, GetActorWorldPos) },
	{ "GetActorPuckVector", LUA_OPCODE(Lua_V2, GetActorPuckVector) },
	{ "GetActorYawToPoint", LUA_OPCODE(Lua_V2, GetActorYawToPoint) },
	{ "PutActorInSet", LUA_OPCODE(Lua_V2, PutActorInSet) },
	{ "PutActorAt", LUA_OPCODE(Lua_V2, PutActorAt) },
	{ "PutActorInOverworld", LUA_OPCODE(Lua_V2, PutActorInOverworld) },
	{ "RemoveActorFromOverworld", LUA_OPCODE(Lua_V2, RemoveActorFromOverworld) },
	{ "ClearOverworld", LUA_OPCODE(Lua_V2, ClearOverworld) },
	{ "ToggleOverworld", LUA_OPCODE(Lua_V2, ToggleOverworld) },
	{ "WalkActorVector", LUA_OPCODE(Lua_V2, WalkActorVector) },
	{ "WalkActorForward", LUA_OPCODE(Lua_V2, WalkActorForward) },
	{ "ActorStopMoving", LUA_OPCODE(Lua_V2, ActorStopMoving) },
	{ "WalkActorTo", LUA_OPCODE(Lua_V2, WalkActorTo) },
	{ "WalkActorToAvoiding", LUA_OPCODE(Lua_V2, WalkActorToAvoiding) },
	{ "ActorLookAt", LUA_OPCODE(Lua_V2, ActorLookAt) },
	{ "IsActorLooking", LUA_OPCODE(Lua_V2, IsActorLooking) },
	{ "SetActorLookRate", LUA_OPCODE(Lua_V2, SetActorLookRate) },
	{ "GetActorLookRate", LUA_OPCODE(Lua_V2, GetActorLookRate) },
	{ "SetActorFOV", LUA_OPCODE(Lua_V2, SetActorFOV) },
	{ "GetActorFOV", LUA_OPCODE(Lua_V2, GetActorFOV) },
	{ "GetVisibleThings", LUA_OPCODE(Lua_V2, GetVisibleThings) },
	{ "SetActorLighting", LUA_OPCODE(Lua_V2, SetActorLighting) },
	{ "SetActorHead", LUA_OPCODE(Lua_V2, SetActorHead) },
	{ "SetActorHeadLimits", LUA_OPCODE(Lua_V2, SetActorHeadLimits) },
	{ "SetActorVisibility", LUA_OPCODE(Lua_V2, SetActorVisibility) },
	{ "SetActorFollowBoxes", LUA_OPCODE(Lua_V2, SetActorFollowBoxes) },
	{ "ActorActivateShadow", LUA_OPCODE(Lua_V2, ActorActivateShadow) },
	{ "ShutUpActor", LUA_OPCODE(Lua_V2, ShutUpActor) },
	{ "EnableActorPuck", LUA_OPCODE(Lua_V2, EnableActorPuck) },
	{ "SetActorGlobalAlpha", LUA_OPCODE(Lua_V2, SetActorGlobalAlpha) },
	{ "SetActorLocalAlpha", LUA_OPCODE(Lua_V2, SetActorLocalAlpha) },
	{ "SetActorTessellation", LUA_OPCODE(Lua_V2, SetActorTessellation) },
	{ "SetActorCollisionMode", LUA_OPCODE(Lua_V2, SetActorCollisionMode) },
	{ "SetActorCollisionScale", LUA_OPCODE(Lua_V2, SetActorCollisionScale) },
	{ "SetActorSortOrder", LUA_OPCODE(Lua_V2, SetActorSortOrder) },
	{ "GetActorSortOrder", LUA_OPCODE(Lua_V2, GetActorSortOrder) },
	{ "AttachActor", LUA_OPCODE(Lua_V2, AttachActor) },
	{ "DetachActor", LUA_OPCODE(Lua_V2, DetachActor) },
	{ "IsActorInSector", LUA_OPCODE(Lua_V2, IsActorInSector) },
	{ "GetActorSector", LUA_OPCODE(Lua_V2, GetActorSector) },
	{ "GetPointSector", LUA_OPCODE(Lua_V2, GetPointSector) },
	{ "TurnActor", LUA_OPCODE(Lua_V2, TurnActor) },
	{ "GetActorRot", LUA_OPCODE(Lua_V2, GetActorRot) },
	{ "SetActorRot", LUA_OPCODE(Lua_V2, SetActorRot) },
	{ "SetActorPitch", LUA_OPCODE(Lua_V2, SetActorPitch) },
	{ "SetActorRoll", LUA_OPCODE(Lua_V2, SetActorRoll) },
	{ "IsActorTurning", LUA_OPCODE(Lua_V2, IsActorTurning) },
	{ "PointActorAt", LUA_OPCODE(Lua_V2, PointActorAt) },
	{ "TurnActorTo", LUA_OPCODE(Lua_V2, TurnActorTo) },
	{ "PlayActorChore", LUA_OPCODE(Lua_V2, PlayActorChore) },
	{ "PlayActorChoreLooping", LUA_OPCODE(Lua_V2, PlayActorChoreLooping) },
	{ "IsActorMoving", LUA_OPCODE(Lua_V2, IsActorMoving) },
	{ "IsActorResting", LUA_OPCODE(Lua_V2, IsActorResting) },
	{ "GetActorChores", LUA_OPCODE(Lua_V2, GetActorChores) },
	{ "IsActorChoring", LUA_OPCODE(Lua_V2, IsActorChoring) },
	{ "StopActorChores", LUA_OPCODE(Lua_V2, StopActorChores) },
	{ "IsChoreValid", LUA_OPCODE(Lua_V2, IsChoreValid) },
	{ "IsChorePlaying", LUA_OPCODE(Lua_V2, IsChorePlaying) },
	{ "IsChoreLooping", LUA_OPCODE(Lua_V2, IsChoreLooping) },
	{ "SetChoreLooping", LUA_OPCODE(Lua_V2, SetChoreLooping) },
	{ "PlayChore", LUA_OPCODE(Lua_V2, PlayChore) },
	{ "FadeInChore", LUA_OPCODE(Lua_V2, FadeInChore) },
	{ "FadeOutChore", LUA_OPCODE(Lua_V2, FadeOutChore) },
	{ "StopChore", LUA_OPCODE(Lua_V2, StopChore) },
	{ "PauseChore", LUA_OPCODE(Lua_V2, PauseChore) },
	{ "AdvanceChore", LUA_OPCODE(Lua_V2, AdvanceChore) },
	{ "CompleteChore", LUA_OPCODE(Lua_V2, CompleteChore) },
	{ "LockChore", LUA_OPCODE(Lua_V2, LockChore) },
	{ "UnlockChore", LUA_OPCODE(Lua_V2, UnlockChore) },
	{ "LockChoreSet", LUA_OPCODE(Lua_V2, LockChoreSet) },
	{ "UnlockChoreSet", LUA_OPCODE(Lua_V2, UnlockChoreSet) },
	{ "EnableControl", LUA_OPCODE(Lua_V2, EnableControl) },
	{ "DisableControl", LUA_OPCODE(Lua_V2, DisableControl) },
	{ "GetControlState", LUA_OPCODE(Lua_V2, GetControlState) },
	{ "ApplyForceFeedback", LUA_OPCODE(Lua_V2, ApplyForceFeedback) },
	{ "PrintMessage", LUA_OPCODE(Lua_V2, PrintMessage) },
	{ "PrintError", LUA_OPCODE(Lua_V2, PrintError) },
	{ "PrintWarning", LUA_OPCODE(Lua_V2, PrintWarning) },
	{ "PrintDebug", LUA_OPCODE(Lua_V2, PrintDebug) },
	{ "EnableSoakText", LUA_OPCODE(Lua_V2, EnableSoakText) },
	{ "MakeCurrentSet", LUA_OPCODE(Lua_V2, MakeCurrentSet) },
	{ "LockSet", LUA_OPCODE(Lua_V2, LockSet) },
	{ "UnLockSet", LUA_OPCODE(Lua_V2, UnLockSet) },
	{ "LockBackground", LUA_OPCODE(Lua_V2, LockBackground) },
	{ "UnLockBackground", LUA_OPCODE(Lua_V2, UnLockBackground) },
	{ "MakeCurrentSetup", LUA_OPCODE(Lua_V2, MakeCurrentSetup) },
	{ "GetCurrentSetup", LUA_OPCODE(Lua_V2, GetCurrentSetup) },
	{ "NextSetup", LUA_OPCODE(Lua_V2, NextSetup) },
	{ "PreviousSetup", LUA_OPCODE(Lua_V2, PreviousSetup) },
	{ "StartMovie", LUA_OPCODE(Lua_V2, StartMovie) },
	{ "StopMovie", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "PauseMovie", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "EscapeMovie", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "IsMoviePlaying", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "PlaySound", LUA_OPCODE(Lua_V2, PlaySound) },
	{ "StopSound", LUA_OPCODE(Lua_V2, StopSound) },
	{ "StopAllSounds", LUA_OPCODE(Lua_V2, StopAllSounds) },
	{ "LoadSound", LUA_OPCODE(Lua_V2, LoadSound) },
	{ "FreeSound", LUA_OPCODE(Lua_V2, FreeSound) },
	{ "PlayLoadedSound", LUA_OPCODE(Lua_V2, PlayLoadedSound) },
	{ "IsSoundPlaying", LUA_OPCODE(Lua_V2, IsSoundPlaying) },
	{ "GetGroupVolume", LUA_OPCODE(Lua_V2, GetGroupVolume) },
	{ "SetGroupVolume", LUA_OPCODE(Lua_V2, SetGroupVolume) },
	{ "GetSoundVolume", LUA_OPCODE(Lua_V2, GetSoundVolume) },
	{ "SetSoundVolume", LUA_OPCODE(Lua_V2, SetSoundVolume) },
	{ "EnableAudioGroup", LUA_OPCODE(Lua_V2, EnableAudioGroup) },
	{ "EnableVoiceFX", LUA_OPCODE(Lua_V2, EnableVoiceFX) },
	{ "SetStereoAudio", LUA_OPCODE(Lua_V2, SetStereoAudio) },
	{ "SetAudioMixAhead", LUA_OPCODE(Lua_V2, SetAudioMixAhead) },
	{ "PlaySoundFrom", LUA_OPCODE(Lua_V2, PlaySoundFrom) },
	{ "PlayLoadedSoundFrom", LUA_OPCODE(Lua_V2, PlayLoadedSoundFrom) },
	{ "SetReverb", LUA_OPCODE(Lua_V2, SetReverb) },
	{ "UpdateSoundPosition", LUA_OPCODE(Lua_V2, UpdateSoundPosition) },
	{ "pspu", LUA_OPCODE(Lua_V2, pspu) },
	{ "dsnd", LUA_OPCODE(Lua_V2, dsnd) },
	{ "ImSetState", LUA_OPCODE(Lua_V2, ImSetState) },
	{ "ImStateHasLooped", LUA_OPCODE(Lua_V2, ImStateHasLooped) },
	{ "ImStateHasEnded", LUA_OPCODE(Lua_V2, ImStateHasEnded) },
	{ "ImPushState", LUA_OPCODE(Lua_V2, ImPushState) },
	{ "ImPopState", LUA_OPCODE(Lua_V2, ImPopState) },
	{ "ImFlushStack", LUA_OPCODE(Lua_V2, ImFlushStack) },
	{ "ImGetMillisecondPosition", LUA_OPCODE(Lua_V2, ImGetMillisecondPosition) },
	{ "KillMusicAndWait", LUA_OPCODE(Lua_V2, KillMusicAndWait) },
	{ "IsSectorActive", LUA_OPCODE(Lua_V2, IsSectorActive) },
	{ "MakeSectorActive", LUA_OPCODE(Lua_V2, MakeSectorActive) },
	{ "GetSectorOppositeEdge", LUA_OPCODE(Lua_V2, GetSectorOppositeEdge) },
	{ "GetSectorName", LUA_OPCODE(Lua_V2, GetSectorName) },
	{ "WorldToScreen", LUA_OPCODE(Lua_V2, WorldToScreen) },
	{ "OverWorldToScreen", LUA_OPCODE(Lua_V2, OverWorldToScreen) },
	{ "GetCameraPosition", LUA_OPCODE(Lua_V2, GetCameraPosition) },
	{ "SetCameraPosition", LUA_OPCODE(Lua_V2, SetCameraPosition) },
	{ "CameraLookAt", LUA_OPCODE(Lua_V2, CameraLookAt) },
	{ "GetCameraLookVector", LUA_OPCODE(Lua_V2, GetCameraLookVector) },
	{ "GetCameraYaw", LUA_OPCODE(Lua_V2, GetCameraYaw) },
	{ "GetCameraPitch", LUA_OPCODE(Lua_V2, GetCameraPitch) },
	{ "GetCameraRoll", LUA_OPCODE(Lua_V2, GetCameraRoll) },
	{ "PerSecond", LUA_OPCODE(Lua_V2, PerSecond) },
	{ "SnapToPlane", LUA_OPCODE(Lua_V2, SnapToPlane) },
	{ "GetAngleBetweenVectors", LUA_OPCODE(Lua_V2, GetAngleBetweenVectors) },
	{ "GetAngleBetweenActors", LUA_OPCODE(Lua_V2, GetAngleBetweenActors) },
	{ "TurnLightOn", LUA_OPCODE(Lua_V2, TurnLightOn) },
	{ "IsLightOn", LUA_OPCODE(Lua_V2, IsLightOn) },
	{ "SetLightPosition", LUA_OPCODE(Lua_V2, SetLightPosition) },
	{ "SetAmbientLight", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "RenderModeUser", LUA_OPCODE(Lua_V2, RenderModeUser) },
	{ "DimScreen", LUA_OPCODE(Lua_V2, DimScreen) },
	{ "DimRegion", LUA_OPCODE(Lua_V2, DimRegion) },
	{ "UndimAll", LUA_OPCODE(Lua_V2, UndimAll) },
	{ "UndimRegion", LUA_OPCODE(Lua_V2, UndimRegion) },
	{ "SetVScanOffsets", LUA_OPCODE(Lua_V2, SetVScanOffsets) },
	{ "NewLayer", LUA_OPCODE(Lua_V2, NewLayer) },
	{ "FreeLayer", LUA_OPCODE(Lua_V2, FreeLayer) },
	{ "GetLayerSortOrder", LUA_OPCODE(Lua_V2, GetLayerSortOrder) },
	{ "SetLayerSortOrder", LUA_OPCODE(Lua_V2, SetLayerSortOrder) },
	{ "GetLayerFrame", LUA_OPCODE(Lua_V2, GetLayerFrame) },
	{ "SetLayerFrame", LUA_OPCODE(Lua_V2, SetLayerFrame) },
	{ "AdvanceLayerFrame", LUA_OPCODE(Lua_V2, AdvanceLayerFrame) },
	{ "MakeTextObject", LUA_OPCODE(Lua_V2, MakeTextObject) },
	{ "KillTextObject", LUA_OPCODE(Lua_V2, KillTextObject) },
	{ "ChangeTextObject", LUA_OPCODE(Lua_V2, ChangeTextObject) },
	{ "GetTextObjectDimensions", LUA_OPCODE(Lua_V2, GetTextObjectDimensions) },
	{ "ExpireText", LUA_OPCODE(Lua_V2, ExpireText) },
	{ "PurgeText", LUA_OPCODE(Lua_V2, PurgeText) },
	{ "PushText", LUA_OPCODE(Lua_V2, PushText) },
	{ "PopText", LUA_OPCODE(Lua_V2, PopText) },
	{ "SetTextSpeed", LUA_OPCODE(Lua_V2, SetTextSpeed) },
	{ "GetTextSpeed", LUA_OPCODE(Lua_V2, GetTextSpeed) },
	{ "SetSpeechMode", LUA_OPCODE(Lua_V2, SetSpeechMode) },
	{ "GetSpeechMode", LUA_OPCODE(Lua_V2, GetSpeechMode) },
	{ "SetSayLineDefaults", LUA_OPCODE(Lua_V2, SetSayLineDefaults) },
	{ "SetActorTalkColor", LUA_OPCODE(Lua_V2, SetActorTalkColor) },
	{ "GetActorTalkColor", LUA_OPCODE(Lua_V2, GetActorTalkColor) },
	{ "SayLine", LUA_OPCODE(Lua_V2, SayLine) },
	{ "IsMessageGoing", LUA_OPCODE(Lua_V2, IsMessageGoing) },
	{ "SetTranslationMode", LUA_OPCODE(Lua_V2, SetTranslationMode) },
	{ "GetTranslationMode", LUA_OPCODE(Lua_V2, GetTranslationMode) },
	{ "LocalizeString", LUA_OPCODE(Lua_V2, LocalizeString) },
	{ "GetFontDimensions", LUA_OPCODE(Lua_V2, GetFontDimensions) },
	{ "GetTextCharPosition", LUA_OPCODE(Lua_V2, GetTextCharPosition) },
	{ "SetOffscreenTextPos", LUA_OPCODE(Lua_V2, SetOffscreenTextPos) },
	{ "MakeColor", LUA_OPCODE(Lua_V2, MakeColor) },
	{ "GetColorComponents", LUA_OPCODE(Lua_V2, GetColorComponents) },
	{ "ToggleDrawFPS", LUA_OPCODE(Lua_V2, ToggleDrawFPS) },
	{ "ToggleDebugDraw", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ToggleDrawCameras", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ToggleDrawLights", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ToggleDrawSectors", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ToggleDrawBBoxes", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ToggleDrawPerformance", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ToggleDrawActorStats", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "MemDump", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "AllocDump", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "LuaMemDump", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ReportAllResources", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "ReportLRUResources", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "GLReport", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "YawCamera", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "PitchCamera", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "RollCamera", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "GetCameraFOV", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "SetCameraFOV", LUA_OPCODE(Lua_V2, UnimplementedFunctionPs2) },
	{ "SetActorReflection", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "GetCPUSpeed", LUA_OPCODE(Lua_V2, GetCPUSpeed) },
	{ "EngineDisplay", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "SetGamma", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "ImSelectSet", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "ImGetSfxVol", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "ImGetVoiceVol", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) },
	{ "ImGetMusicVol", LUA_OPCODE(Lua_V2, ObsoleteFunctionPs2) }
};

static const struct luaL_reg monkeyEntryOpcodes[] = {
	{ "  concatfallback", LUA_OPCODE(LuaBase, concatFallback) },
	{ "  typeoverride", LUA_OPCODE(LuaBase, typeOverride) },
	{ "  dfltcontrol", LUA_OPCODE(Lua_V2, EmptyFunction) }
};

static const struct luaL_reg monkeyDemoEntryOpcodes[] = {
	{ "  concatfallback", LUA_OPCODE(LuaBase, concatFallback) },
	{ "  typeoverride", LUA_OPCODE(LuaBase, typeOverride) },
	{ "  dfltcontrol", LUA_OPCODE(Lua_V2, NoControlHandler) }
};

static const struct luaL_reg monkeyPrimitiveOpcodes[] = {
	{ "Draw3DLine", LUA_OPCODE(Lua_V2, Draw3DLine) },
	{ "DrawLine", LUA_OPCODE(Lua_V2, DrawLine) },
	{ "DrawPolygon", LUA_OPCODE(Lua_V2, DrawPolygon) },
	{ "DrawRectangle", LUA_OPCODE(Lua_V2, DrawRectangle) },
	{ "ChangePrimitive", LUA_OPCODE(Lua_V2, ChangePrimitive) },
	{ "KillPrimitive", LUA_OPCODE(Lua_V2, KillPrimitive) },
	{ "PurgePrimitiveQueue", LUA_OPCODE(Lua_V2, PurgePrimitiveQueue) }
};

Lua_V2::Lua_V2(EmiEngine *engine) : Lua_V1(true), _statsOverlayFlags(8), _saveDataSize(0), _saveWriter(nullptr), _engine(engine), _find(nullptr), _gcTimer(0.0f), _started(false) {
	for (int i = 0; i < kNumFieldRefs; i++)
		_fieldRefs[i] = 0;
}

void Lua_V2::registerMainOpcodes() {
	if (_engine->isDemo()) {
		luaL_openlib(monkeyDemoMainOpcodes, ARRAYSIZE(monkeyDemoMainOpcodes));

		return;
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		luaL_openlib(monkeyPs2MainOpcodes, ARRAYSIZE(monkeyPs2MainOpcodes));
	else
		luaL_openlib(monkeyMainOpcodes, ARRAYSIZE(monkeyMainOpcodes));
}

void Lua_V2::registerPrimitiveOpcodes() {
	luaL_openlib(monkeyPrimitiveOpcodes, ARRAYSIZE(monkeyPrimitiveOpcodes));
}

void Lua_V2::recognizeOpcodes() {
	if (_engine->isDemo())
		luaL_addlibtolist(monkeyDemoEntryOpcodes, ARRAYSIZE(monkeyDemoEntryOpcodes));
	else
		luaL_addlibtolist(monkeyEntryOpcodes, ARRAYSIZE(monkeyEntryOpcodes));
}

int Lua_V2::startup() {
	if (_started)
		return 0;

	lua_FuncIDsSetEmiDemo(_engine->isDemo());
	lua_FuncIDsSetEmiPs2(_engine->getGamePlatform() == Common::kPlatformPS2);

	if (_engine->isDemo()) {
		// The demo has no sleep_for, and its io.remove does nothing
		lua_pushnil();
		lua_setglobal("sleep_for");
		lua_register("remove", LUA_OPCODE(Lua_V2, EmptyFunction));
	}

	registerMainOpcodes();
	recognizeOpcodes();
	makeSystemTable();
	registerTypeOverride();
	registerSectorConstants();
	registerConcatFallback();

	lua_savehook = identityHook;
	lua_restorehook = identityHook;

	for (int i = 0; i < kNumFieldRefs; i++) {
		lua_pushstring(fieldNames[i]);
		_fieldRefs[i] = lua_ref(1);
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		_engine->_textScreenRect[0] = 0;
		_engine->_textScreenRect[1] = 0;
		_engine->_textScreenRect[2] = 640;
		_engine->_textScreenRect[3] = 448;
	}

	_engine->_offscreenTextRect.x = _engine->_textScreenRect[2] / 2;
	_engine->_offscreenTextRect.y = truncateToInt((double)(_engine->_textScreenRect[3] / 2) - (double)_engine->_textScreenRect[3] * (double)0.4375f);
	_engine->_offscreenTextRect.w = 0;
	_engine->_offscreenTextRect.h = 0;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		lua_SetRandomFunctions(EmiEngine::nextRandom, EmiEngine::seedRandom);
		_engine->countDemos();
	} else {
		_engine->createInputRecorder(0);
	}

	_lockedChores.clear();
	_lockedChoreSets.clear();
	_lockedRooms.clear();
	_lockedBackgrounds.clear();

	_started = true;

	return 1;
}

int Lua_V2::shutdown() {
	if (!_started)
		return 0;

	for (int i = 0; i < kNumFieldRefs; i++)
		lua_unref(_fieldRefs[i]);

	if (_find)
		FileUtilFind::disposeFind(_find);

	lua_unref(_refSystemTable);
	lua_savehook = nullptr;
	lua_restorehook = nullptr;
	lua_SetRandomFunctions(nullptr, nullptr);
	_started = false;

	return 0;
}

int Lua_V2::boot(int saveGameResume, const char *bootParam) {
	if (dofile("_system.lua"))
		return 0;

	lua_pushnil();

	if (bootParam)
		lua_pushstring(bootParam);
	else
		lua_pushnil();

	int result = lua_call("BOOT");

	applyTankControlsPs2();

	return result == 0;
}

void Lua_V2::update(float seconds) {
	double timer = _gcTimer + seconds;

	_gcTimer = (float)timer;

	if (timer > 10.0) {
		lua_collectgarbage(0);
		_gcTimer = 0.0f;
	}

	float frameTime = (float)((double)seconds * 1000.0);

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("frameTime");
	lua_pushnumber(frameTime);
	lua_settable();
	lua_endblock();

	lua_runtasks((int)frameTime);
}

int Lua_V2::dofile(const char *filename) {
	LFile *file = _engine->getFileServices()->open(filename, "rb");

	if (!file)
		return 2;

	file->seek(0, SEEK_END);

	int size = file->tell();

	file->seek(0, SEEK_SET);

	if (size < 0)
		size = 0;

	char *buffer = new char[size + 1];
	int count = file->read(buffer, size);

	buffer[count] = 0;
	_engine->getFileServices()->close(file, true);

	Common::String patchName(filename);

	patchName.replace('\\', '/');
	patchName.toLowercase();

	Common::MemoryReadStream *original = new Common::MemoryReadStream((const byte *)buffer, count);
	Common::SeekableReadStream *patched = wrapPatchedFile(original, Common::Path(patchName, '/'));
	int result;

	if (patched == original) {
		delete original;

		result = lua_dobuffer(buffer, count, filename);
	} else {
		int patchedSize = (int)patched->size();
		char *patchedBuffer = new char[patchedSize + 1];
		int patchedCount = (int)patched->read(patchedBuffer, patchedSize);

		patchedBuffer[patchedCount] = 0;
		delete patched;

		result = lua_dobuffer(patchedBuffer, patchedCount, filename);

		delete[] patchedBuffer;
	}

	delete[] buffer;

	return result;
}

bool Lua_V2::logError(const char *message) {
	LConsoleServices *console = _engine->getConsoleServices();

	if (!console)
		return false;

	console->logError("%s", message);

	return true;
}

Common::SeekableReadStream *Lua_V2::openPrefsForReading(Common::SeekableReadStream *realFile) {
	return EmiRegistry::openPrefsForReading(_engine, realFile);
}

Common::WriteStream *Lua_V2::openPrefsForWriting(Common::WriteStream *realFile) {
	return EmiRegistry::openPrefsForWriting(_engine, realFile);
}

Common::SeekableReadStream *Lua_V2::openBundleStream(const char *path) {
	LFileServices *fileServices = _engine->getFileServices();

	if (!fileServices)
		return nullptr;

	LFile *file = fileServices->open(path, "r");

	if (!file)
		return nullptr;

	file->seek(0, SEEK_END);

	int size = file->tell();

	file->seek(0, SEEK_SET);

	Common::MemoryWriteStreamDynamic data(DisposeAfterUse::NO);
	byte buffer[1024];

	while (size > 0) {
		uint count = file->read(buffer, MIN(size, 1024));

		if (!count)
			break;

		data.write(buffer, count);
		size -= count;
	}

	fileServices->close(file, true);

	return new Common::MemoryReadStream(data.getData(), data.size(), DisposeAfterUse::YES);
}

int32 Lua_V2::identityHook(int32 tag, int32 data, SaveRestoreAccess *saveFunc) {
	return data;
}

void Lua_V2::makeSystemTable() {
	lua_Object systemTable = lua_createtable();

	lua_pushobject(systemTable);
	lua_setglobal("system");

	lua_pushobject(systemTable);
	_refSystemTable = lua_ref(1);

	makeControlsTable(_refSystemTable);

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("axisHandler");

	if (_engine->isDemo())
		lua_pushcfunction(LUA_OPCODE(Lua_V2, NoControlHandler));
	else
		lua_pushcfunction(LUA_OPCODE(Lua_V2, EmptyFunction));

	lua_settable();

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("buttonHandler");

	if (_engine->isDemo())
		lua_pushcfunction(LUA_OPCODE(Lua_V2, NoControlHandler));
	else
		lua_pushcfunction(LUA_OPCODE(Lua_V2, EmptyFunction));

	lua_settable();
}

void Lua_V2::callCameraChange(int oldSetup, int newSetup) {
	if (!_started) {
		warning("Lua_V2::callCameraChange(): the system table is not ready");

		return;
	}

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("camChangeHandler");

	lua_Object handler = lua_gettable();

	if (lua_isfunction(handler)) {
		lua_pushnumber((float)oldSetup);
		lua_pushnumber((float)newSetup);
		lua_callfunction(handler);
	}

	lua_endblock();
}

void Lua_V2::callPostCameraChange(int setup) {
	if (!_started) {
		warning("Lua_V2::callPostCameraChange(): the system table is not ready");

		return;
	}

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("postCamChangeHandler");

	lua_Object handler = lua_gettable();

	if (lua_isfunction(handler)) {
		lua_pushnumber((float)setup);
		lua_callfunction(handler);
	}

	lua_endblock();
}

void Lua_V2::callCollisionHandler(Actor *first, Actor *second, float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2) {
	if (!_started) {
		warning("Lua_V2::callCollisionHandler(): the system table is not ready");

		return;
	}

	if (!first || !second) {
		warning("Lua_V2::callCollisionHandler(): missing actor");

		return;
	}

	lua_beginblock();
	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("collisionHandler");

	lua_Object handler = lua_gettable();
	lua_Object function;

	if (lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring("collisionHandler");

		function = lua_gettable();

		if (!lua_isfunction(function)) {
			logMessage(kConsoleError, "Installed collision handler handler has no member func '%s'\n", "collisionHandler");

			lua_endblock();

			return;
		}

		lua_pushobject(handler);
	} else if (lua_isfunction(handler)) {
		function = handler;
	} else {
		if (!lua_isnil(handler))
			logMessage(kConsoleError, "Installed costume marker handler is invalid.\n");

		lua_endblock();

		return;
	}

	lua_pushusertag(first->_id, MKTAG('R', 'T', 'C', 'A'));
	lua_pushusertag(second->_id, MKTAG('R', 'T', 'C', 'A'));
	lua_pushnumber(x0);
	lua_pushnumber(y0);
	lua_pushnumber(z0);
	lua_pushnumber(x1);
	lua_pushnumber(y1);
	lua_pushnumber(z1);
	lua_pushnumber(x2);
	lua_pushnumber(y2);
	lua_pushnumber(z2);
	lua_callfunction(function);
	lua_endblock();
}

void Lua_V2::typeOverride() {
	lua_Object data = lua_getparam(1);

	if (lua_isuserdata(data)) {
		int32 tag = lua_tag(data);

		if (tag == MKTAG(' ', 'T', 'E', 'S')) {
			lua_pushstring("set");
			lua_pushnumber((float)tag);
			return;
		}

		if (tag == MKTAG('R', 'T', 'C', 'A')) {
			lua_pushstring("actor");
			lua_pushnumber((float)tag);
			return;
		}
	}

	lua_Object oldType = lua_getref(_refTypeOverride);

	lua_pushobject(data);
	lua_callfunction(oldType);

	lua_Object result1 = lua_getresult(1);
	lua_Object result2 = lua_getresult(2);

	lua_pushobject(result1);
	lua_pushobject(result2);
}

void Lua_V2::concatFallback() {
	lua_Object params[2];
	char result[128];

	params[0] = lua_getparam(1);
	params[1] = lua_getparam(2);
	result[0] = 0;

	for (int i = 0; i < 2; i++) {
		lua_Object param = params[i];
		Common::String text;

		if (lua_isstring(param)) {
			text = lua_getstring(param);

			if (text.contains('%'))
				warning("Lua_V2::concatFallback(): the string contains a %% character");
		} else if (lua_isnil(param)) {
			text = "(nil)";
		} else if (lua_isuserdata(param) && lua_tag(param) == MKTAG('R', 'T', 'C', 'A')) {
			text = Common::String::format("(actor%d:)", lua_getuserdata(param));
		} else {
			lua_pushobject(params[0]);
			lua_pushobject(params[1]);
			lua_callfunction(lua_getref(_refOldConcatFallback));
			lua_pushobject(lua_getresult(1));
			return;
		}

		if (Common::strlcat(result, text.c_str(), sizeof(result)) >= sizeof(result))
			warning("Lua_V2::concatFallback(): the result has been truncated");
	}

	lua_pushstring(result);
}

void Lua_V2::UnimplementedFunction() {
	// The demo stubs are silent
	if (_engine->isDemo())
		return;

	lua_printstack(nullptr);
}

void Lua_V2::ObsoleteFunction() {
}

void Lua_V2::NoControlHandler() {
	logMessage(kConsoleDebug, "There is no control handler to take the message.\n");
}

void Lua_V2::logMessage(int level, const char *format, ...) {
	va_list args;

	va_start(args, format);
	Common::String message = Common::String::vformat(format, args);
	va_end(args);

	LConsoleServices *console = _engine->getConsoleServices();

	if (!console) {
		warning("%s", message.c_str());

		return;
	}

	switch (level) {
	case kConsoleStatus:
		console->logStatus("%s", message.c_str());

		break;
	case kConsoleWarning:
		console->logWarning("%s", message.c_str());

		break;
	case kConsoleError:
		console->logError("%s", message.c_str());

		break;
	default:
		console->logDebug("%s", message.c_str());

		break;
	}
}

bool Lua_V2::disabledFunction(int level, const char *format, lua_Object argumentObj) {
	if (_engine->getGamePlatform() != Common::kPlatformPS2)
		return false;

	const char *argument = argumentObj != LUA_NOOBJECT ? lua_getstring(argumentObj) : nullptr;

	logMessage(level, format, argument ? argument : "");
	lua_pushnil();

	return true;
}

bool Lua_V2::fileExists(const char *name) {
	LFile *file = _engine->getFileServices()->open(name, "r");

	if (!file)
		return false;

	_engine->getFileServices()->close(file, true);

	return true;
}

void Lua_V2::FindFileOrSwapCDs() {
	// Only the demo uses the file search functions
	if (!_engine->isDemo())
		return;

	lua_Object captionObj = lua_getparam(1);
	lua_Object textObj = lua_getparam(2);
	lua_Object nameObj = lua_getparam(3);

	if (!lua_isstring(captionObj) || !lua_isstring(textObj) || !lua_isstring(nameObj)) {
		logMessage(kConsoleStatus, "All three arguments to FindFileOrSwapCDs must be strings.\n");

		return;
	}

	const char *caption = lua_getstring(captionObj);
	const char *text = lua_getstring(textObj);
	const char *name = lua_getstring(nameObj);

	if (fileExists(name)) {
		lua_pushnumber(1.0f);
		lua_pushnil();

		return;
	}

	_engine->setMusicState(0, 0);
	_engine->getSounds()->stopAllSounds();

	Common::String message(caption);

	if (!message.empty())
		message += "\n\n";

	message += text;

	for (;;) {
		GUI::MessageDialog dialog(Common::U32String(message), _("OK"), _("Cancel"));

		if (dialog.runModal() != GUI::kMessageOK || _engine->shouldQuit()) {
			lua_pushnil();
			lua_pushnil();

			return;
		}

		if (fileExists(name)) {
			lua_pushnumber(1.0f);
			lua_pushnumber(1.0f);

			return;
		}
	}
}

void Lua_V2::CheckForFile() {
	if (!_engine->isDemo())
		return;

	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj)) {
		logMessage(kConsoleStatus, "Argument to CheckForFile must be a string!\n");

		return;
	}

	if (fileExists(lua_getstring(nameObj)))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V2::FindFileOnAnyCD() {
	if (!_engine->isDemo())
		return;

	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj)) {
		logMessage(kConsoleDebug, "FindFileOnAnyCD: argument must be a string filename.\n");
		lua_pushnil();

		return;
	}

	if (!lua_getstring(nameObj)) {
		logMessage(kConsoleDebug, "FindFileOnAnyCD: argument is invalid string.\n");
		lua_pushnil();

		return;
	}

	// There are no CD-ROM drives to search
	lua_pushnil();
}

void Lua_V2::EmptyFunction() {
}

} // end of namespace Grim
