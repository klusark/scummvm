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
#include <string.h>

namespace Grim {

enum {
	kMuralDarkAlpha = 204
};

// The thresholds of the gray fades are the floats next to 0.025 and 0.05
static const float kFadeStep = 1.0f / 18.0f;
static const float kFadeThreshold = 1.0f / 12.0f;
static const float kGrayAlphaStep = 1.0f / 60.0f;
static const float kGrayDesaturateStep = 1.0f / 30.0f;
static const float kGrayAlphaTarget = 0.7f;
static const float kGrayAlphaThreshold = 0.025000002f;
static const float kGrayDesaturateTarget = 0.6f;
static const float kGrayDesaturateThreshold = 0.050000004f;

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

OverlayManager::OverlayManager(HdCanvas *canvas) : _canvas(canvas), _numOverlays(0), _numImages(0), _serial(0),
		_spinnerHandle(kNoHandle) {
	memset(_overlays, 0, sizeof(_overlays));
	memset(_images, 0, sizeof(_images));
}

OverlayManager::~OverlayManager() {
	for (int i = 0; i < _numImages; i++) {
		delete[] _images[i]->pixels;
		delete _images[i];
		_images[i] = nullptr;
	}

	_numImages = 0;
}

void OverlayManager::reset() {
	for (int i = 0; i < _numImages; i++) {
		freePixels(_images[i]);
		delete _images[i];
		_images[i] = nullptr;
	}

	_numImages = 0;
	_numOverlays = 0;
	_serial = 0;
	_spinnerHandle = kNoHandle;
	memset(_overlays, 0, sizeof(_overlays));
}

int OverlayManager::findOverlay(int handle) const {
	for (int i = 0; i < _numOverlays; i++) {
		if (_overlays[i].handle == handle)
			return i;
	}

	return -1;
}

OverlayManager::Overlay *OverlayManager::fromHandle(int handle) {
	int index = findOverlay(handle);

	if (index < 0)
		return nullptr;

	if (_overlays[index].image)
		ensureLoaded(_overlays[index].image);

	return &_overlays[index];
}

OverlayManager::CachedImage *OverlayManager::acquireImage(const Common::String &name) {
	int victim = -1;

	for (int i = 0; i < _numImages; i++) {
		CachedImage *image = _images[i];

		if (image->name == name)
			return image;

		if (image->refCount == 0 && image->cacheMode != kCachePersistent && (victim < 0 || image->lastHandle < _images[victim]->lastHandle))
			victim = i;
	}

	int slot = _numImages;

	if (_numImages == kMaxImages) {
		if (victim < 0) {
			warning("OverlayManager::acquireImage(): the image cache is full");
			victim = 0;
		}

		releaseImage(_images[victim]);
		slot = victim;
	} else {
		_numImages++;
	}

	if (name.size() >= kSavedNameSize)
		warning("OverlayManager::acquireImage(): the file name is too long");

	CachedImage *image = new CachedImage();

	image->name = name;
	image->refCount = 0;
	image->cacheMode = kCacheDefault;
	image->pixels = nullptr;
	image->width = 0;
	image->height = 0;
	image->loadAllowed = true;
	image->lastHandle = 0;
	_images[slot] = image;

	return image;
}

void OverlayManager::releaseImage(CachedImage *image) {
	freePixels(image);
	image->refCount = 0;
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
	if (image->pixels || image->name.empty() || !image->loadAllowed)
		return;

	size_t length = image->name.size();

	if (length >= 4 && image->name.hasSuffix(".png")) {
		if (!loadPng(image)) {
			image->loadAllowed = false;
			return;
		}
	} else if (length >= 5 && image->name.hasSuffix(".save")) {
		buildMural(image);
	} else {
		warning("OverlayManager::ensureLoaded(): unknown image file type for %s", image->name.c_str());
	}

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
				red = source[x * bytesPerPixel];
				green = source[x * bytesPerPixel + 1];
				blue = source[x * bytesPerPixel + 2];

				if (bytesPerPixel == 4)
					alpha = source[x * bytesPerPixel + 3];
			}

			destination[0] = (byte)((red * alpha) / 255);
			destination[1] = (byte)((green * alpha) / 255);
			destination[2] = (byte)((blue * alpha) / 255);
			destination[3] = (byte)alpha;
			destination += 4;
		}
	}

	image->pixels = pixels;
	image->width = width;
	image->height = height;

	return true;
}

void OverlayManager::buildMural(CachedImage *image) {
	byte *pixels = new byte[kMuralWidth * kMuralHeight * 4]();

	renderMural(image->name.c_str(), (int)image->name.size() - 5, pixels);

	image->pixels = pixels;
	image->width = kMuralWidth;
	image->height = kMuralHeight;
}

void OverlayManager::renderMural(const char *puzzles, int numPuzzles, byte *pixels) {
	if (numPuzzles > kMuralPuzzles)
		numPuzzles = kMuralPuzzles;

	for (int i = 0; i < numPuzzles; i++) {
		if (puzzles[i] != '0')
			continue;

		for (int j = 0; j < 10 && muralPixels[i][j] >= 0; j++)
			pixels[muralPixels[i][j] * 4 + 3] = kMuralDarkAlpha;
	}
}

int OverlayManager::save(Common::WriteStream *stream) const {
	byte firewall[24];

	memset(firewall, 0xFF, sizeof(firewall));

	stream->writeSint32LE(_numOverlays);
	stream->writeSint32LE(_numImages);
	stream->writeSint32LE(_serial);
	stream->write(firewall, 24);

	for (int i = 0; i < _numImages; i++) {
		const CachedImage *image = _images[i];
		char name[kSavedNameSize];

		memset(name, 0, sizeof(name));
		Common::strlcpy(name, image->name.c_str(), sizeof(name));
		stream->write(name, sizeof(name));
		stream->writeSint32LE(image->lastHandle);
		stream->writeSint32LE(image->refCount);
		stream->writeSint32LE(image->loadAllowed ? 1 : 0);
		stream->writeSint32LE(image->cacheMode);
		stream->write(firewall, 24);
	}

	for (int i = 0; i < _numOverlays; i++) {
		const Overlay &overlay = _overlays[i];
		int imageIndex = -1;

		for (int j = 0; j < _numImages; j++) {
			if (_images[j] == overlay.image) {
				imageIndex = j;
				break;
			}
		}

		stream->writeSint32LE(imageIndex);
		stream->writeSint32LE(overlay.handle);
		stream->writeSint32LE(overlay.x);
		stream->writeSint32LE(overlay.y);
		stream->writeSint32LE(overlay.coords);
		stream->writeSint32LE(overlay.layer);
		stream->writeSint32LE(overlay.widthOverride);
		stream->writeSint32LE(overlay.heightOverride);
		stream->writeSint32LE(overlay.hidden);
		stream->writeFloatLE(overlay.alpha);
		stream->writeSint32LE(overlay.fade);
		stream->writeSint32LE(overlay.flags);
		stream->writeFloatLE(overlay.desaturate);
		stream->writeSint32LE(overlay.mirror);
		stream->write(firewall, 12);
	}

	if (stream->err()) {
		warning("OverlayManager::save(): failed to write the overlays");
		return 1;
	}

	return 0;
}

int OverlayManager::restore(Common::ReadStream *stream) {
	byte firewall[24];

	int numOverlays = stream->readSint32LE();
	int numImages = stream->readSint32LE();
	int serial = stream->readSint32LE();

	stream->read(firewall, 24);

	if (stream->err() || stream->eos() || numOverlays < 0 || numOverlays > kMaxOverlays || numImages < 0 ||
			numImages > kMaxImages || serial < 0 || serial >= kHandleCount) {
		warning("OverlayManager::restore(): invalid overlay counts %d and %d", numOverlays, numImages);
		return 1;
	}

	for (int i = 0; i < _numImages; i++) {
		freePixels(_images[i]);
		delete _images[i];
		_images[i] = nullptr;
	}

	_numImages = 0;
	_numOverlays = 0;
	memset(_overlays, 0, sizeof(_overlays));
	_spinnerHandle = kNoHandle;
	_serial = serial;

	for (int i = 0; i < numImages; i++) {
		char name[kSavedNameSize];
		CachedImage *image = new CachedImage();

		stream->read(name, sizeof(name));
		name[sizeof(name) - 1] = '\0';

		image->name = name;
		image->lastHandle = stream->readSint32LE();
		image->refCount = stream->readSint32LE();
		image->loadAllowed = stream->readSint32LE() != 0;
		image->cacheMode = stream->readSint32LE();
		image->pixels = nullptr;
		image->width = 0;
		image->height = 0;

		stream->read(firewall, 24);
		_images[_numImages++] = image;
	}

	for (int i = 0; i < numOverlays; i++) {
		Overlay overlay;
		int imageIndex = stream->readSint32LE();

		overlay.handle = stream->readSint32LE();
		overlay.x = stream->readSint32LE();
		overlay.y = stream->readSint32LE();
		overlay.coords = stream->readSint32LE();
		overlay.layer = stream->readSint32LE();
		overlay.widthOverride = stream->readSint32LE();
		overlay.heightOverride = stream->readSint32LE();
		overlay.hidden = stream->readSint32LE();
		overlay.alpha = stream->readFloatLE();
		overlay.fade = stream->readSint32LE();
		overlay.flags = stream->readSint32LE();
		overlay.desaturate = stream->readFloatLE();
		overlay.mirror = stream->readSint32LE();
		stream->read(firewall, 12);

		if (stream->err() || stream->eos()) {
			warning("OverlayManager::restore(): failed to read the overlay %d of %d", i + 1, numOverlays);
			return 1;
		}

		if (imageIndex >= _numImages) {
			warning("OverlayManager::restore(): the overlay %d has an unknown image", overlay.handle);
			imageIndex = -1;
		}

		if (overlay.flags == -1)
			overlay.flags = 0;

		overlay.image = imageIndex >= 0 ? _images[imageIndex] : nullptr;
		_overlays[_numOverlays++] = overlay;
	}

	return 0;
}

// The handle is the next number that no slot has, the slot after the last one included
int OverlayManager::create(const Common::String &name, int x, int y, const OverlayProps &props) {
	if (_numOverlays >= kMaxOverlays) {
		warning("OverlayManager::create(): too many overlays");
		return kNoHandle;
	}

	CachedImage *image = nullptr;

	if (!name.empty())
		image = acquireImage(name);

	int slot = _numOverlays++;
	Overlay *overlay = &_overlays[slot];

	overlay->image = image;
	overlay->x = x;
	overlay->y = y;
	overlay->widthOverride = -1;
	overlay->heightOverride = -1;
	overlay->coords = props.coords;
	overlay->alpha = 1.0f;
	overlay->desaturate = 0.0f;
	overlay->fade = kFadeNone;
	overlay->layer = props.layer;
	overlay->hidden = 0;
	overlay->flags = props.clearBuffer ? 1 : 0;

	int handle = _serial;
	bool used = true;

	while (used) {
		handle = (handle + 1) % kHandleCount;
		used = false;

		for (int i = 0; i < _numOverlays; i++) {
			if (_overlays[i].handle == handle) {
				used = true;
				break;
			}
		}
	}

	_serial = handle;
	overlay->handle = handle;
	overlay->mirror = props.mirror;

	if (image) {
		image->cacheMode = props.cache;
		image->refCount++;
		image->lastHandle = handle;

		ensureLoaded(image);
	}

	overlay->widthOverride = props.width;
	overlay->heightOverride = props.height;

	return handle;
}

void OverlayManager::move(int handle, int x, int y) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	if (_overlays[index].image)
		ensureLoaded(_overlays[index].image);

	_overlays[index].x = x;
	_overlays[index].y = y;
}

void OverlayManager::setLayer(int handle, int layer) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	if (_overlays[index].image)
		ensureLoaded(_overlays[index].image);

	_overlays[index].layer = layer;
}

void OverlayManager::setHidden(int handle, int hidden) {
	int index = findOverlay(handle);

	if (index >= 0)
		_overlays[index].hidden = hidden;
}

void OverlayManager::removeOverlay(int index) {
	for (int i = index + 1; i < _numOverlays; i++)
		_overlays[i - 1] = _overlays[i];

	_numOverlays--;
}

void OverlayManager::destroy(int handle) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	CachedImage *image = _overlays[index].image;

	if (image) {
		image->refCount--;

		if (image->cacheMode == kCacheNever && image->refCount == 0) {
			freePixels(image);
			image->refCount = 0;
		}
	}

	removeOverlay(index);
}

void OverlayManager::getDimensions(int handle, int *width, int *height) {
	*width = 0;
	*height = 0;

	int index = findOverlay(handle);

	if (index < 0)
		return;

	const Overlay &overlay = _overlays[index];
	int imageWidth = 0;
	int imageHeight = 0;

	if (overlay.image) {
		ensureLoaded(overlay.image);
		imageWidth = overlay.image->width;
		imageHeight = overlay.image->height;
	}

	*width = overlay.widthOverride > 0 ? overlay.widthOverride : imageWidth;
	*height = overlay.heightOverride > 0 ? overlay.heightOverride : imageHeight;
}

void OverlayManager::clearCache() {
	for (int i = 0; i < _numImages; i++) {
		CachedImage *image = _images[i];

		if (image->refCount == 0 && image->cacheMode != kCachePersistent) {
			freePixels(image);
			image->refCount = 0;
		}
	}
}

void OverlayManager::fade(int handle, int type, bool hasValue, float value) {
	int index = findOverlay(handle);

	if (index < 0)
		return;

	Overlay &overlay = _overlays[index];

	if (overlay.image)
		ensureLoaded(overlay.image);

	overlay.fade = type;

	if (type == kFadeSet)
		overlay.alpha = hasValue ? value : 0.0f;
	else if (type == kFadeIn)
		overlay.alpha = 0.0f;
}

void OverlayManager::showLoadSpinner(bool show, int x, int y) {
	if (show) {
		if (_spinnerHandle != kNoHandle)
			return;

		OverlayProps props;

		props.layer = 5;
		props.coords = kCoordsGlobal;
		props.cache = kCachePersistent;
		_spinnerHandle = create("save_wait_icon.png", x, y, props);

		int index = findOverlay(_spinnerHandle);

		if (index < 0)
			return;

		int width = 0;
		int height = 0;

		getDimensions(_spinnerHandle, &width, &height);
		_overlays[index].x -= width / 2;
		_overlays[index].y -= height / 2;
	} else if (_spinnerHandle >= 0) {
		destroy(_spinnerHandle);
		_spinnerHandle = kNoHandle;
	}
}

bool OverlayManager::loadCached(const Common::String &name, int cacheMode) {
	if (name.empty())
		return false;

	CachedImage *image = acquireImage(name);

	image->cacheMode = cacheMode;

	return true;
}

void OverlayManager::setImage(int handle, const Common::String &name) {
	Overlay *overlay = fromHandle(handle);

	if (!overlay)
		return;

	overlay->image = name.empty() ? nullptr : acquireImage(name);
}

bool OverlayManager::getLayer(int handle, int *layer) {
	Overlay *overlay = fromHandle(handle);

	if (!overlay)
		return false;

	*layer = overlay->layer;

	return true;
}

bool OverlayManager::hasImage(int handle) {
	Overlay *overlay = fromHandle(handle);

	return overlay && overlay->image;
}

void OverlayManager::preloadImage(const Common::String &name) {
	if (name.empty())
		return;

	ensureLoaded(acquireImage(name));
}

void OverlayManager::drawOverlay(const Overlay &overlay, int boxing) {
	int x = overlay.x;
	int y = overlay.y;

	if (overlay.coords == kCoordsGameScreen) {
		x += 240;
	} else if (overlay.coords == kCoordsLegacy) {
		y = 1080 * y / 480;
		x = 1440 * x / 640 + 240;
	}

	if (overlay.flags & 1)
		_canvas->clearBuffer();

	CachedImage *image = overlay.image;

	if (!image->pixels)
		return;

	int width = overlay.widthOverride > 0 ? overlay.widthOverride : image->width;
	int height = overlay.heightOverride > 0 ? overlay.heightOverride : image->height;
	float scaleX;
	float scaleY;
	float offsetX;
	float offsetY;
	float widthScale;
	float heightScale;

	if (boxing == 1) {
		_canvas->computeScaling(false, 1920, 1080, &scaleX, &scaleY, &offsetX, &offsetY);
		widthScale = scaleX;
		heightScale = scaleY;
	} else {
		_canvas->computeScaling(_canvas->isStretched(), 1440, 1080, &scaleX, &scaleY, &offsetX, &offsetY);
		offsetX = (float)((double)offsetX - 240.0 * (double)scaleX);
		widthScale = scaleX < scaleY ? scaleX : scaleY;
		heightScale = widthScale;
	}

	// The position is a double expression rounded to float once, and so is the size
	float pixelX = (float)((double)scaleX * (double)x + (double)offsetX);
	float pixelY = (float)((double)scaleY * (double)y + (double)offsetY);

	_canvas->queueOverlay(image->pixels, image->width, image->height, pixelX, pixelY,
			(float)((double)width * (double)widthScale), (float)((double)height * (double)heightScale), overlay.mirror,
			overlay.alpha, overlay.desaturate);
}

// The end is tested before the step, and the ends snap
bool OverlayManager::stepFade(Overlay &overlay) {
	switch (overlay.fade) {
	case kFadeIn:
		if (fabsf(overlay.alpha + -1.0f) < kFadeThreshold) {
			overlay.alpha = 1.0f;
			overlay.fade = kFadeNone;
		} else {
			overlay.alpha += overlay.alpha > 1.0f ? -kFadeStep : kFadeStep;
		}

		break;
	case kFadeOut:
	case kFadeOutDelete:
		if (fabsf(overlay.alpha) < kFadeThreshold) {
			int fade = overlay.fade;

			overlay.alpha = 0.0f;
			overlay.fade = kFadeNone;

			if (fade == kFadeOutDelete)
				return true;
		} else {
			overlay.alpha += overlay.alpha > 0.0f ? -kFadeStep : kFadeStep;
		}

		break;
	case kFadeToGray:
		if (fabsf(overlay.alpha + -kGrayAlphaTarget) >= kGrayAlphaThreshold) {
			overlay.alpha += overlay.alpha > kGrayAlphaTarget ? -kGrayAlphaStep : kGrayAlphaStep;
		} else {
			overlay.alpha = kGrayAlphaTarget;

			if (fabsf(overlay.desaturate + -kGrayDesaturateTarget) < kGrayDesaturateThreshold) {
				overlay.desaturate = kGrayDesaturateTarget;
				overlay.fade = kFadeNone;
			} else {
				overlay.desaturate += overlay.desaturate > kGrayDesaturateTarget ? -kGrayDesaturateStep : kGrayDesaturateStep;
			}
		}

		break;
	case kFadeFromGray:
		if (fabsf(overlay.alpha + -1.0f) >= kGrayAlphaThreshold) {
			overlay.alpha += overlay.alpha > 1.0f ? -kGrayAlphaStep : kGrayAlphaStep;
		} else {
			overlay.alpha = 1.0f;

			if (fabsf(overlay.desaturate) < kGrayDesaturateThreshold) {
				overlay.desaturate = 0.0f;
				overlay.fade = kFadeNone;
			} else {
				overlay.desaturate += overlay.desaturate > 0.0f ? -kGrayDesaturateStep : kGrayDesaturateStep;
			}
		}

		break;
	default:
		break;
	}

	return false;
}

void OverlayManager::render(int layer, int boxing) {
	for (int i = 0; i < _numOverlays; i++) {
		Overlay &overlay = _overlays[i];

		if (overlay.layer != layer || overlay.hidden == 2 || !overlay.image)
			continue;

		ensureLoaded(overlay.image);
		drawOverlay(overlay, boxing);

		if (stepFade(overlay)) {
			destroy(overlay.handle);
			i--;
		}
	}
}

} // End of namespace Grim
