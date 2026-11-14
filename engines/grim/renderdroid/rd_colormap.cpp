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

#include "common/endian.h"
#include "common/str.h"
#include "common/stream.h"

#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"

namespace Grim {

RdColormap *RenderDroid::ColormapModule::load(char *path) {
	RdColormap *colormap = new RdColormap;
	memset(colormap, 0, sizeof(RdColormap));

	if (loadEntry(path, colormap))
		return colormap;

	destroy(colormap);
	return nullptr;
}

int RenderDroid::ColormapModule::loadEntry(char *path, RdColormap *colormap) {
	Common::SeekableReadStream *file = _rd->_hs->openFile(path);

	if (!file)
		return 0;

	Common::strlcpy(colormap->name, _rd->_stdFile->fromPath(path), 32);

	uint32 magic = file->readUint32BE();
	file->skip(4);
	colormap->flags = file->readUint32LE();
	colormap->tint[0] = file->readFloatLE();
	colormap->tint[1] = file->readFloatLE();
	colormap->tint[2] = file->readFloatLE();
	file->skip(40);

	if (magic != MKTAG('C', 'M', 'P', ' ')) {
		delete file;
		buildGrayRamp(colormap);
		return 1;
	}

	file->read(colormap->colors, 768);

	byte firstRed = colormap->colors[0];
	byte firstGreen = colormap->colors[1];
	byte firstBlue = colormap->colors[2];

	// The demo does not change the colors that are equal to the first one
	for (int i = 0; i < 254 && !_demoFormat; i++) {
		if (colormap->colors[0] == firstRed)
			colormap->colors[0] = colormap->colors[0] >= 247 ? colormap->colors[0] - 8 : colormap->colors[0] + 8;

		if (colormap->colors[1] == firstGreen)
			colormap->colors[1] = colormap->colors[1] >= 247 ? colormap->colors[1] - 8 : colormap->colors[1] + 8;

		if (colormap->colors[2] == firstBlue)
			colormap->colors[2] = colormap->colors[2] >= 247 ? colormap->colors[2] - 8 : colormap->colors[2] + 8;
	}

	if (colormap->flags & 4) {
		colormap->rgb16 = new byte[0x8000];
		buildRGB16(colormap->rgb16, colormap->colors, 0, 0, 0, &_rd->_colorInfo);
	} else {
		colormap->rgb16 = nullptr;
	}

	if (_demoFormat) {
		colormap->lightLevelAlloc = new byte[16640];
		colormap->lightLevel = colormap->lightLevelAlloc;

		if ((uintptr)colormap->lightLevelAlloc & 0xFF)
			colormap->lightLevel = colormap->lightLevelAlloc + 256 - ((uintptr)colormap->lightLevelAlloc & 0xFF);

		file->read(colormap->lightLevel, 0x4000);
	}

	if (!(colormap->flags & 1)) {
		delete file;
		return 1;
	}

	colormap->transparencyAlloc = new byte[65792];
	colormap->transparency = colormap->transparencyAlloc;

	if ((uintptr)colormap->transparencyAlloc & 0xFF)
		colormap->transparency = colormap->transparencyAlloc + 256 - ((uintptr)colormap->transparencyAlloc & 0xFF);

	file->read(colormap->transparency, 0x10000);

	if (colormap->flags & 4) {
		colormap->rgb16Alpha = new byte[0x10000];

		if (_rd->_colorInfo.greenBits == 5) {
			file->skip(0x10000);
			file->read(colormap->rgb16Alpha, 0x10000);
		} else {
			file->read(colormap->rgb16Alpha, 0x10000);
			file->skip(0x10000);
		}
	} else {
		colormap->rgb16Alpha = nullptr;
	}

	colormap->reserved832 = 0;
	colormap->reserved836 = 0;

	delete file;
	return 1;
}

void RenderDroid::ColormapModule::destroy(RdColormap *&colormap) {
	freeEntry(colormap);
	delete colormap;
	colormap = nullptr;
}

void RenderDroid::ColormapModule::freeEntry(RdColormap *colormap) {
	if (colormap->lightLevelAlloc) {
		delete[] colormap->lightLevelAlloc;
		colormap->lightLevelAlloc = nullptr;
		colormap->lightLevel = nullptr;
	}

	if (colormap->rgb16) {
		delete[] colormap->rgb16;
		colormap->rgb16 = nullptr;
	}

	if (colormap->flags & 1) {
		if (colormap->transparencyAlloc) {
			delete[] colormap->transparencyAlloc;
			colormap->transparencyAlloc = nullptr;
			colormap->transparency = nullptr;
		}

		if (colormap->rgb16Alpha) {
			delete[] colormap->rgb16Alpha;
			colormap->rgb16Alpha = nullptr;
		}
	}
}

int RenderDroid::ColormapModule::setCurrent(RdColormap *colormap) {
	if (_curMap != colormap)
		_curMap = colormap;

	if (!_identityMap)
		setIdentity(colormap);

	return 1;
}

int RenderDroid::ColormapModule::setIdentity(RdColormap *colormap) {
	if (_identityMap != colormap) {
		_identityMap = colormap;

#if defined(GRIM_STD3D)
		if (_rd->_curAcceleration > 0)
			_rd->_std3D->setCurrentPalette(colormap->colors, 90);
#endif
	}

	return 1;
}

int RenderDroid::ColormapModule::size(RdColormap *colormap) {
	int result = 848;

	if (colormap->lightLevelAlloc)
		result = 17488;

	if (colormap->transparencyAlloc)
		result += 65792;

	if (colormap->rgb16)
		result += 0x8000;

	if (colormap->rgb16Alpha)
		result += 0x10000;

	return result;
}

int RenderDroid::ColormapModule::buildGrayRamp(RdColormap *colormap) {
	Common::strlcpy(colormap->name, "GREYRAMP.CMP", 32);
	colormap->flags = 0;
	colormap->tint[0] = 1.0f;
	colormap->tint[1] = 1.0f;
	colormap->tint[2] = 1.0f;

	colormap->lightLevelAlloc = new byte[16640];
	colormap->lightLevel = colormap->lightLevelAlloc;

	if ((uintptr)colormap->lightLevelAlloc & 0xFF)
		colormap->lightLevel = colormap->lightLevelAlloc + 256 - ((uintptr)colormap->lightLevelAlloc & 0xFF);

	for (uint i = 0; i < 256; i++) {
		// The overlapping writes are in the original
		colormap->colors[i] = i;
		colormap->colors[i + 1] = i;
		colormap->colors[i + 2] = i;

		uint level = 0;
		for (uint j = 0; j < 0x4000; j += 256)
			colormap->lightLevel[i + j] = i * (level++ >> 6);
	}

	return 1;
}

int RenderDroid::ColormapModule::buildRGB16(byte *dst, byte *palette, byte r, byte g, byte b, StdColorFormat *colorFormat) {
	uint16 *out = (uint16 *)dst;
	byte *color = palette;

	double targetR = r;
	double targetG = g;
	double targetB = b;

	for (uint col = 0; col < 8; col++) {
		byte red = color[0];
		byte green = color[1];
		byte blue = color[2];
		color += 3;

		for (uint row = 0; row < 64; row++) {
			uint16 value = (byte)(green >> colorFormat->greenLoss) << colorFormat->greenShift;
			value |= (byte)(red >> colorFormat->redLoss) << colorFormat->redShift;
			value |= (byte)(blue >> colorFormat->blueLoss) << colorFormat->blueShift;
			out[row * 256 + col] = value;
		}
	}

	color = palette + 24;

	for (uint col = 8; col < 256; col++) {
		if (_rd->_remastered) {
			float floatR = (float)color[0];
			float floatG = (float)color[1];
			float floatB = (float)color[2];
			color += 3;

			float floatStepR = (float)((targetR - floatR) * 0.015625);
			float floatStepG = (float)((targetG - floatG) * 0.015625);
			float floatStepB = (float)((targetB - floatB) * 0.015625);

			for (uint row = 64; row > 0; row--) {
				floatR = (float)((double)floatR + floatStepR);
				floatG = (float)((double)floatG + floatStepG);
				floatB = (float)((double)floatB + floatStepB);

				int red = (int)(floatR <= 0.0f ? (double)floatR - 0.5 : (double)floatR + 0.5);
				int green = (int)(floatG <= 0.0f ? (double)floatG - 0.5 : (double)floatG + 0.5);
				int blue = (int)(floatB <= 0.0f ? (double)floatB - 0.5 : (double)floatB + 0.5);

				out[(row - 1) * 256 + col] = (red >> colorFormat->redLoss << colorFormat->redShift) |
					(green >> colorFormat->greenLoss << colorFormat->greenShift) |
					(blue >> colorFormat->blueLoss << colorFormat->blueShift);
			}

			continue;
		}

		double curR = color[0];
		double curG = color[1];
		double curB = color[2];
		color += 3;

		double stepR = (targetR - curR) * 0.015625;
		double stepG = (targetG - curG) * 0.015625;
		float stepB = (float)((targetB - curB) * 0.015625);

		for (uint row = 64; row > 0; row--) {
			curR = curR + stepR;
			curG = curG + stepG;
			curB = curB + stepB;

			int red = (int)(curR > 0.0 ? curR + 0.5 : curR - 0.5);
			int green = (int)(curG > 0.0 ? curG + 0.5 : curG - 0.5);
			int blue = (int)(curB > 0.0 ? curB + 0.5 : curB - 0.5);

			out[(row - 1) * 256 + col] = (red >> colorFormat->redLoss << colorFormat->redShift) |
				(green >> colorFormat->greenLoss << colorFormat->greenShift) |
				(blue >> colorFormat->blueLoss << colorFormat->blueShift);
		}
	}

	return 1;
}

} // End of namespace Grim
