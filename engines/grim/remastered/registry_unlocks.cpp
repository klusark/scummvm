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

#include "common/savefile.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "engines/grim/remastered/registry_unlocks.h"

namespace Grim {

static const char *const registryFileName = "registry.sav";

RegistryUnlocks::RegistryUnlocks() : _loaded(false), _dirty(false), _entriesKept(false) {
	memset(_entries, 0, sizeof(_entries));
	memset(_cutsceneBits, 0, sizeof(_cutsceneBits));
	memset(_conceptBits, 0, sizeof(_conceptBits));
}

void RegistryUnlocks::unlockCutscene(int index) {
	ensureLoaded();

	if (index < 0 || index >= kNumCutsceneSlots) {
		warning("RegistryUnlocks::unlockCutscene(): the index %d is out of range", index);
		return;
	}

	if (testBit(_cutsceneBits, kNumCutsceneSlots, index))
		return;

	_cutsceneBits[index >> 3] |= (byte)(1 << (index & 7));
	_dirty = true;
}

bool RegistryUnlocks::isCutsceneUnlocked(int index) {
	ensureLoaded();

	return testBit(_cutsceneBits, kNumCutsceneSlots, index);
}

void RegistryUnlocks::unlockConcept(int index) {
	ensureLoaded();

	if (index < 0 || index >= kNumConceptSlots) {
		warning("RegistryUnlocks::unlockConcept(): the index %d is out of range", index);
		return;
	}

	if (testBit(_conceptBits, kNumConceptSlots, index))
		return;

	_conceptBits[index >> 3] |= (byte)(1 << (index & 7));
	_dirty = true;
}

bool RegistryUnlocks::isConceptUnlocked(int index) {
	ensureLoaded();

	return testBit(_conceptBits, kNumConceptSlots, index);
}

void RegistryUnlocks::resetUnlocks() {
	ensureLoaded();

	memset(_cutsceneBits, 0, sizeof(_cutsceneBits));
	memset(_conceptBits, 0, sizeof(_conceptBits));
	_dirty = true;
}

bool RegistryUnlocks::importRegistrySav(Common::SeekableReadStream *stream) {
	ensureLoaded();

	if (!mergeRegistrySav(stream))
		return false;

	_dirty = true;

	return true;
}

void RegistryUnlocks::saveFile() {
	if (!_dirty)
		return;

	Common::OutSaveFile *file = g_system->getSavefileManager()->openForSaving(registryFileName, false);

	if (!file) {
		warning("RegistryUnlocks::saveFile(): cannot create %s", registryFileName);
		return;
	}

	file->writeUint32LE(kRegistrySavVersion);
	file->write(_entries, sizeof(_entries));
	file->write(_cutsceneBits, sizeof(_cutsceneBits));
	file->write(_conceptBits, sizeof(_conceptBits));
	file->finalize();

	if (file->err())
		warning("RegistryUnlocks::saveFile(): cannot write %s", registryFileName);
	else
		_dirty = false;

	delete file;
}

void RegistryUnlocks::ensureLoaded() {
	if (_loaded)
		return;

	_loaded = true;

	Common::InSaveFile *file = g_system->getSavefileManager()->openForLoading(registryFileName);

	if (!file)
		return;

	mergeRegistrySav(file);
	delete file;
}

bool RegistryUnlocks::mergeRegistrySav(Common::SeekableReadStream *stream) {
	byte entries[kEntriesBytes];
	byte unlocks[kCutsceneBytes + kConceptBytes];

	if (!stream || stream->size() != kRegistrySavSize) {
		warning("RegistryUnlocks::mergeRegistrySav(): the registry file does not have the expected size");
		return false;
	}

	if (!stream->seek(0) || stream->readUint32LE() != (uint32)kRegistrySavVersion) {
		warning("RegistryUnlocks::mergeRegistrySav(): the registry file does not have the expected version");
		return false;
	}

	if (!stream->seek(kEntriesOffset) || stream->read(entries, sizeof(entries)) != sizeof(entries) ||
			stream->read(unlocks, sizeof(unlocks)) != sizeof(unlocks)) {
		warning("RegistryUnlocks::mergeRegistrySav(): cannot read the registry file");
		return false;
	}

	if (!_entriesKept) {
		memcpy(_entries, entries, sizeof(_entries));
		_entriesKept = true;
	}

	for (uint i = 0; i < kCutsceneBytes; i++)
		_cutsceneBits[i] |= unlocks[i];

	for (uint i = 0; i < kConceptBytes; i++)
		_conceptBits[i] |= unlocks[kCutsceneBytes + i];

	return true;
}

bool RegistryUnlocks::testBit(const byte *bits, uint slots, int index) {
	if (index < 0 || (uint)index >= slots)
		return false;

	return (bits[index >> 3] & (1 << (index & 7))) != 0;
}

} // End of namespace Grim
