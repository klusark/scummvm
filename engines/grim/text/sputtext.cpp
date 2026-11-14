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

#include "common/str.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/hotspot.h"
#include "engines/grim/text/hdtext.h"
#include "engines/grim/text/normaltext.h"
#include "engines/grim/text/polyglot.h"
#include "engines/grim/res.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/text/text.h"

namespace Grim {

static uint16 colorToRgb565(int color) {
	uint value = (uint)color;

	return (uint16)(((value >> 19) & 0x1F) | (32 * (((uint16)(value >> 3) << 6) | ((value >> 10) & 0x3F))));
}

SputText::SputText(Res *res, RenderDroid *rd, NormalText *normalText, Text *text, BlastText *blastText, SputTime *sputTime, Polyglot *polyglot, bool isRemastered) :
		_res(res), _rd(rd), _normalText(normalText), _text(text), _blastText(blastText), _sputTime(sputTime),
		_polyglot(polyglot),
		_sputRender(nullptr), _sputRoom(nullptr), _grim(nullptr),
		_hs(nullptr), _initialized(0), _started(0), _locked(0), _textQueue(nullptr), _font(nullptr), _emergencyFont(nullptr),
		_translationMode(0), _centerValue(320), _offscreenTextPos(450), _textPosX(0), _textPosY(0),
		_luaTextObjXRef(0), _luaTextObjYRef(0), _luaTextObjFontRef(0), _luaTextObjWidthRef(0), _luaTextObjHeightRef(0),
		_luaTextObjFGColorRef(0), _luaTextObjBGColorRef(0), _luaTextObjFXColorRef(0), _luaTextObjHIColorRef(0),
		_luaTextObjDurationRef(0), _luaTextObjCenterRef(0), _luaTextObjLJustifyRef(0), _luaTextObjRJustifyRef(0),
		_luaTextObjVolumeRef(0), _luaTextObjBufferRef(0), _luaTextObjBackgroundRef(0),
		_luaTextObjCoordsRef(0), _luaTextObjLeftClipRef(0), _luaTextObjAlphaRef(0), _luaTextObjLayerRef(0),
		_luaTextObjFadeRef(0), _luaTextObjHighlightRef(0), _luaTextObjCacheRef(0), _luaTextObjMirrorModeRef(0) {
	_printLineClip.x = 0;
	_printLineClip.y = 0;
	_printLineClip.width = 640;
	_printLineClip.height = 480;

	_printLineParams.surface = _blastText->getSurface();
	_printLineParams.clip = &_printLineClip;
	_printLineParams.font = nullptr;
	_printLineParams.x = 0;
	_printLineParams.y = 100;
	_printLineParams.fgColor = 0;
	_printLineParams.bgColor = 0;
	_printLineParams.fxColor = 0;
	_printLineParams.scaleX = 0x10000;
	_printLineParams.scaleY = 0x10000;
	_printLineParams.flags = 4;
	_printLineParams.colorMap = nullptr;
	_printLineParams.coords = 0;
	_printLineParams.highlight = -1;
	_printLineParams.alpha = 1.0f;

	_defaultTextParams.surface = _blastText->getSurface();
	_defaultTextParams.clip = nullptr;
	_defaultTextParams.font = nullptr;
	_defaultTextParams.x = 0;
	_defaultTextParams.y = 100;
	_defaultTextParams.fgColor = 0;
	_defaultTextParams.bgColor = 0;
	_defaultTextParams.fxColor = 0;
	_defaultTextParams.scaleX = 0x10000;
	_defaultTextParams.scaleY = 0x10000;
	_defaultTextParams.flags = isRemastered ? 0x45 : 5;
	_defaultTextParams.colorMap = nullptr;
	_defaultTextParams.coords = 0;
	_defaultTextParams.highlight = -1;
	_defaultTextParams.alpha = 1.0f;

	_blastTextParams.surface = _blastText->getSurface();
	_blastTextParams.clip = nullptr;
	_blastTextParams.font = nullptr;
	_blastTextParams.x = 0;
	_blastTextParams.y = 200;
	_blastTextParams.fgColor = 10;
	_blastTextParams.bgColor = 0;
	_blastTextParams.fxColor = 0;
	_blastTextParams.scaleX = 0x10000;
	_blastTextParams.scaleY = 0x10000;
	_blastTextParams.flags = 4;
	_blastTextParams.colorMap = nullptr;
	_blastTextParams.coords = 0;
	_blastTextParams.highlight = -1;
	_blastTextParams.alpha = 1.0f;
}

void SputText::killEvent(SputTextEvent *event) {
	if (event) {
		if (event->next)
			event->next->prev = event->prev;

		if (event->prev)
			event->prev->next = event->next;

		if (event == _textQueue)
			_textQueue = event->next;

		invalidateHdText(event);
		delete event;
	}
}

void SputText::update(int force) {
	if (_grim->isRemastered()) {
		updateHd(force, 0);
		return;
	}

	if (!_locked) {
		int frameTime = _sputTime->getFrameTime();
		SputTextEvent *event = _textQueue;

		while (event) {
			if (event->duration > 0 || (event->flags & 1)) {
				int flags = event->flags;

				// The demo draws all the events at every frame
				if ((_grim->getGameFlags() & ADGF_DEMO) || (((flags & 8) || force) && !(flags & 2)))
					drawTextEvent(event, 1);

				event->duration -= frameTime;
				event = event->next;
			} else {
				SputTextEvent *next = event->next;

				killEvent(event);
				event = next;
			}
		}
	}
}

int SputText::initialize(SysStruct *hs) {
	_hs = hs;

	if (_normalText->initialize(hs))
		return 1;

	int result = _text->sysInitialize(hs);

	if (result)
		return 1;

	_initialized = 1;

	return result;
}

void SputText::pause() {
	_locked = 1;
}

void SputText::resume() {
	_locked = 0;
}

SputTextEvent *SputText::createActorText(SputActor *actor, const char *text, int x, int y) {
	BlastTextSurface *surface = _blastText->getSurface();
	StdRect clip;
	StdRect offscreenRect;
	TextParams params = _defaultTextParams;

	if (!actor || !actor->extra || !params.font) {
		warning("SputText::createActorText(): no actor, actor extra data or font");
		return nullptr;
	}

	if (_grim->isRemastered()) {
		int penX = x;
		int penY = y;

		if (x == -1 && y == -1) {
			penX = surface->width / 2;
			penY = surface->height - 40;
		}

		clip.x = 320;
		clip.y = 0;
		clip.width = 1280;
		clip.height = 1080;
		HotspotManager::convertRectNative(&clip, HotspotManager::kCoordsLegacy);

		params.clip = &clip;
		params.fgColor = actor->extra->talkColor;
		params.bgColor = 0;
		params.fxColor = 0;
		params.flags |= 0x65;
		params.x = penX;
		params.y = penY;

		SputTextEvent *hdEvent = allocEvent(&params, text);

		hdEvent->actor = actor;
		actor->flags |= 4;

		return hdEvent;
	}

	clip.x = 0;
	clip.y = 0;
	clip.width = surface->width;
	clip.height = surface->height;
	offscreenRect.x = _centerValue;
	offscreenRect.y = _offscreenTextPos;
	offscreenRect.width = _textPosX;
	offscreenRect.height = _textPosY;

	params.clip = &clip;
	params.fgColor = actor->extra->talkColor;
	params.bgColor = 0;
	params.fxColor = 0;

	int mode = _grim->getLibMode();
	StdRect *rect;

	if (!actor->onScreen || mode == 3) {
		rect = &offscreenRect;

		if (!actor->costumeStack || (actor->flags & 2)) {
			if (!scumm_stricmp(actor->curSetName, _sputRoom->getCurrentRoomName())) {
				int screen[3];

				if (_rd->_primit3.getScreenCoord(actor->actorPos, screen)) {
					offscreenRect.x = screen[0];
					offscreenRect.y = screen[1];
				}
			}
		}
	} else {
		rect = &actor->drawnRect;
	}

	int halfWidth = _text->getStringWidth(&params, text) / 2;
	int penX = x;

	if (x == -1)
		penX = rect->x + rect->width / 2;

	if (penX - halfWidth < 40)
		penX = halfWidth + 40;

	if (halfWidth + penX > surface->width - 40)
		penX = surface->width - halfWidth - 40;

	params.x = penX;
	params.y = params.font->baseLine;

	int height = _text->getStringHeight(&params, text);
	int penY = y;

	if (y == -1)
		penY = rect->y - height;

	if (penY < 10)
		penY = 10;

	if (penY + height > surface->height - 10)
		penY = surface->height - height - 10;

	params.x = penX;
	params.y = params.font->baseLine + penY;

	SputTextEvent *event = allocEvent(&params, text);

	event->actor = actor;
	actor->flags |= 4;

	return event;
}

SputTextEvent *SputText::createActorTextDemo(SputActor *actor, const char *text) {
	BlastTextSurface *surface = _blastText->getSurface();
	StdRect clip;
	StdRect defaultRect;
	TextParams params = _defaultTextParams;

	if (!actor || !actor->extra || !params.font) {
		warning("SputText::createActorTextDemo(): no actor, actor extra data or font");
		return nullptr;
	}

	clip.x = 0;
	clip.y = 0;
	clip.width = surface->width;
	clip.height = surface->height;
	defaultRect.x = 320;
	defaultRect.y = 450;
	defaultRect.width = 0;
	defaultRect.height = 0;

	params.clip = &clip;
	params.fgColor = actor->extra->talkColor;
	params.bgColor = 0;
	params.fxColor = 0;

	StdRect *rect = &defaultRect;

	if (actor->onScreen) {
		rect = &actor->drawnRect;
	} else if (!actor->costumeStack || (actor->flags & 2)) {
		int screen[3];

		if (!scumm_stricmp(actor->curSetName, _sputRoom->getCurrentRoomName()) && _rd->_primit3.getScreenCoord(actor->actorPos, screen)) {
			defaultRect.x = screen[0];
			defaultRect.y = screen[1];
		}
	}

	int halfWidth = _text->getStringWidth(&params, text) / 2;
	int halfRect = rect->width / 2;
	int penX = rect->x + (halfRect > halfWidth ? halfRect : halfWidth);

	if (penX < 40)
		penX = 40;

	if (penX > surface->width - 40)
		penX = surface->width - 40;

	params.x = penX;
	params.y = params.font->baseLine;

	int height = _text->getStringHeight(&params, text);
	int penY = rect->y - height - 10;

	if (penY < 0)
		penY = 0;

	if (penY > surface->height - height)
		penY = surface->height - height;

	params.x = penX;
	params.y = params.font->baseLine + penY;

	SputTextEvent *event = allocEvent(&params, text);

	event->actor = actor;

	return event;
}

SputTextEvent *SputText::addToQueue(SputTextEvent *event, int duration, int flags) {
	if (event) {
		if (!event->params.font) {
			warning("SputText::addToQueue(): the event has no font");
			return event;
		}

		event->duration = duration;
		event->flags = flags;
		event->x = event->params.x;

		int y = event->params.y - event->params.font->baseLine;

		event->y = y;
		event->params.y = y;
		event->next = _textQueue;

		if (_textQueue)
			_textQueue->prev = event;

		_textQueue = event;
	}

	return event;
}

int SputText::release() {
	_hs = nullptr;
	_normalText->terminate();
	_text->sysTerminate();

	int result = _initialized;

	if (_initialized)
		_initialized = 0;

	return result;
}

int SputText::startup() {
	if (_started || _normalText->startup() || _text->sysStartup())
		return 1;

	_font = _text->getSystemFont();
	_printLineParams.font = _font;
	_defaultTextParams.font = _font;
	_blastTextParams.font = _font;

	_lua->registerTextOpcodes();

	if (_grim->isRemastered()) {
		lua_pushstring("x");
		_luaTextObjXRef = lua_ref(1);
		lua_pushstring("y");
		_luaTextObjYRef = lua_ref(1);
		lua_pushstring("cache");
		_luaTextObjCacheRef = lua_ref(1);
		lua_pushstring("font");
		_luaTextObjFontRef = lua_ref(1);
		lua_pushstring("width");
		_luaTextObjWidthRef = lua_ref(1);
		lua_pushstring("leftclip");
		_luaTextObjLeftClipRef = lua_ref(1);
		lua_pushstring("height");
		_luaTextObjHeightRef = lua_ref(1);
		lua_pushstring("fgcolor");
		_luaTextObjFGColorRef = lua_ref(1);
		lua_pushstring("bgcolor");
		_luaTextObjBGColorRef = lua_ref(1);
		lua_pushstring("fxcolor");
		_luaTextObjFXColorRef = lua_ref(1);
		lua_pushstring("hicolor");
		_luaTextObjHIColorRef = lua_ref(1);
		lua_pushstring("duration");
		_luaTextObjDurationRef = lua_ref(1);
		lua_pushstring("center");
		_luaTextObjCenterRef = lua_ref(1);
		lua_pushstring("ljustify");
		_luaTextObjLJustifyRef = lua_ref(1);
		lua_pushstring("rjustify");
		_luaTextObjRJustifyRef = lua_ref(1);
		lua_pushstring("layer");
		_luaTextObjLayerRef = lua_ref(1);
		lua_pushstring("highlight");
		_luaTextObjHighlightRef = lua_ref(1);
		lua_pushstring("coords");
		_luaTextObjCoordsRef = lua_ref(1);
		lua_pushstring("volume");
		_luaTextObjVolumeRef = lua_ref(1);
		lua_pushstring("pan");
		_luaTextObjBufferRef = lua_ref(1);
		lua_pushstring("background");
		_luaTextObjBackgroundRef = lua_ref(1);
		lua_pushstring("alpha");
		_luaTextObjAlphaRef = lua_ref(1);
		lua_pushstring("fade");
		_luaTextObjFadeRef = lua_ref(1);
		lua_pushstring("mirrormode");
		_luaTextObjMirrorModeRef = lua_ref(1);
	} else {
		lua_pushstring("x");
		_luaTextObjXRef = lua_ref(1);
		lua_pushstring("y");
		_luaTextObjYRef = lua_ref(1);
		lua_pushstring("font");
		_luaTextObjFontRef = lua_ref(1);
		lua_pushstring("width");
		_luaTextObjWidthRef = lua_ref(1);
		lua_pushstring("height");
		_luaTextObjHeightRef = lua_ref(1);
		lua_pushstring("fgcolor");
		_luaTextObjFGColorRef = lua_ref(1);
		lua_pushstring("bgcolor");
		_luaTextObjBGColorRef = lua_ref(1);
		lua_pushstring("fxcolor");
		_luaTextObjFXColorRef = lua_ref(1);
		lua_pushstring("hicolor");
		_luaTextObjHIColorRef = lua_ref(1);
		lua_pushstring("duration");
		_luaTextObjDurationRef = lua_ref(1);
		lua_pushstring("center");
		_luaTextObjCenterRef = lua_ref(1);
		lua_pushstring("ljustify");
		_luaTextObjLJustifyRef = lua_ref(1);
		lua_pushstring("rjustify");
		_luaTextObjRJustifyRef = lua_ref(1);
		lua_pushstring("volume");
		_luaTextObjVolumeRef = lua_ref(1);
		lua_pushstring("pan");
		_luaTextObjBufferRef = lua_ref(1);
		lua_pushstring("background");
		_luaTextObjBackgroundRef = lua_ref(1);
	}

	_started = 1;

	return 0;
}

int SputText::shutdown() {
	_normalText->shutdown();
	_text->sysShutdown();

	if (_grim->isRemastered()) {
		lua_unref(_luaTextObjXRef);
		lua_unref(_luaTextObjYRef);
		lua_unref(_luaTextObjFontRef);
		lua_unref(_luaTextObjWidthRef);
		lua_unref(_luaTextObjLeftClipRef);
		lua_unref(_luaTextObjHeightRef);
		lua_unref(_luaTextObjFGColorRef);
		lua_unref(_luaTextObjBGColorRef);
		lua_unref(_luaTextObjFXColorRef);
		lua_unref(_luaTextObjHIColorRef);
		lua_unref(_luaTextObjDurationRef);
		lua_unref(_luaTextObjCenterRef);
		lua_unref(_luaTextObjAlphaRef);
		lua_unref(_luaTextObjFadeRef);
		lua_unref(_luaTextObjLJustifyRef);
		lua_unref(_luaTextObjRJustifyRef);
		lua_unref(_luaTextObjLayerRef);
		lua_unref(_luaTextObjHighlightRef);
		lua_unref(_luaTextObjCoordsRef);
		lua_unref(_luaTextObjCacheRef);
		lua_unref(_luaTextObjVolumeRef);
		lua_unref(_luaTextObjBufferRef);
		lua_unref(_luaTextObjBackgroundRef);
		lua_unref(_luaTextObjMirrorModeRef);
	} else {
		lua_unref(_luaTextObjXRef);
		lua_unref(_luaTextObjYRef);
		lua_unref(_luaTextObjFontRef);
		lua_unref(_luaTextObjWidthRef);
		lua_unref(_luaTextObjHeightRef);
		lua_unref(_luaTextObjFGColorRef);
		lua_unref(_luaTextObjBGColorRef);
		lua_unref(_luaTextObjFXColorRef);
		lua_unref(_luaTextObjHIColorRef);
		lua_unref(_luaTextObjDurationRef);
		lua_unref(_luaTextObjCenterRef);
		lua_unref(_luaTextObjLJustifyRef);
		lua_unref(_luaTextObjRJustifyRef);
		lua_unref(_luaTextObjVolumeRef);
		lua_unref(_luaTextObjBufferRef);
		lua_unref(_luaTextObjBackgroundRef);
	}

	while (_textQueue)
		killEvent(_textQueue);

	_started = 0;

	return 0;
}

SputTextEvent *SputText::allocEvent(const TextParams *params, const char *text) {
	SputTextEvent *event = new SputTextEvent();
	BlastTextSurface *surface = _blastText->getSurface();

	if (!text) {
		warning("SputText::allocEvent(): no text");
		text = "";
	}

	// The demo keeps only the first 127 characters
	if (_grim->getGameFlags() & ADGF_DEMO)
		Common::strlcpy(event->text, text, 128);
	else
		Common::strlcpy(event->text, text, getTextCapacity());

	event->x = params->x;
	event->y = params->y;
	event->params = *params;

	if (_emergencyFont)
		event->params.font = _emergencyFont;

	event->params.colorMap = (byte *)_sputRender->getColors();
	event->params.clip = &event->clip;

	if (params->clip) {
		event->clip = *params->clip;
	} else {
		event->clip.x = 0;
		event->clip.y = 0;
		event->clip.width = surface->width;
		event->clip.height = surface->height;
	}

	event->actor = nullptr;
	event->serial = (int)(intptr)event;

	initHdEvent(event);

	return event;
}

const char *SputText::extractSayLineData(const char *msgId, char *translation) {
	if (translation)
		*translation = 0;

	const char *translated = _polyglot->getTranslation(msgId);
	const char *result;

	if (translation && msgId && *msgId == '/') {
		// The demo takes the voice name up to the last slash
		const char *slash = (_grim->getGameFlags() & ADGF_DEMO) ? strrchr(msgId, '/') : strchr(msgId + 1, '/');

		if (slash == msgId)
			slash = nullptr;

		result = slash;

		if (slash) {
			int length = slash - msgId - 1;

			if (length > 31) {
				warning("SputText::extractSayLineData(): the voice name is too long, truncated");
				length = 31;
			}

			strncpy(translation, msgId + 1, length);
			translation[length] = 0;
		}
	} else {
		result = translated;
	}

	if (_translationMode == 1) {
		if (!result) {
			warning("SputText::extractSayLineData(): the message has no closing slash");
			return translated;
		}

		return result + 1;
	}

	if (_translationMode == 2)
		return msgId;

	return translated;
}

void SputText::drawTextEvent(SputTextEvent *event, int markDirty) {
	if (!event)
		return;

	if (_grim->isRemastered()) {
		drawHdEvent(event, HdText::getTextPage(event->params.flags));
		return;
	}

	if (!event->params.font) {
		warning("SputText::drawTextEvent(): the event has no font");
		return;
	}

	StdRect rect;
	uint16 *colors = _sputRender->getColors();
	int baseLine = event->params.font->baseLine;

	event->params.x = event->x;
	rect.x = event->x;
	event->params.y = event->y + baseLine;
	rect.y = event->params.y - baseLine;
	rect.width = _text->getStringWidth(&event->params, event->text);
	rect.height = _text->getStringHeight(&event->params, event->text);

	if (event->params.flags & 1)
		rect.x += rect.width / -2;

	int foreground = event->params.fgColor;
	int background = event->params.bgColor;
	int effect = event->params.fxColor;

	colors[0] = colorToRgb565(foreground);
	colors[1] = colorToRgb565(background);
	colors[2] = colorToRgb565(effect);

	// The demo does not apply the gamma to the text
	if (!(_grim->getGameFlags() & ADGF_DEMO))
		_sputRender->colorCorrect(colors, 3);

	event->params.fgColor = 0;
	event->params.bgColor = 1;
	event->params.fxColor = 2;

	_text->printText(&event->params, "%s", event->text);

	event->params.x = event->x;
	event->params.y = event->y + event->params.font->baseLine;

	if (markDirty)
		_sputRender->markDirtyRect(&rect, 0);

	event->params.fxColor = effect;
	event->params.bgColor = background;
	event->params.fgColor = foreground;
}

SputTextEvent *SputText::findSerialNumber(int serial) {
	SputTextEvent *event = _textQueue;

	while (event) {
		if (event->serial == serial)
			return event;

		event = event->next;
	}

	return nullptr;
}

int SputText::getTextObjectParams(TextParams *params, lua_Object tableObj, SputTextEvent *event) {
	StdRect *clip = params->clip;
	lua_Object obj;

	if (_grim->isRemastered()) {
		lua_pushobject(tableObj);
		lua_pushobject(lua_getref(_luaTextObjCoordsRef));
		obj = lua_gettable();

		if (obj && lua_isnumber(obj))
			params->coords = luaFtol(lua_getnumber(obj));
	}

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjXRef));
	obj = lua_gettable();

	if (obj && lua_isnumber(obj)) {
		int x = luaFtol(lua_getnumber(obj));

		params->x = x;

		if (event)
			event->x = x;
	}

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjYRef));
	obj = lua_gettable();

	if (obj && lua_isnumber(obj)) {
		int y = luaFtol(lua_getnumber(obj));

		if (_grim->isRemastered()) {
			params->y = y;
		} else {
			int baseLine = 0;

			if (params->font)
				baseLine = params->font->baseLine;
			else
				warning("SputText::getTextObjectParams(): no font");

			params->y = y + baseLine;
		}

		if (event)
			event->y = y;
	}

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjFontRef));
	obj = lua_gettable();

	if (obj && lua_isuserdata(obj) && lua_tag(obj) == kSputTagFont && !_emergencyFont) {
		BlastTextFont *font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(lua_getuserdata(obj)));

		if (font)
			params->font = font;
		else
			warning("SputText::getTextObjectParams(): font resource not found");
	}

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjWidthRef));
	obj = lua_gettable();

	if (obj && lua_isnumber(obj) && clip)
		clip->width = luaFtol(lua_getnumber(obj));

	if (_grim->isRemastered()) {
		lua_pushobject(tableObj);
		lua_pushobject(lua_getref(_luaTextObjLeftClipRef));
		obj = lua_gettable();

		if (obj && lua_isnumber(obj) && clip)
			clip->x = luaFtol(lua_getnumber(obj));
	}

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjHeightRef));
	obj = lua_gettable();

	if (obj && lua_isnumber(obj) && clip)
		clip->height = luaFtol(lua_getnumber(obj));

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjFGColorRef));
	obj = lua_gettable();

	if (obj && lua_isuserdata(obj) && lua_tag(obj) == MKTAG('R','L','O','C'))
		params->fgColor = lua_getuserdata(obj);

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjBGColorRef));
	obj = lua_gettable();

	if (obj && lua_isuserdata(obj) && lua_tag(obj) == MKTAG('R','L','O','C'))
		params->bgColor = lua_getuserdata(obj);

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjFXColorRef));
	obj = lua_gettable();

	if (obj && lua_isuserdata(obj) && lua_tag(obj) == MKTAG('R','L','O','C'))
		params->fxColor = lua_getuserdata(obj);

	if (_grim->isRemastered()) {
		lua_pushobject(tableObj);
		lua_pushobject(lua_getref(_luaTextObjAlphaRef));
		obj = lua_gettable();

		if (obj && !lua_isnil(obj))
			params->alpha = lua_getnumber(obj);
	}

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjCenterRef));
	obj = lua_gettable();

	if (obj && !lua_isnil(obj))
		params->flags = 5;

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjLJustifyRef));
	obj = lua_gettable();

	if (obj && !lua_isnil(obj))
		params->flags = 4;

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjRJustifyRef));
	obj = lua_gettable();

	if (obj && !lua_isnil(obj))
		params->flags = 6;

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjDurationRef));
	obj = lua_gettable();

	int duration = -1;

	if (obj && lua_isnumber(obj))
		duration = luaFtol(lua_getnumber(obj));

	if (_grim->isRemastered()) {
		lua_pushobject(tableObj);
		lua_pushobject(lua_getref(_luaTextObjLayerRef));
		obj = lua_gettable();

		if (obj && lua_isnumber(obj)) {
			int layer = luaFtol(lua_getnumber(obj));

			if (layer == 1)
				params->flags |= 0x10;
			else if (layer == 2)
				params->flags |= 0x80;
			else if (layer == 3)
				params->flags |= 0x100;
			else if (layer == 4)
				params->flags |= 0x200;
		}

		lua_pushobject(tableObj);
		lua_pushobject(lua_getref(_luaTextObjFadeRef));
		obj = lua_gettable();

		if (obj && lua_isnumber(obj)) {
			int fade = luaFtol(lua_getnumber(obj));

			params->flags &= ~0x1C00;

			if (fade == 1) {
				params->flags |= 0x400;
				params->alpha = 0.0f;
			} else if (fade == 2) {
				params->flags |= 0x800;
			} else if (fade == 3) {
				params->flags |= 0x1000;
			}
		}

		lua_pushobject(tableObj);
		lua_pushobject(lua_getref(_luaTextObjHighlightRef));
		obj = lua_gettable();

		if (obj && lua_isnumber(obj))
			params->highlight = luaFtol(lua_getnumber(obj));
	}

	return duration;
}

void SputText::parseSayLineTable(lua_Object tableObj, int *background, int *volume, int *pan, int *x, int *y) {
	lua_Object obj;

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjXRef));
	obj = lua_gettable();

	if (lua_isnumber(obj) && *x)
		*x = luaFtol(lua_getnumber(obj));

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjYRef));
	obj = lua_gettable();

	if (lua_isnumber(obj) && *y)
		*y = luaFtol(lua_getnumber(obj));

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjBackgroundRef));
	obj = lua_gettable();

	if (!lua_isnil(obj) && *background)
		*background = 0;

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjVolumeRef));
	obj = lua_gettable();

	if (lua_isnumber(obj) && *volume)
		*volume = luaFtol(lua_getnumber(obj));

	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjBufferRef));
	obj = lua_gettable();

	if (lua_isnumber(obj) && *pan)
		*pan = luaFtol(lua_getnumber(obj));
}

int SputText::save(Common::WriteStream *stream) {
	int numEvents = 0;

	for (SputTextEvent *event = _textQueue; event; event = event->next)
		numEvents++;

	stream->writeSint32LE(numEvents);

	if (stream->err())
		return 1;

	for (SputTextEvent *event = _textQueue; event; event = event->next) {
		stream->write(event->text, 512);
		stream->writeSint32LE(0);
		stream->writeSint32LE(0);
		stream->writeSint32LE(_res->serialFromHandle(event->params.font));
		stream->writeSint32LE(event->params.x);
		stream->writeSint32LE(event->params.y);
		stream->writeSint32LE(event->params.fgColor);
		stream->writeSint32LE(event->params.bgColor);
		stream->writeSint32LE(event->params.fxColor);
		stream->writeSint32LE(event->params.scaleX);
		stream->writeSint32LE(event->params.scaleY);
		stream->writeSint32LE(event->params.flags);
		stream->writeSint32LE(event->x);
		stream->writeSint32LE(event->y);
		stream->writeSint32LE(event->clip.x);
		stream->writeSint32LE(event->clip.y);
		stream->writeSint32LE(event->clip.width);
		stream->writeSint32LE(event->clip.height);
		stream->writeSint32LE(event->duration);
		stream->writeSint32LE(0);
		stream->writeSint32LE(event->flags);
		stream->writeSint32LE(event->serial);
	}

	stream->writeSint32LE(_res->serialFromHandle(_printLineParams.font));
	stream->writeSint32LE(_res->serialFromHandle(_defaultTextParams.font));
	stream->writeSint32LE(_res->serialFromHandle(_blastTextParams.font));

	if (stream->err()) {
		warning("SputText::save(): write error");
		return 1;
	}

	return 0;
}

int SputText::restore(Common::ReadStream *stream) {
	while (_textQueue)
		killEvent(_textQueue);

	int numEvents = stream->readSint32LE();

	if (stream->err() || stream->eos() || numEvents < 0) {
		warning("SputText::restore(): invalid event count");
		return 1;
	}

	for (int i = 0; i < numEvents; i++) {
		SputTextEvent *event = new SputTextEvent();

		stream->read(event->text, 512);

		stream->readSint32LE();
		stream->readSint32LE();
		int fontSerial = stream->readSint32LE();

		event->params.x = stream->readSint32LE();
		event->params.y = stream->readSint32LE();
		event->params.fgColor = stream->readSint32LE();
		event->params.bgColor = stream->readSint32LE();
		event->params.fxColor = stream->readSint32LE();
		event->params.scaleX = stream->readSint32LE();
		event->params.scaleY = stream->readSint32LE();
		event->params.flags = stream->readSint32LE();
		event->x = stream->readSint32LE();
		event->y = stream->readSint32LE();
		event->clip.x = stream->readSint32LE();
		event->clip.y = stream->readSint32LE();
		event->clip.width = stream->readSint32LE();
		event->clip.height = stream->readSint32LE();
		event->duration = stream->readSint32LE();

		stream->readSint32LE();

		event->flags = stream->readSint32LE();
		event->serial = stream->readSint32LE();

		if (stream->err() || stream->eos()) {
			warning("SputText::restore(): read error");
			delete event;
			event = nullptr;

			return 1;
		}

		if (event->text[511] != 0) {
			warning("SputText::restore(): unterminated event text");
			event->text[511] = 0;
		}

		event->params.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(fontSerial));

		event->params.surface = _blastText->getSurface();
		event->params.clip = &event->clip;
		event->params.colorMap = (byte *)_sputRender->getColors();
		event->actor = nullptr;
		initHdEvent(event);
		event->params.coords = 0;
		event->params.highlight = -1;
		event->params.alpha = 1.0f;

		event->prev = nullptr;
		event->next = _textQueue;

		if (_textQueue)
			_textQueue->prev = event;

		_textQueue = event;
	}

	int printLineFontSerial = stream->readSint32LE();
	int defaultFontSerial = stream->readSint32LE();
	int blastTextFontSerial = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputText::restore(): read error");
		return 1;
	}

	_font = _text->getSystemFont();
	_printLineParams.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(printLineFontSerial));
	_defaultTextParams.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(defaultFontSerial));
	_blastTextParams.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(blastTextFontSerial));

	if (!_printLineParams.font)
		_printLineParams.font = _font;

	if (!_defaultTextParams.font)
		_defaultTextParams.font = _font;

	if (!_blastTextParams.font)
		_blastTextParams.font = _font;

	return 0;
}

void SputText::update(int force, int page) {
	if (_grim->isRemastered())
		updateHd(force, page);
	else
		update(force);
}

int SputText::getTextCapacity() const {
	if (_grim->isRemastered())
		return kTextEventCapacity;

	return kTextEventClassicCapacity;
}

int SputText::getCoordsFromTable(lua_Object tableObj) {
	lua_pushobject(tableObj);
	lua_pushobject(lua_getref(_luaTextObjCoordsRef));

	lua_Object obj = lua_gettable();

	if (obj && lua_isnumber(obj))
		return luaFtol(lua_getnumber(obj));

	return 0;
}

void SputText::getHdClip(StdRect *clip, int coords) {
	clip->x = 240;
	clip->y = 0;
	clip->width = 1440;
	clip->height = 1080;
	HotspotManager::convertRectNative(clip, coords);
}

void SputText::initHdEvent(SputTextEvent *event) {
	event->firstLine = 0;
	event->lineCount = 999999;
	event->hdCache = nullptr;
}

void SputText::invalidateHdText(SputTextEvent *event) {
	if (!event->hdCache)
		return;

	delete event->hdCache;
	event->hdCache = nullptr;
}

bool SputText::makeHdInfo(SputTextEvent *event, HdTextInfo *info) {
	HdText *hdText = _blastText->getHdText();

	if (!hdText)
		return false;

	BlastTextFont *font = event->params.font;

	if (!font) {
		font = _text->getSystemFont();
		event->flags &= ~1;
	}

	const HdFont *hdFont = nullptr;
	int baseLine = 0;

	if (font) {
		hdFont = font->hdFont;
		baseLine = font->baseLine;
	}

	if (!hdFont)
		hdFont = hdText->getCustomFont();

	if (!hdFont)
		return false;

	int x = event->x;
	int y = event->y - baseLine;
	int coords = event->params.coords;
	StdRect rect = event->clip;

	HotspotManager::convertCoords1080(&x, &y, coords, false);
	HotspotManager::convertCoords1080(&rect.x, &rect.y, coords, false);
	HotspotManager::convertCoords1080(&rect.width, &rect.height, coords, true);

	int flags = event->params.flags;

	if (flags & 1) {
		int space = x - rect.x;

		if (rect.x + rect.width - x < space)
			space = rect.x + rect.width - x;

		info->alignX = 0.5f;
		info->maxWidth = 2 * space;
	} else if (flags & 2) {
		info->alignX = 1.0f;
		info->maxWidth = x - rect.x;
	} else {
		info->alignX = 0.0f;
		info->maxWidth = rect.x + rect.width - x;
	}

	if (flags & 0x20)
		info->alignY = 1.0f;
	else if (flags & 0x40)
		info->alignY = 0.5f;
	else
		info->alignY = 0.0f;

	info->text = event->text;
	info->font = hdFont;
	info->x = x;
	info->y = y;
	info->color = event->params.fgColor;
	info->alpha = event->params.alpha;
	info->firstLine = event->firstLine;
	info->lineCount = event->lineCount;

	return true;
}

HdTextCache *SputText::getHdLayout(SputTextEvent *event) {
	if (event->hdCache)
		return event->hdCache;

	HdTextInfo info;

	if (!makeHdInfo(event, &info))
		return nullptr;

	HdTextCache *cache = new HdTextCache();

	_blastText->getHdText()->layout(info, cache);
	event->hdCache = cache;

	return cache;
}

bool SputText::queryTextObject(SputTextEvent *event, int *x, int *y, int *width, int *height) {
	*x = 0;
	*y = 0;
	*width = 0;
	*height = 0;

	HdTextCache *cache = getHdLayout(event);
	HdCanvas *canvas = _grim->getHdCanvas();
	HdTextInfo info;

	if (!cache || !canvas || !makeHdInfo(event, &info))
		return false;

	HdText::queryText(info, cache, canvas->getTextCanvas(), x, y, width, height);

	return true;
}

int SputText::getHdLineCount(SputTextEvent *event) {
	HdTextCache *cache = getHdLayout(event);

	if (!cache)
		return 0;

	return (int)cache->rows.size();
}

int SputText::getHdLineCharacterCount(SputTextEvent *event, int line) {
	HdTextCache *cache = getHdLayout(event);

	if (!cache || line < 0 || line >= (int)cache->rows.size())
		return 0;

	return cache->rows[line].numQuads;
}

void SputText::luaGetFontDimensions() {
	lua_Object fontObj = lua_getparam(1);
	BlastTextFont *font = nullptr;

	if (lua_isuserdata(fontObj) && lua_tag(fontObj) == kSputTagFont)
		font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(lua_getuserdata(fontObj)));

	const HdFont *hdFont = font ? font->hdFont : nullptr;

	if (!hdFont) {
		warning("SputText::luaGetFontDimensions(): the font has no TrueType file");
		lua_pushnumber(0.0f);

		return;
	}

	lua_pushnumber((float)HdText::getFontHeight(hdFont));
}

void SputText::drawHdEvent(SputTextEvent *event, int page) {
	HdCanvas *canvas = _grim->getHdCanvas();

	if (!canvas || !canvas->isActive())
		return;

	HdTextCache *cache = getHdLayout(event);
	HdTextInfo info;

	if (!cache || !makeHdInfo(event, &info))
		return;

	HdTextDraw draw;

	_blastText->getHdText()->buildDraw(info, cache, canvas->getTextCanvas(), &draw);
	canvas->queueText(draw);
}

void SputText::updateHd(int force, int page) {
	if (_locked)
		return;

	int frameTime = _sputTime->getFrameTime();
	SputTextEvent *event = _textQueue;

	while (event) {
		SputTextEvent *next = event->next;
		bool deleteEvent = false;

		if (event->params.flags & 0x400) {
			event->params.alpha += 1.0f / 18.0f;

			if (event->params.alpha >= 1.0f) {
				event->params.alpha = 1.0f;
				event->params.flags &= ~0x400;
			}
		} else if (event->params.flags & 0x1800) {
			event->params.alpha -= 1.0f / 18.0f;

			if (event->params.alpha <= 0.0f) {
				event->params.alpha = 0.0f;

				if (event->params.flags & 0x1000)
					deleteEvent = true;

				event->params.flags &= ~0x1800;
			}
		}

		if ((event->duration <= 0 && (!(event->flags & 1) || deleteEvent)) || (event->duration > 0 && deleteEvent)) {
			killEvent(event);
			event = next;
			continue;
		}

		if ((force || (event->flags & 8)) && !(event->flags & 2) && HdText::getTextPage(event->params.flags) == page)
			drawHdEvent(event, page);

		event->duration -= frameTime;
		event = next;
	}
}

} // End of namespace Grim
