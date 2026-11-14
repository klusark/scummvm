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
#include "common/textconsole.h"

#include "engines/grim/remastered/look/s3tc.h"

namespace Grim {

uint S3tc::getBlockBytes(Format format) {
	if (format == kDxt1)
		return 8;

	return 16;
}

uint32 S3tc::getLevelSize(Format format, uint width, uint height) {
	if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension) {
		warning("S3tc::getLevelSize(): invalid dimensions %ux%u", width, height);
		return 0;
	}

	uint blocksX = (width + 3) >> 2;
	uint blocksY = (height + 3) >> 2;

	return blocksX * blocksY * getBlockBytes(format);
}

void S3tc::expand565(uint color, byte *rgb) {
	uint red = (color >> 11) & 0x1F;
	uint green = (color >> 5) & 0x3F;
	uint blue = color & 0x1F;

	rgb[0] = (byte)((red << 3) | (red >> 2));
	rgb[1] = (byte)((green << 2) | (green >> 4));
	rgb[2] = (byte)((blue << 3) | (blue >> 2));
}

void S3tc::decodeColorBlock(const byte *block, bool isDxt1, byte *rgba) {
	uint color0 = READ_LE_UINT16(block);
	uint color1 = READ_LE_UINT16(block + 2);
	byte palette[4][4];

	expand565(color0, palette[0]);
	expand565(color1, palette[1]);

	if (!isDxt1 || color0 > color1) {
		for (uint i = 0; i < 3; i++) {
			palette[2][i] = (byte)((2 * palette[0][i] + palette[1][i]) / 3);
			palette[3][i] = (byte)((palette[0][i] + 2 * palette[1][i]) / 3);
		}

		palette[3][3] = 255;
	} else {
		for (uint i = 0; i < 3; i++) {
			palette[2][i] = (byte)((palette[0][i] + palette[1][i]) / 2);
			palette[3][i] = 0;
		}

		palette[3][3] = 0;
	}

	palette[0][3] = 255;
	palette[1][3] = 255;
	palette[2][3] = 255;

	uint32 indices = READ_LE_UINT32(block + 4);

	for (uint pixel = 0; pixel < kBlockPixels; pixel++) {
		const byte *color = palette[(indices >> (2 * pixel)) & 3];

		memcpy(rgba + 4 * pixel, color, 4);
	}
}

void S3tc::decodeDxt3Alpha(const byte *block, byte *rgba) {
	for (uint pixel = 0; pixel < kBlockPixels; pixel++) {
		uint nibble = (block[pixel >> 1] >> (4 * (pixel & 1))) & 0x0F;

		rgba[4 * pixel + 3] = (byte)(nibble * 17);
	}
}

void S3tc::decodeDxt5Alpha(const byte *block, byte *rgba) {
	uint alpha[8];

	alpha[0] = block[0];
	alpha[1] = block[1];

	if (alpha[0] > alpha[1]) {
		for (uint i = 1; i < 7; i++)
			alpha[i + 1] = ((7 - i) * alpha[0] + i * alpha[1]) / 7;
	} else {
		for (uint i = 1; i < 5; i++)
			alpha[i + 1] = ((5 - i) * alpha[0] + i * alpha[1]) / 5;

		alpha[6] = 0;
		alpha[7] = 255;
	}

	uint64 indices = 0;

	for (int i = 5; i >= 0; i--)
		indices = (indices << 8) | block[2 + i];

	for (uint pixel = 0; pixel < kBlockPixels; pixel++)
		rgba[4 * pixel + 3] = (byte)alpha[(indices >> (3 * pixel)) & 7];
}

void S3tc::decodeBlock(Format format, const byte *block, byte *rgba) {
	switch (format) {
	case kDxt1:
		decodeColorBlock(block, true, rgba);
		break;

	case kDxt3:
		decodeColorBlock(block + 8, false, rgba);
		decodeDxt3Alpha(block, rgba);
		break;

	case kDxt5:
		decodeColorBlock(block + 8, false, rgba);
		decodeDxt5Alpha(block, rgba);
		break;

	default:
		warning("S3tc::decodeBlock(): unknown format %d", (int)format);
		memset(rgba, 0, 4 * kBlockPixels);
		break;
	}
}

bool S3tc::decodeLevel(Format format, uint width, uint height, const byte *blocks, uint32 size, byte *rgba) {
	uint32 levelSize = getLevelSize(format, width, height);

	if (!levelSize || !blocks || !rgba) {
		warning("S3tc::decodeLevel(): invalid arguments");
		return false;
	}

	if (size < levelSize) {
		warning("S3tc::decodeLevel(): %u bytes of block data, %u needed", size, levelSize);
		return false;
	}

	uint blockBytes = getBlockBytes(format);
	uint blocksX = (width + 3) >> 2;
	uint blocksY = (height + 3) >> 2;
	byte decoded[4 * kBlockPixels];

	for (uint blockY = 0; blockY < blocksY; blockY++) {
		for (uint blockX = 0; blockX < blocksX; blockX++) {
			decodeBlock(format, blocks + (blockY * blocksX + blockX) * blockBytes, decoded);

			for (uint y = 0; y < 4; y++) {
				uint row = blockY * 4 + y;

				if (row >= height)
					break;

				for (uint x = 0; x < 4; x++) {
					uint column = blockX * 4 + x;

					if (column >= width)
						break;

					memcpy(rgba + (row * width + column) * 4, decoded + (y * 4 + x) * 4, 4);
				}
			}
		}
	}

	return true;
}

} // End of namespace Grim
