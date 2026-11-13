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

#ifndef GRIM_SPUTCONTROL_H
#define GRIM_SPUTCONTROL_H

#include "common/scummsys.h"

#include "engines/grim/std/std_sys.h"

namespace Grim {

class GrimEngine;
class RenderDroid;
class SputRoom;
class StdControl;

enum {
	kNumControlSymbols = 167,
	kNumControlSymbolsRemastered = 163,
	kMaxControlCallbacks = 9
};

struct ControlSymbol {
	uint flags;
	int code;
	int lastValue;
	const char *name;
};

class SputControlCallback {
public:
	virtual ~SputControlCallback() {}

	virtual int controlCallback(float frameTime) = 0;
};

class SputControl : public SputControlCallback {
public:
	SputControl(RenderDroid *rd, SputRoom *sputRoom, StdControl *stdControl, bool remastered);

	void setGrimEngine(GrimEngine *grim) { _grim = grim; }

	int controlCallback(float frameTime) override;
	void resetSymbols();
	void enableControl(uint index);
	void disableControl(uint index);
	int initialize(SysStruct *hs);
	int open();
	int close();
	int addCallback(SputControlCallback *callback);
	void clearCallbacks();
	void update(float frameTime);
	int terminate();
	int startup();
	int shutdown();

	ControlSymbol *getSymbols() { return _controlSymbols; }
	int getNumSymbols() const { return _numSymbols; }

private:
	int standardHandler();
	int changeFOV(double step);

	RenderDroid *_rd;
	SputRoom *_sputRoom;
	StdControl *_stdControl;
	GrimEngine *_grim;
	SysStruct *_hs;
	int _initialized;
	int _started;
	int _controlOpen;
	int _numCallbacks;
	int _numSymbols;
	SputControlCallback *_callbacks[kMaxControlCallbacks];
	ControlSymbol _controlSymbols[kNumControlSymbols];
};

} // End of namespace Grim

#endif
