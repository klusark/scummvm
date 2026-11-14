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

#ifndef GRIM_REMASTERED_LOOK_LOOK_MATH_H
#define GRIM_REMASTERED_LOOK_LOOK_MATH_H

#include "common/scummsys.h"

namespace Grim {

class RemasteredLookMath {
public:
	static void expand34(const float *matrix12, float *out16);
	static void multiply(const float *first, const float *second, float *out16);
	static bool invert(const float *matrix, float *out16);
	static void normalize3(const float *in, float *out);
	static float inverseOneMinus(float value);
	static float inverseDifference(float high, float low);
	static float distanceSquared(const float *first, const float *second);
	static void buildShadowMatrices(const float *lightMatrix, int type, float sizeX, float sizeY, float nearDistance,
			float farDistance, float *viewProjection, float *worldToShadowMap);
};

} // End of namespace Grim

#endif
