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

#include "engines/grim/grim.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/remastered/sputmouse.h"
#include "engines/grim/render/sputprimitive.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"
#include "engines/grim/std/std_bitmap.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

static int getScreenColor(int32 color) {
	const StdColorFormat *format = g_grim->getColorInfo();
	uint value = (uint)color;
	uint red = (value & 0xFF) >> format->redLoss << format->redShift;
	uint green = ((value >> 8) & 0xFF) >> format->greenLoss << format->greenShift;
	uint blue = ((value >> 16) & 0xFF) >> format->blueLoss << format->blueShift;

	return (int)((uint16)green | (uint16)blue | (uint16)red);
}

bool Lua_V1::isImage(lua_Object obj) {
	return lua_isuserdata(obj) && lua_tag(obj) == MKTAG('F','U','B','V');
}

int Lua_V1::findImage(lua_Object obj) {
	int handle = lua_getuserdata(obj);

	for (int i = 0; i < kMaxImages; i++) {
		if (_images[i] && _imageHandles[i] == handle)
			return i;
	}

	warning("Lua_V1::findImage(): invalid image handle %d", handle);

	return -1;
}

void Lua_V1::pushImage(StdVBuffer *image) {
	for (int i = 0; i < kMaxImages; i++) {
		if (!_images[i]) {
			_images[i] = image;
			_imageHandles[i] = _nextImageHandle++;
			lua_pushusertag(_imageHandles[i], MKTAG('F','U','B','V'));
			return;
		}
	}

	warning("Lua_V1::pushImage(): too many images");

	g_grim->_stdDisplay->vBufferFree(image);
	lua_pushnil();
}

void Lua_V1::GetImage() {
	lua_Object nameObj = lua_getparam(1);

	if (lua_isstring(nameObj)) {
		Common::String path(lua_getstring(nameObj));
		StdBitmapStruct *bitmap = g_grim->_stdBitmap->loadImage(path, 0, 0);

		if (bitmap) {
			if (!bitmap->frames || bitmap->numFrames == 0 || !bitmap->frames[0]) {
				warning("Lua_V1::GetImage(): the image has no frames");
				g_grim->_stdBitmap->unloadBitmap(bitmap);
				lua_pushnil();
				return;
			}

			StdVBuffer *image = bitmap->frames[0];

			image->colorKey = 63519;
			bitmap->frames[0] = nullptr;
			g_grim->_stdBitmap->unloadBitmap(bitmap);

			if (image->tex.format.bpp == 16) {
				g_grim->_sputRender->colorCorrect((uint16 *)image->pixels, image->tex.width * image->tex.height);
				g_grim->_sputRender->colorCorrect((uint16 *)&image->colorKey, 1);
			} else {
				warning("Lua_V1::GetImage(): the image is not 16 bpp");
			}

			pushImage(image);
			return;
		}
	}

	lua_pushnil();
}

void Lua_V1::FreeImage() {
	lua_Object imageObj = lua_getparam(1);

	if (isImage(imageObj)) {
		int slot = findImage(imageObj);

		if (slot >= 0) {
			g_grim->_stdDisplay->vBufferFree(_images[slot]);
			_imageHandles[slot] = 0;
		}
	}
}

void Lua_V1::BlastImage() {
	lua_Object imageObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object modeObj = lua_getparam(4);
	char colorKey = (char)(lua_isnil(modeObj) == 0);

	if (isImage(imageObj) && lua_isnumber(xObj) && lua_isnumber(yObj)) {
		int slot = findImage(imageObj);

		if (slot >= 0) {
			int x = luaFtol(lua_getnumber(xObj));
			int y = luaFtol(lua_getnumber(yObj));
			StdVBuffer *dst = g_grim->_sputRender->drawBuffer();

			if (!dst) {
				warning("Lua_V1::BlastImage(): the display is not set");
				return;
			}

			g_grim->_stdDisplay->vBufferCopy(dst, _images[slot], x, y, nullptr, colorKey);
		}
	}
}

void Lua_V1::CleanBuffer() {
	g_grim->_sputRender->cleanDrawBuffer();
}

void Lua_V1::StartFullscreenMovie() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object loopObj = lua_getparam(2);

	if (g_grim->isRemastered() && g_grim->getSputMouse())
		g_grim->getSputMouse()->setVisibleInMovie(false);

	if (lua_isstring(nameObj) && g_grim->playMovie(lua_getstring(nameObj), 1, !lua_isnil(loopObj), -1, -1) == 1)
		pushbool(true);
	else
		pushbool(false);
}

void Lua_V1::StartMovie() {
	lua_Object nameObj = lua_getparam(1);
	lua_Object loopObj = lua_getparam(2);
	lua_Object xObj = lua_getparam(3);
	lua_Object yObj = lua_getparam(4);

	if (lua_isstring(nameObj)) {
		g_grim->getImuse()->soundsFloodMusicBuffer();

		int y = lua_isnumber(yObj) ? luaFtol(lua_getnumber(yObj)) : -1;
		int x = lua_isnumber(xObj) ? luaFtol(lua_getnumber(xObj)) : -1;

		if (g_grim->playMovie(lua_getstring(nameObj), 0, !lua_isnil(loopObj), x, y) == 1) {
			pushbool(true);
			return;
		}
	}

	pushbool(false);
}

void Lua_V1::StartFullscreenMovieDemo() {
	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj)) {
		warning("StartMovieFullScreen: Argument must be a string.");
		lua_pushnil();
		return;
	}

	if (g_grim->playMovie(lua_getstring(nameObj), 1, 0, -1, -1) != 1)
		warning("Start Movie:%s FAIL", lua_getstring(nameObj));

	lua_pushnumber(1.0f);
}

void Lua_V1::StartMovieDemo() {
	lua_Object nameObj = lua_getparam(1);

	if (!lua_isstring(nameObj)) {
		warning("StartMovie: Argument must be a string.");
		lua_pushnil();
		return;
	}

	if (g_grim->playMovie(lua_getstring(nameObj), 0, 0, -1, -1) != 1)
		warning("Start Movie:%s FAIL", lua_getstring(nameObj));

	lua_pushnumber(1.0f);
}

void Lua_V1::IsFullscreenMoviePlaying() {
	if (g_grim->isRemastered()) {
		pushbool(g_grim->getLibMode() == 3 || (g_grim->_sputSmush->videoIsPlayingMovie() && g_grim->_sputSmush->videoIsVisible()));
		return;
	}

	pushbool(g_grim->_sputSmush->moviePlaying() != 0);
}

void Lua_V1::IsFullscreenMoviePlayingDemo() {
	pushbool(g_grim->getLibMode() == 3);
}

void Lua_V1::IsMoviePlaying() {
	pushbool(g_grim->_sputSmush->moviePlaying() != 0);
}

void Lua_V1::StopMovie() {
	g_grim->_sputSmush->stopMovie();
}

void Lua_V1::PauseMovie() {
	if (g_grim->useRemasterFixes() && !(g_grim->getGameFlags() & ADGF_DEMO))
		g_grim->_sputSmush->pause(!lua_isnil(lua_getparam(1)));
	else
		g_grim->_sputSmush->pause(lua_isnil(lua_getparam(1)));
}

void Lua_V1::PurgePrimitiveQueue() {
	PrimitiveQueue *queue = g_grim->_sputPrimitive->getCurrent();

	queue->purge();
	queue->resetId();
}

void Lua_V1::DrawPolygon() {
	PrimitiveQueue *queue = g_grim->_sputPrimitive->getCurrent();
	int layer = 2;
	int color = 0;
	lua_Object pointsObj = lua_getparam(1);
	lua_Object tableObj = lua_getparam(2);

	if (!lua_istable(pointsObj)) {
		lua_pushnil();
		return;
	}

	if (lua_istable(tableObj)) {
		lua_pushobject(tableObj);
		lua_pushstring("color");
		lua_Object colorObj = lua_gettable();
		if (lua_isuserdata(colorObj) && lua_tag(colorObj) == MKTAG('R','L','O','C'))
			color = getScreenColor(lua_getuserdata(colorObj));

		lua_pushobject(tableObj);
		lua_pushstring("layer");
		lua_Object layerObj = lua_gettable();
		if (lua_isnumber(layerObj))
			layer = luaFtol(lua_getnumber(layerObj));
	}

	int numPoints = 0;
	int index = 1;

	while (numPoints < 250) {
		lua_pushobject(pointsObj);
		lua_pushnumber((float)index);

		if (lua_isnil(lua_gettable()))
			break;

		index += 2;
		numPoints++;
	}

	if (numPoints <= 0) {
		lua_pushnil();
		return;
	}

	int *points = new int[numPoints * 2]();

	index = 1;
	for (int i = 0; i < numPoints; i++) {
		lua_pushobject(pointsObj);
		lua_pushnumber((float)index);
		index++;
		points[i * 2] = luaFtol(lua_getnumber(lua_gettable()));

		lua_pushobject(pointsObj);
		lua_pushnumber((float)index);
		index++;
		lua_Object yObj = lua_gettable();
		if (!lua_isnil(yObj))
			points[i * 2 + 1] = luaFtol(lua_getnumber(yObj));
		else
			points[i * 2 + 1] = 0;
	}

	int id = queue->addPolygon(numPoints, points, color, layer);

	delete[] points;
	points = nullptr;

	lua_pushnumber((float)(uint)id);
}

void Lua_V1::DrawLine() {
	PrimitiveQueue *queue = g_grim->_sputPrimitive->getCurrent();
	int layer = 2;
	int color = 0;
	lua_Object x1Obj = lua_getparam(1);
	lua_Object y1Obj = lua_getparam(2);
	lua_Object x2Obj = lua_getparam(3);
	lua_Object y2Obj = lua_getparam(4);
	lua_Object tableObj = lua_getparam(5);

	if (!lua_isnumber(x1Obj) || !lua_isnumber(y1Obj) || !lua_isnumber(x2Obj) || !lua_isnumber(y2Obj)) {
		lua_pushnil();
		return;
	}

	int x1 = luaFtol(lua_getnumber(x1Obj));
	int y1 = luaFtol(lua_getnumber(y1Obj));
	int x2 = luaFtol(lua_getnumber(x2Obj));
	int y2 = luaFtol(lua_getnumber(y2Obj));

	if (lua_istable(tableObj)) {
		lua_pushobject(tableObj);
		lua_pushstring("color");
		lua_Object colorObj = lua_gettable();
		if (lua_isuserdata(colorObj) && lua_tag(colorObj) == MKTAG('R','L','O','C'))
			color = getScreenColor(lua_getuserdata(colorObj));

		lua_pushobject(tableObj);
		lua_pushstring("layer");
		lua_Object layerObj = lua_gettable();
		if (lua_isnumber(layerObj))
			layer = luaFtol(lua_getnumber(layerObj));
	}

	lua_pushnumber((float)(uint)queue->addLine(x1, y1, x2, y2, color, layer));
}

void Lua_V1::ChangePrimitive() {
	PrimitiveQueue *queue = g_grim->_sputPrimitive->getCurrent();
	lua_Object idObj = lua_getparam(1);
	lua_Object tableObj = lua_getparam(2);

	if (!lua_isnumber(idObj) || !lua_istable(tableObj))
		return;

	int id = luaFtol(lua_getnumber(idObj));

	lua_pushobject(tableObj);
	lua_pushstring("color");
	lua_Object colorObj = lua_gettable();
	if (lua_isuserdata(colorObj) && lua_tag(colorObj) == MKTAG('R','L','O','C')) {
		queue->changeColor(id, getScreenColor(lua_getuserdata(colorObj)));

		if (g_grim->isRemastered())
			queue->changeColorRgb(id, (uint)lua_getuserdata(colorObj));
	}

	lua_pushobject(tableObj);
	lua_pushstring("layer");
	lua_Object layerObj = lua_gettable();
	if (lua_isnumber(layerObj))
		queue->changeLayer(id, luaFtol(lua_getnumber(layerObj)));

	lua_pushobject(tableObj);
	lua_pushstring("xoffset");
	lua_Object xObj = lua_gettable();
	lua_pushobject(tableObj);
	lua_pushstring("yoffset");
	lua_Object yObj = lua_gettable();
	if (lua_isnumber(xObj) || lua_isnumber(yObj)) {
		int x = 0;
		int y = 0;
		if (lua_isnumber(xObj))
			x = luaFtol(lua_getnumber(xObj));
		if (lua_isnumber(yObj))
			y = luaFtol(lua_getnumber(yObj));
		queue->move(id, x, y);
	}

	lua_pushobject(tableObj);
	lua_pushstring("x");
	xObj = lua_gettable();
	lua_pushobject(tableObj);
	lua_pushstring("y");
	yObj = lua_gettable();
	if (lua_isnumber(xObj) || lua_isnumber(yObj)) {
		int x = -1;
		int y = -1;
		if (lua_isnumber(xObj))
			x = luaFtol(lua_getnumber(xObj));
		if (lua_isnumber(yObj))
			y = luaFtol(lua_getnumber(yObj));
		queue->moveTo(id, x, y);
	}

	lua_pushobject(tableObj);
	lua_pushstring("x2");
	xObj = lua_gettable();
	lua_pushobject(tableObj);
	lua_pushstring("y2");
	yObj = lua_gettable();
	if (lua_isnumber(xObj) || lua_isnumber(yObj)) {
		int x = -1;
		int y = -1;
		if (lua_isnumber(xObj))
			x = luaFtol(lua_getnumber(xObj));
		if (lua_isnumber(yObj))
			y = luaFtol(lua_getnumber(yObj));
		queue->moveEndpoint(id, x, y);
	}

	lua_pushobject(tableObj);
	lua_pushstring("width");
	lua_Object width = lua_gettable();
	lua_pushobject(tableObj);
	lua_pushstring("height");
	lua_Object height = lua_gettable();
	if (lua_isnumber(width) || lua_isnumber(height)) {
		int w = -1;
		int h = -1;
		if (lua_isnumber(width))
			w = luaFtol(lua_getnumber(width));
		if (lua_isnumber(height))
			h = luaFtol(lua_getnumber(height));
		queue->changeDimensions(id, w, h);
	}
}

void Lua_V1::DrawRectangle() {
	PrimitiveQueue *queue = g_grim->_sputPrimitive->getCurrent();
	int layer = 2;
	int color = 0;
	int filled = 0;
	uint colorRgb = 0;
	float alpha = 1.0f;
	lua_Object x1Obj = lua_getparam(1);
	lua_Object y1Obj = lua_getparam(2);
	lua_Object x2Obj = lua_getparam(3);
	lua_Object y2Obj = lua_getparam(4);
	lua_Object tableObj = lua_getparam(5);

	if (!lua_isnumber(x1Obj) || !lua_isnumber(y1Obj) || !lua_isnumber(x2Obj) || !lua_isnumber(y2Obj)) {
		lua_pushnil();
		return;
	}

	int x1 = luaFtol(lua_getnumber(x1Obj));
	int y1 = luaFtol(lua_getnumber(y1Obj));
	int x2 = luaFtol(lua_getnumber(x2Obj));
	int y2 = luaFtol(lua_getnumber(y2Obj));

	if (lua_istable(tableObj)) {
		lua_pushobject(tableObj);
		lua_pushstring("color");
		lua_Object colorObj = lua_gettable();
		if (lua_isuserdata(colorObj) && lua_tag(colorObj) == MKTAG('R','L','O','C')) {
			color = getScreenColor(lua_getuserdata(colorObj));
			colorRgb = (uint)lua_getuserdata(colorObj);
		}

		lua_pushobject(tableObj);
		lua_pushstring("filled");
		lua_Object filledObj = lua_gettable();
		if (!lua_isnil(filledObj))
			filled = 1;

		lua_pushobject(tableObj);
		lua_pushstring("layer");
		lua_Object layerObj = lua_gettable();
		if (lua_isnumber(layerObj))
			layer = luaFtol(lua_getnumber(layerObj));

		if (g_grim->isRemastered()) {
			lua_pushobject(tableObj);
			lua_pushstring("alpha");
			lua_Object alphaObj = lua_gettable();
			if (lua_isnumber(alphaObj))
				alpha = lua_getnumber(alphaObj);
		}
	}

	int id = queue->addRect(x1, y1, x2, y2, color, filled, layer);

	if (g_grim->isRemastered()) {
		queue->changeColorRgb(id, colorRgb);
		queue->setAlpha(id, alpha);
	}

	lua_pushnumber((float)(uint)id);
}

void Lua_V1::BlastRect() {
	lua_Object x1Obj = lua_getparam(1);
	lua_Object y1Obj = lua_getparam(2);
	lua_Object x2Obj = lua_getparam(3);
	lua_Object y2Obj = lua_getparam(4);
	lua_Object tableObj = lua_getparam(5);
	uint16 color = 0xFFFF;
	int filled = 0;

	if (!lua_isnumber(x1Obj) || !lua_isnumber(y1Obj) || !lua_isnumber(x2Obj) || !lua_isnumber(y2Obj)) {
		lua_pushnil();
		return;
	}

	int x1 = luaFtol(lua_getnumber(x1Obj));
	int y1 = luaFtol(lua_getnumber(y1Obj));
	int x2 = luaFtol(lua_getnumber(x2Obj));
	int y2 = luaFtol(lua_getnumber(y2Obj));

	if (lua_istable(tableObj)) {
		lua_pushobject(tableObj);
		lua_pushstring("color");
		lua_Object colorObj = lua_gettable();
		if (lua_isuserdata(colorObj) && lua_tag(colorObj) == MKTAG('R','L','O','C')) {
			uint value = (uint)lua_getuserdata(colorObj);

			color = (uint16)(((value >> 19) & 0x1F) | (((value >> 10) & 0x3F) << 5) | (((value >> 3) & 0x1F) << 11));
			g_grim->_sputRender->colorCorrect(&color, 1);
		}

		lua_pushobject(tableObj);
		lua_pushstring("filled");
		lua_Object filledObj = lua_gettable();
		if (!lua_isnil(filledObj))
			filled = 1;
	}

	if (x2 < x1 || y2 < y1 || x1 < 0 || y1 < 0 || x2 > 639 || y2 > 479)
		return;

	StdVBuffer *buffer = g_grim->_sputRender->drawBuffer();

	if (!buffer || buffer->tex.format.bpp != 16 || buffer->tex.width < 640 || buffer->tex.height < 480) {
		warning("Lua_V1::BlastRect(): unsupported draw buffer");
		return;
	}

	int width = x2 - x1 + 1;
	int height = y2 - y1 + 1;

	g_grim->_stdDisplay->vBufferLock(buffer);

	if (!buffer->pixels) {
		warning("Lua_V1::BlastRect(): the draw buffer could not be locked");
		g_grim->_stdDisplay->vBufferUnlock(buffer);
		return;
	}

	int pitch = buffer->tex.pitchInBytes;
	byte *rowPointer = buffer->pixels + pitch * y1 + x1 * 2;
	uint16 *pixel = (uint16 *)rowPointer;

	for (int i = 0; i < width; i++)
		pixel[i] = color;

	if (height > 2) {
		for (int row = 0; row < height - 2; row++) {
			rowPointer += pitch;
			pixel = (uint16 *)rowPointer;

			int count = filled ? width - 1 : 1;

			for (int i = 0; i < count; i++)
				pixel[i] = color;

			pixel[width - 1] = color;
		}
	}

	if (height > 1) {
		rowPointer += pitch;
		pixel = (uint16 *)rowPointer;

		for (int i = 0; i < width; i++)
			pixel[i] = color;
	}

	g_grim->_stdDisplay->vBufferUnlock(buffer);
}

void Lua_V1::KillPrimitive() {
	lua_Object idObj = lua_getparam(1);

	if (lua_isnumber(idObj))
		g_grim->_sputPrimitive->getCurrent()->deletePrimitive(luaFtol(lua_getnumber(idObj)));
}

void Lua_V1::DimScreen() {
	// The demo dims to half, not to a quarter
	g_grim->_sputRender->dim(0, (g_grim->getGameFlags() & ADGF_DEMO) ? 1 : 2);
}

void Lua_V1::DimRegion() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object widthObj = lua_getparam(3);
	lua_Object heightObj = lua_getparam(4);
	lua_Object scaleObj = lua_getparam(5);
	int x = luaFtol(lua_getnumber(xObj));
	int y = luaFtol(lua_getnumber(yObj));
	int width = luaFtol(lua_getnumber(widthObj));
	int height = luaFtol(lua_getnumber(heightObj));

	g_grim->_sputRender->dimRegion(x, y, width, height, lua_getnumber(scaleObj));
}

void Lua_V1::ScreenShot() {
	lua_Object widthObj = lua_getparam(1);
	lua_Object heightObj = lua_getparam(2);
	int width = 640;
	int height = 480;

	if (!lua_isnil(widthObj) && lua_isnumber(widthObj))
		width = luaFtol(lua_getnumber(widthObj));

	if (!lua_isnil(heightObj) && lua_isnumber(heightObj))
		height = luaFtol(lua_getnumber(heightObj));

	StdVBuffer *image = g_grim->_sputRender->screenShot(width, height);

	if (image)
		pushImage(image);
	else
		lua_pushnil();
}

void Lua_V1::SetGamma() {
	lua_Object levelObj = lua_getparam(1);

	if (!lua_isnumber(levelObj))
		return;

	if (g_grim->getLibMode() != 4)
		g_grim->_sputRender->setGamma(lua_getnumber(levelObj));
}

void Lua_V1::Display() {
	g_grim->_sputRender->pageFlip();
}

void Lua_V1::EngineDisplay() {
	g_grim->_sputRender->setPageFlip(luaFtol(lua_getnumber(lua_getparam(1))));
}

void Lua_V1::ForceRefresh() {
	g_grim->_sputRender->forceRefresh();
}

void Lua_V1::RenderModeUser() {
	lua_Object modeObj = lua_getparam(1);

	if (lua_isnil(modeObj) && g_grim->getLibMode() == 4) {
		g_grim->_sputSmush->pause(0);
		g_grim->_sputRender->forceRefresh();
		g_grim->setLibMode(g_grim->getPrevMode());
	} else if ((!lua_isnil(modeObj) || (g_grim->getGameFlags() & ADGF_DEMO)) && g_grim->getLibMode() != 4) {
		// The demo enters the user mode also when the argument is nil
		g_grim->setPrevMode(g_grim->getLibMode());
		g_grim->_sputSmush->pause(1);
		g_grim->setLibMode(4);
	}
}

void Lua_V1::IrisUp() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object timeObj = lua_getparam(3);
	int time = luaFtol(lua_getnumber(timeObj));
	int y = luaFtol(lua_getnumber(yObj));
	int x = luaFtol(lua_getnumber(xObj));

	g_grim->_sputRender->irisUp(x, y, time);
}

void Lua_V1::IrisDown() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object timeObj = lua_getparam(3);
	int time = luaFtol(lua_getnumber(timeObj));
	int y = luaFtol(lua_getnumber(yObj));
	int x = luaFtol(lua_getnumber(xObj));

	g_grim->_sputRender->irisDown(x, y, time);
}

void Lua_V1::IrisClear() {
	g_grim->_sputRender->clearIris();
}

void Lua_V1::PreRender() {
	g_grim->_sputRender->preRendered(getbool(1), getbool(2));
}

void Lua_V1::ResetTextures() {
#if defined(GRIM_STD3D)
	if (g_grim->_std3D)
		g_grim->_std3D->purgeTextureCache();
#endif
}

} // end of namespace Grim
