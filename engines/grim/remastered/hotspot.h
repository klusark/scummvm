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

#ifndef GRIM_HOTSPOT_H
#define GRIM_HOTSPOT_H

#include "common/scummsys.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class OverlayManager;
class RenderDroid;
class SputActorModule;
class SputRoom;
class SysStruct;

struct Hotspot {
	enum {
		kMaxPath = 6,
		kMaxPolygon = 14,
		kNameSize = 40,
		kIdSize = 32
	};

	int added;
	int handle;
	int setup;
	char name[kNameSize];
	char id[kIdSize];
	int type;
	int cursor;
	int pathLen;
	float path[kMaxPath][3];
	float offset[3];
	int actorIndex;
	int polyNum;
	int poly[kMaxPolygon][2];
	int coordMode;
	int overlayHandle;
	float wait;
};

// Click areas of the scenes (the .hot file of each set) and the ones that the
// scripts add. Polygon points are in the coordinates of the hotspot (coordMode).
class HotspotManager {
public:
	enum {
		kMaxHotspots = 128,
		kNumHandles = 1024,
		kSetNameSize = 40
	};

	enum Type {
		kTypeNone,
		kTypeObject,
		kTypeWalk,
		kTypeDynamic,
		kTypeSpecial,
		kTypeDialog,
		kTypeOptions,
		kTypeOverlay,
		kTypeMenu,
		kTypeMenuDialog
	};

	enum Cursor {
		kCursorNone,
		kCursorHotspot,
		kCursorWalk,
		kCursorArrowRight,
		kCursorArrowLeft,
		kCursorArrowUp,
		kCursorArrowDown
	};

	// Modes of the query, the numbers of the HOTSPOT_* constants of the scripts
	enum QueryMode {
		kModeNormal = 0,
		kModeSpecial = 1,
		kModeMenu = 2,
		kModeOnlyPath = 3,
		kModeQuery = 5
	};

	enum Coords {
		kCoordsLegacy,
		kCoordsGameScreen,
		kCoordsGlobal
	};

	HotspotManager(SysStruct *hs, SputRoom *sputRoom, SputActorModule *sputActor, RenderDroid *rd);
	~HotspotManager();

	// The overlays that show an image when the pointer is on a hotspot
	void setOverlays(OverlayManager *overlays) { _overlays = overlays; }

	// The 1920x1080 canvas and the coordinate spaces of the scripts
	static void convertCoords1080(int *x, int *y, int mode, bool isSize);
	static void convertCoordsNative(int *x, int *y, int mode, bool isSize);

	// Loads the hotspots of the set unless it is the one already loaded. The
	// hotspots added by the scripts stay.
	void load(const char *setName);
	void loadCurrentRoom();

	// Hotspots under a point of the canvas, the ones with priority come first
	int getActiveHotspots(int canvasX, int canvasY, const Hotspot **out, int max, int mode);

	int addHotspot(const char *id, int x, int y, int width, int height, int cursor, int boost, int menuType,
			const char *hoverImage, int hoverLayer, int coords);
	void removeHotspot(int handle);
	void updateHotspot(const char *id, int x, int y, int coords);
	void linkHotspot(const char *id, int actorIndex);

	// Block of the save game after the two cursor handles
	int serialize(Common::WriteStream *stream) const;
	int deserialize(Common::ReadStream *stream);

private:
	struct TextBuffer {
		char *data;
		int size;
		int pos;
	};

	static bool isCompatible(int mode, int type, int cursor);
	static bool isPriorityCompatible(int mode, int type);
	static bool isInside(int (*poly)[2], int numPoints, int x, int y);

	void clear();
	void removeFileHotspots();
	void removeAt(int index);
	int newHandle();
	bool isInCurrentSetup(const Hotspot *hotspot) const;
	bool getPolygon(const Hotspot *hotspot, int (*poly)[2]) const;
	void getCentroid(int (*poly)[2], int numPoints, int *x, int *y) const;
	void setHoverHidden(const Hotspot *hotspot, int hidden);

	bool parseSet(TextBuffer *text);
	int parseSetup(TextBuffer *text, int setup);
	bool parseHotspot(TextBuffer *text, int setup, Hotspot *hotspot);
	const char *parseLine(TextBuffer *text, const char *key);

	SysStruct *_hs;
	SputRoom *_sputRoom;
	SputActorModule *_sputActor;
	RenderDroid *_rd;
	OverlayManager *_overlays;

	Hotspot *_hotspots[kMaxHotspots];
	int _numHotspots;
	int _nextHandle;
	char _setName[kSetNameSize];
};

} // End of namespace Grim

#endif
