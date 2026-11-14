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

#ifndef GRIM_REMASTERED_LOOK_LOOK_TEXTURES_H
#define GRIM_REMASTERED_LOOK_LOOK_TEXTURES_H

#include "common/array.h"
#include "common/hash-ptr.h"
#include "common/hashmap.h"
#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_device.h"

namespace Grim {

class SysStruct;

class RemasteredLookTextureCache {
public:
	explicit RemasteredLookTextureCache(RemasteredLookDevice *device);
	~RemasteredLookTextureCache();

	void clear();
	RemasteredLookTexture *getTexture(const byte *pixels, int width, int height, int bpp, const byte *palette, bool keyFlag, int keyValue,
			bool forceInvalidate, bool zImage);
	void invalidate(const void *key);
	void release(const void *key);
	void loadReplacement(SysStruct *hs, const void *pixels, const char *materialName, int cellIndex);

private:
	enum {
		kNumVariants = 5
	};

	struct Surface {
		RemasteredLookTexture *texture;
		bool valid;

		Surface() : texture(nullptr), valid(false) {}
	};

	struct Entry {
		const byte *palette;
		int width;
		int height;
		int bpp;
		bool keyFlag;
		int keyValue;
		bool zImage;
		Surface surface;
		Surface variants[kNumVariants];
		const byte *variantPalettes[kNumVariants];

		Entry();
	};

	typedef Common::HashMap<const void *, Entry *> EntryMap;
	typedef Common::HashMap<const void *, RemasteredLookTexture *> ReplacementMap;

	void upload(Surface &surface, const byte *pixels, const Entry &entry, const byte *palette, bool keyFlag, int keyValue);
	void freeEntry(Entry *entry);

	RemasteredLookDevice *_remasteredLookDevice;
	EntryMap _entries;
	ReplacementMap _replacements;
};

} // End of namespace Grim

#endif
