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

#include "common/algorithm.h"
#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "engines/grim/emi/sound/audiosystem.h"
#include "engines/grim/emi/sound/caudiodevice.h"
#include "engines/grim/emi/sound/cmusicps2.h"
#include "engines/grim/emi/core/cpathingfileservices.h"
#include "engines/grim/emi/sound/cwaveaudiodevice.h"
#include "engines/grim/emi/sound/cwavechannel.h"
#include "engines/grim/emi/sound/cwavesound.h"
#include "engines/grim/emi/sound/demomusic.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/sound/lipdata.h"
#include "engines/grim/emi/core/lstring.h"
#include "engines/grim/emi/core/profilertimer.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/emi/sound/streamscript.h"

namespace Grim {

static const uint kMaxHandle = 0xFFFF;
static const uint kUpdateIntervalMs = 30;
static const uint kDemoUpdateIntervalMs = 10;
static const uint kThrottledUpdateIntervalMs = 100;
static const int kMaxStartAttempts = 16;
static const int kReverbPresetCount = 28;
static const float kLoadingLoopFadeSeconds = 0.5f;
static const int kReverbPresets[kReverbPresetCount] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
	14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27
};

const float Sounds::kDbStep = (float)((double)(0.0f - -40.0f) * (double)0.01010101f);
const float Sounds::kInvDbStep = (float)(1.0 / (double)Sounds::kDbStep);
const float Sounds::kUnityGain = (float)pow(10.0, (double)0.0f * (double)0.05f);
const float Sounds::kDefaultMasterGain = (float)pow(10.0, (double)0.0f * (double)0.05f);
const float Sounds::kMovieMasterGain = (float)pow(10.0, (double)-4.0f * (double)0.05f);

Sounds::Sounds(EmiEngine *engine) {
	_engine = engine;
	_audioSystem = nullptr;
	_musicPs2 = nullptr;
	_streamScript = nullptr;
	_demoMusic = nullptr;
	_soundCounter = 1;
	_audioMixAhead = 16;
	_soundsEnabled = true;
	_musicEnabled = true;
	_musicCategoryEnabled = true;
	_voiceEnabled = true;
	_sfxEnabled = true;
	_voiceFxEnabled = 1;
	_savedMusicState = -1;
	_musicActive = false;
	_musicState = 0;
	_musicSet = false;
	_musicVolume = 1.0f;
	_pushedMusicState = -1;
	_pushedMusicPosition = -1;
	_lastUpdateMs = 0;
	_lastThrottledUpdateMs = 0;
	_loadingLoopCue = nullptr;
	_loadingLoopFade = 0;
	_loadingLoopFadeMs = 0;
	_firstCue = nullptr;
	_lastCue = nullptr;
	_cueCount = 0;
	_createdCount = 0;
	_destroyedCount = 0;
	_updatedCount = 0;

	if (_engine->isDemo())
		_demoMusic = new DemoMusicScript(this);
	else if (_engine->getGamePlatform() == Common::kPlatformPS2)
		_musicPs2 = new CMusicPs2(this);
	else
		_streamScript = new StreamScript(this);
}

Sounds::~Sounds() {
	shutdownSounds();

	// Cues and channels must be gone before the audio device, and nothing may outlive this object
	CSoundCue::releaseAllCues(this);
	_soundMap.clear();

	delete _musicPs2;
	_musicPs2 = nullptr;
	delete _streamScript;
	_streamScript = nullptr;
	delete _demoMusic;
	_demoMusic = nullptr;
	delete _audioSystem;
	_audioSystem = nullptr;
}

EmiEngine *Sounds::getEngine() const {
	return _engine;
}

AudioSystem *Sounds::getAudioSystem() const {
	return _audioSystem;
}

int16 Sounds::getAudioMixAhead() const {
	return _audioMixAhead;
}

LResource *Sounds::createWaveSoundResource() {
	return new CWaveSound();
}

float Sounds::soundVolumeToGain(int volume) {
	if (volume >= 100)
		return kUnityGain;

	if (volume > 0)
		return (float)pow(10.0, ((double)(volume - 1) * (double)kDbStep + (double)-40.0f) * (double)0.05f);

	return 0.0f;
}

int Sounds::soundGainToVolume(float gain) {
	if (gain >= kUnityGain)
		return 100;

	if (!(gain > 0.0f))
		return 0;

	return (int)((log10((double)gain) * 20.0 - (double)-40.0f) * (double)kInvDbStep) + 1;
}

uint16 Sounds::registerSound(CSoundCue *cue) {
	uint16 handle = _soundCounter;

	_soundMap[handle] = cue;
	cue->_handle = _soundCounter;
	_soundCounter++;

	return handle;
}

void Sounds::unregisterSound(uint16 soundHandle) {
	_soundMap.erase(soundHandle);
}

CSoundCue *Sounds::findSoundCue(uint soundHandle) {
	CSoundCue *cue = nullptr;

	// The disasm keys the map with the whole dword
	if (soundHandle > kMaxHandle)
		return nullptr;

	if (_soundMap.tryGetVal((uint16)soundHandle, cue))
		return cue;

	return nullptr;
}

bool Sounds::isCueRegistered(CSoundCue *cue) {
	for (CSoundCue::SoundCueMap::iterator it = _soundMap.begin(); it != _soundMap.end(); ++it) {
		if (it->_value == cue)
			return true;
	}

	return false;
}

bool Sounds::soundsUpdate(const bool *force) {
	_engine->_soundsUpdateTimer.start();

	// PS2 updates sounds on every call, without an interval
	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		if (!isAudioActive()) {
			_engine->_soundsUpdateTimer.stop();

			return false;
		}

		updateLoadingLoop();
		CSoundCue::updateSoundCues(this);
		setAudioMixAhead(&_audioMixAhead, false);
		_audioSystem->updateAudioDevice();
		updateMusic();
		_engine->_soundsUpdateTimer.stop();

		return true;
	}

	if (isAudioActive()) {
		uint now = g_system->getMillis();
		bool isDue;

		// The demo has a plain 10 ms interval, no forced updates and no mix-ahead
		if (_engine->isDemo())
			isDue = now - _lastUpdateMs > kDemoUpdateIntervalMs || now < _lastUpdateMs;
		else
			isDue = *force || now - _lastUpdateMs > kUpdateIntervalMs || now < _lastUpdateMs;

		if (isDue) {
			_lastUpdateMs = now;
			CSoundCue::updateSoundCues(this);

			if (!_engine->isDemo()) {
				int16 keepPreference = -1;

				setAudioMixAhead(&keepPreference, false);
			}

			_audioSystem->updateAudioDevice();
			updateMusic();
			_engine->_soundsUpdateTimer.stop();

			return true;
		}
	}

	_engine->_soundsUpdateTimer.stop();

	return false;
}

bool Sounds::soundsUpdateThrottled(const bool *force) {
	if (!isAudioActive())
		return false;

	uint now = g_system->getMillis();

	if (!*force && now - _lastThrottledUpdateMs <= kThrottledUpdateIntervalMs && now >= _lastThrottledUpdateMs)
		return false;

	_lastThrottledUpdateMs = now;

	// The disasm only serves the Miles driver here
	return true;
}

void Sounds::startSounds(bool startMusicNow) {
	if (!_soundsEnabled || _audioSystem)
		return;

	_audioSystem = new AudioSystem(this);

	if (_audioSystem->_audioAvailable) {
		restoreMasterGain();

		// The demo loads its music map together with the audio system
		if (_musicEnabled && _musicCategoryEnabled && (startMusicNow || _engine->isDemo()))
			startMusic(-1);

		if (_engine->getGamePlatform() == Common::kPlatformPS2) {
			_loadingLoopCue = CSoundCue::newSoundCue(this, LString("@LOADLOOP.SCX"), 1, nullptr);

			// PS2 keeps this cue out of the cue table, so it is never saved, released or updated with the others
			if (_loadingLoopCue)
				_loadingLoopCue->unlinkFromCueList();
		}
	} else {
		// The EXE keeps the audio settings in static data, so saves still read and write them without a device
		_musicEnabled = false;
		_soundsEnabled = false;
		_sfxEnabled = false;
		_voiceEnabled = false;
		_musicCategoryEnabled = false;
	}
}

bool Sounds::isAudioActive() const {
	return _audioSystem && _audioSystem->_audioAvailable;
}

void Sounds::shutdownSounds() {
	if (isAudioActive()) {
		stopMusic();

		// A fresh object has the state of the reset module
		if (_engine->getGamePlatform() == Common::kPlatformPS2) {
			delete _musicPs2;
			_musicPs2 = new CMusicPs2(this);
		}

		releaseAllSounds();

		// PS2 never frees it, but we must so that the engine can be restarted
		if (_loadingLoopCue) {
			CSoundCue::deleteSoundCue(_loadingLoopCue);
			_loadingLoopCue = nullptr;
		}

		_loadingLoopFade = 0;

		delete _audioSystem;
		_audioSystem = nullptr;
	}
}

void Sounds::releaseAllSounds() {
	// PS2 only deletes the cues in the cue table
	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		_soundMap.clear();
		CSoundCue::releaseAllCues(this);

		return;
	}

	Common::Array<uint16> handles;

	for (CSoundCue::SoundCueMap::iterator it = _soundMap.begin(); it != _soundMap.end(); ++it)
		handles.push_back(it->_key);

	Common::sort(handles.begin(), handles.end());

	for (uint i = 0; i < handles.size(); i++)
		stopSound(handles[i]);

	_soundMap.clear();
	CSoundCue::updateSoundCues(this);

	if (isAudioActive()) {
		_audioSystem->updateAudioDevice();
		_audioSystem->stopAllChannels();
	}

	CSoundCue::releaseAllCues(this);
}

void Sounds::releaseUnlockedSounds() {
	CSoundCue::releaseUnlockedCues(this);
}

void Sounds::soundCueLipCallback(const char *soundName, CSoundCue *cue) {
	LipData *lipData = nullptr;
	int lipSize = LipData::readFile(_engine, soundName, &lipData);

	if (lipSize)
		cue->setLipData(lipData, lipSize);
}

void Sounds::saveSounds(SaveRestoreAccess *access) {
	access(&_soundCounter, sizeof(_soundCounter));
	_audioSystem->saveAudioSettings(access);
	CSoundCue::saveCues(this, access);
}

void Sounds::restoreSounds(SaveRestoreAccess *access) {
	access(&_soundCounter, sizeof(_soundCounter));
	_audioSystem->restoreAudioSettings(access);
	CSoundCue::restoreCues(this, access, &_soundMap);
}

void Sounds::setSoundSystemEnabled(bool enabled) {
	_soundsEnabled = enabled;
}

void Sounds::setMusicSystemEnabled(bool enabled) {
	_musicEnabled = enabled;
}

uint Sounds::loadSound(const char *soundName, int category) {
	return loadSound(LString(soundName), category);
}

uint Sounds::loadSound(const LString &soundName, int category) {
	if (!isAudioActive())
		return 0;

	uint dotPosition = soundName.find('.');
	bool hasDot = dotPosition != LString::npos;
	LString fullName;

	if (!hasDot || !LString(soundName.c_str() + dotPosition + 1).equalsIgnoreCase("SND"))
		fullName = LString("@") + soundName;
	else
		fullName = soundName;

	const char *defaultExtension = ".wav";
	int cueCategory = 0;

	switch (category) {
	// The demo has no category enable flags
	case 1:
		if (!_engine->isDemo() && !_sfxEnabled)
			return 0;

		cueCategory = 1;
		defaultExtension = ".aif";

		break;

	case 2:
		if (!_engine->isDemo() && !_voiceEnabled)
			return 0;

		cueCategory = 2;

		break;

	case 3:
		if (!_engine->isDemo() && !_musicCategoryEnabled)
			return 0;

		cueCategory = 3;

		break;

	default:
		break;
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		defaultExtension = ".SCX";

	if (!hasDot)
		fullName += defaultExtension;

	CSoundCue *cue = CSoundCue::newSoundCue(this, fullName, cueCategory, nullptr);

	if (!cue)
		return 0;

	if (category == 2) {
		LipData *lipData = nullptr;
		int lipSize = LipData::readFile(_engine, soundName.c_str(), &lipData);

		if (lipSize)
			cue->setLipData(lipData, lipSize);
	}

	return registerSound(cue);
}

void Sounds::playSound(const LString &soundName, int volume) {
	if (!isAudioActive() || !_sfxEnabled)
		return;

	float gain = soundVolumeToGain(volume);
	LString fullName(soundName);
	LString extension;

	LStringUtil::getExtension(fullName, extension);
	fullName.clear();

	if (extension != "snd" && extension != "SND")
		fullName += '@';

	fullName += soundName;

	if (soundName.find('.') == LString::npos)
		fullName += _engine->getGamePlatform() == Common::kPlatformPS2 ? ".SCX" : ".aif";

	CSoundCue *cue = CSoundCue::newSoundCue(this, fullName, 1, nullptr);

	if (cue) {
		cue->setVolume(gain);
		cue->start(true);
	}
}

void Sounds::playSoundFrom(const LString &soundName, float x, float y, float z, int16 volume) {
	// PS2 ignores positional sounds for two frames after a set change
	if (_engine->getGamePlatform() == Common::kPlatformPS2 && _engine->isSetChanging())
		return;

	uint soundHandle = loadSound(soundName, 1);

	if (soundHandle) {
		startSound(soundHandle, 1, false, volume);
		setSoundPosition(soundHandle, x, y, z);
		unregisterSound((uint16)soundHandle);
	}
}

void Sounds::playLoadedSoundFrom(uint soundHandle, float x, float y, float z, bool loop, int16 volume) {
	startSound(soundHandle, 0, loop, volume);
	setSoundPosition(soundHandle, x, y, z);
}

void Sounds::startSound(uint soundHandle, int startMode, bool loop, int volume) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return;

	// The demo has no category enable flags
	if (!_engine->isDemo()) {
		if (cue->_category == 1) {
			if (!_sfxEnabled)
				return;
		} else if (cue->_category == 2 && !_voiceEnabled) {
			return;
		}
	}

	if (loop)
		cue->_flags |= CSoundCue::kFlagLoop;
	else
		cue->_flags &= ~CSoundCue::kFlagLoop;

	cue->setVolume(soundVolumeToGain(volume));

	int attempts = 0;

	if (cue->start(startMode != 0))
		return;

	// The demo makes a single attempt
	if (_engine->isDemo())
		return;

	if (cue->_category == 2) {
		while (attempts < kMaxStartAttempts) {
			CSoundCue::stopCueToFreeChannel(this, true, true);
			attempts++;

			if (cue->start(startMode != 0))
				return;
		}
	} else {
		while (attempts < kMaxStartAttempts) {
			CSoundCue::stopCueToFreeChannel(this, true, false);
			attempts++;

			if (cue->start(startMode != 0))
				return;
		}
	}
}

void Sounds::stopSound(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	// The demo flags the cue and stops it; the next update deletes it
	if (_engine->isDemo()) {
		if (cue) {
			cue->_flags |= CSoundCue::kFlagReleaseWhenDone;
			cue->stop();
		}

		releaseSoundWhenDone(soundHandle);

		return;
	}

	if (cue)
		CSoundCue::deleteSoundCue(cue);

	unregisterSound((uint16)soundHandle);
}

void Sounds::stopSoundPlayback(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (cue) {
		if (!_engine->isDemo() && (cue->_flags & CSoundCue::kFlagReleaseWhenDone) != 0)
			unregisterSound((uint16)soundHandle);

		cue->stop();
	}
}

void Sounds::releaseSoundWhenDone(uint soundHandle) {
	// The demo only drops the handle; the cue is deleted by the update once it is done
	if (_engine->isDemo()) {
		unregisterSound((uint16)soundHandle);

		return;
	}

	CSoundCue *cue = findSoundCue(soundHandle);

	if (cue) {
		if (cue->getState() == CSoundCue::kStatePlaying) {
			cue->_flags |= CSoundCue::kFlagReleaseWhenDone;
			unregisterSound((uint16)soundHandle);

			return;
		}

		CSoundCue::deleteSoundCue(cue);
	}

	unregisterSound((uint16)soundHandle);
}

void Sounds::pauseSound(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (cue)
		cue->pause();
}

void Sounds::resumeSound(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (cue)
		cue->resume();
}

bool Sounds::isSoundPlaying(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	return cue && cue->getState() == CSoundCue::kStatePlaying;
}

bool Sounds::isSoundPlayingNoFiniteLoop(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return false;

	// The demo channels have no finite loop count test
	if (_engine->isDemo())
		return cue->getState() == CSoundCue::kStatePlaying;

	return cue->getState() == CSoundCue::kStatePlaying && !cue->hasFiniteLoopCount();
}

bool Sounds::isSoundPaused(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	return cue && cue->getState() == CSoundCue::kStatePaused;
}

void Sounds::setSoundPosition(uint soundHandle, float x, float y, float z) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return;

	if (cue->_category == 1) {
		if (!_sfxEnabled)
			return;
	} else if (cue->_category == 2 && !_voiceEnabled) {
		return;
	}

	cue->setPosition(x, y, z);
}

bool Sounds::setSoundVolume(uint soundHandle, int volume) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return false;

	cue->setVolume(soundVolumeToGain(volume));

	return true;
}

bool Sounds::getSoundVolume(uint soundHandle, int *volume) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return false;

	*volume = soundGainToVolume(cue->getVolume());

	return true;
}

int Sounds::getSoundPosition(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return 0;

	return cue->getPositionMs(nullptr);
}

int Sounds::getSoundLength(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return 0;

	// The disasm returns the handle if the channel writes no length
	int length = (int)soundHandle;

	cue->getPositionMs(&length);

	return length;
}

LipData *Sounds::getSoundLipData(uint soundHandle) {
	CSoundCue *cue = findSoundCue(soundHandle);

	if (!cue)
		return nullptr;

	return cue->_lipData;
}

bool Sounds::setCategoryVolume(int category, int volume) {
	if (!isAudioActive())
		return false;

	float gain = soundVolumeToGain(volume);
	bool isDemo = _engine->isDemo();

	// The demo has no category enable flags
	switch (category) {
	case 0:
		if (isDemo || _soundsEnabled)
			_audioSystem->setMasterGain(gain);

		return true;

	case 1:
		if (isDemo || _sfxEnabled)
			_audioSystem->setCategoryGain(1, gain);

		return true;

	case 2:
		if (isDemo || _voiceEnabled)
			_audioSystem->setCategoryGain(2, gain);

		return true;

	case 3:
		if (isDemo || _musicCategoryEnabled) {
			_audioSystem->setCategoryGain(3, gain);
			setMusicVolume(gain);
		}

		return true;

	default:
		break;
	}

	return false;
}

int Sounds::getCategoryVolume(int category) {
	if (!isAudioActive())
		return 0;

	float gain = 0.0f;

	switch (category) {
	case 0:
		// The demo reports nothing for the master group
		if (!_engine->isDemo())
			gain = _audioSystem->getMasterGain();

		break;

	case 1:
		gain = _audioSystem->getCategoryGain(1);

		break;

	case 2:
		gain = _audioSystem->getCategoryGain(2);

		break;

	case 3:
		gain = _audioSystem->getCategoryGain(3);

		break;

	default:
		break;
	}

	return soundGainToVolume(gain);
}

void Sounds::setCategoryEnabled(int category, bool enabled) {
	if (enabled) {
		if (!_soundsEnabled)
			return;
	} else {
		setCategoryVolume(category, 0);
	}

	switch (category) {
	case 1:
		_sfxEnabled = enabled;

		break;

	case 2:
		_voiceEnabled = enabled;

		break;

	case 3:
		if (enabled && _musicEnabled) {
			startMusic(_savedMusicState);
			_musicCategoryEnabled = enabled;
		} else {
			if (_musicCategoryEnabled)
				_savedMusicState = stopMusic();

			_musicCategoryEnabled = enabled;
		}

		break;

	default:
		break;
	}
}

void Sounds::setVoiceFxEnabled(int enabled) {
	if (_audioSystem)
		_audioSystem->setGlobalReverbEnabled(enabled);

	_voiceFxEnabled = (byte)enabled;
}

void Sounds::setReverb(int preset, float *param1, float *param2, float *param3, float *param4, float *param5) {
	if (!_voiceFxEnabled)
		return;

	if (!isAudioActive())
		return;

	if (preset < 0 || preset >= kReverbPresetCount) {
		warning("Sounds::setReverb(): the preset %d is out of range", preset);

		return;
	}

	_audioSystem->setGlobalReverb(kReverbPresets[preset], param1, param2, param3, param4, param5);
}

void Sounds::setSonyReverb(int type, int depth, int delay, int feedback) {
	if (!_voiceFxEnabled) {
		LConsoleServices *console = _engine->getConsoleServices();

		if (console) {
			console->logError("***************************\n");
			console->logError("SoundsSetReverb: !bReverbOk\n");
			console->logError("***************************\n");
		}

		return;
	}

	if (isAudioActive())
		_audioSystem->setSonyReverb(type, depth, delay, feedback);
}

void Sounds::restoreMasterGain() {
	if (isAudioActive())
		_audioSystem->setMasterGain(kDefaultMasterGain);
}

void Sounds::lowerMasterGainForMovie() {
	if (isAudioActive())
		_audioSystem->setMasterGain(kMovieMasterGain);
}

void Sounds::setAudioMixAhead(const int16 *mixAhead, bool store) {
	// The demo has no mix-ahead control
	if (_engine->isDemo())
		return;

	if (*mixAhead < 0) {
		uint16 storedPreference = (uint16)_audioMixAhead;

		if (_audioSystem)
			_audioSystem->setAudioDevicePreference(&storedPreference);

		return;
	}

	uint16 newPreference = (uint16)*mixAhead;

	if (_audioSystem)
		_audioSystem->setAudioDevicePreference(&newPreference);

	if (store)
		_audioMixAhead = *mixAhead;
}

void Sounds::setSoundListener(CNode *listener) {
	if (_audioSystem)
		_audioSystem->setAudioListener(listener);
}

void Sounds::suspendAudio() {
	if (isAudioActive()) {
		if (!_audioSystem->_digitalHandleReleased) {
			if (_audioSystem->_device)
				_audioSystem->_device->releaseDigitalHandle();

			_audioSystem->_digitalHandleReleased = true;
		}
	}
}

void Sounds::resumeAudio() {
	if (isAudioActive()) {
		if (_audioSystem->_digitalHandleReleased) {
			if (_audioSystem->_device)
				_audioSystem->_device->reacquireDigitalHandle();

			_audioSystem->_digitalHandleReleased = false;
		}
	}
}

void Sounds::pauseAll() {
	if (isAudioActive()) {
		if (_audioSystem->_device)
			_audioSystem->_device->pauseAll();
	}
}

void Sounds::resumeAll() {
	if (isAudioActive()) {
		if (_audioSystem->_device)
			_audioSystem->_device->resumeAll();
	}
}

void Sounds::stopAllSounds() {
	stopAllMusic();
	releaseAllSounds();
}

void Sounds::updateMusic() {
	if (_musicActive) {
		if (_demoMusic)
			_demoMusic->updateStreams();
		else if (_engine->getGamePlatform() == Common::kPlatformPS2)
			_musicPs2->updateStates();
		else
			_streamScript->updateStreams();
	}
}

void Sounds::startMusic(int newState) {
	if (_musicActive)
		return;

	if (_demoMusic) {
		if (!_demoMusic->loadMusicMap())
			return;

		_musicActive = true;

		if (newState != -1) {
			bool immediate = false;

			setMusicState(newState, -1, immediate);
		}

		return;
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		if (!_musicPs2->openMusicTable()) {
			_musicPs2->clearAudioSearchDirs();

			return;
		}

		_musicActive = true;

		if (newState != -1) {
			bool immediate = false;

			setMusicState(newState, -1, immediate);
		}

		return;
	}

	const char *setDirectory = _musicSet ? "Mego\\" : "Spago\\";
	Common::SeekableReadStream *secrets = _streamScript->openStream("Textures\\secrets.m4b");

	if (secrets) {
		LString directory = LString("Textures\\") + setDirectory;

		_streamScript->pushAudioSearchDir(directory.c_str());

		delete secrets;
	}

	const char *cdPath = _engine->getPathingFileServices()->getCdPath();

	if (cdPath) {
		LString directory = LString(cdPath) + "\\" + "Textures\\";

		_streamScript->pushAudioSearchDir(directory.c_str());

		directory += setDirectory;

		_streamScript->pushAudioSearchDir(directory.c_str());
	} else {
		_streamScript->pushAudioSearchDir("Textures\\");
	}

	const char *musicScript = "FullMonkeyMap.imt";

	if (!_streamScript->hasAudioFile(musicScript)) {
		int disc = _engine->getDiscNumber();

		if (disc == 1)
			musicScript = "FullMonkeyMap1.imt";
		else if (disc == 2)
			musicScript = "FullMonkeyMap2.imt";
	}

	if (!_streamScript->openMusicScript(musicScript)) {
		_streamScript->clearAudioSearchDirs();

		return;
	}

	_musicActive = true;

	if (newState != -1) {
		bool immediate = false;

		setMusicState(newState, -1, immediate);
	}
}

int Sounds::stopMusic() {
	if (_musicActive) {
		if (_demoMusic) {
			_demoMusic->closeMusicMap();
		} else if (_engine->getGamePlatform() == Common::kPlatformPS2) {
			_musicPs2->closeMusicTable();
			_musicPs2->clearAudioSearchDirs();
		} else {
			_streamScript->closeStreamScript();
			_streamScript->clearAudioSearchDirs();
		}

		_musicActive = false;
	}

	return _musicState;
}

void Sounds::setMusicVolume(float volume) {
	if (_musicActive) {
		_musicVolume = volume;

		if (_demoMusic)
			_demoMusic->setMasterVolume(volume);
		else if (_engine->getGamePlatform() == Common::kPlatformPS2)
			_musicPs2->setMusicTableVolume(volume);
		else
			_streamScript->setMusicStreamVolume(volume);
	}
}

void Sounds::setMusicState(int newState, int startMs, const bool &immediate) {
	if (_demoMusic) {
		if (_musicActive) {
			_musicState = newState;

			_demoMusic->startCue(newState);
		}

		return;
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		if (_musicActive && !_musicPs2->isSameFile(newState, _musicState)) {
			_musicState = newState;

			_musicPs2->startState(newState, startMs, immediate);
		}

		return;
	}

	if (_musicActive) {
		uint stateId = (uint)newState;

		_musicState = newState;

		_streamScript->startMusicStream(stateId, startMs, immediate);
	}
}

void Sounds::saveMusicState(SaveRestoreAccess *access) {
	access(&_musicVolume, sizeof(_musicVolume));
	access(&_musicState, sizeof(_musicState));
	access(&_pushedMusicState, sizeof(_pushedMusicState));
	access(&_pushedMusicPosition, sizeof(_pushedMusicPosition));
}

void Sounds::restoreMusicState(SaveRestoreAccess *access) {
	access(&_musicVolume, sizeof(_musicVolume));

	setMusicVolume(_musicVolume);

	access(&_musicState, sizeof(_musicState));
	access(&_pushedMusicState, sizeof(_pushedMusicState));
	access(&_pushedMusicPosition, sizeof(_pushedMusicPosition));

	// The demo restarts the saved cue at load time
	if (_demoMusic && _musicState != 0)
		_demoMusic->startCue(_musicState);
}

void Sounds::pauseMusicStreams() {
	if (_demoMusic) {
		if (_musicActive)
			_demoMusic->pauseStreams();

		return;
	}

	// PS2 has no music pause
	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return;

	if (_musicActive)
		_streamScript->pauseStreams();
}

void Sounds::resumeMusicStreams() {
	if (_demoMusic) {
		if (_musicActive)
			_demoMusic->resumeStreams();

		return;
	}

	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return;

	if (_musicActive)
		_streamScript->resumeStreams();
}

void Sounds::stopAllMusic() {
	if (_musicActive) {
		_musicState = 0;

		if (_demoMusic)
			_demoMusic->closeAllStreams();
		else if (_engine->getGamePlatform() == Common::kPlatformPS2)
			_musicPs2->closeAllStates();
		else
			_streamScript->closeAllStreams();
	}
}

void Sounds::selectMusicSet(int16 set) {
	if (_musicActive && _musicSet == (set != 0))
		return;

	_musicSet = set != 0;

	int stoppedState = stopMusic();

	startMusic(stoppedState);
}

bool Sounds::consumeMusicFlags(const uint &musicId, bool &looped, bool &ended) {
	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		int stateId = (int)musicId;

		return _musicPs2->consumeStateFlags(stateId, looped, ended);
	}

	return _streamScript->consumeMusicStreamFlags(musicId, looped, ended);
}

void Sounds::pushMusicState(int musicId, const bool &immediate) {
	if (_musicActive) {
		uint stateId = (uint)_musicState;
		bool relative = false;

		_pushedMusicState = _musicState;

		if (_engine->getGamePlatform() == Common::kPlatformPS2)
			_pushedMusicPosition = _musicPs2->getStatePositionMs(_musicState, relative);
		else
			_pushedMusicPosition = _streamScript->getMusicStreamPositionMs(stateId, relative);

		setMusicState(musicId, -1, immediate);
	}
}

void Sounds::popMusicState(const bool &immediate) {
	if (_musicActive && _pushedMusicState != -1) {
		setMusicState(_pushedMusicState, _pushedMusicPosition, immediate);

		_pushedMusicState = -1;
	}
}

void Sounds::flushMusicStateStack() {
	_pushedMusicState = -1;
}

int Sounds::getMusicPositionMs(const uint &musicId) {
	bool relative = true;

	if (_engine->getGamePlatform() == Common::kPlatformPS2) {
		int stateId = (int)musicId;

		return _musicPs2->getStatePositionMs(stateId, relative);
	}

	return _streamScript->getMusicStreamPositionMs(musicId, relative);
}

void Sounds::killMusicAndWait(bool loadingLoop) {
	bool immediate = false;
	bool force = true;
	LConsoleServices *console = _engine->getConsoleServices();

	setMusicState(0, -1, immediate);

	if (console)
		console->logDebug("SOUNDS WAITING FOR MUSIC TO FADE!!\n");

	if (loadingLoop && _musicPs2->getStreamPosition())
		fadeLoadingLoop(1);

	while (_musicPs2->getStreamPosition())
		soundsUpdate(&force);

	console = _engine->getConsoleServices();

	if (console)
		console->logDebug("SOUNDS DONE WAITING FOR MUSIC FADE.\n");
}

void Sounds::killMusicForSet(const char *setName) {
	// The first set change only arms the SPU clear
	if (_engine->armSpuClear())
		clearSpuMemory();

	killMusicAndWait(setName && scumm_stricmp(setName, "blank.set") != 0);
}

void Sounds::stopAllForLoad() {
	stopAllMusic();
	releaseAllSounds();

	if (_engine->isSpuClearArmed())
		clearSpuMemory();

	// The IOP closes streams asynchronously and waits, while ours close immediately
	_musicPs2->closeStream();
}

void Sounds::clearSpuMemory() {
	if (!isAudioActive() || !_audioSystem->_device)
		return;

	CWaveAudioDevice *device = dynamic_cast<CWaveAudioDevice *>(_audioSystem->_device);

	if (!device)
		return;

	for (uint i = 0; i < device->_channels.size(); i++) {
		if (device->_channels[i])
			device->_channels[i]->endSpuSample();
	}
}

void Sounds::fadeLoadingLoop(int direction) {
	if (!_loadingLoopCue)
		return;

	if (direction != 1 && direction != -1)
		return;

	if (direction == -1 && _loadingLoopCue->getState() != CSoundCue::kStatePlaying)
		return;

	_loadingLoopFade = direction;
	_loadingLoopFadeMs = g_system->getMillis();

	if (_loadingLoopFade == 1) {
		_loadingLoopCue->_flags |= CSoundCue::kFlagLoop;
		_loadingLoopCue->setVolume(0.0f);
		_loadingLoopCue->play(false);
	}
}

void Sounds::updateLoadingLoop() {
	if (!_loadingLoopFade)
		return;

	uint now = g_system->getMillis();
	float step = (float)(now - _loadingLoopFadeMs) / 1000.0f / kLoadingLoopFadeSeconds;
	float volume = _loadingLoopCue->_volume;

	if (_loadingLoopFade > 0) {
		volume += step;

		if (!(volume <= 1.0f))
			volume = 1.0f;

		if (volume == 1.0f)
			_loadingLoopFade = 0;
	} else if (_loadingLoopFade == -1) {
		// The fade-out starts one update late
		_loadingLoopFade = -2;
	} else {
		volume -= step;

		if (volume < 0.0f)
			volume = 0.0f;

		if (volume == 0.0f) {
			_loadingLoopCue->stop();
			_loadingLoopFade = 0;
		}
	}

	_loadingLoopCue->setVolume(volume);

	if (_loadingLoopFade)
		_loadingLoopFadeMs = now;
}

void Sounds::setStereoAudio(bool stereo) {
	if (!_audioSystem)
		return;

	CWaveAudioDevice *device = dynamic_cast<CWaveAudioDevice *>(_audioSystem->_device);

	if (device)
		device->setSpuStereo(stereo);
}

} // End of namespace Grim
