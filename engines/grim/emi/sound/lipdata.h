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

#ifndef GRIM_EMI_LIPDATA_H
#define GRIM_EMI_LIPDATA_H

#include "common/scummsys.h"

#include "engines/grim/lua/lua.h"

namespace Grim {

struct LipEntry {
	byte time[2];
	byte code[2];
};

class EmiEngine;

// Byte image of a .lip file, never constructed: the module data live in EmiEngine
struct LipData {
	enum {
		kBufferSize = 2000,
		kMouthTableSize = 46
	};

	struct MouthEntry {
		uint32 code;
		uint32 shape;
	};

	static void initMouthTable(EmiEngine *engine);
	static int readFile(EmiEngine *engine, const char *name, LipData **lipData);
	static void saveFileName(EmiEngine *engine, SaveRestoreAccess *accessFunc);
	static int restoreFileName(EmiEngine *engine, SaveRestoreAccess *accessFunc);
	static int codeToMouthShape(EmiEngine *engine, int code);
	static int compareMouthEntries(const void *first, const void *second);
	static void sortMouthTable(EmiEngine *engine);
	static int getLipMouthShape(EmiEngine *engine, uint soundHandle, int position);

	int findCode(int position) const;

	byte reserved0[4];
	byte count[4];
	LipEntry entries[1];
};

} // End of namespace Grim

#endif
