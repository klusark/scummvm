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

#ifndef GRIM_EMI_TEXTEVENT_H
#define GRIM_EMI_TEXTEVENT_H

#include "common/array.h"
#include "common/list.h"
#include "common/scummsys.h"

#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/text/cjkstring.h"
#include "engines/grim/emi/text/ctextfont.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class CDrawContext;
class EmiEngine;

class TextEvent {
public:
	struct TextRun {
		int key;	// font page for the .laf path, UTF-16 unit for the CJK glyph path
		int firstVertex;
		int vertexCount;
	};

	enum {
		kRestoreBufferSize = 512,
		kTagBufferSize = 32
	};

	explicit TextEvent(EmiEngine *engine);
	TextEvent(EmiEngine *engine, const LString &newText);
	TextEvent(EmiEngine *engine, const char *newText);
	TextEvent(EmiEngine *engine, const CjkString &newText);
	TextEvent(EmiEngine *engine, const uint16 *newText);
	~TextEvent();

	void *operator new(size_t size);
	void operator delete(void *block);

	void init();
	void setPosition(int x, int y);
	int activate(float newDuration, int newFlags);
	void freeGeometry();
	void buildGeometry();
	const char *layoutLine(const char *lineStart, int *outWidth, int *outNumChars);
	byte nextChar(const char **cursor);
	int getWidth();
	int getHeight();
	int getCharWidth(uint16 character);
	void draw(CDrawContext *drawContext);
	bool save(SaveRestoreAccess *accessFunc);
	bool restore(SaveRestoreAccess *accessFunc);
	bool setFont(const char *fontName);
	bool setFont(LResourceHandle<CTextFont> newFont);

	// CJK text path
	void setText(const char *newText);
	void setCjkText(const CjkString &newText);
	int getCjkPrefixWidth(int numChars);
	bool allocGeometry(bool shadowPass);
	void buildGeometry(bool shadowPass);
	int measureWord(const uint16 *word, int *outNumChars);
	const uint16 *layoutLine(const uint16 *lineStart, int *outWidth, int *outNumChars);
	uint16 nextChar(const uint16 **cursor);
	void drawCjk(CDrawContext *drawContext);

	static int update(EmiEngine *engine, CDrawContext *drawContext, bool drawAll);
	static bool startup(EmiEngine *engine, CDrawContext *drawContext);
	static void shutdown(EmiEngine *engine);
	static TextEvent *getEvent(EmiEngine *engine, int eventId);
	static const char *translate(EmiEngine *engine, const char *msg, char *buffer);
	static CjkString translateCjk(EmiEngine *engine, const char *msg, char *buffer);
	static CjkString concatCjk(const CjkString &first, const CjkString &second);
	static void holdAll(EmiEngine *engine);
	static void releaseAll(EmiEngine *engine);
	static bool saveAll(EmiEngine *engine, SaveRestoreAccess *accessFunc);
	static bool restoreAll(EmiEngine *engine, SaveRestoreAccess *accessFunc);

	LString _text;
	CjkString _cjkText;
	float _duration;
	uint _flags;
	int _actorId;
	int _clipRect[4];
	LResourceHandle<CTextFont> _font;
	int _x;
	int _y;
	int _layer;
	int _hold;
	Color _color;
	Color _outlineColor;
	float _scaleX;
	float _scaleY;
	byte _justify;
	bool _geometryReady;
	int _id;
	Common::Array<TextRun> _runs;
	int _vertexCount;
	int _writeIndex;
	float *_positions;
	float *_normals;
	byte *_colors;
	float *_uvs;

private:
	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
