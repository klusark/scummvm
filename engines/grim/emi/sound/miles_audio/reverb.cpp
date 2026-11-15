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
#include "common/textconsole.h"

#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/sound/miles_audio/driver.h"
#include "engines/grim/emi/sound/miles_audio/reverb.h"

namespace Grim {

struct EMIMilesReverbPreset {
	float mix;
	float time;
	float damping;
	float predelay;
};

static const int kPresetCount = 27;
static const int kMaxPreset = 26;

static const EMIMilesReverbPreset kPresets[kPresetCount] = {
	{ 0.0f, 1.493f, 0.5f, 0.0f },
	{ 0.25f, 0.1f, 0.4f, 0.0f },
	{ 0.417f, 0.4f, 0.666f, 0.0f },
	{ 0.5f, 1.499f, 0.166f, 0.0f },
	{ 0.208f, 0.478f, 0.1f, 0.0f },
	{ 0.3f, 2.309f, 0.6f, 0.0f },
	{ 0.203f, 4.279f, 0.5f, 0.0f },
	{ 0.5f, 3.961f, 0.5f, 0.0f },
	{ 0.5f, 2.886f, 1.3f, 0.0f },
	{ 0.361f, 7.284f, 0.332f, 0.0f },
	{ 0.5f, 10.0f, 0.3f, 0.0f },
	{ 0.153f, 0.259f, 0.55f, 0.0f },
	{ 0.361f, 1.493f, 0.5f, 0.0f },
	{ 0.444f, 2.697f, 0.638f, 0.0f },
	{ 0.25f, 1.49f, 0.5f, 100.0f },
	{ 0.111f, 1.49f, 0.5f, 100.0f },
	{ 0.08f, 1.2f, 0.5f, 100.0f },
	{ 0.05f, 0.8f, 0.7f, 200.0f },
	{ 0.55f, 3.0f, 0.45f, 150.0f },
	{ 0.12f, 0.8f, 0.18f, 150.0f },
	{ 0.208f, 1.652f, 1.5f, 0.0f },
	{ 0.652f, 2.886f, 0.25f, 0.0f },
	{ 1.0f, 1.499f, 0.1f, 0.0f },
	{ 0.875f, 8.392f, 1.39f, 0.0f },
	{ 0.139f, 10.0f, 1.5f, 0.0f },
	{ 0.486f, 7.563f, 4.0f, 0.0f },
	// The DLL reads this row past the end of the table (its delay times)
	{ 0.0297f, 0.0371f, 0.0411f, 0.0437f }
};

static const float kLineDelaySeconds[EMIMilesFilter::kLineCount] = {
	0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.005f, 0.0017f
};

static const float kAllpassTimeConstants[2] = {
	0.09685350209474564f, 0.032924000173807144f
};

static const float kMinActiveMix = 0.0001f;
static const float kMaxTime = 10000.0f;
static const float kMinTime = 0.01f;
static const float kMaxPredelay = 250.0f;
static const float kMinDamping = 20.0f;
static const float kMinPresetDamping = 200.0f;
static const float kPi = 3.1415927f;
static const float kAntiDenormal = 1e-10f;
static const float kMillisecond = 0.001f;
static const float kRightWetWeight = 0.15f;
static const float kLeftWetWeight = 0.25f;

EMIMilesFilter *EMIMilesFilter::create(LMemoryServices *memoryServices, int driverRate) {
	EMIMilesFilter *filter = new EMIMilesFilter(memoryServices, driverRate);

	bool allocated = filter->_predelayBuffer != nullptr;

	for (int i = 0; i < kLineCount; i++) {
		if (!filter->_lineBuffer[i])
			allocated = false;
	}

	if (!allocated) {
		warning("EMIMilesFilter::create(): out of memory");
		delete filter;

		return nullptr;
	}

	return filter;
}

EMIMilesFilter::EMIMilesFilter(LMemoryServices *memoryServices, int driverRate) {
	_memoryServices = memoryServices;
	_rate = (float)driverRate;
	_active = false;
	_timeMs = 3000.0f;
	_predelayMs = 0.0f;
	_dampingHz = 5000.0f;
	_mix = 0.0f;
	_environment = 0.0f;

	for (int i = 0; i < kLineCount; i++) {
		_lineIndex[i] = 0;
		_lineGain[i] = 0.0f;
		_lineSize[i] = (int)llrint((double)_rate * (double)kLineDelaySeconds[i]);
		_lineBuffer[i] = (float *)_memoryServices->mallocImpl((uint)(_lineSize[i] * sizeof(float)));

		if (_lineBuffer[i])
			memset(_lineBuffer[i], 0, _lineSize[i] * sizeof(float));
	}

	int predelayCapacity = (int)llrint((double)kMaxPredelay * (double)kMillisecond * (double)_rate);

	_predelayIndex = 0;
	_predelayLength = 0;
	_predelayBuffer = (float *)_memoryServices->mallocImpl((uint)(predelayCapacity * sizeof(float)));

	if (_predelayBuffer)
		memset(_predelayBuffer, 0, predelayCapacity * sizeof(float));

	_x1 = 0.0f;
	_x2 = 0.0f;
	_y1 = 0.0f;
	_y2 = 0.0f;
	_b0 = 0.0f;
	_b1 = 0.0f;
	_b2 = 0.0f;
	_a1 = 0.0f;
	_a2 = 0.0f;

	memset(_work, 0, sizeof(_work));

	recomputeParameters();
}

EMIMilesFilter::~EMIMilesFilter() {
	for (int i = 0; i < kLineCount; i++) {
		if (_lineBuffer[i])
			_memoryServices->freeImpl(_lineBuffer[i]);
	}

	if (_predelayBuffer)
		_memoryServices->freeImpl(_predelayBuffer);
}

void *EMIMilesFilter::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl((uint)size);
}

void EMIMilesFilter::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

void EMIMilesFilter::setTime(float milliseconds) {
	_timeMs = milliseconds;

	if (milliseconds > kMaxTime)
		_timeMs = kMaxTime;
	else if (milliseconds < kMinTime)
		_timeMs = kMinTime;

	recomputeParameters();
}

void EMIMilesFilter::setMix(float mix) {
	_mix = mix;

	if (mix > 1.0f)
		_mix = 1.0f;
	else if (mix < 0.0f)
		_mix = 0.0f;

	recomputeParameters();
}

void EMIMilesFilter::setDamping(float hertz) {
	_dampingHz = hertz;

	if (hertz > _rate * 0.5f)
		_dampingHz = _rate * 0.5f;
	else if (hertz < kMinDamping)
		_dampingHz = kMinDamping;

	recomputeParameters();
}

void EMIMilesFilter::setEnvironment(float environment) {
	_environment = environment;

	if (environment > (float)kMaxPreset)
		_environment = (float)kMaxPreset;
	else if (environment < 0.0f)
		_environment = 0.0f;

	int index = (int)llrint((double)_environment);

	if (index < 0 || index > kMaxPreset) {
		warning("EMIMilesFilter::setEnvironment(): invalid environment %f", environment);
		index = 0;
	}

	const EMIMilesReverbPreset &preset = kPresets[index];

	_timeMs = (float)((double)preset.time * 1000.0);
	_mix = preset.mix;
	_dampingHz = (float)((double)_rate * 0.125 * (double)preset.damping);
	_predelayMs = preset.predelay;

	if (_predelayMs > kMaxPredelay)
		_predelayMs = kMaxPredelay;
	else if (_predelayMs < 0.0f)
		_predelayMs = 0.0f;

	if (_dampingHz > _rate * 0.125f)
		_dampingHz = _rate * 0.125f;
	else if (_dampingHz < kMinPresetDamping)
		_dampingHz = kMinPresetDamping;

	if (_mix > 1.0f)
		_mix = 1.0f;
	else if (_mix < 0.0f)
		_mix = 0.0f;

	if (_timeMs > kMaxTime)
		_timeMs = kMaxTime;
	else if (_timeMs < kMinTime)
		_timeMs = kMinTime;

	recomputeParameters();
}

bool EMIMilesFilter::isActive() const {
	return _active;
}

void EMIMilesFilter::recomputeParameters() {
	if (_mix >= kMinActiveMix) {
		_active = true;

		float seconds = (float)((double)_timeMs * (double)kMillisecond);

		for (int i = 0; i < kLineCount; i++) {
			double denominator = i < 4 ? (double)seconds : (double)kAllpassTimeConstants[i - 4];

			_lineGain[i] = (float)pow((double)kMillisecond, (double)kLineDelaySeconds[i] / denominator);
		}

		_predelayLength = (int)llrint((double)_predelayMs * (double)kMillisecond * (double)_rate);

		double angle = (double)_dampingHz * (double)kPi / (double)_rate;
		double cotangent = 1.0 / tan(angle);
		double root2 = sqrt(2.0);
		double gain = 1.0 / ((root2 + cotangent) * cotangent + 1.0);

		_b0 = (float)gain;
		_b1 = (float)(gain + gain);
		_b2 = (float)gain;
		_a1 = (float)((1.0 - cotangent * cotangent) * gain * 2.0);
		_a2 = (float)((1.0 - root2 * cotangent + cotangent * cotangent) * gain);
	} else {
		if (_active) {
			_x1 = 0.0f;
			_x2 = 0.0f;
			_y1 = 0.0f;
			_y2 = 0.0f;
		}

		_active = false;
	}
}

bool EMIMilesFilter::process(const byte **src, uint32 *srcFract, const byte *srcEnd, int32 **dst, const int32 *dstEnd, int32 *prevL, int32 *prevR, int ratio, int scaleL, int scaleR, int, EMIMilesDriver *mixer, uint flags) {
	if (!_active)
		return false;

	if (!(flags & EMIMilesDriver::kMixOutputStereo)) {
		warning("EMIMilesFilter::process(): mono output is not supported");

		return false;
	}

	int available = (int)(dstEnd - *dst);

	if (available > kWorkSamples) {
		warning("EMIMilesFilter::process(): build buffer span of %d samples is too big", available);
		available = kWorkSamples;
	} else if (available < 0) {
		available = 0;
	}

	memset(_work, 0, available * sizeof(int32));

	int32 *workPtr = _work;

	mixer->mixerMerge(src, srcFract, srcEnd, &workPtr, _work + available, prevL, prevR, ratio, scaleL, scaleR, flags);

	int count = (int)(workPtr - _work);
	float dryGain = (float)(1.0 - (double)_mix);
	double mix = (double)_mix;
	double dry = (double)dryGain;
	int32 *out = *dst;

	for (int k = 0; k < count; k += 2) {
		double left = (double)_work[k];
		float right = (float)_work[k + 1];

		float delayed = _predelayBuffer[_predelayIndex];

		_predelayBuffer[_predelayIndex] = (float)((double)right + left);

		if (++_predelayIndex >= _predelayLength)
			_predelayIndex = 0;

		double accumulator = (double)delayed * (double)_b0 + (double)_x2 * (double)_b2;

		accumulator += (double)_x1 * (double)_b1;
		accumulator -= (double)_a1 * (double)_y1;
		accumulator -= (double)_a2 * (double)_y2;

		float filtered = (float)accumulator;

		_x2 = _x1;
		_x1 = delayed;
		_y2 = _y1;
		_y1 = (float)((double)filtered + (double)kAntiDenormal);

		float comb[4];

		for (int i = 0; i < 4; i++)
			comb[i] = _lineBuffer[i][_lineIndex[i]];

		float combSum = (float)((((double)comb[1] + (double)comb[2]) + (double)comb[3]) + (double)comb[0]);

		for (int i = 0; i < 4; i++) {
			_lineBuffer[i][_lineIndex[i]] = (float)((double)_lineGain[i] * (double)comb[i] + (double)filtered);

			if (++_lineIndex[i] >= _lineSize[i])
				_lineIndex[i] = 0;
		}

		float oldFirst = _lineBuffer[4][_lineIndex[4]];
		double firstNew = (double)oldFirst * (double)_lineGain[4] + (double)combSum;

		_lineBuffer[4][_lineIndex[4]] = (float)firstNew;

		float firstOut = (float)((double)oldFirst - firstNew * (double)_lineGain[4]);

		if (++_lineIndex[4] >= _lineSize[4])
			_lineIndex[4] = 0;

		float oldSecond = _lineBuffer[5][_lineIndex[5]];
		float feedback = (float)((double)oldSecond * (double)_lineGain[5]);
		double secondNew = (double)firstOut * (double)kLeftWetWeight + (double)feedback;

		_lineBuffer[5][_lineIndex[5]] = (float)secondNew;

		double wetLeft = (double)oldSecond - secondNew * (double)_lineGain[5];
		float outLeft = (float)(wetLeft * mix + left * dry);
		double wetRight = (double)oldSecond - ((double)firstOut * (double)kRightWetWeight + (double)feedback) * (double)_lineGain[5];
		double outRight = wetRight * mix + (double)right * dry;

		if (++_lineIndex[5] >= _lineSize[5])
			_lineIndex[5] = 0;

		out[k] = (int32)((uint32)out[k] + (uint32)(uint64)llrint((double)outLeft));
		out[k + 1] = (int32)((uint32)out[k + 1] + (uint32)(uint64)llrint(outRight));
	}

	*dst += count;

	return true;
}

} // End of namespace Grim
