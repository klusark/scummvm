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

#ifndef GRIM_MATERIALPATH_H
#define GRIM_MATERIALPATH_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Common {
class SeekableReadStream;
}

namespace Grim {

class Mcb;
class SysStruct;

class MaterialPath {
public:
	enum {
		kMaxNameLength = 255,
		kResourceDirectoryCount = 40
	};

	struct Source {
		enum Kind {
			kDirectory,
			kBaseDirectory,
			kArchives
		};

		Kind kind;
		Common::String name;
		bool extraDataPath;
	};

	static bool buildFileName(const char *materialName, int cellIndex, Common::String &fileName);
	static const char *getResourceDirectory(uint index);
	static void getSearchOrder(bool hasExtraDataPath, Common::Array<Source> &sources);
	static Common::SeekableReadStream *open(SysStruct *hs, const char *materialName, int cellIndex);
	static bool load(SysStruct *hs, const char *materialName, int cellIndex, Mcb &mcb);
};

} // End of namespace Grim

#endif
