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

#include "common/system.h"

#include "engines/grim/simclock.h"
#include "engines/grim/sputtime.h"

namespace Grim {

SputTime::SputTime(bool isDemo) : _maxFrameTime(isDemo ? 500 : 300), _hs(nullptr), _initialized(0), _started(0), _lastRealTime(0), _gameTime(0),
		_frameTime(0), _frameTimeFlex(0.0f) {
}

int SputTime::getGameTime() const {
	return _gameTime;
}

int SputTime::getFrameTime() const {
	return _frameTime;
}

float SputTime::getFrameTimeFlex() const {
	return _frameTimeFlex;
}

uint SputTime::advance() {
	uint time = grimMillis();
	uint frameTime;

	if (time >= _lastRealTime)
		frameTime = time - _lastRealTime;
	else
		frameTime = time - _lastRealTime - 1;

	_frameTime = frameTime;
	_lastRealTime = time;

	if (frameTime >= _maxFrameTime) {
		frameTime = _maxFrameTime;
		_frameTime = _maxFrameTime;
	}

	uint result = frameTime + _gameTime;

	_gameTime = (int)((uint)_gameTime + frameTime);
	_frameTimeFlex = (float)((double)frameTime * 0.001f);

	return result;
}

uint SputTime::advanceManual(uint ticks) {
	uint frameTime = ticks;

	_frameTime = ticks;

	if (ticks >= _maxFrameTime) {
		frameTime = _maxFrameTime;
		_frameTime = _maxFrameTime;
	}

	_gameTime = (int)((uint)_gameTime + frameTime);
	_frameTimeFlex = (float)((double)frameTime * 0.001f);
	_lastRealTime = grimMillis();

	return _lastRealTime;
}

uint SputTime::reset() {
	_gameTime = 0;
	_frameTime = 0;
	_frameTimeFlex = 0.0f;
	_lastRealTime = grimMillis();

	return _lastRealTime;
}

int SputTime::initialize(SysStruct *hs) {
	_hs = hs;
	_initialized = 1;

	return 0;
}

int SputTime::terminate() {
	_hs = nullptr;

	if (_initialized)
		_initialized = 0;

	return 0;
}

int SputTime::startup() {
	if (_started)
		return 1;

	_started = 1;
	reset();

	return 0;
}

void SputTime::shutdown() {
	_started = 0;
}

} // End of namespace Grim
