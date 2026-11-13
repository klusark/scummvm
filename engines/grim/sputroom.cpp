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
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/puck.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/sputobjectstate.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/std/std_bitmap.h"
#include "engines/grim/std/std_conffile.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/std/std_lbm.h"
#include "engines/grim/walkplane.h"

namespace Grim {

SputRoom::SputRoom(RenderDroid *rd, Res *res, SputResource *sputResource, StdDisplay *stdDisplay, StdBitmap *stdBitmap, StdLbm *stdLbm,
		StdConffile *stdConffile, WalkSystem *walkSystem) : _rd(rd), _res(res), _sputResource(sputResource), _stdDisplay(stdDisplay),
		_stdBitmap(stdBitmap), _stdLbm(stdLbm), _stdConffile(stdConffile), _walkSystem(walkSystem), _sputRender(nullptr),
		_grim(nullptr), _sputActor(nullptr), _actors(nullptr), _hs(nullptr), _initialized(0), _started(0), _soundList(nullptr), _actorNodeList(nullptr),
		_loadingRoom(nullptr), _updateLoop(0), _numSections(0) {
	memset(&_currentRoomRef, 0, sizeof(_currentRoomRef));
	memset(_currentRoomName, 0, sizeof(_currentRoomName));

	for (int i = 0; i < 32; i++) {
		memset(_sections[i].name, 0, sizeof(_sections[i].name));
		_sections[i].handler = nullptr;
	}
}

StdVBuffer *SputRoom::getSetupBackground(Room *room, int setupIndex) {
	RoomSetup *setup = &room->setups[setupIndex];
	ResRef *bitmapRef = &setup->bitmapRef;

	if (bitmapRef->valid) {
		_res->touch(bitmapRef->handle);
	} else {
		if (scumm_stricmp(setup->bitmapName, "<none>")) {
			void *handle = _sputResource->alloc(kSputTagBknd, setup->bitmapName);

			if (handle)
				_res->setReference(bitmapRef, handle);
		}

		if (!bitmapRef->valid) {
			void *handle = _sputResource->alloc(kSputTagBknd, "dfltroom.bm");

			_res->setReference(bitmapRef, handle);

			if (!bitmapRef->valid)
				return nullptr;

			Common::strlcpy(setup->bitmapName, "<none>", 32);
			setup->zBufferName[0] = 0;
		}
	}

	StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(bitmapRef->handle);

	if (bitmap->curFrame < 0 || (uint)bitmap->curFrame >= bitmap->numFrames) {
		warning("SputRoom::getSetupBackground(): frame %d of %s is out of range", bitmap->curFrame, setup->bitmapName);
		return nullptr;
	}

	return bitmap->frames[bitmap->curFrame];
}

StdVBuffer *SputRoom::getSetupZBackground(Room *room, int setupIndex) {
	RoomSetup *setup = &room->setups[setupIndex];
	ResRef *zBufferRef = &setup->zbufferRef;

	if (zBufferRef->valid) {
		_res->touch(zBufferRef->handle);
	} else if (setup->zBufferName[0]) {
		if (strcmp(setup->zBufferName, "<none>")) {
			void *handle = _sputResource->alloc(kSputTagZbuf, setup->zBufferName);

			if (handle)
				_res->setReference(zBufferRef, handle);
		}
	}

	if (zBufferRef->valid)
		return static_cast<StdVBuffer *>(zBufferRef->handle);

	return nullptr;
}

void SputRoom::relocateSounds() {
	freeSoundTable();

	for (RoomSound *sound = _soundList; sound; sound = sound->next)
		setupSoundPosition(sound);
}

void SputRoom::setSoundParameters(Room *room, float maxDist, byte minVol, byte maxVol, float panFactor, float maxRadius) {
	room->minVol = minVol;
	room->maxDist = maxDist;
	room->maxVol = maxVol;
	room->panFactor = panFactor;
	room->maxRadius = maxRadius;
}

void SputRoom::getSoundParameters(Room *room, float *maxDist, byte *minVol, byte *maxVol, float *panFactor, float *maxRadius) {
	*maxDist = room->maxDist;
	*minVol = room->minVol;
	*maxVol = room->maxVol;
	*panFactor = room->panFactor;
	*maxRadius = room->maxRadius;
}

int SputRoom::setSoundPosition(const char *soundName, float *pos, int minVol, int maxVol, float maxDist, float radius) {
	int result = g_grim->getImuse()->soundsIsSoundPlaying(soundName);

	if (result) {
		RoomSound *sound = allocSoundTable(soundName, pos, minVol, maxVol, maxDist, radius);

		return setupSoundPosition(sound);
	}

	return result;
}

void SputRoom::setCurrentRoom(Room *room, bool runUpdate) {
	if (!_currentRoomRef.valid || _currentRoomRef.handle != room) {
		if (_currentRoomRef.valid) {
			unReference(static_cast<Room *>(_currentRoomRef.handle));
			_res->removeReference(&_currentRoomRef);
			_currentRoomName[0] = 0;
		}

		if (room) {
			_res->setReference(&_currentRoomRef, room);

			if (!_currentRoomRef.valid)
				error("SputRoom::setCurrentRoom(): Bad room Reference");

			loadResources(static_cast<Room *>(_currentRoomRef.handle));
			Common::strlcpy(_currentRoomName, room->name, 32);

			if (!(_grim->getGameFlags() & ADGF_DEMO))
				setSoundParameters(room, 8.0f, 20, 127, 1.5f, 0.8f);

			if (runUpdate && _grim->useRemasterFixes() && !(_grim->getGameFlags() & ADGF_DEMO))
				_sputActor->updateActors(0.0f);
		}
	}
}

Room *SputRoom::getCurrentRoom() {
	if (_currentRoomRef.valid)
		return static_cast<Room *>(_currentRoomRef.handle);

	return nullptr;
}

const char *SputRoom::getCurrentRoomName() {
	return _currentRoomName;
}

int SputRoom::save(Common::WriteStream *stream) {
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
		warning("SputRoom::save(): failed to write the room count");
		return 1;
	}

	_res->allocatedIterFirst(&next, &handle, kSputTagSet);

	while (next) {
		if (_res->countDependents(handle)) {
			Room *room = static_cast<Room *>(handle);

			stream->writeSint32LE(_res->serialFromHandle(handle));
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
						error("SputRoom::save(): Bad Bitmap Reference");

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
		warning("SputRoom::save(): failed to write the room state");
		return 1;
	}

	return 0;
}

int SputRoom::restore(Common::ReadStream *stream) {
	int numRooms = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputRoom::restore(): failed to read the room count");
		return 1;
	}

	for (int i = 0; i < numRooms; i++) {
		int serial = stream->readSint32LE();

		if (stream->err() || stream->eos()) {
			warning("SputRoom::restore(): failed to read room %d of %d", i + 1, numRooms);
			return 1;
		}

		Room *room = static_cast<Room *>(_res->handleFromSerialNumber(serial));

		if (!room) {
			warning("SputRoom::restore(): room %d not found", serial);
			return 1;
		}

		room->curSetup = stream->readSint32LE();

		if (_currentRoomRef.valid)
			_res->removeReference(&_currentRoomRef);

		_res->setReference(&_currentRoomRef, room);

		if (!_currentRoomRef.valid)
			error("SputRoom::restore(): Bad room Reference");

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
					warning("SputRoom::restore(): failed to read object state %d of setup %d", j + 1, setupIndex);
					return 1;
				}

				if (bitmapName[31] || zbufferName[31]) {
					warning("SputRoom::restore(): object state name is not terminated");
					bitmapName[31] = 0;
					zbufferName[31] = 0;
				}

				addObjectState(setupIndex, objectType, bitmapName, zbufferName, copyMode);

				SputObjectState *head = setup->ObjectStates;

				if (!head) {
					warning("SputRoom::restore(): could not add object state %d of setup %d", j + 1, setupIndex);
					continue;
				}

				if (head->colorImage.bitmapRef.valid)
					static_cast<StdBitmapStruct *>(head->colorImage.bitmapRef.handle)->curFrame = colorFrame;

				if (head->zImage.bitmapRef.valid)
					static_cast<StdBitmapStruct *>(head->zImage.bitmapRef.handle)->curFrame = zFrame;
			}

			for (int k = 0; k < numStates; k++) {
				if (!setup->ObjectStates) {
					warning("SputRoom::restore(): setup %d has no object states to reorder", setupIndex);
					break;
				}

				setup->ObjectStates = setup->ObjectStates->moveToBack(setup->ObjectStates);
			}
		}
	}

	int currentSerial = stream->readSint32LE();
	Room *currentRoom = static_cast<Room *>(_res->handleFromSerialNumber(currentSerial));

	setCurrentRoom(currentRoom);

	if (currentRoom) {
		_grim->makeCurrentSetupLib(currentRoom->curSetup);

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
		warning("SputRoom::restore(): failed to read the current room state");
		return 1;
	}

	return 0;
}

int SputRoom::initialize(SysStruct *hs) {
	_hs = hs;
	memset(&_currentRoomRef, 0, sizeof(_currentRoomRef));
	_initialized = 1;

	return 0;
}

StdVBuffer *SputRoom::loadZBuffer(const char *fileName) {
	return loadZBufferEntry(fileName);
}

StdVBuffer *SputRoom::loadZBufferEntry(const char *fileName) {
	Common::String path(fileName);
	StdBitmapStruct *bitmap = _stdBitmap->loadLbm(path, 0, 0);

	if (!bitmap)
		return nullptr;

	if (!bitmap->frames || !bitmap->numFrames) {
		warning("SputRoom::loadZBufferEntry(): bitmap %s has no frames", fileName);
		_stdBitmap->unloadBitmap(bitmap);
		return nullptr;
	}

	StdVBuffer *frame = bitmap->frames[0];

	delete[] bitmap->frames;
	bitmap->frames = nullptr;
	bitmap->numFrames = 0;
	_stdBitmap->unloadBitmap(bitmap);

	return frame;
}

void SputRoom::freeZBuffer(StdVBuffer *&zBuffer) {
	freeZBufferEntry(zBuffer);
}

void SputRoom::freeZBufferEntry(StdVBuffer *&zBuffer) {
	_stdDisplay->vBufferFree(zBuffer);
}

Room *SputRoom::loadRoom(const char *fileName) {
	Room *room = new Room;

	memset(room, 0, sizeof(Room));
	newRoomEntry(room);

	if (loadRoomEntry(fileName, room)) {
		RoomActorNode *actorNode = findActorNode(fileName);

		if (!actorNode)
			actorNode = allocActorNode(fileName);

		room->actorNode = actorNode;

		for (RoomActorLink *link = actorNode->actors; link; link = link->next) {
			Extra *extra = _actors[link->actorIndex].extra;

			if (extra) {
				Puck *puck = extra->puck;

				if (puck) {
					if (extra->inBoxes)
						puck->validate();
				}
			}
		}

		return room;
	}

	freeRoom(room);

	return nullptr;
}

void SputRoom::newRoomEntry(Room *room) {
	memset(room, 0, sizeof(Room));
	room->numLights = 0;
	room->numSetups = 0;
	room->setups = nullptr;
	room->curSetup = 0;
	room->curColormap = 0;
	room->numcolormaps = 1;
	room->colormaps = nullptr;
	room->colormapName[0] = 0;
	Common::strlcpy(room->colormapName, "<none>", 32);
	Common::strlcpy(room->name, "", 32);
	room->structSize = (_grim->getGameFlags() & ADGF_DEMO) ? 6856 : 6920;
}

int SputRoom::loadRoomEntry(const char *fileName, Room *room) {
	_loadingRoom = room;
	Common::strlcpy(room->name, _stdLbm->stripPath(fileName), 128);

	return loadSections(fileName) != 0;
}

void SputRoom::unReference(Room *room) {
	if (!room->roomRef.valid) {
		if (room->colormaps) {
			for (int i = 0; i < room->numcolormaps; i++)
				_res->removeReference(&room->colormaps[i]);
		}

		for (int i = 0; i < (int)room->numSetups; i++) {
			_res->removeReference(&room->setups[i].bitmapRef);
			_res->removeReference(&room->setups[i].zbufferRef);

			while (room->setups[i].ObjectStates) {
				SputObjectState *objectState = room->setups[i].ObjectStates;

				room->setups[i].ObjectStates = objectState->next;
				objectState->unLoadResources();
				delete objectState;
			}
		}
	}
}

void SputRoom::freeRoom(Room *&room) {
	freeRoomEntry(room);
	delete room;
	room = nullptr;
}

void SputRoom::freeRoomEntry(Room *room) {
	unReference(room);

	if (room->colormaps) {
		delete[] room->colormaps;
		room->colormaps = nullptr;
	}

	freeRoomSetups(room);

	delete[] room->objects;
	room->objects = nullptr;
	room->numObjects = 0;

	_walkSystem->killCollection(room->name);

	RoomActorNode *actorNode = room->actorNode;

	if (actorNode) {
		for (RoomActorLink *link = actorNode->actors; link; link = actorNode->actors) {
			_sputActor->resetShadows(&_actors[link->actorIndex]);
			removeActor(room->name, &_actors[link->actorIndex]);

			if (actorNode->actors == link) {
				warning("SputRoom::freeRoomEntry(): could not remove the actor %d from room %s", link->actorIndex, room->name);
				break;
			}
		}

		freeActorNode(actorNode);
		room->actorNode = nullptr;
	}
}

void SputRoom::freeRoomSetups(Room *room) {
	if (room->numSetups) {
		for (uint i = 0; i < room->numSetups; i++)
			freeSetupEntry(room, &room->setups[i]);

		if (room->numSetups && room->setups)
			delete[] room->setups;
	}

	room->setups = nullptr;
	room->numSetups = 0;
}

void SputRoom::freeSetupEntry(Room *room, RoomSetup *setup) {
	if (setup->camera.prdCamera)
		_rd->_camera.destroy(setup->camera.prdCamera);

	if (setup->camera.thing)
		_rd->_thing.destroy(setup->camera.thing);

	delete[] setup->objectArt;
	setup->objectArt = nullptr;
	setup->ObjectStates = nullptr;
}

void SputRoom::freeSoundTable() {
	RoomSound *sound = _soundList;
	RoomSound *prev = nullptr;

	while (sound) {
		if (g_grim->getImuse()->soundsIsSoundPlaying(sound->soundName)) {
			prev = sound;
			sound = sound->next;
		} else {
			RoomSound *free = sound;

			sound = sound->next;

			if (free == _soundList) {
				_soundList = sound;
				prev = nullptr;
			} else {
				prev->next = sound;
			}

			delete free;
		}
	}
}

RoomSound *SputRoom::allocSoundTable(const char *soundName, float *pos, int minVol, int maxVol, float maxDist, float radius) {
	freeSoundTable();

	RoomSound *sound = new RoomSound;

	sound->soundName = soundName;
	sound->next = _soundList;
	sound->pos[0] = pos[0];
	sound->pos[1] = pos[1];
	sound->pos[2] = pos[2];
	sound->minVol = minVol;
	sound->maxVol = maxVol;
	sound->maxDist = maxDist;
	sound->radius = radius;
	_soundList = sound;

	return sound;
}

int SputRoom::setupSoundPosition(RoomSound *sound) {
	Room *room = getCurrentRoom();

	if (!room)
		return 0;

	RoomSetup *setup = &room->setups[room->curSetup];
	RdVector3 vec;

	vec[0] = (float)((double)sound->pos[0] - setup->camera.position[0]);
	vec[1] = (float)((double)sound->pos[1] - setup->camera.position[1]);
	vec[2] = (float)((double)sound->pos[2] - setup->camera.position[2]);

	double distance = (double)_rd->normalize3QuickAcc(vec) - sound->radius;

	if (distance >= 0.0) {
		uint minVol = (uint)sound->minVol;
		uint maxVol = (uint)sound->maxVol;
		double value = (double)(uint)(maxVol - minVol) / sound->maxDist * ((double)sound->maxDist - distance) + (double)minVol;
		uint volume = 0;

		if (value > -9.2e18 && value < 9.2e18)
			volume = (uint)(int64)value;
		else
			warning("SputRoom::setupSoundPosition(): volume out of range");

		if (volume < minVol)
			volume = minVol;
		else if (volume > maxVol)
			volume = maxVol;

		g_grim->getImuse()->soundsSetParam(sound->soundName, IMUSE_P_VOLUME, (int)volume);
	}

	double pan = ((double)setup->camera.posMatrix[0][0] * vec[0] + (double)setup->camera.posMatrix[0][1] * vec[1] +
			(double)setup->camera.posMatrix[0][2] * vec[2]) * room->panFactor;

	if (!(pan >= -1.0))
		pan = -1.0;
	else if (pan > 1.0)
		pan = 1.0;

	uint panValue = (uint)(int64)((pan + 1.0) * 64.0);

	if (panValue > 127)
		panValue = 127;

	g_grim->getImuse()->soundsSetParam(sound->soundName, IMUSE_P_PAN, (int)panValue);

	return 1;
}

int SputRoom::loadResources(Room *room) {
	room->structSize = (_grim->getGameFlags() & ADGF_DEMO) ? 6856 : 6920;

	g_grim->getImuse()->soundsFloodMusicBuffer();

	if (room->numcolormaps && !room->colormaps) {
		warning("SputRoom::loadResources(): room %s has no colormap references", room->name);
		return 0;
	}

	for (uint i = 0; i < (uint)room->numcolormaps; i++) {
		ResRef *colormapRef = &room->colormaps[i];

		if (colormapRef->valid) {
			_res->touch(colormapRef->handle);
		} else {
			if (scumm_stricmp(room->colormapName, "<none>")) {
				void *handle = _sputResource->alloc(kSputTagCmap, room->colormapName);

				_res->setReference(colormapRef, handle);
			}

			if (!colormapRef->valid) {
				void *handle = _sputResource->alloc(kSputTagCmap, "dfltcmp.cmp");

				_res->setReference(colormapRef, handle);

				if (!colormapRef->valid)
					return 0;
			}
		}
	}

	for (uint i = 0; i < room->numSetups; i++) {
		RoomSetup *setup = &room->setups[i];

		if (!_updateLoop) {
			ResRef *bitmapRef = &setup->bitmapRef;

			if (bitmapRef->valid) {
				_res->touch(bitmapRef->handle);
			} else {
				if (scumm_stricmp(setup->bitmapName, "<none>")) {
					void *handle = _sputResource->alloc(kSputTagBknd, setup->bitmapName);

					_res->setReference(bitmapRef, handle);
				}

				if (!bitmapRef->valid) {
					void *handle = _sputResource->alloc(kSputTagBknd, "dfltroom.bm");

					_res->setReference(bitmapRef, handle);

					if (!bitmapRef->valid)
						return 0;

					Common::strlcpy(setup->bitmapName, "<none>", 32);
					setup->zBufferName[0] = 0;
				}
			}

			if (setup->zBufferName[0]) {
				ResRef *zBufferRef = &setup->zbufferRef;

				if (!zBufferRef->valid) {
					void *handle = _sputResource->alloc(kSputTagZbuf, setup->zBufferName);

					if (handle)
						_res->setReference(zBufferRef, handle);
				}

				if (zBufferRef->valid)
					_res->touch(zBufferRef->handle);
			}
		}

		if (_grim->getGameFlags() & ADGF_DEMO) {
			if (!setup->ObjectStates) {
				createObjectStates(room, setup, i);
			} else {
				for (SputObjectState *state = setup->ObjectStates; state; state = state->next)
					state->loadResources();
			}

			for (SputObjectState *state = setup->ObjectStates; state; state = state->next) {
				if (state->colorImage.bitmapRef.valid)
					room->structSize += _stdBitmap->memUsage(static_cast<StdBitmapStruct *>(state->colorImage.bitmapRef.handle));

				if (state->zImage.bitmapRef.valid)
					room->structSize += _stdBitmap->memUsage(static_cast<StdBitmapStruct *>(state->zImage.bitmapRef.handle));
			}
		}

		if (!setup->camera.prdCamera) {
			setup->camera.prdCamera = _rd->_camera.create(setup->camera.fov, 1, setup->camera.nearClip, setup->camera.farClip, 1.0f);

			if (!setup->camera.prdCamera)
				return 0;

			setup->camera.thing = _rd->_thing.create(0);
			_rd->_thing.setCamera(setup->camera.thing, setup->camera.prdCamera);
			_rd->lookAt(setup->camera.posMatrix, setup->camera.position, setup->camera.interest, setup->camera.roll);

			for (uint j = 0; j < room->numLights; j++) {
				RoomLight *light = &room->lights[j];

				_rd->_light.setAngles(&light->light, light->light.angleX, light->light.angleY);
				_rd->_camera.addLight(setup->camera.prdCamera, &light->light, &light->position);
			}
		}
	}

	return 1;
}

int SputRoom::terminate() {
	_hs = nullptr;

	if (_initialized)
		_initialized = 0;

	return 0;
}

int SputRoom::startup() {
	if (_started)
		return 1;

	return readSections();
}

int SputRoom::shutdown() {
	RoomActorNode *node = _actorNodeList;

	while (node) {
		RoomActorLink *link = node->actors;

		while (link) {
			RoomActorLink *nextLink = link->next;

			delete link;
			link = nextLink;
		}

		RoomActorNode *nextNode = node->next;

		delete node;
		node = nextNode;
	}

	_actorNodeList = nullptr;

	while (_soundList) {
		RoomSound *nextSound = _soundList->next;

		delete _soundList;
		_soundList = nextSound;
	}

	// The room can be gone already: the resources that were freed have invalidated the reference
	if (_grim->isRemastered() && _currentRoomRef.valid)
		_res->removeReference(&_currentRoomRef);

	_started = 0;

	return 0;
}

RoomLight *SputRoom::getLight(Room *room, int index) {
	Room *currentRoom = room;

	if (!room)
		currentRoom = getCurrentRoom();

	if (!currentRoom) {
		warning("SputRoom::getLight(): no current room");
		return nullptr;
	}

	if (index < 0 || index >= (int)currentRoom->numLights)
		return nullptr;

	return &currentRoom->lights[index];
}

int SputRoom::findLightByName(Room *room, const char *name) {
	Room *currentRoom = room;

	if (!room)
		currentRoom = getCurrentRoom();

	if (!currentRoom) {
		warning("SputRoom::findLightByName(): no current room");
		return -1;
	}

	int index = 0;

	if ((int)currentRoom->numLights > 0) {
		do {
			if (!scumm_stricmp(name, currentRoom->lights[index].name))
				break;

			index++;
		} while (index < (int)currentRoom->numLights);
	}

	if (index >= (int)currentRoom->numLights)
		return -1;

	return index;
}

SputObjectState *SputRoom::addObjectState(int setupIndex, int objectType, const char *bitmapName, const char *zbufferName, int copyMode) {
	Room *room = getCurrentRoom();

	if (!room || setupIndex < 0 || (uint)setupIndex >= room->numSetups) {
		warning("SputRoom::addObjectState(): no room or invalid setup %d", setupIndex);
		return nullptr;
	}

	RoomSetup *setup = &room->setups[setupIndex];

	setup->ObjectStates = new SputObjectState(setup->ObjectStates, bitmapName, zbufferName, objectType, _res, _sputResource,
			_stdDisplay, _stdBitmap, _sputRender, (_grim->getGameFlags() & ADGF_DEMO) != 0);
	setup->ObjectStates->loadResources();
	setup->ObjectStates->setupIndex = setupIndex;

	if (copyMode <= -1)
		setup->ObjectStates->setCopyMode(objectType == 3);
	else
		setup->ObjectStates->setCopyMode(copyMode);

	return setup->ObjectStates;
}

void SputRoom::createObjectStates(Room *room, RoomSetup *setup, int setupIndex) {
	if (!setup->objectArt)
		return;

	// The states are added at the head of the list, the first object has to end up there
	for (int i = (int)room->numObjects - 1; i >= 0; i--) {
		RoomObjectArt *art = &setup->objectArt[i];

		if (!art->bitmapName[0])
			continue;

		SputObjectState *state = new SputObjectState(setup->ObjectStates, art->bitmapName, art->zBufferName, 3, _res, _sputResource,
				_stdDisplay, _stdBitmap, _sputRender, true);

		setup->ObjectStates = state;
		state->setupIndex = setupIndex;
		state->setCopyMode(1);
		state->loadResources();
	}
}

void SputRoom::removeObjectState(SputObjectState *obj) {
	if (obj) {
		Room *room = getCurrentRoom();

		if (!room) {
			warning("SputRoom::removeObjectState(): no current room");
			return;
		}

		RoomSetup *setup = &room->setups[obj->setupIndex];
		SputObjectState *head = setup->ObjectStates;
		SputObjectState *next;

		if (head == obj)
			next = obj->next;
		else
			next = head;

		delete obj;
		setup->ObjectStates = next;
	}
}

void SputRoom::removeActor(const char *roomName, SputActor *actor) {
	RoomActorNode *actorNode = findActorNode(roomName);

	if (actorNode) {
		actor->onScreen = 0;

		RoomActorLink *link = actorNode->actors;

		while (link) {
			if (&_actors[link->actorIndex] == actor)
				break;

			link = link->next;
		}

		if (link) {
			Extra *extra = actor->extra;

			if (extra) {
				Puck *puck = extra->puck;

				if (puck)
					puck->_plane = nullptr;
			}

			if (link->next)
				link->next->prev = link->prev;

			if (link->prev)
				link->prev->next = link->next;

			if (actorNode->actors == link)
				actorNode->actors = link->next;

			delete link;
			actor->curSetName[0] = 0;
		}
	}
}

void SputRoom::addActor(const char *roomName, SputActor *actor) {
	if (!actor || !_actors) {
		warning("SputRoom::addActor(): no actor or no actor array");
		return;
	}

	if (roomName && !strcmp(roomName, actor->curSetName))
		return;

	removeActor(actor->curSetName, actor);

	if (roomName) {
		RoomActorNode *actorNode = findActorNode(roomName);

		if (!actorNode)
			actorNode = allocActorNode(roomName);

		RoomActorLink *link = actorNode->actors;

		while (link) {
			if (&_actors[link->actorIndex] == actor)
				return;

			link = link->next;
		}

		link = new RoomActorLink;
		link->actorIndex = 0;
		link->prev = nullptr;
		link->next = nullptr;
		link->actorIndex = (int)(actor - _actors);
		link->next = actorNode->actors;

		if (actorNode->actors)
			actorNode->actors->prev = link;

		actorNode->actors = link;

		if (strlen(roomName) >= 32)
			warning("SputRoom::addActor(): room name %s is too long", roomName);

		Common::strlcpy(actor->curSetName, roomName, 32);
	} else {
		actor->onScreen = 0;
	}
}

RoomActorLink *SputRoom::getRoomActors(const char *roomName) {
	RoomActorNode *actorNode = findActorNode(roomName);

	if (actorNode)
		return actorNode->actors;

	return nullptr;
}

RoomActorNode *SputRoom::findActorNode(const char *roomName) {
	RoomActorNode *node = _actorNodeList;

	if (!roomName || !node)
		return nullptr;

	while (scumm_stricmp(node->name, roomName)) {
		node = node->next;

		if (!node)
			return nullptr;
	}

	return node;
}

RoomActorNode *SputRoom::allocActorNode(const char *roomName) {
	RoomActorNode *node = new RoomActorNode;

	memset(node, 0, sizeof(RoomActorNode));

	if (strlen(roomName) >= 128)
		warning("SputRoom::allocActorNode(): room name %s is too long", roomName);

	Common::strlcpy(node->name, roomName, 128);
	node->next = _actorNodeList;
	_actorNodeList = node;

	return node;
}

void SputRoom::freeActorNode(RoomActorNode *node) {
	RoomActorNode *current = _actorNodeList;
	RoomActorNode *prev = nullptr;

	if (current) {
		do {
			if (current == node)
				break;

			prev = current;
			current = current->next;
		} while (current);

		if (current) {
			if (prev)
				prev->next = current->next;
			else
				_actorNodeList = current->next;

			delete current;
		}
	}
}

} // End of namespace Grim
