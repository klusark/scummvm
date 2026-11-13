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

#ifndef GRIM_STD_STD_MATH_H
#define GRIM_STD_STD_MATH_H

#include "common/scummsys.h"

namespace Grim {

class StdMath {
public:
	explicit StdMath(bool isRemastered = false) : _remastered(isRemastered) {}

	float flexPower(float base, int exponent);
	float rint(float value);
	float normalizeAngle(float angle);
	float normalizeAngleAcute(float angle);
	void sinCos(float angle, float *sin, float *cos);
	float tan(float angle);
	float arcSin2(float value);
	float sqrtImpl(float value);
	float arcSin3(float value);
	float arcTan4(float y, float x);

private:
	float arcSin3Remastered(float value);

	static const float kSinCosTable[4096];
	static const float kTanTable[4096];

	bool _remastered;
};

} // End of namespace Grim

#endif
