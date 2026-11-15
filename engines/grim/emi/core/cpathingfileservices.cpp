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

#include "common/platform.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/emi/core/cpathingfileservices.h"
#include "engines/grim/emi/emiengine.h"

namespace Grim {

struct TypeDirectory {
	const char *extension;
	const char *directory;
};

// The last row, the bundle extension, depends on the game: .m4b or .lab (demo)
static const int kNumTypeDirectories = 19;
static const char *const kBundleDirectory = ".\\";
static const char *const kBundleExtension = ".m4b";
static const char *const kBundleExtensionDemo = ".lab";

static const TypeDirectory kTypeDirectories[kNumTypeDirectories] = {
	{ ".ani", ".\\Animation\\" },
	{ ".til", ".\\Backgrounds\\" },
	{ ".cos", ".\\Costumes\\" },
	{ ".laf", ".\\Fonts\\" },
	{ ".tga", ".\\Materials\\" },
	{ ".pcx", ".\\Materials\\" },
	{ ".mes", ".\\Meshes\\" },
	{ ".spr", ".\\Meshes\\" },
	{ ".snm", ".\\Movies\\" },
	{ ".lua", ".\\Scripts\\" },
	{ ".set", ".\\Sets\\" },
	{ ".aif", ".\\Sfx\\" },
	{ ".skl", ".\\Skeletons\\" },
	{ ".wav", ".\\Voice\\" },
	{ ".wvc", ".\\Voice\\" },
	{ ".lip", ".\\Lip\\" },
	{ ".sur", ".\\Materials\\" },
	{ ".mov", ".\\Movies\\" },
	{ ".bik", ".\\Movies\\" }
};

CPathingFileServices::CPathingFileServices(EmiEngine *engine) : LFileServices(engine) {
	memset(_cdPath, 0, sizeof(_cdPath));
	memset(_suspendedCdPath, 0, sizeof(_suspendedCdPath));
}

const char *CPathingFileServices::getTypeDirectory(const char *extension) {
	for (int i = 0; i < kNumTypeDirectories; i++) {
		if (scumm_strnicmp(extension, kTypeDirectories[i].extension, 4) == 0)
			return kTypeDirectories[i].directory;
	}

	const char *bundleExtension = _engine->isDemo() ? kBundleExtensionDemo : kBundleExtension;

	if (scumm_strnicmp(extension, bundleExtension, 4) == 0)
		return kBundleDirectory;

	return nullptr;
}

char *CPathingFileServices::getCdPath() {
	return _cdPath[0] != 0 ? _cdPath : nullptr;
}

void CPathingFileServices::setCdPath(const char *path) {
	Common::strcpy_s(_cdPath, path);
}

void CPathingFileServices::suspendCdPath() {
	Common::strcpy_s(_suspendedCdPath, _cdPath);
	_cdPath[0] = 0;
}

void CPathingFileServices::resumeCdPath() {
	Common::strcpy_s(_cdPath, _suspendedCdPath);
}

void CPathingFileServices::openInto(LFile *file, const char *path, const char *mode) {
	const char *current = path;
	bool leadingDot = false;

	if (!file) {
		warning("CPathingFileServices::openInto(): null file");

		return;
	}

	if (*path == '.') {
		LFileServices::openInto(file, path, mode);
		if (file->isOpen())
			return;

		current = path[1] ? path + 2 : path + 1;
		leadingDot = true;
	}

	if (!strchr(current, ':')) {
		LString resolved;

		if (resolvePath(resolved, current))
			LFileServices::openInto(file, resolved.c_str(), mode);
	}

	if (!file->isOpen() && !leadingDot)
		LFileServices::openInto(file, current, mode);
}

LFile *CPathingFileServices::open(const char *path, const char *mode) {
	const char *current = path;
	LFile *file = nullptr;
	bool leadingDot = false;

	if (*path == '.') {
		file = LFileServices::open(path, mode);
		if (file)
			return file;

		current = path[1] ? path + 2 : path + 1;
		leadingDot = true;
	}

	if (!strchr(current, ':')) {
		LString resolved;

		if (resolvePath(resolved, current))
			file = LFileServices::open(resolved.c_str(), mode);

		if (file)
			return file;
	}

	if (!leadingDot)
		return LFileServices::open(current, mode);

	return file;
}

LFile *CPathingFileServices::openExtended(const char *path, const char *mode, int unused1, int unused2) {
	return open(path, mode);
}

bool CPathingFileServices::exists(const char *path) {
	LString resolved;

	if (_engine->getGamePlatform() == Common::kPlatformPS2)
		return LFileServices::exists(path);

	if (strchr(path, ':'))
		return false;

	if (!resolvePath(resolved, path))
		return false;

	return LFileServices::exists(resolved.c_str());
}

int CPathingFileServices::getModTime(const char *path) {
	LString resolved;

	if (strchr(path, ':'))
		return 0;

	if (!resolvePath(resolved, path))
		return 0;

	return LFileServices::getModTime(resolved.c_str());
}

bool CPathingFileServices::resolvePath(LString &outPath, const char *name) {
	const char *extension = strrchr(name, '.');
	if (!extension)
		return false;

	const char *typeDirectory = getTypeDirectory(extension);
	if (!typeDirectory)
		return false;

	outPath = _cdPath[0] ? _cdPath : ".";
	outPath += typeDirectory + 1;
	outPath += name;

	return true;
}

} // End of namespace Grim
