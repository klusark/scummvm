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

#ifndef GRIM_HDTEXT_H
#define GRIM_HDTEXT_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/std/std_display.h"

namespace Grim {

struct BlastTextFont;

class HdFont {
public:
	enum {
		kNumGlyphs = 255,
		kAtlasSize = 2048
	};

	struct Glyph {
		byte *alpha;
		int width;
		int height;
		int atlasX;
		int atlasY;
		float xOffset;
		float yOffset;
		float advance;
	};

	HdFont(const Common::String &path, float size, int minPixels, float scale);
	~HdFont();

	void regenerate(float scale);
	void writeAtlas(byte *destination) const;
	uint getGeneration() const { return _generation; }
	uint getAtlasUsedHeight() const { return _atlasUsedHeight; }
	bool isLoaded() const { return _loaded; }
	const Common::String &getPath() const { return _path; }
	float getRequestedSize() const { return _requestedSize; }
	float getPixelSize() const { return _pixelSize; }
	const Glyph *getGlyph(int c) const;

private:
	HdFont(const HdFont &);
	HdFont &operator=(const HdFont &);

	float computePixelSize(float scale) const;
	void freeGlyphs();
	void loadGlyphs(float scale);

	Common::String _path;
	float _requestedSize;
	int _minPixels;
	float _pixelSize;
	bool _loaded;
	byte *_data;
	long _dataSize;
	uint _generation;
	uint _atlasUsedHeight;
	Glyph _glyphs[kNumGlyphs];
	Glyph _emptyGlyph;
};

struct HdTextQuad {
	float x;
	float y;
	int width;
	int height;
	int character;
	const HdFont::Glyph *glyph;
};

struct HdTextRow {
	int firstQuad;
	int numQuads;
	float x;
	float y;
	float width;
	float height;
};

struct HdTextInfo {
	const char *text;
	const HdFont *font;
	int x;
	int y;
	int maxWidth;
	float alignX;
	float alignY;
	int color;
	float alpha;
	int firstLine;
	int lineCount;
	float widthScale;
};

class HdTextCache {
public:
	HdTextCache();

	Common::Array<HdTextQuad> quads;
	Common::Array<HdTextRow> rows;
	float pixelSize;
	float minX;
	float minY;
	float maxRight;
	float maxBottom;
};

struct HdTextCanvas {
	int width;
	int backWidth;
	int backHeight;
	bool boxingHD;
	bool stretched;
};

struct HdTextDraw {
	HdTextDraw();

	Common::Array<HdTextQuad> quads;
	const HdFont *font;
	float originX;
	float originY;
	float textRed;
	float textGreen;
	float textBlue;
	float alpha;
};

class HdText {
public:
	enum {
		kCanvasWidth = 1920,
		kCanvasHeight = 1080
	};

	HdText();
	~HdText();

	void attachFont(BlastTextFont *font, const char *name);
	const HdFont *getCustomFont();
	const HdFont *getDebugFont();
	void regenerate(float scale);

	void layout(const HdTextInfo &info, HdTextCache *cache);
	void buildDraw(const HdTextInfo &info, const HdTextCache *cache, const HdTextCanvas &canvas, HdTextDraw *draw) const;

	static void queryText(const HdTextInfo &info, const HdTextCache *cache, const HdTextCanvas &canvas, int *x, int *y,
			int *width, int *height);
	static int getFontHeight(const HdFont *font, const HdTextCanvas &canvas);
	static int getTextPage(int flags);

private:
	HdText(const HdText &);
	HdText &operator=(const HdText &);

	const HdFont *loadFont(const Common::String &path, float size, int minPixels);
	void splitLines(HdTextCache *cache, const char *text, float maxWidth) const;
	void measure(HdTextCache *cache) const;
	void align(HdTextCache *cache, float alignX, float alignY) const;
	static bool isWordCharacter(int c);

	Common::Array<HdFont *> _fonts;
	float _scale;
	const HdFont *_customFont;
	bool _customFontTried;
	const HdFont *_debugFont;
	bool _debugFontTried;
};

} // End of namespace Grim

#endif
