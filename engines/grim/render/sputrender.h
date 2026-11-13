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

#ifndef GRIM_SPUTRENDER_H
#define GRIM_SPUTRENDER_H

#include "common/scummsys.h"

#include "engines/grim/res.h"
#include "engines/grim/sputcontrol.h"
#include "engines/grim/std/std_display.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class BlastText;
class GrimEngine;
class LegacyImporter;
class Lua_V1;
class RenderDroid;
class SputActorModule;
class SputPrimitive;
class SputResource;
class SputRoom;
class SputSmush;
class SputText;
class SputTime;
class Std3D;
class StdControl;
class WalkSystem;
struct RdCanvas;
struct Room;
struct RoomCamera;
struct RoomSetup;
struct SputActor;

typedef int (*SputRenderPaletteFunc)(byte *, int);
typedef int (*SputRenderPageFlipFunc)();

struct DirtyRect {
	DirtyRect *next;
	DirtyRect *prev;
	int page;
	int serial;
	uint pageIndex;
	StdRect rect;
	DirtyRect *nextFree;
};

struct CacheRect {
	int magic;
	StdRect rect;
	StdVBuffer *color;
	StdVBuffer *z;
	CacheRect *next;
};

// Only the demo has these: bitmaps of animation components that are drawn as primitives
struct AnimationBitmap {
	ResRef bitmapRef;
	int layer;
	AnimationBitmap *prev;
	AnimationBitmap *next;
};

struct ActorSortInfo {
	char kind;
	float depth;
	int actorIndex;
};

class SputRender : public SputControlCallback {
	friend class LegacyImporter;

public:
	SputRender(RenderDroid *rd, StdDisplay *stdDisplay, StdBitmap *stdBitmap, Res *res, SputResource *sputResource,
			SputPrimitive *sputPrimitive, WalkSystem *walkSystem, SputRoom *sputRoom, StdVBuffer *backBuffer, StdVBuffer *mainGfxBuffer,
			SputTime *sputTime, SputControl *sputControl);

	void setActors(SputActor *actors) { _actors = actors; }
	void setSputActor(SputActorModule *sputActor) { _sputActor = sputActor; }
	void setSputText(SputText *sputText) { _sputText = sputText; }
	void setBlastText(BlastText *blastText) { _blastText = blastText; }
	void setSputSmush(SputSmush *sputSmush) { _sputSmush = sputSmush; }
	void setStdControl(StdControl *stdControl) { _stdControl = stdControl; }
	void setStd3D(Std3D *std3D) { _std3D = std3D; }
	void setGrimEngine(GrimEngine *grim) { _grim = grim; }
	void setLua(Lua_V1 *lua) { _lua = lua; }

	int initialize(SysStruct *hs);
	int terminate();
	int startup();
	int shutdown();
	int save(Common::WriteStream *stream);
	int restore(Common::ReadStream *stream);
	int open();
	void close();
	int setDisplay(StdVBuffer *vBuffer, StdVBuffer *zBuffer, int xStart, int yStart, int xEnd, int yEnd, int *options);
	int queryDisplayState();
	void using3DHardware(int b3D);
	int is3DHardware() const { return _b3DAcceleration; }
	void pageCountIs(int numPages);
	void setPaletteCallback(SputRenderPaletteFunc func);
	void setPageFlipCallback(SputRenderPageFlipFunc func);
	SputRenderPaletteFunc getPaletteFunc();
	void setPageFlip(int enable);
	int pageFlip();
	int setRenderOptions(const int *options);
	int controlCallback(float frameTime) override;
	void forceRefresh();

	void markDirtyRect(const StdRect *rectPointer, int page);
	void markAllPagesDirty(const StdRect *rectPointer, int page);
	void freeCacheRects();
	CacheRect *createCacheRect(int x, int y, int width, int height);
	void restoreCacheRect();
	void addAnimationBitmap(void *bitmap, int layer);
	void removeAnimationBitmap(void *bitmap);
	void freeBuffers();

	StdVBuffer *drawBuffer();
	StdVBuffer *cleanArtBuffer();
	StdVBuffer *cleanZBuffer();
	int copyGFXBuffer();
	int cleanDrawBuffer();
	void fillDrawBuffer(StdVBuffer *src);
	StdVBuffer *screenShot(int width, int height);
	void saveScreen(const char *path);
	void saveZBuffer(const char *path);
	void saveDrawBuffer(const char *path);

	void buildGammaTable(const StdColorFormat *format, float gamma);
	void colorCorrect(uint16 *colors, int count);
	void setGamma(float gamma);
	float getGamma() const { return _gammaCorrection; }
	void dim(int force, char shift);
	void dimRegion(int x, int y, int width, int height, float scale);

	void getIris(float percent, int x, int y);
	void irisDown(int x, int y, int duration);
	void irisUp(int x, int y, int duration);
	int irisComplete();
	void clearIris();

	void drawProgressBar(float progress);
	void allowLoadImage(int enable);
	void drawLoadingImage();

	void drawDebuggingToggle();
	void syncCamera(Room *room, int setup);
	void useSetup(Room *room, int setup);
	void update();
	int loadSetupBackground(const StdColorFormat *format);
	void preRendered(int bitmap, int zBuffer);
	void reverseProject(int x, int y, float *out);
	int buildShadowBitmap(float *out, float *pointA, float *pointB, float a, float b, float c, float d);
	void restoreDirtyRects(StdVBuffer *dst, StdVBuffer *zBuffer);
	void drawPrimitives(int layerMask);
	void drawAnimationBitmaps(int layer);
	void sortActors();
	void sortActorsDemo();
	void resetActorSortList();
	ActorSortInfo *allocActorSortEntry();
	static int compareActorDepth(const void *a, const void *b);
	void drawActor(SputActor *actor);
	void drawActorDebug(SputActor *actor);
	void drawRoomDebug();
	void drawCameraDebug(Room *room);
	void drawActorBoxes(Room *room, uint setup);
	void drawActorBox(RoomSetup *setup);
	void drawBoundingBox();
	void drawWalkPlanes();

	// Remastered only
	bool disable3DRendering();

	StdVBuffer *getCleanBuffer() const { return _cleanBuffer; }
	int getRenderFlags() const { return _renderFlags; }
	uint16 *getColors() { return _colors; }
	void setSkipPrimitives(int skip) { _skipPrimitives = skip; }
	int getCamActorIndex() const { return _camActorIndex; }
	RoomCamera *getCamera() const { return _camera; }
	const uint16 *getColorLookup() const { return _colorLookup; }

private:
	// Layers of the primitives drawn on the canvas of the remastered
	enum {
		kPrimitiveLayerOverlay = 2,
		kPrimitiveLayerMenu = 4,
		kPrimitiveLayerDialog = 8
	};

	void markDirtyRectInternal(StdRect *rect, int page, uint pageIndex);

	// Remastered only (sputrender_remastered.cpp)
	void clearSceneBuffer();
	void updateTextPage(int force, int page);
	bool updateMovieRemastered();
	void drawOverlayLayers(int mode);
	void drawLastLayers();
	void drawNoRoomRemastered();
	void drawPrimitivesCanvas(int layerMask);

	RenderDroid *_rd;
	StdDisplay *_stdDisplay;
	StdBitmap *_stdBitmap;
	Res *_res;
	SputResource *_sputResource;
	SputPrimitive *_sputPrimitive;
	WalkSystem *_walkSystem;
	SputRoom *_sputRoom;
	StdVBuffer *_backBuffer;
	StdVBuffer *_mainGfxBuffer;

	SputActorModule *_sputActor;
	SputActor *_actors;
	SputText *_sputText;
	BlastText *_blastText;
	SputSmush *_sputSmush;
	SputTime *_sputTime;
	SputControl *_sputControl;
	StdControl *_stdControl;
	Std3D *_std3D = nullptr;
	GrimEngine *_grim;
	Lua_V1 *_lua = nullptr;

	SysStruct *_hs;
	int _initialized;
	int _started;

	int _renderVars[7];
	uint16 _colorLookup[65536];
	float _gammaCorrection;
	StdColorFormat *_colorInfo;
	int _b3DAcceleration;
	int _acceleratedZBuffers;

	StdVBuffer *_drawBuffer;
	StdVBuffer *_zBuffer;
	RdCanvas *_canvas;
	StdVBuffer *_cleanBuffer;
	StdVBuffer *_cleanArtBuffer;
	StdVBuffer *_cleanZBuffer;
	int _decompressionBuffer;

	int _numPages;
	DirtyRect *_dirtyRectQueue;
	DirtyRect *_freeDirtyRects;
	int _numDirtyRects;
	int _nextSerialNumber;
	CacheRect *_cacheRectList;
	AnimationBitmap *_animationList;
	int _reset;

	int _irisStart;
	float _irisDuration;
	int _irisCenterX;
	int _irisCenterY;
	int _renderFlags;
	int _skipPrimitives;

	SputRenderPaletteFunc _setPaletteFunc;
	SputRenderPageFlipFunc _pageFlipFunc;
	int _pageFlipEnable;
	int _movieJustFinished;
	int _clearedBehindMovie;
	int _camActorIndex;

	int _debugFlags;
	int _debugKey1Down;
	int _debugKey2Down;
	int _debugKey4Down;
	int _debugKey8Down;
	int _debugKey10Down;

	int _renderEnable;
	int _loadImageAllowed;
	int _perfCounter;
	int _waitBitmapTimerMs;
	int _waitBitmapFrame;
	ResRef _waitBitmapRef;

	float _ambient;
	int _curView;
	Room *_lastRoom;
	RoomCamera *_camera;
	uint16 _colors[24];
	uint16 _dirtyRectColor;
	int _printLuaStack;
	int _numSortActors;
	ActorSortInfo _actorSortInfo[128];
	ActorSortInfo *_sortedActors[128];
};

} // End of namespace Grim

#endif
