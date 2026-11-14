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

#include "engines/grim/std/std_display.h"
#include "engines/grim/text/hdtext.h"

namespace Grim {

class OverlayManager;
struct StdVBuffer;

class HdCanvas {
public:
	enum {
		kWidth = 1920,
		kHeight = 1080,
		kAreaWidth = 1440
	};

	HdCanvas();
	~HdCanvas();

	void open();
	void close();
	bool isActive() const { return _active; }
	void present(const StdVBuffer *scene, float gamma);

	void queueBitmap(int layer, const byte *rgba, int width, int height, int x, int y, byte alpha);

	void clearLayer(int layer);

	void beginFrame();

	void queueOverlay(const byte *premultipliedRgba, int srcWidth, int srcHeight, float x, float y, float width, float height,
			int mirror, float alpha, float desaturate);

	void queueFilledRectangle(int left, int top, int right, int bottom, uint colour, float alpha);
	void queueText(const HdTextDraw &draw);

	void queueMovie(const byte *rgba, int width, int height, int contentSerial = -1);
	void clearBuffer();
	void dropPixels(const byte *pixels);

	void setGamma(float gamma);

	OverlayManager *getOverlays() const { return _overlays; }

	// The option is sampled at the end of every present, so a frame uses the value of the one before
	bool isStretched() const { return _stretched; }
	float getAreaScaleX() const;
	float getWidescreenCorrection() const;

	HdTextCanvas getTextCanvas() const;

	static void computeScaling(bool stretch, int referenceWidth, int referenceHeight, float *scaleX, float *scaleY,
			float *offsetX, float *offsetY);

	byte *getFrontScene(const StdVBuffer *engineScene, int *pitchInBytes, int *width, int *height);

private:
	enum ItemKind {
		kItemBitmap,
		kItemOverlay,
		kItemFill,
		kItemText,
		kItemMovie
	};

	enum {
		kPowCells = 4096,
		kSceneCacheEntries = 2
	};

	struct Item {
		Item();

		int kind;
		const byte *pixels;
		int srcWidth;
		int srcHeight;

		int layer;
		int bitmapX;
		int bitmapY;
		int bitmapAlpha;

		float x;
		float y;
		float width;
		float height;
		int mirror;
		float alpha;
		float desaturate;
		float gamma;

		float fillRed;
		float fillGreen;
		float fillBlue;

		int serial;
		bool changing;

		HdTextDraw text;
	};

	struct SceneCache {
		SceneCache();
		~SceneCache();

		bool valid;
		bool hasFrame;
		int serial;
		uint32 lastUse;
		int width;
		int height;
		int pitch;
		int bpp;
		int redBits;
		int greenBits;
		int blueBits;
		int redShift;
		int greenShift;
		int blueShift;
		bool stretched;
		float gamma;
		size_t sourceSize;
		byte *source;
		uint32 *frame;

	private:
		SceneCache(const SceneCache &);
		SceneCache &operator=(const SceneCache &);
	};

	struct PowTable {
		PowTable();

		bool valid;
		float exponent;
		double threshold[255];
		byte cell[kPowCells + 1];
	};

	void updateSoftwareAlpha();
	void prepareFrontScene(const StdVBuffer *scene);
	void swapScene(const StdVBuffer *scene);
	void buildPowTable(PowTable *table, float exponent);
	byte lookupPow(const PowTable &table, float value) const;
	const float *getHorizontalRow(int row, int keepRow);
	const float *getItemRow(const byte *pixels, int srcWidth, int row, int keepRow, int channels, int columns);

	void releaseSceneCaches();
	bool isSceneSupported() const;
	bool sceneCacheMatches(const SceneCache &entry) const;
	SceneCache *findSceneCache(bool *matched);
	void storeSceneFrame(SceneCache *entry);
	int findCoveringItem() const;
	bool coversFrame(const Item &item) const;
	bool hasChangingItem() const;
	void composeFrame(int firstItem, SceneCache *entry, bool matched);
	void fillFrame();

	static bool sameFloat(float first, float second);
	static bool sameItem(const Item &first, const Item &second);
	static bool sameQueue(const Common::Array<Item> &first, const Common::Array<Item> &second);

	void drawScene();
	void drawItem(const Item &item);
	void drawBitmap(const Item &item);
	void drawOverlay(const Item &item);
	void drawFill(const Item &item);
	void drawText(const Item &item);
	void drawMovie(const Item &item);

	void blendPixel(int x, int y, float red, float green, float blue, float alpha);
	void writePixel(int x, int y, int red, int green, int blue);
	uint32 packPixel(int red, int green, int blue) const;

	static int toByte(float value);
	static int clampPixel(float value, int limit);

	bool _active;
	bool _stretched;
	uint32 *_frame;
	int _redShift;
	int _greenShift;
	int _blueShift;
	uint32 _opaqueMask;
	float _gamma;

	Common::Array<Item> _items;

	byte *_frontScene;
	StdColorFormat _sceneFormat;
	int _sceneWidth;
	int _sceneHeight;
	int _scenePitch;
	float _redLevels[256];
	float _greenLevels[256];
	float _blueLevels[256];

	float _softwareAlpha;
	float _softwareAlphaEnd;
	float _softwareAlphaStart;
	float _softwareAlphaBlendTime;
	double _previousFrameTime;
	bool _hasPreviousFrame;

	int _redMax;
	int _greenMax;
	int _blueMax;
	Common::Array<int> _columnFirst;
	Common::Array<int> _columnSecond;
	Common::Array<float> _columnFraction;

	Common::Array<float> _horizontalRows[2];
	int _horizontalRowIndex[2];

	PowTable _scenePow;
	PowTable _moviePow;
	float _byteToFloat[256];

	Common::Array<float> _textScratch;
	Common::Array<float> _textRowFirst;
	Common::Array<float> _textRowSecond;

	Common::Array<int> _itemColumnFirst;
	Common::Array<int> _itemColumnSecond;
	Common::Array<float> _itemColumnFraction;
	Common::Array<float> _itemRows[2];
	int _itemRowIndex[2];

	SceneCache _sceneCache[kSceneCacheEntries];
	uint32 _sceneUseCounter;
	int _sceneSerial;

	Common::Array<Item> _composedItems;
	int _composedSceneToken;
	bool _composedValid;

	OverlayManager *_overlays;
};

} // End of namespace Grim

#endif
