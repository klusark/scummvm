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

#ifndef GRIM_EMI_EMIENGINE_H
#define GRIM_EMI_EMIENGINE_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/list.h"
#include "common/str.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/core/linearrandom.h"
#include "engines/grim/emi/math/cvector.h"
#include "engines/grim/emi/math/irect.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/sound/lipdata.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/core/profilertimer.h"
#include "engines/grim/grim.h"

namespace Graphics {
struct Surface;
}

namespace Grim {

class AInst;
class ANode;
class Background;
class CBundleServices;
class CDCVertexArray;
class CDrawContext;
class CGrimeConsole;
class ChoreInstance;
class CLightNode;
class COGLContext;
class CPathingFileServices;
class CTextFont;
class CTexture;
class CTinyGLContext;
class CWeightMap;
class DemoReplay;
struct DisplayMode;
class DimRegion;
class EmiRegistry;
struct Event;
class EventGenerator;
class EventHandler;
class InputRecorder;
class Joystick;
class KeyGenerator;
class LABitmap;
class LConsoleServices;
class LFileServices;
class LMemoryServices;
class Layer;
class Lua_V2;
class Movie;
class Overworld;
class Polyglot_v2;
class PrimitiveQueue_v2;
class Puck_v2;
class ResourceManager;
class RMesh;
class RMeshInstance;
class Room_v2;
class Sounds;
class SputControl_v2;
class TextEvent;
class ZFile;
class ZFileServices;

class EmiEngine : public GrimEngine {
public:
	enum {
		kModeNone = 0,
		kModePaused = 1,
		kModeGame = 2,
		kModeMovie = 3,
		kModeOverworld = 4
	};

	enum {
		kMessageBoxTypeOk = 0,
		kMessageBoxOk = 1,
		kMessageBoxYesNoCancel = 3,
		kMessageBoxYesNo = 4,
		kMessageBoxCancel = 2,
		kMessageBoxYes = 6,
		kMessageBoxNo = 7
	};

	EmiEngine(OSystem *syst, uint32 gameFlags, GrimGameType gameType, Common::Platform platform, Common::Language language);
	~EmiEngine() override;

	LMemoryServices *getMemoryServices();
	LConsoleServices *getConsoleServices();
	LFileServices *getFileServices();
	CPathingFileServices *getPathingFileServices();
	ZFileServices *getZFileServices();
	CBundleServices *getBundleServices();
	LResourceManager *getResourceManager();
	ResourceManager *getGameResourceManager();
	CDrawContext *getDrawContext();
	Overworld *getOverworld();
	InputRecorder *getInputRecorder();
	DemoReplay *getDemoReplay();
	Lua_V2 *getLua();
	KeyGenerator *getKeyGenerator();
	Polyglot_v2 *getPolyglot();
	Sounds *getSounds();
	SputControl_v2 *getControl();
	Joystick *getJoystick();
	PrimitiveQueue_v2 *getPrimitiveQueue();
	const Common::List<CLightNode *> &getActiveLights() const;
	bool isCjkMode() const { return _cjkMode; }
	bool isDemo() const { return _isDemo; }

	int registerConsole(LConsoleServices *console);
	void deleteAllConsoles();
	int pushFileServices(LFileServices *services);
	LFileServices *popFileServices();
	uint getNumFileServices();
	void deleteAllFileServices();
	void setResourceManager(LResourceManager *manager);
	void setBundleServices(CBundleServices *services);
	void createInputRecorder(int mode);
	void destroyInputRecorder();
	void setReplayRequest(int mode, const char *name);
	void countDemos();
	int getNextDemoNum();
	static int32 nextRandom();
	static void seedRandom(uint32 seed);
	void destroyOverworld();
	void setDrawContext(CDrawContext *drawContext);

	int renderUpdate(CDrawContext *drawContext);
	int updateManual(CDrawContext *drawContext, bool draw, int manualMs);

	int getMode();
	void setMode(int newMode);
	int getSavedMode();
	void setSavedMode(int newMode);
	int getCpuSpeed();
	void setCpuSpeed(int newSpeed);
	bool hasGammaControl();
	void setGamma(float gamma);
	bool setIsland(const char *island);
	float getFrameTime();
	bool isDumpingFrames();
	void setSaveLoadIdle(bool idle);
	void setPause(bool pause);
	int getDiscNumber();
	bool findDisc(int disc);
	static uint getTotalPhysMem();
	static int messageBox(const char *caption, const char *text, uint flags);
	static bool inputBox(const char *caption, const char *prompt, char *buffer, int bufferSize, const char *defaultText);
	void saveScreenshot();
	bool buildSpecScreenTextures(int index);
	static void readScreenRegion(LABitmap *bitmap, const char *buffer, int bytesPerPixel, float startX, float endX, float startY, float endY);
	bool grabScreen();
	bool makeThumbnailTexture(int index, bool fromScreen);
	bool loadThumbnailTexture(int index, const char *fileName);
	bool clearTextureSlot(int index);
	LFile *openSaveFile(const char *fileName, const char *mode);
	void closeSaveFile(LFile *file);
	void beginFrameDump(const char *path);
	void endFrameDump();
	bool playMovie(const char *fileName, bool subtitles, bool flag6194, bool flag6195, int startFrame);
	void setPendingMusicState(int musicState);
	void showMessage(const char *text, const char *caption);
	void showMessage(const uint16 *text, const uint16 *caption);
	bool onCharacter(byte character);
	bool callExitHook();
	bool startupModules(CDrawContext *drawContext);
	void shutdownModules();
	bool openGameview(const char *bootParam);
	void closeGameview();

	static Common::Array<Common::Keymap *> initKeymapsEmi(const char *target);
	static bool readSaveStrings(Common::SeekableReadStream *stream, Common::Array<Common::String> &strings);
	static Graphics::Surface *readSaveThumbnail(Common::SeekableReadStream *stream, bool isPs2 = false);
	static Common::String getSaveDescription(const Common::Array<Common::String> &strings, int slot);
	static bool isLegacySaveFile(const Common::String &fileName);
	static Common::String getSavePrefix(Common::Platform platform, Common::Language language);
	static Common::String applySavePrefix(const Common::String &name, Common::Platform platform, Common::Language language);
	static uint getMemoryCardId();
	static bool hasMemoryCardSaves();
	bool deleteSaveFile(const char *fileName);
	bool copySaveFile(const char *sourceName, const char *targetName);

	const char *justLoadedFileName();
	bool justSaved();
	bool armSpuClear();
	bool isSpuClearArmed();
	void startSetChangeFrames();
	bool isSetChanging();

	const char *getUpdateFilename() override;
	Common::Error loadGameState(int slot) override;
	void syncSoundSettings() override;
	void applyGameSettings() override;

	void saveGame(CDrawContext *drawContext);
	void loadGame(CDrawContext *drawContext);

protected:
	Common::Error run() override;
	void pauseEngineIntern(bool pause) override;

private:
	friend class Actor;
	friend class ActorPs2;
	friend class ANode;
	friend class AInst;
	friend class Background;
	friend class ChoreInstance;
	friend class CLightNode;
	friend class COGLContext;
	friend class DemoReplay;
	friend class CTextFont;
	friend class CTinyGLContext;
	friend class CWeightMap;
	friend class DimRegion;
	friend class EmiRegistry;
	friend class EventGenerator;
	friend class EventHandler;
	friend class LConsoleServices;
	friend class Layer;
	friend struct LipData;
	friend class Lua_V2;
	friend class Movie;
	friend class Polyglot_v2;
	friend class ProfilerTimer;
	friend class Puck_v2;
	friend class Puck_v2Ps2;
	friend class RMesh;
	friend class RMeshInstance;
	friend class Room_v2;
	friend class Sounds;
	friend class SputControl_v2;
	friend class TextEvent;

	enum {
		kNumStatsEvents = 5
	};

	enum {
		kPadSquare = 0,
		kPadTriangle = 1,
		kPadCircle = 2,
		kPadCross = 3,
		kPadR1 = 4,
		kPadL1 = 5,
		kPadR2 = 6,
		kPadL2 = 7,
		kPadStart = 8,
		kPadSelect = 9,
		kPadDpadUp = 10,
		kPadDpadDown = 11,
		kPadDpadLeft = 12,
		kPadDpadRight = 13,
		kPadLeftStick = 14,
		kPadRightStick = 15
	};

	enum {
		kActionToggleMovement = 100
	};

	void mountEmiPatchArchives();
	bool initialize();
	void playAspyrLogo(CDrawContext *drawContext);
	void showLoadScreenPs2(CDrawContext *drawContext);
	void shutdown();
	bool runFrame();
	void pumpEvents();
	void handleKeyPress(const Common::KeyState &key, bool repeat);
	void handleKeyRelease(const Common::KeyState &key);
	void handleDebugKeys(int virtualKey);
	void injectMovementToggleKey();
	void syncMovementOption();
	void handleActivation(bool active);
	int mapPadButton(int button);
	static void addPadAction(Common::Keymap *keymap, const char *id, const Common::U32String &label, int padButton, const char *input);
	static void addPadActionsPs2(Common::Keymap *keymap);
	static void addKeyActionsPc(Common::Keymap *keymap);
	void showSplash(CDrawContext *drawContext);
	void pixelTest();
	float updateFrameTime(int manualMs);
	void drawStatsOverlay(CDrawContext *drawContext);
	void dumpFrame();
	void applyPendingGameSettings(int manualMs);
	void applyPendingMusicState();
	bool createDemoReplay(int replayMode, const char *name);
	void destroyDemoReplay();
	void applyReplayRequest();

	CDrawContext *createDrawContext();
	static bool openDrawContext(CDrawContext *drawContext, DisplayMode *mode);
	void saveTga(const char *path);

	static int writeSaveData(void *data, int bytes);
	static int readSaveData(void *data, int bytes);
	static void halveBitmap(LABitmap *source, LABitmap *destination);
	static void scaleToThumbnail(LABitmap *source, LABitmap *destination);
	LABitmap *getScreenBitmap();
	Common::String getSaveFileName(const char *path);
	static bool readSavedText(SaveRestoreAccess *accessFunc, char *buffer, int bufferSize);
	static void writeSavedText(SaveRestoreAccess *accessFunc, const LString &text);
	void saveGameData(SaveRestoreAccess *accessFunc);
	void clearGameState();
	bool restoreGameData(SaveRestoreAccess *accessFunc);
	bool restoreGameDataFromRam(const byte *data, uint size);
	static bool writeMemoryCardId(uint id);

	void updateSounds(bool force);
	bool updateMovie(float frameTime);
	void startupMovies();
	void releaseSubtitleFont();
	void setMusicState(int musicState, int startMs);

	// Memory service, created first and deleted last
	LMemoryServices *_memoryServices;

	// Services
	LConsoleServices *_consoleServices;
	LConsoleServices **_consoles;
	uint _consoleCount;
	uint _consoleArraySize;
	CGrimeConsole *_grimeConsole;
	Common::Array<LFileServices *> _fileServicesStack;
	LFileServices *_fileServices;
	CPathingFileServices *_pathingFileServices;
	ZFileServices *_zFileServices;
	CBundleServices *_bundleServices;
	LResourceManager _defaultResourceManager;
	LResourceManager *_resourceManager;
	ResourceManager *_gameResourceManager;
	CDrawContext *_drawContext;
	Overworld *_overworld;
	InputRecorder *_inputRecorder;
	Lua_V2 *_lua;
	KeyGenerator *_keyGenerator;
	Polyglot_v2 *_polyglot;

	// Instance modules
	Sounds *_sounds;
	SputControl_v2 *_control;
	Joystick *_joystick;
	PrimitiveQueue_v2 *_primitiveQueue;

	// Engine state
	bool _modulesStarted;
	bool _gameviewOpen;
	int _screenshotCount;
	int _frameCount;
	float _rawFrameTime;
	double _lastStamp;
	bool _firstFrame;
	bool _backspaceHeld;
	bool _controlHeld;
	TextEvent *_statsEvents[kNumStatsEvents];
	ZFile *_saveFile;
	LABitmap *_screenBitmap;
	int _mode;
	int _pausedMode;
	int _discNumber;
	int _frameDumpCount;
	LString _frameDumpPath;
	int _savedMode;
	int _cpuSpeed;
	LString _island;
	float _frameTime;
	bool _dumpingFrames;
	bool _saveLoadIdle;
	int _frontCopyCount;
	int _pendingMusicState;
	bool _gameSettingsPending;
	int _controlSkipFrames;

	DemoReplay *_demoReplay = nullptr;
	int _replayRequest = -1;
	LString _replayRequestName;
	int _replayMusicFrames = 0;
	int _demoIndex = -1;
	int _demoCount = 0;
	LinearRandom _random;

	int _libJustSaved = 0;
	int _soundRestartFrames = 0;
	int _setChangeFrames = 0;
	bool _spuClearArmed = false;
	int _movementSyncFrames = 0;
	bool _movementSyncWasCameraRelative = false;

	// Registry
	Common::String _registrySyncedSnapshot;

	// Actor
	Common::List<Actor *> _actors;
	int _nextActorId;
	Actor *_selectedActor;
	Actor *_speakers[Actor::kNumSpeakers];
	int _numSpeakers;
	int _speechMode;
	int _actorTextRate;
	IRect _offscreenTextRect;
	Actor *_collisionActors[Actor::kMaxCollisionActors];
	int _collisionCount;
	CDCVertexArray *_shadowVertices;
	CVector _hitPoint;
	CVector _collisionOldPosition;
	CVector _collisionNewPosition;

	// Puck
	int _puckRecursionDepth;

	// Chore instances
	Common::HashMap<uint, ChoreInstance *> _choreInstances;
	uint _nextChoreInstanceId;
	bool _choreInstancesStarted;

	// Rooms
	bool _roomsStarted;
	Room_v2 *_currentRoom;
	LResourceHandle<Room_v2> _currentRoomHandle;
	Room_v2 *_lastDrawnRoom;
	int _lastDrawnSetup;

	// Text events
	Common::List<TextEvent *> _textEvents;
	TextEvent *_defaultTextEvent;
	int _textMode;
	int _textScreenRect[4];
	int _nextTextEventId;
	bool _textEventsStarted;
	bool _cjkMode;
	bool _isDemo;
	int _glyphAdvance;
	Common::HashMap<uint16, CTexture *> _glyphMap;

	// Movies
	Movie *_currentMovie;
	int _movieFirstFrame;
	bool _ps2MovieRequested;
	LString _ps2MovieName;
	Common::WriteStream *_ps2StateWriter;
	Common::ReadStream *_ps2StateReader;
	CTexture *_loadingIconPs2;

	// Events
	Common::List<EventGenerator *> _eventGenerators;
	Common::List<Event *> _eventQueue;
	Common::List<EventHandler *> _eventHandlers;

	// Dim regions
	DimRegion *_dimRegionHead;
	DimRegion *_dimRegionTail;
	int _dimRegionCount;
	int _nextDimRegionId;

	// Animation instances
	Common::List<AInst *> _animInstances;

	// Lights
	Common::List<CLightNode *> _activeLights;

	// Layers
	int _nextLayerId;
	bool _layer16BitFix;

	// Lip data
	byte _lipBuffer[LipData::kBufferSize];
	LString _lipFileName;
	LipData::MouthEntry _lipMouthTable[LipData::kMouthTableSize];

	// Profiler
	ProfilerTimer *_profilerOldest;
	ProfilerTimer *_profilerNewest;
	int _profilerTimerCount;
	ProfilerTimer *_profilerCallStack[ProfilerTimer::kMaxCallStack];
	int _profilerCallStackDepth;
	int _profilerFrameCounter;
	int _profilerSnapshotInterval;
	uint64 _profilerCalibrationTicks;
	double _profilerCalibrationSeconds;
	double _profilerSecondsPerTick;
	int _profilerSecondsPerTickCalls;
	ProfilerTimer _rootTimer;

	// Timers, constructed in this order
	ProfilerTimer _actorCalcOnScreenTimer;
	ProfilerTimer _actorDrawTimer;
	ProfilerTimer _actorDrawShadowTimer;
	ProfilerTimer _updateAllActorsTimer;
	ProfilerTimer _updateANodeTimer;
	ProfilerTimer _oglOutputDrawPolyTimer;
	ProfilerTimer _oglSetPolyOptionsTimer;
	ProfilerTimer _oglSetTexturesTimer;
	ProfilerTimer _oglSubmitVerticesTimer;
	ProfilerTimer _oglDrawTrianglesTimer;
	ProfilerTimer _oglDrawQuadsTimer;
	ProfilerTimer _oglDrawElementsTimer;
	ProfilerTimer _tinyglOutputDrawPolyTimer;
	ProfilerTimer _tinyglSetPolyOptionsTimer;
	ProfilerTimer _tinyglSetTexturesTimer;
	ProfilerTimer _tinyglSubmitVerticesTimer;
	ProfilerTimer _tinyglDrawTrianglesTimer;
	ProfilerTimer _tinyglDrawQuadsTimer;
	ProfilerTimer _tinyglDrawElementsTimer;
	ProfilerTimer _weightMapDeformTimer;
	ProfilerTimer _updateManualTimer;
	ProfilerTimer _renderUpdateTimer;
	ProfilerTimer _layerDrawTimer;
	ProfilerTimer _rmeshLightingTimer;
	ProfilerTimer _rmeshOutputTimer;
	ProfilerTimer _rmeshUpdateTimer;
	ProfilerTimer _rmeshComputeTransformsTimer;
	ProfilerTimer _rmeshFutzingTimer;
	ProfilerTimer _roomDrawTimer;
	ProfilerTimer _soundsUpdateTimer;
	ProfilerTimer _controlUpdateTimer;
	ProfilerTimer _textUpdateTimer;
};

} // End of namespace Grim

#endif
