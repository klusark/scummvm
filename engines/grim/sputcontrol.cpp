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

#include "common/debug.h"
#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/sputcontrol.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/stdcontrol.h"

namespace Grim {

static const ControlSymbol controlSymbolTable[kNumControlSymbols] = {
	{ 4, 1, 0, "KEY_ESCAPE" },
	{ 4, 2, 0, "KEY_1" },
	{ 4, 3, 0, "KEY_2" },
	{ 4, 4, 0, "KEY_3" },
	{ 4, 5, 0, "KEY_4" },
	{ 4, 6, 0, "KEY_5" },
	{ 4, 7, 0, "KEY_6" },
	{ 4, 8, 0, "KEY_7" },
	{ 4, 9, 0, "KEY_8" },
	{ 4, 10, 0, "KEY_9" },
	{ 4, 11, 0, "KEY_0" },
	{ 4, 12, 0, "KEY_MINUS" },
	{ 4, 13, 0, "KEY_EQUALS" },
	{ 4, 14, 0, "KEY_BACK" },
	{ 4, 15, 0, "KEY_TAB" },
	{ 4, 16, 0, "KEY_Q" },
	{ 4, 17, 0, "KEY_W" },
	{ 4, 18, 0, "KEY_E" },
	{ 4, 19, 0, "KEY_R" },
	{ 4, 20, 0, "KEY_T" },
	{ 4, 21, 0, "KEY_Y" },
	{ 4, 22, 0, "KEY_U" },
	{ 4, 23, 0, "KEY_I" },
	{ 4, 24, 0, "KEY_O" },
	{ 4, 25, 0, "KEY_P" },
	{ 4, 26, 0, "KEY_LBRACKET" },
	{ 4, 27, 0, "KEY_RBRACKET" },
	{ 4, 28, 0, "KEY_RETURN" },
	{ 4, 29, 0, "KEY_LCONTROL" },
	{ 4, 30, 0, "KEY_A" },
	{ 4, 31, 0, "KEY_S" },
	{ 4, 32, 0, "KEY_D" },
	{ 4, 33, 0, "KEY_F" },
	{ 4, 34, 0, "KEY_G" },
	{ 4, 35, 0, "KEY_H" },
	{ 4, 36, 0, "KEY_J" },
	{ 4, 37, 0, "KEY_K" },
	{ 4, 38, 0, "KEY_L" },
	{ 4, 39, 0, "KEY_SEMICOLON" },
	{ 4, 40, 0, "KEY_APOSTROPHE" },
	{ 4, 41, 0, "KEY_GRAVE" },
	{ 4, 42, 0, "KEY_LSHIFT" },
	{ 4, 43, 0, "KEY_BACKSLASH" },
	{ 4, 44, 0, "KEY_Z" },
	{ 4, 45, 0, "KEY_X" },
	{ 4, 46, 0, "KEY_C" },
	{ 4, 47, 0, "KEY_V" },
	{ 4, 48, 0, "KEY_B" },
	{ 4, 49, 0, "KEY_N" },
	{ 4, 50, 0, "KEY_M" },
	{ 4, 51, 0, "KEY_COMMA" },
	{ 4, 52, 0, "KEY_PERIOD" },
	{ 4, 53, 0, "KEY_SLASH" },
	{ 4, 54, 0, "KEY_RSHIFT" },
	{ 4, 55, 0, "KEY_MULTIPLY" },
	{ 4, 56, 0, "KEY_LMENU" },
	{ 4, 57, 0, "KEY_SPACE" },
	{ 4, 58, 0, "KEY_CAPITAL" },
	{ 4, 59, 0, "KEY_F1" },
	{ 4, 60, 0, "KEY_F2" },
	{ 4, 61, 0, "KEY_F3" },
	{ 4, 62, 0, "KEY_F4" },
	{ 4, 63, 0, "KEY_F5" },
	{ 4, 64, 0, "KEY_F6" },
	{ 4, 65, 0, "KEY_F7" },
	{ 4, 66, 0, "KEY_F8" },
	{ 4, 67, 0, "KEY_F9" },
	{ 4, 68, 0, "KEY_F10" },
	{ 4, 69, 0, "KEY_NUMLOCK" },
	{ 4, 70, 0, "KEY_SCROLL" },
	{ 4, 71, 0, "KEY_NUMPAD7" },
	{ 4, 72, 0, "KEY_NUMPAD8" },
	{ 4, 73, 0, "KEY_NUMPAD9" },
	{ 4, 74, 0, "KEY_SUBTRACT" },
	{ 4, 75, 0, "KEY_NUMPAD4" },
	{ 4, 76, 0, "KEY_NUMPAD5" },
	{ 4, 77, 0, "KEY_NUMPAD6" },
	{ 4, 78, 0, "KEY_ADD" },
	{ 4, 79, 0, "KEY_NUMPAD1" },
	{ 4, 80, 0, "KEY_NUMPAD2" },
	{ 4, 81, 0, "KEY_NUMPAD3" },
	{ 4, 82, 0, "KEY_NUMPAD0" },
	{ 4, 83, 0, "KEY_DECIMAL" },
	{ 4, 87, 0, "KEY_F11" },
	{ 4, 88, 0, "KEY_F12" },
	{ 4, 100, 0, "KEY_F13" },
	{ 4, 101, 0, "KEY_F14" },
	{ 4, 102, 0, "KEY_F15" },
	{ 4, 112, 0, "KEY_KANA" },
	{ 4, 121, 0, "KEY_CONVERT" },
	{ 4, 123, 0, "KEY_NOCONVERT" },
	{ 4, 125, 0, "KEY_YEN" },
	{ 4, 141, 0, "KEY_NUMPADEQUALS" },
	{ 4, 144, 0, "KEY_CIRCUMFLEX" },
	{ 4, 145, 0, "KEY_AT" },
	{ 4, 146, 0, "KEY_COLON" },
	{ 4, 147, 0, "KEY_UNDERLINE" },
	{ 4, 148, 0, "KEY_KANJI" },
	{ 4, 149, 0, "KEY_STOP" },
	{ 4, 150, 0, "KEY_AX" },
	{ 4, 151, 0, "KEY_UNLABELED" },
	{ 4, 156, 0, "KEY_NUMPADENTER" },
	{ 4, 157, 0, "KEY_RCONTROL" },
	{ 4, 179, 0, "KEY_NUMPADCOMMA" },
	{ 4, 181, 0, "KEY_DIVIDE" },
	{ 4, 183, 0, "KEY_SYSRQ" },
	{ 4, 184, 0, "KEY_RMENU" },
	{ 4, 199, 0, "KEY_HOME" },
	{ 4, 200, 0, "KEY_UP" },
	{ 4, 201, 0, "KEY_PRIOR" },
	{ 4, 203, 0, "KEY_LEFT" },
	{ 4, 205, 0, "KEY_RIGHT" },
	{ 4, 207, 0, "KEY_END" },
	{ 4, 208, 0, "KEY_DOWN" },
	{ 4, 209, 0, "KEY_NEXT" },
	{ 4, 210, 0, "KEY_INSERT" },
	{ 4, 211, 0, "KEY_DELETE" },
	{ 4, 219, 0, "KEY_LWIN" },
	{ 4, 220, 0, "KEY_RWIN" },
	{ 4, 221, 0, "KEY_APPS" },
	{ 4, 256, 0, "KEY_JOY1_B1" },
	{ 4, 257, 0, "KEY_JOY1_B2" },
	{ 4, 258, 0, "KEY_JOY1_B3" },
	{ 4, 259, 0, "KEY_JOY1_B4" },
	{ 4, 260, 0, "KEY_JOY1_B5" },
	{ 4, 261, 0, "KEY_JOY1_B6" },
	{ 4, 262, 0, "KEY_JOY1_B7" },
	{ 4, 263, 0, "KEY_JOY1_B8" },
	{ 4, 264, 0, "KEY_JOY1_B9" },
	{ 4, 265, 0, "KEY_JOY1_B10" },
	{ 4, 266, 0, "KEY_JOY1_HLEFT" },
	{ 4, 267, 0, "KEY_JOY1_HUP" },
	{ 4, 268, 0, "KEY_JOY1_HRIGHT" },
	{ 4, 269, 0, "KEY_JOY1_HDOWN" },
	{ 4, 270, 0, "KEY_JOY2_B1" },
	{ 4, 271, 0, "KEY_JOY2_B2" },
	{ 4, 272, 0, "KEY_JOY2_B3" },
	{ 4, 273, 0, "KEY_JOY2_B4" },
	{ 4, 274, 0, "KEY_JOY2_B5" },
	{ 4, 275, 0, "KEY_JOY2_B6" },
	{ 4, 276, 0, "KEY_JOY2_B7" },
	{ 4, 277, 0, "KEY_JOY2_B8" },
	{ 4, 278, 0, "KEY_JOY2_B9" },
	{ 4, 279, 0, "KEY_JOY2_B10" },
	{ 4, 280, 0, "KEY_JOY2_HLEFT" },
	{ 4, 281, 0, "KEY_JOY2_HUP" },
	{ 4, 282, 0, "KEY_JOY2_HRIGHT" },
	{ 4, 283, 0, "KEY_JOY2_HDOWN" },
	{ 4, 284, 0, "KEY_MOUSE_B1" },
	{ 4, 285, 0, "KEY_MOUSE_B2" },
	{ 4, 286, 0, "KEY_MOUSE_B3" },
	{ 4, 287, 0, "KEY_MOUSE_B4" },
	{ 2, 0, 0, "AXIS_JOY1_X" },
	{ 2, 1, 0, "AXIS_JOY1_Y" },
	{ 2, 2, 0, "AXIS_JOY1_Z" },
	{ 2, 3, 0, "AXIS_JOY1_R" },
	{ 2, 4, 0, "AXIS_JOY1_U" },
	{ 2, 5, 0, "AXIS_JOY1_V" },
	{ 2, 6, 0, "AXIS_JOY2_X" },
	{ 2, 7, 0, "AXIS_JOY2_Y" },
	{ 2, 8, 0, "AXIS_JOY2_Z" },
	{ 2, 9, 0, "AXIS_JOY2_R" },
	{ 2, 10, 0, "AXIS_JOY2_U" },
	{ 2, 11, 0, "AXIS_JOY2_V" },
	{ 2, 12, 0, "AXIS_MOUSE_X" },
	{ 2, 13, 0, "AXIS_MOUSE_Y" },
	{ 2, 14, 0, "AXIS_MOUSE_Z" }
};

// Symbol order of the remastered, the Lua state stores the index. The codes are the same
// internal codes of the table above. The second F11 entry (the F10 key) keeps the name of the
// original, and the pad buttons 11 and 12 share their codes with the first two hat codes.
static const ControlSymbol controlSymbolTableRemastered[kNumControlSymbolsRemastered] = {
	{ 4, 1, 0, "KEY_ESCAPE" },
	{ 4, 2, 0, "KEY_1" },
	{ 4, 3, 0, "KEY_2" },
	{ 4, 4, 0, "KEY_3" },
	{ 4, 5, 0, "KEY_4" },
	{ 4, 6, 0, "KEY_5" },
	{ 4, 7, 0, "KEY_6" },
	{ 4, 8, 0, "KEY_7" },
	{ 4, 9, 0, "KEY_8" },
	{ 4, 10, 0, "KEY_9" },
	{ 4, 11, 0, "KEY_0" },
	{ 4, 12, 0, "KEY_MINUS" },
	{ 4, 13, 0, "KEY_EQUALS" },
	{ 4, 14, 0, "KEY_BACK" },
	{ 4, 15, 0, "KEY_TAB" },
	{ 4, 16, 0, "KEY_Q" },
	{ 4, 17, 0, "KEY_W" },
	{ 4, 18, 0, "KEY_E" },
	{ 4, 19, 0, "KEY_R" },
	{ 4, 20, 0, "KEY_T" },
	{ 4, 21, 0, "KEY_Y" },
	{ 4, 22, 0, "KEY_U" },
	{ 4, 23, 0, "KEY_I" },
	{ 4, 24, 0, "KEY_O" },
	{ 4, 25, 0, "KEY_P" },
	{ 4, 26, 0, "KEY_LBRACKET" },
	{ 4, 27, 0, "KEY_RBRACKET" },
	{ 4, 28, 0, "KEY_RETURN" },
	{ 4, 29, 0, "KEY_LCONTROL" },
	{ 4, 30, 0, "KEY_A" },
	{ 4, 31, 0, "KEY_S" },
	{ 4, 32, 0, "KEY_D" },
	{ 4, 33, 0, "KEY_F" },
	{ 4, 34, 0, "KEY_G" },
	{ 4, 35, 0, "KEY_H" },
	{ 4, 36, 0, "KEY_J" },
	{ 4, 37, 0, "KEY_K" },
	{ 4, 38, 0, "KEY_L" },
	{ 4, 39, 0, "KEY_SEMICOLON" },
	{ 4, 40, 0, "KEY_APOSTROPHE" },
	{ 4, 41, 0, "KEY_GRAVE" },
	{ 4, 42, 0, "KEY_LSHIFT" },
	{ 4, 43, 0, "KEY_BACKSLASH" },
	{ 4, 44, 0, "KEY_Z" },
	{ 4, 45, 0, "KEY_X" },
	{ 4, 46, 0, "KEY_C" },
	{ 4, 47, 0, "KEY_V" },
	{ 4, 48, 0, "KEY_B" },
	{ 4, 49, 0, "KEY_N" },
	{ 4, 50, 0, "KEY_M" },
	{ 4, 51, 0, "KEY_COMMA" },
	{ 4, 52, 0, "KEY_PERIOD" },
	{ 4, 53, 0, "KEY_SLASH" },
	{ 4, 54, 0, "KEY_RSHIFT" },
	{ 4, 55, 0, "KEY_MULTIPLY" },
	{ 4, 56, 0, "KEY_LMENU" },
	{ 4, 57, 0, "KEY_SPACE" },
	{ 4, 58, 0, "KEY_CAPITAL" },
	{ 4, 59, 0, "KEY_F1" },
	{ 4, 60, 0, "KEY_F2" },
	{ 4, 61, 0, "KEY_F3" },
	{ 4, 62, 0, "KEY_F4" },
	{ 4, 63, 0, "KEY_F5" },
	{ 4, 64, 0, "KEY_F6" },
	{ 4, 65, 0, "KEY_F7" },
	{ 4, 66, 0, "KEY_F8" },
	{ 4, 67, 0, "KEY_F9" },
	{ 4, 68, 0, "KEY_F11" },
	{ 4, 69, 0, "KEY_NUMLOCK" },
	{ 4, 70, 0, "KEY_SCROLL" },
	{ 4, 71, 0, "KEY_NUMPAD7" },
	{ 4, 72, 0, "KEY_NUMPAD8" },
	{ 4, 73, 0, "KEY_NUMPAD9" },
	{ 4, 74, 0, "KEY_SUBTRACT" },
	{ 4, 75, 0, "KEY_NUMPAD4" },
	{ 4, 76, 0, "KEY_NUMPAD5" },
	{ 4, 77, 0, "KEY_NUMPAD6" },
	{ 4, 78, 0, "KEY_ADD" },
	{ 4, 79, 0, "KEY_NUMPAD1" },
	{ 4, 80, 0, "KEY_NUMPAD2" },
	{ 4, 81, 0, "KEY_NUMPAD3" },
	{ 4, 82, 0, "KEY_NUMPAD0" },
	{ 4, 83, 0, "KEY_DECIMAL" },
	{ 4, 87, 0, "KEY_F11" },
	{ 4, 88, 0, "KEY_F12" },
	{ 4, 100, 0, "KEY_F13" },
	{ 4, 101, 0, "KEY_F14" },
	{ 4, 102, 0, "KEY_F15" },
	{ 4, 141, 0, "KEY_NUMPADEQUALS" },
	{ 4, 145, 0, "KEY_AT" },
	{ 4, 146, 0, "KEY_COLON" },
	{ 4, 147, 0, "KEY_UNDERLINE" },
	{ 4, 149, 0, "KEY_STOP" },
	{ 4, 156, 0, "KEY_NUMPADENTER" },
	{ 4, 157, 0, "KEY_RCONTROL" },
	{ 4, 179, 0, "KEY_NUMPADCOMMA" },
	{ 4, 181, 0, "KEY_DIVIDE" },
	{ 4, 183, 0, "KEY_SYSRQ" },
	{ 4, 184, 0, "KEY_RMENU" },
	{ 4, 199, 0, "KEY_HOME" },
	{ 4, 200, 0, "KEY_UP" },
	{ 4, 201, 0, "KEY_PRIOR" },
	{ 4, 203, 0, "KEY_LEFT" },
	{ 4, 205, 0, "KEY_RIGHT" },
	{ 4, 207, 0, "KEY_END" },
	{ 4, 208, 0, "KEY_DOWN" },
	{ 4, 209, 0, "KEY_NEXT" },
	{ 4, 210, 0, "KEY_INSERT" },
	{ 4, 211, 0, "KEY_DELETE" },
	{ 4, 219, 0, "KEY_LWIN" },
	{ 4, 220, 0, "KEY_RWIN" },
	{ 4, 221, 0, "KEY_APS" },
	{ 4, 256, 0, "KEY_JOY1_B1" },
	{ 4, 257, 0, "KEY_JOY1_B2" },
	{ 4, 258, 0, "KEY_JOY1_B3" },
	{ 4, 259, 0, "KEY_JOY1_B4" },
	{ 4, 260, 0, "KEY_JOY1_B5" },
	{ 4, 261, 0, "KEY_JOY1_B6" },
	{ 4, 262, 0, "KEY_JOY1_B7" },
	{ 4, 263, 0, "KEY_JOY1_B8" },
	{ 4, 264, 0, "KEY_JOY1_B9" },
	{ 4, 265, 0, "KEY_JOY1_B10" },
	{ 4, 266, 0, "KEY_JOY1_B11" },
	{ 4, 267, 0, "KEY_JOY1_B12" },
	{ 4, 266, 0, "KEY_JOY1_HLEFT" },
	{ 4, 267, 0, "KEY_JOY1_HUP" },
	{ 4, 268, 0, "KEY_JOY1_HRIGHT" },
	{ 4, 269, 0, "KEY_JOY1_HDOWN" },
	{ 4, 270, 0, "KEY_JOY2_B1" },
	{ 4, 271, 0, "KEY_JOY2_B2" },
	{ 4, 272, 0, "KEY_JOY2_B3" },
	{ 4, 273, 0, "KEY_JOY2_B4" },
	{ 4, 274, 0, "KEY_JOY2_B5" },
	{ 4, 275, 0, "KEY_JOY2_B6" },
	{ 4, 276, 0, "KEY_JOY2_B7" },
	{ 4, 277, 0, "KEY_JOY2_B8" },
	{ 4, 278, 0, "KEY_JOY2_B9" },
	{ 4, 279, 0, "KEY_JOY2_B10" },
	{ 4, 280, 0, "KEY_JOY2_HLEFT" },
	{ 4, 281, 0, "KEY_JOY2_HUP" },
	{ 4, 282, 0, "KEY_JOY2_HRIGHT" },
	{ 4, 283, 0, "KEY_JOY2_HDOWN" },
	{ 4, 284, 0, "KEY_MOUSE_B1" },
	{ 4, 285, 0, "KEY_MOUSE_B2" },
	{ 4, 286, 0, "KEY_MOUSE_B3" },
	{ 4, 287, 0, "KEY_MOUSE_B4" },
	{ 4, 288, 0, "KEY_MOUSE_LONG" },
	{ 4, 289, 0, "KEY_MOUSE_PING" },
	{ 2, 0, 0, "AXIS_JOY1_X" },
	{ 2, 1, 0, "AXIS_JOY1_Y" },
	{ 2, 2, 0, "AXIS_JOY1_Z" },
	{ 2, 3, 0, "AXIS_JOY1_R" },
	{ 2, 4, 0, "AXIS_JOY1_U" },
	{ 2, 5, 0, "AXIS_JOY1_V" },
	{ 2, 6, 0, "AXIS_JOY2_X" },
	{ 2, 7, 0, "AXIS_JOY2_Y" },
	{ 2, 8, 0, "AXIS_JOY2_Z" },
	{ 2, 9, 0, "AXIS_JOY2_R" },
	{ 2, 10, 0, "AXIS_JOY2_U" },
	{ 2, 11, 0, "AXIS_JOY2_V" },
	{ 2, 12, 0, "AXIS_MOUSE_X" },
	{ 2, 13, 0, "AXIS_MOUSE_Y" },
	{ 2, 14, 0, "AXIS_MOUSE_Z" }
};

SputControl::SputControl(RenderDroid *rd, SputRoom *sputRoom, StdControl *stdControl, bool remastered) : _rd(rd), _sputRoom(sputRoom),
		_stdControl(stdControl), _grim(nullptr), _hs(nullptr), _initialized(0), _started(0), _controlOpen(0), _numCallbacks(0),
		_numSymbols(remastered ? kNumControlSymbolsRemastered : kNumControlSymbols) {
	memset(_callbacks, 0, sizeof(_callbacks));
	memset(_controlSymbols, 0, sizeof(_controlSymbols));

	if (remastered)
		memcpy(_controlSymbols, controlSymbolTableRemastered, sizeof(controlSymbolTableRemastered));
	else
		memcpy(_controlSymbols, controlSymbolTable, sizeof(controlSymbolTable));
}

int SputControl::controlCallback(float frameTime) {
	return standardHandler() != 0;
}

void SputControl::resetSymbols() {
	for (int i = 0; i < _numSymbols; i++) {
		_controlSymbols[i].flags &= 0xFFFFFFF6;
		_controlSymbols[i].lastValue = 0;
	}

	_stdControl->reset();
}

void SputControl::enableControl(uint index) {
	if (index < (uint)_numSymbols) {
		ControlSymbol *symbol = &_controlSymbols[index];

		if (symbol->flags & 2)
			_stdControl->pressButton(symbol->code);

		if (!(symbol->flags & 1))
			symbol->flags |= 1;
	}
}

void SputControl::disableControl(uint index) {
	if (index < (uint)_numSymbols) {
		ControlSymbol *symbol = &_controlSymbols[index];

		if (symbol->flags & 1)
			symbol->flags &= ~1u;
	}
}

int SputControl::initialize(SysStruct *hs) {
	if (!_initialized) {
		_hs = hs;
		_initialized = 1;
	}

	return 0;
}

int SputControl::open() {
	int result = _stdControl->open();

	if (result) {
		result = 1;
		_controlOpen = 1;
	}

	return result;
}

int SputControl::close() {
	if (!_controlOpen)
		return 0;

	int result = _stdControl->close();

	if (result) {
		_controlOpen = 0;
		return 1;
	}

	return result;
}

int SputControl::addCallback(SputControlCallback *callback) {
	if (_numCallbacks >= kMaxControlCallbacks) {
		warning("SputControl::addCallback(): too many callbacks");
		return _numCallbacks;
	}

	_callbacks[_numCallbacks] = callback;
	_numCallbacks++;

	return _numCallbacks;
}

void SputControl::update(float frameTime) {
	if (_numCallbacks > 0) {
		_stdControl->readControls();

		for (int i = 0; i < _numCallbacks; i++) {
			if (_callbacks[i] && _callbacks[i]->controlCallback(frameTime))
				break;
		}

		_rd->_primit3.clearFrameCounters();
	}
}

int SputControl::standardHandler() {
	if (_grim->getGameFlags() & ADGF_DEMO) {
		int keyDown = _stdControl->readKey(29, nullptr, nullptr);
		if (!keyDown)
			return 0;

		keyDown = _stdControl->readKey(78, nullptr, nullptr);
		if (keyDown)
			changeFOV(0.5);

		keyDown = _stdControl->readKey(74, nullptr, nullptr);
		if (keyDown)
			return changeFOV(-0.5);

		return 0;
	}

	int keyDown = _grim->getDebugKeys();

	if (!keyDown)
		return 0;

	keyDown = _stdControl->readKey(29, nullptr, nullptr);
	if (!keyDown)
		return 0;

	double step;

	keyDown = _stdControl->readKey(78, nullptr, nullptr);
	if (keyDown) {
		step = 0.5;
	} else {
		keyDown = _stdControl->readKey(74, nullptr, nullptr);
		if (!keyDown)
			return 0;

		step = -0.5;
	}

	return changeFOV(step);
}

int SputControl::changeFOV(double step) {
	Room *room = _sputRoom->getCurrentRoom();

	if (!room || room->curSetup < 0 || (uint)room->curSetup >= room->numSetups) {
		warning("SputControl::changeFOV(): there is no current setup");
		return 0;
	}

	RdCamera *camera = room->setups[room->curSetup].camera.prdCamera;

	if (_grim->getGameFlags() & ADGF_DEMO)
		debug("Changing FOV from %f to %f...", (double)camera->fov, (double)camera->fov + step);

	_rd->_camera.setFOV(camera, (float)((double)camera->fov + step));

	return 1;
}

int SputControl::terminate() {
	_hs = nullptr;

	if (_initialized)
		_initialized = 0;

	return 0;
}

int SputControl::startup() {
	if (_started)
		return 1;

	if (!_stdControl->isStarted() && !_stdControl->startup())
		return 1;

	resetSymbols();
	addCallback(this);
	_started = 1;

	return 0;
}

int SputControl::shutdown() {
	_stdControl->shutdown();
	_started = 0;

	return 0;
}

} // End of namespace Grim
