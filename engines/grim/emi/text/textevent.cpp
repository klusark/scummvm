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

#include "common/textconsole.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/text/polyglot_v2.h"
#include "engines/grim/emi/core/profilertimer.h"
#include "engines/grim/emi/text/textevent.h"

namespace Grim {

static const int kQuadsFlags = 0x20004;

static Glyph *findGlyph(CTextFont *textFont, int character) {
	if (!textFont || !textFont->_charIndex || !textFont->_glyphs)
		return nullptr;

	if (character > textFont->_lastChar)
		return nullptr;

	if (character < textFont->_firstChar) {
		warning("TextEvent: character %d is below the first character of the font", character);

		return nullptr;
	}

	int index = textFont->_charIndex[character - textFont->_firstChar];

	if (index >= textFont->_numGlyphs) {
		warning("TextEvent: glyph index %d is out of range", index);

		return nullptr;
	}

	return &textFont->_glyphs[index];
}

TextEvent::TextEvent(EmiEngine *engine) : _font(engine), _engine(engine) {
	_clipRect[0] = 0;
	_clipRect[1] = 0;
	_clipRect[2] = 0;
	_clipRect[3] = 0;

	init();
}

TextEvent::TextEvent(EmiEngine *engine, const LString &newText) : _font(engine), _engine(engine) {
	_clipRect[0] = 0;
	_clipRect[1] = 0;
	_clipRect[2] = 0;
	_clipRect[3] = 0;

	init();

	if (_engine->isCjkMode())
		_cjkText = CjkString::fromNarrow(newText.c_str());
	else
		_text = newText;

	_geometryReady = false;
}

TextEvent::TextEvent(EmiEngine *engine, const char *newText) : _font(engine), _engine(engine) {
	_clipRect[0] = 0;
	_clipRect[1] = 0;
	_clipRect[2] = 0;
	_clipRect[3] = 0;

	init();

	if (!newText)
		warning("TextEvent::TextEvent(): no text");
	else if (_engine->isCjkMode())
		_cjkText = CjkString::fromNarrow(newText);
	else
		_text = newText;

	_geometryReady = false;
}

TextEvent::TextEvent(EmiEngine *engine, const CjkString &newText) : _font(engine), _engine(engine) {
	_clipRect[0] = 0;
	_clipRect[1] = 0;
	_clipRect[2] = 0;
	_clipRect[3] = 0;

	init();

	_cjkText = newText;
	_geometryReady = false;
}

TextEvent::TextEvent(EmiEngine *engine, const uint16 *newText) : _font(engine), _engine(engine) {
	_clipRect[0] = 0;
	_clipRect[1] = 0;
	_clipRect[2] = 0;
	_clipRect[3] = 0;

	init();

	if (newText)
		_cjkText.assign(newText);
	else
		warning("TextEvent::TextEvent(): no text");

	_geometryReady = false;
}

TextEvent::~TextEvent() {
	Common::List<TextEvent *>::iterator it = _engine->_textEvents.begin();

	while (it != _engine->_textEvents.end()) {
		if (*it == this)
			it = _engine->_textEvents.erase(it);
		else
			++it;
	}

	freeGeometry();
}

void *TextEvent::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void TextEvent::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

void TextEvent::init() {
	// The disasm leaves flags and layer uninitialized
	_flags = 0;
	_layer = 0;
	_actorId = 0;
	_hold = 0;
	_id = _engine->_nextTextEventId;
	_engine->_nextTextEventId++;

	TextEvent *defaultEvent = _engine->_defaultTextEvent;

	if (defaultEvent) {
		_clipRect[0] = defaultEvent->_clipRect[0];
		_clipRect[1] = defaultEvent->_clipRect[1];
		_clipRect[2] = defaultEvent->_clipRect[2];
		_clipRect[3] = defaultEvent->_clipRect[3];
		if (_engine->getGamePlatform() == Common::kPlatformPS2) {
			setPosition(defaultEvent->_x, defaultEvent->_y);
			setFont("arial14bos.laf");
		} else {
			_x = defaultEvent->_x;
			_y = defaultEvent->_y;
			_geometryReady = false;

			LResourceHandle<CTextFont> defaultFont(_engine);

			defaultFont.setResource(defaultEvent->_font._resource);
			setFont(defaultFont);
		}

		_color = defaultEvent->_color;
		_geometryReady = false;
		_outlineColor = defaultEvent->_outlineColor;
		_geometryReady = false;
		_justify = defaultEvent->_justify;
		_geometryReady = false;

		double durationMs = defaultEvent->_duration;
		int truncatedMs = INT_MIN;

		if (durationMs > -2147483648.0 && durationMs < 2147483648.0)
			truncatedMs = (int)durationMs;
		else
			warning("TextEvent::init(): the default duration is out of range");

		if (_engine->getGamePlatform() == Common::kPlatformPS2)
			_duration = (float)truncatedMs / 1000.0f;
		else
			_duration = (float)((double)truncatedMs * (double)0.001f);
	} else if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		_clipRect[0] = 32;
		_clipRect[1] = 32;
		_clipRect[2] = 576;
		_clipRect[3] = 404;
		setPosition(32, 32);
		setFont("arial14bos.laf");
		_color = Color::kWhite;
		_geometryReady = false;
		_outlineColor = Color::kWhite;
		_justify = 4;
		_geometryReady = false;
		_duration = (float)-0.001;
	} else {
		if (_engine->isCjkMode()) {
			_clipRect[0] = 2;
			_clipRect[1] = 0;
			_clipRect[2] = _engine->_textScreenRect[2] - 4;
			_clipRect[3] = _engine->_textScreenRect[3];
		} else {
			_clipRect[0] = 0;
			_clipRect[1] = 0;
			_clipRect[2] = _engine->_textScreenRect[2];
			_clipRect[3] = _engine->_textScreenRect[3];
		}

		_x = 0;
		_y = 0;
		_geometryReady = false;
		_font.setResource(nullptr);
		_color = Color::kWhite;
		_geometryReady = false;
		_outlineColor = Color();
		_justify = 4;
		_geometryReady = false;
		_duration = (float)-0.001;
	}

	_text = "";
	_cjkText.clear();
	_geometryReady = false;
	_scaleX = 1.0f;
	_scaleY = 1.0f;
	_geometryReady = false;
	_vertexCount = 0;
	_writeIndex = 0;
	_positions = nullptr;
	_normals = nullptr;
	_colors = nullptr;
	_uvs = nullptr;
}

void TextEvent::setPosition(int x, int y) {
	_x = x;

	if (_x < _clipRect[0])
		_x = _clipRect[0];

	if (_clipRect[0] + _clipRect[2] < _x)
		_x = _clipRect[0] + _clipRect[2];

	_y = y;

	if (_y < _clipRect[1])
		_y = _clipRect[1];

	if (_clipRect[1] + _clipRect[3] < _y)
		_y = _clipRect[1] + _clipRect[3];

	_geometryReady = false;
}

int TextEvent::activate(float newDuration, int newFlags) {
	_duration = newDuration;
	_flags = newFlags;
	_engine->_textEvents.push_back(this);

	return _engine->_textEvents.size();
}

void TextEvent::freeGeometry() {
	_vertexCount = 0;

	if (_positions)
		_engine->getMemoryServices()->freeImpl(_positions);

	_positions = nullptr;

	if (_normals)
		_engine->getMemoryServices()->freeImpl(_normals);

	_normals = nullptr;

	if (_colors)
		_engine->getMemoryServices()->freeImpl(_colors);

	_colors = nullptr;

	if (_uvs)
		_engine->getMemoryServices()->freeImpl(_uvs);

	_uvs = nullptr;
	_runs.clear();
}

void TextEvent::buildGeometry() {
	freeGeometry();

	uint length = _text.size();
	int numVertices = 0;
	int lastPage = -1;

	_vertexCount = 4 * length;

	_positions = (float *)_engine->getMemoryServices()->mallocImpl(_vertexCount * 3 * sizeof(float));
	_colors = (byte *)_engine->getMemoryServices()->mallocImpl(_vertexCount * 4 * sizeof(byte));
	_uvs = (float *)_engine->getMemoryServices()->mallocImpl(_vertexCount * 2 * sizeof(float));

	// The disasm never checks, an empty text gets no memory (size 0) and builds nothing
	if (_vertexCount != 0 && (!_positions || !_colors || !_uvs)) {
		warning("TextEvent::buildGeometry(): out of memory");

		return;
	}

	if (_vertexCount != 0) {
		memset(_positions, 0, _vertexCount * 3 * sizeof(float));
		memset(_colors, 0, _vertexCount * 4 * sizeof(byte));
		memset(_uvs, 0, _vertexCount * 2 * sizeof(float));
	}

	const char *cursor = _text.c_str();
	CTextFont *lockedFont = nullptr;

	if (_font.lock())
		lockedFont = _font._typed;

	if (!lockedFont) {
		warning("TextEvent::buildGeometry(): no font");

		return;
	}

	int lineHeight = (int)((double)lockedFont->_lineHeight * (double)_scaleY);
	int cursorX = _x;
	int cursorY = _y;
	int previousChar = 0;
	const char *lineStart = cursor;

	while (lineStart) {
		int lineWidth;
		int lineChars;
		const char *nextLine = layoutLine(lineStart, &lineWidth, &lineChars);
		int justification = _justify & 3;

		if (justification == 2)
			cursorX += -(lineWidth / 2);
		else if (justification == 1)
			cursorX -= lineWidth;

		for (int i = 0; i < lineChars; i++) {
			int character = nextChar(&cursor);
			int skippedChar = previousChar;

			previousChar = character;

			if ((skippedChar == 10 && character == 13) || !character)
				continue;

			CTextFont *glyphFont = nullptr;

			if (_font.lock())
				glyphFont = _font._typed;

			Glyph *glyph = findGlyph(glyphFont, character);

			_font.unlock();

			if (!glyph) {
				warning("TextEvent::buildGeometry(): missing glyph for character %d", character);

				continue;
			}

			if (character != 32) {
				if (glyph->page != lastPage || _runs.empty()) {
					TextRun run;

					run.key = glyph->page;
					run.firstVertex = numVertices;
					run.vertexCount = 0;
					_runs.push_back(run);
					lastPage = glyph->page;
				}

				float left = (float)((double)glyph->offsetX * (double)_scaleX + (double)cursorX);
				float top = (float)((double)glyph->offsetY * (double)_scaleY + (double)cursorY);
				float right = (float)((double)(uint)glyph->alpha.getWidth() * (double)_scaleX + (double)left);
				float bottom = (float)((double)(uint)glyph->alpha.getHeight() * (double)_scaleY + (double)top);

				if (left < right && top < bottom) {
					float halfWidth = (float)(uint)(_engine->_textScreenRect[2] / 2);
					float halfHeight = (float)(uint)(_engine->_textScreenRect[3] / 2);

					if (halfWidth == 0.0f || halfHeight == 0.0f) {
						// The original could divide by zero here, let's avoid that...
						warning("TextEvent::buildGeometry(): the screen size is zero");

						continue;
					}

					if (numVertices + 4 > _vertexCount) {
						warning("TextEvent::buildGeometry(): too many glyphs");

						continue;
					}

					double normalLeft = ((double)left - (double)halfWidth) / (double)halfWidth;
					float normalTop = (float)(((double)top - (double)halfHeight) / (double)halfHeight);
					double normalRight = ((double)right - (double)halfWidth) / (double)halfWidth;
					float normalBottom = (float)(((double)bottom - (double)halfHeight) / (double)halfHeight);
					float *vertex = _positions + numVertices * 3;

					vertex[0] = (float)normalLeft;
					vertex[1] = -normalTop;
					vertex[2] = 1.0f;
					vertex[3] = (float)normalRight;
					vertex[4] = -normalTop;
					vertex[5] = 1.0f;
					vertex[6] = (float)normalRight;
					vertex[7] = -normalBottom;
					vertex[8] = 1.0f;
					vertex[9] = (float)normalLeft;
					vertex[10] = -normalBottom;
					vertex[11] = 1.0f;

					byte red = (uint)_color._r <= 0xFF ? (byte)_color._r : 0xFF;
					byte green = (uint)_color._g <= 0xFF ? (byte)_color._g : 0xFF;
					byte blue = (uint)_color._b <= 0xFF ? (byte)_color._b : 0xFF;
					byte alpha = (uint)_color._a <= 0xFF ? (byte)_color._a : 0xFF;

					for (int corner = 0; corner < 4; corner++) {
						byte *vertexColor = _colors + (numVertices + corner) * 4;

						vertexColor[2] = red;
						vertexColor[1] = green;
						vertexColor[0] = blue;
						vertexColor[3] = alpha;
					}

					float *vertexUvs = _uvs + numVertices * 2;

					vertexUvs[0] = glyph->u0;
					vertexUvs[1] = glyph->v0;
					vertexUvs[2] = glyph->u1;
					vertexUvs[3] = glyph->v0;
					vertexUvs[4] = glyph->u1;
					vertexUvs[5] = glyph->v1;
					vertexUvs[6] = glyph->u0;
					vertexUvs[7] = glyph->v1;

					_runs[_runs.size() - 1].vertexCount += 4;
					numVertices += 4;
				}
			}

			float scaledAdvance = (float)((double)glyph->advance * (double)_scaleX);

			cursorX += (int)lrint((double)scaledAdvance);
		}

		if (nextLine) {
			cursorX = _x;
			cursorY += lineHeight;
		}

		lineStart = nextLine;
		cursor = nextLine;
	}

	_geometryReady = true;
	_font.unlock();
}

const char *TextEvent::layoutLine(const char *lineStart, int *outWidth, int *outNumChars) {
	int clipLeft = _clipRect[0];
	int clipRight = _clipRect[2];
	int justification = _justify & 3;
	int leftSpace = _x - clipLeft;
	int rightSpace = clipLeft + (clipRight - _x);
	int maxWidth;

	if (justification == 2) {
		if (leftSpace < rightSpace)
			rightSpace = leftSpace;

		maxWidth = rightSpace * 2;
	} else if (justification == 1) {
		maxWidth = leftSpace;
	} else {
		maxWidth = rightSpace;
	}

	const char *cursor = lineStart;
	const char *wrapPoint = lineStart;
	const char *result;
	int numSpaces = 0;
	int pendingWidth = 0;
	int wordWidth = 0;
	int totalWidth = 0;
	int wordChars = 0;
	int totalChars = 0;
	int character = nextChar(&cursor);

	if (character) {
		while (true) {
			if ((character == 32 || character == 254) && (_justify & 4)) {
				if (wordWidth + pendingWidth + totalWidth > maxWidth) {
					if (totalChars) {
						result = wrapPoint;
					} else {
						totalWidth += wordWidth + pendingWidth;
						totalChars = numSpaces + wordChars;
						result = cursor;
					}

					if (outNumChars)
						*outNumChars = totalChars;

					if (outWidth)
						*outWidth = totalWidth;

					return result;
				}

				totalWidth += wordWidth + pendingWidth;
				totalChars += wordChars + numSpaces;
				pendingWidth = getCharWidth(character);
				wordChars = 0;
				numSpaces = 1;
				wordWidth = 0;
				wrapPoint = cursor;
				character = nextChar(&cursor);

				if (!character)
					break;

				while (character == 32 || character == 254) {
					int spaceWidth = getCharWidth(character);

					wrapPoint = cursor;
					pendingWidth += spaceWidth;
					numSpaces++;
					character = nextChar(&cursor);

					if (!character)
						break;
				}

				if (!character)
					break;
			}

			if (character == 13 || character == 10) {
				if (totalWidth + wordWidth + pendingWidth > maxWidth && (_justify & 4) && totalChars) {
					result = wrapPoint;
				} else {
					totalWidth += wordWidth + pendingWidth;
					totalChars += numSpaces + wordChars;
					result = cursor;

					if (character == 13 && *cursor == 10)
						result = cursor + 1;
				}

				if (outNumChars)
					*outNumChars = totalChars;

				if (outWidth)
					*outWidth = totalWidth;

				return result;
			}

			wordWidth += getCharWidth(character);
			wordChars++;
			character = nextChar(&cursor);

			if (!character)
				break;
		}
	}

	int lineWidth = wordWidth + pendingWidth;

	if (lineWidth + totalWidth > maxWidth && (_justify & 4) && totalChars) {
		result = wrapPoint;
	} else {
		totalWidth += lineWidth;
		result = nullptr;
		totalChars += wordChars + numSpaces;
	}

	if (outNumChars)
		*outNumChars = totalChars;

	if (outWidth)
		*outWidth = totalWidth;

	return result;
}

byte TextEvent::nextChar(const char **cursor) {
	const char *current = *cursor;
	byte character = (byte)*current;

	*cursor = current + 1;

	return character;
}

int TextEvent::getWidth() {
	if (_engine->isCjkMode()) {
		int cjkWidth = 0;
		int cjkLineWidth = 0;
		const uint16 *cjkCursor = _cjkText.c_str();
		CTextFont *cjkFont = nullptr;

		if (_font.lock())
			cjkFont = _font._typed;

		if (cjkFont) {
			do {
				cjkCursor = layoutLine(cjkCursor, &cjkLineWidth, nullptr);

				if (cjkWidth <= cjkLineWidth)
					cjkWidth = cjkLineWidth;
			} while (cjkCursor);

			_font.unlock();
		}

		return cjkWidth;
	}

	int width = 0;
	int lineWidth = 0;
	const char *cursor = _text.c_str();
	CTextFont *lockedFont = nullptr;

	if (_font.lock())
		lockedFont = _font._typed;

	if (lockedFont) {
		do {
			cursor = layoutLine(cursor, &lineWidth, nullptr);

			if (width <= lineWidth)
				width = lineWidth;
		} while (cursor);

		_font.unlock();
	}

	return width;
}

int TextEvent::getHeight() {
	if (_engine->isCjkMode()) {
		int cjkHeight = 0;
		CTextFont *cjkFont = nullptr;

		if (_font.lock())
			cjkFont = _font._typed;

		if (!cjkFont) {
			warning("TextEvent::getHeight(): no font");

			return 0;
		}

		int cjkLineHeight = (int)((double)cjkFont->_lineHeight * (double)_scaleY);
		const uint16 *cjkCursor = _cjkText.c_str();

		do {
			cjkCursor = layoutLine(cjkCursor, nullptr, nullptr);
			cjkHeight += cjkLineHeight;
		} while (cjkCursor);

		_font.unlock();

		return cjkHeight;
	}

	int height = 0;
	CTextFont *lockedFont = nullptr;

	if (_font.lock())
		lockedFont = _font._typed;

	if (!lockedFont) {
		warning("TextEvent::getHeight(): no font");

		return 0;
	}

	int lineHeight = (int)((double)lockedFont->_lineHeight * (double)_scaleY);
	const char *cursor = _text.c_str();

	do {
		cursor = layoutLine(cursor, nullptr, nullptr);
		height += lineHeight;
	} while (cursor);

	_font.unlock();

	return height;
}

int TextEvent::getCharWidth(uint16 character) {
	if (_engine->isCjkMode()) {
		int cjkWidth = (int)((double)(uint16)_engine->_glyphAdvance * (double)_scaleX);

		if (character == 32 || character == 254)
			cjkWidth >>= 1;

		return cjkWidth;
	}

	int width = 0;
	CTextFont *lockedFont = nullptr;

	if (_font.lock())
		lockedFont = _font._typed;

	if (lockedFont) {
		Glyph *glyph = findGlyph(lockedFont, character);

		if (glyph)
			width = (int)((double)glyph->advance * (double)_scaleX);

		_font.unlock();
	} else {
		warning("TextEvent::getCharWidth(): no font");
	}

	return width;
}

void TextEvent::draw(CDrawContext *drawContext) {
	if (_engine->isCjkMode()) {
		drawCjk(drawContext);

		return;
	}

	CTextFont *lockedFont = nullptr;

	if (_font.lock())
		lockedFont = _font._typed;

	if (!lockedFont) {
		warning("TextEvent::draw(): no font");

		return;
	}

	if (lockedFont->_pages.empty())
		lockedFont->buildTextures(drawContext);

	if (!_geometryReady)
		buildGeometry();

	drawContext->setOrtho(-1.0f, 1.0f, -1.0f, 1.0f);

	VertexStreams streams;

	streams.count = _vertexCount;
	streams.positions = _positions;
	streams.normals = _normals;
	streams.colors = reinterpret_cast<const uint32 *>(_colors);
	streams.uvs = _uvs;

	for (uint i = 0; i < _runs.size(); i++) {
		CTexture *texture = lockedFont->getTexture(drawContext, _runs[i].key);

		drawContext->drawQuads(kQuadsFlags, texture, _runs[i].firstVertex, _runs[i].vertexCount, &streams);
	}

	_font.unlock();
}

void TextEvent::setText(const char *newText) {
	if (!newText) {
		warning("TextEvent::setText(): no text");

		return;
	}

	if (_engine->isCjkMode())
		_cjkText = CjkString::fromNarrow(newText);
	else
		_text = newText;

	_geometryReady = false;
}

void TextEvent::setCjkText(const CjkString &newText) {
	_cjkText = newText;
	_geometryReady = false;
}

int TextEvent::getCjkPrefixWidth(int numChars) {
	int width = 0;

	for (int i = 0; i < numChars; i++) {
		if ((uint)i >= _cjkText.size())
			break;

		// The disasm reads only the low byte of each unit here
		width += getCharWidth((byte)(_cjkText[i] & 0xFF));
	}

	return width;
}

uint16 TextEvent::nextChar(const uint16 **cursor) {
	const uint16 *current = *cursor;
	uint16 character = *current;

	*cursor = current + 1;

	return character;
}

int TextEvent::measureWord(const uint16 *word, int *outNumChars) {
	int width = 0;
	int numChars = 0;
	uint16 character = *word;

	while (character) {
		if (character == 32 || character == 254)
			break;

		width += getCharWidth(character);
		numChars++;
		word++;

		// A CJK character ends the word after itself, and a CJK character that follows ends it before itself
		if (character > 0xFF)
			break;

		character = *word;

		if (character > 0xFF)
			break;
	}

	if (outNumChars)
		*outNumChars = numChars;

	return width;
}

const uint16 *TextEvent::layoutLine(const uint16 *lineStart, int *outWidth, int *outNumChars) {
	int clipLeft = _clipRect[0];
	int clipRight = _clipRect[2];
	int justification = _justify & 3;
	int leftSpace = _x - clipLeft;
	int rightSpace = clipLeft + (clipRight - _x);
	int maxWidth;

	if (justification == 2) {
		if (leftSpace < rightSpace)
			rightSpace = leftSpace;

		maxWidth = rightSpace * 2;
	} else if (justification == 1) {
		maxWidth = leftSpace;
	} else {
		maxWidth = rightSpace;
	}

	const uint16 *cursor = lineStart;
	int totalWidth = 0;
	int gapWidth = 0;
	int totalChars = 0;
	bool reachedEnd = false;

	if (*cursor == 0) {
		reachedEnd = true;
	} else {
		while (true) {
			int wordChars;
			int wordWidth = measureWord(cursor, &wordChars);

			// The disasm has no "at least one word per line" check
			if ((_justify & 4) && totalWidth + wordWidth + gapWidth > maxWidth)
				break;

			totalWidth += gapWidth + wordWidth;
			totalChars += wordChars;
			cursor += wordChars;
			gapWidth = 0;

			while (*cursor == 32 || *cursor == 254) {
				gapWidth += getCharWidth(*cursor);
				cursor++;
				totalChars++;
			}

			if (*cursor == 0) {
				reachedEnd = true;

				break;
			}
		}
	}

	if (reachedEnd)
		cursor = nullptr;

	if (outNumChars)
		*outNumChars = totalChars;

	if (outWidth)
		*outWidth = totalWidth;

	if (totalChars)
		return cursor;

	// Nothing fitted, so one character is skipped
	return cursor ? cursor + 1 : nullptr;
}

bool TextEvent::allocGeometry(bool shadowPass) {
	freeGeometry();

	uint length = _cjkText.size();

	_vertexCount = (shadowPass ? 8 : 4) * length;
	_writeIndex = 0;

	_positions = (float *)_engine->getMemoryServices()->mallocImpl(_vertexCount * 3 * sizeof(float));
	_colors = (byte *)_engine->getMemoryServices()->mallocImpl(_vertexCount * 4 * sizeof(byte));
	_uvs = (float *)_engine->getMemoryServices()->mallocImpl(_vertexCount * 2 * sizeof(float));

	// The disasm never checks, an empty text gets no memory (size 0) and builds nothing
	if (_vertexCount != 0 && (!_positions || !_colors || !_uvs)) {
		warning("TextEvent::allocGeometry(): out of memory");

		return false;
	}

	if (_vertexCount != 0) {
		memset(_positions, 0, _vertexCount * 3 * sizeof(float));
		memset(_colors, 0, _vertexCount * 4 * sizeof(byte));
		memset(_uvs, 0, _vertexCount * 2 * sizeof(float));
	}

	return true;
}

void TextEvent::buildGeometry(bool shadowPass) {
	const uint16 *cursor = _cjkText.c_str();
	int cursorX = _x;
	int cursorY = _y;
	int previousChar = 0;
	Color color = _color;

	if (shadowPass) {
		color = Color();
		cursorX = _x + 1;
		cursorY = _y + 1;
	}

	while (true) {
		int lineWidth;
		int lineChars;
		const uint16 *nextLine = layoutLine(cursor, &lineWidth, &lineChars);
		int justification = _justify & 3;

		if (justification == 2)
			cursorX += lineWidth / -2;
		else if (justification == 1)
			cursorX -= lineWidth;

		for (int i = 0; i < lineChars; i++) {
			uint16 character = nextChar(&cursor);
			int skippedChar = previousChar;

			previousChar = character;

			if ((skippedChar == 10 && character == 13) || !character)
				continue;

			if (character != 32) {
				// The run comes first, a glyph with no texture is skipped when drawing
				TextRun run;

				run.key = character;
				run.firstVertex = _writeIndex;
				run.vertexCount = 0;
				_runs.push_back(run);

				float left = (float)cursorX;
				double cellHeight = (double)_scaleY * 16.0;
				float top = (float)((double)cursorY - cellHeight);
				float right = (float)((double)_scaleX * 16.0 + (double)left);
				float bottom = (float)(cellHeight + (double)top);

				if (left < right && top < bottom) {
					float halfWidth = (float)(uint)(_engine->_textScreenRect[2] / 2);
					float halfHeight = (float)(uint)(_engine->_textScreenRect[3] / 2);

					if (halfWidth == 0.0f || halfHeight == 0.0f) {
						// The original could divide by zero here, let's avoid that...
						warning("TextEvent::buildGeometry(): the screen size is zero");
					} else if (_writeIndex + 4 > _vertexCount) {
						warning("TextEvent::buildGeometry(): too many glyphs");
					} else {
						double normalLeft = ((double)left - (double)halfWidth) / (double)halfWidth;
						float normalTop = (float)(((double)top - (double)halfHeight) / (double)halfHeight);
						double normalRight = ((double)right - (double)halfWidth) / (double)halfWidth;
						float normalBottom = (float)(((double)bottom - (double)halfHeight) / (double)halfHeight);
						float *vertex = _positions + _writeIndex * 3;

						vertex[0] = (float)normalLeft;
						vertex[1] = -normalTop;
						vertex[2] = 1.0f;
						vertex[3] = (float)normalRight;
						vertex[4] = -normalTop;
						vertex[5] = 1.0f;
						vertex[6] = (float)normalRight;
						vertex[7] = -normalBottom;
						vertex[8] = 1.0f;
						vertex[9] = (float)normalLeft;
						vertex[10] = -normalBottom;
						vertex[11] = 1.0f;

						byte red = (uint)color._r <= 0xFF ? (byte)color._r : 0xFF;
						byte green = (uint)color._g <= 0xFF ? (byte)color._g : 0xFF;
						byte blue = (uint)color._b <= 0xFF ? (byte)color._b : 0xFF;
						byte alpha = (uint)color._a <= 0xFF ? (byte)color._a : 0xFF;

						for (int corner = 0; corner < 4; corner++) {
							byte *vertexColor = _colors + (_writeIndex + corner) * 4;

							vertexColor[2] = red;
							vertexColor[1] = green;
							vertexColor[0] = blue;
							vertexColor[3] = alpha;
						}

						float *vertexUvs = _uvs + _writeIndex * 2;

						vertexUvs[0] = 0.0f;
						vertexUvs[1] = 0.0f;
						vertexUvs[2] = 1.0f;
						vertexUvs[3] = 0.0f;
						vertexUvs[4] = 1.0f;
						vertexUvs[5] = 1.0f;
						vertexUvs[6] = 0.0f;
						vertexUvs[7] = 1.0f;

						_runs[_runs.size() - 1].vertexCount += 4;
						_writeIndex += 4;
					}
				}
			}

			// Spaces and glyphs with no texture advance too
			cursorX += getCharWidth(character);
		}

		if (nextLine) {
			// The shadow offset of x applies to the first line only, y keeps it
			cursorX = _x;
			cursorY += 16;
		}

		cursor = nextLine;

		if (!nextLine)
			break;
	}

	_geometryReady = true;
}

void TextEvent::drawCjk(CDrawContext *drawContext) {
	if (!_geometryReady) {
		bool shadow = (_color._r + _color._g + _color._b > 100) && _engine->getMode() != EmiEngine::kModeOverworld;

		if (Actor::actorDeref(_engine, _actorId) && shadow && _color._a == Color::kBlue._a && _color._r == Color::kBlue._r &&
				_color._g == Color::kBlue._g && _color._b == Color::kBlue._b) {
			_color._g = 75;
			_color._r = 75;
		}

		if (!allocGeometry(shadow))
			return;

		if (shadow)
			buildGeometry(true);

		buildGeometry(false);
	}

	drawContext->setOrtho(-1.0f, 1.0f, -1.0f, 1.0f);

	VertexStreams streams;

	streams.count = _vertexCount;
	streams.positions = _positions;
	streams.normals = _normals;
	streams.colors = reinterpret_cast<const uint32 *>(_colors);
	streams.uvs = _uvs;

	for (uint i = 0; i < _runs.size(); i++) {
		CTexture *texture = nullptr;

		if (_engine->_glyphMap.tryGetVal((uint16)_runs[i].key, texture) && texture)
			drawContext->drawQuads(kQuadsFlags, texture, _runs[i].firstVertex, _runs[i].vertexCount, &streams);
	}
}

bool TextEvent::save(SaveRestoreAccess *accessFunc) {
	int length;

	if (_engine->isCjkMode()) {
		length = (int)_cjkText.size() + 1;

		accessFunc(&length, sizeof(length));
		accessFunc((void *)_cjkText.c_str(), 2 * length);
	} else {
		length = (int)_text.size() + 1;

		accessFunc(&length, sizeof(length));
		accessFunc((void *)_text.c_str(), length);
	}

	accessFunc(&_x, sizeof(_x));
	accessFunc(&_y, sizeof(_y));
	accessFunc(&_duration, sizeof(_duration));
	accessFunc(&_actorId, sizeof(_actorId));
	accessFunc(&_flags, sizeof(_flags));
	accessFunc(&_id, sizeof(_id));

	for (int i = 0; i < 4; i++)
		accessFunc(&_clipRect[i], sizeof(int));

	LString fontName;

	if (_font._resource)
		fontName = _font._resource->_name;
	else
		warning("TextEvent::save(): no font");

	length = (int)fontName.size() + 1;

	accessFunc(&length, sizeof(length));
	accessFunc((void *)fontName.c_str(), length);
	accessFunc(&_layer, sizeof(_layer));
	accessFunc(&_hold, sizeof(_hold));
	accessFunc(&_color, sizeof(_color));
	accessFunc(&_outlineColor, sizeof(_outlineColor));
	accessFunc(&_scaleX, sizeof(_scaleX));
	accessFunc(&_scaleY, sizeof(_scaleY));
	accessFunc(&_justify, sizeof(_justify));

	return true;
}

bool TextEvent::restore(SaveRestoreAccess *accessFunc) {
	char buffer[kRestoreBufferSize + 1];
	int length;

	accessFunc(&length, sizeof(length));

	if (length < 1 || length > kRestoreBufferSize) {
		warning("TextEvent::restore(): invalid text length %d", length);

		return false;
	}

	if (_engine->isCjkMode()) {
		// The length counts UTF-16 units here
		uint16 cjkBuffer[kRestoreBufferSize + 1];

		accessFunc(cjkBuffer, 2 * length);
		cjkBuffer[length] = 0;
		_cjkText.assign(cjkBuffer);
	} else {
		accessFunc(buffer, length);
		buffer[length] = 0;
		_text = buffer;
	}

	accessFunc(&_x, sizeof(_x));
	accessFunc(&_y, sizeof(_y));
	accessFunc(&_duration, sizeof(_duration));
	accessFunc(&_actorId, sizeof(_actorId));
	accessFunc(&_flags, sizeof(_flags));
	accessFunc(&_id, sizeof(_id));

	for (int i = 0; i < 4; i++)
		accessFunc(&_clipRect[i], sizeof(int));

	accessFunc(&length, sizeof(length));

	if (length < 1 || length > kRestoreBufferSize) {
		warning("TextEvent::restore(): invalid font name length %d", length);

		return false;
	}

	accessFunc(buffer, length);
	buffer[length] = 0;

	LString fontName = buffer;

	if (fontName.empty()) {
		_font.setResource(nullptr);
	} else {
		LResourceHandle<CTextFont> restoredFont(_engine, fontName);

		_font.setResource(restoredFont._resource);
	}

	accessFunc(&_layer, sizeof(_layer));
	accessFunc(&_hold, sizeof(_hold));
	accessFunc(&_color, sizeof(_color));
	accessFunc(&_outlineColor, sizeof(_outlineColor));
	accessFunc(&_scaleX, sizeof(_scaleX));
	accessFunc(&_scaleY, sizeof(_scaleY));
	accessFunc(&_justify, sizeof(_justify));
	_geometryReady = false;

	return true;
}

bool TextEvent::setFont(const char *fontName) {
	if (!fontName) {
		warning("TextEvent::setFont(): no font name");

		return false;
	}

	LString fontLabel = fontName;
	LResourceHandle<CTextFont> newFont(_engine, fontLabel);

	if (newFont.lock() && newFont._typed) {
		newFont.unlock();
		_font.setResource(nullptr);
		_font.setResource(newFont._resource);
		_geometryReady = false;

		return true;
	}

	return false;
}

bool TextEvent::setFont(LResourceHandle<CTextFont> newFont) {
	if (!newFont._resource)
		return false;

	if (!newFont.lock())
		return false;

	if (!newFont._typed)
		return false;

	newFont.unlock();
	_font.setResource(nullptr);
	_font.setResource(newFont._resource);
	_geometryReady = false;

	return true;
}

int TextEvent::update(EmiEngine *engine, CDrawContext *drawContext, bool drawAll) {
	if (engine->isDumpingFrames())
		return 0;

	engine->_textUpdateTimer.start();

	Common::List<TextEvent *>::iterator it = engine->_textEvents.begin();

	while (it != engine->_textEvents.end()) {
		TextEvent *event = *it;

		++it;

		if (engine->isDemo() || !event->_hold) {
			if (event->_duration > 0.0f || (event->_flags & 1)) {
				uint eventFlags = event->_flags;

				if (((eventFlags & 8) || drawAll) && !(eventFlags & 2))
					event->draw(drawContext);

				event->_duration = (float)((double)event->_duration - (double)engine->getFrameTime());
			} else {
				delete event;
			}
		}
	}

	int result = drawContext->flushDrawLists();

	engine->_textUpdateTimer.stop();

	return result;
}

bool TextEvent::startup(EmiEngine *engine, CDrawContext *drawContext) {
	if (engine->_textEventsStarted)
		return false;

	int viewport[4];
	int *rect = drawContext->getViewportRect(viewport);

	engine->_textScreenRect[0] = rect[0];
	engine->_textScreenRect[1] = rect[1];
	engine->_textScreenRect[2] = rect[2];
	engine->_textScreenRect[3] = rect[3];

	engine->_textEvents.clear();
	engine->_defaultTextEvent = new TextEvent(engine);
	engine->_defaultTextEvent->_flags = 0;
	engine->_textEventsStarted = true;

	return true;
}

void TextEvent::shutdown(EmiEngine *engine) {
	Common::List<TextEvent *>::iterator it = engine->_textEvents.begin();

	while (it != engine->_textEvents.end()) {
		TextEvent *event = *it;

		++it;

		if (event)
			delete event;
	}

	engine->_textEvents.clear();

	if (engine->_defaultTextEvent) {
		delete engine->_defaultTextEvent;
		engine->_defaultTextEvent = nullptr;
	}

	engine->_textEventsStarted = false;
}

TextEvent *TextEvent::getEvent(EmiEngine *engine, int eventId) {
	Common::List<TextEvent *>::iterator it;

	for (it = engine->_textEvents.begin(); it != engine->_textEvents.end(); ++it) {
		if ((*it)->_id == eventId)
			return *it;
	}

	return nullptr;
}

const char *TextEvent::translate(EmiEngine *engine, const char *msg, char *buffer) {
	const char *tagEnd = nullptr;
	const char *translated;

	if (buffer)
		*buffer = 0;

	Polyglot_v2 *polyglot = engine->getPolyglot();

	if (polyglot) {
		translated = polyglot->getTranslation(msg);
	} else {
		warning("TextEvent::translate(): no translation table");

		translated = msg;
	}

	if (buffer && msg && *msg == '/') {
		tagEnd = strchr(msg + 1, '/');

		if (tagEnd) {
			int tagLength = (int)(tagEnd - msg) - 1;

			if (tagLength >= kTagBufferSize) {
				warning("TextEvent::translate(): the tag is too long");

				tagLength = kTagBufferSize - 1;
			}

			memcpy(buffer, msg + 1, tagLength);
			buffer[tagLength] = 0;
		}
	}

	if (engine->_textMode == 1) {
		if (tagEnd)
			return tagEnd + 1;

		warning("TextEvent::translate(): the text has no tag");

		return translated;
	}

	if (engine->_textMode != 2)
		return translated;

	return msg;
}

CjkString TextEvent::translateCjk(EmiEngine *engine, const char *msg, char *buffer) {
	CjkString result;

	if (buffer)
		*buffer = 0;

	Polyglot_v2 *polyglot = engine->getPolyglot();

	// The translation mode is applied inside the lookup, and the table text is copied right away
	if (polyglot) {
		const uint16 *translated = polyglot->getCjkTranslation(msg, engine->_textMode);

		if (translated)
			result.assign(translated);
	} else {
		warning("TextEvent::translateCjk(): no translation table");

		if (msg)
			result = CjkString::fromNarrow(msg);
	}

	if (buffer && msg && *msg == '/') {
		const char *tagEnd = strchr(msg + 1, '/');

		if (tagEnd) {
			int tagLength = (int)(tagEnd - msg) - 1;

			if (tagLength >= kTagBufferSize) {
				warning("TextEvent::translateCjk(): the tag is too long");

				tagLength = kTagBufferSize - 1;
			}

			memcpy(buffer, msg + 1, tagLength);
			buffer[tagLength] = 0;
		}
	}

	return result;
}

CjkString TextEvent::concatCjk(const CjkString &first, const CjkString &second) {
	Common::Array<uint16> units;

	for (uint i = 0; i < first.size(); i++)
		units.push_back(first[i]);

	for (uint i = 0; i < second.size(); i++)
		units.push_back(second[i]);

	return CjkString(units.data(), units.size());
}

void TextEvent::holdAll(EmiEngine *engine) {
	Common::List<TextEvent *>::iterator it;

	for (it = engine->_textEvents.begin(); it != engine->_textEvents.end(); ++it)
		(*it)->_hold++;
}

void TextEvent::releaseAll(EmiEngine *engine) {
	Common::List<TextEvent *>::iterator it;

	for (it = engine->_textEvents.begin(); it != engine->_textEvents.end(); ++it) {
		TextEvent *event = *it;

		event->_hold--;

		if (event->_hold < 0)
			event->_hold = 0;
	}
}

bool TextEvent::saveAll(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	uint count = engine->_textEvents.size();

	accessFunc(&count, sizeof(count));

	Common::List<TextEvent *>::iterator it;

	for (it = engine->_textEvents.begin(); it != engine->_textEvents.end(); ++it)
		(*it)->save(accessFunc);

	accessFunc(&engine->_nextTextEventId, sizeof(engine->_nextTextEventId));
	accessFunc(&engine->_textMode, sizeof(engine->_textMode));

	if (engine->_defaultTextEvent)
		engine->_defaultTextEvent->save(accessFunc);
	else
		warning("TextEvent::saveAll(): no default event");

	return true;
}

bool TextEvent::restoreAll(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	Common::List<TextEvent *>::iterator it = engine->_textEvents.begin();

	while (it != engine->_textEvents.end()) {
		TextEvent *event = *it;

		++it;

		if (event)
			delete event;
	}

	engine->_textEvents.clear();

	int count;

	accessFunc(&count, sizeof(count));

	for (int i = 0; i < count; i++) {
		TextEvent *event = new TextEvent(engine);

		if (!event->restore(accessFunc)) {
			delete event;

			return false;
		}

		engine->_textEvents.push_back(event);
	}

	accessFunc(&engine->_nextTextEventId, sizeof(engine->_nextTextEventId));
	accessFunc(&engine->_textMode, sizeof(engine->_textMode));

	if (engine->_defaultTextEvent)
		engine->_defaultTextEvent->restore(accessFunc);
	else
		warning("TextEvent::restoreAll(): no default event");

	return true;
}

} // End of namespace Grim
