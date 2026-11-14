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
#include FT_TRUETYPE_TABLES_H

#include "engines/grim/remastered/hdcanvas.h"
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

int HdFont::readGlyphBox(FT_FaceRec_ *face, int locFormat, int glyph, int *xMin, int *yMin, int *xMax, int *yMax) {
	*xMin = 0;
	*yMin = 0;
	*xMax = 0;
	*yMax = 0;

	if (locFormat < 0 || locFormat > 1)
		return -1;

	if (glyph < 0 || glyph >= (int)face->num_glyphs)
		return 1;

	byte entry[8];
	FT_ULong length = (locFormat == 0) ? 4 : 8;
	FT_ULong first;
	FT_ULong second;

	if (FT_Load_Sfnt_Table(face, FT_MAKE_TAG('l', 'o', 'c', 'a'), (FT_ULong)glyph * (locFormat == 0 ? 2 : 4), entry, &length) != 0)
		return -1;

	if (locFormat == 0) {
		first = (FT_ULong)((entry[0] << 8) | entry[1]) * 2;
		second = (FT_ULong)((entry[2] << 8) | entry[3]) * 2;
	} else {
		first = ((FT_ULong)entry[0] << 24) | ((FT_ULong)entry[1] << 16) | ((FT_ULong)entry[2] << 8) | (FT_ULong)entry[3];
		second = ((FT_ULong)entry[4] << 24) | ((FT_ULong)entry[5] << 16) | ((FT_ULong)entry[6] << 8) | (FT_ULong)entry[7];
	}

	if (first == second)
		return 1;

	byte header[10];

	length = 10;

	if (FT_Load_Sfnt_Table(face, FT_MAKE_TAG('g', 'l', 'y', 'f'), first, header, &length) != 0)
		return -1;

	*xMin = (int16)((header[2] << 8) | header[3]);
	*yMin = (int16)((header[4] << 8) | header[5]);
	*xMax = (int16)((header[6] << 8) | header[7]);
	*yMax = (int16)((header[8] << 8) | header[9]);

	return 0;
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

	TT_HoriHeader *hhea = (TT_HoriHeader *)FT_Get_Sfnt_Table(face, FT_SFNT_HHEA);
	TT_Header *head = (TT_Header *)FT_Get_Sfnt_Table(face, FT_SFNT_HEAD);
	int locFormat = head ? (int)head->Index_To_Loc_Format : -1;
	long cell;

	if (hhea)
		cell = (long)hhea->Ascender - (long)hhea->Descender;
	else
		cell = (long)face->ascender - (long)face->descender;

	if (cell <= 0) {
		warning("HdFont: '%s' has no usable metrics", _path.c_str());
		FT_Done_Face(face);
		return;
	}

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

		int boxXMin;
		int boxYMin;
		int boxXMax;
		int boxYMax;

		if (readGlyphBox(face, locFormat, (int)index, &boxXMin, &boxYMin, &boxXMax, &boxYMax) < 0) {
			FT_BBox box;

			FT_Outline_Get_CBox(&slot->outline, &box);

			boxXMin = (int)box.xMin;
			boxYMin = (int)box.yMin;
			boxXMax = (int)box.xMax;
			boxYMax = (int)box.yMax;
		}

		FT_Outline_Transform(&slot->outline, &matrix);

		float scaledXMin = (float)((double)boxXMin * (double)scale);
		float scaledYMin = (float)((double)boxYMin * (double)scale);
		float scaledXMax = (float)((double)boxXMax * (double)scale);
		float scaledYMax = (float)((double)boxYMax * (double)scale);
		int left = (int)floor((double)scaledXMin);
		int bottom = (int)floor((double)scaledYMin);
		int right = (int)ceil((double)scaledXMax);
		int top = (int)ceil((double)scaledYMax);
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

	for (const byte *p = (const byte *)info.text; *p; p++) {
		const HdFont::Glyph *glyph = font->getGlyph(*p);
		HdTextQuad quad;

		// The sums are done in double and stored to float once
		quad.x = floorf((float)(((double)glyph->xOffset + (double)penX) + 0.5)) - 2.0f;
		quad.y = (float)(baseLine - 2) + floorf((float)((double)glyph->yOffset + 0.5));
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

	cache->maxRight = maxRight;
	cache->maxBottom = maxBottom;
}

void HdText::align(HdTextCache *cache, float alignX, float alignY) const {
	int numRows = (int)cache->rows.size();
	float height = cache->pixelSize;

	int firstLines = numRows > 1 ? 1 : numRows;
	int otherLines = numRows - 1 > 0 ? numRows - 1 : 0;
	float firstHeight = (float)((double)firstLines * (double)height);

	// 1.2f widened to double
	float blockHeight = (float)(((double)height * 1.2000000476837158) * (double)otherLines + (double)firstHeight);
	float shiftY = ceilf((float)((double)(-alignY) * (double)blockHeight));
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
	float scaleX;
	float scaleY;
	float offsetX;
	float offsetY;

	HdCanvas::computeScaling(stretch, canvas.width, 1080, &scaleX, &scaleY, &offsetX, &offsetY);

	double shiftWidth = (double)(uint32)(kCanvasWidth - canvas.width);

	offsetX = (float)((double)offsetX - (shiftWidth * (double)scaleX) * 0.5);

	draw->originX = (float)((double)info.x * (double)scaleX + (double)offsetX);
	draw->originY = (float)((double)info.y * (double)scaleY + (double)offsetY);

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

	int firstQuad = cache->rows[firstRow].firstQuad;
	int endQuad = (int)cache->quads.size();

	if (lastRow < numRows)
		endQuad = cache->rows[lastRow].firstQuad + cache->rows[lastRow].numQuads;

	// 1.21f widened to double
	if (firstRow != 0)
		draw->originY = (float)((double)draw->originY - ((double)cache->pixelSize * (double)firstRow) * 1.2100000381469727);

	for (int i = firstQuad; i < endQuad; i++)
		draw->quads.push_back(cache->quads[i]);

	float alpha = info.alpha;

	draw->textRed = (float)(((double)(info.color & 0xFF) / 255.0) * (double)alpha);
	draw->textGreen = (float)(((double)((info.color >> 8) & 0xFF) / 255.0) * (double)alpha);
	draw->textBlue = (float)(((double)((info.color >> 16) & 0xFF) / 255.0) * (double)alpha);
	draw->alpha = alpha;
}

void HdText::queryText(const HdTextInfo &info, const HdTextCache *cache, const HdTextCanvas &canvas, int *x, int *y,
		int *width, int *height) {
	bool stretch = canvas.stretched && !canvas.boxingHD;
	float scaleX = (float)((double)(uint32)kCanvasWidth / (double)(uint32)canvas.width);
	float scaleY = (float)((double)(uint32)kCanvasHeight / 1080.0);
	float uniform = scaleX > scaleY ? scaleY : scaleX;

	if (!stretch) {
		scaleX = uniform;
		scaleY = uniform;
	}

	int textX = (int)((double)info.x * (double)scaleX);
	int textY = (int)((double)info.y * (double)scaleY);
	float sumX = (float)((double)cache->minX + (double)textX);

	*x = (int)floorf((float)((double)sumX / (double)scaleX));
	*y = (int)floorf((float)((double)textY / (double)scaleY));

	if (stretch) {
		uniform = scaleY > scaleX ? scaleX : scaleY;
		scaleX = uniform;
		scaleY = uniform;
	}

	*width = (int)ceilf(fabsf(cache->maxRight / scaleX));
	*height = (int)ceilf(fabsf(cache->maxBottom / scaleY));
}

} // End of namespace Grim
