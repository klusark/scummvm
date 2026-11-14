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

#include "common/archive.h"
#include "common/debug-channels.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/language.h"
#include "common/config-manager.h"
#include "common/compression/deflate.h"
#include "common/translation.h"
#include "common/endian.h"
#include "common/events.h"
#include "common/savefile.h"
#include "common/serializer.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"

#include "graphics/cursorman.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"
#include "graphics/renderer.h"

#include "gui/error.h"
#include "gui/gui-manager.h"
#include "gui/message.h"

#include "image/png.h"

#include "engines/engine.h"
#include "engines/util.h"

#include "engines/grim/md5check.h"
#include "engines/grim/md5checkdialog.h"
#include "engines/grim/debug.h"
#include "engines/grim/grim.h"
#include "engines/grim/lua.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/remastered/commentary.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/dialoglog.h"
#include "engines/grim/remastered/hotspot.h"
#include "engines/grim/remastered/lua_remastered.h"
#include "engines/grim/remastered/sputmouse.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/legacy_savegames/legacy_savegames.h"
#include "engines/grim/lab.h"
#include "engines/grim/patchr.h"
#include "engines/grim/update/update.h"
#include "engines/grim/debugger.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/stdcontrol.h"
#include "engines/grim/text/blasttext.h"
#include "engines/grim/chore/cos.h"
#include "engines/grim/text/normaltext.h"
#include "engines/grim/text/polyglot.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/res.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/sputcontrol.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/render/sputprimitive.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/movie/sputsmush_demo.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"
#include "engines/grim/std/std_bitmap.h"
#include "engines/grim/std/std_bmp.h"
#include "engines/grim/std/std_conffile.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/std/std_file.h"
#include "engines/grim/std/std_gif.h"
#include "engines/grim/std/std_lbm.h"
#include "engines/grim/std/std_math.h"
#include "engines/grim/std/std_pcx.h"
#include "engines/grim/std/std_pic.h"
#include "engines/grim/text/text.h"
#include "engines/grim/walkplane.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

GrimEngine *g_grim = nullptr;

GrimEngine::GrimEngine(OSystem *syst, uint32 gameFlags, GrimGameType gameType, Common::Platform platform, Common::Language language) :
		Engine(syst) {
	g_grim = this;

	if (gameType != GType_MONKEY4)
		setDebugger(new Debugger());

	_gameType = gameType;
	_gameFlags = gameFlags;
	_gamePlatform = platform;
	_gameLanguage = language;

	// Set default settings
	ConfMan.registerDefault("grim_3d_acceleration", false);
	ConfMan.registerDefault("grim_remaster_bugfixes", true);

	_showFps = ConfMan.getBool("show_fps");

	_mixer->setVolumeForSoundType(Audio::Mixer::kPlainSoundType, isRemastered() ? Audio::Mixer::kMaxMixerVolume : 192);
	_mixer->setVolumeForSoundType(Audio::Mixer::kSFXSoundType, ConfMan.getInt("sfx_volume"));
	_mixer->setVolumeForSoundType(Audio::Mixer::kSpeechSoundType, ConfMan.getInt("speech_volume"));
	_mixer->setVolumeForSoundType(Audio::Mixer::kMusicSoundType, ConfMan.getInt("music_volume"));

	int speed = ConfMan.getInt("engine_speed");
	if (speed == 0) {
		_speedLimitMs = 0;
	} else if (speed < 0 || speed > 100) {
		_speedLimitMs = 1000 / 60;
		ConfMan.setInt("engine_speed", 1000 / _speedLimitMs);
	} else {
		_speedLimitMs = 1000 / speed;
	}
	_listFilesIter = nullptr;

	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "movies"); // Add 'movies' subdirectory for the demo
	if (gameType == GType_MONKEY4 && (gameFlags & ADGF_DEMO))
		SearchMan.addSubDirectoryMatching(gameDataDir, "music"); // The EMI demo keeps its music files there
	SearchMan.addSubDirectoryMatching(gameDataDir, "credits");
	SearchMan.addSubDirectoryMatching(gameDataDir, "widescreen");
}

GrimEngine::~GrimEngine() {
	destroyModules();

	delete _imuse;
	_imuse = nullptr;
	MD5Check::clear();

	ConfMan.flushToDisk();

	g_grim = nullptr;
}

const char *GrimEngine::getUpdateFilename() {
	if (!(getGameFlags() & ADGF_DEMO) && !isRemastered())
		return "gfupd101.exe";
	else
		return nullptr;
}

Common::Error GrimEngine::run() {
	if (_gameType != GType_GRIM)
		return Common::Error(Common::kUnsupportedGameidError, _s("This game is not supported by this version of the Grim engine"));

	if (!isRemastered() && !checkGameData("Grim Fandango"))
		return Common::kUserCanceled;

	return runGrime();
}

bool GrimEngine::checkGameData(const char *gameName) {
	ConfMan.registerDefault("check_gamedata", true);
	if (ConfMan.getBool("check_gamedata")) {
		MD5CheckDialog d;
		if (!d.runModal()) {
			Common::U32String confirmString = Common::U32String::format(_(
			        "ScummVM found some problems with your game data files.\n"
			        "Running ScummVM nevertheless may cause game bugs or even crashes.\n"
			        "Do you still want to run %s?"),
			        gameName
			        );
			GUI::MessageDialog msg(confirmString, _("Yes"), _("No"));
			if (msg.runModal() != GUI::kMessageOK) {
				return false;
			}
		}

		ConfMan.setBool("check_gamedata", false);
		ConfMan.flushToDisk();
	}

	return true;
}

Common::KeymapArray GrimEngine::initKeymapsGrim(const char *target) {
	using namespace Common;

	Keymap *engineKeyMap = new Keymap(Keymap::kKeymapTypeGame, "grim", "Grim Fandango");
	Action *act;

	const bool remastered = target && ConfMan.get("extra", target).contains("Remastered");

	engineKeyMap->setPartialMatchAllowed(false);

	addKeyAction(engineKeyMap, kStandardActionMoveUp, _("Up"), KeyState(KEYCODE_UP), "UP", remastered ? nullptr : "JOY_UP");
	addKeyAction(engineKeyMap, kStandardActionMoveDown, _("Down"), KeyState(KEYCODE_DOWN), "DOWN", remastered ? nullptr : "JOY_DOWN");
	addKeyAction(engineKeyMap, kStandardActionMoveLeft, _("Left"), KeyState(KEYCODE_LEFT), "LEFT", remastered ? nullptr : "JOY_LEFT");
	addKeyAction(engineKeyMap, kStandardActionMoveRight, _("Right"), KeyState(KEYCODE_RIGHT), "RIGHT", remastered ? nullptr : "JOY_RIGHT");
	addKeyAction(engineKeyMap, "MVNW", _("Up left"), KeyState(KEYCODE_KP7, '7'), "KP7", nullptr);
	addKeyAction(engineKeyMap, "MVNE", _("Up right"), KeyState(KEYCODE_KP9, '9'), "KP9", nullptr);
	addKeyAction(engineKeyMap, "MVSW", _("Down left"), KeyState(KEYCODE_KP1, '1'), "KP1", nullptr);
	addKeyAction(engineKeyMap, "MVSE", _("Down right"), KeyState(KEYCODE_KP3, '3'), "KP3", nullptr);

	addKeyAction(engineKeyMap, "BRUN", _("Run"), KeyState(KEYCODE_LSHIFT), "LSHIFT", remastered ? nullptr : "JOY_RIGHT_TRIGGER");
	addKeyAction(engineKeyMap, "EXAM", _("Examine"), KeyState(KEYCODE_e, 'e'), "e", remastered ? nullptr : "JOY_B");
	addKeyAction(engineKeyMap, "EXAMALT", _("Examine (alternative)"), KeyState(KEYCODE_s, 's'), "s", nullptr);
	addKeyAction(engineKeyMap, "BUSE", _("Use / Talk"), KeyState(KEYCODE_u, 'u'), "u", remastered ? nullptr : "JOY_Y");
	addKeyAction(engineKeyMap, "USEALT", _("Use / Talk (alternative)"), KeyState(KEYCODE_w, 'w'), "w", nullptr);
	addKeyAction(engineKeyMap, "PICK", _("Pick up / Put away"), KeyState(KEYCODE_p, 'p'), "p", remastered ? nullptr : "JOY_X");
	addKeyAction(engineKeyMap, "PICKALT", _("Pick up / Put away (alternative)"), KeyState(KEYCODE_a, 'a'), "a", nullptr);
	addKeyAction(engineKeyMap, "INVT", _("Inventory"), KeyState(KEYCODE_i, 'i'), "i", remastered ? nullptr : "JOY_LEFT_TRIGGER");
	addKeyAction(engineKeyMap, "INVTALT", _("Inventory (alternative)"), KeyState(KEYCODE_d, 'd'), "d", nullptr);
	addKeyAction(engineKeyMap, "SKLI", _("Skip dialog lines"), KeyState(KEYCODE_PERIOD, '.'), "PERIOD", remastered ? nullptr : "JOY_LEFT_SHOULDER");
	addKeyAction(engineKeyMap, "GAZE", _("Change gaze"), KeyState(KEYCODE_DELETE), "DELETE", remastered ? nullptr : "JOY_LEFT_STICK");
	addKeyAction(engineKeyMap, "RETURN", _("Confirm"), KeyState(KEYCODE_RETURN, ASCII_RETURN), "RETURN", remastered ? nullptr : "JOY_A", remastered ? nullptr : "JOY_RIGHT_STICK");
	addKeyAction(engineKeyMap, "RETURNKP", _("Confirm (keypad)"), KeyState(KEYCODE_KP_ENTER, ASCII_RETURN), "KP_ENTER", nullptr);
	addKeyAction(engineKeyMap, "PGUP", _("Page up"), KeyState(KEYCODE_PAGEUP), "PAGEUP", remastered ? nullptr : "JOY_RIGHT_SHOULDER");
	addKeyAction(engineKeyMap, "PGDN", _("Page down"), KeyState(KEYCODE_PAGEDOWN), "PAGEDOWN", nullptr);
	addKeyAction(engineKeyMap, "PGHM", _("Page home"), KeyState(KEYCODE_HOME), "HOME", nullptr);
	addKeyAction(engineKeyMap, "PGEN", _("Page end"), KeyState(KEYCODE_END), "END", nullptr);

	// I18N: Skipping cutscene playback
	addKeyAction(engineKeyMap, kStandardActionSkip, _("Skip"), KeyState(KEYCODE_ESCAPE, ASCII_ESCAPE), "ESCAPE", remastered ? nullptr : "JOY_BACK");
	addKeyAction(engineKeyMap, kStandardActionOpenMainMenu, _("Game menu"), KeyState(KEYCODE_F1), "F1", remastered ? nullptr : "JOY_START");
	addKeyAction(engineKeyMap, kStandardActionPause, _("Pause"), KeyState(KEYCODE_PAUSE), "PAUSE", nullptr);

	act = new Action("GMM", _("Global Main Menu"));
	act->setEvent(EVENT_MAINMENU);
	act->addDefaultInputMapping("C+F5");
	act->addDefaultInputMapping("JOY_GUIDE");
	engineKeyMap->addAction(act);

	addStickActions(engineKeyMap);

	return Keymap::arrayOf(engineKeyMap);
}

void GrimEngine::addKeyAction(Common::Keymap *keymap, const char *id, const Common::U32String &label, const Common::KeyState &key, const char *keyboardInput, const char *padInput, const char *padInput2) {
	Common::Action *act = new Common::Action(id, label);

	act->setKeyEvent(key);

	if (keyboardInput) {
		act->addDefaultInputMapping(keyboardInput);
		act->allowKbdRepeats();
	}

	if (padInput)
		act->addDefaultInputMapping(padInput);

	if (padInput2)
		act->addDefaultInputMapping(padInput2);

	keymap->addAction(act);
}

void GrimEngine::addStickAction(Common::Keymap *keymap, const char *id, const Common::U32String &label, int stickAction, const char *input) {
	Common::Action *act = new Common::Action(id, label);

	act->setCustomBackendActionAxisEvent(kStickActionFirst + stickAction);
	act->addDefaultInputMapping(input);
	keymap->addAction(act);
}

void GrimEngine::addStickActions(Common::Keymap *keymap) {
	addStickAction(keymap, "LSLEFT", _("Left stick left"), 0, "JOY_LEFT_STICK_X-");
	addStickAction(keymap, "LSRIGHT", _("Left stick right"), 1, "JOY_LEFT_STICK_X+");
	addStickAction(keymap, "LSUP", _("Left stick up"), 2, "JOY_LEFT_STICK_Y-");
	addStickAction(keymap, "LSDOWN", _("Left stick down"), 3, "JOY_LEFT_STICK_Y+");
	addStickAction(keymap, "RSLEFT", _("Right stick left"), 4, "JOY_RIGHT_STICK_X-");
	addStickAction(keymap, "RSRIGHT", _("Right stick right"), 5, "JOY_RIGHT_STICK_X+");
	addStickAction(keymap, "RSUP", _("Right stick up"), 6, "JOY_RIGHT_STICK_Y-");
	addStickAction(keymap, "RSDOWN", _("Right stick down"), 7, "JOY_RIGHT_STICK_Y+");
}

int GrimEngine::applyStickAction(int *axes, const Common::Event &event) {
	int offset = (int)event.customType - kStickActionFirst;

	if (offset < 0 || offset >= kNumStickAxes * 2)
		return -1;

	int axis = offset / 2;
	bool positive = (offset & 1) != 0;
	int magnitude = event.joystick.position;

	// The magnitude of the minimum position does not fit in the event
	if (magnitude < 0)
		magnitude = Common::JOYAXIS_MAX;

	if (magnitude != 0)
		axes[axis] = positive ? magnitude : -magnitude;
	else if ((positive && axes[axis] > 0) || (!positive && axes[axis] < 0))
		axes[axis] = 0;

	return axis;
}

int GrimEngine::getJoystickButtonIndex(int button, bool remastered) {
	if (remastered) {
		switch (button) {
		case Common::JOYSTICK_BUTTON_A:
			return 0;
		case Common::JOYSTICK_BUTTON_B:
			return 1;
		case Common::JOYSTICK_BUTTON_X:
			return 2;
		case Common::JOYSTICK_BUTTON_Y:
			return 3;
		case Common::JOYSTICK_BUTTON_DPAD_UP:
			return 4;
		case Common::JOYSTICK_BUTTON_DPAD_DOWN:
			return 5;
		case Common::JOYSTICK_BUTTON_DPAD_LEFT:
			return 6;
		case Common::JOYSTICK_BUTTON_DPAD_RIGHT:
			return 7;
		case Common::JOYSTICK_BUTTON_LEFT_SHOULDER:
			return 8;
		case Common::JOYSTICK_BUTTON_RIGHT_SHOULDER:
			return 9;
		case Common::JOYSTICK_BUTTON_START:
			return 10;
		case Common::JOYSTICK_BUTTON_RIGHT_STICK:
			return 11;
		default:
			return -1;
		}
	}

	// Generic gamepad button order
	switch (button) {
	case Common::JOYSTICK_BUTTON_A:
		return 0;
	case Common::JOYSTICK_BUTTON_B:
		return 1;
	case Common::JOYSTICK_BUTTON_X:
		return 2;
	case Common::JOYSTICK_BUTTON_Y:
		return 3;
	case Common::JOYSTICK_BUTTON_LEFT_SHOULDER:
		return 4;
	case Common::JOYSTICK_BUTTON_RIGHT_SHOULDER:
		return 5;
	case Common::JOYSTICK_BUTTON_BACK:
		return 6;
	case Common::JOYSTICK_BUTTON_START:
		return 7;
	case Common::JOYSTICK_BUTTON_LEFT_STICK:
		return 8;
	case Common::JOYSTICK_BUTTON_RIGHT_STICK:
		return 9;
	default:
		return -1;
	}
}

Common::Error GrimEngine::loadGameState(int slot) {
	assert(slot >= 0);

	if (isRemastered()) {
		requestRemasteredLoad(slot, false);

		return Common::kNoError;
	}

	loadLib(Common::String::format("grim%02d.gsv", slot).c_str());
	return Common::kNoError;
}

void GrimEngine::clearEventQueue() {
	g_system->getEventManager()->purgeKeyboardEvents();
	g_system->getEventManager()->purgeMouseEvents();
}

bool GrimEngine::hasFeature(EngineFeature f) const {
	return
		(f == kSupportsReturnToLauncher) ||
		(f == kSupportsLoadingDuringRuntime) ||
		(f == kSupportsQuitDialogOverride && !isRemastered());
}

void GrimEngine::pauseEngineIntern(bool pause) {
	if (pause) {
		_enginePaused = _libMode != 1;
		setPause(1);
	} else if (_enginePaused) {
		_enginePaused = 0;
		setPause(0);

		if (_stdControl)
			_stdControl->flush();
	}
}

void GrimEngine::debugLua(const Common::String &str) {
	lua_dostring(str.c_str());
}

void GrimEngine::mountPatchArchives() {
	const char *updateFilename = getUpdateFilename();

	if (updateFilename && !SearchMan.hasArchive("update")) {
		Common::File *updStream = new Common::File();

		if (updStream->open(updateFilename)) {
			Common::Archive *update = loadUpdateArchive(updStream);

			if (update)
				SearchMan.add("update", update, 1);
		} else {
			delete updStream;
		}

		if (!SearchMan.hasArchive("update")) {
			Common::String urlForPatchDownload = "https://downloads.scummvm.org/frs/extras/patches/gfupd101.exe";
			Common::U32String errorMessage = Common::U32String::format(_("The original patch of Grim Fandango\n"
							"is missing. Please download it from\n"
							"%s\n"
							"and put it in the game data files directory"), urlForPatchDownload.c_str());

			GUIErrorMessageWithURL(errorMessage, urlForPatchDownload.c_str());
			error("%s not found", updateFilename);
		}
	}

	bool demo = (getGameFlags() & ADGF_DEMO) != 0;
	const char *labName = demo ? "grimdemo.mus" : "grim-patch.lab";

	if (!demo && !isRemastered() && !SearchMan.hasFile(labName))
		error("%s", "grim-patch.lab not found");

	if (!isRemastered() && !SearchMan.hasArchive(labName)) {
		Common::ArchiveMemberList files;

		SearchMan.listMatchingMembers(files, labName);

		if (!files.empty()) {
			Lab *lab = new Lab();

			if (lab->open(files.front()->getPathInArchive(), false))
				SearchMan.add(labName, lab, 0, true);
			else
				delete lab;
		}
	}

	Common::ArchiveMemberList patches;

	Debug::debugImpl(Debug::Patchr, "%d patch files available", SearchMan.listMatchingMembers(patches, "*.patchr"));
}

Common::SeekableReadStream *GrimEngine::openStreamFile(const Common::String &fname) {
	Common::Path path(fname, '/');

	path.toLowercase();

	if (!SearchMan.hasFile(path))
		return nullptr;

	Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(path);

	stream = wrapPatchedFile(stream, path);

	// This will only have an effect if the stream is actually compressed.
	return Common::wrapCompressedReadStream(stream);
}

Common::Error GrimEngine::runGrime() {
	mountPatchArchives();

	createImuse();
	createModules();

	if (!grimStartup()) {
		grimShutdown();
		destroyModules();

		return Common::kUnknownError;
	}

	if (ConfMan.hasKey("save_slot"))
		loadGameState(ConfMan.getInt("save_slot"));

	while (!shouldQuit() && !_quit) {
		uint32 startTime = g_system->getMillis();

		pumpEvents();
		grimMain();

		uint32 endTime = g_system->getMillis();

		if (endTime >= startTime && endTime - startTime < _speedLimitMs)
			g_system->delayMillis(_speedLimitMs - (endTime - startTime));
	}

	grimShutdown();
	destroyModules();

	return Common::kNoError;
}

void GrimEngine::createImuse() {
	bool demo = getGameFlags() & ADGF_DEMO;

	// The remastered works at 48000Hz
	int baseSampleRate = isRemastered() ? IMUSE_REMASTERED_SAMPLERATE : IMUSE_BASE_SAMPLERATE;
	int sampleRate = baseSampleRate;
	ConfMan.registerDefault("imuse_sample_rate", baseSampleRate);
	if (ConfMan.hasKey("imuse_sample_rate", _targetName)) {
		// Only accept sample rates which are a multiple or submultiple of the base one
		int configSampleRate = ConfMan.getInt("imuse_sample_rate");
		if ((configSampleRate % (baseSampleRate / 2)) == 0 &&
			configSampleRate >= baseSampleRate / 2 &&
			configSampleRate <= baseSampleRate * 4) {
			sampleRate = configSampleRate;
		}
	}

	bool lowLatencyMode = false;
	ConfMan.registerDefault("imuse_low_latency_mode", false);
	if (ConfMan.hasKey("imuse_low_latency_mode", _targetName))
		lowLatencyMode = ConfMan.getBool("imuse_low_latency_mode");

	_imuse = new IMuse(sampleRate, _mixer, &_imuseMutex, lowLatencyMode, demo, isRemastered());
}

void GrimEngine::createModules() {
	_stdMath = new StdMath(isRemastered());
	_stdFile = new StdFile();
	_stdDisplay = new StdDisplay();
	_stdConffile = new StdConffile(this);
	_stdBitmap = new StdBitmap(_stdDisplay, _stdFile, this);
	_stdDisplay->setStdBitmap(_stdBitmap);

#if defined(GRIM_STD3D)
	_std3D = new Std3D(_stdDisplay, _stdBitmap);
#endif

	_stdDisplay->setStd3D(_std3D);
	_stdLbm = new StdLbm(_stdDisplay, _stdFile, _stdBitmap, this);
	_stdPcx = new StdPcx(_stdDisplay, _stdFile, _stdBitmap, this);
	_stdGif = new StdGif(_stdDisplay, _stdFile, _stdBitmap, this);
	_stdPic = new StdPic(_stdDisplay, _stdFile, this);
	_stdBmp = new StdBmp(_stdDisplay, _stdFile, this);
	_stdBitmap->setImageFormats(_stdLbm, _stdPcx, _stdGif, _stdPic, _stdBmp);

	bool isDemo = (_gameFlags & ADGF_DEMO) != 0;

	_rd = new RenderDroid(_stdMath, _stdDisplay, _stdConffile, _stdFile, isDemo, isRemastered());
	_rd->setStd3D(_std3D);
	_walkSystem = new WalkSystem(_rd, _stdMath, _stdConffile, isDemo, isRemastered());

	_res = new Res(isDemo);
	_sputResource = new SputResource(_res, _rd, _stdDisplay, _stdBitmap);
	_stdControl = new StdControl(isRemastered());

	StdControlInput *controlInput = _stdControl->getInput();
	StdControlJoyCaps *joyCaps = &controlInput->joyCaps[0];

	controlInput->numJoysticks = 1;
	joyCaps->available = 1;
	joyCaps->flags = isRemastered() ? 0 : 0x0F;
	joyCaps->maxButtons = isRemastered() ? 12 : 10;

	for (int axis = 0; axis < 6; axis++) {
		joyCaps->axisMin[axis] = -32768;
		joyCaps->axisMax[axis] = 32767;
	}

	controlInput->joyState[0].flags = isRemastered() ? 0 : 0x3C;

	_wuRegistry = new WuRegistry();
	_polyglotV1 = new Polyglot();
	_sputTime = new SputTime(isDemo);
	_sputPrimitive = new SputPrimitive(_rd);
	_sputRoom = new SputRoom(_rd, _res, _sputResource, _stdDisplay, _stdBitmap, _stdLbm, _stdConffile, _walkSystem);
	_sputControl = new SputControl(_rd, _sputRoom, _stdControl, isRemastered());

	_text = new Text();
	_blastText = new BlastText(_text);
	_text->setBlastText(_blastText);
	_normalText = new NormalText();
	_sputText = new SputText(_res, _rd, _normalText, _text, _blastText, _sputTime, _polyglotV1, isRemastered());

	_mainGfxBuffer = new StdVBuffer();
	_backBuffer = new StdVBuffer();
	_stdDisplay->setBuffers(_mainGfxBuffer, _backBuffer);
	_sputRender = new SputRender(_rd, _stdDisplay, _stdBitmap, _res, _sputResource, _sputPrimitive, _walkSystem, _sputRoom,
			_backBuffer, _mainGfxBuffer, _sputTime, _sputControl);
	_sputCostume = new SputCostume(_res, _rd, _sputResource, _sputRender, _sputRoom, _stdLbm, _wuRegistry);
	_sputActor = new SputActorModule(_res, _rd, _stdMath, _stdDisplay, _walkSystem, _sputResource, _sputRoom, _sputRender,
			_sputCostume, _sputText, _sputTime);
	if (_gameFlags & ADGF_DEMO) {
		SputSmushDemo *sputSmushDemo = new SputSmushDemo(_res, _stdDisplay, _sputRender, _mainGfxBuffer, _wuRegistry);

		sputSmushDemo->setGrimEngine(this);
		_sputSmush = sputSmushDemo;
	} else {
		_sputSmush = new SputSmush(_res, _stdDisplay, _sputRender, _mainGfxBuffer, _wuRegistry);
	}

	if (isRemastered())
		_hdCanvas = new HdCanvas();

	if (isRemastered())
		_luaV1 = new Lua_Remastered();
	else
		_luaV1 = new Lua_V1(false);

	if (isRemastered()) {
		_sputMouse = new SputMouse(this, this, _stdControl, _sputRoom, _sputActor, _walkSystem, _rd);
		_dialogLog = new DialogLog();
		_sputMouse->getHotspots()->setOverlays(_hdCanvas->getOverlays());
		_sputMouse->startup();
		_commentary = new Commentary(_res, _imuse, _sputText, _sputSmush, _wuRegistry, _hdCanvas->getOverlays());
	}

	_res->setSputRender(_sputRender);
	_sputResource->setSputRender(_sputRender);
	_sputResource->setSputRoom(_sputRoom);
	_sputResource->setCos(_sputCostume->getCos());
	_sputResource->setText(_text);
	_sputResource->setBlastText(_blastText);
	_sputResource->setWuRegistry(_wuRegistry);
	_sputPrimitive->setSputRender(_sputRender);
	_sputPrimitive->setLua(_luaV1);
	_sputRoom->setSputRender(_sputRender);
	_sputRoom->setSputActor(_sputActor);
	_sputRoom->setGrimEngine(this);
	_sputText->setSputRender(_sputRender);
	_sputText->setSputRoom(_sputRoom);
	_sputText->setSputActor(_sputActor);
	_sputText->setGrimEngine(this);
	_sputText->setLua(_luaV1);
	_sputRender->setSputActor(_sputActor);
	_sputRender->setSputText(_sputText);
	_sputRender->setBlastText(_blastText);
	_sputRender->setSputSmush(_sputSmush);
	_sputRender->setStdControl(_stdControl);
	_sputRender->setStd3D(_std3D);
	_sputRender->setGrimEngine(this);
	_sputRender->setLua(_luaV1);
	_sputCostume->setSputActor(_sputActor);
	_sputCostume->setGrimEngine(this);
	_sputCostume->setLua(_luaV1);
	_sputActor->setGrimEngine(this);
	_sputActor->setLua(_luaV1);
	_sputActor->setStd3D(_std3D);
	_sputControl->setGrimEngine(this);
}

void GrimEngine::destroyModules() {
	if (_displayOpen)
		shutdownDisplay();

	freeVideoDevices();

	delete _commentary;
	_commentary = nullptr;
	if (_sputMouse)
		_sputMouse->shutdown();

	delete _sputMouse;
	_sputMouse = nullptr;
	delete _dialogLog;
	_dialogLog = nullptr;
	delete _luaV1;
	_luaV1 = nullptr;
	delete _hdCanvas;
	_hdCanvas = nullptr;
	delete _sputSmush;
	_sputSmush = nullptr;
	delete _sputActor;
	_sputActor = nullptr;
	delete _sputCostume;
	_sputCostume = nullptr;
	delete _sputRender;
	_sputRender = nullptr;
	delete _backBuffer;
	_backBuffer = nullptr;
	delete _mainGfxBuffer;
	_mainGfxBuffer = nullptr;
	delete _sputText;
	_sputText = nullptr;
	delete _normalText;
	_normalText = nullptr;
	delete _blastText;
	_blastText = nullptr;
	delete _text;
	_text = nullptr;
	delete _sputControl;
	_sputControl = nullptr;
	delete _sputRoom;
	_sputRoom = nullptr;
	delete _sputPrimitive;
	_sputPrimitive = nullptr;
	delete _sputTime;
	_sputTime = nullptr;
	delete _polyglotV1;
	_polyglotV1 = nullptr;
	delete _wuRegistry;
	_wuRegistry = nullptr;
	delete _stdControl;
	_stdControl = nullptr;
	delete _sputResource;
	_sputResource = nullptr;
	delete _res;
	_res = nullptr;
	delete _walkSystem;
	_walkSystem = nullptr;
	delete _rd;
	_rd = nullptr;

#if defined(GRIM_STD3D)
	delete _std3D;
#endif

	_std3D = nullptr;
	delete _stdBmp;
	_stdBmp = nullptr;
	delete _stdPic;
	_stdPic = nullptr;
	delete _stdGif;
	_stdGif = nullptr;
	delete _stdPcx;
	_stdPcx = nullptr;
	delete _stdLbm;
	_stdLbm = nullptr;
	delete _stdBitmap;
	_stdBitmap = nullptr;
	delete _stdConffile;
	_stdConffile = nullptr;
	delete _stdDisplay;
	_stdDisplay = nullptr;
	delete _stdFile;
	_stdFile = nullptr;
	delete _stdMath;
	_stdMath = nullptr;
}

uint GrimEngine::getHardwareBackends() const {
	uint mask = 0;

	if (isRemastered())
		return mask;

#if defined(GRIM_STD3D)
	uint32 availableTypes = Graphics::Renderer::getAvailableTypes();

	if (Std3D::isBackendCompiled(kStd3DBackendGl) && (availableTypes & Graphics::kRendererTypeOpenGLShaders))
		mask |= 1 << kStdDisplayBackendGl;

	if (Std3D::isBackendCompiled(kStd3DBackendTinyGl) && (availableTypes & Graphics::kRendererTypeTinyGL))
		mask |= 1 << kStdDisplayBackendTinyGl;

	Graphics::RendererType desiredRendererType = Graphics::Renderer::parseTypeCode(ConfMan.get("renderer"));

	if (desiredRendererType == Graphics::kRendererTypeOpenGLShaders) {
		if (mask & (1 << kStdDisplayBackendGl))
			mask = 1 << kStdDisplayBackendGl;
		else if (mask)
			warning("GrimEngine::getHardwareBackends(): the OpenGL with shaders renderer is not available, using the other renderer");
	} else if (desiredRendererType == Graphics::kRendererTypeTinyGL) {
		if (mask & (1 << kStdDisplayBackendTinyGl))
			mask = 1 << kStdDisplayBackendTinyGl;
		else if (mask)
			warning("GrimEngine::getHardwareBackends(): the software renderer is not available, using the other renderer");
	} else if (desiredRendererType == Graphics::kRendererTypeOpenGL) {
		warning("GrimEngine::getHardwareBackends(): the OpenGL renderer is not supported, using the default renderer");
	}
#endif

	return mask;
}

int GrimEngine::isBackendUsable(int backend) {
	uint numDevices = _stdDisplay->getNumDevices();
	StdDisplayDevice *devices = _stdDisplay->getDevices();

	for (uint i = 0; i < numDevices; i++) {
		if (devices[i].type == 1 && devices[i].backend == backend && devices[i].usable)
			return 1;
	}

	return 0;
}

int GrimEngine::hasUsableHardwareDevice() {
	return isBackendUsable(kStdDisplayBackendGl) || isBackendUsable(kStdDisplayBackendTinyGl);
}

int GrimEngine::chooseHardwareBackend() {
	StdDisplayDevice *curDevice = _stdDisplay->getCurDevice();

	if (_displayOpen && curDevice && curDevice->type == 1 && isBackendUsable(curDevice->backend))
		return curDevice->backend;

	if (_preferredBackend != kStdDisplayBackendNone && isBackendUsable(_preferredBackend))
		return _preferredBackend;

	if (!_wuRegistry->getInt("ExperimentalDisplay", 1)) {
		byte lastDisplayGuid[16];

		memset(lastDisplayGuid, 0, sizeof(lastDisplayGuid));

		if (_wuRegistry->getBinary("LastDisplayDevice", lastDisplayGuid, sizeof(lastDisplayGuid))) {
			uint numDevices = _stdDisplay->getNumDevices();
			StdDisplayDevice *devices = _stdDisplay->getDevices();

			for (uint i = 0; i < numDevices; i++) {
				if (devices[i].type == 1 && devices[i].usable && !memcmp(devices[i].guid, lastDisplayGuid, sizeof(lastDisplayGuid)))
					return devices[i].backend;
			}
		}
	}

	if (isBackendUsable(kStdDisplayBackendGl))
		return kStdDisplayBackendGl;

	if (isBackendUsable(kStdDisplayBackendTinyGl))
		return kStdDisplayBackendTinyGl;

	return kStdDisplayBackendNone;
}

int GrimEngine::grimStartup() {
	_hardwareBackends = getHardwareBackends();
	_useHardware = ConfMan.getBool("grim_3d_acceleration") ? 1 : 0;
	_remasterFixes = (!(_gameFlags & ADGF_DEMO) && (isRemastered() || ConfMan.getBool("grim_remaster_bugfixes"))) ? 1 : 0;

	_stdDisplay->startup();
	_polyglotV1->initialize(this);
	_polyglotV1->startup();

	if (!isRemastered())
		_polyglotV1->configure("grim.tab");

	_wuRegistry->startup("1.0");

	if (isRemastered()) {
		initLanguage();
		_wuRegistry->saveInt("RenderingMode", _renderingMode);
	}

	_rd->startup(this);

	if (!_rd->open(0))
		return 0;

	if (!_stdDisplay->enumerateDevices(_hardwareBackends))
		return 0;

	initDisplay();

	if (_enumerateVideo && _show)
		_wuRegistry->saveInt("ExperimentalDisplay", 1);

	if (!initDisplayAndDevice())
		return 0;

	if (sputInitialize(this))
		return 0;

	if (sputStartup()) {
		fatalError(_polyglotV1->getTranslation("/engn004/Please make sure you are not running any other DirectX apps that take exclusive control of your hardware."),
				_polyglotV1->getTranslation("/engn005/Grim Fandango could not initialize your hardware!"));
		return 0;
	}

	_luaV1->registerHardwareOpcodes();
	setShutdownCallback(shutdownCallback);
	setPaletteCallback(paletteCallback);
	setPageFlipCallback(pageFlipCallback);
	findResources();
	loadSplash();

	if (bundleDofile()) {
		fatalError(_polyglotV1->getTranslation("/engn006/Please make sure that the game is properly installed and that the CD is in your drive."),
				_polyglotV1->getTranslation("/engn007/Grim Fandango could not find files!"));
		return 0;
	}

	if (_commentary)
		_commentary->loadDefinitions();

	if (startEngine(_saveGameToResume, nullptr))
		return 0;

	return 1;
}

void GrimEngine::initLanguage() {
	int language = kLanguageEnglish;

	if (ConfMan.hasKey("grim_language")) {
		language = ConfMan.getInt("grim_language");
	} else {
		Common::Language launcherLanguage = _gameLanguage;

		if (ConfMan.hasKey("language"))
			launcherLanguage = Common::parseLanguage(ConfMan.get("language"));

		switch (launcherLanguage) {
		case Common::DE_DEU:
			language = kLanguageGerman;
			break;
		case Common::ES_ESP:
			language = kLanguageSpanish;
			break;
		case Common::FR_FRA:
			language = kLanguageFrench;
			break;
		case Common::IT_ITA:
			language = kLanguageItalian;
			break;
		case Common::PT_BRA:
			language = kLanguagePortuguese;
			break;
		default:
			language = kLanguageEnglish;
			break;
		}
	}

	if (language < 0 || language >= kNumLanguages) {
		warning("GrimEngine::initLanguage(): invalid language %d, using English", language);
		language = kLanguageEnglish;
	}

	_language = language;
	_wuRegistry->saveInt("Language", _language);
	loadTranslations();
}

void GrimEngine::loadTranslations() {
	Common::String tabFile = Common::String::format("grim.%s.tab", getLanguageCode());

	_polyglotV1->shutdown();
	_polyglotV1->configure(tabFile.c_str());
}

const char *GrimEngine::getLanguageCode() const {
	static const char *const codes[kNumLanguages] = { "en", "de", "es", "fr", "it", "pt" };

	return codes[_language];
}

void GrimEngine::setLanguage(int language) {
	if (language < 0 || language >= kNumLanguages) {
		warning("GrimEngine::setLanguage(): invalid language %d, ignored", language);
		return;
	}

	_language = language;
	loadTranslations();
}

void GrimEngine::setRenderingMode(int mode) {
	// Only the classic renderer exists
}

void GrimEngine::grimMain() {
	if (_quit)
		return;

	if (_displayChangePending) {
		int mode = _libMode;

		if (!selectDisplayMode(_pendingDevice, _pendingMode, _pending3DDevice)) {
			warning("GrimEngine::grimMain(): could not change the display, going back to another display");

			selectFallbackDisplay(_pendingDevice, _pending3DDevice != -1);
		}

		_libMode = 2;
		_sputRender->dim(1, 2);
		_libMode = mode;
		_displayChangePending = 0;
	}

	loop(1, _firstFrameFlag);
	_firstFrameFlag = 0;
	_grimFrameCount++;

	uint32 curTime = g_system->getMillis();
	uint32 elapsed = curTime - _grimLastFpsTime;

	if (elapsed > 1000) {
		_grimLastFpsTime = curTime;

		int numFrames = _grimFrameCount - _grimLastFpsFrame;

		_grimLastFpsFrame = _grimFrameCount;
		_grimFps = (double)numFrames * 1000.0 / (double)elapsed;
	}

	if (_showFps) {
		Common::String text = Common::String::format("%02.3f", _grimFps);
		textOut(text.c_str(), 4, 4);

		StdRect rect = { 0, 0, 80, 30 };

		markDirtyRect(&rect, 0);
	}

	if (_showDebugOverlay) {
		int y = 30;

		for (int i = 0; i < kNumPerfCounters; i++) {
			Common::String text = Common::String::format("%s %02.3f", _perfCounters[i].name, _perfCounters[i].elapsed);
			textOut(text.c_str(), 4, y);

			StdRect rect = { 4, y, 190, 20 };

			markDirtyRect(&rect, 0);
			_perfCounters[i].elapsed = 0.0;
			y += 20;
		}
	}

	timedPageFlip();
}

void GrimEngine::textOut(const char *text, int x, int y) {
	if (!_displayOpen || !_backBuffer)
		return;

	const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);

	if (!font) {
		warning("GrimEngine::textOut(): no built-in font available");
		return;
	}

	bool videoMemory = _backBuffer->type == 1;

	if (videoMemory && !_stdDisplay->vBufferLock(_backBuffer)) {
		warning("GrimEngine::textOut(): could not lock the back buffer");
		return;
	}

	if (_backBuffer->pixels) {
		Graphics::PixelFormat pixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);
		Graphics::Surface surface;

		surface.init(_backBuffer->tex.width, _backBuffer->tex.height, _backBuffer->tex.pitchInBytes, _backBuffer->pixels, pixelFormat);

		uint32 color = surface.format.RGBToColor(255, 255, 0);

		font->drawString(&surface, Common::String(text), x, y, surface.w - x, color);
	}

	if (videoMemory)
		_stdDisplay->vBufferUnlock(_backBuffer);
}

void GrimEngine::grimShutdown() {
	freeVideoDevices();
	close();

	if (_sputStarted)
		sputShutdown();

	sputTerminate();

	_rd->close();
	_rd->shutdown();

	shutdownDisplay();
	_stdDisplay->restoreDisplayMode();

	_wuRegistry->shutdown();
	_polyglotV1->shutdown();
	_stdDisplay->shutdown();
}

int GrimEngine::shutdownCallback(int arg) {
	g_grim->_quit = 1;

	return 0;
}

Common::SeekableReadStream *GrimEngine::openFile(const char *path) {
	Common::String name(path);

	name.replace('\\', '/');
	name.toLowercase();

	Common::String baseName = Common::lastPathComponent(name, '/');
	Common::Path fullPath(name, '/');
	Common::Path basePath(baseName, Common::Path::kNoSeparator);
	Common::SeekableReadStream *stream = nullptr;

	if (SearchMan.hasFile(fullPath)) {
		stream = SearchMan.createReadStreamForMember(fullPath);
		stream = wrapPatchedFile(stream, fullPath);
	} else if (SearchMan.hasFile(basePath)) {
		stream = SearchMan.createReadStreamForMember(basePath);
		stream = wrapPatchedFile(stream, basePath);
	}

	return stream;
}

int GrimEngine::sputInitialize(SysStruct *hs) {
	_sputHS = hs;

	int result = callRegisterModules(hs, 0, kNumBaseModules);

	if (!result)
		result = callRegisterModules(hs, kNumBaseModules, kNumMainModules);

	_sputInitialized = result == 0;

	return result;
}

int GrimEngine::sputTerminate() {
	_sputHS = nullptr;

	if (_sputInitialized) {
		callRelease(kNumBaseModules, kNumMainModules);
		callRelease(0, kNumBaseModules);
		_sputInitialized = 0;
	}

	return 0;
}

int GrimEngine::sputStartup() {
	if (_sputStarted)
		return 1;

	int result = runInitTable(0, kNumBaseModules);

	if (!result) {
		int physicalRam = kPhysicalRam;

		// The demo always uses 12288000 bytes, whatever the memory of the machine
		if (_gameFlags & ADGF_DEMO) {
			_sputResource->setMemoryUsage(12288000, 0);
		} else if (physicalRam > 0x1000000) {
			int memoryLimit = _res->getMemoryLimit();

			if (memoryLimit >= (int)(physicalRam - 0x800000))
				memoryLimit = physicalRam - 0x800000;

			_sputResource->setMemoryUsage(memoryLimit, 0);
		} else {
			_sputResource->setMemoryUsage(10485760, 0);
		}

		result = runInitTable(kNumBaseModules, kNumMainModules);
	}

	_sputStarted = result == 0;

	return result;
}

int GrimEngine::sputShutdown() {
	callTableFunc(kNumBaseModules, kNumMainModules);

	int result = callTableFunc(0, kNumBaseModules);

	_sputStarted = 0;

	return result;
}

int GrimEngine::callRegisterModules(SysStruct *hs, int firstModule, int count) {
	for (int i = 0; i < count; i++) {
		if (initializeModule(firstModule + i, hs)) {
			for (int j = i - 1; j >= 0; j--)
				terminateModule(firstModule + j);

			return 1;
		}
	}

	return 0;
}

int GrimEngine::runInitTable(int firstModule, int count) {
	for (int i = 0; i < count; i++) {
		if (startupModule(firstModule + i)) {
			for (int j = i - 1; j >= 0; j--)
				shutdownModule(firstModule + j);

			return 1;
		}
	}

	return 0;
}

int GrimEngine::callTableFunc(int firstModule, int count) {
	for (int i = count - 1; i >= 0; i--)
		shutdownModule(firstModule + i);

	return 0;
}

int GrimEngine::callRelease(int firstModule, int count) {
	for (int i = count - 1; i >= 0; i--)
		terminateModule(firstModule + i);

	return 0;
}

int GrimEngine::initializeModule(int module, SysStruct *hs) {
	switch (module) {
	case kSputMainModuleRoom:
		return _sputRoom->initialize(hs);
	case kSputMainModuleTime:
		return _sputTime->initialize(hs);
	case kSputMainModuleLua:
		return _luaV1->initialize(hs);
	case kSputMainModuleCostume:
		return _sputCostume->initialize(hs);
	case kSputMainModuleActor:
		return _sputActor->initialize(hs);
	case kSputMainModuleResource:
		return _sputResource->initialize(hs);
	case kSputMainModuleControl:
		return _sputControl->initialize(hs);
	case kSputMainModuleSounds:
		return 0;
	case kSputMainModuleRender:
		return _sputRender->initialize(hs);
	case kSputMainModuleText:
		return _sputText->initialize(hs);
	case kSputMainModulePrimitive:
		return _sputPrimitive->initialize(hs);
	case kSputMainModuleSmush:
		return _sputSmush->initialize(hs);
	default:
		return 0;
	}
}

int GrimEngine::startupModule(int module) {
	switch (module) {
	case kSputMainModuleRoom:
		return _sputRoom->startup();
	case kSputMainModuleTime:
		return _sputTime->startup();
	case kSputMainModuleLua:
		return _luaV1->startup();
	case kSputMainModuleCostume:
		return _sputCostume->startup();
	case kSputMainModuleActor:
		return _sputActor->startup();
	case kSputMainModuleResource:
		return _sputResource->startup();
	case kSputMainModuleControl:
		return _sputControl->startup();
	case kSputMainModuleSounds:
		return 0;
	case kSputMainModuleRender:
		return _sputRender->startup();
	case kSputMainModuleText:
		return _sputText->startup();
	case kSputMainModulePrimitive:
		return _sputPrimitive->startup();
	case kSputMainModuleSmush:
		return _sputSmush->startup();
	default:
		return 0;
	}
}

void GrimEngine::shutdownModule(int module) {
	switch (module) {
	case kSputMainModuleRoom:
		_sputRoom->shutdown();
		break;
	case kSputMainModuleTime:
		_sputTime->shutdown();
		break;
	case kSputMainModuleLua:
		_luaV1->shutdown();
		break;
	case kSputMainModuleCostume:
		_sputCostume->shutdown();
		break;
	case kSputMainModuleActor:
		_sputActor->shutdown();
		break;
	case kSputMainModuleResource:
		_sputResource->shutdown();
		break;
	case kSputMainModuleControl:
		_sputControl->shutdown();
		break;
	case kSputMainModuleSounds:
		break;
	case kSputMainModuleRender:
		_sputRender->shutdown();
		break;
	case kSputMainModuleText:
		_sputText->shutdown();
		break;
	case kSputMainModulePrimitive:
		_sputPrimitive->shutdown();
		break;
	case kSputMainModuleSmush:
		_sputSmush->shutdown();
		break;
	default:
		break;
	}
}

void GrimEngine::terminateModule(int module) {
	switch (module) {
	case kSputMainModuleRoom:
		_sputRoom->terminate();
		break;
	case kSputMainModuleTime:
		_sputTime->terminate();
		break;
	case kSputMainModuleLua:
		_luaV1->terminate();
		break;
	case kSputMainModuleCostume:
		_sputCostume->terminate();
		break;
	case kSputMainModuleActor:
		_sputActor->terminate();
		break;
	case kSputMainModuleResource:
		_sputResource->terminate();
		break;
	case kSputMainModuleControl:
		_sputControl->terminate();
		break;
	case kSputMainModuleSounds:
		break;
	case kSputMainModuleRender:
		_sputRender->terminate();
		break;
	case kSputMainModuleText:
		_sputText->release();
		break;
	case kSputMainModulePrimitive:
		_sputPrimitive->terminate();
		break;
	case kSputMainModuleSmush:
		_sputSmush->terminate();
		break;
	default:
		break;
	}
}

int GrimEngine::bundleDofile() {
	int found = _luaV1->findSystemFiles();

	return !found;
}

int GrimEngine::startEngine(int saveGameResume, const char *bootParam) {
	if (_sputOpen)
		return 1;

	int booted = _luaV1->boot(saveGameResume, bootParam);

	if (!booted)
		return 1;

	_libMode = 2;
	_sputRender->open();

	if (!_sputControl->open())
		return 1;

	_sputOpen = 1;

	return 0;
}

int GrimEngine::loop(int render, int manualTicks) {
	if (_libSaveRequest)
		save();

	if (_libLoadRequest)
		restore();

	if (_remasteredEraseRequest)
		eraseRemastered();

	if (_remasteredNewGameRequest)
		newGameRemastered();

	setPerfCounter(0);
	setPerfCounter(10);
	_imuse->soundsPeriodic();
	queryPerfCounterDiff(10);

	int result = _libMode;

	if (_libMode) {
		setPerfCounter(1);
		_sputControl->update(_sputTime->getFrameTimeFlex());
		queryPerfCounterDiff(1);

		int mode = _libMode;

		if (mode != 1 || manualTicks) {
			_loopCount++;

			if (manualTicks)
				_sputTime->advanceManual(manualTicks);
			else
				_sputTime->advance();

			if (_runLua) {
				setPerfCounter(2);
				_luaV1->update();
				queryPerfCounterDiff(2);
			}

			mode = _libMode;

			if (mode != 3 && (mode != 4 || (_gameFlags & ADGF_DEMO))) {
				setPerfCounter(3);
				if (_gameFlags & ADGF_DEMO)
					_sputActor->updateActorsDemo(_sputTime->getFrameTimeFlex());
				else
					_sputActor->updateActors(_sputTime->getFrameTimeFlex());
				queryPerfCounterDiff(3);
				mode = _libMode;
			}

			if (_commentary) {
				_commentary->update();
				mode = _libMode;
			}
		}

		if (render && (mode != 1 || manualTicks)) {
			_sputRender->update();

			if (_gameFlags & ADGF_DEMO)
				_rd->advanceFrame();

			_renderPending = 1;
		}

		queryPerfCounterDiff(0);

		result = _libImuseState;

		if (result != -1) {
			result = _imuse->imSetState(result);
			_libImuseState = -1;
		}
	}

	return result;
}

void GrimEngine::timedPageFlip() {
	setPerfCounter(8);

	if (isRemastered() && _libMode != kLibModeExiting) {
		if (_displayPrevMode == kLibModeMenu && _libMode != kLibModeMenu && _renderingMode != kRenderingModeHd) {
			_sputRender->getIris(1.0f, 320, 240);
			_hdCanvas->clearBuffer();
			_hdCanvas->getOverlays()->render(40, 0);
		}

		_displayPrevMode = _libMode;
	}

	if (isRemastered()) {
		if (_libMode != kLibModeExiting)
			_sputRender->pageFlip();

		queryPerfCounterDiff(8);
		return;
	}

	switch (_libMode) {
	case 3:
		if (!_sputRender->queryDisplayState())
			_sputRender->pageFlip();

		break;
	case 2:
		_sputRender->pageFlip();
		break;
	case 1:
		if (_renderPending) {
			_sputRender->pageFlip();
			_renderPending = 0;
		}

		break;
	default:
		break;
	}

	queryPerfCounterDiff(8);
}

bool GrimEngine::isWidescreenStretched() {
	return (int)_wuRegistry->getFloat("Widescreen", 0.0f) == 2;
}

void GrimEngine::setPerfCounter(uint slot) {
	if (slot >= (uint)kNumPerfCounters)
		error("GrimEngine::setPerfCounter(): slot %u out of range", slot);

	_perfCounters[slot].start = (double)g_system->getMillis();
}

int GrimEngine::queryPerfCounterDiff(uint slot) {
	if (slot >= (uint)kNumPerfCounters)
		error("GrimEngine::queryPerfCounterDiff(): slot %u out of range", slot);

	double elapsed = (double)g_system->getMillis() - _perfCounters[slot].start;

	_perfCounters[slot].elapsed = elapsed;

	return (int)elapsed;
}

void GrimEngine::setPrevMode(int mode) {
	_sputPrevMode = mode;
}

int GrimEngine::getPrevMode() const {
	return _sputPrevMode;
}

void GrimEngine::overrideSelectedActorCostume(int instance) {
	SputActor *actor = _sputActor->getSelectedActor();

	if (actor) {
		_luaV1->doCostumeOverrideCallback(actor, 1);
		_sputActor->pushCostume(actor, nullptr, instance);
	}
}

void GrimEngine::restoreSelectedActorCostume() {
	SputActor *actor = _sputActor->getSelectedActor();

	if (actor) {
		_luaV1->doCostumeOverrideCallback(actor, 0);
		_sputActor->popCostume(actor, 0);
	}
}

int GrimEngine::setDisplay(StdVBuffer *vBuffer, StdVBuffer *zBuffer) {
	_sputRender->pageCountIs(_numDrawBuffers);
	_sputRender->setDisplay(vBuffer, zBuffer, 0, 0, vBuffer->tex.width - 1, vBuffer->tex.height - 1, _libRenderOptions);

	return 0;
}

int GrimEngine::unsetDisplay() {
	_sputRender->close();

	return 0;
}

void GrimEngine::makeCurrentSet(const char *name) {
	if (name) {
		Room *room = static_cast<Room *>(_sputResource->alloc(kSputTagSet, name));

		if (room && room->colormaps && room != _sputRoom->getCurrentRoom()) {
			_sputRoom->setCurrentRoom(room);

			if (isRemastered())
				_sputMouse->getHotspots()->load(name);

			_walkSystem->markPathFinderDirty();
			_walkSystem->setCollection(room->name);
			_sputRender->syncCamera(room, room->curSetup);
		}
	} else {
		_sputRoom->setCurrentRoom(nullptr);
	}
}

void GrimEngine::close() {
	if (_sputOpen) {
		_libMode = 0;
		_sputControl->close();
		_sputRender->close();
		_sputRoom->setCurrentRoom(nullptr);
		_sputOpen = 0;
	}
}

void GrimEngine::setDebugToggleCallback(void (*func)(int)) {
	_luaV1->setDebugToggleCallback(func);
}

void GrimEngine::setPaletteCallback(int (*func)(byte *, int)) {
	_sputRender->setPaletteCallback(func);
}

void GrimEngine::setShutdownCallback(int (*func)(int)) {
	_shutdownCallback = func;
}

void GrimEngine::setPageFlipCallback(int (*func)()) {
	_sputRender->setPageFlipCallback(func);
}

void GrimEngine::using3DHardware(int b3D) {
	_sputRender->using3DHardware(b3D);
}

int GrimEngine::addResourcePath(const char *path) {
	return _sputResource->addResourcePath(path);
}

int GrimEngine::findResources() {
	return _sputResource->findResources();
}

void GrimEngine::markDirtyRect(const StdRect *rectPointer, int page) {
	_sputRender->markDirtyRect(rectPointer, page);
}

void GrimEngine::makeCurrentSetupLib(uint setup) {
	Room *room = _sputRoom->getCurrentRoom();

	if (!room) {
		warning("GrimEngine::makeCurrentSetupLib(): no current room");
		return;
	}

	uint numSetups = room->numSetups;
	int newSetup = (int)setup;

	if (setup >= numSetups) {
		if (!numSetups) {
			warning("GrimEngine::makeCurrentSetupLib(): the room has no setups");
			return;
		}

		newSetup = (int)(setup % numSetups);
	}

	int oldSetup = room->curSetup;

	if (newSetup != oldSetup) {
		bool demo = (_gameFlags & ADGF_DEMO) != 0;

		room->curSetup = newSetup;

		if (!demo)
			_sputRoom->setSoundParameters(room, 8.0f, 20, 127, 1.5f, 0.8f);

		_luaV1->cameraChange(oldSetup, newSetup);

		if (!demo)
			_sputRoom->relocateSounds();
	}
}

void GrimEngine::nextSetup() {
	Room *room = _sputRoom->getCurrentRoom();

	if (!room || !room->numSetups) {
		warning("GrimEngine::nextSetup(): no current room or no setups");
		return;
	}

	makeCurrentSetupLib((uint)(room->curSetup + 1) % room->numSetups);
}

void GrimEngine::previousSetup() {
	Room *room = _sputRoom->getCurrentRoom();

	if (!room || !room->numSetups) {
		warning("GrimEngine::previousSetup(): no current room or no setups");
		return;
	}

	makeCurrentSetupLib((uint)(room->curSetup + room->numSetups - 1) % room->numSetups);
}

int GrimEngine::confirmClose() {
	int result = _luaV1->doExitCallback();

	return result == 0;
}

void GrimEngine::requestShutdown(int arg) {
	if (_shutdownCallback)
		_shutdownCallback(arg);
}

void GrimEngine::setPause(int pause) {
	if (!_libMode)
		return;

	if (pause) {
		if (_libMode != 1) {
			_libPauseSavedMode = _libMode;
			_libPauseSavedOldMode = _libPauseOldMode;
			_libMode = 1;
			_imuse->imPause();
			_sputSmush->pause(1);
		}
	} else if (_libMode == 1) {
		_libMode = _libPauseSavedMode;
		_libPauseOldMode = _libPauseSavedOldMode;
		_imuse->imResume();
		_sputSmush->pause(0);
	}
}

int GrimEngine::doPauseCallback() {
	int result = _luaV1->doPauseCallback();

	return result;
}

int GrimEngine::characterCallback(int character) {
	int result = _luaV1->doCharacterCallback(character);

	return result;
}

int GrimEngine::setImuseState(int state) {
	_libImuseState = state;

	return state;
}

int GrimEngine::playMovie(const char *name, int fullscreen, int looping, int x, int y) {
	if (fullscreen)
		_sputSmush->setTarget(_mainGfxBuffer);
	else
		_sputSmush->setTarget(_sputRender->drawBuffer());

	if (_sputSmush->startMovie(name, looping, x, y))
		return 0;

	if (fullscreen) {
		_libMode = 3;
		return 1;
	}

	if (_libMode == 3)
		_libMode = 2;

	return 1;
}

int GrimEngine::userPaint() {
	int result = 0;

	if (_libMode == 4) {
		_luaV1->doUserPaintCallback();
	}

	return result;
}

void GrimEngine::saveLib(const char *fileName) {
	if (isRemastered()) {
		warning("GrimEngine::saveLib(): the remastered saves through its own slots");

		return;
	}

	_libSaveRequest = 1;
	_libSaveGameFileName = fileName ? fileName : "";
}

void GrimEngine::loadLib(const char *fileName) {
	if (isRemastered()) {
		warning("GrimEngine::loadLib(): the remastered loads through its own slots");

		return;
	}

	_libLoadRequest = 1;
	_libSaveGameFileName = fileName ? fileName : "";
}

int GrimEngine::justLoaded() {
	int result = _libJustLoaded;

	_libJustLoaded = 0;

	return result;
}

static const StdColorFormat saveImageFormat = { 1, 16, 5, 6, 5, 11, 5, 0, 3, 2, 3, 0, 0, 0 };

static const int kSoundsSaveVersion = 30;

static int luaZwrite(void *buffer, int size) {
	return g_grim->zwrite(buffer, size);
}

static int luaZread(void *buffer, int size) {
	return g_grim->zread(buffer, size);
}

void GrimEngine::save() {
	if (isRemastered()) {
		saveRemastered();

		return;
	}

	_libSaveRequest = 0;
	_sputRender->allowLoadImage(0);
	_sputRender->drawProgressBar(0.1f);

	Common::String fileName = _libSaveGameFileName;

	if (fileName.empty())
		fileName = "grim.sav";

	_saveWriteStream = g_system->getSavefileManager()->openForSaving(fileName);

	if (!_saveWriteStream)
		return;

	_luaV1->saveGameCallback();

	StdVBuffer *shot = _sputRender->screenShot(250, 188);
	uint32 imageSize = 0;

	if (shot) {
		StdColorFormat format = saveImageFormat;
		StdVBuffer *image = _stdDisplay->vBufferConvertColorFormat(&format, shot);

		if (image) {
			imageSize = TO_LE_32((uint32)image->tex.sizeInBytes);
			zwrite(&imageSize, 4);
			zwrite(image->pixels, image->tex.sizeInBytes);
			_stdDisplay->vBufferFree(image);
		}
	} else {
		warning("GrimEngine::save(): no screenshot for the savegame");
	}

	if (!imageSize)
		zwrite(&imageSize, 4);

	_sputRender->drawProgressBar(0.2f);
	_sputCostume->getCos()->save(_saveWriteStream);
	_sputRender->drawProgressBar(0.3f);
	_res->sysSave(_saveWriteStream);
	_sputRender->drawProgressBar(0.4f);
	_sputText->save(_saveWriteStream);
	_sputRoom->save(_saveWriteStream);
	_sputRender->drawProgressBar(0.5f);
	_sputActor->save(_saveWriteStream);
	_sputRender->save(_saveWriteStream);
	_sputPrimitive->save(_saveWriteStream);
	_sputSmush->save(_saveWriteStream);

	uint32 prevMode = TO_LE_32((uint32)_sputPrevMode);

	zwrite(&prevMode, 4);
	_sputRender->drawProgressBar(0.75f);
	lua_Save(luaZwrite);

	_sputRender->drawProgressBar(1.0f);
	soundsSave(_saveWriteStream);

	_saveWriteStream->finalize();
	delete _saveWriteStream;
	_saveWriteStream = nullptr;

	_sputRender->allowLoadImage(1);
}

int GrimEngine::zwrite(const void *buffer, int size) {
	if (size && _saveWriteStream->write(buffer, size) != (uint32)size)
		error("GrimEngine::zwrite(): write error");

	return 0;
}

void GrimEngine::restore() {
	if (isRemastered()) {
		restoreRemastered();

		return;
	}

	_libLoadRequest = 0;

	Common::String fileName = _libSaveGameFileName;

	if (fileName.empty())
		fileName = "grim.sav";

	_saveReadStream = g_system->getSavefileManager()->openForLoading(fileName);

	if (!_saveReadStream)
		return;

	if (LegacySaveFile::isLegacySave(_saveReadStream)) {
		restoreLegacy();
		return;
	}

	restorePrologue();

	uint32 blockSize = 0;

	zread(&blockSize, 4);
	_saveReadStream->seek((int)FROM_LE_32(blockSize), SEEK_CUR);

	uint32 imageSize = 0;

	zread(&imageSize, 4);
	_saveReadStream->seek((int)FROM_LE_32(imageSize), SEEK_CUR);

	_sputRender->drawProgressBar(0.2f);
	_sputCostume->getCos()->restore(_saveReadStream);
	_sputRender->drawProgressBar(0.3f);
	_res->sysLoad(_saveReadStream);
	_sputText->restore(_saveReadStream);
	_sputRender->drawProgressBar(0.4f);
	_sputRoom->restore(_saveReadStream);
	_sputActor->restore(_saveReadStream);
	_sputRender->restore(_saveReadStream);
	_sputPrimitive->restore(_saveReadStream);
	_sputSmush->restore(_saveReadStream);

	_libMode = 4;

	uint32 prevMode = 0;

	zread(&prevMode, 4);
	_sputPrevMode = (int)FROM_LE_32(prevMode);
	_sputRender->drawProgressBar(0.75f);
	lua_Restore(luaZread);

	_sputRender->drawProgressBar(1.0f);
	soundsRestore(_saveReadStream);
	restoreEpilogue();
}

void GrimEngine::restorePrologue() {
	_sputRender->allowLoadImage(0);
	_sputRender->drawProgressBar(0.1f);
	_imuse->imStopAllSounds();
	_sputSmush->stopMovie();
	_sputSmush->periodic();
	_res->freeAll();
	_sputActor->clearShadowMaterial();
	_sputResource->stopPurge(1);
}

void GrimEngine::restoreEpilogue() {
	_sputResource->stopPurge(0);

	Room *room = _sputRoom->getCurrentRoom();
	int curSetup = 0;

	if (room)
		curSetup = room->curSetup;

	_sputRender->useSetup(room, curSetup);

	delete _saveReadStream;
	_saveReadStream = nullptr;

	_luaV1->dofile("patch05.bin");
	_libJustLoaded = 1;
	_sputRender->allowLoadImage(1);

	StdRect rect = { 0, 0, 640, 480 };

	_sputRender->markAllPagesDirty(&rect, 1);
}

int GrimEngine::zread(void *buffer, int size) {
	if (size)
		_saveReadStream->read(buffer, size);

	return 0;
}

StdVBuffer *GrimEngine::getSaveGameImage(const char *fileName) {
	Common::SeekableReadStream *file = g_system->getSavefileManager()->openForLoading(fileName);

	if (!file)
		return nullptr;

	Common::Array<byte> legacyPixels;
	bool legacy = LegacySaveFile::isLegacySave(file);

	if (legacy) {
		if (!legacyReadThumbnail(file, legacyPixels)) {
			delete file;
			return nullptr;
		}
	} else {
		int blockSize = file->readSint32LE();

		file->seek(blockSize, SEEK_CUR);
	}

	StdVBufferTexFmt format;

	memset(&format, 0, sizeof(format));
	format.pitchInPixels = 2;
	format.width = 250;
	format.height = 188;
	format.pitchInBytes = 500;
	format.format = saveImageFormat;

	StdVBuffer *image = _stdDisplay->vBufferNew(&format, 0, 0, nullptr);

	if (!image) {
		delete file;
		return nullptr;
	}

	int size = legacy ? (int)legacyPixels.size() : file->readSint32LE();

	if (size != image->tex.sizeInBytes) {
		warning("GrimEngine::getSaveGameImage(): unexpected image size %d", size);
		_stdDisplay->vBufferFree(image);
		delete file;
		return nullptr;
	}

	if (legacy)
		memcpy(image->pixels, legacyPixels.data(), size);
	else
		file->read(image->pixels, size);

	delete file;

	return image;
}

void GrimEngine::imuseSave() {
	Common::OutSaveFile *file = g_system->getSavefileManager()->openForSaving("grim.tmp");

	if (!file)
		return;

	soundsSave(file);
	file->finalize();
	delete file;
}

void GrimEngine::imuseRestore() {
	Common::InSaveFile *file = g_system->getSavefileManager()->openForLoading("grim.tmp");

	if (!file)
		return;

	_imuse->imStopAllSounds();
	soundsRestore(file);
	delete file;
	g_system->getSavefileManager()->removeSavefile("grim.tmp");
}

void GrimEngine::soundsSave(Common::WriteStream *stream) {
	Common::Serializer ser(nullptr, stream);

	ser.setVersion(kSoundsSaveVersion);
	_imuse->soundsSaveLoad(ser);
}

void GrimEngine::soundsRestore(Common::SeekableReadStream *stream, bool legacyLayout) {
	Common::Serializer ser(stream, nullptr);

	ser.setVersion(kSoundsSaveVersion);
	_imuse->soundsSaveLoad(ser, legacyLayout);
}

enum {
	kDisplayWidth = 640,
	kDisplayHeight = 480
};

void GrimEngine::fatalError(const char *text, const char *caption) {
	Common::String message = Common::String::format("%s\n\n%s", caption, text);

	GUI::displayErrorDialog(Common::U32String(message));
	shutdownDisplay();
	CursorMan.showMouse(true);
}

int GrimEngine::initDisplay() {
	memset(&_videoDeviceConfig, 0, sizeof(_videoDeviceConfig));
	_videoDeviceConfig.tex.width = kDisplayWidth;
	_videoDeviceConfig.tex.height = kDisplayHeight;
	_videoDeviceConfig.tex.format = _colorInfoCopy;

	freeVideoDevices();

	uint numDevices = _stdDisplay->getNumDevices();

	for (uint i = 0; i < numDevices; i++) {
		GrimVideoDevice *videoDevice = &_videoDevices[i];
		uint numModes = 0;

		if (_stdDisplay->enumerateModes(i))
			numModes = _stdDisplay->getNumModes();

		videoDevice->numModes = numModes;
		videoDevice->modes = new StdDisplayMode[numModes];
		memcpy(videoDevice->modes, _stdDisplay->getModes(), sizeof(StdDisplayMode) * numModes);
		videoDevice->num3DDevices = 0;
		videoDevice->a3DDevices = nullptr;

		StdDisplayDevice *device = &_stdDisplay->getDevices()[i];

		if (device->type == 1 && !numModes)
			_stdDisplay->setDeviceUsable(i, 0);

#if defined(GRIM_STD3D)
		if (device->type == 1 && device->usable && device->b3D) {
			if (_stdDisplay->createBackend(device->backend) && (_std3D->canProbeWithoutMode() || _std3D->openBackendMode(kDisplayWidth, kDisplayHeight)) && _std3D->enumerateDevices()) {
				uint numThreeDDevices = _std3D->getNumDevices();

				videoDevice->num3DDevices = numThreeDDevices;
				videoDevice->a3DDevices = new Std3DDevice[numThreeDDevices];
				memcpy(videoDevice->a3DDevices, _std3D->getDevices(), sizeof(Std3DDevice) * numThreeDDevices);
				_std3D->releaseDirect3D();
			} else {
				warning("GrimEngine::initDisplay(): the display device \"%s\" can not be used", device->description);
				_stdDisplay->setDeviceUsable(i, 0);
			}

			_std3D->closeBackend();
		}
#endif

		_stdDisplay->closeDisplay();
	}

	return numDevices;
}

void GrimEngine::freeVideoDevices() {
	for (int i = 0; i < kStdDisplayMaxDevices; i++) {
		GrimVideoDevice *videoDevice = &_videoDevices[i];

		delete[] videoDevice->a3DDevices;
		videoDevice->a3DDevices = nullptr;
		delete[] videoDevice->modes;
		videoDevice->modes = nullptr;
		videoDevice->numModes = 0;
		videoDevice->num3DDevices = 0;
	}
}

int GrimEngine::initDisplayAndDevice() {
	int device;
	int mode;
	int threeDDevice;

	if (!loadDisplaySettings(&device, &mode, &threeDDevice) || (threeDDevice != -1) != (_useHardware != 0))
		chooseDefaultDisplay(_show && _enumerateVideo, _useHardware, &device, &mode, &threeDDevice);

	int result = selectDisplayMode(device, mode, threeDDevice);

	if (result && threeDDevice != -1 && !isHardware3D()) {
		warning("GrimEngine::initDisplayAndDevice(): the 3D device could not be started");
		result = 0;
	}

	if (!result && _stdDisplay->getDevices()[device].type == 1) {
		warning("GrimEngine::initDisplayAndDevice(): the hardware display failed, going back to another display");

		result = selectFallbackDisplay(device, threeDDevice != -1);
	}

	return result;
}

int GrimEngine::selectFallbackDisplay(int failedDevice, int want3D) {
	int device = failedDevice;
	int mode = 0;
	int threeDDevice = -1;
	int result = 0;

	do {
		if (device >= 0 && (uint)device < _stdDisplay->getNumDevices() && _stdDisplay->getDevices()[device].type == 1)
			_stdDisplay->setDeviceUsable(device, 0);

		chooseDefaultDisplay(0, want3D, &device, &mode, &threeDDevice);
		result = selectDisplayMode(device, mode, threeDDevice);

		if (result && threeDDevice != -1 && !isHardware3D()) {
			warning("GrimEngine::selectFallbackDisplay(): the 3D device could not be started");
			result = 0;
		}
	} while (!result && _stdDisplay->getDevices()[device].type == 1);

	return result;
}

int GrimEngine::loadDisplaySettings(int *devicePointer, int *mode, int *p3DDevice) {
	if (_wuRegistry->getInt("ExperimentalDisplay", 1)) {
		debug("grimdemo_DefaultDisplay: Last switch was not successful.");
		return 0;
	}

	byte zeroGuid[16];
	byte lastDisplayGuid[16];
	byte last3DGuid[16];

	memset(zeroGuid, 0, sizeof(zeroGuid));
	memset(lastDisplayGuid, 0, sizeof(lastDisplayGuid));
	memset(last3DGuid, 0, sizeof(last3DGuid));

	if (!_wuRegistry->getBinary("LastDisplayDevice", lastDisplayGuid, sizeof(lastDisplayGuid))) {
		debug("grimdemo_DefaultDisplay: Could not find matching device.");
		return 0;
	}

	_wuRegistry->getBinary("Last3DDevice", last3DGuid, sizeof(last3DGuid));

	uint numDevices = _stdDisplay->getNumDevices();
	StdDisplayDevice *devices = _stdDisplay->getDevices();
	uint device = 0;

	while (device < numDevices) {
		if (!memcmp(devices[device].guid, lastDisplayGuid, sizeof(lastDisplayGuid)))
			break;

		device++;
	}

	if (device == numDevices || (devices[device].type == 1 && !devices[device].usable)) {
		debug("grimdemo_DefaultDisplay: Could not find matching device.");
		return 0;
	}

	GrimVideoDevice *videoDevice = &_videoDevices[device];

	*devicePointer = device;
	*mode = _stdDisplay->findClosestMode(&_videoDeviceConfig, videoDevice->modes, videoDevice->numModes);

	if (!memcmp(last3DGuid, zeroGuid, sizeof(zeroGuid))) {
		if (devices[device].backend == kStdDisplayBackendTinyGl) {
			debug("grimdemo_DefaultDisplay: The TinyGL display is only used with a 3D device.");
			return 0;
		}

		*p3DDevice = -1;
		return 1;
	}

	uint threeDDevice = 0;

	while (threeDDevice < videoDevice->num3DDevices) {
		if (!memcmp(videoDevice->a3DDevices[threeDDevice].guid, last3DGuid, sizeof(last3DGuid)))
			break;

		threeDDevice++;
	}

	if (threeDDevice == videoDevice->num3DDevices) {
		debug("grimdemo_DefaultDisplay: 3D device no longer present.");
		return 0;
	}

	*p3DDevice = threeDDevice;
	return 1;
}

int GrimEngine::chooseDefaultDisplay(int preferWindowed, int want3D, int *devicePointer, int *mode, int *p3DDevice) {
	StdDisplayDeviceFlags wanted;

	int backend = kStdDisplayBackendNone;

	if (!preferWindowed)
		backend = chooseHardwareBackend();

	if (backend == kStdDisplayBackendNone || (backend == kStdDisplayBackendTinyGl && !want3D)) {
		backend = kStdDisplayBackendNone;
		preferWindowed = 1;
		want3D = 0;
	}

	if (preferWindowed) {
		wanted.type = 0;
		wanted.hasHardware = 0;
	} else {
		wanted.type = 1;
		wanted.hasHardware = 1;
	}

	wanted.hasGuid = 1;
	wanted.b3D = want3D;
	wanted.primary = want3D == 0;
	wanted.backend = backend;

	int device = _stdDisplay->findClosestDevice(&wanted);
	GrimVideoDevice *videoDevice = &_videoDevices[device];

	*devicePointer = device;
	*mode = _stdDisplay->findClosestMode(&_videoDeviceConfig, videoDevice->modes, videoDevice->numModes);
	*p3DDevice = -1;

#if defined(GRIM_STD3D)
	if (videoDevice->num3DDevices && want3D) {
		Std3DDevice wantedDevice;

		memset(&wantedDevice, 0, sizeof(wantedDevice));
		wantedDevice.hardware = 1;
		wantedDevice.perspectiveCorrect = 1;
		wantedDevice.zBuffer = 1;
		wantedDevice.colorModelFlags = 2;

		int threeDDevice = _std3D->findClosestDevice(&wantedDevice, videoDevice->a3DDevices, videoDevice->num3DDevices);

		*p3DDevice = threeDDevice;

		if (!videoDevice->a3DDevices[threeDDevice].hardware)
			*p3DDevice = -1;
	}
#endif

	return 1;
}

int GrimEngine::selectDisplayMode(int deviceIndex, int modeIndex, int threeDIndex) {
	shutdownDisplay();

	if (_displayOpen)
		return 0;

	if (deviceIndex < 0 || (uint)deviceIndex >= _stdDisplay->getNumDevices()) {
		warning("GrimEngine::selectDisplayMode(): invalid display device %d", deviceIndex);
		return 0;
	}

	StdDisplayDevice *device = &_stdDisplay->getDevices()[deviceIndex];

	if (device->type == 1 && !device->usable) {
		warning("GrimEngine::selectDisplayMode(): the hardware display is not available");
		return 0;
	}

	_wuRegistry->saveInt("ExperimentalDisplay", 1);

	if (!_stdDisplay->enumerateModes(deviceIndex))
		return 0;

	StdVBufferTexFmt displayTex;

	if (device->type == 1) {
		if (!_stdDisplay->openDisplay(modeIndex, nullptr, 1)) {
			_stdDisplay->closeDisplay();
			return 0;
		}

		displayTex = _stdDisplay->getDisplayMode()->tex;
		_colorInfoCopy = displayTex.format;
		_numDrawBuffers = _stdDisplay->getNumDrawBuffers();
	} else {
		Graphics::PixelFormat pixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);

		if (_hdCanvas) {
			_hdCanvas->open();
		} else {
			initGraphics(kDisplayWidth, kDisplayHeight, &pixelFormat);

			if (g_system->getScreenFormat() != pixelFormat)
				error("GrimEngine::selectDisplayMode(): the screen does not support the RGB565 pixel format");
		}

		memset(&displayTex, 0, sizeof(displayTex));
		displayTex.width = kDisplayWidth;
		displayTex.height = kDisplayHeight;
		displayTex.pitchInBytes = kDisplayWidth * 2;
		displayTex.pitchInPixels = kDisplayWidth;
		displayTex.sizeInBytes = kDisplayWidth * kDisplayHeight * 2;
		displayTex.format = _colorInfoCopy;

		_mainGfxBuffer->type = 0;
		_mainGfxBuffer->lockCount = 0;
		_mainGfxBuffer->videoMemory = 0;
		_mainGfxBuffer->tex = displayTex;
		_mainGfxBuffer->paletteRGB = nullptr;
		_mainGfxBuffer->pixels = new byte[displayTex.sizeInBytes]();
		_mainGfxBuffer->allocSize = displayTex.sizeInBytes;
		_mainGfxBuffer->colorKey = 0;
		*_backBuffer = *_mainGfxBuffer;
		_numDrawBuffers = 1;
	}

	_displayOpen = 1;

	_stdDisplay->vBufferFill(_backBuffer, 0, nullptr);
	pageFlipCallback();
	_stdDisplay->vBufferFill(_backBuffer, 0, nullptr);
	_rd->setColorInfo(_colorInfoCopy);

	StdVBuffer *zBuffer = nullptr;

	if (select3DDevice(threeDIndex)) {
#if defined(GRIM_STD3D)
		zBuffer = _std3D->getZBuffer();
#endif
	} else {
		StdVBufferTexFmt zFormat = displayTex;

		zFormat.format.bpp = 16;
		zBuffer = _stdDisplay->vBufferNew(&zFormat, 0, 0, nullptr);
	}

	_zBuffer = zBuffer;
	setDisplay(_backBuffer, _zBuffer);
	_wuRegistry->saveBinary("LastDisplayDevice", device->guid, sizeof(device->guid));
	_show = device->type == 0;

	if (device->type == 1)
		_preferredBackend = device->backend;

	return 1;
}

int GrimEngine::select3DDevice(int index) {
	shutdown3D();

#if defined(GRIM_STD3D)
	if (index != -1 && _std3D->enumerateDevices()) {
		GrimVideoDevice *videoDevice = &_videoDevices[getCurVideoDeviceIndex()];

		if (index < 0 || (uint)index >= videoDevice->num3DDevices)
			warning("GrimEngine::select3DDevice(): invalid 3D device %d", index);

		if (_std3D->startup(index, 1)) {
			_rd->setAcceleration(_rd->getAcceleration() | 1);
			using3DHardware(1);
			_wuRegistry->saveBinary("Last3DDevice", _std3D->getCurDevice()->guid, sizeof(_std3D->getCurDevice()->guid));
			ConfMan.setBool("grim_3d_acceleration", true);

			return 1;
		}
	}
#endif

	byte zeroGuid[16];

	memset(zeroGuid, 0, sizeof(zeroGuid));
	_wuRegistry->saveBinary("Last3DDevice", zeroGuid, sizeof(zeroGuid));

	if (hasUsableHardwareDevice())
		ConfMan.setBool("grim_3d_acceleration", false);

	return 0;
}

void GrimEngine::shutdown3D() {
#if defined(GRIM_STD3D)
	if (_std3D->isAvailable()) {
		_std3D->shutdown();
		_std3D->releaseDirect3D();
		_rd->setAcceleration(_rd->getAcceleration() & ~1);
		using3DHardware(0);
	}
#endif
}

void GrimEngine::shutdownDisplay() {
	if (!_displayOpen)
		return;

	StdDisplayDevice *device = _stdDisplay->getCurDevice();
	bool videoDevice = device && device->type == 1;

	unsetDisplay();

	if (isHardware3D())
		shutdown3D();
	else
		_stdDisplay->vBufferFree(_zBuffer);

	_zBuffer = nullptr;
	_stdDisplay->releaseSurfaces();
	_stdDisplay->closeDisplay();

#if defined(GRIM_STD3D)
	if (videoDevice)
		_std3D->closeBackend();
#endif

	if (!videoDevice) {
		delete[] _mainGfxBuffer->pixels;
		_mainGfxBuffer->pixels = nullptr;
		_backBuffer->pixels = nullptr;
	}

	if (_hdCanvas)
		_hdCanvas->close();

	_displayOpen = 0;
	_wuRegistry->saveInt("ExperimentalDisplay", 0);
}

int GrimEngine::getCurVideoDeviceIndex() {
	return _stdDisplay->getCurDeviceIndex();
}

int GrimEngine::getCur3DDeviceIndex() {
#if defined(GRIM_STD3D)
	if (isHardware3D())
		return _std3D->getCurDeviceIndex();
#endif

	return -1;
}

int GrimEngine::isHardware3D() {
	return _sputRender->is3DHardware();
}

int GrimEngine::paletteCallback(byte *palette, int alpha) {
#if defined(GRIM_STD3D)
	if (g_grim->isHardware3D())
		g_grim->_std3D->setCurrentPalette(palette, (byte)alpha);
#endif

	return 1;
}

void GrimEngine::loadSplash() {
	StdBitmapStruct *bitmap = _stdBitmap->zbmLoad("splash.bm", 0, 0);

	if (!bitmap)
		return;

	StdVBuffer *frame = bitmap->frames[0];

	if (!frame)
		return;

	for (int i = 0; i < frame->tex.height; i++)
		_sputRender->colorCorrect((uint16 *)(frame->pixels + frame->tex.pitchInBytes * i), frame->tex.width);

	_stdDisplay->vBufferCopy(_mainGfxBuffer, frame, 0, 0, nullptr, 0);

	if (_backBuffer->type == 1)
		_stdDisplay->vBufferCopy(_backBuffer, frame, 0, 0, nullptr, 0);

	pageFlipCallback();
	_stdDisplay->vBufferCopy(_mainGfxBuffer, frame, 0, 0, nullptr, 0);
	_stdBitmap->unloadBitmap(bitmap);
}

int GrimEngine::pageFlipCallback() {
	StdDisplayDevice *device = g_grim->_stdDisplay->getCurDevice();

	if (g_grim->_displayOpen && device && device->type == 1)
		return g_grim->_stdDisplay->update();

	const StdVBuffer *buffer = g_grim->_mainGfxBuffer;

	if (!buffer->pixels) {
		warning("GrimEngine::pageFlipCallback(): the display is not open");
		return 0;
	}

	if (g_grim->_hdCanvas && g_grim->_hdCanvas->isActive()) {
		float gamma = g_grim->_sputRender ? g_grim->_sputRender->getGamma() : 1.0f;

		g_grim->_hdCanvas->present(buffer, gamma);
		return 1;
	}

	g_system->copyRectToScreen(buffer->pixels, buffer->tex.pitchInBytes, 0, 0, buffer->tex.width, buffer->tex.height);
	g_system->updateScreen();

	return 1;
}

static int keyCodeToScanCode(Common::KeyCode keyCode) {
	switch (keyCode) {
	case Common::KEYCODE_ESCAPE:
		return 1;
	case Common::KEYCODE_1:
		return 2;
	case Common::KEYCODE_2:
		return 3;
	case Common::KEYCODE_3:
		return 4;
	case Common::KEYCODE_4:
		return 5;
	case Common::KEYCODE_5:
		return 6;
	case Common::KEYCODE_6:
		return 7;
	case Common::KEYCODE_7:
		return 8;
	case Common::KEYCODE_8:
		return 9;
	case Common::KEYCODE_9:
		return 10;
	case Common::KEYCODE_0:
		return 11;
	case Common::KEYCODE_MINUS:
		return 12;
	case Common::KEYCODE_EQUALS:
		return 13;
	case Common::KEYCODE_BACKSPACE:
		return 14;
	case Common::KEYCODE_TAB:
		return 15;
	case Common::KEYCODE_q:
		return 16;
	case Common::KEYCODE_w:
		return 17;
	case Common::KEYCODE_e:
		return 18;
	case Common::KEYCODE_r:
		return 19;
	case Common::KEYCODE_t:
		return 20;
	case Common::KEYCODE_y:
		return 21;
	case Common::KEYCODE_u:
		return 22;
	case Common::KEYCODE_i:
		return 23;
	case Common::KEYCODE_o:
		return 24;
	case Common::KEYCODE_p:
		return 25;
	case Common::KEYCODE_LEFTBRACKET:
		return 26;
	case Common::KEYCODE_RIGHTBRACKET:
		return 27;
	case Common::KEYCODE_RETURN:
		return 28;
	case Common::KEYCODE_LCTRL:
		return 29;
	case Common::KEYCODE_a:
		return 30;
	case Common::KEYCODE_s:
		return 31;
	case Common::KEYCODE_d:
		return 32;
	case Common::KEYCODE_f:
		return 33;
	case Common::KEYCODE_g:
		return 34;
	case Common::KEYCODE_h:
		return 35;
	case Common::KEYCODE_j:
		return 36;
	case Common::KEYCODE_k:
		return 37;
	case Common::KEYCODE_l:
		return 38;
	case Common::KEYCODE_SEMICOLON:
		return 39;
	case Common::KEYCODE_QUOTE:
		return 40;
	case Common::KEYCODE_BACKQUOTE:
		return 41;
	case Common::KEYCODE_LSHIFT:
		return 42;
	case Common::KEYCODE_BACKSLASH:
		return 43;
	case Common::KEYCODE_z:
		return 44;
	case Common::KEYCODE_x:
		return 45;
	case Common::KEYCODE_c:
		return 46;
	case Common::KEYCODE_v:
		return 47;
	case Common::KEYCODE_b:
		return 48;
	case Common::KEYCODE_n:
		return 49;
	case Common::KEYCODE_m:
		return 50;
	case Common::KEYCODE_COMMA:
		return 51;
	case Common::KEYCODE_PERIOD:
		return 52;
	case Common::KEYCODE_SLASH:
		return 53;
	case Common::KEYCODE_RSHIFT:
		return 54;
	case Common::KEYCODE_KP_MULTIPLY:
		return 55;
	case Common::KEYCODE_LALT:
		return 56;
	case Common::KEYCODE_SPACE:
		return 57;
	case Common::KEYCODE_CAPSLOCK:
		return 58;
	case Common::KEYCODE_F1:
		return 59;
	case Common::KEYCODE_F2:
		return 60;
	case Common::KEYCODE_F3:
		return 61;
	case Common::KEYCODE_F4:
		return 62;
	case Common::KEYCODE_F5:
		return 63;
	case Common::KEYCODE_F6:
		return 64;
	case Common::KEYCODE_F7:
		return 65;
	case Common::KEYCODE_F8:
		return 66;
	case Common::KEYCODE_F9:
		return 67;
	case Common::KEYCODE_F10:
		return 68;
	case Common::KEYCODE_NUMLOCK:
		return 69;
	case Common::KEYCODE_SCROLLOCK:
		return 70;
	case Common::KEYCODE_KP7:
		return 71;
	case Common::KEYCODE_KP8:
		return 72;
	case Common::KEYCODE_KP9:
		return 73;
	case Common::KEYCODE_KP_MINUS:
		return 74;
	case Common::KEYCODE_KP4:
		return 75;
	case Common::KEYCODE_KP5:
		return 76;
	case Common::KEYCODE_KP6:
		return 77;
	case Common::KEYCODE_KP_PLUS:
		return 78;
	case Common::KEYCODE_KP1:
		return 79;
	case Common::KEYCODE_KP2:
		return 80;
	case Common::KEYCODE_KP3:
		return 81;
	case Common::KEYCODE_KP0:
		return 82;
	case Common::KEYCODE_KP_PERIOD:
		return 83;
	case Common::KEYCODE_F11:
		return 87;
	case Common::KEYCODE_F12:
		return 88;
	case Common::KEYCODE_KP_EQUALS:
		return 141;
	case Common::KEYCODE_KP_ENTER:
		return 156;
	case Common::KEYCODE_RCTRL:
		return 157;
	case Common::KEYCODE_KP_DIVIDE:
		return 181;
	case Common::KEYCODE_SYSREQ:
	case Common::KEYCODE_PRINT:
		return 183;
	case Common::KEYCODE_RALT:
		return 184;
	case Common::KEYCODE_PAUSE:
		return 197;
	case Common::KEYCODE_HOME:
		return 199;
	case Common::KEYCODE_UP:
		return 200;
	case Common::KEYCODE_PAGEUP:
		return 201;
	case Common::KEYCODE_LEFT:
		return 203;
	case Common::KEYCODE_RIGHT:
		return 205;
	case Common::KEYCODE_END:
		return 207;
	case Common::KEYCODE_DOWN:
		return 208;
	case Common::KEYCODE_PAGEDOWN:
		return 209;
	case Common::KEYCODE_INSERT:
		return 210;
	case Common::KEYCODE_DELETE:
		return 211;
	case Common::KEYCODE_LSUPER:
	case Common::KEYCODE_LMETA:
		return 219;
	case Common::KEYCODE_RSUPER:
	case Common::KEYCODE_RMETA:
		return 220;
	case Common::KEYCODE_MENU:
		return 221;
	default:
		return 0;
	}
}

void GrimEngine::pumpEvents() {
	Common::EventManager *eventMan = g_system->getEventManager();
	StdControlInput *input = _stdControl->getInput();
	Common::Event event;

	while (eventMan->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_QUIT:
		case Common::EVENT_RETURN_TO_LAUNCHER:
			if (!isRemastered() && !confirmClose()) {
				eventMan->resetQuit();
				eventMan->resetReturnToLauncher();
			}

			break;
		case Common::EVENT_FOCUS_GAINED:
			handleFocus(1);
			break;
		case Common::EVENT_FOCUS_LOST:
			handleFocus(0);
			break;
		case Common::EVENT_KEYDOWN:
			handleKey(event.type, event.kbd, event.kbdRepeat);
			break;
		case Common::EVENT_KEYUP:
			handleKey(event.type, event.kbd, false);
			break;
		case Common::EVENT_MOUSEMOVE:
			input->mouse.x += event.relMouse.x;
			input->mouse.y += event.relMouse.y;
			break;
		case Common::EVENT_WHEELUP:
			input->mouse.z += 120;
			break;
		case Common::EVENT_WHEELDOWN:
			input->mouse.z -= 120;
			break;
		case Common::EVENT_LBUTTONDOWN:
			addMouseButtonEvent(12, 0x80);
			break;
		case Common::EVENT_LBUTTONUP:
			addMouseButtonEvent(12, 0);
			break;
		case Common::EVENT_RBUTTONDOWN:
			addMouseButtonEvent(isRemastered() ? 14 : 13, 0x80);
			break;
		case Common::EVENT_RBUTTONUP:
			addMouseButtonEvent(isRemastered() ? 14 : 13, 0);
			break;
		case Common::EVENT_MBUTTONDOWN:
			addMouseButtonEvent(isRemastered() ? 13 : 14, 0x80);
			break;
		case Common::EVENT_MBUTTONUP:
			addMouseButtonEvent(isRemastered() ? 13 : 14, 0);
			break;
		case Common::EVENT_X1BUTTONDOWN:
			addMouseButtonEvent(15, 0x80);
			break;
		case Common::EVENT_X1BUTTONUP:
			addMouseButtonEvent(15, 0);
			break;
		case Common::EVENT_JOYAXIS_MOTION:
			if (event.joystick.axis < 6) {
				int position = event.joystick.position;

				if (isRemastered() && ABS(position) < 12000)
					position = 0;

				input->joyState[0].axisPos[event.joystick.axis] = position;
			}

			break;
		case Common::EVENT_CUSTOM_BACKEND_ACTION_AXIS: {
			int axis = applyStickAction(_stickAxes, event);

			if (axis >= 0) {
				int position = _stickAxes[axis];

				if (isRemastered() && ABS(position) < 12000)
					position = 0;

				input->joyState[0].axisPos[axis] = position;
			}

			break;
		}
		case Common::EVENT_JOYBUTTON_DOWN: {
			int button = getJoystickButtonIndex(event.joystick.button, isRemastered());

			if (button >= 0)
				input->joyState[0].buttons |= 1 << button;

			break;
		}
		case Common::EVENT_JOYBUTTON_UP: {
			int button = getJoystickButtonIndex(event.joystick.button, isRemastered());

			if (button >= 0)
				input->joyState[0].buttons &= ~(1 << button);

			break;
		}
		default:
			break;
		}
	}
}

void GrimEngine::handleFocus(int hasFocus) {
	if (hasFocus == _hasFocus)
		return;

	_hasFocus = hasFocus;
	_stdControl->setActivation(hasFocus);

	if (hasFocus) {
		_sputRender->forceRefresh();

		if (_stdControl->isOpen())
			_stdControl->flush();

		if (!isRemastered())
			CursorMan.showMouse(true);
	}
}

void GrimEngine::handleKey(Common::EventType type, const Common::KeyState &key, bool repeat) {
	Common::KeyCode keycode = key.keycode;

	if (isRemastered()) {
		if (type == Common::EVENT_KEYDOWN && !repeat && _stdControl->isKeyMappingActive()) {
			_luaV1->notifyKeyMapped(_stdControl->endKeyMapping(keycode));
			keycode = Common::KEYCODE_INVALID;
		} else {
			keycode = _stdControl->translateKey(keycode);
		}
	}

	int scanCode = keyCodeToScanCode(keycode);

	if (type == Common::EVENT_KEYUP) {
		if (scanCode)
			addKeyboardEvent(scanCode, 0);

		if (key.keycode == Common::KEYCODE_BACKSPACE)
			_backspaceHeldFlag = 0;

		return;
	}

	if (scanCode && (!repeat || isRemastered()))
		addKeyboardEvent(scanCode, 0x80);

	handleKeyDebug(key);

	if (key.ascii > 0 && key.ascii < 256 && key.ascii != 127)
		characterCallback(key.ascii);
}

void GrimEngine::handleKeyDebug(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_BACKSPACE:
		if (_debugKeys)
			_backspaceHeldFlag = 1;

		break;
	case Common::KEYCODE_PAUSE:
		doPauseCallback();
		break;
	case Common::KEYCODE_f:
		if (_backspaceHeldFlag)
			_showFps = !_showFps;

		break;
	case Common::KEYCODE_p:
		if (_backspaceHeldFlag)
			_showDebugOverlay = !_showDebugOverlay;

		break;
	case Common::KEYCODE_s:
		if (_backspaceHeldFlag)
			_sputRender->saveDrawBuffer(Common::String::format("shot%d.bmp", _screenshotCounter++).c_str());

		break;
	case Common::KEYCODE_t:
		if (_backspaceHeldFlag) {
#if defined(GRIM_STD3D)
			_std3D->purgeTextureCache();
#endif
		}

		break;
	case Common::KEYCODE_z:
		if (_backspaceHeldFlag)
			_sputRender->saveZBuffer(Common::String::format("shot%d.zbm", _zbufShotCounter++).c_str());

		break;
	case Common::KEYCODE_F12:
		if (_backspaceHeldFlag)
			_luaV1->dofile("grimpatch.bin");

		break;
	default:
		break;
	}
}

void GrimEngine::addKeyboardEvent(uint offset, int data) {
	StdControlInput *input = _stdControl->getInput();

	input->keyboardState[offset] = (byte)data;

	if (input->numKeyboardEvents < 32) {
		StdControlDeviceEvent *event = &input->keyboardEvents[input->numKeyboardEvents];

		event->offset = offset;
		event->data = data;
		event->timeStamp = (int)g_system->getMillis();
		input->numKeyboardEvents++;
	} else {
		input->keyboardOverflow = 1;
	}
}

void GrimEngine::addMouseButtonEvent(uint offset, int data) {
	StdControlInput *input = _stdControl->getInput();

	input->mouse.buttons[offset - 12] = (byte)data;

	if (input->numMouseEvents < 32) {
		StdControlDeviceEvent *event = &input->mouseEvents[input->numMouseEvents];

		event->offset = offset;
		event->data = data;
		event->timeStamp = (int)g_system->getMillis();
		input->numMouseEvents++;
	} else {
		input->mouseOverflow = 1;
	}
}

} // end of namespace Grim
