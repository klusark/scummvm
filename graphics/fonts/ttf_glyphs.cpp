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

// Since FreeType2 includes files, which contain forbidden symbols, we need to
// allow all symbols here.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"
#ifdef USE_FREETYPE2

#include "graphics/fonts/ttf_glyphs.h"

#include "common/textconsole.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_IMAGE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

namespace Graphics {

namespace {

struct SpanTarget {
	byte *pixels;
	int width;
	int height;
};

void spanCallback(int y, int count, const FT_Span *spans, void *user) {
	SpanTarget *target = static_cast<SpanTarget *>(user);

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

int readGlyphBox(FT_Face face, int locFormat, int glyph, int *xMin, int *yMin, int *xMax, int *yMax) {
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

} // End of anonymous namespace

struct TTFGlyphs::Impl {
	Impl() : library(nullptr), face(nullptr), locFormat(-1) {}

	FT_Library library;
	FT_Face face;
	int locFormat;
};

TTFGlyphs::TTFGlyphs() : _impl(new Impl()) {
}

TTFGlyphs::~TTFGlyphs() {
	if (_impl->face)
		FT_Done_Face(_impl->face);

	if (_impl->library)
		FT_Done_FreeType(_impl->library);

	delete _impl;
}

bool TTFGlyphs::load(const byte *data, uint32 size) {
	if (_impl->library || _impl->face)
		return false;

	FT_Library library = nullptr;

	if (FT_Init_FreeType(&library) != 0) {
		warning("TTFGlyphs::load(): cannot initialize FreeType");
		return false;
	}

	_impl->library = library;

	FT_Face face = nullptr;

	if (FT_New_Memory_Face(library, (const FT_Byte *)data, (FT_Long)size, 0, &face) != 0)
		return false;

	_impl->face = face;

	TT_Header *head = (TT_Header *)FT_Get_Sfnt_Table(face, FT_SFNT_HEAD);

	_impl->locFormat = head ? (int)head->Index_To_Loc_Format : -1;

	return true;
}

int TTFGlyphs::getCellHeight() const {
	FT_Face face = _impl->face;

	if (!face)
		return 0;

	TT_HoriHeader *hhea = (TT_HoriHeader *)FT_Get_Sfnt_Table(face, FT_SFNT_HHEA);

	if (hhea)
		return (int)((long)hhea->Ascender - (long)hhea->Descender);

	return (int)((long)face->ascender - (long)face->descender);
}

bool TTFGlyphs::loadGlyph(uint32 chr, GlyphInfo *info) {
	info->advance = 0;
	info->hasOutline = false;
	info->xMin = 0;
	info->yMin = 0;
	info->xMax = 0;
	info->yMax = 0;

	FT_Face face = _impl->face;

	if (!face)
		return false;

	FT_UInt index = FT_Get_Char_Index(face, (FT_ULong)chr);

	if (FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0)
		return false;

	FT_GlyphSlot slot = face->glyph;

	info->advance = (int)slot->metrics.horiAdvance;

	if (slot->format != FT_GLYPH_FORMAT_OUTLINE || slot->outline.n_points == 0)
		return true;

	info->hasOutline = true;

	if (readGlyphBox(face, _impl->locFormat, (int)index, &info->xMin, &info->yMin, &info->xMax, &info->yMax) < 0) {
		FT_BBox box;

		FT_Outline_Get_CBox(&slot->outline, &box);

		info->xMin = (int)box.xMin;
		info->yMin = (int)box.yMin;
		info->xMax = (int)box.xMax;
		info->yMax = (int)box.yMax;
	}

	return true;
}

void TTFGlyphs::renderGlyph(float scale, int left, int bottom, int width, int height, byte *pixels) {
	FT_Face face = _impl->face;

	if (!face)
		return;

	FT_GlyphSlot slot = face->glyph;
	FT_Matrix matrix;

	matrix.xx = (FT_Fixed)(scale * 64.0f * 65536.0f + 0.5f);
	matrix.xy = 0;
	matrix.yx = 0;
	matrix.yy = matrix.xx;

	FT_Outline_Transform(&slot->outline, &matrix);
	FT_Outline_Translate(&slot->outline, (FT_Pos)(-left * 64), (FT_Pos)(-bottom * 64));

	SpanTarget target;

	target.pixels = pixels;
	target.width = width;
	target.height = height;

	FT_Raster_Params params;

	memset(&params, 0, sizeof(params));
	params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
	params.gray_spans = spanCallback;
	params.user = &target;

	FT_Outline_Render(_impl->library, &slot->outline, &params);
}

} // End of namespace Graphics

#endif
