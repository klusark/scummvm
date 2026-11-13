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

#include "common/memstream.h"
#include "common/stream.h"
#include "common/system.h"

#include "engines/grim/grim.h"
#include "engines/grim/imuse/imuse_engine.h"

namespace Grim {

enum {
	kRemasteredFilelistNameSize = 33
};

// The savegames of the remastered have the structures of the iMUSE as they are in memory, with
// the pointers in slots of 8 bytes. The pointers are not used when loading, only the one of the
// stream matters as a flag.
void IMuse::dispatchSaveLoadRemastered(Common::Serializer &ser) {
	IMuseDispatch emptyDispatch;

	memset(&emptyDispatch, 0, sizeof(emptyDispatch));

	for (int l = 0; l < IMUSE_SAVED_TRACKS; l++) {
		IMuseDispatch *dispatch = l < IMUSE_MAX_DISPATCHES ? &_dispatches[l] : &emptyDispatch;
		int32 trackPtr = 0;
		int32 streamPtr = dispatch->streamPtr ? 1 : 0;
		int32 streamZoneList = 0;
		int32 fadeBuf = 0;
		int32 pointerHigh = 0;

		ser.syncAsSint32LE(trackPtr, VER(30));
		ser.syncAsSint32LE(pointerHigh, VER(30));
		ser.syncAsSint32LE(dispatch->wordSize, VER(30));
		ser.syncAsSint32LE(dispatch->sampleRate, VER(30));
		ser.syncAsSint32LE(dispatch->channelCount, VER(30));
		ser.syncAsSint32LE(dispatch->swapEndiannessFlag, VER(30));
		ser.syncAsSint32LE(dispatch->currentOffset, VER(30));
		ser.syncAsSint32LE(dispatch->audioRemaining, VER(30));
		ser.syncArray(dispatch->map, IMUSE_MAX_MAP_SIZE, Common::Serializer::Sint32LE, VER(30));
		ser.syncAsSint32LE(streamPtr, VER(30));
		ser.syncAsSint32LE(pointerHigh, VER(30));
		ser.syncAsSint32LE(dispatch->streamBufID, VER(30));
		ser.syncAsSint32LE(streamZoneList, VER(30));
		ser.syncAsSint32LE(pointerHigh, VER(30));
		ser.syncAsSint32LE(dispatch->streamErrFlag, VER(30));
		ser.syncAsSint32LE(fadeBuf, VER(30));
		ser.syncAsSint32LE(pointerHigh, VER(30));
		ser.syncAsSint32LE(dispatch->fadeOffset, VER(30));
		ser.syncAsSint32LE(dispatch->fadeRemaining, VER(30));
		ser.syncAsSint32LE(dispatch->fadeWordSize, VER(30));
		ser.syncAsSint32LE(dispatch->fadeSampleRate, VER(30));
		ser.syncAsSint32LE(dispatch->fadeChannelCount, VER(30));
		ser.syncAsSint32LE(dispatch->fadeSyncFlag, VER(30));
		ser.syncAsSint32LE(dispatch->fadeSyncDelta, VER(30));
		ser.syncAsSint32LE(dispatch->fadeVol, VER(30));
		ser.syncAsSint32LE(dispatch->fadeSlope, VER(30));

		// The stream is allocated again later
		if (ser.isLoading())
			dispatch->streamPtr = streamPtr ? (IMuseStream *)1 : nullptr;
	}

	if (ser.isLoading()) {
		for (int i = 0; i < IMUSE_LARGE_FADES; i++)
			_dispatchLargeFadeFlags[i] = 0;

		for (int i = 0; i < IMUSE_SMALL_FADES; i++)
			_dispatchSmallFadeFlags[i] = 0;

		for (int i = 0; i < IMUSE_MAX_STREAMZONES; i++)
			_streamZones[i].useFlag = 0;
	}
}

void IMuse::tracksSaveLoadRemastered(Common::Serializer &ser) {
	Common::StackLock lock(*_mutex);
	dispatchSaveLoadRemastered(ser);

	IMuseTrack emptyTrack;

	memset(&emptyTrack, 0, sizeof(emptyTrack));

	for (int l = 0; l < IMUSE_SAVED_TRACKS; l++) {
		IMuseTrack *track = l < _trackCount ? &_tracks[l] : &emptyTrack;
		int32 next = 0;
		int32 prev = 0;
		int32 dispatchPtr = 0;
		int32 pointerHigh = 0;

		ser.syncAsSint32LE(next, VER(30));
		ser.syncAsSint32LE(pointerHigh, VER(30));
		ser.syncAsSint32LE(prev, VER(30));
		ser.syncAsSint32LE(pointerHigh, VER(30));
		ser.syncAsSint32LE(dispatchPtr, VER(30));
		ser.syncAsSint32LE(pointerHigh, VER(30));
		ser.syncAsSint32LE(track->soundId, VER(30));
		ser.syncAsSint32LE(track->marker, VER(30));
		ser.syncAsSint32LE(track->group, VER(30));
		ser.syncAsSint32LE(track->priority, VER(30));
		ser.syncAsSint32LE(track->vol, VER(30));
		ser.syncAsSint32LE(track->effVol, VER(30));
		ser.syncAsSint32LE(track->pan, VER(30));
		ser.syncAsSint32LE(track->detune, VER(30));
		ser.syncAsSint32LE(track->transpose, VER(30));
		ser.syncAsSint32LE(track->pitchShift, VER(30));
		ser.syncAsSint32LE(track->mailbox, VER(30));
		ser.syncAsSint32LE(track->jumpHook, VER(30));
	}

	if (ser.isLoading()) {
		for (int l = 0; l < _trackCount; l++) {
			_tracks[l].prev = nullptr;
			_tracks[l].next = nullptr;
			_tracks[l].dispatchPtr = dispatchGetDispatchByTrackId(l);
			_tracks[l].dispatchPtr->trackPtr = &_tracks[l];

			if (_tracks[l].soundId)
				addTrackToList(&_trackList, &_tracks[l]);
		}

		dispatchRestoreStreamZones();
	}
}

void IMuse::imSaveLoadFilelistRemastered(Common::Serializer &ser) {
	IMuseSoundList *sound;
	int loadedCount = 0;
	int openedCount = 0;
	int soundId = 0;
	int refCount = 0;
	char filename[36] = {};

	if (ser.isSaving()) {
		for (sound = _loadedSoundList; sound; sound = sound->next)
			loadedCount++;

		for (sound = _openedSoundsList; sound; sound = sound->next)
			openedCount++;
	} else {
		for (int i = 0; i < IMUSE_MAX_SOUND_LIST; i++) {
			_mainSoundList[i].prev = nullptr;
			_mainSoundList[i].next = nullptr;
			_mainSoundList[i].soundId = 0;
		}
	}

	ser.syncAsSint32LE(loadedCount, VER(30));
	ser.syncAsSint32LE(openedCount, VER(30));

	// The loaded sounds, then the opened ones
	for (int list = 0; list < 2; list++) {
		int count = list == 0 ? loadedCount : openedCount;

		sound = list == 0 ? _loadedSoundList : _openedSoundsList;

		for (int i = 0; i < count; i++) {
			int32 prevPtr = 0;
			int32 nextPtr = 0;
			int32 pointerHigh = 0;

			if (ser.isSaving()) {
				soundId = sound->soundId;
				refCount = sound->refCount;
				Common::strlcpy(filename, sound->filename, kRemasteredFilelistNameSize);
				sound = sound->next;
			}

			ser.syncAsSint32LE(prevPtr, VER(30));
			ser.syncAsSint32LE(pointerHigh, VER(30));
			ser.syncAsSint32LE(nextPtr, VER(30));
			ser.syncAsSint32LE(pointerHigh, VER(30));
			ser.syncAsSint32LE(soundId, VER(30));
			ser.syncArray(filename, kRemasteredFilelistNameSize, Common::Serializer::SByte, VER(30));
			ser.syncAsSint32LE(refCount, VER(30));

			if (!ser.isLoading())
				continue;

			if (list == 0) {
				if (!imLoadSound(filename, soundId)) {
					debug(5, "IMuse::imSaveLoadFilelistRemastered(): ERROR: couldn't load file %s", filename);
					return;
				}

				if (soundId != _loadedSoundList->soundId) {
					debug(5, "IMuse::imSaveLoadFilelistRemastered(): ERROR: detected modified sound number");
					return;
				}

				_loadedSoundList->refCount = refCount;
			} else {
				if (!imOpenSound(filename, soundId))
					debug(5, "IMuse::imSaveLoadFilelistRemastered(): ERROR: couldn't open file %s", filename);

				if (soundId != _openedSoundsList->soundId) {
					debug(5, "IMuse::imSaveLoadFilelistRemastered(): ERROR: detected modified sound number");
					return;
				}

				_openedSoundsList->refCount = refCount;
			}
		}
	}
}

void IMuse::scriptSaveLoadRemastered(Common::Serializer &ser) {
	if (ser.isLoading()) {
		imUnloadAll();
		imCloseAll();
	}

	ser.syncAsSint32LE(_curMusicState, VER(30));
	ser.syncAsSint32LE(_curMusicSeq, VER(30));
	ser.syncAsSint32LE(_nextSeqToPlay, VER(30));
	ser.syncArray(_attributes, ARRAYSIZE(_attributes), Common::Serializer::Sint32LE, VER(30));
	imSaveLoadFilelistRemastered(ser);
}

void IMuse::imSaveLoadRemastered(Common::Serializer &ser) {
	Common::StackLock lock(*_mutex);

	if (ser.isLoading())
		imStopAllSounds();

	_fadesHandler->saveLoad(ser);
	_triggersHandler->saveLoad(ser);
	tracksSaveLoadRemastered(ser);
}

void IMuse::soundsSaveLoadBlockDataRemastered(Common::Serializer &ser, bool isImSave) {
	if (!isImSave) {
		scriptSaveLoadRemastered(ser);
		return;
	}

	int32 tag = 49;

	ser.syncAsSint32LE(tag, VER(30));

	if (ser.isLoading() && tag != 49)
		warning("IMuse::soundsSaveLoadBlockDataRemastered(): unexpected tag %d", tag);

	imSaveLoadRemastered(ser);
}

// The ImSave and ImSaveScript data are stored as a size followed by the data,
// the ImSave one starts with the tag 49
void IMuse::soundsSaveLoadBlockRemastered(Common::Serializer &ser, bool isImSave) {
	int32 size = 0;

	if (ser.isSaving()) {
		Common::MemoryWriteStreamDynamic blob(DisposeAfterUse::YES);
		Common::Serializer blobSer(nullptr, &blob);

		blobSer.setVersion(ser.getVersion());
		soundsSaveLoadBlockDataRemastered(blobSer, isImSave);

		size = (int32)blob.size();
		ser.syncAsSint32LE(size, VER(30));
		ser.syncBytes(blob.getData(), size, VER(30));
		return;
	}

	ser.syncAsSint32LE(size, VER(30));

	byte *data = new byte[size];

	ser.syncBytes(data, size, VER(30));

	Common::MemoryReadStream blob(data, size);
	Common::Serializer blobSer(&blob, nullptr);

	blobSer.setVersion(ser.getVersion());
	soundsSaveLoadBlockDataRemastered(blobSer, isImSave);

	if (blob.pos() != size)
		warning("IMuse::soundsSaveLoadBlockRemastered(): %d bytes read out of %d", (int)blob.pos(), size);

	delete[] data;
}

void IMuse::soundsSaveLoadRemastered(Common::Serializer &ser) {
	Common::StackLock lock(*_mutex);

	char effectName[sizeof(_curEffectName)];

	_soundsLegacyLayout = false;

	if (ser.isSaving()) {
		Common::strlcpy(effectName, _curEffectId ? _curEffectName : "OFF", sizeof(effectName));
		_curSpeechFile[0] = '\0';
	}

	ser.syncArray(effectName, sizeof(effectName), Common::Serializer::SByte, VER(30));

	if (ser.isLoading()) {
		imStopAllSounds();
		imFlushSounds();
		soundsCloseVoiceFile();
		cacheReset();
		iMUSESetVoiceEffect(effectName);
		_curSpeechFile[0] = '\0';
	}

	soundsSaveLoadBlockRemastered(ser, true);
	soundsSaveLoadBlockRemastered(ser, false);

	if (ser.isLoading()) {
		for (int soundId = imGetNextSound(0); soundId; soundId = imGetNextSound(soundId)) {
			if (soundId <= kTalkSoundID)
				imStopSound(soundId);
		}

		soundsCloseVoiceFile();
	}

	int32 numSounds = 0;
	int savedSlots[IMUSE_MAX_CACHED_SOUNDS];

	if (ser.isSaving()) {
		// Only active sounds with a native iMUS map are saved...
		for (int slot = 0; slot < IMUSE_MAX_CACHED_SOUNDS; slot++) {
			IMuseCachedSound *sound = &_soundCache[slot];

			if (sound->soundFilename[0] && sound->soundUncompData &&
				imGetParam(slot + 100, IMUSE_P_SND_TRACK_NUM) > 0 &&
				!memcmp(sound->soundMap, "iMUS", 4) && READ_BE_UINT16(sound->soundMap + 14) != 56)
				savedSlots[numSounds++] = slot;
		}
	}

	ser.syncAsSint32LE(numSounds, VER(30));
	if (numSounds < 0 || numSounds > IMUSE_MAX_CACHED_SOUNDS)
		return;

	// The cached sounds are restarted with the same ids, volume, pan and hook
	for (int i = 0; i < numSounds; i++) {
		char soundName[32] = {};
		int32 luaSoundId = 0, priority = 0;

		if (ser.isSaving()) {
			Common::strlcpy(soundName, _soundCache[savedSlots[i]].soundFilename, sizeof(soundName));
			luaSoundId = _soundCache[savedSlots[i]].luaSoundId;
			priority = _soundCache[savedSlots[i]].priority;
		}

		ser.syncArray(soundName, sizeof(soundName), Common::Serializer::SByte, VER(30));
		ser.syncAsSint32LE(luaSoundId, VER(30));
		ser.syncAsSint32LE(priority, VER(30));

		if (ser.isLoading()) {
			_currentSoundSlot = luaSoundId - 1111;
			soundsLoadStartSound(soundName, priority, IMUSE_GROUP_SFX);
		}
	}

	for (int i = 0; i < numSounds; i++) {
		int32 luaSoundId = 0, volume = 0, pan = 0;

		if (ser.isSaving()) {
			luaSoundId = _soundCache[savedSlots[i]].luaSoundId;
			volume = imGetParam(savedSlots[i] + 100, IMUSE_P_VOLUME);
			pan = imGetParam(savedSlots[i] + 100, IMUSE_P_PAN);
		}

		ser.syncAsSint32LE(luaSoundId, VER(30));
		ser.syncAsSint32LE(volume, VER(30));
		ser.syncAsSint32LE(pan, VER(30));

		if (ser.isLoading()) {
			soundsSetParam((const char *)(intptr)luaSoundId, IMUSE_P_VOLUME, volume);
			soundsSetParam((const char *)(intptr)luaSoundId, IMUSE_P_PAN, pan);
		}
	}

	for (int i = 0; i < numSounds; i++) {
		int32 luaSoundId = 0, hookId = 0;

		if (ser.isSaving()) {
			luaSoundId = _soundCache[savedSlots[i]].luaSoundId;
			hookId = imGetHook(savedSlots[i] + 100);
		}

		ser.syncAsSint32LE(luaSoundId, VER(30));
		ser.syncAsSint32LE(hookId, VER(30));

		if (ser.isLoading())
			iMUSESetHook((const char *)(intptr)luaSoundId, hookId);
	}
}

} // End of namespace Grim
