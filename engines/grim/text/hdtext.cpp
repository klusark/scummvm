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

#include "common/scummsys.h"

#include "common/archive.h"
#include "common/path.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "graphics/fonts/ttf_glyphs.h"

#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/text/blasttext.h"
#include "engines/grim/text/hdtext.h"

namespace Grim {

HdFont::HdFont(const Common::String &path, float size, int minPixels) :
		_path(path), _requestedSize(size), _pixelSize(0.0f), _loaded(false) {
	memset(_glyphs, 0, sizeof(_glyphs));
	memset(&_emptyGlyph, 0, sizeof(_emptyGlyph));

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
		loadGlyphs(data, dataSize, minPixels);
	else
		warning("HdFont: cannot read the font file '%s'", path.c_str());

	delete[] data;
}

HdFont::~HdFont() {
	for (int i = 0; i < kNumGlyphs; i++)
		delete[] _glyphs[i].alpha;
}

void HdFont::loadGlyphs(const byte *data, long dataSize, int minPixels) {
	Graphics::TTFGlyphs face;

	if (!face.load(data, (uint32)dataSize)) {
		warning("HdFont: '%s' is not a valid font file", _path.c_str());
		return;
	}

	long cell = face.getCellHeight();

	if (cell <= 0) {
		warning("HdFont: '%s' has no usable metrics", _path.c_str());
		return;
	}

	float pixels = ceilf(_requestedSize * 8.0f) / 8.0f;

	if (pixels < (float)minPixels)
		pixels = (float)minPixels;

	_pixelSize = pixels;

	float scale = pixels / (float)cell;

	for (int c = 0; c < kNumGlyphs; c++) {
		Glyph *glyph = &_glyphs[c];
		Graphics::TTFGlyphs::GlyphInfo info;

		if (!face.loadGlyph((uint32)c, &info))
			continue;

		glyph->advance = (float)info.advance * scale;

		if (pixels <= 18.0f)
			glyph->advance += 1.0f;

		if (!info.hasOutline)
			continue;

		float scaledXMin = (float)((double)info.xMin * (double)scale);
		float scaledYMin = (float)((double)info.yMin * (double)scale);
		float scaledXMax = (float)((double)info.xMax * (double)scale);
		float scaledYMax = (float)((double)info.yMax * (double)scale);
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

		face.renderGlyph(scale, left, bottom, width, height, glyph->alpha);
	}

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

HdText::HdText() : _customFont(nullptr), _customFontTried(false) {
}

HdText::~HdText() {
	for (uint i = 0; i < _fonts.size(); i++)
		delete _fonts[i];

	_fonts.clear();
}

const HdFont *HdText::loadFont(const Common::String &path, float size, int minPixels) {
	for (uint i = 0; i < _fonts.size(); i++) {
		if (_fonts[i]->getRequestedSize() == size && _fonts[i]->getPath().equalsIgnoreCase(path))
			return _fonts[i];
	}

	HdFont *font = new HdFont(path, size, minPixels);

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
