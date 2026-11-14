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

#include "engines/grim/movie/theoraconverter.h"

#ifdef USE_THEORADEC

#include "common/textconsole.h"

#include "graphics/pixelformat.h"
#include "graphics/surface.h"

namespace Grim {

TheoraConverter::TheoraConverter() : _warnedPixelFormat(false) {
	initYUVConversionModule();
}

void TheoraConverter::initYUVConversionModule() {
	for (int i = 0; i < kTableSize; i++) {
		_yTable[i] = (int)((double)(i - 16) * 9535.988);
		_rvTable[i] = (int)((double)(i - 128) * 13074.932);
		_guTable[i] = (int)((double)(i - 128) * 3203.572);
		_gvTable[i] = (int)((double)(i - 128) * 6660.596);
		_buTable[i] = (int)((double)(i - 128) * 16531.956);
	}
}

void TheoraConverter::convert(Graphics::Surface *dest, th_pixel_fmt pixelFormat,
                              const th_img_plane &y, const th_img_plane &u, const th_img_plane &v) {
	if (dest->format != Graphics::PixelFormat::createFormatRGBA32())
		error("TheoraConverter::convert(): the output surface must be RGBA32");

	if (pixelFormat != TH_PF_420 && !_warnedPixelFormat) {
		warning("TheoraConverter::convert(): the chroma planes are not 4:2:0, the picture will be wrong");
		_warnedPixelFormat = true;
	}

	assert(dest->w == y.width && dest->h == y.height);
	assert((y.width & 1) == 0 && (y.height & 1) == 0);

	decodeRGBX(dest, y, u, v);
}

void TheoraConverter::decodeRGBX(Graphics::Surface *dest, const th_img_plane &y, const th_img_plane &u, const th_img_plane &v) const {
	const int width = y.width;
	const int height = y.height;
	const int redShift = dest->format.rShift;
	const int greenShift = dest->format.gShift;
	const int blueShift = dest->format.bShift;
	const uint32 opaque = (uint32)0xFF << dest->format.aShift;
	byte *destBase = (byte *)dest->getPixels();

	for (int row = 0; row < height; row += 2) {
		const byte *lumaEven = y.data + row * y.stride;
		const byte *lumaOdd = y.data + (row + 1) * y.stride;
		const byte *cbRow = u.data + ((row * u.stride) >> 1);
		const byte *crRow = v.data + ((row * v.stride) >> 1);
		uint32 *destEven = (uint32 *)(destBase + row * dest->pitch);
		uint32 *destOdd = (uint32 *)(destBase + (row + 1) * dest->pitch);

		for (int x = 0; x < width; x += 2) {
			const int cb = cbRow[x >> 1];
			const int cr = crRow[x >> 1];
			const int redTerm = _rvTable[cr];
			const int greenTerm = _guTable[cb] + _gvTable[cr];
			const int blueTerm = _buTable[cb];

			destEven[x] = makePixel(_yTable[lumaEven[x]], redTerm, greenTerm, blueTerm, redShift, greenShift, blueShift, opaque);
			destOdd[x] = makePixel(_yTable[lumaOdd[x]], redTerm, greenTerm, blueTerm, redShift, greenShift, blueShift, opaque);
			destEven[x + 1] = makePixel(_yTable[lumaEven[x + 1]], redTerm, greenTerm, blueTerm, redShift, greenShift, blueShift, opaque);
			destOdd[x + 1] = makePixel(_yTable[lumaOdd[x + 1]], redTerm, greenTerm, blueTerm, redShift, greenShift, blueShift, opaque);
		}
	}
}

TheoraAudioConverter::TheoraAudioConverter() : _gain(1.0f), _warnedInvalidSample(false) {
}

void TheoraAudioConverter::setAudioGain(float gain) {
	if (gain < 0.0f)
		_gain = 0.0f;
	else
		_gain = gain;
}

int16 TheoraAudioConverter::convert(float sample) {
	// The gain is applied below 1.0 only
	if (_gain < 1.0f)
		sample *= _gain;

	if (sample > 1.0f)
		sample = 1.0f;
	else if (sample < -1.0f)
		sample = -1.0f;

	if (sample != sample) {
		if (!_warnedInvalidSample) {
			warning("TheoraAudioConverter::convert(): invalid audio sample");
			_warnedInvalidSample = true;
		}

		return 0;
	}

	return (int16)(int)((double)sample * 32767.0);
}

} // End of namespace Grim

#endif // USE_THEORADEC
