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

#ifndef GRIM_EMI_KEYGENERATOR_H
#define GRIM_EMI_KEYGENERATOR_H

#include "common/keyboard.h"
#include "common/scummsys.h"

#include "engines/grim/emi/input/eventgenerator.h"

namespace Grim {

class KeyGenerator : public EventGenerator {
public:
	enum {
		kFlagExtended = 0x1000000,
		kFlagRepeat = 0x40000000
	};

	explicit KeyGenerator(EmiEngine *engine);
	~KeyGenerator() override;

	void poll() override;

	void keyDown(int vk, int keyFlags, int timestamp);
	void keyUp(int vk, int keyFlags, int timestamp);

	static int keyCodeToVirtualKey(Common::KeyCode keyCode, bool numLockOn);

	uint _modifiers;
};

} // End of namespace Grim

#endif
