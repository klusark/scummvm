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

#include "engines/grim/emi/input/keygenerator.h"

namespace Grim {

KeyGenerator::KeyGenerator(EmiEngine *engine) : EventGenerator(engine, 0), _modifiers(0) {
}

KeyGenerator::~KeyGenerator() {
}

void KeyGenerator::poll() {
}

void KeyGenerator::keyDown(int vk, int keyFlags, int timestamp) {
	int code = vk;

	switch (vk) {
	case 13:
		// The disasm sets the same code again for an extended RETURN, so KEY_NPENTER is never produced
		if (keyFlags & kFlagExtended)
			code = 13;

		_modifiers |= Event::kModifierCapsLock;
		break;
	case 16:
		_modifiers |= Event::kModifierShift;
		break;
	case 17:
		_modifiers |= Event::kModifierControl;
		break;
	case 18:
		_modifiers |= Event::kModifierAlt;
		break;
	case 20:
		_modifiers |= Event::kModifierCapsLock;
		break;
	case 91:
	case 92:
		_modifiers |= Event::kModifierWin;
		break;
	case 93:
		_modifiers |= Event::kModifierApps;
		break;
	default:
		break;
	}

	Event *event = new Event();

	event->type = Event::kKeyDown;
	event->vk = code;
	event->timestamp = (uint)timestamp;
	event->modifiers = _modifiers;

	if (keyFlags & kFlagRepeat)
		event->modifiers = _modifiers | Event::kModifierRepeat;

	postEvent(_engine, event);
}

void KeyGenerator::keyUp(int vk, int keyFlags, int timestamp) {
	switch (vk) {
	case 16:
		_modifiers &= ~(uint)Event::kModifierShift;
		break;
	case 17:
		_modifiers &= ~(uint)Event::kModifierControl;
		break;
	case 18:
		_modifiers &= ~(uint)Event::kModifierAlt;
		break;
	case 20:
		_modifiers &= ~(uint)Event::kModifierCapsLock;
		break;
	case 91:
	case 92:
		_modifiers &= ~(uint)Event::kModifierWin;
		break;
	case 93:
		_modifiers &= ~(uint)Event::kModifierApps;
		break;
	default:
		break;
	}

	Event *event = new Event();

	event->type = Event::kKeyUp;
	event->vk = vk;
	event->timestamp = (uint)timestamp;
	event->modifiers = _modifiers;

	postEvent(_engine, event);
}

int KeyGenerator::keyCodeToVirtualKey(Common::KeyCode keyCode, bool numLockOn) {
	// The exe takes the virtual key straight from the window message, so with Num Lock off
	// the keypad gives the navigation keys
	if (!numLockOn) {
		switch (keyCode) {
		case Common::KEYCODE_KP0:
			return 0x2D;
		case Common::KEYCODE_KP1:
			return 0x23;
		case Common::KEYCODE_KP2:
			return 0x28;
		case Common::KEYCODE_KP3:
			return 0x22;
		case Common::KEYCODE_KP4:
			return 0x25;
		case Common::KEYCODE_KP5:
			return 0x0C;
		case Common::KEYCODE_KP6:
			return 0x27;
		case Common::KEYCODE_KP7:
			return 0x24;
		case Common::KEYCODE_KP8:
			return 0x26;
		case Common::KEYCODE_KP9:
			return 0x21;
		case Common::KEYCODE_KP_PERIOD:
			return 0x2E;
		default:
			break;
		}
	}

	if (keyCode >= Common::KEYCODE_0 && keyCode <= Common::KEYCODE_9)
		return 0x30 + (keyCode - Common::KEYCODE_0);

	if (keyCode >= Common::KEYCODE_a && keyCode <= Common::KEYCODE_z)
		return 0x41 + (keyCode - Common::KEYCODE_a);

	if (keyCode >= Common::KEYCODE_F1 && keyCode <= Common::KEYCODE_F12)
		return 0x70 + (keyCode - Common::KEYCODE_F1);

	if (keyCode >= Common::KEYCODE_KP0 && keyCode <= Common::KEYCODE_KP9)
		return 0x60 + (keyCode - Common::KEYCODE_KP0);

	switch (keyCode) {
	case Common::KEYCODE_BACKSPACE:
		return 0x08;
	case Common::KEYCODE_TAB:
		return 0x09;
	case Common::KEYCODE_CLEAR:
		return 0x0C;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
		return 0x0D;
	case Common::KEYCODE_LSHIFT:
	case Common::KEYCODE_RSHIFT:
		return 0x10;
	case Common::KEYCODE_LCTRL:
	case Common::KEYCODE_RCTRL:
		return 0x11;
	case Common::KEYCODE_LALT:
	case Common::KEYCODE_RALT:
		return 0x12;
	case Common::KEYCODE_PAUSE:
		return 0x13;
	case Common::KEYCODE_CAPSLOCK:
		return 0x14;
	case Common::KEYCODE_ESCAPE:
		return 0x1B;
	case Common::KEYCODE_SPACE:
		return 0x20;
	case Common::KEYCODE_PAGEUP:
		return 0x21;
	case Common::KEYCODE_PAGEDOWN:
		return 0x22;
	case Common::KEYCODE_END:
		return 0x23;
	case Common::KEYCODE_HOME:
		return 0x24;
	case Common::KEYCODE_LEFT:
		return 0x25;
	case Common::KEYCODE_UP:
		return 0x26;
	case Common::KEYCODE_RIGHT:
		return 0x27;
	case Common::KEYCODE_DOWN:
		return 0x28;
	case Common::KEYCODE_INSERT:
		return 0x2D;
	case Common::KEYCODE_DELETE:
		return 0x2E;
	case Common::KEYCODE_LSUPER:
		return 0x5B;
	case Common::KEYCODE_RSUPER:
		return 0x5C;
	case Common::KEYCODE_MENU:
		return 0x5D;
	case Common::KEYCODE_KP_MULTIPLY:
		return 0x6A;
	case Common::KEYCODE_KP_PLUS:
		return 0x6B;
	case Common::KEYCODE_KP_MINUS:
		return 0x6D;
	case Common::KEYCODE_KP_PERIOD:
		return 0x6E;
	case Common::KEYCODE_KP_DIVIDE:
		return 0x6F;
	case Common::KEYCODE_NUMLOCK:
		return 0x90;
	case Common::KEYCODE_SCROLLOCK:
		return 0x91;
	case Common::KEYCODE_SEMICOLON:
		return 0xBA;
	case Common::KEYCODE_EQUALS:
		return 0xBB;
	case Common::KEYCODE_COMMA:
		return 0xBC;
	case Common::KEYCODE_MINUS:
		return 0xBD;
	case Common::KEYCODE_PERIOD:
		return 0xBE;
	case Common::KEYCODE_SLASH:
		return 0xBF;
	case Common::KEYCODE_BACKQUOTE:
		return 0xC0;
	case Common::KEYCODE_LEFTBRACKET:
		return 0xDB;
	case Common::KEYCODE_BACKSLASH:
		return 0xDC;
	case Common::KEYCODE_RIGHTBRACKET:
		return 0xDD;
	case Common::KEYCODE_QUOTE:
		return 0xDE;
	default:
		return 0;
	}
}

} // End of namespace Grim
