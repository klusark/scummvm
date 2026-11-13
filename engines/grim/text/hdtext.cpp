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
#include "common/util.h"

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

HdTextCache::HdTextCache() : pixelSize(0.0f), minX(0.0f), minY(0.0f), maxRight(0.0f), maxBottom(0.0f) {
}

HdTextDraw::HdTextDraw() : originX(0.0f), originY(0.0f), textRed(0.0f), textGreen(0.0f), textBlue(0.0f), alpha(0.0f) {
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

	if (c >= 33 && c <= 63 && ((0x44002843u >> (c - 33)) & 1))
		return true;

	return (c & 0xC0) == 0xC0;
}

void HdText::layout(const HdTextInfo &info, HdTextCache *cache) {
	const HdFont *font = info.font;

	cache->quads.clear();
	cache->rows.clear();
	cache->pixelSize = font->getPixelSize();
	cache->minX = 0.0f;
	cache->minY = 0.0f;
	cache->maxRight = 0.0f;
	cache->maxBottom = 0.0f;

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
	float lineHeight = (float)(int)ceilf(cache->pixelSize * 1.2f);
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

		float advance = (x + (float)cache->quads[i].width) - x;

		if (i + 1 < numQuads && cache->quads[i + 1].x > x)
			advance = cache->quads[i + 1].x - x;

		wordWidth += advance;

		// A newline is not a hard break: it breaks like a character that is past the right edge
		if (c != 182 && (c == '\n' || x >= maxWidth)) {
			int breakAt = i;

			if (wordStart != -1 && !(maxWidth < wordWidth))
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
	float previousLeft = 9999999.0f;
	float maxRight = 0.0f;
	float maxBottom = 0.0f;
	int line = -1;

	for (int i = 0; i < numQuads; i++) {
		float left = cache->quads[i].x;
		float top = cache->quads[i].y;
		float right = left + (float)cache->quads[i].width;
		float bottom = top + (float)cache->quads[i].height;

		// A new line starts where the horizontal position does not increase
		if (previousLeft >= left) {
			HdTextRow newRow;

			newRow.firstQuad = i;
			newRow.numQuads = 0;
			newRow.x = left;
			newRow.y = top;
			newRow.width = 0.0f;
			newRow.height = 0.0f;
			cache->rows.push_back(newRow);
			line++;
		}

		previousLeft = left;

		HdTextRow *row = &cache->rows[line];

		if (right - row->x > row->width)
			row->width = right - row->x;

		if (bottom - row->y > row->height)
			row->height = bottom - row->y;

		if (right > maxRight)
			maxRight = right;

		if (bottom > maxBottom)
			maxBottom = bottom;

		row->numQuads++;
	}

	// These two are the largest right and bottom coordinates, taken before the alignment
	cache->maxRight = maxRight;
	cache->maxBottom = maxBottom;
}

void HdText::align(HdTextCache *cache, float alignX, float alignY) const {
	int numRows = (int)cache->rows.size();
	float height = cache->pixelSize;

	// The block is one line high plus 1.2 lines for every other line (not rounded here)
	float firstLine = numRows > 1 ? 1.0f : (float)numRows;
	float otherLines = numRows - 1 > 0 ? (float)(numRows - 1) : 0.0f;
	float shiftY = ceilf(-((firstLine * height + (height * 1.2f) * otherLines) * alignY));
	float blockMinX = 999999.0f;
	float blockMinY = 999999.0f;

	for (int i = 0; i < numRows; i++) {
		HdTextRow *row = &cache->rows[i];
		float shiftX = ceilf(-(row->width * alignX));
		float minX = 999999.0f;
		float minY = 999999.0f;

		for (int j = 0; j < row->numQuads; j++) {
			HdTextQuad *quad = &cache->quads[row->firstQuad + j];

			quad->x += shiftX;
			quad->y += shiftY;

			if (quad->x < minX)
				minX = quad->x;

			if (quad->y < minY)
				minY = quad->y;
		}

		row->x = minX;
		row->y = minY;

		if (minX < blockMinX)
			blockMinX = minX;

		if (minY < blockMinY)
			blockMinY = minY;
	}

	cache->minX = blockMinX;
	cache->minY = blockMinY;
}

void HdText::buildDraw(const HdTextInfo &info, const HdTextCache *cache, const HdTextCanvas &canvas, HdTextDraw *draw) const {
	draw->quads.clear();

	int numRows = (int)cache->rows.size();

	if (numRows == 0)
		return;

	bool stretch = canvas.stretched && !canvas.boxingHD;
	float backWidth = (float)kCanvasWidth;
	float backHeight = (float)kCanvasHeight;
	float canvasWidth = (float)canvas.width;
	float canvasHeight = 1080.0f;
	float scaleX = backWidth / canvasWidth;
	float scaleY;
	float offsetX = 0.0f;
	float offsetY = 0.0f;

	if (stretch) {
		scaleY = backHeight / canvasHeight;
	} else {
		scaleY = MIN(backHeight / canvasHeight, scaleX);
		offsetY = (backHeight - canvasHeight * scaleY) * 0.5f;
		scaleX = scaleY;
		offsetX = (backWidth - canvasWidth * scaleY) * 0.5f;
	}

	float shiftX = (float)(kCanvasWidth - canvas.width);
	float baseX = (shiftX * scaleX) * -0.5f;

	draw->originX = (baseX + offsetX) + scaleX * (float)info.x;
	draw->originY = offsetY + scaleY * (float)info.y;

	// The first line is clamped to the last one, the count to the lines there are, and the
	// block always has at least its first line
	int firstRow = info.firstLine;

	if (numRows - 1 <= firstRow)
		firstRow = numRows - 1;

	if (firstRow < 0)
		firstRow = 0;

	int count = info.lineCount;

	if (count > numRows)
		count = numRows;

	int lastRow = count + firstRow - 1;

	if (lastRow < firstRow)
		lastRow = firstRow;

	// A last line past the end draws everything that is left
	int firstQuad = cache->rows[firstRow].firstQuad;
	int endQuad = (int)cache->quads.size();

	if (lastRow < numRows)
		endQuad = cache->rows[lastRow].firstQuad + cache->rows[lastRow].numQuads;

	if (firstRow != 0)
		draw->originY = (((float)firstRow * cache->pixelSize) * -1.21f) + draw->originY;

	for (int i = firstQuad; i < endQuad; i++)
		draw->quads.push_back(cache->quads[i]);

	float alpha = info.alpha;

	draw->textRed = ((float)(info.color & 0xFF) / 255.0f) * alpha;
	draw->textGreen = ((float)((info.color >> 8) & 0xFF) / 255.0f) * alpha;
	draw->textBlue = ((float)((info.color >> 16) & 0xFF) / 255.0f) * alpha;
	draw->alpha = alpha;
}

void HdText::queryText(const HdTextInfo &info, const HdTextCache *cache, const HdTextCanvas &canvas, int *x, int *y,
		int *width, int *height) {
	bool stretch = canvas.stretched && !canvas.boxingHD;
	float scaleX = (float)kCanvasWidth / (float)canvas.width;
	float scaleY = (float)kCanvasHeight / 1080.0f;
	float uniform = MIN(scaleY, scaleX);

	if (!stretch) {
		scaleX = uniform;
		scaleY = uniform;
	}

	*x = (int)floorf(((float)(int)((float)info.x * scaleX) + cache->minX) / scaleX);
	*y = (int)floorf((float)(int)((float)info.y * scaleY) / scaleY);

	// With the stretch the size of the text keeps the shape: both axes use the smaller scale
	if (stretch) {
		uniform = MIN(scaleX, scaleY);
		scaleX = uniform;
		scaleY = uniform;
	}

	*width = (int)ceilf(fabsf(cache->maxRight / scaleX));
	*height = (int)ceilf(fabsf(cache->maxBottom / scaleY));
}

} // End of namespace Grim
