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

#include "common/endian.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"

namespace Grim {

RdMaterialLoaderFunc RenderDroid::MaterialModule::registerLoader(RdMaterialLoaderFunc loader) {
	RdMaterialLoaderFunc previous = _loader;
	_loader = loader;
	return previous;
}

RdMaterialUnloaderFunc RenderDroid::MaterialModule::registerUnloader(RdMaterialUnloaderFunc unloader) {
	RdMaterialUnloaderFunc previous = _unloader;
	_unloader = unloader;
	return previous;
}

RdMaterial *RenderDroid::MaterialModule::load(char *path, int videoMemory, int videoMemoryOnly) {
	if (_loader)
		return _loader(path, videoMemory, videoMemoryOnly);

	RdMaterial *material = new RdMaterial;

	if (loadEntry(path, material, videoMemory, videoMemoryOnly))
		return material;

	destroy(material);
	return nullptr;
}

int RenderDroid::MaterialModule::loadEntry(char *path, RdMaterial *material, int videoMemory, int videoMemoryOnly) {
	memset(material, 0, sizeof(RdMaterial));

	Common::SeekableReadStream *file = _rd->_hs->openFile(path);

	if (!file)
		return 0;

	uint32 magic = file->readUint32BE();
	int version = file->readSint32LE();

	if (magic != MKTAG('M', 'A', 'T', ' ') || version != 50) {
		delete file;
		return 0;
	}

	uint flags = file->readUint32LE();
	uint numCels = file->readUint32LE();
	uint numTextures = file->readUint32LE();

	if (numCels > 16) {
		warning("RenderDroid::MaterialModule::loadEntry(): too many cels in %s", path);
		delete file;
		return 0;
	}

	material->numTextures = numTextures;
	material->flags = flags;
	material->numCels = numCels;
	material->curCel = 0;

	StdColorFormat *format = &material->format;
	format->colorMode = file->readSint32LE();
	format->bpp = file->readSint32LE();
	format->redBits = file->readSint32LE();
	format->greenBits = file->readSint32LE();
	format->blueBits = file->readSint32LE();
	format->redShift = file->readSint32LE();
	format->greenShift = file->readSint32LE();
	format->blueShift = file->readSint32LE();
	format->redLoss = file->readSint32LE();
	format->greenLoss = file->readSint32LE();
	format->blueLoss = file->readSint32LE();
	format->alphaBits = file->readSint32LE();
	format->alphaShift = file->readSint32LE();
	format->alphaLoss = file->readSint32LE();

	uint textureIndices[16];
	memset(textureIndices, 0, sizeof(textureIndices));

	for (uint i = 0; i < numCels; i++) {
		RdMatCel *cel = new RdMatCel;
		memset(cel, 0, sizeof(RdMatCel));
		material->cels[i] = cel;

		cel->flags = file->readUint32LE();
		cel->colorIndex = file->readSint32LE();
		cel->reserved8 = file->readSint32LE();
		cel->reserved12 = file->readSint32LE();
		cel->reserved16 = file->readSint32LE();
		cel->reserved20 = file->readSint32LE();

		if (cel->flags & 8) {
			cel->reserved24 = file->readSint32LE();
			file->skip(8);
			textureIndices[i] = file->readUint32LE();
		}
	}

	material->textures = nullptr;

	if (numTextures) {
		material->textures = new RdTexture[numTextures];
		memset(material->textures, 0, sizeof(RdTexture) * numTextures);
	}

	for (uint i = 0; i < numTextures; i++) {
		RdTexture *texture = &material->textures[i];

		int width = file->readSint32LE();
		int height = file->readSint32LE();
		texture->flags = file->readUint32LE();
		texture->reserved4 = file->readSint32LE();
		texture->colorKey = file->readSint32LE();
		uint numMips = file->readUint32LE();

		if (numMips > 4) {
			warning("RenderDroid::MaterialModule::loadEntry(): too many mip levels in %s", path);
			delete file;
			return 0;
		}

		int log2Width = 0;
		for (int w = width; w > 1; w >>= 1)
			log2Width++;

		texture->widthLog2 = log2Width;
		texture->widthMask = width - 1;
		texture->heightMask = height - 1;
		texture->numMips = numMips;

		StdVBufferTexFmt texFormat;
		memset(&texFormat, 0, sizeof(StdVBufferTexFmt));
		texFormat.width = width;
		texFormat.height = height;
		texFormat.format.colorMode = material->format.colorMode;
		texFormat.format.bpp = material->format.bpp;

		for (uint mip = 0; mip < numMips; mip++) {
			texture->cache[0][mip].inCache = 0;
			texture->cache[0][mip].lastFrame = 0;
			texture->cache[1][mip].inCache = 0;
			texture->cache[1][mip].lastFrame = 0;

			texture->mips[mip] = _rd->_stdDisplay->vBufferNew(&texFormat, videoMemory, videoMemoryOnly, nullptr);

			if (texture->flags & 1)
				_rd->_stdDisplay->vBufferSetColorKey(texture->mips[mip], texture->colorKey);

			_rd->_stdDisplay->vBufferLock(texture->mips[mip]);
			file->read(texture->mips[mip]->pixels, texture->mips[mip]->tex.sizeInBytes);
			_rd->_stdDisplay->vBufferUnlock(texture->mips[mip]);

			texFormat.width = (uint)texFormat.width >> 1;
			texFormat.height = (uint)texFormat.height >> 1;
		}
	}

	for (uint i = 0; i < numCels; i++) {
		if (material->cels[i]->flags & 8) {
			if (textureIndices[i] >= numTextures) {
				warning("RenderDroid::MaterialModule::loadEntry(): texture index out of range in %s", path);
				delete file;
				return 0;
			}

			material->cels[i]->texture = &material->textures[textureIndices[i]];
		}
	}

	if (_rd->_remasteredLookRenderer) {
		for (uint t = 0; t < numTextures; t++) {
			RdTexture *texture = &material->textures[t];

			if (!texture->numMips)
				continue;

			int owner = 0;

			for (uint i = 0; i < numCels; i++) {
				if ((material->cels[i]->flags & 8) && textureIndices[i] == t)
					owner = (int)i;
			}

			_rd->_remasteredLookRenderer->notifyMaterialLoad(texture->mips[0]->pixels, path, owner);
		}
	}

	if (material->flags & 1)
		file->skip(768);

	Common::strlcpy(material->name, _rd->_stdFile->fromPath(path), 32);

	delete file;
	return 1;
}

int RenderDroid::MaterialModule::destroy(RdMaterial *&material) {
	if (_unloader) {
		int result = _unloader(material);
		material = nullptr;
		return result;
	}

	freeEntry(material);
	delete material;
	material = nullptr;

	return 1;
}

void RenderDroid::MaterialModule::freeEntry(RdMaterial *material) {
	if (_rd->_remasteredLookRenderer)
		_rd->_remasteredLookRenderer->notifyFree(material);

	for (uint i = 0; i < material->numCels; i++) {
		delete material->cels[i];
		material->cels[i] = nullptr;
	}

	if (!material->textures)
		return;

	for (uint i = 0; i < material->numTextures; i++) {
		RdTexture *texture = &material->textures[i];

		for (uint mip = 0; mip < texture->numMips; mip++) {
			_rd->_stdDisplay->vBufferFree(texture->mips[mip]);

#if defined(GRIM_STD3D)
			RdTexCacheEntry *entry = &texture->cache[0][mip];

			if (entry->inCache && _rd->_std3D) {
				_rd->_std3D->releaseTexture(entry);
				entry->inCache = 0;
				entry->lastFrame = 0;
				_rd->_std3D->cacheListRemove(entry);
			}
#endif
		}
	}

	delete[] material->textures;
	material->textures = nullptr;
}

int RenderDroid::MaterialModule::addToTextureCache(RdMaterial *, RdTexture *texture, int mipLevel, int alt) {
#if defined(GRIM_STD3D)
	StdVBuffer *mip = texture->mips[mipLevel];
	RdTexCacheEntry *entry = &texture->cache[alt ? 1 : 0][mipLevel];

	if (entry->inCache) {
		_rd->_std3D->cacheListTouch(entry);
		return 1;
	}

	if (_rd->_std3D->addToTextureCache(mip, entry, texture->flags & 1, alt))
		return 1;
#endif

	return 0;
}

} // End of namespace Grim
