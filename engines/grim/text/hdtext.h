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

// FreeType types, declared here so that only hdtext.cpp includes the FreeType headers
struct FT_LibraryRec_;
struct FT_Span_;

namespace Grim {

struct BlastTextFont;

// A TrueType font at one pixel size, with the glyphs 0..254 rasterized once
class HdFont {
public:
	enum {
		kNumGlyphs = 255
	};

	struct Glyph {
		byte *alpha;
		int width;
		int height;
		float xOffset;
		float yOffset;
		float advance;
	};

	HdFont(FT_LibraryRec_ *library, const Common::String &path, float size, int minPixels);
	~HdFont();

	bool isLoaded() const { return _loaded; }
	const Common::String &getPath() const { return _path; }
	float getRequestedSize() const { return _requestedSize; }
	float getPixelSize() const { return _pixelSize; }
	const Glyph *getGlyph(int c) const;

private:
	HdFont(const HdFont &);
	HdFont &operator=(const HdFont &);

	static void spanCallback(int y, int count, const FT_Span_ *spans, void *user);
	static int floorDiv64(long value);
	static int ceilDiv64(long value);

	void loadGlyphs(FT_LibraryRec_ *library, const byte *data, long dataSize, int minPixels);

	Common::String _path;
	float _requestedSize;
	float _pixelSize;
	bool _loaded;
	Glyph _glyphs[kNumGlyphs];
	Glyph _emptyGlyph;
};

// One glyph box of the layout, in canvas pixels relative to the origin of the text
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

// What the layout needs to know about a text object (all in canvas pixels)
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
};

// Layout of one text object, kept until the object changes. The last four fields are
// coordinates, not sizes: the right and bottom edges of the unaligned text, and the
// top left corner of the aligned one.
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

// The state of the text canvas of the renderer
struct HdTextCanvas {
	int width;
	bool boxingHD;
	bool stretched;
};

// What the canvas draws for one text object: the boxes in order and the constants of the text shader
struct HdTextDraw {
	HdTextDraw();

	Common::Array<HdTextQuad> quads;
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

	// Finds FontsHD/<NAME>.txt for a locked font and links the TrueType font to it
	void attachFont(BlastTextFont *font, const char *name);
	const HdFont *getCustomFont();

	void layout(const HdTextInfo &info, HdTextCache *cache);
	void buildDraw(const HdTextInfo &info, const HdTextCache *cache, const HdTextCanvas &canvas, HdTextDraw *draw) const;

	static void queryText(const HdTextInfo &info, const HdTextCache *cache, const HdTextCanvas &canvas, int *x, int *y,
			int *width, int *height);
	static int getFontHeight(const HdFont *font);
	static int getTextPage(int flags);

	static void convertCoords1080(int *x, int *y, int mode, bool isSize);
	static void convertCoordsNative(int *x, int *y, int mode, bool isSize);
	static void convertRectNative(StdRect *rect, int mode);

private:
	HdText(const HdText &);
	HdText &operator=(const HdText &);

	const HdFont *loadFont(const Common::String &path, float size, int minPixels);
	void splitLines(HdTextCache *cache, const char *text, float maxWidth) const;
	void measure(HdTextCache *cache) const;
	void align(HdTextCache *cache, float alignX, float alignY) const;
	static bool isWordCharacter(int c);

	FT_LibraryRec_ *_library;
	Common::Array<HdFont *> _fonts;
	const HdFont *_customFont;
	bool _customFontTried;
};

} // End of namespace Grim

#endif
