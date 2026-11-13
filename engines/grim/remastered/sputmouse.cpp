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

// Loads the images of the first count cursors and keeps their names, the other slots have no image
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

// The image of a cursor slot. A slot out of the table has no image.
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

// The cursor shows when show is set, the animation cursor always hides
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

// A cursor with an image and a layer is centered on the pointer
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

// Nothing happens without a camera. The animation moves on every 30 ms: the end is tested
// before the next image is shown, so the last image of the animation is shown too.
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

// The ray of the pointer from the camera of the room: its position and the far point, 100 units away
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

	float focalLen = camera->focalLen;
	RdVector3 point;
	RdVector3 direction;
	RdMatrix34 cameraMatrix;

	point[0] = ((float)x - camera->canvas->xCenter) / focalLen;
	point[1] = 1.0f;
	point[2] = -((float)y - camera->canvas->yCenter) / focalLen;
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

// The depth of the scene at a point of the screen, as a point of the world. The z buffer of the
// scene without the actors is used.
bool SputMouse::reverseProject(SputRender *sputRender, int x, int y, RdVector3 out) const {
	RdCamera *camera = _rd->_camera.getCurrent();
	RoomCamera *roomCamera = sputRender->getCamera();
	StdVBuffer *zBuffer = sputRender->cleanZBuffer();

	if (!camera || !camera->canvas || !roomCamera || !zBuffer || !zBuffer->pixels || camera->focalLen == 0.0f)
		return false;

	if (x < 0 || y < 0 || x > 639 || y > 479 || x >= zBuffer->tex.width || y >= zBuffer->tex.height)
		return false;

	uint16 zValue = ((const uint16 *)zBuffer->pixels)[zBuffer->tex.width * y + x];
	float scaledZ = (float)(int)((uint)zValue << (camera->canvas->zScaleShift & 31));

	// A depth of zero is infinitely far
	if (scaledZ == 0.0f)
		return false;

	float distance = 1.0f / (scaledZ * 1.52587890625e-5f) + -1.0f;

	if (!(distance <= 100.0f))
		return false;

	float focalLen = camera->focalLen;
	float centerX = camera->canvas->xCenter + -0.5f;
	float centerY = camera->canvas->yCenter + -0.5f;
	RdVector3 point;

	point[0] = (((float)x - centerX) * distance) / focalLen;
	point[1] = distance;
	point[2] = -(((float)y - centerY) * distance) / focalLen;
	_rd->transformPoint34(out, point, roomCamera->posMatrix);

	return true;
}

// The point where the line through start and end meets the plane of a walk plane. The hit is left as
// it was when the plane has no vertices or the line is parallel to it.
void SputMouse::raycast(const WalkPlane *plane, const float *start, const float *end, float *hit) const {
	if (plane->_numVerts == 0)
		return;

	float deltaX = end[0] - start[0];
	float deltaY = end[1] - start[1];
	float deltaZ = end[2] - start[2];
	float normalX = plane->_normal[0];
	float normalY = plane->_normal[1];
	float normalZ = plane->_normal[2];
	float denominator = deltaZ * normalZ + (deltaY * normalY + deltaX * normalX);
	float magnitude = denominator < 0.0f ? -denominator : denominator;

	if (!((double)magnitude >= 0.000001))
		return;

	const WpVertex *vertex = plane->_verts[0];
	float t = ((vertex->z - start[2]) * normalZ + ((vertex->y - start[1]) * normalY + (vertex->x - start[0]) * normalX)) / denominator;

	hit[0] = deltaX * t + start[0];
	hit[1] = deltaY * t + start[1];
	hit[2] = deltaZ * t + start[2];
}

// With onGround the point is where the ray meets the plane z = 0, moved onto the nearest walk plane.
// Otherwise it is the walk plane under the pointer when it is nearer than the depth of the scene, or
// the walkable point closest to that depth. When both fail the ray is moved down in small steps and
// then up until it meets a walk plane.
bool SputMouse::getFloorWalkPos(bool onGround, SputRender *sputRender, RdVector3 out) {
	RdVector3 start;
	RdVector3 end;

	if (!getCameraRay(sputRender, start, end))
		return false;

	if (onGround) {
		// The original divides by zero when the ray is level
		if (end[2] - start[2] == 0.0f) {
			warning("SputMouse::getFloorWalkPos(): the ray does not meet the ground");
			return false;
		}

		float scale = -start[2] / (end[2] - start[2]);
		RdVector3 point;

		point[0] = (end[0] - start[0]) * scale + start[0];
		point[1] = (end[1] - start[1]) * scale + start[1];
		point[2] = scale * (end[2] - start[2]) + start[2];

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

	if (reverseProject(sputRender, x, y, projected)) {
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
			float hitDistance = sqrtf((start[2] - hit[2]) * (start[2] - hit[2]) +
					((start[1] - hit[1]) * (start[1] - hit[1]) + (start[0] - hit[0]) * (start[0] - hit[0])));
			float projectedDistance = sqrtf((start[2] - projected[2]) * (start[2] - projected[2]) +
					((start[1] - projected[1]) * (start[1] - projected[1]) + (start[0] - projected[0]) * (start[0] - projected[0])));

			// The plane is nearer than the depth of the scene
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

			float distance = sqrtf((projected[2] - closest[2]) * (projected[2] - closest[2]) +
					((projected[1] - closest[1]) * (projected[1] - closest[1]) + (projected[0] - closest[0]) * (projected[0] - closest[0])));

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

	// Nothing close to the depth: move the ray down in small steps, then up
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

		start[2] = start[2] + -0.015f;
		end[2] = end[2] + -0.015f;
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

		start[2] = start[2] + 0.015f;
		end[2] = end[2] + 0.015f;
	}

	return false;
}

// Closest approach of two lines: the one through the actor along the axis, and the one of the pointer
// ray, both moved back by t along the axis. The shifted far point of the ray is used as its direction,
// like the original does. The scalar is the axis dotted with the actor position plus the parameter.
void SputMouse::projectToAxis(SputRender *sputRender, const float *actorPos, const float *axis, float t, float *scalar, float *parameter) {
	RdVector3 start;
	RdVector3 end;

	*parameter = 0.0f;
	*scalar = axis[2] * actorPos[2] + (axis[1] * actorPos[1] + axis[0] * actorPos[0]);

	if (!getCameraRay(sputRender, start, end)) {
		warning("SputMouse::projectToAxis(): there is no camera");
		return;
	}

	// Both ends of the ray move back by t along the axis. The moved far point is the direction.
	float originX = start[0] - axis[0] * t;
	float originY = start[1] - axis[1] * t;
	float originZ = start[2] - t * axis[2];
	float directionX = end[0] - axis[0] * t;
	float directionY = end[1] - axis[1] * t;
	float directionZ = end[2] - t * axis[2];
	float axisDotDirection = axis[2] * directionZ + (axis[1] * directionY + axis[0] * directionX);
	float directionLength = directionZ * directionZ + (directionY * directionY + directionX * directionX);
	float axisLength = axis[2] * axis[2] + (axis[1] * axis[1] + axis[0] * axis[0]);
	float toActorX = actorPos[0] - originX;
	float toActorY = actorPos[1] - originY;
	float toActorZ = actorPos[2] - originZ;
	float directionDotToActor = toActorZ * directionZ + (toActorY * directionY + toActorX * directionX);
	float axisDotToActor = axis[2] * toActorZ + (axis[1] * toActorY + axis[0] * toActorX);
	float denominator = directionLength * axisLength - axisDotDirection * axisDotDirection;

	// The original divides by zero here
	if (denominator == 0.0f) {
		warning("SputMouse::projectToAxis(): the axis and the ray are parallel");
		*scalar = 0.0f;

		return;
	}

	float tau = (directionDotToActor * axisDotDirection - axisDotToActor * directionLength) / denominator;

	*parameter = tau;
	*scalar = *scalar + tau;
}

// The lines of a dialog menu go up from the point (x, y), each one itemHeight tall.
// Returns the number of the line under the pointer, from 1, or 0.
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

	// Both edges of the area and of each line count as inside
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

// A point the actor walks to under the pointer. The pointer axes give a ray from the camera.
// A ray that goes up does nothing. The first triangle of a walk plane that the ray crosses is
// the target; when there is none, the closest point of the plane that is nearest to the ray.
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
	float focalLen = camera->focalLen;
	RdVector3 point;
	RdVector3 direction;
	RdMatrix34 cameraMatrix;

	point[0] = (((axisX * (float)centerX) + (float)centerX) - (float)centerX) / focalLen;
	point[1] = 1.0f;
	point[2] = -(((axisY * (float)centerY) + (float)centerY) - (float)centerY) / focalLen;
	_rd->_camera.getMatrix(cameraMatrix);
	_rd->transformPoint34Acc(point, cameraMatrix);

	direction[0] = point[0] - roomCamera->position[0];
	direction[1] = point[1] - roomCamera->position[1];
	direction[2] = point[2] - roomCamera->position[2];
	_rd->normalize3Acc(direction);

	if (!(direction[2] * 1.0f + (direction[1] * 0.0f + direction[0] * 0.0f) < 0.0f))
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

	// No triangle: the closest point of the plane that is nearest to the ray
	float nearest = 3.4028235e38f;
	RdVector3 target;
	RdVector3 crossing = { 0.0f, 0.0f, 0.0f };
	bool hasCrossing = false;
	bool hasTarget = false;

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if ((plane->_type & kPlaneTypeWalk) == 0)
			continue;

		// The fourth value of the plane is the tolerance of the plane. A segment that misses leaves the
		// crossing of the previous plane, the original starts from an undefined point.
		if (segmentPlaneIntersect(crossing, roomCamera->position, end, plane->_normal[0], plane->_normal[1], plane->_normal[2], plane->_tolerance))
			hasCrossing = true;
		else if (!hasCrossing)
			warning("SputMouse::walkActorToMouseTarget(): the ray is parallel to the first walk plane");

		RdVector3 closest;

		closest[0] = crossing[0];
		closest[1] = crossing[1];
		closest[2] = crossing[2];
		plane->closestPoint(closest);

		float distance = sqrtf((closest[2] - crossing[2]) * (closest[2] - crossing[2]) +
				((closest[1] - crossing[1]) * (closest[1] - crossing[1]) + (closest[0] - crossing[0]) * (closest[0] - crossing[0])));

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

// The point where the segment from start to end meets the plane through the origin with a normal and
// a constant. False when the segment is nearly parallel to it.
bool SputMouse::segmentPlaneIntersect(float *out, const float *start, const float *end, float normalX, float normalY, float normalZ, float constant) const {
	float startSide = constant + (start[2] * normalZ + (start[1] * normalY + start[0] * normalX));
	float endSide = constant + (end[2] * normalZ + (end[1] * normalY + end[0] * normalX));
	float difference = startSide - endSide;
	float magnitude = difference < 0.0f ? -difference : difference;

	if (!((double)magnitude >= 0.1))
		return false;

	float t = startSide / difference;

	out[0] = (end[0] - start[0]) * t + start[0];
	out[1] = (end[1] - start[1]) * t + start[1];
	out[2] = t * (end[2] - start[2]) + start[2];

	return true;
}

// The point where the segment from start to end crosses the triangle a, b, c, only from the side that
// the triangle faces. The point is left at zero when it does not.
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
	float normalX = abY * acZ - abZ * acY;
	float normalY = abZ * acX - abX * acZ;
	float normalZ = abX * acY - abY * acX;
	float backX = start[0] - end[0];
	float backY = start[1] - end[1];
	float backZ = start[2] - end[2];
	float side = normalZ * backZ + (normalY * backY + backX * normalX);

	if (!(side > 0.0f))
		return false;

	float apX = start[0] - a[0];
	float apY = start[1] - a[1];
	float apZ = start[2] - a[2];
	float distance = normalZ * apZ + (normalY * apY + normalX * apX);

	if (distance < 0.0f || distance > side)
		return false;

	float crossX = backY * apZ - apY * backZ;
	float crossY = backZ * apX - apZ * backX;
	float crossZ = backX * apY - apX * backY;
	float v = acZ * crossZ + (acY * crossY + acX * crossX);

	if (v < 0.0f || v > side)
		return false;

	float w = abZ * crossZ + (abY * crossY + abX * crossX);

	if (!(w <= -0.0f) || v - w > side)
		return false;

	float inverse = 1.0f / side;
	float weightB = v * inverse;
	float weightC = w * inverse;
	float weightA = (1.0f - weightB) + weightC;

	out[0] = a[0] * weightA + 0.0f;
	out[1] = a[1] * weightA + 0.0f;
	out[2] = weightA * a[2] + 0.0f;
	out[0] = b[0] * weightB + out[0];
	out[1] = b[1] * weightB + out[1];
	out[2] = weightB * b[2] + out[2];
	out[0] = out[0] - c[0] * weightC;
	out[1] = out[1] - c[1] * weightC;
	out[2] = out[2] - weightC * c[2];

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

// The cursor images are loaded again, the cursor overlays come from the overlay block
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
