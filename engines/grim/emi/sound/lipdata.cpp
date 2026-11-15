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
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/sound/lipdata.h"
#include "engines/grim/emi/sound/sounds.h"

namespace Grim {

static const int kFileNameBufferSize = 512;
static const uint kLipHeaderSize = 8;
static const int kLipMaxEntries = (LipData::kBufferSize - kLipHeaderSize) / sizeof(LipEntry) - 1;
static const int kLipRestCode = 95;
static const int kLipRestShape = 5;
static const int kLipUnknownShape = 6;

static const LipData::MouthEntry kInitialMouthTable[LipData::kMouthTableSize] = {
	{ 95, 5 }, { 593, 6 }, { 97, 6 }, { 230, 6 }, { 652, 13 }, { 596, 6 }, { 601, 6 }, { 98, 11 },
	{ 679, 7 }, { 100, 7 }, { 240, 10 }, { 603, 13 }, { 616, 13 }, { 602, 14 }, { 605, 14 }, { 101, 6 },
	{ 102, 9 }, { 103, 13 }, { 609, 13 }, { 104, 13 }, { 618, 13 }, { 105, 8 }, { 676, 7 }, { 107, 7 },
	{ 108, 10 }, { 619, 10 }, { 109, 11 }, { 110, 13 }, { 331, 13 }, { 111, 12 }, { 112, 11 }, { 114, 7 },
	{ 635, 7 }, { 633, 7 }, { 115, 7 }, { 643, 7 }, { 116, 7 }, { 638, 7 }, { 952, 10 }, { 650, 14 },
	{ 117, 14 }, { 118, 9 }, { 119, 14 }, { 106, 13 }, { 122, 7 }, { 658, 7 }
};

void LipData::initMouthTable(EmiEngine *engine) {
	for (int i = 0; i < kMouthTableSize; i++)
		engine->_lipMouthTable[i] = kInitialMouthTable[i];
}

int LipData::readFile(EmiEngine *engine, const char *name, LipData **lipData) {
	const char *source = name;
	int result = 0;

	if (*source == '@')
		source++;

	LString path(source);
	uint dot = path.rfind('.');

	if (dot != LString::npos)
		path.erase(dot);

	bool isPs2 = engine->getGamePlatform() == Common::kPlatformPS2;

	if (isPs2)
		engine->_lipFileName = path;

	path += ".lip";

	LFile *file = engine->getFileServices()->open(path.c_str(), "rb");

	if (file) {
		int readCount = file->read(engine->_lipBuffer, kBufferSize);

		if (file->eof())
			result = readCount;

		// findCode() walks "count" entries, so a count past the buffer is clamped there
		if (readCount >= (int)kLipHeaderSize) {
			int entryCount = (int)READ_LE_UINT32(engine->_lipBuffer + 4);

			if (entryCount > kLipMaxEntries)
				warning("LipData::readFile(): '%s' has entry count %d, clamped to %d", path.c_str(), entryCount, kLipMaxEntries);
		}

		engine->getFileServices()->close(file, true);
	}

	if (isPs2 && !result)
		engine->_lipFileName.clear();

	*lipData = (LipData *)engine->_lipBuffer;

	return result;
}

void LipData::saveFileName(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	int length = (int)engine->_lipFileName.size() + 1;

	accessFunc(&length, sizeof(length));
	accessFunc((void *)engine->_lipFileName.c_str(), length);
}

int LipData::restoreFileName(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	char buffer[kFileNameBufferSize + 1];
	int length = 0;

	accessFunc(&length, sizeof(length));

	if (length < 0 || length > kFileNameBufferSize) {
		warning("LipData::restoreFileName(): invalid name length %d", length);

		return 0;
	}

	accessFunc(buffer, length);
	buffer[length] = 0;

	LipData *lipData = nullptr;

	return readFile(engine, buffer, &lipData);
}

int LipData::codeToMouthShape(EmiEngine *engine, int code) {
	if (code == kLipRestCode)
		return kLipRestShape;

	MouthEntry key;

	key.code = code;
	key.shape = 0;

	const MouthEntry *found = (const MouthEntry *)bsearch(&key, engine->_lipMouthTable, kMouthTableSize, sizeof(MouthEntry), compareMouthEntries);

	if (found)
		return (uint16)found->shape;

	return kLipUnknownShape;
}

int LipData::compareMouthEntries(const void *first, const void *second) {
	return *(const int *)first - *(const int *)second;
}

int LipData::findCode(int position) const {
	int entryCount = (int)READ_LE_UINT32(count);
	int target = position >> 4;

	if (entryCount > kLipMaxEntries)
		entryCount = kLipMaxEntries;

	for (int i = 0; i < entryCount; i++) {
		int entryTime = READ_LE_INT16(entries[i].time);

		if (target < entryTime)
			return kLipRestCode;

		if (target < READ_LE_INT16(entries[i + 1].time))
			return READ_LE_INT16(entries[i].code);
	}

	return kLipRestCode;
}

void LipData::sortMouthTable(EmiEngine *engine) {
	qsort(engine->_lipMouthTable, kMouthTableSize, sizeof(MouthEntry), compareMouthEntries);
}

int LipData::getLipMouthShape(EmiEngine *engine, uint soundHandle, int position) {
	Sounds *sounds = engine->getSounds();
	LipData *lipData = sounds->getSoundLipData(soundHandle);

	if (!lipData)
		return 0;

	int soundPosition = position;

	if (position < 0)
		soundPosition = sounds->getSoundPosition(soundHandle);
	else if (position > sounds->getSoundLength(soundHandle))
		return 0;

	return codeToMouthShape(engine, lipData->findCode(soundPosition));
}

} // End of namespace Grim
