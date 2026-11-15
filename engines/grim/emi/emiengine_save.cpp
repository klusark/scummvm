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

#include "common/language.h"
#include "common/memstream.h"
#include "common/platform.h"
#include "common/savefile.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "graphics/pixelformat.h"
#include "graphics/surface.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/resources/cbundle.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/images/labitmap.h"
#include "engines/grim/emi/graphics/lcolorformat.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/movie/movie.h"
#include "engines/grim/emi/scene/overworld.h"
#include "engines/grim/emi/resources/resourcemanager.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/emi/text/textevent.h"
#include "engines/grim/emi/files/zfile.h"
#include "engines/grim/legacy_savegames/legacy_savegames.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

struct SpecScreenRect {
	int x;
	int y;
	int width;
	int height;
};

struct Ps2ScreenRegion {
	int width;
	int height;
	float startX;
	float endX;
	float startY;
	float endY;
};

static const int kSavedTextBufferSize = 512;
static const int kThumbnailTextureWidth = 256;
static const int kThumbnailTextureHeight = 128;
static const int kThumbnailImageWidth = 160;
static const int kThumbnailImageHeight = 120;
static const int kScreenWidth = 640;
static const int kScreenBufferHeight = 512;
static const int kNumSpecScreenTextures = 8;
static const uint kSpecScreenTextureFlags = 775;
static const uint kMaxSaveTableSize = 0x10000;
static const uint kMemoryCardNoFolder = 2;
static const uint kMemoryCardError = 3;
static const uint kMemoryCardFirstId = 4;
static const char *const kMemoryCardIdFileName = "monkey4ps2.card";
static const char *const kMemoryCardSavePattern = "efmi-ps2###.gsv";
static const char *const kSaveFileExtension = ".gsv";

static const SpecScreenRect kSpecScreenRects[kNumSpecScreenTextures] = {
	{ 0, 0, 256, 256 },
	{ 256, 0, 256, 256 },
	{ 512, 0, 128, 128 },
	{ 512, 128, 128, 128 },
	{ 0, 256, 256, 256 },
	{ 256, 256, 256, 256 },
	{ 512, 256, 128, 128 },
	{ 512, 384, 128, 128 }
};

static const int kScreenReadHeight = 480;
static const int kNumPs2ScreenTextures = 6;

static const Ps2ScreenRegion kPs2ScreenRegions[kNumPs2ScreenTextures] = {
	{ 256, 256, -1.0f, -0.2f, -1.0f, 0.0f },
	{ 256, 256, -0.2f, 0.6f, -1.0f, 0.0f },
	{ 128, 256, 0.6f, 1.0f, -1.0f, 0.0f },
	{ 256, 256, -1.0f, -0.2f, 0.0f, 1.0f },
	{ 256, 256, -0.2f, 0.6f, 0.0f, 1.0f },
	{ 128, 256, 0.6f, 1.0f, 0.0f, 1.0f }
};

Common::Error EmiEngine::loadGameState(int slot) {
	assert(slot >= 0);

	// PS2 scripts compare the loaded name with the card file names
	if (getGamePlatform() == Common::kPlatformPS2)
		loadLib(Common::String::format("efmi%03d", slot).c_str());
	else
		loadLib(Common::String::format("Saves/efmi%03d.gsv", slot).c_str());

	return Common::kNoError;
}

Common::String EmiEngine::getSaveFileName(const char *path) {
	Common::String name = path;

	for (uint i = 0; i < name.size(); i++) {
		if (name[i] == '\\')
			name.setChar('/', i);
	}

	while (name.hasPrefix("./"))
		name = name.substr(2);

	if (name.hasPrefixIgnoreCase("saves/"))
		name = name.substr(6);

	name = applySavePrefix(name, getGamePlatform(), getGameLanguage());

	// PS2 scripts use slot names like efmi000 without an extension
	if (getGamePlatform() == Common::kPlatformPS2 && !name.hasSuffixIgnoreCase(kSaveFileExtension))
		name += kSaveFileExtension;

	return name;
}

Common::String EmiEngine::getSavePrefix(Common::Platform platform, Common::Language language) {
	// The platform wins, so a PS2 save never gets a language prefix
	if (platform == Common::kPlatformPS2)
		return "efmi-ps2";

	if (language == Common::KO_KOR || language == Common::ZH_CHN || language == Common::ZH_TWN)
		return Common::String("efmi-") + Common::getLanguageCode(language);

	return "efmi";
}

Common::String EmiEngine::applySavePrefix(const Common::String &name, Common::Platform platform, Common::Language language) {
	Common::String prefix = getSavePrefix(platform, language);

	// Scripts use slot names like efmi000; PS2 and CJK files get their own prefix so they never clash with the other saves
	if (prefix != "efmi" && name.size() > 4 && name.hasPrefixIgnoreCase("efmi") &&
			(Common::isDigit(name[4]) || name[4] == '*' || name[4] == '?'))
		return prefix + name.substr(4);

	return name;
}

LFile *EmiEngine::openSaveFile(const char *fileName, const char *mode) {
	if (!fileName || !*fileName) {
		warning("EmiEngine::openSaveFile(): no file name");

		return nullptr;
	}

	Common::String saveName = getSaveFileName(fileName);
	Common::SaveFileManager *saveFileManager = g_system->getSavefileManager();
	LFile *file = new LFile(this);

	if (strchr(mode, 'w')) {
		file->_writeStream = saveFileManager->openForSaving(saveName, false);
		file->_openFlags = 18;
	} else {
		file->_readStream = saveFileManager->openRawFile(saveName);
	}

	if (!file->isOpen()) {
		delete file;

		return nullptr;
	}

	file->_fileName = saveName;

	ZFile *zFile = _zFileServices->createZFile(file, mode);

	if (!zFile) {
		delete file;

		return nullptr;
	}

	return zFile;
}

void EmiEngine::closeSaveFile(LFile *file) {
	if (file)
		_zFileServices->closeZFile(static_cast<ZFile *>(file));
}

// The Lua and sound code take plain function pointers, so these two reach the engine through g_grim
int EmiEngine::writeSaveData(void *data, int bytes) {
	EmiEngine *engine = static_cast<EmiEngine *>(g_grim);

	if (bytes) {
		if (!engine->_saveFile) {
			warning("EmiEngine::writeSaveData(): no save file");

			return 1;
		}

		engine->_saveFile->writeCompressed(data, bytes);
	}

	return 1;
}

int EmiEngine::readSaveData(void *data, int bytes) {
	EmiEngine *engine = static_cast<EmiEngine *>(g_grim);

	if (bytes) {
		if (!engine->_saveFile) {
			warning("EmiEngine::readSaveData(): no save file");

			return 1;
		}

		engine->_saveFile->read(data, bytes);
	}

	return 1;
}

bool EmiEngine::readSavedText(SaveRestoreAccess *accessFunc, char *buffer, int bufferSize) {
	int length = 0;

	accessFunc(&length, sizeof(length));

	if (length < 0 || length > bufferSize) {
		warning("EmiEngine::readSavedText(): invalid text length %d", length);
		buffer[0] = 0;

		return false;
	}

	accessFunc(buffer, length);

	if (length == 0) {
		buffer[0] = 0;
	} else if (buffer[length - 1] != 0) {
		warning("EmiEngine::readSavedText(): the text is not terminated");
		buffer[length - 1] = 0;
	}

	return true;
}

void EmiEngine::writeSavedText(SaveRestoreAccess *accessFunc, const LString &text) {
	int length = (int)text.size() + 1;

	accessFunc(&length, sizeof(length));
	accessFunc((void *)text.c_str(), length);
}

LABitmap *EmiEngine::getScreenBitmap() {
	if (!_screenBitmap)
		_screenBitmap = new LABitmap(this);

	return _screenBitmap;
}

void EmiEngine::halveBitmap(LABitmap *source, LABitmap *destination) {
	LColorFormat rgbFormat(6);
	int bytesPerPixel = rgbFormat._bytesPerPixel;
	int sourceStride = bytesPerPixel * source->getWidth();
	int halfWidth = (int)((uint)source->getWidth() >> 1);
	int halfHeight = (int)((uint)source->getHeight() >> 1);

	destination->create(halfWidth, halfHeight, rgbFormat, nullptr);

	const byte *first = (const byte *)source->getPixels();
	byte *target = (byte *)destination->getPixels();

	if (halfHeight > 0 && (!first || !target)) {
		warning("EmiEngine::halveBitmap(): the bitmap has no pixels");

		return;
	}

	for (int y = 0; y < halfHeight; y++) {
		if (halfWidth > 0) {
			const byte *second = first + sourceStride;

			for (int x = 0; x < halfWidth; x++) {
				const byte *firstNext = first + bytesPerPixel;
				const byte *secondNext = second + bytesPerPixel;

				int alpha = rgbFormat.getAlpha(first);
				int red = rgbFormat.getRed(first);
				int green = rgbFormat.getGreen(first);
				int blue = rgbFormat.getBlue(first);

				alpha += rgbFormat.getAlpha(second);
				red += rgbFormat.getRed(second);
				green += rgbFormat.getGreen(second);
				blue += rgbFormat.getBlue(second);

				alpha += rgbFormat.getAlpha(firstNext);
				red += rgbFormat.getRed(firstNext);
				green += rgbFormat.getGreen(firstNext);
				blue += rgbFormat.getBlue(firstNext);

				alpha += rgbFormat.getAlpha(secondNext);
				red += rgbFormat.getRed(secondNext);
				green += rgbFormat.getGreen(secondNext);
				blue += rgbFormat.getBlue(secondNext);

				first = firstNext + bytesPerPixel;
				second = secondNext + bytesPerPixel;

				rgbFormat.packPixel(target, ((uint)alpha + 1) >> 2, ((uint)red + 1) >> 2, ((uint)green + 1) >> 2, ((uint)blue + 1) >> 2);
				target += bytesPerPixel;
			}
		}

		first += sourceStride;
	}
}

void EmiEngine::scaleToThumbnail(LABitmap *source, LABitmap *destination) {
	LColorFormat rgbFormat(6);
	int bytesPerPixel = rgbFormat._bytesPerPixel;

	destination->create(kThumbnailTextureWidth, kThumbnailTextureHeight, rgbFormat, nullptr);

	const byte *sourcePixels = (const byte *)source->getPixels();
	byte *targetPixels = (byte *)destination->getPixels();

	if (!sourcePixels || !targetPixels || source->getWidth() <= 0 || source->getHeight() <= 0) {
		warning("EmiEngine::scaleToThumbnail(): bitmap has no pixels");

		return;
	}

	float stepX = (float)source->getWidth() / (float)kThumbnailTextureWidth;
	float stepY = (float)source->getHeight() / (float)kThumbnailTextureHeight;

	for (int row = 0; row < kThumbnailTextureHeight; row++) {
		const byte *sourceLine = sourcePixels + (int)((float)row * stepY) * source->_stride;
		byte *targetLine = targetPixels + row * destination->_stride;

		for (int column = 0; column < kThumbnailTextureWidth; column++)
			memcpy(targetLine + column * bytesPerPixel, sourceLine + (int)((float)column * stepX) * bytesPerPixel, bytesPerPixel);
	}
}

bool EmiEngine::grabScreen() {
	CDrawContext *drawContext = _drawContext;

	if (!drawContext) {
		warning("EmiEngine::grabScreen(): no draw context");

		return false;
	}

	drawContext->readBackBuffer(getScreenBitmap());

	return true;
}

bool EmiEngine::makeThumbnailTexture(int index, bool fromScreen) {
	CDrawContext *drawContext = _drawContext;

	if (!drawContext) {
		warning("EmiEngine::makeThumbnailTexture(): no draw context");

		return false;
	}

	LABitmap screen(this);
	LColorFormat rgbFormat(6);

	drawContext->readBackBuffer(&screen);

	if (fromScreen)
		grabScreen();

	screen.convertFormat(rgbFormat);

	LABitmap halved(this);

	halveBitmap(&screen, &halved);
	halveBitmap(&halved, &screen);

	int width = 1;

	while (width < screen._width)
		width *= 2;

	int height = 1;

	while (height < screen._height)
		height *= 2;

	LABitmap texture(this, width, height, screen._colorFormat, nullptr, false);
	const byte *sourcePixels = (const byte *)screen._pixels;
	byte *targetPixels = (byte *)texture._pixels;

	if (screen._height > 0 && (!sourcePixels || !targetPixels)) {
		warning("EmiEngine::makeThumbnailTexture(): the bitmap has no pixels");

		return false;
	}

	for (int row = 0; row < screen._height; row++)
		memcpy(targetPixels + row * texture._stride, sourcePixels + row * screen._stride, screen._stride);

	return drawContext->setCachedTexture(index, &texture);
}

bool EmiEngine::loadThumbnailTexture(int index, const char *fileName) {
	bool isPs2 = getGamePlatform() == Common::kPlatformPS2;
	LABitmap bitmap(this);
	LColorFormat rgbFormat(6);
	LColorFormat imageFormat(isPs2 ? 15 : 6);

	bitmap.create(kThumbnailTextureWidth, kThumbnailTextureHeight, imageFormat, nullptr);

	_saveFile = static_cast<ZFile *>(openSaveFile(fileName, "rb"));

	if (!_saveFile)
		return false;

	int tableSize = 0;

	readSaveData(&tableSize, sizeof(tableSize));
	_saveFile->seek(tableSize, 1);

	if (!isPs2) {
		char version[kSavedTextBufferSize + 1];

		readSavedText(readSaveData, version, kSavedTextBufferSize);
	}

	uint imageSize = 0;

	readSaveData(&imageSize, sizeof(imageSize));

	if (imageSize > (uint)bitmap._sizeBytes) {
		warning("EmiEngine::loadThumbnailTexture(): invalid image size %u", imageSize);
		imageSize = bitmap._sizeBytes;
	}

	readSaveData(bitmap._pixels, imageSize);
	closeSaveFile(_saveFile);
	_saveFile = nullptr;

	if (isPs2)
		bitmap.convertFormat(rgbFormat);

	if (!_drawContext) {
		warning("EmiEngine::loadThumbnailTexture(): no draw context");

		return false;
	}

	_drawContext->setCachedTexture(index, &bitmap);

	return true;
}

bool EmiEngine::buildSpecScreenTextures(int index) {
	CDrawContext *drawContext = _drawContext;

	if (!drawContext) {
		warning("EmiEngine::buildSpecScreenTextures(): no draw context");

		return false;
	}

	int bytesPerPixel = drawContext->_displayMode.bpp / 8;
	uint bufferSize = (uint)(kScreenWidth * kScreenBufferHeight * bytesPerPixel) * sizeof(char);
	char *buffer = (char *)_memoryServices->mallocImpl(bufferSize);

	if (!buffer) {
		warning("EmiEngine::buildSpecScreenTextures(): out of memory");

		return false;
	}

	// The disasm leaves the rows below the back buffer uninitialized
	memset(buffer, 0, bufferSize);
	drawContext->readBackBufferRaw(buffer, bufferSize);

	LColorFormat format(bytesPerPixel == 4 ? LColorFormat::kFormat9 : LColorFormat::kFormat5);

	if (getGamePlatform() == Common::kPlatformPS2) {
		for (int i = 0; i < kNumPs2ScreenTextures; i++) {
			const Ps2ScreenRegion &region = kPs2ScreenRegions[i];
			LABitmap bitmap(this, region.width, region.height, format, nullptr, false);

			readScreenRegion(&bitmap, buffer, bytesPerPixel, region.startX, region.endX, region.startY, region.endY);
			drawContext->setCachedTexture(index + i, &bitmap);
		}

		_memoryServices->freeImpl(buffer);

		return true;
	}

	for (int i = 0; i < kNumSpecScreenTextures; i++) {
		const SpecScreenRect &rect = kSpecScreenRects[i];

		drawContext->setCachedTexture(index + i, rect.width, rect.height, format, nullptr, "SpecScreenTex");

		CTexture *texture = drawContext->getCachedTexture(index + i);

		if (texture) {
			texture->updateTexture(&format, rect.width, rect.height, kScreenWidth * bytesPerPixel,
					(const byte *)&buffer[bytesPerPixel * (rect.x + kScreenWidth * rect.y)]);
			texture->_flags = kSpecScreenTextureFlags;
			texture->applyFlags();
		}
	}

	_memoryServices->freeImpl(buffer);

	return true;
}

void EmiEngine::readScreenRegion(LABitmap *bitmap, const char *buffer, int bytesPerPixel, float startX, float endX, float startY, float endY) {
	byte *targetPixels = (byte *)bitmap->_pixels;

	if (!targetPixels || bitmap->_width <= 0 || bitmap->_height <= 0) {
		warning("EmiEngine::readScreenRegion(): bitmap has no pixels");

		return;
	}

	// On PS2 the frame starts 46 rows down in GS memory, while ours starts at row 0
	float halfWidth = (float)kScreenWidth / 2.0f;
	float halfHeight = (float)kScreenReadHeight / 2.0f;
	float left = halfWidth + startX * halfWidth;
	float right = halfWidth + endX * halfWidth;
	float top = halfHeight + startY * halfHeight;
	float bottom = halfHeight + endY * halfHeight;
	float stepX = (right - left) / (float)bitmap->_width;
	float stepY = (bottom - top) / (float)bitmap->_height;

	for (int row = 0; row < bitmap->_height; row++) {
		int sourceRow = (int)(top + (float)row * stepY);
		const char *sourceLine = buffer + sourceRow * kScreenWidth * bytesPerPixel;
		byte *targetLine = targetPixels + row * bitmap->_stride;

		for (int column = 0; column < bitmap->_width; column++) {
			int sourceColumn = (int)(left + (float)column * stepX);

			memcpy(targetLine + column * bytesPerPixel, sourceLine + sourceColumn * bytesPerPixel, bytesPerPixel);
		}
	}
}

bool EmiEngine::clearTextureSlot(int index) {
	if (!_drawContext) {
		warning("EmiEngine::clearTextureSlot(): no draw context");

		return false;
	}

	LABitmap bitmap(this);
	LColorFormat format(9);

	bitmap.create(2, 2, format, nullptr);

	uint32 *pixels = (uint32 *)bitmap._pixels;

	if (pixels) {
		for (int i = 0; i < 4; i++)
			pixels[i] = 0xFFFFFFFF;
	}

	return _drawContext->setCachedTexture(index, &bitmap);
}

void EmiEngine::saveGame(CDrawContext *drawContext) {
	bool isPs2 = getGamePlatform() == Common::kPlatformPS2;
	bool cardWasEmpty = isPs2 && !hasMemoryCardSaves();

	_libSaveRequest = 0;
	setSaveLoadIdle(false);

	_saveFile = static_cast<ZFile *>(openSaveFile(_libSaveGameFileName.c_str(), "wb"));

	bool fileOpened = _saveFile != nullptr;

	if (_saveFile) {
		_lua->saveGameCallback(writeSaveData);

		LColorFormat rgbFormat(6);
		LABitmap *screen = getScreenBitmap();

		screen->convertFormat(rgbFormat);

		if (isPs2) {
			LABitmap thumbnail(this);

			scaleToThumbnail(screen, &thumbnail);
			thumbnail.convertFormat(LColorFormat(15));

			int imageSize = thumbnail._sizeBytes;

			writeSaveData(&imageSize, sizeof(imageSize));
			writeSaveData(thumbnail._pixels, imageSize);
			saveGameData(writeSaveData);
		} else {
			LString version;

			_lua->getVersion(version);

			int length = (int)version.size() + 1;

			writeSaveData(&length, sizeof(length));
			writeSaveData((void *)version.c_str(), length);

			LABitmap halved(this);

			halveBitmap(screen, &halved);
			halveBitmap(&halved, screen);

			int width = 1;

			while (width < screen->_width)
				width *= 2;

			int height = 1;

			while (height < screen->_height)
				height *= 2;

			LABitmap thumbnail(this, width, height, screen->_colorFormat, nullptr, false);
			const byte *sourcePixels = (const byte *)screen->_pixels;
			byte *targetPixels = (byte *)thumbnail._pixels;

			if (screen->_height > 0 && (!sourcePixels || !targetPixels)) {
				warning("EmiEngine::saveGame(): the screenshot has no pixels");
			} else {
				for (int row = 0; row < screen->_height; row++)
					memcpy(targetPixels + row * thumbnail._stride, sourcePixels + row * screen->_stride, screen->_stride);
			}

			int imageSize = thumbnail._sizeBytes;

			writeSaveData(&imageSize, sizeof(imageSize));
			writeSaveData(thumbnail._pixels, imageSize);

			length = (int)_island.size() + 1;

			writeSaveData(&length, sizeof(length));
			writeSaveData((void *)_island.c_str(), length);

			Room_v2::saveCurrentRoom(this, writeSaveData);
			Actor::saveActors(this, writeSaveData);
			TextEvent::saveAll(this, writeSaveData);
			_sounds->saveSounds(writeSaveData);
			_sounds->saveMusicState(writeSaveData);
			_lua->saveLocks(writeSaveData);
			lua_Save(writeSaveData);
		}
	}

	closeSaveFile(_saveFile);
	_saveFile = nullptr;

	if (isPs2) {
		// The card id is created with the first save, like the card folder on PS2
		if (!fileOpened) {
			if (_consoleServices)
				_consoleServices->logDebug("ERROR: could not access memory card\n");
		} else if (cardWasEmpty && !writeMemoryCardId(Lua_V2::getClockUniqueUint())) {
			if (_consoleServices)
				_consoleServices->logDebug("ERROR: Save to memory card FAILED\n");
		} else {
			if (_consoleServices)
				_consoleServices->logDebug("Save to memory card SUCCESS\n");

			_libJustSaved = 1;
		}
	}

	setSaveLoadIdle(true);
}

void EmiEngine::loadGame(CDrawContext *drawContext) {
	_libLoadRequest = 0;
	_saveFile = static_cast<ZFile *>(openSaveFile(_libSaveGameFileName.c_str(), "rb"));

	if (_saveFile && getGamePlatform() == Common::kPlatformPS2) {
		int skippedSize = 0;

		setSaveLoadIdle(false);

		readSaveData(&skippedSize, sizeof(skippedSize));
		_saveFile->seek(skippedSize, 1);
		readSaveData(&skippedSize, sizeof(skippedSize));
		_saveFile->seek(skippedSize, 1);

		if (getMemoryCardId() >= kMemoryCardFirstId && restoreGameData(readSaveData))
			_libJustLoaded = 1;
	} else if (_saveFile) {
		int tableSize = 0;
		char text[kSavedTextBufferSize + 1];

		readSaveData(&tableSize, sizeof(tableSize));
		_saveFile->seek(tableSize, 1);
		readSavedText(readSaveData, text, kSavedTextBufferSize);

		setSaveLoadIdle(false);
		Lua_V2::static_NukeAllScriptLocks();

		_sounds->stopAllMusic();
		_sounds->releaseAllSounds();

		ResourceManager *resourceManager = _gameResourceManager;

		if (resourceManager) {
			getOverworld()->clearActors();
			Actor::freeAllActors(this);
			Room_v2::loadRoom(this, nullptr);

			if (!_island.empty())
				_bundleServices->closeIslandBundles(_island.c_str());

			resourceManager->purgeAll();

			int imageSize = 0;

			readSaveData(&imageSize, sizeof(imageSize));
			_saveFile->seek(imageSize, 1);
			readSavedText(readSaveData, text, kSavedTextBufferSize);
			_island = text;

			if (!_island.empty())
				_bundleServices->openIslandBundles(_island.c_str());

			Room_v2::restoreCurrentRoom(this, readSaveData);
			Actor::restoreActors(this, readSaveData);
			TextEvent::restoreAll(this, readSaveData);
			_sounds->restoreSounds(readSaveData);
			_sounds->restoreMusicState(readSaveData);
			_lua->restoreLocks(readSaveData);
			_sounds->releaseUnlockedSounds();
			lua_Restore(readSaveData);
			_lua->updateJoySelected();
			_libJustLoaded = 1;
		}
	}

	closeSaveFile(_saveFile);
	_saveFile = nullptr;
	setSaveLoadIdle(true);
}

void EmiEngine::saveGameData(SaveRestoreAccess *accessFunc) {
	LString version;

	_lua->getVersion(version);
	writeSavedText(accessFunc,version);
	writeSavedText(accessFunc,_island);
	Room_v2::saveCurrentRoom(this, accessFunc);
	Actor::saveActors(this, accessFunc);
	TextEvent::saveAll(this, accessFunc);
	_sounds->saveSounds(accessFunc);
	_sounds->saveMusicState(accessFunc);
	_lua->saveLocks(accessFunc);
	lua_Save(accessFunc);
}

void EmiEngine::clearGameState() {
	Lua_V2::static_NukeAllScriptLocks();
	_sounds->stopAllForLoad();
	getOverworld()->clearActors();
	Actor::freeAllActors(this);
	Room_v2::loadRoom(this, nullptr);

	if (!_island.empty())
		_bundleServices->closeIslandBundles(_island.c_str());

	_gameResourceManager->purgeAll();
}

bool EmiEngine::restoreGameData(SaveRestoreAccess *accessFunc) {
	char text[kSavedTextBufferSize + 1];
	LString currentVersion;

	readSavedText(accessFunc,text, kSavedTextBufferSize);
	_lua->getVersion(currentVersion);

	if (currentVersion != text) {
		warning("EmiEngine::restoreGameData(): the saved version '%s' differs from '%s'", text, currentVersion.c_str());

		return false;
	}

	clearGameState();
	readSavedText(accessFunc,text, kSavedTextBufferSize);
	_island = text;

	if (!_island.empty())
		_bundleServices->openIslandBundles(_island.c_str());

	Room_v2::restoreCurrentRoom(this, accessFunc);
	Actor::restoreActors(this, accessFunc);
	TextEvent::restoreAll(this, accessFunc);
	_sounds->restoreSounds(accessFunc);
	_sounds->restoreMusicState(accessFunc);
	_lua->restoreLocks(accessFunc);
	lua_Restore(accessFunc);
	_lua->updateJoySelected();

	return true;
}

bool EmiEngine::restoreGameDataFromRam(const byte *data, uint size) {
	Common::MemoryReadStream reader(data, size);

	_ps2StateReader = &reader;

	bool restored = restoreGameData(Movie::readPs2State);

	_ps2StateReader = nullptr;

	return restored;
}

bool EmiEngine::isLegacySaveFile(const Common::String &fileName) {
	Common::InSaveFile *stream = g_system->getSavefileManager()->openForLoading(fileName);

	if (!stream)
		return false;

	bool legacy = LegacySaveFile::isLegacySave(stream);

	delete stream;

	return legacy;
}

bool EmiEngine::hasMemoryCardSaves() {
	return !g_system->getSavefileManager()->listSavefiles(kMemoryCardSavePattern).empty();
}

uint EmiEngine::getMemoryCardId() {
	// The virtual card is always inserted and formatted; without saves there is no folder
	if (!hasMemoryCardSaves())
		return kMemoryCardNoFolder;

	Common::InSaveFile *stream = g_system->getSavefileManager()->openForLoading(kMemoryCardIdFileName);
	uint id = 0;

	if (stream) {
		uint32 value = 0;

		if (stream->read(&value, sizeof(value)) == sizeof(value))
			id = FROM_LE_32(value);

		delete stream;
	}

	if (id >= kMemoryCardFirstId)
		return id;

	// The original erases the whole card folder here; we keep the saves and create a new id
	warning("EmiEngine::getMemoryCardId(): no valid card id, created a new one");
	id = Lua_V2::getClockUniqueUint();

	if (!writeMemoryCardId(id))
		return kMemoryCardError;

	return id;
}

bool EmiEngine::writeMemoryCardId(uint id) {
	Common::OutSaveFile *stream = g_system->getSavefileManager()->openForSaving(kMemoryCardIdFileName, false);

	if (!stream) {
		warning("EmiEngine::writeMemoryCardId(): cannot create card id file");

		return false;
	}

	stream->writeUint32LE(id);
	stream->finalize();

	bool written = !stream->err();

	delete stream;

	return written;
}

bool EmiEngine::deleteSaveFile(const char *fileName) {
	return g_system->getSavefileManager()->removeSavefile(getSaveFileName(fileName));
}

bool EmiEngine::copySaveFile(const char *sourceName, const char *targetName) {
	Common::SaveFileManager *saveFileManager = g_system->getSavefileManager();
	Common::InSaveFile *source = saveFileManager->openRawFile(getSaveFileName(sourceName));

	if (!source)
		return false;

	Common::OutSaveFile *target = saveFileManager->openForSaving(getSaveFileName(targetName), false);

	if (!target) {
		delete source;

		return false;
	}

	// Byte copy of the compressed file, without the 400 KB buffer limit
	target->writeStream(source);
	target->finalize();

	bool copied = !source->err() && !target->err();

	delete target;
	delete source;

	return copied;
}

const char *EmiEngine::justLoadedFileName() {
	if (!_libJustLoaded)
		return nullptr;

	_libJustLoaded = 0;

	// Restored sounds resume two frames later
	_soundRestartFrames = 2;

	return _libSaveGameFileName.c_str();
}

bool EmiEngine::armSpuClear() {
	bool armed = _spuClearArmed;

	_spuClearArmed = true;

	return armed;
}

bool EmiEngine::isSpuClearArmed() {
	return _spuClearArmed;
}

void EmiEngine::startSetChangeFrames() {
	_setChangeFrames = 1;
}

bool EmiEngine::isSetChanging() {
	return _setChangeFrames > 0;
}

bool EmiEngine::justSaved() {
	if (!_libJustSaved)
		return false;

	if (_consoleServices)
		_consoleServices->logDebug("SAVEGAME: telling scripts just saved.\n");

	_libJustSaved = 0;

	return true;
}

bool EmiEngine::readSaveStrings(Common::SeekableReadStream *stream, Common::Array<Common::String> &strings) {
	strings.clear();

	if (LegacySaveFile::isLegacySave(stream))
		return false;

	int32 totalBytes = stream->readSint32LE();

	if (stream->err() || stream->eos() || totalBytes < 0 || (uint)totalBytes > kMaxSaveTableSize)
		return false;

	while (totalBytes > 0) {
		int32 length = stream->readSint32LE();

		if (stream->err() || stream->eos() || length < 1 || length > totalBytes - 4)
			return false;

		Common::Array<char> text(length);

		if (stream->read(text.data(), length) != (uint32)length)
			return false;

		text[length - 1] = '\0';
		strings.push_back(Common::String(text.data()));
		totalBytes -= length + 4;
	}

	return true;
}

Graphics::Surface *EmiEngine::readSaveThumbnail(Common::SeekableReadStream *stream, bool isPs2) {
	int imageWidth = isPs2 ? kThumbnailTextureWidth : kThumbnailImageWidth;
	int imageHeight = isPs2 ? kThumbnailTextureHeight : kThumbnailImageHeight;
	int bytesPerPixel = isPs2 ? 2 : 3;

	if (!isPs2) {
		int32 versionLength = stream->readSint32LE();

		if (stream->err() || stream->eos() || versionLength < 0 || versionLength > kSavedTextBufferSize)
			return nullptr;

		stream->seek(versionLength, SEEK_CUR);
	}

	uint32 imageSize = stream->readUint32LE();
	uint32 expectedSize = kThumbnailTextureWidth * kThumbnailTextureHeight * bytesPerPixel;

	if (stream->err() || stream->eos() || imageSize != expectedSize)
		return nullptr;

	Common::Array<byte> pixels(imageSize);

	if (stream->read(pixels.data(), imageSize) != imageSize)
		return nullptr;

	Graphics::PixelFormat format(4, 8, 8, 8, 8, 24, 16, 8, 0);
	Graphics::Surface *thumbnail = new Graphics::Surface();

	thumbnail->create(imageWidth, imageHeight, format);

	LColorFormat imageFormat(15);

	for (int y = 0; y < imageHeight; y++) {
		const byte *source = &pixels[y * kThumbnailTextureWidth * bytesPerPixel];
		uint32 *target = (uint32 *)thumbnail->getBasePtr(0, y);

		for (int x = 0; x < imageWidth; x++) {
			if (isPs2) {
				target[x] = format.RGBToColor(imageFormat.getRed(source + 2 * x), imageFormat.getGreen(source + 2 * x), imageFormat.getBlue(source + 2 * x));
			} else {
				target[x] = format.RGBToColor(source[3 * x], source[3 * x + 1], source[3 * x + 2]);
			}
		}
	}

	// The whole screen is squeezed into the texture: give it back the aspect ratio of the screen
	if (isPs2) {
		Graphics::Surface *scaled = thumbnail->scale(kThumbnailImageWidth, kThumbnailImageHeight, true);

		thumbnail->free();
		delete thumbnail;

		return scaled;
	}

	return thumbnail;
}

Common::String EmiEngine::getSaveDescription(const Common::Array<Common::String> &strings, int slot) {
	if (strings.empty() || strings[0].empty())
		return Common::String::format("Slot %d", slot);

	return strings[0];
}

} // End of namespace Grim
