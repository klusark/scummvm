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

#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/text/blasttext.h"
#include "engines/grim/grim.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/stdcontrol.h"

namespace Grim {

SputRender::SputRender(RenderDroid *rd, StdDisplay *stdDisplay, StdBitmap *stdBitmap, Res *res, SputResource *sputResource,
		SputPrimitive *sputPrimitive, WalkSystem *walkSystem, SputRoom *sputRoom, StdVBuffer *backBuffer, StdVBuffer *mainGfxBuffer,
		SputTime *sputTime, SputControl *sputControl) :
		_rd(rd), _stdDisplay(stdDisplay), _stdBitmap(stdBitmap), _res(res), _sputResource(sputResource),
		_sputPrimitive(sputPrimitive), _walkSystem(walkSystem), _sputRoom(sputRoom), _backBuffer(backBuffer), _mainGfxBuffer(mainGfxBuffer),
		_sputActor(nullptr), _actors(nullptr), _sputText(nullptr), _blastText(nullptr), _sputSmush(nullptr), _sputTime(sputTime),
		_sputControl(sputControl), _stdControl(nullptr), _grim(nullptr), _hs(nullptr), _initialized(0), _started(0),
		_gammaCorrection(1.0f), _colorInfo(nullptr), _b3DAcceleration(0), _acceleratedZBuffers(1),
		_drawBuffer(nullptr), _zBuffer(nullptr), _canvas(nullptr), _cleanBuffer(nullptr), _cleanArtBuffer(nullptr),
		_cleanZBuffer(nullptr), _decompressionBuffer(0), _numPages(1), _dirtyRectQueue(nullptr), _freeDirtyRects(nullptr), _numDirtyRects(0),
		_nextSerialNumber(0), _cacheRectList(nullptr), _animationList(nullptr), _reset(0), _irisStart(0), _irisDuration(0.0f), _irisCenterX(320),
		_irisCenterY(240), _renderFlags(0), _skipPrimitives(0), _setPaletteFunc(nullptr), _pageFlipFunc(nullptr),
		_pageFlipEnable(1), _movieJustFinished(0), _clearedBehindMovie(0), _camActorIndex(0), _debugFlags(8), _debugKey1Down(0),
		_debugKey2Down(0), _debugKey4Down(0), _debugKey8Down(0), _debugKey10Down(0), _renderEnable(1),
		_loadImageAllowed(1), _perfCounter(0), _waitBitmapTimerMs(0), _waitBitmapFrame(0),
		_ambient(0.2f), _curView(-1), _lastRoom(nullptr), _camera(nullptr), _dirtyRectColor(0), _printLuaStack(0),
		_numSortActors(0) {
	memset(_renderVars, 0, sizeof(_renderVars));
	memset(_colorLookup, 0, sizeof(_colorLookup));
	memset(&_waitBitmapRef, 0, sizeof(_waitBitmapRef));
	memset(_colors, 0, sizeof(_colors));
	memset(_actorSortInfo, 0, sizeof(_actorSortInfo));
	memset(_sortedActors, 0, sizeof(_sortedActors));
}

int SputRender::initialize(SysStruct *hs) {
	_hs = hs;

	_sputControl->addCallback(this);
	_initialized = 1;

	return 0;
}

int SputRender::terminate() {
	_hs = nullptr;

	if (_initialized)
		_initialized = 0;

	return 0;
}

int SputRender::startup() {
	if (_started)
		return 1;

	_started = 1;
	_renderVars[0] = 4;
	_renderVars[1] = 3;
	_renderVars[2] = 1;
	_renderVars[3] = 1;
	_renderVars[4] = 0;
	_renderVars[5] = 0;
	_renderVars[6] = 2;

	return 0;
}

int SputRender::shutdown() {
	freeBuffers();

	while (_dirtyRectQueue) {
		DirtyRect *next = _dirtyRectQueue->next;

		delete _dirtyRectQueue;
		_dirtyRectQueue = next;
	}

	_numDirtyRects = 0;

	while (_freeDirtyRects) {
		DirtyRect *next = _freeDirtyRects->nextFree;

		delete _freeDirtyRects;
		_freeDirtyRects = next;
	}

	_started = 0;

	return 0;
}

int SputRender::save(Common::WriteStream *stream) {
	stream->writeSint32LE(_irisStart);
	stream->writeFloatLE(_irisDuration);
	stream->writeSint32LE(_irisCenterX);
	stream->writeSint32LE(_irisCenterY);
	stream->writeFloatLE(_gammaCorrection);
	stream->writeSint32LE(_renderFlags);
	stream->writeSint32LE(_pageFlipEnable);
	stream->writeSint32LE(_camActorIndex);
	stream->writeSint32LE(_movieJustFinished);

	if (stream->err()) {
		warning("SputRender::save(): failed to write the render state");
		return 1;
	}

	return 0;
}

int SputRender::restore(Common::ReadStream *stream) {
	_irisStart = stream->readSint32LE();
	_irisDuration = stream->readFloatLE();
	_irisCenterX = stream->readSint32LE();
	_irisCenterY = stream->readSint32LE();
	_gammaCorrection = stream->readFloatLE();
	_renderFlags = stream->readSint32LE();
	_pageFlipEnable = stream->readSint32LE();
	_camActorIndex = stream->readSint32LE();
	_movieJustFinished = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputRender::restore(): failed to read the render state");
		return 1;
	}

	return 0;
}

int SputRender::open() {
	_camActorIndex = _sputActor->allocateActor();
	_sputActor->actorDeref(_camActorIndex)->flags |= 1;

	return 1;
}

void SputRender::close() {
	if (_canvas)
		_rd->_canvas.destroy(_canvas);

	if (_cleanArtBuffer)
		_stdDisplay->vBufferFree(_cleanArtBuffer);

	if (_cleanZBuffer)
		_stdDisplay->vBufferFree(_cleanZBuffer);

	if (_cleanBuffer)
		_stdDisplay->vBufferFree(_cleanBuffer);

	_decompressionBuffer = 0;
}

int SputRender::setDisplay(StdVBuffer *vBuffer, StdVBuffer *zBuffer, int xStart, int yStart, int xEnd, int yEnd, int *options) {
	close();

	_canvas = _rd->_canvas.create(1, vBuffer, zBuffer, xStart, yStart, xEnd, yEnd, 0);
	_zBuffer = zBuffer;
	_drawBuffer = vBuffer;

	StdVBufferTexFmt format = vBuffer->tex;

	_cleanBuffer = _stdDisplay->vBufferNew(&format, 0, 0, nullptr);

	if (vBuffer->videoMemory)
		_cleanArtBuffer = _stdDisplay->vBufferNew(&format, 1, 1, nullptr);
	else
		_cleanArtBuffer = _stdDisplay->vBufferNew(&format, 0, 0, nullptr);

	// The demo always keeps the clean Z buffer in system memory
	int videoMemory = (_b3DAcceleration && !_acceleratedZBuffers && !(_grim->getGameFlags() & ADGF_DEMO)) ? 1 : 0;

	_cleanZBuffer = _stdDisplay->vBufferNew(&format, videoMemory, videoMemory, nullptr);

	if (!_cleanArtBuffer || !_cleanZBuffer)
		return 0;

	BlastTextSurface *textSurface = _blastText->getSurface();

	textSurface->pixels = _backBuffer->pixels;
	textSurface->bytesPerPixel = (uint)_backBuffer->tex.format.bpp >> 3;
	textSurface->height = _backBuffer->tex.height;
	textSurface->pitch = _backBuffer->tex.pitchInBytes;
	textSurface->width = _drawBuffer->tex.width;

	options[3] |= 2;
	setRenderOptions(options);

	Room *room = _sputRoom->getCurrentRoom();

	if (room)
		_rd->_camera.setAmbientLight(room->setups[room->curSetup].camera.prdCamera, _ambient);

	_colorInfo = &_backBuffer->tex.format;
	loadSetupBackground(_grim->getColorInfo());
	_decompressionBuffer = 1;

	room = _sputRoom->getCurrentRoom();

	if (room)
		useSetup(room, room->curSetup);

	if (!(_grim->getGameFlags() & ADGF_DEMO))
		forceRefresh();

	return 1;
}

int SputRender::queryDisplayState() {
	if (!_canvas) {
		warning("SputRender::queryDisplayState(): the display is not set");
		return 0;
	}

	return _canvas->vBuffer->type == 1;
}

void SputRender::using3DHardware(int b3D) {
	_b3DAcceleration = b3D;
}

void SputRender::pageCountIs(int numPages) {
	_numPages = 1;

	if (numPages >= 1)
		_numPages = numPages;
}

void SputRender::setPaletteCallback(SputRenderPaletteFunc func) {
	_setPaletteFunc = func;
}

void SputRender::setPageFlipCallback(SputRenderPageFlipFunc func) {
	_pageFlipFunc = func;
}

SputRenderPaletteFunc SputRender::getPaletteFunc() {
	return _setPaletteFunc;
}

void SputRender::setPageFlip(int enable) {
	_pageFlipEnable = enable;
}

int SputRender::pageFlip() {
	if (_grim->isRemastered()) {
		int flipResult = 0;

		if (_pageFlipFunc)
			flipResult = _pageFlipFunc();

		_movieJustFinished = 0;

		return flipResult;
	}

	int result = _pageFlipEnable;

	if (_pageFlipEnable) {
		result = _movieJustFinished;

		if (!_movieJustFinished && _pageFlipFunc)
			result = _pageFlipFunc();
	}

	_movieJustFinished = 0;

	return result;
}

int SputRender::setRenderOptions(const int *options) {
	memcpy(_renderVars, options, sizeof(_renderVars));

	_rd->setGeometryMode(_renderVars[0]);
	_rd->setLightingMode(_renderVars[1]);
	_rd->setTextureMode(_renderVars[2]);
	_rd->setRenderOptions(_renderVars[3]);
	_rd->setSortingMethod(_renderVars[4]);
	_rd->setOcclusionMethod(_renderVars[5]);

	return _rd->setZBufferMethod(_renderVars[6]);
}

int SputRender::controlCallback(float frameTime) {
	int result = 0;
	int keyDown;

	keyDown = _grim->getDebugKeys();
	if (!keyDown)
		return 0;

	keyDown = _stdControl->readKey(29, nullptr, nullptr);
	if (!keyDown)
		return result;

	keyDown = _stdControl->readKey(32, nullptr, nullptr);
	if (!keyDown)
		return result;

	result = 1;

	keyDown = _stdControl->readKey(57, nullptr, nullptr);
	if (keyDown) {
		if (!_debugKey1Down) {
			drawDebuggingToggle();
			_debugKey1Down = 1;
		}
	} else {
		_debugKey1Down = 0;
	}

	if ((_debugFlags & 1) == 0)
		return result;

	keyDown = _stdControl->readKey(48, nullptr, nullptr);
	if (keyDown) {
		if (!_debugKey8Down) {
			if (_debugFlags & 8) {
				_debugKey8Down = 1;
				_debugFlags &= ~8;
			} else {
				_debugKey8Down = 1;
				_debugFlags |= 8;
			}
		}
	} else {
		_debugKey8Down = 0;
	}

	keyDown = _stdControl->readKey(38, nullptr, nullptr);
	if (keyDown) {
		if (!_debugKey2Down) {
			if (_debugFlags & 2) {
				_debugKey2Down = 1;
				_debugFlags &= ~2;
			} else {
				_debugKey2Down = 1;
				_debugFlags |= 2;
			}
		}
	} else {
		_debugKey2Down = 0;
	}

	keyDown = _stdControl->readKey(46, nullptr, nullptr);
	if (keyDown) {
		if (!_debugKey4Down) {
			if (_debugFlags & 4) {
				_debugKey4Down = 1;
				_debugFlags &= ~4;
			} else {
				_debugKey4Down = 1;
				_debugFlags |= 4;
			}
		}
	} else {
		_debugKey4Down = 0;
	}

	keyDown = _stdControl->readKey(19, nullptr, nullptr);
	if (!keyDown) {
		_debugKey10Down = 0;
		return result;
	}

	if (_debugKey10Down)
		return result;

	if (_debugFlags & 0x10) {
		_debugKey10Down = 1;
		_debugFlags &= ~0x10;
	} else {
		_debugKey10Down = 1;
		_debugFlags |= 0x10;
	}

	return result;
}

void SputRender::forceRefresh() {
	_reset = 1;
}

void SputRender::markDirtyRect(const StdRect *rectPointer, int page) {
	StdRect rect = *rectPointer;

	if (rect.width > 0 && rect.height > 0) {
		int width = rect.x % 4 + rect.width;

		rect.x = rect.x - rect.x % 4;
		rect.width = width;

		if (width % 4)
			rect.width = 4 - width % 4 + width;

		markDirtyRectInternal(&rect, page, _numPages - 1);
	}
}

void SputRender::markAllPagesDirty(const StdRect *rectPointer, int page) {
	StdRect rect = *rectPointer;

	if (rect.width > 0 && rect.height > 0) {
		int width = rect.x % 4 + rect.width;

		rect.x = rect.x - rect.x % 4;
		rect.width = width;

		if (width % 4)
			rect.width = 4 - width % 4 + width;

		for (int i = 0; i < _numPages; i++)
			markDirtyRectInternal(&rect, page, i);
	}
}

void SputRender::markDirtyRectInternal(StdRect *rect, int page, uint pageIndex) {
	int left = rect->x;
	int right = rect->x + rect->width;
	int top = rect->y;
	int bottom = rect->y + rect->height;
	DirtyRect *node = _dirtyRectQueue;

	while (node) {
		if (node->pageIndex != pageIndex) {
			node = node->next;
			continue;
		}

		int nodeLeft = node->rect.x;
		int nodeTop = node->rect.y;
		int nodeRight = nodeLeft + node->rect.width;
		int nodeBottom = nodeTop + node->rect.height;

		if (left >= nodeRight || right <= nodeLeft || top >= nodeBottom || bottom <= nodeTop) {
			node = node->next;
			continue;
		}

		if (left >= nodeLeft && top >= nodeTop && right <= nodeRight && bottom <= nodeBottom) {
			if (page > node->page)
				node->page = page;

			return;
		}

		if (node->next)
			node->next->prev = node->prev;

		if (node->prev)
			node->prev->next = node->next;
		else
			_dirtyRectQueue = node->next;

		if (node->page > page)
			page = node->page;

		DirtyRect *freed = node;

		node = node->next;
		_numDirtyRects--;
		freed->nextFree = _freeDirtyRects;
		_freeDirtyRects = freed;

		if (nodeLeft >= left && nodeLeft <= right && nodeTop >= top && nodeTop <= bottom &&
				nodeRight >= left && nodeRight <= right && nodeBottom >= top && nodeBottom <= bottom)
			continue;

		StdRect strip;

		strip.x = MIN(nodeLeft, left);
		strip.y = MAX(nodeTop, top);
		strip.width = MAX(nodeRight, right) - strip.x;
		strip.height = MIN(nodeBottom, bottom) - strip.y;

		int dirty = page != 0 || (node && node->page != 0);

		markDirtyRectInternal(&strip, dirty, pageIndex);

		StdRect piece;

		if (nodeTop != top) {
			if (nodeTop < top) {
				piece.x = nodeLeft;
				piece.y = nodeTop;
				piece.width = nodeRight - nodeLeft;
				piece.height = strip.y - nodeTop;
			} else {
				piece.x = left;
				piece.y = top;
				piece.width = right - left;
				piece.height = strip.y - top;
			}

			dirty = page != 0 || (node && node->page != 0);

			markDirtyRectInternal(&piece, dirty, pageIndex);
		}

		if (nodeBottom != bottom) {
			int stripBottom = strip.y + strip.height;

			if (nodeBottom > bottom) {
				piece.x = nodeLeft;
				piece.y = stripBottom;
				piece.width = nodeRight - nodeLeft;
				piece.height = nodeBottom - stripBottom;
			} else {
				piece.x = left;
				piece.y = stripBottom;
				piece.width = right - left;
				piece.height = bottom - stripBottom;
			}

			dirty = page != 0 || (node && node->page != 0);

			markDirtyRectInternal(&piece, dirty, pageIndex);
		}

		return;
	}

	DirtyRect *newObject;

	if (_freeDirtyRects) {
		newObject = _freeDirtyRects;
		_freeDirtyRects = newObject->nextFree;
		newObject->nextFree = nullptr;
	} else {
		newObject = new DirtyRect();
	}

	_numDirtyRects++;
	newObject->rect = *rect;
	newObject->page = page;
	newObject->serial = _nextSerialNumber++;
	newObject->pageIndex = pageIndex;
	newObject->next = _dirtyRectQueue;

	if (_dirtyRectQueue)
		_dirtyRectQueue->prev = newObject;

	newObject->prev = nullptr;
	_dirtyRectQueue = newObject;
}

void SputRender::freeCacheRects() {
	DirtyRect *node = _dirtyRectQueue;

	while (node) {
		DirtyRect *next = node->next;

		node->nextFree = _freeDirtyRects;
		_freeDirtyRects = node;
		node = next;
		_dirtyRectQueue = next;
	}

	_numDirtyRects = 0;
}

} // End of namespace Grim
