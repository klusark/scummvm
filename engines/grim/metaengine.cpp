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

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/grim.h"
#include "engines/grim/legacy_savegames/legacy_savegames.h"
#include "engines/grim/remastered/savestore.h"

#include "common/endian.h"
#include "common/language.h"
#include "common/platform.h"
#include "common/system.h"
#include "common/savefile.h"
#include "common/translation.h"
#include "common/config-manager.h"

#include "graphics/surface.h"

namespace Grim {

static const int kSaveImageWidth = 250;
static const int kSaveImageHeight = 188;
static const int kLauncherThumbnailWidth = 160;
static const int kLauncherThumbnailHeight = 120;

// Same gamma curve as SputRender::buildGammaTable applies to the save image in the game menu
static void correctSaveImageGamma(Common::Array<byte> &pixels, float gamma) {
	if (!(gamma >= 0.0f) || gamma == 1.0f)
		return;

	for (uint i = 0; i + 1 < pixels.size(); i += 2) {
		uint16 value = READ_LE_UINT16(&pixels[i]);
		float red = (float)pow((double)((value >> 11) & 0x1F) / 31.0, (double)gamma);
		float green = (float)pow((double)((value >> 5) & 0x3F) / 63.0, (double)gamma);
		float blue = (float)pow((double)(value & 0x1F) / 31.0, (double)gamma);

		value = (uint16)(((uint)(31.0 * red) << 11) | ((uint)(63.0 * green) << 5) | (uint)(31.0 * blue));
		WRITE_LE_UINT16(&pixels[i], value);
	}
}

static bool readSaveStrings(Common::SeekableReadStream *stream, Common::Array<Common::String> &strings) {
	if (LegacySaveFile::isLegacySave(stream))
		return legacyReadSaveStrings(stream, strings);

	strings.clear();

	int32 totalBytes = stream->readSint32LE();

	if (stream->err() || stream->eos() || totalBytes < 0 || totalBytes > stream->size() - 4)
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

static bool readSaveThumbnail(Common::SeekableReadStream *stream, Common::Array<byte> &pixels) {
	if (LegacySaveFile::isLegacySave(stream))
		return legacyReadThumbnail(stream, pixels);

	int32 blockSize = stream->readSint32LE();

	if (stream->err() || stream->eos() || blockSize < 0 || blockSize > stream->size() - 8)
		return false;

	stream->seek(blockSize, SEEK_CUR);

	int32 imageSize = stream->readSint32LE();

	if (stream->err() || stream->eos() || imageSize != kSaveImageWidth * kSaveImageHeight * 2)
		return false;

	pixels.resize(imageSize);

	return stream->read(pixels.data(), imageSize) == (uint32)imageSize;
}

static const ADExtraGuiOptionsMap gameGuiOptions[] = {
	{
		GAMEOPTION_SHOW_FPS,
		{
			_s("Show FPS"),
			_s("Show the current FPS-rate, while you play."),
			"show_fps",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_3D_ACCELERATION,
		{
			_s("Enable 3D acceleration"),
			_s("Use the hardware accelerated 3D renderer. It can also be changed from the game settings."),
			"grim_3d_acceleration",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_REMASTER_BUGFIXES,
		{
			_s("Apply Remastered bug fixes"),
			_s("Apply the fixes to the game engine bugs that were corrected in the Remastered version."),
			"grim_remaster_bugfixes",
			true,
			0,
			0
		}
	},
	{
		GAMEOPTION_EMI_MOVIE_SUBTITLES,
		{
			_s("Movie subtitles"),
			_s("Show the subtitles during the movies."),
			"movie_subtitles",
			false,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

class GrimMetaEngine : public AdvancedMetaEngine<Grim::GrimGameDescription> {
public:
	const char *getName() const override {
		return "grim";
	}

	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		return gameGuiOptions;
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const Grim::GrimGameDescription *desc) const override;

	bool hasFeature(MetaEngineFeature f) const override;

	Common::KeymapArray initKeymaps(const char *target) const override;

	SaveStateList listSaves(const char *target) const override;

	SaveStateDescriptor querySaveMetaInfos(const char *target, int slot) const override;

	bool removeSaveState(const char *target, int slot) const override;

	Common::String getSavegameFile(int saveGameIdx, const char *target = nullptr) const override;

};

Common::Error GrimMetaEngine::createInstance(OSystem *syst, Engine **engine, const Grim::GrimGameDescription *desc) const {
	const GrimGameDescription *gd = (const GrimGameDescription *)desc;

	if (gd->gameType == GType_MONKEY4) {
#ifdef ENABLE_MONKEY4
		*engine = new EmiEngine(syst, gd->desc.flags, gd->gameType, gd->desc.platform, gd->desc.language);

		return Common::kNoError;
#else
		return Common::Error(Common::kUnsupportedGameidError, _s("Escape from Monkey Island support is not compiled in"));
#endif
	}

	*engine = new GrimEngine(syst, gd->desc.flags, gd->gameType, gd->desc.platform, gd->desc.language);

	return Common::kNoError;
}

bool GrimMetaEngine::hasFeature(MetaEngineFeature f) const {
	return
		(f == kSupportsListSaves) ||
		(f == kSupportsDeleteSave) ||
		(f == kSavesSupportMetaInfo) ||
		(f == kSavesSupportThumbnail) ||
		(f == kSupportsLoadingDuringStartup);
}

// efmi, efmi-ps2 or efmi-ko / efmi-cn / efmi-tw: the same rule the engine uses for the save files
static Common::String getEmiSavePrefix(const char *target) {
	Common::Platform platform = Common::kPlatformUnknown;
	Common::Language language = Common::UNK_LANG;

	if (target) {
		platform = Common::parsePlatform(ConfMan.get("platform", target));
		language = Common::parseLanguage(ConfMan.get("language", target));
	}

	return EmiEngine::getSavePrefix(platform, language);
}

Common::String GrimMetaEngine::getSavegameFile(int saveGameIdx, const char *target) const {
	Common::String gameId = target ? ConfMan.get("gameid", target) : Common::String();
	Common::String extra = target ? ConfMan.get("extra", target) : Common::String();
	bool isEmi = gameId == "monkey4";
	bool isRemastered = extra.contains("Remastered");
	Common::String prefix = getEmiSavePrefix(target);

	if (isRemastered) {
		if (saveGameIdx == kSavegameFilePattern)
			return Common::String("grim-remastered-savedata####/grim.gsv");

		return Common::String::format("grim-remastered-savedata%04d/grim.gsv", saveGameIdx);
	}

	if (saveGameIdx == kSavegameFilePattern)
		return isEmi ? Common::String::format("%s###.gsv", prefix.c_str()) : Common::String("grim##.gsv");

	if (isEmi)
		return Common::String::format("%s%03d.gsv", prefix.c_str(), saveGameIdx);

	return Common::String::format("grim%02d.gsv", saveGameIdx);
}

bool GrimMetaEngine::removeSaveState(const char *target, int slot) const {
	if (ConfMan.get("extra", target).contains("Remastered"))
		return SaveStore::eraseSlot(slot) == SaveStore::kStatusOk;

	return AdvancedMetaEngine::removeSaveState(target, slot);
}

Common::KeymapArray GrimMetaEngine::initKeymaps(const char *target) const {
	Common::String gameId = ConfMan.get("gameid", target);

#ifdef ENABLE_GRIM
	if (gameId == "grim") {
		return Grim::GrimEngine::initKeymapsGrim(target);
	}
#endif
#ifdef ENABLE_MONKEY4
	if (gameId == "monkey4") {
		return Grim::EmiEngine::initKeymapsEmi(target);
	}
#endif

	return AdvancedMetaEngine::initKeymaps(target);
}

SaveStateList GrimMetaEngine::listSaves(const char *target) const {
	Common::String gameId = ConfMan.get("gameid", target);
	Common::String extra = ConfMan.get("extra", target);
	const bool isDemo = extra.contains("Demo");
	const bool isRemastered = extra.contains("Remastered");
	Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
	Common::StringArray filenames;
	const Common::String prefix = getEmiSavePrefix(target);
	Common::String pattern = gameId == "monkey4" ? prefix + "###.gsv" : "grim##.gsv";
	SaveStateList saveList;

	if (isDemo)
		return saveList; // Demos do not support saving

	if (isRemastered) {
		Common::Array<SaveStore::SlotInfo> slots;

		SaveStore::listSlots(slots);

		for (uint i = 0; i < slots.size(); i++)
			saveList.push_back(SaveStateDescriptor(this, slots[i].slot, slots[i].title));

		return saveList;
	}

	filenames = saveFileMan->listSavefiles(pattern);

	for (Common::StringArray::const_iterator file = filenames.begin(); file != filenames.end(); ++file) {
		// Obtain the last digits of the filename, since they correspond to the save slot
		int slotNum = atoi(file->c_str() + (gameId == "monkey4" ? prefix.size() : 4));

		if (slotNum >= 0) {
			Common::SeekableReadStream *filePointer = saveFileMan->openForLoading(*file);
			Common::Array<Common::String> strings;

			if (filePointer && gameId == "monkey4") {
				if (EmiEngine::readSaveStrings(filePointer, strings))
					saveList.push_back(SaveStateDescriptor(this, slotNum, EmiEngine::getSaveDescription(strings, slotNum)));
			} else if (filePointer && readSaveStrings(filePointer, strings)) {
				saveList.push_back(SaveStateDescriptor(this, slotNum, strings.empty() ? Common::String::format("Slot %d", slotNum) : strings[0]));
			}

			delete filePointer;
		}
	}

	Common::sort(saveList.begin(), saveList.end(), SaveStateDescriptorSlotComparator());
	return saveList;
}

SaveStateDescriptor GrimMetaEngine::querySaveMetaInfos(const char *target, int slot) const {
	Common::String gameId = ConfMan.get("gameid", target);
	Common::String extra = ConfMan.get("extra", target);
	Common::SaveFileManager *saveFileMan = g_system->getSavefileManager();
	Common::String fileName = getSavegameFile(slot, target);

	if (extra.contains("Remastered")) {
		Common::Array<SaveStore::SlotInfo> slots;

		SaveStore::listSlots(slots);

		for (uint i = 0; i < slots.size(); i++) {
			if (slots[i].slot != slot)
				continue;

			SaveStateDescriptor remasteredDesc(this, slot, slots[i].title);

			remasteredDesc.setThumbnail(SaveStore::makeMuralThumbnail(slots[i].mural.c_str()));

			return remasteredDesc;
		}

		return SaveStateDescriptor();
	}

	Common::SeekableReadStream *file = saveFileMan->openForLoading(fileName);
	Common::Array<Common::String> strings;

	if (!file)
		return SaveStateDescriptor();

	if (gameId == "monkey4") {
		if (!EmiEngine::readSaveStrings(file, strings)) {
			delete file;
			return SaveStateDescriptor();
		}

		SaveStateDescriptor emiDesc(this, slot, EmiEngine::getSaveDescription(strings, slot));
		bool isPs2 = Common::parsePlatform(ConfMan.get("platform", target)) == Common::kPlatformPS2;
		Graphics::Surface *emiThumbnail = EmiEngine::readSaveThumbnail(file, isPs2);

		if (emiThumbnail)
			emiDesc.setThumbnail(emiThumbnail);

		delete file;
		return emiDesc;
	}

	if (!readSaveStrings(file, strings)) {
		delete file;
		return SaveStateDescriptor();
	}

	SaveStateDescriptor desc(this, slot, strings.empty() ? Common::String::format("Slot %d", slot) : strings[0]);
	Common::Array<byte> pixels;

	file->seek(0);

	if (readSaveThumbnail(file, pixels)) {
		if (ConfMan.hasKey("grim_gamma", target))
			correctSaveImageGamma(pixels, ConfMan.getFloat("grim_gamma", target));

		Graphics::Surface *thumbnail = new Graphics::Surface();

		thumbnail->create(kSaveImageWidth, kSaveImageHeight, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
		memcpy(thumbnail->getPixels(), pixels.data(), pixels.size());

		Graphics::Surface *scaled = thumbnail->scale(kLauncherThumbnailWidth, kLauncherThumbnailHeight, true);

		thumbnail->free();
		delete thumbnail;
		desc.setThumbnail(scaled);
	}

	delete file;
	return desc;
}

} // End of namespace Grim

#if PLUGIN_ENABLED_DYNAMIC(GRIM)
	REGISTER_PLUGIN_DYNAMIC(GRIM, PLUGIN_TYPE_ENGINE, Grim::GrimMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(GRIM, PLUGIN_TYPE_ENGINE, Grim::GrimMetaEngine);
#endif
