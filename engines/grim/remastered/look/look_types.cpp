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

#include "common/textconsole.h"

#include "engines/grim/remastered/look/look_types.h"

#include <string.h>

namespace Grim {

LookConstBlock::LookConstBlock() : layout(nullptr) {
	memset(data, 0, sizeof(data));
}

void LookConstBlock::clear() {
	layout = nullptr;
	memset(data, 0, sizeof(data));
}

void LookConstBlock::setLayout(const LookConstLayout *newLayout) {
	layout = newLayout;
	memset(data, 0, sizeof(data));
}

void LookConstBlock::setFloats(uint offset, const float *values, uint count) {
	if (offset + count * sizeof(float) > sizeof(data)) {
		warning("LookConstBlock::setFloats(): offset %u is out of range", offset);
		return;
	}

	memcpy(data + offset, values, count * sizeof(float));
}

void LookConstBlock::setFloat(uint offset, float value) {
	setFloats(offset, &value, 1);
}

float *LookConstBlock::getFloats(uint offset) {
	return (float *)(data + offset);
}

LookImage::LookImage() : format(kLookFormatRgba8), width(0), height(0), numLevels(0) {
	memset(levels, 0, sizeof(levels));
}

LookCaps::LookCaps() : compressedTextures(false), maxTextureSize(0), maxDrawBuffers(0) {
}

RemasteredLookTargetSetup::RemasteredLookTargetSetup() : depth(nullptr) {
	for (int i = 0; i < kLookMaxTargets; i++)
		color[i] = nullptr;
}

LookCall::LookCall() {
	reset();
}

void LookCall::reset() {
	primitive = 4;
	program = -1;
	vertexConstants.clear();
	pixelConstants.clear();
	hasVertexConstants = false;
	hasPixelConstants = false;
	depthFunc = kLookDepthAlways;
	depthWrite = false;
	blendEnable = false;
	blendMode = kLookBlendPremultiplied;
	numTextures = 0;
	numSamplers = 0;
	count = -2;
	first = 0;
	vertices = nullptr;
	indices = nullptr;

	for (int i = 0; i < kLookMaxTextures; i++) {
		textures[i] = nullptr;
		samplers[i].wrap = 0;
		samplers[i].unused = 0;
		samplers[i].filter = 0;
		samplers[i].mip = 0;
	}
}

void LookCall::setVertexConstants(const LookConstBlock &block) {
	vertexConstants = block;
	hasVertexConstants = true;
}

void LookCall::setPixelConstants(const LookConstBlock &block) {
	pixelConstants = block;
	hasPixelConstants = true;
}

} // End of namespace Grim
