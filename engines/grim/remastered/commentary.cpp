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
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/remastered/commentary.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/res.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/std/std_conffile.h"
#include "engines/grim/std/std_sort.h"
#include "engines/grim/text/blasttext.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

const int Commentary::kSpeakerColors[kNumSpeakerColors] = {
	0x00288EE6, 0x00288EE6, 0x00288EE6, 0x00288EE6, 0x00288EE6,
	0x00288EE6, 0x00288EE6, 0x00288EE6, 0x00288EE6, 0x00288EE6,
	0x00288EE6, 0x00288EE6, 0x00288EE6, 0x00288EE6, 0x00288EE6
};

Commentary::Commentary(Res *res, IMuse *imuse, SputText *sputText, SputSmush *sputSmush, WuRegistry *wuRegistry, OverlayManager *overlays) :
		_res(res), _imuse(imuse), _sputText(sputText), _sputSmush(sputSmush), _wuRegistry(wuRegistry), _overlays(overlays),
		_entries(nullptr), _numEntries(0), _loaded(false), _availableIndex(-1), _currentIndex(-1), _isPlaying(false),
		_currentLineCount(0), _nextLineSwitchTime(0), _fadeBoxHandle(OverlayManager::kNoHandle), _groupVolumesCached(false) {
	_entries = new Entry[kMaxEntries]();

	memset(_heard, 0, sizeof(_heard));
	memset(_hasPlayed, 0, sizeof(_hasPlayed));
	memset(_wrappedLineCharacters, 0, sizeof(_wrappedLineCharacters));
	memset(_savedGroupVolumes, 0, sizeof(_savedGroupVolumes));
}

Commentary::~Commentary() {
	delete[] _entries;
}

int Commentary::compareEntries(const void *a, const void *b) {
	return strncmp(static_cast<const Entry *>(a)->name, static_cast<const Entry *>(b)->name, kNameSize);
}

int Commentary::compareKey(const void *key, const void *element) {
	return strncmp(static_cast<const char *>(key), static_cast<const Entry *>(element)->name, kNameSize);
}

void Commentary::startup() {
	memset(_heard, 0, sizeof(_heard));
}

void Commentary::shutdown() {
	for (int i = 0; i < kMaxEntries; i++) {
		for (int k = 0; k < kMaxSubtitles; k++)
			_entries[i].subtitles[k].eventSerial = 0;
	}

	_currentIndex = -1;
}

void Commentary::loadDefinitions() {
	StdConffile conffile(g_grim);
	Entry *entry = nullptr;
	int state = kStateName;
	int count = 0;
	int subtitleIndex = 0;
	bool done = false;

	if (_loaded)
		return;

	_loaded = true;
	_numEntries = 0;

	if (!conffile.open("commentary_def.txt")) {
		warning("Commentary::loadDefinitions(): commentary_def.txt not found");
		return;
	}

	while (!done && conffile.readLine()) {
		const char *line = conffile.getLine();

		switch (state) {
		case kStateName: {
			char name[kNameSize + 1];

			if (sscanf(line, "%32s", name) != 1)
				break;

			if (strlen(name) >= kNameSize)
				warning("Commentary::loadDefinitions(): line %d: the name %s is too long", conffile.getLineNumber(), name);

			if (_numEntries >= kMaxEntries) {
				warning("Commentary::loadDefinitions(): more than %d entries, the rest is ignored", (int)kMaxEntries);
				done = true;
				break;
			}

			entry = &_entries[_numEntries];
			memset(entry, 0, sizeof(Entry));
			Common::strlcpy(entry->name, name, sizeof(entry->name));
			state = kStateFile;
			break;
		}
		case kStateFile: {
			char audioFile[kNameSize + 1];

			if (sscanf(line, "%32s", audioFile) == 1) {
				if (strlen(audioFile) >= kNameSize)
					warning("Commentary::loadDefinitions(): line %d: the file name %s is too long", conffile.getLineNumber(), audioFile);

				Common::strlcpy(entry->audioFile, audioFile, sizeof(entry->audioFile));
				state = kStateCount;
			}

			break;
		}
		case kStateCount:
			count = 0;

			if (sscanf(line, "%d", &count) != 1 || count < 0) {
				warning("Commentary::loadDefinitions(): line %d: the number of subtitles of %s is not valid", conffile.getLineNumber(), entry->name);
				count = 0;
			}

			subtitleIndex = 0;

			if (count == 0) {
				_numEntries++;
				state = kStateName;
			} else {
				state = kStateSubtitle;
			}

			break;
		case kStateSubtitle:
			if (subtitleIndex < kMaxSubtitles) {
				Subtitle *subtitle = &entry->subtitles[subtitleIndex];
				char stringId[kNameSize + 1];

				stringId[0] = 0;

				if (sscanf(line, "%d %32s %d %d", &subtitle->speaker, stringId, &subtitle->startMs, &subtitle->endMs) != 4)
					warning("Commentary::loadDefinitions(): line %d: the subtitle is not valid", conffile.getLineNumber());

				if (strlen(stringId) >= kNameSize)
					warning("Commentary::loadDefinitions(): line %d: the string %s is too long", conffile.getLineNumber(), stringId);

				Common::strlcpy(subtitle->stringId, stringId, sizeof(subtitle->stringId));
				entry->numSubtitles = subtitleIndex + 1;
			} else if (subtitleIndex == kMaxSubtitles) {
				warning("Commentary::loadDefinitions(): %s has more than %d subtitles, the rest is ignored", entry->name, (int)kMaxSubtitles);
			}

			subtitleIndex++;

			if (subtitleIndex >= count) {
				_numEntries++;
				state = kStateName;
			}

			break;
		default:
			break;
		}
	}

	conffile.close();

	if (_numEntries > 1)
		Grim::sort(_entries, _numEntries, sizeof(Entry), compareEntries);

	for (int i = 0; i < _numEntries; i++)
		_entries[i].index = i;
}

Commentary::Entry *Commentary::findEntry(const char *name) const {
	char key[kNameSize + 1];

	memset(key, 0, sizeof(key));
	strncpy(key, name, kNameSize);

	for (int i = 0; i < kNameSize; i++) {
		if (key[i] >= 'A' && key[i] <= 'Z')
			key[i] += 0x20;
	}

	return static_cast<Entry *>(Grim::binarySearch(key, _entries, _numEntries, sizeof(Entry), compareKey));
}

bool Commentary::isValidIndex(int index) const {
	return index >= 0 && index < kMaxEntries;
}

void Commentary::setCommentary(const char *name) {
	loadDefinitions();

	Entry *entry = findEntry(name);

	_availableIndex = entry ? entry->index : -1;
}

void Commentary::clearCommentary() {
	_availableIndex = -1;
}

bool Commentary::hasHeardCommentary(const char *name) {
	loadDefinitions();

	Entry *entry = findEntry(name);

	if (!entry || entry->index == -1)
		return false;

	return ((_heard[entry->index / 32] >> (entry->index & 31)) & 1) != 0;
}

bool Commentary::isGamePaused() const {
	lua_Object pauser = lua_getglobal("game_pauser");

	if (!lua_istable(pauser)) {
		warning("Commentary::isGamePaused(): game_pauser is not a table");
		return false;
	}

	lua_Object paused = lua_getfield(pauser, "is_paused");

	return !lua_isnil(paused) && luaFtol(lua_getnumber(paused)) != 0;
}

BlastTextFont *Commentary::getTalkFont() const {
	lua_Object fontObj = lua_getglobal("talk_font");

	if (!lua_isuserdata(fontObj) || lua_tag(fontObj) != kSputTagFont)
		return nullptr;

	return static_cast<BlastTextFont *>(_res->handleFromSerialNumber(lua_getuserdata(fontObj)));
}

void Commentary::notifyLua(int playing) const {
	LuaObjects objects;

	objects.add(playing);
	LuaBase::instance()->callback("NotifyCommentary", objects);
}

void Commentary::cacheGroupVolumes() {
	_groupVolumesCached = true;

	for (int group = 0; group < kNumGroups; group++)
		_savedGroupVolumes[group] = _imuse->imSetGroupVol(group, -1);

	if (_savedGroupVolumes[IMUSE_GROUP_MUSIC] > 0)
		_imuse->imSetGroupVol(IMUSE_GROUP_MUSIC, kDuckedVolume);

	if (_savedGroupVolumes[IMUSE_GROUP_SPEECH] > 0)
		_imuse->imSetGroupVol(IMUSE_GROUP_SPEECH, kDuckedVolume);

	if (_savedGroupVolumes[IMUSE_GROUP_SFX] > 0)
		_imuse->imSetGroupVol(IMUSE_GROUP_SFX, kDuckedVolume);

	if (_sputSmush->isHdMoviePlaying())
		_sputSmush->setCommentaryDucking(true);
}

void Commentary::restoreGroupVolumes() {
	if (!_groupVolumesCached)
		return;

	_sputSmush->setCommentaryDucking(false);

	for (int group = 0; group < kNumGroups; group++) {
		if (_imuse->imSetGroupVol(group, -1) == kDuckedVolume)
			_imuse->imSetGroupVol(group, _savedGroupVolumes[group]);
	}

	memset(_savedGroupVolumes, 0, sizeof(_savedGroupVolumes));
	_groupVolumesCached = false;
}

void Commentary::playCurrentCommentary() {
	loadDefinitions();

	if (_isPlaying) {
		stopImmediately();
		return;
	}

	if (_availableIndex == -1)
		return;

	if (_wuRegistry->getInt("DirectorsCommentary", 0) == 0)
		return;

	stopImmediately();

	if (!isValidIndex(_availableIndex)) {
		warning("Commentary::playCurrentCommentary(): commentary %d does not exist", _availableIndex);
		return;
	}

	_currentIndex = _availableIndex;

	Entry *entry = &_entries[_currentIndex];
	int sound = _imuse->soundsLoadStartSound(entry->audioFile, kSoundPriority, kGroupCommentary);

	if (sound <= 0) {
		warning("Commentary::playCurrentCommentary(): cannot start %s", entry->audioFile);
		_currentIndex = -1;
		return;
	}

	cacheGroupVolumes();

	_isPlaying = true;

	OverlayProps props;

	props.layer = isGamePaused() ? 20 : 1;
	props.coords = OverlayManager::kCoordsGlobal;
	props.mirror = 0;
	props.cache = OverlayManager::kCacheDefault;
	props.width = 3840;
	props.height = 300;
	_fadeBoxHandle = _overlays->create("fadebox-top.png", 0, 0, props);

	notifyLua(1);

	Entry *heard = findEntry(entry->name);

	if (heard && heard->index != -1)
		_heard[heard->index / 32] |= 1u << (heard->index & 31);
}

void Commentary::stopImmediately() {
	if (_currentIndex != -1) {
		Entry *entry = &_entries[_currentIndex];

		_imuse->soundsStopSound(entry->audioFile);

		for (int i = 0; i < entry->numSubtitles; i++)
			killSubtitleEvent(&entry->subtitles[i]);
	}

	if (_fadeBoxHandle >= 0) {
		_overlays->destroy(_fadeBoxHandle);
		_fadeBoxHandle = OverlayManager::kNoHandle;
	}

	_currentIndex = -1;
	_isPlaying = false;
	memset(_hasPlayed, 0, sizeof(_hasPlayed));

	restoreGroupVolumes();

	notifyLua(0);
}

int Commentary::getVolume() const {
	return _imuse->imSetGroupVol(kGroupCommentary, -1);
}

void Commentary::setVolume(int volume) {
	_imuse->imSetGroupVol(kGroupCommentary, volume);
}

int Commentary::getWrappedLineCharacters(int line) const {
	if (line < 0 || line >= kMaxWrappedLines) {
		warning("Commentary::getWrappedLineCharacters(): line %d is out of range", line);
		return 0;
	}

	return _wrappedLineCharacters[line];
}

void Commentary::killSubtitleEvent(Subtitle *subtitle) {
	if (subtitle->eventSerial == 0)
		return;

	SputTextEvent *event = _sputText->findSerialNumber(subtitle->eventSerial);

	if (event)
		_sputText->killEvent(event);

	subtitle->eventSerial = 0;
}

SputTextEvent *Commentary::createSubtitleEvent(const char *text, int color, int x, int y, int time, int flags) {
	StdRect clip;

	clip.x = 40;
	clip.y = 0;
	clip.width = 530;
	clip.height = 480;

	TextParams params = *_sputText->getDefaultTextParams();

	params.clip = &clip;
	params.font = getTalkFont();
	params.x = x;
	params.y = y;
	params.fgColor = color;
	params.bgColor = 0;
	params.fxColor = 0;
	params.scaleX = 0x10000;
	params.scaleY = 0x10000;
	params.flags = 4;
	params.colorMap = nullptr;
	params.coords = 0;
	params.highlight = -1;
	params.alpha = 1.0f;

	if (isGamePaused())
		params.flags |= 0x10;

	SputTextEvent *event = _sputText->allocEvent(&params, text);

	if (!event->params.font) {
		warning("Commentary::createSubtitleEvent(): there is no talk font, the subtitle is not shown");
		_sputText->killEvent(event);
		return nullptr;
	}

	event = _sputText->addToQueue(event, time, flags);

	_currentLineCount = _sputText->getHdLineCount(event);
	event->lineCount = kLinesPerPage;

	memset(_wrappedLineCharacters, 0, sizeof(_wrappedLineCharacters));

	for (int line = 0; line < kMaxWrappedLines; line++)
		_wrappedLineCharacters[line] = _sputText->getHdLineCharacterCount(event, line);

	return event;
}

// The ratio is stored as a float and the product is made in double precision
int Commentary::getPageTime(int subtitleIndex, const SputTextEvent *event) const {
	if (_currentIndex == -1)
		return 0;

	const Entry *entry = &_entries[_currentIndex];

	if (entry->numSubtitles <= subtitleIndex)
		return 0;

	const Subtitle *subtitle = &entry->subtitles[subtitleIndex];
	uint32 length = (uint32)strlen(event->text);

	if (length == 0) {
		warning("Commentary::getPageTime(): the subtitle has no text");
		return 0;
	}

	uint32 characters = (uint32)(getWrappedLineCharacters(event->firstLine) + getWrappedLineCharacters(event->firstLine + 1));
	float ratio = (float)((double)characters / (double)length);

	return (int)((double)ratio * (double)(subtitle->endMs - subtitle->startMs));
}

void Commentary::update() {
	if (_currentIndex == -1 || !_isPlaying)
		return;

	Entry *entry = &_entries[_currentIndex];
	int position = _imuse->soundsGetParam(entry->audioFile, IMUSE_P_SND_POS_IN_MS);

	if (position < 0) {
		stopImmediately();
		return;
	}

	for (int i = 0; i < entry->numSubtitles; i++) {
		Subtitle *subtitle = &entry->subtitles[i];

		if (_hasPlayed[i] || position < subtitle->startMs)
			continue;

		if (subtitle->eventSerial == 0) {
			char voiceName[kNameSize];
			const char *text = _sputText->extractSayLineData(subtitle->stringId, voiceName);
			int speaker = subtitle->speaker;

			if (!text) {
				warning("Commentary::update(): the subtitle %s has no text", subtitle->stringId);
				text = "";
			}

			if (speaker < 0 || speaker >= kNumSpeakerColors) {
				warning("Commentary::update(): the speaker %d has no color", speaker);
				speaker = kNumSpeakerColors - 1;
			}

			SputTextEvent *event = createSubtitleEvent(text, kSpeakerColors[speaker], 40, 20, 0, 1);

			if (!event) {
				_hasPlayed[i] = true;
				continue;
			}

			subtitle->eventSerial = event->serial;

			if ((uint)_currentLineCount < 3)
				_nextLineSwitchTime = 0;
			else
				_nextLineSwitchTime = subtitle->startMs + getPageTime(i, event);
		} else if (position <= subtitle->endMs) {
			SputTextEvent *event = _sputText->findSerialNumber(subtitle->eventSerial);

			if (!event) {
				warning("Commentary::update(): the event of the subtitle %s was removed", subtitle->stringId);
				subtitle->eventSerial = 0;
				_hasPlayed[i] = true;
			} else if ((uint)(_nextLineSwitchTime - 1) < (uint)position) {
				event->firstLine += kLinesPerPage;
				_nextLineSwitchTime += getPageTime(i, event);
			}
		} else {
			killSubtitleEvent(subtitle);
			_hasPlayed[i] = true;
		}
	}
}

void Commentary::saveRemastered(Common::WriteStream *stream) const {
	stream->writeSint32LE(_availableIndex);

	for (int i = 0; i < kNumHeardWords; i++)
		stream->writeUint32LE(_heard[i]);
}

bool Commentary::restoreRemastered(Common::ReadStream *stream) {
	_availableIndex = stream->readSint32LE();

	for (int i = 0; i < kNumHeardWords; i++)
		_heard[i] = stream->readUint32LE();

	return !stream->err() && !stream->eos();
}

} // End of namespace Grim
