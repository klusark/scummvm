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

#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/grim.h"
#include "engines/grim/remastered/commentary.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/lua_remastered.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/stdcontrol.h"

namespace Grim {

enum {
	kCanvasWidth = 1920,
	kCanvasHeight = 1080
};

static struct luaL_reg remasteredMainOpcodes[] = {
	{ "EnginePause", LUA_OPCODE(Lua_V1, EnginePause) },
	{ "EngineResume", LUA_OPCODE(Lua_V1, EngineResume) },
	{ "EngineDisplay", LUA_OPCODE(Lua_V1, EngineDisplay) },
	{ "PrintStack", LUA_OPCODE(Lua_Remastered, PrintStack) },
	{ "GetPlatform", LUA_OPCODE(Lua_V1, GetPlatform) },
	{ "SearchForFileOrSwapCDs", LUA_OPCODE(Lua_V1, SearchForFileOrSwapCDs) },
	{ "CheckForFile", LUA_OPCODE(Lua_V1, CheckForFile) },
	{ "New", LUA_OPCODE(Lua_Remastered, New) },
	{ "Load", LUA_OPCODE(Lua_Remastered, Load) },
	{ "Save", LUA_OPCODE(Lua_Remastered, Save) },
	{ "Erase", LUA_OPCODE(Lua_Remastered, Erase) },
	{ "GetSaveStatus", LUA_OPCODE(Lua_Remastered, GetSaveStatus) },
	{ "StartCheckOfCrossSaveStatus", LUA_OPCODE(Lua_V1, StartCheckOfCrossSaveStatus) },
	{ "GetCrossSaveStatus", LUA_OPCODE(Lua_V1, GetCrossSaveStatus) },
	{ "UnlockAchievement", LUA_OPCODE(Lua_V1, UnlockAchievement) },
	{ "ResetAchievements", LUA_OPCODE(Lua_V1, ResetAchievements) },
	{ "AreAchievementsInstalled", LUA_OPCODE(Lua_V1, AreAchievementsInstalled) },
	{ "GlobalSaveResolved", LUA_OPCODE(Lua_V1, GlobalSaveResolved) },
	{ "UnlockCutscene", LUA_OPCODE(Lua_V1, UnlockCutscene) },
	{ "IsCutsceneUnlocked", LUA_OPCODE(Lua_V1, IsCutsceneUnlocked) },
	{ "UnlockConcept", LUA_OPCODE(Lua_V1, UnlockConcept) },
	{ "IsConceptUnlocked", LUA_OPCODE(Lua_V1, IsConceptUnlocked) },
	{ "SetActorColormap", LUA_OPCODE(Lua_V1, SetActorColormap) },
	{ "GetActorCostume", LUA_OPCODE(Lua_V1, GetActorCostume) },
	{ "SetActorCostume", LUA_OPCODE(Lua_V1, SetActorCostume) },
	{ "GetActorScale", LUA_OPCODE(Lua_V1, GetActorScale) },
	{ "SetActorScale", LUA_OPCODE(Lua_V1, SetActorScale) },
	{ "GetActorTimeScale", LUA_OPCODE(Lua_V1, GetActorTimeScale) },
	{ "SetActorTimeScale", LUA_OPCODE(Lua_V1, SetActorTimeScale) },
	{ "GetActorNodeLocation", LUA_OPCODE(Lua_V1, GetActorNodeLocation) },
	{ "SetActorWalkChore", LUA_OPCODE(Lua_V1, SetActorWalkChore) },
	{ "SetActorTurnChores", LUA_OPCODE(Lua_V1, SetActorTurnChores) },
	{ "SetActorRestChore", LUA_OPCODE(Lua_V1, SetActorRestChore) },
	{ "SetActorMumblechore", LUA_OPCODE(Lua_V1, SetActorMumblechore) },
	{ "SetActorTalkChore", LUA_OPCODE(Lua_V1, SetActorTalkChore) },
	{ "SetActorWalkRate", LUA_OPCODE(Lua_V1, SetActorWalkRate) },
	{ "GetActorWalkRate", LUA_OPCODE(Lua_V1, GetActorWalkRate) },
	{ "SetActorTurnRate", LUA_OPCODE(Lua_V1, SetActorTurnRate) },
	{ "GetActorTurnRate", LUA_OPCODE(Lua_V1, GetActorTurnRate) },
	{ "SetSelectedActor", LUA_OPCODE(Lua_V1, SetSelectedActor) },
	{ "LoadActor", LUA_OPCODE(Lua_V1, LoadActor) },
	{ "GetActorPos", LUA_OPCODE(Lua_V1, GetActorPos) },
	{ "GetActorRect", LUA_OPCODE(Lua_V1, GetActorRect) },
	{ "GetActorPuckVector", LUA_OPCODE(Lua_V1, GetActorPuckVector) },
	{ "GetActorYawToPoint", LUA_OPCODE(Lua_V1, GetActorYawToPoint) },
	{ "SetActorReflection", LUA_OPCODE(Lua_V1, SetActorReflection) },
	{ "SetActorOffsetYaw", LUA_OPCODE(Lua_V1, SetActorOffsetYaw) },
	{ "PutActorAtInterest", LUA_OPCODE(Lua_V1, PutActorAtInterest) },
	{ "PutActorAtOrigin", LUA_OPCODE(Lua_Remastered, NoOpPutActorAtOrigin) },
	{ "PutActorAt", LUA_OPCODE(Lua_V1, PutActorAt) },
	{ "PutActorInSet", LUA_OPCODE(Lua_V1, PutActorInSet) },
	{ "GetClippedPos", LUA_OPCODE(Lua_Remastered, NoOpGetClippedPos) },
	{ "WalkActorVector", LUA_OPCODE(Lua_V1, WalkActorVector) },
	{ "WalkActorForward", LUA_OPCODE(Lua_V1, WalkActorForward) },
	{ "DriveActorTo", LUA_OPCODE(Lua_Remastered, NoOpDriveActorTo) },
	{ "WalkActorTo", LUA_OPCODE(Lua_V1, WalkActorTo) },
	{ "WalkActorToAvoiding", LUA_OPCODE(Lua_V1, WalkActorToAvoiding) },
	{ "WalkActorToInterest", LUA_OPCODE(Lua_Remastered, NoOpWalkActorToInterest) },
	{ "WalkActorToMouseTarget", LUA_OPCODE(Lua_V1, WalkActorToMouseTarget) },
	{ "ActorLookAt", LUA_OPCODE(Lua_V1, ActorLookAt) },
	{ "IsActorLooking", LUA_OPCODE(Lua_V1, IsActorLooking) },
	{ "SetActorLookRate", LUA_OPCODE(Lua_V1, SetActorLookRate) },
	{ "GetActorLookRate", LUA_OPCODE(Lua_V1, GetActorLookRate) },
	{ "CanActorSee", LUA_OPCODE(Lua_V1, CanActorSee) },
	{ "GetVisibleThings", LUA_OPCODE(Lua_V1, GetVisibleThings) },
	{ "GetCameraActor", LUA_OPCODE(Lua_V1, GetCameraActor) },
	{ "SetActorHead", LUA_OPCODE(Lua_V1, SetActorHead) },
	{ "SetActorVisibility", LUA_OPCODE(Lua_V1, SetActorVisibility) },
	{ "SetActorFollowBoxes", LUA_OPCODE(Lua_V1, SetActorFollowBoxes) },
	{ "ShutUpActor", LUA_OPCODE(Lua_V1, ShutUpActor) },
	{ "SetActorFrustrumCull", LUA_OPCODE(Lua_V1, SetActorFrustrumCull) },
	{ "SetActorBWMode", LUA_OPCODE(Lua_V1, SetActorBWMode) },
	{ "SetActorHKHackMode", LUA_OPCODE(Lua_V1, SetActorHKHackMode) },
	{ "PathfinderMode", LUA_OPCODE(Lua_V1, PathfinderMode) },
	{ "IsActorInSector", LUA_OPCODE(Lua_V1, IsActorInSector) },
	{ "GetActorSector", LUA_OPCODE(Lua_V1, GetActorSector) },
	{ "IsPointInSector", LUA_OPCODE(Lua_V1, IsPointInSector) },
	{ "GetPointSector", LUA_OPCODE(Lua_V1, GetPointSector) },
	{ "TurnActor", LUA_OPCODE(Lua_V1, TurnActor) },
	{ "GetActorRot", LUA_OPCODE(Lua_V1, GetActorRot) },
	{ "SetActorRot", LUA_OPCODE(Lua_V1, SetActorRot) },
	{ "SetActorPitch", LUA_OPCODE(Lua_V1, SetActorPitch) },
	{ "SetActorRoll", LUA_OPCODE(Lua_V1, SetActorRoll) },
	{ "IsActorTurning", LUA_OPCODE(Lua_V1, IsActorTurning) },
	{ "PlayActorChore", LUA_OPCODE(Lua_V1, PlayActorChore) },
	{ "PlayActorChoreLooping", LUA_OPCODE(Lua_V1, PlayActorChoreLooping) },
	{ "StopActorChore", LUA_OPCODE(Lua_V1, StopActorChore) },
	{ "CompleteActorChore", LUA_OPCODE(Lua_V1, CompleteActorChore) },
	{ "IsActorMoving", LUA_OPCODE(Lua_V1, IsActorMoving) },
	{ "IsActorChoring", LUA_OPCODE(Lua_V1, IsActorChoring) },
	{ "IsActorResting", LUA_OPCODE(Lua_V1, IsActorResting) },
	{ "SetActorChoreLooping", LUA_OPCODE(Lua_V1, SetActorChoreLooping) },
	{ "GetActorChores", LUA_OPCODE(Lua_V1, GetActorChores) },
	{ "GetActorLoopingChores", LUA_OPCODE(Lua_V1, GetActorLoopingChores) },
	{ "GetActorCostumeDepth", LUA_OPCODE(Lua_V1, GetActorCostumeDepth) },
	{ "WorldToScreen", LUA_OPCODE(Lua_V1, WorldToScreen) },
	{ "ScreenToWorld", LUA_OPCODE(Lua_V1, ScreenToWorld) },
	{ "exit", LUA_OPCODE(Lua_V1, Exit) },
	{ "FunctionName", LUA_OPCODE(Lua_V1, FunctionName) },
	{ "EnableDebugKeys", LUA_OPCODE(Lua_V1, EnableDebugKeys) },
	{ "LockFont", LUA_OPCODE(Lua_V1, LockFont) },
	{ "UnLockFont", LUA_OPCODE(Lua_Remastered, NoOpUnLockFont) },
	{ "LockSound", LUA_OPCODE(Lua_Remastered, NoOpLockSound) },
	{ "UnlockSound", LUA_OPCODE(Lua_Remastered, NoOpUnlockSound) },
	{ "LockCostume", LUA_OPCODE(Lua_Remastered, NoOpLockCostume) },
	{ "UnlockCostume", LUA_OPCODE(Lua_Remastered, NoOpUnlockCostume) },
	{ "EnableControl", LUA_OPCODE(Lua_V1, EnableControl) },
	{ "DisableControl", LUA_OPCODE(Lua_V1, DisableControl) },
	{ "GetControlState", LUA_OPCODE(Lua_V1, GetControlState) },
	{ "PrintMessage", LUA_OPCODE(Lua_V1, PrintMessage) },
	{ "PrintError", LUA_OPCODE(Lua_V1, PrintError) },
	{ "PrintWarning", LUA_OPCODE(Lua_V1, PrintWarning) },
	{ "PrintDebug", LUA_OPCODE(Lua_V1, PrintDebug) },
	{ "MakeCurrentSet", LUA_OPCODE(Lua_V1, MakeCurrentSet) },
	{ "LockSet", LUA_OPCODE(Lua_V1, LockSet) },
	{ "UnLockSet", LUA_OPCODE(Lua_V1, UnLockSet) },
	{ "MakeCurrentSetup", LUA_OPCODE(Lua_V1, MakeCurrentSetup) },
	{ "GetCurrentSetup", LUA_OPCODE(Lua_V1, GetCurrentSetup) },
	{ "NextSetup", LUA_OPCODE(Lua_V1, NextSetup) },
	{ "PreviousSetup", LUA_OPCODE(Lua_V1, PreviousSetup) },
	{ "StartFullscreenMovie", LUA_OPCODE(Lua_V1, StartFullscreenMovie) },
	{ "IsFullscreenMoviePlaying", LUA_OPCODE(Lua_V1, IsFullscreenMoviePlaying) },
	{ "StartMovie", LUA_OPCODE(Lua_V1, StartMovie) },
	{ "StopMovie", LUA_OPCODE(Lua_V1, StopMovie) },
	{ "PauseMovie", LUA_OPCODE(Lua_V1, PauseMovie) },
	{ "IsMoviePlaying", LUA_OPCODE(Lua_V1, IsMoviePlaying) },
	{ "PlaySound", LUA_OPCODE(Lua_Remastered, NoOpPlaySound) },
	{ "PlaySoundAt", LUA_OPCODE(Lua_Remastered, NoOpPlaySoundAt) },
	{ "PlaySoundAttached", LUA_OPCODE(Lua_Remastered, NoOpPlaySoundAttached) },
	{ "IsSoundPlaying", LUA_OPCODE(Lua_Remastered, NoOpIsSoundPlaying) },
	{ "SetSoundPosition", LUA_OPCODE(Lua_V1, SetSoundPosition) },
	{ "FindSaveGames", LUA_OPCODE(Lua_Remastered, FindSaveGames) },
	{ "InitiateFindSaveGames", LUA_OPCODE(Lua_Remastered, InitiateFindSaveGames) },
	{ "GetFindSaveGameStatus", LUA_OPCODE(Lua_Remastered, GetFindSaveGameStatus) },
	{ "FileFindFirst", LUA_OPCODE(Lua_V1, FileFindFirst) },
	{ "FileFindNext", LUA_OPCODE(Lua_V1, FileFindNext) },
	{ "FileFindDispose", LUA_OPCODE(Lua_V1, FileFindDispose) },
	{ "InputDialog", LUA_OPCODE(Lua_V1, InputDialog) },
	{ "QueryDialog", LUA_OPCODE(Lua_V1, QueryDialog) },
	{ "WriteRegistryValue", LUA_OPCODE(Lua_V1, WriteRegistryValue) },
	{ "ReadRegistryValue", LUA_OPCODE(Lua_V1, ReadRegistryValue) },
	{ "ReadRegistryIntValue", LUA_OPCODE(Lua_V1, ReadRegistryIntValue) },
	{ "SaveRegistryToDisk", LUA_OPCODE(Lua_V1, SaveRegistryToDisk) },
	{ "GetSectorVertices", LUA_OPCODE(Lua_Remastered, NoOpGetSectorVertices) },
	{ "GetSectorOppositeEdge", LUA_OPCODE(Lua_V1, GetSectorOppositeEdge) },
	{ "IsSectorActive", LUA_OPCODE(Lua_V1, IsSectorActive) },
	{ "MakeSectorActive", LUA_OPCODE(Lua_V1, MakeSectorActive) },
	{ "PreRender", LUA_OPCODE(Lua_V1, PreRender) },
	{ "SpewStartup", LUA_OPCODE(Lua_V1, SpewStartup) },
	{ "SpewShutdown", LUA_OPCODE(Lua_V1, SpewShutdown) },
	{ "GetCurrentScript", LUA_OPCODE(Lua_V1, GetCurrentScript) },
	{ "PrintActorCostumes", LUA_OPCODE(Lua_V1, PrintActorCostumes) },
	{ "PushActorCostume", LUA_OPCODE(Lua_V1, PushActorCostume) },
	{ "PopActorCostume", LUA_OPCODE(Lua_V1, PopActorCostume) },
	{ "LoadCostume", LUA_OPCODE(Lua_V1, LoadCostume) },
	{ "GetPhysicalBytes", LUA_OPCODE(Lua_V1, GetPhysicalBytes) },
	{ "SetMemoryUsage", LUA_OPCODE(Lua_V1, SetMemoryUsage) },
	{ "RotateVector", LUA_OPCODE(Lua_V1, RotateVector) },
	{ "GetCameraPosition", LUA_OPCODE(Lua_V1, GetCameraPosition) },
	{ "SetCameraPosition", LUA_OPCODE(Lua_V1, SetCameraPosition) },
	{ "GetCameraInterest", LUA_OPCODE(Lua_V1, GetCameraInterest) },
	{ "SetCameraInterest", LUA_OPCODE(Lua_V1, SetCameraInterest) },
	{ "GetCameraFOV", LUA_OPCODE(Lua_V1, GetCameraFOV) },
	{ "SetCameraFOV", LUA_OPCODE(Lua_V1, SetCameraFOV) },
	{ "GetCameraRoll", LUA_OPCODE(Lua_V1, GetCameraRoll) },
	{ "SetCameraRoll", LUA_OPCODE(Lua_V1, SetCameraRoll) },
	{ "GetCameraLookVector", LUA_OPCODE(Lua_V1, GetCameraLookVector) },
	{ "EnableBackgroundRender", LUA_OPCODE(Lua_V1, EnableBackgroundRender) },
	{ "PointActorAt", LUA_OPCODE(Lua_V1, PointActorAt) },
	{ "TurnActorTo", LUA_OPCODE(Lua_V1, TurnActorTo) },
	{ "PerSecond", LUA_OPCODE(Lua_V1, PerSecond) },
	{ "SnapToPlane", LUA_OPCODE(Lua_V1, SnapToPlane) },
	{ "GetAngleBetweenVectors", LUA_OPCODE(Lua_V1, GetAngleBetweenVectors) },
	{ "GetAngleBetweenActors", LUA_OPCODE(Lua_V1, GetAngleBetweenActors) },
	{ "SetAmbientLight", LUA_OPCODE(Lua_V1, SetAmbientLight) },
	{ "FindLight", LUA_OPCODE(Lua_V1, FindLight) },
	{ "TurnLightOn", LUA_OPCODE(Lua_V1, TurnLightOn) },
	{ "IsLightOn", LUA_OPCODE(Lua_V1, IsLightOn) },
	{ "SetLightType", LUA_OPCODE(Lua_V1, SetLightType) },
	{ "GetLightType", LUA_OPCODE(Lua_V1, GetLightType) },
	{ "SetLightPosition", LUA_OPCODE(Lua_V1, SetLightPosition) },
	{ "GetLightPosition", LUA_OPCODE(Lua_V1, GetLightPosition) },
	{ "SetLightRotation", LUA_OPCODE(Lua_V1, SetLightRotation) },
	{ "GetLightRotation", LUA_OPCODE(Lua_V1, GetLightRotation) },
	{ "SetLightAngles", LUA_OPCODE(Lua_V1, SetLightAngles) },
	{ "GetLightAngles", LUA_OPCODE(Lua_V1, GetLightAngles) },
	{ "SetLightIntensity", LUA_OPCODE(Lua_V1, SetLightIntensity) },
	{ "GetLightIntensity", LUA_OPCODE(Lua_V1, GetLightIntensity) },
	{ "PointLightAt", LUA_OPCODE(Lua_V1, PointLightAt) },
	{ "LightMgrStartup", LUA_OPCODE(Lua_V1, LightMgrStartup) },
	{ "LightMgrShutdown", LUA_OPCODE(Lua_V1, LightMgrShutdown) },
	{ "LightMgrSetChange", LUA_OPCODE(Lua_V1, LightMgrSetChange) },
	{ "ImStartRecording", LUA_OPCODE(Lua_V1, ImStartRecording) },
	{ "ImStopRecording", LUA_OPCODE(Lua_V1, ImStopRecording) },
	{ "ImStartSound", LUA_OPCODE(Lua_V1, ImStartSound) },
	{ "play", LUA_OPCODE(Lua_V1, ImStartSound) },
	{ "ImStopSound", LUA_OPCODE(Lua_V1, ImStopSound) },
	{ "ImStopAllSounds", LUA_OPCODE(Lua_V1, ImStopAllSounds) },
	{ "quiet", LUA_OPCODE(Lua_V1, ImStopAllSounds) },
	{ "ImGetParam", LUA_OPCODE(Lua_V1, ImGetParam) },
	{ "ImSetParam", LUA_OPCODE(Lua_V1, ImSetParam) },
	{ "ImFadeParam", LUA_OPCODE(Lua_V1, ImFadeParam) },
	{ "ImGetMasterVol", LUA_OPCODE(Lua_V1, ImGetMasterVol) },
	{ "ImSetMasterVol", LUA_OPCODE(Lua_V1, ImSetMasterVol) },
	{ "ImGetSfxVol", LUA_OPCODE(Lua_V1, ImGetSfxVol) },
	{ "ImSetSfxVol", LUA_OPCODE(Lua_V1, ImSetSfxVol) },
	{ "ImGetVoiceVol", LUA_OPCODE(Lua_V1, ImGetVoiceVol) },
	{ "ImSetVoiceVol", LUA_OPCODE(Lua_V1, ImSetVoiceVol) },
	{ "ImGetMusicVol", LUA_OPCODE(Lua_V1, ImGetMusicVol) },
	{ "ImSetMusicVol", LUA_OPCODE(Lua_V1, ImSetMusicVol) },
	{ "ImGetCommentaryVol", LUA_OPCODE(Lua_V1, ImGetCommentaryVol) },
	{ "ImSetCommentaryVol", LUA_OPCODE(Lua_V1, ImSetCommentaryVol) },
	{ "ImStartVoice", LUA_OPCODE(Lua_V1, ImStartVoice) },
	{ "ImStopVoice", LUA_OPCODE(Lua_V1, ImStopVoice) },
	{ "ImSetState", LUA_OPCODE(Lua_V1, ImSetState) },
	{ "ImSetSequence", LUA_OPCODE(Lua_V1, ImSetSequence) },
	{ "ImSetAttribute", LUA_OPCODE(Lua_V1, ImSetAttribute) },
	{ "ImSetCuePoint", LUA_OPCODE(Lua_V1, ImSetCuePoint) },
	{ "ImPause", LUA_OPCODE(Lua_V1, ImPause) },
	{ "ImResume", LUA_OPCODE(Lua_V1, ImResume) },
	{ "vfx", LUA_OPCODE(Lua_V1, ImSetVoiceEffect) },
	{ "ImSetVoiceEffect", LUA_OPCODE(Lua_V1, ImSetVoiceEffect) },
	{ "ImGetMemoryFootprint", LUA_OPCODE(Lua_V1, ImGetMemoryFootprint) },
	{ "ImGetSoundCacheSize", LUA_OPCODE(Lua_V1, ImGetSoundCacheSize) },
	{ "LoadBundle", LUA_OPCODE(Lua_Remastered, NoOpLoadBundle) },
	{ "UnloadBundle", LUA_OPCODE(Lua_Remastered, NoOpUnloadBundle) },
	{ "SetGamma", LUA_OPCODE(Lua_V1, SetGamma) },
	{ "SetResolutionScaling", LUA_OPCODE(Lua_V1, SetResolutionScaling) },
	{ "SetAdvancedLighting", LUA_OPCODE(Lua_V1, SetAdvancedLighting) },
	{ "SetActorWalkDominate", LUA_OPCODE(Lua_V1, SetActorWalkDominate) },
	{ "SetActorConstrain", LUA_OPCODE(Lua_V1, SetActorConstrain) },
	{ "RenderModeUser", LUA_OPCODE(Lua_V1, RenderModeUser) },
	{ "SetGameRenderMode", LUA_OPCODE(Lua_V1, SetGameRenderMode) },
	{ "GetGameRenderMode", LUA_OPCODE(Lua_V1, GetGameRenderMode) },
	{ "ForceRefresh", LUA_OPCODE(Lua_V1, ForceRefresh) },
	{ "DimScreen", LUA_OPCODE(Lua_V1, DimScreen) },
	{ "DimRegion", LUA_OPCODE(Lua_V1, DimRegion) },
	{ "CleanBuffer", LUA_OPCODE(Lua_V1, CleanBuffer) },
	{ "Display", LUA_OPCODE(Lua_V1, Display) },
	{ "SetSpeechMode", LUA_OPCODE(Lua_V1, SetSpeechMode) },
	{ "GetSpeechMode", LUA_OPCODE(Lua_V1, GetSpeechMode) },
	{ "SetShadowColor", LUA_OPCODE(Lua_V1, SetShadowColor) },
	{ "ActivateActorShadow", LUA_OPCODE(Lua_V1, ActivateActorShadow) },
	{ "ActorShadow", LUA_OPCODE(Lua_V1, ActorShadow) },
	{ "SetActorShadowPlane", LUA_OPCODE(Lua_V1, SetActorShadowPlane) },
	{ "SetActorShadowPoint", LUA_OPCODE(Lua_V1, SetActorShadowPoint) },
	{ "SetActiveShadow", LUA_OPCODE(Lua_V1, SetActiveShadow) },
	{ "ActorDistToShadowPlane", LUA_OPCODE(Lua_V1, ActorDistToShadowPlane) },
	{ "KillActorShadows", LUA_OPCODE(Lua_V1, KillActorShadows) },
	{ "AddShadowPlane", LUA_OPCODE(Lua_V1, AddShadowPlane) },
	{ "SetActorShadowValid", LUA_OPCODE(Lua_V1, SetActorShadowValid) },
	{ "NewObjectState", LUA_OPCODE(Lua_V1, NewObjectState) },
	{ "FreeObjectState", LUA_OPCODE(Lua_V1, FreeObjectState) },
	{ "SetObjectType", LUA_OPCODE(Lua_V1, SetObjectType) },
	{ "SendObjectToBack", LUA_OPCODE(Lua_V1, SendObjectToBack) },
	{ "SendObjectToFront", LUA_OPCODE(Lua_V1, SendObjectToFront) },
	{ "ActorToClean", LUA_OPCODE(Lua_V1, ActorToClean) },
	{ "FlushControls", LUA_OPCODE(Lua_V1, FlushControls) },
	{ "SetActorCollisionMode", LUA_OPCODE(Lua_V1, SetActorCollisionMode) },
	{ "SetActorCollisionScale", LUA_OPCODE(Lua_V1, SetActorCollisionScale) },
	{ "MarkDirtyRect", LUA_OPCODE(Lua_V1, MarkDirtyRect) },
	{ "CacheRect", LUA_OPCODE(Lua_V1, CacheRect) },
	{ "RestoreCacheRect", LUA_OPCODE(Lua_V1, RestoreCacheRect) },
	{ "ResReport", LUA_OPCODE(Lua_V1, ResReport) },
	{ "HardwareAccelerated", LUA_OPCODE(Lua_V1, HardwareAccelerated) },
	{ "SetActorClipActive", LUA_OPCODE(Lua_V1, SetActorClipActive) },
	{ "SetActorClipPlane", LUA_OPCODE(Lua_V1, SetActorClipPlane) },
	{ "FadeOutChore", LUA_OPCODE(Lua_V1, FadeOutChore) },
	{ "FadeInChore", LUA_OPCODE(Lua_V1, FadeInChore) },
	{ "IrisDown", LUA_OPCODE(Lua_V1, IrisDown) },
	{ "IrisUp", LUA_OPCODE(Lua_V1, IrisUp) },
	{ "IrisComplete", LUA_OPCODE(Lua_V1, IrisComplete) },
	{ "IrisClear", LUA_OPCODE(Lua_V1, IrisClear) },
	{ "DialogLogGetLineCount", LUA_OPCODE(Lua_V1, DialogLogGetLineCount) },
	{ "DialogLogGetLine", LUA_OPCODE(Lua_V1, DialogLogGetLine) },
	{ "DialogLogAddLine", LUA_OPCODE(Lua_V1, DialogLogAddLine) },
	{ "ScreenShot", LUA_OPCODE(Lua_V1, ScreenShot) },
	{ "SaveScreen", LUA_OPCODE(Lua_V1, SaveScreen) },
	{ "GetSaveGameImage", LUA_OPCODE(Lua_V1, GetSaveGameImage) },
	{ "GetImage", LUA_OPCODE(Lua_V1, GetImage) },
	{ "FreeImage", LUA_OPCODE(Lua_V1, FreeImage) },
	{ "BlastImage", LUA_OPCODE(Lua_V1, BlastImage) },
	{ "BlastRect", LUA_OPCODE(Lua_V1, BlastRect) },
	{ "SubmitSaveGameData", LUA_OPCODE(Lua_V1, SubmitSaveGameData) },
	{ "GetSaveGameData", LUA_OPCODE(Lua_V1, GetSaveGameData) },
	{ "SetTextSpeed", LUA_OPCODE(Lua_V1, SetTextSpeed) },
	{ "GetTextSpeed", LUA_OPCODE(Lua_V1, GetTextSpeed) },
	{ "FindFileOnAnyCD", LUA_OPCODE(Lua_V1, FindFileOnAnyCD) },
	{ "DetachFromResources", LUA_OPCODE(Lua_V1, DetachFromResources) },
	{ "AttachToResources", LUA_OPCODE(Lua_V1, AttachToResources) },
	{ "SetSoundParameters", LUA_OPCODE(Lua_V1, SetSoundParameters) },
	{ "GetSoundParameters", LUA_OPCODE(Lua_V1, GetSoundParameters) },
	{ "TEST", LUA_OPCODE(Lua_V1, TEST) },
	{ "ActorPuckOrient", LUA_OPCODE(Lua_V1, ActorPuckOrient) },
	{ "GetSystemTime", LUA_OPCODE(Lua_V1, GetSystemTime) },
	{ "GetTickCount", LUA_OPCODE(Lua_V1, GetTickCount) },
	{ "ActorVoiceIs3D", LUA_OPCODE(Lua_V1, ActorVoiceIs3D) },
	{ "JustLoaded", LUA_OPCODE(Lua_V1, JustLoaded) },
	{ "DumpMemoryLog", LUA_OPCODE(Lua_V1, DumpMemoryLog) },
	{ "ResetTextures", LUA_OPCODE(Lua_V1, ResetTextures) },
	{ "BuildCache", LUA_OPCODE(Lua_V1, BuildCache) },
	{ "RestoreCache", LUA_OPCODE(Lua_V1, RestoreCache) },
	{ "ShrinkBoxes", LUA_OPCODE(Lua_V1, ShrinkBoxes) },
	{ "UnShrinkBoxes", LUA_OPCODE(Lua_V1, UnShrinkBoxes) },
	{ "GetShrinkPos", LUA_OPCODE(Lua_V1, GetShrinkPos) },
	{ "NukeResources", LUA_OPCODE(Lua_V1, NukeResources) },
	{ "SetActorInvClipNode", LUA_OPCODE(Lua_V1, SetActorInvClipNode) },
	{ "GetDiskFreeSpace", LUA_OPCODE(Lua_V1, GetDiskFreeSpace) },
	{ "SetWalkSystemFadeTime", LUA_OPCODE(Lua_V1, SetWalkSystemFadeTime) },
	{ "SaveIMuse", LUA_OPCODE(Lua_V1, SaveIMuse) },
	{ "RestoreIMuse", LUA_OPCODE(Lua_V1, RestoreIMuse) },
	{ "GetMemoryUsage", LUA_OPCODE(Lua_V1, GetMemoryUsage) },
	{ "SetLanguage", LUA_OPCODE(Lua_V1, SetLanguage) },
	{ "GetLanguage", LUA_OPCODE(Lua_V1, GetLanguage) },
	{ "GetLanguageCount", LUA_OPCODE(Lua_V1, GetLanguageCount) },
	{ "Is3DHardwareEnabled", LUA_OPCODE(Lua_Remastered, Is3DHardwareEnabled) },
	{ "SetMouseSpeedScale", LUA_OPCODE(Lua_Remastered, SetMouseSpeedScale) },
	{ "SetKeyMappingMode", LUA_OPCODE(Lua_Remastered, SetKeyMappingMode) },
	{ "ResetKeyMappingToDefault", LUA_OPCODE(Lua_Remastered, ResetKeyMappingToDefault) },
	{ "GetRemappedKeyName", LUA_OPCODE(Lua_Remastered, GetRemappedKeyName) },
	{ "GetRemappedKeyHint", LUA_OPCODE(Lua_Remastered, GetRemappedKeyHint) },
	{ "LoadRemappedKeys", LUA_OPCODE(Lua_V1, LoadRemappedKeys) },
	{ "SaveRemappedKeys", LUA_OPCODE(Lua_V1, SaveRemappedKeys) }
};

static struct luaL_reg remasteredCommentaryOpcodes[] = {
	{ "SetCommentary", LUA_OPCODE(Lua_Remastered, SetCommentary) },
	{ "ClearCommentary", LUA_OPCODE(Lua_Remastered, ClearCommentary) },
	{ "PlayCurrentCommentary", LUA_OPCODE(Lua_Remastered, PlayCurrentCommentary) },
	{ "EnableCommentary", LUA_OPCODE(Lua_Remastered, EnableCommentary) },
	{ "StopCommentaryImmediately", LUA_OPCODE(Lua_Remastered, StopCommentaryImmediately) },
	{ "HasHeardCommentary", LUA_OPCODE(Lua_Remastered, HasHeardCommentary) },
	{ "IsPlayingCommentary", LUA_OPCODE(Lua_Remastered, IsPlayingCommentary) }
};

static struct luaL_reg remasteredOverlayOpcodes[] = {
	{ "OverlayCreate", LUA_OPCODE(Lua_Remastered, OverlayCreate) },
	{ "OverlayMove", LUA_OPCODE(Lua_Remastered, OverlayMove) },
	{ "OverlaySetLayer", LUA_OPCODE(Lua_Remastered, OverlaySetLayer) },
	{ "OverlayDestroy", LUA_OPCODE(Lua_Remastered, OverlayDestroy) },
	{ "OverlayDimensions", LUA_OPCODE(Lua_Remastered, OverlayDimensions) },
	{ "OverlayClearCache", LUA_OPCODE(Lua_Remastered, OverlayClearCache) },
	{ "OverlayFade", LUA_OPCODE(Lua_Remastered, OverlayFade) },
	{ "OverlayGetScreenSize", LUA_OPCODE(Lua_Remastered, OverlayGetScreenSize) },
	{ "OverlayScreenBoxingMode", LUA_OPCODE(Lua_Remastered, OverlayScreenBoxingMode) },
	{ "OverlayShowLoadSpinner", LUA_OPCODE(Lua_Remastered, OverlayShowLoadSpinner) },
	{ "WidescreenCorrectionFactor", LUA_OPCODE(Lua_Remastered, WidescreenCorrectionFactor) }
};

static struct luaL_reg remasteredMouseOpcodes[] = {
	{ "QueryActiveHotspots", LUA_OPCODE(Lua_V1, QueryActiveHotspots) },
	{ "GetFloorWalkPos", LUA_OPCODE(Lua_V1, GetFloorWalkPos) },
	{ "GetSelectedDialog", LUA_OPCODE(Lua_V1, GetSelectedDialog) },
	{ "SetCursor", LUA_OPCODE(Lua_V1, SetCursor) },
	{ "ProjectToAxis", LUA_OPCODE(Lua_V1, ProjectToAxis) },
	{ "ShowCursor", LUA_OPCODE(Lua_V1, ShowCursor) },
	{ "AddHotspot", LUA_OPCODE(Lua_V1, AddHotspot) },
	{ "RemoveHotspot", LUA_OPCODE(Lua_V1, RemoveHotspot) },
	{ "GetCursorPosition", LUA_OPCODE(Lua_V1, GetCursorPosition) },
	{ "CursorAnimation", LUA_OPCODE(Lua_V1, CursorAnimation) },
	{ "UpdateHotspot", LUA_OPCODE(Lua_V1, UpdateHotspot) },
	{ "LinkHotspot", LUA_OPCODE(Lua_V1, LinkHotspot) },
	{ "PreloadCursors", LUA_OPCODE(Lua_V1, PreloadCursors) },
	{ "CursorMovieEnabled", LUA_OPCODE(Lua_V1, CursorMovieEnabled) }
};

static struct luaL_reg remasteredExtraOpcodes[] = {
	{ "  concatfallback", LUA_OPCODE(LuaBase, concatFallback) },
	{ "  typeoverride", LUA_OPCODE(LuaBase, typeOverride) },
	{ "  dfltcamera", LUA_OPCODE(Lua_V1, DefaultCamChangeHandler) },
	{ "  dfltcontrol", LUA_OPCODE(Lua_V1, DefaultControlHandler) }
};

Lua_Remastered::Lua_Remastered() : Lua_V1(false, false) {
	lua_FuncIDsSetRemastered(true);
}

void Lua_Remastered::recognizeOpcodes() {
	luaL_addlibtolist(remasteredExtraOpcodes, ARRAYSIZE(remasteredExtraOpcodes));
}

void Lua_Remastered::registerMainOpcodes() {
	luaL_openlib(remasteredMainOpcodes, ARRAYSIZE(remasteredMainOpcodes));
}

void Lua_Remastered::registerCostumeOpcodes() {
	Lua_V1::registerCostumeOpcodes();
	luaL_openlib(remasteredCommentaryOpcodes, ARRAYSIZE(remasteredCommentaryOpcodes));
}

void Lua_Remastered::registerPrimitiveOpcodes() {
	Lua_V1::registerPrimitiveOpcodes();
	luaL_openlib(remasteredOverlayOpcodes, ARRAYSIZE(remasteredOverlayOpcodes));
	luaL_openlib(remasteredMouseOpcodes, ARRAYSIZE(remasteredMouseOpcodes));
}

void Lua_Remastered::registerHardwareOpcodes() {
}

void Lua_Remastered::Is3DHardwareEnabled() {
	lua_pushnumber(1.0f);
}

void Lua_Remastered::reportStub(const char *opcode, const char *module) {
	for (uint i = 0; i < _reportedStubs.size(); i++) {
		if (_reportedStubs[i] == opcode)
			return;
	}

	_reportedStubs.push_back(opcode);
	warning("Lua_Remastered::%s(): the %s is not implemented yet, the call is ignored or answered with a neutral value", opcode, module);
}

void Lua_Remastered::Load() {
	lua_Object slotObj = lua_getparam(1);
	lua_Object lastObj = lua_getparam(2);

	if (!lua_isnumber(slotObj))
		return;

	bool loadLast = !lua_isnil(lastObj) && luaFtol(lua_getnumber(lastObj)) != 0;

	g_grim->requestRemasteredLoad(luaFtol(lua_getnumber(slotObj)), loadLast);
}

void Lua_Remastered::Save() {
	lua_Object slotObj = lua_getparam(1);
	lua_Object muralObj = lua_getparam(2);
	lua_Object setObj = lua_getparam(3);
	lua_Object titleObj = lua_getparam(4);

	if (!lua_isnumber(slotObj) || !lua_isstring(muralObj) || !lua_isnumber(setObj) || !lua_isstring(titleObj))
		return;

	g_grim->requestRemasteredSave(luaFtol(lua_getnumber(slotObj)), lua_getstring(muralObj), luaFtol(lua_getnumber(setObj)),
			lua_getstring(titleObj));
}

void Lua_Remastered::PrintStack() {
}

void Lua_Remastered::New() {
	g_grim->requestRemasteredNewGame();
}

void Lua_Remastered::Erase() {
	lua_Object slotObj = lua_getparam(1);

	if (!lua_isnumber(slotObj))
		return;

	g_grim->requestRemasteredErase(luaFtol(lua_getnumber(slotObj)));
}

void Lua_Remastered::GetSaveStatus() {
	lua_pushnumber((float)g_grim->getRemasteredSaveStatus());
}

void Lua_Remastered::FindSaveGames() {
	if (_foundSaves.empty()) {
		lua_pushnil();
		return;
	}

	lua_Object list = lua_createtable();

	for (uint i = 0; i < _foundSaves.size(); i++) {
		const SaveStore::SlotInfo &info = _foundSaves[i];

		lua_pushobject(list);
		lua_pushnumber((float)i);

		lua_Object entry = lua_createtable();

		lua_pushobject(entry);
		lua_settable();

		lua_pushobject(entry);
		lua_pushstring("slot");
		lua_pushnumber((float)info.slot);
		lua_settable();

		lua_pushobject(entry);
		lua_pushstring("title");
		lua_pushstring(info.title.c_str());
		lua_settable();

		Common::String timeDate;

		if (getSaveSlotTime(info.slot, &timeDate) && !timeDate.empty()) {
			lua_pushobject(entry);
			lua_pushstring("timeDateString");
			lua_pushstring(timeDate.c_str());
			lua_settable();
		}

		lua_pushobject(entry);
		lua_pushstring("mural_info");
		lua_pushstring(info.mural.c_str());
		lua_settable();

		lua_pushobject(entry);
		lua_pushstring("setIndex");
		lua_pushnumber((float)info.setIndex);
		lua_settable();
	}

	lua_pushobject(list);
}

void Lua_Remastered::InitiateFindSaveGames() {
	SaveStore::listSlots(_foundSaves);
	_findSaveGamesStatus = 1;
}

void Lua_Remastered::GetFindSaveGameStatus() {
	lua_pushnumber((float)_findSaveGamesStatus);
}

void Lua_Remastered::SetMouseSpeedScale() {
}

void Lua_Remastered::SetKeyMappingMode() {
	int slot = luaFtol(lua_getnumber(lua_getparam(1)));

	g_grim->getStdControl()->setKeyMappingMode(slot);
}

void Lua_Remastered::ResetKeyMappingToDefault() {
	g_grim->getStdControl()->resetKeyMappingToDefault();
}

void Lua_Remastered::GetRemappedKeyName() {
	int slot = luaFtol(lua_getnumber(lua_getparam(1)));

	if (slot < 0 || slot >= StdControl::kNumRemappedKeys) {
		warning("Lua_Remastered::GetRemappedKeyName(): slot %d out of range", slot);
		lua_pushstring("");
		return;
	}

	lua_pushstring(StdControl::getSdlKeyName(g_grim->getStdControl()->getRemappedKey(slot)).c_str());
}

void Lua_Remastered::GetRemappedKeyHint() {
	int slot = luaFtol(lua_getnumber(lua_getparam(1)));

	if (slot < 0 || slot >= StdControl::kNumRemappedKeys) {
		warning("Lua_Remastered::GetRemappedKeyHint(): slot %d out of range", slot);
		lua_pushstring("key_empty.png");
		lua_pushstring("");
		return;
	}

	int sdlKey = g_grim->getStdControl()->getRemappedKey(slot);
	const char *glyphFile = nullptr;
	const char *glyphText = nullptr;

	if (StdControl::getKeyGlyph(sdlKey, &glyphFile, &glyphText)) {
		lua_pushstring(glyphFile);

		if (glyphText)
			lua_pushstring(glyphText);
		else
			lua_pushnil();
	} else {
		lua_pushstring("key_empty.png");
		lua_pushstring(StdControl::getSdlKeyName(sdlKey).c_str());
	}
}

void Lua_Remastered::NoOpPutActorAtOrigin() {
}

void Lua_Remastered::NoOpGetClippedPos() {
}

void Lua_Remastered::NoOpDriveActorTo() {
}

void Lua_Remastered::NoOpWalkActorToInterest() {
}

void Lua_Remastered::NoOpUnLockFont() {
}

void Lua_Remastered::NoOpLockSound() {
}

void Lua_Remastered::NoOpUnlockSound() {
}

void Lua_Remastered::NoOpLockCostume() {
}

void Lua_Remastered::NoOpUnlockCostume() {
}

void Lua_Remastered::NoOpPlaySound() {
}

void Lua_Remastered::NoOpPlaySoundAt() {
}

void Lua_Remastered::NoOpPlaySoundAttached() {
}

void Lua_Remastered::NoOpIsSoundPlaying() {
}

void Lua_Remastered::NoOpGetSectorVertices() {
}

void Lua_Remastered::NoOpLoadBundle() {
}

void Lua_Remastered::NoOpUnloadBundle() {
}

int Lua_Remastered::controlCallback(float frameTime) {
	return controlCallbackRemastered(frameTime);
}

void Lua_Remastered::OverlayCreate() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object propsObj = lua_getparam(4);

	if (!lua_isstring(nameObj)) {
		lua_pushnumber(-1.0f);
		return;
	}

	OverlayProps props;
	int x = lua_isnumber(xObj) ? luaFtol(lua_getnumber(xObj)) : 0;
	int y = lua_isnumber(yObj) ? luaFtol(lua_getnumber(yObj)) : 0;

	if (lua_istable(propsObj)) {
		lua_Object cacheObj = lua_getfield(propsObj, "cache");

		if (lua_isnumber(cacheObj))
			props.cache = luaFtol(lua_getnumber(cacheObj));

		lua_Object layerObj = lua_getfield(propsObj, "layer");

		if (lua_isnumber(layerObj))
			props.layer = luaFtol(lua_getnumber(layerObj));

		lua_Object coordsObj = lua_getfield(propsObj, "coords");

		if (lua_isnumber(coordsObj))
			props.coords = luaFtol(lua_getnumber(coordsObj));

		lua_Object clearObj = lua_getfield(propsObj, "clear_buffer");

		if (lua_isnumber(clearObj) && lua_getnumber(clearObj) != 0.0f)
			props.clearBuffer = true;

		lua_Object mirrorObj = lua_getfield(propsObj, "mirrormode");

		if (lua_isnumber(mirrorObj))
			props.mirror = luaFtol(lua_getnumber(mirrorObj));

		lua_Object widthObj = lua_getfield(propsObj, "width");

		if (lua_isnumber(widthObj))
			props.width = luaFtol(lua_getnumber(widthObj));

		lua_Object heightObj = lua_getfield(propsObj, "height");

		if (lua_isnumber(heightObj))
			props.height = luaFtol(lua_getnumber(heightObj));
	}

	int handle = g_grim->getHdCanvas()->getOverlays()->create(lua_getstring(nameObj), x, y, props);

	lua_pushnumber((float)handle);
}

void Lua_Remastered::OverlayMove() {
	lua_Object handleObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);

	g_grim->getHdCanvas()->getOverlays()->move(luaFtol(lua_getnumber(handleObj)), luaFtol(lua_getnumber(xObj)),
			luaFtol(lua_getnumber(yObj)));
}

void Lua_Remastered::OverlaySetLayer() {
	lua_Object handleObj = lua_getparam(1);
	lua_Object layerObj = lua_getparam(2);

	g_grim->getHdCanvas()->getOverlays()->setLayer(luaFtol(lua_getnumber(handleObj)), luaFtol(lua_getnumber(layerObj)));
}

void Lua_Remastered::OverlayDestroy() {
	lua_Object handleObj = lua_getparam(1);

	if (lua_isnil(handleObj))
		return;

	g_grim->getHdCanvas()->getOverlays()->destroy(luaFtol(lua_getnumber(handleObj)));
}

void Lua_Remastered::OverlayDimensions() {
	lua_Object handleObj = lua_getparam(1);
	int width = 0;
	int height = 0;

	g_grim->getHdCanvas()->getOverlays()->getDimensions(luaFtol(lua_getnumber(handleObj)), &width, &height);

	lua_pushnumber((float)width);
	lua_pushnumber((float)height);
}

void Lua_Remastered::OverlayClearCache() {
	g_grim->getHdCanvas()->getOverlays()->clearCache();
}

void Lua_Remastered::OverlayFade() {
	lua_Object handleObj = lua_getparam(1);
	lua_Object typeObj = lua_getparam(2);
	lua_Object valueObj = lua_getparam(3);

	g_grim->getHdCanvas()->getOverlays()->fade(luaFtol(lua_getnumber(handleObj)), luaFtol(lua_getnumber(typeObj)), true,
			lua_getnumber(valueObj));
}

void Lua_Remastered::OverlayGetScreenSize() {
	lua_pushnumber((float)kCanvasWidth);
	lua_pushnumber((float)kCanvasHeight);
}

void Lua_Remastered::OverlayScreenBoxingMode() {
}

void Lua_Remastered::OverlayShowLoadSpinner() {
	lua_Object showObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	int x = lua_isnumber(xObj) ? luaFtol(lua_getnumber(xObj)) : 0;
	int y = lua_isnumber(yObj) ? luaFtol(lua_getnumber(yObj)) : 0;

	g_grim->getHdCanvas()->getOverlays()->showLoadSpinner(luaFtol(lua_getnumber(showObj)) != 0, x, y);
}

void Lua_Remastered::WidescreenCorrectionFactor() {
	lua_pushnumber(g_grim->getHdCanvas()->getWidescreenCorrection());
}

void Lua_Remastered::SetCommentary() {
	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj))
		return;

	g_grim->getCommentary()->setCommentary(lua_getstring(nameObj));
}

void Lua_Remastered::ClearCommentary() {
	g_grim->getCommentary()->clearCommentary();
}

void Lua_Remastered::PlayCurrentCommentary() {
	g_grim->getCommentary()->playCurrentCommentary();
}

void Lua_Remastered::EnableCommentary() {
	lua_Object enableObj = lua_getparam(1);

	if (lua_isnumber(enableObj) && luaFtol(lua_getnumber(enableObj)) == 0)
		g_grim->getCommentary()->stopImmediately();
}

void Lua_Remastered::StopCommentaryImmediately() {
	g_grim->getCommentary()->stopImmediately();
}

void Lua_Remastered::HasHeardCommentary() {
	lua_Object nameObj = lua_getparam(1);

	if (lua_isstring(nameObj) && g_grim->getCommentary()->hasHeardCommentary(lua_getstring(nameObj)))
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_Remastered::IsPlayingCommentary() {
	if (g_grim->getCommentary()->isPlaying())
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

} // end of namespace Grim
