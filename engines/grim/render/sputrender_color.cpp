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

#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/grim.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/sputobjectstate.h"
#include "engines/grim/render/sputprimitive.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_bitmap.h"

namespace Grim {

void SputRender::buildGammaTable(const StdColorFormat *format, float gamma) {
	if (!(gamma >= 0.0f)) {
		warning("SputRender::buildGammaTable(): invalid gamma");
		return;
	}

	if (format->redBits < 0 || format->redBits > 30 || format->greenBits < 0 || format->greenBits > 30 ||
			format->blueBits < 0 || format->blueBits > 30 || format->redShift < 0 || format->redShift > 31 ||
			format->greenShift < 0 || format->greenShift > 31 || format->blueShift < 0 || format->blueShift > 31) {
		warning("SputRender::buildGammaTable(): invalid color format");
		return;
	}

	// The remastered applies the gamma when the frame is presented
	double exponent = _grim->isRemastered() ? 1.0 : (double)gamma;

	for (int i = 0; i < 0x10000; i++) {
		float red = (float)((double)((i >> 11) & 0x1F) / 31.0);
		float green = (float)((double)((i >> 5) & 0x3F) / 63.0);
		float blue = (float)((double)(i & 0x1F) / 31.0);

		red = (float)pow((double)red, exponent);
		green = (float)pow((double)green, exponent);
		blue = (float)pow((double)blue, exponent);

		red = (float)((double)((1 << format->redBits) - 1) * (double)red);
		green = (float)((double)((1 << format->greenBits) - 1) * (double)green);
		blue = (float)((double)((1 << format->blueBits) - 1) * (double)blue);

		uint16 value = (uint16)((uint)(int)(int64)(double)red << format->redShift);

		value |= (uint16)((uint)(int)(int64)(double)green << format->greenShift);
		value |= (uint16)((uint)(int)(int64)(double)blue << format->blueShift);
		_colorLookup[i] = value;
	}

	_gammaCorrection = gamma;
}

void SputRender::colorCorrect(uint16 *colors, int count) {
	if (count < 0) {
		warning("SputRender::colorCorrect(): negative count");
		return;
	}

	for (int i = 0; i < count; i++)
		colors[i] = _colorLookup[colors[i]];
}

void SputRender::setGamma(float gamma) {
	// The demo always rebuilds the tables
	if (_gammaCorrection != gamma || (_grim->getGameFlags() & ADGF_DEMO)) {
		_gammaCorrection = gamma;

		// The quads of the canvas use the gamma at the moment they are queued
		if (_grim->getHdCanvas())
			_grim->getHdCanvas()->setGamma(gamma);

		loadSetupBackground(_colorInfo);

		Room *room = _sputRoom->getCurrentRoom();

		if (room) {
			if (_grim->getGameFlags() & ADGF_DEMO) {
				for (uint i = 0; i < room->numSetups; i++) {
					for (SputObjectState *state = room->setups[i].ObjectStates; state; state = state->next) {
						state->colorImage.cleanSaved = 0;
						state->zImage.cleanSaved = 0;
					}
				}
			}

			useSetup(room, room->curSetup);
		}
	}
}

void SputRender::dim(int force, char shift) {
	int mode = _grim->getLibMode();

	if (mode == 4 && !force)
		return;

	if (!_cleanArtBuffer) {
		warning("SputRender::dim(): the display is not set");
		return;
	}

	bool demo = (_grim->getGameFlags() & ADGF_DEMO) != 0;

	if (mode == 3) {
		_sputSmush->setTarget(_backBuffer);

		// The demo shows the current frame of the movie in the buffer
		if (demo)
			_sputSmush->blitFrame();
	} else {
		_sputText->pause();

		// The demo has no primitives to pause
		if (!demo)
			_sputPrimitive->pause();

		_sputTime->advanceManual(0);
		update();

		if (demo)
			_rd->advanceFrame();
		else
			_rd->finishFrame();

		_sputText->resume();

		if (!demo)
			_sputPrimitive->resume();
	}

	copyGFXBuffer();

	StdVBuffer *buffer = _cleanArtBuffer;
	StdColorFormat *format = &buffer->tex.format;

	if (shift < 0 || shift > 31 || format->bpp < 16 || format->greenBits < 1 || format->greenBits > 32 ||
			format->blueBits < 1 || format->blueBits > 32 || format->redShift < 0 || format->redShift > 31 ||
			format->greenShift < 0 || format->greenShift > 31 || format->redLoss < 0 || format->redLoss > 31 ||
			format->greenLoss < 0 || format->greenLoss > 31 || format->blueLoss < 0 || format->blueLoss > 31) {
		warning("SputRender::dim(): unsupported shift or color format");
		return;
	}

	_stdDisplay->vBufferLock(buffer);

	uint greenMask = 0xFFFFFFFF >> (32 - format->greenBits);
	uint blueMask = 0xFFFFFFFF >> (32 - format->blueBits);

	for (int y = 0; y < buffer->tex.height; y++) {
		uint16 *pixelPointer = (uint16 *)(buffer->pixels + buffer->tex.pitchInBytes * y);

		for (int x = 0; x < buffer->tex.width; x++) {
			int room = _sputRoom->getCurrentRoom() != nullptr;
			uint pixel;

			// The demo doesn't clear the pixels when there is no room
			if (room || demo)
				pixel = *pixelPointer;
			else
				pixel = 0;

			int red = (int)((pixel >> format->redShift) << format->redLoss);
			int green = (int)((greenMask & (pixel >> format->greenShift)) << format->greenLoss);
			int blue = (int)(blueMask & pixel);
			int gray = MAX(MAX(red, green), blue);

			gray = gray >> shift;
			*pixelPointer = (uint16)((gray >> format->blueLoss) | ((gray >> format->greenLoss) << format->greenShift) |
					((gray >> format->redLoss) << format->redShift));
			pixelPointer++;
		}
	}

	_stdDisplay->vBufferUnlock(buffer);

	StdRect rect;

	rect.x = 0;
	rect.y = 0;
	rect.width = buffer->tex.width;
	rect.height = buffer->tex.height;
	markAllPagesDirty(&rect, 0);
}

void SputRender::dimRegion(int x, int y, int width, int height, float scale) {
	StdColorFormat *format = &_backBuffer->tex.format;

	if (format->bpp != 16 || format->redBits < 1 || format->redBits > 32 || format->greenBits < 1 || format->greenBits > 32 ||
			format->blueBits < 1 || format->blueBits > 32 || format->redShift < 0 || format->redShift > 31 ||
			format->greenShift < 0 || format->greenShift > 31 || format->blueShift < 0 || format->blueShift > 31) {
		warning("SputRender::dimRegion(): unsupported color format");
		return;
	}

	if (x < 0 || y < 0 || width < 0 || height < 0 || x + width > _backBuffer->tex.width || y + height > _backBuffer->tex.height) {
		warning("SputRender::dimRegion(): region outside of the back buffer");
		return;
	}

	if (!(scale >= -1000000.0f && scale <= 1000000.0f)) {
		warning("SputRender::dimRegion(): invalid scale");
		return;
	}

	uint redMask = 0xFFFFFFFF >> (32 - format->redBits);
	uint greenMask = 0xFFFFFFFF >> (32 - format->greenBits);
	uint blueMask = 0xFFFFFFFF >> (32 - format->blueBits);

	_stdDisplay->vBufferLock(_backBuffer);

	byte *rowPointer = _backBuffer->pixels + 2 * x + _backBuffer->tex.pitchInBytes * y;

	for (int row = 0; row < height; row++) {
		uint16 *pixelPointer = (uint16 *)rowPointer;

		for (int column = 0; column < width; column++) {
			uint pixel = *pixelPointer;
			int red = (int)(int64)((double)(redMask & (uint16)(pixel >> format->redShift)) * (double)scale);
			int green = (int)(int64)((double)(greenMask & (uint16)(pixel >> format->greenShift)) * (double)scale);
			int blue = (int)(int64)((double)(blueMask & (uint16)(pixel >> format->blueShift)) * (double)scale);

			*pixelPointer = (uint16)(((uint)red << format->redShift) | ((uint)green << format->greenShift) | ((uint)blue << format->blueShift));
			pixelPointer++;
		}

		rowPointer += _backBuffer->tex.pitchInBytes;
	}

	_stdDisplay->vBufferUnlock(_backBuffer);
}

void SputRender::getIris(float percent, int x, int y) {
	if (!(percent >= 0.0f && percent <= 1.0f)) {
		warning("SputRender::getIris(): percent is not between 0 and 1");
		return;
	}

	if (x < 0 || x > 640 || y < 0 || y > 480 || _backBuffer->tex.pitchInBytes < 1280 || _backBuffer->tex.height < 480) {
		warning("SputRender::getIris(): iris center or back buffer out of range");
		return;
	}

	int top = (int)((double)y * (double)percent);
	int left = (int)((double)x * (double)percent);
	int bottom = (int)((double)(480 - y) * (double)percent);
	int right = (int)((double)(640 - x) * (double)percent);
	byte *pixels = _backBuffer->pixels;
	int pitch = _backBuffer->tex.pitchInBytes;

	_stdDisplay->vBufferLock(_backBuffer);

	int row = 0;

	if (top > 0) {
		byte *rowPointer = pixels;

		row = top;

		for (int i = 0; i < top; i++) {
			memset(rowPointer, 0, 1280);
			rowPointer += pitch;
		}
	}

	if (row < 480 - bottom) {
		byte *leftPointer = pixels + row * pitch;
		byte *rightPointer = pixels + 2 * (640 - right) + row * pitch;
		int numRows = 480 - bottom - row;

		for (int i = 0; i < numRows; i++) {
			memset(leftPointer, 0, 2 * left);
			leftPointer += pitch;
			memset(rightPointer, 0, 2 * right);
			rightPointer += pitch;
		}

		row = 480 - bottom;
	}

	if (row < 480) {
		byte *rowPointer = pixels + row * pitch;

		for (int i = 0; i < 480 - row; i++) {
			memset(rowPointer, 0, 1280);
			rowPointer += pitch;
		}
	}

	_stdDisplay->vBufferUnlock(_backBuffer);
}

void SputRender::irisDown(int x, int y, int duration) {
	int gameTime = _sputTime->getGameTime();

	_irisStart = gameTime;
	_irisCenterY = y;
	_irisDuration = (float)duration;
	_irisCenterX = x;
	_renderFlags |= 4;
}

void SputRender::irisUp(int x, int y, int duration) {
	int gameTime = _sputTime->getGameTime();

	_irisStart = gameTime;
	_irisCenterY = y;
	_irisCenterX = x;
	_irisDuration = (float)(-(double)duration);
	_renderFlags |= 4;
}

int SputRender::irisComplete() {
	if ((_renderFlags & 8) != 0)
		return 1;

	return ((uint)~_renderFlags >> 2) & 1;
}

void SputRender::clearIris() {
	StdRect rect;

	_renderFlags &= ~0xC;
	rect.width = 640;
	rect.height = 480;
	rect.y = 0;
	rect.x = 0;
	markAllPagesDirty(&rect, 0);
}

void SputRender::drawProgressBar(float progress) {
	uint16 color = 0xFFE0;

	if (progress > 1.0f)
		progress = 1.0f;

	colorCorrect(&color, 1);

	if (_mainGfxBuffer->tex.format.bpp != 16 || _mainGfxBuffer->tex.width < 590 || _mainGfxBuffer->tex.height < 468) {
		warning("SputRender::drawProgressBar(): unsupported buffer");
		return;
	}

	double barWidth = (double)progress * 540.0;
	int filled = 0;

	if (barWidth > 0.0)
		filled = (int)barWidth;

	// In the Remastered the bar is drawn into the front surface, which is the picture that the next flip shows
	HdCanvas *canvas = _grim->getHdCanvas();
	bool inFrontScene = _grim->isRemastered() && canvas && canvas->isActive();
	byte *pixels = nullptr;
	int pitch = _mainGfxBuffer->tex.pitchInBytes;

	if (inFrontScene) {
		int frontWidth = 0;
		int frontHeight = 0;

		pixels = canvas->getFrontScene(_mainGfxBuffer, &pitch, &frontWidth, &frontHeight);

		if (!pixels || frontWidth < 590 || frontHeight < 468) {
			warning("SputRender::drawProgressBar(): unsupported front surface");
			return;
		}
	} else {
		_stdDisplay->vBufferLock(_mainGfxBuffer);
		pixels = _mainGfxBuffer->pixels;
	}

	byte *rowPointer = pixels + 455 * pitch;
	uint16 *pixel = (uint16 *)(rowPointer + 100);

	for (int i = 0; i < 540; i++)
		*pixel++ = color;

	for (int row = 0; row < 12; row++) {
		rowPointer += 2 * ((uint)pitch >> 1);
		pixel = (uint16 *)(rowPointer + 100);
		((uint16 *)rowPointer)[589] = 0xFFE0;
		((uint16 *)rowPointer)[50] = 0xFFE0;

		for (int i = 0; i < filled; i++)
			*pixel++ = color;
	}

	pixel = (uint16 *)(rowPointer + 100);

	for (int i = 0; i < 540; i++)
		*pixel++ = color;

	if (!inFrontScene)
		_stdDisplay->vBufferUnlock(_mainGfxBuffer);
}

void SputRender::allowLoadImage(int enable) {
	_renderEnable = enable;
}

void SputRender::drawLoadingImage() {
	if (_loadImageAllowed && _renderEnable) {
		int perfCounterDiff = _grim->queryPerfCounterDiff(12);

		_grim->setPerfCounter(12);
		int elapsed = perfCounterDiff + _perfCounter;

		_perfCounter += perfCounterDiff;

		if (_perfCounter > 5000) {
			_perfCounter = 0;
			_waitBitmapTimerMs = 0;
			return;
		}

		if (elapsed > 3500) {
			_perfCounter = 3501;
			_waitBitmapTimerMs += perfCounterDiff;

			if (_waitBitmapTimerMs >= 250) {
				_waitBitmapTimerMs = 0;

				if (!_waitBitmapRef.valid) {
					_loadImageAllowed = 0;

					void *handle = _sputResource->alloc(kSputTagBknd, "wait.bm");

					if (!handle)
						return;

					_res->setReference(&_waitBitmapRef, handle);
					_loadImageAllowed = 1;

					if (!_waitBitmapRef.valid)
						error("SputRender::drawLoadingImage(): Bad Bitmap Reference");
				}

				StdBitmapStruct *bitmap = (StdBitmapStruct *)_waitBitmapRef.handle;

				// The original could divide by zero here, let's avoid that...
				if (bitmap->numFrames == 0) {
					warning("SputRender::drawLoadingImage(): the loading bitmap has no frames");
					return;
				}

				_waitBitmapFrame = (int)((uint)(_waitBitmapFrame + 1) % bitmap->numFrames);
				_stdDisplay->vBufferCopy(_mainGfxBuffer, bitmap->frames[_waitBitmapFrame], bitmap->xPos, bitmap->yPos, nullptr, 1);

				StdRect rect;

				rect.x = bitmap->xPos;
				rect.y = bitmap->yPos;
				rect.width = bitmap->frames[0]->tex.width;
				rect.height = bitmap->frames[0]->tex.height;
				markAllPagesDirty(&rect, 1);
			}
		}
	}
}

} // End of namespace Grim
