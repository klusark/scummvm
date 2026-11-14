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

#include "engines/grim/grim.h"
#include "engines/grim/puck.h"
#include "engines/grim/remastered/hotspot.h"
#include "engines/grim/remastered/sputmouse.h"
#include "engines/grim/sputobjectstate.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/std/std_bitmap.h"
#include "engines/grim/walkplane.h"

namespace Grim {

int SputRoom::saveRemastered(Common::WriteStream *stream) {
	ResNode *next;
	void *handle;
	int numRooms = 0;

	_res->allocatedIterFirst(&next, &handle, kSputTagSet);

	while (next) {
		if (_res->countDependents(handle) > 0)
			numRooms++;

		_res->allocatedIterNext(&next, &handle, kSputTagSet);
	}

	stream->writeSint32LE(numRooms);

	if (stream->err()) {
		warning("SputRoom::saveRemastered(): failed to write the room count");
		return 1;
	}

	_res->allocatedIterFirst(&next, &handle, kSputTagSet);

	while (next) {
		if (_res->countDependents(handle)) {
			Room *room = static_cast<Room *>(handle);

			stream->writeSint32LE(_res->serialFromHandle(handle));
			stream->writeSint32LE(0);
			stream->writeSint32LE(room->curSetup);
			stream->writeSint32LE(room->roomRef.valid);

			for (int i = 0; i < (int)room->numSetups; i++) {
				RoomSetup *setup = &room->setups[i];
				int numStates = 0;

				for (SputObjectState *state = setup->ObjectStates; state; state = state->next)
					numStates++;

				stream->writeSint32LE(numStates);

				for (SputObjectState *state = setup->ObjectStates; state; state = state->next) {
					stream->writeSint32LE(state->type);
					stream->write(state->colorImage.fileNameArray, 32);
					stream->write(state->zImage.fileNameArray, 32);
					stream->writeSint32LE(state->colorImage.copyMode);

					if (!state->colorImage.bitmapRef.valid)
						error("SputRoom::saveRemastered(): Bad Bitmap Reference");

					StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(state->colorImage.bitmapRef.handle);

					stream->writeSint32LE(bitmap->curFrame);
					stream->writeSint32LE(state->zImage.curFrame);
				}
			}
		}

		_res->allocatedIterNext(&next, &handle, kSputTagSet);
	}

	Room *currentRoom = getCurrentRoom();

	stream->writeSint32LE(_res->serialFromHandle(currentRoom));
	stream->writeSint32LE(0);

	if (currentRoom) {
		stream->writeByte(currentRoom->minVol);
		stream->writeByte(currentRoom->maxVol);
		stream->writeFloatLE(currentRoom->maxDist);
		stream->writeFloatLE(currentRoom->panFactor);
		stream->writeFloatLE(currentRoom->maxRadius);

		for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
			stream->writeSint32LE(plane->_number);
			stream->writeSint32LE(plane->_active);
			stream->writeSint32LE(plane->_type);
			stream->writeSint32LE(plane->_enabled);
			stream->writeFloatLE(plane->_tolerance);
		}
	}

	if (stream->err()) {
		warning("SputRoom::saveRemastered(): failed to write the room state");
		return 1;
	}

	return 0;
}

int SputRoom::restoreRemastered(Common::ReadStream *stream) {
	int numRooms = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputRoom::restoreRemastered(): failed to read the room count");
		return 1;
	}

	for (int i = 0; i < numRooms; i++) {
		int serial = stream->readSint32LE();

		stream->readSint32LE();

		if (stream->err() || stream->eos()) {
			warning("SputRoom::restoreRemastered(): failed to read room %d of %d", i + 1, numRooms);
			return 1;
		}

		Room *room = static_cast<Room *>(_res->handleFromSerialNumber(serial));

		if (!room) {
			warning("SputRoom::restoreRemastered(): room %d not found", serial);
			return 1;
		}

		room->curSetup = stream->readSint32LE();

		if (_currentRoomRef.valid)
			_res->removeReference(&_currentRoomRef);

		_res->setReference(&_currentRoomRef, room);

		if (!_currentRoomRef.valid)
			error("SputRoom::restoreRemastered(): Bad room Reference");

		loadResources(static_cast<Room *>(_currentRoomRef.handle));
		Common::strlcpy(_currentRoomName, room->name, 32);

		int roomRef = stream->readSint32LE();

		if (roomRef)
			_res->setReference(&room->roomRef, room);

		for (int setupIndex = 0; setupIndex < (int)room->numSetups; setupIndex++) {
			RoomSetup *setup = &room->setups[setupIndex];
			int numStates = stream->readSint32LE();

			for (int j = 0; j < numStates; j++) {
				char bitmapName[32];
				char zbufferName[32];
				int objectType = stream->readSint32LE();

				stream->read(bitmapName, 32);
				stream->read(zbufferName, 32);

				int copyMode = stream->readSint32LE();
				int colorFrame = stream->readSint32LE();
				int zFrame = stream->readSint32LE();

				if (stream->err() || stream->eos()) {
					warning("SputRoom::restoreRemastered(): failed to read object state %d of setup %d", j + 1, setupIndex);
					return 1;
				}

				if (bitmapName[31] || zbufferName[31]) {
					warning("SputRoom::restoreRemastered(): object state name is not terminated");
					bitmapName[31] = 0;
					zbufferName[31] = 0;
				}

				addObjectState(setupIndex, objectType, bitmapName, zbufferName, copyMode);

				SputObjectState *head = setup->ObjectStates;

				if (!head) {
					warning("SputRoom::restoreRemastered(): could not add object state %d of setup %d", j + 1, setupIndex);
					continue;
				}

				if (head->colorImage.bitmapRef.valid)
					static_cast<StdBitmapStruct *>(head->colorImage.bitmapRef.handle)->curFrame = colorFrame;

				if (head->zImage.bitmapRef.valid)
					static_cast<StdBitmapStruct *>(head->zImage.bitmapRef.handle)->curFrame = zFrame;
			}

			for (int k = 0; k < numStates; k++) {
				if (!setup->ObjectStates) {
					warning("SputRoom::restoreRemastered(): setup %d has no object states to reorder", setupIndex);
					break;
				}

				setup->ObjectStates = setup->ObjectStates->moveToBack(setup->ObjectStates);
			}
		}
	}

	int currentSerial = stream->readSint32LE();

	stream->readSint32LE();

	Room *currentRoom = static_cast<Room *>(_res->handleFromSerialNumber(currentSerial));

	setCurrentRoom(currentRoom);

	if (currentRoom) {
		_grim->makeCurrentSetupLib(currentRoom->curSetup);
		_grim->getSputMouse()->getHotspots()->load(currentRoom->name);

		_walkSystem->setCollection(currentRoom->name);

		currentRoom->minVol = stream->readByte();
		currentRoom->maxVol = stream->readByte();
		currentRoom->maxDist = stream->readFloatLE();
		currentRoom->panFactor = stream->readFloatLE();
		currentRoom->maxRadius = stream->readFloatLE();

		for (WalkPlane *plane = _walkSystem->getHead(); plane; plane = plane->_next) {
			int number = stream->readSint32LE();

			if (number == plane->_number) {
				plane->_active = stream->readSint32LE();
				_walkSystem->markPathFinderDirty();
				plane->setType(stream->readSint32LE());
				plane->_enabled = stream->readSint32LE();

				float tolerance = stream->readFloatLE();

				if (tolerance == 0.0f)
					plane->_tolerance = 0.005f;
				else
					plane->_tolerance = tolerance;
			}
		}
	}

	if (stream->err() || stream->eos()) {
		warning("SputRoom::restoreRemastered(): failed to read the current room state");
		return 1;
	}

	return 0;
}

} // End of namespace Grim
