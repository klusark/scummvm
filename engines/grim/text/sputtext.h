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

#ifndef GRIM_SPUTTEXT_H
#define GRIM_SPUTTEXT_H

#include "common/scummsys.h"

#include "engines/grim/text/blasttext.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/std/std_sys.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class GrimEngine;
class HdTextCache;
class LegacyImporter;
class Lua_V1;
class NormalText;
class Polyglot;
class RenderDroid;
class Res;
class SputActorModule;
class SputRender;
class SputRoom;
class SputTime;
class Text;
struct HdTextInfo;
struct SputActor;

enum {
	// The remastered texts can be up to 8191 characters long, the others keep 511
	kTextEventCapacity = 8192,
	kTextEventClassicCapacity = 512
};

struct SputTextEvent {
	SputTextEvent *next;
	SputTextEvent *prev;
	char text[kTextEventCapacity];
	TextParams params;
	int x;
	int y;
	StdRect clip;
	int duration;
	SputActor *actor;
	int flags;
	int serial;
	// Remastered only: the lines to show and the layout of the HD text
	int firstLine;
	int lineCount;
	HdTextCache *hdCache;
};

class SputText {
public:
	SputText(Res *res, RenderDroid *rd, NormalText *normalText, Text *text, BlastText *blastText, SputTime *sputTime, Polyglot *polyglot);

	void setSputRender(SputRender *sputRender) { _sputRender = sputRender; }
	void setSputRoom(SputRoom *sputRoom) { _sputRoom = sputRoom; }
	void setSputActor(SputActorModule *sputActor) { _sputActor = sputActor; }
	void setGrimEngine(GrimEngine *grim) { _grim = grim; }
	void setLua(Lua_V1 *lua) { _lua = lua; }

	void killEvent(SputTextEvent *event);
	void update(int force);
	void update(int force, int page);
	int initialize(SysStruct *hs);
	void pause();
	void resume();
	SputTextEvent *createActorText(SputActor *actor, const char *text, int x, int y);
	SputTextEvent *createActorTextDemo(SputActor *actor, const char *text);
	SputTextEvent *addToQueue(SputTextEvent *event, int duration, int flags);
	int release();
	int startup();
	int shutdown();
	SputTextEvent *allocEvent(const TextParams *params, const char *text);
	const char *extractSayLineData(const char *msgId, char *translation);
	void drawTextEvent(SputTextEvent *event, int markDirty);
	SputTextEvent *findSerialNumber(int serial);
	int getTextObjectParams(TextParams *params, lua_Object tableObj, SputTextEvent *event);
	int getCoordsFromTable(lua_Object tableObj);
	void getHdClip(StdRect *clip, int coords);
	void queueBlastEvent(SputTextEvent *event);
	int getTextCapacity() const;
	bool queryTextObject(SputTextEvent *event, int *x, int *y, int *width, int *height);
	int getHdLineCount(SputTextEvent *event);
	int getHdLineCharacterCount(SputTextEvent *event, int line);
	void invalidateHdText(SputTextEvent *event);
	void luaGetFontDimensions();
	void parseSayLineTable(lua_Object tableObj, int *background, int *volume, int *pan, int *x, int *y);
	TextParams *getDefaultTextParams() { return &_defaultTextParams; }
	int save(Common::WriteStream *stream);
	int restore(Common::ReadStream *stream);
	int saveRemastered(Common::WriteStream *stream);
	int restoreRemastered(Common::ReadStream *stream);

private:
	friend class Lua_V1;
	friend class LegacyImporter;

	Res *_res;
	RenderDroid *_rd;
	NormalText *_normalText;
	Text *_text;
	BlastText *_blastText;
	SputTime *_sputTime;
	Polyglot *_polyglot;
	SputRender *_sputRender;
	SputRoom *_sputRoom;
	SputActorModule *_sputActor = nullptr;
	GrimEngine *_grim;
	Lua_V1 *_lua = nullptr;

	void initHdEvent(SputTextEvent *event);
	void dropHdBitmap(HdTextCache *cache);
	void updateHd(int force, int page);
	void drawHdEvent(SputTextEvent *event, int page);
	HdTextCache *getHdLayout(SputTextEvent *event);
	bool makeHdInfo(SputTextEvent *event, HdTextInfo *info);

	SysStruct *_hs;
	int _initialized;
	int _started;
	int _locked;

	SputTextEvent *_textQueue;
	BlastTextFont *_font;
	BlastTextFont *_emergencyFont;
	int _translationMode;

	int _centerValue;
	int _offscreenTextPos;
	int _textPosX;
	int _textPosY;

	StdRect _printLineClip;
	TextParams _printLineParams;
	TextParams _defaultTextParams;
	TextParams _blastTextParams;

	int _luaTextObjXRef;
	int _luaTextObjYRef;
	int _luaTextObjFontRef;
	int _luaTextObjWidthRef;
	int _luaTextObjHeightRef;
	int _luaTextObjFGColorRef;
	int _luaTextObjBGColorRef;
	int _luaTextObjFXColorRef;
	int _luaTextObjHIColorRef;
	int _luaTextObjDurationRef;
	int _luaTextObjCenterRef;
	int _luaTextObjLJustifyRef;
	int _luaTextObjRJustifyRef;
	int _luaTextObjVolumeRef;
	int _luaTextObjBufferRef;
	int _luaTextObjBackgroundRef;
	int _luaTextObjCoordsRef;
	int _luaTextObjLeftClipRef;
	int _luaTextObjAlphaRef;
	int _luaTextObjLayerRef;
	int _luaTextObjFadeRef;
	int _luaTextObjHighlightRef;
};

} // End of namespace Grim

#endif
