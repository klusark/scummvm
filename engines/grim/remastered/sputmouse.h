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

#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/remastered/hotspot.h"
#include "engines/grim/renderdroid/rd.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class GrimEngine;
class OverlayManager;
class SputRender;
class SputRoom;
class StdControl;
class SysStruct;
class WalkPlane;
class WalkSystem;
struct SputActor;

// The cursor of the remastered and the queries that the scripts make with the mouse.
// The cursor is two entries of the overlays, drawn with the others of the cursor layer.
// Positions are on the 1920x1080 canvas, the play area is 1440 wide and starts at 240.
class SputMouse {
public:
	enum {
		kNumCursors = 23,
		kNumCursorImages = 8,
		kCursorLayer = 4,
		kWaveStepMs = 30
	};

	SputMouse(GrimEngine *grim, SysStruct *hs, StdControl *stdControl, SputRoom *sputRoom, SputActorModule *sputActor,
			WalkSystem *walkSystem, RenderDroid *rd);

	HotspotManager *getHotspots() { return &_hotspots; }

	// Loads the cursor images and creates the two cursor overlays: they have to be the first
	// overlays of the game. The hotspots need their overlays before. Returns 0 the first time.
	int startup();
	void shutdown();

	void getCanvasPosition(int *x, int *y) const;
	// Mouse axis of the controls, -1 to 1 around the center of the 640x480 screen
	float getAxisNormalized(int axis, bool invert);

	void setCursor(int index);
	void showCursor(bool show);
	void cursorAnimation(int first, int last);
	void preloadCursors();
	void setVisibleInMovie(int visible) { _visibleInMovie = visible; }
	bool isVisibleInMovie() const { return _visibleInMovie != 0; }

	// Once per frame, before the overlays of the cursor layer are drawn
	void update();

	// The render module gives the camera and the depth of the scene under the pointer
	bool getFloorWalkPos(bool onGround, SputRender *sputRender, RdVector3 out);
	void projectToAxis(SputRender *sputRender, const float *actorPos, const float *axis, float t, float *scalar, float *parameter);
	int getSelectedDialog(int x, int y, int itemHeight, int numLines) const;
	void walkActorToMouseTarget(SputActor *actor, SputRender *sputRender);

	// Block of the save game: the two cursor handles, then the hotspots
	int save(Common::WriteStream *stream);
	int restore(Common::ReadStream *stream);

private:
	bool getCameraRay(SputRender *sputRender, RdVector3 start, RdVector3 end) const;
	bool reverseProject(SputRender *sputRender, int x, int y, RdVector3 out) const;
	void raycast(const WalkPlane *plane, const float *start, const float *end, float *hit) const;
	bool segmentPlaneIntersect(float *out, const float *start, const float *end, float normalX, float normalY, float normalZ, float constant) const;
	bool segmentTriangleIntersect(float *out, const float *a, const float *b, const float *c, const float *start, const float *end) const;
	void loadCursors(int count);
	const Common::String &getCursorName(int index) const;
	void placeCursor(int handle, int x, int y);

	GrimEngine *_grim;
	StdControl *_stdControl;
	SputActorModule *_sputActor;
	WalkSystem *_walkSystem;
	RenderDroid *_rd;

	HotspotManager _hotspots;
	// Names of the cached cursor images, empty where the slot has no image
	Common::String _cursorNames[kNumCursors];
	Common::String _noImage;
	bool _started;
	int _visibleInMovie;
	int _mainHandle;
	int _animHandle;
	int _wave;
	int _waveEnd;
	uint32 _waveNext;
};

} // End of namespace Grim

#endif
