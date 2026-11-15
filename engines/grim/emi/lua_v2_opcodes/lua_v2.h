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

#ifndef GRIM_LUA_V2
#define GRIM_LUA_V2

#include "common/array.h"

#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"

namespace Grim {

struct FileUtilFind;
class Actor;
class Camera;
class CjkString;
class CLightNode;
class Color;
class EmiEngine;
class TextEvent;

class Lua_V2 : public Lua_V1 {
public:
	typedef Lua_V2 LuaClass;

	explicit Lua_V2(EmiEngine *engine);

	int startup() override;
	int shutdown() override;
	int boot(int saveGameResume, const char *bootParam) override;
	using Lua_V1::update;
	void update(float seconds);
	int dofile(const char *filename) override;
	bool logError(const char *message) override;
	Common::SeekableReadStream *openBundleStream(const char *path) override;
	Common::SeekableReadStream *openPrefsForReading(Common::SeekableReadStream *realFile) override;
	Common::WriteStream *openPrefsForWriting(Common::WriteStream *realFile) override;
	void recognizeOpcodes() override;
	void registerMainOpcodes() override;
	void registerPrimitiveOpcodes() override;

	void callCameraChange(int oldSetup, int newSetup);
	void callPostCameraChange(int setup);
	bool isSoundLocked(int soundHandle);
	void callCollisionHandler(Actor *first, Actor *second, float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2);
	bool buttonHandler(int vk, bool down);
	void callAxisHandler(int index, float value);
	void axisHandler();
	void updateJoySelected();

	int doExitCallback() override;
	int doPauseCallback() override;
	int doCharacterCallback(int character) override;
	bool callPrefsInit();
	using Lua_V1::saveGameCallback;
	void saveGameCallback(SaveRestoreAccess *writer);
	void getVersion(LString &version);
	void saveLocks(SaveRestoreAccess *writer);
	void restoreLocks(SaveRestoreAccess *reader);
	void toggleStatsOverlay(bool hide);
	static uint getClockUniqueUint();

	enum {
		kStatsOverlayFlag = 0x20,
		kStatsProfilerFlag = 0x40
	};

	uint _statsOverlayFlags;

protected:
	void typeOverride() override;
	void concatFallback() override;

	DECLARE_LUA_OPCODE(UnimplementedFunction);
	DECLARE_LUA_OPCODE(EmptyFunction);

	// Opcodes that only the demo has
	DECLARE_LUA_OPCODE(ObsoleteFunction);
	DECLARE_LUA_OPCODE(NoControlHandler);
	DECLARE_LUA_OPCODE(FindFileOrSwapCDs);
	DECLARE_LUA_OPCODE(CheckForFile) override;
	DECLARE_LUA_OPCODE(FindFileOnAnyCD) override;
	DECLARE_LUA_OPCODE(ImPause);
	DECLARE_LUA_OPCODE(ImResume);

	DECLARE_LUA_OPCODE(Draw3DLine);
	DECLARE_LUA_OPCODE(DrawLine) override;
	DECLARE_LUA_OPCODE(DrawPolygon) override;
	DECLARE_LUA_OPCODE(DrawRectangle) override;
	DECLARE_LUA_OPCODE(ChangePrimitive) override;
	DECLARE_LUA_OPCODE(KillPrimitive) override;
	DECLARE_LUA_OPCODE(PurgePrimitiveQueue) override;

	DECLARE_LUA_OPCODE(EnginePause) override;
	DECLARE_LUA_OPCODE(EngineResume) override;
	DECLARE_LUA_OPCODE(GetPhysicalBytes) override;
	DECLARE_LUA_OPCODE(GetMemoryUsage) override;
	DECLARE_LUA_OPCODE(SetMemoryUsage) override;
	DECLARE_LUA_OPCODE(SetActiveCD);
	DECLARE_LUA_OPCODE(GetActiveCD);
	DECLARE_LUA_OPCODE(AreWeInternational);
	DECLARE_LUA_OPCODE(FileFindFirst) override;
	DECLARE_LUA_OPCODE(FileFindNext) override;
	DECLARE_LUA_OPCODE(FileFindDispose) override;
	DECLARE_LUA_OPCODE(TextFileGetLineCount) override;
	DECLARE_LUA_OPCODE(TextFileGetLine) override;
	DECLARE_LUA_OPCODE(InputDialog) override;
	DECLARE_LUA_OPCODE(QueryDialog) override;
	DECLARE_LUA_OPCODE(ScreenShot) override;
	DECLARE_LUA_OPCODE(MakeScreenTextures);
	DECLARE_LUA_OPCODE(SubmitSaveGameData) override;
	DECLARE_LUA_OPCODE(GetSaveGameData) override;
	DECLARE_LUA_OPCODE(Load) override;
	DECLARE_LUA_OPCODE(Save) override;
	DECLARE_LUA_OPCODE(JustLoaded) override;
	DECLARE_LUA_OPCODE(ScreenshotForSavegame);
	DECLARE_LUA_OPCODE(ThumbnailFromScreen);
	DECLARE_LUA_OPCODE(ThumbnailFromFile);
	DECLARE_LUA_OPCODE(ClearSpecialtyTexture);
	DECLARE_LUA_OPCODE(Exit) override;
	DECLARE_LUA_OPCODE(FunctionName) override;
	DECLARE_LUA_OPCODE(EnableControl) override;
	DECLARE_LUA_OPCODE(DisableControl) override;
	DECLARE_LUA_OPCODE(GetControlState) override;
	DECLARE_LUA_OPCODE(PrintMessage) override;
	DECLARE_LUA_OPCODE(PrintError) override;
	DECLARE_LUA_OPCODE(PrintWarning) override;
	DECLARE_LUA_OPCODE(PrintDebug) override;
	DECLARE_LUA_OPCODE(MakeCurrentSet) override;
	DECLARE_LUA_OPCODE(LockSet) override;
	DECLARE_LUA_OPCODE(UnLockSet) override;
	DECLARE_LUA_OPCODE(LockBackground);
	DECLARE_LUA_OPCODE(UnLockBackground);
	DECLARE_LUA_OPCODE(MakeCurrentSetup) override;
	DECLARE_LUA_OPCODE(GetCurrentSetup) override;
	DECLARE_LUA_OPCODE(NextSetup) override;
	DECLARE_LUA_OPCODE(PreviousSetup) override;
	DECLARE_LUA_OPCODE(StartMovie) override;
	DECLARE_LUA_OPCODE(StopMovie) override;
	DECLARE_LUA_OPCODE(PauseMovie) override;
	DECLARE_LUA_OPCODE(EscapeMovie);
	DECLARE_LUA_OPCODE(IsMoviePlaying) override;
	DECLARE_LUA_OPCODE(WorldToScreen) override;
	DECLARE_LUA_OPCODE(GetCameraPosition) override;
	DECLARE_LUA_OPCODE(SetCameraPosition) override;
	DECLARE_LUA_OPCODE(CameraLookAt);
	DECLARE_LUA_OPCODE(GetCameraFOV) override;
	DECLARE_LUA_OPCODE(SetCameraFOV) override;
	DECLARE_LUA_OPCODE(GetCameraYaw);
	DECLARE_LUA_OPCODE(YawCamera);
	DECLARE_LUA_OPCODE(GetCameraPitch);
	DECLARE_LUA_OPCODE(PitchCamera);
	DECLARE_LUA_OPCODE(GetCameraRoll) override;
	DECLARE_LUA_OPCODE(RollCamera);
	DECLARE_LUA_OPCODE(GetCameraLookVector) override;
	DECLARE_LUA_OPCODE(PerSecond) override;
	DECLARE_LUA_OPCODE(SnapToPlane) override;
	DECLARE_LUA_OPCODE(GetAngleBetweenVectors) override;
	DECLARE_LUA_OPCODE(TurnLightOn) override;
	DECLARE_LUA_OPCODE(IsLightOn) override;
	DECLARE_LUA_OPCODE(SetLightPosition) override;
	DECLARE_LUA_OPCODE(GammaEnabled);
	DECLARE_LUA_OPCODE(SetGamma) override;
	DECLARE_LUA_OPCODE(SetPixelCenter);
	DECLARE_LUA_OPCODE(LoadBundle) override;
	DECLARE_LUA_OPCODE(RenderModeUser) override;
	DECLARE_LUA_OPCODE(DimScreen) override;
	DECLARE_LUA_OPCODE(DimRegion) override;
	DECLARE_LUA_OPCODE(UndimAll);
	DECLARE_LUA_OPCODE(UndimRegion);
	DECLARE_LUA_OPCODE(SetSpeechMode) override;
	DECLARE_LUA_OPCODE(GetSpeechMode) override;
	DECLARE_LUA_OPCODE(GetCPUSpeed);
	DECLARE_LUA_OPCODE(NewLayer);
	DECLARE_LUA_OPCODE(FreeLayer);
	DECLARE_LUA_OPCODE(GetLayerSortOrder);
	DECLARE_LUA_OPCODE(SetLayerSortOrder);
	DECLARE_LUA_OPCODE(GetLayerFrame);
	DECLARE_LUA_OPCODE(SetLayerFrame);
	DECLARE_LUA_OPCODE(AdvanceLayerFrame);
	DECLARE_LUA_OPCODE(SetTextSpeed) override;
	DECLARE_LUA_OPCODE(GetTextSpeed) override;
	DECLARE_LUA_OPCODE(IsMessageGoing) override;
	DECLARE_LUA_OPCODE(SetSayLineDefaults) override;
	DECLARE_LUA_OPCODE(SetActorTalkColor) override;
	DECLARE_LUA_OPCODE(GetActorTalkColor) override;
	DECLARE_LUA_OPCODE(SayLine) override;
	DECLARE_LUA_OPCODE(MakeTextObject) override;
	DECLARE_LUA_OPCODE(KillTextObject) override;
	DECLARE_LUA_OPCODE(ChangeTextObject) override;
	DECLARE_LUA_OPCODE(GetTextObjectDimensions) override;
	DECLARE_LUA_OPCODE(BlastText) override;
	DECLARE_LUA_OPCODE(ExpireText) override;
	DECLARE_LUA_OPCODE(PurgeText) override;
	DECLARE_LUA_OPCODE(PushText);
	DECLARE_LUA_OPCODE(PopText);
	DECLARE_LUA_OPCODE(MakeColor) override;
	DECLARE_LUA_OPCODE(GetColorComponents) override;
	DECLARE_LUA_OPCODE(SetTranslationMode) override;
	DECLARE_LUA_OPCODE(GetTranslationMode) override;
	DECLARE_LUA_OPCODE(LocalizeString) override;
	DECLARE_LUA_OPCODE(GetFontDimensions) override;
	DECLARE_LUA_OPCODE(GetTextCharPosition) override;
	DECLARE_LUA_OPCODE(SetOffscreenTextPos) override;
	DECLARE_LUA_OPCODE(ReportAllResources);
	DECLARE_LUA_OPCODE(ReportLRUResources);
	DECLARE_LUA_OPCODE(ReportUsedResources);
	DECLARE_LUA_OPCODE(ReportUnusedResources);
	DECLARE_LUA_OPCODE(PurgeAmount);
	DECLARE_LUA_OPCODE(PurgeAllResources);
	DECLARE_LUA_OPCODE(PurgeUnusedResources);
	DECLARE_LUA_OPCODE(PurgeUnlockedResources);
	DECLARE_LUA_OPCODE(NukeAllScriptLocks);
	DECLARE_LUA_OPCODE(FRUTEY_Begin);
	DECLARE_LUA_OPCODE(FRUTEY_End);

	DECLARE_LUA_OPCODE(LoadActor) override;
	DECLARE_LUA_OPCODE(UnloadActor);
	DECLARE_LUA_OPCODE(GetActorTimeScale) override;
	DECLARE_LUA_OPCODE(SetActorTimeScale) override;
	DECLARE_LUA_OPCODE(SetActorWalkDominate) override;
	DECLARE_LUA_OPCODE(SetSelectedActor) override;
	DECLARE_LUA_OPCODE(SetActorWalkChore) override;
	DECLARE_LUA_OPCODE(SetActorTurnChores) override;
	DECLARE_LUA_OPCODE(SetActorRestChore) override;
	DECLARE_LUA_OPCODE(SetActorMumblechore) override;
	DECLARE_LUA_OPCODE(SetActorTalkChore) override;
	DECLARE_LUA_OPCODE(SetActorWalkRate) override;
	DECLARE_LUA_OPCODE(GetActorWalkRate) override;
	DECLARE_LUA_OPCODE(SetActorTurnRate) override;
	DECLARE_LUA_OPCODE(GetActorTurnRate) override;
	DECLARE_LUA_OPCODE(GetActorPos) override;
	DECLARE_LUA_OPCODE(GetActorWorldPos);
	DECLARE_LUA_OPCODE(GetActorPuckVector) override;
	DECLARE_LUA_OPCODE(GetActorYawToPoint) override;
	DECLARE_LUA_OPCODE(SetActorReflection) override;
	DECLARE_LUA_OPCODE(PutActorInSet) override;
	DECLARE_LUA_OPCODE(PutActorAt) override;
	DECLARE_LUA_OPCODE(PutActorInOverworld);
	DECLARE_LUA_OPCODE(RemoveActorFromOverworld);
	DECLARE_LUA_OPCODE(ClearOverworld);
	DECLARE_LUA_OPCODE(ToggleOverworld);
	DECLARE_LUA_OPCODE(WalkActorVector) override;
	DECLARE_LUA_OPCODE(WalkActorForward) override;
	DECLARE_LUA_OPCODE(ActorStopMoving);
	DECLARE_LUA_OPCODE(WalkActorTo) override;
	DECLARE_LUA_OPCODE(WalkActorToAvoiding) override;
	DECLARE_LUA_OPCODE(ActorLookAt) override;
	DECLARE_LUA_OPCODE(IsActorLooking) override;
	DECLARE_LUA_OPCODE(SetActorLookRate) override;
	DECLARE_LUA_OPCODE(GetActorLookRate) override;
	DECLARE_LUA_OPCODE(SetActorFOV);
	DECLARE_LUA_OPCODE(GetActorFOV);
	DECLARE_LUA_OPCODE(GetVisibleThings) override;
	DECLARE_LUA_OPCODE(SetActorLighting);
	DECLARE_LUA_OPCODE(SetActorHead) override;
	DECLARE_LUA_OPCODE(SetActorHeadLimits);
	DECLARE_LUA_OPCODE(SetActorVisibility) override;
	DECLARE_LUA_OPCODE(SetActorFollowBoxes) override;
	DECLARE_LUA_OPCODE(ActorActivateShadow);
	DECLARE_LUA_OPCODE(ShutUpActor) override;
	DECLARE_LUA_OPCODE(EnableActorPuck);
	DECLARE_LUA_OPCODE(SetActorGlobalAlpha);
	DECLARE_LUA_OPCODE(SetActorLocalAlpha);
	DECLARE_LUA_OPCODE(SetActorCollisionMode) override;
	DECLARE_LUA_OPCODE(SetActorCollisionScale) override;
	DECLARE_LUA_OPCODE(SetActorSortOrder);
	DECLARE_LUA_OPCODE(GetActorSortOrder);
	DECLARE_LUA_OPCODE(AttachActor);
	DECLARE_LUA_OPCODE(DetachActor);
	DECLARE_LUA_OPCODE(IsActorInSector) override;
	DECLARE_LUA_OPCODE(GetActorSector) override;
	DECLARE_LUA_OPCODE(IsPointInSector) override;
	DECLARE_LUA_OPCODE(GetPointSector) override;
	DECLARE_LUA_OPCODE(TurnActor) override;
	DECLARE_LUA_OPCODE(GetActorRot) override;
	DECLARE_LUA_OPCODE(SetActorRot) override;
	DECLARE_LUA_OPCODE(SetActorPitch) override;
	DECLARE_LUA_OPCODE(SetActorRoll) override;
	DECLARE_LUA_OPCODE(IsActorTurning) override;
	DECLARE_LUA_OPCODE(PointActorAt) override;
	DECLARE_LUA_OPCODE(TurnActorTo) override;
	DECLARE_LUA_OPCODE(PlayActorChore) override;
	DECLARE_LUA_OPCODE(PlayActorChoreLooping) override;
	DECLARE_LUA_OPCODE(IsActorMoving) override;
	DECLARE_LUA_OPCODE(IsActorResting) override;
	DECLARE_LUA_OPCODE(GetActorChores) override;
	DECLARE_LUA_OPCODE(StopActorChores);
	DECLARE_LUA_OPCODE(IsChoreValid);
	DECLARE_LUA_OPCODE(IsChorePlaying);
	DECLARE_LUA_OPCODE(IsChoreLooping);
	DECLARE_LUA_OPCODE(SetChoreLooping);
	DECLARE_LUA_OPCODE(PlayChore);
	DECLARE_LUA_OPCODE(FadeInChore) override;
	DECLARE_LUA_OPCODE(FadeOutChore) override;
	DECLARE_LUA_OPCODE(StopChore);
	DECLARE_LUA_OPCODE(PauseChore);
	DECLARE_LUA_OPCODE(AdvanceChore);
	DECLARE_LUA_OPCODE(CompleteChore);
	DECLARE_LUA_OPCODE(LockChore);
	DECLARE_LUA_OPCODE(UnlockChore);
	DECLARE_LUA_OPCODE(LockChoreSet);
	DECLARE_LUA_OPCODE(UnlockChoreSet);
	DECLARE_LUA_OPCODE(IsSectorActive) override;
	DECLARE_LUA_OPCODE(MakeSectorActive) override;
	DECLARE_LUA_OPCODE(GetSectorOppositeEdge) override;
	DECLARE_LUA_OPCODE(GetSectorName);
	DECLARE_LUA_OPCODE(GetAngleBetweenActors) override;

	DECLARE_LUA_OPCODE(PlaySound) override;
	DECLARE_LUA_OPCODE(StopSound);
	DECLARE_LUA_OPCODE(StopAllSounds);
	DECLARE_LUA_OPCODE(LoadSound);
	DECLARE_LUA_OPCODE(FreeSound);
	DECLARE_LUA_OPCODE(PlayLoadedSound);
	DECLARE_LUA_OPCODE(IsSoundPlaying) override;
	DECLARE_LUA_OPCODE(GetGroupVolume);
	DECLARE_LUA_OPCODE(SetGroupVolume);
	DECLARE_LUA_OPCODE(GetSoundVolume);
	DECLARE_LUA_OPCODE(SetSoundVolume);
	DECLARE_LUA_OPCODE(EnableAudioGroup);
	DECLARE_LUA_OPCODE(EnableVoiceFX);
	DECLARE_LUA_OPCODE(SetAudioMixAhead);
	DECLARE_LUA_OPCODE(PlaySoundFrom);
	DECLARE_LUA_OPCODE(PlayLoadedSoundFrom);
	DECLARE_LUA_OPCODE(SetReverb);
	DECLARE_LUA_OPCODE(UpdateSoundPosition);
	DECLARE_LUA_OPCODE(ImSetState) override;
	DECLARE_LUA_OPCODE(ImSelectSet);
	DECLARE_LUA_OPCODE(ImStateHasLooped);
	DECLARE_LUA_OPCODE(ImStateHasEnded);
	DECLARE_LUA_OPCODE(ImPushState);
	DECLARE_LUA_OPCODE(ImPopState);
	DECLARE_LUA_OPCODE(ImFlushStack);
	DECLARE_LUA_OPCODE(ImGetMillisecondPosition);

	DECLARE_LUA_OPCODE(SetReplayMode);
	DECLARE_LUA_OPCODE(GetReplayMode);
	DECLARE_LUA_OPCODE(GetNextDemoNum);
	DECLARE_LUA_OPCODE(NotifyGameState);
	DECLARE_LUA_OPCODE(DeleteSavedGame);
	DECLARE_LUA_OPCODE(CopySavedGame);
	DECLARE_LUA_OPCODE(JustSaved);
	DECLARE_LUA_OPCODE(GetMemoryCardId);
	DECLARE_LUA_OPCODE(GetUniqueUint);
	DECLARE_LUA_OPCODE(FormatMemoryCard);
	DECLARE_LUA_OPCODE(SetActorTessellation);
	DECLARE_LUA_OPCODE(IsActorChoring) override;
	DECLARE_LUA_OPCODE(ApplyForceFeedback);
	DECLARE_LUA_OPCODE(EnableSoakText);
	DECLARE_LUA_OPCODE(SetStereoAudio);
	DECLARE_LUA_OPCODE(pspu);
	DECLARE_LUA_OPCODE(dsnd);
	DECLARE_LUA_OPCODE(KillMusicAndWait);
	DECLARE_LUA_OPCODE(OverWorldToScreen);
	DECLARE_LUA_OPCODE(SetVScanOffsets);
	DECLARE_LUA_OPCODE(UnimplementedFunctionPs2);
	DECLARE_LUA_OPCODE(ObsoleteFunctionPs2);
	DECLARE_LUA_OPCODE(ToggleDrawFPS);

protected:
	enum {
		kConsoleStatus,
		kConsoleWarning,
		kConsoleError,
		kConsoleDebug
	};

	bool isActor(lua_Object object);
	Actor *getActor(lua_Object object, const char *opcodeName);
	CLightNode *getLightArg(int paramNumber, const char *opcodeName);
	void logMessage(int level, const char *format, ...);
	bool disabledFunction(int level, const char *format, lua_Object argumentObj);
	Camera *getCurrentCamera(const char *name);
	bool getChoreId(lua_Object idObj, const char *name, uint *instanceId);
	bool getChoreSeconds(lua_Object secondsObj, const char *name, const char *requirement, float *seconds);
	bool fileExists(const char *name);
	void lockBackgroundWithScene(const LString &name, bool recordName);
	void unlockBackgroundWithScene(const LString &name, bool forgetName);

private:
	enum {
		kFieldX,
		kFieldY,
		kFieldFont,
		kFieldWidth,
		kFieldHeight,
		kFieldHicolor,
		kFieldFgcolor,
		kFieldBgcolor,
		kFieldFxcolor,
		kFieldDuration,
		kFieldLayer,
		kFieldCenter,
		kFieldLjustify,
		kFieldRjustify,
		kFieldVolume,
		kFieldPan,
		kNumFieldRefs
	};

	struct LockedChore {
		LString choreName;
		LString choreSetName;
	};

	void makeSystemTable();
	void makeControlsTable(int ref);
	void applyTextTable(lua_Object table, TextEvent *event);
	bool joinTextStrings(lua_Object table, CjkString *result);
	void readSayLineTable(lua_Object table, int *volume, int *pan, int *x, int *y);
	void dumpResources(int mode);
	void purgeResources(int mode);

	static int32 identityHook(int32 tag, int32 data, SaveRestoreAccess *saveFunc);
	static lua_Object getTableField(lua_Object table, const char *key);
	static bool getColorField(lua_Object table, Color &color);
	static bool getLayerField(lua_Object table, int *layer);
	static bool getFloatPairFields(lua_Object table, const char *firstKey, const char *secondKey, float defaultValue, float *first, float *second);
	static int truncateToInt(double value);

	Common::Array<LockedChore> _lockedChores;
	Common::Array<LString> _lockedChoreSets;
	Common::Array<LString> _lockedRooms;
	Common::Array<LString> _lockedBackgrounds;
	Common::Array<uint> _lockedSounds;
	int _saveDataSize;
	SaveRestoreAccess *_saveWriter;

	EmiEngine *_engine;
	int _fieldRefs[kNumFieldRefs];
	FileUtilFind *_find;
	float _gcTimer;
	bool _started;
};

} // end of namespace Grim

#endif
