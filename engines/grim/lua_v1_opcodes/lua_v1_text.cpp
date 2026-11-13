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

#include "common/savefile.h"
#include "common/system.h"

#include "engines/grim/grim.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/inputdialog.h"
#include "engines/grim/text/blasttext.h"
#include "engines/grim/text/hdtext.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/text/text.h"
#include "engines/grim/lua/lauxlib.h"

namespace Grim {

SputTextEvent *Lua_V1::getTextEvent(lua_Object obj) {
	SputTextEvent *event = g_grim->_sputText->findSerialNumber(lua_getuserdata(obj));

	if (!event)
		warning("Lua_V1::getTextEvent(): the text object does not exist");

	return event;
}

/* Destroy a text object since we don't need it anymore
 * note that the menu creates more objects than it needs,
 * so it deletes some objects right after creating them
 */
void Lua_V1::KillTextObject() {
	lua_Object textObj = lua_getparam(1);

	if (textObj && lua_isuserdata(textObj) && lua_tag(textObj) == MKTAG('T', 'X', 'E', 'T')) {
		SputTextEvent *event = getTextEvent(textObj);

		if (event)
			event->flags &= ~1;
	}
}

/* Make changes to a text object based on the parameters passed
 * in the table in the LUA parameter 2.
 */
void Lua_V1::ChangeTextObject() {
	lua_Object textObj = lua_getparam(1);
	if (lua_isuserdata(textObj) && lua_tag(textObj) == MKTAG('T', 'X', 'E', 'T')) {
		SputTextEvent *event = getTextEvent(textObj);
		SputText *sputText = g_grim->_sputText;
		int paramId = 2;

		if (!event)
			return;

		if (g_grim->getGameFlags() & ADGF_DEMO) {
			// The demo copies the strings as they are, does not move the object and returns nothing
			const char *string = nullptr;

			for (int i = 0; i < 2; i++) {
				lua_Object paramObj = lua_getparam(paramId++);

				if (!paramObj)
					break;

				if (lua_isstring(paramObj)) {
					string = lua_getstring(paramObj);
					Common::strlcpy(event->text, string, 128);
				} else if (lua_isnumber(paramObj)) {
					if (string)
						Common::strlcpy(event->text, string, 128);
				} else if (lua_istable(paramObj)) {
					sputText->getTextObjectParams(&event->params, paramObj, nullptr);
				} else {
					warning("ChangeTextObject args must be formattable, string, or transid");
					break;
				}
			}

			return;
		}

		for (int i = 0; i < 2; i++) {
			lua_Object paramObj = lua_getparam(paramId++);
			if (!paramObj)
				break;

			if (lua_isstring(paramObj)) {
				const char *text = sputText->extractSayLineData(lua_getstring(paramObj), nullptr);

				Common::strlcpy(event->text, text, sputText->getTextCapacity());
			} else {
				if (!lua_istable(paramObj))
					break;
				sputText->getTextObjectParams(&event->params, paramObj, event);
			}
		}

		// The HD layout has to be done again
		if (g_grim->isRemastered())
			sputText->invalidateHdText(event);

		lua_pushnumber((float)g_grim->_text->getStringWidth(&event->params, event->text));
		lua_pushnumber((float)g_grim->_text->getStringHeight(&event->params, event->text));
	}
}

void Lua_V1::GetTextSpeed() {
	lua_pushnumber((float)g_grim->_sputActor->getActorTextRate());
}

void Lua_V1::SetTextSpeed() {
	lua_Object speedObj = lua_getparam(1);
	if (!lua_isnumber(speedObj))
		return;

	g_grim->_sputActor->setActorTextRate(luaFtol(lua_getnumber(speedObj)));
}

void Lua_V1::MakeTextObject() {
	SputText *sputText = g_grim->_sputText;
	StdRect clip;

	clip.x = 0;
	clip.y = 0;
	clip.width = g_grim->_blastText->getSurface()->width;
	clip.height = g_grim->_blastText->getSurface()->height;

	lua_Object textObj = lua_getparam(1);
	if (!lua_isstring(textObj)) {
		return;
	}

	const char *msgId = lua_getstring(textObj);
	TextParams params = sputText->_blastTextParams;

	params.clip = &clip;

	lua_Object tableObj = lua_getparam(2);

	if (g_grim->isRemastered()) {
		int coords = 0;

		if (tableObj && lua_istable(tableObj))
			coords = sputText->getCoordsFromTable(tableObj);

		params.coords = coords;
		sputText->getHdClip(&clip, coords);
	}

	if (tableObj && lua_istable(tableObj))
		sputText->getTextObjectParams(&params, tableObj, nullptr);

	char translation[32];
	const char *text = sputText->extractSayLineData(msgId, translation);
	SputTextEvent *event = sputText->allocEvent(&params, text);

	lua_pushusertag(event->serial, MKTAG('T', 'X', 'E', 'T'));

	// The demo returns only the text object
	if (!(g_grim->getGameFlags() & ADGF_DEMO)) {
		lua_pushnumber((float)g_grim->_text->getStringWidth(&event->params, event->text));
		lua_pushnumber((float)g_grim->_text->getStringHeight(&event->params, event->text));
	}

	sputText->addToQueue(event, 0, 1);
}

void Lua_V1::GetTextObjectDimensions() {
	lua_Object textObj = lua_getparam(1);

	if (g_grim->isRemastered()) {
		// The size and the position come from the HD layout, in the units of the coordinates asked
		lua_Object modeObj = lua_getparam(2);
		int mode = luaFtol(lua_getnumber(modeObj));
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;

		// Anything else than a text object leaves the stack as it is
		if (!lua_isuserdata(textObj) || lua_tag(textObj) != MKTAG('T', 'X', 'E', 'T'))
			return;

		SputTextEvent *event = getTextEvent(textObj);

		if (event && event->text[0] && g_grim->_sputText->queryTextObject(event, &x, &y, &width, &height)) {
			HdText::convertCoordsNative(&x, &y, mode, false);
			HdText::convertCoordsNative(&width, &height, mode, true);
		}

		lua_pushnumber((float)width);
		lua_pushnumber((float)height);
		lua_pushnumber((float)x);
		lua_pushnumber((float)y);

		return;
	}

	if (lua_isuserdata(textObj) && lua_tag(textObj) == MKTAG('T', 'X', 'E', 'T')) {
		SputTextEvent *event = getTextEvent(textObj);

		if (event) {
			lua_pushnumber((float)g_grim->_text->getStringWidth(&event->params, event->text));
			lua_pushnumber((float)g_grim->_text->getStringHeight(&event->params, event->text));
		}
	}
}

void Lua_V1::ExpireText() {
	SputTextEvent *event = g_grim->_sputText->_textQueue;

	while (event) {
		SputTextEvent *next = event->next;

		if (event->flags & 4) {
			SputActor *actor = event->actor;

			if (g_grim->getGameFlags() & ADGF_DEMO) {
				// The demo does not kill the event: it clears the flags and the duration is set below
				event->flags &= ~5;
			} else {
				g_grim->_sputText->killEvent(event);
			}

			if (actor && actor->extra)
				actor->extra->sayText = nullptr;

			if (g_grim->getGameFlags() & ADGF_DEMO)
				event->duration = -1;
		} else {
			event->duration = -1;
		}

		event = next;
	}
}

void Lua_V1::PurgeText() {
	SputTextEvent *event = g_grim->_sputText->_textQueue;

	while (event) {
		event->duration = -1;
		event->flags &= ~1;
		event = event->next;
	}
}

void Lua_V1::GetTextCharPosition() {
	lua_Object textObj = lua_getparam(1);
	lua_Object posObj = lua_getparam(2);
	if (lua_isuserdata(textObj) && lua_tag(textObj) == MKTAG('T', 'X', 'E', 'T')) {
		SputTextEvent *event = getTextEvent(textObj);
		int pos = luaFtol(lua_getnumber(posObj));
		int width = 0;

		if (!event)
			return;

		if (pos > 0) {
			const char *text = event->text;

			do {
				char c = *text;

				if (!c)
					break;

				text++;
				width += g_grim->_text->getCharKernedWidth(&event->params, c);
			} while ((int)(text - event->text) < pos);
		}

		lua_pushnumber((float)width);
	}
}

void Lua_V1::BlastText() {
	SputText *sputText = g_grim->_sputText;
	StdRect clip;

	clip.x = 0;
	clip.y = 0;
	clip.width = g_grim->_blastText->getSurface()->width;
	clip.height = g_grim->_blastText->getSurface()->height;

	lua_Object textObj = lua_getparam(1);
	if (!lua_isstring(textObj)) {
		return;
	}

	const char *msgId = lua_getstring(textObj);
	bool remastered = g_grim->isRemastered();

	// The remastered reports the call as unsupported and still draws the text once
	if (remastered)
		warning("BlastText is not supported.");

	// The remastered draws the text on the canvas, not on the buffer of the scene
	if (!remastered)
		g_grim->_stdDisplay->vBufferLock(g_grim->_sputRender->drawBuffer());

	TextParams params = sputText->_blastTextParams;

	if (!remastered)
		params.surface->pixels = g_grim->_sputRender->drawBuffer()->pixels;

	params.clip = &clip;

	lua_Object tableObj = lua_getparam(2);

	if (remastered) {
		int coords = 0;

		if (tableObj && lua_istable(tableObj))
			coords = sputText->getCoordsFromTable(tableObj);

		params.coords = coords;
		sputText->getHdClip(&clip, coords);
	}

	if (tableObj && lua_istable(tableObj))
		sputText->getTextObjectParams(&params, tableObj, nullptr);

	char translation[32];
	const char *text = sputText->extractSayLineData(msgId, translation);
	SputTextEvent *event = sputText->allocEvent(&params, text);

	sputText->drawTextEvent(event, 0);

	if (!remastered)
		g_grim->_stdDisplay->vBufferUnlock(g_grim->_sputRender->drawBuffer());

	sputText->killEvent(event);
}

void Lua_V1::SetOffscreenTextPos() {
	// The remastered stores the position but nothing reads it
	if (g_grim->isRemastered())
		return;

	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	SputText *sputText = g_grim->_sputText;

	if (lua_isnumber(xObj))
		sputText->_centerValue = luaFtol(lua_getnumber(xObj));
	else
		sputText->_centerValue = 320;

	if (lua_isnumber(yObj))
		sputText->_offscreenTextPos = luaFtol(lua_getnumber(yObj));
	else
		sputText->_offscreenTextPos = 450;
}

void Lua_V1::TextFileGetLine() {
	char textBuf[512];
	lua_Object nameObj = lua_getparam(1);
	lua_Object posObj = lua_getparam(2);
	Common::SeekableReadStream *file;

	if (lua_isnil(nameObj) || lua_isnil(posObj)) {
		lua_pushnil();
		return;
	}

	const char *filename = lua_getstring(nameObj);
	Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
	file = saveFileMan->openForLoading(filename);
	if (!file) {
		lua_pushnil();
		return;
	}

	int pos = luaFtol(lua_getnumber(posObj));

	if (pos >= 0)
		file->seek(pos, SEEK_SET);

	memset(textBuf, 0, sizeof(textBuf));
	file->readLine(textBuf, sizeof(textBuf));
	delete file;

	lua_pushstring(textBuf);
}

void Lua_V1::TextFileGetLineCount() {
	char textBuf[512];
	lua_Object nameObj = lua_getparam(1);

	if (lua_isnil(nameObj)) {
		lua_pushnil();
		return;
	}

	const char *filename = luaL_check_string(1);
	Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
	Common::SeekableReadStream *file = saveFileMan->openForLoading(filename);
	if (!file) {
		lua_pushnil();
		return;
	}

	lua_Object result = lua_createtable();

	int line = 0;
	for (;;) {
		if (file->eos())
			break;
		lua_pushobject(result);
		lua_pushnumber((float)line);
		lua_pushnumber((float)file->pos());
		lua_settable();
		file->readLine(textBuf, sizeof(textBuf));
		line++;
	}
	delete file;

	lua_pushobject(result);
	lua_pushstring("count");
	lua_pushnumber((float)line);
	lua_settable();
	lua_pushobject(result);
}

// Localization function

void Lua_V1::LocalizeString() {
	char translation[32];
	char buf[640];
	lua_Object obj = lua_getparam(1);

	if (lua_isstring(obj)) {
		const char *text = g_grim->_sputText->extractSayLineData(lua_getstring(obj), translation);

		Common::sprintf_s(buf, "/%s/%s", translation, text);
		lua_pushstring(buf);
	}
}

void Lua_V1::SetSayLineDefaults() {
	lua_Object tableObj = lua_getparam(1);
	if (tableObj && lua_istable(tableObj))
		g_grim->_sputText->getTextObjectParams(g_grim->_sputText->getDefaultTextParams(), tableObj, nullptr);
}

void Lua_V1::SayLine() {
	SputActorModule *sputActor = g_grim->_sputActor;
	SputText *sputText = g_grim->_sputText;
	int vol = 127, pan = 64, paramId = 2;
	int x = -1, y = -1;
	int background = 1;
	const char *msgId = nullptr;
	lua_Object paramObj = lua_getparam(1);

	if ((lua_isuserdata(paramObj) && lua_tag(paramObj) == MKTAG('R','T','C','A'))
			|| lua_isstring(paramObj) || lua_istable(paramObj)) {
		SputActor *actor = sputActor->getSelectedActor();
		if (lua_isuserdata(paramObj) && lua_tag(paramObj) == MKTAG('R','T','C','A')) {
			actor = sputActor->actorDeref(lua_getuserdata(paramObj));
			paramId = 3;
			paramObj = lua_getparam(2);
		}
		if (actor) {
			while (!lua_isnil(paramObj)) {
				if (!lua_isstring(paramObj) && !lua_isnumber(paramObj) && !lua_istable(paramObj))
					return;
				if (lua_istable(paramObj)) {
					sputText->parseSayLineTable(paramObj, &background, &vol, &pan, &x, &y);
				} else if (lua_isnumber(paramObj)) {
					background = 0;
				} else {
					char translation[32];

					msgId = lua_getstring(paramObj);
					sputText->extractSayLineData(msgId, translation);
				}
				paramObj = lua_getparam(paramId++);
			}
			if (msgId)
				sputActor->sayLine(actor, msgId, background, vol, pan, x, y);
		}
	}
}

void Lua_V1::SayLineDemo() {
	SputActorModule *sputActor = g_grim->_sputActor;
	lua_Object paramObj = lua_getparam(1);

	if (!(lua_isuserdata(paramObj) && lua_tag(paramObj) == MKTAG('R','T','C','A')) && !lua_isstring(paramObj) && !lua_isnumber(paramObj)) {
		warning("SayLine: Invalid argument");
		return;
	}

	SputActor *actor = sputActor->getSelectedActor();

	if (lua_isuserdata(paramObj) && lua_tag(paramObj) == MKTAG('R','T','C','A')) {
		actor = sputActor->actorDeref(lua_getuserdata(paramObj));
		paramObj = lua_getparam(2);
	}

	if (!actor) {
		warning("SayLine: Invalid actor handle");
		return;
	}

	if (!lua_isstring(paramObj)) {
		warning("SayLine: Invalid message");
		return;
	}

	char translation[32];
	const char *msgId = lua_getstring(paramObj);

	if (!g_grim->_sputText->extractSayLineData(msgId, translation))
		warning("SayLine: Invalid message format");

	sputActor->sayLineDemo(actor, msgId);
}

void Lua_V1::ShutUpActorDemo() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (actor)
		g_grim->_sputActor->shutUpDemo(actor);
}

void Lua_V1::ShutUpActor() {
	lua_Object actorObj = lua_getparam(1);

	if (!isSputActor(actorObj))
		return;

	SputActor *actor = getSputActor(actorObj);

	if (actor)
		g_grim->_sputActor->shutUp(actor);
}

void Lua_V1::PrintLine() {
	SputText *sputText = g_grim->_sputText;
	BlastTextSurface *surface = g_grim->_blastText->getSurface();
	int duration = -1;
	lua_Object param1Obj = lua_getparam(1);
	lua_Object param2Obj = lua_getparam(2);

	if (sputText->_printLineClip.width >= surface->width)
		sputText->_printLineClip.width = surface->width;

	if (sputText->_printLineClip.height >= surface->height)
		sputText->_printLineClip.height = surface->height;

	if ((lua_isstring(param1Obj) || lua_isnil(param1Obj)) && (lua_istable(param2Obj) || lua_isnil(param2Obj))) {
		TextParams localParams;
		TextParams *params;

		if (lua_isnil(param1Obj)) {
			params = &sputText->_printLineParams;
		} else {
			localParams = sputText->_printLineParams;
			params = &localParams;
		}

		if (lua_istable(param2Obj))
			duration = sputText->getTextObjectParams(params, param2Obj, nullptr);

		if (lua_isstring(param1Obj)) {
			char translation[32];
			const char *text = sputText->extractSayLineData(lua_getstring(param1Obj), translation);

			if (duration < 0)
				duration = (int)((double)(40 + 3 * (int)strlen(text)) * 16.6666679f);

			SputTextEvent *event = sputText->allocEvent(params, text);

			sputText->addToQueue(event, duration, 0);
		}
	}
}

void Lua_V1::InputDialog() {
	lua_Object titleObj = lua_getparam(1);
	lua_Object messageObj = lua_getparam(2);
	lua_Object defaultObj = lua_getparam(3);

	if (!lua_isstring(titleObj) || !lua_isstring(messageObj) || (!lua_isnil(defaultObj) && !lua_isstring(defaultObj))) {
		lua_pushnil();
		return;
	}

	Common::String str = lua_getstring(titleObj);
	str += ": ";
	str += lua_getstring(messageObj);

	Common::String defaultString;

	if (lua_isstring(defaultObj))
		defaultString = lua_getstring(defaultObj);

	Grim::InputDialog d(str, defaultString);
	int res = d.runModal();
	// The KeyUp event for CTRL has been eat by the gui loop, so we
	// need to reset it manually.
	g_grim->clearEventQueue();
	if (res) {
		Common::String text = d.getString();

		if (text.size() > 100)
			text = text.substr(0, 100);

		lua_pushstring(text.c_str());
	} else {
		lua_pushnil();
	}
}

void Lua_V1::IsMessageGoing() {
	lua_Object actorObj = lua_getparam(1);

	if (!actorObj || (lua_isuserdata(actorObj) && lua_tag(actorObj) == MKTAG('R','T','C','A')) || lua_isnil(actorObj)) {
		if (lua_isuserdata(actorObj) && lua_tag(actorObj) == MKTAG('R','T','C','A')) {
			SputActor *actor = g_grim->_sputActor->actorDeref(lua_getuserdata(actorObj));
			if (actor && actor->extra && actor->extra->talking) {
				lua_pushnumber(1.0f);
				return;
			}
		} else {
			SputTextEvent *event = g_grim->_sputText->_textQueue;

			if (g_grim->_sputActor->getNumActorsTalking()) {
				lua_pushnumber(1.0f);
				return;
			}

			while (event) {
				if (!(event->flags & 1)) {
					lua_pushnumber(1.0f);
					return;
				}

				event = event->next;
			}
		}
		lua_pushnil();
	}
}

void Lua_V1::GetTranslationMode() {
	lua_pushnumber((float)g_grim->_sputText->_translationMode);
}

void Lua_V1::SetTranslationMode() {
	lua_Object paramObj = lua_getparam(1);

	if (lua_isnil(paramObj)) {
		g_grim->_sputText->_translationMode = 0;
	} else if (lua_isnumber(paramObj)) {
		int mode = luaFtol(lua_getnumber(paramObj));

		if (mode < 0)
			mode = 0;
		else if (mode > 2)
			mode = 2;

		g_grim->_sputText->_translationMode = mode;
	}
}

} // end of namespace Grim
