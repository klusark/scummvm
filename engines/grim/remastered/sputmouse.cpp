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

#include "graphics/surface.h"

#include "image/png.h"

#include "engines/grim/grim.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/sputmouse.h"
#include "engines/grim/stdcontrol.h"
#include "engines/grim/walkplane.h"

namespace Grim {

enum {
	kWaveStepMs = 30,
	kCursorAlpha = 255,
	kDialogWidth = 1440 * 420 / 640,
	kPlaneTypeWalk = 0x1000
};

static const float kRayLength = 100.0f;
static const float kFloorSearchRadius = 5.0f;

SputMouse::SputMouse(GrimEngine *grim, SysStruct *hs, StdControl *stdControl, SputRoom *sputRoom, SputActorModule *sputActor,
		WalkSystem *walkSystem, RenderDroid *rd) :
		_grim(grim), _stdControl(stdControl), _hs(hs), _walkSystem(walkSystem), _rd(rd), _hotspots(hs, sputRoom, sputActor, rd),
		_imagesLoaded(false), _defaultCursor(0), _cursorShown(false), _waveActive(false), _wave(0), _waveEnd(-1), _waveNext(0),
		_visibleInMovie(false) {
	for (int i = 0; i < kNumCursorImages; i++) {
		_images[i].width = 0;
		_images[i].height = 0;
	}
}

void SputMouse::getCanvasPosition(int *x, int *y) const {
	_stdControl->getMouseCanvasPosition(_grim->getLibMode() == GrimEngine::kLibModeMenu, x, y);
}

void SputMouse::getAxes(float *axisX, float *axisY) const {
	int x;
	int y;

	getCanvasPosition(&x, &y);
	HotspotManager::convertCoordsNative(&x, &y, HotspotManager::kCoordsLegacy, false);

	float valueX = ((float)x - 320.0f) / 320.0f;
	float valueY = ((float)y - 240.0f) / 240.0f;

	*axisX = MAX(-1.0f, MIN(1.0f, valueX));
	*axisY = MAX(-1.0f, MIN(1.0f, valueY));
}

void SputMouse::setCursor(int index) {
	_defaultCursor = index;
}

void SputMouse::showCursor(bool show) {
	_cursorShown = show;
	_waveActive = false;
	_waveEnd = -1;
}

void SputMouse::cursorAnimation(int first, int last) {
	_waveActive = true;
	_wave = first;
	_waveEnd = last;
	_waveNext = g_system->getMillis() + kWaveStepMs;
}

void SputMouse::loadCursorImage(int index) {
	Common::String name = Common::String::format("cursor%02d.png", index);
	Common::SeekableReadStream *stream = _hs->openFile(name.c_str());

	if (!stream) {
		warning("SputMouse::loadCursorImage(): can't open %s", name.c_str());
		return;
	}

	Image::PNGDecoder decoder;
	bool loaded = decoder.loadStream(*stream);

	delete stream;

	if (!loaded) {
		warning("SputMouse::loadCursorImage(): can't decode %s", name.c_str());
		return;
	}

	const Graphics::Palette &palette = decoder.getPalette();
	Graphics::Surface *converted = decoder.getSurface()->convertTo(Graphics::PixelFormat::createFormatRGBA32(), palette.data(), palette.size());
	CursorImage *image = &_images[index];

	image->width = converted->w;
	image->height = converted->h;
	image->pixels.resize(converted->w * converted->h * 4);

	for (int y = 0; y < converted->h; y++)
		memcpy(&image->pixels[y * converted->w * 4], converted->getBasePtr(0, y), converted->w * 4);

	converted->free();
	delete converted;
}

void SputMouse::preloadCursors() {
	_imagesLoaded = true;

	for (int i = 0; i < kNumCursorImages; i++) {
		if (_images[i].pixels.empty())
			loadCursorImage(i);
	}
}

void SputMouse::update() {
	if (!_imagesLoaded)
		preloadCursors();

	if (!_waveActive)
		return;

	uint32 now = g_system->getMillis();

	if (now < _waveNext)
		return;

	_wave++;
	_waveNext = now + kWaveStepMs;

	if (_wave >= _waveEnd) {
		_waveActive = false;
		_waveEnd = -1;
	}
}

// The cursor is centered on the pointer. Slots without an image draw nothing.
void SputMouse::drawCursorImage(HdCanvas *canvas, int index) {
	if (index < 0 || index >= kNumCursorImages || _images[index].pixels.empty())
		return;

	const CursorImage *image = &_images[index];
	int x;
	int y;

	getCanvasPosition(&x, &y);
	canvas->queueBitmap(kCursorLayer, &image->pixels[0], image->width, image->height, x - image->width / 2, y - image->height / 2, kCursorAlpha);
}

void SputMouse::draw(HdCanvas *canvas) {
	if (!_imagesLoaded)
		preloadCursors();

	if (_cursorShown)
		drawCursorImage(canvas, _defaultCursor);

	if (_waveActive)
		drawCursorImage(canvas, _wave);
}

// The ray of the pointer from the camera, as a segment of 100 units
bool SputMouse::getCameraRay(RdVector3 start, RdVector3 end) const {
	RdCamera *camera = _rd->_camera.getCurrent();

	if (!camera || !camera->canvas || camera->focalLen == 0.0f)
		return false;

	int x;
	int y;

	getCanvasPosition(&x, &y);
	HotspotManager::convertCoordsNative(&x, &y, HotspotManager::kCoordsLegacy, false);

	RdVector3 direction;
	RdVector3 world;
	RdMatrix34 cameraMatrix;

	direction[0] = ((float)x - camera->canvas->xCenter) / camera->focalLen;
	direction[1] = 1.0f;
	direction[2] = -((float)y - camera->canvas->yCenter) / camera->focalLen;
	_rd->normalize3Acc(direction);

	_rd->invertOrtho34(cameraMatrix, camera->viewMatrix);
	_rd->transformVector34(world, direction, cameraMatrix);
	_rd->_camera.getPosition(start);

	for (int i = 0; i < 3; i++)
		end[i] = start[i] + world[i] * kRayLength;

	return true;
}

bool SputMouse::isWalkable(const WalkPlane *plane) const {
	return (plane->_type & kPlaneTypeWalk) != 0 && plane->_active && plane->_numVerts > 0;
}

// Nearest walkable plane that the segment crosses inside its polygon
bool SputMouse::findPlaneHit(const float *start, const float *end, float *out) const {
	double bestT = 2.0;
	bool found = false;

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if (!isWalkable(plane))
			continue;

		const WpVertex *vertex = plane->_verts[0];
		double direction[3];
		double toPlane[3];
		double denominator = 0.0;
		double numerator = 0.0;

		for (int i = 0; i < 3; i++) {
			direction[i] = (double)end[i] - start[i];
			denominator += plane->_normal[i] * direction[i];
		}

		toPlane[0] = (double)vertex->x - start[0];
		toPlane[1] = (double)vertex->y - start[1];
		toPlane[2] = (double)vertex->z - start[2];

		for (int i = 0; i < 3; i++)
			numerator += plane->_normal[i] * toPlane[i];

		if (fabs(denominator) < 0.000001)
			continue;

		double t = numerator / denominator;

		if (t < 0.0 || t > 1.0 || t >= bestT)
			continue;

		float hit[3];

		for (int i = 0; i < 3; i++)
			hit[i] = (float)(start[i] + t * direction[i]);

		if (!plane->within(hit, 0))
			continue;

		bestT = t;
		out[0] = hit[0];
		out[1] = hit[1];
		out[2] = hit[2];
		found = true;
	}

	return found;
}

// With onGround the point is where the ray meets the plane z = 0, moved onto the nearest walk plane.
// Otherwise it is the first walk plane under the pointer, or the walkable point closest to a plane
// that the ray crosses.
bool SputMouse::getFloorWalkPos(bool onGround, RdVector3 out) {
	RdVector3 start;
	RdVector3 end;

	if (!getCameraRay(start, end))
		return false;

	if (onGround) {
		double deltaZ = (double)end[2] - start[2];

		if (fabs(deltaZ) < 0.000001)
			return false;

		double t = -(double)start[2] / deltaZ;
		RdVector3 point;

		for (int i = 0; i < 3; i++)
			point[i] = (float)(start[i] + t * ((double)end[i] - start[i]));

		WalkPlane *plane = _walkSystem->planeClosest(point, kPlaneTypeWalk);

		if (!plane)
			return false;

		plane->closestPoint(point);
		out[0] = point[0];
		out[1] = point[1];
		out[2] = point[2];

		return true;
	}

	if (findPlaneHit(start, end, out))
		return true;

	// Nothing under the pointer: look for the closest walkable point

	double bestDistance = (double)kFloorSearchRadius * kFloorSearchRadius;
	bool found = false;

	for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
		if (!isWalkable(plane))
			continue;

		const WpVertex *vertex = plane->_verts[0];
		double vertexPos[3];
		double denominator = 0.0;
		double numerator = 0.0;

		vertexPos[0] = vertex->x;
		vertexPos[1] = vertex->y;
		vertexPos[2] = vertex->z;

		for (int i = 0; i < 3; i++) {
			denominator += plane->_normal[i] * ((double)end[i] - start[i]);
			numerator += plane->_normal[i] * (vertexPos[i] - start[i]);
		}

		if (fabs(denominator) < 0.000001)
			continue;

		double t = numerator / denominator;

		if (t < 0.0 || t > 1.0)
			continue;

		float hit[3];
		float closest[3];

		for (int i = 0; i < 3; i++) {
			hit[i] = (float)(start[i] + t * ((double)end[i] - start[i]));
			closest[i] = hit[i];
		}

		plane->closestPoint(closest);

		double distance = 0.0;

		for (int i = 0; i < 3; i++)
			distance += ((double)closest[i] - hit[i]) * ((double)closest[i] - hit[i]);

		if (distance < bestDistance) {
			bestDistance = distance;
			out[0] = closest[0];
			out[1] = closest[1];
			out[2] = closest[2];
			found = true;
		}
	}

	return found;
}

// Closest approach of the pointer ray to the line through the actor, moved by offset along the axis.
// The scalar is the position of that point along the axis, the parameter its distance from the moved
// actor position: it is positive when the pointer is ahead of the actor on the axis.
void SputMouse::projectToAxis(const float *actorPos, const float *axis, float offset, float *scalar, float *parameter) {
	RdVector3 start;
	RdVector3 end;
	double base[3];

	for (int i = 0; i < 3; i++)
		base[i] = (double)actorPos[i] + (double)offset * axis[i];

	*parameter = 0.0f;
	*scalar = (float)(base[0] * axis[0] + base[1] * axis[1] + base[2] * axis[2]);

	if (!getCameraRay(start, end))
		return;

	double direction[3];
	double fromRay[3];
	double axisDotAxis = 0.0;
	double axisDotRay = 0.0;
	double rayDotRay = 0.0;
	double axisDotFrom = 0.0;
	double rayDotFrom = 0.0;

	for (int i = 0; i < 3; i++) {
		direction[i] = (double)end[i] - start[i];
		fromRay[i] = base[i] - start[i];
		axisDotAxis += (double)axis[i] * axis[i];
		axisDotRay += (double)axis[i] * direction[i];
		rayDotRay += direction[i] * direction[i];
		axisDotFrom += (double)axis[i] * fromRay[i];
		rayDotFrom += direction[i] * fromRay[i];
	}

	double denominator = axisDotAxis * rayDotRay - axisDotRay * axisDotRay;

	if (fabs(denominator) < 0.000001)
		return;

	double u = (axisDotRay * rayDotFrom - rayDotRay * axisDotFrom) / denominator;
	double closest[3];

	for (int i = 0; i < 3; i++)
		closest[i] = base[i] + u * axis[i];

	*parameter = (float)u;
	*scalar = (float)(closest[0] * axis[0] + closest[1] * axis[1] + closest[2] * axis[2]);
}

// The lines of a dialog menu go up from the point (x, y), each one itemHeight tall.
// Returns the number of the line under the pointer, from 1, or 0.
int SputMouse::getSelectedDialog(int x, int y, int itemHeight, int numLines) const {
	int left = x;
	int bottom = y;
	int width = 0;
	int height = itemHeight;
	int pointerX;
	int pointerY;

	HotspotManager::convertCoords1080(&left, &bottom, HotspotManager::kCoordsLegacy, false);
	HotspotManager::convertCoords1080(&width, &height, HotspotManager::kCoordsLegacy, true);
	getCanvasPosition(&pointerX, &pointerY);

	if (pointerX < left || pointerX >= left + kDialogWidth)
		return 0;

	for (int i = 0; i < numLines; i++) {
		int top = bottom + (i - numLines) * height;

		if (pointerY >= top && pointerY < top + height)
			return i + 1;
	}

	return 0;
}

// The cursor overlays of the original saves were always the first two overlays
int SputMouse::save(Common::WriteStream *stream) {
	stream->writeSint32LE(1);
	stream->writeSint32LE(2);

	if (stream->err()) {
		warning("SputMouse::save(): failed to write the cursor handles");
		return 1;
	}

	return _hotspots.serialize(stream);
}

int SputMouse::restore(Common::ReadStream *stream) {
	stream->readSint32LE();
	stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputMouse::restore(): failed to read the cursor handles");
		return 1;
	}

	_visibleInMovie = false;
	preloadCursors();

	return _hotspots.deserialize(stream);
}

} // End of namespace Grim
