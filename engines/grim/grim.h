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

#ifndef GRIM_ENGINE_H
#define GRIM_ENGINE_H

#include "engines/engine.h"
#include "engines/advancedDetector.h"

#include "common/str-array.h"
#include "common/hashmap.h"
#include "common/events.h"
#include "common/mutex.h"

#include "graphics/renderer.h"

#include "engines/grim/detection.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/std/std_sys.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Grim {

class BlastText;
class Commentary;
class DialogLog;
class HdCanvas;
class IMuse;
class RemasteredLookRenderer;
class NormalText;
class Polyglot;
class RenderDroid;
class Res;
class StdBitmap;
class StdBmp;
class StdConffile;
class StdControl;
class StdFile;
class StdGif;
class StdLbm;
class Std3D;
class StdMath;
class StdPcx;
class StdPic;
class Text;
class WuRegistry;
class SputActorModule;
class SputControl;
class SputCostume;
class SputPrimitive;
class SputRender;
class SputResource;
class SputMouse;
class SputRoom;
class SputSmush;
class SputText;
class SputTime;
class StdDisplay;
class SysStruct;
class WalkSystem;
struct Std3DDevice;
struct StdRect;
struct StdVBuffer;

class LuaBase;
class Lua_V1;
class LegacyImporter;

enum SputMainModule {
	kSputMainModuleFileUtil,
	kSputMainModuleMemory,
	kSputMainModuleRoom,
	kSputMainModuleTime,
	kSputMainModuleLua,
	kSputMainModuleCostume,
	kSputMainModuleActor,
	kSputMainModuleResource,
	kSputMainModuleControl,
	kSputMainModuleSounds,
	kSputMainModuleRender,
	kSputMainModuleText,
	kSputMainModulePrimitive,
	kSputMainModuleSmush,
	kNumSputMainModules
};

enum {
	kNumBaseModules = 8,
	kNumMainModules = 6,
	kNumPerfCounters = 13,
	kPhysicalRam = 0x7FFFFFFF
};

struct PerfCounter {
	const char *name;
	double start;
	double elapsed;
};

struct GrimVideoDevice {
	uint numModes;
	StdDisplayMode *modes;
	uint num3DDevices;
	Std3DDevice *a3DDevices;
};

class GrimEngine : public Engine, public SysStruct {
	friend class Lua_V1;
	friend class LuaBase;
	friend class LegacyImporter;

protected:
	// Engine APIs
	Common::Error run() override;

public:
	GrimEngine(OSystem *syst, uint32 gameFlags, GrimGameType gameType, Common::Platform platform, Common::Language language);
	virtual ~GrimEngine();

	uint32 getGameFlags() { return _gameFlags; }
	GrimGameType getGameType() { return _gameType; }
	Common::Language getGameLanguage() { return _gameLanguage; }
	Common::Platform getGamePlatform() { return _gamePlatform; }
	virtual const char *getUpdateFilename();
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override { return true; }
	Common::Error loadGameState(int slot) override;
	bool isRemastered() const { return !!(_gameFlags & ADGF_REMASTERED); }
	bool useRemasterFixes() const { return _remasterFixes != 0; }
	StdControl *getStdControl() const { return _stdControl; }

	enum {
		kLanguageEnglish,
		kLanguageGerman,
		kLanguageSpanish,
		kLanguageFrench,
		kLanguageItalian,
		kLanguagePortuguese,
		kNumLanguages
	};

	enum {
		kRenderingModeHd = 0,
		kRenderingModeClassic = 1
	};

	enum {
		kLibModeNormal = 2,
		kLibModeUser = 4,
		kLibModeMenu = 5,
		kLibModeExiting = 6
	};

	int getLanguage() const { return _language; }
	void setLanguage(int language);
	const char *getLanguageCode() const;
	static int getLanguageCount() { return kNumLanguages; }
	int getRenderingMode() const;
	void setRenderingMode(int mode);

	void clearEventQueue();

	// sputMain module
	int sputInitialize(SysStruct *hs);
	int sputTerminate();
	int sputStartup();
	int sputShutdown();
	int callRegisterModules(SysStruct *hs, int firstModule, int count);
	int runInitTable(int firstModule, int count);
	int callTableFunc(int firstModule, int count);
	int callRelease(int firstModule, int count);
	int bundleDofile();
	int startEngine(int saveGameResume, const char *bootParam);
	int loop(int render, int manualTicks);
	void timedPageFlip();
	void setPerfCounter(uint slot);
	int queryPerfCounterDiff(uint slot);
	void save();
	int zwrite(const void *buffer, int size);
	void restore();
	int zread(void *buffer, int size);

	void requestRemasteredSave(int slot, const char *mural, int setIndex, const char *title);
	void requestRemasteredLoad(int slot, bool loadLast);
	void requestRemasteredErase(int slot);
	void requestRemasteredNewGame();
	int getRemasteredSaveStatus() const { return _remasteredSaveStatus; }
	StdVBuffer *getSaveGameImage(const char *fileName);
	void setPrevMode(int mode);
	int getPrevMode() const;
	void imuseSave();
	void imuseRestore();
	PerfCounter *getPerfCounters() { return _perfCounters; }
	IMuse *getImuse() const { return _imuse; }
	SputCostume *getSputCostume() const { return _sputCostume; }
	SputResource *getSputResource() const { return _sputResource; }
	HdCanvas *getHdCanvas() const { return _hdCanvas; }
	bool isWidescreenStretched();
	SputMouse *getSputMouse() const { return _sputMouse; }
	DialogLog *getDialogLog() const { return _dialogLog; }
	Commentary *getCommentary() const { return _commentary; }

	// grimlib module
	void overrideSelectedActorCostume(int instance);
	void restoreSelectedActorCostume();
	int setDisplay(StdVBuffer *vBuffer, StdVBuffer *zBuffer);
	int unsetDisplay();
	void makeCurrentSet(const char *name);
	void close();
	void setDebugToggleCallback(void (*func)(int));
	void setPaletteCallback(int (*func)(byte *, int));
	void setShutdownCallback(int (*func)(int));
	void setPageFlipCallback(int (*func)());
	void using3DHardware(int b3D);
	int addResourcePath(const char *path);
	int findResources();
	void markDirtyRect(const StdRect *rectPointer, int page);
	void makeCurrentSetupLib(uint setup);
	void nextSetup();
	void previousSetup();
	int confirmClose();
	void requestShutdown(int arg);
	void setPause(int pause);
	int doPauseCallback();
	int characterCallback(int character);
	int setImuseState(int state);
	int playMovie(const char *name, int fullscreen, int looping, int x, int y);
	int userPaint();
	void saveLib(const char *fileName);
	void loadLib(const char *fileName);
	int justLoaded();
	int getLibMode() const { return _libMode; }
	void setLibMode(int mode) { _libMode = mode; }
	int getTextCanvasWidth() const { return _textCanvasWidth; }
	void setTextCanvasWidth(int width) { _textCanvasWidth = width; }
	int getDebugKeys() const { return _debugKeys; }
	void setDebugKeys(int debugKeys) { _debugKeys = debugKeys; }
	const StdColorFormat *getColorInfo() const { return &_colorInfoCopy; }

	// grim module
	Common::SeekableReadStream *openFile(const char *path) override;
	Common::SeekableReadStream *openStreamFile(const Common::String &fname);

	// Engine APIs
	bool hasFeature(EngineFeature f) const override;

	static Common::Array<Common::Keymap *> initKeymapsGrim(const char *target);

	Common::StringArray _listFiles;
	Common::StringArray::const_iterator _listFilesIter;

	void debugLua(const Common::String &str);

protected:
	void pauseEngineIntern(bool pause) override;

	enum {
		kStickActionFirst = 12000,
		kNumStickAxes = 4
	};

	Common::Mutex _imuseMutex;
	unsigned _speedLimitMs;
	bool _showFps;

	uint32 _gameFlags;
	GrimGameType _gameType;
	Common::Platform _gamePlatform;
	Common::Language _gameLanguage;

	// sputMain and grimlib modules
	int initializeModule(int module, SysStruct *hs);
	int startupModule(int module);
	void shutdownModule(int module);
	void terminateModule(int module);
	void soundsSave(Common::WriteStream *stream);
	void soundsRestore(Common::SeekableReadStream *stream, bool legacyLayout = false);
	void restorePrologue();
	void restoreEpilogue();
	void restoreLegacy();
	void saveRemastered();
	void restoreRemastered();
	void eraseRemastered();
	void newGameRemastered();
	static int saveWriteCallback(void *buffer, int size);
	static int saveReadCallback(void *buffer, int size);

	// grim module
	bool checkGameData(const char *gameName);
	Common::Error runGrime();
	void mountPatchArchives();
	void createImuse();
	void createModules();
	void destroyModules();
	int grimStartup();
	void initLanguage();
	void loadTranslations();
	void grimMain();
	void textOut(const char *text, int x, int y);
	void grimShutdown();
	void fatalError(const char *text, const char *caption);
	uint getHardwareBackends() const;
	int isBackendUsable(int backend);
	int hasUsableHardwareDevice();
	int chooseHardwareBackend();
	int initDisplay();
	void freeVideoDevices();
	int initDisplayAndDevice();
	int selectFallbackDisplay(int failedDevice, int want3D);
	int loadDisplaySettings(int *devicePointer, int *mode, int *p3DDevice);
	int chooseDefaultDisplay(int preferWindowed, int want3D, int *devicePointer, int *mode, int *p3DDevice);
	int selectDisplayMode(int deviceIndex, int modeIndex, int threeDIndex);
	int select3DDevice(int index);
	void shutdown3D();
	void shutdownDisplay();
	int getCurVideoDeviceIndex();
	int getCur3DDeviceIndex();
	int isHardware3D();
	void loadSplash();
	void pumpEvents();
	static void addKeyAction(Common::Keymap *keymap, const char *id, const Common::U32String &label, const Common::KeyState &key, const char *keyboardInput, const char *padInput, const char *padInput2 = nullptr);
	static void addStickAction(Common::Keymap *keymap, const char *id, const Common::U32String &label, int stickAction, const char *input);
	static void addStickActions(Common::Keymap *keymap);
	static int applyStickAction(int *axes, const Common::Event &event);
	static int getJoystickButtonIndex(int button, bool remastered = false);
	void handleFocus(int hasFocus);
	void handleKey(Common::EventType type, const Common::KeyState &key, bool repeat);
	void handleKeyDebug(const Common::KeyState &key);
	void addKeyboardEvent(uint offset, int data);
	void addMouseButtonEvent(uint offset, int data);
	static int shutdownCallback(int arg);
	static int paletteCallback(byte *palette, int alpha);
	static int pageFlipCallback();

	StdMath *_stdMath = nullptr;
	StdFile *_stdFile = nullptr;
	StdConffile *_stdConffile = nullptr;
	StdBitmap *_stdBitmap = nullptr;
	StdLbm *_stdLbm = nullptr;
	StdPcx *_stdPcx = nullptr;
	StdGif *_stdGif = nullptr;
	StdPic *_stdPic = nullptr;
	StdBmp *_stdBmp = nullptr;
	RenderDroid *_rd = nullptr;
	StdControl *_stdControl = nullptr;
	WuRegistry *_wuRegistry = nullptr;
	Polyglot *_polyglotV1 = nullptr;
	Text *_text = nullptr;
	BlastText *_blastText = nullptr;
	NormalText *_normalText = nullptr;
	StdVBuffer *_backBuffer = nullptr;
	StdVBuffer *_zBuffer = nullptr;
	StdColorFormat _colorInfoCopy = { 1, 16, 5, 6, 5, 11, 5, 0, 3, 2, 3, 0, 0, 0 };
	int _numDrawBuffers = 1;
	int _hasFocus = 1;
	int _firstFrameFlag = 0;
	int _quit = 0;
	int _displayOpen = 0;
	int _showDebugOverlay = 0;
	int _backspaceHeldFlag = 0;
	int _stickAxes[kNumStickAxes] = { 0, 0, 0, 0 };
	int _enginePaused = 0;
	int _saveGameToResume = 0;
	int _grimFrameCount = 0;
	int _grimLastFpsFrame = 0;
	uint32 _grimLastFpsTime = 0;
	double _grimFps = 0.0;
	int _screenshotCounter = 0;
	int _zbufShotCounter = 0;
	int _enumerateVideo = 0;
	int _show = 0;
	int _useHardware = 0;
	uint _hardwareBackends = 0;
	int _preferredBackend = kStdDisplayBackendNone;
	int _remasterFixes = 0;
	int _language = kLanguageEnglish;
	int _renderingMode = kRenderingModeClassic;
	int _pendingDevice = 0;
	int _pendingMode = 0;
	int _pending3DDevice = -1;
	int _displayChangePending = 0;
	Std3D *_std3D = nullptr;
	GrimVideoDevice _videoDevices[kStdDisplayMaxDevices] = {};
	StdDisplayMode _videoDeviceConfig = {};

	Res *_res = nullptr;
	StdDisplay *_stdDisplay = nullptr;
	HdCanvas *_hdCanvas = nullptr;
	RemasteredLookRenderer *_remasteredLookRenderer = nullptr;
	SputResource *_sputResource = nullptr;
	SputRoom *_sputRoom = nullptr;
	SputTime *_sputTime = nullptr;
	SputCostume *_sputCostume = nullptr;
	SputActorModule *_sputActor = nullptr;
	SputControl *_sputControl = nullptr;
	SputRender *_sputRender = nullptr;
	SputText *_sputText = nullptr;
	SputPrimitive *_sputPrimitive = nullptr;
	SputSmush *_sputSmush = nullptr;
	SputMouse *_sputMouse = nullptr;
	DialogLog *_dialogLog = nullptr;
	Commentary *_commentary = nullptr;
	Lua_V1 *_luaV1 = nullptr;
	WalkSystem *_walkSystem = nullptr;
	StdVBuffer *_mainGfxBuffer = nullptr;

	SysStruct *_sputHS = nullptr;
	int _sputInitialized = 0;
	int _sputStarted = 0;
	int _loopCount = 0;
	int _renderPending = 0;
	int _displayPrevMode = 0;
	int _runLua = 1;
	int _sputPrevMode = 0;
	PerfCounter _perfCounters[kNumPerfCounters] = {
		{ "Grimlib", 0.0, 0.0 }, { "Controls", 0.0, 0.0 }, { "Lua", 0.0, 0.0 }, { "Actors", 0.0, 0.0 }, { "Rects", 0.0, 0.0 },
		{ "2D", 0.0, 0.0 }, { "3D", 0.0, 0.0 }, { "Rasterize", 0.0, 0.0 }, { "Display", 0.0, 0.0 }, { "ISO", 0.0, 0.0 },
		{ "Sound", 0.0, 0.0 }, { "Shadows", 0.0, 0.0 }, { "WAITICON", 0.0, 0.0 }
	};
	Common::WriteStream *_saveWriteStream = nullptr;
	Common::SeekableReadStream *_saveReadStream = nullptr;

	int _libMode = 0;
	int _textCanvasWidth = 1440;
	int _libPauseOldMode = 0;
	int _libPauseSavedMode = 0;
	int _libPauseSavedOldMode = 0;
	int _debugKeys = 0;
	int _sputOpen = 0;
	int _libSaveRequest = 0;
	int _libLoadRequest = 0;
	Common::String _libSaveGameFileName;
	int _libJustLoaded = 0;

	int _remasteredSaveStatus = 0;
	int _remasteredSaveSlot = 0;
	int _remasteredSaveSetIndex = 0;
	Common::String _remasteredSaveMural;
	Common::String _remasteredSaveTitle;
	int _remasteredLoadSlot = 0;
	bool _remasteredLoadLast = false;
	int _remasteredEraseRequest = 0;
	int _remasteredEraseSlot = 0;
	int _remasteredNewGameRequest = 0;
	int _libImuseState = -1;
	int (*_shutdownCallback)(int) = nullptr;
	int _libRenderOptions[8] = { 4, 3, 1, 1, 0, 0, 2, 0 };

private:
	IMuse *_imuse = nullptr;
};

extern GrimEngine *g_grim;

} // end of namespace Grim

#endif
