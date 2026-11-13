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

// FreeType includes files that contain forbidden symbols
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#include "common/archive.h"
#include "common/path.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

#include "engines/grim/text/blasttext.h"
#include "engines/grim/text/hdtext.h"

namespace Grim {

namespace {

struct HdSpanTarget {
	byte *pixels;
	int width;
	int height;
};

}

HdFont::HdFont(FT_LibraryRec_ *library, const Common::String &path, float size, int minPixels) :
		_path(path), _requestedSize(size), _pixelSize(0.0f), _loaded(false) {
	memset(_glyphs, 0, sizeof(_glyphs));
	memset(&_emptyGlyph, 0, sizeof(_emptyGlyph));

	if (!library)
		return;

	Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(Common::Path(path, '/'));

	if (!stream) {
		warning("HdFont: cannot open the font file '%s'", path.c_str());
		return;
	}

	long dataSize = (long)stream->size();

	if (dataSize <= 0) {
		warning("HdFont: the font file '%s' is empty", path.c_str());
		delete stream;
		return;
	}

	byte *data = new byte[dataSize];
	long readSize = (long)stream->read(data, dataSize);

	delete stream;

	if (readSize == dataSize)
		loadGlyphs(library, data, dataSize, minPixels);
	else
		warning("HdFont: cannot read the font file '%s'", path.c_str());

	delete[] data;
}

HdFont::~HdFont() {
	for (int i = 0; i < kNumGlyphs; i++)
		delete[] _glyphs[i].alpha;
}

int HdFont::floorDiv64(long value) {
	if (value >= 0)
		return (int)(value / 64);

	return -(int)((-value + 63) / 64);
}

int HdFont::ceilDiv64(long value) {
	if (value >= 0)
		return (int)((value + 63) / 64);

	return -(int)((-value) / 64);
}

void HdFont::spanCallback(int y, int count, const FT_Span_ *spans, void *user) {
	HdSpanTarget *target = static_cast<HdSpanTarget *>(user);

	if (y < 0 || y >= target->height)
		return;

	byte *row = target->pixels + (target->height - 1 - y) * target->width;

	for (int i = 0; i < count; i++) {
		int start = spans[i].x;
		int length = spans[i].len;

		for (int j = 0; j < length; j++) {
			int x = start + j;

			if (x >= 0 && x < target->width)
				row[x] = spans[i].coverage;
		}
	}
}

void HdFont::loadGlyphs(FT_LibraryRec_ *library, const byte *data, long dataSize, int minPixels) {
	FT_Face face = nullptr;

	if (FT_New_Memory_Face(library, (const FT_Byte *)data, (FT_Long)dataSize, 0, &face) != 0) {
		warning("HdFont: '%s' is not a valid font file", _path.c_str());
		return;
	}

	long cell = (long)face->ascender - (long)face->descender;

	if (cell <= 0) {
		warning("HdFont: '%s' has no usable metrics", _path.c_str());
		FT_Done_Face(face);
		return;
	}

	// The size goes in steps of 1/8 pixel and the scale makes the cell (ascent minus descent) that tall
	float pixels = ceilf(_requestedSize * 8.0f) / 8.0f;

	if (pixels < (float)minPixels)
		pixels = (float)minPixels;

	_pixelSize = pixels;

	float scale = pixels / (float)cell;
	FT_Matrix matrix;

	matrix.xx = (FT_Fixed)(scale * 64.0f * 65536.0f + 0.5f);
	matrix.xy = 0;
	matrix.yx = 0;
	matrix.yy = matrix.xx;

	for (int c = 0; c < kNumGlyphs; c++) {
		Glyph *glyph = &_glyphs[c];
		FT_UInt index = FT_Get_Char_Index(face, (FT_ULong)c);

		if (FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0)
			continue;

		FT_GlyphSlot slot = face->glyph;

		glyph->advance = (float)slot->metrics.horiAdvance * scale;

		if (pixels <= 18.0f)
			glyph->advance += 1.0f;

		if (slot->format != FT_GLYPH_FORMAT_OUTLINE || slot->outline.n_points == 0)
			continue;

		FT_Outline_Transform(&slot->outline, &matrix);

		FT_BBox box;

		FT_Outline_Get_CBox(&slot->outline, &box);

		int left = floorDiv64((long)box.xMin);
		int bottom = floorDiv64((long)box.yMin);
		int right = ceilDiv64((long)box.xMax);
		int top = ceilDiv64((long)box.yMax);
		int width = right - left;
		int height = top - bottom;

		glyph->xOffset = (float)left;
		glyph->yOffset = (float)-top;

		if (width <= 0 || height <= 0)
			continue;

		if (width > 4096 || height > 4096) {
			warning("HdFont: the glyph %d of '%s' is too big", c, _path.c_str());
			continue;
		}

		glyph->alpha = new byte[width * height]();
		glyph->width = width;
		glyph->height = height;

		FT_Outline_Translate(&slot->outline, (FT_Pos)(-left * 64), (FT_Pos)(-bottom * 64));

		HdSpanTarget target;

		target.pixels = glyph->alpha;
		target.width = width;
		target.height = height;

		FT_Raster_Params params;

		memset(&params, 0, sizeof(params));
		params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
		params.gray_spans = HdFont::spanCallback;
		params.user = &target;

		FT_Outline_Render(library, &slot->outline, &params);
	}

	FT_Done_Face(face);
	_loaded = true;
}

const HdFont::Glyph *HdFont::getGlyph(int c) const {
	// The byte 182 is the non-breaking space
	if (c == 182)
		c = 32;

	if (c < 0 || c >= kNumGlyphs)
		return &_emptyGlyph;

	return &_glyphs[c];
}

HdTextCache::HdTextCache() : pixelSize(0.0f), originX(0), originY(0), minX(0), minY(0), maxWidth(0), maxHeight(0),
		pixels(nullptr), bitmapWidth(0), bitmapHeight(0), bitmapX(0), bitmapY(0), bitmapAlpha(-1.0f),
		bitmapFirstLine(0), bitmapLineCount(0) {
}

HdTextCache::~HdTextCache() {
	freeBitmap();
}

// A negative alpha means that the bitmap has not been drawn yet
void HdTextCache::freeBitmap() {
	delete[] pixels;
	pixels = nullptr;
	bitmapWidth = 0;
	bitmapHeight = 0;
	bitmapAlpha = -1.0f;
}

HdText::HdText() : _library(nullptr), _customFont(nullptr), _customFontTried(false) {
	FT_Library library = nullptr;

	if (FT_Init_FreeType(&library) != 0) {
		warning("HdText: cannot initialize FreeType");
		return;
	}

	_library = library;
}

HdText::~HdText() {
	for (uint i = 0; i < _fonts.size(); i++)
		delete _fonts[i];

	_fonts.clear();

	if (_library)
		FT_Done_FreeType(_library);
}

const HdFont *HdText::loadFont(const Common::String &path, float size, int minPixels) {
	for (uint i = 0; i < _fonts.size(); i++) {
		if (_fonts[i]->getRequestedSize() == size && _fonts[i]->getPath().equalsIgnoreCase(path))
			return _fonts[i];
	}

	HdFont *font = new HdFont(_library, path, size, minPixels);

	if (!font->isLoaded()) {
		delete font;
		return nullptr;
	}

	_fonts.push_back(font);

	return font;
}

void HdText::attachFont(BlastTextFont *font, const char *name) {
	Common::String upperName(name);

	upperName.toUppercase();

	Common::String mapPath = "FontsHD/" + upperName + ".txt";
	Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(Common::Path(mapPath, '/'));

	if (!stream) {
		warning("HdText: the font mapping '%s' does not exist", mapPath.c_str());
		return;
	}

	char buffer[256];

	memset(buffer, 0, sizeof(buffer));
	stream->read(buffer, sizeof(buffer) - 1);
	delete stream;

	char fontName[256];
	float size = 0.0f;

	fontName[0] = 0;

	if (sscanf(buffer, "%255s %f", fontName, &size) != 2 || size <= 0.0f) {
		warning("HdText: the font mapping '%s' is invalid, expected a font name and a size in pixels", mapPath.c_str());
		return;
	}

	const HdFont *hdFont = loadFont(Common::String("FontsHD/") + fontName, size, 16);

	if (!hdFont) {
		warning("HdText: cannot load the font '%s' for '%s'", fontName, mapPath.c_str());
		return;
	}

	font->hdFont = hdFont;
}

const HdFont *HdText::getCustomFont() {
	if (!_customFontTried) {
		_customFontTried = true;
		_customFont = loadFont("FontsHD/FuturaStd-Heavy.ttf", 44.0f, 16);
	}

	return _customFont;
}

int HdText::getFontHeight(const HdFont *font) {
	return (int)ceilf(font->getPixelSize());
}

int HdText::getTextPage(int flags) {
	int page = 0;

	if (flags & 0x100)
		page = 3;

	if (flags & 0x80)
		page = 2;

	if (flags & 0x10)
		page = 1;

	if (flags & 0x200)
		page = 4;

	return page;
}

void HdText::convertCoords1080(int *x, int *y, int mode, bool isSize) {
	switch (mode) {
	case 0:
		*x = (int)((int64)1440 * *x / 640) + (isSize ? 0 : 240);
		*y = (int)((int64)1080 * *y / 480);
		break;
	case 1:
		if (!isSize)
			*x += 240;

		break;
	default:
		break;
	}
}

void HdText::convertCoordsNative(int *x, int *y, int mode, bool isSize) {
	switch (mode) {
	case 0:
		*x = (int)((int64)640 * (*x - (isSize ? 0 : 240)) / 1440);
		*y = (int)((int64)480 * *y / 1080);
		break;
	case 1:
		if (!isSize)
			*x -= 240;

		break;
	default:
		break;
	}
}

void HdText::convertRectNative(StdRect *rect, int mode) {
	switch (mode) {
	case 0:
		rect->x = (int)((int64)640 * (rect->x - 240) / 1440);
		rect->y = (int)((int64)480 * rect->y / 1080);
		rect->width = (int)((int64)640 * rect->width / 1440);
		rect->height = (int)((int64)480 * rect->height / 1080);
		break;
	case 1:
		rect->x -= 240;
		break;
	default:
		break;
	}
}

bool HdText::isWordCharacter(int c) {
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
		return true;

	if (c >= 33 && c <= 63 && ((0x440E8143u >> (c - 33)) & 1))
		return true;

	return (c & 0xC0) == 0xC0;
}

float HdText::coverage(const HdFont::Glyph *glyph, int x, int y) {
	if (x < 0 || y < 0 || x >= glyph->width || y >= glyph->height)
		return 0.0f;

	return (float)glyph->alpha[y * glyph->width + x] / 255.0f;
}

void HdText::layout(const HdTextInfo &info, HdTextCache *cache) {
	const HdFont *font = info.font;

	cache->quads.clear();
	cache->rows.clear();
	cache->freeBitmap();
	cache->pixelSize = font->getPixelSize();
	cache->originX = info.x;
	cache->originY = info.y;
	cache->minX = 0;
	cache->minY = 0;
	cache->maxWidth = 0;
	cache->maxHeight = 0;

	int baseLine = (int)ceilf(cache->pixelSize);
	float penX = 0.0f;

	// One box per byte, with two pixels around the glyph for the outline
	for (const byte *p = (const byte *)info.text; *p; p++) {
		const HdFont::Glyph *glyph = font->getGlyph(*p);
		HdTextQuad quad;

		quad.x = floorf(glyph->xOffset + penX + 0.5f) - 2.0f;
		quad.y = (float)(baseLine - 2) + floorf(glyph->yOffset + 0.5f);
		quad.width = glyph->width + 4;
		quad.height = glyph->height + 4;
		quad.character = *p;
		quad.glyph = glyph;
		cache->quads.push_back(quad);
		penX += glyph->advance;
	}

	splitLines(cache, info.text, (float)info.maxWidth);
	measure(cache);
	align(cache, info.alignX, info.alignY);
}

void HdText::splitLines(HdTextCache *cache, const char *text, float maxWidth) const {
	float lineHeight = ceilf(cache->pixelSize * 1.2f);
	float previousX = -999999.0f;
	float wordWidth = 0.0f;
	int wordStart = -1;
	int lineStart = 0;
	int numQuads = (int)cache->quads.size();

	for (int i = 0; i < numQuads; i++) {
		int c = (byte)text[i];
		bool isWord = isWordCharacter(c);
		float x = cache->quads[i].x;

		if (previousX != -999999.0f && previousX >= x)
			lineStart = i;

		float advance = (float)cache->quads[i].width;

		if (i + 1 < numQuads && cache->quads[i + 1].x > x)
			advance = cache->quads[i + 1].x - x;

		wordWidth += advance;

		if (c != 182 && (c == '\n' || x >= maxWidth)) {
			int breakAt = i;

			if (c != '\n' && wordStart != -1 && !(maxWidth < wordWidth))
				breakAt = wordStart;

			if (lineStart != breakAt) {
				float shift = cache->quads[breakAt].x;

				for (int j = breakAt; j < numQuads; j++) {
					cache->quads[j].x -= shift;
					cache->quads[j].y += lineHeight;
				}

				lineStart = breakAt;
			}
		}

		if (isWord) {
			if (wordStart == -1) {
				wordStart = i;
				wordWidth = 0.0f;
			}
		} else {
			wordStart = -1;
		}

		previousX = x;
	}
}

void HdText::measure(HdTextCache *cache) const {
	int numQuads = (int)cache->quads.size();
	int i = 0;

	// A new row starts where the horizontal position goes back
	while (i < numQuads) {
		int left = (int)cache->quads[i].x;
		int right = left + cache->quads[i].width;
		int top = (int)cache->quads[i].y;
		int bottom = top + cache->quads[i].height;
		int j = i + 1;

		while (j < numQuads && cache->quads[j].x >= cache->quads[j - 1].x) {
			int quadLeft = (int)cache->quads[j].x;
			int quadTop = (int)cache->quads[j].y;

			if (quadLeft < left)
				left = quadLeft;

			if (quadLeft + cache->quads[j].width > right)
				right = quadLeft + cache->quads[j].width;

			if (quadTop < top)
				top = quadTop;

			if (quadTop + cache->quads[j].height > bottom)
				bottom = quadTop + cache->quads[j].height;

			j++;
		}

		HdTextRow row;

		row.firstQuad = i;
		row.numQuads = j - i;
		row.x = left;
		row.y = top;
		row.width = right - left;
		row.height = bottom - top;
		cache->rows.push_back(row);

		if (row.width > cache->maxWidth)
			cache->maxWidth = row.width;

		if (bottom > cache->maxHeight)
			cache->maxHeight = bottom;

		i = j;
	}
}

void HdText::align(HdTextCache *cache, float alignX, float alignY) const {
	int numRows = (int)cache->rows.size();

	if (numRows == 0)
		return;

	for (int i = 0; i < numRows; i++) {
		HdTextRow *row = &cache->rows[i];
		int shift = -(int)ceilf((float)row->width * alignX);

		for (int j = 0; j < row->numQuads; j++)
			cache->quads[row->firstQuad + j].x += (float)shift;

		row->x += shift;
	}

	float height = cache->pixelSize;
	float blockHeight = height + 1.2f * height * (float)(numRows - 1);
	int shiftY = -(int)ceilf(blockHeight * alignY);

	if (shiftY != 0) {
		for (uint i = 0; i < cache->quads.size(); i++)
			cache->quads[i].y += (float)shiftY;

		for (int i = 0; i < numRows; i++)
			cache->rows[i].y += shiftY;
	}

	cache->minX = cache->rows[0].x;
	cache->minY = cache->rows[0].y;

	for (int i = 1; i < numRows; i++) {
		if (cache->rows[i].x < cache->minX)
			cache->minX = cache->rows[i].x;

		if (cache->rows[i].y < cache->minY)
			cache->minY = cache->rows[i].y;
	}
}

void HdText::render(const HdTextInfo &info, HdTextCache *cache) {
	cache->freeBitmap();
	cache->bitmapAlpha = info.alpha;
	cache->bitmapFirstLine = info.firstLine;
	cache->bitmapLineCount = info.lineCount;

	int numRows = (int)cache->rows.size();
	int firstRow = info.firstLine;

	if (firstRow < 0)
		firstRow = 0;

	if (firstRow >= numRows)
		return;

	int lastRow = numRows;

	if (info.lineCount < numRows - firstRow)
		lastRow = firstRow + (info.lineCount > 0 ? info.lineCount : 0);

	if (lastRow <= firstRow)
		return;

	int firstQuad = cache->rows[firstRow].firstQuad;
	int endQuad = cache->rows[lastRow - 1].firstQuad + cache->rows[lastRow - 1].numQuads;
	int shiftY = 0;

	if (firstRow > 0)
		shiftY = -(int)floorf((float)firstRow * cache->pixelSize * 1.21f + 0.5f);

	// The rectangle that holds every box of the visible rows, cut to the canvas
	int left = kCanvasWidth;
	int top = kCanvasHeight;
	int right = 0;
	int bottom = 0;

	for (int i = firstQuad; i < endQuad; i++) {
		const HdTextQuad &quad = cache->quads[i];
		int quadLeft = info.x + (int)quad.x;
		int quadTop = info.y + shiftY + (int)quad.y;

		if (quadLeft < left)
			left = quadLeft;

		if (quadTop < top)
			top = quadTop;

		if (quadLeft + quad.width > right)
			right = quadLeft + quad.width;

		if (quadTop + quad.height > bottom)
			bottom = quadTop + quad.height;
	}

	if (left < 0)
		left = 0;

	if (top < 0)
		top = 0;

	if (right > kCanvasWidth)
		right = kCanvasWidth;

	if (bottom > kCanvasHeight)
		bottom = kCanvasHeight;

	if (right <= left || bottom <= top)
		return;

	int width = right - left;
	int height = bottom - top;
	float *colorCover = new float[width * height]();
	float *alphaCover = new float[width * height]();
	float alpha = info.alpha;

	if (alpha < 0.0f)
		alpha = 0.0f;

	if (alpha > 1.0f)
		alpha = 1.0f;

	// The text shader draws, for every box, the glyph over a black halo that is the sum of the
	// glyph shifted around by a pixel, and the boxes are blended in order
	static const int haloOffsets[11][2] = {
		{ -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 },
		{ -2, -2 }, { -2, -1 }, { -1, -2 }
	};

	for (int i = firstQuad; i < endQuad; i++) {
		const HdTextQuad &quad = cache->quads[i];
		const HdFont::Glyph *glyph = quad.glyph;

		if (!glyph->alpha)
			continue;

		int quadLeft = info.x + (int)quad.x;
		int quadTop = info.y + shiftY + (int)quad.y;
		int startX = quadLeft > left ? quadLeft : left;
		int startY = quadTop > top ? quadTop : top;
		int endX = quadLeft + quad.width < right ? quadLeft + quad.width : right;
		int endY = quadTop + quad.height < bottom ? quadTop + quad.height : bottom;

		for (int y = startY; y < endY; y++) {
			for (int x = startX; x < endX; x++) {
				int glyphX = x - (quadLeft + 2);
				int glyphY = y - (quadTop + 2);
				float cover = coverage(glyph, glyphX, glyphY);
				float halo = 0.0f;

				for (int k = 0; k < 11; k++)
					halo += coverage(glyph, glyphX + haloOffsets[k][0], glyphY + haloOffsets[k][1]);

				if (halo > 1.0f)
					halo = 1.0f;

				float outAlpha = alpha * (cover + halo * (1.0f - cover));
				float outColor = alpha * cover;
				int index = (y - top) * width + (x - left);

				alphaCover[index] = outAlpha + alphaCover[index] * (1.0f - outAlpha);
				colorCover[index] = outColor + colorCover[index] * (1.0f - outAlpha);
			}
		}
	}

	int red = info.color & 0xFF;
	int green = (info.color >> 8) & 0xFF;
	int blue = (info.color >> 16) & 0xFF;
	byte *pixels = new byte[width * height * 4];

	for (int i = 0; i < width * height; i++) {
		float a = alphaCover[i];
		byte *dest = pixels + i * 4;

		if (a <= 0.0f) {
			dest[0] = 0;
			dest[1] = 0;
			dest[2] = 0;
			dest[3] = 0;
			continue;
		}

		if (a > 1.0f)
			a = 1.0f;

		float fraction = colorCover[i] / a;

		if (fraction > 1.0f)
			fraction = 1.0f;

		dest[0] = (byte)((float)red * fraction + 0.5f);
		dest[1] = (byte)((float)green * fraction + 0.5f);
		dest[2] = (byte)((float)blue * fraction + 0.5f);
		dest[3] = (byte)(a * 255.0f + 0.5f);
	}

	delete[] colorCover;
	delete[] alphaCover;

	cache->pixels = pixels;
	cache->bitmapWidth = width;
	cache->bitmapHeight = height;
	cache->bitmapX = left;
	cache->bitmapY = top;
}

} // End of namespace Grim
