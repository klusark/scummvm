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

#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "graphics/cursorman.h"

#include "engines/grim/grim.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/stdcontrol.h"

namespace Grim {

static const int remappableKeyDefaults[StdControl::kNumRemappedKeys] = {
	0x40000052, 0x40000051, 0x40000050, 0x4000004f, 0x400000e1, 0x65, 0x75, 0x70, 0x69, 0x7f, 0x2e, 8, 0x7a
};

struct StdControlKeyGlyph {
	int sdlKey;
	Common::KeyCode keycode;
	const char *glyphFile;
	const char *glyphText;
	const char *name;
};

static const StdControlKeyGlyph keyGlyphs[] = {
	{ 8, Common::KEYCODE_BACKSPACE, "key_bs.png", nullptr, "Backspace" },
	{ 0x40000039, Common::KEYCODE_CAPSLOCK, "key_caps.png", nullptr, "CapsLock" },
	{ 0x7f, Common::KEYCODE_DELETE, "key_del.png", nullptr, "Delete" },
	{ 0x40000051, Common::KEYCODE_DOWN, "key_down.png", nullptr, "Down" },
	{ 0x4000004d, Common::KEYCODE_END, "key_end.png", nullptr, "End" },
	{ 0x1b, Common::KEYCODE_ESCAPE, "key_esc.png", nullptr, "Escape" },
	{ 0xd, Common::KEYCODE_RETURN, "key_enter.png", nullptr, "Return" },
	{ 0x4000004a, Common::KEYCODE_HOME, "key_home.png", nullptr, "Home" },
	{ 0x40000049, Common::KEYCODE_INSERT, "key_ins.png", nullptr, "Insert" },
	{ 0x400000e2, Common::KEYCODE_LALT, "key_lalt.png", nullptr, "Left Alt" },
	{ 0x400000e0, Common::KEYCODE_LCTRL, "key_lctrl.png", nullptr, "Left Ctrl" },
	{ 0x40000050, Common::KEYCODE_LEFT, "key_left.png", nullptr, "Left" },
	{ 0x400000e1, Common::KEYCODE_LSHIFT, "key_lshift.png", nullptr, "Left Shift" },
	{ 0x40000058, Common::KEYCODE_KP_ENTER, "key_numenter.png", nullptr, "Keypad Enter" },
	{ 0x4000004b, Common::KEYCODE_PAGEUP, "key_pgup.png", nullptr, "PageUp" },
	{ 0x4000004e, Common::KEYCODE_PAGEDOWN, "key_pgdn.png", nullptr, "PageDown" },
	{ 0x400000e6, Common::KEYCODE_RALT, "key_ralt.png", nullptr, "Right Alt" },
	{ 0x400000e4, Common::KEYCODE_RCTRL, "key_rctrl.png", nullptr, "Right Ctrl" },
	{ 0x4000004f, Common::KEYCODE_RIGHT, "key_right.png", nullptr, "Right" },
	{ 0x400000e5, Common::KEYCODE_RSHIFT, "key_rshift.png", nullptr, "Right Shift" },
	{ 0x20, Common::KEYCODE_SPACE, "key_space.png", nullptr, "Space" },
	{ 9, Common::KEYCODE_TAB, "key_tab.png", nullptr, "Tab" },
	{ 0x40000052, Common::KEYCODE_UP, "key_up.png", nullptr, "Up" },
	{ 0x40000062, Common::KEYCODE_KP0, "key_numpad.png", "0", "Keypad 0" },
	{ 0x40000059, Common::KEYCODE_KP1, "key_numpad.png", "1", "Keypad 1" },
	{ 0x4000005a, Common::KEYCODE_KP2, "key_numpad.png", "2", "Keypad 2" },
	{ 0x4000005b, Common::KEYCODE_KP3, "key_numpad.png", "3", "Keypad 3" },
	{ 0x4000005c, Common::KEYCODE_KP4, "key_numpad.png", "4", "Keypad 4" },
	{ 0x4000005d, Common::KEYCODE_KP5, "key_numpad.png", "5", "Keypad 5" },
	{ 0x4000005e, Common::KEYCODE_KP6, "key_numpad.png", "6", "Keypad 6" },
	{ 0x4000005f, Common::KEYCODE_KP7, "key_numpad.png", "7", "Keypad 7" },
	{ 0x40000060, Common::KEYCODE_KP8, "key_numpad.png", "8", "Keypad 8" },
	{ 0x40000061, Common::KEYCODE_KP9, "key_numpad.png", "9", "Keypad 9" },
	{ 0x40000057, Common::KEYCODE_KP_PLUS, "key_numpad.png", "+", "Keypad +" },
	{ 0x40000056, Common::KEYCODE_KP_MINUS, "key_numpad.png", "=", "Keypad -" },
	{ 0x40000054, Common::KEYCODE_KP_DIVIDE, "key_numpad.png", "/", "Keypad /" },
	{ 0x40000055, Common::KEYCODE_KP_MULTIPLY, "key_numpad.png", "*", "Keypad *" },
	{ 0x40000063, Common::KEYCODE_KP_PERIOD, "key_numpad.png", ".", "Keypad ." }
};

enum {
	kSdlKeyEscape = 0x1b,
	kSdlKeyReturn = 0xd,
	kSdlKeyPrintScreen = 0x40000046,
	kSdlKeyUp = 0x40000052,
	kSdlKeyDown = 0x40000051,
	kSdlKeyLeft = 0x40000050,
	kSdlKeyRight = 0x4000004f,
	kSdlKeyFirstPrintable = 33,
	kSdlKeyLastPrintable = 126
};

StdControl::StdControl(bool remastered) : _remastered(remastered), _keyMappingActive(false), _mappingSlot(-1), _startup(0), _open(0),
		_controlsActive(0), _controlsIdle(0), _disableKeyboard(0), _readMouse(1), _readJoysticks(0), _curReadTime(0), _lastReadTime(0),
		_readDelta(0), _deltaTime(1.0f), _deltaTimeMs(1.0f), _prevMouseX(0), _prevMouseY(0) {
	for (int i = 0; i < kNumRemappedKeys; i++)
		_remappedKeys[i] = remappableKeyDefaults[i];

	memset(_abReadJoy, 0, sizeof(_abReadJoy));
	memset(_readJoyFlags, 0, sizeof(_readJoyFlags));
	memset(_joyPresent, 0, sizeof(_joyPresent));
	memset(_joyMaxButtons, 0, sizeof(_joyMaxButtons));
	memset(_joyHasPOV, 0, sizeof(_joyHasPOV));
	memset(_axes, 0, sizeof(_axes));
	memset(_axisPos, 0, sizeof(_axisPos));
	memset(_keyInfo, 0, sizeof(_keyInfo));
	memset(&_input, 0, sizeof(_input));
}

int StdControl::startup() {
	memset(_axes, 0, sizeof(_axes));
	memset(_axisPos, 0, sizeof(_axisPos));

	if (_remastered)
		resetKeyMappingToDefault();

	initJoysticks();
	initMouse();
	reset();

	if (_remastered)
		CursorMan.showMouse(false);

	_startup = 1;

	return 1;
}

void StdControl::shutdown() {
	_startup = 0;
}

int StdControl::open() {
	_open = 1;
	setActivation(1);

	return 1;
}

int StdControl::close() {
	if (!_open)
		return 0;

	setActivation(0);
	_open = 0;

	return 1;
}

void StdControl::flush() {
	_curReadTime = g_system->getMillis();

	flushKeyboard();
	flushMouse();
}

void StdControl::reset() {
	_readMouse = 0;
	_readJoysticks = 0;
	_abReadJoy[0] = 0;
	_readJoyFlags[0] = 1664;
	_abReadJoy[1] = 0;
	_readJoyFlags[1] = 1664;

	for (int i = 0; i < kNumAxes; i++)
		_axes[i].flags &= ~2;
}

int StdControl::toggleMouse() {
	_readMouse = _readMouse == 0;

	return _readMouse;
}

int StdControl::pressButton(uint button) {
	if (button >= kNumAxes)
		return 0;

	if ((_axes[button].flags & 1) == 0)
		return 0;

	_axes[button].flags |= 2;
	setButtonPressed(button);

	return 1;
}

void StdControl::readControls() {
	if (!_controlsActive)
		return;

	_controlsIdle = 1;
	_curReadTime = g_system->getMillis();

	if (_curReadTime != _lastReadTime) {
		uint delta = (uint)_curReadTime - (uint)_lastReadTime;

		_readDelta = delta;

		double invDelta = 1.0 / (double)delta;

		memset(_axisPos, 0, sizeof(_axisPos));
		_deltaTime = (float)invDelta;
		_deltaTimeMs = (float)(invDelta * 1000.0);

		readKeyboard();

		if (_readJoysticks)
			readJoysticks();

		readMouse();

		_lastReadTime = _curReadTime;
	}
}

float StdControl::readAxis(int axis) {
	if (!_controlsActive)
		return 0.0f;

	if (axis < 0 || axis >= kNumAxes) {
		warning("StdControl::readAxis(): axis %d out of range", axis);
		return 0.0f;
	}

	const StdControlAxis *axisPointer = &_axes[axis];

	if ((axisPointer->flags & 2) == 0)
		return 0.0f;

	int pos = _axisPos[axis] - axisPointer->center;

	if (pos == 0)
		return 0.0f;

	if ((axisPointer->flags & 8) == 0 && axisPointer->deadZone) {
		int magnitude = pos;

		if (magnitude < 0)
			magnitude = -magnitude;

		if (magnitude < axisPointer->deadZone)
			return 0.0f;
	}

	double result = (double)axisPointer->invRange * (double)pos;

	if (fabs(result) <= (double)0.00001f)
		result = 0.0;

	if (_controlsIdle && result != 0.0)
		_controlsIdle = 0;

	return (float)result;
}

int StdControl::readKey(int key, int *pressCount, int *doubleClick, int *longPress) {
	if (!_controlsActive || _disableKeyboard || key < 0 || key >= kNumKeys) {
		if (key < 0 || key >= kNumKeys)
			warning("StdControl::readKey(): key %d out of range", key);

		if (pressCount)
			*pressCount = 0;

		if (doubleClick)
			*doubleClick = 0;

		if (longPress)
			*longPress = 0;

		return 0;
	}

	StdControlKey *keyPointer = &_keyInfo[key];

	if (_controlsIdle && keyPointer->state)
		_controlsIdle = 0;

	if (pressCount)
		*pressCount = keyPointer->pressCount;

	if (doubleClick)
		*doubleClick = keyPointer->doubleClick;

	if (longPress) {
		*longPress = 0;

		if (keyPointer->longFlag == 0) {
			*longPress = (uint)(keyPointer->frameTime - keyPointer->changeTime) > 400;

			if (*longPress)
				keyPointer->longFlag = 1;
		}
	}

	return keyPointer->state;
}

void StdControl::setActivation(int active) {
	if (!_open)
		return;

	if (active) {
		if (_readMouse)
			CursorMan.showMouse(false);

		_controlsActive = 1;
	} else {
		if (!_remastered)
			CursorMan.showMouse(true);

		_controlsActive = 0;
	}
}

void StdControl::initJoysticks() {
	for (int i = 0; i < 2; i++) {
		_joyPresent[i] = 0;
		_joyMaxButtons[i] = 0;
		_joyHasPOV[i] = 0;
	}

	int numDevs = _input.numJoysticks;

	if (numDevs >= 2)
		numDevs = 2;

	for (int joy = 0; joy < numDevs; joy++) {
		const StdControlJoyCaps *caps = &_input.joyCaps[joy];
		int axisBase = 6 * joy;

		if (!caps->available)
			continue;

		_joyPresent[joy] = 1;

		int maxButtons = caps->maxButtons;

		if (maxButtons >= 14)
			maxButtons = 14;

		_joyMaxButtons[joy] = maxButtons;

		float deadZone = _remastered ? 0.2f : 0.1f;

		enableAxis(axisBase, caps->axisMin[0], caps->axisMax[0], deadZone);
		enableAxis(axisBase + 1, caps->axisMin[1], caps->axisMax[1], deadZone);

		if (caps->flags & 1)
			enableAxis(axisBase + 2, caps->axisMin[2], caps->axisMax[2], deadZone);

		if (caps->flags & 2)
			enableAxis(axisBase + 3, caps->axisMin[3], caps->axisMax[3], deadZone);

		if (caps->flags & 4)
			enableAxis(axisBase + 4, caps->axisMin[4], caps->axisMax[4], deadZone);

		if (caps->flags & 8)
			enableAxis(axisBase + 5, caps->axisMin[5], caps->axisMax[5], deadZone);

		if (caps->flags & 0x10)
			_joyHasPOV[joy] = 1;
	}
}

void StdControl::initMouse() {
	if (_remastered) {
		enableAxis(12, 0, 640, 0.0f);
		enableAxis(13, 0, 480, 0.0f);

		return;
	}

	enableAxis(12, -250, 250, 0.0f);
	enableAxis(13, -200, 200, 0.0f);
	enableAxis(14, -20, 20, 0.0f);
}

void StdControl::setButtonPressed(uint button) {
	if (button >= 12 && button <= 14) {
		_readMouse = 1;
	} else if (button <= 11) {
		_readJoysticks = 1;

		int joy = button > 5 ? 1 : 0;

		_abReadJoy[joy] = 1;

		switch (button - 6 * joy) {
		case 0:
			_readJoyFlags[joy] |= 1;
			break;
		case 1:
			_readJoyFlags[joy] |= 2;
			break;
		case 2:
			_readJoyFlags[joy] |= 4;
			break;
		case 3:
			_readJoyFlags[joy] |= 8;
			break;
		case 4:
			_readJoyFlags[joy] |= 0x10;
			break;
		case 5:
			_readJoyFlags[joy] |= 0x20;
			break;
		default:
			break;
		}
	}
}

void StdControl::readKeyboard() {
	if (_disableKeyboard)
		return;

	if (_remastered)
		applyKeyboardEvents();

	int now = _curReadTime;

	for (int i = 0; i < kNumKeys; i++) {
		StdControlKey *key = &_keyInfo[i];

		key->frameTime = now;
		key->state = key->rawState;
		key->pressCount = 0;
		key->doubleClick = 0;
	}

	if (!_remastered)
		applyKeyboardEvents();
}

void StdControl::applyKeyboardEvents() {
	int numEvents = _input.numKeyboardEvents;

	if (numEvents > 32) {
		warning("StdControl::applyKeyboardEvents(): too many keyboard events");
		numEvents = 32;
	}

	for (int i = 0; i < numEvents; i++) {
		const StdControlDeviceEvent *event = &_input.keyboardEvents[i];

		if (event->offset >= 256) {
			warning("StdControl::applyKeyboardEvents(): key %u out of range", event->offset);
			continue;
		}

		updateKey(&_keyInfo[event->offset], event->data, event->timeStamp, false);
	}

	int overflow = _input.keyboardOverflow;

	_input.numKeyboardEvents = 0;
	_input.keyboardOverflow = 0;

	if (overflow) {
		int now = _curReadTime;

		for (int i = 0; i < 256; i++)
			updateKey(&_keyInfo[i], _input.keyboardState[i], now, false);
	}
}

void StdControl::flushKeyboard() {
	_input.numKeyboardEvents = 0;
	_input.keyboardOverflow = 0;

	int now = _curReadTime;

	for (int i = 0; i < 256; i++) {
		StdControlKey *key = &_keyInfo[i];

		if (key->rawState) {
			key->rawState = 0;
			key->changeTime = now;
		}
	}
}

void StdControl::readJoysticks() {
	int buttonMask = 1;

	for (int joy = 0; joy < 2; joy++) {
		int keyBase = 256 + 14 * joy;

		if (!_abReadJoy[joy] || !_joyPresent[joy])
			continue;

		const StdControlJoyState *state = &_input.joyState[joy];

		if (state->result)
			break;

		int *axisPos = &_axisPos[6 * joy];
		int gotFlags = state->flags;

		axisPos[0] = state->axisPos[0];
		axisPos[1] = state->axisPos[1];

		if (gotFlags & 4)
			axisPos[2] = state->axisPos[2];

		if (gotFlags & 8)
			axisPos[3] = state->axisPos[3];

		if (gotFlags & 0x10)
			axisPos[4] = state->axisPos[4];

		if (gotFlags & 0x20)
			axisPos[5] = state->axisPos[5];

		int now = _curReadTime;
		int numButtons = _remastered ? 12 : 10;

		for (int button = 0; button < numButtons; button++) {
			updateKey(&_keyInfo[keyBase + button], buttonMask & state->buttons, now, false);
			buttonMask *= 2;
		}

		if (!_remastered && (state->flags & 0x200)) {
			int pov = state->pov;
			int povA = pov >= 22500 && pov <= 31500;
			int povB = (pov >= 31500 || pov <= 4500) && pov >= 0 && pov != 0xFFFF;
			int povC = pov >= 4500 && pov <= 13500;
			int povD = pov >= 13500 && pov <= 22500;

			updateKey(&_keyInfo[keyBase + 10], povA, now, false);
			updateKey(&_keyInfo[keyBase + 11], povB, now, false);
			updateKey(&_keyInfo[keyBase + 12], povC, now, false);
			updateKey(&_keyInfo[keyBase + 13], povD, now, false);
		}
	}
}

void StdControl::readMouse() {
	if (!_readMouse)
		return;

	if (_remastered) {
		Common::Point mouse = g_system->getEventManager()->getMousePos();

		_axisPos[14] = 0;
		_axisPos[12] = 640 * (mouse.x - kCanvasPlayOffset) / kCanvasPlayWidth;
		_axisPos[13] = 480 * mouse.y / kCanvasHeight;
	} else {
		_axisPos[14] = _input.mouse.z;
		_axisPos[12] = _input.mouse.x;
		_axisPos[13] = _input.mouse.y;

		if ((uint)_readDelta < 0x19) {
			_axisPos[12] = (_input.mouse.x + _prevMouseX) >> 1;
			_axisPos[13] = (_input.mouse.y + _prevMouseY) >> 1;
		}
	}

	_prevMouseX = _input.mouse.x;
	_prevMouseY = _input.mouse.y;
	_input.mouse.x = 0;
	_input.mouse.y = 0;
	_input.mouse.z = 0;

	if (_remastered) {
		int now = _curReadTime;

		_input.numMouseEvents = 0;
		_input.mouseOverflow = 0;

		for (int i = 0; i < 4; i++)
			updateKey(&_keyInfo[284 + i], _input.mouse.buttons[i], now, true);

		updateKey(&_keyInfo[kKeyMouseLong], _input.mouse.buttons[0], now, true);

		return;
	}

	int numEvents = _input.numMouseEvents;

	if (numEvents > 32) {
		warning("StdControl::readMouse(): too many mouse events");
		numEvents = 32;
	}

	for (int i = 0; i < numEvents; i++) {
		const StdControlDeviceEvent *event = &_input.mouseEvents[i];

		if (event->offset < 12 || event->offset > 15)
			continue;

		updateKey(&_keyInfo[event->offset + 272], event->data, event->timeStamp, false);
	}

	int overflow = _input.mouseOverflow;

	_input.numMouseEvents = 0;
	_input.mouseOverflow = 0;

	if (overflow) {
		int now = _curReadTime;

		for (int i = 0; i < 4; i++)
			updateKey(&_keyInfo[284 + i], _input.mouse.buttons[i], now, false);
	}
}

void StdControl::flushMouse() {
	if (!_readMouse)
		return;

	_input.mouse.x = 0;
	_input.mouse.y = 0;
	_input.mouse.z = 0;
	_input.numMouseEvents = 0;
	_input.mouseOverflow = 0;

	int now = _curReadTime;

	for (int i = 0; i < 4; i++)
		updateKey(&_keyInfo[284 + i], _input.mouse.buttons[i], now, _remastered);

	if (_remastered)
		updateKey(&_keyInfo[kKeyMouseLong], _input.mouse.buttons[0], now, true);
}

void StdControl::getMouseCanvasPosition(bool menuMode, int *x, int *y) const {
	Common::Point mouse = g_system->getEventManager()->getMousePos();
	HdCanvas *canvas = g_grim->getHdCanvas();

	if (!canvas) {
		warning("StdControl::getMouseCanvasPosition(): there is no canvas");
		*x = 0;
		*y = 0;
		return;
	}

	float scaleX;
	float scaleY;
	float offsetX;
	float offsetY;

	if (menuMode) {
		HdCanvas::computeScaling(false, kCanvasWidth, kCanvasHeight, &scaleX, &scaleY, &offsetX, &offsetY);
	} else {
		HdCanvas::computeScaling(canvas->isStretched(), kCanvasPlayWidth, kCanvasHeight, &scaleX, &scaleY, &offsetX, &offsetY);

		offsetX = (float)((double)offsetX - (double)scaleX * (double)kCanvasPlayOffset);
	}

	*x = (int)(((double)mouse.x - (double)offsetX) / (double)scaleX);
	*y = (int)(((double)mouse.y - (double)offsetY) / (double)scaleY);
}

void StdControl::enableAxis(int axis, int minVal, int maxVal, float deadZone) {
	int center = minVal + (maxVal - minVal + 1) / 2;
	StdControlAxis *axisPointer = &_axes[axis];

	axisPointer->flags |= 1;
	axisPointer->minVal = minVal;
	axisPointer->maxVal = maxVal;
	axisPointer->center = center;

	double halfRange = (double)(maxVal - center);

	axisPointer->invRange = (float)(1.0 / halfRange);

	if (deadZone == 0.0f)
		axisPointer->deadZone = 0;
	else
		axisPointer->deadZone = (int)((double)deadZone * halfRange);
}

void StdControl::updateKey(StdControlKey *key, int down, int time, bool canLongPress) {
	if (down != key->rawState) {
		if (down && (uint)time - (uint)key->lastPressTime < 200)
			key->doubleClick = 1;

		key->rawState = down;
		key->changeTime = time;

		if (!down)
			return;

		key->pressCount++;
		key->state = down;
		key->longFlag = canLongPress ? 0 : 1;
	} else if (!down) {
		return;
	}

	key->lastPressTime = time;
}

void StdControl::setKeyMappingMode(int slot) {
	_mappingSlot = slot;
	_keyMappingActive = true;
}

void StdControl::resetKeyMappingToDefault() {
	for (int i = 0; i < kNumRemappedKeys; i++)
		_remappedKeys[i] = remappableKeyDefaults[i];
}

int StdControl::getRemappedKey(int slot) const {
	if (slot < 0 || slot >= kNumRemappedKeys) {
		warning("StdControl::getRemappedKey(): slot %d out of range", slot);
		return 0;
	}

	return _remappedKeys[slot];
}

void StdControl::setRemappedKey(int slot, int sdlKey) {
	if (slot < 0 || slot >= kNumRemappedKeys) {
		warning("StdControl::setRemappedKey(): slot %d out of range", slot);
		return;
	}

	_remappedKeys[slot] = sdlKey;
}

Common::KeyCode StdControl::translateKey(Common::KeyCode keycode) const {
	if (_keyMappingActive)
		return keycode;

	int sdlKey = getSdlKey(keycode);

	if (!sdlKey)
		return keycode;

	for (int i = 0; i < kNumRemappedKeys; i++) {
		if (_remappedKeys[i] == sdlKey) {
			Common::KeyCode defaultKeycode = getKeyCode(remappableKeyDefaults[i]);

			return defaultKeycode == Common::KEYCODE_INVALID ? keycode : defaultKeycode;
		}
	}

	return keycode;
}

int StdControl::endKeyMapping(Common::KeyCode keycode) {
	int sdlKey = getSdlKey(keycode);

	_keyMappingActive = false;

	int result = validateMappedKey(_mappingSlot, sdlKey);

	if (result == 0)
		_remappedKeys[_mappingSlot] = sdlKey;

	return result;
}

int StdControl::validateMappedKey(int slot, int sdlKey) const {
	if (slot < 0 || slot >= kNumRemappedKeys)
		return 2;

	if (sdlKey == kSdlKeyEscape)
		return 4;

	if ((isReservedKey(sdlKey) && sdlKey != remappableKeyDefaults[slot]) || !isNameableKey(sdlKey))
		return 1;

	for (int i = 0; i < kNumRemappedKeys; i++) {
		if (i != slot && _remappedKeys[i] == sdlKey)
			return 3;
	}

	return 0;
}

bool StdControl::isReservedKey(int sdlKey) {
	switch (sdlKey) {
	case kSdlKeyEscape:
	case kSdlKeyUp:
	case kSdlKeyDown:
	case kSdlKeyLeft:
	case kSdlKeyRight:
	case kSdlKeyReturn:
	case kSdlKeyPrintScreen:
	case '-':
	case '=':
		return true;
	default:
		return sdlKey >= '0' && sdlKey <= '9';
	}
}

bool StdControl::isNameableKey(int sdlKey) {
	if (sdlKey >= kSdlKeyFirstPrintable && sdlKey <= kSdlKeyLastPrintable)
		return true;

	return getKeyGlyph(sdlKey, nullptr, nullptr);
}

int StdControl::getSdlKey(Common::KeyCode keycode) {
	for (uint i = 0; i < ARRAYSIZE(keyGlyphs); i++) {
		if (keyGlyphs[i].keycode == keycode)
			return keyGlyphs[i].sdlKey;
	}

	if (keycode == Common::KEYCODE_PRINT)
		return kSdlKeyPrintScreen;

	if ((int)keycode >= kSdlKeyFirstPrintable && (int)keycode <= kSdlKeyLastPrintable)
		return (int)keycode;

	return 0;
}

Common::KeyCode StdControl::getKeyCode(int sdlKey) {
	for (uint i = 0; i < ARRAYSIZE(keyGlyphs); i++) {
		if (keyGlyphs[i].sdlKey == sdlKey)
			return keyGlyphs[i].keycode;
	}

	if (sdlKey == kSdlKeyPrintScreen)
		return Common::KEYCODE_PRINT;

	if (sdlKey >= kSdlKeyFirstPrintable && sdlKey <= kSdlKeyLastPrintable)
		return (Common::KeyCode)sdlKey;

	return Common::KEYCODE_INVALID;
}

Common::String StdControl::getSdlKeyName(int sdlKey) {
	for (uint i = 0; i < ARRAYSIZE(keyGlyphs); i++) {
		if (keyGlyphs[i].sdlKey == sdlKey)
			return keyGlyphs[i].name;
	}

	if (sdlKey == kSdlKeyPrintScreen)
		return "PrintScreen";

	if (sdlKey >= kSdlKeyFirstPrintable && sdlKey <= kSdlKeyLastPrintable) {
		int character = sdlKey;

		if (character >= 'a' && character <= 'z')
			character -= 'a' - 'A';

		return Common::String::format("%c", character);
	}

	return Common::String();
}

bool StdControl::getKeyGlyph(int sdlKey, const char **glyphFile, const char **glyphText) {
	for (uint i = 0; i < ARRAYSIZE(keyGlyphs); i++) {
		if (keyGlyphs[i].sdlKey == sdlKey) {
			if (glyphFile)
				*glyphFile = keyGlyphs[i].glyphFile;

			if (glyphText)
				*glyphText = keyGlyphs[i].glyphText;

			return true;
		}
	}

	return false;
}

} // End of namespace Grim
