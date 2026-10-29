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

#ifndef GRIM_SINCOS_H
#define GRIM_SINCOS_H

namespace Grim {

/**
 * The sine and cosine of an angle in degrees the way the original engine has them: a table of 4096 values for a quarter turn,
 * interpolated, with the original's rounding to floats. They are off the true values by up to a few 1e-4 (more at the edges of the
 * quadrants), and the original turns and walks its actors with them: an actor that walks due west in the original drifts north by
 * about 3e-4 of its way, which decides to which side a head-on collision with a box slides it.
 */
void originalSinCos(float degrees, float &sine, float &cosine);

/**
 * The arcsine in degrees of the original (FUN_0047c4a0): a series in the argument, or in sqrt(1 - x^2) from 90 degrees above 0.7071,
 * with the original's rounding to floats. The original takes the angle of every turn from it.
 */
float originalAsinDeg(float x);

/**
 * The vector divided by its length, as the original does it (FUN_004469a0): the length and the quotient in extended precision.
 */
void originalNormalize2(float &x, float &y);

/**
 * The frame of a walker (forward and right vectors, flat) turned by an angle in degrees about its up vector (FUN_004a83c0 with the
 * up vector (0, 0, 1)), px and py the position of the walker, through which the original sends the forward vector. The right vector is
 * made again from the forward one.
 */
void originalTurnFrame(float &fwdX, float &fwdY, float &rightX, float &rightY, float px, float py, float degrees);

} // end of namespace Grim

#endif
