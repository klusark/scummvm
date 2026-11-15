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

#include "engines/grim/emi/sound/audiosystem.h"
#include "engines/grim/emi/sound/caudiochannel.h"
#include "engines/grim/emi/nodes/cnode.h"
#include "engines/grim/emi/sound/csnd.h"
#include "engines/grim/emi/sound/csoundcue.h"
#include "engines/grim/emi/sound/cwavesound.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/core/lstring.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/sound/sounds.h"

namespace Grim {

static const uint kSndFlagsMask = 0x003FFFFF;
static const int kSavedPositionSize = 16;
static const int kSavedPositionOffset = 4;

CSoundCue::CSoundCue(Sounds *sounds, LResourceHandle<CSnd> *sndHandle, CNode *node) : _soundHandle(sounds->getEngine()), _sounds(sounds) {
	_prev = nullptr;
	_next = nullptr;
	_sound = nullptr;
	_priority = 0.0f;
	_volume = 0.0f;
	_mvol = 0.0f;
	_pan = 0.0f;
	_reserved48 = 0;
	_freq = 0.0f;
	_state = kStateNone;
	_cutoffNear = 0.0f;
	_cutoffFar = 0.0f;
	_flags = 0;

	for (int i = 0; i < 5; i++)
		_reserved72[i] = 0;

	_nodeRef._cue = nullptr;
	_channel = nullptr;
	_group = 0;
	_lipData = nullptr;
	_lipSize = 0;
	_handle = 0;
	_category = 0;

	if (sounds->getEngine()->getGamePlatform() == Common::kPlatformPS2) {
		_handle = sounds->_soundCounter;
		sounds->_soundCounter++;
	}

	if (!sndHandle->lock() || !sndHandle->_typed) {
		initDefaults(node);
	} else {
		CSnd *snd = sndHandle->_typed;

		initFromSnd(snd, node);
		_soundHandle.setResource(snd->getSoundHandle()->_resource);

		if (_soundHandle.lock())
			_sound = dynamic_cast<CWaveSound *>(_soundHandle._typed);
		else
			_sound = nullptr;
	}

	sndHandle->unlock();

	_nodeRef._cue = this;

	if (_sounds->_firstCue)
		_sounds->_firstCue->_prev = this;

	_prev = nullptr;
	_next = _sounds->_firstCue;
	_sounds->_firstCue = this;

	if (!_sounds->_lastCue)
		_sounds->_lastCue = this;

	_sounds->_cueCount++;
}

CSoundCue::CSoundCue(Sounds *sounds, const LString &name, CNode *node) : _soundHandle(sounds->getEngine()), _sounds(sounds) {
	_prev = nullptr;
	_next = nullptr;
	_sound = nullptr;
	_priority = 0.0f;
	_volume = 0.0f;
	_mvol = 0.0f;
	_pan = 0.0f;
	_reserved48 = 0;
	_freq = 0.0f;
	_state = kStateNone;
	_cutoffNear = 0.0f;
	_cutoffFar = 0.0f;
	_flags = 0;

	for (int i = 0; i < 5; i++)
		_reserved72[i] = 0;

	_nodeRef._cue = nullptr;
	_channel = nullptr;
	_group = 0;
	_lipData = nullptr;
	_lipSize = 0;
	_handle = 0;
	_category = 0;

	if (sounds->getEngine()->getGamePlatform() == Common::kPlatformPS2) {
		_handle = sounds->_soundCounter;
		sounds->_soundCounter++;
	}

	LString upperName(name);

	upperName.toUppercase();

	if (upperName.find(".SND") == LString::npos) {
		initDefaults(node);

		LResourceHandle<LSound> rawHandle(_sounds->getEngine(), name);

		_soundHandle.setResource(rawHandle._resource);

		if (_soundHandle.lock())
			_sound = dynamic_cast<CWaveSound *>(_soundHandle._typed);
		else
			_sound = nullptr;
	} else {
		LResourceHandle<CSnd> sndHandle(_sounds->getEngine(), name);

		if (sndHandle.lock() && sndHandle._typed) {
			CSnd *snd = sndHandle._typed;

			initFromSnd(snd, node);
			_soundHandle.setResource(snd->getSoundHandle()->_resource);

			if (_soundHandle.lock())
				_sound = dynamic_cast<CWaveSound *>(_soundHandle._typed);
			else
				_sound = nullptr;
		} else {
			initDefaults(node);
		}

		sndHandle.unlock();
	}

	_nodeRef._cue = this;

	if (_sounds->_firstCue)
		_sounds->_firstCue->_prev = this;

	_prev = nullptr;
	_next = _sounds->_firstCue;
	_sounds->_firstCue = this;

	if (!_sounds->_lastCue)
		_sounds->_lastCue = this;

	_sounds->_cueCount++;
}

CSoundCue::~CSoundCue() {
	releaseChannel();
	_soundHandle.unlock();
	_sound = nullptr;

	if (_lipData)
		delete[] reinterpret_cast<byte *>(_lipData);

	_lipData = nullptr;

	if (this == _sounds->_firstCue) {
		CSoundCue *oldFirst = _sounds->_firstCue;

		_sounds->_firstCue = _sounds->_firstCue->_next;

		if (_sounds->_firstCue)
			_sounds->_firstCue->_prev = nullptr;
		else
			_sounds->_lastCue = nullptr;

		oldFirst->_prev = nullptr;
		oldFirst->_next = nullptr;
		_sounds->_cueCount--;
	} else if (this == _sounds->_lastCue) {
		CSoundCue *oldLast = _sounds->_lastCue;

		_sounds->_lastCue = _sounds->_lastCue->_prev;

		if (_sounds->_lastCue)
			_sounds->_lastCue->_next = nullptr;
		else
			_sounds->_firstCue = nullptr;

		oldLast->_next = nullptr;
		oldLast->_prev = nullptr;
		_sounds->_cueCount--;
	} else if (_next && _prev) {
		_next->_prev = _prev;
		_prev->_next = _next;
		_sounds->_cueCount--;
		_prev = nullptr;
		_next = nullptr;
	} else if (_next || _prev) {
		warning("CSoundCue::~CSoundCue(): inconsistent cue list");
	}

	_sounds->_destroyedCount++;
}

void *CSoundCue::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void CSoundCue::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

bool CSoundCue::play(bool releaseWhenDone) {
	stop();

	if (releaseWhenDone)
		_flags |= kFlagReleaseWhenDone;
	else
		_flags &= ~kFlagReleaseWhenDone;

	startPlay(false);

	return _channel != nullptr;
}

bool CSoundCue::startPlay(bool startPaused) {
	AudioSystem *audioSystem = _sounds->getAudioSystem();

	releaseChannel();

	if (!audioSystem->isCueInRange(this))
		return _channel != nullptr;

	_channel = audioSystem->allocateAudioChannel(this);

	if (!_channel) {
		_state = kStateWaiting;

		return _channel != nullptr;
	}

	if (_channel->assignToSample(this)) {
		_channel->setVolume(getCategoryGain() * _volume);
		_channel->setPan(_pan);

		if (startPaused) {
			_channel->pause();

			return _channel != nullptr;
		}

		_channel->start();
		_state = kStateStarted;

		return _channel != nullptr;
	}

	releaseChannel();
	_state = kStateWaiting;

	if (audioSystem->_debugLevel > 1) {
		const char *failedName = "<file_error>";

		if (_soundHandle._resource)
			failedName = _soundHandle._resource->_name.c_str();

		warning("CSoundCue::startPlay(): failed to initialize channel for sound <%s>", failedName);
	}

	return _channel != nullptr;
}

bool CSoundCue::startPlay() {
	AudioSystem *audioSystem = _sounds->getAudioSystem();

	releaseChannel();

	if (!audioSystem->isCueInRange(this))
		return _channel != nullptr;

	_channel = audioSystem->allocateAudioChannel(this);

	if (!_channel) {
		_state = kStateWaiting;

		return _channel != nullptr;
	}

	if (_channel->assignToSample(this)) {
		_state = kStateStopped;

		return _channel != nullptr;
	}

	releaseChannel();
	_state = kStateWaiting;

	if (audioSystem->_debugLevel > 1) {
		const char *failedName = "<file_error>";

		if (_soundHandle._resource)
			failedName = _soundHandle._resource->_name.c_str();

		warning("CSoundCue::startPlay(): failed to initialize channel for sound <%s>", failedName);
	}

	return _channel != nullptr;
}

bool CSoundCue::update() {
	if ((_flags & kFlagStartInRange) != 0) {
		bool inRange = _sounds->getAudioSystem()->isCueInRange(this);

		if (inRange) {
			_flags |= kFlagInRange;

			if (getState() != kStatePlaying)
				play(true);
		} else {
			_flags &= ~kFlagInRange;

			if (getState() == kStatePlaying)
				stop();
		}
	}

	if (_state == kStateWaiting || _state == kStateStopped || _state == kStateFinished) {
		if ((_flags & kFlagStartInRange) == 0 && (_flags & kFlagReleaseWhenDone) != 0)
			return false;
	}

	return true;
}

CSoundCue *CSoundCue::newSoundCue(Sounds *sounds, LResourceHandle<CSnd> *sndHandle, int category, CNode *node) {
	CSoundCue *cue = nullptr;

	if (category == 0)
		warning("CSoundCue::newSoundCue(): the default category is not supported");
	else
		cue = new CSoundCue(sounds, sndHandle, node);

	if (cue)
		cue->_category = category;

	if (!cue || cue->_sound)
		return cue;

	deleteSoundCue(cue);

	return nullptr;
}

CSoundCue *CSoundCue::newSoundCue(Sounds *sounds, const LString &name, int category, CNode *node) {
	CSoundCue *cue = nullptr;

	if (name.empty())
		return nullptr;

	LString fullName(name);
	LString extension;

	LStringUtil::getExtension(fullName, extension);
	extension.toUppercase();

	if (extension.empty()) {
		extension = "SND";
		fullName += ".";
		fullName += extension;
	}

	if (extension == "SND") {
		LResourceHandle<CSnd> sndHandle(sounds->getEngine(), fullName);

		cue = newSoundCue(sounds, &sndHandle, category, node);
	} else {
		switch (category) {
		case 0:
			warning("CSoundCue::newSoundCue(): the default category is not supported");

			break;

		case 2:
			cue = new CSoundCue(sounds, name, node);
			cue->_category = 2;

			break;

		case 3:
			warning("CSoundCue::newSoundCue(): the music category is not supported");

			break;

		case 4:
			break;

		default:
			cue = new CSoundCue(sounds, name, node);
			cue->_category = category;

			break;
		}
	}

	if (cue && !cue->_sound) {
		deleteSoundCue(cue);
		cue = nullptr;
	}

	return cue;
}

void CSoundCue::deleteSoundCue(CSoundCue *cue) {
	if (cue) {
		cue->stop();
		delete cue;
	} else {
		warning("CSoundCue::deleteSoundCue(): passed a NULL pointer");
	}
}

void CSoundCue::releaseAllCues(Sounds *sounds) {
	while (sounds->_cueCount) {
		if (!sounds->_firstCue) {
			warning("CSoundCue::releaseAllCues(): inconsistent cue list");

			break;
		}

		delete sounds->_firstCue;
	}
}

void CSoundCue::releaseUnlockedCues(Sounds *sounds) {
	// The demo has no script-locked sound list: the restored cues belong to the restored Lua state
	if (sounds->getEngine()->isDemo())
		return;

	CSoundCue *cue = sounds->_firstCue;

	while (cue) {
		CSoundCue *current = cue;

		cue = cue->_next;

		if (!sounds->getEngine()->getLua()->isSoundLocked((int16)current->_handle)) {
			CAudioChannel *currentChannel = current->_channel;

			if (!currentChannel || !currentChannel->isPaused()) {
				sounds->unregisterSound(current->_handle);
				deleteSoundCue(current);
			}
		}
	}
}

void CSoundCue::updateSoundCues(Sounds *sounds) {
	CSoundCue *cue = sounds->_firstCue;

	sounds->_updatedCount = 0;

	while (cue) {
		CSoundCue *current = cue;

		sounds->_updatedCount++;
		cue = cue->_next;

		if (sounds->getEngine()->isDemo()) {
			if (!current->update()) {
				// The registry entry must not outlive the cue
				if (sounds->findSoundCue(current->_handle) == current) {
					warning("CSoundCue::updateSoundCues(): deleting a registered sound cue (handle %d)", (int)current->_handle);

					sounds->unregisterSound(current->_handle);
				}

				deleteSoundCue(current);
			}

			continue;
		}

		if (!current->update() && !sounds->isCueRegistered(current))
			deleteSoundCue(current);
	}
}

bool CSoundCue::stopCueToFreeChannel(Sounds *sounds, bool finiteLoopsFirst, bool anyNonLooping) {
	CSoundCue *found = nullptr;

	if (finiteLoopsFirst) {
		CSoundCue *cue = sounds->_firstCue;

		while (cue) {
			CSoundCue *current = cue;

			cue = cue->_next;

			if (current->_category != 2 && current->hasFiniteLoopCount()) {
				found = current;

				break;
			}
		}
	}

	if (!found) {
		if (!anyNonLooping)
			return false;

		CSoundCue *cue = sounds->_firstCue;

		while (cue) {
			CSoundCue *current = cue;

			cue = cue->_next;

			if (current->_category != 2 && (current->_flags & kFlagLoop) == 0) {
				found = current;

				break;
			}
		}

		if (!found)
			return false;
	}

	found->stop();

	return true;
}

bool CSoundCue::saveCues(Sounds *sounds, SaveRestoreAccess *accessFunc) {
	CSoundCue *cue = sounds->_firstCue;

	if (sounds->getEngine()->getGamePlatform() == Common::kPlatformPS2) {
		int savedCounter = sounds->_soundCounter;
		int savedCount = (int)sounds->_cueCount;

		accessFunc(&savedCounter, sizeof(savedCounter));
		accessFunc(&savedCount, sizeof(savedCount));
	} else {
		int16 savedCount = (int16)sounds->_cueCount;

		accessFunc(&savedCount, sizeof(savedCount));
	}

	while (cue) {
		CSoundCue *current = cue;

		cue = cue->_next;
		current->save(accessFunc);
	}

	return true;
}

bool CSoundCue::restoreCues(Sounds *sounds, SaveRestoreAccess *accessFunc, SoundCueMap *cueMap, const void *lipData, int lipSize) {
	bool isPs2 = sounds->getEngine()->getGamePlatform() == Common::kPlatformPS2;
	LString cueName;
	int restoredCount = 0;
	int nameLength = 0;
	int category = 0;
	byte loopFlag = 0;
	byte pausedFlag = 0;
	float restoredVolume = 0.0f;
	byte positionBytes[kSavedPositionSize];
	int restoredPositionMs = 0;
	float restoredPan = 0.0f;
	int restoredHandle = 0;
	int restoredCounter = 0;

	if (isPs2) {
		accessFunc(&restoredCounter, sizeof(restoredCounter));
		accessFunc(&restoredCount, sizeof(restoredCount));
	} else {
		int16 shortCount = 0;

		accessFunc(&shortCount, sizeof(shortCount));
		restoredCount = shortCount;
	}

	memset(positionBytes, 0, sizeof(positionBytes));

	for (int i = 0; i < restoredCount; i++) {
		if (isPs2) {
			accessFunc(&nameLength, sizeof(nameLength));
		} else {
			int16 shortLength = 0;

			accessFunc(&shortLength, sizeof(shortLength));
			nameLength = shortLength;
		}

		if (nameLength < 0) {
			warning("CSoundCue::restoreCues(): invalid name length %d", (int)nameLength);

			return false;
		}

		char *nameBuffer = new char[nameLength + 1];

		accessFunc(nameBuffer, nameLength);
		nameBuffer[nameLength] = 0;
		cueName = nameBuffer;
		delete[] nameBuffer;

		accessFunc(&category, sizeof(category));
		accessFunc(&loopFlag, sizeof(loopFlag));
		accessFunc(&pausedFlag, sizeof(pausedFlag));
		accessFunc(&restoredVolume, sizeof(restoredVolume));
		accessFunc(positionBytes, kSavedPositionSize);
		accessFunc(&restoredPositionMs, sizeof(restoredPositionMs));
		accessFunc(&restoredPan, sizeof(restoredPan));

		if (isPs2) {
			accessFunc(&restoredHandle, sizeof(restoredHandle));
		} else {
			uint16 shortHandle = 0;

			accessFunc(&shortHandle, sizeof(shortHandle));
			restoredHandle = shortHandle;
		}

		CSoundCue *cue = newSoundCue(sounds, cueName, category, nullptr);

		if (!cue || (isPs2 && restoredHandle == -1))
			continue;

		if (loopFlag)
			cue->_flags |= kFlagLoop;
		else
			cue->_flags &= ~kFlagLoop;

		if (pausedFlag) {
			cue->startPlay(true);

			if (cue->_channel)
				cue->_channel->setPaused(true);
		} else if (cue->_channel) {
			cue->_channel->setPaused(false);
		}

		if (cue->_category == 2) {
			if (!isPs2)
				sounds->soundCueLipCallback(cueName.c_str(), cue);
			else if (lipSize)
				cue->setLipData(lipData, lipSize);
		}

		SoundPosition restoredPosition;

		restoredPosition._valid = positionBytes[0] != 0;
		memcpy(&restoredPosition._pos, positionBytes + kSavedPositionOffset, sizeof(restoredPosition._pos));

		cue->setVolume(restoredVolume);
		cue->setPosition(restoredPosition);
		cue->setPositionMs(restoredPositionMs);
		cue->setPan(restoredPan);

		if (restoredHandle) {
			(*cueMap)[(uint)restoredHandle] = cue;
			cue->_handle = restoredHandle;
		}
	}

	// The cues created above used up counter values
	if (isPs2)
		sounds->_soundCounter = restoredCounter;

	return true;
}

int CSoundCue::getSoundType() {
	if (!_soundHandle._resource)
		return CWaveSound::kTypeWave;

	LSound *lockedSound = nullptr;

	if (_soundHandle.lock())
		lockedSound = _soundHandle._typed;

	if (!lockedSound)
		return CWaveSound::kTypeWave;

	int type = lockedSound->getType();

	_soundHandle.unlock();

	return type;
}

bool CSoundCue::start(bool releaseWhenDone) {
	if ((_flags & kFlagStartInRange) != 0)
		return false;

	return play(releaseWhenDone);
}

void CSoundCue::stop() {
	if (_channel)
		_channel->stop();
}

void CSoundCue::pause() {
	if (_channel)
		_channel->pause();
}

void CSoundCue::resume() {
	if (_channel)
		_channel->resume();
}

float CSoundCue::getPriority() {
	return _priority;
}

float CSoundCue::getVolume() {
	return _volume;
}

float CSoundCue::getPan() {
	return _pan;
}

float CSoundCue::getCutoffNear() {
	return _cutoffNear;
}

float CSoundCue::getCutoffFar() {
	return _cutoffFar;
}

float CSoundCue::getCategoryGain() {
	switch (_category) {
	case 1:
		return _sounds->getAudioSystem()->getCategoryGain(1);

	case 2:
		return _sounds->getAudioSystem()->getCategoryGain(2);

	case 3:
		return _sounds->getAudioSystem()->getCategoryGain(3);

	default:
		break;
	}

	return 1.0f;
}

void CSoundCue::setVolume(float newVolume) {
	_volume = newVolume;

	float gain = getCategoryGain();

	if (_channel)
		_channel->setVolume(gain * newVolume);
}

void CSoundCue::setPan(float newPan) {
	_pan = newPan;

	if (_channel)
		_channel->setPan(newPan);
}

bool CSoundCue::setPosition(float x, float y, float z) {
	_position.set(x, y, z);
	updateSpatialization();

	return true;
}

bool CSoundCue::setPosition(const SoundPosition &newPosition) {
	_position = newPosition;
	updateSpatialization();

	return true;
}

bool CSoundCue::updateSpatialization() {
	// The demo cues have no position
	if (_sounds->getEngine()->isDemo() || !_position._valid)
		return false;

	float worldX = _position._pos._x;
	float worldY = _position._pos._y;
	float worldZ = _position._pos._z;
	float localX = worldX;
	float localY = worldY;
	float localZ = worldZ;
	float panValue;
	float volumeFactor = 1.0f;

	CNode *listenerNode = _sounds->getAudioSystem()->_listener._node;

	if (listenerNode)
		listenerNode->worldToLocal(&worldX, &worldY, &worldZ, &localX, &localY, &localZ);

	if (localZ < 0.0f || localZ > 0.0f) {
		double ratio = (double)localX / (double)localZ;
		double twiceRatio = ratio + ratio;

		panValue = (float)twiceRatio;

		if (!(twiceRatio >= 0.0))
			panValue = panValue * -1.0f;

		if (panValue > 1.0f)
			panValue = 1.0f;

		if (!(localX >= 0.0f))
			panValue = panValue * -1.0f;

		if (!(localZ >= 0.0f)) {
			volumeFactor = 0.125f;
		} else {
			double ratioSquared = ratio * ratio;

			if (ratioSquared > (double)0.2f) {
				double reduced = 1.0 - (ratioSquared - (double)0.2f);

				volumeFactor = (float)reduced;

				if (reduced < 0.125)
					volumeFactor = 0.125f;
			}
		}
	} else {
		volumeFactor = 0.125f;

		if (!(localX >= 0.0f)) {
			panValue = -1.0f;
		} else {
			panValue = 0.0f;

			if (localX > 0.0f)
				panValue = 1.0f;
		}
	}

	setPan(panValue);

	double distanceSquared = (double)localZ * localZ + (double)localY * localY + (double)localX * localX;

	volumeFactor = volumeFactor * _volume;

	if (distanceSquared > 9.0) {
		double scaled = (sqrt(distanceSquared) - (double)3.0f) * (double)0.12f;

		volumeFactor = (float)(9.0 / (scaled * scaled + 9.0) * volumeFactor);
	}

	float gain = getCategoryGain();

	if (_channel)
		_channel->setVolume(gain * volumeFactor);

	return true;
}

bool CSoundCue::hasFiniteLoopCount() {
	return _channel && _channel->hasFiniteLoopCount();
}

bool CSoundCue::getPosition(CVector *outPosition) {
	if ((_flags & kFlagPositional) == 0)
		return false;

	CNode *followedNode = _nodeRef._node;

	if (followedNode) {
		if ((followedNode->_nodeFlags & CNode::kWorldValid) == 0)
			followedNode->updateWorldTransform();

		*outPosition = followedNode->_world._pos;

		return ((_flags >> 29) & 1) != 0;
	}

	*outPosition = _fixedPosition;

	return ((_flags >> 29) & 1) != 0;
}

int CSoundCue::initDefaults(CNode *node) {
	_priority = 0.5f;
	_volume = 1.0f;
	_mvol = 1.0f;
	_pan = 0.0f;
	_freq = 1.0f;
	_cutoffNear = 1.0f;
	_cutoffFar = 50.0f;
	_flags = 0;
	_channel = nullptr;
	_fixedPosition = CVector();
	_state = kStateNone;
	_reserved48 = 0;

	for (int i = 0; i < 5; i++)
		_reserved72[i] = 0;

	_soundName.clear();
	_lipData = nullptr;

	if (node) {
		_nodeRef.setNode(node);
		_flags |= kFlagPositional;
	}

	_sounds->_createdCount++;

	return (int)_sounds->_createdCount;
}

void CSoundCue::initFromSnd(CSnd *snd, CNode *node) {
	initDefaults(node);

	if (!snd)
		return;

	_volume = snd->getVolume();
	_mvol = snd->getMvol();
	_priority = snd->getPrio();
	_pan = snd->getPan();
	_cutoffNear = snd->getCutoff0();
	_cutoffFar = snd->getCutoff1();
	_freq = snd->getFreq();
	_flags |= (uint)snd->getFlags() & kSndFlagsMask;
	_group = snd->getGroup();
	_soundName = snd->_name;
}

void CSoundCue::setLipData(const void *data, uint size) {
	if (_lipData && _lipSize < size) {
		warning("CSoundCue::setLipData(): the lip data does not fit in the old buffer");

		delete[] reinterpret_cast<byte *>(_lipData);
		_lipData = nullptr;
	}

	if (!_lipData)
		_lipData = reinterpret_cast<LipData *>(new byte[size]);

	memcpy(_lipData, data, size);
	_lipSize = size;
}

void CSoundCue::releaseChannel() {
	if (_channel)
		_channel->release();

	_channel = nullptr;
}

void CSoundCue::unlinkFromCueList() {
	if (_sounds->_firstCue != this && !_prev)
		return;

	if (_prev)
		_prev->_next = _next;
	else
		_sounds->_firstCue = _next;

	if (_next)
		_next->_prev = _prev;
	else
		_sounds->_lastCue = _prev;

	_prev = nullptr;
	_next = nullptr;
	_sounds->_cueCount--;
}

int CSoundCue::getState() {
	int channelStatus = 0;

	if (_channel)
		channelStatus = _channel->getStatus();

	switch (channelStatus) {
	case 2:
		return 5;

	case 3:
	case 4:
		return 2;

	case 5:
		return 3;

	default:
		break;
	}

	return 0;
}

int CSoundCue::getPositionMs(int *totalMs) {
	if (_channel)
		return _channel->getPositionMs(totalMs);

	if (totalMs)
		*totalMs = 0;

	return 0;
}

void CSoundCue::setPositionMs(int milliseconds) {
	if (_channel)
		_channel->setPositionMs(milliseconds);
}

bool CSoundCue::save(SaveRestoreAccess *accessFunc) {
	const char *savedName = "<file_error>";

	if (_soundHandle._resource)
		savedName = _soundHandle._resource->_name.c_str();

	bool isPs2 = _sounds->getEngine()->getGamePlatform() == Common::kPlatformPS2;
	int savedLength = (int)strlen(savedName);
	byte positionBytes[kSavedPositionSize];
	byte loopFlag = 0;
	byte pausedFlag = 0;

	memset(positionBytes, 0, sizeof(positionBytes));
	positionBytes[0] = _position._valid ? 1 : 0;
	memcpy(positionBytes + kSavedPositionOffset, &_position._pos, sizeof(_position._pos));

	if (_channel)
		pausedFlag = _channel->isPaused() ? 1 : 0;

	loopFlag = (_flags & kFlagLoop) != 0 ? 1 : 0;

	int savedPositionMs = getPositionMs(nullptr);

	if (isPs2) {
		// The text goes out with its terminator
		savedLength++;
		accessFunc(&savedLength, sizeof(savedLength));
	} else {
		int16 shortLength = (int16)savedLength;

		accessFunc(&shortLength, sizeof(shortLength));
	}

	accessFunc(const_cast<char *>(savedName), savedLength);
	accessFunc(&_category, sizeof(_category));
	accessFunc(&loopFlag, sizeof(loopFlag));
	accessFunc(&pausedFlag, sizeof(pausedFlag));
	accessFunc(&_volume, sizeof(_volume));
	accessFunc(positionBytes, kSavedPositionSize);
	accessFunc(&savedPositionMs, sizeof(savedPositionMs));
	accessFunc(&_pan, sizeof(_pan));

	if (isPs2) {
		accessFunc(&_handle, sizeof(_handle));
	} else {
		uint16 shortHandle = (uint16)_handle;

		accessFunc(&shortHandle, sizeof(shortHandle));
	}

	return true;
}

} // End of namespace Grim
