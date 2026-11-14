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

#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/actor/sputactor.h"
#include "engines/grim/grim.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/remastered/pathfinder.h"
#include "engines/grim/remastered/sputmouse.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/stdcontrol.h"
#include "engines/grim/walkplane.h"

namespace Grim {

enum {
	kPlaneTypeWalk = 0x1000,
	kDialogWidth = 420,
	kOverlayCoordsGlobal = 2
};

SputMouse::SputMouse(GrimEngine *grim, SysStruct *hs, StdControl *stdControl, SputRoom *sputRoom, SputActorModule *sputActor,
		WalkSystem *walkSystem, RenderDroid *rd) :
		_grim(grim), _stdControl(stdControl), _sputActor(sputActor), _walkSystem(walkSystem), _rd(rd),
		_hotspots(hs, sputRoom, sputActor, rd), _started(false), _visibleInMovie(0), _mainHandle(-1), _animHandle(-1),
		_wave(0), _waveEnd(-1), _waveNext(0) {
}

void SputMouse::loadCursors(int count) {
	OverlayManager *overlays = _hotspots.getOverlays();

	for (int i = 0; i < kNumCursors; i++) {
		_cursorNames[i].clear();

		if (i < count && overlays && overlays->loadCached(Common::String::format("cursor%02d.png", i), OverlayManager::kCachePersistent))
			_cursorNames[i] = Common::String::format("cursor%02d.png", i);
	}
}

int SputMouse::startup() {
	OverlayManager *overlays = _hotspots.getOverlays();

	if (_started)
		return 1;

	if (!overlays) {
		warning("SputMouse::startup(): there are no overlays");
		return 1;
	}

	_started = true;
	_visibleInMovie = 0;
	loadCursors(kNumCursorImages);

	OverlayProps props;

	props.cache = OverlayManager::kCachePersistent;
	props.layer = 0;
	props.coords = kOverlayCoordsGlobal;
	_mainHandle = overlays->create(_noImage, 0, 0, props);
	_wave = 0;
	_waveEnd = -1;
	_animHandle = overlays->create(_noImage, 0, 0, props);

	return 0;
}

void SputMouse::shutdown() {
	for (int i = 0; i < kNumCursors; i++)
		_cursorNames[i].clear();

	_hotspots.clear();
	_walkSystem->getPathFinder()->freePolygons();
	_started = false;
}

void SputMouse::getCanvasPosition(int *x, int *y) const {
	_stdControl->getMouseCanvasPosition(_grim->getLibMode() == GrimEngine::kLibModeMenu, x, y);
}

float SputMouse::getAxisNormalized(int axis, bool invert) {
	float value = _stdControl->readAxis(axis);

	if (invert)
		value = -value;

	if (value >= -1.0f)
		return MIN(value, 1.0f);

	return -1.0f;
}

const Common::String &SputMouse::getCursorName(int index) const {
	if (index < 0 || index >= kNumCursors) {
		if (index >= kNumCursors)
			warning("SputMouse::getCursorName(): cursor %d does not exist", index);

		return _noImage;
	}

	return _cursorNames[index];
}

void SputMouse::setCursor(int index) {
	OverlayManager *overlays = _hotspots.getOverlays();

	if (overlays)
		overlays->setImage(_mainHandle, getCursorName(index));
}

void SputMouse::showCursor(bool show) {
	OverlayManager *overlays = _hotspots.getOverlays();

	_waveEnd = -1;

	if (!overlays)
		return;

	overlays->setLayer(_mainHandle, show ? kCursorLayer : 0);
	overlays->setLayer(_animHandle, 0);
}

void SputMouse::cursorAnimation(int first, int last) {
	OverlayManager *overlays = _hotspots.getOverlays();
	int layer;

	if (!overlays || !overlays->getLayer(_animHandle, &layer))
		return;

	overlays->setLayer(_animHandle, kCursorLayer);
	_wave = first;
	_waveEnd = last;
	_waveNext = g_system->getMillis() + kWaveStepMs;
	overlays->setImage(_animHandle, getCursorName(first));
}

void SputMouse::preloadCursors() {
	OverlayManager *overlays = _hotspots.getOverlays();

	if (!overlays)
		return;

	for (int i = 0; i < kNumCursors; i++) {
		if (!_cursorNames[i].empty())
			overlays->preloadImage(_cursorNames[i]);
	}
}

void SputMouse::placeCursor(int handle, int x, int y) {
	OverlayManager *overlays = _hotspots.getOverlays();
	int layer;

	if (!overlays->getLayer(handle, &layer) || layer == 0 || !overlays->hasImage(handle))
		return;

	int width;
	int height;

	overlays->getDimensions(handle, &width, &height);
	overlays->move(handle, x - width / 2, y - height / 2);
}

// The end is tested before the next image is shown, so the last image is shown too
void SputMouse::update() {
	OverlayManager *overlays = _hotspots.getOverlays();

	if (!overlays || !_rd->_camera.getCurrent())
		return;

	int x;
	int y;

	_stdControl->getMouseCanvasPosition(false, &x, &y);
	placeCursor(_mainHandle, x, y);
	placeCursor(_animHandle, x, y);

	if (_waveEnd >= 0) {
		uint32 now = g_system->getMillis();

		if (now > _waveNext) {
			_waveNext += kWaveStepMs;

			if (_wave >= _waveEnd) {
				overlays->setLayer(_animHandle, 0);
				_waveEnd = -1;
			} else {
				_wave++;
				overlays->setImage(_animHandle, getCursorName(_wave));
			}
		}
	}
}

bool SputMouse::getCameraRay(SputRender *sputRender, RdVector3 start, RdVector3 end) const {
	RdCamera *camera = _rd->_camera.getCurrent();
	RoomCamera *roomCamera = sputRender->getCamera();

	if (!camera)
		return false;

	if (!camera->canvas || !roomCamera || camera->focalLen == 0.0f) {
		warning("SputMouse::getCameraRay(): the camera is not ready");
		return false;
	}

	int x;
	int y;

	getCanvasPosition(&x, &y);
	HotspotManager::convertCoordsNative(&x, &y, HotspotManager::kCoordsLegacy, false);

	double focalLen = camera->focalLen;
	RdVector3 point;
	RdVector3 direction;
	RdMatrix34 cameraMatrix;

	point[0] = (float)(((double)x - camera->canvas->xCenter) / focalLen);
	point[1] = 1.0f;
	point[2] = (float)(-(((double)y - camera->canvas->yCenter) / focalLen));
	_rd->_camera.getMatrix(cameraMatrix);
	_rd->transformPoint34Acc(point, cameraMatrix);

	direction[0] = point[0] - roomCamera->position[0];
	direction[1] = point[1] - roomCamera->position[1];
	direction[2] = point[2] - roomCamera->position[2];
	_rd->normalize3Acc(direction);

	for (int i = 0; i < 3; i++) {
		direction[i] = direction[i] * 100.0f;
		end[i] = roomCamera->position[i] + direction[i];
		start[i] = roomCamera->position[i];
	}

	return true;
}

// The squares are rounded to float one by one, their sum is not
float SputMouse::distanceExactDifferences(const float *a, const float *b) const {
	double differenceX = (double)a[0] - (double)b[0];
	double differenceY = (double)a[1] - (double)b[1];
	double differenceZ = (double)a[2] - (double)b[2];
	float squareX = (float)(differenceX * differenceX);
	float squareY = (float)(differenceY * differenceY);
	float squareZ = (float)(differenceZ * differenceZ);

	return (float)sqrt(((double)squareX + squareY) + squareZ);
}

// Here the differences are rounded to float before they are squared
float SputMouse::distanceFloatDifferences(const float *a, const float *b) const {
	float differenceX = a[0] - b[0];
	float differenceY = a[1] - b[1];
	float differenceZ = a[2] - b[2];
	float squareX = differenceX * differenceX;
	float squareY = differenceY * differenceY;
	float squareZ = differenceZ * differenceZ;

	return (float)sqrt(((double)squareX + squareY) + squareZ);
}

void SputMouse::raycast(WalkPlane *plane, const float *start, const float *end, float *hit) const {
	if (plane->_numVerts == 0)
		return;

	RdVector3 delta;
	RdVector3 toVertex;
	const WpVertex *vertex = plane->_verts[0];

	delta[0] = end[0] - start[0];
	delta[1] = end[1] - start[1];
	delta[2] = end[2] - start[2];
	toVertex[0] = vertex->x - start[0];
	toVertex[1] = vertex->y - start[1];
	toVertex[2] = vertex->z - start[2];

	float denominator = _rd->dot3(plane->_normal, delta);
	float magnitude = denominator < 0.0f ? -denominator : denominator;

	if ((double)magnitude < 0.000001)
		return;

	float productX = plane->_normal[0] * toVertex[0];
	float productY = plane->_normal[1] * toVertex[1];
	float productZ = plane->_normal[2] * toVertex[2];
	float t = (float)((((double)productX + productY) + productZ) / (double)denominator);
	float stepX = delta[0] * t;
	float stepY = delta[1] * t;
	float stepZ = delta[2] * t;

	hit[0] = stepX + start[0];
	hit[1] = stepY + start[1];
	hit[2] = stepZ + start[2];
}

bool SputMouse::getFloorWalkPos(bool onGround, SputRender *sputRender, RdVector3 out) {
	RdVector3 start;
	RdVector3 end;

	if (!getCameraRay(sputRender, start, end))
		return false;

	if (onGround) {
		// The original could divide by zero here, let's avoid that...
		if (end[2] == start[2]) {
			warning("SputMouse::getFloorWalkPos(): the ray does not meet the ground");
			return false;
		}

		float scale = (float)(-(double)start[2] / ((double)end[2] - (double)start[2]));
		float deltaX = end[0] - start[0];
		float deltaY = end[1] - start[1];
		float deltaZ = end[2] - start[2];
		float stepX = deltaX * scale;
		float stepY = deltaY * scale;
		float stepZ = scale * deltaZ;
		RdVector3 point;

		point[0] = start[0] + stepX;
		point[1] = start[1] + stepY;
		point[2] = start[2] + stepZ;

		WalkPlane *plane = _walkSystem->planeClosest(point, kPlaneTypeWalk);

		if (!plane) {
			warning("SputMouse::getFloorWalkPos(): there is no walk plane");
			return false;
		}

		plane->closestPoint(point);
		out[0] = point[0];
		out[1] = point[1];
		out[2] = point[2];

		return true;
	}

	int x;
	int y;

	getCanvasPosition(&x, &y);
	HotspotManager::convertCoordsNative(&x, &y, HotspotManager::kCoordsLegacy, false);

	RdVector3 projected;

	if (sputRender->reverseProject(x, y, projected)) {
		RdVector3 hit;
		WalkPlane *found = nullptr;

		for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
			if ((plane->_type & kPlaneTypeWalk) == 0 || !plane->_active)
				continue;

			hit[0] = 1000.0f;
			hit[1] = 1000.0f;
			hit[2] = 1000.0f;
			raycast(plane, start, end, hit);

			if (plane->within(hit, 1)) {
				found = plane;
				break;
			}
		}

		if (found) {
			float hitDistance = distanceExactDifferences(start, hit);
			float projectedDistance = distanceFloatDifferences(start, projected);

			if (projectedDistance > hitDistance) {
				out[0] = hit[0];
				out[1] = hit[1];
				out[2] = hit[2];

				return true;
			}
		}

		RdVector3 closest;

		closest[0] = projected[0];
		closest[1] = projected[1];
		closest[2] = projected[2];

		WalkPlane *closestPlane = _walkSystem->planeClosest(closest, kPlaneTypeWalk);

		if (closestPlane) {
			closestPlane->closestPoint(closest);

			float distance = distanceExactDifferences(projected, closest);

			if (distance < 5.0f) {
				out[0] = closest[0];
				out[1] = closest[1];
				out[2] = closest[2];

				return true;
			}
		} else {
			warning("SputMouse::getFloorWalkPos(): there is no walk plane");
		}
	}

	RdVector3 hit;

	for (int i = 0; i < 100; i++) {
		for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
			if ((plane->_type & kPlaneTypeWalk) == 0 || !plane->_active)
				continue;

			hit[0] = 1000.0f;
			hit[1] = 1000.0f;
			hit[2] = 1000.0f;
			raycast(plane, start, end, hit);

			if (plane->within(hit, 1)) {
				out[0] = hit[0];
				out[1] = hit[1];
				out[2] = hit[2];

				return true;
			}
		}

		start[2] = (float)((double)start[2] - (double)0.015f);
		end[2] = (float)((double)end[2] - (double)0.015f);
	}

	getCameraRay(sputRender, start, end);

	for (int i = 0; i < 10; i++) {
		for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
			if ((plane->_type & kPlaneTypeWalk) == 0 || !plane->_active)
				continue;

			hit[0] = 1000.0f;
			hit[1] = 1000.0f;
			hit[2] = 1000.0f;
			raycast(plane, start, end, hit);

			if (plane->within(hit, 1)) {
				out[0] = hit[0];
				out[1] = hit[1];
				out[2] = hit[2];

				return true;
			}
		}

		start[2] = (float)((double)start[2] + (double)0.015f);
		end[2] = (float)((double)end[2] + (double)0.015f);
	}

	return false;
}

// The disasm uses the shifted far point of the ray as its direction
void SputMouse::projectToAxis(SputRender *sputRender, float *actorPos, float *axis, float t, float *scalar, float *parameter) {
	RdVector3 start;
	RdVector3 end;

	float positionProductY = actorPos[1] * axis[1];
	float positionProductX = actorPos[0] * axis[0];
	float positionProductZ = actorPos[2] * axis[2];
	double positionDot = ((double)positionProductY + positionProductX) + positionProductZ;

	*parameter = 0.0f;
	*scalar = (float)positionDot;

	if (!getCameraRay(sputRender, start, end)) {
		warning("SputMouse::projectToAxis(): there is no camera");
		return;
	}

	float shiftX = t * axis[0];
	float shiftY = t * axis[1];
	float shiftZ = t * axis[2];
	RdVector3 origin;
	RdVector3 direction;
	RdVector3 toActor;

	origin[0] = start[0] - shiftX;
	origin[1] = start[1] - shiftY;
	origin[2] = start[2] - shiftZ;
	direction[0] = end[0] - shiftX;
	direction[1] = end[1] - shiftY;
	direction[2] = end[2] - shiftZ;
	toActor[0] = actorPos[0] - origin[0];
	toActor[1] = actorPos[1] - origin[1];
	toActor[2] = actorPos[2] - origin[2];

	float negAxisDotDirection = -_rd->dot3(axis, direction);
	float directionLength = _rd->dot3(direction, direction);
	float negAxisDotToActor = -_rd->dot3(axis, toActor);
	float directionDotToActor = _rd->dot3(toActor, direction);
	float axisLength = _rd->dot3(axis, axis);
	double numerator = (double)negAxisDotToActor * directionLength - (double)directionDotToActor * negAxisDotDirection;
	double denominator = (double)axisLength * directionLength - (double)negAxisDotDirection * negAxisDotDirection;

	// The original could divide by zero here, let's avoid that...
	if (denominator == 0.0) {
		warning("SputMouse::projectToAxis(): the axis and the ray are parallel");
		*scalar = 0.0f;

		return;
	}

	float tau = (float)(numerator / denominator);

	*parameter = tau;
	*scalar = (float)(positionDot + (double)tau);
}

int SputMouse::getSelectedDialog(int x, int y, int itemHeight, int numLines) const {
	int width = kDialogWidth;
	int height = itemHeight;
	int pointerX;
	int pointerY;

	getCanvasPosition(&pointerX, &pointerY);
	HotspotManager::convertCoords1080(&x, &y, HotspotManager::kCoordsLegacy, false);
	HotspotManager::convertCoords1080(&width, &height, HotspotManager::kCoordsLegacy, true);

	if (numLines <= 0)
		return 0;

	if (pointerX < x || x + width < pointerX)
		return 0;

	int lineBottom = y;
	int line = 0;

	while (pointerY < lineBottom - height * numLines || height * (1 - numLines) + lineBottom < pointerY) {
		line++;
		lineBottom += height;

		if (line >= numLines)
			return 0;
	}

	return line + 1;
}

void SputMouse::walkActorToMouseTarget(SputActor *actor, SputRender *sputRender) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RoomCamera *roomCamera = sputRender->getCamera();

	if (!camera || !camera->canvas || !roomCamera || camera->focalLen == 0.0f) {
		warning("SputMouse::walkActorToMouseTarget(): the camera is not ready");
		return;
	}

	int centerX = (int)camera->canvas->xCenter;
	int centerY = (int)camera->canvas->yCenter;
	float axisX = getAxisNormalized(12, false);
	float axisY = getAxisNormalized(13, false);
	float centerXFloat = (float)centerX;
	float centerYFloat = (float)centerY;
	double focalLen = camera->focalLen;
	RdVector3 point;
	RdVector3 direction;
	RdMatrix34 cameraMatrix;

	float screenX = (float)((double)axisX * centerXFloat + centerXFloat);
	float screenY = (float)((double)axisY * centerYFloat + centerYFloat);

	point[0] = (float)(((double)screenX - centerXFloat) / focalLen);
	point[1] = 1.0f;
	point[2] = (float)(-(((double)screenY - centerYFloat) / focalLen));
	_rd->_camera.getMatrix(cameraMatrix);
	_rd->transformPoint34Acc(point, cameraMatrix);

	direction[0] = point[0] - roomCamera->position[0];
	direction[1] = point[1] - roomCamera->position[1];
	direction[2] = point[2] - roomCamera->position[2];
	_rd->normalize3Acc(direction);

	float facingY = 0.0f * direction[1];
	float facingX = 0.0f * direction[0];
	float facingZ = 1.0f * direction[2];

	if (((double)facingY + facingX) + facingZ >= 0.0)
		return;

	RdVector3 end;
	RdVector3 goal = { 0.0f, 0.0f, 0.0f };

	for (int i = 0; i < 3; i++) {
		direction[i] = direction[i] * 100.0f;
		end[i] = roomCamera->position[i] + direction[i];
	}

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if ((plane->_type & kPlaneTypeWalk) == 0 || plane->_numTriangles <= 0)
			continue;

		for (int i = 0; i < plane->_numTriangles; i++) {
			int first = plane->_triangles[i][0];
			int second = plane->_triangles[i][1];
			int third = plane->_triangles[i][2];

			if (first < 0 || first >= plane->_numVerts || second < 0 || second >= plane->_numVerts || third < 0 || third >= plane->_numVerts) {
				warning("SputMouse::walkActorToMouseTarget(): a triangle of %s uses a vertex that does not exist", plane->_name);
				continue;
			}

			RdVector3 hit;

			if (segmentTriangleIntersect(hit, &plane->_verts[first]->x, &plane->_verts[second]->x, &plane->_verts[third]->x,
					roomCamera->position, end)) {
				_sputActor->actorWalkTo(actor, hit, goal, nullptr);
				return;
			}
		}
	}

	float nearest = 3.4028235e38f;
	RdVector3 target;
	RdVector3 crossing = { 0.0f, 0.0f, 0.0f };
	bool hasCrossing = false;
	bool hasTarget = false;

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if ((plane->_type & kPlaneTypeWalk) == 0)
			continue;

		// A segment that misses leaves the crossing of the previous plane
		if (segmentPlaneIntersect(crossing, roomCamera->position, end, plane->_normal[0], plane->_normal[1], plane->_normal[2], plane->_tolerance))
			hasCrossing = true;
		else if (!hasCrossing)
			warning("SputMouse::walkActorToMouseTarget(): the ray is parallel to the first walk plane");

		RdVector3 closest;

		closest[0] = crossing[0];
		closest[1] = crossing[1];
		closest[2] = crossing[2];
		plane->closestPoint(closest);

		float distance = distanceExactDifferences(closest, crossing);

		if (nearest > distance) {
			nearest = distance;
			target[0] = closest[0];
			target[1] = closest[1];
			target[2] = closest[2];
			hasTarget = true;
		}
	}

	if (!hasTarget) {
		warning("SputMouse::walkActorToMouseTarget(): there is no walk plane to walk to");
		return;
	}

	_sputActor->actorWalkTo(actor, target, goal, nullptr);
}

bool SputMouse::segmentPlaneIntersect(float *out, const float *start, const float *end, float normalX, float normalY, float normalZ, float constant) const {
	float deltaX = end[0] - start[0];
	float deltaY = end[1] - start[1];
	float deltaZ = end[2] - start[2];
	float startSide = (float)((((double)start[0] * normalX + (double)start[1] * normalY) + (double)start[2] * normalZ) + constant);
	float endSide = (float)((((double)end[0] * normalX + (double)end[1] * normalY) + (double)end[2] * normalZ) + constant);
	double difference = (double)startSide - (double)endSide;
	double magnitude = difference < 0.0 ? -difference : difference;

	if (magnitude < 0.1)
		return false;

	float t = (float)((double)startSide / difference);

	out[0] = (float)((double)t * deltaX + start[0]);
	out[1] = (float)((double)t * deltaY + start[1]);
	out[2] = (float)((double)t * deltaZ + start[2]);

	return true;
}

bool SputMouse::segmentTriangleIntersect(float *out, const float *a, const float *b, const float *c, const float *start, const float *end) const {
	out[0] = 0.0f;
	out[1] = 0.0f;
	out[2] = 0.0f;

	float abX = b[0] - a[0];
	float abY = b[1] - a[1];
	float abZ = b[2] - a[2];
	float acX = c[0] - a[0];
	float acY = c[1] - a[1];
	float acZ = c[2] - a[2];
	RdVector3 ab = { abX, abY, abZ };
	RdVector3 ac = { acX, acY, acZ };
	RdVector3 normal;
	RdVector3 back;
	RdVector3 ap;
	RdVector3 cross;

	normal[0] = abY * acZ - abZ * acY;
	normal[1] = abZ * acX - abX * acZ;
	normal[2] = abX * acY - abY * acX;
	back[0] = start[0] - end[0];
	back[1] = start[1] - end[1];
	back[2] = start[2] - end[2];

	float side = _rd->dot3(normal, back);

	if (side <= 0.0f)
		return false;

	ap[0] = start[0] - a[0];
	ap[1] = start[1] - a[1];
	ap[2] = start[2] - a[2];

	float distance = _rd->dot3(normal, ap);

	if (distance < 0.0f || distance > side)
		return false;

	cross[0] = ap[2] * back[1] - ap[1] * back[2];
	cross[1] = ap[0] * back[2] - ap[2] * back[0];
	cross[2] = ap[1] * back[0] - ap[0] * back[1];

	float v = _rd->dot3(ac, cross);

	if (v < 0.0f || v > side)
		return false;

	float w = -_rd->dot3(ab, cross);

	if (w < 0.0f || (double)w + (double)v > (double)side)
		return false;

	float inverse = 1.0f / side;
	float weightB = v * inverse;
	float weightC = w * inverse;
	float weightA = (float)((1.0 - (double)weightB) - (double)weightC);

	out[0] = a[0] * weightA + 0.0f;
	out[1] = a[1] * weightA + 0.0f;
	out[2] = weightA * a[2] + 0.0f;
	out[0] = b[0] * weightB + out[0];
	out[1] = b[1] * weightB + out[1];
	out[2] = weightB * b[2] + out[2];
	out[0] = out[0] + c[0] * weightC;
	out[1] = out[1] + c[1] * weightC;
	out[2] = out[2] + weightC * c[2];

	return true;
}

int SputMouse::save(Common::WriteStream *stream) {
	stream->writeSint32LE(_mainHandle);
	stream->writeSint32LE(_animHandle);

	if (stream->err()) {
		warning("SputMouse::save(): failed to write the cursor handles");
		return 1;
	}

	return _hotspots.serialize(stream);
}

int SputMouse::restore(Common::ReadStream *stream) {
	_visibleInMovie = 0;
	loadCursors(kNumCursors);

	int mainHandle = stream->readSint32LE();
	int animHandle = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputMouse::restore(): failed to read the cursor handles");
		return 1;
	}

	_mainHandle = mainHandle;
	_animHandle = animHandle;

	return _hotspots.deserialize(stream);
}

} // End of namespace Grim
