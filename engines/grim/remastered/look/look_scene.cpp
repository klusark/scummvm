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

#include "engines/grim/remastered/look/look_scene.h"

#include <string.h>

namespace Grim {

LookCamera::LookCamera() : viewDistance(0.0f) {
	memset(view, 0, sizeof(view));
	memset(invView, 0, sizeof(invView));
}

LookLightInfo::LookLightInfo() : shadingBias(0.0f), shadingLimit(0.0f), innerRadius(0.0f), falloffPower(0.0f),
		verticalInner(0.0f), verticalPower(0.0f), shadowMode(0), shadowQuality(0), shadowBias(0.0f), shadowSoftness(0.0f),
		sizeX(0.0f), sizeY(0.0f), nearDistance(0.0f), sizeZ(0.0f), maxDistance(0.0f), keylight(0) {
	memset(position, 0, sizeof(position));
	memset(matrix, 0, sizeof(matrix));
	memset(inner, 0, sizeof(inner));
	memset(outer, 0, sizeof(outer));
}

LookCutsceneLight::LookCutsceneLight() : type(0) {
	memset(position, 0, sizeof(position));
	memset(direction, 0, sizeof(direction));
	memset(color, 0, sizeof(color));
	memset(settings, 0, sizeof(settings));
}

LookCompositorInfo::LookCompositorInfo() : minLuminance(0.0f), maxLuminance(1.0f), power(1.0f), aoStrength(0.5f) {
	for (int i = 0; i < 3; i++) {
		white[i] = 1.0f;
		black[i] = 0.0f;
	}
}

} // End of namespace Grim
