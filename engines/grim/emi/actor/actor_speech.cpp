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
#include "common/textconsole.h"
#include "common/ustr.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/sound/lipdata.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/emi/text/cjkstring.h"
#include "engines/grim/emi/text/textevent.h"

namespace Grim {

static const int kVoiceIdSize = TextEvent::kTagBufferSize;
static const int kVoicePathSize = 36;
static const int kVoiceSoundMode = 2;
static const int kMouthSlot = 4;
static const int kFirstVisemeSlot = 6;
static const int kLastVisemeSlot = 14;
static const float kVisemeFadeTime = 0.0625f;
static const int kSpeechTextFlags = 13;
static const int kMinTextRate = 1;
static const int kMaxTextRate = 10;
static const int kPs2TextMinX = 160;
static const int kPs2TextMaxX = 480;
static const int kPs2TextMinY = 32;
static const int kPs2TextMaxY = 436;
static const int kTextMarginX = 40;
static const int kTextMarginY = 25;

static int truncateSpeechTime(double value) {
	if (!(value > -2147483649.0 && value < 2147483648.0)) {
		warning("Actor::updateActorSpeech(): the time is out of range");

		return -2147483647 - 1;
	}

	return (int)value;
}

void Actor::updateActorSpeech(float deltaTime) {
	if (!_extra) {
		warning("Actor::updateActorSpeech(): missing extra data");

		return;
	}

	if (!_extra->_talking)
		return;

	if (_extra->_soundHandle && _extra->_lipsyncActive) {
		int mouthShape;

		if (_engine->isDumpingFrames()) {
			mouthShape = LipData::getLipMouthShape(_engine, (uint)_extra->_soundHandle, _extra->_visemeMs);
			_extra->_visemeMs = truncateSpeechTime((double)deltaTime * 1000.0 + (double)_extra->_visemeMs);
		} else {
			mouthShape = LipData::getLipMouthShape(_engine, (uint)_extra->_soundHandle, -1);
		}

		if (mouthShape == 0) {
			shutUp(false);
		} else if (mouthShape < 0 || mouthShape >= kNumChoreSlots) {
			warning("Actor::updateActorSpeech(): invalid mouth shape %d", mouthShape);
		} else if (mouthShape != _extra->_visemeCurrent) {
			int currentShape = _extra->_visemeCurrent;

			if (currentShape != 0) {
				if (currentShape < 0 || currentShape >= kNumChoreSlots) {
					warning("Actor::updateActorSpeech(): invalid current mouth shape %d", currentShape);
				} else {
					if (ChoreInstance::isInstanceValid(_engine, _choreSlotIds[currentShape])) {
						if (_engine->isDemo())
							ChoreInstance::getInstance(_engine, _choreSlotIds[currentShape])->stop();
						else
							ChoreInstance::getInstance(_engine, _choreSlotIds[currentShape])->fadeOut(kVisemeFadeTime);
					}

					_choreSlotIds[currentShape] = 0;
				}
			}

			uint startedId;

			startChore(&startedId, _choreSlotNames[mouthShape], _choreSlotSetNames[mouthShape]);
			_choreSlotIds[mouthShape] = startedId;

			if (ChoreInstance::isInstanceValid(_engine, startedId)) {
				if (_engine->isDemo())
					ChoreInstance::getInstance(_engine, _choreSlotIds[mouthShape])->setPlaying();
				else
					ChoreInstance::getInstance(_engine, _choreSlotIds[mouthShape])->fadeIn(kVisemeFadeTime);
			}

			_extra->_visemeCurrent = mouthShape;
		}
	}

	int elapsedMs = truncateSpeechTime((double)deltaTime * -1000.0);

	_extra->_talkTimerMs = (int)((uint)_extra->_talkTimerMs + (uint)elapsedMs);

	if ((_extra->_soundHandle && !isVoicePlaying()) || (!_extra->_soundHandle && _extra->_talkTimerMs < 0))
		shutUp(false);
}

void Actor::sayLine(const char *text, int volume, int pan, int x, int y) {
	if (!text) {
		warning("Actor::sayLine(): the text is null");

		return;
	}

	if (!_extra)
		allocateActorExtra();

	bool textEnabled = (_engine->_speechMode & 1) != 0;
	bool voiceEnabled = (_engine->_speechMode & 2) != 0;
	char voiceId[kVoiceIdSize];
	char voicePath[kVoicePathSize];
	const char *lineText = nullptr;
	CjkString cjkLine;

	if (_engine->isCjkMode())
		cjkLine = TextEvent::translateCjk(_engine, text, voiceId);
	else
		lineText = TextEvent::translate(_engine, text, voiceId);

	voicePath[0] = 0;

	if (voiceId[0] && _engine->getGamePlatform() == Common::kPlatformPS2) {
		Common::sprintf_s(voicePath, "%s.SCX", voiceId);

		if (!_engine->getFileServices()->exists(voicePath))
			voicePath[0] = 0;
	} else if (voiceId[0]) {
		Common::sprintf_s(voicePath, "%s.wvc", voiceId);

		LFile *voiceFile = _engine->getFileServices()->open(voicePath, "rb");

		if (!voiceFile) {
			Common::sprintf_s(voicePath, "%s.wav", voiceId);
			voiceFile = _engine->getFileServices()->open(voicePath, "rb");
		}

		if (voiceFile)
			_engine->getFileServices()->close(voiceFile, true);
		else
			voicePath[0] = 0;
	}

	if (_extra->_talking) {
		if (voiceId[0]) {
			if (!memchr(_extra->_sayLineId, 0, Extra_v2::kSayLineIdSize))
				warning("Actor::sayLine(): the current line id is not terminated");
			else if (strcmp(_extra->_sayLineId, voiceId) == 0)
				return;
		}

		shutUp(true);
	}

	_extra->_talkTimerMs = 0;
	_extra->_talking = true;

	int idLength = 0;

	while (idLength < Extra_v2::kSayLineIdSize && voiceId[idLength]) {
		_extra->_sayLineId[idLength] = voiceId[idLength];
		idLength++;
	}

	while (idLength < Extra_v2::kSayLineIdSize) {
		_extra->_sayLineId[idLength] = 0;
		idLength++;
	}

	_engine->_numSpeakers++;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		bool assigned = false;

		for (int slot = 0; slot < kNumSpeakers; slot++) {
			Actor *speaker = _engine->_speakers[slot];

			if (speaker) {
				if (speaker->_extra && speaker->_extra->_talking)
					speaker->shutUp(true);
			} else if (!assigned) {
				_engine->_speakers[slot] = this;
				assigned = true;
			}
		}
	} else {
		int speakerSlot = 0;

		while (speakerSlot < kNumSpeakers && _engine->_speakers[speakerSlot])
			speakerSlot++;

		if (speakerSlot < kNumSpeakers)
			_engine->_speakers[speakerSlot] = this;
	}

	bool createText = textEnabled;

	if (voiceEnabled) {
		if (voicePath[0]) {
			_extra->_soundHandle = (int)_engine->getSounds()->loadSound(voicePath, kVoiceSoundMode);

			if (_extra->_soundHandle)
				_engine->getSounds()->startSound((uint)_extra->_soundHandle, 0, false, volume);

			_extra->_lipsyncActive = _engine->getSounds()->getSoundLipData((uint)_extra->_soundHandle) != nullptr;

			if (_extra->_lipsyncActive)
				_extra->_visemeMs = 0;
		}

		_extra->_talkTimerMs = -1;

		if (!_extra->_soundHandle)
			createText = true;
	}

	if (createText) {
		TextEvent *textEvent;
		uint lineLength;

		if (_engine->isCjkMode()) {
			textEvent = createActorText(cjkLine, x, y);
			lineLength = cjkLine.size();
		} else {
			LString lineString(lineText);

			textEvent = createActorText(lineString, x, y);
			lineLength = (uint)strlen(lineText);
		}

		_extra->_textEventId = textEvent->_id;

		if (!_extra->_soundHandle)
			_extra->_talkTimerMs = (int)(10u * ((uint)(11 - _engine->_actorTextRate) * lineLength + 64u));

		textEvent->activate(-1.0f, kSpeechTextFlags);
	}

	if (_extra->_lipsyncActive) {
		int visemeSlot = kFirstVisemeSlot;

		while (visemeSlot <= kLastVisemeSlot && _choreSlotNames[visemeSlot].empty())
			visemeSlot++;

		if (visemeSlot > kLastVisemeSlot)
			_extra->_lipsyncActive = false;
	}

	if (!_extra->_lipsyncActive) {
		uint startedId;

		startChore(&startedId, _choreSlotNames[kMouthSlot], _choreSlotSetNames[kMouthSlot]);
		_choreSlotIds[kMouthSlot] = startedId;

		if (ChoreInstance::isInstanceValid(_engine, startedId))
			ChoreInstance::getInstance(_engine, _choreSlotIds[kMouthSlot])->setPlaying();
	}
}

TextEvent *Actor::createActorText(const LString &text, int x, int y) {
	TextEvent *textEvent = new TextEvent(_engine, text);

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		if (_extra)
			textEvent->_color = _extra->_talkColor;
		else
			warning("Actor::createActorText(): missing extra data");

		textEvent->_justify = 6;
		textEvent->_geometryReady = false;

		int rect[4];

		if (!_onScreen) {
			rect[0] = _engine->_offscreenTextRect.x;
			rect[1] = _engine->_offscreenTextRect.y;
			rect[2] = _engine->_offscreenTextRect.w;
			rect[3] = _engine->_offscreenTextRect.h;
		} else {
			rect[0] = _drawnRect[0];
			rect[1] = _drawnRect[1];
			rect[2] = _drawnRect[2];
			rect[3] = _drawnRect[3];
		}

		int textX = x;

		if (x == -1)
			textX = rect[0] + rect[2] / 2;

		if (textX < kPs2TextMinX)
			textX = kPs2TextMinX;

		if (textX > kPs2TextMaxX)
			textX = kPs2TextMaxX;

		textEvent->setPosition(textX, kPs2TextMinY);

		int height = textEvent->getHeight();
		int textY = y;

		if (y == -1)
			textY = rect[1] - height;

		if (textY < kPs2TextMinY)
			textY = kPs2TextMinY;

		if (textY + height > kPs2TextMaxY)
			textY = kPs2TextMaxY - height;

		textEvent->setPosition(textX, textY);
		textEvent->_actorId = _id;
		_actorFlags |= 4;

		return textEvent;
	}

	textEvent->_geometryReady = false;
	textEvent->_clipRect[0] = _engine->_textScreenRect[0];
	textEvent->_clipRect[1] = _engine->_textScreenRect[1];
	textEvent->_clipRect[2] = _engine->_textScreenRect[2];
	textEvent->_clipRect[3] = _engine->_textScreenRect[3];

	if (_extra)
		textEvent->_color = _extra->_talkColor;
	else
		warning("Actor::createActorText(): missing extra data");

	textEvent->_justify = 6;
	textEvent->_geometryReady = false;

	int rectX;
	int rectY;
	int rectWidth;

	if (!_onScreen || _engine->getMode() == EmiEngine::kModeMovie) {
		rectX = _engine->_offscreenTextRect.x;
		rectY = _engine->_offscreenTextRect.y;
		rectWidth = _engine->_offscreenTextRect.w;
	} else {
		rectX = _drawnRect[0];
		rectY = _drawnRect[1];
		rectWidth = _drawnRect[2];
	}

	int halfWidth = textEvent->getWidth() / 2;
	int textX = x;

	if (x == -1)
		textX = rectX + rectWidth / 2;

	if (textX - halfWidth < kTextMarginX)
		textX = halfWidth + kTextMarginX;

	if (textX + halfWidth > _engine->_textScreenRect[2] - kTextMarginX)
		textX = _engine->_textScreenRect[2] - halfWidth - kTextMarginX;

	textEvent->_x = textX;
	textEvent->_y = 0;
	textEvent->_geometryReady = false;

	int height = textEvent->getHeight();
	int textY = y;

	if (y == -1)
		textY = rectY - height;

	if (textY < kTextMarginY)
		textY = kTextMarginY;

	if (textY + height > _engine->_textScreenRect[3] - kTextMarginY)
		textY = _engine->_textScreenRect[3] - height - kTextMarginY;

	textEvent->_x = textX;
	textEvent->_y = textY;
	textEvent->_geometryReady = false;
	textEvent->_actorId = _id;
	_actorFlags |= 4;

	return textEvent;
}

TextEvent *Actor::createActorText(const CjkString &text, int x, int y) {
	TextEvent *textEvent = new TextEvent(_engine, text);

	// Unlike the English one, this does not copy the text screen rect into the clip rect:
	// the event keeps the default clip rect
	textEvent->_geometryReady = false;

	if (_extra)
		textEvent->_color = _extra->_talkColor;
	else
		warning("Actor::createActorText(): missing extra data");

	textEvent->_justify = 6;
	textEvent->_geometryReady = false;

	int rectX;
	int rectY;
	int rectWidth;

	if (!_onScreen || _engine->getMode() == EmiEngine::kModeMovie) {
		rectX = _engine->_offscreenTextRect.x;
		rectY = _engine->_offscreenTextRect.y;
		rectWidth = _engine->_offscreenTextRect.w;
	} else {
		rectX = _drawnRect[0];
		rectY = _drawnRect[1];
		rectWidth = _drawnRect[2];
	}

	int halfWidth = textEvent->getWidth() / 2;
	int textX = x;

	if (x == -1)
		textX = rectX + rectWidth / 2;

	if (textX - halfWidth < kTextMarginX)
		textX = halfWidth + kTextMarginX;

	if (textX + halfWidth > _engine->_textScreenRect[2] - kTextMarginX)
		textX = _engine->_textScreenRect[2] - halfWidth - kTextMarginX;

	textEvent->_x = textX;
	textEvent->_y = 0;
	textEvent->_geometryReady = false;

	int height = textEvent->getHeight();
	int textY = y;

	if (y == -1)
		textY = rectY - height;

	if (textY < kTextMarginY)
		textY = kTextMarginY;

	if (textY + height > _engine->_textScreenRect[3] - kTextMarginY)
		textY = _engine->_textScreenRect[3] - height - kTextMarginY;

	textEvent->_x = textX;
	textEvent->_y = textY;
	textEvent->_geometryReady = false;
	textEvent->_actorId = _id;
	_actorFlags |= 4;

	return textEvent;
}

void Actor::shutUp(bool stopNow) {
	if (!_extra)
		return;

	if (_extra->_soundHandle) {
		if (_engine->isDemo()) {
			// The demo has a single mode: stop the playback, then release the sound
			_engine->getSounds()->stopSoundPlayback((uint)_extra->_soundHandle);
			_engine->getSounds()->releaseSoundWhenDone((uint)_extra->_soundHandle);
		} else if (stopNow) {
			_engine->getSounds()->stopSound((uint)_extra->_soundHandle);
		} else {
			_engine->getSounds()->releaseSoundWhenDone((uint)_extra->_soundHandle);
		}

		_extra->_soundHandle = 0;
	}

	if (_extra->_textEventId) {
		TextEvent *textEvent = TextEvent::getEvent(_engine, _extra->_textEventId);

		if (textEvent)
			delete textEvent;
	}

	if (ChoreInstance::isInstanceValid(_engine, _choreSlotIds[kMouthSlot]))
		ChoreInstance::getInstance(_engine, _choreSlotIds[kMouthSlot])->stop();

	_choreSlotIds[kMouthSlot] = 0;

	int currentShape = _extra->_visemeCurrent;

	if (currentShape != 0) {
		if (currentShape < 0 || currentShape >= kNumChoreSlots) {
			warning("Actor::shutUp(): invalid current mouth shape %d", currentShape);
		} else {
			if (ChoreInstance::isInstanceValid(_engine, _choreSlotIds[currentShape])) {
				if (_engine->isDemo())
					ChoreInstance::getInstance(_engine, _choreSlotIds[currentShape])->stop();
				else
					ChoreInstance::getInstance(_engine, _choreSlotIds[currentShape])->fadeOut(kVisemeFadeTime);
			}

			_choreSlotIds[currentShape] = 0;
		}

		_extra->_visemeCurrent = 0;
	}

	for (int slot = kFirstVisemeSlot; slot <= kLastVisemeSlot && !_engine->isDemo(); slot++) {
		if (ChoreInstance::isInstanceValid(_engine, _choreSlotIds[slot])) {
			// The disasm passes the string object itself as the %s argument, let's use the name...
			Common::String message = Common::String::format("%s still talk choring", _actorName.c_str());

			warning("%s", message.c_str());
			_engine->showMessage(message.c_str(), "Warning:");
		}
	}

	for (int i = 0; i < kNumSpeakers; i++) {
		if (_engine->_speakers[i] == this)
			_engine->_speakers[i] = nullptr;
	}

	_extra->_lipsyncActive = false;
	_extra->_textEventId = 0;
	_extra->_sayLineId[0] = 0;
	_extra->_talking = false;

	if (_engine->_numSpeakers > 0)
		_engine->_numSpeakers--;
}

bool Actor::isVoicePlaying() {
	if (!_extra) {
		warning("Actor::isVoicePlaying(): missing extra data");

		return false;
	}

	if (!_extra->_soundHandle)
		return false;

	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return _engine->getSounds()->isSoundPlayingPs2((uint)_extra->_soundHandle, _actorName.c_str());

	if (_engine->isDemo())
		return _engine->getSounds()->isSoundPlaying((uint)_extra->_soundHandle);

	return _engine->getSounds()->isSoundPlayingNoFiniteLoop((uint)_extra->_soundHandle);
}

int Actor::getNumActorsTalking(EmiEngine *engine) {
	return engine->_numSpeakers;
}

int Actor::setSpeechMode(EmiEngine *engine, int newSpeechMode) {
	if (newSpeechMode != engine->_speechMode && (newSpeechMode & 3) != 0)
		engine->_speechMode = newSpeechMode;

	return newSpeechMode;
}

int Actor::getSpeechMode(EmiEngine *engine) {
	return engine->_speechMode;
}

int Actor::setActorTextRate(EmiEngine *engine, int newTextRate) {
	if (newTextRate < kMinTextRate)
		newTextRate = kMinTextRate;
	else if (newTextRate > kMaxTextRate)
		newTextRate = kMaxTextRate;

	engine->_actorTextRate = newTextRate;

	return newTextRate;
}

int Actor::getActorTextRate(EmiEngine *engine) {
	return engine->_actorTextRate;
}

} // End of namespace Grim
