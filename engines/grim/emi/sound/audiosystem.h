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

#ifndef GRIM_EMI_AUDIOSYSTEM_H
#define GRIM_EMI_AUDIOSYSTEM_H

#include "common/scummsys.h"

#include "engines/grim/emi/nodes/cnoderef.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class CAudioChannel;
class CAudioDevice;
class CNode;
class CSoundCue;
class EmiEngine;
class EMIMilesDriver;
class Sounds;

class AudioSystem {
public:
	explicit AudioSystem(Sounds *sounds);
	~AudioSystem();

	void *operator new(size_t size);
	void operator delete(void *block);

	bool createAudioDevice();
	void destroyAudioDevice();
	void updateAudioDevice();
	CAudioChannel *allocateAudioChannel(CSoundCue *cue);
	void setAudioListener(CNode *node);
	bool isCueInRange(CSoundCue *cue);
	float setMasterGain(float gain);
	float getMasterGain();
	float setCategoryGain(int category, float gain);
	float getCategoryGain(int category);
	void setGlobalReverbEnabled(int enabled);
	bool setGlobalReverb(int preset, const float *first, const float *second, const float *third, const float *fourth, const float *fifth);
	bool getGlobalReverb(int *preset, float *first, float *second, float *third, float *fourth, float *fifth);
	bool setSonyReverb(int type, int depth, int delay, int feedback);
	bool setAudioDevicePreference(const uint16 *preference);
	void stopAllChannels();
	bool saveAudioSettings(SaveRestoreAccess *accessFunc);
	bool restoreAudioSettings(SaveRestoreAccess *accessFunc);
	EMIMilesDriver *getMilesDriver();

	int16 _refCount;
	int _debugLevel;
	bool _audioAvailable;
	bool _digitalHandleReleased;
	CAudioDevice *_device;
	int _reverbPreset;
	float _reverbValues[5];
	byte _reverbEnabled;
	int _sonyReverbType;
	int _sonyReverbDepth;
	int _sonyReverbDelay;
	int _sonyReverbFeedback;
	CNodeRef _listener;
	float _masterGain;
	float _categoryGain[3];
	uint16 _devicePreference;

private:
	Sounds *_sounds;
	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
