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

#include "engines/grim/text/polyglot.h"
#include "engines/grim/std/std_sort.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

Polyglot::Polyglot() : _hs(nullptr), _languageMalloc(nullptr), _languageLine(nullptr), _lineCount(0) {
}

int Polyglot::initialize(SysStruct *hs) {
	_hs = hs;

	return 0;
}

int Polyglot::startup() {
	return 0;
}

void Polyglot::shutdown() {
	delete[] _languageMalloc;
	_languageMalloc = nullptr;

	delete[] _languageLine;
	_languageLine = nullptr;
	_lineCount = 0;
}

void Polyglot::configure(const char *tabFile) {
	Common::SeekableReadStream *file = _hs->openFile(tabFile);

	if (!file)
		return;

	int size = file->size();
	uint32 magic = 0;

	if (size >= 4)
		magic = file->readUint32LE();

	file->seek(0);

	_languageMalloc = new char[size + 3];
	memset(_languageMalloc + size, 0, 3);
	file->read(_languageMalloc, size);
	delete file;

	if (magic == MKTAG('E', 'N', 'C', 'R')) {
		size -= 4;

		for (int i = 0; i < size; i++)
			_languageMalloc[i] = _languageMalloc[i + 4] ^ 0xDD;
	}

	char *line = _languageMalloc;

	do {
		line = findChar(line, 13) + 2;
		_lineCount++;
	} while (line - _languageMalloc < size - 1);

	_languageLine = new char *[_lineCount];

	line = _languageMalloc;
	int lineIndex = 0;

	do {
		char *lineEnd = findChar(line, 13);

		*lineEnd = 0;

		char *tab = strchr(line, 9);

		if (tab)
			*tab = 0;

		for (char *p = line; *p; p++) {
			if (*p >= 'a' && *p <= 'z')
				*p -= 0x20;
		}

		_languageLine[lineIndex] = line;
		line = lineEnd + 2;
		lineIndex++;
	} while (line - _languageMalloc < size - 1);

	sort(_languageLine, _lineCount, sizeof(char *), WuRegistry::compareNames);
}

char *Polyglot::findChar(char *s, int c) {
	char *result = s;

	for (char i = *s; i; i = *++result) {
		if (i == c)
			break;
	}

	return result;
}

const char *Polyglot::getTranslation(const char *msg) {
	char key[256];

	if (!msg)
		return nullptr;

	if (*msg != '/')
		return msg;

	const char *text = strchr(msg + 1, '/');

	if (text) {
		text++;
	} else {
		warning("Polyglot::getTranslation(): the message has no closing slash");
		text = msg;
	}

	if (!_languageMalloc)
		return text;

	if (strlen(msg + 1) >= sizeof(key))
		warning("Polyglot::getTranslation(): the message is too long, truncated");

	Common::strlcpy(key, msg + 1, sizeof(key));

	char *slash = strchr(key, '/');

	if (slash)
		*slash = 0;

	for (char *p = key; *p; p++) {
		if (*p >= 'a' && *p <= 'z')
			*p -= 0x20;
	}

	const char *keyPointer = key;
	char **found = (char **)binarySearch(&keyPointer, _languageLine, _lineCount, sizeof(char *), WuRegistry::compareNames);

	if (!found)
		return text;

	const char *translation = *found + strlen(*found) + 1;

	if (!*translation)
		return msg;

	return translation;
}

} // End of namespace Grim
