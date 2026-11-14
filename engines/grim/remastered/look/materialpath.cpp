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

#include "engines/grim/remastered/look/materialpath.h"
#include "engines/grim/remastered/look/mcb.h"
#include "engines/grim/std/std_sys.h"

namespace Grim {

static const char *const materialResourceDirectories[MaterialPath::kResourceDirectoryCount] = {
	"Animation/", "Backgrounds/", "Costumes/", "Colormaps/", "Commentary/", "Fonts/", "Materials/", "MaterialsHD/",
	"Models/", "Movies/", "Objects/", "Scripts/", "Sets/", "Voices/", "Footsteps/", "Hotspots/", "Images/", "ImagesHD/",
	"ImagesHD/concepts/", "ImagesHD/cutscenes/", "ImagesHD/saveimages/", "SFX/", "Loops/", "L3d/", "SetX/",
	"DEBUG_Animation/", "DEBUG_Backgrounds/", "DEBUG_Costumes/", "DEBUG_Colormaps/", "DEBUG_Fonts/", "DEBUG_Materials/",
	"DEBUG_Models/", "DEBUG_Movies/", "DEBUG_Objects/", "DEBUG_Scripts/", "DEBUG_Sets/", "DEBUG_Voices/",
	"DEBUG_Footsteps/", "DEBUG_SFX/", "DEBUG_Loops/"
};

bool MaterialPath::buildFileName(const char *materialName, int cellIndex, Common::String &fileName) {
	fileName.clear();

	if (!materialName) {
		warning("MaterialPath::buildFileName(): no material name");
		return false;
	}

	Common::String name(materialName);

	name += Common::String::format(".%d.mcb", cellIndex);

	if (name.size() > kMaxNameLength)
		name = name.substr(0, kMaxNameLength);

	if (name == "dflt.mat.0.mcb")
		return false;

	if (!name.hasSuffix(".mcb"))
		return false;

	fileName = name;

	return true;
}

const char *MaterialPath::getResourceDirectory(uint index) {
	if (index >= kResourceDirectoryCount) {
		warning("MaterialPath::getResourceDirectory(): index %u out of range", index);
		return nullptr;
	}

	return materialResourceDirectories[index];
}

void MaterialPath::getSearchOrder(bool hasExtraDataPath, Common::Array<Source> &sources) {
	sources.clear();

	uint groups = hasExtraDataPath ? 2 : 1;

	for (uint group = 0; group < groups; group++) {
		bool extra = hasExtraDataPath && group == 0;
		Source source;

		source.kind = Source::kDirectory;
		source.extraDataPath = extra;

		for (int i = kResourceDirectoryCount - 1; i >= 0; i--) {
			source.name = materialResourceDirectories[i];
			sources.push_back(source);
		}

		source.kind = Source::kBaseDirectory;
		source.name.clear();
		sources.push_back(source);

		source.kind = Source::kArchives;
		sources.push_back(source);
	}
}

Common::SeekableReadStream *MaterialPath::open(SysStruct *hs, const char *materialName, int cellIndex) {
	if (!hs) {
		warning("MaterialPath::open(): no file system");
		return nullptr;
	}

	Common::String fileName;

	if (!buildFileName(materialName, cellIndex, fileName))
		return nullptr;

	return hs->openFile(fileName.c_str());
}

bool MaterialPath::load(SysStruct *hs, const char *materialName, int cellIndex, Mcb &mcb) {
	mcb.clear();

	Common::SeekableReadStream *stream = open(hs, materialName, cellIndex);

	if (!stream)
		return false;

	bool loaded = mcb.load(stream);

	delete stream;
	stream = nullptr;

	return loaded;
}

} // End of namespace Grim
