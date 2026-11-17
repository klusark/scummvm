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

#include "common/endian.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/ustr.h"

#include "gui/message.h"

#include "graphics/surface.h"
#include "graphics/renderer.h"

#include "math/matrix3.h"

#include "engines/grim/debug.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/grim.h"
#include "engines/grim/text/blasttext.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/legacy_savegames/legacy_savegames.h"
#include "engines/grim/res.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/chore/cos.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/sputobjectstate.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_math.h"
#include "engines/grim/stdcontrol.h"
#include "engines/grim/text/text.h"
#include "engines/grim/walkplane.h"
#include "engines/grim/wuregistry.h"
#include "engines/grim/lua/lauxlib.h"
#include "engines/grim/lua/luadebug.h"
#include "engines/grim/lua/lualib.h"

namespace Grim {

int luaFtol(float value) {
	double number = value;

	if (!(number > -9223372036854775809.0 && number < 9223372036854775808.0)) {
		warning("luaFtol(): value out of range");
		return 0;
	}

	int64 result = (int64)number;

	if (result < -2147483648LL || result > 2147483647LL)
		warning("luaFtol(): value out of range");

	return (int)(uint32)result;
}

int luaFtolSse(float value) {
	if (!g_grim->isRemastered())
		return luaFtol(value);

	double number = value;

	if (!(number > -2147483649.0 && number < 2147483648.0))
		return (int)0x80000000;

	return (int)number;
}

byte clamp_color(int c) {
	if (c < 0)
		return 0;
	else if (c > 255)
		return 255;
	else
		return c;
}

Common::String getLuaMessage() {
	lua_Object obj = lua_getparam(1);
	if (lua_isnil(obj))
		return "(nil)";
	if (!lua_isstring(obj))
		return {};
	Common::String msg(lua_getstring(obj));
	// Our print functions will handle the newline
	if (msg[msg.size() - 1] == '\n')
		msg.deleteLastChar();
	return msg;
}

int32 luaA_passresults();

void Lua_V1::new_dofile() {
	const char *fname_str = luaL_check_string(1);
	if (dofile(fname_str) == 0)
		if (luaA_passresults() == 0)
			lua_pushuserdata(0);
}

// Debugging message functions

void Lua_V1::PrintDebug() {
	if (!Debug::isChannelEnabled(Debug::Scripts))
		return;
	Common::String msg("Debug: ");
	msg += getLuaMessage();
	debug("%s", msg.c_str());
}

void Lua_V1::PrintError() {
	if (!Debug::isChannelEnabled(Debug::Scripts))
		return;
	Common::String msg("Error: ");
	msg += getLuaMessage();
	warning("%s", msg.c_str());
}

void Lua_V1::PrintWarning() {
	if (!Debug::isChannelEnabled(Debug::Scripts))
		return;
	Common::String msg("Warning: ");
	msg += getLuaMessage();
	warning( "%s", msg.c_str());
}

void Lua_V1::FunctionName() {
	const char *name = nullptr;
	const char *filename = nullptr;
	int32 line = 0;
	char buf[256];
	lua_Object funcObj = lua_getparam(1);

	if (!lua_isfunction(funcObj))
		return;

	lua_funcinfo(funcObj, &filename, &line);
	char funcType = *lua_getobjname(funcObj, &name);

	const char *typeName = "unnamed function";
	const char *funcName = "<unknown>";

	if (funcType == 'g') {
		typeName = "function";
		funcName = name;
	} else if (funcType == 't') {
		typeName = "tag method";
		funcName = name;
	}

	Common::sprintf_s(buf, "%s '%s' (%s:%d)\n", typeName, funcName, filename, (int)line);
	lua_pushstring(buf);
}

void Lua_V1::CheckForFile() {
	lua_Object obj = lua_getparam(1);

	if (!lua_isstring(obj))
		return;

	Common::SeekableReadStream *file = g_grim->openFile(lua_getstring(obj));

	if (file) {
		delete file;
		lua_pushnumber(1.0f);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::MakeColor() {
	lua_Object rObj = lua_getparam(1);
	lua_Object gObj = lua_getparam(2);
	lua_Object bObj = lua_getparam(3);
	lua_Object aObj = lua_getparam(4);
	uint r, g, b, a;

	if (!lua_isnumber(rObj))
		r = 0;
	else
		r = clamp_color((int)lua_getnumber(rObj));

	if (!lua_isnumber(gObj))
		g = 0;
	else
		g = clamp_color((int)lua_getnumber(gObj));

	if (!lua_isnumber(bObj))
		b = 0;
	else
		b = clamp_color((int)lua_getnumber(bObj));

	if (!lua_isnumber(aObj))
		a = 0;
	else
		a = clamp_color((int)lua_getnumber(aObj));

	lua_pushusertag((int32)(r | (g << 8) | (b << 16) | (a << 24)), MKTAG('R','L','O','C'));
}

void Lua_V1::GetColorComponents() {
	lua_Object colorObj = lua_getparam(1);
	uint32 color = lua_getuserdata(colorObj);
	lua_pushnumber(color & 0xFF);
	lua_pushnumber((color >> 8) & 0xFF);
	lua_pushnumber((color >> 16) & 0xFF);
}

void Lua_V1::ReadRegistryValue() {
	lua_Object keyObj = lua_getparam(1);

	if (!lua_isstring(keyObj)) {
		lua_pushnil();
		return;
	}
	const char *key = lua_getstring(keyObj);
	WuRegistry *wuRegistry = g_grim->_wuRegistry;

	uint type = wuRegistry->queryType(key);
	if (type == WuRegistry::kValueBinary) {
		lua_pushnumber(wuRegistry->getFloat(key, 0.0f));
	} else if (type == WuRegistry::kValueString) {
		char value[256];

		value[0] = '\0';
		wuRegistry->getStr(key, value, 255, "");
		lua_pushstring(value);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::WriteRegistryValue() {
	lua_Object keyObj = lua_getparam(1);

	if (!lua_isstring(keyObj))
		return;

	const char *key = lua_getstring(keyObj);
	lua_Object valObj = lua_getparam(2);
	WuRegistry *wuRegistry = g_grim->_wuRegistry;

	if (g_grim->isRemastered()) {
		if (lua_isnumber(valObj))
			wuRegistry->saveFloat(key, lua_getnumber(valObj));
		else if (lua_isstring(valObj))
			wuRegistry->saveStr(key, lua_getstring(valObj));

		return;
	}

	if (lua_isstring(valObj))
		wuRegistry->saveStr(key, lua_getstring(valObj));
	else if (lua_isnumber(valObj))
		wuRegistry->saveFloat(key, lua_getnumber(valObj));
}

void Lua_V1::GetAngleBetweenVectors() {
	lua_Object vec1Obj = lua_getparam(1);
	lua_Object vec2Obj = lua_getparam(2);

	if (!lua_istable(vec1Obj) || !lua_istable(vec2Obj)) {
		lua_pushnil();
		return;
	}

	RdVector3 vec1;
	RdVector3 vec2;

	lua_pushobject(vec1Obj);
	lua_pushstring("x");
	vec1[0] = lua_getnumber(lua_gettable());
	lua_pushobject(vec1Obj);
	lua_pushstring("y");
	vec1[1] = lua_getnumber(lua_gettable());
	lua_pushobject(vec1Obj);
	lua_pushstring("z");
	vec1[2] = lua_getnumber(lua_gettable());
	lua_pushobject(vec2Obj);
	lua_pushstring("x");
	vec2[0] = lua_getnumber(lua_gettable());
	lua_pushobject(vec2Obj);
	lua_pushstring("y");
	vec2[1] = lua_getnumber(lua_gettable());
	lua_pushobject(vec2Obj);
	lua_pushstring("z");
	vec2[2] = lua_getnumber(lua_gettable());

	g_grim->_rd->normalize3Acc(vec1);
	g_grim->_rd->normalize3Acc(vec2);

	float dot = 0.0f;

	if (g_grim->isRemastered()) {
		float productY = (float)((double)vec2[1] * vec1[1]);
		float productX = (float)((double)vec2[0] * vec1[0]);
		float productZ = (float)((double)vec2[2] * vec1[2]);

		dot = (float)(((double)productY + productX) + productZ);

		float absDot = dot;

		if (absDot < 0.0f)
			absDot = -absDot;

		float delta = (float)((double)absDot - 1.0);

		if (delta > -0.001f && delta < 0.001f) {
			lua_pushnumber(0.0f);
			return;
		}
	} else {
		dot = (float)(((double)vec2[0] * vec1[0] + (double)vec2[1] * vec1[1]) + (double)vec2[2] * vec1[2]);
	}

	lua_pushnumber((float)(90.0f - (double)g_grim->_stdMath->arcSin3(dot)));
}

void Lua_V1::GetAngleBetweenVectorsDemo() {
	lua_Object vec1Obj = lua_getparam(1);
	lua_Object vec2Obj = lua_getparam(2);

	if (!lua_istable(vec1Obj) || !lua_istable(vec2Obj)) {
		warning("GetAngleBetweenVectors: Both arguments must be tables in the format { x, y, z }.");
		lua_pushnil();
		return;
	}

	lua_pushobject(vec1Obj);
	lua_pushstring("x");
	float x1 = lua_getnumber(lua_gettable());
	lua_pushobject(vec1Obj);
	lua_pushstring("y");
	float y1 = lua_getnumber(lua_gettable());
	lua_pushobject(vec1Obj);
	lua_pushstring("z");
	float z1 = lua_getnumber(lua_gettable());
	lua_pushobject(vec2Obj);
	lua_pushstring("x");
	float x2 = lua_getnumber(lua_gettable());
	lua_pushobject(vec2Obj);
	lua_pushstring("y");
	float y2 = lua_getnumber(lua_gettable());
	lua_pushobject(vec2Obj);
	lua_pushstring("z");
	double z2 = lua_getnumber(lua_gettable());

	float dot = (float)(((double)x2 * x1 + (double)y2 * y1) + z2 * z1);

	lua_pushnumber((float)(90.0 - (double)g_grim->_stdMath->arcSin3(dot)));
}

void Lua_V1::Is3DHardwareEnabled() {
	if (g_grim->isHardware3D())
		lua_pushnumber(1.0);
	else
		lua_pushnil();
}

void Lua_V1::SetHardwareState() {
	lua_Object stateObj = lua_getparam(1);
	int curDevice = g_grim->getCurVideoDeviceIndex();
	int cur3DDevice = g_grim->getCur3DDeviceIndex();
	int want3D = lua_isnil(stateObj) == 0;

	g_grim->chooseDefaultDisplay(0, want3D, &g_grim->_pendingDevice, &g_grim->_pendingMode, &g_grim->_pending3DDevice);

	if (g_grim->_pendingDevice != curDevice || g_grim->_pending3DDevice != cur3DDevice)
		g_grim->_displayChangePending = 1;
}

void Lua_V1::SetVideoDevices() {
	lua_Object deviceObj = lua_getparam(1);
	lua_Object modeObj = lua_getparam(2);
	int curDevice = g_grim->getCurVideoDeviceIndex();
	int cur3DDevice = g_grim->getCur3DDeviceIndex();

	g_grim->_pendingDevice = luaFtol(lua_getnumber(deviceObj));

	int numDevices = (int)g_grim->_stdDisplay->getNumDevices();

	if (g_grim->_pendingDevice >= 0 && g_grim->_pendingDevice < numDevices) {
		GrimVideoDevice *videoDevice = &g_grim->_videoDevices[g_grim->_pendingDevice];

		g_grim->_pendingMode = g_grim->_stdDisplay->findClosestMode(&g_grim->_videoDeviceConfig, videoDevice->modes, videoDevice->numModes);
	} else {
		g_grim->_pendingMode = 0;
	}

	int mode3D = luaFtol(lua_getnumber(modeObj));

	g_grim->_pending3DDevice = mode3D;

	if (g_grim->_pendingDevice >= 0 && g_grim->_pendingDevice < numDevices &&
		g_grim->_stdDisplay->getDevices()[g_grim->_pendingDevice].usable &&
		mode3D >= -1 && mode3D < (int)g_grim->_videoDevices[g_grim->_pendingDevice].num3DDevices &&
		(g_grim->_pendingDevice != curDevice || mode3D != cur3DDevice))
		g_grim->_displayChangePending = 1;
}

void Lua_V1::GetVideoDevices() {
	lua_pushnumber((float)g_grim->getCurVideoDeviceIndex());
	lua_pushnumber((float)g_grim->getCur3DDeviceIndex());
}

void Lua_V1::EnumerateVideoDevices() {
	lua_Object result = lua_createtable();
	int firstDevice = g_grim->_enumerateVideo == 0 ? 1 : 0;
	int numDevices = (int)g_grim->_stdDisplay->getNumDevices();
	StdDisplayDevice *devices = g_grim->_stdDisplay->getDevices();
	int numListed = 0;

	for (int i = firstDevice; i < numDevices; i++) {
		if (devices[i].type == 1 && !devices[i].usable)
			continue;

		lua_pushobject(result);
		lua_pushnumber((float)i);
		lua_pushstring(devices[i].description);
		lua_settable();
		numListed++;
	}

	// Without hardware devices the options menu would be empty
	if (!numListed && numDevices > 0) {
		lua_pushobject(result);
		lua_pushnumber(0.0f);
		lua_pushstring(devices[0].description);
		lua_settable();
	}

	lua_pushobject(result);
}

void Lua_V1::Enumerate3DDevices() {
	lua_Object numObj = lua_getparam(1);
	if (!lua_isnumber(numObj))
		return;

	int deviceIndex = luaFtol(lua_getnumber(numObj));
	lua_Object result = lua_createtable();

	if (deviceIndex < 0 || deviceIndex >= (int)g_grim->_stdDisplay->getNumDevices()) {
		warning("Lua_V1::Enumerate3DDevices(): invalid display device %d", deviceIndex);
		lua_pushobject(result);
		return;
	}

	StdDisplayDevice *device = &g_grim->_stdDisplay->getDevices()[deviceIndex];
	GrimVideoDevice *videoDevice = &g_grim->_videoDevices[deviceIndex];
	int list;

	if (!device->primary && device->b3D) {
		list = 1;
	} else {
		lua_pushobject(result);
		lua_pushnumber(-1.0);
		lua_pushstring("/engn003/Software");
		lua_settable();
		list = device->b3D;
	}

	if (list) {
		for (uint i = 0; i < videoDevice->num3DDevices; i++) {
			if (!videoDevice->a3DDevices[i].hardware)
				continue;

			lua_pushobject(result);
			lua_pushnumber((float)i);
			lua_pushstring(videoDevice->a3DDevices[i].description);
			lua_settable();
		}
	}

	lua_pushobject(result);
}

/* RotateVector takes a vector and rotates it around
 * the point (0,0,0) by the requested number of degrees.
 * This function is used to calculate the locations for
 * getting on and off of the Bone Wagon and for going up
 * and down the slide with the chain at the end of the world.
 */
void Lua_V1::RotateVector() {
	lua_Object vecObj = lua_getparam(1);
	lua_Object rotObj = lua_getparam(2);

	if (!lua_istable(vecObj) || !lua_istable(rotObj)) {
		lua_pushnil();
		return;
	}

	RdVector3 vec;
	RdVector3 rot;

	lua_pushobject(vecObj);
	lua_pushstring("x");
	vec[0] = lua_getnumber(lua_gettable());
	lua_pushobject(vecObj);
	lua_pushstring("y");
	vec[1] = lua_getnumber(lua_gettable());
	lua_pushobject(vecObj);
	lua_pushstring("z");
	vec[2] = lua_getnumber(lua_gettable());

	lua_pushobject(rotObj);
	lua_pushstring("x");
	rot[0] = lua_getnumber(lua_gettable());
	lua_pushobject(rotObj);
	lua_pushstring("y");
	rot[1] = lua_getnumber(lua_gettable());
	lua_pushobject(rotObj);
	lua_pushstring("z");
	rot[2] = lua_getnumber(lua_gettable());

	g_grim->_rd->rotate3Acc(vec, rot);

	lua_Object resObj = lua_createtable();
	lua_pushobject(resObj);
	lua_pushstring("x");
	lua_pushnumber(vec[0]);
	lua_settable();
	lua_pushobject(resObj);
	lua_pushstring("y");
	lua_pushnumber(vec[1]);
	lua_settable();
	lua_pushobject(resObj);
	lua_pushstring("z");
	lua_pushnumber(vec[2]);
	lua_settable();

	lua_pushobject(resObj);
}

void Lua_V1::WorldToScreen() {
	lua_Object worldX = lua_getparam(1);
	lua_Object worldY = lua_getparam(2);
	lua_Object worldZ = lua_getparam(3);
	if (!lua_isnumber(worldX) || !lua_isnumber(worldY) || !lua_isnumber(worldZ)) {
		return;
	}
	RdVector3 worldPos;
	int screen[3];

	worldPos[0] = lua_getnumber(worldX);
	worldPos[1] = lua_getnumber(worldY);
	worldPos[2] = lua_getnumber(worldZ);

	if (g_grim->_rd->_primit3.getScreenCoord(worldPos, screen)) {
		lua_pushnumber((float)screen[0]);
		lua_pushnumber((float)screen[1]);
	} else {
		lua_pushnil();
		lua_pushnil();
	}
}

void Lua_V1::FileFindDispose() {
	g_grim->_listFiles.clear();
	g_grim->_listFilesIter = nullptr;
}

void Lua_V1::FileFindNext() {
	if (g_grim->_listFilesIter == g_grim->_listFiles.end()) {
		lua_pushnil();
		Lua_V1::FileFindDispose();
	} else {
		lua_pushstring(g_grim->_listFilesIter->c_str());
		g_grim->_listFilesIter++;
	}
}

void Lua_V1::FileFindFirst() {
	lua_Object extObj = lua_getparam(1);
	if (!lua_isstring(extObj)) {
		lua_pushnil();
		return;
	}

	Lua_V1::FileFindDispose();

	const char *extension = lua_getstring(extObj);

	if (g_grim->isRemastered()) {
		lua_Object dirObj = lua_getparam(2);
		Common::String directory;

		if (!lua_isnil(dirObj))
			directory = lua_getstring(dirObj);

		Common::String pattern = extension;

		if (!strchr(extension, '.'))
			pattern = "*." + pattern;

		if (directory == ".")
			directory = ConfMan.get("path");

		Common::FSList children;
		Common::FSNode node((Common::Path(directory, Common::Path::kNativeSeparator)));

		if (!directory.empty() && node.exists() && node.isDirectory())
			node.getChildren(children, Common::FSNode::kListAll);

		for (uint i = 0; i < children.size(); i++) {
			if (children[i].getName().matchString(pattern, true))
				g_grim->_listFiles.push_back(children[i].getName());
		}

		Common::sort(g_grim->_listFiles.begin(), g_grim->_listFiles.end());
		g_grim->_listFilesIter = g_grim->_listFiles.begin();

		if (g_grim->_listFilesIter == g_grim->_listFiles.end())
			lua_pushnil();
		else
			Lua_V1::FileFindNext();

		return;
	}

	// A name without a dot is the extension of the pattern
	Common::String searchString = extension;
	if (!strchr(extension, '.'))
		searchString = "*." + searchString;

	if (searchString == "*.gsv") {
		searchString = "grim##.gsv";
	}

	// The saves are in the save manager, which is what the current folder stands for
	lua_Object dirObj = lua_getparam(2);
	if (lua_isstring(dirObj) && !strcmp(lua_getstring(dirObj), ".")) {
		Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
		g_grim->_listFiles = saveFileMan->listSavefiles(searchString);
	}

	Common::sort(g_grim->_listFiles.begin(), g_grim->_listFiles.end());
	g_grim->_listFilesIter = g_grim->_listFiles.begin();

	if (g_grim->_listFilesIter == g_grim->_listFiles.end())
		lua_pushnil();
	else
		Lua_V1::FileFindNext();
}

void Lua_V1::PerSecond() {
	lua_Object rateObj = lua_getparam(1);

	if (g_grim->isRemastered() && !lua_isnumber(rateObj))
		lua_pushnil();

	float frameTime = g_grim->_sputTime->getFrameTimeFlex();

	lua_pushnumber((float)((double)lua_getnumber(rateObj) * frameTime));
}

void Lua_V1::EnableControl() {
	lua_Object numObj = lua_getparam(1);

	if (!lua_isnumber(numObj)) {
		lua_pushnil();
		return;
	}
	g_grim->_sputControl->enableControl((uint)luaFtol(lua_getnumber(numObj)));
}

void Lua_V1::DisableControl() {
	lua_Object numObj = lua_getparam(1);

	if (!lua_isnumber(numObj)) {
		lua_pushnil();
		return;
	}
	g_grim->_sputControl->disableControl((uint)luaFtol(lua_getnumber(numObj)));
}

void Lua_V1::GetControlState() {
	lua_Object numObj = lua_getparam(1);

	if (!lua_isnumber(numObj)) {
		if (g_grim->isRemastered())
			lua_pushnil();

		return;
	}

	uint index = (uint)luaFtol(lua_getnumber(numObj));

	if (index >= (uint)g_grim->_sputControl->getNumSymbols())
		return;

	ControlSymbol *symbol = &g_grim->_sputControl->getSymbols()[index];

	if (symbol->flags & 4) {
		int pressCount = 0;
		int doubleClick = 0;

		if (g_grim->_stdControl->readKey(symbol->code, &pressCount, &doubleClick))
			lua_pushnumber(1.0f);
		else
			lua_pushnil();

		if (doubleClick)
			lua_pushnumber(1.0f);
		else
			lua_pushnil();
	} else {
		float axis = g_grim->_stdControl->readAxis(symbol->code);

		if (symbol->flags & 8)
			axis = -axis;

		lua_pushnumber(axis);
	}
}

void Lua_V1::Exit() {
	lua_Object exitObj = lua_getparam(1);

	if (lua_isnumber(exitObj))
		g_grim->requestShutdown(luaFtolSse(lua_getnumber(exitObj)));
	else
		g_grim->requestShutdown(0);
}

void Lua_V1::SetSpeechMode() {
	lua_Object modeObj = lua_getparam(1);

	if (!lua_isnumber(modeObj))
		return;

	int mode = (int)lua_getnumber(modeObj);

	if (mode >= 1 && mode <= 3)
		g_grim->_sputActor->setSpeechMode(mode);
}

void Lua_V1::GetSpeechMode() {
	int mode = g_grim->_sputActor->getSpeechMode();

	if (mode & 2) {
		if (mode & 1) {
			lua_pushnumber(3.0f);
			return;
		}
	} else if (mode & 1) {
		lua_pushnumber(1.0f);
		return;
	}

	lua_pushnumber(2.0f);
}

void Lua_V1::GetDiskFreeSpace() {
	lua_pushnumber(700.0f);
}

void Lua_V1::GetCurrentScript() {
	current_script();
}

void Lua_V1::GetSaveGameImage() {
	lua_Object nameObj = lua_getparam(1);

	if (lua_isstring(nameObj)) {
		StdVBuffer *image = g_grim->getSaveGameImage(lua_getstring(nameObj));

		if (image) {
			g_grim->_sputRender->colorCorrect((uint16 *)image->pixels, image->tex.width * image->tex.height);
			pushImage(image);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::SubmitSaveGameData() {
	lua_Object tableObj = lua_getparam(1);
	int index = 0;

	if (!g_grim->_saveWriteStream) {
		warning("Lua_V1::SubmitSaveGameData(): no save in progress");
		return;
	}

	_totSaveBytes = 0;

	for (;;) {
		lua_pushobject(tableObj);
		lua_pushnumber((float)index);
		index++;

		lua_Object entryObj = lua_gettable();

		if (lua_isnil(entryObj))
			break;

		if (!lua_isstring(entryObj)) {
			warning("Lua_V1::SubmitSaveGameData(): the table must contain only strings");
			return;
		}

		_totSaveBytes += (int)strlen(lua_getstring(entryObj)) + 5;
	}

	if (!_totSaveBytes)
		return;

	uint32 totalBytes = TO_LE_32((uint32)_totSaveBytes);

	g_grim->zwrite(&totalBytes, 4);

	index = 0;

	for (;;) {
		lua_pushobject(tableObj);
		lua_pushnumber((float)index);
		index++;

		lua_Object entryObj = lua_gettable();

		if (lua_isnil(entryObj))
			break;

		const char *str = lua_getstring(entryObj);
		int length = (int)strlen(str) + 1;
		uint32 lengthLE = TO_LE_32((uint32)length);

		g_grim->zwrite(&lengthLE, 4);
		g_grim->zwrite(str, length);
	}
}

void Lua_V1::GetSaveGameData() {
	lua_Object param = lua_getparam(1);
	if (!lua_isstring(param))
		return;
	Common::SeekableReadStream *file = g_system->getSavefileManager()->openForLoading(lua_getstring(param));

	if (!file)
		return;

	if (LegacySaveFile::isLegacySave(file)) {
		Common::Array<Common::String> strings;

		if (legacyReadSaveStrings(file, strings)) {
			lua_Object legacyResult = lua_createtable();

			for (uint i = 0; i < strings.size(); i++) {
				lua_pushobject(legacyResult);
				lua_pushnumber((float)i);
				lua_pushstring(strings[i].c_str());
				lua_settable();
			}

			lua_pushobject(legacyResult);
		}

		delete file;
		return;
	}

	lua_Object result = lua_createtable();
	char str[1024];
	int index = 0;
	int totalBytes = file->readSint32LE();

	while (totalBytes > 0) {
		int size = file->readSint32LE();

		if (file->err() || file->eos() || size < 1 || size > (int)sizeof(str)) {
			warning("Lua_V1::GetSaveGameData(): invalid string size %d", size);
			break;
		}

		if (file->read(str, size) != (uint32)size) {
			warning("Lua_V1::GetSaveGameData(): unexpected end of file");
			break;
		}

		str[size - 1] = '\0';

		lua_pushobject(result);
		lua_pushnumber((float)index);
		lua_pushstring(str);
		lua_settable();
		index++;
		totalBytes -= size + 4;
	}

	lua_pushobject(result);
	delete file;
}

void Lua_V1::Load() {
	lua_Object fileNameObj = lua_getparam(1);

	if (lua_isnil(fileNameObj))
		g_grim->loadLib(nullptr);
	else if (lua_isstring(fileNameObj))
		g_grim->loadLib(lua_getstring(fileNameObj));
}

void Lua_V1::Save() {
	lua_Object fileNameObj = lua_getparam(1);

	if (lua_isnil(fileNameObj))
		g_grim->saveLib(nullptr);
	else if (lua_isstring(fileNameObj))
		g_grim->saveLib(lua_getstring(fileNameObj));
}

void Lua_V1::Remove() {
	if (g_system->getSavefileManager()->removeSavefile(luaL_check_string(1)))
		lua_pushuserdata(0);
	else {
		lua_pushnil();
		lua_pushstring(g_system->getSavefileManager()->getErrorDesc().c_str());
	}
}

void Lua_V1::LockFont() {
	lua_Object param1 = lua_getparam(1);
	if (lua_isstring(param1)) {
		const char *fontName = lua_getstring(param1);
		void *font = nullptr;

		if (g_grim->isRemastered()) {
			ResNode *loaded = g_grim->_res->isResourceLoaded(kSputTagFont, fontName);

			if (loaded)
				font = loaded->handle;
		}

		if (!font)
			font = g_grim->_sputResource->alloc(kSputTagFont, fontName);

		if (font) {
			ResNode *node = g_grim->_res->checkAllocatedRes(font);

			if (node) {
				if (g_grim->getGameFlags() & ADGF_DEMO) {
					ResRef *ref = new ResRef();

					memset(ref, 0, sizeof(ResRef));
					g_grim->_res->setReference(ref, font);
					_lockedFontRefs.push_back(ref);
				}

				lua_pushusertag(node->serial, kSputTagFont);
				return;
			}

			warning("Lua_V1::LockFont(): font resource not found");
		}
	}

	lua_pushnil();
}

void Lua_V1::UnLockFontDemo() {
	lua_Object param1 = lua_getparam(1);

	if (!lua_isuserdata(param1) || lua_tag(param1) != kSputTagFont) {
		warning("UnLockFont: Invalid font handle!");
		return;
	}

	void *handle = g_grim->_res->handleFromSerialNumber(lua_getuserdata(param1));

	for (uint i = 0; i < _lockedFontRefs.size(); i++) {
		ResRef *ref = _lockedFontRefs[i];

		if (ref->handle == handle) {
			g_grim->_res->removeReference(ref);
			delete ref;
			_lockedFontRefs.remove_at(i);
			return;
		}
	}

	warning("UnLockFont: font handle is not locked");
}

void Lua_V1::JustLoaded() {
	if (g_grim->justLoaded())
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::EnableDebugKeys() {
	g_grim->setDebugKeys(1);

	if (_debugToggleFunc && !(g_grim->getGameFlags() & ADGF_DEMO))
		_debugToggleFunc(1);
}

void Lua_V1::FlushControls() {
	g_grim->_stdControl->flush();
}

void Lua_V1::LightMgrSetChange() {
	// TODO: LightManager (Win32 light editor) not ported
}

void Lua_V1::LightMgrStartup() {
	// TODO: LightManager (Win32 light editor) not ported
}

void Lua_V1::LightMgrShutdown() {
	// TODO: LightManager (Win32 light editor) not ported
}

void Lua_V1::SetEmergencyFont() {
	lua_Object fontObj = lua_getparam(1);
	SputText *sputText = g_grim->_sputText;

	if (lua_isstring(fontObj)) {
		if (sputText->_emergencyFont) {
			g_grim->_text->freeFont(sputText->_emergencyFont);
			sputText->_emergencyFont = nullptr;
		}

		g_grim->_blastText->load(lua_getstring(fontObj), sputText->_emergencyFont);
	} else if (lua_isnil(fontObj) && sputText->_emergencyFont) {
		g_grim->_text->freeFont(sputText->_emergencyFont);
		sputText->_emergencyFont = nullptr;
	}
}

void Lua_V1::NukeResources() {
	if (g_grim->isRemastered())
		return;

	g_grim->_sputRoom->setCurrentRoom(nullptr);
	g_grim->_sputActor->nukeCostumeStacks();
	g_grim->_res->freeAll();
	g_grim->_sputActor->clearShadowMaterial();
}

void Lua_V1::AttachToResources() {
	g_grim->_sputResource->closeResources();
	g_grim->findResources();
}

void Lua_V1::DetachFromResources() {
	g_grim->_sputSmush->stopMovie();
	g_grim->getImuse()->soundsCloseVoiceFile();
	g_grim->_sputResource->closeResources();
}

void Lua_V1::SpewStartup() {
	// nothing to implement
	// originally this opcode launch external library 'spew.dll'
}

void Lua_V1::GetMemoryUsage() {
	lua_pushnumber((float)g_grim->_res->totalAllocated());
}

void Lua_V1::GetFontDimensions() {
	if (g_grim->isRemastered()) {
		g_grim->_sputText->luaGetFontDimensions();
		return;
	}

	warning("GetFontDimensions(): the original dereferences a Lua object as a font pointer and crashes");
}

int Lua_V1::queryDialog(const char *caption, const char *text, int type) {
	if (g_grim->isRemastered())
		return 0;

	Common::U32String message(Common::String::format("%s\n%s", caption, text));
	Common::U32String defaultButton;
	Common::U32StringArray altButtons;

	if (type == 1) {
		defaultButton = Common::U32String("OK");
		altButtons.push_back(Common::U32String("Cancel"));
	} else {
		defaultButton = Common::U32String("Yes");
		altButtons.push_back(Common::U32String("No"));

		if (type == 3)
			altButtons.push_back(Common::U32String("Cancel"));
	}

	GUI::MessageDialog dialog(message, defaultButton, altButtons);
	int result = dialog.runModal();

	g_grim->clearEventQueue();

	if (type == 1)
		return result == GUI::kMessageOK ? 1 : 2;

	if (result == GUI::kMessageOK)
		return 6;

	if (result == GUI::kMessageAlt)
		return 7;

	return type == 3 ? 2 : 7;
}

bool Lua_V1::getLightIndex(lua_Object obj, int *index) {
	if (lua_isnumber(obj)) {
		*index = luaFtolSse(lua_getnumber(obj));
		return true;
	}

	if (lua_isstring(obj)) {
		*index = g_grim->_sputRoom->findLightByName(nullptr, lua_getstring(obj));
		return true;
	}

	return false;
}

void Lua_V1::EnginePause() {
	g_grim->setPause(1);
}

void Lua_V1::EngineResume() {
	g_grim->setPause(0);
}

void Lua_V1::SearchForFileOrSwapCDs() {
	lua_Object captionObj = lua_getparam(1);
	lua_Object textObj = lua_getparam(2);
	lua_Object fileObj = lua_getparam(3);

	if (!lua_isstring(captionObj) || !lua_isstring(textObj) || !lua_isstring(fileObj))
		return;

	Common::SeekableReadStream *file = g_grim->openFile(lua_getstring(fileObj));

	if (file) {
		delete file;
		lua_pushnumber(1.0f);
		lua_pushnil();
		return;
	}

	g_grim->getImuse()->imSetState(0);
	g_grim->getImuse()->imStopAllSounds();
	g_grim->getImuse()->soundsCloseVoiceFile();

	while (true) {
		if (queryDialog(lua_getstring(captionObj), lua_getstring(textObj), 1) == 2)
			break;

		g_grim->_sputResource->closeResources();
		g_grim->findResources();

		file = g_grim->openFile(lua_getstring(fileObj));

		if (file) {
			delete file;
			lua_pushnumber(1.0f);
			lua_pushnumber(1.0f);
			return;
		}

		if (g_grim->isRemastered()) {
			warning("Lua_V1::SearchForFileOrSwapCDs(): the dialog never answers, the original loops forever");
			break;
		}
	}

	lua_pushnil();
	lua_pushnil();
}

void Lua_V1::GetActorTurnRate() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	lua_pushnumber(actor->extra->turnRate);
}

void Lua_V1::SetActorOffsetYaw() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object yawObj = lua_getparam(2);

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	actor->extra->offsetYaw = lua_getnumber(yawObj);
}

void Lua_V1::IsActorLooking() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (actor->mode & 2)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::CanActorSee() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object targetObj = lua_getparam(2);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	float distance;
	int visible;

	if (lua_isnumber(targetObj)) {
		lua_Object yObj = lua_getparam(3);
		lua_Object zObj = lua_getparam(4);
		float point[3];

		point[0] = lua_getnumber(targetObj);
		point[1] = lua_isnumber(yObj) ? lua_getnumber(yObj) : 0.0f;
		point[2] = lua_isnumber(zObj) ? lua_getnumber(zObj) : 0.0f;
		visible = g_grim->_sputActor->canActorSeePoint(actor, point, &distance);
	} else if (isSputActor(targetObj)) {
		SputActor *target = getSputActor(targetObj);

		if (!target)
			return;

		visible = g_grim->_sputActor->canActorSeeActor(actor, target, &distance);
	} else if (lua_isnil(targetObj) && g_grim->_sputActor->getSelectedActor()) {
		visible = g_grim->_sputActor->canActorSeeActor(g_grim->_sputActor->getSelectedActor(), actor, &distance);
	} else {
		return;
	}

	if (visible)
		lua_pushnumber(distance);
	else
		lua_pushnil();
}

void Lua_V1::GetActorLoopingChores() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object costumeObj = lua_getparam(2);
	CostumeNode *node;

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (!findCostumeInStack(costumeObj, actor->costumeStack, &node))
		return;

	if (!node) {
		lua_pushnil();
		return;
	}

	Cos *cos = g_grim->_sputCostume->getCos();
	lua_Object choreTable = lua_createtable();
	int numChores = cos->instCountChores(node->costumeInstance);
	int loopingCount = 0;

	for (int i = 0; i < numChores; i++) {
		int status = cos->instChoreStatus(node->costumeInstance, i);

		if ((status & kCosChoreStatePlaying) && (status & kCosChoreStateLooping)) {
			lua_pushobject(choreTable);
			lua_pushnumber((float)i);
			lua_pushnumber((float)i);
			lua_settable();
			loopingCount++;
		}
	}

	lua_pushobject(choreTable);
	lua_pushstring("count");
	lua_pushnumber((float)loopingCount);
	lua_settable();

	lua_pushobject(choreTable);
}

void Lua_V1::ScreenToWorld() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj))
		return;

	float xValue = lua_getnumber(xObj);
	float yValue = lua_getnumber(yObj);
	int x;
	int y;

	if (g_grim->isRemastered()) {
		x = (xValue >= -2147483648.0f && xValue < 2147483648.0f) ? (int)xValue : INT_MIN;
		y = (yValue >= -2147483648.0f && yValue < 2147483648.0f) ? (int)yValue : INT_MIN;
	} else {
		int64 wideX = (xValue >= -9223372036854775808.0f && xValue < 9223372036854775808.0f) ? (int64)xValue : (-9223372036854775807LL - 1);
		int64 wideY = (yValue >= -9223372036854775808.0f && yValue < 9223372036854775808.0f) ? (int64)yValue : (-9223372036854775807LL - 1);
		x = (int)(uint32)((uint64)wideX & 0xFFFFFFFFu);
		y = (int)(uint32)((uint64)wideY & 0xFFFFFFFFu);
	}

	if (x < 0 || x > 639 || y < 0 || y > 479)
		return;

	float world[3];

	if (!g_grim->_sputRender->reverseProject(x, y, world)) {
		warning("Lua_V1::ScreenToWorld(): the point cannot be projected");
		world[0] = 0.0f;
		world[1] = 0.0f;
		world[2] = 0.0f;
	}

	lua_pushnumber(world[0]);
	lua_pushnumber(world[1]);
	lua_pushnumber(world[2]);
}

void Lua_V1::PrintMessage() {
	lua_Object messageObj = lua_getparam(1);
	Common::String message;

	if (lua_isnil(messageObj))
		message = "(nil)";
	else if (lua_isstring(messageObj))
		message = lua_getstring(messageObj);
	else
		return;

	if (message.size() > 1023)
		message = message.substr(0, 1023);

	if (!message.empty() && message.lastChar() == '\n')
		message.deleteLastChar();

	debug("%s", message.c_str());
}

void Lua_V1::QueryDialog() {
	lua_Object captionObj = lua_getparam(1);
	lua_Object textObj = lua_getparam(2);
	lua_Object thirdObj = lua_getparam(3);

	if (lua_isstring(captionObj) && lua_isstring(textObj)) {
		const char *caption = lua_getstring(captionObj);
		const char *text = lua_getstring(textObj);
		int result = queryDialog(caption, text, lua_isnil(thirdObj) ? 4 : 3) - 2;

		if (result == 0) {
			lua_pushnumber(0.0f);
			return;
		}

		if (result != 5) {
			lua_pushnumber(1.0f);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::SpewShutdown() {
	warning("Lua_V1::SpewShutdown(): not implemented");
}

void Lua_V1::GetPhysicalBytes() {
	lua_pushnumber((float)(int)kPhysicalRam);
}

void Lua_V1::SetMemoryUsage() {
	lua_Object limitObj = lua_getparam(1);
	lua_Object purgeObj = lua_getparam(2);
	int purgeAmount = luaFtolSse(lua_getnumber(purgeObj));
	int limit = luaFtolSse(lua_getnumber(limitObj));

	g_grim->_sputResource->setMemoryUsage(limit, purgeAmount);
}

void Lua_V1::GetCameraInterest() {
	RoomCamera *camera = g_grim->_sputRender->getCamera();

	if (!camera) {
		warning("GetCameraInterest(): no camera");
		lua_pushnil();
		return;
	}

	lua_pushnumber(camera->interest[0]);
	lua_pushnumber(camera->interest[1]);
	lua_pushnumber(camera->interest[2]);
}

void Lua_V1::SnapToPlane() {
	lua_Object planeObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	const char *planeName = lua_getstring(planeObj);
	WalkPlane *plane = nullptr;

	if (planeName)
		plane = g_grim->_walkSystem->planeNamed(planeName);

	if (!plane) {
		lua_pushnil();
		return;
	}

	float point[3];

	point[0] = lua_getnumber(xObj);
	point[1] = lua_getnumber(yObj);
	point[2] = lua_getnumber(zObj);
	plane->snapTo(point);

	lua_pushnumber(point[0]);
	lua_pushnumber(point[1]);
	lua_pushnumber(point[2]);
}

void Lua_V1::FindLight() {
	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj)) {
		lua_pushnil();
		return;
	}

	int index = g_grim->_sputRoom->findLightByName(nullptr, lua_getstring(nameObj));

	if (index == -1)
		lua_pushnil();
	else
		lua_pushnumber((float)index);
}

void Lua_V1::IsLightOn() {
	lua_Object lightObj = lua_getparam(1);
	int lightIndex;

	if (!getLightIndex(lightObj, &lightIndex)) {
		lua_pushnil();
		return;
	}

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (light && light->light.active)
		lua_pushnumber(1.0f);
	else
		lua_pushnil();
}

void Lua_V1::SetLightType() {
	lua_Object lightObj = lua_getparam(1);
	lua_Object typeObj = lua_getparam(2);
	Room *room = g_grim->_sputRoom->getCurrentRoom();
	int lightIndex;

	if (!getLightIndex(lightObj, &lightIndex))
		return;

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (!light || !lua_isstring(typeObj))
		return;

	const char *typeName = lua_getstring(typeObj);

	if (strstr(typeName, "parallel") || strstr(typeName, "global"))
		light->light.type = 1;
	else if (strstr(typeName, "spot"))
		light->light.type = 3;
	else
		light->light.type = 2;

	for (uint s = 0; s < room->numSetups; s++) {
		RdCamera *camera = room->setups[s].camera.prdCamera;

		if (!camera || !camera->lights[lightIndex]) {
			warning("SetLightType(): setup %u has no camera light %d", s, lightIndex);
			continue;
		}

		camera->lights[lightIndex]->type = light->light.type;
	}
}

void Lua_V1::GetLightType() {
	lua_Object lightObj = lua_getparam(1);
	int lightIndex;

	if (getLightIndex(lightObj, &lightIndex)) {
		RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

		if (light) {
			if (light->light.type == 1) {
				lua_pushstring("parallel");
				return;
			}

			if (light->light.type == 3) {
				lua_pushstring("spot");
				return;
			}

			if (light->light.type == 2) {
				lua_pushstring("omni");
				return;
			}
		}
	}

	lua_pushnil();
}

void Lua_V1::GetLightPosition() {
	lua_Object lightObj = lua_getparam(1);
	int lightIndex;

	if (getLightIndex(lightObj, &lightIndex) && g_grim->_sputRoom->getCurrentRoom()) {
		RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

		if (light) {
			lua_pushnumber(light->position[0]);
			lua_pushnumber(light->position[1]);
			lua_pushnumber(light->position[2]);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::SetLightRotation() {
	lua_Object lightObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	Room *room = g_grim->_sputRoom->getCurrentRoom();
	int lightIndex;

	if (!getLightIndex(lightObj, &lightIndex))
		return;

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (!light)
		return;

	RdVector3 rotation;
	RdVector3 direction;

	rotation[0] = lua_isnumber(xObj) ? lua_getnumber(xObj) : 0.0f;
	rotation[1] = lua_isnumber(yObj) ? lua_getnumber(yObj) : 0.0f;
	rotation[2] = lua_isnumber(zObj) ? lua_getnumber(zObj) : 0.0f;

	direction[0] = 0.0f;
	direction[1] = 1.0f;
	direction[2] = 0.0f;
	g_grim->_rd->rotate3Acc(direction, rotation);

	light->light.direction[0] = direction[0];
	light->light.direction[1] = direction[1];
	light->light.direction[2] = direction[2];

	for (uint s = 0; s < room->numSetups; s++) {
		RdCamera *camera = room->setups[s].camera.prdCamera;

		if (!camera || !camera->lights[lightIndex]) {
			warning("SetLightRotation(): setup %u has no camera light %d", s, lightIndex);
			continue;
		}

		camera->lights[lightIndex]->direction[0] = direction[0];
		camera->lights[lightIndex]->direction[1] = direction[1];
		camera->lights[lightIndex]->direction[2] = direction[2];
	}
}

void Lua_V1::GetLightRotation() {
	lua_Object lightObj = lua_getparam(1);
	int lightIndex;

	if (getLightIndex(lightObj, &lightIndex)) {
		RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

		if (light) {
			RdVector3 angles;

			g_grim->_rd->extractAngles(light->light.direction, angles);
			lua_pushnumber(angles[0]);
			lua_pushnumber(angles[1]);
			lua_pushnumber(angles[2]);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::SetLightAngles() {
	lua_Object lightObj = lua_getparam(1);
	lua_Object angleXObj = lua_getparam(2);
	lua_Object angleYObj = lua_getparam(3);
	int lightIndex;

	if (!getLightIndex(lightObj, &lightIndex))
		return;

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (!light)
		return;

	float angleX = lua_isnumber(angleXObj) ? lua_getnumber(angleXObj) : 0.0f;
	float angleY = lua_isnumber(angleYObj) ? lua_getnumber(angleYObj) : 0.0f;

	g_grim->_rd->_light.setAngles(&light->light, angleX, angleY);
}

void Lua_V1::GetLightAngles() {
	lua_Object lightObj = lua_getparam(1);
	int lightIndex;

	if (getLightIndex(lightObj, &lightIndex)) {
		RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

		if (light) {
			lua_pushnumber(light->light.angleX);
			lua_pushnumber(light->light.angleY);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::GetLightIntensity() {
	lua_Object lightObj = lua_getparam(1);
	int lightIndex;

	lua_getparam(2);

	if (getLightIndex(lightObj, &lightIndex)) {
		RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

		if (light) {
			lua_pushnumber(light->light.intensity);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::PointLightAt() {
	lua_Object lightObj = lua_getparam(1);
	lua_Object targetObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	int lightIndex;

	if (!getLightIndex(lightObj, &lightIndex))
		return;

	RoomLight *light = g_grim->_sputRoom->getLight(nullptr, lightIndex);

	if (!light || (!isSputActor(targetObj) && !lua_isnumber(targetObj)))
		return;

	float x;
	float y;
	float z;

	if (isSputActor(targetObj)) {
		SputActor *actor = getSputActor(targetObj);

		if (!actor)
			return;

		x = actor->actorPos[0];
		y = actor->actorPos[1];
		z = actor->actorPos[2];
	} else {
		x = lua_getnumber(targetObj);
		y = lua_isnumber(yObj) ? lua_getnumber(yObj) : 0.0f;
		z = lua_isnumber(zObj) ? lua_getnumber(zObj) : 0.0f;
	}

	RdVector3 direction;

	direction[0] = x - light->position[0];
	direction[1] = y - light->position[1];
	direction[2] = z - light->position[2];
	g_grim->_rd->normalize3Acc(direction);

	light->light.direction[0] = direction[0];
	light->light.direction[1] = direction[1];
	light->light.direction[2] = direction[2];
}

void Lua_V1::ImStartRecording() {
	warning("Lua_V1::ImStartRecording(): not implemented");
}

void Lua_V1::ImStopRecording() {
	warning("Lua_V1::ImStopRecording(): not implemented");
}

void Lua_V1::ImGetMasterVol() {
	lua_pushnumber((float)g_grim->getImuse()->imSetGroupVol(0, -1));
}

void Lua_V1::ImSetMasterVol() {
	lua_Object volObj = lua_getparam(1);

	if (lua_isnumber(volObj))
		g_grim->getImuse()->imSetGroupVol(0, luaFtolSse(lua_getnumber(volObj)));
}

void Lua_V1::ImStartVoice() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object volumeObj = lua_getparam(2);
	lua_Object panObj = lua_getparam(3);

	if ((lua_isstring(nameObj) || lua_isnil(nameObj)) && lua_isnumber(volumeObj) && lua_isnumber(panObj)) {
		int pan = luaFtolSse(lua_getnumber(panObj));
		int volume = luaFtolSse(lua_getnumber(volumeObj));
		const char *name = lua_getstring(nameObj);

		if (!name) {
			warning("Lua_V1::ImStartVoice(): no voice name");
			return;
		}

		g_grim->getImuse()->soundStartVoiceStream(name, volume, pan);
	}
}

void Lua_V1::ImStopVoice() {
	g_grim->getImuse()->soundStopVoiceStream();
}

void Lua_V1::ImSetAttribute() {
	lua_Object attributeObj = lua_getparam(1);
	lua_Object valueObj = lua_getparam(2);

	if (lua_isnumber(attributeObj) && lua_isnumber(valueObj)) {
		int value = luaFtolSse(lua_getnumber(valueObj));
		int attribute = luaFtolSse(lua_getnumber(attributeObj));

		lua_pushnumber((float)g_grim->getImuse()->imSetAttribute(attribute, value));
	}
}

void Lua_V1::ImSetCuePoint() {
	lua_Object cueObj = lua_getparam(1);

	if (lua_isnumber(cueObj))
		g_grim->getImuse()->imSetCuePoint(luaFtolSse(lua_getnumber(cueObj)));
}

void Lua_V1::ImGetMemoryFootprint() {
	lua_pushnumber((float)(uint)(g_grim->isRemastered() ? -1 : g_grim->getImuse()->iMUSEGetMemoryFootprint()));
}

void Lua_V1::ImGetSoundCacheSize() {
	lua_pushnumber((float)(uint)(g_grim->isRemastered() ? -1 : g_grim->getImuse()->iMUSEGetSoundCacheSize()));
}

void Lua_V1::ActorShadow() {
	lua_Object actorObj = lua_getparam(1);

	if (isSputActor(actorObj)) {
		SputActor *actor = getSputActor(actorObj);

		if (actor && g_grim->_sputActor->actorHasShadow(actor)) {
			lua_pushnumber(1.0f);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::ActorDistToShadowPlane() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object indexObj = lua_getparam(2);
	int shadowIndex = luaFtolSse(lua_getnumber(indexObj));

	if (!isSputActor(actorObj)) {
		lua_pushnil();
		return;
	}

	SputActor *actor = getSputActor(actorObj);

	if (!actor) {
		lua_pushnil();
		return;
	}

	if (!actor->extra)
		g_grim->_sputActor->allocateActorExtra(actor);

	if (shadowIndex < 0 || shadowIndex >= (int)ARRAYSIZE(actor->extra->shadows)) {
		warning("Lua_V1::ActorDistToShadowPlane(): invalid shadow index %d", shadowIndex);
		lua_pushnil();
		return;
	}

	Grim::ActorShadow *shadow = &actor->extra->shadows[shadowIndex];
	float distance;

	if (g_grim->isRemastered()) {
		double sum = (double)shadow->normal[1] * actor->actorPos[1] + (double)shadow->normal[0] * actor->actorPos[0];

		sum = sum + (double)shadow->normal[2] * actor->actorPos[2];
		sum = sum + shadow->planeD;
		distance = (float)sum;

		if (distance < 0.0f)
			distance = -distance;
	} else {
		double sum = (double)shadow->normal[0] * actor->actorPos[0] + (double)shadow->normal[2] * actor->actorPos[2];

		sum = sum + (double)actor->actorPos[1] * shadow->normal[1];
		sum = sum + shadow->planeD;

		if (sum < 0.0)
			sum = -sum;

		distance = (float)sum;
	}

	lua_pushnumber(distance);
}

void Lua_V1::MarkDirtyRect() {
	lua_Object firstObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object widthObj = lua_getparam(3);
	lua_Object heightObj = lua_getparam(4);

	if (isSputActor(firstObj)) {
		SputActor *actor = getSputActor(firstObj);

		if (!actor)
			return;

		if (actor->onScreen && actor->drawnRect.width > 0 && actor->drawnRect.height > 0)
			g_grim->_sputRender->markDirtyRect(&actor->drawnRect, 1);
		else
			lua_pushnil();
	} else {
		StdRect rect;

		rect.x = luaFtolSse(lua_getnumber(firstObj));
		rect.y = luaFtolSse(lua_getnumber(yObj));
		rect.width = luaFtolSse(lua_getnumber(widthObj));
		rect.height = luaFtolSse(lua_getnumber(heightObj));
		g_grim->_sputRender->markDirtyRect(&rect, 1);
	}
}

void Lua_V1::CacheRect() {
	lua_Object firstObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object widthObj = lua_getparam(3);
	lua_Object heightObj = lua_getparam(4);
	Grim::CacheRect *rect;

	if (isSputActor(firstObj)) {
		SputActor *actor = getSputActor(firstObj);

		if (!actor || !actor->onScreen || actor->drawnRect.width <= 0 || actor->drawnRect.height <= 0) {
			lua_pushnil();
			return;
		}

		rect = g_grim->_sputRender->createCacheRect(actor->drawnRect.x, actor->drawnRect.y, actor->drawnRect.width, actor->drawnRect.height);
	} else {
		int x = luaFtolSse(lua_getnumber(firstObj));
		int y = luaFtolSse(lua_getnumber(yObj));
		int width = luaFtolSse(lua_getnumber(widthObj));
		int height = luaFtolSse(lua_getnumber(heightObj));

		rect = g_grim->_sputRender->createCacheRect(x, y, width, height);
	}

	lua_pushusertag((int32)(intptr)rect, MKTAG('P', 'O', 'I', 'N'));
}

void Lua_V1::RestoreCacheRect() {
	lua_Object rectObj = lua_getparam(1);
	int32 handle = lua_getuserdata(rectObj);

	if (handle) {
		Grim::CacheRect *rect = g_grim->_sputRender->findCacheRect(handle);

		if (rect && rect->magic == 0x11111111)
			g_grim->_sputRender->restoreCacheRect();
	}
}

void Lua_V1::ResReport() {
	warning("Lua_V1::ResReport(): not implemented");
}

void Lua_V1::HardwareAccelerated() {
	if (g_grim->_sputRender->is3DHardware())
		lua_pushnumber(1.0f);
	else
		lua_pushnumber(0.0f);
}

void Lua_V1::IrisComplete() {
	lua_pushnumber((float)(g_grim->_sputRender->irisComplete() != 0));
}

void Lua_V1::SaveScreen() {
	lua_Object pathObj = lua_getparam(1);

	if (!lua_isnil(pathObj) && lua_isstring(pathObj))
		g_grim->_sputRender->saveScreen(lua_getstring(pathObj));
}

void Lua_V1::FindFileOnAnyCD() {
	lua_Object fileObj = lua_getparam(1);

	if (!lua_isstring(fileObj) || !lua_getstring(fileObj)) {
		lua_pushnil();
		return;
	}

	Common::SeekableReadStream *file = g_grim->openFile(lua_getstring(fileObj));

	if (file) {
		delete file;
		lua_pushnumber(1.0f);
	} else {
		lua_pushnil();
	}
}

void Lua_V1::SetSoundParameters() {
	lua_Object maxDistObj = lua_getparam(1);
	lua_Object minVolObj = lua_getparam(2);
	lua_Object maxVolObj = lua_getparam(3);
	lua_Object panFactorObj = lua_getparam(4);
	lua_Object maxRadiusObj = lua_getparam(5);
	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (!room)
		return;

	if (lua_isnil(maxDistObj) && lua_isnil(minVolObj) && lua_isnil(maxVolObj) && lua_isnil(panFactorObj) && lua_isnil(maxRadiusObj)) {
		g_grim->_sputRoom->setSoundParameters(room, 8.0f, 20, 127, 1.5f, 0.80000001f);
		return;
	}

	float maxDist;
	byte minVol;
	byte maxVol;
	float panFactor;
	float maxRadius;

	g_grim->_sputRoom->getSoundParameters(room, &maxDist, &minVol, &maxVol, &panFactor, &maxRadius);

	float newMaxRadius = lua_isnil(maxRadiusObj) ? maxRadius : lua_getnumber(maxRadiusObj);
	float newPanFactor = lua_isnil(panFactorObj) ? panFactor : lua_getnumber(panFactorObj);
	byte newMaxVol = lua_isnil(maxVolObj) ? maxVol : (byte)luaFtolSse(lua_getnumber(maxVolObj));
	byte newMinVol = lua_isnil(minVolObj) ? minVol : (byte)luaFtolSse(lua_getnumber(minVolObj));
	float newMaxDist = lua_isnil(maxDistObj) ? maxDist : lua_getnumber(maxDistObj);

	g_grim->_sputRoom->setSoundParameters(room, newMaxDist, newMinVol, newMaxVol, newPanFactor, newMaxRadius);
}

void Lua_V1::GetSoundParameters() {
	Room *room = g_grim->_sputRoom->getCurrentRoom();

	if (!room)
		return;

	float maxDist;
	byte minVol;
	byte maxVol;
	float panFactor;
	float maxRadius;

	g_grim->_sputRoom->getSoundParameters(room, &maxDist, &minVol, &maxVol, &panFactor, &maxRadius);

	lua_pushnumber(maxDist);
	lua_pushnumber((float)minVol);
	lua_pushnumber((float)maxVol);
	lua_pushnumber(panFactor);
	lua_pushnumber(maxRadius);
}

void Lua_V1::TEST() {
	g_grim->_walkSystem->shrinkBoxes(0.005f);
}

void Lua_V1::GetTickCount() {
	lua_pushnumber((float)g_system->getMillis());
}

void Lua_V1::ActorVoiceIs3D() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object flagObj = lua_getparam(2);
	SputActor *actor = getSputActor(actorObj);

	if (!actor)
		return;

	if (lua_isnil(flagObj))
		actor->flags &= ~8;
	else
		actor->flags |= 8;
}

void Lua_V1::DumpMemoryLog() {
	warning("Lua_V1::DumpMemoryLog(): not implemented");
}

void Lua_V1::BuildCache() {
	g_grim->_walkSystem->buildCache();
}

void Lua_V1::RestoreCache() {
	g_grim->_walkSystem->restoreCache();
}

void Lua_V1::SetWalkSystemFadeTime() {
	lua_Object timeObj = lua_getparam(1);

	if (!lua_isnil(timeObj))
		g_grim->_sputActor->setWalkSystemFadeTime(luaFtolSse(lua_getnumber(timeObj)));
}

void Lua_V1::GetKeyLength() {
	lua_Object keyObj = lua_getparam(1);
	ResNode *node = g_grim->_res->resourceHandleFromSerialNumber(lua_getuserdata(keyObj));

	if (!node || !node->handle) {
		warning("Lua_V1::GetKeyLength(): bad keyframe reference");
		return;
	}

	RdKeyframe *keyframe = static_cast<RdKeyframe *>(node->handle);

	lua_pushnumber((float)((double)keyframe->numFrames / keyframe->fps * 1000.0));
}

void Lua_V1::GetSmushLength() {
	warning("Lua_V1::GetSmushLength(): not implemented");
}

// Opcode tables, in the order of the original executable. A saved Lua state stores every
// C function as (number of lists registered after its list << 16) | index in the list, so
// the content, the order and the aliased entries must not change.
// Entries that use LuaBase::dummyHandler are the do-nothing function of the original.
struct luaL_reg grimMainOpcodes[] = {
	{ "EnginePause", LUA_OPCODE(Lua_V1, EnginePause) },
	{ "EngineResume", LUA_OPCODE(Lua_V1, EngineResume) },
	{ "EngineDisplay", LUA_OPCODE(Lua_V1, EngineDisplay) },
	{ "SearchForFileOrSwapCDs", LUA_OPCODE(Lua_V1, SearchForFileOrSwapCDs) },
	{ "CheckForFile", LUA_OPCODE(Lua_V1, CheckForFile) },
	{ "Load", LUA_OPCODE(Lua_V1, Load) },
	{ "Save", LUA_OPCODE(Lua_V1, Save) },
	{ "SetActorColormap", LUA_OPCODE(Lua_V1, SetActorColormap) },
	{ "GetActorCostume", LUA_OPCODE(Lua_V1, GetActorCostume) },
	{ "SetActorCostume", LUA_OPCODE(Lua_V1, SetActorCostume) },
	{ "GetActorScale", LUA_OPCODE(Lua_V1, GetActorScale) },
	{ "SetActorScale", LUA_OPCODE(Lua_V1, SetActorScale) },
	{ "GetActorTimeScale", LUA_OPCODE(Lua_V1, GetActorTimeScale) },
	{ "SetActorTimeScale", LUA_OPCODE(Lua_V1, SetActorTimeScale) },
	{ "GetActorNodeLocation", LUA_OPCODE(Lua_V1, GetActorNodeLocation) },
	{ "SetActorWalkChore", LUA_OPCODE(Lua_V1, SetActorWalkChore) },
	{ "SetActorTurnChores", LUA_OPCODE(Lua_V1, SetActorTurnChores) },
	{ "SetActorRestChore", LUA_OPCODE(Lua_V1, SetActorRestChore) },
	{ "SetActorMumblechore", LUA_OPCODE(Lua_V1, SetActorMumblechore) },
	{ "SetActorTalkChore", LUA_OPCODE(Lua_V1, SetActorTalkChore) },
	{ "SetActorWalkRate", LUA_OPCODE(Lua_V1, SetActorWalkRate) },
	{ "GetActorWalkRate", LUA_OPCODE(Lua_V1, GetActorWalkRate) },
	{ "SetActorTurnRate", LUA_OPCODE(Lua_V1, SetActorTurnRate) },
	{ "GetActorTurnRate", LUA_OPCODE(Lua_V1, GetActorTurnRate) },
	{ "SetSelectedActor", LUA_OPCODE(Lua_V1, SetSelectedActor) },
	{ "LoadActor", LUA_OPCODE(Lua_V1, LoadActor) },
	{ "GetActorPos", LUA_OPCODE(Lua_V1, GetActorPos) },
	{ "GetActorRect", LUA_OPCODE(Lua_V1, GetActorRect) },
	{ "GetActorPuckVector", LUA_OPCODE(Lua_V1, GetActorPuckVector) },
	{ "GetActorYawToPoint", LUA_OPCODE(Lua_V1, GetActorYawToPoint) },
	{ "SetActorReflection", LUA_OPCODE(Lua_V1, SetActorReflection) },
	{ "SetActorOffsetYaw", LUA_OPCODE(Lua_V1, SetActorOffsetYaw) },
	{ "PutActorAtInterest", LUA_OPCODE(Lua_V1, PutActorAtInterest) },
	{ "PutActorAtOrigin", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "PutActorAt", LUA_OPCODE(Lua_V1, PutActorAt) },
	{ "PutActorInSet", LUA_OPCODE(Lua_V1, PutActorInSet) },
	{ "GetClippedPos", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "WalkActorVector", LUA_OPCODE(Lua_V1, WalkActorVector) },
	{ "WalkActorForward", LUA_OPCODE(Lua_V1, WalkActorForward) },
	{ "DriveActorTo", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "WalkActorTo", LUA_OPCODE(Lua_V1, WalkActorTo) },
	{ "WalkActorToAvoiding", LUA_OPCODE(Lua_V1, WalkActorToAvoiding) },
	{ "WalkActorToInterest", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "ActorLookAt", LUA_OPCODE(Lua_V1, ActorLookAt) },
	{ "IsActorLooking", LUA_OPCODE(Lua_V1, IsActorLooking) },
	{ "SetActorLookRate", LUA_OPCODE(Lua_V1, SetActorLookRate) },
	{ "GetActorLookRate", LUA_OPCODE(Lua_V1, GetActorLookRate) },
	{ "CanActorSee", LUA_OPCODE(Lua_V1, CanActorSee) },
	{ "GetVisibleThings", LUA_OPCODE(Lua_V1, GetVisibleThings) },
	{ "GetCameraActor", LUA_OPCODE(Lua_V1, GetCameraActor) },
	{ "SetActorHead", LUA_OPCODE(Lua_V1, SetActorHead) },
	{ "SetActorVisibility", LUA_OPCODE(Lua_V1, SetActorVisibility) },
	{ "SetActorFollowBoxes", LUA_OPCODE(Lua_V1, SetActorFollowBoxes) },
	{ "ShutUpActor", LUA_OPCODE(Lua_V1, ShutUpActor) },
	{ "SetActorFrustrumCull", LUA_OPCODE(Lua_V1, SetActorFrustrumCull) },
	{ "IsActorInSector", LUA_OPCODE(Lua_V1, IsActorInSector) },
	{ "GetActorSector", LUA_OPCODE(Lua_V1, GetActorSector) },
	{ "IsPointInSector", LUA_OPCODE(Lua_V1, IsPointInSector) },
	{ "GetPointSector", LUA_OPCODE(Lua_V1, GetPointSector) },
	{ "TurnActor", LUA_OPCODE(Lua_V1, TurnActor) },
	{ "GetActorRot", LUA_OPCODE(Lua_V1, GetActorRot) },
	{ "SetActorRot", LUA_OPCODE(Lua_V1, SetActorRot) },
	{ "SetActorPitch", LUA_OPCODE(Lua_V1, SetActorPitch) },
	{ "SetActorRoll", LUA_OPCODE(Lua_V1, SetActorRoll) },
	{ "IsActorTurning", LUA_OPCODE(Lua_V1, IsActorTurning) },
	{ "PlayActorChore", LUA_OPCODE(Lua_V1, PlayActorChore) },
	{ "PlayActorChoreLooping", LUA_OPCODE(Lua_V1, PlayActorChoreLooping) },
	{ "StopActorChore", LUA_OPCODE(Lua_V1, StopActorChore) },
	{ "CompleteActorChore", LUA_OPCODE(Lua_V1, CompleteActorChore) },
	{ "IsActorMoving", LUA_OPCODE(Lua_V1, IsActorMoving) },
	{ "IsActorChoring", LUA_OPCODE(Lua_V1, IsActorChoring) },
	{ "IsActorResting", LUA_OPCODE(Lua_V1, IsActorResting) },
	{ "SetActorChoreLooping", LUA_OPCODE(Lua_V1, SetActorChoreLooping) },
	{ "GetActorChores", LUA_OPCODE(Lua_V1, GetActorChores) },
	{ "GetActorLoopingChores", LUA_OPCODE(Lua_V1, GetActorLoopingChores) },
	{ "GetActorCostumeDepth", LUA_OPCODE(Lua_V1, GetActorCostumeDepth) },
	{ "WorldToScreen", LUA_OPCODE(Lua_V1, WorldToScreen) },
	{ "ScreenToWorld", LUA_OPCODE(Lua_V1, ScreenToWorld) },
	{ "exit", LUA_OPCODE(Lua_V1, Exit) },
	{ "FunctionName", LUA_OPCODE(Lua_V1, FunctionName) },
	{ "EnableDebugKeys", LUA_OPCODE(Lua_V1, EnableDebugKeys) },
	{ "LockFont", LUA_OPCODE(Lua_V1, LockFont) },
	{ "UnLockFont", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "LockSound", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "UnlockSound", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "LockCostume", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "UnlockCostume", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "EnableControl", LUA_OPCODE(Lua_V1, EnableControl) },
	{ "DisableControl", LUA_OPCODE(Lua_V1, DisableControl) },
	{ "GetControlState", LUA_OPCODE(Lua_V1, GetControlState) },
	{ "PrintMessage", LUA_OPCODE(Lua_V1, PrintMessage) },
	{ "PrintError", LUA_OPCODE(Lua_V1, PrintDebug) }, // same function as PrintWarning and PrintDebug
	{ "PrintWarning", LUA_OPCODE(Lua_V1, PrintDebug) }, // same function as PrintError and PrintDebug
	{ "PrintDebug", LUA_OPCODE(Lua_V1, PrintDebug) }, // same function as PrintError and PrintWarning
	{ "MakeCurrentSet", LUA_OPCODE(Lua_V1, MakeCurrentSet) },
	{ "LockSet", LUA_OPCODE(Lua_V1, LockSet) },
	{ "UnLockSet", LUA_OPCODE(Lua_V1, UnLockSet) },
	{ "MakeCurrentSetup", LUA_OPCODE(Lua_V1, MakeCurrentSetup) },
	{ "GetCurrentSetup", LUA_OPCODE(Lua_V1, GetCurrentSetup) },
	{ "NextSetup", LUA_OPCODE(Lua_V1, NextSetup) },
	{ "PreviousSetup", LUA_OPCODE(Lua_V1, PreviousSetup) },
	{ "StartFullscreenMovie", LUA_OPCODE(Lua_V1, StartFullscreenMovie) },
	{ "IsFullscreenMoviePlaying", LUA_OPCODE(Lua_V1, IsFullscreenMoviePlaying) }, // same function as IsMoviePlaying
	{ "StartMovie", LUA_OPCODE(Lua_V1, StartMovie) },
	{ "StopMovie", LUA_OPCODE(Lua_V1, StopMovie) },
	{ "PauseMovie", LUA_OPCODE(Lua_V1, PauseMovie) },
	{ "IsMoviePlaying", LUA_OPCODE(Lua_V1, IsFullscreenMoviePlaying) }, // same function as IsFullscreenMoviePlaying
	{ "PlaySound", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "PlaySoundAt", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "PlaySoundAttached", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "IsSoundPlaying", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "SetSoundPosition", LUA_OPCODE(Lua_V1, SetSoundPosition) },
	{ "FileFindFirst", LUA_OPCODE(Lua_V1, FileFindFirst) },
	{ "FileFindNext", LUA_OPCODE(Lua_V1, FileFindNext) },
	{ "FileFindDispose", LUA_OPCODE(Lua_V1, FileFindDispose) },
	{ "InputDialog", LUA_OPCODE(Lua_V1, InputDialog) },
	{ "QueryDialog", LUA_OPCODE(Lua_V1, QueryDialog) },
	{ "WriteRegistryValue", LUA_OPCODE(Lua_V1, WriteRegistryValue) },
	{ "ReadRegistryValue", LUA_OPCODE(Lua_V1, ReadRegistryValue) },
	{ "GetSectorVertices", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "GetSectorOppositeEdge", LUA_OPCODE(Lua_V1, GetSectorOppositeEdge) },
	{ "IsSectorActive", LUA_OPCODE(Lua_V1, IsSectorActive) },
	{ "MakeSectorActive", LUA_OPCODE(Lua_V1, MakeSectorActive) },
	{ "PreRender", LUA_OPCODE(Lua_V1, PreRender) },
	{ "SpewStartup", LUA_OPCODE(Lua_V1, SpewStartup) },
	{ "SpewShutdown", LUA_OPCODE(Lua_V1, SpewShutdown) },
	{ "GetCurrentScript", LUA_OPCODE(Lua_V1, GetCurrentScript) },
	{ "PrintActorCostumes", LUA_OPCODE(Lua_V1, PrintActorCostumes) },
	{ "PushActorCostume", LUA_OPCODE(Lua_V1, PushActorCostume) },
	{ "PopActorCostume", LUA_OPCODE(Lua_V1, PopActorCostume) },
	{ "LoadCostume", LUA_OPCODE(Lua_V1, LoadCostume) },
	{ "GetPhysicalBytes", LUA_OPCODE(Lua_V1, GetPhysicalBytes) },
	{ "SetMemoryUsage", LUA_OPCODE(Lua_V1, SetMemoryUsage) },
	{ "RotateVector", LUA_OPCODE(Lua_V1, RotateVector) },
	{ "GetCameraPosition", LUA_OPCODE(Lua_V1, GetCameraPosition) },
	{ "SetCameraPosition", LUA_OPCODE(Lua_V1, SetCameraPosition) },
	{ "GetCameraInterest", LUA_OPCODE(Lua_V1, GetCameraInterest) },
	{ "SetCameraInterest", LUA_OPCODE(Lua_V1, SetCameraInterest) },
	{ "GetCameraFOV", LUA_OPCODE(Lua_V1, GetCameraFOV) },
	{ "SetCameraFOV", LUA_OPCODE(Lua_V1, SetCameraFOV) },
	{ "GetCameraRoll", LUA_OPCODE(Lua_V1, GetCameraRoll) },
	{ "SetCameraRoll", LUA_OPCODE(Lua_V1, SetCameraRoll) },
	{ "GetCameraLookVector", LUA_OPCODE(Lua_V1, GetCameraLookVector) },
	{ "PointActorAt", LUA_OPCODE(Lua_V1, PointActorAt) },
	{ "TurnActorTo", LUA_OPCODE(Lua_V1, TurnActorTo) },
	{ "PerSecond", LUA_OPCODE(Lua_V1, PerSecond) },
	{ "SnapToPlane", LUA_OPCODE(Lua_V1, SnapToPlane) },
	{ "GetAngleBetweenVectors", LUA_OPCODE(Lua_V1, GetAngleBetweenVectors) },
	{ "GetAngleBetweenActors", LUA_OPCODE(Lua_V1, GetAngleBetweenActors) },
	{ "SetAmbientLight", LUA_OPCODE(Lua_V1, SetAmbientLight) },
	{ "FindLight", LUA_OPCODE(Lua_V1, FindLight) },
	{ "TurnLightOn", LUA_OPCODE(Lua_V1, TurnLightOn) },
	{ "IsLightOn", LUA_OPCODE(Lua_V1, IsLightOn) },
	{ "SetLightType", LUA_OPCODE(Lua_V1, SetLightType) },
	{ "GetLightType", LUA_OPCODE(Lua_V1, GetLightType) },
	{ "SetLightPosition", LUA_OPCODE(Lua_V1, SetLightPosition) },
	{ "GetLightPosition", LUA_OPCODE(Lua_V1, GetLightPosition) },
	{ "SetLightRotation", LUA_OPCODE(Lua_V1, SetLightRotation) },
	{ "GetLightRotation", LUA_OPCODE(Lua_V1, GetLightRotation) },
	{ "SetLightAngles", LUA_OPCODE(Lua_V1, SetLightAngles) },
	{ "GetLightAngles", LUA_OPCODE(Lua_V1, GetLightAngles) },
	{ "SetLightIntensity", LUA_OPCODE(Lua_V1, SetLightIntensity) },
	{ "GetLightIntensity", LUA_OPCODE(Lua_V1, GetLightIntensity) },
	{ "PointLightAt", LUA_OPCODE(Lua_V1, PointLightAt) },
	{ "LightMgrStartup", LUA_OPCODE(Lua_V1, LightMgrStartup) },
	{ "LightMgrShutdown", LUA_OPCODE(Lua_V1, LightMgrShutdown) },
	{ "LightMgrSetChange", LUA_OPCODE(Lua_V1, LightMgrSetChange) },
	{ "ImStartRecording", LUA_OPCODE(Lua_V1, ImStartRecording) },
	{ "ImStopRecording", LUA_OPCODE(Lua_V1, ImStopRecording) },
	{ "ImStartSound", LUA_OPCODE(Lua_V1, ImStartSound) }, // same function as play
	{ "play", LUA_OPCODE(Lua_V1, ImStartSound) }, // same function as ImStartSound
	{ "ImStopSound", LUA_OPCODE(Lua_V1, ImStopSound) },
	{ "ImStopAllSounds", LUA_OPCODE(Lua_V1, ImStopAllSounds) }, // same function as quiet
	{ "quiet", LUA_OPCODE(Lua_V1, ImStopAllSounds) }, // same function as ImStopAllSounds
	{ "ImGetParam", LUA_OPCODE(Lua_V1, ImGetParam) },
	{ "ImSetParam", LUA_OPCODE(Lua_V1, ImSetParam) },
	{ "ImFadeParam", LUA_OPCODE(Lua_V1, ImFadeParam) },
	{ "ImGetMasterVol", LUA_OPCODE(Lua_V1, ImGetMasterVol) },
	{ "ImSetMasterVol", LUA_OPCODE(Lua_V1, ImSetMasterVol) },
	{ "ImGetSfxVol", LUA_OPCODE(Lua_V1, ImGetSfxVol) },
	{ "ImSetSfxVol", LUA_OPCODE(Lua_V1, ImSetSfxVol) },
	{ "ImGetVoiceVol", LUA_OPCODE(Lua_V1, ImGetVoiceVol) },
	{ "ImSetVoiceVol", LUA_OPCODE(Lua_V1, ImSetVoiceVol) },
	{ "ImGetMusicVol", LUA_OPCODE(Lua_V1, ImGetMusicVol) },
	{ "ImSetMusicVol", LUA_OPCODE(Lua_V1, ImSetMusicVol) },
	{ "ImStartVoice", LUA_OPCODE(Lua_V1, ImStartVoice) },
	{ "ImStopVoice", LUA_OPCODE(Lua_V1, ImStopVoice) },
	{ "ImSetState", LUA_OPCODE(Lua_V1, ImSetState) },
	{ "ImSetSequence", LUA_OPCODE(Lua_V1, ImSetSequence) },
	{ "ImSetAttribute", LUA_OPCODE(Lua_V1, ImSetAttribute) },
	{ "ImSetCuePoint", LUA_OPCODE(Lua_V1, ImSetCuePoint) },
	{ "ImPause", LUA_OPCODE(Lua_V1, ImPause) },
	{ "ImResume", LUA_OPCODE(Lua_V1, ImResume) },
	{ "vfx", LUA_OPCODE(Lua_V1, ImSetVoiceEffect) }, // same function as ImSetVoiceEffect
	{ "ImSetVoiceEffect", LUA_OPCODE(Lua_V1, ImSetVoiceEffect) }, // same function as vfx
	{ "ImGetMemoryFootprint", LUA_OPCODE(Lua_V1, ImGetMemoryFootprint) },
	{ "ImGetSoundCacheSize", LUA_OPCODE(Lua_V1, ImGetSoundCacheSize) },
	{ "LoadBundle", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "UnloadBundle", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "SetGamma", LUA_OPCODE(Lua_V1, SetGamma) },
	{ "SetActorWalkDominate", LUA_OPCODE(Lua_V1, SetActorWalkDominate) },
	{ "SetActorConstrain", LUA_OPCODE(Lua_V1, SetActorConstrain) },
	{ "RenderModeUser", LUA_OPCODE(Lua_V1, RenderModeUser) },
	{ "ForceRefresh", LUA_OPCODE(Lua_V1, ForceRefresh) },
	{ "DimScreen", LUA_OPCODE(Lua_V1, DimScreen) },
	{ "DimRegion", LUA_OPCODE(Lua_V1, DimRegion) },
	{ "CleanBuffer", LUA_OPCODE(Lua_V1, CleanBuffer) },
	{ "Display", LUA_OPCODE(Lua_V1, Display) },
	{ "SetSpeechMode", LUA_OPCODE(Lua_V1, SetSpeechMode) },
	{ "GetSpeechMode", LUA_OPCODE(Lua_V1, GetSpeechMode) },
	{ "SetShadowColor", LUA_OPCODE(Lua_V1, SetShadowColor) },
	{ "ActivateActorShadow", LUA_OPCODE(Lua_V1, ActivateActorShadow) },
	{ "ActorShadow", LUA_OPCODE(Lua_V1, ActorShadow) },
	{ "SetActorShadowPlane", LUA_OPCODE(Lua_V1, SetActorShadowPlane) },
	{ "SetActorShadowPoint", LUA_OPCODE(Lua_V1, SetActorShadowPoint) },
	{ "SetActiveShadow", LUA_OPCODE(Lua_V1, SetActiveShadow) },
	{ "ActorDistToShadowPlane", LUA_OPCODE(Lua_V1, ActorDistToShadowPlane) },
	{ "KillActorShadows", LUA_OPCODE(Lua_V1, KillActorShadows) },
	{ "AddShadowPlane", LUA_OPCODE(Lua_V1, AddShadowPlane) },
	{ "SetActorShadowValid", LUA_OPCODE(Lua_V1, SetActorShadowValid) },
	{ "NewObjectState", LUA_OPCODE(Lua_V1, NewObjectState) },
	{ "FreeObjectState", LUA_OPCODE(Lua_V1, FreeObjectState) },
	{ "SetObjectType", LUA_OPCODE(Lua_V1, SetObjectType) },
	{ "SendObjectToBack", LUA_OPCODE(Lua_V1, SendObjectToBack) },
	{ "SendObjectToFront", LUA_OPCODE(Lua_V1, SendObjectToFront) },
	{ "ActorToClean", LUA_OPCODE(Lua_V1, ActorToClean) },
	{ "FlushControls", LUA_OPCODE(Lua_V1, FlushControls) },
	{ "SetActorCollisionMode", LUA_OPCODE(Lua_V1, SetActorCollisionMode) },
	{ "SetActorCollisionScale", LUA_OPCODE(Lua_V1, SetActorCollisionScale) },
	{ "MarkDirtyRect", LUA_OPCODE(Lua_V1, MarkDirtyRect) },
	{ "CacheRect", LUA_OPCODE(Lua_V1, CacheRect) },
	{ "RestoreCacheRect", LUA_OPCODE(Lua_V1, RestoreCacheRect) },
	{ "ResReport", LUA_OPCODE(Lua_V1, ResReport) },
	{ "HardwareAccelerated", LUA_OPCODE(Lua_V1, HardwareAccelerated) },
	{ "SetActorClipActive", LUA_OPCODE(Lua_V1, SetActorClipActive) },
	{ "SetActorClipPlane", LUA_OPCODE(Lua_V1, SetActorClipPlane) },
	{ "FadeOutChore", LUA_OPCODE(Lua_V1, FadeOutChore) },
	{ "FadeInChore", LUA_OPCODE(Lua_V1, FadeInChore) },
	{ "IrisDown", LUA_OPCODE(Lua_V1, IrisDown) },
	{ "IrisUp", LUA_OPCODE(Lua_V1, IrisUp) },
	{ "IrisComplete", LUA_OPCODE(Lua_V1, IrisComplete) },
	{ "IrisClear", LUA_OPCODE(Lua_V1, IrisClear) },
	{ "TextFileGetLineCount", LUA_OPCODE(Lua_V1, TextFileGetLineCount) },
	{ "TextFileGetLine", LUA_OPCODE(Lua_V1, TextFileGetLine) },
	{ "ScreenShot", LUA_OPCODE(Lua_V1, ScreenShot) },
	{ "SaveScreen", LUA_OPCODE(Lua_V1, SaveScreen) },
	{ "GetSaveGameImage", LUA_OPCODE(Lua_V1, GetSaveGameImage) },
	{ "GetImage", LUA_OPCODE(Lua_V1, GetImage) },
	{ "FreeImage", LUA_OPCODE(Lua_V1, FreeImage) },
	{ "BlastImage", LUA_OPCODE(Lua_V1, BlastImage) },
	{ "BlastRect", LUA_OPCODE(Lua_V1, BlastRect) },
	{ "SubmitSaveGameData", LUA_OPCODE(Lua_V1, SubmitSaveGameData) },
	{ "GetSaveGameData", LUA_OPCODE(Lua_V1, GetSaveGameData) },
	{ "SetTextSpeed", LUA_OPCODE(Lua_V1, SetTextSpeed) },
	{ "GetTextSpeed", LUA_OPCODE(Lua_V1, GetTextSpeed) },
	{ "FindFileOnAnyCD", LUA_OPCODE(Lua_V1, FindFileOnAnyCD) },
	{ "DetachFromResources", LUA_OPCODE(Lua_V1, DetachFromResources) },
	{ "AttachToResources", LUA_OPCODE(Lua_V1, AttachToResources) },
	{ "SetSoundParameters", LUA_OPCODE(Lua_V1, SetSoundParameters) },
	{ "GetSoundParameters", LUA_OPCODE(Lua_V1, GetSoundParameters) },
	{ "TEST", LUA_OPCODE(Lua_V1, TEST) },
	{ "ActorPuckOrient", LUA_OPCODE(Lua_V1, ActorPuckOrient) },
	{ "GetTickCount", LUA_OPCODE(Lua_V1, GetTickCount) },
	{ "ActorVoiceIs3D", LUA_OPCODE(Lua_V1, ActorVoiceIs3D) },
	{ "JustLoaded", LUA_OPCODE(Lua_V1, JustLoaded) },
	{ "DumpMemoryLog", LUA_OPCODE(Lua_V1, DumpMemoryLog) },
	{ "ResetTextures", LUA_OPCODE(Lua_V1, ResetTextures) },
	{ "BuildCache", LUA_OPCODE(Lua_V1, BuildCache) },
	{ "RestoreCache", LUA_OPCODE(Lua_V1, RestoreCache) },
	{ "ShrinkBoxes", LUA_OPCODE(Lua_V1, ShrinkBoxes) },
	{ "UnShrinkBoxes", LUA_OPCODE(Lua_V1, UnShrinkBoxes) },
	{ "GetShrinkPos", LUA_OPCODE(Lua_V1, GetShrinkPos) },
	{ "NukeResources", LUA_OPCODE(Lua_V1, NukeResources) },
	{ "SetActorInvClipNode", LUA_OPCODE(Lua_V1, SetActorInvClipNode) },
	{ "GetDiskFreeSpace", LUA_OPCODE(Lua_V1, GetDiskFreeSpace) },
	{ "SetWalkSystemFadeTime", LUA_OPCODE(Lua_V1, SetWalkSystemFadeTime) },
	{ "SaveIMuse", LUA_OPCODE(Lua_V1, SaveIMuse) },
	{ "RestoreIMuse", LUA_OPCODE(Lua_V1, RestoreIMuse) },
	{ "GetMemoryUsage", LUA_OPCODE(Lua_V1, GetMemoryUsage) }
};

struct luaL_reg grimCostumeOpcodes[] = {
	{ "GetKeyLength", LUA_OPCODE(Lua_V1, GetKeyLength) },
	{ "GetSmushLength", LUA_OPCODE(Lua_V1, GetSmushLength) }
};

static struct luaL_reg grimTextOpcodes[] = {
	{ "IsMessageGoing", LUA_OPCODE(Lua_V1, IsMessageGoing) },
	{ "SetSayLineDefaults", LUA_OPCODE(Lua_V1, SetSayLineDefaults) },
	{ "SetActorTalkColor", LUA_OPCODE(Lua_V1, SetActorTalkColor) },
	{ "GetActorTalkColor", LUA_OPCODE(Lua_V1, GetActorTalkColor) },
	{ "SayLine", LUA_OPCODE(Lua_V1, SayLine) },
	{ "PrintLine", LUA_OPCODE(Lua_V1, PrintLine) },
	{ "MakeTextObject", LUA_OPCODE(Lua_V1, MakeTextObject) },
	{ "GetTextObjectDimensions", LUA_OPCODE(Lua_V1, GetTextObjectDimensions) },
	{ "GetFontDimensions", LUA_OPCODE(Lua_V1, GetFontDimensions) },
	{ "ChangeTextObject", LUA_OPCODE(Lua_V1, ChangeTextObject) },
	{ "KillTextObject", LUA_OPCODE(Lua_V1, KillTextObject) },
	{ "BlastText", LUA_OPCODE(Lua_V1, BlastText) },
	{ "ExpireText", LUA_OPCODE(Lua_V1, ExpireText) },
	{ "PurgeText", LUA_OPCODE(Lua_V1, PurgeText) },
	{ "MakeColor", LUA_OPCODE(Lua_V1, MakeColor) },
	{ "GetColorComponents", LUA_OPCODE(Lua_V1, GetColorComponents) },
	{ "SetTranslationMode", LUA_OPCODE(Lua_V1, SetTranslationMode) },
	{ "GetTranslationMode", LUA_OPCODE(Lua_V1, GetTranslationMode) },
	{ "GetTextCharPosition", LUA_OPCODE(Lua_V1, GetTextCharPosition) },
	{ "LocalizeString", LUA_OPCODE(Lua_V1, LocalizeString) },
	{ "SetEmergencyFont", LUA_OPCODE(Lua_V1, SetEmergencyFont) },
	{ "SetOffscreenTextPos", LUA_OPCODE(Lua_V1, SetOffscreenTextPos) }
};

struct luaL_reg grimPrimitivesOpcodes[] = {
	{ "DrawLine", LUA_OPCODE(Lua_V1, DrawLine) },
	{ "DrawPolygon", LUA_OPCODE(Lua_V1, DrawPolygon) },
	{ "DrawRectangle", LUA_OPCODE(Lua_V1, DrawRectangle) },
	{ "ChangePrimitive", LUA_OPCODE(Lua_V1, ChangePrimitive) },
	{ "KillPrimitive", LUA_OPCODE(Lua_V1, KillPrimitive) },
	{ "PurgePrimitiveQueue", LUA_OPCODE(Lua_V1, PurgePrimitiveQueue) }
};

struct luaL_reg grimHardwareOpcodes[] = {
	{ "Is3DHardwareEnabled", LUA_OPCODE(Lua_V1, Is3DHardwareEnabled) },
	{ "GetVideoDevices", LUA_OPCODE(Lua_V1, GetVideoDevices) },
	{ "SetVideoDevices", LUA_OPCODE(Lua_V1, SetVideoDevices) },
	{ "SetHardwareState", LUA_OPCODE(Lua_V1, SetHardwareState) },
	{ "Enumerate3DDevices", LUA_OPCODE(Lua_V1, Enumerate3DDevices) },
	{ "EnumerateVideoDevices", LUA_OPCODE(Lua_V1, EnumerateVideoDevices) }
};

// Opcode tables of the demo executable. It has no primitive and no hardware opcodes
struct luaL_reg grimDemoMainOpcodes[] = {
	{ "EnginePause", LUA_OPCODE(Lua_V1, EnginePause) },
	{ "EngineResume", LUA_OPCODE(Lua_V1, EngineResume) },
	{ "SetActorColormap", LUA_OPCODE(Lua_V1, SetActorColormap) },
	{ "GetActorCostume", LUA_OPCODE(Lua_V1, GetActorCostume) },
	{ "SetActorCostume", LUA_OPCODE(Lua_V1, SetActorCostume) },
	{ "GetActorScale", LUA_OPCODE(Lua_V1, GetActorScale) },
	{ "SetActorScale", LUA_OPCODE(Lua_V1, SetActorScale) },
	{ "GetActorNodeLocation", LUA_OPCODE(Lua_V1, GetActorNodeLocation) },
	{ "SetActorWalkChore", LUA_OPCODE(Lua_V1, SetActorWalkChore) },
	{ "SetActorTurnChores", LUA_OPCODE(Lua_V1, SetActorTurnChores) },
	{ "SetActorRestChore", LUA_OPCODE(Lua_V1, SetActorRestChore) },
	{ "SetActorMumblechore", LUA_OPCODE(Lua_V1, SetActorMumblechore) },
	{ "SetActorTalkChore", LUA_OPCODE(Lua_V1, SetActorTalkChore) },
	{ "SetActorWalkRate", LUA_OPCODE(Lua_V1, SetActorWalkRate) },
	{ "GetActorWalkRate", LUA_OPCODE(Lua_V1, GetActorWalkRate) },
	{ "SetActorTurnRate", LUA_OPCODE(Lua_V1, SetActorTurnRate) },
	{ "GetActorTurnRate", LUA_OPCODE(Lua_V1, GetActorTurnRate) },
	{ "SetSelectedActor", LUA_OPCODE(Lua_V1, SetSelectedActor) },
	{ "LoadActor", LUA_OPCODE(Lua_V1, LoadActor) },
	{ "GetActorPos", LUA_OPCODE(Lua_V1, GetActorPos) },
	{ "GetActorRect", LUA_OPCODE(Lua_V1, GetActorRect) },
	{ "GetActorPuckVector", LUA_OPCODE(Lua_V1, GetActorPuckVector) },
	{ "PutActorAtInterest", LUA_OPCODE(Lua_V1, PutActorAtInterest) },
	{ "PutActorAtOrigin", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "PutActorAt", LUA_OPCODE(Lua_V1, PutActorAt) },
	{ "PutActorInSet", LUA_OPCODE(Lua_V1, PutActorInSet) },
	{ "GetClippedPos", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "WalkActorVector", LUA_OPCODE(Lua_V1, WalkActorVector) },
	{ "WalkActorForward", LUA_OPCODE(Lua_V1, WalkActorForward) },
	{ "DriveActorTo", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "WalkActorTo", LUA_OPCODE(Lua_V1, WalkActorTo) },
	{ "WalkActorToInterest", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "ActorLookAt", LUA_OPCODE(Lua_V1, ActorLookAt) },
	{ "IsActorLooking", LUA_OPCODE(Lua_V1, IsActorLooking) },
	{ "SetActorLookRate", LUA_OPCODE(Lua_V1, SetActorLookRate) },
	{ "GetActorLookRate", LUA_OPCODE(Lua_V1, SetActorLookRate) }, // same function as SetActorLookRate
	{ "CanActorSee", LUA_OPCODE(Lua_V1, CanActorSee) },
	{ "GetVisibleThings", LUA_OPCODE(Lua_V1, GetVisibleThings) },
	{ "GetCameraActor", LUA_OPCODE(Lua_V1, GetCameraActor) },
	{ "SetActorHead", LUA_OPCODE(Lua_V1, SetActorHead) },
	{ "SetActorVisibility", LUA_OPCODE(Lua_V1, SetActorVisibility) },
	{ "SetActorFollowBoxes", LUA_OPCODE(Lua_V1, SetActorFollowBoxes) },
	{ "ShutUpActor", LUA_OPCODE(Lua_V1, ShutUpActorDemo) },
	{ "IsActorInSector", LUA_OPCODE(Lua_V1, IsActorInSector) },
	{ "GetActorSector", LUA_OPCODE(Lua_V1, GetActorSector) },
	{ "IsPointInSector", LUA_OPCODE(Lua_V1, IsPointInSector) },
	{ "GetPointSector", LUA_OPCODE(Lua_V1, GetPointSector) },
	{ "TurnActor", LUA_OPCODE(Lua_V1, TurnActor) },
	{ "GetActorRot", LUA_OPCODE(Lua_V1, GetActorRot) },
	{ "SetActorRot", LUA_OPCODE(Lua_V1, SetActorRot) },
	{ "IsActorTurning", LUA_OPCODE(Lua_V1, IsActorTurning) },
	{ "PlayActorChore", LUA_OPCODE(Lua_V1, PlayActorChore) },
	{ "PlayActorChoreLooping", LUA_OPCODE(Lua_V1, PlayActorChoreLooping) },
	{ "StopActorChore", LUA_OPCODE(Lua_V1, StopActorChore) },
	{ "CompleteActorChore", LUA_OPCODE(Lua_V1, CompleteActorChore) },
	{ "IsActorMoving", LUA_OPCODE(Lua_V1, IsActorMoving) },
	{ "IsActorChoring", LUA_OPCODE(Lua_V1, IsActorChoring) },
	{ "IsActorResting", LUA_OPCODE(Lua_V1, IsActorResting) },
	{ "SetActorChoreLooping", LUA_OPCODE(Lua_V1, SetActorChoreLooping) },
	{ "GetActorChores", LUA_OPCODE(Lua_V1, GetActorChores) },
	{ "GetActorLoopingChores", LUA_OPCODE(Lua_V1, GetActorLoopingChores) },
	{ "GetActorCostumeDepth", LUA_OPCODE(Lua_V1, GetActorCostumeDepth) },
	{ "WorldToScreen", LUA_OPCODE(Lua_V1, WorldToScreen) },
	{ "ScreenToWorld", LUA_OPCODE(Lua_V1, ScreenToWorld) },
	{ "exit", LUA_OPCODE(Lua_V1, Exit) },
	{ "EnableDebugKeys", LUA_OPCODE(Lua_V1, EnableDebugKeys) },
	{ "LockSet", LUA_OPCODE(Lua_V1, LockSetDemo) },
	{ "UnlockSet", LUA_OPCODE(Lua_V1, UnlockSetDemo) },
	{ "LockFont", LUA_OPCODE(Lua_V1, LockFont) },
	{ "UnLockFont", LUA_OPCODE(Lua_V1, UnLockFontDemo) },
	{ "LockSound", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "UnlockSound", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "LockCostume", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "UnlockCostume", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "EnableControl", LUA_OPCODE(Lua_V1, EnableControl) },
	{ "DisableControl", LUA_OPCODE(Lua_V1, DisableControl) },
	{ "GetControlState", LUA_OPCODE(Lua_V1, GetControlState) },
	{ "PrintMessage", LUA_OPCODE(Lua_V1, PrintMessage) },
	{ "PrintError", LUA_OPCODE(Lua_V1, PrintDebug) },
	{ "PrintWarning", LUA_OPCODE(Lua_V1, PrintDebug) },
	{ "PrintDebug", LUA_OPCODE(Lua_V1, PrintDebug) },
	{ "MakeCurrentSet", LUA_OPCODE(Lua_V1, MakeCurrentSet) },
	{ "MakeCurrentSetup", LUA_OPCODE(Lua_V1, MakeCurrentSetup) },
	{ "GetCurrentSetup", LUA_OPCODE(Lua_V1, GetCurrentSetup) },
	{ "NextSetup", LUA_OPCODE(Lua_V1, NextSetup) },
	{ "PreviousSetup", LUA_OPCODE(Lua_V1, PreviousSetup) },
	{ "StartFullscreenMovie", LUA_OPCODE(Lua_V1, StartFullscreenMovieDemo) },
	{ "IsFullscreenMoviePlaying", LUA_OPCODE(Lua_V1, IsFullscreenMoviePlayingDemo) },
	{ "StartMovie", LUA_OPCODE(Lua_V1, StartMovieDemo) },
	{ "StopMovie", LUA_OPCODE(Lua_V1, StopMovie) },
	{ "PauseMovie", LUA_OPCODE(Lua_V1, PauseMovie) },
	{ "IsMoviePlaying", LUA_OPCODE(Lua_V1, IsMoviePlaying) },
	{ "PlaySound", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "PlaySoundAt", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "PlaySoundAttached", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "IsSoundPlaying", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "SetSoundPosition", LUA_OPCODE(Lua_V1, SetSoundPositionDemo) },
	{ "FileFindFirst", LUA_OPCODE(Lua_V1, FileFindFirst) },
	{ "FileFindNext", LUA_OPCODE(Lua_V1, FileFindNext) },
	{ "FileFindDispose", LUA_OPCODE(Lua_V1, FileFindDispose) },
	{ "InputDialog", LUA_OPCODE(Lua_V1, InputDialog) },
	{ "QueryDialog", LUA_OPCODE(Lua_V1, QueryDialog) },
	{ "WriteRegistryValue", LUA_OPCODE(Lua_V1, WriteRegistryValue) },
	{ "ReadRegistryValue", LUA_OPCODE(Lua_V1, ReadRegistryValue) },
	{ "GetSectorVertices", LUA_OPCODE(LuaBase, dummyHandler) },
	{ "GetSectorOppositeEdge", LUA_OPCODE(Lua_V1, GetSectorOppositeEdge) },
	{ "IsSectorActive", LUA_OPCODE(Lua_V1, IsSectorActive) },
	{ "MakeSectorActive", LUA_OPCODE(Lua_V1, MakeSectorActive) },
	{ "PreRender", LUA_OPCODE(Lua_V1, PreRender) },
	{ "SpewStartup", LUA_OPCODE(Lua_V1, SpewStartup) },
	{ "SpewShutdown", LUA_OPCODE(Lua_V1, SpewShutdown) },
	{ "GetCurrentScript", LUA_OPCODE(Lua_V1, GetCurrentScript) },
	{ "PrintActorCostumes", LUA_OPCODE(Lua_V1, PrintActorCostumes) },
	{ "PushActorCostume", LUA_OPCODE(Lua_V1, PushActorCostume) },
	{ "PopActorCostume", LUA_OPCODE(Lua_V1, PopActorCostume) },
	{ "LoadCostume", LUA_OPCODE(Lua_V1, LoadCostume) },
	{ "SetMemoryUsage", LUA_OPCODE(Lua_V1, SetMemoryUsage) },
	{ "GetCameraPosition", LUA_OPCODE(Lua_V1, GetCameraPosition) },
	{ "SetCameraPosition", LUA_OPCODE(Lua_V1, SetCameraPosition) },
	{ "GetCameraInterest", LUA_OPCODE(Lua_V1, GetCameraInterest) },
	{ "SetCameraInterest", LUA_OPCODE(Lua_V1, SetCameraInterest) },
	{ "GetCameraFOV", LUA_OPCODE(Lua_V1, GetCameraFOV) },
	{ "SetCameraFOV", LUA_OPCODE(Lua_V1, SetCameraFOV) },
	{ "GetCameraRoll", LUA_OPCODE(Lua_V1, GetCameraRoll) },
	{ "SetCameraRoll", LUA_OPCODE(Lua_V1, SetCameraRoll) },
	{ "GetCameraLookVector", LUA_OPCODE(Lua_V1, GetCameraLookVector) },
	{ "PointActorAt", LUA_OPCODE(Lua_V1, PointActorAt) },
	{ "TurnActorTo", LUA_OPCODE(Lua_V1, TurnActorTo) },
	{ "PerSecond", LUA_OPCODE(Lua_V1, PerSecond) },
	{ "SnapToPlane", LUA_OPCODE(Lua_V1, SnapToPlane) },
	{ "GetAngleBetweenVectors", LUA_OPCODE(Lua_V1, GetAngleBetweenVectorsDemo) },
	{ "GetAngleBetweenActors", LUA_OPCODE(Lua_V1, GetAngleBetweenActors) },
	{ "SetAmbientLight", LUA_OPCODE(Lua_V1, SetAmbientLight) },
	{ "FindLight", LUA_OPCODE(Lua_V1, FindLight) },
	{ "TurnLightOn", LUA_OPCODE(Lua_V1, TurnLightOn) },
	{ "IsLightOn", LUA_OPCODE(Lua_V1, IsLightOn) },
	{ "SetLightType", LUA_OPCODE(Lua_V1, SetLightType) },
	{ "GetLightType", LUA_OPCODE(Lua_V1, GetLightType) },
	{ "SetLightPosition", LUA_OPCODE(Lua_V1, SetLightPosition) },
	{ "GetLightPosition", LUA_OPCODE(Lua_V1, GetLightPosition) },
	{ "SetLightRotation", LUA_OPCODE(Lua_V1, SetLightRotation) },
	{ "GetLightRotation", LUA_OPCODE(Lua_V1, GetLightRotation) },
	{ "SetLightAngles", LUA_OPCODE(Lua_V1, SetLightAngles) },
	{ "GetLightAngles", LUA_OPCODE(Lua_V1, GetLightAngles) },
	{ "SetLightIntensity", LUA_OPCODE(Lua_V1, SetLightIntensity) },
	{ "GetLightIntensity", LUA_OPCODE(Lua_V1, GetLightIntensity) },
	{ "PointLightAt", LUA_OPCODE(Lua_V1, PointLightAt) },
	{ "LightMgrStartup", LUA_OPCODE(Lua_V1, LightMgrStartup) },
	{ "LightMgrShutdown", LUA_OPCODE(Lua_V1, LightMgrShutdown) },
	{ "LightMgrSetChange", LUA_OPCODE(Lua_V1, LightMgrSetChange) },
	{ "ImStartSound", LUA_OPCODE(Lua_V1, ImStartSound) },
	{ "ImStopSound", LUA_OPCODE(Lua_V1, ImStopSound) },
	{ "ImStopAllSounds", LUA_OPCODE(Lua_V1, ImStopAllSounds) },
	{ "ImGetParam", LUA_OPCODE(Lua_V1, ImGetParam) },
	{ "ImSetParam", LUA_OPCODE(Lua_V1, ImSetParam) },
	{ "ImFadeParam", LUA_OPCODE(Lua_V1, ImFadeParam) },
	{ "ImGetMasterVol", LUA_OPCODE(Lua_V1, ImGetMasterVol) },
	{ "ImSetMasterVol", LUA_OPCODE(Lua_V1, ImSetMasterVol) },
	{ "ImGetSfxVol", LUA_OPCODE(Lua_V1, ImGetSfxVol) },
	{ "ImSetSfxVol", LUA_OPCODE(Lua_V1, ImSetSfxVol) },
	{ "ImGetVoiceVol", LUA_OPCODE(Lua_V1, ImGetVoiceVol) },
	{ "ImSetVoiceVol", LUA_OPCODE(Lua_V1, ImSetVoiceVol) },
	{ "ImGetMusicVol", LUA_OPCODE(Lua_V1, ImGetMusicVol) },
	{ "ImSetMusicVol", LUA_OPCODE(Lua_V1, ImSetMusicVol) },
	{ "ImStartVoice", LUA_OPCODE(Lua_V1, ImStartVoice) },
	{ "ImStopVoice", LUA_OPCODE(Lua_V1, ImStopVoice) },
	{ "ImSetState", LUA_OPCODE(Lua_V1, ImSetState) },
	{ "ImSetSequence", LUA_OPCODE(Lua_V1, ImSetSequence) },
	{ "ImSetAttribute", LUA_OPCODE(Lua_V1, ImSetAttribute) },
	{ "ImSetCuePoint", LUA_OPCODE(Lua_V1, ImSetCuePoint) },
	{ "ImPause", LUA_OPCODE(Lua_V1, ImPause) },
	{ "ImResume", LUA_OPCODE(Lua_V1, ImResume) },
	{ "LoadBundle", LUA_OPCODE(Lua_V1, LoadBundleDemo) },
	{ "UnloadBundle", LUA_OPCODE(Lua_V1, UnloadBundleDemo) },
	{ "SetGamma", LUA_OPCODE(Lua_V1, SetGamma) },
	{ "SetActorWalkDominate", LUA_OPCODE(Lua_V1, SetActorWalkDominate) },
	{ "SetActorConstrain", LUA_OPCODE(Lua_V1, SetActorConstrain) },
	{ "RenderModeUser", LUA_OPCODE(Lua_V1, RenderModeUser) },
	{ "DimScreen", LUA_OPCODE(Lua_V1, DimScreen) },
	{ "CleanBuffer", LUA_OPCODE(Lua_V1, CleanBuffer) },
	{ "Display", LUA_OPCODE(Lua_V1, Display) },
	{ "SetSpeechMode", LUA_OPCODE(Lua_V1, SetSpeechMode) },
	{ "GetSpeechMode", LUA_OPCODE(Lua_V1, GetSpeechMode) },
	{ "SetShadowColor", LUA_OPCODE(Lua_V1, SetShadowColorDemo) },
	{ "ActivateActorShadow", LUA_OPCODE(Lua_V1, ActivateActorShadow) },
	{ "ActorShadow", LUA_OPCODE(Lua_V1, ActorShadow) },
	{ "SetActorShadowPlane", LUA_OPCODE(Lua_V1, SetActorShadowPlane) },
	{ "SetActorShadowPoint", LUA_OPCODE(Lua_V1, SetActorShadowPoint) },
	{ "SetActiveShadow", LUA_OPCODE(Lua_V1, SetActiveShadow) },
	{ "ActorDistToShadowPlane", LUA_OPCODE(Lua_V1, ActorDistToShadowPlane) },
	{ "KillActorShadows", LUA_OPCODE(Lua_V1, KillActorShadows) },
	{ "AddShadowPlane", LUA_OPCODE(Lua_V1, AddShadowPlane) },
	{ "SetActorShadowValid", LUA_OPCODE(Lua_V1, SetActorShadowValid) },
	{ "FlushControls", LUA_OPCODE(Lua_V1, FlushControls) }
};

static struct luaL_reg grimDemoTextOpcodes[] = {
	{ "IsMessageGoing", LUA_OPCODE(Lua_V1, IsMessageGoing) },
	{ "SetSayLineDefaults", LUA_OPCODE(Lua_V1, SetSayLineDefaults) },
	{ "SetActorTalkColor", LUA_OPCODE(Lua_V1, SetActorTalkColor) },
	{ "SayLine", LUA_OPCODE(Lua_V1, SayLineDemo) },
	{ "PrintLine", LUA_OPCODE(Lua_V1, PrintLine) },
	{ "MakeTextObject", LUA_OPCODE(Lua_V1, MakeTextObject) },
	{ "ChangeTextObject", LUA_OPCODE(Lua_V1, ChangeTextObject) },
	{ "KillTextObject", LUA_OPCODE(Lua_V1, KillTextObject) },
	{ "BlastText", LUA_OPCODE(Lua_V1, BlastText) },
	{ "ExpireText", LUA_OPCODE(Lua_V1, ExpireText) },
	{ "PurgeText", LUA_OPCODE(Lua_V1, PurgeText) },
	{ "MakeColor", LUA_OPCODE(Lua_V1, MakeColor) }
};

struct luaL_reg grimDemoCostumeOpcodes[] = {
	{ "GetKeyLength", LUA_OPCODE(Lua_V1, GetKeyLength) }
};

Lua_V1::Lua_V1(bool isEmi, bool openState) : LuaBase(isEmi, openState), _hs(nullptr), _initialized(0), _debugToggleFunc(nullptr), _totSaveBytes(0), _nextImageHandle(1), _nextObjectStateHandle(1),
		_resolutionScaling(0.0f), _advancedLighting(0), _controlEventCount(0), _lastInputMode(0) {
	for (int i = 0; i < kMaxImages; i++) {
		_imageHandles[i] = 0;
		_images[i] = nullptr;
	}

	lua_FuncIDsSetRemastered(false);
}

void Lua_V1::registerMainOpcodes() {
	if (g_grim->getGameFlags() & ADGF_DEMO)
		luaL_openlib(grimDemoMainOpcodes, ARRAYSIZE(grimDemoMainOpcodes));
	else
		luaL_openlib(grimMainOpcodes, ARRAYSIZE(grimMainOpcodes));
}

void Lua_V1::registerCostumeOpcodes() {
	if (g_grim->getGameFlags() & ADGF_DEMO)
		luaL_openlib(grimDemoCostumeOpcodes, ARRAYSIZE(grimDemoCostumeOpcodes));
	else
		luaL_openlib(grimCostumeOpcodes, ARRAYSIZE(grimCostumeOpcodes));
}

void Lua_V1::registerTextOpcodes() {
	if (g_grim->getGameFlags() & ADGF_DEMO)
		luaL_openlib(grimDemoTextOpcodes, ARRAYSIZE(grimDemoTextOpcodes));
	else
		luaL_openlib(grimTextOpcodes, ARRAYSIZE(grimTextOpcodes));
}

void Lua_V1::registerPrimitiveOpcodes() {
	if (g_grim->getGameFlags() & ADGF_DEMO)
		return;

	luaL_openlib(grimPrimitivesOpcodes, ARRAYSIZE(grimPrimitivesOpcodes));
}

void Lua_V1::registerHardwareOpcodes() {
	if (g_grim->getGameFlags() & ADGF_DEMO)
		return;

	luaL_openlib(grimHardwareOpcodes, ARRAYSIZE(grimHardwareOpcodes));
}

int Lua_V1::initialize(SysStruct *hs) {
	_hs = hs;
	_initialized = 1;

	return 0;
}

int Lua_V1::terminate() {
	for (uint i = 0; i < _lockedFontRefs.size(); i++) {
		g_grim->_res->removeReference(_lockedFontRefs[i]);
		delete _lockedFontRefs[i];
	}

	_lockedFontRefs.clear();
	_hs = nullptr;

	if (_initialized)
		_initialized = 0;

	return 0;
}

int Lua_V1::startup() {
	if (g_grim->isRemastered()) {
		lua_open(false, true);
		lua_iolibopen();
		lua_strlibopen();
		lua_mathlibopen();
	}

	registerMainOpcodes();
	recognizeOpcodes();
	makeSystemTable();
	registerTypeOverride();

	if (g_grim->getGameFlags() & ADGF_DEMO)
		forceDemo();

	registerSectorConstants();
	registerConcatFallback();

	lua_savehook = saveIndirector;
	lua_restorehook = restoreIndirector;

	g_grim->_sputControl->addCallback(this);

	return 0;
}

int Lua_V1::shutdown() {
	bool remastered = g_grim->isRemastered();

	if (remastered)
		g_grim->_sputControl->clearCallbacks();

	FileFindDispose();

	lua_unref(_refSystemTable);
	lua_savehook = nullptr;
	lua_restorehook = nullptr;

	if (remastered) {
		lua_close();
		lua_removelibslists();
		lua_iolibclose();
	}

	return 0;
}

int Lua_V1::findSystemFiles() {
	return dofile("_system.lua") == 0;
}

int Lua_V1::boot(int saveGameResume, const char *bootParam) {
	if (saveGameResume)
		lua_pushnumber(1);
	else
		lua_pushnil();

	if (bootParam)
		lua_pushstring(bootParam);
	else
		lua_pushnil();

	return lua_call("BOOT") == 0;
}

int Lua_V1::bootSecondStage(const char *defaultSet) {
	lua_pushstring(defaultSet);
	lua_pushnil();

	return lua_call("BOOTTWO") == 0;
}

void Lua_V1::update() {
	int movieTime = g_grim->_sputSmush->getMovieTime();
	int frameTime = g_grim->_sputTime->getFrameTime();

	LuaBase::update(frameTime, movieTime);
}

void Lua_V1::makeSystemTable() {
	lua_Object systemTable = lua_createtable();
	lua_pushobject(systemTable);
	lua_setglobal("system");

	lua_pushobject(systemTable);
	_refSystemTable = lua_ref(1);

	registerControls();

	if (g_grim->isRemastered()) {
		lua_pushobject(lua_getref(_refSystemTable));
		lua_pushstring("camChangeHandler");
		lua_pushcfunction(LUA_OPCODE(Lua_V1, DefaultCamChangeHandler));
		lua_settable();

		lua_pushobject(lua_getref(_refSystemTable));
		lua_pushstring("axisHandler");
		lua_pushcfunction(LUA_OPCODE(Lua_V1, DefaultControlHandler));
		lua_settable();

		lua_pushobject(lua_getref(_refSystemTable));
		lua_pushstring("inputModeHandler");
		lua_pushcfunction(LUA_OPCODE(Lua_V1, DefaultControlHandler));
		lua_settable();

		lua_pushobject(lua_getref(_refSystemTable));
		lua_pushstring("buttonHandler");
		lua_pushcfunction(LUA_OPCODE(Lua_V1, DefaultControlHandler));
		lua_settable();

		return;
	}

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("camChangeHandler");
	lua_pushcfunction(LUA_OPCODE(LuaBase, dummyHandler));
	lua_settable();

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("axisHandler");
	lua_pushcfunction(LUA_OPCODE(LuaBase, dummyHandler));
	lua_settable();

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("buttonHandler");
	lua_pushcfunction(LUA_OPCODE(LuaBase, dummyHandler));
	lua_settable();
}

void Lua_V1::DefaultCamChangeHandler() {
}

void Lua_V1::DefaultControlHandler() {
}

void Lua_V1::registerControls() {
	lua_Object controlsTable = lua_createtable();

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring("controls");
	lua_pushobject(controlsTable);
	lua_settable();

	ControlSymbol *symbols = g_grim->_sputControl->getSymbols();
	int numSymbols = g_grim->_sputControl->getNumSymbols();

	for (int i = 0; i < numSymbols; i++) {
		lua_pushobject(controlsTable);
		lua_pushstring(symbols[i].name);
		lua_pushnumber((float)i);
		lua_settable();
	}
}

bool Lua_V1::getSystemHandler(const char *name, bool allowTable, lua_Object *funcPointer, lua_Object *self) {
	*funcPointer = LUA_NOOBJECT;
	*self = LUA_NOOBJECT;

	lua_pushobject(lua_getref(_refSystemTable));
	lua_pushstring(name);
	lua_Object handler = lua_gettable();

	if (allowTable && lua_istable(handler)) {
		lua_pushobject(handler);
		lua_pushstring(name);
		lua_Object func = lua_gettable();

		if (!lua_isfunction(func))
			return false;

		*funcPointer = func;
		*self = handler;

		return true;
	}

	if (lua_isfunction(handler)) {
		*funcPointer = handler;

		return true;
	}

	return false;
}

int Lua_V1::controlCallback(float frameTime) {
	StdControl *stdControl = g_grim->_stdControl;
	ControlSymbol *symbols = g_grim->_sputControl->getSymbols();
	int numSymbols = g_grim->_sputControl->getNumSymbols();
	lua_Object buttonFunc;
	lua_Object buttonSelf;
	lua_Object axisFunc;
	lua_Object axisSelf;

	lua_beginblock();

	bool buttonHandler = getSystemHandler("buttonHandler", true, &buttonFunc, &buttonSelf);
	bool axisHandler = getSystemHandler("axisHandler", true, &axisFunc, &axisSelf);

	for (int i = 0; i < numSymbols; i++) {
		ControlSymbol *symbol = &symbols[i];

		if (!(symbol->flags & 1))
			continue;

		if (symbol->flags & 4) {
			int pressCount = 0;
			int doubleClick = 0;
			int value = stdControl->readKey(symbol->code, &pressCount, &doubleClick);

			if (value != symbol->lastValue && buttonHandler) {
				if (buttonSelf != LUA_NOOBJECT)
					lua_pushobject(buttonSelf);

				lua_pushnumber((float)i);

				if (value)
					lua_pushnumber(1);
				else
					lua_pushnil();

				lua_pushnumber((float)(uint)pressCount);
				lua_pushnumber((float)doubleClick);
				lua_callfunction(buttonFunc);
				symbol->lastValue = value;
			}
		} else {
			float value = stdControl->readAxis(symbol->code);
			float lastValue;

			if (symbol->flags & 8)
				value = -value;

			memcpy(&lastValue, &symbol->lastValue, sizeof(lastValue));

			if (value != lastValue && axisHandler) {
				if (axisSelf != LUA_NOOBJECT)
					lua_pushobject(axisSelf);

				lua_pushnumber((float)i);
				lua_pushnumber(value);
				lua_callfunction(axisFunc);
				memcpy(&symbol->lastValue, &value, sizeof(value));
			}
		}
	}

	lua_endblock();

	return 0;
}

void Lua_V1::setDebugToggleCallback(void (*func)(int)) {
	_debugToggleFunc = func;
}

void Lua_V1::cameraChange(int oldSetup, int newSetup) {
	lua_Object func;
	lua_Object self;

	lua_beginblock();

	if (getSystemHandler("camChangeHandler", false, &func, &self)) {
		lua_pushnumber((float)oldSetup);
		lua_pushnumber((float)newSetup);
		lua_callfunction(func);
	}

	lua_endblock();
}

void Lua_V1::postCameraChange(int setup) {
	lua_Object func;
	lua_Object self;

	lua_beginblock();

	if (getSystemHandler("postCamChangeHandler", false, &func, &self)) {
		lua_pushnumber((float)setup);
		lua_callfunction(func);
	}

	lua_endblock();
}

void Lua_V1::collisionHandler(SputActor *actor, SputActor *other, const float *pos0, const float *pos1, const float *pos2) {
	lua_Object func;
	lua_Object self;

	lua_beginblock();

	if (getSystemHandler("collisionHandler", true, &func, &self)) {
		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_pushusertag(g_grim->_sputActor->getActorHandle(actor), MKTAG('R','T','C','A'));
		lua_pushusertag(g_grim->_sputActor->getActorHandle(other), MKTAG('R','T','C','A'));
		lua_pushnumber(pos0[0]);
		lua_pushnumber(pos0[1]);
		lua_pushnumber(pos0[2]);
		lua_pushnumber(pos1[0]);
		lua_pushnumber(pos1[1]);
		lua_pushnumber(pos1[2]);
		lua_pushnumber(pos2[0]);
		lua_pushnumber(pos2[1]);
		lua_pushnumber(pos2[2]);
		lua_callfunction(func);
	}

	lua_endblock();
}

void Lua_V1::doCostumeOverrideCallback(SputActor *actor, int override) {
	lua_Object func;
	lua_Object self;

	lua_beginblock();

	int actorHandle = g_grim->_sputActor->getActorHandle(actor);

	if (getSystemHandler("costumeOverrideHandler", true, &func, &self)) {
		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_pushusertag(actorHandle, MKTAG('R','T','C','A'));

		if (override)
			lua_pushnumber(1);
		else
			lua_pushnil();

		lua_callfunction(func);
	}

	lua_endblock();
}

void Lua_V1::doCostumeMarkerCallback(SputActor *actor, int markerType) {
	lua_Object func;
	lua_Object self;

	if (!markerType)
		return;

	int actorHandle = g_grim->_sputActor->getActorHandle(actor);

	lua_beginblock();

	if (getSystemHandler("costumeMarkerHandler", true, &func, &self)) {
		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_pushusertag(actorHandle, MKTAG('R','T','C','A'));
		lua_pushnumber((float)(uint)markerType);
		lua_callfunction(func);
	}

	lua_endblock();
}

void Lua_V1::doUserPaintCallback() {
	lua_Object func;
	lua_Object self;

	lua_beginblock();

	if (getSystemHandler("userPaintHandler", true, &func, &self)) {
		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_callfunction(func);
	}

	lua_endblock();
}

int Lua_V1::doExitCallback() {
	lua_Object func;
	lua_Object self;

	lua_beginblock();

	bool found = getSystemHandler("exitHandler", true, &func, &self);

	if (found) {
		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_callfunction(func);
	}

	lua_endblock();

	return found;
}

int Lua_V1::doPauseCallback() {
	lua_Object func;
	lua_Object self;

	lua_beginblock();

	bool found = getSystemHandler("pauseHandler", true, &func, &self);

	if (found) {
		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_callfunction(func);
	}

	lua_endblock();

	return found;
}

int Lua_V1::doCharacterCallback(int character) {
	lua_Object func;
	lua_Object self;
	int result = 0;

	lua_beginblock();

	if (getSystemHandler("characterHandler", true, &func, &self)) {
		char str[2];

		str[0] = (char)(byte)character;
		str[1] = '\0';

		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_pushstring(str);
		lua_callfunction(func);
		result = !lua_isnil(lua_getresult(1));
	}

	lua_endblock();

	return result;
}

void Lua_V1::saveGameCallback() {
	lua_Object func;
	lua_Object self;

	_totSaveBytes = 0;

	lua_beginblock();

	if (getSystemHandler("saveGameCallback", true, &func, &self)) {
		if (self != LUA_NOOBJECT)
			lua_pushobject(self);

		lua_callfunction(func);
	}

	if (!_totSaveBytes)
		g_grim->zwrite(&_totSaveBytes, 4);

	lua_endblock();
}

int32 Lua_V1::saveIndirector(int32 tag, int32 data, SaveRestoreAccess *saveFunc) {
	if (tag == MKTAG('T', 'A', 'T', 'S')) {
		int setup;
		int index;

		if (!g_grim->_sputRoom->getCurrentRoom() || !findObjectState(data, &setup, &index))
			return -1;

		return index | (setup << 16);
	}

	return data;
}

int32 Lua_V1::restoreIndirector(int32 tag, int32 data, SaveRestoreAccess *restoreFunc) {
	if (tag == MKTAG('T', 'A', 'T', 'S')) {
		Room *room = g_grim->_sputRoom->getCurrentRoom();

		if (data == -1 || !room)
			return 0;

		uint setup = (uint)data >> 16;
		uint index = (uint)data & 0xFFFF;

		if (setup >= room->numSetups) {
			warning("Lua_V1::restoreIndirector(): invalid setup %u for an object state", setup);
			return 0;
		}

		SputObjectState *state = room->setups[setup].ObjectStates;

		while (state && index) {
			state = state->next;
			index--;
		}

		if (!state) {
			warning("Lua_V1::restoreIndirector(): object state not found");
			return 0;
		}

		return getObjectStateHandle(state);
	}

	if (tag == MKTAG('T', 'X', 'E', 'T')) {
		if (g_grim->_sputText->findSerialNumber(data))
			return data;

		return 0;
	}

	if (g_grim->isRemastered() && tag == kSputTagFont) {
		if (g_grim->_res->handleFromSerialNumber(data))
			return data;

		return 0;
	}

	return data;
}

} // end of namespace Grim
