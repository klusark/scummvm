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

#ifndef GRIM_REMASTERED_LOOK_LOOK_PROGRAMS_H
#define GRIM_REMASTERED_LOOK_LOOK_PROGRAMS_H

#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_types.h"

namespace Grim {

class RemasteredLookPrograms {
public:
	enum {
		kOutputOne = 1,
		kOutputFloat = 2,
		kOutputThree = 3
	};

	static const char *getPixelName(int program);
	static const char *getVertexName(int program);
	static int getVertexFormat(int program);
	static int getOutputKind(int program);
	static bool isKnown(int program);
};

} // End of namespace Grim

#endif
