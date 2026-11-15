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

#ifndef GRIM_EMI_CSOUNDCUE_H
#define GRIM_EMI_CSOUNDCUE_H

#include "common/hashmap.h"
#include "common/scummsys.h"

#include "engines/grim/emi/sound/cnoderef_soundcue.h"
#include "engines/grim/emi/math/cvector.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/sound/lsound.h"
#include "engines/grim/emi/sound/soundposition.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class CAudioChannel;
class CNode;
class CSnd;
class CWaveSound;
class Sounds;
struct LipData;

class CSoundCue {
public:
	enum {
		kStateNone = 0,
		kStateWaiting = 1,
		kStatePlaying = 2,
		kStateStopped = 3,
		kStatePaused = 4,
		kStateFinished = 5,
		kStateStarted = 6
	};

	static const uint kFlagForceChannel = 0x00000001;
	static const uint kFlagLoop = 0x00000004;
	static const uint kFlagStartInRange = 0x00000008;
	static const uint kFlagAlwaysInRange = 0x00000020;
	static const uint kFlagPositional = 0x20000000;
	static const uint kFlagInRange = 0x40000000;
	static const uint kFlagReleaseWhenDone = 0x80000000;

	typedef Common::HashMap<uint, CSoundCue *> SoundCueMap;

	CSoundCue(Sounds *sounds, LResourceHandle<CSnd> *sndHandle, CNode *node);
	CSoundCue(Sounds *sounds, const LString &name, CNode *node);
	virtual ~CSoundCue();

	void *operator new(size_t size);
	void operator delete(void *block);

	virtual bool play(bool releaseWhenDone);
	virtual bool startPlay(bool startPaused);
	virtual bool startPlay();
	virtual bool update();

	static CSoundCue *newSoundCue(Sounds *sounds, LResourceHandle<CSnd> *sndHandle, int category, CNode *node);
	static CSoundCue *newSoundCue(Sounds *sounds, const LString &name, int category, CNode *node);
	static void deleteSoundCue(CSoundCue *cue);
	static void releaseAllCues(Sounds *sounds);
	static void releaseUnlockedCues(Sounds *sounds);
	static void updateSoundCues(Sounds *sounds);
	static bool stopCueToFreeChannel(Sounds *sounds, bool finiteLoopsFirst, bool anyNonLooping);
	static bool saveCues(Sounds *sounds, SaveRestoreAccess *accessFunc);
	static bool restoreCues(Sounds *sounds, SaveRestoreAccess *accessFunc, SoundCueMap *cueMap, const void *lipData, int lipSize);

	int getSoundType();
	bool start(bool releaseWhenDone);
	void stop();
	void pause();
	void resume();
	float getPriority();
	float getVolume();
	float getPan();
	float getCutoffNear();
	float getCutoffFar();
	float getCategoryGain();
	void setVolume(float newVolume);
	void setPan(float newPan);
	bool setPosition(float x, float y, float z);
	bool setPosition(const SoundPosition &newPosition);
	bool updateSpatialization();
	bool hasFiniteLoopCount();
	bool getPosition(CVector *outPosition);
	int initDefaults(CNode *node);
	void initFromSnd(CSnd *snd, CNode *node);
	void setLipData(const void *data, uint size);
	void releaseChannel();
	void unlinkFromCueList();
	int getState();
	int getPositionMs(int *totalMs);
	void setPositionMs(int milliseconds);
	bool save(SaveRestoreAccess *accessFunc);

	CSoundCue *_prev;
	CSoundCue *_next;
	CWaveSound *_sound;
	float _priority;
	float _volume;
	float _mvol;
	float _pan;
	SoundPosition _position;
	int _reserved48;
	float _freq;
	int _state;
	float _cutoffNear;
	float _cutoffFar;
	uint _flags;
	int _reserved72[5];
	CNodeRef_SoundCue _nodeRef;
	CVector _fixedPosition;
	LString _soundName;
	CAudioChannel *_channel;
	int _group;
	LipData *_lipData;
	uint _lipSize;
	int _handle;
	int _category;
	LResourceHandle<LSound> _soundHandle;

private:
	Sounds *_sounds;
};

} // End of namespace Grim

#endif
