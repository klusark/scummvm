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

#include "common/endian.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/text/blasttext.h"
#include "engines/grim/text/hdtext.h"
#include "engines/grim/text/text.h"

namespace Grim {

static BlastTextGlyph *findGlyph(BlastTextFont *font, int c) {
	if (c > font->lastChar)
		return nullptr;

	if (c < font->firstChar) {
		warning("BlastText: character %d is below the first character of the font", c);
		return nullptr;
	}

	return &font->glyphs[font->charMap[c - font->firstChar]];
}

BlastText::BlastText(Text *text) : _text(text), _hdText(nullptr) {
	memset(&_surface, 0, sizeof(_surface));
	memset(_charMap, 0, sizeof(_charMap));
}

BlastText::~BlastText() {
	delete _hdText;
	_hdText = nullptr;
}

HdText *BlastText::getHdText() {
	if (!_hdText && g_grim->isRemastered())
		_hdText = new HdText();

	return _hdText;
}

int BlastText::load(const char *fileName, BlastTextFont *&font) {
	int result = 1;

	// Remastered draws all the text from TrueType, the font file is not read
	if (g_grim->isRemastered()) {
		BlastTextFont *emptyFont = new BlastTextFont();

		emptyFont->lastChar = -1;
		font = emptyFont;

		return 0;
	}

	if (!_text->_hs) {
		warning("BlastText::load(): the text system is not initialized");
		return result;
	}

	Common::SeekableReadStream *file = _text->_hs->openFile(fileName);

	if (file) {
		result = read(file, font);
		delete file;
		file = nullptr;
	}

	return result;
}

int BlastText::initFont(BlastTextFont *&font) {
	const byte *data = systemFontData;
	int numGlyphs = READ_LE_UINT16(data);
	const byte map[2] = { 0, 255 };

	BlastTextFont *newFont = new BlastTextFont();

	newFont->lineHeight = data[2];
	newFont->baseLine = data[2] - 1;
	newFont->reserved12 = 0;
	newFont->reserved16 = 1;
	newFont->firstChar = 0;
	newFont->lastChar = numGlyphs;

	int numPixels = 0;

	for (int i = 0; i < numGlyphs; i++) {
		uint32 offset = READ_LE_UINT32(data + 4 + 4 * i);

		if (offset)
			numPixels += data[offset] * data[offset + 1];
	}

	newFont->charMap = new uint16[numGlyphs + 1];

	for (int i = 0; i < numGlyphs; i++)
		newFont->charMap[i] = i;

	newFont->charMap[numGlyphs] = 0;
	newFont->glyphs = new BlastTextGlyph[numGlyphs]();
	newFont->pixels = new byte[numPixels]();

	int pixelOffset = 0;

	for (int i = 0; i < numGlyphs; i++) {
		uint32 offset = READ_LE_UINT32(data + 4 + 4 * i);

		if (offset) {
			BlastTextGlyph *glyph = &newFont->glyphs[i];
			signed char xOffset = data[offset + 2];

			glyph->width = data[offset];
			glyph->height = data[offset + 1];
			glyph->xOffset = xOffset;
			glyph->advance = (signed char)(xOffset + (signed char)glyph->width);
			glyph->reserved20 = 1;
			glyph->yOffset = (signed char)((signed char)data[offset + 3] - (signed char)newFont->baseLine);
			glyph->dataOffset = pixelOffset;
			glyph->pixels = newFont->pixels + pixelOffset;

			if (newFont->minY > glyph->yOffset)
				newFont->minY = glyph->yOffset;

			uint bottom = glyph->height + (uint)(int)glyph->yOffset;

			if (newFont->maxY > bottom)
				bottom = newFont->maxY;

			newFont->maxY = bottom;

			initFontHelper(data + 4 + offset, glyph, 1, map);

			pixelOffset += glyph->height * glyph->width;
		}
	}

	font = newFont;

	return 0;
}

void BlastText::initFontHelper(const byte *source, BlastTextGlyph *glyph, int bitsPerPixel, const byte *map) {
	byte *dest = glyph->pixels;
	int bitPosition = -1;
	int bits = 0;

	for (uint row = 0; row < glyph->height; row++) {
		for (uint column = 0; column < glyph->width; column++) {
			if (bitPosition < 0) {
				bits = *source++;
				bitPosition = 8 - bitsPerPixel;
			}

			*dest++ = map[(255 >> (8 - bitsPerPixel)) & (bits >> bitPosition)];
			bitPosition -= bitsPerPixel;
		}
	}
}

int BlastText::read(Common::SeekableReadStream *file, BlastTextFont *&font) {
	file->readSint32LE();
	int dataSize = file->readSint32LE();

	BlastTextFont *newFont = new BlastTextFont();

	newFont->lineHeight = file->readSint32LE();
	newFont->baseLine = file->readSint32LE();
	newFont->reserved12 = file->readSint32LE();
	newFont->reserved16 = file->readSint32LE();
	newFont->firstChar = file->readSint32LE();
	newFont->lastChar = file->readSint32LE();

	int numGlyphs = newFont->lastChar - newFont->firstChar + 1;

	if (numGlyphs < 0 || numGlyphs > 65536 || dataSize < 0 || dataSize > 0x1000000 || (int64)numGlyphs * 18 > file->size() - file->pos()) {
		warning("BlastText::read(): invalid font header");
		delete newFont;
		newFont = nullptr;

		return 1;
	}

	newFont->charMap = new uint16[numGlyphs + 1];

	for (int i = 0; i < numGlyphs; i++)
		newFont->charMap[i] = file->readUint16LE();

	newFont->charMap[numGlyphs] = 0;
	newFont->glyphs = new BlastTextGlyph[numGlyphs]();

	for (int i = 0; i < numGlyphs; i++) {
		BlastTextGlyph *glyph = &newFont->glyphs[i];

		glyph->dataOffset = file->readSint32LE();
		glyph->advance = file->readSByte();
		glyph->xOffset = file->readSByte();
		glyph->yOffset = file->readSByte();
		glyph->pad = file->readByte();
		glyph->width = file->readUint32LE();
		glyph->height = file->readUint32LE();
		glyph->reserved20 = 1;

		if (newFont->minY > glyph->yOffset)
			newFont->minY = glyph->yOffset;

		uint bottom = glyph->height + (uint)(int)glyph->yOffset;

		if (newFont->maxY > bottom)
			bottom = newFont->maxY;

		newFont->maxY = bottom;
	}

	newFont->pixels = new byte[dataSize]();
	file->read(newFont->pixels, dataSize);

	font = newFont;

	return 0;
}

const char *BlastText::measureString(TextParams *params, BlastTextFont *font, const char *text, int *width, int *count, int advance) {
	StdRect *clip = params->clip;

	if (!clip) {
		warning("BlastText::measureString(): no clip rectangle");
		return nullptr;
	}

	int flags = params->flags;
	int leftSpace = params->x - clip->x;
	int rightSpace = clip->x + clip->width - params->x;
	int available;

	if (flags & 1) {
		if (leftSpace < rightSpace)
			rightSpace = leftSpace;

		available = rightSpace * 2;
	} else if (flags & 2) {
		available = leftSpace;
	} else {
		available = rightSpace;
	}

	const char *savedText = text;
	int spaceWidth = 0;
	int spaceCount = 0;
	int wordWidth = 0;
	int wordChars = 0;
	int lineWidth = 0;
	int lineChars = 0;
	int c = _text->getNextChar(&text);

	if (c != 0) {
		for (;;) {
			if ((c == 32 || c == 254) && (flags & 4)) {
				int total = lineWidth + spaceWidth + wordWidth;

				if (total > available) {
					if (lineChars) {
						text = savedText;
					} else {
						lineWidth += spaceWidth + wordWidth;
						lineChars = spaceCount + wordChars;
					}

					break;
				}

				lineWidth = total;
				lineChars += spaceCount + wordChars;
				spaceWidth = charWidth(params, font, c);
				wordWidth = 0;
				wordChars = 0;
				spaceCount = 1;
				savedText = text;
				c = _text->getNextChar(&text);

				if (c == 0) {
					lineWidth += spaceWidth;
					lineChars += spaceCount;
					break;
				}

				while (c == 32 || c == 254) {
					spaceWidth += charWidth(params, font, c);
					spaceCount++;
					savedText = text;
					c = _text->getNextChar(&text);

					if (c == 0)
						break;
				}

				if (c == 0) {
					lineWidth += spaceWidth;
					lineChars += spaceCount;
					break;
				}
			}

			if (c == 13 || c == 10) {
				int total = lineWidth + spaceWidth + wordWidth;

				if (total > available && (flags & 4)) {
					text = savedText;
				} else {
					lineWidth = total;
					lineChars += spaceCount + wordChars;

					if (c == 13 && *text == 10)
						text++;
				}

				break;
			}

			wordWidth += charWidth(params, font, c);
			wordChars++;
			c = _text->getNextChar(&text);

			if (c == 0)
				break;
		}
	}

	const char *next;

	if (c != 0) {
		next = text;
	} else {
		spaceWidth += wordWidth;

		int total = lineWidth + spaceWidth;

		if (total > available && (flags & 4)) {
			if (lineChars) {
				next = savedText;
			} else {
				lineWidth = total;
				lineChars = spaceCount + wordChars;
				next = nullptr;
			}
		} else {
			lineWidth = total;
			lineChars += spaceCount + wordChars;
			next = nullptr;
		}
	}

	if (advance) {
		int x = params->x;

		if (next)
			params->y += (params->scaleY * (uint)font->lineHeight + 0xFFFF) >> 16;
		else if (flags & 1)
			params->x = x + lineWidth / 2;
		else if (flags & 2)
			params->x = x - lineWidth;
		else
			params->x = x + lineWidth;
	}

	if (count)
		*count = lineChars;

	if (width)
		*width = lineWidth;

	return next;
}

uint BlastText::charWidth(const TextParams *params, BlastTextFont *font, int c) {
	BlastTextGlyph *glyph = findGlyph(font, c);

	if (!glyph)
		return 0;

	uint width = (uint)(int)glyph->advance;

	if (width <= glyph->width)
		width = glyph->width;

	return (width * params->scaleX + 0xFFFF) >> 16;
}

uint BlastText::charPositionHelper(const TextParams *params, BlastTextFont *font, byte c) {
	BlastTextGlyph *glyph = findGlyph(font, c);

	if (!glyph)
		return 0;

	return (params->scaleX * (uint)(int)glyph->advance + 0xFFFF) >> 16;
}

void BlastText::drawString(TextParams *params, BlastTextFont *font, const char *text) {
	int previousChar = 0;
	int savedFlags = params->flags;

	params->flags = savedFlags & ~8;
	_charMap[0] = (byte)params->bgColor;
	_charMap[128] = (byte)params->fxColor;
	_charMap[255] = (byte)params->fgColor;

	if (text) {
		int center = savedFlags & 1;
		const char *next;

		do {
			int width;
			int count;

			next = measureString(params, font, text, &width, &count, 0);

			int x = params->x;
			int y = params->y;

			if (center)
				params->x = x - width / 2;
			else if (savedFlags & 2)
				params->x = x - width;

			for (int i = 0; i < count; i++) {
				int c = _text->getNextChar(&text);

				if (previousChar != 10 || c != 13)
					drawChars(params, font, c);

				previousChar = c;
			}

			if (next) {
				params->x = x;
				params->y = y + ((font->lineHeight * params->scaleY + 0xFFFF) >> 16);
			}

			text = next;
		} while (next);
	}

	params->flags = savedFlags;
}

void BlastText::drawChars(TextParams *params, BlastTextFont *font, int c) {
	int mode = 0;
	uint scaleX = params->scaleX;
	uint scaleY = params->scaleY;
	int penY = params->y;
	StdRect *clip = params->clip;
	int penX = params->x;

	if (!clip) {
		warning("BlastText::drawChars(): no clip rectangle");
		return;
	}

	BlastTextGlyph *glyph = findGlyph(font, c);

	if (!glyph)
		return;

	glyph->pixels = font->pixels + glyph->dataOffset;

	int x;
	int y;
	int width;
	int height;

	if (scaleX == 0x10000 && scaleY == 0x10000) {
		width = glyph->width;
		height = glyph->height;
		x = glyph->xOffset + penX;
		y = glyph->yOffset + penY;
	} else {
		x = (int)((scaleX * (uint)(int)glyph->xOffset + 0xFFFF) >> 16) + penX;
		width = (int)((scaleX * glyph->width + 0xFFFF) >> 16);
		height = (int)((scaleY * glyph->height + 0xFFFF) >> 16);
		mode = 1;
		y = (int)((scaleY * (uint)(int)glyph->yOffset + 0xFFFF) >> 16) + penY;
	}

	if (!(x >= clip->x && y >= clip->y && x + width < clip->x + clip->width && y + height < clip->y + clip->height))
		mode |= 2;

	if (params->flags & 8)
		mode |= 4;

	if (!params->surface || !params->surface->pixels) {
		warning("BlastText::drawChars(): no surface");
	} else {
		switch (mode) {
		case 0:
			drawCharsCase0(glyph, font, params);
			break;
		case 2:
			drawCharsCase1(glyph, font, params);
			break;
		case 4:
			drawCharsCase4(glyph, font, params);
			break;
		case 6:
			drawCharsCase6(glyph, font, params);
			break;
		default:
			break;
		}
	}

	params->x = (int)((scaleX * (uint)(int)glyph->advance + 0xFFFF) >> 16) + params->x;
}

void BlastText::drawCharsCase0(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params) {
	uint height = glyph->height;
	int width = glyph->width;
	const byte *source = glyph->pixels;
	BlastTextSurface *surface = params->surface;
	int bytesPerPixel = surface->bytesPerPixel;
	int pitch = surface->pitch;
	byte *rowPointer = surface->pixels + pitch * (params->y + glyph->yOffset) + bytesPerPixel * (params->x + glyph->xOffset);
	const byte *map = params->colorMap;

	if (!map)
		map = _charMap;

	const byte *foreground = map + bytesPerPixel * params->fgColor;
	const byte *effect = map + bytesPerPixel * params->fxColor;

	for (uint row = 0; row < height; row++) {
		byte *dest = rowPointer;

		if (width > 0) {
			for (int column = 0; column < width; column++) {
				int value = *source++;

				if (value == 0) {
					dest += bytesPerPixel;
				} else if (value == 128) {
					WRITE_UINT16(dest, READ_UINT16(effect));
					dest += bytesPerPixel;
				} else if (value == 255) {
					WRITE_UINT16(dest, READ_UINT16(foreground));
					dest += bytesPerPixel;
				}
			}
		}

		rowPointer += pitch;
	}
}

void BlastText::drawCharsCase4(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params) {
	int height = glyph->height;
	int width = glyph->width;
	const byte *source = glyph->pixels;
	BlastTextSurface *surface = params->surface;
	int pitch = surface->pitch;
	int topPadding = glyph->yOffset - font->minY;
	byte *dest = surface->pixels + pitch * (params->y + font->minY) + glyph->xOffset + params->x;
	const byte *map = params->colorMap;

	if (!map)
		map = _charMap;

	for (int row = 0; row < topPadding; row++) {
		byte *cell = dest;

		for (int column = 0; column < width; column++)
			*cell++ = *map;

		dest += pitch;
	}

	int bottomPadding = (int)(font->maxY - glyph->height - (uint)(int)glyph->yOffset);

	for (int row = 0; row < height; row++) {
		byte *cell = dest;

		for (int column = 0; column < width; column++)
			*cell++ = map[*source++];

		dest += pitch;
	}

	for (int row = 0; row < bottomPadding; row++) {
		byte *cell = dest;

		for (int column = 0; column < width; column++)
			*cell++ = *map;

		dest += pitch;
	}
}

void BlastText::drawCharsCase1(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params) {
	int xOffset = glyph->xOffset;
	int yOffset = glyph->yOffset;
	int glyphWidth = glyph->width;
	BlastTextSurface *surface = params->surface;
	int bytesPerPixel = surface->bytesPerPixel;
	int pitch = surface->pitch;
	StdRect *clip = params->clip;
	int startY = yOffset + params->y;
	int startX = xOffset + params->x;

	int left = startX;

	if (left <= clip->x)
		left = clip->x;

	left -= startX;

	int top = startY;

	if (top <= clip->y)
		top = clip->y;

	top -= startY;

	int rightLimit = startX + glyphWidth;

	if (rightLimit >= clip->x + clip->width)
		rightLimit = clip->x + clip->width;

	rightLimit -= startX;

	int glyphHeight = glyph->height;
	int bottomLimit = clip->y + clip->height;

	if (startY + glyphHeight < bottomLimit)
		bottomLimit = startY + glyphHeight;

	bottomLimit -= startY;

	if (left < rightLimit && top < bottomLimit) {
		const byte *sourceRow = glyph->pixels + top * glyphWidth + left;
		byte *destRow = surface->pixels + (left + startX) * bytesPerPixel + pitch * startY;
		const byte *map = params->colorMap;

		if (!map)
			map = _charMap;

		const byte *foreground = map + params->fgColor * bytesPerPixel;
		const byte *effect = map + params->fxColor * bytesPerPixel;
		int visibleWidth = rightLimit - left;
		int rows = bottomLimit - top;
		int y = startY;

		for (int row = 0; row < rows; row++) {
			if (y >= clip->y && y < clip->y + clip->height && visibleWidth > 0) {
				const byte *source = sourceRow;
				byte *dest = destRow;

				for (int column = 0; column < visibleWidth; column++) {
					int value = *source++;

					if (value == 0) {
						dest += bytesPerPixel;
					} else if (value == 128) {
						WRITE_UINT16(dest, READ_UINT16(effect));
						dest += bytesPerPixel;
					} else if (value == 255) {
						WRITE_UINT16(dest, READ_UINT16(foreground));
						dest += bytesPerPixel;
					}
				}
			}

			destRow += pitch;
			sourceRow += glyphWidth;
			y++;
		}
	}
}

void BlastText::drawCharsCase6(BlastTextGlyph *glyph, BlastTextFont *font, TextParams *params) {
	int glyphWidth = glyph->width;
	int glyphHeight = glyph->height;
	BlastTextSurface *surface = params->surface;
	StdRect *clip = params->clip;
	int pitch = surface->pitch;
	int startX = params->x + glyph->xOffset;
	int topPadding = glyph->yOffset - font->minY;
	int startY = font->minY + params->y;

	int left = startX;

	if (left <= clip->x)
		left = clip->x;

	left -= startX;

	int top = startY;

	if (top <= clip->y)
		top = clip->y;

	top -= startY;

	int rightLimit = startX + glyphWidth;

	if (rightLimit >= clip->x + clip->width)
		rightLimit = clip->x + clip->width;

	rightLimit -= startX;

	int bottomLimit = clip->y + clip->height;

	if (startY + glyphHeight < bottomLimit)
		bottomLimit = startY + glyphHeight;

	bottomLimit -= startY;

	if (left < rightLimit && top < bottomLimit) {
		const byte *sourceRow = glyph->pixels + left;
		byte *dest = surface->pixels + pitch * startY + left + startX;
		const byte *map = params->colorMap;

		if (!map)
			map = _charMap;

		int visibleWidth = rightLimit - left;
		int y = startY;

		for (int row = 0; row < topPadding; row++) {
			if (y >= clip->y && y < clip->y + clip->height && visibleWidth > 0) {
				byte *cell = dest;

				for (int column = 0; column < visibleWidth; column++)
					*cell++ = *map;
			}

			dest += pitch;
			y++;
		}

		int bottomPadding = (int)(font->maxY - (uint)glyphHeight - (uint)(int)glyph->yOffset);

		for (int row = 0; row < glyphHeight; row++) {
			if (y >= clip->y && y < clip->y + clip->height && visibleWidth > 0) {
				const byte *source = sourceRow;
				byte *cell = dest;

				for (int column = 0; column < visibleWidth; column++)
					*cell++ = map[*source++];
			}

			sourceRow += glyphWidth;
			dest += pitch;
			y++;
		}

		for (int row = 0; row < bottomPadding; row++) {
			if (y >= clip->y && y < clip->y + clip->height && visibleWidth > 0) {
				byte *cell = dest;

				for (int column = 0; column < visibleWidth; column++)
					*cell++ = *map;
			}

			dest += pitch;
			y++;
		}
	}
}

} // End of namespace Grim
