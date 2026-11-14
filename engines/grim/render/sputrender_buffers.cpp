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

#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/std/std_bitmap.h"

namespace Grim {

void SputRender::freeBuffers() {
	while (_cacheRectList) {
		CacheRect *rect = _cacheRectList;

		_cacheRectList = rect->next;

		if (rect->color)
			_stdDisplay->vBufferFree(rect->color);

		if (rect->z)
			_stdDisplay->vBufferFree(rect->z);

		delete rect;
	}
}

CacheRect *SputRender::createCacheRect(int x, int y, int width, int height) {
	if (!_cleanArtBuffer || !_cleanZBuffer) {
		warning("SputRender::createCacheRect(): the display is not set");
		return nullptr;
	}

	CacheRect *rect = new CacheRect();

	rect->magic = 0x11111111;
	rect->rect.x = x;
	rect->rect.y = y;
	rect->rect.width = width;
	rect->rect.height = height;

	StdVBufferTexFmt format = _cleanArtBuffer->tex;

	format.height = height;
	format.width = width;
	rect->color = _stdDisplay->vBufferNew(&format, 0, 0, nullptr);
	_stdDisplay->vBufferCopy(rect->color, _cleanArtBuffer, 0, 0, &rect->rect, 0);

	format = _cleanZBuffer->tex;
	format.width = width;
	format.height = height;
	rect->z = _stdDisplay->vBufferNew(&format, 0, 0, nullptr);
	_stdDisplay->vBufferCopy(rect->z, _cleanZBuffer, 0, 0, &rect->rect, 0);

	rect->next = _cacheRectList;
	_cacheRectList = rect;

	return rect;
}

void SputRender::restoreCacheRect() {
	CacheRect *rect = _cacheRectList;

	if (!rect || !_cleanArtBuffer || !_cleanZBuffer) {
		warning("SputRender::restoreCacheRect(): no cache rectangle to restore");
		return;
	}

	_stdDisplay->vBufferCopy(_cleanArtBuffer, rect->color, rect->rect.x, rect->rect.y, nullptr, 0);
	_stdDisplay->vBufferCopy(_cleanZBuffer, rect->z, rect->rect.x, rect->rect.y, nullptr, 0);
	markAllPagesDirty(&rect->rect, 1);
}

void SputRender::addAnimationBitmap(void *bitmap, int layer) {
	for (AnimationBitmap *node = _animationList; node; node = node->next) {
		if (!node->bitmapRef.valid)
			warning("SputRender::addAnimationBitmap(): Bad Bitmap Reference");

		if (node->bitmapRef.handle == bitmap)
			return;
	}

	AnimationBitmap *node = new AnimationBitmap();

	memset(node, 0, sizeof(AnimationBitmap));
	node->next = _animationList;

	if (_animationList)
		_animationList->prev = node;

	_animationList = node;
	node->layer = layer;
	_res->setReference(&node->bitmapRef, bitmap);
}

void SputRender::removeAnimationBitmap(void *bitmap) {
	AnimationBitmap *node = _animationList;

	while (node && !(node->bitmapRef.valid && node->bitmapRef.handle == bitmap))
		node = node->next;

	if (!node)
		return;

	if (node->next)
		node->next->prev = node->prev;

	if (node->prev)
		node->prev->next = node->next;
	else
		_animationList = node->next;

	_res->removeReference(&node->bitmapRef);
	delete node;
}

void SputRender::drawAnimationBitmaps(int layer) {
	for (AnimationBitmap *node = _animationList; node; node = node->next) {
		if (node->layer != layer)
			continue;

		if (!node->bitmapRef.valid) {
			warning("SputRender::drawAnimationBitmaps(): Bad Bitmap Reference");
			continue;
		}

		StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(node->bitmapRef.handle);
		int frame = bitmap->curFrame;

		if (frame < 0)
			continue;

		if (frame >= (int)bitmap->numFrames) {
			warning("SputRender::drawAnimationBitmaps(): frame %d is out of range", frame);
			continue;
		}

		StdVBuffer *framePointer = bitmap->frames[frame];

		if (framePointer->type == 2) {
			fillDrawBuffer(framePointer);
			_stdDisplay->vBufferCopy(_backBuffer, _cleanBuffer, bitmap->xPos, bitmap->yPos, nullptr, 1);
		} else {
			_stdDisplay->vBufferCopy(_backBuffer, framePointer, bitmap->xPos, bitmap->yPos, nullptr, 1);
		}
	}
}

StdVBuffer *SputRender::drawBuffer() {
	return _backBuffer;
}

StdVBuffer *SputRender::cleanArtBuffer() {
	return _cleanArtBuffer;
}

StdVBuffer *SputRender::cleanZBuffer() {
	return _cleanZBuffer;
}

int SputRender::copyGFXBuffer() {
	if (!_cleanArtBuffer) {
		warning("SputRender::copyGFXBuffer(): the display is not set");
		return 0;
	}

	return _stdDisplay->vBufferCopy(_cleanArtBuffer, _backBuffer, 0, 0, nullptr, 0);
}

int SputRender::cleanDrawBuffer() {
	int mode = _grim->getLibMode();
	int result = mode;

	if (mode == 4) {
		if (!_cleanArtBuffer) {
			warning("SputRender::cleanDrawBuffer(): the display is not set");
			return 0;
		}

		StdRect rect;

		rect.x = 0;
		rect.y = 0;
		rect.width = 640;
		rect.height = 480;

		return _stdDisplay->vBufferCopy(_backBuffer, _cleanArtBuffer, 0, 0, &rect, 0);
	}

	return result;
}

void SputRender::fillDrawBuffer(StdVBuffer *src) {
	if (!_cleanBuffer) {
		warning("SputRender::fillDrawBuffer(): the display is not set");
		return;
	}

	int size = src->tex.width * src->tex.height;

	_stdDisplay->vBufferDeCompressTo(src, _cleanBuffer);

	RemasteredLookRenderer *remasteredLookRenderer = _rd->getRemasteredLookRenderer();

	if (remasteredLookRenderer) {
		if (_cleanBuffer->tex.width == src->tex.width && _cleanBuffer->tex.height == src->tex.height &&
				_cleanBuffer->tex.format.bpp == src->tex.format.bpp)
			remasteredLookRenderer->notifyChange(_cleanBuffer->pixels);
		else
			remasteredLookRenderer->notifyFree(_cleanBuffer->pixels);
	}

	_cleanBuffer->tex = src->tex;
	_cleanBuffer->colorKey = 63519;

	if (src->tex.format.colorMode != 5) {
		colorCorrect((uint16 *)_cleanBuffer->pixels, size);
		_cleanBuffer->colorKey = _colorLookup[63519];
	} else if (_grim->getGameFlags() & ADGF_DEMO) {
		_cleanBuffer->colorKey = _colorLookup[63519];
	}
}

StdVBuffer *SputRender::screenShot(int width, int height) {
	if (!_cleanArtBuffer) {
		warning("SputRender::screenShot(): the display is not set");
		return nullptr;
	}

	int oldMode = _grim->getLibMode();

	_grim->setLibMode(2);
	_reset = 1;
	dim(1, 0);
	_grim->setLibMode(oldMode);

	StdVBuffer *clean = _cleanArtBuffer;

	if (width < 0 || height < 0 || clean->tex.format.bpp != 16 || clean->tex.width < 640 || clean->tex.height < 480) {
		warning("SputRender::screenShot(): unsupported buffer or size");
		return nullptr;
	}

	StdVBufferTexFmt format = clean->tex;

	format.width = width;
	format.height = height;

	StdVBuffer *shot = _stdDisplay->vBufferNew(&format, 0, 0, nullptr);

	// The original could divide by zero here, let's avoid that...
	float xStep = 0.0f;
	float yStep = 0.0f;

	if (width > 0)
		xStep = (float)(640.0 / (double)width);

	if (height > 0)
		yStep = (float)(480.0 / (double)height);

	_stdDisplay->vBufferLock(shot);
	_stdDisplay->vBufferLock(clean);

	double y = 0.0;

	for (int row = 0; row < height; row++) {
		const uint16 *srcRow = (const uint16 *)(clean->pixels + clean->tex.pitchInBytes * (int)y);
		uint16 *dstRow = (uint16 *)(shot->pixels + shot->tex.pitchInBytes * row);
		double x = 0.0;

		for (int column = 0; column < width; column++) {
			dstRow[column] = srcRow[(int)x];
			x = x + (double)xStep;
		}

		y = y + (double)yStep;
	}

	if (_rd->getRemasteredLookRenderer())
		_rd->getRemasteredLookRenderer()->notifyChange(shot->pixels);

	_stdDisplay->vBufferUnlock(shot);
	_stdDisplay->vBufferUnlock(clean);
	_reset = 1;

	return shot;
}

void SputRender::saveScreen(const char *path) {
	StdColorFormat format;

	format.redBits = 8;
	format.greenBits = 8;
	format.blueBits = 8;
	format.greenShift = 8;
	format.colorMode = 1;
	format.bpp = 24;
	format.redShift = 16;
	format.blueShift = 0;
	format.redLoss = 0;
	format.greenLoss = 0;
	format.blueLoss = 0;
	format.alphaBits = 0;
	format.alphaShift = 0;
	format.alphaLoss = 0;

	if (path) {
		StdBitmapStruct *bitmap = _stdBitmap->vBufferToBitmap(_backBuffer, 0, 0);

		if (!bitmap) {
			warning("SputRender::saveScreen(): failed to convert the back buffer");
			return;
		}

		_stdBitmap->convertColorFormat(&format, bitmap);

		Common::String fileName(path);

		_stdBitmap->loadLbm2(fileName, bitmap);
		_stdBitmap->unloadBitmap(bitmap);
	}
}

void SputRender::saveZBuffer(const char *path) {
	if (!_zBuffer || !path) {
		warning("SputRender::saveZBuffer(): no Z buffer or no file name");
		return;
	}

	_stdDisplay->vBufferLock(_zBuffer);
	_stdDisplay->vBufferUnlock(_zBuffer);

	StdBitmapStruct *bitmap = _stdBitmap->vBufferToBitmap(_zBuffer, 0, 0);

	if (!bitmap) {
		warning("SputRender::saveZBuffer(): failed to convert the Z buffer");
		return;
	}

	_stdBitmap->write(path, bitmap);
	_stdBitmap->unloadBitmap(bitmap);
}

void SputRender::saveDrawBuffer(const char *path) {
	StdColorFormat format;

	format.redBits = 8;
	format.greenBits = 8;
	format.blueBits = 8;
	format.greenShift = 8;
	format.colorMode = 1;
	format.bpp = 24;
	format.redShift = 16;
	format.blueShift = 0;
	format.redLoss = 0;
	format.greenLoss = 0;
	format.blueLoss = 0;
	format.alphaBits = 0;
	format.alphaShift = 0;
	format.alphaLoss = 0;

	if (!path) {
		warning("SputRender::saveDrawBuffer(): no file name");
		return;
	}

	StdBitmapStruct *bitmap = _stdBitmap->vBufferToBitmap(_backBuffer, 0, 0);

	if (!bitmap) {
		warning("SputRender::saveDrawBuffer(): failed to convert the back buffer");
		return;
	}

	_stdBitmap->convertColorFormat(&format, bitmap);

	Common::String fileName(path);

	_stdBitmap->loadLbm2(fileName, bitmap);
	_stdBitmap->unloadBitmap(bitmap);
}

} // End of namespace Grim
