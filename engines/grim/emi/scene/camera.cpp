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

#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"

namespace Grim {

static const float kPi = 3.14159274f;
static const float kHalfTurnDegrees = 180.0f;
static const float kDegreesToRadians = 3.14159274f * 0.00555555569f;
static const float kRadiansToDegrees = kHalfTurnDegrees / kPi;
static const int kDefaultViewportWidth = 640;
static const int kDefaultViewportHeight = 480;
static const float kDefaultFov = 90.0f;

Camera::Camera(CDrawContext *drawContext) : _drawContext(drawContext) {
	_nearClip = 0.0f;
	_farClip = 0.0f;

	_view._rot.setIdentity();
	_view._pos._x = 0.0f;
	_view._pos._y = 0.0f;
	_view._pos._z = 0.0f;

	IRect viewport;

	viewport.x = 0;
	viewport.y = 0;
	viewport.w = kDefaultViewportWidth;
	viewport.h = kDefaultViewportHeight;

	setViewport(&viewport, kDefaultFov);
	updateViewMatrix();

	_reserved368 = nullptr;
	_reserved372 = false;
}

Camera::~Camera() {
}

CDrawContext *Camera::getDrawContext() const {
	return _drawContext;
}

void Camera::applyProjection() {
	if (_drawContext)
		_drawContext->setPerspective(_fovX, _fovY, _nearClip, _farClip);
}

void Camera::updateViewMatrix() {
	if ((_nodeFlags & kWorldValid) == 0)
		updateWorldTransform();

	const float *m = _world._rot._m;
	float negX = -_world._pos._x;
	float negY = -_world._pos._y;
	float negZ = -_world._pos._z;
	float posX = (float)((double)negZ * m[2] + (double)negY * m[1] + (double)negX * m[0]);
	float posY = (float)((double)negX * m[3] + (double)negZ * m[5] + (double)negY * m[4]);
	float posZ = (float)((double)negZ * m[8] + (double)negY * m[7] + (double)negX * m[6]);

	_view._rot._m[0] = m[0];
	_view._rot._m[1] = m[3];
	_view._rot._m[2] = m[6];
	_view._rot._m[3] = m[1];
	_view._rot._m[4] = m[4];
	_view._rot._m[5] = m[7];
	_view._rot._m[6] = m[2];
	_view._rot._m[7] = m[5];
	_view._rot._m[8] = m[8];
	_view._pos._x = posX;
	_view._pos._y = posY;
	_view._pos._z = posZ;
}

void Camera::setFov(float newFovX, float newFovY) {
	_fovX = newFovX;
	_fovY = newFovY;

	double tanHalfX = tan((double)newFovX * 0.5 * kDegreesToRadians);
	double tanHalfY = tan((double)newFovY * 0.5 * kDegreesToRadians);

	if (tanHalfX == 0.0) {
		// The original could divide by zero here, let's avoid that...
		warning("Camera::setFov(): invalid horizontal field of view");
		_invTanHalfFovX = 0.0f;
	} else {
		_invTanHalfFovX = (float)(1.0 / tanHalfX);
	}

	if (tanHalfY == 0.0) {
		// The original could divide by zero here, let's avoid that...
		warning("Camera::setFov(): invalid vertical field of view");
		_invTanHalfFovY = 0.0f;
	} else {
		_invTanHalfFovY = (float)(1.0 / tanHalfY);
	}

	_focalX = (float)((double)_halfWidth * _invTanHalfFovX);
	_focalY = (float)((double)_halfHeight * _invTanHalfFovY);
}

void Camera::setViewport(const IRect *rect, float newFovX) {
	_halfWidth = (float)((double)rect->w * 0.5);
	_halfHeight = (float)((double)rect->h * 0.5);

	if (newFovX != 0.0f) {
		if (rect->w == 0) {
			// The original could divide by zero here, let's avoid that...
			warning("Camera::setViewport(): the viewport has no width");
			return;
		}

		double angle = atan2(tan((double)newFovX * 0.5 * kDegreesToRadians) * ((double)rect->h / (double)rect->w), 1.0);
		float newFovY = (float)(angle * kRadiansToDegrees * 2.0);

		setFov(newFovX, newFovY);
	}
}

IRect Camera::getViewportRect() {
	int rect[4] = { 0, 0, 0, 0 };
	IRect result;

	if (_drawContext)
		_drawContext->getViewportRect(rect);
	else
		warning("Camera::getViewportRect(): no draw context");

	result.x = rect[0];
	result.y = rect[1];
	result.w = rect[2];
	result.h = rect[3];

	return result;
}

void Camera::drawLineLoop(DrawVertex *vertices, int count) {
	if (_drawContext)
		_drawContext->drawPrim(4, vertices, count, false);
}

} // End of namespace Grim
