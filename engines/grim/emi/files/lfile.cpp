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

#include "common/archive.h"
#include "common/file.h"
#include "common/path.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/core/lmemoryservices.h"

namespace Grim {

static LString normalizePath(const char *path) {
	LString result(path);

	for (uint i = 0; i < result.size(); i++) {
		if (result[i] == '\\')
			result.setChar('/', i);
	}

	while (result.hasPrefix("./"))
		result = result.substr(2);

	return result;
}

static Common::SeekableReadStream *openReadStream(const char *path) {
	return SearchMan.createReadStreamForMember(Common::Path(normalizePath(path), '/'));
}

static Common::WriteStream *openWriteStream(const char *path) {
	LString normalized = normalizePath(path);

	Common::DumpFile *file = new Common::DumpFile();
	if (!file->open(Common::Path(normalized, '/'), true)) {
		delete file;
		return nullptr;
	}

	return file;
}

static byte getOpenFlags(const char *mode) {
	byte flags = strchr(mode, 'b') != nullptr ? 16 : 32;

	if (strchr(mode, 'w'))
		flags |= 2;
	else
		flags |= 1;

	return flags;
}

static bool openStreams(const char *path, const char *mode, Common::SeekableReadStream *&readStream, Common::WriteStream *&writeStream) {
	readStream = nullptr;
	writeStream = nullptr;

	if (strchr(mode, 'a') || strchr(mode, '+'))
		warning("openStreams(): unsupported mode '%s'", mode);

	if (strchr(mode, 'w'))
		writeStream = openWriteStream(path);
	else
		readStream = openReadStream(path);

	return readStream || writeStream;
}

LFile::LFile(EmiEngine *engine) : _engine(engine) {
	_readStream = nullptr;
	_writeStream = nullptr;
	_openFlags = 17;
	_memData = nullptr;
	_memCursor = nullptr;
	_memSize = 0;
}

LFile::~LFile() {
	if (_memData)
		_engine->getMemoryServices()->freeImpl(_memData);

	_memData = nullptr;
	_memCursor = nullptr;
	_memSize = 0;

	closeStreams();
}

void *LFile::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void LFile::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

bool LFile::isOpen() const {
	return _readStream || _writeStream;
}

void LFile::closeStreams() {
	delete _readStream;
	delete _writeStream;
	_readStream = nullptr;
	_writeStream = nullptr;
}

void LFile::reset() {
	_readStream = nullptr;
	_writeStream = nullptr;
	_openFlags = 17;
	_memData = nullptr;
	_memCursor = nullptr;
	_memSize = 0;
	_fileName.clear();
	_name.clear();
}

uint LFile::read(void *buffer, uint size) {
	if (!_memData)
		return _readStream ? _readStream->read(buffer, size) : 0;

	int available = (int)_memSize - (int)(_memCursor - _memData);
	if (available < 0)
		available = 0;

	if (size > (uint)available) {
		warning("LFile::read(): read beyond the end of the memory file");
		size = available;
	}

	memcpy(buffer, _memCursor, size);
	_memCursor += size;

	return size;
}

char *LFile::readLine(char *buffer, int maxCount) {
	char *line = getString(buffer, maxCount);

	if (!line)
		return line;

	while (true) {
		char first = *line;

		if (first != ';' && first != '#' && first != '\n' && first != '\r') {
			char *hash = strchr(line, '#');
			if (hash)
				*hash = 0;

			char *next = line + 1;
			bool accepted;

			if (Common::isSpace((byte)first)) {
				char current;

				do {
					current = *next++;
				} while (Common::isSpace((byte)current));

				if (current == 0) {
					warning("LFile::readLine(): line made only of spaces");
					accepted = false;
				} else {
					accepted = *next != 0;
				}
			} else {
				accepted = *next != 0;
			}

			if (accepted)
				break;
		}

		line = getString(buffer, maxCount);
		if (!line)
			return line;
	}

	for (char *p = line; *p; p++) {
		if (Common::isUpper((byte)*p))
			*p += 'a' - 'A';
	}

	char *last = line + strlen(line) - 1;
	while (last >= line && (*last == '\n' || *last == '\r')) {
		*last = 0;
		last--;
	}

	return line;
}

uint LFile::write(const void *buffer, uint size) {
	if (_memData)
		return 0;

	return _writeStream ? _writeStream->write(buffer, size) : 0;
}

int LFile::eof() {
	if (_memData)
		return (uint)(_memCursor - _memData) >= _memSize;

	return _readStream && _readStream->eos();
}

int LFile::tell() {
	if (_memData)
		return _memCursor - _memData;

	if (_readStream)
		return _readStream->pos();

	if (_writeStream)
		return _writeStream->pos();

	warning("LFile::tell(): file not open");

	return -1;
}

int LFile::seek(int offset, int whence) {
	if (!_memData) {
		if (whence != 0 && whence != 1 && whence != 2)
			return 0;

		if (_readStream)
			return _readStream->seek(offset, whence) ? 0 : -1;

		if (_writeStream) {
			Common::SeekableWriteStream *seekableStream = dynamic_cast<Common::SeekableWriteStream *>(_writeStream);

			if (seekableStream)
				return seekableStream->seek(offset, whence) ? 0 : -1;

			warning("LFile::seek(): write stream is not seekable");

			return -1;
		}

		warning("LFile::seek(): file not open");

		return -1;
	}

	if (whence == 0) {
		if ((uint)offset > _memSize)
			return 1;

		_memCursor = _memData + offset;

		return 0;
	}

	if (whence == 1) {
		int position = (int)(_memCursor - _memData) + offset;

		if ((uint)position <= _memSize && position >= 0) {
			_memCursor += offset;

			return 0;
		}

		return 1;
	}

	if (whence == 2) {
		if ((uint)offset <= _memSize) {
			_memCursor = _memData + _memSize - offset;

			return 0;
		}

		return 1;
	}

	return 0;
}

int LFile::writeFormat(const char *format, ...) {
	if (_memData)
		return 0;

	if (!_writeStream) {
		warning("LFile::writeFormat(): file not open for writing");

		return -1;
	}

	va_list args;
	va_start(args, format);
	LString text = LString::vformat(format, args);
	va_end(args);

	_writeStream->write(text.c_str(), text.size());

	return text.size();
}

int LFile::unreadChar(int character) {
	if (!_memData) {
		if (!_readStream || character == -1 || _readStream->pos() == 0)
			return -1;

		_readStream->seek(-1, SEEK_CUR);

		return character;
	}

	if (_memCursor - _memData <= 0)
		return 0;

	_memCursor--;

	return character;
}

uint LFile::loadAll() {
	if (!_readStream) {
		warning("LFile::loadAll(): file not open for reading");

		return 0;
	}

	seek(0, 2);
	uint size = tell() + 1;
	seek(0, 0);

	_memData = (char *)_engine->getMemoryServices()->mallocImpl(size);
	_memCursor = _memData;
	_memSize = _readStream->read(_memData, size);
	_memData[size - 1] = 0;

	return _memSize;
}

const char *LFile::getName() {
	return nullptr;
}

char *LFile::getString(char *buffer, int maxCount) {
	if (maxCount <= 0) {
		warning("LFile::getString(): invalid buffer size");

		return nullptr;
	}

	if (!_memData) {
		if (!_readStream)
			return nullptr;

		int count = 0;

		while (count < maxCount - 1) {
			byte character;

			if (_readStream->read(&character, 1) != 1)
				break;

			buffer[count++] = character;

			if (character == '\n')
				break;
		}

		if (count == 0 && maxCount > 1)
			return nullptr;

		buffer[count] = 0;

		return buffer;
	}

	if ((uint)(_memCursor - _memData) >= _memSize)
		return nullptr;

	char *end = strchr(_memCursor, '\n');
	if (!end)
		end = strchr(_memCursor, 0);

	int length = end - _memCursor + 1;
	if (length >= maxCount)
		length = maxCount - 1;

	memcpy(buffer, _memCursor, length);
	buffer[length] = 0;
	_memCursor += length;

	return buffer;
}

LFileServices::LFileServices(EmiEngine *engine) : _engine(engine) {
	if (!engine->getFileServices())
		engine->pushFileServices(this);
}

LFileServices::~LFileServices() {
}

void *LFileServices::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void LFileServices::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

void LFileServices::openInto(LFile *file, const char *path, const char *mode) {
	byte flags = getOpenFlags(mode);
	Common::SeekableReadStream *readStream;
	Common::WriteStream *writeStream;

	if (!openStreams(path, mode, readStream, writeStream))
		return;

	if (!file) {
		delete readStream;
		delete writeStream;

		return;
	}

	file->_readStream = readStream;
	file->_writeStream = writeStream;
	file->_openFlags = flags;
	file->_fileName = path;
}

LFile *LFileServices::open(const char *path, const char *mode) {
	byte flags = getOpenFlags(mode);
	Common::SeekableReadStream *readStream;
	Common::WriteStream *writeStream;

	if (!openStreams(path, mode, readStream, writeStream))
		return nullptr;

	LFile *file = new LFile(_engine);
	file->_openFlags = flags;
	file->_readStream = readStream;
	file->_writeStream = writeStream;
	file->_fileName = path;

	return file;
}

LFile *LFileServices::openExtended(const char *path, const char *mode, int unused1, int unused2) {
	return LFileServices::open(path, mode);
}

void LFileServices::openIntoAndLoad(LFile *file, const char *path, const char *mode) {
	openInto(file, path, mode);

	if (file)
		file->loadAll();
}

LFile *LFileServices::openAndLoad(const char *path, const char *mode) {
	LFile *file = open(path, mode);

	if (file)
		file->loadAll();

	return file;
}

LFile *LFileServices::openExtendedAndLoad(const char *path, const char *mode, int unused1, int unused2) {
	LFile *file = open(path, mode);

	if (file)
		file->loadAll();

	return file;
}

bool LFileServices::exists(const char *path) {
	return SearchMan.hasFile(Common::Path(normalizePath(path), '/'));
}

int LFileServices::getModTime(const char *path) {
	// ScummVM has no modification time
	return 0;
}

void LFileServices::close(LFile *file, bool destroy) {
	if (!file) {
		warning("LFileServices::close(): null file");

		return;
	}

	if (destroy)
		delete file;
	else
		file->closeStreams();
}

int LFileServices::getSize(const char *path) {
	LFile *file = open(path, "rb");

	if (!file) {
		warning("LFileServices::getSize(): cannot open the file");

		return 0;
	}

	file->seek(0, 2);
	int size = file->tell();

	delete file;

	return size;
}

bool LFileServices::resolvePath(LString &outPath, const char *name) {
	return false;
}

} // End of namespace Grim
