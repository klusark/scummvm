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

#include "common/list.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/util.h"

#include "graphics/pixelformat.h"

#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/std/std_display.h"

#include <math.h>

namespace Grim {

enum {
	// One entry per value of an interpolated 8-bit channel with 8 fraction bits
	kSceneGammaEntries = 255 * 256 + 1
};

HdCanvas::HdCanvas() : _active(false), _frame(nullptr), _redShift(16), _greenShift(8), _blueShift(0), _opaqueMask(0xFF000000),
		_sceneSrcWidth(0), _sceneSrcHeight(0), _sceneDstWidth(0), _sceneDstHeight(0), _sceneOffsetX(0), _sceneOffsetY(0),
		_columnIndex(nullptr), _columnWeight(nullptr), _rowIndex(nullptr), _rowWeight(nullptr), _sceneRgb(nullptr), _sceneRgbSize(0),
		_sceneGamma(-1.0f), _sceneGammaTable(nullptr), _overlayGamma(1.0f), _overlays(nullptr) {
	for (int i = 0; i < 256; i++)
		_overlayGammaTable[i] = (byte)i;

	_overlays = new OverlayManager(this);
}

HdCanvas::~HdCanvas() {
	close();
	delete _overlays;
	_overlays = nullptr;
	delete[] _sceneGammaTable;
	_sceneGammaTable = nullptr;
}

void HdCanvas::open() {
	Common::List<Graphics::PixelFormat> supported = g_system->getSupportedFormats();
	Graphics::PixelFormat wanted = Graphics::PixelFormat::createFormatBGRA32();

	for (Common::List<Graphics::PixelFormat>::const_iterator it = supported.begin(); it != supported.end(); ++it) {
		if (it->bytesPerPixel == 4 && it->rLoss == 0 && it->gLoss == 0 && it->bLoss == 0) {
			wanted = *it;
			break;
		}
	}

	initGraphics(kWidth, kHeight, &wanted);

	Graphics::PixelFormat format = g_system->getScreenFormat();

	if (format.bytesPerPixel != 4 || format.rLoss != 0 || format.gLoss != 0 || format.bLoss != 0)
		error("HdCanvas::open(): the screen does not support a 32-bit true color format");

	_redShift = format.rShift;
	_greenShift = format.gShift;
	_blueShift = format.bShift;
	_opaqueMask = 0;

	if (format.aLoss < 8)
		_opaqueMask = (uint32)(((1u << (8 - format.aLoss)) - 1) << format.aShift);

	if (!_frame)
		_frame = new uint32[kWidth * kHeight]();

	_active = true;
}

void HdCanvas::close() {
	_active = false;
	_items.clear();
	delete[] _frame;
	_frame = nullptr;
	freeSceneTables();
}

void HdCanvas::freeSceneTables() {
	delete[] _columnIndex;
	_columnIndex = nullptr;
	delete[] _columnWeight;
	_columnWeight = nullptr;
	delete[] _rowIndex;
	_rowIndex = nullptr;
	delete[] _rowWeight;
	_rowWeight = nullptr;
	delete[] _sceneRgb;
	_sceneRgb = nullptr;
	_sceneRgbSize = 0;
	_sceneSrcWidth = 0;
	_sceneSrcHeight = 0;
	_sceneDstWidth = 0;
	_sceneDstHeight = 0;
}

void HdCanvas::beginFrame() {
	_items.clear();
}

void HdCanvas::queueBitmap(int layer, const byte *rgba, int width, int height, int x, int y, byte alpha) {
	if (!rgba || width <= 0 || height <= 0)
		return;

	Item item = Item();

	item.kind = kItemBitmap;
	item.layer = layer;
	item.pixels = rgba;
	item.premultiplied = false;
	item.srcWidth = width;
	item.srcHeight = height;
	item.x = x;
	item.y = y;
	item.width = width;
	item.height = height;
	item.alpha = alpha;
	_items.push_back(item);
}

void HdCanvas::clearLayer(int layer) {
	for (int i = (int)_items.size() - 1; i >= 0; i--) {
		if (_items[i].kind == kItemBitmap && _items[i].layer == layer)
			_items.remove_at(i);
	}
}

void HdCanvas::queueOverlay(int layer, const byte *premultipliedRgba, int srcWidth, int srcHeight, int x, int y, int width, int height,
		int mirror, float alpha, float desaturate) {
	if (!premultipliedRgba || srcWidth <= 0 || srcHeight <= 0 || width <= 0 || height <= 0)
		return;

	Item item = Item();

	item.kind = kItemOverlay;
	item.layer = layer;
	item.pixels = premultipliedRgba;
	item.premultiplied = true;
	item.srcWidth = srcWidth;
	item.srcHeight = srcHeight;
	item.x = x;
	item.y = y;
	item.width = width;
	item.height = height;
	item.mirror = mirror;
	item.alpha = (int)(CLIP(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
	item.desaturate = (int)(CLIP(desaturate, 0.0f, 1.0f) * 256.0f + 0.5f);
	item.useGamma = true;
	_items.push_back(item);
}

void HdCanvas::queueFill(int x, int y, int width, int height, int red, int green, int blue, int alpha) {
	Item item = Item();

	item.kind = kItemFill;
	item.x = x;
	item.y = y;
	item.width = width;
	item.height = height;
	item.alpha = CLIP(alpha, 0, 255);
	item.fillRed = CLIP(red, 0, 255);
	item.fillGreen = CLIP(green, 0, 255);
	item.fillBlue = CLIP(blue, 0, 255);
	_items.push_back(item);
}

// Hides everything queued so far in the frame
void HdCanvas::clearBuffer() {
	queueFill(0, 0, kWidth, kHeight, 0, 0, 0, 255);
}

void HdCanvas::dropPixels(const byte *pixels) {
	for (uint i = 0; i < _items.size(); i++) {
		if (_items[i].pixels == pixels)
			_items[i].pixels = nullptr;
	}
}

uint32 HdCanvas::packPixel(int red, int green, int blue) const {
	return ((uint32)red << _redShift) | ((uint32)green << _greenShift) | ((uint32)blue << _blueShift) | _opaqueMask;
}

void HdCanvas::buildGammaTable(byte *table, int size, double exponent) {
	if (!(exponent > 0.0)) {
		warning("HdCanvas::buildGammaTable(): invalid gamma exponent, using 1.0");
		exponent = 1.0;
	}

	for (int i = 0; i < size; i++) {
		double value = pow((double)i / (double)(size - 1), exponent) * 255.0 + 0.5;

		table[i] = (byte)CLIP((int)value, 0, 255);
	}
}

void HdCanvas::present(const StdVBuffer *scene, float gamma) {
	if (!_active || !_frame)
		return;

	for (int i = 0; i < kWidth * kHeight; i++)
		_frame[i] = _opaqueMask;

	drawScene(scene, gamma);

	if (gamma != _overlayGamma) {
		buildGammaTable(_overlayGammaTable, 256, gamma);
		_overlayGamma = gamma;
	}

	for (uint i = 0; i < _items.size(); i++)
		drawItem(_items[i]);

	g_system->copyRectToScreen(_frame, kWidth * 4, 0, 0, kWidth, kHeight);
	g_system->updateScreen();
}

void HdCanvas::buildSceneTables(int srcWidth, int srcHeight, int dstWidth, int dstHeight) {
	if (_columnIndex && _sceneSrcWidth == srcWidth && _sceneSrcHeight == srcHeight)
		return;

	freeSceneTables();

	_sceneSrcWidth = srcWidth;
	_sceneSrcHeight = srcHeight;
	_sceneDstWidth = dstWidth;
	_sceneDstHeight = dstHeight;
	_sceneOffsetX = (kWidth - dstWidth) / 2;
	_sceneOffsetY = (kHeight - dstHeight) / 2;
	_columnIndex = new int[dstWidth];
	_columnWeight = new int[dstWidth];
	_rowIndex = new int[dstHeight];
	_rowWeight = new int[dstHeight];
	_sceneRgbSize = srcWidth * srcHeight * 3;
	_sceneRgb = new byte[_sceneRgbSize];

	// The sample positions are the centers of the destination pixels, in texel space
	for (int x = 0; x < dstWidth; x++) {
		double position = ((double)x + 0.5) * (double)srcWidth / (double)dstWidth - 0.5;
		int base = (int)floor(position);

		if (base < 0) {
			_columnIndex[x] = 0;
			_columnWeight[x] = 0;
		} else if (base >= srcWidth - 1) {
			_columnIndex[x] = srcWidth - 1;
			_columnWeight[x] = 0;
		} else {
			_columnIndex[x] = base;
			_columnWeight[x] = (int)((position - (double)base) * 256.0);
		}
	}

	for (int y = 0; y < dstHeight; y++) {
		double position = ((double)y + 0.5) * (double)srcHeight / (double)dstHeight - 0.5;
		int base = (int)floor(position);

		if (base < 0) {
			_rowIndex[y] = 0;
			_rowWeight[y] = 0;
		} else if (base >= srcHeight - 1) {
			_rowIndex[y] = srcHeight - 1;
			_rowWeight[y] = 0;
		} else {
			_rowIndex[y] = base;
			_rowWeight[y] = (int)((position - (double)base) * 256.0);
		}
	}
}

// The software surface is presented bilinear, with the gamma of the present shader
void HdCanvas::drawScene(const StdVBuffer *scene, float gamma) {
	if (!scene || !scene->pixels)
		return;

	const StdColorFormat *format = &scene->tex.format;
	int srcWidth = scene->tex.width;
	int srcHeight = scene->tex.height;

	if (srcWidth < 2 || srcHeight < 2 || format->bpp != 16) {
		warning("HdCanvas::drawScene(): unsupported scene surface");
		return;
	}

	double scale = MIN((double)kWidth / (double)srcWidth, (double)kHeight / (double)srcHeight);
	int dstWidth = MIN((int)((double)srcWidth * scale + 0.5), (int)kWidth);
	int dstHeight = MIN((int)((double)srcHeight * scale + 0.5), (int)kHeight);

	buildSceneTables(srcWidth, srcHeight, dstWidth, dstHeight);

	if (!_sceneGammaTable)
		_sceneGammaTable = new byte[kSceneGammaEntries];

	if (gamma != _sceneGamma) {
		buildGammaTable(_sceneGammaTable, kSceneGammaEntries, 0.8 * (double)gamma);
		_sceneGamma = gamma;
	}

	// Expand every texel to 8 bits per channel once
	byte redTable[256];
	byte greenTable[256];
	byte blueTable[256];
	int redMax = (1 << format->redBits) - 1;
	int greenMax = (1 << format->greenBits) - 1;
	int blueMax = (1 << format->blueBits) - 1;

	if (format->redBits < 1 || format->redBits > 8 || format->greenBits < 1 || format->greenBits > 8 ||
			format->blueBits < 1 || format->blueBits > 8) {
		warning("HdCanvas::drawScene(): unsupported scene color format");
		return;
	}

	for (int i = 0; i <= redMax; i++)
		redTable[i] = (byte)((i * 255 + redMax / 2) / redMax);

	for (int i = 0; i <= greenMax; i++)
		greenTable[i] = (byte)((i * 255 + greenMax / 2) / greenMax);

	for (int i = 0; i <= blueMax; i++)
		blueTable[i] = (byte)((i * 255 + blueMax / 2) / blueMax);

	for (int y = 0; y < srcHeight; y++) {
		const uint16 *source = (const uint16 *)(scene->pixels + y * scene->tex.pitchInBytes);
		byte *destination = _sceneRgb + y * srcWidth * 3;

		for (int x = 0; x < srcWidth; x++) {
			uint value = source[x];

			destination[0] = redTable[(value >> format->redShift) & redMax];
			destination[1] = greenTable[(value >> format->greenShift) & greenMax];
			destination[2] = blueTable[(value >> format->blueShift) & blueMax];
			destination += 3;
		}
	}

	int rowBytes = srcWidth * 3;

	for (int y = 0; y < dstHeight; y++) {
		int rowTop = _rowIndex[y];
		int rowBottom = MIN(rowTop + 1, srcHeight - 1);
		int weightY = _rowWeight[y];
		const byte *top = _sceneRgb + rowTop * rowBytes;
		const byte *bottom = _sceneRgb + rowBottom * rowBytes;
		uint32 *destination = _frame + (_sceneOffsetY + y) * kWidth + _sceneOffsetX;

		for (int x = 0; x < dstWidth; x++) {
			int left = _columnIndex[x] * 3;
			int right = MIN(_columnIndex[x] + 1, srcWidth - 1) * 3;
			int weightX = _columnWeight[x];
			int channel[3];

			for (int c = 0; c < 3; c++) {
				int upper = top[left + c] * (256 - weightX) + top[right + c] * weightX;
				int lower = bottom[left + c] * (256 - weightX) + bottom[right + c] * weightX;

				channel[c] = _sceneGammaTable[(upper * (256 - weightY) + lower * weightY) >> 8];
			}

			destination[x] = packPixel(channel[0], channel[1], channel[2]);
		}
	}
}

void HdCanvas::drawItem(const Item &item) {
	if (item.kind == kItemFill)
		drawFill(item);
	else
		drawImage(item);
}

void HdCanvas::drawFill(const Item &item) {
	int left = MAX(item.x, 0);
	int top = MAX(item.y, 0);
	int right = MIN(item.x + item.width, (int)kWidth);
	int bottom = MIN(item.y + item.height, (int)kHeight);

	if (left >= right || top >= bottom || item.alpha <= 0)
		return;

	int alpha = item.alpha;
	int red = (item.fillRed * alpha + 127) / 255;
	int green = (item.fillGreen * alpha + 127) / 255;
	int blue = (item.fillBlue * alpha + 127) / 255;
	uint32 solid = packPixel(item.fillRed, item.fillGreen, item.fillBlue);
	int inverse = 255 - alpha;

	for (int y = top; y < bottom; y++) {
		uint32 *destination = _frame + y * kWidth;

		for (int x = left; x < right; x++) {
			if (alpha == 255) {
				destination[x] = solid;
				continue;
			}

			uint32 value = destination[x];
			int destRed = red + ((int)((value >> _redShift) & 0xFF) * inverse + 127) / 255;
			int destGreen = green + ((int)((value >> _greenShift) & 0xFF) * inverse + 127) / 255;
			int destBlue = blue + ((int)((value >> _blueShift) & 0xFF) * inverse + 127) / 255;

			destination[x] = packPixel(MIN(destRed, 255), MIN(destGreen, 255), MIN(destBlue, 255));
		}
	}
}

// Blends an RGBA image with premultiplied alpha, sampled with the nearest texel
void HdCanvas::drawImage(const Item &item) {
	if (!item.pixels || item.alpha <= 0)
		return;

	int left = MAX(item.x, 0);
	int top = MAX(item.y, 0);
	int right = MIN(item.x + item.width, (int)kWidth);
	int bottom = MIN(item.y + item.height, (int)kHeight);

	if (left >= right || top >= bottom)
		return;

	bool scaled = item.width != item.srcWidth || item.height != item.srcHeight;
	bool useGamma = item.useGamma && _overlayGamma != 1.0f;

	for (int y = top; y < bottom; y++) {
		int row = y - item.y;
		int sourceY = row;

		if (scaled)
			sourceY = (int)(((int64)(2 * row + 1) * item.srcHeight) / (int64)(2 * item.height));

		if (item.mirror & 2)
			sourceY = item.srcHeight - 1 - sourceY;

		const byte *sourceRow = item.pixels + (int64)sourceY * item.srcWidth * 4;
		uint32 *destination = _frame + y * kWidth;

		for (int x = left; x < right; x++) {
			int sourceX = x - item.x;

			if (scaled)
				sourceX = (int)(((int64)(2 * sourceX + 1) * item.srcWidth) / (int64)(2 * item.width));

			if (item.mirror & 1)
				sourceX = item.srcWidth - 1 - sourceX;

			const byte *pixel = sourceRow + sourceX * 4;
			int alpha = pixel[3];

			if (alpha == 0)
				continue;

			int red = pixel[0];
			int green = pixel[1];
			int blue = pixel[2];

			if (!item.premultiplied) {
				red = (red * alpha + 127) / 255;
				green = (green * alpha + 127) / 255;
				blue = (blue * alpha + 127) / 255;
			}

			if (item.desaturate > 0) {
				int luma = (red * 54 + green * 183 + blue * 18 + 128) >> 8;

				red += ((luma - red) * item.desaturate) >> 8;
				green += ((luma - green) * item.desaturate) >> 8;
				blue += ((luma - blue) * item.desaturate) >> 8;
			}

			if (useGamma) {
				red = _overlayGammaTable[CLIP(red, 0, 255)];
				green = _overlayGammaTable[CLIP(green, 0, 255)];
				blue = _overlayGammaTable[CLIP(blue, 0, 255)];
			}

			if (item.alpha != 255) {
				red = (red * item.alpha + 127) / 255;
				green = (green * item.alpha + 127) / 255;
				blue = (blue * item.alpha + 127) / 255;
				alpha = (alpha * item.alpha + 127) / 255;
			}

			if (alpha == 255) {
				destination[x] = packPixel(red, green, blue);
				continue;
			}

			uint32 value = destination[x];
			int inverse = 255 - alpha;
			int destRed = red + ((int)((value >> _redShift) & 0xFF) * inverse + 127) / 255;
			int destGreen = green + ((int)((value >> _greenShift) & 0xFF) * inverse + 127) / 255;
			int destBlue = blue + ((int)((value >> _blueShift) & 0xFF) * inverse + 127) / 255;

			destination[x] = packPixel(MIN(destRed, 255), MIN(destGreen, 255), MIN(destBlue, 255));
		}
	}
}

} // End of namespace Grim
