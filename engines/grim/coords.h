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

#ifndef GRIM_COORDS_H
#define GRIM_COORDS_H

#include "math/vector2d.h"
#include "math/vector3d.h"
#include "math/angle.h"
#include "math/line3d.h"
#include "math/quat.h"
#include "math/rotation3d.h"

#include "engines/grim/grim.h"

namespace Grim {

/*
 * Grim Fandango uses a Z-up world where actors face +Y at yaw 0.
 * EMI uses a Y-up world where actors face +Z at yaw 0.
 */

inline bool isYUp() {
	return g_grim->getGameType() == GType_MONKEY4;
}

/** Returns the component of v along the world up axis. */
inline float getHeight(const Math::Vector3d &v) {
	return isYUp() ? v.y() : v.z();
}

/** Zeroes the component of v along the world up axis. */
inline void flattenToGround(Math::Vector3d &v) {
	if (isYUp())
		v.y() = 0;
	else
		v.z() = 0;
}

/** Maps an EMI vector to Grim space. Identity for Grim. Self-inverse. */
inline Math::Vector3d convertToGrimSpace(const Math::Vector3d &v) {
	if (isYUp())
		return Math::Vector3d(-v.x(), v.z(), v.y());
	return v;
}

/** Returns the unit vector on the ground plane an actor faces at the given yaw. */
inline Math::Vector3d getForwardVector(const Math::Angle &yaw) {
	if (isYUp())
		return Math::Vector3d(yaw.getSine(), 0, yaw.getCosine());
	return Math::Vector3d(-yaw.getSine(), yaw.getCosine(), 0);
}

/** Inverse of getForwardVector. Returns 0 for a vector with no ground component. */
inline Math::Angle getYawFromDirection(const Math::Vector3d &dir) {
	if (isYUp()) {
		if (dir.x() == 0 && dir.z() == 0)
			return 0;
		return Math::Angle::arcTangent2(dir.x(), dir.z());
	}
	if (dir.x() == 0 && dir.y() == 0)
		return 0;
	return Math::Angle::arcTangent2(-dir.x(), dir.y());
}

/** Sign to apply to a yaw delta so that turning has the same on-screen sense in both games. */
inline float getTurnSign() {
	return isYUp() ? -1.f : 1.f;
}

/** Intersects two lines projected onto the ground plane. */
inline bool intersectOnGround(Math::Line3d &a, const Math::Line3d &b, Math::Vector3d *pos) {
	return a.intersectLine2d(b, pos, isYUp());
}

/** Forward vector at the given yaw, tilted onto a plane with the given normal. */
inline Math::Vector3d getForwardVectorOnPlane(const Math::Angle &yaw, const Math::Vector3d &normal) {
	if (isYUp()) {
		Math::Angle ax = Math::Vector2d(normal.x(), normal.y()).getAngle();
		Math::Angle az = Math::Vector2d(normal.z(), normal.y()).getAngle();

		float y1 = yaw.getCosine() * az.getCosine();
		float y2 = yaw.getSine() * ax.getCosine();
		return Math::Vector3d(-yaw.getSine() * ax.getSine(), y1 + y2,
		                      -yaw.getCosine() * az.getSine());
	}

	Math::Angle ax = Math::Vector2d(normal.x(), normal.z()).getAngle();
	Math::Angle ay = Math::Vector2d(normal.y(), normal.z()).getAngle();

	float z1 = -yaw.getCosine() * ay.getCosine();
	float z2 = yaw.getSine() * ax.getCosine();
	return Math::Vector3d(-yaw.getSine() * ax.getSine(),
	                      yaw.getCosine() * ay.getSine(), z1 + z2);
}

/** Builds a rotation from yaw, pitch and roll using the game's Euler order. */
template<class T>
inline void buildFromYawPitchRoll(T &rot, const Math::Angle &yaw, const Math::Angle &pitch, const Math::Angle &roll) {
	if (isYUp())
		rot.buildFromEuler(roll, yaw, pitch, Math::EO_ZYX);
	else
		rot.buildFromEuler(yaw, pitch, roll, Math::EO_ZXY);
}

/** Extracts yaw, pitch and roll from a rotation using the game's Euler order. Null pointers are allowed. */
template<class T>
inline void getYawPitchRoll(const T &rot, Math::Angle *yaw, Math::Angle *pitch, Math::Angle *roll) {
	if (isYUp())
		rot.getEuler(roll, yaw, pitch, Math::EO_ZYX);
	else
		rot.getEuler(yaw, pitch, roll, Math::EO_ZXY);
}

inline Math::Quaternion quatFromYawPitchRoll(const Math::Angle &yaw, const Math::Angle &pitch, const Math::Angle &roll) {
	if (isYUp())
		return Math::Quaternion::fromEuler(roll, yaw, pitch, Math::EO_ZYX);
	return Math::Quaternion::fromEuler(yaw, pitch, roll, Math::EO_ZXY);
}

} // end of namespace Grim

#endif
