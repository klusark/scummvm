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

#ifndef GRIM_STDCONTROL_H
#define GRIM_STDCONTROL_H

#include "common/keyboard.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Grim {

struct StdControlAxis {
	int flags;
	int minVal;
	int maxVal;
	int center;
	int deadZone;
	float invRange;
};

struct StdControlKey {
	int state;
	int frameTime;
	int rawState;
	int changeTime;
	int lastPressTime;
	int pressCount;
	int doubleClick;
	int longFlag;
};

struct StdControlDeviceEvent {
	uint offset;
	int data;
	int timeStamp;
};

struct StdControlMouseState {
	int x;
	int y;
	int z;
	byte buttons[4];
};

struct StdControlJoyCaps {
	int available;
	int flags;
	int maxButtons;
	int axisMin[6];
	int axisMax[6];
};

struct StdControlJoyState {
	int result;
	int flags;
	int axisPos[6];
	int buttons;
	int pov;
};

struct StdControlInput {
	StdControlDeviceEvent keyboardEvents[32];
	int numKeyboardEvents;
	int keyboardOverflow;
	byte keyboardState[256];

	StdControlMouseState mouse;
	StdControlDeviceEvent mouseEvents[32];
	int numMouseEvents;
	int mouseOverflow;

	int numJoysticks;
	StdControlJoyCaps joyCaps[2];
	StdControlJoyState joyState[2];
};

class StdControl {
public:
	enum {
		kNumAxes = 15,
		kNumKeys = 290,
		kKeyMouseLong = 288,
		kKeyMousePing = 289
	};

	// Size of the canvas the remastered scripts use for the cursor position
	enum {
		kCanvasWidth = 1920,
		kCanvasHeight = 1080,
		kCanvasPlayWidth = 1440,
		kCanvasPlayOffset = 240
	};

	// Number of keys of the remastered that can be remapped
	enum {
		kNumRemappedKeys = 13
	};

	StdControl(bool remastered);

	StdControlInput *getInput() { return &_input; }
	int isStarted() const { return _startup; }
	int isOpen() const { return _open; }

	// Cursor position on the 1920x1080 canvas, menuMode is true while the library mode is 5
	void getMouseCanvasPosition(bool menuMode, int *x, int *y) const;

	int startup();
	void shutdown();
	int open();
	int close();
	void flush();
	void reset();
	int pressButton(uint button);
	void readControls();
	float readAxis(int axis);
	int readKey(int key, int *pressCount, int *doubleClick, int *longPress = nullptr);
	void setActivation(int active);

	// Key remapping of the remastered. The keys are SDL key codes, the ones the registry stores.
	void setKeyMappingMode(int slot);
	void resetKeyMappingToDefault();
	int getRemappedKey(int slot) const;
	void setRemappedKey(int slot, int sdlKey);
	bool isKeyMappingActive() const { return _keyMappingActive; }
	Common::KeyCode translateKey(Common::KeyCode keycode) const;
	int endKeyMapping(Common::KeyCode keycode);
	static int getSdlKey(Common::KeyCode keycode);
	static Common::KeyCode getKeyCode(int sdlKey);
	static Common::String getSdlKeyName(int sdlKey);
	static bool getKeyGlyph(int sdlKey, const char **glyphFile, const char **glyphText);

private:
	static bool isReservedKey(int sdlKey);
	static bool isNameableKey(int sdlKey);
	int validateMappedKey(int slot, int sdlKey) const;

	void initJoysticks();
	void initMouse();
	void setButtonPressed(uint button);
	void readKeyboard();
	void applyKeyboardEvents();
	void flushKeyboard();
	void readJoysticks();
	void readMouse();
	void flushMouse();
	void enableAxis(int axis, int minVal, int maxVal, float deadZone);
	void updateKey(StdControlKey *key, int down, int time, bool canLongPress);

	bool _remastered;
	bool _keyMappingActive;
	int _mappingSlot;
	int _remappedKeys[kNumRemappedKeys];
	int _startup;
	int _open;
	int _controlsActive;
	int _controlsIdle;
	int _disableKeyboard;
	int _readMouse;
	int _readJoysticks;
	int _abReadJoy[2];
	int _readJoyFlags[2];
	int _joyPresent[2];
	int _joyMaxButtons[2];
	int _joyHasPOV[2];
	int _curReadTime;
	int _lastReadTime;
	int _readDelta;
	float _deltaTime;
	float _deltaTimeMs;
	int _prevMouseX;
	int _prevMouseY;

	StdControlAxis _axes[kNumAxes];
	int _axisPos[kNumAxes];
	StdControlKey _keyInfo[kNumKeys];

	StdControlInput _input;
};

} // End of namespace Grim

#endif
