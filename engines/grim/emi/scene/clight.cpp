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

#include "engines/grim/emi/scene/clight.h"
#include "engines/grim/emi/core/lmemoryservices.h"

namespace Grim {

static const float kMinLength = 1e-20f;
static const float kNoLight = -1.0f;

CLight::CLight(Color lightColor, float lightIntensity, float lightFocusDistance, float lightSpreadDistance) {
	_type = kTypeOmni;
	_color = lightColor;
	_spreadDistance = lightSpreadDistance;
	_intensity = lightIntensity;
	_umbraAngle = 0.0f;
	_penumbraAngle = 0.0f;
	_focusDistance = lightFocusDistance;
}

void *CLight::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void CLight::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

void CLight::setColor(int newR, int newG, int newB) {
	_color._r = newR;
	_color._g = newG;
	_color._b = newB;
}

void CLight::addLighting(Color *accumulator, const float *lightTransform, const float *position, const float *normal) {
	double factor = evaluate(lightTransform, position, normal);
	float factorStored = (float)factor;

	if (factor >= 0.0) {
		float addedR = (float)((double)(uint)_color._r * factorStored);

		accumulator->_r += (int)lrint(addedR);

		float addedG = (float)((double)(uint)_color._g * factorStored);

		accumulator->_g += (int)lrint(addedG);

		float addedB = (float)((double)(uint)_color._b * factorStored);

		accumulator->_b += (int)lrint(addedB);
	}
}

float CLight::evaluate(const float *lightTransform, const float *position, const float *normal) {
	// The original could divide by zero in this function, let's avoid that...
	switch (_type) {
	case kTypeOmni: {
		float dirX = lightTransform[9] - position[0];
		float dirY = lightTransform[10] - position[1];
		float dirZ = lightTransform[11] - position[2];
		double distance = sqrt((double)dirZ * dirZ + (double)dirY * dirY + (double)dirX * dirX);

		if (distance >= (double)kMinLength) {
			double inverse = 1.0 / distance;

			dirX = (float)(dirX * inverse);
			dirY = (float)(dirY * inverse);
			dirZ = (float)(dirZ * inverse);
		} else {
			distance = 0.0;
		}

		float distanceStored = (float)distance;

		if (distance > _spreadDistance)
			return kNoLight;

		double dotProduct = (double)dirY * normal[1] + (double)dirZ * normal[2] + (double)dirX * normal[0];

		if (dotProduct < 0.0)
			return kNoLight;

		if (distanceStored > (double)_focusDistance && _spreadDistance != _focusDistance)
			dotProduct = dotProduct * (((double)_spreadDistance - distanceStored) / ((double)_spreadDistance - _focusDistance));

		return (float)(dotProduct * _intensity);
	}
	case kTypeSpot: {
		float axisX = -lightTransform[6];
		float axisY = -lightTransform[7];
		float axisZ = -lightTransform[8];
		double dirX = (double)lightTransform[9] - position[0];
		float dirYStored = (float)((double)lightTransform[10] - position[1]);
		double dirZ = (double)lightTransform[11] - position[2];
		float distance = (float)sqrt((double)dirYStored * dirYStored + dirZ * dirZ + dirX * dirX);

		if (distance > (double)_spreadDistance)
			return kNoLight;

		if (distance == 0.0f)
			return kNoLight;

		double inverse = 1.0 / distance;
		float unitX = (float)(inverse * dirX);
		float unitY = (float)(dirYStored * inverse);
		double unitZ = dirZ * inverse;
		double cosine = unitZ * normal[2] + (double)unitY * normal[1] + (double)unitX * normal[0];
		float cosineStored = (float)cosine;

		if (cosine < 0.0)
			return kNoLight;

		float angleCosine = (float)(unitZ * axisZ + (double)unitY * axisY + (double)axisX * unitX);
		float angle = (float)acos(angleCosine);

		if (angle > _penumbraAngle)
			return kNoLight;

		double result;

		if (angle <= (double)_umbraAngle)
			result = cosineStored;
		else if (_penumbraAngle != _umbraAngle)
			result = ((double)_penumbraAngle - angle) / ((double)_penumbraAngle - _umbraAngle) * cosineStored;
		else
			result = cosineStored;

		if (distance > (double)_focusDistance && _spreadDistance != _focusDistance)
			result = result * (((double)_spreadDistance - distance) / ((double)_spreadDistance - _focusDistance));

		return (float)(result * _intensity);
	}
	case kTypeDirect: {
		double result = (double)-lightTransform[8] * normal[2] + (double)-lightTransform[7] * normal[1] + (double)-lightTransform[6] * normal[0];

		if (result >= 0.0)
			return (float)(result * _intensity);

		return kNoLight;
	}
	case kTypeAmbient:
		return _intensity;
	default:
		return kNoLight;
	}
}

bool CLight::isOutOfRange(const float *lightTransform, const BBox *box) {
	if (_type != kTypeOmni && _type != kTypeSpot)
		return false;

	double squaredDistance = 0.0;
	float posX = lightTransform[9];
	float posY = lightTransform[10];
	float posZ = lightTransform[11];

	if (posX >= box->_min._x) {
		if (posX > box->_max._x) {
			double delta = (double)posX - box->_max._x;

			squaredDistance = delta * delta;
		}
	} else {
		double delta = (double)posX - box->_min._x;

		squaredDistance = delta * delta;
	}

	if (posY >= box->_min._y) {
		if (posY > box->_max._y) {
			double delta = (double)posY - box->_max._y;

			squaredDistance = squaredDistance + delta * delta;
		}
	} else {
		double delta = (double)posY - box->_min._y;

		squaredDistance = squaredDistance + delta * delta;
	}

	if (posZ >= box->_min._z) {
		if (posZ > box->_max._z) {
			double delta = (double)posZ - box->_max._z;

			squaredDistance = squaredDistance + delta * delta;
		}
	} else {
		double delta = (double)posZ - box->_min._z;

		squaredDistance = squaredDistance + delta * delta;
	}

	return squaredDistance > (double)_spreadDistance * _spreadDistance;
}

} // End of namespace Grim
