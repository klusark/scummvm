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

#ifndef GRIM_BLASTTEXT_H
#define GRIM_BLASTTEXT_H

#include "common/scummsys.h"

#include "engines/grim/std/std_display.h"

namespace Common {
class SeekableReadStream;
}

namespace Grim {

class HdFont;
class HdText;
class Text;

extern const byte systemFontData[];

struct BlastTextSurface {
	byte *pixels;
	int width;
	int height;
	int bytesPerPixel;
	int reserved16;
	int pitch;
};

struct BlastTextGlyph {
	int dataOffset;
	signed char advance;
	signed char xOffset;
	signed char yOffset;
	byte pad;
	byte *pixels;
	uint width;
	uint height;
	int reserved20;
};

struct BlastTextFont {
	int lineHeight;
	int baseLine;
	int reserved12;
	int reserved16;
	int minY;
	uint maxY;
	int firstChar;
	int lastChar;
	uint16 *charMap;
	BlastTextGlyph *glyphs;
	byte *pixels;
	const HdFont *hdFont;
};

struct TextParams {
	BlastTextSurface *surface;
	StdRect *clip;
	BlastTextFont *font;
	int x;
	int y;
	int fgColor;
	int bgColor;
	int fxColor;
	uint scaleX;
	uint scaleY;
	int flags;
	byte *colorMap;
	int coords;
	int highlight;
	float alpha;
};

class BlastText {
public:
	BlastText(Text *text);
	~BlastText();

	BlastTextSurface *getSurface() { return &_surface; }
	HdText *getHdText();

	int load(const char *fileName, BlastTextFont *&font);
	int initFont(BlastTextFont *&font);
	void initFontHelper(const byte *source, BlastTextGlyph *glyph, int bitsPerPixel, const byte *map);
	int read(Common::SeekableReadStream *file, BlastTextFont *&font);
	const char *measureString(TextParams *params, BlastTextFont *font, const char *text, int *width, int *count, int advance);
	uint charWidth(const TextParams *params, BlastTextFont *font, int c);
	uint charPositionHelper(const TextParams *params, BlastTextFont *font, byte c);
	void drawString(TextParams *params, BlastTextFont *font, const char *text);
	void drawChars(TextParams *params, BlastTextFont *font, int c);
	void drawCharsCase0(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params);
	void drawCharsCase4(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params);
	void drawCharsCase1(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params);
	void drawCharsCase6(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params);

	friend class Text;

private:
	Text *_text;
	HdText *_hdText;
	BlastTextSurface _surface;
	byte _charMap[256];
};

} // End of namespace Grim

#endif
