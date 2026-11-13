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

#ifndef GRIM_SPUTMOUSE_H
#define GRIM_SPUTMOUSE_H

#include "common/array.h"
#include "common/scummsys.h"

#include "engines/grim/remastered/hotspot.h"
#include "engines/grim/renderdroid/rd.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class GrimEngine;
class HdCanvas;
class SputActorModule;
class SputRoom;
class StdControl;
class SysStruct;
class WalkPlane;
class WalkSystem;

// The cursor of the remastered and the queries that the scripts make with the mouse.
// Positions are on the 1920x1080 canvas, the play area is 1440 wide and starts at 240.
class SputMouse {
public:
	enum {
		kNumCursors = 23,
		kNumCursorImages = 8,
		kCursorLayer = 4
	};

	SputMouse(GrimEngine *grim, SysStruct *hs, StdControl *stdControl, SputRoom *sputRoom, SputActorModule *sputActor,
			WalkSystem *walkSystem, RenderDroid *rd);

	HotspotManager *getHotspots() { return &_hotspots; }

	void getCanvasPosition(int *x, int *y) const;
	// The position as the mouse axes of the original: -1 to 1 around the center of the 640x480 screen
	void getAxes(float *axisX, float *axisY) const;

	void setCursor(int index);
	void showCursor(bool show);
	void cursorAnimation(int first, int last);
	void preloadCursors();
	void setVisibleInMovie(bool visible) { _visibleInMovie = visible; }
	bool isVisibleInMovie() const { return _visibleInMovie; }

	// Once per frame, before the cursor is drawn
	void update();
	// Puts the cursor in the queue of the canvas, on the layer of the cursor
	void draw(HdCanvas *canvas);

	bool getFloorWalkPos(bool onGround, RdVector3 out);
	void projectToAxis(const float *actorPos, const float *axis, float offset, float *scalar, float *parameter);
	int getSelectedDialog(int x, int y, int itemHeight, int numLines) const;

	// Block of the save game: the two cursor handles, then the hotspots
	int save(Common::WriteStream *stream);
	int restore(Common::ReadStream *stream);

private:
	struct CursorImage {
		Common::Array<byte> pixels;
		int width;
		int height;
	};

	bool getCameraRay(RdVector3 start, RdVector3 end) const;
	bool isWalkable(const WalkPlane *plane) const;
	bool findPlaneHit(const float *start, const float *end, float *out) const;
	void loadCursorImage(int index);
	void drawCursorImage(HdCanvas *canvas, int index);

	GrimEngine *_grim;
	StdControl *_stdControl;
	SysStruct *_hs;
	WalkSystem *_walkSystem;
	RenderDroid *_rd;

	HotspotManager _hotspots;
	CursorImage _images[kNumCursorImages];
	bool _imagesLoaded;
	int _defaultCursor;
	bool _cursorShown;
	bool _waveActive;
	int _wave;
	int _waveEnd;
	uint32 _waveNext;
	bool _visibleInMovie;
};

} // End of namespace Grim

#endif
