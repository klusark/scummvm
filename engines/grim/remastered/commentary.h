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

#ifndef GRIM_REMASTERED_COMMENTARY_H
#define GRIM_REMASTERED_COMMENTARY_H

#include "common/scummsys.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class IMuse;
class OverlayManager;
class Res;
class SputSmush;
class SputText;
class WuRegistry;
struct BlastTextFont;
struct SputTextEvent;

/**
 * The director's commentary: the definitions of commentary_def.txt, the sound of a commentary
 * (an iMUSE sound in its own volume group), the timed subtitles, the dark strip at the top of the
 * screen and the 7 Lua functions that drive them. The scripts choose the commentary of a room
 * with SetCommentary and the player starts or stops it from the on-screen button.
 */
class Commentary {
public:
	enum {
		kMaxEntries = 250,
		kMaxSubtitles = 25,
		kMaxWrappedLines = 20,
		kNameSize = 32,
		kNumGroups = 6,
		kNumHeardWords = 8
	};

	// A subtitle line of an entry, shown between its start and its end (milliseconds of the sound)
	struct Subtitle {
		char stringId[kNameSize];
		int speaker;
		int startMs;
		int endMs;
		int eventSerial;
	};

	struct Entry {
		char name[kNameSize];
		char audioFile[kNameSize];
		int numSubtitles;
		int index;
		Subtitle subtitles[kMaxSubtitles];
	};

	Commentary(Res *res, IMuse *imuse, SputText *sputText, SputSmush *sputSmush, WuRegistry *wuRegistry, OverlayManager *overlays);
	~Commentary();

	// Reads commentary_def.txt, once
	void loadDefinitions();

	// The module hooks of a new game: the heard commentaries are forgotten, the subtitles of the
	// entries are not shown anymore
	void startup();
	void shutdown();

	// The Lua functions
	void setCommentary(const char *name);
	void clearCommentary();
	void playCurrentCommentary();
	void stopImmediately();
	bool hasHeardCommentary(const char *name);

	// True from the start of a commentary until it ends or is stopped
	bool isPlaying() const { return _isPlaying; }

	// Volume of the commentary group of iMUSE, 0 to 127
	int getVolume() const;
	void setVolume(int volume);

	// Once per frame, after the actors
	void update();

	// The part of a savegame: the commentary that is set and the bits of the ones already heard
	void saveRemastered(Common::WriteStream *stream) const;
	bool restoreRemastered(Common::ReadStream *stream);

private:
	enum {
		kGroupCommentary = 5,
		kSoundPriority = 124,
		kDuckedVolume = 5,
		kLinesPerPage = 2,
		kNumSpeakerColors = 15
	};

	enum {
		kStateName,
		kStateFile,
		kStateCount,
		kStateSubtitle
	};

	static const int kSpeakerColors[kNumSpeakerColors];

	static int compareEntries(const void *a, const void *b);
	static int compareKey(const void *key, const void *element);

	Entry *findEntry(const char *name) const;
	bool isValidIndex(int index) const;
	bool isGamePaused() const;
	BlastTextFont *getTalkFont() const;
	void notifyLua(int playing) const;
	void cacheGroupVolumes();
	void restoreGroupVolumes();
	SputTextEvent *createSubtitleEvent(const char *text, int color, int x, int y, int time, int flags);
	int getPageTime(int subtitleIndex, const SputTextEvent *event) const;
	void killSubtitleEvent(Subtitle *subtitle);
	int getWrappedLineCharacters(int line) const;

	Res *_res;
	IMuse *_imuse;
	SputText *_sputText;
	SputSmush *_sputSmush;
	WuRegistry *_wuRegistry;
	OverlayManager *_overlays;

	Entry *_entries;
	int _numEntries;
	bool _loaded;

	int _availableIndex;
	int _currentIndex;
	bool _isPlaying;
	uint32 _heard[kNumHeardWords];

	bool _hasPlayed[kMaxSubtitles];
	int _currentLineCount;
	int _nextLineSwitchTime;
	int _wrappedLineCharacters[kMaxWrappedLines];

	int _fadeBoxHandle;
	bool _groupVolumesCached;
	int _savedGroupVolumes[kNumGroups];
};

} // End of namespace Grim

#endif
