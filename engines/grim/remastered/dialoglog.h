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

#ifndef GRIM_DIALOGLOG_H
#define GRIM_DIALOGLOG_H

#include "common/scummsys.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

// Ring buffer with the last lines of dialog, kept in the save game
class DialogLog {
public:
	enum {
		kMaxLines = 100,
		kLineSize = 150
	};

	DialogLog();

	// Empties the log
	void clear();

	void addLine(const char *text);

	// Number of line slots in use: 0 when the log is empty, the tail index when the ring has not
	// wrapped yet and 100 after that
	int getLineCount() const;
	// The line at a slot, or null when the slot is not in use
	const char *getLine(int slot) const;
	// Walks the ring from the oldest line. Starts with the cursor at -1 and gives the cursor of
	// each line (the last slot, where the next line goes, is never given). Null at the end.
	const char *getNextLine(int *cursor) const;
	// Copies at most length characters of the text from the start of a slot, like strncpy: the
	// text goes on in the next slots when the line has no end. The rest of the buffer is zero.
	bool copyLine(int slot, char *buffer, int bufferSize, int length) const;

	// Block of the save game: the lines, then the head and the tail index
	int save(Common::WriteStream *stream) const;
	int restore(Common::ReadStream *stream);

private:
	char _lines[kMaxLines][kLineSize];
	int _head;
	int _tail;
};

} // End of namespace Grim

#endif
