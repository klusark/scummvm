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

#include "common/algorithm.h"
#include "common/fs.h"
#include "common/path.h"
#include "common/str-array.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/text/blasttext.h"
#include "engines/grim/text/hdtext.h"
#include "engines/grim/chore/cos.h"
#include "engines/grim/grim.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/text/text.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

static const char *const resourceDirectories[] = {
	"Animation", "Backgrounds", "Costumes", "Colormaps", "Fonts", "Materials", "Models", "Movies", "Objects", "Scripts", "Sets",
	"Voices", "Footsteps", "SFX", "Loops",
	"DEBUG_Animation", "DEBUG_Backgrounds", "DEBUG_Costumes", "DEBUG_Colormaps", "DEBUG_Fonts", "DEBUG_Materials", "DEBUG_Models",
	"DEBUG_Movies", "DEBUG_Objects", "DEBUG_Scripts", "DEBUG_Sets", "DEBUG_Voices", "DEBUG_Footsteps", "DEBUG_SFX", "DEBUG_Loops"
};

static RdMaterial *materialLoader(char *path, int videoMemory, int videoMemoryOnly) {
	return g_grim->getSputResource()->mat(path, videoMemory, videoMemoryOnly);
}

static int materialUnloader(RdMaterial *material) {
	return g_grim->getSputResource()->unlock(material);
}

static int saveVerifier(const char *fileName) {
	return g_grim->getSputResource()->saveVerifyCallback(fileName);
}

SputResource::SputResource(Res *res, RenderDroid *rd, StdDisplay *stdDisplay, StdBitmap *stdBitmap) : _res(res), _rd(rd),
		_stdDisplay(stdDisplay), _stdBitmap(stdBitmap), _sputRender(nullptr), _sputRoom(nullptr), _cos(nullptr),
		_text(nullptr), _blastText(nullptr), _wuRegistry(nullptr), _hs(nullptr), _initialized(0), _started(0), _loaderLocked(0), _totalAllowedLimit(0xBB8000), _purgeAmount(0x271000),
		_oldMatLoader(nullptr), _oldMatUnloader(nullptr) {
}

int SputResource::handler(ResHandlerMsg *msg) {
	switch (msg->tag) {
	case kSputTagBknd:
		return loadResBknd(msg);
	case kSputTagCost:
		return loadResCost(msg);
	case kSputTagCmap:
		return loadResCmap(msg);
	case kSputTagFont:
		return loadResFont(msg);
	case kSputTagKeyf:
		return loadResKeyf(msg);
	case kSputTagMat:
		return loadResMat(msg);
	case kSputTagModl:
		return loadResModl(msg);
	case kSputTagSet:
		return loadResSet(msg);
	case kSputTagSnd:
		return loadResSnd(msg);
	case kSputTagSync:
		return loadResSync(msg);
	case kSputTagZbuf:
		return loadResZbuf(msg);
	case kSputTagMem:
		return loadResMem(msg);
	default:
		break;
	}

	return 0;
}

int SputResource::stopPurge(int state) {
	_loaderLocked = state;

	return state;
}

void *SputResource::alloc(int resType, const char *name) {
	void *handle = _res->getResource(resType, name);

	if (handle && _res->totalAllocated() > _totalAllowedLimit && !_loaderLocked) {
		ResRef ref;
		uint bytesFreed;

		ref.valid = 0;
		_res->setReference(&ref, handle);
		_res->freeResources(_purgeAmount, &bytesFreed);
		_res->removeReference(&ref);
	}

	return handle;
}

int SputResource::closeResources() {
	_res->fileCloseAll();

	return 1;
}

int SputResource::findResources() {
	char dataDirectoryArray[512];

	dataDirectoryArray[0] = 0;
	_wuRegistry->getStr("GrimDataDir", dataDirectoryArray, sizeof(dataDirectoryArray), "");

	Common::String dataDirectory(dataDirectoryArray);
	Common::String currentDirectory(dataDirectory);

	if (!_extraDataPath.empty() && !_extraDataPath.equalsIgnoreCase(dataDirectory))
		addResourcePathInternal(_extraDataPath.c_str());

	if (!currentDirectory.equalsIgnoreCase(_extraDataPath))
		addResourcePathInternal(currentDirectory.c_str());

	return 1;
}

int SputResource::addResourcePath(const char *path) {
	_extraDataPath = path;

	return 1;
}

int SputResource::addResourcePathInternal(const char *path) {
	const char separator = Common::Path::kNativeSeparator;
	Common::String directory(path);

	if (!directory.empty() && (directory.lastChar() == '\\' || directory.lastChar() == '/'))
		directory.deleteLastChar();

	if (directory.empty())
		return 0;

	Common::FSNode node((Common::Path(directory, separator)));
	Common::FSList files;
	Common::FSList subDirectories;
	Common::StringArray labNames;

	node.getChildren(files, Common::FSNode::kListFilesOnly);
	node.getChildren(subDirectories, Common::FSNode::kListDirectoriesOnly);

	for (uint i = 0; i < files.size(); i++) {
		Common::String name(files[i].getName());

		name.toLowercase();

		if (!name.hasSuffix(".lab") || name == "datausr.lab")
			continue;

		labNames.push_back(name);
	}

	Common::sort(labNames.begin(), labNames.end());

	directory += separator;

	if (!_res->fileOpen(directory.c_str()))
		return 0;

	_res->fileMoveToFront(directory.c_str());

	for (uint i = 0; i < labNames.size(); i++) {
		Common::String labPath(directory);

		labPath += labNames[i];

		if (_res->fileOpen(labPath.c_str()))
			_res->fileMoveToFront(labPath.c_str());
	}

	for (uint i = 0; i < ARRAYSIZE(resourceDirectories); i++) {
		for (uint j = 0; j < subDirectories.size(); j++) {
			if (!subDirectories[j].getName().equalsIgnoreCase(resourceDirectories[i]))
				continue;

			Common::String subPath(directory);

			subPath += subDirectories[j].getName();
			subPath += separator;

			if (_res->fileOpen(subPath.c_str()))
				_res->fileMoveToFront(subPath.c_str());
		}
	}

	return 1;
}

int SputResource::initialize(SysStruct *hs) {
	_hs = hs;

	int result = _res->sysInitialize(hs);

	if (!result)
		_initialized = 1;

	return result;
}

int SputResource::logToFile(const char *path) {
	return _res->log(path) == 1;
}

int SputResource::terminate() {
	_res->sysTerminate();
	_hs = nullptr;
	_initialized = 0;

	return 0;
}

int SputResource::startup() {
	if (_started)
		return 1;

	if (!_res->sysStartup())
		_started = registerHandlers();

	_oldMatLoader = _rd->_material.registerLoader(materialLoader);
	_oldMatUnloader = _rd->_material.registerUnloader(materialUnloader);
	_cos->registerSaveVerify(saveVerifier);

	return _started == 0;
}

int SputResource::saveVerifyCallback(const char *fileName) {
	return _res->initLists(kSputTagCost, fileName);
}

int SputResource::shutdown() {
	_res->sysShutdown();
	_rd->_material.registerLoader(_oldMatLoader);
	_rd->_material.registerUnloader(_oldMatUnloader);
	_started = 0;

	return 0;
}

int SputResource::registerHandlers() {
	const int tags[12] = { kSputTagBknd, kSputTagCost, kSputTagCmap, kSputTagFont, kSputTagKeyf, kSputTagMat, kSputTagModl, kSputTagSet,
		kSputTagSnd, kSputTagSync, kSputTagZbuf, kSputTagMem };
	int result = 0;

	for (uint i = 0; i < 12; i++) {
		result = _res->sysRegisterHandler(tags[i], this);

		if (result != 1)
			return 0;
	}

	return result;
}

int SputResource::setMemoryUsage(int totalAllowedLimit, int purgeAmount) {
	if (totalAllowedLimit)
		_totalAllowedLimit = totalAllowedLimit;

	if (purgeAmount)
		_purgeAmount = purgeAmount;

	return purgeAmount;
}

int SputResource::loadResSync(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagSync)
		return 0;

	switch (msg->op) {
	case 1: {
		IMuseLipSync *lipSync = g_grim->getImuse()->grimSoundLipSyncLoad(msg->name);

		if (!lipSync)
			return 0;

		return _res->allocCompletion(msg->tag, msg->name, lipSync);
	}
	case 2:
		g_grim->getImuse()->grimSoundLipSyncFree(static_cast<IMuseLipSync *>(msg->data));

		return 1;
	case 3: {
		IMuseLipSync *lipSync = static_cast<IMuseLipSync *>(msg->data);

		msg->result = lipSync->size + 40;

		return 1;
	}
	default:
		break;
	}

	return 0;
}

int SputResource::loadResBknd(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagBknd)
		return 0;

	switch (msg->op) {
	case 1: {
		Common::String path(msg->name);
		StdBitmapStruct *bitmap = _stdBitmap->loadLbm(path, 0, 0);

		if (!bitmap)
			return 0;

		// The demo does not correct the colors of the backgrounds
		if (!_rd->isDemo() && !bitmap->encoding && bitmap->format.colorMode == 1) {
			for (int i = 0; i < (int)bitmap->numFrames; i++) {
				StdVBuffer *frame = bitmap->frames[i];

				_stdDisplay->vBufferLock(frame);

				for (int row = 0; row < frame->tex.height; row++)
					_sputRender->colorCorrect((uint16 *)(frame->pixels + frame->tex.pitchInBytes * row), frame->tex.width);

				_stdDisplay->vBufferUnlock(frame);

				uint16 colorKey = 63519;

				_sputRender->colorCorrect(&colorKey, 1);
				frame->colorKey = colorKey;
			}
		}

		return _res->allocCompletion(msg->tag, msg->name, bitmap);
	}
	case 2: {
		StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(msg->data);

		_stdBitmap->unloadBitmap(bitmap);

		return 1;
	}
	case 3: {
		StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(msg->data);

		msg->result = _stdBitmap->memUsage(bitmap);

		return 1;
	}
	default:
		break;
	}

	return 0;
}

void SputResource::loadBknd() {
	ResNode *next;
	void *handle;

	_res->allocatedIterFirst(&next, &handle, kSputTagBknd);

	while (handle) {
		StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(handle);

		if (!bitmap->encoding) {
			Common::String path(bitmap->name);
			StdBitmapStruct *newBitmap = _stdBitmap->loadLbm(path, 0, 0);

			if (newBitmap) {
				if (newBitmap->numFrames < bitmap->numFrames) {
					warning("SputResource::loadBknd(): bitmap %s has fewer frames than the loaded one", bitmap->name);
				} else {
					for (int j = 0; j < (int)bitmap->numFrames; j++) {
						StdVBuffer *oldFrame = bitmap->frames[j];

						bitmap->frames[j] = newBitmap->frames[j];
						newBitmap->frames[j] = oldFrame;
					}
				}

				_stdBitmap->unloadBitmap(newBitmap);
			}

			for (int k = 0; k < (int)bitmap->numFrames; k++) {
				StdVBuffer *frame = bitmap->frames[k];

				_stdDisplay->vBufferLock(frame);

				for (int row = 0; row < frame->tex.height; row++)
					_sputRender->colorCorrect((uint16 *)(frame->pixels + frame->tex.pitchInBytes * row), frame->tex.width);

				_stdDisplay->vBufferUnlock(frame);
			}
		}

		uint16 colorKey = 63519;

		_sputRender->colorCorrect(&colorKey, 1);
		bitmap->colorKey = colorKey;

		for (int n = 0; n < (int)bitmap->numFrames; n++) {
			uint16 frameColorKey = 63519;

			_sputRender->colorCorrect(&frameColorKey, 1);
			bitmap->frames[n]->colorKey = frameColorKey;
		}

		_res->allocatedIterNext(&next, &handle, kSputTagBknd);
	}
}

int SputResource::loadResCost(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagCost)
		return 0;

	switch (msg->op) {
	case 1: {
		int costume = _cos->load(msg->name);

		if (!costume)
			return 0;

		return _res->allocCompletion(msg->tag, msg->name, (void *)(intptr)costume);
	}
	case 2:
		_cos->freeCostume((int)(intptr)msg->data);

		return 1;
	case 3:
		msg->result = 1;

		return 1;
	default:
		break;
	}

	return 0;
}

int SputResource::loadResCmap(ResHandlerMsg *msg) {
	StdColorFormat colorFormat;

	colorFormat.colorMode = 1;
	colorFormat.bpp = 16;
	colorFormat.redBits = 5;
	colorFormat.greenBits = 6;
	colorFormat.blueBits = 5;
	colorFormat.redShift = 11;
	colorFormat.greenShift = 5;
	colorFormat.blueShift = 0;
	colorFormat.redLoss = 3;
	colorFormat.greenLoss = 2;
	colorFormat.blueLoss = 3;
	colorFormat.alphaBits = 0;
	colorFormat.alphaShift = 0;
	colorFormat.alphaLoss = 0;

	if (msg->tag != kSputTagCmap)
		return 0;

	switch (msg->op) {
	case 1: {
		RdColormap *colormap = _rd->_colormap.load(const_cast<char *>(msg->name));

		if (!colormap)
			return 0;

		if (colormap->rgb16) {
			_rd->_colormap.buildRGB16(colormap->rgb16, colormap->colors, 0, 0, 0, &colorFormat);
			_sputRender->colorCorrect((uint16 *)colormap->rgb16, 0x4000);
		} else {
			warning("SputResource::loadResCmap(): colormap %s has no RGB16 table", msg->name);
		}

		return _res->allocCompletion(msg->tag, msg->name, colormap);
	}
	case 2: {
		RdColormap *colormap = static_cast<RdColormap *>(msg->data);

		_rd->_colormap.destroy(colormap);

		return 1;
	}
	case 3: {
		RdColormap *colormap = static_cast<RdColormap *>(msg->data);

		msg->result = _rd->_colormap.size(colormap);

		return 1;
	}
	default:
		break;
	}

	return 0;
}

int SputResource::loadResFont(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagFont)
		return 0;

	switch (msg->op) {
	case 1: {
		BlastTextFont *font = nullptr;

		if (!_blastText->load(msg->name, font)) {
			int result = _res->allocCompletion(msg->tag, msg->name, font);

			// The demo can purge the fonts
			if (!_rd->isDemo())
				_res->setPurgeOnFree(font, 0);

			// The remastered draws the font from the TrueType file named in FontsHD
			if (g_grim->isRemastered())
				_blastText->getHdText()->attachFont(font, msg->name);

			return result;
		}

		break;
	}
	case 2: {
		BlastTextFont *font = static_cast<BlastTextFont *>(msg->data);

		_text->freeFont(font);

		return 1;
	}
	case 3:
		msg->result = 1;

		return 1;
	default:
		break;
	}

	return 0;
}

int SputResource::loadResKeyf(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagKeyf)
		return 0;

	switch (msg->op) {
	case 1: {
		RdKeyframe *keyframe = _rd->_keyframe.load(const_cast<char *>(msg->name));

		if (!keyframe)
			return 0;

		// The demo keeps the rate of the file
		if (!_rd->isDemo())
			keyframe->fps = 15.0f;

		return _res->allocCompletion(msg->tag, msg->name, keyframe);
	}
	case 2: {
		RdKeyframe *keyframe = static_cast<RdKeyframe *>(msg->data);

		_rd->_keyframe.destroy(keyframe);

		return 1;
	}
	case 3: {
		RdKeyframe *keyframe = static_cast<RdKeyframe *>(msg->data);

		msg->result = 132;

		for (int i = 0; i < (int)keyframe->numNodes; i++) {
			msg->result += 44;
			msg->result += 56 * keyframe->paNodes[i].numEntries;
		}

		return 1;
	}
	default:
		break;
	}

	return 0;
}

int SputResource::loadResMat(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagMat)
		return 0;

	switch (msg->op) {
	case 1: {
		char defaultName[] = "dflt.mat";
		RdMaterial *material = new RdMaterial;

		if (!_rd->_material.loadEntry(const_cast<char *>(msg->name), material, 0, 0) && !_rd->_material.loadEntry(defaultName, material, 0, 0)) {
			_rd->_material.freeEntry(material);
			delete material;
			material = nullptr;

			return 0;
		}

		int result = _res->allocCompletion(msg->tag, msg->name, material);
		_res->setPurgeOnFree(material, 0);

		return result;
	}
	case 2: {
		RdMaterial *material = static_cast<RdMaterial *>(msg->data);

		_rd->_material.freeEntry(material);
		delete material;
		material = nullptr;

		return 1;
	}
	case 3:
		msg->result = 0;

		return 1;
	default:
		break;
	}

	return 0;
}

int SputResource::loadResModl(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagModl)
		return 0;

	switch (msg->op) {
	case 1: {
		RdModel3 *model3 = _rd->_model3.load(const_cast<char *>(msg->name));

		if (!model3)
			return 0;

		return _res->allocCompletion(msg->tag, msg->name, model3);
	}
	case 2: {
		RdModel3 *model3 = static_cast<RdModel3 *>(msg->data);

		_rd->_model3.destroy(model3);

		return 1;
	}
	case 3:
		msg->result = 1;

		return 1;
	default:
		break;
	}

	return 0;
}

int SputResource::loadResSet(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagSet)
		return 0;

	switch (msg->op) {
	case 1: {
		Room *room = _sputRoom->loadRoom(msg->name);

		if (!room)
			return 0;

		return _res->allocCompletion(msg->tag, msg->name, room);
	}
	case 2: {
		Room *room = static_cast<Room *>(msg->data);

		_sputRoom->freeRoom(room);

		return 1;
	}
	case 3: {
		Room *room = static_cast<Room *>(msg->data);

		msg->result = room->structSize;

		return 1;
	}
	default:
		break;
	}

	return 0;
}

int SputResource::loadResSnd(ResHandlerMsg *msg) {
	return 0;
}

int SputResource::loadResZbuf(ResHandlerMsg *msg) {
	if (msg->tag != kSputTagZbuf)
		return 0;

	switch (msg->op) {
	case 1: {
		StdVBuffer *zBuffer = _sputRoom->loadZBuffer(msg->name);

		if (!zBuffer)
			return 0;

		return _res->allocCompletion(msg->tag, msg->name, zBuffer);
	}
	case 2: {
		StdVBuffer *zBuffer = static_cast<StdVBuffer *>(msg->data);

		_sputRoom->freeZBuffer(zBuffer);

		return 1;
	}
	case 3: {
		StdVBuffer *zBuffer = static_cast<StdVBuffer *>(msg->data);

		msg->result = zBuffer->tex.sizeInBytes;

		return 1;
	}
	default:
		break;
	}

	return 0;
}

int SputResource::loadResMem(ResHandlerMsg *msg) {
	ResMemBlock *block = static_cast<ResMemBlock *>(msg->data);

	if (msg->op == 2) {
		if (!block) {
			warning("SputResource::loadResMem(): no memory block to free");
			return 1;
		}

		delete[] block->data;
		block->data = nullptr;
		delete block;
		block = nullptr;
	} else if (msg->op == 3) {
		if (!block) {
			warning("SputResource::loadResMem(): no memory block to measure");
			return 1;
		}

		msg->result = block->size;

		return 1;
	}

	return 1;
}

int SputResource::cmap() {
	StdColorFormat colorFormat;
	ResNode *next;
	void *handle;

	colorFormat.colorMode = 1;
	colorFormat.bpp = 16;
	colorFormat.redBits = 5;
	colorFormat.greenBits = 6;
	colorFormat.blueBits = 5;
	colorFormat.redShift = 11;
	colorFormat.greenShift = 5;
	colorFormat.blueShift = 0;
	colorFormat.redLoss = 3;
	colorFormat.greenLoss = 2;
	colorFormat.blueLoss = 3;
	colorFormat.alphaBits = 0;
	colorFormat.alphaShift = 0;
	colorFormat.alphaLoss = 0;

	_res->allocatedIterFirst(&next, &handle, kSputTagCmap);

	while (handle) {
		RdColormap *colormap = static_cast<RdColormap *>(handle);

		if (colormap->rgb16) {
			_rd->_colormap.buildRGB16(colormap->rgb16, colormap->colors, 0, 0, 0, &colorFormat);
			_sputRender->colorCorrect((uint16 *)colormap->rgb16, 0x4000);
		} else {
			warning("SputResource::cmap(): colormap %s has no RGB16 table", colormap->name);
		}

		_res->allocatedIterNext(&next, &handle, kSputTagCmap);
	}

	return 0;
}

RdMaterial *SputResource::mat(char *path, int videoMemory, int videoMemoryOnly) {
	RdMaterial *material = static_cast<RdMaterial *>(alloc(kSputTagMat, path));

	if (!material) {
		warning("SputResource::mat(): could not load material %s", path);
		return nullptr;
	}

	if (!material->lockCount)
		_res->setPurgeOnFree(material, 0);

	material->lockCount++;

	return material;
}

int SputResource::unlock(RdMaterial *material) {
	if (!material) {
		warning("SputResource::unlock(): no material given");
		return 0;
	}

	if (material->lockCount)
		material->lockCount--;

	if (!material->lockCount)
		return _res->setPurgeOnFree(material, 1);

	return 1;
}

int SputResource::reportAllocations() {
	return _res->dump("Grim.res", 0);
}

} // End of namespace Grim
