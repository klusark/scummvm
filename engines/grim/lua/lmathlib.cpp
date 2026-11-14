/*
** Lua standard mathematical library
** See Copyright Notice in lua.h
*/

#define FORBIDDEN_SYMBOL_EXCEPTION_rand
#define FORBIDDEN_SYMBOL_EXCEPTION_srand

#include "common/scummsys.h"
#include "common/textconsole.h"

#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/lstate.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/lua/lualib.h"

namespace Grim {

#define FROMRAD(a) ((a) * (180.0 / (float)M_PI))
#define TORAD(a)    ((a) * ((float)M_PI / 180.0))

#define REMASTERED_FROMRAD 57.29577951308232
#define REMASTERED_TORAD   0.0174532925199433

static void math_abs() {
	float d = luaL_check_number(1);
	if (d < 0)
		d = -d;
	lua_pushnumber(d);
}

static void math_sin() {
	if (luaIsRemastered)
		lua_pushnumber(sin((double)luaL_check_number(1) * REMASTERED_TORAD));
	else
		lua_pushnumber(sin(TORAD(luaL_check_number(1))));
}

static void math_cos() {
	if (luaIsRemastered)
		lua_pushnumber(cos((double)luaL_check_number(1) * REMASTERED_TORAD));
	else
		lua_pushnumber(cos(TORAD(luaL_check_number(1))));
}

static void math_tan() {
	if (luaIsRemastered)
		lua_pushnumber(tan((double)luaL_check_number(1) * REMASTERED_TORAD));
	else
		lua_pushnumber(tan(TORAD(luaL_check_number(1))));
}

static void math_asin() {
	if (luaIsRemastered)
		lua_pushnumber(asin((double)luaL_check_number(1)) * REMASTERED_FROMRAD);
	else
		lua_pushnumber(FROMRAD(asin(luaL_check_number(1))));
}

static void math_acos() {
	if (luaIsRemastered)
		lua_pushnumber(acos((double)luaL_check_number(1)) * REMASTERED_FROMRAD);
	else
		lua_pushnumber(FROMRAD(acos(luaL_check_number(1))));
}

static void math_atan() {
	if (luaIsRemastered)
		lua_pushnumber(atan((double)luaL_check_number(1)) * REMASTERED_FROMRAD);
	else
		lua_pushnumber(FROMRAD(atan(luaL_check_number(1))));
}

static void math_atan2() {
	if (luaIsRemastered) {
		double x = luaL_check_number(2);
		double y = luaL_check_number(1);

		lua_pushnumber(atan2(y, x) * REMASTERED_FROMRAD);
	} else {
		lua_pushnumber(FROMRAD(atan2(luaL_check_number(1), luaL_check_number(2))));
	}
}

static void math_ceil() {
	if (luaIsRemastered)
		lua_pushnumber(ceil((double)luaL_check_number(1)));
	else
		lua_pushnumber(ceil(luaL_check_number(1)));
}

static void math_floor() {
	if (luaIsRemastered)
		lua_pushnumber(floor((double)luaL_check_number(1)));
	else
		lua_pushnumber(floor(luaL_check_number(1)));
}

static void math_mod() {
	if (luaIsRemastered) {
		double y = luaL_check_number(2);
		double x = luaL_check_number(1);

		lua_pushnumber(fmod(x, y));
	} else {
		lua_pushnumber(fmod(luaL_check_number(1), luaL_check_number(2)));
	}
}

static void math_sqrt() {
	if (luaIsRemastered)
		lua_pushnumber(sqrt((double)luaL_check_number(1)));
	else
		lua_pushnumber(sqrt(luaL_check_number(1)));
}

static void math_pow() {
	if (luaIsRemastered) {
		double y = luaL_check_number(2);
		double x = luaL_check_number(1);

		lua_pushnumber(pow(x, y));
	} else {
		lua_pushnumber(pow(luaL_check_number(1), luaL_check_number(2)));
	}
}

static void math_log() {
	if (luaIsRemastered)
		lua_pushnumber(log((double)luaL_check_number(1)));
	else
		lua_pushnumber(log(luaL_check_number(1)));
}

static void math_log10() {
	if (luaIsRemastered)
		lua_pushnumber(log10((double)luaL_check_number(1)));
	else
		lua_pushnumber(log10(luaL_check_number(1)));
}

static void math_exp() {
	if (luaIsRemastered)
		lua_pushnumber(exp((double)luaL_check_number(1)));
	else
		lua_pushnumber(exp(luaL_check_number(1)));
}

static void math_deg() {
	if (luaIsRemastered)
		lua_pushnumber((double)luaL_check_number(1) * REMASTERED_FROMRAD);
	else
		lua_pushnumber(luaL_check_number(1) * (180.0 / (float)M_PI));
}

static void math_rad() {
	if (luaIsRemastered)
		lua_pushnumber((double)luaL_check_number(1) * REMASTERED_TORAD);
	else
		lua_pushnumber(luaL_check_number(1) * ((float)M_PI / 180.0));
}

static void math_min() {
	int32 i = 1;
	float dmin = luaL_check_number(i);
	while (lua_getparam(++i) != LUA_NOOBJECT) {
		float d = luaL_check_number(i);
		if (d < dmin)
			dmin = d;
	}
	lua_pushnumber(dmin);
}

static void math_max() {
	int32 i = 1;
	float dmax = luaL_check_number(i);
	while (lua_getparam(++i) != LUA_NOOBJECT) {
		float d = luaL_check_number(i);
		if (d > dmax)
			dmax = d;
	}
	lua_pushnumber(dmax);
}

static void math_random() {
	float r;

	if (luaIsRemastered)
		r = (float)((double)(rand() % 0x7FFF) / 32767.0);
	else if (luaIsEmi)
		r = (float)((double)(rand() % 0x7FFF) * (1.0 / 32767.0));
	else
		r = (float)(rand() % RAND_MAX) / (float)RAND_MAX;

	float l = luaL_opt_number(1, 0);
	if (l == 0)
		lua_pushnumber(r);
	else if (luaIsRemastered) {
		double product = (double)r * (double)l;
		int32 truncated = -2147483647 - 1;

		if (product > -2147483649.0 && product < 2147483648.0)
			truncated = (int32)product;
		else
			warning("math_random(): the range is out of the limits of an integer");

		lua_pushnumber((float)(int32)((uint32)truncated + 1));
	} else if (luaIsEmi)
		lua_pushnumber((float)((int32)((int64)((double)l * (double)r) + 1)));
	else
		lua_pushnumber((int32)(r * l) + 1);
}

static void math_randomseed() {
	if (luaIsRemastered) {
		double seed = luaL_check_number(1);

		if (seed >= -9223372036854775808.0 && seed < 9223372036854775808.0) {
			srand((unsigned int)(int64)seed);
		} else {
			warning("math_randomseed(): the seed is out of the limits of an integer");
			srand(0);
		}
	} else {
		srand((unsigned int)luaL_check_number(1));
	}
}

static struct luaL_reg mathlib[] = {
	{ "abs",        math_abs},
	{ "sin",        math_sin},
	{ "cos",        math_cos},
	{ "tan",        math_tan},
	{ "asin",       math_asin},
	{ "acos",       math_acos},
	{ "atan",       math_atan},
	{ "atan2",      math_atan2},
	{ "ceil",       math_ceil},
	{ "floor",      math_floor},
	{ "mod",        math_mod},
	{ "sqrt",       math_sqrt},
	{ "min",        math_min},
	{ "max",        math_max},
	{ "log",        math_log},
	{ "log10",      math_log10},
	{ "exp",        math_exp},
	{ "deg",        math_deg},
	{ "rad",        math_rad},
	{ "random",     math_random},
	{ "randomseed", math_randomseed}
};

static luaL_reg powFunc[] = {
	{ "math_pow",   math_pow }
};

/*
** Open math library
*/
void lua_mathlibopen() {
	luaL_openlib(mathlib, ARRAYSIZE(mathlib));
	luaL_addlibtolist(powFunc, ARRAYSIZE(powFunc));
	lua_pushstring("deg");
	lua_setglobal("_TRIGMODE");
	lua_pushcfunction(math_pow);
	lua_pushnumber(0);  // to get its tag
	lua_settagmethod(lua_tag(lua_pop()), "pow");
	lua_pushnumber((float)M_PI);
	lua_setglobal("PI");
}

} // end of namespace Grim
