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
#include "common/list.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/text/ctextfont.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/text/textevent.h"

namespace Grim {

static const int kColorTag = MKTAG('R', 'L', 'O', 'C');
static const int kTextTag = MKTAG('T', 'X', 'E', 'T');
static const int kLocalizedBufferSize = 640;
static const float kHalf = 0.5f;
static const float kPs2ScreenWidth = 640.0f;
static const float kPs2ScreenHeight = 448.0f;
static const float kPs2HalfScreenWidth = 320.0f;
static const float kPs2HalfScreenHeight = 224.0f;
static const float kPs2DefaultOffscreenY = 0.875f;
static const float kMillisecondsToSeconds = 0.001f;

int Lua_V2::truncateToInt(double value) {
	if (!(value > -9223372036854775809.0 && value < 9223372036854775808.0)) {
		warning("Lua_V2::truncateToInt(): value out of range");

		return 0;
	}

	int64 result = (int64)value;

	if (result < -2147483648LL || result > 2147483647LL)
		warning("Lua_V2::truncateToInt(): value out of range");

	return (int)result;
}

void Lua_V2::applyTextTable(lua_Object table, TextEvent *event) {
	if (!event)
		return;

	int screenWidth = _engine->_textScreenRect[2];
	int screenHeight = _engine->_textScreenRect[3];
	int x = event->_x;
	int y = event->_y;
	bool xChanged = false;
	bool yChanged = false;
	lua_Object valueObj;

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldX]));

	valueObj = lua_gettable();

	if (valueObj && lua_isnumber(valueObj)) {
		x = truncateToInt((double)lua_getnumber(valueObj) * (double)screenWidth * (double)kHalf + (double)(screenWidth / 2));
		xChanged = true;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldY]));

	valueObj = lua_gettable();

	if (valueObj && lua_isnumber(valueObj)) {
		float halfHeight = (float)(screenHeight / 2);

		y = truncateToInt((double)halfHeight - (double)lua_getnumber(valueObj) * (double)screenHeight * (double)kHalf);
		yChanged = true;
	}

	if (xChanged || yChanged) {
		event->_x = x;
		event->_y = y;
		event->_geometryReady = false;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldFont]));

	valueObj = lua_gettable();

	if (valueObj && lua_isstring(valueObj)) {
		LString fontName = lua_getstring(valueObj);
		LResourceHandle<CTextFont> fontHandle(_engine, fontName);
		bool locked = fontHandle.lock();
		LResourceHandle<CTextFont> lockedFont;

		if (locked)
			lockedFont.setResource(fontHandle._typed);
		else
			lockedFont.setResource(nullptr);

		event->setFont(lockedFont);
		fontHandle.unlock();
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldWidth]));

	valueObj = lua_gettable();

	if (valueObj && lua_isnumber(valueObj))
		event->_clipRect[2] = truncateToInt((double)lua_getnumber(valueObj) * (double)screenWidth);

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldHeight]));

	valueObj = lua_gettable();

	if (valueObj && lua_isnumber(valueObj))
		event->_clipRect[3] = truncateToInt((double)lua_getnumber(valueObj) * (double)screenHeight);

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldFgcolor]));

	valueObj = lua_gettable();

	if (valueObj && lua_isuserdata(valueObj) && lua_tag(valueObj) == kColorTag) {
		event->_color = Color((uint)lua_getuserdata(valueObj));
		event->_geometryReady = false;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldBgcolor]));

	valueObj = lua_gettable();

	if (valueObj && lua_isuserdata(valueObj) && lua_tag(valueObj) == kColorTag) {
		event->_outlineColor = Color((uint)lua_getuserdata(valueObj));
		event->_geometryReady = false;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldCenter]));

	valueObj = lua_gettable();

	if (valueObj && !lua_isnil(valueObj)) {
		event->_justify = 6;
		event->_geometryReady = false;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldLjustify]));

	valueObj = lua_gettable();

	if (valueObj && !lua_isnil(valueObj)) {
		event->_justify = 4;
		event->_geometryReady = false;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldRjustify]));

	valueObj = lua_gettable();

	if (valueObj && !lua_isnil(valueObj)) {
		event->_justify = 5;
		event->_geometryReady = false;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldLayer]));

	valueObj = lua_gettable();

	if (valueObj && lua_isnumber(valueObj)) {
		event->_layer = luaFtol(lua_getnumber(valueObj));
		event->_geometryReady = false;
	}

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldDuration]));

	valueObj = lua_gettable();

	if (valueObj && lua_isnumber(valueObj))
		event->_duration = (float)((double)luaFtol(lua_getnumber(valueObj)) * (double)kMillisecondsToSeconds);
}

// Only the CJK exe has this form: the elements 1, 2, ... are translated and joined
// until the first one that is not a string. Returns false if the first element is not.
bool Lua_V2::joinTextStrings(lua_Object table, CjkString *result) {
	int index = 1;

	for (;;) {
		lua_pushobject(table);
		lua_pushnumber((float)index);

		lua_Object elementObj = lua_gettable();

		if (!lua_isstring(elementObj))
			break;

		*result = TextEvent::concatCjk(*result, TextEvent::translateCjk(_engine, lua_getstring(elementObj), nullptr));
		index++;
	}

	return index > 1;
}

void Lua_V2::readSayLineTable(lua_Object table, int *volume, int *pan, int *x, int *y) {
	int screenWidth = _engine->_textScreenRect[2];
	lua_Object valueObj;

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldX]));

	valueObj = lua_gettable();

	if (lua_isnumber(valueObj) && *x)
		*x = truncateToInt((double)screenWidth * (double)lua_getnumber(valueObj) * (double)kHalf + (double)(screenWidth / 2));

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldY]));

	valueObj = lua_gettable();

	// The disasm uses the screen width for y too
	if (lua_isnumber(valueObj) && *y)
		*y = truncateToInt((double)screenWidth * (double)lua_getnumber(valueObj) * (double)kHalf + (double)(screenWidth / 2));

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldVolume]));

	valueObj = lua_gettable();

	if (lua_isnumber(valueObj) && *volume)
		*volume = luaFtol(lua_getnumber(valueObj));

	lua_pushobject(table);
	lua_pushobject(lua_getref(_fieldRefs[kFieldPan]));

	valueObj = lua_gettable();

	if (lua_isnumber(valueObj) && *pan)
		*pan = luaFtol(lua_getnumber(valueObj));
}

void Lua_V2::SetTextSpeed() {
	lua_Object speedObj = lua_getparam(1);

	if (lua_isnumber(speedObj))
		Actor::setActorTextRate(_engine, luaFtol(lua_getnumber(speedObj)));
	else
		logMessage(kConsoleDebug, "SetTextSpeed: argument must be a number.\n");
}

void Lua_V2::GetTextSpeed() {
	lua_pushnumber((float)Actor::getActorTextRate(_engine));
}

void Lua_V2::IsMessageGoing() {
	lua_Object actorObj = lua_getparam(1);

	if (actorObj && !isActor(actorObj) && !lua_isnil(actorObj)) {
		logMessage(kConsoleDebug, "IsMessageGoing: Parameter, if given, must be an actor.\n");

		return;
	}

	if (isActor(actorObj)) {
		Actor *actor = Actor::actorDeref(_engine, lua_getuserdata(actorObj));

		if (actor && actor->_extra && actor->_extra->_talking) {
			lua_pushnumber(1.0f);

			return;
		}
	} else {
		if (Actor::getNumActorsTalking(_engine)) {
			lua_pushnumber(1.0f);

			return;
		}

		Common::List<TextEvent *>::iterator it;

		for (it = _engine->_textEvents.begin(); it != _engine->_textEvents.end(); ++it) {
			if (!((*it)->_flags & 1)) {
				lua_pushnumber(1.0f);

				return;
			}
		}
	}

	lua_pushnil();
}

void Lua_V2::SetSayLineDefaults() {
	lua_Object tableObj = lua_getparam(1);

	if (tableObj && lua_istable(tableObj))
		applyTextTable(tableObj, _engine->_defaultTextEvent);
	else
		logMessage(kConsoleDebug, "Parameter to SetSayLineDefaults must be a format table.\n");
}

void Lua_V2::SetActorTalkColor() {
	lua_Object actorObj = lua_getparam(1);
	lua_Object colorObj = lua_getparam(2);

	if (!isActor(actorObj)) {
		logMessage(kConsoleDebug, "SetActorTalkColor: First arg must be an actor!\n");

		return;
	}

	Actor *actor = Actor::actorDeref(_engine, lua_getuserdata(actorObj));

	if (!actor) {
		logMessage(kConsoleError, "SetActorTalkColor: Actor is invalid.\n");

		return;
	}

	if (!actor->_extra)
		actor->allocateActorExtra();

	if (lua_isuserdata(colorObj) && lua_tag(colorObj) == kColorTag)
		actor->_extra->_talkColor = Color((uint)lua_getuserdata(colorObj));
	else
		logMessage(kConsoleDebug, "SetActorTalkColor: Actor must be followed by color!\n");
}

void Lua_V2::GetActorTalkColor() {
	lua_Object actorObj = lua_getparam(1);

	if (!isActor(actorObj)) {
		logMessage(kConsoleDebug, "GetActorTalkColor: First arg must be an actor!\n");
		lua_pushnil();

		return;
	}

	Actor *actor = Actor::actorDeref(_engine, lua_getuserdata(actorObj));

	if (!actor) {
		logMessage(kConsoleError, "GetActorTalkColor: Actor is invalid.\n");

		return;
	}

	if (!actor->_extra) {
		lua_pushnil();

		return;
	}

	const Color &talkColor = actor->_extra->_talkColor;
	uint packed = (uint)talkColor._a;

	packed = (packed << 8) | (uint)talkColor._r;
	packed = (packed << 8) | (uint)talkColor._g;
	packed = (packed << 8) | (uint)talkColor._b;

	lua_pushusertag((int32)packed, kColorTag);
}

void Lua_V2::SayLine() {
	const char *text = nullptr;
	int volume = 100;
	int pan = 64;
	int x = -1;
	int y = -1;
	int index = 2;
	lua_Object lineObj = lua_getparam(1);

	if (!isActor(lineObj) && !lua_isstring(lineObj) && !lua_istable(lineObj)) {
		logMessage(kConsoleDebug, "Invalid argument to SayLine!\n");

		return;
	}

	Actor *actor = Actor::getSelectedActor(_engine);

	if (isActor(lineObj)) {
		actor = Actor::actorDeref(_engine, lua_getuserdata(lineObj));
		index = 3;
		lineObj = lua_getparam(2);
	}

	if (!actor) {
		logMessage(kConsoleDebug, "SayLine: Invalid actor!\n");

		return;
	}

	if (lua_isnil(lineObj))
		return;

	while (lua_isstring(lineObj) || lua_isnumber(lineObj) || lua_istable(lineObj)) {
		if (lua_istable(lineObj))
			readSayLineTable(lineObj, &volume, &pan, &x, &y);
		else
			text = lua_getstring(lineObj);

		lineObj = lua_getparam(index);
		index++;

		if (lua_isnil(lineObj)) {
			if (text)
				actor->sayLine(text, volume, pan, x, y);

			return;
		}
	}

	logMessage(kConsoleDebug, "SayLine: invalid parameter %d.\n", index);
}

void Lua_V2::MakeTextObject() {
	lua_Object textObj = lua_getparam(1);
	char tag[TextEvent::kTagBufferSize];
	TextEvent *event;

	if (_engine->isCjkMode()) {
		if (lua_isstring(textObj)) {
			event = new TextEvent(_engine, TextEvent::translateCjk(_engine, lua_getstring(textObj), nullptr));
		} else if (lua_istable(textObj)) {
			CjkString joined;

			joinTextStrings(textObj, &joined);
			event = new TextEvent(_engine, joined);
		} else {
			return;
		}
	} else {
		if (!lua_isstring(textObj)) {
			logMessage(kConsoleDebug, "MakeTextObject: Invalid parameter.\n");

			return;
		}

		const char *translated = TextEvent::translate(_engine, lua_getstring(textObj), tag);

		event = new TextEvent(_engine, LString(translated));
	}

	lua_Object tableObj = lua_getparam(2);

	if (tableObj && lua_istable(tableObj))
		applyTextTable(tableObj, event);

	lua_pushusertag(event->_id, kTextTag);
	lua_pushnumber((float)event->getWidth());
	lua_pushnumber((float)event->getHeight());

	event->activate(0.0f, 1);
}

void Lua_V2::KillTextObject() {
	lua_Object textObj = lua_getparam(1);

	if (textObj && lua_isuserdata(textObj) && lua_tag(textObj) == kTextTag) {
		TextEvent *event = TextEvent::getEvent(_engine, lua_getuserdata(textObj));

		if (event)
			event->_flags &= ~1u;
	}
}

void Lua_V2::ChangeTextObject() {
	lua_Object textObj = lua_getparam(1);
	int index = 2;

	if (!lua_isuserdata(textObj) || lua_tag(textObj) != kTextTag) {
		logMessage(kConsoleDebug, "First arg to ChangeTextObject should be a text object!\n");

		return;
	}

	TextEvent *event = TextEvent::getEvent(_engine, lua_getuserdata(textObj));

	if (!event) {
		logMessage(kConsoleError, "ChangeTextObject: text object is invalid");

		return;
	}

	for (int i = 0; i < 2; i++) {
		lua_Object paramObj = lua_getparam(index);

		index++;

		if (!paramObj)
			break;

		if (lua_isstring(paramObj)) {
			if (_engine->isCjkMode()) {
				event->setCjkText(TextEvent::translateCjk(_engine, lua_getstring(paramObj), nullptr));
			} else {
				event->_text = TextEvent::translate(_engine, lua_getstring(paramObj), nullptr);
				event->_geometryReady = false;
			}
		} else {
			if (!lua_istable(paramObj))
				break;

			CjkString joined;

			// A table that starts with a string replaces the text with all its strings
			if (_engine->isCjkMode() && joinTextStrings(paramObj, &joined))
				event->setCjkText(joined);
			else
				applyTextTable(paramObj, event);
		}
	}

	lua_pushnumber((float)event->getWidth());
	lua_pushnumber((float)event->getHeight());
}

void Lua_V2::GetTextObjectDimensions() {
	lua_Object textObj = lua_getparam(1);

	if (!lua_isuserdata(textObj) || lua_tag(textObj) != kTextTag) {
		logMessage(kConsoleDebug, "First arg to GetTextObjectDimensions should be a text object!\n");

		return;
	}

	TextEvent *event = TextEvent::getEvent(_engine, lua_getuserdata(textObj));

	if (!event) {
		logMessage(kConsoleError, "GetTextObjectDimensions: text object is invalid");

		return;
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		lua_pushnumber((float)event->getWidth() / kPs2HalfScreenWidth);
		lua_pushnumber((float)event->getHeight() / kPs2HalfScreenHeight);

		return;
	}

	int screenWidth = _engine->_textScreenRect[2];

	lua_pushnumber((float)((double)event->getWidth() / (double)(screenWidth / 2)));

	int screenHeight = _engine->_textScreenRect[3];

	lua_pushnumber((float)((double)event->getHeight() / (double)(screenHeight / 2)));
}

void Lua_V2::BlastText() {
	lua_Object textObj = lua_getparam(1);
	char tag[TextEvent::kTagBufferSize];

	if (!lua_isstring(textObj)) {
		logMessage(kConsoleDebug, "BlastText: the first argument must be a string.\n");

		return;
	}

	TextEvent *event;

	if (_engine->isCjkMode()) {
		event = new TextEvent(_engine, TextEvent::translateCjk(_engine, lua_getstring(textObj), tag));
	} else {
		const char *translated = TextEvent::translate(_engine, lua_getstring(textObj), tag);

		event = new TextEvent(_engine, LString(translated));
	}

	lua_Object tableObj = lua_getparam(2);

	if (tableObj && lua_istable(tableObj))
		applyTextTable(tableObj, event);

	delete event;
}

void Lua_V2::ExpireText() {
	Common::List<TextEvent *>::iterator it = _engine->_textEvents.begin();

	while (it != _engine->_textEvents.end()) {
		TextEvent *event = *it;

		++it;

		event->_duration = -1.0f;

		if (event->_flags & 4) {
			event->_flags &= ~5u;

			Actor *actor = Actor::actorDeref(_engine, event->_actorId);

			if (actor && actor->_extra)
				actor->_extra->_textEventId = 0;

			delete event;
		}
	}
}

void Lua_V2::PurgeText() {
	lua_Object keepHeldObj = lua_getparam(1);
	Common::List<TextEvent *>::iterator it;

	for (it = _engine->_textEvents.begin(); it != _engine->_textEvents.end(); ++it) {
		TextEvent *event = *it;

		// The demo ignores its argument and has no held events
		if (_engine->isDemo() || lua_isnil(keepHeldObj) || !event->_hold) {
			event->_duration = -1.0f;
			event->_flags &= ~1u;
		}
	}
}

void Lua_V2::PushText() {
	TextEvent::holdAll(_engine);
}

void Lua_V2::PopText() {
	TextEvent::releaseAll(_engine);
}

void Lua_V2::MakeColor() {
	lua_Object redObj = lua_getparam(1);
	lua_Object greenObj = lua_getparam(2);
	lua_Object blueObj = lua_getparam(3);
	lua_Object alphaObj = lua_getparam(4);
	int red;
	int green;
	int blue;
	int alpha;

	if (lua_isnumber(alphaObj)) {
		alpha = luaFtol(lua_getnumber(alphaObj));

		if (alpha < 0)
			alpha = 0;
		else if (alpha > 255)
			alpha = 255;
	} else {
		alpha = 255;
	}

	if (lua_isnumber(blueObj)) {
		blue = luaFtol(lua_getnumber(blueObj));

		if (blue < 0)
			blue = 0;
		else if (blue > 255)
			blue = 255;
	} else {
		blue = 0;
	}

	if (lua_isnumber(greenObj)) {
		green = luaFtol(lua_getnumber(greenObj));

		if (green < 0)
			green = 0;
		else if (green > 255)
			green = 255;
	} else {
		green = 0;
	}

	if (lua_isnumber(redObj)) {
		red = luaFtol(lua_getnumber(redObj));

		if (red < 0)
			red = 0;
		else if (red > 255)
			red = 255;
	} else {
		red = 0;
	}

	uint packed = (uint)alpha;

	packed = (packed << 8) | (uint)red;
	packed = (packed << 8) | (uint)green;
	packed = (packed << 8) | (uint)blue;

	lua_pushusertag((int32)packed, kColorTag);
}

void Lua_V2::GetColorComponents() {
	lua_Object colorObj = lua_getparam(1);
	Color color((uint)lua_getuserdata(colorObj));

	lua_pushnumber((float)(uint)color._r);
	lua_pushnumber((float)(uint)color._g);
	lua_pushnumber((float)(uint)color._b);
}

void Lua_V2::SetTranslationMode() {
	lua_Object modeObj = lua_getparam(1);

	if (lua_isnil(modeObj)) {
		_engine->_textMode = 0;

		logMessage(kConsoleDebug, "Showing localized text.\n");

		return;
	}

	if (!lua_isnumber(modeObj)) {
		logMessage(kConsoleDebug, "Invalid translation type!\n");

		return;
	}

	int mode = luaFtol(lua_getnumber(modeObj));

	if (mode < 0)
		_engine->_textMode = 0;
	else if (mode > 2)
		_engine->_textMode = 2;
	else
		_engine->_textMode = mode;

	if (_engine->_textMode == 0)
		logMessage(kConsoleDebug, "Showing localized text.\n");
	else if (_engine->_textMode == 1)
		logMessage(kConsoleDebug, "Showing domestic text.\n");
	else
		logMessage(kConsoleDebug, "Showing domestic text with markers.\n");
}

void Lua_V2::GetTranslationMode() {
	lua_pushnumber((float)_engine->_textMode);
}

void Lua_V2::LocalizeString() {
	// The CJK exe has an empty function here: nothing is pushed
	if (_engine->isCjkMode())
		return;

	lua_Object textObj = lua_getparam(1);
	char tag[TextEvent::kTagBufferSize];

	memset(tag, 0, sizeof(tag));

	if (!lua_isstring(textObj)) {
		logMessage(kConsoleDebug, "LocalizeString: Takes a string argument!\n");

		return;
	}

	const char *translated = TextEvent::translate(_engine, lua_getstring(textObj), tag);
	Common::String result;

	if (tag[0])
		result = Common::String::format("/%s/%s", tag, translated);
	else
		result = translated;

	// The original could overflow its buffer here, let's avoid that...
	if (result.size() >= (uint)kLocalizedBufferSize) {
		warning("Lua_V2::LocalizeString(): the string is too long");

		result = result.substr(0, kLocalizedBufferSize - 1);
	}

	lua_pushstring(result.c_str());
}

void Lua_V2::GetFontDimensions() {
	lua_Object fontObj = lua_getparam(1);

	if (!lua_isstring(fontObj))
		return;

	LString fontName = lua_getstring(fontObj);
	LResourceHandle<CTextFont> fontHandle(_engine, fontName);
	CTextFont *font = nullptr;

	if (fontHandle.lock())
		font = fontHandle._typed;

	if (!font) {
		// The original reads the dimensions of the missing font
		warning("Lua_V2::GetFontDimensions(): the font %s cannot be loaded", fontName.c_str());
		fontHandle.unlock();

		return;
	}

	int lineHeight = font->_lineHeight;
	int header52 = font->_header52;
	int header56 = font->_header56;
	int header60 = font->_header60;

	fontHandle.unlock();

	lua_pushnumber((float)lineHeight);
	lua_pushnumber((float)header52);
	lua_pushnumber((float)header56);
	lua_pushnumber((float)header60);
}

void Lua_V2::GetTextCharPosition() {
	lua_Object textObj = lua_getparam(1);
	lua_Object countObj = lua_getparam(2);

	if (!lua_isuserdata(textObj) || lua_tag(textObj) != kTextTag) {
		logMessage(kConsoleDebug, "First arg to GetTextCharPosition should be a text object!\n");

		return;
	}

	TextEvent *event = TextEvent::getEvent(_engine, lua_getuserdata(textObj));

	if (!event) {
		logMessage(kConsoleError, "GetTextCharPosition: text object is invalid");

		return;
	}

	int count = luaFtol(lua_getnumber(countObj));
	int width = 0;

	if (_engine->isCjkMode()) {
		width = event->getCjkPrefixWidth(count);
	} else {
		for (int i = 0; i < count; i++) {
			if ((uint)i >= event->_text.size())
				break;

			width += event->getCharWidth((byte)event->_text[i]);
		}
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		lua_pushnumber((float)width / kPs2HalfScreenWidth);

		return;
	}

	int screenWidth = _engine->_textScreenRect[2];

	lua_pushnumber((float)((double)width / (double)(screenWidth / 2)));
}

void Lua_V2::SetOffscreenTextPos() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	float x;
	double y;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		float ps2X = 0.0f;
		float ps2Y = kPs2DefaultOffscreenY;

		if (lua_isnumber(xObj))
			ps2X = lua_getnumber(xObj);

		if (lua_isnumber(yObj))
			ps2Y = lua_getnumber(yObj);

		_engine->_offscreenTextRect.x = luaFtol(kPs2HalfScreenWidth + (kPs2ScreenWidth * ps2X) / 2.0f);
		_engine->_offscreenTextRect.y = luaFtol(kPs2HalfScreenHeight + (kPs2ScreenHeight * -ps2Y) / 2.0f);

		return;
	}

	if (lua_isnumber(xObj))
		x = lua_getnumber(xObj);
	else
		x = 0.0f;

	if (lua_isnumber(yObj))
		y = lua_getnumber(yObj);
	else
		y = 0.875;

	int screenWidth = _engine->_textScreenRect[2];
	int screenHeight = _engine->_textScreenRect[3];

	_engine->_offscreenTextRect.x = truncateToInt((double)screenWidth * (double)x * (double)kHalf + (double)(screenWidth / 2));
	_engine->_offscreenTextRect.y =truncateToInt((double)(screenHeight / 2) - (double)screenHeight * y * (double)kHalf);
}

} // end of namespace Grim
