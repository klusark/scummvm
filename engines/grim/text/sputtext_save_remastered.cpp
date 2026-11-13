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

#include "common/str.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/actor/sputactor.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/res.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/text/text.h"

namespace Grim {

// The events of the savegames of the remastered: the text takes 8192 bytes, the pointers of the
// surface, the clip and the font and the serial of the event take 8, and there are four more
// values for the highlight, the coordinates and the alpha
int SputText::saveRemastered(Common::WriteStream *stream) {
	int numEvents = 0;

	for (SputTextEvent *event = _textQueue; event; event = event->next)
		numEvents++;

	stream->writeSint32LE(numEvents);

	if (stream->err())
		return 1;

	for (SputTextEvent *event = _textQueue; event; event = event->next) {
		// The surface and the clip only have to be different from zero
		stream->write(event->text, kTextEventCapacity);
		stream->writeSint32LE(1);
		stream->writeSint32LE(0);
		stream->writeSint32LE(1);
		stream->writeSint32LE(0);
		stream->writeSint32LE(_res->serialFromHandle(event->params.font));
		stream->writeSint32LE(0);
		stream->writeSint32LE(event->params.x);
		stream->writeSint32LE(event->params.y);
		stream->writeSint32LE(event->params.fgColor);
		stream->writeSint32LE(event->params.bgColor);
		stream->writeSint32LE(event->params.fxColor);
		stream->writeSint32LE(event->params.scaleX);
		stream->writeSint32LE(event->params.scaleY);
		stream->writeSint32LE(event->params.flags);
		stream->writeSint32LE(0);
		stream->writeSint32LE(event->params.highlight);
		stream->writeSint32LE(event->params.coords);
		stream->writeFloatLE(event->params.alpha);
		stream->writeSint32LE(event->x);
		stream->writeSint32LE(event->y);
		stream->writeSint32LE(event->clip.x);
		stream->writeSint32LE(event->clip.y);
		stream->writeSint32LE(event->clip.width);
		stream->writeSint32LE(event->clip.height);
		stream->writeSint32LE(event->duration);
		stream->writeSint32LE(event->actor ? _sputActor->getActorHandle(event->actor) : -1);
		stream->writeSint32LE(event->flags);
		stream->writeSint32LE(event->serial);
		stream->writeSint32LE(0);
	}

	stream->writeSint32LE(_res->serialFromHandle(_printLineParams.font));
	stream->writeSint32LE(0);
	stream->writeSint32LE(_res->serialFromHandle(_defaultTextParams.font));
	stream->writeSint32LE(0);
	stream->writeSint32LE(_res->serialFromHandle(_blastTextParams.font));
	stream->writeSint32LE(0);

	if (stream->err()) {
		warning("SputText::saveRemastered(): write error");
		return 1;
	}

	return 0;
}

int SputText::restoreRemastered(Common::ReadStream *stream) {
	while (_textQueue)
		killEvent(_textQueue);

	int numEvents = stream->readSint32LE();

	if (stream->err() || stream->eos() || numEvents < 0) {
		warning("SputText::restoreRemastered(): invalid event count");
		return 1;
	}

	for (int i = 0; i < numEvents; i++) {
		SputTextEvent *event = new SputTextEvent();

		stream->read(event->text, kTextEventCapacity);

		// The pointers of the surface and the clip
		stream->readSint32LE();
		stream->readSint32LE();
		stream->readSint32LE();
		stream->readSint32LE();

		int fontSerial = stream->readSint32LE();

		stream->readSint32LE();

		event->params.x = stream->readSint32LE();
		event->params.y = stream->readSint32LE();
		event->params.fgColor = stream->readSint32LE();
		event->params.bgColor = stream->readSint32LE();
		event->params.fxColor = stream->readSint32LE();
		event->params.scaleX = stream->readSint32LE();
		event->params.scaleY = stream->readSint32LE();
		event->params.flags = stream->readSint32LE();

		stream->readSint32LE();

		int highlight = stream->readSint32LE();
		int coords = stream->readSint32LE();
		float alpha = stream->readFloatLE();

		event->x = stream->readSint32LE();
		event->y = stream->readSint32LE();
		event->clip.x = stream->readSint32LE();
		event->clip.y = stream->readSint32LE();
		event->clip.width = stream->readSint32LE();
		event->clip.height = stream->readSint32LE();
		event->duration = stream->readSint32LE();

		// The actor is found again when the actors are restored
		stream->readSint32LE();

		event->flags = stream->readSint32LE();
		event->serial = stream->readSint32LE();

		stream->readSint32LE();

		if (stream->err() || stream->eos()) {
			warning("SputText::restoreRemastered(): read error");
			delete event;
			event = nullptr;

			return 1;
		}

		if (event->text[kTextEventCapacity - 1] != 0) {
			warning("SputText::restoreRemastered(): unterminated event text");
			event->text[kTextEventCapacity - 1] = 0;
		}

		event->params.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(fontSerial));

		event->params.surface = _blastText->getSurface();
		event->params.clip = &event->clip;
		event->params.colorMap = (byte *)_sputRender->getColors();
		event->actor = nullptr;
		initHdEvent(event);
		event->params.coords = coords;
		event->params.highlight = highlight;
		event->params.alpha = alpha;

		event->prev = nullptr;
		event->next = _textQueue;

		if (_textQueue)
			_textQueue->prev = event;

		_textQueue = event;
	}

	int printLineFontSerial = stream->readSint32LE();

	stream->readSint32LE();

	int defaultFontSerial = stream->readSint32LE();

	stream->readSint32LE();

	int blastTextFontSerial = stream->readSint32LE();

	stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputText::restoreRemastered(): read error");
		return 1;
	}

	_font = _text->getSystemFont();
	_printLineParams.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(printLineFontSerial));
	_defaultTextParams.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(defaultFontSerial));
	_blastTextParams.font = static_cast<BlastTextFont *>(_res->handleFromSerialNumber(blastTextFontSerial));

	if (!_printLineParams.font)
		_printLineParams.font = _font;

	if (!_defaultTextParams.font)
		_defaultTextParams.font = _font;

	if (!_blastTextParams.font)
		_blastTextParams.font = _font;

	return 0;
}

} // End of namespace Grim
