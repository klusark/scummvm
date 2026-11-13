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

#ifndef GRIM_SPUTPRIMITIVE_H
#define GRIM_SPUTPRIMITIVE_H

#include "common/scummsys.h"

#include "engines/grim/render/primitivequeue.h"
#include "engines/grim/std/std_sys.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class Lua_V1;
class RenderDroid;
class SputRender;

class SputPrimitive {
public:
	explicit SputPrimitive(RenderDroid *rd);
	~SputPrimitive();

	void setSputRender(SputRender *sputRender);
	void setLua(Lua_V1 *lua) { _lua = lua; }

	int initialize(SysStruct *hs);
	void pause();
	void resume();
	int terminate();
	int startup();
	int shutdown();
	int save(Common::WriteStream *stream);
	int restore(Common::ReadStream *stream);
	int saveRemastered(Common::WriteStream *stream);
	int restoreRemastered(Common::ReadStream *stream);

	PrimitiveQueue *getCurrent() { return &_current; }

private:
	SputRender *_sputRender;
	Lua_V1 *_lua = nullptr;
	SysStruct *_hs;
	int _initialized;
	int _started;
	PrimitiveQueue _current;
};

} // End of namespace Grim

#endif
