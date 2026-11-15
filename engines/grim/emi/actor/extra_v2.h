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

#ifndef GRIM_EMI_EXTRA_V2_H
#define GRIM_EMI_EXTRA_V2_H

#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/math/cquaternion.h"
#include "engines/grim/emi/math/cvector.h"
#include "engines/grim/emi/files/lfile.h"

namespace Grim {

class EmiEngine;
class Puck_v2;

class Extra_v2 {
public:
	enum {
		kMaxPathPoints = 30,
		kNumLookLimits = 4,
		kSayLineIdSize = 12
	};

	explicit Extra_v2(EmiEngine *engine);

	void *operator new(size_t size);
	void operator delete(void *block);

	bool _lookAtActive;
	bool _lookAtHasTarget;
	CVector _lookTarget;
	CVector _lookOffset;
	int _lookTargetActorId;
	CQuaternion _lookGoalQuat;
	CVector _lookDirection;
	CQuaternion _lookCurQuat;
	CVector _lookLimits[kNumLookLimits];
	LString _lookJointName;
	float _lookCosFov;
	float _lookRate;
	bool _walking;
	int _turnDir;
	bool _resting;
	float _walkRate;
	float _turnRate;
	bool _tankTurn;
	bool _aligned;
	bool _snapTurn;
	float _offsetYaw;
	float _alignOff;
	CVector _walkTarget;
	CVector _goal;
	int _numPathPoints;
	CVector _path[kMaxPathPoints];
	int _pathIndex;
	Puck_v2 *_puck;
	bool _inBoxes;
	bool _walkDominate;
	float _reflectionAngle;
	bool _talking;
	int _soundHandle;
	int _talkTimerMs;
	char _sayLineId[kSayLineIdSize];
	int _textEventId;
	bool _lipsyncActive;
	int _visemeCurrent;
	int _visemeMs;
	int _collisionMode;
	float _collisionScale;
	bool _inCollision;
	Color _talkColor;
	float _timeScale;

private:
	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
