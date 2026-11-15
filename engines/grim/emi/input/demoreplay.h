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

#ifndef GRIM_EMI_DEMOREPLAY_H
#define GRIM_EMI_DEMOREPLAY_H

#include "common/scummsys.h"

#include "engines/grim/emi/files/lfile.h"

namespace Grim {

class EmiEngine;
struct ControlEntry;

class DemoReplay {
public:
	enum {
		kModeNone = 0,
		kModePlayback = 1,
		kModeRecord = 2,
		kModeFinished = 3
	};

	enum {
		kRecordNone = 0,
		kRecordKeyUp = 1,
		kRecordKeyDown = 2,
		kRecordSet = 3,
		kRecordFrame = 4,
		kRecordAxis = 5,
		kRecordVoice = 6,
		kRecordSeed = 7
	};

	DemoReplay(EmiEngine *engine, int replayMode, const char *name);
	~DemoReplay();

	void consumeKey(const ControlEntry *control, bool down);
	void consumeAxis(int index, float value);
	void consumeSet();
	void deliverEvents();
	void deliverKey();
	void readRecord();
	float consumeFrame();
	void recordFrame(float frameTime);
	void recordSeed(uint seed);
	uint consumeSeed();
	void recordVoice(bool playing);
	bool consumeVoice(const char *source);
	char recordTag() const;

	int _mode;

private:
	static const uint kNumControls = 137;
	static const uint kHidCodes[kNumControls];

	int findControlIndex(uint hidCode);

	EmiEngine *_engine;
	int _recordType;
	int _recordCount;
	int _frameNumber;
	float _frameTime;
	uint _keyCode;
	int _axisIndex;
	float _axisValue;
	bool _voicePlaying;
	uint _seed;
	LFile *_file;
};

} // End of namespace Grim

#endif
