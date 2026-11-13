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

#ifndef GRIM_OVERLAY_H
#define GRIM_OVERLAY_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class HdCanvas;

// The properties of OverlayCreate
struct OverlayProps {
	int cache = 0;
	int layer = 1;
	int coords = 1;
	bool clearBuffer = false;
	int mirror = 0;
	int width = -1;
	int height = -1;
};

/**
 * The images the scripts put on the 1920x1080 canvas (menus, buttons, cursors, fades).
 * An overlay is a handle with a position, a layer and a fade state; the images behind
 * the handles are shared and cached by name. They are loaded when first needed, from a
 * PNG file or, for the names ending in ".save", built as the mural of a savegame.
 */
class OverlayManager {
public:
	enum {
		kMaxOverlays = 1023,
		kMaxImages = 128,
		kNoHandle = -1
	};

	enum CacheMode {
		kCacheDefault = 0,
		kCachePersistent = 1,
		kCacheNever = 2
	};

	enum FadeType {
		kFadeNone = 0,
		kFadeIn = 1,
		kFadeOut = 2,
		kFadeOutDelete = 3,
		kFadeSet = 4,
		kFadeToGray = 5,
		kFadeFromGray = 6
	};

	enum CoordsMode {
		kCoordsLegacy = 0,
		kCoordsGameScreen = 1,
		kCoordsGlobal = 2
	};

	explicit OverlayManager(HdCanvas *canvas);
	~OverlayManager();

	// Back to the state of a new manager: no image and no overlay
	void reset();

	// An empty name makes an overlay without image. The image is loaded at once.
	int create(const Common::String &name, int x, int y, const OverlayProps &props);
	void move(int handle, int x, int y);
	void setLayer(int handle, int layer);
	void setHidden(int handle, int hidden);
	void destroy(int handle);
	void getDimensions(int handle, int *width, int *height);
	void clearCache();
	void fade(int handle, int type, bool hasValue, float value);
	void showLoadSpinner(bool show, int x, int y);

	// The cursors and the hotspots reach the entries and the cache of the images directly: the image of
	// an entry is replaced without a reference count, an unknown handle does nothing
	bool loadCached(const Common::String &name, int cacheMode);
	void setImage(int handle, const Common::String &name);
	bool getLayer(int handle, int *layer);
	bool hasImage(int handle);
	void preloadImage(const Common::String &name);

	// Queues the overlays of a layer in the canvas and steps their fades
	void render(int layer, int boxing);

	// The block of the overlays in the savegames, the images are loaded again when they are needed
	int save(Common::WriteStream *stream) const;
	int restore(Common::ReadStream *stream);

	enum {
		kMuralWidth = 32,
		kMuralHeight = 10,
		kMuralPuzzles = 60
	};

	// Fills the pixels (RGBA, kMuralWidth x kMuralHeight) of the mural: a '0' puzzle darkens its tiles
	static void renderMural(const char *puzzles, int numPuzzles, byte *pixels);

private:
	enum {
		kHandleCount = 1024,
		kLargeImagePixels = 40001,
		kSavedNameSize = 128
	};

	// lastHandle is the handle of the last overlay created with the image: the cache drops the
	// smallest one first
	struct CachedImage {
		Common::String name;
		int refCount;
		int cacheMode;
		byte *pixels;
		int width;
		int height;
		bool loadAllowed;
		int lastHandle;
	};

	struct Overlay {
		CachedImage *image;
		int handle;
		int x;
		int y;
		int widthOverride;
		int heightOverride;
		float alpha;
		float desaturate;
		int flags;
		int coords;
		int layer;
		int mirror;
		int hidden;
		int fade;
	};

	int findOverlay(int handle) const;
	Overlay *fromHandle(int handle);
	CachedImage *acquireImage(const Common::String &name);
	void ensureLoaded(CachedImage *image);
	bool loadPng(CachedImage *image);
	void buildMural(CachedImage *image);
	void freePixels(CachedImage *image);
	void releaseImage(CachedImage *image);
	void removeOverlay(int index);
	void drawOverlay(const Overlay &overlay, int boxing);
	bool stepFade(Overlay &overlay);

	HdCanvas *_canvas;

	// The slots are kept like the original keeps them: the entries after the last one hold what the
	// compaction left there, and the handles are chosen with them in mind
	Overlay _overlays[kMaxOverlays];
	int _numOverlays;
	CachedImage *_images[kMaxImages];
	int _numImages;
	int _serial;
	int _spinnerHandle;
};

} // End of namespace Grim

#endif
