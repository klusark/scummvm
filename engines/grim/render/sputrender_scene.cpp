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

#include "common/algorithm.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/text/blasttext.h"
#include "engines/grim/grim.h"
#include "engines/grim/lua/luadebug.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/sputobjectstate.h"
#include "engines/grim/render/sputprimitive.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"
#include "engines/grim/std/std_sort.h"

namespace Grim {

void SputRender::drawDebuggingToggle() {
	if (_debugFlags & 1)
		_debugFlags &= ~1;
	else
		_debugFlags |= 1;

	if (_debugFlags & 1)
		return;

	RdCamera *camera = _rd->_camera.getCurrent();

	if (!camera || !camera->canvas) {
		warning("SputRender::drawDebuggingToggle(): no current camera canvas");
		return;
	}

	StdRect rect;

	rect.x = camera->canvas->xStart;
	rect.y = camera->canvas->yStart;
	rect.width = camera->canvas->xEnd - camera->canvas->xStart + 1;
	rect.height = camera->canvas->yEnd - camera->canvas->yStart + 1;

	for (int i = 0; i < _numPages; i++)
		markDirtyRectInternal(&rect, 1, i);
}

void SputRender::syncCamera(Room *room, int setup) {
	RoomSetup *setupPointer = &room->setups[setup];
	ResRef *colormapRef = &room->colormaps[room->curColormap];

	if (!colormapRef->valid)
		error("SputRender::syncCamera(): Bad Colormap Reference");

	_rd->_colormap.setCurrent(static_cast<RdColormap *>(colormapRef->handle));
	_rd->_camera.setCurrent(setupPointer->camera.prdCamera);

	if (_canvas)
		_rd->_camera.setCanvas(setupPointer->camera.prdCamera, _canvas);

	_rd->_camera.setAspectRatio(setupPointer->camera.prdCamera, 1.0f);
	_rd->_camera.update(setupPointer->camera.posMatrix);
}

void SputRender::useSetup(Room *room, int setup) {
	if (!room)
		return;

	if (!_cleanBuffer || !_cleanArtBuffer || !_cleanZBuffer || !_canvas || !_actors) {
		warning("SputRender::useSetup(): the display or the actors are not set");
		return;
	}

	RoomSetup *setupPointer = &room->setups[setup];

	freeBuffers();
	syncCamera(room, setup);
	_renderFlags &= ~3;
	_camera = &setupPointer->camera;

	SputActor *camActor = _sputActor->actorDeref(_camActorIndex);
	float (*orient)[3] = _sputActor->getOrientMatrix(camActor);

	memcpy(orient, setupPointer->camera.posMatrix, sizeof(RdMatrix34));

	StdVBuffer *background = _sputRoom->getSetupBackground(room, setup);

	if (background) {
		if (background->type == 2) {
			fillDrawBuffer(background);
			_stdDisplay->vBufferCopy(_cleanArtBuffer, _cleanBuffer, 0, 0, nullptr, 0);
		} else {
			_stdDisplay->vBufferCopy(_cleanArtBuffer, background, 0, 0, nullptr, 0);
		}
	}

	StdVBuffer *zBackground = _sputRoom->getSetupZBackground(room, setup);

	if (zBackground) {
		if (zBackground->type == 2) {
			_cleanBuffer->tex = _cleanZBuffer->tex;

			if (_cleanZBuffer->videoMemory) {
				_stdDisplay->vBufferDeCompressTo(zBackground, _cleanBuffer);
				_stdDisplay->vBufferCopy(_cleanZBuffer, _cleanBuffer, 0, 0, nullptr, 0);
			} else {
				_stdDisplay->vBufferDeCompressTo(zBackground, _cleanZBuffer);
			}
		} else {
			_stdDisplay->vBufferCopy(_cleanZBuffer, zBackground, 0, 0, nullptr, 0);
		}
	} else {
		if (!_cleanZBuffer->pixels || _cleanZBuffer->tex.sizeInBytes < 0x96000)
			warning("SputRender::useSetup(): the Z buffer is smaller than 640x480x16");
		else
			memset(_cleanZBuffer->pixels, 0, 0x96000);

		_renderFlags |= 2;
	}

	for (SputObjectState *state = setupPointer->ObjectStates; state; state = state->next) {
		// The demo does not load the art here, a load would set the current frame back to none
		if (!(_grim->getGameFlags() & ADGF_DEMO))
			state->loadResources();

		state->reset(_cleanArtBuffer, _cleanZBuffer);
	}

	StdRect rect;

	rect.x = _canvas->xStart;
	rect.y = _canvas->yStart;
	rect.width = _canvas->xEnd - _canvas->xStart + 1;
	rect.height = _canvas->yEnd - _canvas->yStart + 1;
	markAllPagesDirty(&rect, 1);
}

void SputRender::update() {
	int mode = _grim->getLibMode();
	bool remastered = _grim->isRemastered();

	if (remastered) {
		// Nothing is drawn before the engine runs or while it exits
		if (mode == 0 || mode == GrimEngine::kLibModeExiting)
			return;

		_grim->getHdCanvas()->beginFrame();

		if (mode != 3)
			_clearedBehindMovie = 0;

		if (mode == 3) {
			if (!updateMovieRemastered())
				return;

			mode = _grim->getLibMode();
		}
	}

	switch (mode) {
	case 0:
		return;
	case 3: {
		_sputSmush->setTarget(_backBuffer);
		int movieFinished = _sputSmush->periodic();

		if (movieFinished) {
			_grim->setLibMode(2);

			StdRect rect;

			rect.x = 0;
			rect.y = 0;
			rect.width = 640;
			rect.height = 480;

			for (int i = 0; i < _numPages; i++)
				markDirtyRectInternal(&rect, 1, i);

			forceRefresh();

			if (!(_grim->getGameFlags() & ADGF_DEMO))
				_movieJustFinished = 1;
		}

		// The demo doesn't update the speech or the text while a movie is playing,
		// but our main buffer has to be presented here
		if (_grim->getGameFlags() & ADGF_DEMO) {
			pageFlip();
			return;
		}

		float frameTime = _sputTime->getFrameTimeFlex();

		_sputActor->updateSpeechOnly(frameTime);
		_stdDisplay->vBufferLock(_backBuffer);
		_blastText->getSurface()->pixels = _backBuffer->pixels;
		_sputText->update(0);
		_stdDisplay->vBufferUnlock(_backBuffer);
		pageFlip();
		return;
	}
	case 4:
		if (_reset)
			_grim->userPaint();

		_reset = 0;
		return;
	default:
		break;
	}

	if (_printLuaStack) {
		lua_printstack(nullptr);
	}

	Room *room = _sputRoom->getCurrentRoom();

	if (!room) {
		if (remastered)
			drawNoRoomRemastered();

		return;
	}

	if (_lastRoom != room || room->curSetup != _curView || _reset) {
		// The demo doesn't free the dirty rects and has no post camera change handler
		if (!(_grim->getGameFlags() & ADGF_DEMO))
			freeCacheRects();

		_lastRoom = room;
		_curView = _lastRoom->curSetup;
		useSetup(_lastRoom, _curView);

		if (!(_grim->getGameFlags() & ADGF_DEMO))
			_lua->postCameraChange(_curView);
	}

	RoomSetup *setup = &_lastRoom->setups[_curView];

	syncCamera(_lastRoom, _curView);

	for (SputObjectState *state = setup->ObjectStates; state; state = state->next)
		state->update(3, _cleanArtBuffer, _cleanZBuffer);

	_grim->setPerfCounter(4);

	if (_pageFlipEnable)
		restoreDirtyRects(_backBuffer, _zBuffer);

	_grim->queryPerfCounterDiff(4);
	_sputSmush->setTarget(_backBuffer);
	_sputSmush->periodic();

	// The demo draws the animation bitmaps with buffer copies, which need the buffer unlocked
	if (_grim->getGameFlags() & ADGF_DEMO) {
		drawPrimitives(1);
	} else {
		_stdDisplay->vBufferLock(_backBuffer);
		drawPrimitives(1);
		_stdDisplay->vBufferUnlock(_backBuffer);
	}

	for (SputObjectState *state = setup->ObjectStates; state; state = state->next)
		state->update(1, _backBuffer, _zBuffer);

	// The demo advances the frame at the end of the main loop
	if (!(_grim->getGameFlags() & ADGF_DEMO))
		_rd->advanceFrame();

	sortActors();

	if (_numSortActors || (!(_grim->getGameFlags() & ADGF_DEMO) && (_debugFlags & 1))) {
		_grim->setPerfCounter(5);

		int numSorted = 0;

		for (int i = 0; i < _numSortActors; i++) {
			if (_sortedActors[i]->kind == 0) {
				SputActor *enumActor = &_actors[_sortedActors[i]->actorIndex];

				_sputActor->enumCostumeComponents(enumActor->costumeStack->costumeInstance);
			}

			numSorted++;
		}

		_grim->queryPerfCounterDiff(5);

		if (_debugFlags & 1) {
			_stdDisplay->vBufferLock(_backBuffer);

			if (_debugFlags & 8)
				drawWalkPlanes();
			else
				drawBoundingBox();

			_stdDisplay->vBufferUnlock(_backBuffer);
		}

		_grim->setPerfCounter(6);

		for (int i = numSorted - 1; i >= 0; i--) {
			ActorSortInfo *entry = _sortedActors[i];
			SputActor *actor = &_actors[entry->actorIndex];
			float (*orient)[3] = _sputActor->getOrientMatrix(actor);

			if (actor->scale != 1.0f) {
				RdVector3 scale;

				scale[0] = actor->scale;
				scale[1] = actor->scale;
				scale[2] = actor->scale;
				_rd->preScale34(orient, scale);
			}

			if (entry->kind == 0) {
				RdColormap *colormap;

				if (!actor->colorMap)
					warning("SputRender::update(): actor without colormap reference");

				if (actor->colorMap && actor->colorMap->valid) {
					colormap = static_cast<RdColormap *>(actor->colorMap->handle);
				} else {
					ResRef *colormapRef = &_lastRoom->colormaps[_lastRoom->curColormap];

					if (!colormapRef->valid)
						error("SputRender::update(): Bad Colormap Reference");

					colormap = static_cast<RdColormap *>(colormapRef->handle);
				}

				_rd->_colormap.setCurrent(colormap);

				if (_setPaletteFunc)
					_setPaletteFunc(colormap->colors, 0);

				_sputActor->drawCostume(actor->costumeStack, orient);

				if (_debugFlags & 1)
					drawActorDebug(actor);
			} else if (entry->kind == 1) {
				if (!_b3DAcceleration)
					_stdDisplay->vBufferLock(_backBuffer);

				drawActor(actor);

				if (!_b3DAcceleration)
					_stdDisplay->vBufferUnlock(_backBuffer);

				markDirtyRect(&actor->drawnRect, 1);
			}

			if (actor->scale != 1.0f) {
				RdVector3 scale;

				scale[0] = (float)(1.0 / (double)actor->scale);
				scale[1] = scale[0];
				scale[2] = scale[0];
				orient = _sputActor->getOrientMatrix(actor);
				_rd->preScale34(orient, scale);
			}
		}

		_grim->queryPerfCounterDiff(6);
		_grim->setPerfCounter(7);
		_rd->finishFrame();
		_grim->queryPerfCounterDiff(7);

		for (SputObjectState *state = setup->ObjectStates; state; state = state->next)
			state->update(2, _backBuffer, _zBuffer);
	}

	if (_grim->getGameFlags() & ADGF_DEMO) {
		drawPrimitives(2);
	} else {
		_stdDisplay->vBufferLock(_backBuffer);
		drawPrimitives(2);
		_stdDisplay->vBufferUnlock(_backBuffer);
	}

	if (!(_grim->getGameFlags() & ADGF_DEMO) && (_debugFlags & 1))
		drawRoomDebug();

	if (_renderFlags & 4) {
		int gameTime = _sputTime->getGameTime();
		double ratio;

		if (_irisDuration == 0.0f) {
			// The original could divide by zero here, let's avoid that...
			warning("SputRender::update(): the iris duration is zero");
			ratio = 2.0;
		} else {
			ratio = (double)(uint)((uint)gameTime - (uint)_irisStart) / (double)_irisDuration;
		}

		if (ratio < 0.0)
			ratio = -ratio;

		if (!(ratio > 1.0)) {
			_renderFlags &= ~8;
		} else {
			ratio = 1.0;

			if (_irisDuration < 0.0f)
				_renderFlags &= ~4;
			else
				_renderFlags |= 8;
		}

		float percent;

		if (_irisDuration <= 0.0f)
			percent = (float)(1.0 - ratio);
		else
			percent = (float)ratio;

		getIris(percent, _irisCenterX, _irisCenterY);

		if (_irisDuration < 0.0f) {
			StdRect rect;

			rect.x = 0;
			rect.y = 0;
			rect.width = 640;
			rect.height = 480;
			markAllPagesDirty(&rect, 0);
		}
	}

	if (remastered) {
		drawOverlayLayers(_grim->getLibMode());
		_reset = 0;
		return;
	}

	_stdDisplay->vBufferLock(_backBuffer);
	_blastText->getSurface()->pixels = _backBuffer->pixels;
	_sputText->update(1);

	if ((_grim->getGameFlags() & ADGF_DEMO) && (_debugFlags & 1))
		drawRoomDebug();

	_stdDisplay->vBufferUnlock(_backBuffer);
	_reset = 0;
}

int SputRender::loadSetupBackground(const StdColorFormat *format) {
	if (!format || !_colorInfo) {
		warning("SputRender::loadSetupBackground(): the display is not set");
		return 0;
	}

	if (format->redShift < 0 || format->redShift > 31 || format->greenShift < 0 || format->greenShift > 31 ||
			format->blueShift < 0 || format->blueShift > 31 || format->redLoss < 0 || format->redLoss > 31 ||
			format->greenLoss < 0 || format->greenLoss > 31 || format->blueLoss < 0 || format->blueLoss > 31) {
		warning("SputRender::loadSetupBackground(): unsupported color format");
		return 0;
	}

	int redShift = format->redShift;
	int greenShift = format->greenShift;
	int blueShift = format->blueShift;
	int redLoss = format->redLoss;
	int greenLoss = format->greenLoss;
	int blueLoss = format->blueLoss;

	buildGammaTable(_colorInfo, _gammaCorrection);

	_colors[2] = _colorLookup[0];
	_colors[4] = _colorLookup[(uint16)((uint)(128 >> redLoss) << redShift) | (uint16)((uint)(128 >> blueLoss) << blueShift) |
			(uint16)((uint)(128 >> greenLoss) << greenShift)];
	_colors[3] = _colorLookup[(uint16)((uint)(255 >> greenLoss) << greenShift) | (uint16)((uint)(255 >> redLoss) << redShift) |
			(uint16)((uint)(255 >> blueLoss) << blueShift)];
	_colors[5] = _colorLookup[(uint16)((uint)(255 >> greenLoss) << greenShift) | (uint16)((uint)(255 >> redLoss) << redShift)];
	_colors[6] = _colorLookup[(uint16)((uint)(255 >> redLoss) << redShift)];
	_colors[7] = _colorLookup[(uint16)((uint)(255 >> greenLoss) << greenShift)];
	_colors[8] = _colorLookup[(uint16)((uint)(255 >> blueLoss) << blueShift)];
	_colors[10] = _colorLookup[(uint16)((uint)(128 >> redLoss) << redShift)];
	_colors[11] = _colorLookup[(uint16)((uint)(128 >> greenLoss) << greenShift)];
	_colors[12] = _colorLookup[(uint16)((uint)(128 >> redLoss) << redShift) | (uint16)((uint)(128 >> greenLoss) << greenShift)];
	_colors[13] = _colorLookup[(uint16)((uint)(128 >> blueLoss) << blueShift)];
	_colors[9] = _colorLookup[(uint16)((uint)(128 >> redLoss) << redShift) | (uint16)((uint)(128 >> blueLoss) << blueShift)];
	_colors[14] = _colorLookup[(uint16)((uint)(128 >> blueLoss) << blueShift) | (uint16)((uint)(128 >> greenLoss) << greenShift)];
	_colors[15] = _colorLookup[(uint16)((uint)(192 >> redLoss) << redShift) | (uint16)((uint)(192 >> blueLoss) << blueShift) |
			(uint16)((uint)(192 >> greenLoss) << greenShift)];
	_colors[16] = _colorLookup[(uint16)((uint)(192 >> redLoss) << redShift) | (uint16)((uint)(192 >> blueLoss) << blueShift) |
			(uint16)((uint)(220 >> greenLoss) << greenShift)];
	_colors[17] = _colorLookup[(uint16)((uint)(166 >> redLoss) << redShift) | (uint16)((uint)(202 >> greenLoss) << greenShift) |
			(uint16)((uint)(240 >> blueLoss) << blueShift)];
	_colors[18] = _colorLookup[(uint16)((uint)(180 >> redLoss) << redShift) | (uint16)((uint)(180 >> blueLoss) << blueShift) |
			(uint16)((uint)(180 >> greenLoss) << greenShift)];
	_colors[19] = _colorLookup[(uint16)((uint)(100 >> redLoss) << redShift) | (uint16)((uint)(100 >> blueLoss) << blueShift) |
			(uint16)((uint)(100 >> greenLoss) << greenShift)];
	_colors[20] = _colorLookup[(uint16)((uint)(255 >> redLoss) << redShift) | (uint16)((uint)(255 >> blueLoss) << blueShift)];
	_colors[21] = _colorLookup[(uint16)((uint)(255 >> greenLoss) << greenShift) | (uint16)((uint)(255 >> blueLoss) << blueShift)];

	// The demo converts only the colormaps
	if (!(_grim->getGameFlags() & ADGF_DEMO))
		_sputResource->loadBknd();

	return _sputResource->cmap();
}

void SputRender::preRendered(int bitmap, int zBuffer) {
	_renderFlags |= 3;

	Room *room = _sputRoom->getCurrentRoom();

	if (!room)
		return;

	RoomSetup *setup = &room->setups[room->curSetup];

	if (bitmap && setup->bitmapRef.valid)
		_renderFlags &= ~1;

	if (zBuffer && setup->zbufferRef.valid)
		_renderFlags &= ~2;

	RdCamera *camera = _rd->_camera.getCurrent();

	if (!camera || !camera->canvas) {
		warning("SputRender::preRendered(): no current camera canvas");
		return;
	}

	StdRect rect;

	rect.x = camera->canvas->xStart;
	rect.y = camera->canvas->yStart;
	rect.width = camera->canvas->xEnd - camera->canvas->xStart + 1;
	rect.height = camera->canvas->yEnd - camera->canvas->yStart + 1;

	for (int i = 0; i < _numPages; i++)
		markDirtyRectInternal(&rect, 1, i);
}

void SputRender::reverseProject(int x, int y, float *out) {
	RdCamera *camera = _rd->_camera.getCurrent();

	if (!camera)
		return;

	RdCanvas *canvas = camera->canvas;

	if (!canvas)
		return;

	if (!_zBuffer || !_zBuffer->pixels || !_camera || x < 0 || y < 0 || x >= _zBuffer->tex.width || y >= _zBuffer->tex.height) {
		warning("SputRender::reverseProject(): the point is outside the Z buffer");
		return;
	}

	uint16 zPixel = ((uint16 *)_zBuffer->pixels)[_zBuffer->tex.width * y + x];
	int scaledZ = (int)((uint)zPixel << (canvas->zScaleShift & 31));
	float depth = (float)((double)scaledZ * 0.0000152587890625);
	double focalDepth = (double)camera->focalLen * (double)depth;

	if (depth == 0.0f || focalDepth == 0.0) {
		// The original could divide by zero here, let's avoid that...
		warning("SputRender::reverseProject(): the Z buffer value is zero");
		return;
	}

	RdVector3 point;

	point[1] = (float)(1.0 / (double)depth - 1.0);
	point[0] = (float)(((double)x - (double)canvas->xCenter) * (1.0 / focalDepth));
	point[2] = (float)(((double)y - (double)canvas->yCenter) * (-1.0 / focalDepth));
	_rd->transformPoint34(out, point, _camera->posMatrix);
}

int SputRender::buildShadowBitmap(float *out, float *pointA, float *pointB, float a, float b, float c, float d) {
	// The demo intersects the line with the plane without checking the denominator
	if (_grim->getGameFlags() & ADGF_DEMO) {
		float demoDeltaY = (float)((double)pointB[1] - pointA[1]);
		float demoDeltaZ = (float)((double)pointB[2] - pointA[2]);
		double demoDistA = (double)b * pointA[1] + (double)c * pointA[2] + (double)a * pointA[0] + d;
		double demoDistB = (double)b * pointB[1] + (double)c * pointB[2] + (double)a * pointB[0] + d;
		float demoT = (float)(demoDistA / (demoDistA - demoDistB));
		double demoX = ((double)pointB[0] - pointA[0]) * demoT + pointA[0];

		out[0] = (float)demoX;
		out[1] = (float)((double)demoT * demoDeltaY + pointA[1]);
		out[2] = (float)((double)demoT * demoDeltaZ + pointA[2]);

		return 1;
	}

	double deltaX = (double)pointB[0] - (double)pointA[0];
	float deltaY = (float)((double)pointB[1] - (double)pointA[1]);
	float deltaZ = (float)((double)pointB[2] - (double)pointA[2]);
	double distA = (((double)b * (double)pointA[1] + (double)c * (double)pointA[2]) + (double)a * (double)pointA[0]) + (double)d;
	float distB = (float)((((double)b * (double)pointB[1] + (double)c * (double)pointB[2]) + (double)a * (double)pointB[0]) + (double)d);
	float distAStored = (float)distA;
	double difference = distA - (double)distB;
	float differenceStored = (float)difference;
	double absDifference = (double)differenceStored;

	if (!(difference >= 0.0))
		absDifference = -absDifference;

	if (!(absDifference >= 0.1))
		return 0;

	double t = (double)distAStored / (double)differenceStored;

	out[0] = (float)(t * deltaX + (double)pointA[0]);
	out[1] = (float)(t * (double)deltaY + (double)pointA[1]);
	out[2] = (float)(t * (double)deltaZ + (double)pointA[2]);

	return 1;
}

void SputRender::restoreDirtyRects(StdVBuffer *dst, StdVBuffer *zBuffer) {
	DirtyRect *node = _dirtyRectQueue;
	Room *room = _sputRoom->getCurrentRoom();

	if (!room)
		return;

	if (!dst || !_cleanArtBuffer || !_cleanZBuffer || !_canvas) {
		warning("SputRender::restoreDirtyRects(): the display is not set");
		return;
	}

	if (_debugFlags & 1) {
		StdRect canvasRect;

		canvasRect.x = _canvas->xStart;
		canvasRect.y = _canvas->yStart;
		canvasRect.width = _canvas->xEnd - _canvas->xStart + 1;
		canvasRect.height = _canvas->yEnd - _canvas->yStart + 1;
		_stdDisplay->vBufferCopy(dst, _cleanArtBuffer, canvasRect.x, canvasRect.y, &canvasRect, 0);
	}

	_dirtyRectColor++;

	while (node) {
		StdRect *rect = &node->rect;

		if (zBuffer && node->page) {
			if (_renderFlags & 2) {
				if (_b3DAcceleration) {
#if defined(GRIM_STD3D)
					_std3D->setActiveRect(rect);
					_std3D->clearZBuffer();
#endif
				} else {
					_stdDisplay->vBufferFill(zBuffer, 0, rect);
				}
			} else {
				_stdDisplay->vBufferCopy(zBuffer, _cleanZBuffer, rect->x, rect->y, rect, 0);
			}
		}

		int remaining = (int)node->pageIndex;

		node->pageIndex = (uint)remaining - 1;

		if (remaining > 0) {
			node = node->next;
			continue;
		}

		if (_renderFlags & 1)
			_stdDisplay->vBufferFill(dst, 0, rect);
		else
			_stdDisplay->vBufferCopy(dst, _cleanArtBuffer, rect->x, rect->y, rect, 0);

		if (node->next)
			node->next->prev = node->prev;

		if (node->prev)
			node->prev->next = node->next;

		if (node == _dirtyRectQueue)
			_dirtyRectQueue = node->next;

		RdCamera *camera = _rd->_camera.getCurrent();

		if ((_debugFlags & 1) && (_debugFlags & 0x10)) {
			if (!camera || !camera->canvas) {
				warning("SputRender::restoreDirtyRects(): no current camera canvas");
			} else {
				_stdDisplay->vBufferLock(_backBuffer);
				_rd->_primit2.drawRectangle(camera->canvas, rect->x, rect->y, rect->x + rect->width, rect->y + rect->height,
						_dirtyRectColor, 0xFFFFFFFF);
				_stdDisplay->vBufferUnlock(_backBuffer);
			}
		}

		DirtyRect *freed = node;

		node = node->next;
		_numDirtyRects--;
		freed->nextFree = _freeDirtyRects;
		_freeDirtyRects = freed;
	}
}

void SputRender::drawPrimitives(int layerMask) {
	if (_grim->getGameFlags() & ADGF_DEMO) {
		drawAnimationBitmaps(layerMask - 1);
		return;
	}

	if (_skipPrimitives)
		return;

	RdCamera *camera = _rd->_camera.getCurrent();

	if (!camera) {
		warning("SputRender::drawPrimitives(): no current camera");
		return;
	}

	_sputPrimitive->getCurrent()->draw(camera->canvas, layerMask);
}

void SputRender::sortActorsDemo() {
	if (!_actors || !_camera) {
		warning("SputRender::sortActorsDemo(): no actors or camera");
		return;
	}

	int numActors = _sputActor->getNumActors();

	// The demo checks all the actors, not the ones linked to the room
	for (int i = 0; i < numActors; i++) {
		SputActor *actor = &_actors[i];

		actor->onScreen = 0;

		if (!actor->curSetName[0] || scumm_stricmp(actor->curSetName, _sputRoom->getCurrentRoomName()) || (actor->flags & 3) != 0)
			continue;

		ActorSortInfo *entry = allocActorSortEntry();

		if (!entry)
			break;

		RdVector3 vec;
		RdVector3 out;

		vec[0] = (float)((double)actor->actorPos[0] - (double)_camera->position[0]);
		vec[1] = (float)((double)actor->actorPos[1] - (double)_camera->position[1]);
		vec[2] = (float)((double)actor->actorPos[2] - (double)_camera->position[2]);
		_rd->transformVectorOrtho34(out, vec, _camera->posMatrix);
		entry->depth = out[1];
		entry->actorIndex = i;
		entry->kind = actor->costumeStack ? 0 : -1;
	}

	sort(_sortedActors, _numSortActors, sizeof(ActorSortInfo *), compareActorDepth);
}

void SputRender::sortActors() {
	resetActorSortList();

	if (_grim->getGameFlags() & ADGF_DEMO) {
		sortActorsDemo();
		return;
	}

	Room *room = _sputRoom->getCurrentRoom();

	if (!room || !room->actorNode || !_actors || !_camera) {
		warning("SputRender::sortActors(): no current room, actors or camera");
		return;
	}

	for (RoomActorLink *link = room->actorNode->actors; link; link = link->next) {
		SputActor *actor = &_actors[link->actorIndex];

		actor->onScreen = 0;

		if ((actor->flags & 3) == 0) {
			ActorSortInfo *entry = allocActorSortEntry();

			if (!entry)
				return;

			RdVector3 vec;
			RdVector3 out;

			vec[0] = (float)((double)actor->actorPos[0] - (double)_camera->position[0]);
			vec[1] = (float)((double)actor->actorPos[1] - (double)_camera->position[1]);
			vec[2] = (float)((double)actor->actorPos[2] - (double)_camera->position[2]);
			_rd->transformVectorOrtho34(out, vec, _camera->posMatrix);
			entry->depth = out[1];

			if (actor->costumeStack) {
				entry->kind = 0;
				entry->actorIndex = (int)(actor - _actors);
			} else {
				entry->actorIndex = (int)(actor - _actors);

				CostumeNode *costumeStack = actor->costumeStack;

				if (costumeStack && costumeStack->headThing)
					entry->kind = 1;
				else
					entry->kind = -1;
			}
		}
	}

	sort(_sortedActors, _numSortActors, sizeof(ActorSortInfo *), compareActorDepth);
}

void SputRender::resetActorSortList() {
	_numSortActors = 0;

	for (int i = 0; i < 128; i++)
		_sortedActors[i] = &_actorSortInfo[i];
}

ActorSortInfo *SputRender::allocActorSortEntry() {
	if (_numSortActors >= 128)
		return nullptr;

	return &_actorSortInfo[_numSortActors++];
}

int SputRender::compareActorDepth(const void *a, const void *b) {
	const ActorSortInfo *infoA = *(const ActorSortInfo *const *)a;
	const ActorSortInfo *infoB = *(const ActorSortInfo *const *)b;

	if (!(infoA->depth >= infoB->depth))
		return 1;

	return 0;
}

void SputRender::drawActor(SputActor *actor) {
	if (!actor->costumeStack || !_drawBuffer) {
		warning("SputRender::drawActor(): the actor has no costume or the display is not set");
		return;
	}

	int *viewport = _rd->_cache.getViewportRect();

	viewport[0] = _backBuffer->tex.width;
	viewport[1] = _drawBuffer->tex.height;
	viewport[2] = 0;
	viewport[3] = 0;
	actor->drawnRect.x = 0;
	actor->drawnRect.y = 0;
	actor->drawnRect.width = 0;
	actor->drawnRect.height = 0;

	float (*orient)[3] = _sputActor->getOrientMatrix(actor);

	if (_rd->_thing.draw(actor->costumeStack->headThing, orient)) {
		actor->onScreen = 1;
		actor->drawnRect.x = viewport[0];
		actor->drawnRect.y = viewport[1];
		actor->drawnRect.width = viewport[2] - viewport[0];
		actor->drawnRect.height = viewport[3] - viewport[1];
	}
}

} // End of namespace Grim
