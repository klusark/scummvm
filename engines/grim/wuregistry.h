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

#ifndef GRIM_WUREGISTRY_H
#define GRIM_WUREGISTRY_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/remastered/registry_unlocks.h"

namespace Grim {

struct WuRegistryValue {
	Common::String name;
	uint type;
	bool inConfMan;
	Common::Array<byte> data;
};

class WuRegistry {
public:
	enum ValueType {
		kValueString = 1,
		kValueBinary = 3
	};

	WuRegistry();
	~WuRegistry();

	int startup(const char *version);
	int loadIndex();
	void shutdown();
	void freeIndex();
	int saveInt(const char *valueName, int value);
	int addIndex(const char *valueName);
	int findIndex(const char *valueName);
	int saveFloat(const char *valueName, float value);
	int getInt(const char *valueName, int defaultValue);
	float getFloat(const char *valueName, float defaultValue);
	int saveBinary(const char *valueName, const byte *data, uint size);
	int getBinary(const char *valueName, byte *data, uint size);
	int saveStr(const char *valueName, const char *data);
	int getStr(const char *valueName, char *data, uint size, const char *defaultValue);
	uint queryType(const char *valueName);
	void saveRegistry();

	// The remastered: when a save slot was written, as the list of saves shows it
	void saveSlotTime(int slot);
	bool getSlotTime(int slot, Common::String *time);
	void removeSlotTime(int slot);

	void unlockCutscene(int index);
	bool isCutsceneUnlocked(int index);
	void unlockConcept(int index);
	bool isConceptUnlocked(int index);
	void resetUnlocks();
	bool importRegistrySav(Common::SeekableReadStream *stream);

	static int compareNames(const void *a, const void *b);

private:
	int findValue(const char *valueName) const;
	bool claimKey(const char *valueName, uint type, Common::String *key);
	bool findConfKey(const char *valueName, Common::String *key) const;
	void setValue(const char *valueName, uint type, const byte *data, uint size);
	bool queryValue(const char *valueName, uint *type, byte *data, uint *size) const;

	int _started;
	Common::Array<WuRegistryValue> _values;
	char **_valueNames;
	uint _numValues;
	RegistryUnlocks _unlocks;
};

} // End of namespace Grim

#endif
