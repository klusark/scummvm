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

#ifndef GRIM_EMI_SOUNDS_H
#define GRIM_EMI_SOUNDS_H

#include "common/scummsys.h"

#include "engines/grim/emi/sound/csoundcue.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class AudioSystem;
class CMusicPs2;
class CNode;
class DemoMusicScript;
class EmiEngine;
class LResource;
class StreamScript;
struct LipData;

class Sounds {
public:
	explicit Sounds(EmiEngine *engine);
	~Sounds();

	EmiEngine *getEngine() const;
	AudioSystem *getAudioSystem() const;
	int16 getAudioMixAhead() const;

	static LResource *createWaveSoundResource();
	static float soundVolumeToGain(int volume);
	static int soundGainToVolume(float gain);

	uint registerSound(CSoundCue *cue);
	void unregisterSound(uint soundHandle);
	CSoundCue *findSoundCue(uint soundHandle);
	bool isCueRegistered(CSoundCue *cue);
	bool soundsUpdate(const bool *force);
	bool soundsUpdateThrottled(const bool *force);
	void startSounds(bool startMusicNow);
	void shutdownSounds();
	void releaseAllSounds();
	void releaseUnlockedSounds();
	void soundCueLipCallback(const char *soundName, CSoundCue *cue);
	void saveSounds(SaveRestoreAccess *accessFunc);
	void restoreSounds(SaveRestoreAccess *accessFunc);
	void setSoundSystemEnabled(bool enabled);
	void setMusicSystemEnabled(bool enabled);
	uint getLoadFailure() const;
	uint loadSound(const char *soundName, int category);
	uint loadSound(const LString &soundName, int category);
	void playSound(const LString &soundName, int volume);
	void playSoundFrom(const LString &soundName, float x, float y, float z, int16 volume);
	void playLoadedSoundFrom(uint soundHandle, float x, float y, float z, bool loop, int16 volume);
	void startSound(uint soundHandle, int startMode, bool loop, int volume);
	void stopSound(uint soundHandle);
	void stopSoundPlayback(uint soundHandle);
	void releaseSoundWhenDone(uint soundHandle);
	void pauseSound(uint soundHandle);
	void resumeSound(uint soundHandle);
	bool isSoundPlaying(uint soundHandle);
	bool isSoundPlayingNoFiniteLoop(uint soundHandle);
	bool isSoundPlayingPs2(uint soundHandle, const char *source);
	bool isSoundPaused(uint soundHandle);
	void setSoundPosition(uint soundHandle, float x, float y, float z);
	bool setSoundVolume(uint soundHandle, int volume);
	bool getSoundVolume(uint soundHandle, int *volume);
	int getSoundPosition(uint soundHandle);
	int getSoundLength(uint soundHandle);
	LipData *getSoundLipData(uint soundHandle);
	bool setCategoryVolume(int category, int volume);
	int getCategoryVolume(int category);
	void setCategoryEnabled(int category, bool enabled);
	void setVoiceFxEnabled(int enabled);
	void setReverb(int preset, float *param1, float *param2, float *param3, float *param4, float *param5);
	void setSonyReverb(int type, int depth, int delay, int feedback);
	void restoreMasterGain();
	void lowerMasterGainForMovie();
	void setAudioMixAhead(const int16 *mixAhead, bool store);
	void setSoundListener(CNode *listener);
	void suspendAudio();
	void resumeAudio();
	void pauseAll();
	void resumeAll();
	void stopAllSounds();

	void updateMusic();
	void startMusic(int newState);
	int stopMusic();
	void setMusicVolume(float volume);
	void setMusicState(int newState, int startMs, const bool &immediate);
	void saveMusicState(SaveRestoreAccess *accessFunc);
	void restoreMusicState(SaveRestoreAccess *accessFunc);
	void pauseMusicStreams();
	void resumeMusicStreams();
	void stopAllMusic();
	void selectMusicSet(int16 set);
	bool consumeMusicFlags(const uint &musicId, bool &looped, bool &ended);
	void pushMusicState(int musicId, const bool &immediate);
	void popMusicState(const bool &immediate);
	void flushMusicStateStack();
	int takePushedMusicState();
	int getMusicPositionMs(const uint &musicId);
	void killMusicAndWait(bool loadingLoop);
	void killMusicForSet(const char *setName);
	void stopAllForLoad();
	void clearSpuMemory();
	void fadeLoadingLoop(int direction);
	void updateLoadingLoop();
	void setStereoAudio(bool stereo);

private:
	friend class CSoundCue;

	static const float kDbStep;
	static const float kInvDbStep;
	static const float kUnityGain;
	static const float kDefaultMasterGain;
	static const float kMovieMasterGain;

	bool isAudioActive() const;

	EmiEngine *_engine;
	AudioSystem *_audioSystem;
	CMusicPs2 *_musicPs2;
	StreamScript *_streamScript;
	DemoMusicScript *_demoMusic;
	CSoundCue::SoundCueMap _soundMap;
	int _soundCounter;
	int16 _audioMixAhead;
	bool _soundsEnabled;
	bool _musicEnabled;
	bool _musicCategoryEnabled;
	bool _voiceEnabled;
	bool _sfxEnabled;
	byte _voiceFxEnabled;
	int _savedMusicState;
	bool _musicActive;
	int _musicState;
	bool _musicSet;
	float _musicVolume;
	int _pushedMusicState;
	int _pushedMusicPosition;
	uint _lastUpdateMs;
	uint _lastThrottledUpdateMs;
	CSoundCue *_loadingLoopCue;
	int _loadingLoopFade;
	uint _loadingLoopFadeMs;

	CSoundCue *_firstCue;
	CSoundCue *_lastCue;
	int _cueCount;
	uint _createdCount;
	uint _destroyedCount;
	uint _updatedCount;
};

} // End of namespace Grim

#endif
