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

#include "common/list.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/util.h"

#include "graphics/pixelformat.h"
#include "graphics/renderer.h"

#include "engines/grim/grim.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/std/std_display.h"

#include <math.h>
#include <string.h>

namespace Grim {

HdCanvas::Item::Item() : kind(kItemBitmap), pixels(nullptr), srcWidth(0), srcHeight(0), layer(0), bitmapX(0), bitmapY(0),
		bitmapAlpha(255), x(0.0f), y(0.0f), width(0.0f), height(0.0f), mirror(0), alpha(1.0f), desaturate(0.0f), gamma(1.0f),
		fillRed(0.0f), fillGreen(0.0f), fillBlue(0.0f), serial(0), changing(false) {
}

HdCanvas::PowTable::PowTable() : valid(false), exponent(1.0f) {
	memset(threshold, 0, sizeof(threshold));
	memset(cell, 0, sizeof(cell));
}

HdCanvas::SceneCache::SceneCache() : valid(false), hasFrame(false), serial(0), lastUse(0), width(0), height(0), pitch(0), bpp(0),
		redBits(0), greenBits(0), blueBits(0), redShift(0), greenShift(0), blueShift(0), stretched(false), gamma(1.0f),
		sourceSize(0), source(nullptr), frame(nullptr) {
}

HdCanvas::SceneCache::~SceneCache() {
	delete[] source;
	delete[] frame;
}

HdCanvas::HdCanvas() : _active(false), _stretched(false), _gpu(false), _remasteredLookRenderer(nullptr), _overlayFrame(nullptr), _frame(nullptr), _redShift(16), _greenShift(8), _blueShift(0), _opaqueMask(0xFF000000),
		_gamma(1.0f), _frontScene(nullptr), _sceneWidth(0), _sceneHeight(0), _scenePitch(0), _softwareAlpha(0.0f),
		_softwareAlphaEnd(0.0f), _softwareAlphaStart(0.0f), _softwareAlphaBlendTime(0.75f), _previousFrameTime(0.0),
		_hasPreviousFrame(false), _redMax(0), _greenMax(0), _blueMax(0), _sceneUseCounter(0), _sceneSerial(0),
		_composedSceneToken(0), _composedValid(false), _overlays(nullptr) {
	memset(&_sceneFormat, 0, sizeof(_sceneFormat));
	memset(_redLevels, 0, sizeof(_redLevels));
	memset(_greenLevels, 0, sizeof(_greenLevels));
	memset(_blueLevels, 0, sizeof(_blueLevels));

	_horizontalRowIndex[0] = -1;
	_horizontalRowIndex[1] = -1;
	_itemRowIndex[0] = -1;
	_itemRowIndex[1] = -1;

	for (int i = 0; i < 256; i++)
		_byteToFloat[i] = (float)i / 255.0f;

	_overlays = new OverlayManager(this);
}

HdCanvas::~HdCanvas() {
	close();
	delete _overlays;
	_overlays = nullptr;
}

bool HdCanvas::openGpu(RemasteredLookRenderer *remasteredLookRenderer) {
	if (!remasteredLookRenderer ||!(Graphics::Renderer::getAvailableTypes() & Graphics::kRendererTypeOpenGLShaders))
		return false;

	initGraphics3d(kWidth, kHeight);

	if (!g_system->hasFeature(OSystem::kFeatureOpenGLForGame) || !remasteredLookRenderer->open(kWidth, kHeight)) {
		warning("HdCanvas::openGpu(): the Remastered look is not available, using the software scene");
		return false;
	}

	_redShift = 0;
	_greenShift = 8;
	_blueShift = 16;
	_opaqueMask = 0xFF000000;

	if (!_frame)
		_frame = new uint32[kWidth * kHeight]();

	if (!_overlayFrame)
		_overlayFrame = new uint32[kWidth * kHeight]();

	releaseSceneCaches();
	_composedItems.clear();
	_composedValid = false;
	_remasteredLookRenderer = remasteredLookRenderer;
	_gpu = true;
	_active = true;

	return true;
}

void HdCanvas::open(RemasteredLookRenderer *remasteredLookRenderer) {
	if (openGpu(remasteredLookRenderer))
		return;

	Common::List<Graphics::PixelFormat> supported = g_system->getSupportedFormats();
	Graphics::PixelFormat wanted = Graphics::PixelFormat::createFormatBGRA32();

	for (Common::List<Graphics::PixelFormat>::const_iterator it = supported.begin(); it != supported.end(); ++it) {
		if (it->bytesPerPixel == 4 && it->rLoss == 0 && it->gLoss == 0 && it->bLoss == 0) {
			wanted = *it;
			break;
		}
	}

	initGraphics(kWidth, kHeight, &wanted);

	Graphics::PixelFormat format = g_system->getScreenFormat();

	if (format.bytesPerPixel != 4 || format.rLoss != 0 || format.gLoss != 0 || format.bLoss != 0)
		error("HdCanvas::open(): the screen does not support a 32-bit true color format");

	_redShift = format.rShift;
	_greenShift = format.gShift;
	_blueShift = format.bShift;
	_opaqueMask = 0;

	if (format.aLoss < 8)
		_opaqueMask = (uint32)(((1u << (8 - format.aLoss)) - 1) << format.aShift);

	if (!_frame)
		_frame = new uint32[kWidth * kHeight]();

	releaseSceneCaches();
	_composedItems.clear();
	_composedValid = false;

	_active = true;
}

void HdCanvas::close() {
	if (_gpu && _remasteredLookRenderer)
		_remasteredLookRenderer->close();

	_gpu = false;
	_remasteredLookRenderer = nullptr;
	delete[] _overlayFrame;
	_overlayFrame = nullptr;
	_active = false;
	_items.clear();
	_composedItems.clear();
	_composedValid = false;
	releaseSceneCaches();
	delete[] _frame;
	_frame = nullptr;
	delete[] _frontScene;
	_frontScene = nullptr;
	_sceneWidth = 0;
	_sceneHeight = 0;
	_scenePitch = 0;
}

void HdCanvas::beginFrame() {
	_items.resize(0);

	if (_gpu && _remasteredLookRenderer)
		_remasteredLookRenderer->beginFrame();
}

void HdCanvas::releaseSceneCaches() {
	for (int i = 0; i < kSceneCacheEntries; i++) {
		SceneCache &entry = _sceneCache[i];

		delete[] entry.source;
		delete[] entry.frame;
		entry.source = nullptr;
		entry.frame = nullptr;
		entry.sourceSize = 0;
		entry.valid = false;
		entry.hasFrame = false;
		entry.lastUse = 0;
	}
}

void HdCanvas::setGamma(float gamma) {
	_gamma = gamma;
}

void HdCanvas::queueBitmap(int layer, const byte *rgba, int width, int height, int x, int y, byte alpha) {
	if (!rgba || width <= 0 || height <= 0)
		return;

	Item item;

	item.kind = kItemBitmap;
	item.layer = layer;
	item.pixels = rgba;
	item.srcWidth = width;
	item.srcHeight = height;
	item.bitmapX = x;
	item.bitmapY = y;
	item.bitmapAlpha = alpha;

	item.changing = true;
	_items.push_back(item);
}

void HdCanvas::clearLayer(int layer) {
	for (int i = (int)_items.size() - 1; i >= 0; i--) {
		if (_items[i].kind == kItemBitmap && _items[i].layer == layer)
			_items.remove_at(i);
	}
}

void HdCanvas::queueOverlay(const byte *premultipliedRgba, int srcWidth, int srcHeight, float x, float y, float width, float height,
		int mirror, float alpha, float desaturate) {
	if (!premultipliedRgba || srcWidth <= 0 || srcHeight <= 0)
		return;

	Item item;

	item.kind = kItemOverlay;
	item.pixels = premultipliedRgba;
	item.srcWidth = srcWidth;
	item.srcHeight = srcHeight;
	item.x = x;
	item.y = y;
	item.width = width;
	item.height = height;
	item.mirror = mirror;

	item.alpha = 1.0f;

	if (alpha <= 1.0f)
		item.alpha = alpha < 0.0f ? 0.0f : alpha;

	item.desaturate = 1.0f;

	if (desaturate <= 1.0f)
		item.desaturate = desaturate < 0.0f ? 0.0f : desaturate;

	item.gamma = _gamma;
	_items.push_back(item);
}

// Every value is computed in double and rounded to float once
void HdCanvas::queueFilledRectangle(int left, int top, int right, int bottom, uint colour, float alpha) {
	int canvasWidth = g_grim->getTextCanvasWidth();
	float scaleX;
	float scaleY;
	float offsetX;
	float offsetY;

	computeScaling(false, canvasWidth, 1080, &scaleX, &scaleY, &offsetX, &offsetY);

	double shiftWidth = (double)(uint32)(kWidth - canvasWidth);

	offsetX = (float)((double)offsetX - (shiftWidth * (double)scaleX) * 0.5);

	Item item;

	item.kind = kItemFill;
	item.x = (float)((double)left * (double)scaleX + (double)offsetX);
	item.y = (float)((double)top * (double)scaleY + (double)offsetY);
	item.width = (float)((double)scaleX * (double)(right - left));
	item.height = (float)((double)scaleY * (double)(bottom - top));
	item.alpha = alpha;
	item.fillRed = (float)(((double)(colour & 0xFF) / 255.0) * (double)alpha);
	item.fillGreen = (float)(((double)((colour >> 8) & 0xFF) / 255.0) * (double)alpha);
	item.fillBlue = (float)(((double)((colour >> 16) & 0xFF) / 255.0) * (double)alpha);
	_items.push_back(item);
}

void HdCanvas::clearBuffer() {
	Item item;

	item.kind = kItemFill;
	item.x = 0.0f;
	item.y = 0.0f;
	item.width = (float)kWidth;
	item.height = (float)kHeight;
	item.alpha = 1.0f;
	_items.push_back(item);
}

void HdCanvas::queueText(const HdTextDraw &draw) {
	if (draw.quads.empty())
		return;

	_items.push_back(Item());

	Item &item = _items.back();

	item.kind = kItemText;
	item.text = draw;
}

void HdCanvas::queueMovie(const byte *rgba, int width, int height, int contentSerial) {
	if (!rgba || width <= 0 || height <= 0)
		return;

	float backWidth = (float)kWidth;
	float backHeight = (float)kHeight;
	float limitX = 640.0f / backWidth;
	float limitY = 480.0f / backHeight;
	float halfWidth;
	float halfHeight;

	if (limitY <= limitX) {
		halfWidth = (float)(480 * width) / (float)(640 * height);
		halfHeight = limitY / limitX;
	} else {
		halfWidth = ((float)width / backWidth) / ((float)height / backHeight);
		halfHeight = 1.0f;
	}

	if (_stretched) {
		halfWidth = (float)(480 * width) / (float)(640 * height);
		halfHeight = 1.0f;
	}

	Item item;

	item.kind = kItemMovie;
	item.pixels = rgba;
	item.srcWidth = width;
	item.srcHeight = height;
	item.x = (-halfWidth + 1.0f) * (backWidth * 0.5f);
	item.y = (-halfHeight + 1.0f) * (backHeight * 0.5f);
	item.width = (halfWidth + halfWidth) * (backWidth * 0.5f);
	item.height = (halfHeight + halfHeight) * (backHeight * 0.5f);
	item.gamma = _gamma;
	item.serial = contentSerial;
	item.changing = contentSerial < 0;
	_items.push_back(item);
}

void HdCanvas::dropPixels(const byte *pixels) {
	for (uint i = 0; i < _items.size(); i++) {
		if (_items[i].pixels == pixels)
			_items[i].pixels = nullptr;
	}

	_composedValid = false;
}

float HdCanvas::getAreaScaleX() const {
	return _stretched ? (float)kWidth / (float)kAreaWidth : 1.0f;
}

float HdCanvas::getWidescreenCorrection() const {
	float scaleX = getAreaScaleX();

	return MIN(scaleX, 1.0f) / scaleX;
}

uint32 HdCanvas::packPixel(int red, int green, int blue) const {
	return ((uint32)red << _redShift) | ((uint32)green << _greenShift) | ((uint32)blue << _blueShift) | _opaqueMask;
}

HdTextCanvas HdCanvas::getTextCanvas() const {
	HdTextCanvas canvas;

	canvas.width = g_grim->getTextCanvasWidth();

	canvas.boxingHD = (double)canvas.width / 1080.0 > 1.7;
	canvas.stretched = _stretched;

	return canvas;
}

// The offsets are computed in double and rounded to float once
void HdCanvas::computeScaling(bool stretch, int referenceWidth, int referenceHeight, float *scaleX, float *scaleY,
		float *offsetX, float *offsetY) {
	float backWidth = (float)(uint32)kWidth;
	float backHeight = (float)(uint32)kHeight;
	float width = (float)(uint32)referenceWidth;
	float height = (float)(uint32)referenceHeight;
	float xScale = (float)((double)backWidth / (double)width);
	float yScale = (float)((double)backHeight / (double)height);
	float scale = xScale > yScale ? yScale : xScale;

	if (stretch) {
		*scaleX = xScale;
		*scaleY = yScale;
		*offsetX = 0.0f;
		*offsetY = 0.0f;
	} else {
		*scaleX = scale;
		*scaleY = scale;
		*offsetX = (float)(((double)backWidth - (double)width * (double)scale) * 0.5);
		*offsetY = (float)(0.5 * ((double)backHeight - (double)height * (double)scale));
	}
}

int HdCanvas::toByte(float value) {
	if (!(value > 0.0f))
		return 0;

	if (value >= 1.0f)
		return 255;

	return (int)(value * 255.0f + 0.5f);
}

int HdCanvas::clampPixel(float value, int limit) {
	float pixel = ceilf(value - 0.5f);

	if (!(pixel > 0.0f))
		return 0;

	if (pixel > (float)limit)
		return limit;

	return (int)pixel;
}

void HdCanvas::updateSoftwareAlpha() {
	double now = (double)g_system->getMillis();

	if (!_hasPreviousFrame) {
		_previousFrameTime = now;
		_hasPreviousFrame = true;
	}

	double previous = _previousFrameTime;

	_previousFrameTime = now;

	float previousEnd = _softwareAlphaEnd;

	_softwareAlphaEnd = g_grim->getRenderingMode() == GrimEngine::kRenderingModeHd ? 0.0f : 1.0f;

	if (fabsf(_softwareAlphaEnd - previousEnd) > 0.001f) {
		_softwareAlphaStart = _softwareAlpha;
		_softwareAlphaBlendTime = 0.0f;
	}

	if (fabsf(_softwareAlphaEnd - _softwareAlpha) > 0.001f) {
		float step = (float)((now - previous) / 1000.0);
		float blend = step + _softwareAlphaBlendTime;

		_softwareAlphaBlendTime = blend < 0.75f ? blend : 0.75f;

		float t = _softwareAlphaBlendTime / 0.75f;

		// The mix is one float store of a double expression
		float value = (float)(((double)t * (double)_softwareAlphaEnd) + ((1.0 - (double)t) * (double)_softwareAlphaStart));
		float clamped = 0.0f;

		if (!(0.0f > value))
			clamped = 1.0f < value ? 1.0f : value;

		_softwareAlpha = (float)((3.0 - (double)(clamped + clamped)) * ((double)clamped * (double)clamped));
	} else {
		_softwareAlpha = _softwareAlphaEnd;
	}
}

void HdCanvas::prepareFrontScene(const StdVBuffer *scene) {
	int pitch = scene->tex.pitchInBytes;
	int height = scene->tex.height;

	if (!_frontScene || _scenePitch != pitch || _sceneHeight != height) {
		delete[] _frontScene;
		_frontScene = new byte[(size_t)pitch * (size_t)height]();
	}

	_scenePitch = pitch;
	_sceneWidth = scene->tex.width;
	_sceneHeight = height;
	_sceneFormat = scene->tex.format;
}

byte *HdCanvas::getFrontScene(const StdVBuffer *engineScene, int *pitchInBytes, int *width, int *height) {
	if (!engineScene || !engineScene->pixels)
		return nullptr;

	prepareFrontScene(engineScene);

	*pitchInBytes = _scenePitch;
	*width = _sceneWidth;
	*height = _sceneHeight;

	return _frontScene;
}

void HdCanvas::swapScene(const StdVBuffer *scene) {
	if (!scene || !scene->pixels)
		return;

	prepareFrontScene(scene);

	size_t size = (size_t)_scenePitch * (size_t)_sceneHeight;
	byte block[4096];

	for (size_t offset = 0; offset < size; offset += sizeof(block)) {
		size_t count = MIN<size_t>(sizeof(block), size - offset);

		memcpy(block, _frontScene + offset, count);
		memcpy(_frontScene + offset, scene->pixels + offset, count);
		memcpy(scene->pixels + offset, block, count);
	}
}

void HdCanvas::buildPowTable(PowTable *table, float exponent) {
	if (table->valid && table->exponent == exponent)
		return;

	if (!(exponent > 0.0f)) {
		warning("HdCanvas::buildPowTable(): invalid gamma exponent, using 1.0");
		exponent = 1.0f;
	}

	double inverse = 1.0 / (double)exponent;

	for (int i = 0; i < 255; i++)
		table->threshold[i] = pow(((double)i + 0.5) / 255.0, inverse);

	int level = 0;

	for (int i = 0; i <= kPowCells; i++) {
		double value = (double)i / (double)kPowCells;

		while (level < 255 && value >= table->threshold[level])
			level++;

		table->cell[i] = (byte)level;
	}

	table->exponent = exponent;
	table->valid = true;
}

byte HdCanvas::lookupPow(const PowTable &table, float value) const {
	if (!(value > 0.0f))
		return 0;

	if (value >= 1.0f)
		return 255;

	int level = table.cell[(int)(value * (float)kPowCells)];

	while (level < 255 && (double)value >= table.threshold[level])
		level++;

	return (byte)level;
}

void HdCanvas::present(const StdVBuffer *scene, float gamma) {
	if (!_active || !_frame)
		return;

	_gamma = gamma;

	if (_gpu) {
		presentGpu(scene);
		return;
	}

	updateSoftwareAlpha();

	int firstItem = findCoveringItem();
	SceneCache *entry = nullptr;
	bool matched = false;
	int sceneToken = 0;
	bool reusable = true;

	if (firstItem < 0 && _frontScene) {
		if (isSceneSupported()) {
			entry = findSceneCache(&matched);
			sceneToken = entry->serial;
		} else {
			reusable = false;
		}
	}

	bool changing = hasChangingItem();

	if (reusable && !changing && _composedValid && sceneToken == _composedSceneToken && sameQueue(_items, _composedItems)) {
		g_system->updateScreen();
	} else {
		composeFrame(firstItem, entry, matched);

		_composedValid = false;

		if (reusable && !changing) {
			_composedItems = _items;
			_composedSceneToken = sceneToken;
			_composedValid = true;
		}

		g_system->copyRectToScreen(_frame, kWidth * 4, 0, 0, kWidth, kHeight);
		g_system->updateScreen();
	}

	swapScene(scene);

	_stretched = g_grim->isWidescreenStretched();
}

void HdCanvas::buildOverlayFrame() {
	for (int i = 0; i < kWidth * kHeight; i++)
		_frame[i] = _opaqueMask;

	for (uint i = 0; i < _items.size(); i++)
		drawItem(_items[i]);

	memcpy(_overlayFrame, _frame, (size_t)kWidth * (size_t)kHeight * sizeof(uint32));

	for (int i = 0; i < kWidth * kHeight; i++)
		_frame[i] = 0xFFFFFFFF;

	for (uint i = 0; i < _items.size(); i++)
		drawItem(_items[i]);

	for (int i = 0; i < kWidth * kHeight; i++) {
		uint32 dark = _overlayFrame[i];
		uint32 light = _frame[i];
		int darkGreen = (int)((dark >> 8) & 0xFF);
		int lightGreen = (int)((light >> 8) & 0xFF);
		int alpha = 255 - (lightGreen - darkGreen);

		if (alpha < 0)
			alpha = 0;
		else if (alpha > 255)
			alpha = 255;

		int red = MIN((int)(dark & 0xFF), alpha);
		int green = MIN(darkGreen, alpha);
		int blue = MIN((int)((dark >> 16) & 0xFF), alpha);

		_overlayFrame[i] = (uint32)red | ((uint32)green << 8) | ((uint32)blue << 16) | ((uint32)alpha << 24);
	}
}

void HdCanvas::presentGpu(const StdVBuffer *scene) {
	bool changing = hasChangingItem();
	bool changed = !(!changing && _composedValid && sameQueue(_items, _composedItems));

	if (changed)
		buildOverlayFrame();

	const byte *software = nullptr;

	if (_frontScene && isSceneSupported())
		software = _frontScene;

	_remasteredLookRenderer->present(software, _scenePitch, _overlayFrame ? (const byte *)_overlayFrame : nullptr, changed);
	g_system->updateScreen();

	_composedValid = false;

	if (!changing) {
		_composedItems = _items;
		_composedValid = true;
	}

	swapScene(scene);

	_stretched = g_grim->isWidescreenStretched();
}

void HdCanvas::fillFrame() {
	for (int i = 0; i < kWidth * kHeight; i++)
		_frame[i] = _opaqueMask;
}

void HdCanvas::composeFrame(int firstItem, SceneCache *entry, bool matched) {
	uint start = 0;

	if (firstItem >= 0) {
		start = (uint)firstItem;
	} else if (entry && entry->hasFrame) {
		memcpy(_frame, entry->frame, (size_t)kWidth * (size_t)kHeight * sizeof(uint32));
	} else {
		fillFrame();
		drawScene();

		if (entry && matched)
			storeSceneFrame(entry);
	}

	for (uint i = start; i < _items.size(); i++)
		drawItem(_items[i]);
}

bool HdCanvas::isSceneSupported() const {
	const StdColorFormat *format = &_sceneFormat;

	return _sceneWidth >= 1 && _sceneHeight >= 1 && format->bpp == 16 && format->redBits >= 1 && format->redBits <= 8 &&
			format->greenBits >= 1 && format->greenBits <= 8 && format->blueBits >= 1 && format->blueBits <= 8;
}

bool HdCanvas::sceneCacheMatches(const SceneCache &entry) const {
	size_t size = (size_t)_scenePitch * (size_t)_sceneHeight;

	if (!entry.valid || entry.sourceSize != size || entry.width != _sceneWidth || entry.height != _sceneHeight ||
			entry.pitch != _scenePitch || entry.bpp != _sceneFormat.bpp || entry.redBits != _sceneFormat.redBits ||
			entry.greenBits != _sceneFormat.greenBits || entry.blueBits != _sceneFormat.blueBits ||
			entry.redShift != _sceneFormat.redShift || entry.greenShift != _sceneFormat.greenShift ||
			entry.blueShift != _sceneFormat.blueShift || entry.stretched != _stretched || !sameFloat(entry.gamma, _gamma))
		return false;

	return memcmp(entry.source, _frontScene, size) == 0;
}

HdCanvas::SceneCache *HdCanvas::findSceneCache(bool *matched) {
	_sceneUseCounter++;

	for (int i = 0; i < kSceneCacheEntries; i++) {
		if (sceneCacheMatches(_sceneCache[i])) {
			_sceneCache[i].lastUse = _sceneUseCounter;
			*matched = true;

			return &_sceneCache[i];
		}
	}

	int victim = 0;

	for (int i = 1; i < kSceneCacheEntries; i++) {
		if (_sceneCache[i].lastUse < _sceneCache[victim].lastUse)
			victim = i;
	}

	SceneCache &entry = _sceneCache[victim];
	size_t size = (size_t)_scenePitch * (size_t)_sceneHeight;

	if (!entry.source || entry.sourceSize != size) {
		delete[] entry.source;
		entry.source = new byte[size];
		entry.sourceSize = size;
	}

	memcpy(entry.source, _frontScene, size);

	entry.valid = true;
	entry.hasFrame = false;
	entry.serial = ++_sceneSerial;
	entry.lastUse = _sceneUseCounter;
	entry.width = _sceneWidth;
	entry.height = _sceneHeight;
	entry.pitch = _scenePitch;
	entry.bpp = _sceneFormat.bpp;
	entry.redBits = _sceneFormat.redBits;
	entry.greenBits = _sceneFormat.greenBits;
	entry.blueBits = _sceneFormat.blueBits;
	entry.redShift = _sceneFormat.redShift;
	entry.greenShift = _sceneFormat.greenShift;
	entry.blueShift = _sceneFormat.blueShift;
	entry.stretched = _stretched;
	entry.gamma = _gamma;
	*matched = false;

	return &entry;
}

void HdCanvas::storeSceneFrame(SceneCache *entry) {
	if (!entry->frame)
		entry->frame = new uint32[kWidth * kHeight];

	memcpy(entry->frame, _frame, (size_t)kWidth * (size_t)kHeight * sizeof(uint32));
	entry->hasFrame = true;
}

bool HdCanvas::sameFloat(float first, float second) {
	return memcmp(&first, &second, sizeof(float)) == 0;
}

bool HdCanvas::sameItem(const Item &first, const Item &second) {
	if (first.changing || second.changing)
		return false;

	if (first.kind != second.kind || first.pixels != second.pixels || first.srcWidth != second.srcWidth ||
			first.srcHeight != second.srcHeight || first.layer != second.layer || first.bitmapX != second.bitmapX ||
			first.bitmapY != second.bitmapY || first.bitmapAlpha != second.bitmapAlpha || first.mirror != second.mirror ||
			first.serial != second.serial)
		return false;

	if (!sameFloat(first.x, second.x) || !sameFloat(first.y, second.y) || !sameFloat(first.width, second.width) ||
			!sameFloat(first.height, second.height) || !sameFloat(first.alpha, second.alpha) ||
			!sameFloat(first.desaturate, second.desaturate) || !sameFloat(first.gamma, second.gamma) ||
			!sameFloat(first.fillRed, second.fillRed) || !sameFloat(first.fillGreen, second.fillGreen) ||
			!sameFloat(first.fillBlue, second.fillBlue))
		return false;

	if (first.kind != kItemText)
		return true;

	const HdTextDraw &firstText = first.text;
	const HdTextDraw &secondText = second.text;

	if (firstText.quads.size() != secondText.quads.size() || !sameFloat(firstText.originX, secondText.originX) ||
			!sameFloat(firstText.originY, secondText.originY) || !sameFloat(firstText.textRed, secondText.textRed) ||
			!sameFloat(firstText.textGreen, secondText.textGreen) || !sameFloat(firstText.textBlue, secondText.textBlue) ||
			!sameFloat(firstText.alpha, secondText.alpha))
		return false;

	for (uint i = 0; i < firstText.quads.size(); i++) {
		const HdTextQuad &a = firstText.quads[i];
		const HdTextQuad &b = secondText.quads[i];

		if (!sameFloat(a.x, b.x) || !sameFloat(a.y, b.y) || a.width != b.width || a.height != b.height ||
				a.character != b.character || a.glyph != b.glyph)
			return false;
	}

	return true;
}

bool HdCanvas::sameQueue(const Common::Array<Item> &first, const Common::Array<Item> &second) {
	if (first.size() != second.size())
		return false;

	for (uint i = 0; i < first.size(); i++) {
		if (!sameItem(first[i], second[i]))
			return false;
	}

	return true;
}

bool HdCanvas::hasChangingItem() const {
	for (uint i = 0; i < _items.size(); i++) {
		if (_items[i].changing)
			return true;
	}

	return false;
}

bool HdCanvas::coversFrame(const Item &item) const {
	if (item.kind == kItemMovie) {
		if (!item.pixels || !(item.width > 0.0f) || !(item.height > 0.0f))
			return false;
	} else if (item.kind == kItemFill) {
		if (item.alpha != 1.0f)
			return false;
	} else {
		return false;
	}

	return clampPixel(item.x, kWidth) == 0 && clampPixel(item.x + item.width, kWidth) == kWidth &&
			clampPixel(item.y, kHeight) == 0 && clampPixel(item.y + item.height, kHeight) == kHeight;
}

int HdCanvas::findCoveringItem() const {
	for (int i = (int)_items.size() - 1; i >= 0; i--) {
		if (coversFrame(_items[i]))
			return i;
	}

	return -1;
}

const float *HdCanvas::getHorizontalRow(int row, int keepRow) {
	for (int i = 0; i < 2; i++) {
		if (_horizontalRowIndex[i] == row)
			return &_horizontalRows[i][0];
	}

	int slot = _horizontalRowIndex[0] == keepRow ? 1 : 0;
	int count = (int)_columnFirst.size();

	_horizontalRowIndex[slot] = row;
	_horizontalRows[slot].resize(count * 3);

	const uint16 *source = (const uint16 *)(_frontScene + (size_t)row * (size_t)_scenePitch);
	float *destination = &_horizontalRows[slot][0];

	for (int i = 0; i < count; i++) {
		uint first = source[_columnFirst[i]];
		uint second = source[_columnSecond[i]];
		float fraction = _columnFraction[i];
		float inverse = 1.0f - fraction;

		float redFirst = _redLevels[(first >> _sceneFormat.redShift) & _redMax];
		float redSecond = _redLevels[(second >> _sceneFormat.redShift) & _redMax];
		float greenFirst = _greenLevels[(first >> _sceneFormat.greenShift) & _greenMax];
		float greenSecond = _greenLevels[(second >> _sceneFormat.greenShift) & _greenMax];
		float blueFirst = _blueLevels[(first >> _sceneFormat.blueShift) & _blueMax];
		float blueSecond = _blueLevels[(second >> _sceneFormat.blueShift) & _blueMax];

		destination[i * 3] = redFirst * inverse + redSecond * fraction;
		destination[i * 3 + 1] = greenFirst * inverse + greenSecond * fraction;
		destination[i * 3 + 2] = blueFirst * inverse + blueSecond * fraction;
	}

	return destination;
}

void HdCanvas::drawScene() {
	if (!_frontScene)
		return;

	const StdColorFormat *format = &_sceneFormat;
	int srcWidth = _sceneWidth;
	int srcHeight = _sceneHeight;

	if (srcWidth < 1 || srcHeight < 1 || format->bpp != 16 || format->redBits < 1 || format->redBits > 8 ||
			format->greenBits < 1 || format->greenBits > 8 || format->blueBits < 1 || format->blueBits > 8) {
		warning("HdCanvas::drawScene(): unsupported scene surface");
		return;
	}

	_redMax = (1 << format->redBits) - 1;
	_greenMax = (1 << format->greenBits) - 1;
	_blueMax = (1 << format->blueBits) - 1;

	for (int i = 0; i <= _redMax; i++)
		_redLevels[i] = _byteToFloat[(i * 255 + _redMax / 2) / _redMax];

	for (int i = 0; i <= _greenMax; i++)
		_greenLevels[i] = _byteToFloat[(i * 255 + _greenMax / 2) / _greenMax];

	for (int i = 0; i <= _blueMax; i++)
		_blueLevels[i] = _byteToFloat[(i * 255 + _blueMax / 2) / _blueMax];

	float scaleX;
	float scaleY;
	float offsetX;
	float offsetY;

	computeScaling(_stretched, srcWidth, srcHeight, &scaleX, &scaleY, &offsetX, &offsetY);

	float quadWidth = scaleX * (float)srcWidth;
	float quadHeight = scaleY * (float)srcHeight;
	int left = clampPixel(offsetX, kWidth);
	int right = clampPixel(offsetX + quadWidth, kWidth);
	int top = clampPixel(offsetY, kHeight);
	int bottom = clampPixel(offsetY + quadHeight, kHeight);

	if (left >= right || top >= bottom)
		return;

	int columns = right - left;

	_columnFirst.resize(columns);
	_columnSecond.resize(columns);
	_columnFraction.resize(columns);

	for (int i = 0; i < columns; i++) {
		double position = (((double)(left + i) + 0.5 - (double)offsetX) * (double)srcWidth) / (double)quadWidth - 0.5;
		int base = (int)floor(position);

		_columnFraction[i] = (float)(position - (double)base);
		_columnFirst[i] = CLIP(base, 0, srcWidth - 1);
		_columnSecond[i] = CLIP(base + 1, 0, srcWidth - 1);
	}

	_horizontalRowIndex[0] = -1;
	_horizontalRowIndex[1] = -1;

	buildPowTable(&_scenePow, 0.8f * _gamma);

	for (int y = top; y < bottom; y++) {
		double position = (((double)y + 0.5 - (double)offsetY) * (double)srcHeight) / (double)quadHeight - 0.5;
		int base = (int)floor(position);
		float fraction = (float)(position - (double)base);
		int first = CLIP(base, 0, srcHeight - 1);
		int second = CLIP(base + 1, 0, srcHeight - 1);
		const float *upper = getHorizontalRow(first, -1);
		const float *lower = getHorizontalRow(second, first);
		float inverse = 1.0f - fraction;
		uint32 *destination = _frame + y * kWidth + left;

		for (int i = 0; i < columns; i++) {
			float red = upper[i * 3] * inverse + lower[i * 3] * fraction;
			float green = upper[i * 3 + 1] * inverse + lower[i * 3 + 1] * fraction;
			float blue = upper[i * 3 + 2] * inverse + lower[i * 3 + 2] * fraction;

			destination[i] = packPixel(lookupPow(_scenePow, red), lookupPow(_scenePow, green), lookupPow(_scenePow, blue));
		}
	}
}

void HdCanvas::drawItem(const Item &item) {
	switch (item.kind) {
	case kItemBitmap:
		drawBitmap(item);
		break;
	case kItemOverlay:
		drawOverlay(item);
		break;
	case kItemFill:
		drawFill(item);
		break;
	case kItemText:
		drawText(item);
		break;
	case kItemMovie:
		drawMovie(item);
		break;
	default:
		break;
	}
}

void HdCanvas::writePixel(int x, int y, int red, int green, int blue) {
	_frame[y * kWidth + x] = packPixel(red, green, blue);
}

void HdCanvas::blendPixel(int x, int y, float red, float green, float blue, float alpha) {
	uint32 *destination = &_frame[y * kWidth + x];
	uint32 value = *destination;
	float inverse = 1.0f - alpha;
	float destRed = _byteToFloat[(value >> _redShift) & 0xFF];
	float destGreen = _byteToFloat[(value >> _greenShift) & 0xFF];
	float destBlue = _byteToFloat[(value >> _blueShift) & 0xFF];

	*destination = packPixel(toByte(red + destRed * inverse), toByte(green + destGreen * inverse), toByte(blue + destBlue * inverse));
}

void HdCanvas::drawBitmap(const Item &item) {
	if (!item.pixels || item.bitmapAlpha <= 0)
		return;

	int left = MAX(item.bitmapX, 0);
	int top = MAX(item.bitmapY, 0);
	int right = MIN(item.bitmapX + item.srcWidth, (int)kWidth);
	int bottom = MIN(item.bitmapY + item.srcHeight, (int)kHeight);

	for (int y = top; y < bottom; y++) {
		const byte *sourceRow = item.pixels + (int64)(y - item.bitmapY) * item.srcWidth * 4;
		uint32 *destination = _frame + y * kWidth;

		for (int x = left; x < right; x++) {
			const byte *pixel = sourceRow + (x - item.bitmapX) * 4;
			int alpha = pixel[3];

			if (alpha == 0)
				continue;

			int red = (pixel[0] * alpha + 127) / 255;
			int green = (pixel[1] * alpha + 127) / 255;
			int blue = (pixel[2] * alpha + 127) / 255;

			if (item.bitmapAlpha != 255) {
				red = (red * item.bitmapAlpha + 127) / 255;
				green = (green * item.bitmapAlpha + 127) / 255;
				blue = (blue * item.bitmapAlpha + 127) / 255;
				alpha = (alpha * item.bitmapAlpha + 127) / 255;
			}

			if (alpha == 255) {
				destination[x] = packPixel(red, green, blue);
				continue;
			}

			uint32 value = destination[x];
			int inverse = 255 - alpha;
			int destRed = red + ((int)((value >> _redShift) & 0xFF) * inverse + 127) / 255;
			int destGreen = green + ((int)((value >> _greenShift) & 0xFF) * inverse + 127) / 255;
			int destBlue = blue + ((int)((value >> _blueShift) & 0xFF) * inverse + 127) / 255;

			destination[x] = packPixel(MIN(destRed, 255), MIN(destGreen, 255), MIN(destBlue, 255));
		}
	}
}

void HdCanvas::drawFill(const Item &item) {
	int left = clampPixel(item.x, kWidth);
	int right = clampPixel(item.x + item.width, kWidth);
	int top = clampPixel(item.y, kHeight);
	int bottom = clampPixel(item.y + item.height, kHeight);

	if (left >= right || top >= bottom)
		return;

	if (item.alpha == 1.0f) {
		uint32 value = packPixel(toByte(item.fillRed), toByte(item.fillGreen), toByte(item.fillBlue));

		for (int y = top; y < bottom; y++) {
			uint32 *destination = _frame + y * kWidth;

			for (int x = left; x < right; x++)
				destination[x] = value;
		}

		return;
	}

	if ((right - left) * (bottom - top) < 1024) {
		for (int y = top; y < bottom; y++) {
			for (int x = left; x < right; x++)
				blendPixel(x, y, item.fillRed, item.fillGreen, item.fillBlue, item.alpha);
		}

		return;
	}

	float inverse = 1.0f - item.alpha;
	byte redLevels[256];
	byte greenLevels[256];
	byte blueLevels[256];

	for (int i = 0; i < 256; i++) {
		redLevels[i] = (byte)toByte(item.fillRed + _byteToFloat[i] * inverse);
		greenLevels[i] = (byte)toByte(item.fillGreen + _byteToFloat[i] * inverse);
		blueLevels[i] = (byte)toByte(item.fillBlue + _byteToFloat[i] * inverse);
	}

	for (int y = top; y < bottom; y++) {
		uint32 *destination = _frame + y * kWidth;

		for (int x = left; x < right; x++) {
			uint32 value = destination[x];

			destination[x] = packPixel(redLevels[(value >> _redShift) & 0xFF], greenLevels[(value >> _greenShift) & 0xFF],
					blueLevels[(value >> _blueShift) & 0xFF]);
		}
	}
}

void HdCanvas::drawOverlay(const Item &item) {
	if (!item.pixels || !(item.width > 0.0f) || !(item.height > 0.0f))
		return;

	int left = clampPixel(item.x, kWidth);
	int right = clampPixel(item.x + item.width, kWidth);
	int top = clampPixel(item.y, kHeight);
	int bottom = clampPixel(item.y + item.height, kHeight);

	if (left >= right || top >= bottom)
		return;

	int srcWidth = item.srcWidth;
	int srcHeight = item.srcHeight;
	int columns = right - left;
	double columnStart;
	double columnEnd;
	double rowStart;
	double rowEnd;

	// Any mirror mode past 3 has all the texture coordinates at the size of the quad
	switch (item.mirror) {
	case 0:
		columnStart = 0.0;
		columnEnd = (double)srcWidth;
		rowStart = 0.0;
		rowEnd = (double)srcHeight;
		break;
	case 1:
		columnStart = (double)srcWidth;
		columnEnd = 0.0;
		rowStart = 0.0;
		rowEnd = (double)srcHeight;
		break;
	case 2:
		columnStart = 0.0;
		columnEnd = (double)srcWidth;
		rowStart = (double)srcHeight;
		rowEnd = 0.0;
		break;
	case 3:
		columnStart = (double)srcWidth;
		columnEnd = 0.0;
		rowStart = (double)srcHeight;
		rowEnd = 0.0;
		break;
	default:
		columnStart = (double)item.width;
		columnEnd = columnStart;
		rowStart = (double)item.height;
		rowEnd = rowStart;

		if (columnStart > 1000000000.0) {
			warning("HdCanvas::drawOverlay(): quad too wide for the mirror mode %d", item.mirror);
			columnStart = 1000000001.0;
			columnEnd = columnStart;
		}

		if (rowStart > 1000000000.0) {
			warning("HdCanvas::drawOverlay(): quad too high for the mirror mode %d", item.mirror);
			rowStart = 1000000001.0;
			rowEnd = rowStart;
		}

		break;
	}

	bool aligned = true;

	_itemColumnFirst.resize(columns);
	_itemColumnSecond.resize(columns);
	_itemColumnFraction.resize(columns);

	for (int i = 0; i < columns; i++) {
		double position = columnStart + ((double)(left + i) + 0.5 - (double)item.x) * (columnEnd - columnStart) / (double)item.width - 0.5;
		int base = (int)floor(position);

		_itemColumnFraction[i] = (float)(position - (double)base);
		_itemColumnFirst[i] = CLIP(base, 0, srcWidth - 1);
		_itemColumnSecond[i] = CLIP(base + 1, 0, srcWidth - 1);

		if (_itemColumnFraction[i] != 0.0f)
			aligned = false;
	}

	float gamma = item.gamma;
	bool usePow = gamma != 1.0f;
	bool plain = item.desaturate == 0.0f;
	float opacity = item.alpha;
	float levelPow[256];
	float scaledLevel[256];
	float scaledAlpha[256];
	byte scaledByte[256];

	if (usePow && plain) {
		for (int i = 0; i < 256; i++)
			levelPow[i] = powf(_byteToFloat[i], gamma);
	}

	if (plain) {
		for (int i = 0; i < 256; i++) {
			float level = usePow ? levelPow[i] : _byteToFloat[i];

			scaledLevel[i] = level * opacity;
			scaledByte[i] = (byte)toByte(scaledLevel[i]);
			scaledAlpha[i] = _byteToFloat[i] * opacity;
		}
	}

	_itemRowIndex[0] = -1;
	_itemRowIndex[1] = -1;

	for (int y = top; y < bottom; y++) {
		double position = rowStart + ((double)y + 0.5 - (double)item.y) * (rowEnd - rowStart) / (double)item.height - 0.5;
		int base = (int)floor(position);
		float fractionY = (float)(position - (double)base);
		int firstRow = CLIP(base, 0, srcHeight - 1);
		int secondRow = CLIP(base + 1, 0, srcHeight - 1);
		bool exact = aligned && fractionY == 0.0f;

		if (exact && plain) {
			const byte *row = item.pixels + (size_t)firstRow * (size_t)srcWidth * 4;
			uint32 *destination = _frame + y * kWidth + left;

			for (int i = 0; i < columns; i++) {
				const byte *a = row + _itemColumnFirst[i] * 4;

				if (a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 0)
					continue;

				float alpha = scaledAlpha[a[3]];

				if (alpha == 1.0f)
					destination[i] = packPixel(scaledByte[a[0]], scaledByte[a[1]], scaledByte[a[2]]);
				else
					blendPixel(left + i, y, scaledLevel[a[0]], scaledLevel[a[1]], scaledLevel[a[2]], alpha);
			}

			continue;
		}

		const byte *exactRow = nullptr;
		const float *upperTexels = nullptr;
		const float *lowerTexels = nullptr;
		float inverseY = 1.0f - fractionY;

		if (exact) {
			exactRow = item.pixels + (size_t)firstRow * (size_t)srcWidth * 4;
		} else {
			upperTexels = getItemRow(item.pixels, srcWidth, firstRow, -1, 4, columns);
			lowerTexels = getItemRow(item.pixels, srcWidth, secondRow, firstRow, 4, columns);
		}

		for (int i = 0; i < columns; i++) {
			const byte *a = nullptr;
			float texel[4];
			bool levels = exact;

			if (exact) {
				a = exactRow + _itemColumnFirst[i] * 4;

				for (int c = 0; c < 4; c++)
					texel[c] = _byteToFloat[a[c]];
			} else {
				for (int c = 0; c < 4; c++)
					texel[c] = upperTexels[i * 4 + c] * inverseY + lowerTexels[i * 4 + c] * fractionY;
			}

			if (texel[3] == 0.0f && texel[0] == 0.0f && texel[1] == 0.0f && texel[2] == 0.0f)
				continue;

			float red = texel[0];
			float green = texel[1];
			float blue = texel[2];

			if (item.desaturate != 0.0f) {
				float luma = red * 0.2125f + green * 0.7154f + blue * 0.0721f;
				float inverse = 1.0f - item.desaturate;

				red = red * inverse + luma * item.desaturate;
				green = green * inverse + luma * item.desaturate;
				blue = blue * inverse + luma * item.desaturate;
				levels = false;
			}

			if (usePow) {
				if (levels) {
					red = levelPow[a[0]];
					green = levelPow[a[1]];
					blue = levelPow[a[2]];
				} else {
					red = powf(red, gamma);
					green = powf(green, gamma);
					blue = powf(blue, gamma);
				}
			}

			float alpha = texel[3] * opacity;

			if (alpha == 1.0f)
				writePixel(left + i, y, toByte(red * opacity), toByte(green * opacity), toByte(blue * opacity));
			else
				blendPixel(left + i, y, red * opacity, green * opacity, blue * opacity, alpha);
		}
	}
}

void HdCanvas::drawText(const Item &item) {
	static const int tapOffsets[11][2] = {
		{ -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 },
		{ -2, -2 }, { -2, -1 }, { -1, -2 }
	};

	const HdTextDraw &draw = item.text;

	for (uint q = 0; q < draw.quads.size(); q++) {
		const HdTextQuad &quad = draw.quads[q];
		const HdFont::Glyph *glyph = quad.glyph;

		if (!glyph || !glyph->alpha)
			continue;

		float quadLeft = quad.x + draw.originX;
		float quadTop = quad.y + draw.originY;
		int quadWidth = quad.width;
		int quadHeight = quad.height;
		int left = clampPixel(quadLeft, kWidth);
		int right = clampPixel(quadLeft + (float)quadWidth, kWidth);
		int top = clampPixel(quadTop, kHeight);
		int bottom = clampPixel(quadTop + (float)quadHeight, kHeight);

		if (left >= right || top >= bottom)
			continue;

		// The atlas is sampled at the corner of the texels, every sample is a mix of four
		float positionX = ((float)left + 0.5f) - quadLeft;
		float positionY = ((float)top + 0.5f) - quadTop;
		int startX = (int)floorf(positionX);
		int startY = (int)floorf(positionY);
		float fractionX = positionX - (float)startX;
		float fractionY = positionY - (float)startY;
		float inverseX = 1.0f - fractionX;
		float inverseY = 1.0f - fractionY;

		int gridWidth = quadWidth + 3;
		int gridHeight = quadHeight + 3;

		_textScratch.resize(gridWidth * gridHeight);
		_textRowFirst.resize(gridWidth);
		_textRowSecond.resize(gridWidth);

		float *upperRow = &_textRowFirst[0];
		float *lowerRow = &_textRowSecond[0];

		for (int n = -1; n < gridHeight; n++) {
			int gy = (n - 4) + 1;
			float *row = n < 0 ? upperRow : lowerRow;

			for (int m = 0; m < gridWidth; m++) {
				float first = 0.0f;
				float second = 0.0f;
				int gx = m - 4;

				if (gy >= 0 && gy < glyph->height) {
					const byte *line = glyph->alpha + gy * glyph->width;

					if (gx >= 0 && gx < glyph->width)
						first = _byteToFloat[line[gx]];

					if (gx + 1 >= 0 && gx + 1 < glyph->width)
						second = _byteToFloat[line[gx + 1]];
				}

				row[m] = first * inverseX + second * fractionX;
			}

			if (n >= 0) {
				for (int m = 0; m < gridWidth; m++)
					_textScratch[n * gridWidth + m] = upperRow[m] * inverseY + lowerRow[m] * fractionY;

				float *swapRow = upperRow;

				upperRow = lowerRow;
				lowerRow = swapRow;
			}
		}

		int tapDelta[11];

		for (int k = 0; k < 11; k++)
			tapDelta[k] = tapOffsets[k][1] * gridWidth + tapOffsets[k][0];

		for (int y = top; y < bottom; y++) {
			int j = startY + (y - top);

			for (int x = left; x < right; x++) {
				int i = startX + (x - left);
				int center = (j + 2) * gridWidth + (i + 2);
				float cover = _textScratch[center];
				float sum = _textScratch[center + tapDelta[0]];

				for (int k = 1; k < 11; k++)
					sum += _textScratch[center + tapDelta[k]];

				if (!(sum > 0.0f))
					sum = 0.0f;

				if (sum > 1.0f)
					sum = 1.0f;

				float shadow = sum * draw.alpha;
				float alpha = shadow * (1.0f - cover) + draw.alpha * cover;
				float red = (0.0f * (1.0f - cover)) + draw.textRed * cover;
				float green = (0.0f * (1.0f - cover)) + draw.textGreen * cover;
				float blue = (0.0f * (1.0f - cover)) + draw.textBlue * cover;

				if (alpha == 0.0f && red == 0.0f && green == 0.0f && blue == 0.0f)
					continue;

				blendPixel(x, y, red, green, blue, alpha);
			}
		}
	}
}

void HdCanvas::drawMovie(const Item &item) {
	if (!item.pixels || !(item.width > 0.0f) || !(item.height > 0.0f))
		return;

	int left = clampPixel(item.x, kWidth);
	int right = clampPixel(item.x + item.width, kWidth);
	int top = clampPixel(item.y, kHeight);
	int bottom = clampPixel(item.y + item.height, kHeight);

	if (left >= right || top >= bottom)
		return;

	int srcWidth = item.srcWidth;
	int srcHeight = item.srcHeight;
	int columns = right - left;
	bool usePow = item.gamma != 1.0f;

	if (usePow)
		buildPowTable(&_moviePow, item.gamma);

	bool aligned = true;

	_itemColumnFirst.resize(columns);
	_itemColumnSecond.resize(columns);
	_itemColumnFraction.resize(columns);

	for (int i = 0; i < columns; i++) {
		double position = ((double)(left + i) + 0.5 - (double)item.x) * (double)srcWidth / (double)item.width - 0.5;
		int base = (int)floor(position);

		_itemColumnFraction[i] = (float)(position - (double)base);
		_itemColumnFirst[i] = CLIP(base, 0, srcWidth - 1);
		_itemColumnSecond[i] = CLIP(base + 1, 0, srcWidth - 1);

		if (_itemColumnFraction[i] != 0.0f)
			aligned = false;
	}

	uint32 redPixels[256];
	uint32 greenPixels[256];
	uint32 bluePixels[256];

	if (aligned) {
		for (int i = 0; i < 256; i++) {
			uint32 level = usePow ? lookupPow(_moviePow, _byteToFloat[i]) : (uint32)toByte(_byteToFloat[i]);

			redPixels[i] = level << _redShift;
			greenPixels[i] = level << _greenShift;
			bluePixels[i] = level << _blueShift;
		}
	}

	_itemRowIndex[0] = -1;
	_itemRowIndex[1] = -1;

	for (int y = top; y < bottom; y++) {
		double position = ((double)y + 0.5 - (double)item.y) * (double)srcHeight / (double)item.height - 0.5;
		int base = (int)floor(position);
		float fractionY = (float)(position - (double)base);
		float inverseY = 1.0f - fractionY;
		int firstRow = CLIP(base, 0, srcHeight - 1);
		int secondRow = CLIP(base + 1, 0, srcHeight - 1);
		uint32 *destination = _frame + y * kWidth + left;

		if (aligned && fractionY == 0.0f) {
			const byte *row = item.pixels + (size_t)firstRow * (size_t)srcWidth * 4;

			for (int i = 0; i < columns; i++) {
				const byte *a = row + _itemColumnFirst[i] * 4;

				destination[i] = redPixels[a[0]] | greenPixels[a[1]] | bluePixels[a[2]] | _opaqueMask;
			}

			continue;
		}

		const float *upper = getItemRow(item.pixels, srcWidth, firstRow, -1, 3, columns);
		const float *lower = getItemRow(item.pixels, srcWidth, secondRow, firstRow, 3, columns);

		for (int i = 0; i < columns; i++) {
			float red = upper[i * 3] * inverseY + lower[i * 3] * fractionY;
			float green = upper[i * 3 + 1] * inverseY + lower[i * 3 + 1] * fractionY;
			float blue = upper[i * 3 + 2] * inverseY + lower[i * 3 + 2] * fractionY;

			if (usePow)
				destination[i] = packPixel(lookupPow(_moviePow, red), lookupPow(_moviePow, green), lookupPow(_moviePow, blue));
			else
				destination[i] = packPixel(toByte(red), toByte(green), toByte(blue));
		}
	}
}

const float *HdCanvas::getItemRow(const byte *pixels, int srcWidth, int row, int keepRow, int channels, int columns) {
	for (int i = 0; i < 2; i++) {
		if (_itemRowIndex[i] == row)
			return &_itemRows[i][0];
	}

	int slot = _itemRowIndex[0] == keepRow ? 1 : 0;

	_itemRowIndex[slot] = row;
	_itemRows[slot].resize(columns * channels);

	const byte *source = pixels + (size_t)row * (size_t)srcWidth * 4;
	float *destination = &_itemRows[slot][0];

	for (int i = 0; i < columns; i++) {
		const byte *first = source + _itemColumnFirst[i] * 4;
		const byte *second = source + _itemColumnSecond[i] * 4;
		float fraction = _itemColumnFraction[i];
		float inverse = 1.0f - fraction;

		for (int c = 0; c < channels; c++)
			destination[i * channels + c] = _byteToFloat[first[c]] * inverse + _byteToFloat[second[c]] * fraction;
	}

	return destination;
}

} // End of namespace Grim
