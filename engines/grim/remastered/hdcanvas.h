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

/**
 * The 1920x1080 output of the remastered in classic mode. The software surface of the
 * scene is scaled into the middle of it, then the 2D items of the frame (fills, overlays,
 * text, movie) are blended in the order they were queued, with the arithmetic of the
 * shaders of the original. The frame lives in system memory and is handed to the backend
 * with copyRectToScreen, so no GPU is needed.
 */
class HdCanvas {
public:
	enum {
		kWidth = 1920,
		kHeight = 1080,
		kAreaWidth = 1440
	};

	HdCanvas();
	~HdCanvas();

	// Output
	void open();
	void close();
	bool isActive() const { return _active; }
	void present(const StdVBuffer *scene, float gamma);

	// A straight RGBA8 bitmap, width * height * 4 bytes owned by the caller and valid until the
	// end of the frame. "layer" is only the key of clearLayer.
	void queueBitmap(int layer, const byte *rgba, int width, int height, int x, int y, byte alpha);

	// Removes the items queued with queueBitmap on a layer
	void clearLayer(int layer);

	// Queue of the frame
	void beginFrame();

	// A premultiplied RGBA8 image drawn in the rectangle (x, y, width, height) of the output (pixels, fractions
	// allowed). The image is filtered bilinearly.
	void queueOverlay(const byte *premultipliedRgba, int srcWidth, int srcHeight, float x, float y, float width, float height,
			int mirror, float alpha, float desaturate);

	// The color is packed red | green << 8 | blue << 16, the rectangle is in the pixels of the text canvas
	void queueFilledRectangle(int left, int top, int right, int bottom, uint colour, float alpha);
	void queueText(const HdTextDraw &draw);

	// A frame of a movie, straight RGBA8, valid until the end of the frame. The serial changes whenever the pixels
	// change; a negative one tells that they may change at any time.
	void queueMovie(const byte *rgba, int width, int height, int contentSerial = -1);
	void clearBuffer();
	void dropPixels(const byte *pixels);

	// The gamma the quads queued from now on use
	void setGamma(float gamma);

	OverlayManager *getOverlays() const { return _overlays; }

	// The widescreen option 2 stretches the 4:3 area (kAreaWidth x kHeight) over the whole output.
	// The option is sampled at the end of every present, so a frame uses the value of the one before.
	bool isStretched() const { return _stretched; }
	float getAreaScaleX() const;
	float getWidescreenCorrection() const;

	// The canvas of the text: its width is the one of the engine, a width of the menus has the boxing
	HdTextCanvas getTextCanvas() const;

	// The scaling of an area of the given size over the output (the back buffer of the renderer is the output)
	static void computeScaling(bool stretch, int referenceWidth, int referenceHeight, float *scaleX, float *scaleY,
			float *offsetX, float *offsetY);

	// The front surface of the scene, the picture that the next present shows. The surface is made like the one of
	// the engine when there is none yet. The pixels, the pitch in bytes, the width and the height are returned.
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

		// Bitmap
		int layer;
		int bitmapX;
		int bitmapY;
		int bitmapAlpha;

		// Overlay and movie
		float x;
		float y;
		float width;
		float height;
		int mirror;
		float alpha;
		float desaturate;
		float gamma;

		// Fill: the constant color of the shader
		float fillRed;
		float fillGreen;
		float fillBlue;

		// Movie: the serial of the pixels. An item whose pixels may change without a new serial is "changing".
		int serial;
		bool changing;

		HdTextDraw text;
	};

	// The scene scaled into the output, kept while the surface of the scene and everything the scaling depends on stay
	// the same. The result is stored only when the same scene comes a second time.
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

	// The output byte of an exponent for every float between 0 and 1: the thresholds of the 255
	// values, and for every cell of the float range the number of thresholds below it
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

	// The surface of the scene that is presented: the one of the previous flip
	byte *_frontScene;
	StdColorFormat _sceneFormat;
	int _sceneWidth;
	int _sceneHeight;
	int _scenePitch;
	float _redLevels[256];
	float _greenLevels[256];
	float _blueLevels[256];

	// Opacity of the scene, it grows from 0 after the start
	float _softwareAlpha;
	float _softwareAlphaEnd;
	float _softwareAlphaStart;
	float _softwareAlphaBlendTime;
	double _previousFrameTime;
	bool _hasPreviousFrame;

	// The columns of the output that show the scene: the two texels each one blends and the weight of the second
	int _redMax;
	int _greenMax;
	int _blueMax;
	Common::Array<int> _columnFirst;
	Common::Array<int> _columnSecond;
	Common::Array<float> _columnFraction;

	// Two rows of the scene already filtered horizontally
	Common::Array<float> _horizontalRows[2];
	int _horizontalRowIndex[2];

	PowTable _scenePow;
	PowTable _moviePow;
	float _byteToFloat[256];

	Common::Array<float> _textScratch;
	Common::Array<float> _textRowFirst;
	Common::Array<float> _textRowSecond;

	// The columns and the two filtered source rows of the overlay or movie that is being drawn
	Common::Array<int> _itemColumnFirst;
	Common::Array<int> _itemColumnSecond;
	Common::Array<float> _itemColumnFraction;
	Common::Array<float> _itemRows[2];
	int _itemRowIndex[2];

	SceneCache _sceneCache[kSceneCacheEntries];
	uint32 _sceneUseCounter;
	int _sceneSerial;

	// What the last composed frame was made of, to skip a frame that would come out the same
	Common::Array<Item> _composedItems;
	int _composedSceneToken;
	bool _composedValid;

	OverlayManager *_overlays;
};

} // End of namespace Grim

#endif
