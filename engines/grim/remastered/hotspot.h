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
class StdConffile;
class SysStruct;
struct StdRect;

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

	void setOverlays(OverlayManager *overlays) { _overlays = overlays; }
	OverlayManager *getOverlays() const { return _overlays; }

	static void convertCoords1080(int *x, int *y, int mode, bool isSize);
	static void convertCoordsNative(int *x, int *y, int mode, bool isSize);
	static void convertRectNative(StdRect *rect, int mode);

	void load(const char *setName);

	void clear();

	int getActiveHotspots(int canvasX, int canvasY, const Hotspot **out, int max, int mode);

	int addHotspot(const char *id, int x, int y, int width, int height, int cursor, int boost, int menuType,
			const char *hoverImage, int hoverLayer, int coords);
	void removeHotspot(int handle);
	void updateHotspot(const char *id, int x, int y, int coords);
	void linkHotspot(const char *id, int actorHandle);

	int serialize(Common::WriteStream *stream) const;
	int deserialize(Common::ReadStream *stream);

private:
	enum {
		kArgumentSize = 1024
	};

	static bool isCompatible(int mode, int type, int cursor);
	static bool isPriorityCompatible(int mode, int type);
	static bool isInside(int (*poly)[2], int numPoints, int x, int y);

	int newHandle();
	void discardLastHotspot();
	bool updateDynamicPolygon(Hotspot *hotspot);
	void getCentroid(int (*poly)[2], int numPoints, int *x, int *y) const;
	void setHoverHidden(const Hotspot *hotspot, int hidden);

	void parseSetup(StdConffile *conffile, int setup);
	const char *parseLine(StdConffile *conffile, const char *key);

	SysStruct *_hs;
	SputRoom *_sputRoom;
	SputActorModule *_sputActor;
	RenderDroid *_rd;
	OverlayManager *_overlays;

	Hotspot *_hotspots[kMaxHotspots];
	int _numHotspots;
	int _serial;
	char _setName[kSetNameSize];
	char _argument[kArgumentSize];
};

} // End of namespace Grim

#endif
