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

#include "engines/grim/emi/emiengine.h"

#include "audio/mixer.h"

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/str.h"
#include "common/textconsole.h"
#include "common/translation.h"
#include "common/ustr.h"

#include "gui/message.h"

#include "engines/grim/debug.h"
#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/resources/cbundle.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/emi_registry.h"
#include "engines/grim/emi/emidebugger.h"
#include "engines/grim/emi/input/demoreplay.h"
#include "engines/grim/emi/input/inputrecorder.h"
#include "engines/grim/emi/input/joystick.h"
#include "engines/grim/emi/input/keygenerator.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/sound/lipdata.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/movie/movie.h"
#include "engines/grim/emi/scene/overworld.h"
#include "engines/grim/emi/text/cjkstring.h"
#include "engines/grim/emi/text/ctextfont.h"
#include "engines/grim/emi/text/polyglot_v2.h"
#include "engines/grim/emi/graphics/primitivequeue_v2.h"
#include "engines/grim/emi/resources/resourcemanager.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/core/cpathingfileservices.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/emi/input/sputcontrol_v2.h"
#include "engines/grim/emi/text/textevent.h"
#include "engines/grim/emi/files/zfile.h"
#include "engines/grim/inputdialog.h"
#include "engines/grim/lab.h"
#include "engines/grim/update/update.h"

namespace Grim {

static const uint kMaxHardCap = 0x4000000;
static const uint kReservedMemory = 0x800000;
static const int kMaxDemos = 10;

EmiEngine::EmiEngine(OSystem *syst, uint32 gameFlags, GrimGameType gameType, Common::Platform platform, Common::Language language) :
		GrimEngine(syst, gameFlags, gameType, platform, language),
		_memoryServices(new LMemoryServices()),
		_consoleServices(nullptr), _consoles(nullptr), _consoleCount(0), _consoleArraySize(0), _grimeConsole(nullptr),
		_fileServicesStack(), _fileServices(nullptr), _pathingFileServices(nullptr), _zFileServices(nullptr),
		_bundleServices(nullptr),
		_defaultResourceManager(), _resourceManager(&_defaultResourceManager), _gameResourceManager(nullptr),
		_drawContext(nullptr), _overworld(nullptr), _inputRecorder(nullptr), _lua(nullptr), _keyGenerator(nullptr),
		_polyglot(nullptr),
		_sounds(nullptr), _control(nullptr), _joystick(nullptr), _primitiveQueue(nullptr),
		_modulesStarted(false), _gameviewOpen(false), _screenshotCount(0), _frameCount(0), _rawFrameTime(0.0f),
		_lastStamp(0.0), _firstFrame(true), _backspaceHeld(false), _controlHeld(false), _statsEvents(),
		_saveFile(nullptr), _screenBitmap(nullptr), _mode(0), _pausedMode(0), _discNumber(0), _frameDumpCount(0),
		_frameDumpPath(), _savedMode(0), _cpuSpeed(1000), _island(), _frameTime(0.0f), _dumpingFrames(false),
		_saveLoadIdle(true), _frontCopyCount(0), _pendingMusicState(-1), _gameSettingsPending(false),
		_controlSkipFrames(0),
		_registrySyncedSnapshot(),
		_actors(), _nextActorId(1), _selectedActor(nullptr), _speakers(), _numSpeakers(0), _speechMode(2),
		_actorTextRate(6), _offscreenTextRect(), _collisionActors(), _collisionCount(0), _shadowVertices(nullptr),
		_hitPoint(), _collisionOldPosition(), _collisionNewPosition(),
		_puckRecursionDepth(0),
		_choreInstances(), _nextChoreInstanceId(1), _choreInstancesStarted(false),
		_roomsStarted(false), _currentRoom(nullptr), _currentRoomHandle(this), _lastDrawnRoom(nullptr),
		_lastDrawnSetup(-1),
		_textEvents(), _defaultTextEvent(nullptr), _textMode(0), _textScreenRect(), _nextTextEventId(1),
		_textEventsStarted(false),
		_cjkMode(language == Common::KO_KOR || language == Common::ZH_CHN || language == Common::ZH_TWN),
		_isDemo((gameFlags & ADGF_DEMO) != 0),
		_glyphAdvance(16), _glyphMap(),
		_currentMovie(nullptr), _movieFirstFrame(1), _ps2MovieRequested(false), _ps2MovieName(),
		_ps2StateWriter(nullptr), _ps2StateReader(nullptr), _loadingIconPs2(nullptr),
		_eventGenerators(), _eventQueue(), _eventHandlers(),
		_dimRegionHead(nullptr), _dimRegionTail(nullptr), _dimRegionCount(0), _nextDimRegionId(0),
		_animInstances(),
		_activeLights(),
		_nextLayerId(1), _layer16BitFix(true),
		_lipBuffer(), _lipFileName(), _lipMouthTable(),
		_profilerOldest(nullptr), _profilerNewest(nullptr), _profilerTimerCount(0), _profilerCallStack(),
		_profilerCallStackDepth(0), _profilerFrameCounter(0), _profilerSnapshotInterval(5),
		_profilerCalibrationTicks(0), _profilerCalibrationSeconds(0.0), _profilerSecondsPerTick(0.0),
		_profilerSecondsPerTickCalls(0),
		_rootTimer(this, "Root", 0),
		_actorCalcOnScreenTimer(this, "Actor::CalcOnScreen()", ProfilerTimer::kTrackNesting),
		_actorDrawTimer(this, "Actor::Draw()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_actorDrawShadowTimer(this, "Actor::DrawShadow()", ProfilerTimer::kTrackNesting),
		_updateAllActorsTimer(this, "Actor::UpdateAllActors()", ProfilerTimer::kTrackNesting),
		_updateANodeTimer(this, "ANode::UpdateANode()", ProfilerTimer::kTrackNesting),
		_oglOutputDrawPolyTimer(this, "OGL::OutputDrawPoly()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_oglSetPolyOptionsTimer(this, "OGL::SetPolyOptions()", ProfilerTimer::kTrackNesting),
		_oglSetTexturesTimer(this, "OGL::SetTextures()", ProfilerTimer::kTrackNesting),
		_oglSubmitVerticesTimer(this, "OGL::SubmitVertices()", ProfilerTimer::kTrackNesting),
		_oglDrawTrianglesTimer(this, "OGL::DrawTriangles()", ProfilerTimer::kTrackNesting),
		_oglDrawQuadsTimer(this, "OGL::DrawQuads()", ProfilerTimer::kTrackNesting),
		_oglDrawElementsTimer(this, "glDrawElements()", ProfilerTimer::kTrackNesting),
		_tinyglOutputDrawPolyTimer(this, "TinyGL::OutputDrawPoly()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_tinyglSetPolyOptionsTimer(this, "TinyGL::SetPolyOptions()", ProfilerTimer::kTrackNesting),
		_tinyglSetTexturesTimer(this, "TinyGL::SetTextures()", ProfilerTimer::kTrackNesting),
		_tinyglSubmitVerticesTimer(this, "TinyGL::SubmitVertices()", ProfilerTimer::kTrackNesting),
		_tinyglDrawTrianglesTimer(this, "TinyGL::DrawTriangles()", ProfilerTimer::kTrackNesting),
		_tinyglDrawQuadsTimer(this, "TinyGL::DrawQuads()", ProfilerTimer::kTrackNesting),
		_tinyglDrawElementsTimer(this, "tglDrawElements()", ProfilerTimer::kTrackNesting),
		_weightMapDeformTimer(this, "CWeightMap::DeformVerts()", ProfilerTimer::kTrackNesting),
		_updateManualTimer(this, "grimlib_UpdateManual()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_renderUpdateTimer(this, "sputRender_Update()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_layerDrawTimer(this, "Layer::Draw()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_rmeshLightingTimer(this, "Lighting", ProfilerTimer::kTrackNesting),
		_rmeshOutputTimer(this, "RMesh::Output()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_rmeshUpdateTimer(this, "RMesh::Update()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_rmeshComputeTransformsTimer(this, "CWeightMap::ComputeTransforms()", ProfilerTimer::kTrackNesting),
		_rmeshFutzingTimer(this, "Futzing", ProfilerTimer::kTrackNesting),
		_roomDrawTimer(this, "Room::Draw()", ProfilerTimer::kTrackNesting | ProfilerTimer::kPrintOther),
		_soundsUpdateTimer(this, "SoundsUpdate()", ProfilerTimer::kTrackNesting),
		_controlUpdateTimer(this, "sputControl_Update()", ProfilerTimer::kTrackNesting),
		_textUpdateTimer(this, "TextEvent::Update()", ProfilerTimer::kTrackNesting) {
	setDebugger(new EmiDebugger());

	EmiRegistry::registerDefaults();
	LipData::initMouthTable(this);
	syncSoundSettings();

	_polyglot = new Polyglot_v2(this);
}

EmiEngine::~EmiEngine() {
	delete _polyglot;
	_polyglot = nullptr;

	// Normally everything below is already gone after shutdown()
	delete _lua;
	_lua = nullptr;
	delete _primitiveQueue;
	_primitiveQueue = nullptr;
	delete _control;
	_control = nullptr;
	delete _joystick;
	_joystick = nullptr;
	delete _gameResourceManager;
	_gameResourceManager = nullptr;
	delete _sounds;
	_sounds = nullptr;

	destroyOverworld();
	destroyInputRecorder();
	destroyDemoReplay();

	CTextFont::freeGlyphMap(this);

	delete _drawContext;
	_drawContext = nullptr;

	delete _zFileServices;
	_zFileServices = nullptr;

	deleteAllConsoles();
	deleteAllFileServices();
	_pathingFileServices = nullptr;

	delete _keyGenerator;
	_keyGenerator = nullptr;

	delete _memoryServices;
}

LMemoryServices *EmiEngine::getMemoryServices() {
	return _memoryServices;
}

LConsoleServices *EmiEngine::getConsoleServices() {
	return _consoleServices;
}

LFileServices *EmiEngine::getFileServices() {
	return _fileServices;
}

CPathingFileServices *EmiEngine::getPathingFileServices() {
	return _pathingFileServices;
}

ZFileServices *EmiEngine::getZFileServices() {
	return _zFileServices;
}

CBundleServices *EmiEngine::getBundleServices() {
	return _bundleServices;
}

LResourceManager *EmiEngine::getResourceManager() {
	return _resourceManager;
}

ResourceManager *EmiEngine::getGameResourceManager() {
	return _gameResourceManager;
}

CDrawContext *EmiEngine::getDrawContext() {
	return _drawContext;
}

Overworld *EmiEngine::getOverworld() {
	if (!_overworld)
		_overworld = new Overworld(this);

	return _overworld;
}

InputRecorder *EmiEngine::getInputRecorder() {
	return _inputRecorder;
}

DemoReplay *EmiEngine::getDemoReplay() {
	return _demoReplay;
}

Lua_V2 *EmiEngine::getLua() {
	return _lua;
}

KeyGenerator *EmiEngine::getKeyGenerator() {
	return _keyGenerator;
}

Sounds *EmiEngine::getSounds() {
	return _sounds;
}

SputControl_v2 *EmiEngine::getControl() {
	return _control;
}

Joystick *EmiEngine::getJoystick() {
	return _joystick;
}

PrimitiveQueue_v2 *EmiEngine::getPrimitiveQueue() {
	return _primitiveQueue;
}

const Common::List<CLightNode *> &EmiEngine::getActiveLights() const {
	return _activeLights;
}

int EmiEngine::registerConsole(LConsoleServices *console) {
	LConsoleServices **oldConsoles = _consoles;
	int result;
	uint i;

	if (oldConsoles) {
		result = _consoleCount;

		if (_consoleCount >= _consoleArraySize) {
			if (_consoleCount == _consoleArraySize) {
				_consoles = (LConsoleServices **)_memoryServices->mallocImpl((_consoleArraySize + 4) * sizeof(LConsoleServices *));

				for (i = 0; i < _consoleArraySize; i++)
					_consoles[i] = oldConsoles[i];

				_consoleArraySize += 4;
				_memoryServices->freeImpl(oldConsoles);
				_consoles[_consoleCount] = console;
				_consoleServices = console;
				_consoleCount++;

				return _consoleCount;
			} else {
				_consoleServices = console;
			}
		} else {
			_consoles[_consoleCount] = console;
			result = _consoleCount + 1;
			_consoleServices = console;
			_consoleCount++;
		}
	} else {
		_consoleArraySize = 4;
		_consoles = (LConsoleServices **)_memoryServices->mallocImpl(_consoleArraySize * sizeof(LConsoleServices *));
		_consoleCount = 1;
		_consoles[0] = console;
		result = _consoleCount;
		_consoleServices = console;
	}

	return result;
}

void EmiEngine::deleteAllConsoles() {
	uint i;

	for (i = 0; i < _consoleCount; i++) {
		LConsoleServices *console = _consoles[i];

		if (console) {
			delete console;

			for (uint j = i; j < _consoleCount; j++) {
				if (_consoles[j] == console)
					_consoles[j] = nullptr;
			}
		}
	}

	_memoryServices->freeImpl(_consoles);
	_consoles = nullptr;
	_consoleCount = 0;
	_consoleArraySize = 0;
	_consoleServices = nullptr;
	_grimeConsole = nullptr;
}

int EmiEngine::pushFileServices(LFileServices *services) {
	_fileServicesStack.push_back(services);
	_fileServices = services;

	return _fileServicesStack.size();
}

LFileServices *EmiEngine::popFileServices() {
	if (_fileServicesStack.empty())
		return nullptr;

	LFileServices *services = _fileServicesStack.back();
	_fileServicesStack.pop_back();

	if (_fileServicesStack.empty()) {
		warning("EmiEngine::popFileServices(): no services left");
		_fileServices = nullptr;
	} else {
		_fileServices = _fileServicesStack.back();
	}

	return services;
}

uint EmiEngine::getNumFileServices() {
	return _fileServicesStack.size();
}

void EmiEngine::deleteAllFileServices() {
	while (!_fileServicesStack.empty()) {
		LFileServices *services = _fileServicesStack.back();
		_fileServicesStack.pop_back();

		delete services;
	}

	_fileServices = nullptr;
}

void EmiEngine::setResourceManager(LResourceManager *manager) {
	_resourceManager = manager;
}

void EmiEngine::setBundleServices(CBundleServices *services) {
	_bundleServices = services;
}

void EmiEngine::createInputRecorder(int mode) {
	if (_inputRecorder)
		return;

	_inputRecorder = new InputRecorder(this, mode);
}

void EmiEngine::destroyInputRecorder() {
	if (!_inputRecorder)
		return;

	delete _inputRecorder;
	_inputRecorder = nullptr;
}

bool EmiEngine::createDemoReplay(int replayMode, const char *name) {
	if (_demoReplay) {
		delete _demoReplay;
		_demoReplay = nullptr;
	}

	// The replay becomes visible only after its constructor, so restoring the state does not consume records
	DemoReplay *replay = new DemoReplay(this, replayMode, name);

	_demoReplay = replay;

	return true;
}

void EmiEngine::destroyDemoReplay() {
	if (!_demoReplay)
		return;

	delete _demoReplay;
	_demoReplay = nullptr;

	if (_consoleServices)
		_consoleServices->logDebug("Replay log DISPOSED\n");
}

void EmiEngine::setReplayRequest(int mode, const char *name) {
	// Recording demos is a developer feature: a stray key press must not start it
	if (mode == DemoReplay::kModeRecord && !ConfMan.getBool("game_devel_mode"))
		return;

	_replayRequest = mode;

	if (name)
		_replayRequestName = name;
	else
		_replayRequestName.clear();

	if (mode == 0)
		_sounds->killMusicAndWait(false);
}

void EmiEngine::applyReplayRequest() {
	int request = _replayRequest;

	_replayRequest = -1;

	if ((uint)request < 2)
		_replayMusicFrames = 3;

	if (request != 0) {
		createDemoReplay(request == 1 ? DemoReplay::kModePlayback : DemoReplay::kModeRecord, _replayRequestName.c_str());

		return;
	}

	if (_demoReplay && _demoReplay->_mode == DemoReplay::kModeFinished) {
		Movie::drawLoadingIconPs2(this);
		lua_dostring("system.currentSet:unlock_demo_resources()");
		lua_dostring("start_script(system.restart)");
	}

	destroyDemoReplay();
}

void EmiEngine::countDemos() {
	int count = 0;

	do {
		LString name = LString::format("DEMOS\\DEMO%d.DEM", count);

		if (!_fileServices->exists(name.c_str()))
			break;

		count++;
	} while (count < kMaxDemos);

	_demoCount = count;

	if (count > 0)
		_demoIndex = 0;
}

int EmiEngine::getNextDemoNum() {
	int current = _demoIndex;

	if (current >= 0) {
		_demoIndex = current + 1;

		if (_demoIndex == _demoCount)
			_demoIndex = 0;
	}

	return current;
}

int32 EmiEngine::nextRandom() {
	return static_cast<EmiEngine *>(g_grim)->_random.next();
}

void EmiEngine::seedRandom(uint32 seed) {
	static_cast<EmiEngine *>(g_grim)->_random.seed(seed);
}

void EmiEngine::destroyOverworld() {
	delete _overworld;
	_overworld = nullptr;
}

void EmiEngine::setDrawContext(CDrawContext *drawContext) {
	_drawContext = drawContext;
}

void EmiEngine::syncSoundSettings() {
	bool mute = ConfMan.hasKey("mute") && ConfMan.getBool("mute");

	_mixer->muteSoundType(Audio::Mixer::kPlainSoundType, mute);
	_mixer->muteSoundType(Audio::Mixer::kMusicSoundType, mute);
	_mixer->muteSoundType(Audio::Mixer::kSFXSoundType, mute);
	_mixer->muteSoundType(Audio::Mixer::kSpeechSoundType, mute);

	// The game applies its own volumes, the mixer ones stay at the maximum
	_mixer->setVolumeForSoundType(Audio::Mixer::kPlainSoundType, Audio::Mixer::kMaxMixerVolume);
	_mixer->setVolumeForSoundType(Audio::Mixer::kMusicSoundType, Audio::Mixer::kMaxMixerVolume);
	_mixer->setVolumeForSoundType(Audio::Mixer::kSFXSoundType, Audio::Mixer::kMaxMixerVolume);
	_mixer->setVolumeForSoundType(Audio::Mixer::kSpeechSoundType, Audio::Mixer::kMaxMixerVolume);
}

void EmiEngine::applyGameSettings() {
	if (EmiRegistry::changedSinceSync(this))
		_gameSettingsPending = true;
}

void EmiEngine::applyPendingGameSettings(int manualMs) {
	if (!_gameSettingsPending || (_mode == kModePaused && manualMs == 0))
		return;

	_gameSettingsPending = false;

	if (!_lua->callPrefsInit())
		warning("EmiEngine::applyPendingGameSettings(): system_prefs:init() is not available");
}

Polyglot_v2 *EmiEngine::getPolyglot() {
	return _polyglot;
}

const char *EmiEngine::getUpdateFilename() {
	if (getGamePlatform() != Common::kPlatformWindows || (getGameFlags() & ADGF_DEMO))
		return nullptr;

	if (SearchMan.hasFile("patch.m4b"))
		return nullptr;

	switch (getGameLanguage()) {
	case Common::FR_FRA:
		return "MonkeyUpdate_FRA.exe";
	case Common::DE_DEU:
		return "MonkeyUpdate_DEU.exe";
	case Common::IT_ITA:
		return "MonkeyUpdate_ITA.exe";
	case Common::PT_BRA:
		return "MonkeyUpdate_BRZ.exe";
	case Common::ES_ESP:
		return "MonkeyUpdate_ESP.exe";
	default:
		return "MonkeyUpdate.exe";
	}
}

void EmiEngine::mountEmiPatchArchives() {
	const char *updateFilename = getUpdateFilename();

	if (updateFilename && !SearchMan.hasArchive("update")) {
		Common::File *updateStream = new Common::File();

		if (updateStream->open(updateFilename)) {
			Common::Archive *update = loadUpdateArchive(updateStream);

			if (update)
				SearchMan.add("update", update, 1);
		} else {
			delete updateStream;
		}

		if (!SearchMan.hasArchive("update")) {
			Common::String urlForRequiredDataFiles = "https://wiki.scummvm.org/index.php?title=Escape_from_Monkey_Island#Required_data_files";
			Common::U32String errorMessage = Common::U32String::format(_("The original patch of Escape from Monkey Island\n"
					"is missing. Please put the file %s\n"
					"in the game data files directory.\n"
					"Go to %s to see a list\n"
					"of the needed files."), updateFilename, urlForRequiredDataFiles.c_str());

			GUIErrorMessageWithURL(errorMessage, urlForRequiredDataFiles.c_str());
			error("%s not found", updateFilename);
		}
	}

	// The demo needs none of our script patches
	if (_isDemo)
		return;

	if (!SearchMan.hasFile("monkey4-patch.m4b"))
		error("%s", "monkey4-patch.m4b not found");

	if (!SearchMan.hasArchive("monkey4-patch.m4b")) {
		Common::ArchiveMemberList files;

		SearchMan.listMatchingMembers(files, "monkey4-patch.m4b");

		if (!files.empty()) {
			Lab *lab = new Lab();

			if (lab->open(files.front()->getPathInArchive(), false))
				SearchMan.add("monkey4-patch.m4b", lab, 0, true);
			else
				delete lab;
		}
	}

	Common::ArchiveMemberList patches;

	Debug::debugImpl(Debug::Patchr, "%d patch files available", SearchMan.listMatchingMembers(patches, "*.patchr"));
}

void EmiEngine::setPause(bool pause) {
	if (!_mode)
		return;

	if (pause) {
		if (_mode == kModePaused)
			return;

		_pausedMode = _mode;
		_mode = kModePaused;
		_sounds->pauseMusicStreams();
		Movie::pauseCurrent(this);
	} else if (_mode == kModePaused) {
		_mode = _pausedMode;
		_sounds->resumeMusicStreams();
		Movie::resumeCurrent(this);
	}
}

int EmiEngine::getDiscNumber() {
	return _discNumber;
}

bool EmiEngine::findDisc(int disc) {
	if (disc != 1 && disc != 2)
		return false;

	// There is no CD, the disc is always found and the CD path is the game directory
	_pathingFileServices->setCdPath(".");
	_discNumber = disc;

	return true;
}

uint EmiEngine::getTotalPhysMem() {
	// TODO the original asks the system, this is a constant
	return 0x40000000;
}

int EmiEngine::messageBox(const char *caption, const char *text, uint flags) {
	Common::String message;

	if (caption && *caption) {
		message = caption;
		message += "\n\n";
	}

	if (text)
		message += text;

	Common::U32String dialogText(message);

	if (flags == kMessageBoxYesNo) {
		GUI::MessageDialog dialog(dialogText, _("Yes"), _("No"));

		if (dialog.runModal() == GUI::kMessageOK)
			return kMessageBoxYes;

		return kMessageBoxNo;
	}

	if (flags == kMessageBoxYesNoCancel) {
		Common::U32StringArray altButtons;

		altButtons.push_back(_("No"));
		altButtons.push_back(_("Cancel"));

		GUI::MessageDialog dialog(dialogText, _("Yes"), altButtons);
		int result = dialog.runModal();

		if (result == GUI::kMessageOK)
			return kMessageBoxYes;

		if (result == GUI::kMessageAlt)
			return kMessageBoxNo;

		return kMessageBoxCancel;
	}

	GUI::MessageDialog dialog(dialogText);

	dialog.runModal();

	return kMessageBoxOk;
}

void EmiEngine::showMessage(const char *text, const char *caption) {
	if (_cjkMode) {
		CjkString captionUnits;
		CjkString textUnits;

		// The lookups share their buffers, so each result is copied at once
		const uint16 *translatedCaption = caption ? _polyglot->getCjkTranslation(caption, _textMode) : nullptr;

		if (translatedCaption)
			captionUnits.assign(translatedCaption);

		const uint16 *translatedText = text ? _polyglot->getCjkTranslation(text, _textMode) : nullptr;

		if (translatedText)
			textUnits.assign(translatedText);

		showMessage(textUnits.c_str(), captionUnits.c_str());
		return;
	}

	if (_drawContext)
		_drawContext->prepareDialog();

	const char *translatedCaption = _polyglot->getTranslation(caption);
	const char *translatedText = _polyglot->getTranslation(text);

	messageBox(translatedCaption, translatedText, kMessageBoxTypeOk);
}

void EmiEngine::showMessage(const uint16 *text, const uint16 *caption) {
	if (_drawContext)
		_drawContext->prepareDialog();

	// The disasm converts with wcstombs, which fails above 0xFF. The ScummVM dialog takes UTF-8.
	Common::String captionUtf8;
	Common::String textUtf8;

	if (caption)
		captionUtf8 = CjkString::toUtf8(caption);

	if (text)
		textUtf8 = CjkString::toUtf8(text);

	messageBox(captionUtf8.c_str(), textUtf8.c_str(), kMessageBoxTypeOk);
}

bool EmiEngine::inputBox(const char *caption, const char *prompt, char *buffer, int bufferSize, const char *defaultText) {
	Common::String message = caption;

	message += ": ";
	message += prompt;

	Common::String defaultString;

	if (defaultText)
		defaultString = defaultText;

	InputDialog dialog(message, defaultString);

	if (!dialog.runModal())
		return false;

	Common::strlcpy(buffer, dialog.getString().c_str(), bufferSize);

	return true;
}

bool EmiEngine::playMovie(const char *fileName, bool subtitles, bool flag6194, bool flag6195, int startFrame) {
	LString name = fileName;

	if (!strrchr(fileName, '.'))
		name += ".bik";

	LString resolvedName;

	if (!_fileServices->resolvePath(resolvedName, name.c_str()))
		return false;

	if (!Movie::startMovie(this, resolvedName.c_str(), subtitles, flag6194, flag6195, startFrame))
		return false;

	_mode = kModeMovie;

	return true;
}

void EmiEngine::setPendingMusicState(int musicState) {
	_pendingMusicState = musicState;
}

int EmiEngine::getMode() {
	return _mode;
}

void EmiEngine::setMode(int newMode) {
	_mode = newMode;
}

int EmiEngine::getSavedMode() {
	return _savedMode;
}

void EmiEngine::setSavedMode(int newMode) {
	_savedMode = newMode;
}

int EmiEngine::getCpuSpeed() {
	return _cpuSpeed;
}

void EmiEngine::setCpuSpeed(int newSpeed) {
	_cpuSpeed = newSpeed;
}

bool EmiEngine::hasGammaControl() {
	return _drawContext && _drawContext->hasGammaControl();
}

void EmiEngine::setGamma(float gamma) {
	if (_drawContext)
		_drawContext->setGamma(gamma);
}

bool EmiEngine::setIsland(const char *island) {
	if (_bundleServices->isIslandBundleOpen(island))
		return true;

	if (!_island.empty()) {
		_bundleServices->closeIslandBundles(_island.c_str());

		if (_gameResourceManager)
			_gameResourceManager->purgeAll();
	}

	_island = island;

	if (_island.empty())
		return true;

	return _bundleServices->openIslandBundles(_island.c_str());
}

float EmiEngine::getFrameTime() {
	return _frameTime;
}

bool EmiEngine::isDumpingFrames() {
	return _dumpingFrames;
}

void EmiEngine::setSaveLoadIdle(bool idle) {
	_saveLoadIdle = idle;
}

bool EmiEngine::startupModules(CDrawContext *drawContext) {
	if (_modulesStarted)
		return false;

	_sounds = new Sounds(this);

	_gameResourceManager = new ResourceManager(this);
	_gameResourceManager->startup();
	Room_v2::startup(this);

	_joystick = new Joystick(this);
	_control = new SputControl_v2(this);
	_control->startup();

	_lua = new Lua_V2(this);
	_lua->startup();

	TextEvent::startup(this, drawContext);

	if (_cjkMode)
		CTextFont::loadGlyphMap(this);

	ChoreInstance::startup(this);
	Actor::startup(this, drawContext);

	_primitiveQueue = new PrimitiveQueue_v2(this);

	if (_primitiveQueue->startup())
		_lua->registerPrimitiveOpcodes();

	_sounds->startSounds(false);
	startupMovies();

	uint physicalMemory = getTotalPhysMem();
	uint hardCap = kMaxHardCap;

	if (hardCap >= physicalMemory - kReservedMemory)
		hardCap = getTotalPhysMem() - kReservedMemory;

	_memoryServices->setHardMemoryCap(hardCap);
	_modulesStarted = true;

	return true;
}

void EmiEngine::shutdownModules() {
	if (!_modulesStarted)
		return;

	if (getGamePlatform() == Common::kPlatformPS2)
		Movie::releaseLoadingIconPs2(this);

	_control->shutdown();
	_sounds->shutdownSounds();
	Actor::shutdown(this);
	ChoreInstance::shutdown(this);
	TextEvent::shutdown(this);
	CTextFont::freeGlyphMap(this);

	_lua->shutdown();
	delete _lua;
	_lua = nullptr;

	Room_v2::shutdown(this);
	_primitiveQueue->shutdown();

	_gameResourceManager->shutdown();
	delete _gameResourceManager;
	_gameResourceManager = nullptr;

	// The sound module is deleted last because the component instances stop their sounds when they are destroyed
	delete _control;
	_control = nullptr;
	delete _joystick;
	_joystick = nullptr;
	delete _primitiveQueue;
	_primitiveQueue = nullptr;
	delete _sounds;
	_sounds = nullptr;

	_modulesStarted = false;
}

bool EmiEngine::openGameview(const char *bootParam) {
	if (_gameviewOpen) {
		_consoleServices->logError("Game is already running.\n");

		return false;
	}

	if (!_lua->boot(0, bootParam))
		return false;

	_mode = kModeGame;

	if (!_control->open())
		return false;

	_gameviewOpen = true;

	return true;
}

void EmiEngine::closeGameview() {
	if (!_gameviewOpen) {
		_consoleServices->logError("Gameview not open.\n");

		return;
	}

	_mode = kModeNone;
	_control->close();
	Room_v2::loadRoom(this, nullptr);
	releaseSubtitleFont();
	_gameviewOpen = false;
}

bool EmiEngine::callExitHook() {
	if (!_lua) {
		warning("EmiEngine::callExitHook(): Lua is not started");

		return true;
	}

	return !_lua->doExitCallback();
}

void EmiEngine::updateSounds(bool force) {
	_sounds->soundsUpdate(&force);
}

bool EmiEngine::updateMovie(float frameTime) {
	return Movie::updateMovie(this);
}

void EmiEngine::startupMovies() {
	Movie::startupMovies(this);
}

void EmiEngine::releaseSubtitleFont() {
	Movie::releaseSubtitleFont();
}

void EmiEngine::setMusicState(int musicState, int startMs) {
	bool immediate = false;

	_sounds->setMusicState(musicState, startMs, immediate);
}

} // End of namespace Grim
