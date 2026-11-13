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

#ifndef GRIM_REGISTRY_UNLOCKS_H
#define GRIM_REGISTRY_UNLOCKS_H

#include "common/scummsys.h"

namespace Common {
class SeekableReadStream;
}

namespace Grim {

// Unlocked cutscenes (64 slots, 40 used) and concept art (128 slots, 98 used)
// of the extras menus. Indices are zero based. They live in the file
// registry.sav of the saves folder, laid out like the original registry file:
// u32 version, 64 entries of 268 bytes, 8 bytes of cutscene bits, 16 bytes of
// concept bits. Bit n is bit (n & 7) of byte (n >> 3).
class RegistryUnlocks {
public:
	RegistryUnlocks();

	void unlockCutscene(int index);
	bool isCutsceneUnlocked(int index);
	void unlockConcept(int index);
	bool isConceptUnlocked(int index);
	void resetUnlocks();

	// Merges the unlocks of a registry file of the original game
	bool importRegistrySav(Common::SeekableReadStream *stream);

	// Writes registry.sav, also when nothing changed
	void saveFile();

private:
	enum {
		kNumCutsceneSlots = 64,
		kNumConceptSlots = 128,
		kCutsceneBytes = kNumCutsceneSlots / 8,
		kConceptBytes = kNumConceptSlots / 8,
		kEntryCount = 64,
		kEntryBytes = 268,
		kEntriesBytes = kEntryCount * kEntryBytes,
		kRegistrySavVersion = 2,
		kEntriesOffset = 4,
		kUnlocksOffset = kEntriesOffset + kEntriesBytes,
		kRegistrySavSize = kUnlocksOffset + kCutsceneBytes + kConceptBytes
	};

	void ensureLoaded();
	bool mergeRegistrySav(Common::SeekableReadStream *stream);
	static bool testBit(const byte *bits, uint slots, int index);

	bool _loaded;
	bool _entriesKept;
	byte _entries[kEntriesBytes];
	byte _cutsceneBits[kCutsceneBytes];
	byte _conceptBits[kConceptBytes];
};

} // End of namespace Grim

#endif
