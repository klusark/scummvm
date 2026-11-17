/*
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/

#define FORBIDDEN_SYMBOL_EXCEPTION_setjmp
#define FORBIDDEN_SYMBOL_EXCEPTION_longjmp
#define FORBIDDEN_SYMBOL_EXCEPTION_fread
#define FORBIDDEN_SYMBOL_EXCEPTION_fwrite
#define FORBIDDEN_SYMBOL_EXCEPTION_fseek
#define FORBIDDEN_SYMBOL_EXCEPTION_sprintf
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_stdin
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_exit

#include "common/savefile.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/memstream.h"
#include "common/util.h"

#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/lua/luadebug.h"
#include "engines/grim/lua/lualib.h"

#include "base/commandLine.h"

#include "engines/grim/grim.h"
#include "engines/grim/lua.h"

#include "engines/grim/emi/emi_registry.h"

namespace Grim {

#define CLOSEDTAG       2
#define IOTAG           1

#define FIRSTARG        3  // 1st and 2nd are upvalues

#define FINPUT          "_INPUT"
#define FOUTPUT         "_OUTPUT"

LuaFile *stderrFile;

enum {
	kRemasteredStdinId = 0x7AFE3008,
	kRemasteredStdoutId = 0x7AFE3028,
	kRemasteredStderrId = 0x7AFE3048,
	kRemasteredLegacyFileIds = 5
};

static int32 lastFileId = 0;
Common::HashMap<int32, LuaFile *> *files;

LuaFile::LuaFile() : _in(nullptr), _out(nullptr), _stdin(false), _stdout(false), _stderr(false), _crlf(false) {
}

int32 lua_translateTextMode(byte *buffer, int32 size) {
	int32 length = 0;

	for (int32 i = 0; i < size; i++) {
		if (buffer[i] == 0x1A)
			break;

		if (buffer[i] == '\r' && i + 1 < size && buffer[i + 1] == '\n')
			continue;

		buffer[length++] = buffer[i];
	}

	return length;
}

LuaFile::~LuaFile() {
	close();
}

void LuaFile::close() {
	delete _in;
	delete _out;
	_in = nullptr;
	_out = nullptr;
	_stdin = _stdout = _stderr = false;
}

bool LuaFile::isOpen() const {
	return _in || _out || _stdin || stdout || stderr;
}

uint32 LuaFile::read(void *buf, uint32 len) {
	if (_stdin) {
		return fread(buf, len, 1, stdin);
	} else if (_in) {
		return _in->read(buf, len);
	} else
		assert(0);
	return 0;
}

uint32 LuaFile::write(const char *buf, uint32 len) {
	if (_stdin)
		error("LuaFile::write() not allowed on stdin");
	if (_in)
		error("LuaFile::write() not allowed on in");
	if (_stdout) {
		return fwrite(buf, len, 1, stdout);
	} else if (_stderr) {
		return fwrite(buf, len, 1, stderr);
	} else if (_out) {
		if (!_crlf)
			return _out->write(buf, len);

		Common::String text;

		for (uint32 i = 0; i < len; i++) {
			if (buf[i] == '\n')
				text += '\r';

			text += buf[i];
		}

		return (_out->write(text.c_str(), text.size()) == text.size()) ? len : 0;
	} else
		assert(0);
	return 0;
}

void LuaFile::seek(int32 pos, int whence) {
	if (_stdin) {
		fseek(stdin, pos, whence);
	} else if (_in) {
		_in->seek(pos, whence);
	} else
		assert(0);
}

// The files of the scripts of the '98 edition are opened in text mode
static bool isTextModeGame() {
	return !luaIsEmi && !g_grim->isRemastered();
}

static Common::SeekableReadStream *makeTextModeStream(Common::SeekableReadStream *stream) {
	int32 size = (int32)stream->size();
	byte *buffer = (byte *)malloc(size);

	stream->read(buffer, size);
	delete stream;

	return new Common::MemoryReadStream(buffer, lua_translateTextMode(buffer, size), DisposeAfterUse::YES);
}

static int32 gettag(int32 i) {
	return (int32)lua_getnumber(lua_getparam(i));
}

static void pushresult(int32 i) {
	if (i)
		lua_pushuserdata(0);
	else {
		lua_pushnil();
		lua_pushstring("File I/O error.");
	}
}

static int32 ishandler(lua_Object f) {
	if (lua_isuserdata(f)) {
		if (lua_tag(f) == gettag(CLOSEDTAG))
			lua_error("cannot access a closed file");
		return lua_tag(f) == gettag(IOTAG);
	}
	else return 0;
}

static LuaFile *getfile(int32 id) {
	if (files->contains(id)) {
		return (*files)[id];
	}

	return nullptr;
}

static LuaFile *getfile(const char *name) {
	lua_Object f = lua_getglobal(name);
	if (!ishandler(f))
		luaL_verror("global variable `%.50s' is not a file handle", name);
	return getfile(lua_getuserdata(f));
}

static LuaFile *getfileparam(const char *name, int32 *arg) {
	lua_Object f = lua_getparam(*arg);
	if (ishandler(f)) {
		(*arg)++;
		return getfile(lua_getuserdata(f));
	} else
		return getfile(name);
}

static Common::String getFileName(const char *path) {
	if (!luaIsEmi)
		return Common::lastPathComponent(path, '\\');

	return Common::lastPathComponent(Common::lastPathComponent(path, '\\'), '/');
}

static void closefile(const char *name) {
	LuaFile *f = getfile(name);

	if (luaIsEmi && !f)
		warning("closefile(): the file handle is not open");
	else
		f->close();

	lua_pushobject(lua_getglobal(name));
	lua_settag(gettag(CLOSEDTAG));
}

static void setfile(int32 id, const char *name, int32 tag) {
	lua_pushusertag(id, tag);
	lua_setglobal(name);
}

static void setreturn(int32 id, const char *name) {
	int32 tag = gettag(IOTAG);
	setfile(id, name, tag);
	lua_pushusertag(id, tag);
}

static int32 stdinFileId() {
	return g_grim->isRemastered() ? kRemasteredStdinId : 1;
}

static int32 stdoutFileId() {
	return g_grim->isRemastered() ? kRemasteredStdoutId : 2;
}

static int32 addfile(LuaFile *f) {
	++lastFileId;
	(*files)[lastFileId] = f;

	return lastFileId;
}

static void io_readfrom() {
	lua_Object f = lua_getparam(FIRSTARG);
	if (f == LUA_NOOBJECT) {
		if (luaIsEmi) {
			closefile(FINPUT);
			setreturn(0, FINPUT);
		} else if (getfile(FINPUT) != getfile(stdinFileId())) {
			closefile(FINPUT);
			setreturn(stdinFileId(), FINPUT);
		}
	} else if (lua_tag(f) == gettag(IOTAG)) {
		int32 id = lua_getuserdata(f);
		LuaFile *current = getfile(id);
		if (!current) {
			pushresult(0);
			return;
		}
		setreturn(id, FINPUT);
	} else {
		const char *s = luaL_check_string(FIRSTARG);
		Common::String fileName = getFileName(s);
		Common::SeekableReadStream *inFile = nullptr;
		Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
		inFile = saveFileMan->openForLoading(fileName);
		if (luaIsEmi && EmiRegistry::isPrefsFile(fileName))
			inFile = LuaBase::instance()->openPrefsForReading(inFile);
		if (!inFile) {
			if (luaIsEmi) {
				LuaBase *lua = LuaBase::instance();
				if (lua)
					inFile = lua->openBundleStream(s);
			} else {
				inFile = g_grim->openStreamFile(s);

				if (!inFile) {
					Common::FSDirectory gameDirectory(ConfMan.getPath("path"));

					inFile = gameDirectory.createReadStreamForMember(Common::Path(s, '/'));
				}
			}
		}
		if (inFile) {
			LuaFile *current = new LuaFile();
			current->_in = isTextModeGame() ? makeTextModeStream(inFile) : inFile;
			current->_filename = s;
			setreturn(addfile(current), FINPUT);
		} else {
			warning("liolib.cpp, io_readfrom(): Could not open file %s", s);
			pushresult(0);
		}
	}
}

static void io_writeto() {
	lua_Object f = lua_getparam(FIRSTARG);
	if (f == LUA_NOOBJECT) {
		if (luaIsEmi) {
			closefile(FOUTPUT);
			setreturn(0, FOUTPUT);
		} else if (getfile(FOUTPUT) != getfile(stdoutFileId())) {
			closefile(FOUTPUT);
			setreturn(stdoutFileId(), FOUTPUT);
		}
	} else if (lua_tag(f) == gettag(IOTAG)) {
		int32 id = lua_getuserdata(f);
		LuaFile *current = getfile(id);
		if (!current->isOpen()) {
			pushresult(0);
			return;
		}
		setreturn(id, FOUTPUT);
	} else {
		Common::String s = getFileName(luaL_check_string(FIRSTARG));
		LuaFile *current;
		Common::WriteStream *outFile = nullptr;
		Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
		outFile = saveFileMan->openForSaving(s, false);
		if (luaIsEmi && EmiRegistry::isPrefsFile(s))
			outFile = LuaBase::instance()->openPrefsForWriting(outFile);
		if (!outFile) {
			pushresult(0);
			return;
		}
		current = new LuaFile();
		current->_out = outFile;
		current->_crlf = isTextModeGame();
		current->_filename = s;
		setreturn(addfile(current), FOUTPUT);
	}
}

static void io_appendto() {
	Common::String s = getFileName(luaL_check_string(FIRSTARG));
	Common::SeekableReadStream *inFile = nullptr;
	Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
	inFile = saveFileMan->openForLoading(s);
	if (!inFile) {
		pushresult(0);
		return;
	}
	int size = inFile->size();
	byte *buf = new byte[size];
	inFile->read(buf, size);
	delete inFile;

	Common::WriteStream *outFile = nullptr;
	outFile = saveFileMan->openForSaving(s);
	if (!outFile)
		pushresult(0);
	else {
		outFile->write(buf, size);
		LuaFile *current = new LuaFile();
		current->_out = outFile;
		current->_crlf = isTextModeGame();
		current->_filename = s;
		setreturn(addfile(current), FOUTPUT);
	}
	delete[] buf;
}

#define NEED_OTHER (EOF - 1)  // just some flag different from EOF

static void io_read() {
	int32 arg = FIRSTARG;
	LuaFile *f = (LuaFile *)getfileparam(FINPUT, &arg);
	char *buff;
	const char *p = luaL_opt_string(arg, "[^\n]*{\n}");
	int inskip = 0;  // to control {skips}
	int c = NEED_OTHER;
	luaL_resetbuffer();
	while (*p) {
		if (*p == '{') {
			inskip++;
			p++;
		} else if (*p == '}') {
			if (inskip == 0)
				lua_error("unbalanced braces in read pattern");
			inskip--;
			p++;
		} else {
			const char *ep;  // get what is next
			int m;  // match result
			if (c == NEED_OTHER) {
				char z;
				if (f->read(&z, 1) != 1)
					c = EOF;
				else
					c = z;
			}
			m = luaI_singlematch((c == EOF) ? 0 : (char)c, p, &ep);
			if (m) {
				if (inskip == 0)
					luaL_addchar(c);
				c = NEED_OTHER;
			}
			switch (*ep) {
			case '*':  // repetition
				if (!m)
					p = ep + 1;  // else stay in (repeat) the same item
				break;
			case '?':  // optional
				p = ep + 1;  // continues reading the pattern
				break;
			default:
				if (m)
					p = ep;  // continues reading the pattern
				else
					goto break_while;   // pattern fails
			}
		}
	}
break_while:
	if (c >= 0) // not EOF nor NEED_OTHER?
		f->seek(-1, SEEK_CUR);
	luaL_addchar(0);
	buff = luaL_buffer();
	if (*buff != 0 || *p == 0)  // read something or did not fail?
		lua_pushstring(buff);
}

static void io_write() {
	int32 arg = FIRSTARG;
	LuaFile *f = (LuaFile *)getfileparam(FOUTPUT, &arg);
	int32 status = 1;
	const char *s;
	while ((s = luaL_opt_string(arg++, nullptr)) != nullptr)
		status = status && ((int32)f->write(s, strlen(s)) != EOF);
	pushresult(status);
}

static const char *const weekdayNames[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char *const monthNames[] = {
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};

static int32 dayOfYear(const TimeDate &t) {
	static const int32 monthStarts[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	int32 year = 1900 + t.tm_year;
	int32 day = monthStarts[t.tm_mon] + t.tm_mday - 1;

	if (t.tm_mon > 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
		day++;

	return day;
}

// The formats of the C locale
static Common::String formatDate(const char *format, const TimeDate &t) {
	Common::String result;
	int32 hour12 = (t.tm_hour % 12 == 0) ? 12 : t.tm_hour % 12;
	int32 yearDay = dayOfYear(t);

	while (*format) {
		if (*format != '%') {
			result += *format++;
			continue;
		}

		format++;

		switch (*format) {
		case 'a':
			result += Common::String(weekdayNames[t.tm_wday], 3);
			break;
		case 'A':
			result += weekdayNames[t.tm_wday];
			break;
		case 'b':
			result += Common::String(monthNames[t.tm_mon], 3);
			break;
		case 'B':
			result += monthNames[t.tm_mon];
			break;
		case 'c':
			result += formatDate("%x %X", t);
			break;
		case 'd':
			result += Common::String::format("%02d", t.tm_mday);
			break;
		case 'H':
			result += Common::String::format("%02d", t.tm_hour);
			break;
		case 'I':
			result += Common::String::format("%02d", hour12);
			break;
		case 'j':
			result += Common::String::format("%03d", yearDay + 1);
			break;
		case 'm':
			result += Common::String::format("%02d", t.tm_mon + 1);
			break;
		case 'M':
			result += Common::String::format("%02d", t.tm_min);
			break;
		case 'p':
			result += (t.tm_hour < 12) ? "AM" : "PM";
			break;
		case 'S':
			result += Common::String::format("%02d", t.tm_sec);
			break;
		case 'U':
			result += Common::String::format("%02d", (yearDay + 7 - t.tm_wday) / 7);
			break;
		case 'w':
			result += Common::String::format("%d", t.tm_wday);
			break;
		case 'W':
			result += Common::String::format("%02d", (yearDay + 7 - (t.tm_wday + 6) % 7) / 7);
			break;
		case 'x':
			result += formatDate("%m/%d/%y", t);
			break;
		case 'X':
			result += formatDate("%H:%M:%S", t);
			break;
		case 'y':
			result += Common::String::format("%02d", t.tm_year % 100);
			break;
		case 'Y':
			result += Common::String::format("%d", 1900 + t.tm_year);
			break;
		case 'Z':
			break;
		case '%':
			result += '%';
			break;
		case 0:
			return result;
		default:
			result += '%';
			result += *format;
			break;
		}

		format++;
	}

	return result;
}

static void io_date() {
	TimeDate t;
	const char *format = luaL_opt_string(1, "%c");

	g_system->getTimeAndDate(t);

	Common::String text = formatDate(format, t);

	if (text.empty() || text.size() >= 512)
		lua_error("invalid `date' format");

	lua_pushstring(text.c_str());
}

static void io_exit() {
	lua_Object o = lua_getparam(1);

	if (luaIsEmi)
		return;

	exit((int)lua_isnumber(o) ? (int)lua_getnumber(o) : 1);
}

static void errorfb() {
	if (luaIsEmi) {
		const char *message = lua_getstring(lua_getparam(1));

		LuaBase *lua = LuaBase::instance();
		if (lua)
			lua->logError(Common::String::format("lua: %s\n", message ? message : "(null)").c_str());

		lua_printstack(nullptr);
		return;
	}

	char buf[256];
	snprintf(buf, 256, "lua: %s\n", lua_getstring(lua_getparam(1)));
	stderrFile->write(buf, strlen(buf));
	lua_printstack(nullptr);
}

static void io_remove() {
	Common::String fileName = luaL_check_string(1);
	Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();

	if (luaIsEmi)
		fileName = getFileName(fileName.c_str());

	if (saveFileMan->removeSavefile(fileName)) {
		lua_pushuserdata(0);
	} else {
		lua_pushnil();
		lua_pushstring(saveFileMan->getErrorDesc().c_str());
	}
}

static void io_setlocale() {
	float category = luaL_opt_number(2, 0.0f);

	luaL_arg_check(category > -1.0f && category < 6.0f, 2, "invalid option");

	const char *name = luaL_check_string(1);

	// Only the C locale exists
	if (!strcmp(name, "C") || !*name)
		lua_pushstring("C");
	else
		lua_pushnil();
}

static void io_execute() {
	warning("io_execute(): not implemented");
}

static void io_rename() {
	if (luaIsEmi)
		return;

	Common::String newName = luaL_check_string(2);
	Common::String oldName = luaL_check_string(1);

	pushresult(g_system->getSavefileManager()->renameSavefile(oldName, newName));
}

static void io_tmpname() {
	warning("io_tmpname(): not implemented");
}

static void io_getenv() {
	luaL_check_string(1);

	// There is no environment to read
	lua_pushnil();
}

static void io_debug() {
	warning("io_debug(): not implemented");
}

static struct luaL_reg iolib[] = {
	{ "setlocale",   io_setlocale },
	{ "execute",     io_execute },
	{ "remove",      io_remove },
	{ "rename",      io_rename },
	{ "tmpname",     io_tmpname },
	{ "getenv",      io_getenv },
	{ "date",        io_date },
	{ "exit",        io_exit },
	{ "debug",       io_debug },
	{ "print_stack", errorfb }
};

static struct luaL_reg iolibEmi[] = {
	{ "setlocale",   io_setlocale },
	{ "execute",     io_execute },
	{ "remove",      io_remove },
	{ "rename",      io_rename },
	{ "tmpname",     io_tmpname },
	{ "getenv",      io_getenv },
	{ "date",        io_date },
	{ "exit",        io_exit },
	{ "print_stack", errorfb }
};

static struct luaL_reg iolibtag[] = {
	{ "readfrom",    io_readfrom },
	{ "writeto",     io_writeto },
	{ "appendto",    io_appendto },
	{ "read",        io_read },
	{ "write",       io_write }
};

static void openwithtags() {
	int32 iotag = lua_newtag();
	int32 closedtag = lua_newtag();
	for (uint32 i = 0; i < ARRAYSIZE(iolibtag); i++) {
		// put both tags as upvalues for these functions
		lua_pushnumber(iotag);
		lua_pushnumber(closedtag);
		lua_pushCclosure(iolibtag[i].func, 2);
		lua_setglobal(iolibtag[i].name);
	}

	if (luaIsEmi)
		return;

	LuaFile* f;

	if (g_grim->isRemastered()) {
		LuaFile *input = new LuaFile();
		LuaFile *output = new LuaFile();

		input->_stdin = true;
		output->_stdout = true;
		stderrFile = new LuaFile();
		stderrFile->_stderr = true;

		(*files)[kRemasteredStdinId] = input;
		(*files)[kRemasteredStdoutId] = output;
		(*files)[kRemasteredStderrId] = stderrFile;

		setfile(kRemasteredStdinId, FINPUT, iotag);
		setfile(kRemasteredStdoutId, FOUTPUT, iotag);
		setfile(kRemasteredStdinId, "_STDIN", iotag);
		setfile(kRemasteredStdoutId, "_STDOUT", iotag);
		setfile(kRemasteredStderrId, "_STDERR", iotag);

		return;
	}

	f = new LuaFile();
	f->_stdin = true;
	setfile(addfile(f), FINPUT, iotag);

	f = new LuaFile();
	f->_stdout = true;
	setfile(addfile(f), FOUTPUT, iotag);

	f = new LuaFile();
	f->_stdin = true;
	setfile(addfile(f), "_STDIN", iotag);

	f = new LuaFile();
	f->_stdout = true;
	setfile(addfile(f), "_STDOUT", iotag);

	stderrFile = new LuaFile();
	stderrFile->_stderr = true;
	setfile(addfile(stderrFile), "_STDERR", iotag);
}

void lua_iolibrestored() {
	static const char *const names[] = { "_INPUT", "_OUTPUT", "_STDIN", "_STDOUT", "_STDERR" };
	static const int32 ids[] = { kRemasteredStdinId, kRemasteredStdoutId, kRemasteredStdinId, kRemasteredStdoutId, kRemasteredStderrId };

	if (luaIsEmi || !g_grim->isRemastered())
		return;

	for (uint32 i = 0; i < ARRAYSIZE(names); i++) {
		lua_Object object = lua_getglobal(names[i]);

		if (!lua_isuserdata(object))
			continue;

		int32 id = lua_getuserdata(object);

		if (id < 1 || id > kRemasteredLegacyFileIds)
			continue;

		setfile(ids[i], names[i], lua_tag(object));
	}
}

void lua_iolibopen() {
	files = new Common::HashMap<int32, LuaFile *>();

	if (g_grim->isRemastered())
		lastFileId = kRemasteredLegacyFileIds;

	if (luaIsEmi)
		luaL_openlib(iolibEmi, ARRAYSIZE(iolibEmi));
	else
		luaL_openlib(iolib, ARRAYSIZE(iolib));

	luaL_addlibtolist(iolibtag, ARRAYSIZE(iolibtag));
	openwithtags();
	lua_pushcfunction(errorfb);
	lua_seterrormethod();
}

void lua_iolibclose() {
	for (Common::HashMap<int32, LuaFile *>::iterator it = files->begin(); it != files->end(); ++it) {
		delete it->_value;
	}
	delete files;
	files = nullptr;
	stderrFile = nullptr;
	lastFileId = 0;
}

} // end of namespace Grim
