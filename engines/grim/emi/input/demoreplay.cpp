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

#include "common/debug.h"
#include "common/endian.h"
#include "common/fs.h"
#include "common/memstream.h"
#include "common/textconsole.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/input/demoreplay.h"
#include "engines/grim/emi/input/joystick.h"
#include "engines/grim/emi/input/sputcontrol_v2.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/movie/movie.h"
#include "engines/grim/emi/sound/sounds.h"

namespace Grim {

static const char *const kDefaultReplayName = "Replay.log";
static const int kButtonCross = 131;
static const int kButtonSelect = 137;
static const int kButtonStart = 136;

const uint DemoReplay::kHidCodes[DemoReplay::kNumControls] = {
	0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
	0x27, 0x2D, 0x2E, 0x2A, 0x2B, 0x35, 0x14, 0x1A, 0x08, 0x15,
	0x17, 0x1C, 0x18, 0x0C, 0x12, 0x13, 0x2F, 0x30, 0x31, 0x04,
	0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33, 0x34,
	0x28, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37,
	0x38, 0x10, 0x11, 0x12, 0x2C, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
	0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x39, 0x53, 0x47,
	0x48, 0x62, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60,
	0x61, 0x63, 0x58, 0x57, 0x56, 0x55, 0x54, 0x52, 0x50, 0x4F,
	0x51, 0x49, 0x4A, 0x4B, 0x4E, 0x4D, 0x4C, 0x80, 0x81, 0x82,
	0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C,
	0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0x00,
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

DemoReplay::DemoReplay(EmiEngine *engine, int replayMode, const char *name) : _mode(replayMode), _engine(engine),
		_recordType(kRecordNone), _recordCount(0), _frameNumber(0), _frameTime(0.0f), _keyCode(0), _axisIndex(0), _axisValue(0.0f), _voicePlaying(false),
		_seed(0), _file(nullptr) {
	LFileServices *fileServices = _engine->getFileServices();
	LString demoName(name ? name : kDefaultReplayName);
	uint slash = demoName.find('/');

	if (slash != LString::npos)
		demoName.setChar('\\', slash);

	LString stateName(demoName);
	uint dot = stateName.rfind('.');

	if (dot != LString::npos)
		stateName.erase(dot);
	else
		warning("DemoReplay::DemoReplay(): the name '%s' has no extension", demoName.c_str());

	stateName += ".sav";

	if (replayMode == kModeRecord) {
		Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);

		_engine->_ps2StateWriter = &state;
		_engine->saveGameData(Movie::writePs2State);
		_engine->_ps2StateWriter = nullptr;

		LFile *stateFile = fileServices->open(stateName.c_str(), "wbc");

		if (stateFile) {
			stateFile->write(state.getData(), state.size());
			fileServices->close(stateFile, true);
		} else {
			warning("DemoReplay::DemoReplay(): could not create '%s'", Common::FSNode(Common::Path(stateName.c_str(), '\\')).getPath().toString().c_str());
		}

		_file = fileServices->open(demoName.c_str(), "wbc");

		if (!_file) {
			warning("DemoReplay::DemoReplay(): could not create '%s'", Common::FSNode(Common::Path(demoName.c_str(), '\\')).getPath().toString().c_str());
			_mode = kModeFinished;

			return;
		}

		recordSeed(Lua_V2::getClockUniqueUint());

		if (_engine->getConsoleServices())
			_engine->getConsoleServices()->logDebug("Replay log RECORDING\n");
	} else if (replayMode == kModePlayback) {
		LFile *stateFile = fileServices->openAndLoad(stateName.c_str(), "rb");

		if (!stateFile) {
			warning("DemoReplay::DemoReplay(): could not open '%s'", stateName.c_str());
			_mode = kModeFinished;

			return;
		}

		_engine->restoreGameDataFromRam((const byte *)stateFile->_memData, stateFile->_memSize);
		fileServices->close(stateFile, true);

		_file = fileServices->openAndLoad(demoName.c_str(), "rb");

		if (!_file) {
			warning("DemoReplay::DemoReplay(): could not open '%s'", demoName.c_str());
			_mode = kModeFinished;

			return;
		}

		debug("replay started: %s, %d bytes", demoName.c_str(), (int)_file->_memSize);

		readRecord();
		_engine->_random.seed(consumeSeed());
		_engine->getSounds()->fadeLoadingLoop(-1);

		if (_engine->getConsoleServices())
			_engine->getConsoleServices()->logDebug("Replay log PLAYBACK\n");
	}
}

DemoReplay::~DemoReplay() {
	if (_file)
		_engine->getFileServices()->close(_file, true);

	_file = nullptr;
}

void DemoReplay::consumeKey(const ControlEntry *control, bool down) {
	if (_mode == kModeNone || _mode == kModeFinished)
		return;

	int index = (int)(control - _engine->getControl()->_controlTable);
	uint hidCode = control->code;

	if (index >= 0 && index < (int)kNumControls)
		hidCode = kHidCodes[index];

	if (_mode == kModePlayback) {
		if (hidCode != _keyCode) {
			warning("DemoReplay::consumeKey(): the key does not match the recording, stopping at byte %d", _file->tell());
			_mode = kModeFinished;
		}

		if (down) {
			if (_recordType != kRecordKeyDown) {
				warning("DemoReplay::consumeKey(): a key press was not expected, stopping at byte %d", _file->tell());
				_mode = kModeFinished;
			}
		} else if (_recordType != kRecordKeyUp) {
			warning("DemoReplay::consumeKey(): a key release was not expected, stopping at byte %d", _file->tell());
			_mode = kModeFinished;
		}

		readRecord();

		return;
	}

	byte data[5];

	data[0] = down ? 'd' : 'u';
	WRITE_LE_UINT32(data + 1, hidCode);
	_file->write(data, sizeof(data));
}

void DemoReplay::consumeAxis(int index, float value) {
	if (_mode == kModeNone || _mode == kModeFinished)
		return;

	if (_mode == kModePlayback) {
		if (index != _axisIndex) {
			warning("DemoReplay::consumeAxis(): the stick does not match the recording, stopping at byte %d", _file->tell());
			_mode = kModeFinished;
		}

		if (_recordType != kRecordAxis) {
			warning("DemoReplay::consumeAxis(): a stick event was not expected, stopping at byte %d", _file->tell());
			_mode = kModeFinished;
		}

		readRecord();

		return;
	}

	byte data[9];
	uint32 valueBits;

	memcpy(&valueBits, &value, sizeof(valueBits));
	data[0] = 'j';
	WRITE_LE_UINT32(data + 1, (uint32)index);
	WRITE_LE_UINT32(data + 5, valueBits);
	_file->write(data, sizeof(data));
}

void DemoReplay::consumeSet() {
	if (_mode == kModeNone || _mode == kModeFinished)
		return;

	if (_mode == kModePlayback) {
		if (_recordType != kRecordSet) {
			warning("DemoReplay::consumeSet(): a set change was not expected, stopping at byte %d", _file->tell());
			_mode = kModeFinished;
		}

		readRecord();

		return;
	}

	byte tag = 's';

	_file->write(&tag, sizeof(tag));
}

void DemoReplay::deliverEvents() {
	if (_mode != kModePlayback)
		return;

	while (_recordType == kRecordKeyUp || _recordType == kRecordKeyDown || _recordType == kRecordAxis) {
		int position = _file->tell();
		int count = _recordCount;

		if (_recordType != kRecordAxis)
			deliverKey();
		else
			_engine->getLua()->callAxisHandler(_axisIndex, _axisValue);

		// The event is consumed by the handler; without that the loop would never end
		if (_recordCount == count) {
			warning("DemoReplay::deliverEvents(): the event was not consumed, stopping at byte %d", position);
			_mode = kModeFinished;
			_recordType = kRecordNone;

			return;
		}
	}

	Joystick *joystick = _engine->getJoystick();

	joystick->update();

	bool pressed = joystick->getControlState(kButtonCross);

	joystick->update();
	pressed |= joystick->getControlState(kButtonSelect);
	joystick->update();
	pressed |= joystick->getControlState(kButtonStart);

	if (pressed) {
		_mode = kModeFinished;
		_recordType = kRecordNone;
	}
}

void DemoReplay::deliverKey() {
	bool down = _recordType == kRecordKeyDown;
	int index = findControlIndex(_keyCode);

	if (index < 0) {
		warning("DemoReplay::deliverKey(): unknown key code %u, stopping at byte %d", _keyCode, _file->tell());
		_mode = kModeFinished;
		_recordType = kRecordNone;

		return;
	}

	_engine->getLua()->buttonHandler((int)_engine->getControl()->_controlTable[index].code, down);
}

void DemoReplay::readRecord() {
	_recordCount++;

	if (_file->eof()) {
		debug("replay finished at byte %d of %d", _file->tell(), (int)_file->_memSize);

		_mode = kModeFinished;
		_recordType = kRecordNone;

		return;
	}

	byte tag = 0;
	byte data[4];

	_file->read(&tag, sizeof(tag));

	switch (tag) {
	case 's':
		_recordType = kRecordSet;

		break;
	case 'f':
		_file->read(data, sizeof(data));

		{
			uint32 frameBits = READ_LE_UINT32(data);

			memcpy(&_frameTime, &frameBits, sizeof(_frameTime));
		}

		_recordType = kRecordFrame;

		break;
	case 'j':
		_recordType = kRecordAxis;
		_file->read(data, sizeof(data));
		_axisIndex = (int)READ_LE_UINT32(data);
		_file->read(data, sizeof(data));

		{
			uint32 axisBits = READ_LE_UINT32(data);

			memcpy(&_axisValue, &axisBits, sizeof(_axisValue));
		}

		break;
	case 'r':
		_recordType = kRecordSeed;
		_file->read(data, sizeof(data));
		_seed = READ_LE_UINT32(data);

		break;
	case 'v':
		_recordType = kRecordVoice;
		_file->read(data, 1);
		_voicePlaying = data[0] != 0;

		break;
	default:
		_recordType = tag == 'u' ? kRecordKeyUp : kRecordKeyDown;
		_file->read(data, sizeof(data));
		_keyCode = READ_LE_UINT32(data);

		break;
	}
}

float DemoReplay::consumeFrame() {
	if (_recordType != kRecordFrame) {
		warning("DemoReplay::consumeFrame(): a frame was expected but the record is '%c', stopping at byte %d", recordTag(), _file->tell());
		_mode = kModeFinished;
	}

	float frameTime = _frameTime;

	_frameNumber++;
	readRecord();

	return frameTime;
}

void DemoReplay::recordFrame(float frameTime) {
	if (_mode != kModeRecord)
		return;

	byte data[5];
	uint32 frameBits;

	memcpy(&frameBits, &frameTime, sizeof(frameBits));
	data[0] = 'f';
	WRITE_LE_UINT32(data + 1, frameBits);
	_file->write(data, sizeof(data));
}

void DemoReplay::recordSeed(uint seed) {
	if (_mode != kModeRecord)
		return;

	byte data[5];

	data[0] = 'r';
	WRITE_LE_UINT32(data + 1, seed);
	_file->write(data, sizeof(data));
}

uint DemoReplay::consumeSeed() {
	if (_recordType != kRecordSeed) {
		warning("DemoReplay::consumeSeed(): the random seed was expected first, stopping at byte %d", _file->tell());
		_mode = kModeFinished;
	}

	uint seed = _seed;

	readRecord();

	return seed;
}

void DemoReplay::recordVoice(bool playing) {
	if (_mode != kModeRecord)
		return;

	byte data[2];

	data[0] = 'v';
	data[1] = playing ? 1 : 0;
	_file->write(data, sizeof(data));
}

char DemoReplay::recordTag() const {
	switch (_recordType) {
	case kRecordKeyUp:
		return 'u';
	case kRecordKeyDown:
		return 'd';
	case kRecordSet:
		return 's';
	case kRecordFrame:
		return 'f';
	case kRecordAxis:
		return 'j';
	case kRecordVoice:
		return 'v';
	case kRecordSeed:
		return 'r';
	default:
		return '?';
	}
}

bool DemoReplay::consumeVoice(const char *source) {
	if (_recordType != kRecordVoice) {
		warning("DemoReplay::consumeVoice(): a voice state was expected by %s but the record is '%c', stopping at byte %d (frame %d)", source, recordTag(), _file->tell(), _frameNumber);
		_mode = kModeFinished;
	}

	bool playing = _voicePlaying;

	readRecord();

	return playing;
}

int DemoReplay::findControlIndex(uint hidCode) {
	for (uint i = 0; i < kNumControls; i++) {
		if (kHidCodes[i] == hidCode)
			return (int)i;
	}

	return -1;
}

} // End of namespace Grim
