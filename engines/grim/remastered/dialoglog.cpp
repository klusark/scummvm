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

// The ring never fills up: when the tail reaches the head, the oldest line is dropped
void DialogLog::addLine(const char *text) {
	Common::strlcpy(_lines[_tail], text, kLineSize);

	_tail = (_tail + 1) % kMaxLines;

	if (_tail == _head)
		_head = (_head + 1) % kMaxLines;
}

int DialogLog::getLineCount() const {
	return (_tail - _head + kMaxLines) % kMaxLines;
}

int DialogLog::getLineIndex(int position) const {
	return (_head + position) % kMaxLines;
}

const char *DialogLog::getLine(int ringIndex) const {
	if (ringIndex < 0 || ringIndex >= kMaxLines) {
		warning("DialogLog::getLine(): invalid line %d", ringIndex);
		return nullptr;
	}

	return _lines[ringIndex];
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

	for (int i = 0; i < kMaxLines; i++)
		_lines[i][kLineSize - 1] = 0;

	_head = head;
	_tail = tail;

	return 0;
}

} // End of namespace Grim
