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

#ifndef GRIM_WORLDAXES_H
#define GRIM_WORLDAXES_H

#include "math/angle.h"
#include "math/matrix4.h"
#include "math/vector3d.h"

#include "engines/grim/grim.h"

namespace Grim {

/**
 * The two games use different coordinate systems for their worlds:
 *
 * - Grim: Z is up and an actor with a yaw of 0 looks along +Y.
 * - EMI: Y is up and an actor with a yaw of 0 looks along +Z, with the X axis flipped.
 *
 * The code of the actors and of the walk sectors is shared, so it is written in the axes of
 * Grim and these functions turn the vectors into the axes of the game that is running. Everything
 * else (sets, sectors, savegames, scripts, rendering) stays in the coordinates of its game.
 */
namespace WorldAxes {

/** Whether the vertical axis of the running game is Y (EMI) and not Z (Grim). */
inline bool isYUp() {
	return g_grim->getGameType() == GType_MONKEY4;
}

/**
 * Converts a direction given in the axes of Grim to the axes of the running game. For EMI the
 * vertical component goes from Z to Y and the X axis is flipped, the horizontal plane of the
 * actors is X-Z.
 *
 * The conversion is its own inverse, so gameToGrimAxes() does the other way.
 */
inline Math::Vector3d grimToGameAxes(const Math::Vector3d &v) {
	if (isYUp())
		return Math::Vector3d(-v.x(), v.z(), v.y());
	return v;
}

/** Converts a direction of the running game to the axes of Grim, see grimToGameAxes(). */
inline Math::Vector3d gameToGrimAxes(const Math::Vector3d &v) {
	return grimToGameAxes(v);
}

/**
 * The sectors of the games are not in the same axes as the directions above: the normals of the sectors of EMI point
 * down where the ones of Grim point up, which makes it a turn of half a circle around the vertical axis from that
 * conversion. For Grim nothing changes, for EMI (x, y, z) goes to (x, -z, y).
 *
 * Converts a normal of a sector of the running game to the axes of Grim, so that the walk along the sector can be
 * calculated for both games the same way. grimToGameSectorAxes() converts the direction that was found back.
 */
inline Math::Vector3d gameSectorToGrimAxes(const Math::Vector3d &v) {
	if (isYUp())
		return Math::Vector3d(v.x(), -v.z(), v.y());
	return v;
}

/** The inverse of gameSectorToGrimAxes(), for EMI (x, y, z) goes to (x, z, -y). */
inline Math::Vector3d grimToGameSectorAxes(const Math::Vector3d &v) {
	if (isYUp())
		return Math::Vector3d(v.x(), v.z(), -v.y());
	return v;
}

/** The direction in the horizontal plane that an actor with this yaw looks along. */
inline Math::Vector3d forward(const Math::Angle &yaw) {
	return grimToGameAxes(Math::Vector3d(-yaw.getSine(), yaw.getCosine(), 0.0f));
}

/** The vector without its vertical component. */
inline Math::Vector3d flattened(const Math::Vector3d &v) {
	Math::Vector3d result(v);
	if (isYUp())
		result.y() = 0.0f;
	else
		result.z() = 0.0f;
	return result;
}

/**
 * The angles of a rotation. The order of the angles in a rotation is different in the games: Grim has yaw, pitch and roll
 * (EO_ZXY) and EMI has roll, yaw and pitch (EO_ZYX). Any of the pointers can be null.
 */
inline void getEuler(const Math::Matrix4 &m, Math::Angle *yaw, Math::Angle *pitch, Math::Angle *roll) {
	if (isYUp())
		m.getEuler(roll, yaw, pitch, Math::EO_ZYX);
	else
		m.getEuler(yaw, pitch, roll, Math::EO_ZXY);
}

/** Builds a rotation from its angles, see getEuler(). */
inline void buildFromEuler(Math::Matrix4 &m, const Math::Angle &yaw, const Math::Angle &pitch, const Math::Angle &roll) {
	if (isYUp())
		m.buildFromEuler(roll, yaw, pitch, Math::EO_ZYX);
	else
		m.buildFromEuler(yaw, pitch, roll, Math::EO_ZXY);
}

} // End of namespace WorldAxes

} // End of namespace Grim

#endif
