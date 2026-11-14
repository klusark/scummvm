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

#include "common/textconsole.h"

#include "engines/grim/remastered/look/look_textures.h"
#include "engines/grim/remastered/look/materialpath.h"
#include "engines/grim/remastered/look/mcb.h"

#include <string.h>

namespace Grim {

RemasteredLookTextureCache::Entry::Entry() : palette(nullptr), width(0), height(0), bpp(0), keyFlag(false), keyValue(0), zImage(false) {
	for (int i = 0; i < kNumVariants; i++)
		variantPalettes[i] = nullptr;
}

RemasteredLookTextureCache::RemasteredLookTextureCache(RemasteredLookDevice *device) : _remasteredLookDevice(device) {
}

RemasteredLookTextureCache::~RemasteredLookTextureCache() {
	clear();
}

void RemasteredLookTextureCache::freeEntry(Entry *entry) {
	if (entry->surface.texture)
		_remasteredLookDevice->destroyTexture(entry->surface.texture);

	for (int i = 0; i < kNumVariants; i++) {
		if (entry->variants[i].texture)
			_remasteredLookDevice->destroyTexture(entry->variants[i].texture);
	}

	delete entry;
}

void RemasteredLookTextureCache::clear() {
	for (EntryMap::iterator it = _entries.begin(); it != _entries.end(); ++it)
		freeEntry(it->_value);

	_entries.clear();

	for (ReplacementMap::iterator it = _replacements.begin(); it != _replacements.end(); ++it)
		_remasteredLookDevice->destroyTexture(it->_value);

	_replacements.clear();
}

void RemasteredLookTextureCache::upload(Surface &surface, const byte *pixels, const Entry &entry, const byte *palette, bool keyFlag, int keyValue) {
	int width = entry.width;
	int height = entry.height;
	Common::Array<byte> converted;
	LookImage image;

	image.width = width;
	image.height = height;
	image.numLevels = 1;
	image.levels[0].width = width;
	image.levels[0].height = height;

	if (palette) {
		converted.resize((uint)width * height * 4);

		for (int i = 0; i < width * height; i++) {
			byte index = pixels[i];
			byte *out = &converted[i * 4];

			if (keyFlag && (int)index == keyValue) {
				out[0] = out[1] = out[2] = out[3] = 0;
			} else {
				out[0] = palette[3 * index];
				out[1] = palette[3 * index + 1];
				out[2] = palette[3 * index + 2];
				out[3] = 255;
			}
		}

		image.format = kLookFormatRgba8;
		image.levels[0].data = &converted[0];
		image.levels[0].size = converted.size();
	} else if (entry.bpp == 16) {
		image.format = entry.zImage ? kLookFormatZ16 : kLookFormatRgb565;
		image.levels[0].data = pixels;
		image.levels[0].size = (uint)width * height * 2;
	} else {
		converted.resize((uint)width * height * 4);
		memset(&converted[0], 0, converted.size());
		image.format = kLookFormatRgba8;
		image.levels[0].data = &converted[0];
		image.levels[0].size = converted.size();
	}

	if (surface.texture) {
		_remasteredLookDevice->updateTexture(surface.texture, image.levels[0].data, width, height);
	} else {
		surface.texture = _remasteredLookDevice->createTexture(image);
	}

	surface.valid = true;
}

RemasteredLookTexture *RemasteredLookTextureCache::getTexture(const byte *pixels, int width, int height, int bpp, const byte *palette, bool keyFlag, int keyValue,
		bool forceInvalidate, bool zImage) {
	if (!pixels)
		return nullptr;

	if (!_replacements.empty()) {
		ReplacementMap::iterator replacement = _replacements.find(pixels);

		if (replacement != _replacements.end())
			return replacement->_value;
	}

	EntryMap::iterator it = _entries.find(pixels);

	if (it == _entries.end()) {
		Entry *entry = new Entry();

		entry->palette = palette;
		entry->width = width;
		entry->height = height;
		entry->bpp = bpp;
		entry->keyFlag = keyFlag;
		entry->keyValue = keyValue;
		entry->zImage = zImage;
		upload(entry->surface, pixels, *entry, palette, keyFlag, keyValue);
		_entries[pixels] = entry;

		return entry->surface.texture;
	}

	Entry *entry = it->_value;
	const byte *oldPalette = entry->palette;

	if (entry->width != width || entry->height != height || entry->bpp != bpp)
		warning("RemasteredLookTextureCache::getTexture(): the cached texture of %p is %dx%d/%d but %dx%d/%d was requested", (const void *)pixels,
				entry->width, entry->height, entry->bpp, width, height, bpp);

	if (entry->bpp == 16 || (entry->keyFlag == keyFlag && entry->keyValue == keyValue)) {
		if (forceInvalidate)
			entry->surface.valid = false;
	} else {
		entry->surface.valid = false;
	}

	if (entry->surface.valid && oldPalette == palette)
		return entry->surface.texture;

	bool paletteChanged = oldPalette != palette && entry->surface.valid;

	if (paletteChanged) {
		int slot = 0;

		while (slot <= 3 && entry->variantPalettes[slot] && entry->variantPalettes[slot] != palette)
			slot++;

		Surface &variant = entry->variants[slot];

		if (variant.texture) {
			if (paletteChanged)
				return variant.texture;
		} else {
			entry->variantPalettes[slot] = palette;
		}

		entry->keyFlag = keyFlag;
		entry->keyValue = keyValue;
		upload(variant, pixels, *entry, palette, keyFlag, keyValue);

		return variant.texture;
	}

	entry->palette = palette;
	entry->keyFlag = keyFlag;
	entry->keyValue = keyValue;
	upload(entry->surface, pixels, *entry, palette, keyFlag, keyValue);

	return entry->surface.texture;
}

void RemasteredLookTextureCache::invalidate(const void *key) {
	EntryMap::iterator it = _entries.find(key);

	if (it != _entries.end())
		it->_value->surface.valid = false;
}

void RemasteredLookTextureCache::release(const void *key) {
	EntryMap::iterator it = _entries.find(key);

	if (it != _entries.end()) {
		freeEntry(it->_value);
		_entries.erase(it);
	}

	ReplacementMap::iterator replacement = _replacements.find(key);

	if (replacement != _replacements.end()) {
		_remasteredLookDevice->destroyTexture(replacement->_value);
		_replacements.erase(replacement);
	}
}

void RemasteredLookTextureCache::loadReplacement(SysStruct *hs, const void *pixels, const char *materialName, int cellIndex) {
	if (!pixels || _replacements.contains(pixels))
		return;

	Mcb mcb;

	if (!MaterialPath::load(hs, materialName, cellIndex, mcb))
		return;

	uint levels = mcb.getUploadedMipCount();

	if (levels == 0 || levels > kLookMaxLevels)
		return;

	LookImage image;
	LookFormat format = kLookFormatDxt1;

	if (mcb.getFormat() == S3tc::kDxt3)
		format = kLookFormatDxt3;
	else if (mcb.getFormat() == S3tc::kDxt5)
		format = kLookFormatDxt5;

	image.format = format;
	image.width = mcb.getWidth();
	image.height = mcb.getHeight();
	image.numLevels = levels;

	for (uint level = 0; level < levels; level++) {
		image.levels[level].data = mcb.getLevelData(level);
		image.levels[level].size = mcb.getLevelSize(level);
		image.levels[level].width = mcb.getLevelWidth(level);
		image.levels[level].height = mcb.getLevelHeight(level);
	}

	RemasteredLookTexture *texture = _remasteredLookDevice->createTexture(image);

	if (texture)
		_replacements[pixels] = texture;
}

} // End of namespace Grim
