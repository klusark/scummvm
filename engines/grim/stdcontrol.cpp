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

#include "engines/grim/stdcontrol.h"

namespace Grim {

StdControl::StdControl(bool remastered) : _remastered(remastered), _startup(0), _open(0), _controlsActive(0), _controlsIdle(0), _disableKeyboard(0),
		_readMouse(1), _readJoysticks(0), _curReadTime(0), _lastReadTime(0), _readDelta(0), _deltaTime(1.0f),
		_deltaTimeMs(1.0f), _prevMouseX(0), _prevMouseY(0) {
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

	initJoysticks();
	initMouse();
	reset();

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

int StdControl::readKey(int key, int *pressCount, int *doubleClick) {
	if (!_controlsActive || _disableKeyboard || key < 0 || key >= kNumKeys) {
		if (key < 0 || key >= kNumKeys)
			warning("StdControl::readKey(): key %d out of range", key);

		if (pressCount)
			*pressCount = 0;

		if (doubleClick)
			*doubleClick = 0;

		return 0;
	}

	const StdControlKey *keyPointer = &_keyInfo[key];

	if (_controlsIdle && keyPointer->state)
		_controlsIdle = 0;

	if (pressCount)
		*pressCount = keyPointer->pressCount;

	if (doubleClick)
		*doubleClick = keyPointer->doubleClick;

	return keyPointer->state;
}

void StdControl::setActivation(int active) {
	if (!_open)
		return;

	if (active) {
		// The remastered scripts draw their own cursor
		if (_readMouse || _remastered)
			CursorMan.showMouse(false);

		_controlsActive = 1;
	} else {
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

		enableAxis(axisBase, caps->axisMin[0], caps->axisMax[0], 0.1f);
		enableAxis(axisBase + 1, caps->axisMin[1], caps->axisMax[1], 0.1f);

		if (caps->flags & 1)
			enableAxis(axisBase + 2, caps->axisMin[2], caps->axisMax[2], 0.1f);

		if (caps->flags & 2)
			enableAxis(axisBase + 3, caps->axisMin[3], caps->axisMax[3], 0.1f);

		if (caps->flags & 4)
			enableAxis(axisBase + 4, caps->axisMin[4], caps->axisMax[4], 0.1f);

		if (caps->flags & 8)
			enableAxis(axisBase + 5, caps->axisMin[5], caps->axisMax[5], 0.1f);

		if (caps->flags & 0x10)
			_joyHasPOV[joy] = 1;
	}
}

void StdControl::initMouse() {
	if (_remastered) {
		// Absolute position over the 640x480 play area
		enableAxis(12, 0, 640, 0.0f);
		enableAxis(13, 0, 480, 0.0f);
	} else {
		enableAxis(12, -250, 250, 0.0f);
		enableAxis(13, -200, 200, 0.0f);
	}

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

	int now = _curReadTime;

	for (int i = 0; i < kNumKeys; i++) {
		StdControlKey *key = &_keyInfo[i];

		key->frameTime = now;
		key->state = key->rawState;
		key->pressCount = 0;
		key->doubleClick = 0;
	}

	int numEvents = _input.numKeyboardEvents;

	if (numEvents > 32) {
		warning("StdControl::readKeyboard(): too many keyboard events");
		numEvents = 32;
	}

	for (int i = 0; i < numEvents; i++) {
		const StdControlDeviceEvent *event = &_input.keyboardEvents[i];

		if (event->offset >= 256) {
			warning("StdControl::readKeyboard(): key %u out of range", event->offset);
			continue;
		}

		updateKey(&_keyInfo[event->offset], event->data, event->timeStamp);
	}

	int overflow = _input.keyboardOverflow;

	_input.numKeyboardEvents = 0;
	_input.keyboardOverflow = 0;

	if (overflow) {
		now = _curReadTime;

		for (int i = 0; i < 256; i++)
			updateKey(&_keyInfo[i], _input.keyboardState[i], now);
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
			updateKey(&_keyInfo[keyBase + button], buttonMask & state->buttons, now);
			buttonMask *= 2;
		}

		// The remastered has no hat codes, buttons 11 and 12 use them
		if (!_remastered && (state->flags & 0x200)) {
			int pov = state->pov;
			int povA = pov >= 22500 && pov <= 31500;
			int povB = (pov >= 31500 || pov <= 4500) && pov >= 0 && pov != 0xFFFF;
			int povC = pov >= 4500 && pov <= 13500;
			int povD = pov >= 13500 && pov <= 22500;

			updateKey(&_keyInfo[keyBase + 10], povA, now);
			updateKey(&_keyInfo[keyBase + 11], povB, now);
			updateKey(&_keyInfo[keyBase + 12], povC, now);
			updateKey(&_keyInfo[keyBase + 13], povD, now);
		}
	}
}

void StdControl::readMouse() {
	if (!_readMouse)
		return;

	if (_remastered) {
		int canvasX;
		int canvasY;

		getMouseCanvasPosition(false, &canvasX, &canvasY);

		_axisPos[14] = 0;
		_axisPos[12] = 640 * (canvasX - kCanvasPlayOffset) / kCanvasPlayWidth;
		_axisPos[13] = 480 * canvasY / kCanvasHeight;
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

	int numEvents = _input.numMouseEvents;

	if (numEvents > 32) {
		warning("StdControl::readMouse(): too many mouse events");
		numEvents = 32;
	}

	for (int i = 0; i < numEvents; i++) {
		const StdControlDeviceEvent *event = &_input.mouseEvents[i];

		if (event->offset < 12 || event->offset > 15)
			continue;

		updateKey(&_keyInfo[event->offset + 272], event->data, event->timeStamp);

		// The long tap key follows the first button
		if (_remastered && event->offset == 12)
			updateKey(&_keyInfo[kKeyMouseLong], event->data, event->timeStamp);
	}

	int overflow = _input.mouseOverflow;

	_input.numMouseEvents = 0;
	_input.mouseOverflow = 0;

	if (overflow) {
		int now = _curReadTime;

		for (int i = 0; i < 4; i++)
			updateKey(&_keyInfo[284 + i], _input.mouse.buttons[i], now);

		if (_remastered)
			updateKey(&_keyInfo[kKeyMouseLong], _input.mouse.buttons[0], now);
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
		updateKey(&_keyInfo[284 + i], _input.mouse.buttons[i], now);

	if (_remastered)
		updateKey(&_keyInfo[kKeyMouseLong], _input.mouse.buttons[0], now);
}

void StdControl::getMouseCanvasPosition(bool menuMode, int *x, int *y) const {
	Common::Point mouse = g_system->getEventManager()->getMousePos();
	float width = (float)g_system->getWidth();
	float height = (float)g_system->getHeight();
	float refWidth = menuMode ? (float)kCanvasWidth : (float)kCanvasPlayWidth;
	float scale = MIN(width / refWidth, height / (float)kCanvasHeight);

	if (scale <= 0.0f) {
		warning("StdControl::getMouseCanvasPosition(): the screen has no size");
		*x = 0;
		*y = 0;
		return;
	}

	float offsetX = (width - refWidth * scale) / 2.0f;
	float offsetY = (height - (float)kCanvasHeight * scale) / 2.0f;

	// The 4:3 play area sits in the middle of the canvas
	if (!menuMode)
		offsetX -= (float)kCanvasPlayOffset * scale;

	*x = (int)(((float)mouse.x - offsetX) / scale);
	*y = (int)(((float)mouse.y - offsetY) / scale);
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

void StdControl::updateKey(StdControlKey *key, int down, int time) {
	if (down != key->rawState) {
		if (down && (uint)time - (uint)key->lastPressTime < 200)
			key->doubleClick = 1;

		key->rawState = down;
		key->changeTime = time;

		if (!down)
			return;

		key->pressCount++;
		key->state = down;
	} else if (!down) {
		return;
	}

	key->lastPressTime = time;
}

} // End of namespace Grim
