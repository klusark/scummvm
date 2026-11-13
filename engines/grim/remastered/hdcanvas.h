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

#ifndef GRIM_HDCANVAS_H
#define GRIM_HDCANVAS_H

#include "common/array.h"
#include "common/scummsys.h"

namespace Grim {

class OverlayManager;
struct StdVBuffer;

/**
 * The 1920x1080 output of the remastered in classic mode. The software surface of the
 * scene is scaled into the middle of it, then the 2D items of the frame (fills, overlays,
 * text) are blended in the order they were queued. The frame lives in system memory and is
 * handed to the backend with copyRectToScreen, so no GPU is needed.
 */
class HdCanvas {
public:
	enum {
		kWidth = 1920,
		kHeight = 1080
	};

	HdCanvas();
	~HdCanvas();

	// Output
	void open();
	void close();
	bool isActive() const { return _active; }
	void present(const StdVBuffer *scene, float gamma);

	// A 2D item put in the queue of the current frame. Pixels are straight (not premultiplied) RGBA8,
	// width * height * 4 bytes, owned by the caller and valid until the end of the frame.
	// "layer" is the original 2D layer/page number (text pages and overlay layers share the numbering of the original).
	void queueBitmap(int layer, const byte *rgba, int width, int height, int x, int y, byte alpha);

	// Removes the items queued with queueBitmap on a layer (the text pages are redone by their owner)
	void clearLayer(int layer);

	// Queue of the frame
	void beginFrame();
	void queueOverlay(int layer, const byte *premultipliedRgba, int srcWidth, int srcHeight, int x, int y, int width, int height,
			int mirror, float alpha, float desaturate);
	void queueFill(int x, int y, int width, int height, int red, int green, int blue, int alpha);
	void clearBuffer();
	void dropPixels(const byte *pixels);

	OverlayManager *getOverlays() const { return _overlays; }

private:
	enum ItemKind {
		kItemBitmap,
		kItemOverlay,
		kItemFill
	};

	struct Item {
		int kind;
		int layer;
		const byte *pixels;
		bool premultiplied;
		int srcWidth;
		int srcHeight;
		int x;
		int y;
		int width;
		int height;
		int mirror;
		int alpha;
		int desaturate;
		bool useGamma;
		int fillRed;
		int fillGreen;
		int fillBlue;
	};

	void buildSceneTables(int srcWidth, int srcHeight, int dstWidth, int dstHeight);
	void drawScene(const StdVBuffer *scene, float gamma);
	void drawItem(const Item &item);
	void drawFill(const Item &item);
	void drawImage(const Item &item);
	void buildGammaTable(byte *table, int size, double exponent);
	void freeSceneTables();

	uint32 packPixel(int red, int green, int blue) const;

	bool _active;
	uint32 *_frame;
	int _redShift;
	int _greenShift;
	int _blueShift;
	uint32 _opaqueMask;

	Common::Array<Item> _items;

	// Tables of the scaled scene
	int _sceneSrcWidth;
	int _sceneSrcHeight;
	int _sceneDstWidth;
	int _sceneDstHeight;
	int _sceneOffsetX;
	int _sceneOffsetY;
	int *_columnIndex;
	int *_columnWeight;
	int *_rowIndex;
	int *_rowWeight;
	byte *_sceneRgb;
	int _sceneRgbSize;

	// Gamma tables: the scene has one entry per 16-bit interpolated value, the overlays one per byte
	float _sceneGamma;
	byte *_sceneGammaTable;
	float _overlayGamma;
	byte _overlayGammaTable[256];

	OverlayManager *_overlays;
};

} // End of namespace Grim

#endif
