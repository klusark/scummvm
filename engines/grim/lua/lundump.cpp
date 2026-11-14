/*
** load bytecodes from files
** See Copyright Notice in lua.h
*/

#include "common/endian.h"
#include "common/textconsole.h"

#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/lfunc.h"
#include "engines/grim/lua/lmem.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lstring.h"
#include "engines/grim/lua/lundump.h"

namespace Grim {

struct NumberFormat {
	bool swapped;
	bool isFloat;
};

static float conv_float(const byte *data) {
	float f;
	byte *fdata = (byte *)(&f);
	fdata[0] = data[3];
	fdata[1] = data[2];
	fdata[2] = data[1];
	fdata[3] = data[0];
	return f;
}

static void unexpectedEOZ(ZIO *Z) {
	if (luaIsRemastered)
		luaL_verror("unexpected end of binary file %s", zname(Z));
	luaL_verror("unexpected end of file in %s", zname(Z));
}

static int32 ezgetc(ZIO *Z) {
	int32 c = zgetc(Z);
	if (c == EOZ)
		unexpectedEOZ(Z);
	return c;
}

static void ezread(ZIO *Z, void *b, int32 n) {
	int32 r = zread(Z, b, n);
	if (r)
		unexpectedEOZ(Z);
}

static uint16 LoadWord(ZIO *Z) {
	uint16 hi = ezgetc(Z);
	uint16 lo = ezgetc(Z);
	return (hi << 8) | lo;
}

static void *LoadBlock(int size, ZIO *Z) {
	void *b = luaM_malloc(size);
	ezread(Z, b, size);
	return b;
}

static uint32 LoadSize(ZIO *Z) {
	uint32 hi = LoadWord(Z);
	uint32 lo = LoadWord(Z);
	return (hi << 16) | lo;
}

static float LoadFloat(ZIO *Z, const NumberFormat &format) {
	if (!luaIsRemastered) {
		uint32 l = LoadSize(Z);
		return conv_float((const byte *)&l);
	}

	if (format.isFloat) {
		byte data[4];
		ezread(Z, data, 4);
		uint32 bits = format.swapped ? READ_BE_UINT32(data) : READ_LE_UINT32(data);
		float value;
		memcpy(&value, &bits, sizeof(value));
		return value;
	}

	byte data[8];
	ezread(Z, data, 8);
	uint64 bits = format.swapped ? READ_BE_UINT64(data) : READ_LE_UINT64(data);
	double wide;
	memcpy(&wide, &bits, sizeof(wide));

	if (wide >= 3.4028235677973366e38 || wide <= -3.4028235677973366e38) {
		warning("LoadFloat(): the number does not fit a float");
		uint32 infinity = (wide > 0.0) ? 0x7F800000 : 0xFF800000;
		float value;
		memcpy(&value, &infinity, sizeof(value));
		return value;
	}

	return (float)wide;
}

static TaggedString *LoadTString(ZIO *Z) {
	int32 size = LoadWord(Z);
	int32 i;

	if (size == 0)
		return nullptr;
	else {
		char *s = luaL_openspace(size);
		ezread(Z, s, size);
		for (i = 0; i < size; i++)
			s[i] ^= 0xff;
		return luaS_new(s);
	}
}

static void LoadLocals(TProtoFunc *tf, ZIO *Z) {
	int32 i, n = LoadWord(Z);
	if (n == 0)
		return;
	tf->locvars = luaM_newvector(n + 1, LocVar);
	for (i = 0; i < n; i++) {
		tf->locvars[i].line = LoadWord(Z);
		tf->locvars[i].varname = LoadTString(Z);
	}
	tf->locvars[i].line = -1; // flag end of vector
	tf->locvars[i].varname = nullptr;
}

static void LoadConstants(TProtoFunc *tf, ZIO *Z, const NumberFormat &format) {
	int32 i, n = LoadWord(Z);
	tf->nconsts = n;
	if (n == 0)
		return;
	tf->consts = luaM_newvector(n, TObject);
	for (i = 0; i < n; i++) {
		TObject *o = tf->consts + i;
		int c = ezgetc(Z);
		switch (c) {
		case ID_NUM:
			ttype(o) = LUA_T_NUMBER;
			nvalue(o) = LoadFloat(Z, format);
			break;
		case ID_STR:
			ttype(o) = LUA_T_STRING;
			tsvalue(o) = LoadTString(Z);
			break;
		case ID_FUN:
			ttype(o) = LUA_T_PROTO;
			tfvalue(o) = nullptr;
		default:
			break;
		}
	}
}

static void LoadFunctions(TProtoFunc *tf, ZIO *Z, const NumberFormat &format);

static TProtoFunc *LoadFunction(ZIO *Z, const NumberFormat &format) {
	TProtoFunc *tf = luaF_newproto();
	tf->lineDefined = LoadWord(Z);
	tf->fileName = LoadTString(Z);
	tf->code = (byte *)LoadBlock(LoadSize(Z), Z);
	LoadConstants(tf, Z, format);
	LoadLocals(tf, Z);
	LoadFunctions(tf, Z, format);

	return tf;
}

static void LoadFunctions(TProtoFunc *tf, ZIO *Z, const NumberFormat &format) {
	// The end of the input after the last function is not an error
	while ((luaIsRemastered ? zgetc(Z) : ezgetc(Z)) == ID_FUNCTION) {
		int32 i = LoadWord(Z);
		TProtoFunc *t = LoadFunction(Z, format);
		TObject *o = tf->consts + i;
		tfvalue(o) = t;
	}
}

static void LoadSignature(ZIO *Z) {
	const char *s = SIGNATURE;

	while (*s && ezgetc(Z) == *s)
		++s;
	if (*s) {
		if (luaIsRemastered)
			luaL_verror("bad signature in binary file %s", zname(Z));
		luaL_verror("bad signature in %s", zname(Z));
	}
}

static void LoadRemasteredNumberFormat(ZIO *Z, int32 sizeofR, NumberFormat *format) {
	if (sizeofR == 4) {
		byte data[4];
		ezread(Z, data, 4);
		uint32 bits = READ_LE_UINT32(data);
		float testNumber = (float)TEST_FLOAT;
		float loaded;
		memcpy(&loaded, &bits, sizeof(loaded));

		if (testNumber != loaded) {
			bits = READ_BE_UINT32(data);
			memcpy(&loaded, &bits, sizeof(loaded));
			if (testNumber != loaded)
				luaL_verror("unknown float representation in binary file %s", zname(Z));
			format->swapped = true;
		}
		format->isFloat = true;
	} else if (sizeofR == 8) {
		byte data[8];
		ezread(Z, data, 8);
		uint64 bits = READ_LE_UINT64(data);
		double testNumber = (double)(float)TEST_FLOAT;
		double loaded;
		memcpy(&loaded, &bits, sizeof(loaded));

		if (testNumber != loaded) {
			bits = READ_BE_UINT64(data);
			memcpy(&loaded, &bits, sizeof(loaded));
			if (testNumber != loaded)
				luaL_verror("unknown float representation in binary file %s", zname(Z));
			format->swapped = true;
		}
		format->isFloat = false;
	} else {
		luaL_verror("floats in binary file %s have %d bytes; expected %d (float) or %d (double)", zname(Z), sizeofR, 4, 8);
	}
}

static void LoadHeader(ZIO *Z, NumberFormat *format) {
	int32 version, sizeofR;

	LoadSignature(Z);
	version = ezgetc(Z);
	if (version > VERSION) {
		if (luaIsRemastered)
			luaL_verror("binary file %s too new: version=0x%02x; expected at most 0x%02x", zname(Z), version, VERSION);
		luaL_verror("%s too new: version=0x%02x; expected at most 0x%02x", zname(Z), version, VERSION);
	}
	if (version < VERSION) { // check last major change
		if (luaIsRemastered)
			luaL_verror("binary file %s too old: version=0x%02x; expected at least 0x%02x", zname(Z), version, VERSION);
		luaL_verror("%s too old: version=0x%02x; expected at least 0x%02x", zname(Z), version, VERSION);
	}
	sizeofR = ezgetc(Z); // test number representation

	if (luaIsRemastered) {
		LoadRemasteredNumberFormat(Z, sizeofR, format);
		return;
	}

	if (sizeofR != sizeof(float))
		luaL_verror("number expected float in %s", zname(Z));
	ezgetc(Z);
	ezgetc(Z);
	ezgetc(Z);
	ezgetc(Z);
}

static TProtoFunc *LoadChunk(ZIO *Z) {
	NumberFormat format;
	format.swapped = false;
	format.isFloat = true;
	LoadHeader(Z, &format);
	return LoadFunction(Z, format);
}

/*
** load one chunk from a file or buffer
** return main if ok and NULL at EOF
*/
TProtoFunc *luaU_undump1(ZIO *Z) {
	int32 c = zgetc(Z);
	if (c == ID_CHUNK)
		return LoadChunk(Z);
	else if (c != EOZ) {
		if (luaIsRemastered)
			luaL_verror("%s is not a lua binary file", zname(Z));
		luaL_verror("%s is not a Lua binary file", zname(Z));
	}
	return nullptr;
}

} // end of namespace Grim
