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

	if (_playing) {
		if (isHdMovieName(_curMovieFile))
			_playing = 0;
		else
			startMovie(_curMovieFile, _looping, _x, _y);
	}

	return 0;
}

bool SputSmush::isHdMovieName(const char *name) {
	static const char *const hdMovieNames[] = {
		"albinizod.snm", "brunopk.snm", "byebye.snm", "byeruba.snm",
		"coffrock.snm", "copaldie.snm", "credits.snm", "crushed.snm",
		"distbeav.snm", "dooropen.snm", "eatbird.snm", "eldepot.snm",
		"escape.snm", "exodus.snm", "falling.snm", "getshcks.snm",
		"glotdriv.snm", "greenhse.snm", "hecdie.snm", "hecgetit.snm",
		"heloruba.snm", "heltrain.snm", "hostage.snm", "intro.snm",
		"logos.snm", "lol.snm", "loladies.snm", "lsahq.snm",
		"mnycutfl.snm", "neonlady.snm", "normarae.snm", "oldjob.snm",
		"plunge.snm", "poseidon.snm", "repmec1c.snm", "repmec3c.snm",
		"reunion.snm", "shanghai.snm", "stocking.snm", "stump1c.snm",
		"stump3c.snm", "subsaway.snm", "swtchbrd.snm", "swtchsuc.snm",
		"swtchuns.snm", "thepearl.snm", "vivamaro.snm", "yr2intro.snm"
	};

	for (uint i = 0; i < ARRAYSIZE(hdMovieNames); i++) {
		if (!strcmp(name, hdMovieNames[i]))
			return true;
	}

	return false;
}

} // End of namespace Grim
