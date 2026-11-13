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
#include "common/textconsole.h"

#include "engines/grim/movie/sputsmush.h"

namespace Grim {

enum {
	// The remastered saves the name of the movie in 255 bytes
	kRemasteredMovieNameSize = 255
};

int SputSmush::saveRemastered(Common::WriteStream *stream) {
	byte padding[kRemasteredMovieNameSize - sizeof(_curMovieFile)];

	memset(padding, 0, sizeof(padding));

	stream->writeSint32LE(_playing);
	stream->writeSint32LE(_paused);
	stream->writeSint32LE(_abort);
	stream->write(_curMovieFile, sizeof(_curMovieFile));
	stream->write(padding, sizeof(padding));
	stream->writeSint32LE(_looping);
	stream->writeSint32LE(_x);
	stream->writeSint32LE(_y);

	if (stream->err()) {
		warning("SputSmush::saveRemastered(): failed to write the movie state");
		return 1;
	}

	return 0;
}

int SputSmush::restoreRemastered(Common::ReadStream *stream) {
	char name[kRemasteredMovieNameSize];

	setTarget(_mainGfxBuffer);

	_playing = stream->readSint32LE();
	_paused = stream->readSint32LE();
	_abort = stream->readSint32LE();
	stream->read(name, sizeof(name));
	_looping = stream->readSint32LE();
	_x = stream->readSint32LE();
	_y = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputSmush::restoreRemastered(): failed to read the movie state");
		return 1;
	}

	name[sizeof(name) - 1] = '\0';

	if (strlen(name) >= sizeof(_curMovieFile))
		warning("SputSmush::restoreRemastered(): the movie name is too long, truncated");

	Common::strlcpy(_curMovieFile, name, sizeof(_curMovieFile));

	if (_playing)
		startMovie(_curMovieFile, _looping, _x, _y);

	return 0;
}

} // End of namespace Grim
