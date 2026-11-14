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

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

RenderDroid::ZRasterModule::ZRasterModule(RenderDroid *rd) : Module(rd), _numVertices(0), _maxMipMap(0), _yMinVert(0), _yMaxVert(0),
		_leftVertexStep(0), _rightVertexStep(0), _intensities(nullptr), _vertices(nullptr), _texVertices(nullptr),
		_rgbToPalette(nullptr), _rgbMap(nullptr), _monoMap(nullptr), _alphaMap(nullptr), _pixels(nullptr),
		_uWrap(0), _vWrap(0), _mipMapNum(0), _texPitch(0), _texRowShift(0), _zScaleShift(0), _dUdXModifier(0),
		_dVdXModifier(0), _dOneOverZdXFixed(0), _oneOverZStep(0.0f), _uOverZStep(0.0f), _vOverZStep(0.0f),
		_dOneOverZdX(0.0f), _dOneOverZdY(0.0f), _dUOverZdX(0.0f), _dUOverZdY(0.0f), _dVOverZdX(0.0f),
		_dVOverZdY(0.0f), _RGBcolor(0), _color(0), _caseU(0), _caseV(0), _caseUStep(0), _caseVStep(0), _warnedIdlePointer(false) {
	memset(_oneOverZ, 0, sizeof(_oneOverZ));
	memset(_uOverZ, 0, sizeof(_uOverZ));
	memset(_vOverZ, 0, sizeof(_vOverZ));
}

bool RenderDroid::ZRasterModule::setZScaleShift() {
	_zScaleShift = _rd->_camera.getCurrent()->canvas->zScaleShift;

	if (_zScaleShift < 0 || _zScaleShift > 31) {
		warning("RenderDroid::ZRasterModule::setZScaleShift(): invalid z scale shift %d", _zScaleShift);
		return false;
	}

	return true;
}

float RenderDroid::ZRasterModule::truncToFloat(double value) {
	float result = (float)value;

	if (fabs((double)result) > fabs(value))
		result = nextafterf(result, 0.0f);

	return result;
}

int RenderDroid::ZRasterModule::toFixed(double value) {
	return (int)floor((float)(value * 65536.0));
}

int RenderDroid::ZRasterModule::winTruncate(double value) {
	if (!(value > -2147483649.0 && value < 2147483648.0))
		return (int)0x80000000;

	return (int)value;
}

int RenderDroid::ZRasterModule::wrapVertex(int index, int last) {
	if (index < 0)
		return last;

	if (index > last)
		return 0;

	return index;
}

void RenderDroid::ZRasterModule::selectGradientVertices(int *vertA, int *vertB, int *refVertPointer) {
	*vertA = 0;
	*vertB = 1;
	*refVertPointer = 2;

	if (_numVertices == 3)
		return;

	int last = (int)_numVertices - 1;

	if (_rd->_remastered) {
		int remasteredDirection;
		int remasteredRef = _yMinVert;
		int following = wrapVertex(_yMinVert + 1, last);

		if (following == _yMaxVert) {
			remasteredDirection = -1;
		} else if (wrapVertex(_yMinVert - 1, last) == _yMaxVert) {
			remasteredDirection = 1;
		} else {
			remasteredRef = following;

			float slope = (float)(((double)_vertices[_yMaxVert][1] - _vertices[_yMinVert][1]) / ((double)_vertices[_yMaxVert][0] - _vertices[_yMinVert][0]));
			float predicted = (float)((double)slope * ((double)_vertices[following][0] - _vertices[_yMinVert][0]) + _vertices[_yMinVert][1]);
			double offset = (double)_vertices[following][1] - predicted;

			if (offset < 0.0)
				offset = -offset;

			remasteredDirection = (offset < 1.0) ? -1 : 1;
		}

		float middle = (float)(((double)_vertices[_yMaxVert][0] + _vertices[_yMinVert][0]) * 0.5);
		float largest = 0.0f;

		for (int vert = wrapVertex(_yMinVert + remasteredDirection, last); vert != _yMaxVert; vert = wrapVertex(vert + remasteredDirection, last)) {
			double spread = (double)_vertices[vert][0] - middle;

			if (spread < 0.0)
				spread = -spread;

			float storedSpread = (float)spread;

			if (largest < storedSpread) {
				largest = storedSpread;
				remasteredRef = vert;
			}
		}

		*vertA = _yMinVert;
		*vertB = _yMaxVert;
		*refVertPointer = remasteredRef;

		return;
	}

	int direction;
	int refVert = -1;
	int nextVert = wrapVertex(_yMinVert + 1, last);

	if (nextVert == _yMaxVert) {
		direction = -1;
	} else if (wrapVertex(_yMinVert - 1, last) == _yMaxVert) {
		direction = 1;
	} else {
		refVert = nextVert;
		double slope = ((double)_vertices[_yMaxVert][1] - _vertices[_yMinVert][1]) / ((double)_vertices[_yMaxVert][0] - _vertices[_yMinVert][0]);
		double deviation = _vertices[nextVert][1] - (slope * ((double)_vertices[nextVert][0] - _vertices[_yMinVert][0]) + _vertices[_yMinVert][1]);
		if (deviation < 0.0)
			deviation = -deviation;
		direction = (deviation >= 1.0) ? 1 : -1;
	}

	float maxDeviation = 0.0f;
	int firstVert = wrapVertex(_yMinVert + direction, last);
	for (int vert = firstVert; vert != _yMaxVert; vert = wrapVertex(vert + direction, last)) {
		float midX = (float)(((double)_vertices[_yMinVert][0] + _vertices[_yMaxVert][0]) * 0.5);
		double deviation = (double)_vertices[vert][0] - midX;
		if (deviation < 0.0)
			deviation = -deviation;
		if (deviation > maxDeviation) {
			maxDeviation = (float)deviation;
			refVert = vert;
		}
	}

	if (refVert < 0) {
		warning("RenderDroid::ZRasterModule::selectGradientVertices(): no reference vertex found");
		refVert = firstVert;
	}

	*vertA = _yMinVert;
	*vertB = _yMaxVert;
	*refVertPointer = refVert;
}

void RenderDroid::ZRasterModule::selectMipLevel(float z) {
	switch (_maxMipMap) {
	case 0:
		_mipMapNum = 0;
		break;
	case 1:
		_mipMapNum = (z > (double)_rd->_mipDistances[1]) ? 1 : 0;
		break;
	case 2:
		if (z <= (double)_rd->_mipDistances[0])
			_mipMapNum = 0;
		else if (z > (double)_rd->_mipDistances[1])
			_mipMapNum = 2;
		else
			_mipMapNum = 1;
		break;
	case 3:
		if (z <= (double)_rd->_mipDistances[0])
			_mipMapNum = 0;
		else if (z <= (double)_rd->_mipDistances[1])
			_mipMapNum = 1;
		else if (z > (double)_rd->_mipDistances[2])
			_mipMapNum = 3;
		else
			_mipMapNum = 2;
		break;
	default:
		warning("RenderDroid::ZRasterModule::selectMipLevel(): invalid mip level count");
		break;
	}
}

int RenderDroid::ZRasterModule::gradientModifier(double dX, double dY) {
	if (dX > 0.0)
		return 0x8000;

	if (dX < 0.0)
		return 0x7FFF;

	return (dY < 0.0) ? 0x7FFF : 0x8000;
}

void RenderDroid::ZRasterModule::calcAffineGradients(float z) {
	int vertA;
	int vertB;
	int refVert;

	selectGradientVertices(&vertA, &vertB, &refVert);

	int numVertices = (int)_numVertices;
	if (numVertices > 32) {
		warning("RenderDroid::ZRasterModule::calcAffineGradients(): too many vertices");
		numVertices = 32;
	}

	for (int i = 0; i < numVertices; i++)
		_oneOverZ[i] = (float)(1.0 / ((double)_vertices[i][2] + 1.0));

	if (_rd->_remastered) {
		float storedDxA = (float)((double)_vertices[vertA][0] - _vertices[refVert][0]);
		float storedDxB = (float)((double)_vertices[vertB][0] - _vertices[refVert][0]);
		float storedDyA = (float)((double)_vertices[vertA][1] - _vertices[refVert][1]);
		float storedDyB = (float)((double)_vertices[vertB][1] - _vertices[refVert][1]);
		float storedDuB = (float)((double)_texVertices[vertB][0] - _texVertices[refVert][0]);
		float storedDuA = (float)((double)_texVertices[vertA][0] - _texVertices[refVert][0]);
		float storedDvB = (float)((double)_texVertices[vertB][1] - _texVertices[refVert][1]);
		float storedDvA = (float)((double)_texVertices[vertA][1] - _texVertices[refVert][1]);
		float storedInvDet = (float)(1.0 / ((double)storedDyA * storedDxB - (double)storedDyB * storedDxA));
		float storedNegInvDet = -storedInvDet;
		float storedDUdX = (float)(((double)storedDuB * storedDyA - (double)storedDuA * storedDyB) * storedInvDet);
		float storedDUdY = (float)(((double)storedDuB * storedDxA - (double)storedDuA * storedDxB) * storedNegInvDet);
		float storedDVdX = (float)(((double)storedDvB * storedDyA - (double)storedDvA * storedDyB) * storedInvDet);
		float storedDVdY = (float)(((double)storedDvB * storedDxA - (double)storedDvA * storedDxB) * storedNegInvDet);

		selectMipLevel(z);
		_dUdXModifier = gradientModifier(storedDUdX, storedDUdY);
		_dVdXModifier = gradientModifier(storedDVdX, storedDVdY);

		return;
	}

	double dxA = (double)_vertices[vertA][0] - _vertices[refVert][0];
	double dxB = (double)_vertices[vertB][0] - _vertices[refVert][0];
	double dyA = (double)_vertices[vertA][1] - _vertices[refVert][1];
	double dyB = (double)_vertices[vertB][1] - _vertices[refVert][1];
	float xA = (float)dxA;
	float xB = (float)dxB;
	double duA = (double)_texVertices[vertA][0] - _texVertices[refVert][0];
	double duB = (double)_texVertices[vertB][0] - _texVertices[refVert][0];
	double dvB = (double)_texVertices[vertB][1] - _texVertices[refVert][1];
	float uA = (float)duA;
	float uB = (float)duB;
	float vB = (float)dvB;
	float vA = (float)((double)_texVertices[vertA][1] - _texVertices[refVert][1]);
	double det = 1.0 / (dyA * xB - dyB * xA);
	float invDet = (float)det;
	float negInvDet = (float)-det;
	float dUdX = (float)((duB * dyA - duA * dyB) * invDet);
	float dUdY = (float)(((double)uB * xA - (double)uA * xB) * negInvDet);
	float dVdX = (float)((vB * dyA - vA * dyB) * invDet);
	float dVdY = (float)(((double)vB * xA - (double)vA * xB) * negInvDet);

	selectMipLevel(z);
	_dUdXModifier = gradientModifier(dUdX, dUdY);
	_dVdXModifier = gradientModifier(dVdX, dVdY);
}

void RenderDroid::ZRasterModule::calcPerspGradients(float z) {
	int vertA;
	int vertB;
	int refVert;

	selectGradientVertices(&vertA, &vertB, &refVert);

	int numVertices = (int)_numVertices;
	if (numVertices > 32) {
		warning("RenderDroid::ZRasterModule::calcPerspGradients(): too many vertices");
		numVertices = 32;
	}

	if (_rd->_remastered) {
		for (int i = 0; i < numVertices; i++) {
			_oneOverZ[i] = (float)(1.0 / ((double)_vertices[i][2] + 1.0));
			_uOverZ[i] = (float)((double)_oneOverZ[i] * _texVertices[i][0]);
			_vOverZ[i] = (float)((double)_texVertices[i][1] * _oneOverZ[i]);
		}

		float storedDyA = (float)((double)_vertices[vertA][1] - _vertices[refVert][1]);
		float storedDyB = (float)((double)_vertices[vertB][1] - _vertices[refVert][1]);
		float storedDxA = (float)((double)_vertices[vertA][0] - _vertices[refVert][0]);
		float storedDxB = (float)((double)_vertices[vertB][0] - _vertices[refVert][0]);
		float storedDOneA = (float)((double)_oneOverZ[vertA] - _oneOverZ[refVert]);
		float storedDOneB = (float)((double)_oneOverZ[vertB] - _oneOverZ[refVert]);
		float storedDUA = (float)((double)_uOverZ[vertA] - _uOverZ[refVert]);
		float storedDUB = (float)((double)_uOverZ[vertB] - _uOverZ[refVert]);
		float storedDVA = (float)((double)_vOverZ[vertA] - _vOverZ[refVert]);
		float storedDVB = (float)((double)_vOverZ[vertB] - _vOverZ[refVert]);
		float storedInvDet = (float)(1.0 / ((double)storedDyA * storedDxB - (double)storedDyB * storedDxA));
		float storedNegInvDet = -storedInvDet;

		_dOneOverZdX = (float)(((double)storedDOneB * storedDyA - (double)storedDOneA * storedDyB) * storedInvDet);
		_dOneOverZdY = (float)(((double)storedDOneB * storedDxA - (double)storedDOneA * storedDxB) * storedNegInvDet);
		_dOneOverZdXFixed = winTruncate((double)_dOneOverZdX * 65536.0);
		_dUOverZdX = (float)(((double)storedDUB * storedDyA - (double)storedDUA * storedDyB) * storedInvDet);
		_dUOverZdY = (float)(((double)storedDUB * storedDxA - (double)storedDUA * storedDxB) * storedNegInvDet);
		_dVOverZdX = (float)(((double)storedDVB * storedDyA - (double)storedDVA * storedDyB) * storedInvDet);
		_dVOverZdY = (float)(((double)storedDVB * storedDxA - (double)storedDVA * storedDxB) * storedNegInvDet);

		selectMipLevel(z);

		float modifierUX = (float)((double)_oneOverZ[vertA] * _dUOverZdX - (double)_uOverZ[vertA] * _dOneOverZdX);
		float modifierUY = (float)((double)_oneOverZ[vertA] * _dUOverZdY - (double)_uOverZ[vertA] * _dOneOverZdY);
		float modifierVX = (float)((double)_oneOverZ[vertA] * _dVOverZdX - (double)_vOverZ[vertA] * _dOneOverZdX);
		float modifierVY = (float)((double)_oneOverZ[vertA] * _dVOverZdY - (double)_vOverZ[vertA] * _dOneOverZdY);

		_dUdXModifier = gradientModifier(modifierUX, modifierUY);
		_dVdXModifier = gradientModifier(modifierVX, modifierVY);

		return;
	}

	for (int i = 0; i < numVertices; i++) {
		double oneOverZ = 1.0 / ((double)_vertices[i][2] + 1.0);
		_oneOverZ[i] = (float)oneOverZ;
		_uOverZ[i] = (float)(_texVertices[i][0] * oneOverZ);
		_vOverZ[i] = (float)((double)_texVertices[i][1] * _oneOverZ[i]);
	}

	double dyA = (double)_vertices[vertA][1] - _vertices[refVert][1];
	double dyB = (double)_vertices[vertB][1] - _vertices[refVert][1];
	float dxA = (float)((double)_vertices[vertA][0] - _vertices[refVert][0]);
	float dxB = (float)((double)_vertices[vertB][0] - _vertices[refVert][0]);
	float dOneA = (float)((double)_oneOverZ[vertA] - _oneOverZ[refVert]);
	float dOneB = (float)((double)_oneOverZ[vertB] - _oneOverZ[refVert]);
	float dUA = (float)((double)_uOverZ[vertA] - _uOverZ[refVert]);
	float dUB = (float)((double)_uOverZ[vertB] - _uOverZ[refVert]);
	float dVA = (float)((double)_vOverZ[vertA] - _vOverZ[refVert]);
	float dVB = (float)((double)_vOverZ[vertB] - _vOverZ[refVert]);
	float invDet = (float)(1.0 / (dyA * dxB - dyB * dxA));
	float negInvDet = -invDet;

	double dOneOverZdX = (dOneB * dyA - dOneA * dyB) * invDet;
	_dOneOverZdX = (float)dOneOverZdX;
	_dOneOverZdY = (float)(((double)dOneB * dxA - (double)dOneA * dxB) * negInvDet);
	_dOneOverZdXFixed = (int)(int64)(dOneOverZdX * 65536.0);
	_dUOverZdX = (float)((dUB * dyA - dUA * dyB) * invDet);
	_dUOverZdY = (float)(((double)dUB * dxA - (double)dUA * dxB) * negInvDet);
	_dVOverZdX = (float)((dVB * dyA - dVA * dyB) * invDet);
	_dVOverZdY = (float)(((double)dVB * dxA - (double)dVA * dxB) * negInvDet);

	selectMipLevel(z);

	double uOverZ = _uOverZ[vertA];
	double vOverZ = _vOverZ[vertA];
	double oneOverZ = _oneOverZ[vertA];
	_dUdXModifier = gradientModifier(oneOverZ * _dUOverZdX - uOverZ * _dOneOverZdX, oneOverZ * _dUOverZdY - uOverZ * _dOneOverZdY);
	_dVdXModifier = gradientModifier(oneOverZ * _dVOverZdX - vOverZ * _dOneOverZdX, oneOverZ * _dVOverZdY - vOverZ * _dOneOverZdY);
}

bool RenderDroid::ZRasterModule::findEdgeSegment(RdZRasterEdge *edge, int vertexStep, uint *vertIdxPointer, uint *nextIdxPointer, float *yStartPointer, int *numLinesPointer) {
	uint vertIdx = edge->vertIdx;
	if (vertIdx == (uint)_yMaxVert)
		return false;

	while (true) {
		uint nextIdx = (uint)wrapVertex((int)vertIdx + vertexStep, (int)_numVertices - 1);
		float yStart = (float)ceil(_vertices[vertIdx][1]);
		int numLines = (int)ceil(_vertices[nextIdx][1]) - (int)yStart;

		edge->y = (int)yStart;
		edge->numLines = numLines;

		if (numLines > 0) {
			*vertIdxPointer = vertIdx;
			*nextIdxPointer = nextIdx;
			*yStartPointer = yStart;
			*numLinesPointer = numLines;
			return true;
		}

		vertIdx = nextIdx;
		if (nextIdx == (uint)_yMaxVert)
			return false;
	}
}

int RenderDroid::ZRasterModule::interpolateEdgeValue(float from, float to, float invDy, float yOffset, int *step) {
	double delta = ((double)to - from) * invDy;
	float deltaStored = (float)delta;

	*step = toFixed(delta);

	return toFixed((double)deltaStored * yOffset + from);
}

int RenderDroid::ZRasterModule::setupEdgeLinear(RdZRasterEdge *edge, int vertexStep, bool textured, bool gouraud) {
	uint vertIdx;
	uint nextIdx;
	float yStart;
	int numLines;

	if (!findEdgeSegment(edge, vertexStep, &vertIdx, &nextIdx, &yStart, &numLines))
		return 1;

	if (_rd->_remastered) {
		if (numLines >= 2) {
			float yOffset = (float)((double)yStart - _vertices[vertIdx][1]);
			float deltaY = (float)((double)_vertices[nextIdx][1] - _vertices[vertIdx][1]);
			float invDy = (float)(1.0 / (double)deltaY);
			float deltaX = (float)((double)_vertices[nextIdx][0] - _vertices[vertIdx][0]);
			float slope = (float)((double)deltaX * invDy);

			edge->dx = winTruncate((double)slope * 65536.0);
			edge->x = winTruncate(((double)slope * yOffset + _vertices[vertIdx][0]) * 65536.0);

			if (textured) {
				float deltaU = (float)(((double)_texVertices[nextIdx][0] - _texVertices[vertIdx][0]) * invDy);
				float deltaV = (float)(((double)_texVertices[nextIdx][1] - _texVertices[vertIdx][1]) * invDy);

				edge->du = winTruncate((double)deltaU * 65536.0);
				edge->u = winTruncate(((double)deltaU * yOffset + _texVertices[vertIdx][0]) * 65536.0);
				edge->dv = winTruncate((double)deltaV * 65536.0);
				edge->v = winTruncate(((double)deltaV * yOffset + _texVertices[vertIdx][1]) * 65536.0);
			}

			if (gouraud) {
				float deltaI = (float)(((double)_intensities[nextIdx] - _intensities[vertIdx]) * invDy);

				edge->dIntensity = winTruncate((double)deltaI * 65536.0);
				edge->intensity = winTruncate(((double)deltaI * yOffset + _intensities[vertIdx]) * 65536.0);
			}

			edge->dOneOverZ = (float)(((double)_oneOverZ[nextIdx] - _oneOverZ[vertIdx]) * invDy);
			edge->oneOverZ = (float)((double)edge->dOneOverZ * yOffset + _oneOverZ[vertIdx]);
		} else {
			edge->x = winTruncate((double)_vertices[vertIdx][0] * 65536.0);

			if (textured) {
				edge->u = winTruncate((double)_texVertices[vertIdx][0] * 65536.0);
				edge->v = winTruncate((double)_texVertices[vertIdx][1] * 65536.0);
			}

			if (gouraud)
				edge->intensity = winTruncate((double)_intensities[vertIdx] * 65536.0);

			edge->oneOverZ = _oneOverZ[vertIdx];
		}

		edge->vertIdx = nextIdx;

		return 0;
	}

	if (numLines >= 2) {
		float yOffset = (float)((double)yStart - _vertices[vertIdx][1]);
		float invDy = (float)(1.0 / ((double)_vertices[nextIdx][1] - _vertices[vertIdx][1]));

		edge->x = interpolateEdgeValue(_vertices[vertIdx][0], _vertices[nextIdx][0], invDy, yOffset, &edge->dx);

		if (textured) {
			edge->u = interpolateEdgeValue(_texVertices[vertIdx][0], _texVertices[nextIdx][0], invDy, yOffset, &edge->du);
			edge->v = interpolateEdgeValue(_texVertices[vertIdx][1], _texVertices[nextIdx][1], invDy, yOffset, &edge->dv);
		}

		if (gouraud)
			edge->intensity = interpolateEdgeValue(_intensities[vertIdx], _intensities[nextIdx], invDy, yOffset, &edge->dIntensity);

		double dOneOverZ = ((double)_oneOverZ[nextIdx] - _oneOverZ[vertIdx]) * invDy;
		edge->dOneOverZ = (float)dOneOverZ;
		edge->oneOverZ = (float)(yOffset * dOneOverZ + _oneOverZ[vertIdx]);
	} else {
		edge->x = toFixed(_vertices[vertIdx][0]);

		if (textured) {
			edge->u = toFixed(_texVertices[vertIdx][0]);
			edge->v = toFixed(_texVertices[vertIdx][1]);
		}

		if (gouraud)
			edge->intensity = toFixed(_intensities[vertIdx]);

		edge->oneOverZ = _oneOverZ[vertIdx];
	}

	edge->vertIdx = nextIdx;

	return 0;
}

int RenderDroid::ZRasterModule::setupLeftEdgePerspective(RdZRasterEdge *edge, int shading) {
	bool gouraud = (shading == kGouraud);
	uint vertIdx;
	uint nextIdx;
	float yStart;
	int numLines;

	if (!findEdgeSegment(edge, _leftVertexStep, &vertIdx, &nextIdx, &yStart, &numLines))
		return 1;

	if (_rd->_remastered) {
		float storedYOffset = (float)((double)yStart - _vertices[vertIdx][1]);
		float startX;

		if (numLines >= 2) {
			float deltaX = (float)((double)_vertices[nextIdx][0] - _vertices[vertIdx][0]);
			float deltaY = (float)((double)_vertices[nextIdx][1] - _vertices[vertIdx][1]);
			float invDy = 0.0f;
			float slope;

			if (gouraud) {
				invDy = (float)(1.0 / (double)deltaY);
				slope = (float)((double)deltaX * invDy);
			} else {
				slope = (float)((double)deltaX / deltaY);
			}

			edge->dx = winTruncate((double)slope * 65536.0);
			startX = (float)((double)storedYOffset * slope + _vertices[vertIdx][0]);
			edge->x = winTruncate(65536.0 * startX);
			edge->dOneOverZ = (float)((double)_dOneOverZdX * slope + _dOneOverZdY);
			edge->dUOverZ = (float)((double)_dUOverZdX * slope + _dUOverZdY);
			edge->dVOverZ = (float)((double)slope * _dVOverZdX + _dVOverZdY);

			if (gouraud) {
				float deltaIntensity = (float)(((double)_intensities[nextIdx] - _intensities[vertIdx]) * invDy);

				edge->dIntensity = winTruncate((double)deltaIntensity * 65536.0);
				edge->intensity = winTruncate(((double)deltaIntensity * storedYOffset + _intensities[vertIdx]) * 65536.0);
			}
		} else {
			startX = _vertices[vertIdx][0];
			edge->x = winTruncate(65536.0 * startX);

			if (gouraud)
				edge->intensity = winTruncate(65536.0 * _intensities[vertIdx]);
		}

		float storedXDelta = (float)((double)startX - _vertices[vertIdx][0]);

		edge->oneOverZ = (float)(((double)_dOneOverZdY * storedYOffset + _oneOverZ[vertIdx]) + (double)_dOneOverZdX * storedXDelta);
		edge->uOverZ = (float)(((double)_dUOverZdY * storedYOffset + _uOverZ[vertIdx]) + (double)_dUOverZdX * storedXDelta);
		edge->vOverZ = (float)(((double)storedYOffset * _dVOverZdY + _vOverZ[vertIdx]) + (double)storedXDelta * _dVOverZdX);
		edge->vertIdx = nextIdx;

		return 0;
	}

	float yOffset = (float)((double)yStart - _vertices[vertIdx][1]);
	float xAtStart;
	float intensityFixed = 0.0f;

	if (numLines >= 2) {
		double dx = (double)_vertices[nextIdx][0] - _vertices[vertIdx][0];
		double dy = (double)_vertices[nextIdx][1] - _vertices[vertIdx][1];
		float invDy = 0.0f;
		double slope;

		if (gouraud) {
			invDy = (float)(1.0 / dy);
			slope = dx * invDy;
		} else {
			slope = dx / dy;
		}

		float slopeStored = (float)slope;
		double xValue = (double)slopeStored * yOffset + _vertices[vertIdx][0];

		xAtStart = (float)xValue;
		edge->dx = toFixed(slope);
		edge->x = toFixed(xValue);
		edge->dOneOverZ = (float)((double)slopeStored * _dOneOverZdX + _dOneOverZdY);
		edge->dUOverZ = (float)((double)slopeStored * _dUOverZdX + _dUOverZdY);
		edge->dVOverZ = (float)((double)slopeStored * _dVOverZdX + _dVOverZdY);

		if (gouraud) {
			double dIntensity = ((double)_intensities[nextIdx] - _intensities[vertIdx]) * invDy;
			float dIntensityStored = (float)dIntensity;
			edge->dIntensity = toFixed(dIntensity);
			intensityFixed = (float)(((double)dIntensityStored * yOffset + _intensities[vertIdx]) * 65536.0);
		}
	} else {
		xAtStart = _vertices[vertIdx][0];
		edge->x = toFixed(xAtStart);
		if (gouraud)
			intensityFixed = (float)(_intensities[vertIdx] * 65536.0);
	}

	if (gouraud)
		edge->intensity = (int)floor(intensityFixed);

	double xDelta = (double)xAtStart - _vertices[vertIdx][0];
	double yOneOverZ = (double)yOffset * _dOneOverZdY;
	double yUOverZ = (double)yOffset * _dUOverZdY;
	double yVOverZ = (double)yOffset * _dVOverZdY;
	double xOneOverZ = xDelta * _dOneOverZdX;
	double xUOverZ = xDelta * _dUOverZdX;
	double xVOverZ = xDelta * _dVOverZdX;
	bool oneYFirst = true;
	bool uYFirst = true;
	bool vYFirst = false;

	if (_rd->isDemo()) {
		if (shading == kUnlit) {
			uYFirst = false;
		} else if (shading == kFlat) {
			oneYFirst = false;
			vYFirst = true;
		} else {
			vYFirst = true;
		}
	}

	if (oneYFirst)
		edge->oneOverZ = (float)(yOneOverZ + _oneOverZ[vertIdx] + xOneOverZ);
	else
		edge->oneOverZ = (float)(xOneOverZ + _oneOverZ[vertIdx] + yOneOverZ);

	if (uYFirst)
		edge->uOverZ = (float)(yUOverZ + _uOverZ[vertIdx] + xUOverZ);
	else
		edge->uOverZ = (float)(xUOverZ + _uOverZ[vertIdx] + yUOverZ);

	if (vYFirst)
		edge->vOverZ = (float)(yVOverZ + _vOverZ[vertIdx] + xVOverZ);
	else
		edge->vOverZ = (float)(xVOverZ + _vOverZ[vertIdx] + yVOverZ);

	edge->vertIdx = nextIdx;

	return 0;
}

int RenderDroid::ZRasterModule::setupRightEdgePerspective(RdZRasterEdge *edge, bool gouraud) {
	uint vertIdx;
	uint nextIdx;
	float yStart;
	int numLines;

	if (!findEdgeSegment(edge, _rightVertexStep, &vertIdx, &nextIdx, &yStart, &numLines))
		return 1;

	if (_rd->_remastered) {
		if (numLines >= 2) {
			float storedYOffset = (float)((double)yStart - _vertices[vertIdx][1]);
			float deltaX = (float)((double)_vertices[nextIdx][0] - _vertices[vertIdx][0]);
			float deltaY = (float)((double)_vertices[nextIdx][1] - _vertices[vertIdx][1]);
			float invDy = 0.0f;
			float slope;

			if (gouraud) {
				invDy = (float)(1.0 / (double)deltaY);
				slope = (float)((double)deltaX * invDy);
			} else {
				slope = (float)((double)deltaX / deltaY);
			}

			edge->dx = winTruncate((double)slope * 65536.0);
			edge->x = winTruncate(((double)slope * storedYOffset + _vertices[vertIdx][0]) * 65536.0);

			if (gouraud) {
				float deltaIntensity = (float)(((double)_intensities[nextIdx] - _intensities[vertIdx]) * invDy);

				edge->dIntensity = winTruncate((double)deltaIntensity * 65536.0);
				edge->intensity = winTruncate(((double)deltaIntensity * storedYOffset + _intensities[vertIdx]) * 65536.0);
			}
		} else {
			edge->x = winTruncate((double)_vertices[vertIdx][0] * 65536.0);

			if (gouraud)
				edge->intensity = winTruncate((double)_intensities[vertIdx] * 65536.0);
		}

		edge->vertIdx = nextIdx;

		return 0;
	}

	float xFixed;
	float intensityFixed = 0.0f;

	if (numLines >= 2) {
		float yOffset = (float)((double)yStart - _vertices[vertIdx][1]);
		double dx = (double)_vertices[nextIdx][0] - _vertices[vertIdx][0];
		double dy = (double)_vertices[nextIdx][1] - _vertices[vertIdx][1];
		float invDy = 0.0f;
		double slope;

		if (gouraud) {
			invDy = (float)(1.0 / dy);
			slope = dx * invDy;
		} else {
			slope = dx / dy;
		}

		float slopeStored = (float)slope;
		edge->dx = toFixed(slope);
		xFixed = (float)(((double)slopeStored * yOffset + _vertices[vertIdx][0]) * 65536.0);

		if (gouraud) {
			double dIntensity = ((double)_intensities[nextIdx] - _intensities[vertIdx]) * invDy;
			float dIntensityStored = (float)dIntensity;
			edge->dIntensity = toFixed(dIntensity);
			intensityFixed = (float)(((double)dIntensityStored * yOffset + _intensities[vertIdx]) * 65536.0);
		}
	} else {
		xFixed = (float)(_vertices[vertIdx][0] * 65536.0);
		if (gouraud)
			intensityFixed = (float)(_intensities[vertIdx] * 65536.0);
	}

	edge->vertIdx = nextIdx;
	edge->x = (int)floor(xFixed);

	if (gouraud)
		edge->intensity = (int)floor(intensityFixed);

	return 0;
}

int RenderDroid::ZRasterModule::setupLeftEdge(RdZRasterEdge *edge, int mapping, int shading) {
	bool gouraud = (shading == kGouraud);

	if (mapping == kPerspective)
		return setupLeftEdgePerspective(edge, shading);

	return setupEdgeLinear(edge, _leftVertexStep, mapping == kAffine, gouraud);
}

int RenderDroid::ZRasterModule::setupRightEdge(RdZRasterEdge *edge, int mapping, int shading) {
	bool gouraud = (shading == kGouraud);

	if (mapping == kPerspective)
		return setupRightEdgePerspective(edge, gouraud);

	return setupEdgeLinear(edge, _rightVertexStep, mapping == kAffine, gouraud);
}

void RenderDroid::ZRasterModule::stepEdge(RdZRasterEdge *edge, int mapping, int shading, bool isLeft) {
	if (mapping == kPerspective) {
		if (isLeft) {
			edge->oneOverZ += edge->dOneOverZ;
			edge->uOverZ += edge->dUOverZ;
			edge->vOverZ += edge->dVOverZ;
		}
	} else {
		edge->oneOverZ += edge->dOneOverZ;
	}

	edge->x += edge->dx;

	if (mapping == kAffine) {
		edge->u += edge->du;
		edge->v += edge->dv;
	}

	if (shading == kGouraud)
		edge->intensity += edge->dIntensity;

	edge->y++;
}

bool RenderDroid::ZRasterModule::beginScanLine(RdZRasterEdge *left, RdZRasterEdge *right, int bpp, bool signedEnd, ScanState *state) {
	RdCanvas *canvas = _rd->_camera.getCurrent()->canvas;
	StdVBuffer *vBuffer = canvas->vBuffer;
	StdVBuffer *zBuffer = canvas->zBuffer;

	state->xLeft = left->x;
	state->xStart = state->xLeft;
	if ((state->xLeft & 0xFFFF) != 0)
		state->xStart = (int)(((uint)state->xLeft + 0x10000) & 0xFFFF0000);

	uint xEnd = right->x;
	if ((xEnd & 0xFFFF) != 0)
		xEnd = (xEnd + 0x10000) & 0xFFFF0000;

	int x = state->xStart >> 16;
	state->x = x;
	state->numPixels = (signedEnd ? ((int)xEnd >> 16) : (int)(xEnd >> 16)) - x;
	if (state->numPixels <= 0)
		return false;

	if (bpp == 8)
		state->dest = &vBuffer->pixels[left->y * vBuffer->tex.pitchInBytes + x];
	else
		state->dest = &vBuffer->pixels[2 * x + 2 * left->y * (vBuffer->tex.pitchInBytes >> 1)];

	state->zPointer = (uint16 *)&zBuffer->pixels[2 * x + left->y * zBuffer->tex.pitchInBytes];
	state->u = 0;
	state->v = 0;
	state->uStep = 0;
	state->vStep = 0;
	state->intensity = 0;
	state->intensityStep = 0;
	state->dither = false;
	state->ditherX = 0;
	state->ditherY = left->y;

	return true;
}

void RenderDroid::ZRasterModule::setupLinearZ(RdZRasterEdge *left, RdZRasterEdge *right, ScanState *state) {
	if (_rd->_remastered) {
		float storedDelta = (float)(((double)right->oneOverZ - left->oneOverZ) * _rd->_raster.getOneOverNFlex(state->numPixels));
		float storedFraction = (float)((double)(state->xStart - state->xLeft) * 0.0000152587890625);

		state->zStep = winTruncate((double)storedDelta * 65536.0);
		state->z = winTruncate(((double)storedDelta * storedFraction + left->oneOverZ) * 65536.0);

		return;
	}

	double dOneOverZ = ((double)right->oneOverZ - left->oneOverZ) * _rd->_raster.getOneOverNFlex(state->numPixels);
	float dOneOverZStored = (float)dOneOverZ;
	double fraction = (double)(state->xStart - state->xLeft) * 0.0000152587890625;

	state->z = (int)floor((float)(((double)dOneOverZStored * fraction + left->oneOverZ) * 65536.0));
	state->zStep = (int)floor((float)(dOneOverZ * 65536.0));
}

void RenderDroid::ZRasterModule::setupLinearIntensity(RdZRasterEdge *left, RdZRasterEdge *right, ScanState *state) {
	int oneOverN = _rd->_raster.getOneOverNFixed(state->numPixels);
	int xOffset = state->xStart - state->xLeft;

	state->intensityStep = (int)((uint64)((int64)oneOverN * (right->intensity - left->intensity)) >> 16);
	if ((uint)state->intensityStep > 0xFFFFFAE2)
		state->intensityStep = 0;

	state->intensity = left->intensity + (int)((uint64)((int64)state->intensityStep * xOffset) >> 16);
}

int RenderDroid::ZRasterModule::lightIndex(int index, bool gouraud) {
	if (gouraud && (index < 0 || index > 0x3FFF)) {
		warning("RenderDroid::ZRasterModule::lightIndex(): light table index out of range");
		return (index < 0) ? 0 : 0x3FFF;
	}

	return index;
}

int RenderDroid::ZRasterModule::readRGB(int index) {
	return *(uint16 *)&_rgbMap[2 * index];
}

bool RenderDroid::ZRasterModule::plotTexel(const PixelMode &mode, byte *dest, byte texel, int lightBase) {
	bool transparent = (mode.flags & kTransparent) != 0;
	bool masked = (mode.flags & kMasked) != 0;
	bool gouraud = (mode.shading == kGouraud);

	if (masked && texel == 0)
		return false;

	if (mode.bpp == 8) {
		if (mode.shading == kUnlit) {
			*dest = transparent ? _alphaMap[256 * texel + *dest] : texel;
		} else if (transparent && masked) {
			*dest = _monoMap[lightIndex(_alphaMap[256 * texel + *dest] + lightBase, gouraud)];
		} else {
			byte lit = _monoMap[lightIndex(texel + lightBase, gouraud)];
			*dest = transparent ? _alphaMap[256 * lit + *dest] : lit;
		}

		return true;
	}

	uint16 *dest16 = (uint16 *)dest;

	if (!transparent) {
		*dest16 = readRGB(lightIndex(lightBase + texel, gouraud));
	} else if (!masked && mode.shading == kFlat && !mode.perspective) {
		*dest16 = readRGB(_alphaMap[256 * _rgbToPalette[readRGB(texel)] + _rgbToPalette[*dest16]]);
	} else if (!masked && mode.shading != kUnlit && mode.perspective) {
		uint litIndex = (uint)readRGB(lightIndex(lightBase + texel, gouraud)) << 8;
		byte litPalette = 0;

		if (litIndex < 0x10000)
			litPalette = _rgbToPalette[litIndex];
		else
			warning("RenderDroid::ZRasterModule::plotTexel(): palette index out of range");

		*dest16 = readRGB(_alphaMap[litPalette + _rgbToPalette[*dest16]]);
	} else {
		*dest16 = readRGB(lightIndex(lightBase + _alphaMap[256 * texel + _rgbToPalette[*dest16]], gouraud));
	}

	return true;
}

void RenderDroid::ZRasterModule::drawTextureRun(int count, ScanState *state, const ScanParams &params) {
	const int dither[4] = { 0, 49152, 0x8000, 0x4000 };
	int bytesPerPixel = params.mode.bpp / 8;
	bool gouraud = (params.mode.shading == kGouraud);

	for (int i = 0; i < count; i++) {
		int zShifted = state->z >> _zScaleShift;

		if ((uint16)zShifted > *state->zPointer) {
			byte texel = _pixels[((state->u & params.uMask) >> 16) | (_vWrap & (state->v >> params.vShift))];
			int lightBase = 0;

			if (gouraud) {
				if (state->dither)
					lightBase = ((state->intensity + dither[2 * (state->ditherY % 2) + state->ditherX % 2]) >> 8) & ~0xFF;
				else
					lightBase = (state->intensity >> 8) & params.lightMask;
			}

			if (plotTexel(params.mode, state->dest, texel, lightBase))
				*state->zPointer = (uint16)zShifted;
		}

		state->u += state->uStep;
		state->v += state->vStep;
		state->intensity += state->intensityStep;
		state->z += state->zStep;
		state->ditherX--;
		state->dest += bytesPerPixel;
		state->zPointer++;
	}
}

void RenderDroid::ZRasterModule::scanLineSolid(RdZRasterEdge *left, RdZRasterEdge *right, int bpp, int shading, int flags) {
	ScanState state;

	if (!beginScanLine(left, right, bpp, false, &state))
		return;

	bool gouraud = (shading == kGouraud);
	bool transparent = (flags & kTransparent) != 0;

	setupLinearZ(left, right, &state);
	if (gouraud)
		setupLinearIntensity(left, right, &state);

	for (int i = 0; i < state.numPixels; i++) {
		int zShifted = state.z >> _zScaleShift;

		if ((uint16)zShifted > *state.zPointer) {
			int lightBase = gouraud ? ((state.intensity >> 8) & 0x3F00) : 0;

			if (bpp == 8) {
				byte lit = gouraud ? _monoMap[lightBase + _color] : _color;
				*state.dest = transparent ? _alphaMap[256 * lit + *state.dest] : lit;
			} else {
				uint16 *dest16 = (uint16 *)state.dest;

				if (!transparent)
					*dest16 = gouraud ? readRGB(lightBase + _color) : _RGBcolor;
				else
					*dest16 = readRGB(lightBase + _alphaMap[256 * _color + _rgbToPalette[*dest16]]);
			}

			*state.zPointer = (uint16)zShifted;
		}

		state.intensity += state.intensityStep;
		state.z += state.zStep;
		state.dest += bpp / 8;
		state.zPointer++;
	}
}

RenderDroid::ZRasterModule::ScanParams RenderDroid::ZRasterModule::makeScanParams(int bpp, int shading, int flags, bool perspective) {
	ScanParams params;

	params.mode.bpp = bpp;
	params.mode.shading = shading;
	params.mode.flags = flags;
	params.mode.perspective = perspective;
	params.uMask = _uWrap;
	params.vShift = 16 - _texRowShift;
	params.lightMask = 0x3F00;
	params.caseMode = false;

	return params;
}

void RenderDroid::ZRasterModule::setupAffineRun(RdZRasterEdge *left, RdZRasterEdge *right, ScanState *state) {
	int oneOverN = _rd->_raster.getOneOverNFixed(state->numPixels);
	int xOffset = state->xStart - state->xLeft;
	int uStepFull = (int)((uint64)((int64)oneOverN * (right->u - left->u)) >> 16);
	int vStepFull = (int)((uint64)((int64)oneOverN * (right->v - left->v)) >> 16);

	state->uStep = uStepFull >> _mipMapNum;
	state->vStep = vStepFull >> _mipMapNum;
	state->u = _dUdXModifier + ((int)((uint)left->u + (uint)((uint64)((int64)uStepFull * xOffset) >> 16)) >> _mipMapNum);
	state->v = _dVdXModifier + ((int)((uint)left->v + (uint)((uint64)((int64)vStepFull * xOffset) >> 16)) >> _mipMapNum);

	setupLinearZ(left, right, state);
}

void RenderDroid::ZRasterModule::scanLineAffine(RdZRasterEdge *left, RdZRasterEdge *right, const ScanParams &params) {
	ScanState state;

	if (!beginScanLine(left, right, params.mode.bpp, false, &state))
		return;

	setupAffineRun(left, right, &state);
	if (params.mode.shading == kGouraud)
		setupLinearIntensity(left, right, &state);

	if (params.mode.shading == kGouraud && params.mode.bpp == 16 && params.mode.flags == 0 && !params.caseMode) {
		state.dither = true;
		state.ditherX = state.x + state.numPixels - 1;

		if (_rd->_remastered && (((uintptr)state.dest & 3) == 2) && (((uintptr)state.zPointer & 3) == 2)) {
			int lastX = state.x + state.numPixels;

			state.ditherX = lastX;
			drawTextureRun(1, &state, params);
			state.ditherX = lastX - 2;
			drawTextureRun(state.numPixels - 1, &state, params);

			return;
		}
	}

	drawTextureRun(state.numPixels, &state, params);

	if (params.caseMode) {
		_caseU = state.u;
		_caseV = state.v;
		_caseUStep = state.uStep;
		_caseVStep = state.vStep;
	}
}

void RenderDroid::ZRasterModule::setRunSteps(ScanState *state, float uEnd, float vEnd, float uStart, float vStart, double stepScale) {
	if (_rd->_remastered) {
		state->uStep = winTruncate(((double)uEnd - uStart) * stepScale * 65536.0) >> _mipMapNum;
		state->vStep = winTruncate(((double)vEnd - vStart) * stepScale * 65536.0) >> _mipMapNum;
		state->u = _dUdXModifier + (winTruncate((double)uStart * 65536.0) >> _mipMapNum);
		state->v = _dVdXModifier + (winTruncate((double)vStart * 65536.0) >> _mipMapNum);

		return;
	}

	state->uStep = (int)floor((float)(((double)uEnd - uStart) * stepScale * 65536.0)) >> _mipMapNum;
	state->vStep = (int)floor((float)(((double)vEnd - vStart) * stepScale * 65536.0)) >> _mipMapNum;
	state->u = _dUdXModifier + ((int)floor((float)(uStart * 65536.0)) >> _mipMapNum);
	state->v = _dVdXModifier + ((int)floor((float)(vStart * 65536.0)) >> _mipMapNum);
}

void RenderDroid::ZRasterModule::setIdleRunCoords(ScanState *state, const ScanParams &params, float oneOverZ, float uStart, float vStart) {
	bool gouraud = (params.mode.shading == kGouraud);
	bool transparent = (params.mode.flags & kTransparent) != 0;
	bool masked = (params.mode.flags & kMasked) != 0;
	bool is8bpp = (params.mode.bpp == 8);

	if (_rd->_remastered) {
		bool pointerValue = masked && is8bpp && (params.mode.shading == kUnlit || transparent);

		if (pointerValue) {
			if (!_warnedIdlePointer) {
				warning("RenderDroid::ZRasterModule::setIdleRunCoords(): the exe leaves a buffer address as the texture coordinate of a single pixel scanline, using the real coordinates");
				_warnedIdlePointer = true;
			}

			state->u = _dUdXModifier + (winTruncate((double)uStart * 65536.0) >> _mipMapNum);
			state->v = _dVdXModifier + (winTruncate((double)vStart * 65536.0) >> _mipMapNum);
			return;
		}

		if (gouraud && (transparent || masked))
			state->u = state->intensity;
		else
			state->u = state->z;

		state->v = state->u;
		return;
	}

	if (!gouraud && !masked && (transparent || is8bpp)) {
		state->u = state->z;
	} else if (gouraud && !masked && is8bpp) {
		state->u = state->intensity;
	} else if (gouraud && transparent && (masked || !is8bpp)) {
		float zFixed = (float)(oneOverZ * 65536.0);
		memcpy(&state->u, &zFixed, sizeof(state->u));
	} else {
		state->u = _dUdXModifier + ((int)floor((float)(uStart * 65536.0)) >> _mipMapNum);
		state->v = _dVdXModifier + ((int)floor((float)(vStart * 65536.0)) >> _mipMapNum);
		return;
	}

	state->v = state->u;
}

void RenderDroid::ZRasterModule::scanLinePerspective(RdZRasterEdge *left, RdZRasterEdge *right, const ScanParams &params) {
	ScanState state;

	if (!beginScanLine(left, right, params.mode.bpp, true, &state))
		return;

	int numPixels = state.numPixels;
	int headPixels = 0;

	if (state.x % 16 != 0) {
		headPixels = 16 - state.x % 16;
		if (numPixels < headPixels)
			headPixels = numPixels;
	}

	int restPixels = numPixels - headPixels;
	int numSpans = 0;
	int tailPixels = 0;

	if (restPixels != 0) {
		numSpans = restPixels / 16;
		tailPixels = restPixels % 16;
		if (tailPixels == 0) {
			tailPixels = 16;
			numSpans--;
		}
	}

	if (params.mode.shading == kGouraud)
		setupLinearIntensity(left, right, &state);

	if (_rd->_remastered) {
		float storedFraction = (float)((double)(state.xStart - state.xLeft) * 0.0000152587890625);
		float startOneOverZ = (float)((double)storedFraction * _dOneOverZdX + left->oneOverZ);
		float startUOverZ = (float)((double)_dUOverZdX * storedFraction + left->uOverZ);
		float startVOverZ = (float)((double)storedFraction * _dVOverZdX + left->vOverZ);
		float startInverse = (float)(1.0 / (double)startOneOverZ);
		float runStartU = (float)((double)startInverse * startUOverZ);
		float runStartV = (float)((double)startInverse * startVOverZ);

		state.z = winTruncate((double)startOneOverZ * 65536.0);
		state.zStep = _dOneOverZdXFixed;

		float nextOneOverZ;
		float nextUOverZ;
		float nextVOverZ;

		if (headPixels != 0) {
			float headCount = (float)headPixels;
			float headOneOverZ = (float)((double)startOneOverZ + (double)headCount * _dOneOverZdX);
			float headUOverZ = (float)((double)startUOverZ + _dUOverZdX * (double)headCount);
			float headVOverZ = (float)((double)startVOverZ + (double)headCount * _dVOverZdX);
			float headInverse = (float)(1.0 / (double)headOneOverZ);
			float headU = (float)((double)headInverse * headUOverZ);
			float headV = (float)((double)headInverse * headVOverZ);

			setRunSteps(&state, headU, headV, runStartU, runStartV, _rd->_raster.getOneOverNFlex(headPixels));
			drawTextureRun(headPixels, &state, params);

			runStartU = headU;
			runStartV = headV;
			nextOneOverZ = (float)((double)headOneOverZ + _oneOverZStep);
			nextUOverZ = (float)((double)_uOverZStep + headUOverZ);
			nextVOverZ = (float)((double)_vOverZStep + headVOverZ);
		} else {
			setIdleRunCoords(&state, params, startOneOverZ, runStartU, runStartV);

			nextOneOverZ = (float)((double)startOneOverZ + _oneOverZStep);
			nextUOverZ = (float)((double)startUOverZ + _uOverZStep);
			nextVOverZ = (float)((double)startVOverZ + _vOverZStep);
		}

		for (int span = numSpans; span > 0; span--) {
			float spanInverse = (float)(1.0 / (double)nextOneOverZ);
			float spanU = (float)((double)spanInverse * nextUOverZ);
			float spanV = (float)((double)spanInverse * nextVOverZ);

			setRunSteps(&state, spanU, spanV, runStartU, runStartV, 0.0625);
			drawTextureRun(16, &state, params);

			runStartU = spanU;
			runStartV = spanV;
			nextOneOverZ = (float)((double)nextOneOverZ + _oneOverZStep);
			nextUOverZ = (float)((double)nextUOverZ + _uOverZStep);
			nextVOverZ = (float)((double)_vOverZStep + nextVOverZ);
		}

		if (tailPixels != 0) {
			if (tailPixels != 1) {
				float tailCount = (float)(tailPixels - 1);
				float tailOneOverZ = (float)(((double)tailCount * _dOneOverZdX - _oneOverZStep) + nextOneOverZ);
				float tailInverse = (float)(1.0 / (double)tailOneOverZ);
				float tailUOverZ = (float)(((double)tailCount * _dUOverZdX - _uOverZStep) + nextUOverZ);
				float tailU = (float)((double)tailUOverZ * tailInverse);
				float tailVOverZ = (float)(((double)tailCount * _dVOverZdX - _vOverZStep) + nextVOverZ);
				float tailV = (float)((double)tailInverse * tailVOverZ);

				setRunSteps(&state, tailU, tailV, runStartU, runStartV, _rd->_raster.getOneOverNFlex(tailPixels - 1));
			}

			drawTextureRun(tailPixels, &state, params);
		}

		return;
	}

	double fraction = (double)(state.xStart - state.xLeft) * 0.0000152587890625;
	float oneOverZ = truncToFloat(truncToFloat(fraction * _dOneOverZdX) + left->oneOverZ);
	float uOverZ = truncToFloat(truncToFloat(fraction * _dUOverZdX) + left->uOverZ);
	float vOverZ = truncToFloat(truncToFloat(fraction * _dVOverZdX) + left->vOverZ);
	double invOneOverZ = 1.0 / oneOverZ;
	float uStart = (float)(invOneOverZ * uOverZ);
	float vStart = (float)(invOneOverZ * vOverZ);

	state.z = (int)floor((float)(oneOverZ * 65536.0));
	state.zStep = _dOneOverZdXFixed;

	if (params.caseMode) {
		state.u = _caseU;
		state.v = _caseV;
		state.uStep = _caseUStep;
		state.vStep = _caseVStep;
	}

	float oneOverZNext;
	float uOverZNext;
	float vOverZNext;

	if (headPixels != 0) {
		double head = (double)headPixels;
		float oneOverZEnd = (float)(head * _dOneOverZdX + oneOverZ);
		float uOverZEnd = (float)(head * _dUOverZdX + uOverZ);
		float vOverZEnd = (float)(head * _dVOverZdX + vOverZ);
		double invEnd = 1.0 / oneOverZEnd;
		float uEnd = (float)(invEnd * uOverZEnd);
		float vEnd = (float)(invEnd * vOverZEnd);

		setRunSteps(&state, uEnd, vEnd, uStart, vStart, _rd->_raster.getOneOverNFlex(headPixels));
		drawTextureRun(headPixels, &state, params);

		uStart = uEnd;
		vStart = vEnd;
		oneOverZNext = oneOverZEnd + _oneOverZStep;
		uOverZNext = uOverZEnd + _uOverZStep;
		vOverZNext = vOverZEnd + _vOverZStep;
	} else {
		if (!params.caseMode)
			setIdleRunCoords(&state, params, oneOverZ, uStart, vStart);

		oneOverZNext = oneOverZ + _oneOverZStep;
		uOverZNext = uOverZ + _uOverZStep;
		vOverZNext = vOverZ + _vOverZStep;
	}

	for (int span = numSpans; span > 0; span--) {
		double invNext = 1.0 / oneOverZNext;
		float uEnd = (float)(invNext * uOverZNext);
		float vEnd = (float)(invNext * vOverZNext);

		setRunSteps(&state, uEnd, vEnd, uStart, vStart, 0.0625);
		drawTextureRun(16, &state, params);

		uStart = uEnd;
		vStart = vEnd;
		oneOverZNext += _oneOverZStep;
		uOverZNext += _uOverZStep;
		vOverZNext += _vOverZStep;
	}

	if (tailPixels != 0) {
		if (tailPixels != 1) {
			double tail = (double)(tailPixels - 1);
			double invTail = 1.0 / (tail * _dOneOverZdX - _oneOverZStep + oneOverZNext);
			float uEnd = (float)((tail * _dUOverZdX - _uOverZStep + uOverZNext) * invTail);
			float vEnd = (float)((tail * _dVOverZdX - _vOverZStep + vOverZNext) * invTail);

			setRunSteps(&state, uEnd, vEnd, uStart, vStart, _rd->_raster.getOneOverNFlex(tailPixels - 1));
		}

		drawTextureRun(tailPixels, &state, params);
	}

	if (params.caseMode) {
		_caseU = state.u;
		_caseV = state.v;
		_caseUStep = state.uStep;
		_caseVStep = state.vStep;
	}
}

void RenderDroid::ZRasterModule::scanLineCase(RdZRasterEdge *left, RdZRasterEdge *right, int mapping, int shading, int pitch) {
	ScanParams params = makeScanParams(16, shading, 0, mapping == kPerspective);

	params.caseMode = true;

	if (pitch != 0) {
		int shift = 16;
		for (int size = pitch; size > 1; size >>= 1)
			shift--;

		params.uMask = (pitch - 1) << 16;
		params.vShift = shift;
	} else if (shading == kGouraud) {
		params.lightMask = -1;
	}

	if (mapping == kPerspective)
		scanLinePerspective(left, right, params);
	else
		scanLineAffine(left, right, params);
}

int RenderDroid::ZRasterModule::roundToInt(double value) {
	double lower = floor(value);
	double fraction = value - lower;

	if (fraction > 0.5 || (fraction == 0.5 && lower - 2.0 * floor(lower * 0.5) != 0.0))
		lower += 1.0;

	// Out of range gives the same result as fistp
	if (!(lower >= -2147483648.0 && lower <= 2147483647.0))
		return -2147483647 - 1;

	return (int)lower;
}

void RenderDroid::ZRasterModule::setCaseRun(float uStart, float vStart, double uEnd, double vEnd, double stepScale, int shift) {
	_caseV = roundToInt((double)vStart * 65536.0) >> shift;
	_caseU = roundToInt((double)uStart * 65536.0) >> shift;
	_caseVStep = roundToInt((vEnd - (double)vStart) * stepScale) >> shift;
	_caseUStep = roundToInt((uEnd - (double)uStart) * stepScale) >> shift;
}

uint RenderDroid::ZRasterModule::zTestMask(ScanState *state, int count) {
	uint mask = 0;

	for (int i = 0; i < count; i++) {
		if ((uint16)state->z > state->zPointer[i]) {
			state->zPointer[i] = (uint16)state->z;
			mask |= 1u << i;
		}

		state->z = (int)((uint)state->z + (uint)state->zStep);
	}

	state->zPointer += count;

	return mask;
}

byte RenderDroid::ZRasterModule::fetchTexel8(int u, int v, int vShift) {
	return _pixels[(_vWrap & ((uint)v >> vShift)) + (((uint)_uWrap & (uint)u) >> 16)];
}

byte RenderDroid::ZRasterModule::shadeTexel8(int shading, byte texel, uint intensity) {
	if (shading == kUnlit)
		return texel;

	if (shading == kFlat)
		return _monoMap[texel];

	return _monoMap[((intensity & 0x3F0000) >> 8) + texel];
}

void RenderDroid::ZRasterModule::drawPixels8(ScanState *state, int count, uint mask, int shading, int vShift) {
	for (int i = 0; i < count; i++) {
		if (mask & (1u << i))
			state->dest[i] = shadeTexel8(shading, fetchTexel8(state->u, state->v, vShift), (uint)state->intensity);

		state->u = (int)((uint)state->u + (uint)state->uStep);
		state->v = (int)((uint)state->v + (uint)state->vStep);
		state->intensity = (int)((uint)state->intensity + (uint)state->intensityStep);
	}

	state->dest += count;
}

void RenderDroid::ZRasterModule::drawBlock8(ScanState *state, uint mask, int shading, int vShift) {
	for (int i = 0; i < 16; i++) {
		if (mask & (1u << i)) {
			uint intensity = (uint)state->intensity + (uint)(i & ~1) * (uint)state->intensityStep;

			state->dest[i] = shadeTexel8(shading, fetchTexel8(state->u, state->v, vShift), intensity);
		}

		state->u = (int)((uint)state->u + (uint)state->uStep);
		state->v = (int)((uint)state->v + (uint)state->vStep);
	}

	state->intensity = (int)((uint)state->intensity + 16u * (uint)state->intensityStep);
	state->dest += 16;
}

void RenderDroid::ZRasterModule::scanLineAffineCase8(RdZRasterEdge *left, RdZRasterEdge *right, int shading, int vShift) {
	ScanState state;

	if (!beginScanLine(left, right, 8, false, &state))
		return;

	setupAffineRun(left, right, &state);
	if (shading == kGouraud)
		setupLinearIntensity(left, right, &state);

	int numBlocks = state.numPixels >> 4;
	int tailPixels = state.numPixels & 15;

	for (int block = 0; block < numBlocks; block++) {
		uint mask = zTestMask(&state, 16);

		drawBlock8(&state, mask, shading, vShift);
	}

	_caseU = state.u;
	_caseV = state.v;
	_caseUStep = state.uStep;
	_caseVStep = state.vStep;

	if (tailPixels != 0) {
		uint mask = zTestMask(&state, tailPixels);

		drawPixels8(&state, tailPixels, mask, shading, vShift);
	}
}

void RenderDroid::ZRasterModule::scanLinePerspectiveCase8(RdZRasterEdge *left, RdZRasterEdge *right, int shading, int vShift, bool blockShiftFromTable) {
	ScanState state;

	if (!beginScanLine(left, right, 8, true, &state))
		return;

	int numPixels = state.numPixels;
	int headPixels = 0;

	if (state.x % 16 != 0) {
		headPixels = 16 - state.x % 16;
		if (numPixels < headPixels)
			headPixels = numPixels;
	}

	int restPixels = numPixels - headPixels;
	int numBlocks = 0;
	int tailPixels = 0;

	if (restPixels != 0) {
		numBlocks = restPixels / 16;
		tailPixels = restPixels % 16;
		if (tailPixels == 0) {
			tailPixels = 16;
			numBlocks--;
		}
	}

	if (shading == kGouraud)
		setupLinearIntensity(left, right, &state);

	// The blocks of the first Gouraud case shift by the low byte of a light table pointer
	int blockShift = _mipMapNum;
	if (blockShiftFromTable)
		blockShift = (int)((uintptr)_monoMap & 0xFF) & 31;

	double fraction = (double)(state.xStart - state.xLeft) * 0.0000152587890625;
	float oneOverZ = truncToFloat(truncToFloat(fraction * (double)_dOneOverZdX) + (double)left->oneOverZ);
	float uOverZ = truncToFloat(truncToFloat(fraction * (double)_dUOverZdX) + (double)left->uOverZ);
	float vOverZ = truncToFloat(truncToFloat(fraction * (double)_dVOverZdX) + (double)left->vOverZ);

	state.z = (int)floor((float)((double)oneOverZ * 65536.0));
	state.zStep = _dOneOverZdXFixed;

	double invOneOverZ = 1.0 / (double)oneOverZ;
	float invStored = (float)invOneOverZ;
	float uStart = (float)(invOneOverZ * (double)uOverZ);
	float vStart = (float)((double)invStored * (double)vOverZ);
	float oneOverZNext;
	float uOverZNext;
	float vOverZNext;

	if (headPixels != 0) {
		double head = (double)headPixels;
		float oneOverZEnd = (float)(head * (double)_dOneOverZdX + (double)oneOverZ);
		float uOverZEnd = (float)(head * (double)_dUOverZdX + (double)uOverZ);
		float vOverZEnd = (float)(head * (double)_dVOverZdX + (double)vOverZ);
		uint mask = zTestMask(&state, headPixels);
		double invEnd = 1.0 / (double)oneOverZEnd;
		double uEnd = (double)uOverZEnd * invEnd;
		double vEnd = (double)vOverZEnd * invEnd;

		if (mask != 0) {
			setCaseRun(uStart, vStart, uEnd, vEnd, (double)_rd->_raster.getOneOverNFlex(headPixels) * 65536.0, _mipMapNum);
			state.u = (int)((uint)_dUdXModifier + (uint)_caseU);
			state.v = (int)((uint)_dVdXModifier + (uint)_caseV);
			state.uStep = _caseUStep;
			state.vStep = _caseVStep;
		}

		drawPixels8(&state, headPixels, mask, shading, vShift);

		uStart = (float)uEnd;
		vStart = (float)vEnd;
		oneOverZNext = (float)((double)oneOverZEnd + (double)_oneOverZStep);
		uOverZNext = (float)((double)uOverZEnd + (double)_uOverZStep);
		vOverZNext = (float)((double)vOverZEnd + (double)_vOverZStep);
	} else {
		oneOverZNext = (float)((double)oneOverZ + (double)_oneOverZStep);
		uOverZNext = (float)((double)uOverZ + (double)_uOverZStep);
		vOverZNext = (float)((double)vOverZ + (double)_vOverZStep);
	}

	if (numBlocks != 0) {
		double invNext = 1.0 / (double)oneOverZNext;
		double uOverZReg = (double)uOverZNext;
		double vOverZReg = (double)vOverZNext;

		for (int block = 0; block < numBlocks; block++) {
			uint mask = zTestMask(&state, 16);

			uOverZNext = (float)uOverZReg;
			vOverZNext = (float)vOverZReg;

			double uEnd = uOverZReg * invNext;
			double vEnd = vOverZReg * invNext;
			double oneOverZReg = (double)oneOverZNext + (double)_oneOverZStep;

			oneOverZNext = (float)oneOverZReg;
			invNext = 1.0 / oneOverZReg;

			if (mask != 0) {
				setCaseRun(uStart, vStart, uEnd, vEnd, 4096.0, blockShift);
				state.u = (int)((uint)_dUdXModifier + (uint)_caseU);
				state.v = (int)((uint)_dVdXModifier + (uint)_caseV);
				state.uStep = _caseUStep;
				state.vStep = _caseVStep;
				drawBlock8(&state, mask, shading, vShift);
				_caseU = state.u;
				_caseV = state.v;
			} else {
				drawBlock8(&state, 0, shading, vShift);
			}

			uStart = (float)uEnd;
			vStart = (float)vEnd;
			uOverZReg = (double)uOverZNext + (double)_uOverZStep;
			vOverZReg = (double)vOverZNext + (double)_vOverZStep;
		}

		uOverZNext = (float)uOverZReg;
		vOverZNext = (float)vOverZReg;
	}

	if (tailPixels != 0) {
		int tail = tailPixels - 1;

		if (tail != 0) {
			double tailCount = (double)tail;

			oneOverZNext = (float)(tailCount * (double)_dOneOverZdX - (double)_oneOverZStep + (double)oneOverZNext);
			uOverZNext = (float)(tailCount * (double)_dUOverZdX - (double)_uOverZStep + (double)uOverZNext);
			vOverZNext = (float)(tailCount * (double)_dVOverZdX - (double)_vOverZStep + (double)vOverZNext);

			double invTail = 1.0 / (double)oneOverZNext;
			double uEnd = (double)uOverZNext * invTail;
			double vEnd = (double)vOverZNext * invTail;

			setCaseRun(uStart, vStart, uEnd, vEnd, (double)_rd->_raster.getOneOverNFlex(tail) * 65536.0, _mipMapNum);
			_caseV = (int)((uint)_caseV + (uint)_dVdXModifier);
			_caseU = (int)((uint)_caseU + (uint)_dUdXModifier);
		}

		state.u = _caseU;
		state.v = _caseV;
		state.uStep = _caseUStep;
		state.vStep = _caseVStep;

		uint mask = zTestMask(&state, tailPixels);

		drawPixels8(&state, tailPixels, mask, shading, vShift);
	}
}

void RenderDroid::ZRasterModule::scanLineCase8(RdZRasterEdge *left, RdZRasterEdge *right, int mapping, int shading, int pitch) {
	int vShift = 16 - _texRowShift;

	if (pitch != 0) {
		vShift = 16;
		for (int size = pitch; size > 1; size >>= 1)
			vShift--;
	}

	if (mapping == kPerspective)
		scanLinePerspectiveCase8(left, right, shading, vShift, shading == kGouraud && pitch == 0);
	else
		scanLineAffineCase8(left, right, shading, vShift);
}

bool RenderDroid::ZRasterModule::setupGeometry(RdProcFace *procFace) {
	_vertices = procFace->vertices;
	_numVertices = procFace->numVertices;
	_yMinVert = procFace->yMinIdx;
	_yMaxVert = procFace->yMaxIdx;

	if (!setZScaleShift())
		return false;

	if (procFace->backface) {
		_leftVertexStep = -1;
		_rightVertexStep = 1;
	} else {
		_leftVertexStep = 1;
		_rightVertexStep = -1;
	}

	return true;
}

bool RenderDroid::ZRasterModule::setupSolidFace(RdProcFace *procFace, RdMatCel *cel, float light, int bpp, int shading, int flags) {
	RdColormap *colormap = procFace->colormap;
	bool transparent = (flags & kTransparent) != 0;
	int colorIndex = cel->colorIndex;

	if (shading == kUnlit && !transparent) {
		if (bpp == 8)
			_color = colorIndex;
		else if (bpp == 16)
			_RGBcolor = *(uint16 *)&colormap->rgb16[2 * (byte)colorIndex + 32256];
	} else if (shading == kUnlit) {
		if (bpp == 8) {
			_color = (byte)colorIndex;
		} else if (bpp == 16) {
			_rgbMap = colormap->rgb16 + 32256;
			_RGBcolor = *(uint16 *)&colormap->rgb16[2 * colorIndex + 32256];
			_rgbToPalette = colormap->rgb16Alpha;
			_color = _rgbToPalette[_RGBcolor];
		}
	} else if (shading == kFlat) {
		int lightRow = (int)floor(light);

		if (bpp == 8) {
			_color = colormap->lightLevel[256 * lightRow + colorIndex];
		} else if (bpp == 16) {
			_RGBcolor = *(uint16 *)&colormap->rgb16[512 * lightRow + 2 * colorIndex];
			if (transparent) {
				_rgbMap = colormap->rgb16 + 32256;
				_rgbToPalette = colormap->rgb16Alpha;
				_color = _rgbToPalette[_RGBcolor];
			}
		}
	} else {
		_color = colorIndex;
		if (bpp == 8) {
			_monoMap = colormap->lightLevel;
		} else if (bpp == 16) {
			_rgbMap = colormap->rgb16;
			if (transparent) {
				_RGBcolor = *(uint16 *)&colormap->rgb16[2 * colorIndex + 32256];
				_rgbToPalette = colormap->rgb16Alpha;
				_color = _rgbToPalette[_RGBcolor];
			}
		}
		_intensities = procFace->intensities;
	}

	if (transparent)
		_alphaMap = _rd->_colormap.getIdentity()->transparency;

	if (!setupGeometry(procFace))
		return false;

	if (_numVertices > 32) {
		warning("RenderDroid::ZRasterModule::setupSolidFace(): too many vertices");
		return false;
	}

	for (uint i = 0; i < _numVertices; i++)
		_oneOverZ[i] = (float)(1.0 / ((double)_vertices[i][2] + 1.0));

	return true;
}

bool RenderDroid::ZRasterModule::setupTexturedFace(RdProcFace *procFace, RdMatCel *cel, float light, int bpp, int mapping, int shading, int flags) {
	RdColormap *colormap = procFace->colormap;
	bool transparent = (flags & kTransparent) != 0;

	if (shading == kUnlit) {
		if (bpp == 16) {
			_rgbMap = colormap->rgb16 + 32256;
			if (transparent)
				_rgbToPalette = colormap->rgb16Alpha;
		}
	} else if (shading == kFlat) {
		int lightRow = (int)floor(light);

		if (bpp == 8) {
			_monoMap = &colormap->lightLevel[256 * lightRow];
		} else if (bpp == 16) {
			_rgbMap = &colormap->rgb16[512 * lightRow];
			if (transparent)
				_rgbToPalette = colormap->rgb16Alpha;
		}
	} else {
		if (bpp == 8) {
			_monoMap = colormap->lightLevel;
		} else if (bpp == 16) {
			_rgbMap = colormap->rgb16;
			if (transparent)
				_rgbToPalette = colormap->rgb16Alpha;
		}
		_intensities = procFace->intensities;
	}

	if (transparent)
		_alphaMap = _rd->_colormap.getIdentity()->transparency;

	RdTexture *texture = cel->texture;

	_maxMipMap = texture->numMips - 1;
	_texVertices = procFace->texVertices;

	if (!setupGeometry(procFace))
		return false;

	if (mapping == kAffine) {
		calcAffineGradients(procFace->zMin);
	} else {
		calcPerspGradients(procFace->zMin);
	}

	_pixels = texture->mips[_mipMapNum]->pixels;
	_texRowShift = texture->widthLog2 - _mipMapNum;
	_texPitch = texture->mips[_mipMapNum]->tex.pitchInBytes;
	_uWrap = (int)((uint)texture->widthMask >> _mipMapNum << 16);
	_vWrap = (int)((uint)texture->heightMask >> _mipMapNum << _texRowShift);

	if (mapping == kPerspective) {
		_oneOverZStep = (float)(_dOneOverZdX * 16.0);
		_uOverZStep = (float)(_dUOverZdX * 16.0);
		_vOverZStep = (float)(_dVOverZdX * 16.0);
	}

	return true;
}

int RenderDroid::ZRasterModule::selectCasePitch(int mapping, int shading) {
	int firstPitch = (mapping == kAffine && shading != kUnlit) ? 4 : 8;

	for (int pitch = firstPitch; pitch <= 256; pitch *= 2) {
		if (_texPitch == pitch)
			return pitch;
	}

	return 0;
}

int RenderDroid::ZRasterModule::selectCasePitch8() {
	for (int pitch = 8; pitch <= 256; pitch *= 2) {
		if (_texPitch == pitch)
			return pitch;
	}

	return 0;
}

int RenderDroid::ZRasterModule::drawNGon(RdProcFace *procFace, RdMatCel *cel, float light, int mapping, int shading, int flags) {
	int bpp = _rd->_camera.getCurrent()->canvas->vBuffer->tex.format.bpp;

	if (mapping == kSolid) {
		if (!setupSolidFace(procFace, cel, light, bpp, shading, flags))
			return 0;
	} else if (!setupTexturedFace(procFace, cel, light, bpp, mapping, shading, flags)) {
		return 0;
	}

	RdZRasterEdge left = RdZRasterEdge();
	RdZRasterEdge right = RdZRasterEdge();

	left.vertIdx = _yMinVert;
	int result = setupLeftEdge(&left, mapping, shading);
	if (result)
		return result;

	right.vertIdx = _yMinVert;
	result = setupRightEdge(&right, mapping, shading);
	if (result)
		return result;

	bool hasCases = (mapping != kSolid && flags == 0 && !_rd->_remastered);
	bool useCase = (hasCases && !_zScaleShift && bpp == 16);
	bool useCase8 = (hasCases && !_zScaleShift && bpp == 8);
	bool drawLines = (bpp == 8 || bpp == 16);
	int casePitch = 0;

	if (useCase)
		casePitch = selectCasePitch(mapping, shading);
	else if (useCase8)
		casePitch = selectCasePitch8();

	ScanParams params = makeScanParams(bpp, shading, flags, mapping == kPerspective);

	if (hasCases && bpp != 8 && bpp != 16)
		warning("RenderDroid::ZRasterModule::drawNGon(): unsupported colour depth");

	while (true) {
		if (useCase)
			scanLineCase(&left, &right, mapping, shading, casePitch);
		else if (useCase8)
			scanLineCase8(&left, &right, mapping, shading, casePitch);
		else if (drawLines && mapping == kSolid)
			scanLineSolid(&left, &right, bpp, shading, flags);
		else if (drawLines && mapping == kAffine)
			scanLineAffine(&left, &right, params);
		else if (drawLines)
			scanLinePerspective(&left, &right, params);

		if (--left.numLines) {
			stepEdge(&left, mapping, shading, true);
		} else {
			result = setupLeftEdge(&left, mapping, shading);
			if (result)
				return result;
		}

		if (--right.numLines) {
			stepEdge(&right, mapping, shading, false);
		} else {
			result = setupRightEdge(&right, mapping, shading);
			if (result)
				return result;
		}
	}
}

int RenderDroid::ZRasterModule::drawNGonLV(RdProcFace *procFace, RdMatCel *cel) {
	StdVBuffer *vBuffer = _rd->_camera.getCurrent()->canvas->vBuffer;
	int bpp = vBuffer->tex.format.bpp;

	_vertices = procFace->vertices;
	_numVertices = procFace->numVertices;

	if (bpp == 8)
		_color = (byte)cel->colorIndex;
	else if (bpp == 16)
		_RGBcolor = *(uint16 *)&procFace->colormap->rgb16[2 * (byte)cel->colorIndex + 32256];
	else
		return 0;

	for (uint i = 0; i < _numVertices; i++) {
		int x = (int)floor(_vertices[i][0]);
		int y = (int)floor(_vertices[i][1]);

		if (_rd->_remastered) {
			x = winTruncate(_vertices[i][0]);
			y = winTruncate(_vertices[i][1]);
		}

		if (x < 0 || y < 0 || x >= vBuffer->tex.width || y >= vBuffer->tex.height) {
			warning("RenderDroid::ZRasterModule::drawNGonLV(): pixel outside the buffer");
			continue;
		}

		if (bpp == 8)
			vBuffer->pixels[y * vBuffer->tex.pitchInBytes + x] = _color;
		else
			*(uint16 *)&vBuffer->pixels[2 * x + 2 * y * (vBuffer->tex.pitchInBytes >> 1)] = _RGBcolor;
	}

	return 0;
}

int RenderDroid::ZRasterModule::drawNGonLW(RdProcFace *procFace, RdMatCel *cel) {
	RdCanvas *canvas = _rd->_camera.getCurrent()->canvas;
	int bpp = canvas->vBuffer->tex.format.bpp;

	_vertices = procFace->vertices;
	_numVertices = procFace->numVertices;

	if (_numVertices == 0)
		return 0;

	uint16 color;
	if (bpp == 8)
		color = _color = (byte)cel->colorIndex;
	else if (bpp == 16)
		color = _RGBcolor = *(uint16 *)&procFace->colormap->rgb16[2 * (byte)cel->colorIndex + 32256];
	else
		return 0;

	for (uint i = 0; i < _numVertices; i++) {
		uint next = (i + 1 < _numVertices) ? i + 1 : 0;

		if (_rd->_remastered)
			_rd->_primit2.drawLine(canvas, winTruncate(_vertices[i][0]), winTruncate(_vertices[i][1]), winTruncate(_vertices[next][0]), winTruncate(_vertices[next][1]), color, 0xFFFFFFFF);
		else
			_rd->_primit2.drawLine(canvas, (int)floor(_vertices[i][0]), (int)floor(_vertices[i][1]), (int)floor(_vertices[next][0]), (int)floor(_vertices[next][1]), color, 0xFFFFFFFF);
	}

	return 0;
}

int RenderDroid::ZRasterModule::drawNGonLS(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kSolid, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonLS_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 8, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonLS_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 16, kUnlit, 0);
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonLS(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kSolid, kUnlit);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonLS(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kSolid, kUnlit);
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonFS(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kSolid, kFlat);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonFS(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kSolid, kFlat);
}

int RenderDroid::ZRasterModule::drawNGonTLS(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kSolid, kUnlit, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTLS_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 8, kUnlit, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTLS_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 16, kUnlit, kTransparent);
}

int RenderDroid::ZRasterModule::drawNGonFS(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kSolid, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonFS_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 8, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonFS_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 16, kFlat, 0);
}

int RenderDroid::ZRasterModule::drawNGonTFS(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kSolid, kFlat, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTFS_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 8, kFlat, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTFS_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 16, kFlat, kTransparent);
}

int RenderDroid::ZRasterModule::drawNGonGS(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kSolid, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonGS_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 8, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonGS_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 16, kGouraud, 0);
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonGS(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kSolid, kGouraud);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonGS(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kSolid, kGouraud);
}

int RenderDroid::ZRasterModule::drawNGonTGS(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kSolid, kGouraud, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTGS_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 8, kGouraud, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTGS_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineSolid(left, right, 16, kGouraud, kTransparent);
}

int RenderDroid::ZRasterModule::drawNGonLAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonLAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kUnlit, 0, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonLAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kUnlit, 0, false));
}

int RenderDroid::ZRasterModule::drawNGonTLAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kUnlit, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTLAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kUnlit, kTransparent, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonTLAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kUnlit, kTransparent, false));
}

int RenderDroid::ZRasterModule::drawNGonMLAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kUnlit, kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMLAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kUnlit, kMasked, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMLAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kUnlit, kMasked, false));
}

int RenderDroid::ZRasterModule::drawNGonMTLAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kUnlit, kTransparent | kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTLAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kUnlit, kTransparent | kMasked, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTLAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kUnlit, kTransparent | kMasked, false));
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonLAT(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kAffine, kUnlit);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonLAT(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kAffine, kUnlit);
}

int RenderDroid::ZRasterModule::drawNGonFAT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kAffine, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonFAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kFlat, 0, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonFAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kFlat, 0, false));
}

int RenderDroid::ZRasterModule::drawNGonTFAT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kAffine, kFlat, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTFAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kFlat, kTransparent, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonTFAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kFlat, kTransparent, false));
}

int RenderDroid::ZRasterModule::drawNGonMFAT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kAffine, kFlat, kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMFAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kFlat, kMasked, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMFAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kFlat, kMasked, false));
}

int RenderDroid::ZRasterModule::drawNGonMTFAT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kAffine, kFlat, kTransparent | kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTFAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kFlat, kTransparent | kMasked, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTFAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kFlat, kTransparent | kMasked, false));
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonFAT(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kAffine, kFlat);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonFAT(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kAffine, kFlat);
}

int RenderDroid::ZRasterModule::drawNGonGAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonGAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kGouraud, 0, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonGAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kGouraud, 0, false));
}

int RenderDroid::ZRasterModule::drawNGonTGAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kGouraud, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTGAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kGouraud, kTransparent, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonTGAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kGouraud, kTransparent, false));
}

int RenderDroid::ZRasterModule::drawNGonMGAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kGouraud, kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMGAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kGouraud, kMasked, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMGAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kGouraud, kMasked, false));
}

int RenderDroid::ZRasterModule::drawNGonMTGAT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kAffine, kGouraud, kTransparent | kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTGAT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(8, kGouraud, kTransparent | kMasked, false));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTGAT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineAffine(left, right, makeScanParams(16, kGouraud, kTransparent | kMasked, false));
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonGAT(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kAffine, kGouraud);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonGAT(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kAffine, kGouraud);
}

int RenderDroid::ZRasterModule::drawNGonLIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonLIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kUnlit, 0, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonLIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kUnlit, 0, true));
}

int RenderDroid::ZRasterModule::drawNGonTLIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kUnlit, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTLIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kUnlit, kTransparent, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonTLIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kUnlit, kTransparent, true));
}

int RenderDroid::ZRasterModule::drawNGonMLIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kUnlit, kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMLIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kUnlit, kMasked, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMLIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kUnlit, kMasked, true));
}

int RenderDroid::ZRasterModule::drawNGonMTLIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kUnlit, kTransparent | kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTLIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kUnlit, kTransparent | kMasked, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTLIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kUnlit, kTransparent | kMasked, true));
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonLIT(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kPerspective, kUnlit);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonLIT(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kPerspective, kUnlit);
}

int RenderDroid::ZRasterModule::drawNGonFIT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kPerspective, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonFIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kFlat, 0, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonFIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kFlat, 0, true));
}

int RenderDroid::ZRasterModule::drawNGonTFIT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kPerspective, kFlat, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTFIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kFlat, kTransparent, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonTFIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kFlat, kTransparent, true));
}

int RenderDroid::ZRasterModule::drawNGonMFIT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kPerspective, kFlat, kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMFIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kFlat, kMasked, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMFIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kFlat, kMasked, true));
}

int RenderDroid::ZRasterModule::drawNGonMTFIT(RdProcFace *procFace, RdMatCel *cel, float light) {
	return drawNGon(procFace, cel, light, kPerspective, kFlat, kTransparent | kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTFIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kFlat, kTransparent | kMasked, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTFIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kFlat, kTransparent | kMasked, true));
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonFIT(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kPerspective, kFlat);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonFIT(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kPerspective, kFlat);
}

int RenderDroid::ZRasterModule::drawNGonGIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawScanLineNGonGIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kGouraud, 0, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonGIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kGouraud, 0, true));
}

int RenderDroid::ZRasterModule::drawNGonTGIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kGouraud, kTransparent);
}

void RenderDroid::ZRasterModule::drawScanLineNGonTGIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kGouraud, kTransparent, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonTGIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kGouraud, kTransparent, true));
}

int RenderDroid::ZRasterModule::drawNGonMGIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kGouraud, kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMGIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kGouraud, kMasked, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMGIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kGouraud, kMasked, true));
}

int RenderDroid::ZRasterModule::drawNGonMTGIT(RdProcFace *procFace, RdMatCel *cel) {
	return drawNGon(procFace, cel, 0.0f, kPerspective, kGouraud, kTransparent | kMasked);
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTGIT_8(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(8, kGouraud, kTransparent | kMasked, true));
}

void RenderDroid::ZRasterModule::drawScanLineNGonMTGIT_16(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLinePerspective(left, right, makeScanParams(16, kGouraud, kTransparent | kMasked, true));
}

int RenderDroid::ZRasterModule::setupLeftEdgeNGonGIT(RdZRasterEdge *edge) {
	return setupLeftEdge(edge, kPerspective, kGouraud);
}

int RenderDroid::ZRasterModule::setupRightEdgeNGonGIT(RdZRasterEdge *edge) {
	return setupRightEdge(edge, kPerspective, kGouraud);
}

void RenderDroid::ZRasterModule::drawNGonLAT_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawNGonLAT_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kUnlit, 8);
}

void RenderDroid::ZRasterModule::drawNGonLAT_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kUnlit, 16);
}

void RenderDroid::ZRasterModule::drawNGonLAT_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kUnlit, 32);
}

void RenderDroid::ZRasterModule::drawNGonLAT_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kUnlit, 64);
}

void RenderDroid::ZRasterModule::drawNGonLAT_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kUnlit, 128);
}

void RenderDroid::ZRasterModule::drawNGonLAT_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kUnlit, 256);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 4);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 8);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 16);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 32);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 64);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 128);
}

void RenderDroid::ZRasterModule::drawNGonFAT_Case7(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kFlat, 256);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 4);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 8);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 16);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 32);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 64);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 128);
}

void RenderDroid::ZRasterModule::drawNGonGAT_Case7(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kAffine, kGouraud, 256);
}

void RenderDroid::ZRasterModule::drawNGonLIT_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawNGonLIT_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kUnlit, 8);
}

void RenderDroid::ZRasterModule::drawNGonLIT_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kUnlit, 16);
}

void RenderDroid::ZRasterModule::drawNGonLIT_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kUnlit, 32);
}

void RenderDroid::ZRasterModule::drawNGonLIT_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kUnlit, 64);
}

void RenderDroid::ZRasterModule::drawNGonLIT_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kUnlit, 128);
}

void RenderDroid::ZRasterModule::drawNGonLIT_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kUnlit, 256);
}

void RenderDroid::ZRasterModule::drawNGonFIT_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawNGonFIT_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kFlat, 8);
}

void RenderDroid::ZRasterModule::drawNGonFIT_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kFlat, 16);
}

void RenderDroid::ZRasterModule::drawNGonFIT_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kFlat, 32);
}

void RenderDroid::ZRasterModule::drawNGonFIT_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kFlat, 64);
}

void RenderDroid::ZRasterModule::drawNGonFIT_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kFlat, 128);
}

void RenderDroid::ZRasterModule::drawNGonFIT_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kFlat, 256);
}

void RenderDroid::ZRasterModule::drawNGonGIT_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawNGonGIT_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kGouraud, 8);
}

void RenderDroid::ZRasterModule::drawNGonGIT_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kGouraud, 16);
}

void RenderDroid::ZRasterModule::drawNGonGIT_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kGouraud, 32);
}

void RenderDroid::ZRasterModule::drawNGonGIT_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kGouraud, 64);
}

void RenderDroid::ZRasterModule::drawNGonGIT_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kGouraud, 128);
}

void RenderDroid::ZRasterModule::drawNGonGIT_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase(left, right, kPerspective, kGouraud, 256);
}

void RenderDroid::ZRasterModule::drawNGonLAT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawNGonLAT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kUnlit, 8);
}

void RenderDroid::ZRasterModule::drawNGonLAT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kUnlit, 16);
}

void RenderDroid::ZRasterModule::drawNGonLAT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kUnlit, 32);
}

void RenderDroid::ZRasterModule::drawNGonLAT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kUnlit, 64);
}

void RenderDroid::ZRasterModule::drawNGonLAT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kUnlit, 128);
}

void RenderDroid::ZRasterModule::drawNGonLAT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kUnlit, 256);
}

void RenderDroid::ZRasterModule::drawNGonFAT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawNGonFAT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kFlat, 8);
}

void RenderDroid::ZRasterModule::drawNGonFAT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kFlat, 16);
}

void RenderDroid::ZRasterModule::drawNGonFAT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kFlat, 32);
}

void RenderDroid::ZRasterModule::drawNGonFAT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kFlat, 64);
}

void RenderDroid::ZRasterModule::drawNGonFAT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kFlat, 128);
}

void RenderDroid::ZRasterModule::drawNGonFAT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kFlat, 256);
}

void RenderDroid::ZRasterModule::drawNGonGAT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawNGonGAT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kGouraud, 8);
}

void RenderDroid::ZRasterModule::drawNGonGAT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kGouraud, 16);
}

void RenderDroid::ZRasterModule::drawNGonGAT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kGouraud, 32);
}

void RenderDroid::ZRasterModule::drawNGonGAT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kGouraud, 64);
}

void RenderDroid::ZRasterModule::drawNGonGAT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kGouraud, 128);
}

void RenderDroid::ZRasterModule::drawNGonGAT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kAffine, kGouraud, 256);
}

void RenderDroid::ZRasterModule::drawNGonLIT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kUnlit, 0);
}

void RenderDroid::ZRasterModule::drawNGonLIT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kUnlit, 8);
}

void RenderDroid::ZRasterModule::drawNGonLIT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kUnlit, 16);
}

void RenderDroid::ZRasterModule::drawNGonLIT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kUnlit, 32);
}

void RenderDroid::ZRasterModule::drawNGonLIT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kUnlit, 64);
}

void RenderDroid::ZRasterModule::drawNGonLIT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kUnlit, 128);
}

void RenderDroid::ZRasterModule::drawNGonLIT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kUnlit, 256);
}

void RenderDroid::ZRasterModule::drawNGonFIT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kFlat, 0);
}

void RenderDroid::ZRasterModule::drawNGonFIT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kFlat, 8);
}

void RenderDroid::ZRasterModule::drawNGonFIT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kFlat, 16);
}

void RenderDroid::ZRasterModule::drawNGonFIT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kFlat, 32);
}

void RenderDroid::ZRasterModule::drawNGonFIT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kFlat, 64);
}

void RenderDroid::ZRasterModule::drawNGonFIT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kFlat, 128);
}

void RenderDroid::ZRasterModule::drawNGonFIT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kFlat, 256);
}

void RenderDroid::ZRasterModule::drawNGonGIT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kGouraud, 0);
}

void RenderDroid::ZRasterModule::drawNGonGIT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kGouraud, 8);
}

void RenderDroid::ZRasterModule::drawNGonGIT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kGouraud, 16);
}

void RenderDroid::ZRasterModule::drawNGonGIT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kGouraud, 32);
}

void RenderDroid::ZRasterModule::drawNGonGIT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kGouraud, 64);
}

void RenderDroid::ZRasterModule::drawNGonGIT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kGouraud, 128);
}

void RenderDroid::ZRasterModule::drawNGonGIT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right) {
	scanLineCase8(left, right, kPerspective, kGouraud, 256);
}

} // End of namespace Grim
