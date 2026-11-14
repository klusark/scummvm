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
#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/remastered/dialoglog.h"

namespace Grim {

DialogLog::DialogLog() : _head(0), _tail(0) {
	memset(_lines, 0, sizeof(_lines));
}

void DialogLog::clear() {
	memset(_lines, 0, sizeof(_lines));
	_head = 0;
	_tail = 0;
}

void DialogLog::addLine(const char *text) {
	char *slot = _lines[_tail];

	memset(slot, 0, kLineSize);

	for (int i = 0; i < kLineSize && text[i]; i++)
		slot[i] = text[i];

	_tail = (_tail + 1) % kMaxLines;

	if (_tail == _head)
		_head = (_head + 1) % kMaxLines;
}

int DialogLog::getLineCount() const {
	if (_head == _tail)
		return 0;

	return (uint)_tail > (uint)_head ? _tail : kMaxLines;
}

const char *DialogLog::getLine(int slot) const {
	if (getLineCount() <= slot)
		return nullptr;

	if (slot < 0) {
		warning("DialogLog::getLine(): invalid slot %d", slot);
		return nullptr;
	}

	return _lines[slot];
}

const char *DialogLog::getNextLine(int *cursor) const {
	if (!cursor)
		return nullptr;

	if (_head == _tail) {
		*cursor = -1;
		return nullptr;
	}

	int current = *cursor;

	if (current == -1) {
		*cursor = _head;
		return _lines[_head];
	}

	if (current < 0 || current >= kMaxLines) {
		warning("DialogLog::getNextLine(): invalid cursor %d", current);
		*cursor = -1;
		return nullptr;
	}

	int next = (current + 1) % kMaxLines;

	if (next == _head || next == _tail) {
		*cursor = -1;
		return nullptr;
	}

	*cursor = next;

	return _lines[current];
}

bool DialogLog::copyLine(int slot, char *buffer, int bufferSize, int length) const {
	if (!getLine(slot))
		return false;

	const char *text = &_lines[0][0] + slot * kLineSize;
	int available = kMaxLines * kLineSize - slot * kLineSize;

	if (length > available)
		length = available;

	if (length > bufferSize - 1)
		length = bufferSize - 1;

	memset(buffer, 0, bufferSize);

	for (int i = 0; i < length && text[i]; i++)
		buffer[i] = text[i];

	return true;
}

int DialogLog::save(Common::WriteStream *stream) const {
	stream->write(_lines, sizeof(_lines));
	stream->writeSint32LE(_head);
	stream->writeSint32LE(_tail);

	if (stream->err()) {
		warning("DialogLog::save(): failed to write the dialog log");
		return 1;
	}

	return 0;
}

int DialogLog::restore(Common::ReadStream *stream) {
	char lines[kMaxLines][kLineSize];

	stream->read(lines, sizeof(lines));

	int head = stream->readSint32LE();
	int tail = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("DialogLog::restore(): failed to read the dialog log");
		return 1;
	}

	if (head < 0 || head >= kMaxLines || tail < 0 || tail >= kMaxLines) {
		warning("DialogLog::restore(): invalid indices %d and %d", head, tail);
		return 1;
	}

	memcpy(_lines, lines, sizeof(_lines));

	_head = head;
	_tail = tail;

	return 0;
}

} // End of namespace Grim
