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

#include "common/scummsys.h"

#include "engines/grim/sincos.h"
#include "engines/grim/sintable.h"

namespace Grim {

// A word of the table, the index as the original has it (-1 and 4096 are the words around the table)
static inline float tableWord(int i) {
	if (i < -1 || i > 4096)
		return 0.f;
	return sinTable[i + 1];
}

// The original rounds down with a control word of its own (round to minus infinity) and keeps the result as a float
static inline float roundDown(float f) {
	return (float)floor((double)f);
}

// The angle in [0, 360), as the original brings it there
static float normalizeAngle(float a) {
	float r;
	if (a >= 0.f) {
		if (a < 360.f)
			return a;
		float n = roundDown((float)((double)a / 360.0));
		r = (float)((double)a - (double)n * 360.0);
	} else {
		a = -a;
		if (360.f <= a) {
			float n = roundDown((float)((double)a / 360.0));
			r = (float)(360.0 - ((double)a - (double)n * 360.0));
		} else {
			r = (float)(360.0 - (double)a);
		}
	}
	if (r == 360.f)
		r = 0.f;
	return r;
}

// (the step between two words, times the fraction of the step, from the first word, as the original computes it)
static inline float lerp(float from, float to, float frac) {
	return (float)(((double)to - (double)from) * (double)frac + (double)from);
}

void originalSinCos(float degrees, float &sine, float &cosine) {
	const float a = normalizeAngle(degrees);
	int quadrant;
	if (a < 90.f)
		quadrant = 0;
	else if (a < 180.f)
		quadrant = 1;
	else if (a < 270.f)
		quadrant = 2;
	else
		quadrant = 3;

	const float x = (float)((double)a * (double)45.511112f);
	const float frac = (float)((double)x - (double)roundDown(x));
	const int i = (int)x;
	const int j = i + 1;

	float s0, s1, c0, c1;
	switch (quadrant) {
	case 0: {
		const bool hi = j >= 0x1000;
		s0 = tableWord(i);
		s1 = hi ? tableWord(0xfff - (j - 0x1000)) : tableWord(j);
		c0 = tableWord(0xfff - i);
		c1 = hi ? -tableWord(j - 0x1000) : tableWord(0xfff - j);
		break;
	}
	case 1: {
		const bool hi = j >= 0x2000;
		s0 = tableWord(0xfff - (i - 0x1000));
		s1 = hi ? -tableWord(j - 0x2000) : tableWord(0xfff - (j - 0x1000));
		c0 = -tableWord(i - 0x1000);
		c1 = hi ? -tableWord(0xfff - (j - 0x2000)) : -tableWord(j - 0x1000);
		break;
	}
	case 2: {
		const bool hi = j >= 0x3000;
		s0 = -tableWord(i - 0x2000);
		s1 = hi ? -tableWord(0xfff - (j - 0x3000)) : -tableWord(j - 0x2000);
		c0 = -tableWord(0xfff - (i - 0x2000));
		c1 = hi ? tableWord(j - 0x3000) : -tableWord(0xfff - (j - 0x2000));
		break;
	}
	default: {
		const bool hi = j >= 0x4000;
		s0 = -tableWord(0xfff - (i - 0x3000));
		s1 = hi ? tableWord(j - 0x4000) : -tableWord(0xfff - (j - 0x3000));
		c0 = tableWord(i - 0x3000);
		c1 = hi ? tableWord(0xfff - (j - 0x4000)) : tableWord(j - 0x3000);
		break;
	}
	}
	sine = lerp(s0, s1, frac);
	cosine = lerp(c0, c1, frac);
}

// x^n the way the original multiplies it: n - 1 float multiplications, each stored as a float (FUN_0047bbe0)
static float powerOf(float x, int n) {
	float r = x;
	for (int k = 0; k < n - 1; k++)
		r = r * x;
	return r;
}

float originalAsinDeg(float x) {
	const bool negative = x < 0.f;
	float a = negative ? -x : x;
	float result;
	// (the series on long doubles: the original keeps the sums on the x87 stack and stores a float after each line)
	const bool big = !(a <= 0.70710677f);
	if (big) {
		const float arg = (float)(1.0L - (long double)a * (long double)a);
		a = (float)sqrtl((long double)arg);
	}
	const float t1 = (float)((long double)powerOf(a, 3) / 6.0L + (long double)a);
	const float t2 = (float)((long double)powerOf(a, 5) * (long double)0.075f + (long double)t1);
	const long double series = ((long double)powerOf(a, 7) * (long double)0.066797f + (long double)t2) * (long double)57.295784f;
	result = big ? (float)(90.0L - series) : (float)series;
	return negative ? -result : result;
}

void originalNormalize2(float &x, float &y) {
	const long double len = sqrtl((long double)x * x + (long double)y * y);
	if (len != 0.0L) {
		const long double inv = 1.0L / len;
		x = (float)(inv * (long double)x);
		y = (float)(inv * (long double)y);
	}
}

void originalTurnFrame(float &fwdX, float &fwdY, float &rightX, float &rightY, float px, float py, float degrees) {
	float s, c;
	originalSinCos(degrees, s, c);
	const float x = (float)((long double)fwdX * (long double)c + (long double)fwdY * (long double)(-s));
	const float y = (float)((long double)fwdX * (long double)s + (long double)fwdY * (long double)c);
	const float x2 = (float)((long double)(float)(x + px) - (long double)px);
	const float y2 = (float)((long double)(float)(y + py) - (long double)py);
	fwdX = x2;
	fwdY = y2;
	originalNormalize2(fwdX, fwdY);
	rightX = fwdY;
	rightY = -fwdX;
	originalNormalize2(rightX, rightY);
}

} // end of namespace Grim
