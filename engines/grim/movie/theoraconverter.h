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

#ifndef GRIM_THEORACONVERTER_H
#define GRIM_THEORACONVERTER_H

#include "common/scummsys.h"

#ifdef USE_THEORADEC

#include "video/theora_decoder.h"

namespace Grim {

class TheoraConverter : public Video::TheoraDecoder::FrameConverter {
public:
	TheoraConverter();

	void convert(Graphics::Surface *dest, th_pixel_fmt pixelFormat,
	             const th_img_plane &y, const th_img_plane &u, const th_img_plane &v) override;

private:
	enum {
		kTableSize = 256,
		kFixedPointShift = 13
	};

	void initYUVConversionModule();
	void decodeRGBX(Graphics::Surface *dest, const th_img_plane &y, const th_img_plane &u, const th_img_plane &v) const;

	static uint32 clampChannel(int value) {
		if ((value & ~0xFF) != 0)
			return value < 0 ? 0 : 255;

		return (uint32)value;
	}

	static uint32 makePixel(int lumaTerm, int redTerm, int greenTerm, int blueTerm,
	                        int redShift, int greenShift, int blueShift, uint32 opaque) {
		return (clampChannel((lumaTerm + redTerm) >> kFixedPointShift) << redShift) |
		       (clampChannel((lumaTerm - greenTerm) >> kFixedPointShift) << greenShift) |
		       (clampChannel((lumaTerm + blueTerm) >> kFixedPointShift) << blueShift) |
		       opaque;
	}

	int _yTable[kTableSize];
	int _rvTable[kTableSize];
	int _guTable[kTableSize];
	int _gvTable[kTableSize];
	int _buTable[kTableSize];

	bool _warnedPixelFormat;
};

class TheoraAudioConverter : public Video::TheoraDecoder::SampleConverter {
public:
	TheoraAudioConverter();

	void setAudioGain(float gain);

	int16 convert(float sample) override;

private:
	float _gain;
	bool _warnedInvalidSample;
};

} // End of namespace Grim

#endif // USE_THEORADEC

#endif
