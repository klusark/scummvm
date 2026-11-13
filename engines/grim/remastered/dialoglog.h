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

	void addLine(const char *text);

	// Number of lines in the log, the oldest one is at ring index getLineIndex(0)
	int getLineCount() const;
	int getLineIndex(int position) const;
	const char *getLine(int ringIndex) const;

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
