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

	int create(const Common::String &name, int x, int y, const OverlayProps &props);
	void move(int handle, int x, int y);
	void setLayer(int handle, int layer);
	void setHidden(int handle, int hidden);
	void destroy(int handle);
	void getDimensions(int handle, int *width, int *height);
	void clearCache();
	void fade(int handle, int type, bool hasValue, float value);
	void showLoadSpinner(bool show, int x, int y);

	// Queues the overlays of a layer in the canvas and steps their fades
	void render(int layer, int boxing);

private:
	enum {
		kHandleCount = 1024,
		kLargeImagePixels = 40001,
		kMuralWidth = 32,
		kMuralHeight = 10,
		kMuralPuzzles = 60
	};

	struct CachedImage {
		Common::String name;
		int refCount;
		int cacheMode;
		byte *pixels;
		int width;
		int height;
		bool loadAllowed;
		uint32 lastUse;
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
	int nextHandle();
	CachedImage *findImage(const Common::String &name) const;
	CachedImage *acquireImage(const Common::String &name, int cacheMode);
	void ensureLoaded(CachedImage *image);
	bool loadPng(CachedImage *image);
	void buildMural(CachedImage *image);
	void freePixels(CachedImage *image);
	void deleteImage(CachedImage *image);
	void drawOverlay(const Overlay &overlay);
	bool stepFade(Overlay &overlay);

	HdCanvas *_canvas;
	Common::Array<Overlay> _overlays;
	Common::Array<CachedImage *> _images;
	int _serial;
	int _spinnerHandle;
	uint32 _useCounter;
};

} // End of namespace Grim

#endif
