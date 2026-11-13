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
#include "common/textconsole.h"

#include "graphics/palette.h"
#include "graphics/surface.h"

#include "image/png.h"

#include "engines/grim/grim.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/overlay.h"

#include <math.h>

namespace Grim {

enum {
	kMuralDarkAlpha = 204
};

// Steps of the fades, per rendered frame
static const float kFadeStep = 1.0f / 18.0f;
static const float kFadeThreshold = 1.0f / 12.0f;
static const float kGrayAlphaStep = 1.0f / 60.0f;
static const float kGrayDesaturateStep = 1.0f / 30.0f;
static const float kGrayAlphaTarget = 0.7f;
static const float kGrayAlphaThreshold = 0.025f;
static const float kGrayDesaturateTarget = 0.6f;
static const float kGrayDesaturateThreshold = 0.05f;

// The pixels of the mural (32x10) that belong to each of the 60 puzzles, as y * 32 + x, ended by -1
static const short muralPixels[60][10] = {
	{ 0, 1, 2, 32, 33, 64, -1, -1, -1, -1 },
	{ 34, 65, 66, 96, 97, -1, -1, -1, -1, -1 },
	{ 67, 98, 99, 130, 131, -1, -1, -1, -1, -1 },
	{ 3, 4, 5, 35, 36, -1, -1, -1, -1, -1 },
	{ 128, 129, 160, 161, 162, -1, -1, -1, -1, -1 },
	{ 6, 37, 38, 68, 69, -1, -1, -1, -1, -1 },
	{ 70, 100, 101, 102, 132, -1, -1, -1, -1, -1 },
	{ 7, 8, 39, 40, 71, -1, -1, -1, -1, -1 },
	{ 72, 103, 104, 133, 134, -1, -1, -1, -1, -1 },
	{ 192, 193, 224, 225, 256, -1, -1, -1, -1, -1 },
	{ 257, 258, 288, 289, 290, -1, -1, -1, -1, -1 },
	{ 163, 194, 195, 226, 227, -1, -1, -1, -1, -1 },
	{ 228, 259, 260, 291, 292, -1, -1, -1, -1, -1 },
	{ 164, 165, 196, 197, 198, -1, -1, -1, -1, -1 },
	{ 229, 230, 261, 262, 293, -1, -1, -1, -1, -1 },
	{ 135, 136, 166, 167, 168, -1, -1, -1, -1, -1 },
	{ 199, 200, 231, 232, 263, -1, -1, -1, -1, -1 },
	{ 264, 294, 295, 296, 297, -1, -1, -1, -1, -1 },
	{ 137, 138, 139, 169, 170, -1, -1, -1, -1, -1 },
	{ 171, 172, 203, 204, 205, -1, -1, -1, -1, -1 },
	{ 201, 202, 233, 234, 265, -1, -1, -1, -1, -1 },
	{ 41, 235, 266, 267, 298, 299, -1, -1, -1, -1 },
	{ 9, 10, 11, 42, -1, -1, -1, -1, -1, -1 },
	{ 73, 74, 75, 105, 106, -1, -1, -1, -1, -1 },
	{ 12, 13, 43, 44, 76, -1, -1, -1, -1, -1 },
	{ 77, 107, 108, 109, 140, -1, -1, -1, -1, -1 },
	{ 78, 110, 141, 142, 173, -1, -1, -1, -1, -1 },
	{ 14, 45, 46, 47, 79, -1, -1, -1, -1, -1 },
	{ 15, 16, 48, 49, 80, -1, -1, -1, -1, -1 },
	{ 81, 111, 112, 113, 144, -1, -1, -1, -1, -1 },
	{ 143, 174, 175, 176, 207, -1, -1, -1, -1, -1 },
	{ 206, 236, 237, 238, 268, 300, -1, -1, -1, -1 },
	{ 269, 270, 271, 301, 302, -1, -1, -1, -1, -1 },
	{ 239, 240, 272, 273, 303, -1, -1, -1, -1, -1 },
	{ 17, 18, 19, 50, 82, -1, -1, -1, -1, -1 },
	{ 114, 145, 146, 177, 178, -1, -1, -1, -1, -1 },
	{ 208, 209, 241, 242, 274, -1, -1, -1, -1, -1 },
	{ 275, 304, 305, 306, 307, -1, -1, -1, -1, -1 },
	{ 210, 211, 243, 244, 276, 308, -1, -1, -1, -1 },
	{ 179, 180, 212, 213, 245, -1, -1, -1, -1, -1 },
	{ 277, 278, 309, 310, 311, -1, -1, -1, -1, -1 },
	{ 51, 52, 53, 84, 85, -1, -1, -1, -1, -1 },
	{ 83, 115, 116, 117, 147, -1, -1, -1, -1, -1 },
	{ 86, 118, 148, 149, 150, -1, -1, -1, -1, -1 },
	{ 20, 21, 22, 54, 55, -1, -1, -1, -1, -1 },
	{ 181, 182, 183, 214, 215, 246, -1, -1, -1, -1 },
	{ 23, 24, 25, 26, 56, 57, -1, -1, -1, -1 },
	{ 87, 88, 89, 119, 120, 121, -1, -1, -1, -1 },
	{ 27, 28, 58, 59, 60, 91, -1, -1, -1, -1 },
	{ 151, 152, 153, 154, 184, 185, -1, -1, -1, -1 },
	{ 216, 247, 248, 279, 280, -1, -1, -1, -1, -1 },
	{ 281, 282, 312, 313, 314, -1, -1, -1, -1, -1 },
	{ 217, 218, 249, 250, 251, 283, -1, -1, -1, -1 },
	{ 252, 284, 285, 315, 316, 317, -1, -1, -1, -1 },
	{ 90, 92, 93, 122, 123, 124, -1, -1, -1, -1 },
	{ 29, 30, 31, 61, 62, 63, -1, -1, -1, -1 },
	{ 155, 156, 186, 187, 188, 219, -1, -1, -1, -1 },
	{ 189, 220, 221, 222, 253, 254, -1, -1, -1, -1 },
	{ 94, 95, 125, 126, 127, 157, 158, -1, -1, -1 },
	{ 159, 190, 191, 223, 255, 286, 287, 318, 319, -1 }
};

OverlayManager::OverlayManager(HdCanvas *canvas) : _canvas(canvas), _serial(0), _spinnerHandle(kNoHandle), _useCounter(0) {
}

OverlayManager::~OverlayManager() {
	for (uint i = 0; i < _images.size(); i++) {
		delete[] _images[i]->pixels;
		delete _images[i];
	}

	_images.clear();
}

int OverlayManager::findOverlay(int handle) const {
	for (uint i = 0; i < _overlays.size(); i++) {
		if (_overlays[i].handle == handle)
			return (int)i;
	}

	return -1;
}

// The handles go from 0 to 1023 and the ones in use are skipped
int OverlayManager::nextHandle() {
	for (int i = 0; i < kHandleCount; i++) {
		_serial = (_serial + 1) % kHandleCount;

		if (findOverlay(_serial) < 0)
			return _serial;
	}

	return kNoHandle;
}

OverlayManager::CachedImage *OverlayManager::findImage(const Common::String &name) const {
	for (uint i = 0; i < _images.size(); i++) {
		if (_images[i]->name == name)
			return _images[i];
	}

	return nullptr;
}

OverlayManager::CachedImage *OverlayManager::acquireImage(const Common::String &name, int cacheMode) {
	CachedImage *image = findImage(name);

	if (image) {
		image->lastUse = ++_useCounter;
		return image;
	}

	if ((int)_images.size() >= kMaxImages) {
		// The least recently used image that nobody references and that is not persistent goes away
		CachedImage *oldest = nullptr;

		for (uint i = 0; i < _images.size(); i++) {
			CachedImage *candidate = _images[i];

			if (candidate->refCount == 0 && candidate->cacheMode != kCachePersistent && (!oldest || candidate->lastUse < oldest->lastUse))
				oldest = candidate;
		}

		if (!oldest) {
			warning("OverlayManager::acquireImage(): the image cache is full");
			return nullptr;
		}

		deleteImage(oldest);
	}

	image = new CachedImage();
	image->name = name;
	image->refCount = 0;
	image->cacheMode = cacheMode;
	image->pixels = nullptr;
	image->width = 0;
	image->height = 0;
	image->loadAllowed = true;
	image->lastUse = ++_useCounter;
	_images.push_back(image);

	return image;
}

void OverlayManager::deleteImage(CachedImage *image) {
	for (uint i = 0; i < _images.size(); i++) {
		if (_images[i] == image) {
			_images.remove_at(i);
			break;
		}
	}

	freePixels(image);
	delete image;
}

void OverlayManager::freePixels(CachedImage *image) {
	if (!image->pixels)
		return;

	if (_canvas)
		_canvas->dropPixels(image->pixels);

	delete[] image->pixels;
	image->pixels = nullptr;
}

void OverlayManager::ensureLoaded(CachedImage *image) {
	if (image->pixels || !image->loadAllowed)
		return;

	bool loaded = false;

	if (image->name.hasSuffixIgnoreCase(".png")) {
		loaded = loadPng(image);
	} else if (image->name.hasSuffixIgnoreCase(".save")) {
		buildMural(image);
		loaded = true;
	} else {
		warning("OverlayManager::ensureLoaded(): unknown image file type for %s", image->name.c_str());
	}

	if (!loaded) {
		image->loadAllowed = false;
		image->width = 0;
		image->height = 0;
		return;
	}

	// The big images are not kept after their last overlay is gone
	if (image->cacheMode == kCacheDefault && image->width * image->height >= kLargeImagePixels)
		image->cacheMode = kCacheNever;
}

bool OverlayManager::loadPng(CachedImage *image) {
	Common::SeekableReadStream *stream = g_grim->openFile(image->name.c_str());

	if (!stream) {
		warning("OverlayManager::loadPng(): the image %s was not found", image->name.c_str());
		return false;
	}

	Image::PNGDecoder decoder;
	bool decoded = decoder.loadStream(*stream);

	delete stream;

	const Graphics::Surface *surface = decoder.getSurface();

	if (!decoded || !surface || surface->w <= 0 || surface->h <= 0) {
		warning("OverlayManager::loadPng(): could not decode the image %s", image->name.c_str());
		return false;
	}

	int width = surface->w;
	int height = surface->h;
	int bytesPerPixel = surface->format.bytesPerPixel;

	if (bytesPerPixel != 1 && bytesPerPixel != 3 && bytesPerPixel != 4) {
		warning("OverlayManager::loadPng(): the image %s has an unsupported pixel format", image->name.c_str());
		return false;
	}

	const Graphics::Palette &palette = decoder.getPalette();
	byte *pixels = new byte[width * height * 4];

	for (int y = 0; y < height; y++) {
		const byte *source = (const byte *)surface->getBasePtr(0, y);
		byte *destination = pixels + y * width * 4;

		for (int x = 0; x < width; x++) {
			int red = 0;
			int green = 0;
			int blue = 0;
			int alpha = 255;

			if (bytesPerPixel == 1) {
				byte paletteRed = 0;
				byte paletteGreen = 0;
				byte paletteBlue = 0;

				if (source[x] < palette.size())
					palette.get(source[x], paletteRed, paletteGreen, paletteBlue);

				red = paletteRed;
				green = paletteGreen;
				blue = paletteBlue;
			} else {
				// The decoder gives the bytes in the order red, green, blue, alpha
				red = source[x * bytesPerPixel];
				green = source[x * bytesPerPixel + 1];
				blue = source[x * bytesPerPixel + 2];

				if (bytesPerPixel == 4)
					alpha = source[x * bytesPerPixel + 3];
			}

			destination[0] = (byte)((red * alpha + 127) / 255);
			destination[1] = (byte)((green * alpha + 127) / 255);
			destination[2] = (byte)((blue * alpha + 127) / 255);
			destination[3] = (byte)alpha;
			destination += 4;
		}
	}

	image->pixels = pixels;
	image->width = width;
	image->height = height;

	return true;
}

// The name is one character per puzzle: '0' when it is not solved, which darkens its tiles of the mural
void OverlayManager::buildMural(CachedImage *image) {
	byte *pixels = new byte[kMuralWidth * kMuralHeight * 4]();
	int numPuzzles = (int)image->name.size() - 5;

	if (numPuzzles > kMuralPuzzles)
		numPuzzles = kMuralPuzzles;

	for (int i = 0; i < numPuzzles; i++) {
		if (image->name[i] != '0')
			continue;

		for (int j = 0; j < 10 && muralPixels[i][j] >= 0; j++)
			pixels[muralPixels[i][j] * 4 + 3] = kMuralDarkAlpha;
	}

	image->pixels = pixels;
	image->width = kMuralWidth;
	image->height = kMuralHeight;
}

int OverlayManager::create(const Common::String &name, int x, int y, const OverlayProps &props) {
	if ((int)_overlays.size() >= kMaxOverlays) {
		warning("OverlayManager::create(): too many overlays");
		return kNoHandle;
	}

	CachedImage *image = acquireImage(name, props.cache);

	if (!image)
		return kNoHandle;

	int handle = nextHandle();

	if (handle == kNoHandle)
		return kNoHandle;

	image->refCount++;

	Overlay overlay;

	overlay.image = image;
	overlay.handle = handle;
	overlay.x = x;
	overlay.y = y;
	overlay.widthOverride = props.width;
	overlay.heightOverride = props.height;
	overlay.alpha = 1.0f;
	overlay.desaturate = 0.0f;
	overlay.flags = props.clearBuffer ? 1 : 0;
	overlay.coords = props.coords;
	overlay.layer = props.layer;
	overlay.mirror = props.mirror;
	overlay.hidden = 0;
	overlay.fade = kFadeNone;
	_overlays.push_back(overlay);

	return handle;
}

void OverlayManager::move(int handle, int x, int y) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	ensureLoaded(_overlays[index].image);
	_overlays[index].x = x;
	_overlays[index].y = y;
}

void OverlayManager::setLayer(int handle, int layer) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	ensureLoaded(_overlays[index].image);
	_overlays[index].layer = layer;
}

void OverlayManager::setHidden(int handle, int hidden) {
	int index = findOverlay(handle);

	if (index >= 0)
		_overlays[index].hidden = hidden;
}

void OverlayManager::destroy(int handle) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	CachedImage *image = _overlays[index].image;

	image->refCount--;

	if (image->cacheMode == kCacheNever && image->refCount == 0)
		freePixels(image);

	_overlays.remove_at(index);
}

// A width or height override wins over the size of the image, an unknown handle has no size
void OverlayManager::getDimensions(int handle, int *width, int *height) {
	*width = 0;
	*height = 0;

	int index = findOverlay(handle);

	if (index < 0)
		return;

	const Overlay &overlay = _overlays[index];

	ensureLoaded(overlay.image);
	*width = overlay.widthOverride > 0 ? overlay.widthOverride : overlay.image->width;
	*height = overlay.heightOverride > 0 ? overlay.heightOverride : overlay.image->height;
}

void OverlayManager::clearCache() {
	for (uint i = 0; i < _images.size(); i++) {
		CachedImage *image = _images[i];

		if (image->refCount == 0 && image->cacheMode != kCachePersistent)
			freePixels(image);
	}
}

void OverlayManager::fade(int handle, int type, bool hasValue, float value) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	Overlay &overlay = _overlays[index];

	ensureLoaded(overlay.image);
	overlay.fade = type;

	if (type == kFadeSet) {
		if (hasValue)
			overlay.alpha = value;
	} else if (type == kFadeIn) {
		overlay.alpha = 0.0f;
	}
}

void OverlayManager::showLoadSpinner(bool show, int x, int y) {
	if (_spinnerHandle != kNoHandle) {
		destroy(_spinnerHandle);
		_spinnerHandle = kNoHandle;
	}

	if (!show)
		return;

	OverlayProps props;

	props.layer = 5;
	props.coords = kCoordsGlobal;
	props.cache = kCachePersistent;
	_spinnerHandle = create("save_wait_icon.png", x, y, props);

	if (_spinnerHandle != kNoHandle) {
		int width = 0;
		int height = 0;

		getDimensions(_spinnerHandle, &width, &height);
		move(_spinnerHandle, x - width / 2, y - height / 2);
	}
}

void OverlayManager::drawOverlay(const Overlay &overlay) {
	int x = overlay.x;
	int y = overlay.y;

	switch (overlay.coords) {
	case kCoordsLegacy:
		x = 1440 * x / 640 + 240;
		y = 1080 * y / 480;
		break;
	case kCoordsGameScreen:
		x += 240;
		break;
	default:
		break;
	}

	if (overlay.flags & 1)
		_canvas->clearBuffer();

	CachedImage *image = overlay.image;

	if (!image->pixels)
		return;

	int width = overlay.widthOverride > 0 ? overlay.widthOverride : image->width;
	int height = overlay.heightOverride > 0 ? overlay.heightOverride : image->height;

	_canvas->queueOverlay(overlay.layer, image->pixels, image->width, image->height, x, y, width, height,
			overlay.mirror, overlay.alpha, overlay.desaturate);
}

// Moves the fade of an overlay one frame on, true when the overlay has to be destroyed
bool OverlayManager::stepFade(Overlay &overlay) {
	switch (overlay.fade) {
	case kFadeIn:
		overlay.alpha += kFadeStep;

		if (fabsf(overlay.alpha - 1.0f) < kFadeThreshold) {
			overlay.alpha = 1.0f;
			overlay.fade = kFadeNone;
		}

		break;
	case kFadeOut:
	case kFadeOutDelete:
		overlay.alpha -= kFadeStep;

		if (fabsf(overlay.alpha) < kFadeThreshold) {
			overlay.alpha = 0.0f;

			if (overlay.fade == kFadeOutDelete)
				return true;

			overlay.fade = kFadeNone;
		}

		break;
	case kFadeToGray:
		if (fabsf(overlay.alpha - kGrayAlphaTarget) >= kGrayAlphaThreshold)
			overlay.alpha += overlay.alpha < kGrayAlphaTarget ? kGrayAlphaStep : -kGrayAlphaStep;
		else if (fabsf(overlay.desaturate - kGrayDesaturateTarget) >= kGrayDesaturateThreshold)
			overlay.desaturate += overlay.desaturate < kGrayDesaturateTarget ? kGrayDesaturateStep : -kGrayDesaturateStep;
		else
			overlay.fade = kFadeNone;

		break;
	case kFadeFromGray:
		if (fabsf(overlay.alpha - 1.0f) >= kGrayAlphaThreshold)
			overlay.alpha += overlay.alpha < 1.0f ? kGrayAlphaStep : -kGrayAlphaStep;
		else if (fabsf(overlay.desaturate) >= kGrayDesaturateThreshold)
			overlay.desaturate += overlay.desaturate < 0.0f ? kGrayDesaturateStep : -kGrayDesaturateStep;
		else
			overlay.fade = kFadeNone;

		break;
	default:
		break;
	}

	return false;
}

// The boxing mode only matters when the output is not 1920x1080 or the widescreen is stretched: neither happens here
void OverlayManager::render(int layer, int boxing) {
	Common::Array<int> toDestroy;

	for (uint i = 0; i < _overlays.size(); i++) {
		Overlay &overlay = _overlays[i];

		if (overlay.layer != layer || overlay.hidden == 2 || !overlay.image)
			continue;

		ensureLoaded(overlay.image);
		overlay.image->lastUse = ++_useCounter;
		drawOverlay(overlay);

		if (stepFade(overlay))
			toDestroy.push_back(overlay.handle);
	}

	for (uint i = 0; i < toDestroy.size(); i++)
		destroy(toDestroy[i]);
}

} // End of namespace Grim
