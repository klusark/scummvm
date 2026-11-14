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

#ifndef GRAPHICS_FONTS_TTF_GLYPHS_H
#define GRAPHICS_FONTS_TTF_GLYPHS_H

#include "common/scummsys.h"

#ifdef USE_FREETYPE2

#include "common/noncopyable.h"

namespace Graphics {

/**
 * Unscaled access to the glyphs of a TrueType font held in memory.
 */
class TTFGlyphs : public Common::NonCopyable {
public:
	/**
	 * Description of a loaded glyph, all values are in font units.
	 */
	struct GlyphInfo {
		int advance;
		bool hasOutline;
		int xMin, yMin, xMax, yMax;
	};

	TTFGlyphs();
	~TTFGlyphs();

	/** Opens the first face of the font data, which must stay valid until the object is destroyed. */
	bool load(const byte *data, uint32 size);

	/** Returns the hhea ascender minus descender, or the face values when there is no hhea table. */
	int getCellHeight() const;

	/** Loads the glyph of a character code, the box is the glyf header one when the font has it. */
	bool loadGlyph(uint32 chr, GlyphInfo *info);

	/** Renders the last loaded glyph scaled to 8-bit coverage, row 0 of the zeroed buffer being the top. */
	void renderGlyph(float scale, int left, int bottom, int width, int height, byte *pixels);

private:
	struct Impl;

	Impl *_impl;
};

} // End of namespace Graphics

#endif

#endif
