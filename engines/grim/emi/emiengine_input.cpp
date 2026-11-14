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

#include "common/config-manager.h"
#include "common/events.h"
#include "common/keyboard.h"
#include "common/platform.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/translation.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"

#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/input/eventgenerator.h"
#include "engines/grim/emi/input/inputrecorder.h"
#include "engines/grim/emi/input/joystick.h"
#include "engines/grim/emi/input/keygenerator.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/input/sputcontrol_v2.h"

namespace Grim {

static const int kVirtualKeyBackspace = 0x08;
static const int kVirtualKeyControl = 0x11;
static const int kVirtualKeyPause = 0x13;
static const int kVirtualKeyF = 0x46;

void EmiEngine::pumpEvents() {
	Common::EventManager *eventManager = g_system->getEventManager();
	Common::Event event;

	while (eventManager->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_QUIT:
		case Common::EVENT_RETURN_TO_LAUNCHER:
			if (!(isDemo() && _mode == kModeMovie) && !callExitHook()) {
				eventManager->resetQuit();
				eventManager->resetReturnToLauncher();
			}

			break;
		case Common::EVENT_FOCUS_GAINED:
			handleActivation(true);
			break;
		case Common::EVENT_FOCUS_LOST:
			handleActivation(false);
			break;
		case Common::EVENT_KEYDOWN:
			handleKeyPress(event.kbd, event.kbdRepeat);
			break;
		case Common::EVENT_KEYUP:
			handleKeyRelease(event.kbd);
			break;
		case Common::EVENT_JOYAXIS_MOTION:
			_joystick->setDeviceAxis(event.joystick.axis, event.joystick.position);
			break;
		case Common::EVENT_CUSTOM_BACKEND_ACTION_AXIS: {
			int axis = applyStickAction(_stickAxes, event);

			if (axis >= 0)
				_joystick->setDeviceAxis(axis, _stickAxes[axis]);

			break;
		}
		case Common::EVENT_JOYBUTTON_DOWN:
			_joystick->setDeviceButton(mapPadButton(event.joystick.button), true);
			break;
		case Common::EVENT_JOYBUTTON_UP:
			_joystick->setDeviceButton(mapPadButton(event.joystick.button), false);
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
			_joystick->setDeviceButton((int)event.customType, true);
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_END:
			_joystick->setDeviceButton((int)event.customType, false);
			break;
		default:
			break;
		}
	}
}

int EmiEngine::mapPadButton(int button) {
	if (getGamePlatform() != Common::kPlatformPS2)
		return getJoystickButtonIndex(button);

	switch (button) {
	case Common::JOYSTICK_BUTTON_A:
		return kPadCross;
	case Common::JOYSTICK_BUTTON_B:
		return kPadCircle;
	case Common::JOYSTICK_BUTTON_X:
		return kPadSquare;
	case Common::JOYSTICK_BUTTON_Y:
		return kPadTriangle;
	case Common::JOYSTICK_BUTTON_BACK:
		return kPadSelect;
	case Common::JOYSTICK_BUTTON_START:
		return kPadStart;
	case Common::JOYSTICK_BUTTON_LEFT_STICK:
		return kPadLeftStick;
	case Common::JOYSTICK_BUTTON_RIGHT_STICK:
		return kPadRightStick;
	case Common::JOYSTICK_BUTTON_LEFT_SHOULDER:
		return kPadL1;
	case Common::JOYSTICK_BUTTON_RIGHT_SHOULDER:
		return kPadR1;
	case Common::JOYSTICK_BUTTON_DPAD_UP:
		return kPadDpadUp;
	case Common::JOYSTICK_BUTTON_DPAD_DOWN:
		return kPadDpadDown;
	case Common::JOYSTICK_BUTTON_DPAD_LEFT:
		return kPadDpadLeft;
	case Common::JOYSTICK_BUTTON_DPAD_RIGHT:
		return kPadDpadRight;
	default:
		return -1;
	}
}

void EmiEngine::handleKeyPress(const Common::KeyState &key, bool repeat) {
	int virtualKey = KeyGenerator::keyCodeToVirtualKey(key.keycode);

	if (virtualKey != 0) {
		int keyFlags = 0;

		if (repeat)
			keyFlags |= KeyGenerator::kFlagRepeat;

		if (key.keycode == Common::KEYCODE_KP_ENTER)
			keyFlags |= KeyGenerator::kFlagExtended;

		_keyGenerator->keyDown(virtualKey, keyFlags, (int)g_system->getMillis());
		handleDebugKeys(virtualKey);
	}

	bool altOnly = (key.flags & Common::KBD_ALT) != 0 && (key.flags & Common::KBD_CTRL) == 0;

	if (key.ascii > 0 && key.ascii < 256 && key.ascii != 127 && !altOnly)
		onCharacter((byte)key.ascii);
}

void EmiEngine::handleKeyRelease(const Common::KeyState &key) {
	int virtualKey = KeyGenerator::keyCodeToVirtualKey(key.keycode);

	if (virtualKey == 0)
		return;

	_keyGenerator->keyUp(virtualKey, 0, (int)g_system->getMillis());

	if (virtualKey == kVirtualKeyBackspace)
		_backspaceHeld = false;
	else if (virtualKey == kVirtualKeyControl)
		_controlHeld = false;
}

void EmiEngine::handleDebugKeys(int virtualKey) {
	switch (virtualKey) {
	case kVirtualKeyBackspace:
		_backspaceHeld = true;
		break;
	case kVirtualKeyControl:
		_controlHeld = true;
		break;
	case kVirtualKeyPause:
		// PS2 has no pause handler
		if (_lua && getGamePlatform() != Common::kPlatformPS2)
			_lua->doPauseCallback();

		break;
	case kVirtualKeyF:
		if (_showFps && _backspaceHeld && _controlHeld)
			_lua->toggleStatsOverlay(false);

		break;
	default:
		break;
	}
}

void EmiEngine::handleActivation(bool active) {
	if (active) {
		if (_keyGenerator) {
			_keyGenerator->clearEvents();
			_control->clearKeyStates();
		}

		if (_drawContext)
			_drawContext->restoreDisplay();
	} else if (_keyGenerator) {
		_keyGenerator->clearEvents();
	}
}

bool EmiEngine::onCharacter(byte character) {
	if (!_lua) {
		warning("EmiEngine::onCharacter(): Lua is not started");

		return false;
	}

	// PS2 has no character handler
	if (getGamePlatform() == Common::kPlatformPS2)
		return false;

	bool handled = _lua->doCharacterCallback(character) != 0;

	if (handled && _inputRecorder)
		_inputRecorder->recordChar(character);

	return handled;
}

void EmiEngine::pauseEngineIntern(bool pause) {
	if (pause) {
		_enginePaused = _mode != kModePaused;
		setPause(true);
	} else if (_enginePaused) {
		_enginePaused = 0;
		setPause(false);
	}
}

Common::Array<Common::Keymap *> EmiEngine::initKeymapsEmi(const char *target) {
	Common::Keymap *engineKeyMap = new Common::Keymap(Common::Keymap::kKeymapTypeGame, "monkey4", "Escape from Monkey Island");
	Common::Action *act;

	engineKeyMap->setPartialMatchAllowed(false);

	if (Common::parsePlatform(ConfMan.get("platform", target)) == Common::kPlatformPS2)
		addPadActionsPs2(engineKeyMap);
	else
		addKeyActionsPc(engineKeyMap);

	act = new Common::Action("GMM", _("Global Main Menu"));
	act->setEvent(Common::EVENT_MAINMENU);
	act->addDefaultInputMapping("C+F5");
	act->addDefaultInputMapping("JOY_GUIDE");
	engineKeyMap->addAction(act);

	addStickActions(engineKeyMap);

	return Common::Keymap::arrayOf(engineKeyMap);
}

void EmiEngine::addPadAction(Common::Keymap *keymap, const char *id, const Common::U32String &label, int padButton, const char *input) {
	Common::Action *act = new Common::Action(id, label);

	act->setCustomEngineActionEvent(padButton);
	act->addDefaultInputMapping(input);
	keymap->addAction(act);
}

void EmiEngine::addPadActionsPs2(Common::Keymap *keymap) {
	addPadAction(keymap, "SQUARE", _("Square"), kPadSquare, "JOY_X");
	addPadAction(keymap, "TRIANGLE", _("Triangle"), kPadTriangle, "JOY_Y");
	addPadAction(keymap, "CIRCLE", _("Circle"), kPadCircle, "JOY_B");
	addPadAction(keymap, "CROSS", _("Cross"), kPadCross, "JOY_A");
	addPadAction(keymap, "R1", _("R1"), kPadR1, "JOY_RIGHT_SHOULDER");
	addPadAction(keymap, "L1", _("L1"), kPadL1, "JOY_LEFT_SHOULDER");
	addPadAction(keymap, "R2", _("R2"), kPadR2, "JOY_RIGHT_TRIGGER");
	addPadAction(keymap, "L2", _("L2"), kPadL2, "JOY_LEFT_TRIGGER");
	addPadAction(keymap, "START", _("Start"), kPadStart, "JOY_START");
	addPadAction(keymap, "SELECT", _("Select"), kPadSelect, "JOY_BACK");
	addPadAction(keymap, "DPADUP", _("D-pad up"), kPadDpadUp, "JOY_UP");
	addPadAction(keymap, "DPADDOWN", _("D-pad down"), kPadDpadDown, "JOY_DOWN");
	addPadAction(keymap, "DPADLEFT", _("D-pad left"), kPadDpadLeft, "JOY_LEFT");
	addPadAction(keymap, "DPADRIGHT", _("D-pad right"), kPadDpadRight, "JOY_RIGHT");
	addPadAction(keymap, "L3", _("L3"), kPadLeftStick, "JOY_LEFT_STICK");
	addPadAction(keymap, "R3", _("R3"), kPadRightStick, "JOY_RIGHT_STICK");
}

void EmiEngine::addKeyActionsPc(Common::Keymap *keymap) {
	addKeyAction(keymap, Common::kStandardActionMoveUp, _("Up"), Common::KeyState(Common::KEYCODE_UP), "UP", "JOY_UP");
	addKeyAction(keymap, Common::kStandardActionMoveDown, _("Down"), Common::KeyState(Common::KEYCODE_DOWN), "DOWN", "JOY_DOWN");
	addKeyAction(keymap, Common::kStandardActionMoveLeft, _("Left"), Common::KeyState(Common::KEYCODE_LEFT), "LEFT", "JOY_LEFT");
	addKeyAction(keymap, Common::kStandardActionMoveRight, _("Right"), Common::KeyState(Common::KEYCODE_RIGHT), "RIGHT", "JOY_RIGHT");

	// I18N: Run is a movement type
	addKeyAction(keymap, "BRUN", _("Run"), Common::KeyState(Common::KEYCODE_LSHIFT), "LSHIFT", "JOY_RIGHT_TRIGGER");
	addKeyAction(keymap, "EXAM", _("Examine / Look"), Common::KeyState(Common::KEYCODE_e, 'e'), "e", "JOY_B");
	addKeyAction(keymap, "EXAMALT", _("Examine / Look (alternative)"), Common::KeyState(Common::KEYCODE_l, 'l'), "l", nullptr);
	addKeyAction(keymap, "BUSE", _("Use / Talk"), Common::KeyState(Common::KEYCODE_u, 'u'), "u", "JOY_Y");
	addKeyAction(keymap, "PICK", _("Pick up / Put away"), Common::KeyState(Common::KEYCODE_KP_PLUS, '+'), "KP_PLUS", "JOY_X");
	addKeyAction(keymap, "PICKALT", _("Pick up / Put away (alternative)"), Common::KeyState(Common::KEYCODE_p, 'p'), "p", nullptr);
	addKeyAction(keymap, "INVT", _("Inventory"), Common::KeyState(Common::KEYCODE_INSERT), "INSERT", "JOY_LEFT_TRIGGER");
	addKeyAction(keymap, "INVTALT", _("Inventory (alternative)"), Common::KeyState(Common::KEYCODE_i, 'i'), "i", nullptr);
	addKeyAction(keymap, "SKLI", _("Skip dialog lines"), Common::KeyState(Common::KEYCODE_PERIOD, '.'), "PERIOD", "JOY_LEFT_SHOULDER");
	addKeyAction(keymap, "SKLIALT", _("Skip dialog lines (alternative)"), Common::KeyState(Common::KEYCODE_DELETE), "DELETE", nullptr);
	addKeyAction(keymap, "QEXT", _("Quick room exit"), Common::KeyState(Common::KEYCODE_o, 'o'), "o", "JOY_LEFT_STICK");
	addKeyAction(keymap, "RETURN", _("Confirm"), Common::KeyState(Common::KEYCODE_RETURN, Common::ASCII_RETURN), "RETURN", "JOY_A", "JOY_RIGHT_STICK");
	addKeyAction(keymap, "RETURNKP", _("Confirm (keypad)"), Common::KeyState(Common::KEYCODE_KP_ENTER, Common::ASCII_RETURN), "KP_ENTER", nullptr);

	// I18N: Cycle means rotate through
	addKeyAction(keymap, "COUP", _("Cycle objects up"), Common::KeyState(Common::KEYCODE_PAGEUP), "PAGEUP", "JOY_RIGHT_SHOULDER");

	// I18N: Cycle means rotate through
	addKeyAction(keymap, "CODW", _("Cycle objects down"), Common::KeyState(Common::KEYCODE_PAGEDOWN), "PAGEDOWN", nullptr);

	// I18N: Skipping cutscene playback
	addKeyAction(keymap, Common::kStandardActionSkip, _("Skip"), Common::KeyState(Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE), "ESCAPE", "JOY_BACK");
	addKeyAction(keymap, Common::kStandardActionOpenMainMenu, _("Game menu"), Common::KeyState(Common::KEYCODE_F1, 0), "F1", "JOY_START");
	addKeyAction(keymap, Common::kStandardActionPause, _("Pause"), Common::KeyState(Common::KEYCODE_SPACE, ' '), "SPACE", nullptr);
}

} // End of namespace Grim
