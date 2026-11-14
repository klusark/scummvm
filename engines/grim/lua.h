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

#ifndef GRIM_LUABASE_H
#define GRIM_LUABASE_H

#include "common/str.h"
#include "common/list.h"

namespace Common {
class SeekableReadStream;
class WriteStream;
}

namespace Grim {

typedef uint32 lua_Object; // from lua/lua.h

/**
 * Declare a Lua opcode in the current class.
 */
#define DECLARE_LUA_OPCODE(func) \
	public:\
	inline static void static_##func() {\
		static_cast<LuaClass *>(LuaBase::instance())->func();\
	}\
	protected:\
	virtual void func()
/**
 * Retrieve the opcode "func" of the class "class".
 */
#define LUA_OPCODE(class, func) \
	class::static_##func

/**
 * @brief A list of arguments to be passed to a Lua function.
 *
 * This is a convenience class to pass arguments to a Lua function, using
 * LuaBase::callback(const char *name, const LuaObjects &objects).
 */
class LuaObjects {
public:
	void add(float number);
	void add(int number);
	void add(const char *str);
	void addNil();

private:
	/**
	 * Pushes all the objects to Lua, in the same order as they were added.
	 */
	void pushObjects() const;

	/**
	 * The struct wrapping the value and the type of the objects.
	 */
	struct Obj {
		enum {
			Nil,
			Number,
			String
		} _type;
		union {
			float number;
			const char *string;
		} _value;
	};
	Common::List<Obj> _objects;

	friend class LuaBase;
};

class LuaBase {
public:
	typedef LuaBase LuaClass;

	LuaBase(bool isEmi, bool openState = true);
	virtual ~LuaBase();
	inline static LuaBase *instance() { return _instance; }

	virtual int dofile(const char *filename);

	void update(int frameTime, int movieTime);
	void setFrameTime(float frameTime);
	void setMovieTime(float movieTime);
	virtual void recognizeOpcodes();

	// Force the demo flag
	virtual void forceDemo();

	/**
	 * Call a Lua function in the system table.
	 *
	 * @param name The name of the function.
	 */
	bool callback(const char *name);
	/**
	 * Call a Lua function in the system table passing the specified arguments.
	 *
	 * @param name The name of the function;
	 * @param objects The arguments to be passed to the function.
	 */
	bool callback(const char *name, const LuaObjects &objects);

	/**
	 * Report an interpreter error to the host's log.
	 *
	 * @param message The fully formatted message.
	 * @return true if the host handled the message.
	 */
	virtual bool logError(const char *message);

	/**
	 * Open a file from the host's bundles for the interpreter's file library.
	 *
	 * @param path The path of the file.
	 * @return A stream owned by the caller, or nullptr if the host has no such file.
	 */
	virtual Common::SeekableReadStream *openBundleStream(const char *path);

	/**
	 * Wrap a preferences file opened for reading by the interpreter's file library.
	 *
	 * @param realFile The stream of the file on disk, or nullptr.
	 * @return The stream the interpreter should read, owned by the caller.
	 */
	virtual Common::SeekableReadStream *openPrefsForReading(Common::SeekableReadStream *realFile);

	/**
	 * Wrap a preferences file opened for writing by the interpreter's file library.
	 *
	 * @param realFile The stream of the file on disk, or nullptr.
	 * @return The stream the interpreter should write, owned by the caller.
	 */
	virtual Common::WriteStream *openPrefsForWriting(Common::WriteStream *realFile);

protected:
	bool getbool(int num);
	void pushbool(bool val);

	void registerTypeOverride();
	void registerSectorConstants();
	void registerConcatFallback();

	DECLARE_LUA_OPCODE(dummyHandler);
	DECLARE_LUA_OPCODE(typeOverride);
	DECLARE_LUA_OPCODE(concatFallback);

	int _refSystemTable;
	int _refTypeOverride;
	int _refOldConcatFallback;

private:
	unsigned int _frameTimeCollection;

	static LuaBase *_instance;

	friend class LuaObjects;
};

} // end of namespace Grim

#endif
