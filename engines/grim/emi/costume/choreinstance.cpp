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

#include "common/textconsole.h"

#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/costume/component.h"
#include "engines/grim/emi/costume/componentinstance.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lmemoryservices.h"

namespace Grim {

ChoreInstance::ChoreInstance(EmiEngine *engine, Actor *actor, uint instanceId) : LResourceHandle<ChoreSet>(engine) {
	_id = instanceId;
	_engine->_choreInstances[_id] = this;
	_owner = actor;
	_flags = 0;
	_time = 0.0f;
	_stopTime = 0.0f;

	_resource = getManager()->findResource("");

	if (!_resource) {
		_resource = getManager()->createResource("");

		if (!_resource)
			allocateResource();

		_resource->resSetName("");
		getManager()->addResource(_resource);
	}

	setResource(_resource);
}

ChoreInstance::ChoreInstance(EmiEngine *engine, Actor *actor, const LString &newChoreName, const LString &choreSetName) : LResourceHandle<ChoreSet>(engine) {
	_id = _engine->_nextChoreInstanceId++;
	_engine->_choreInstances[_id] = this;
	_flags = 0;
	_owner = actor;
	_time = 0.0f;
	_stopTime = 0.0f;

	bind(newChoreName, choreSetName);
}

ChoreInstance::~ChoreInstance() {
	for (uint i = 0; i < _instances.size(); i++) {
		delete _instances[i];
		_instances[i] = nullptr;
	}

	_instances.clear();
	_engine->_choreInstances.erase(_id);
}

void *ChoreInstance::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void ChoreInstance::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

void ChoreInstance::bind(const LString &newChoreName, const LString &choreSetName) {
	_choreName = newChoreName;

	{
		LResourceHandle<ChoreSet> choreSetHandle(_engine, choreSetName);

		setResource(choreSetHandle._resource);
	}

	Chore *chore = nullptr;
	bool hasChoreSet = lock() && _typed;

	if (hasChoreSet)
		chore = _typed->findChore(_choreName);

	if (_engine->isDemo()) {
		if (!hasChoreSet)
			warning("ChoreInstance::bind(): no chore set named '%s' was found, the chore will be ignored", choreSetName.c_str());
		else if (!chore)
			warning("ChoreInstance::bind(): no chore named '%s' was found, the chore will be ignored", _choreName.c_str());
	}

	if (chore) {
		for (uint i = 0; i < chore->_tracks.size(); i++) {
			ChoreTrack *track = chore->_tracks[i];

			if (!track) {
				warning("ChoreInstance::bind(): missing track %u", i);
				_instances.push_back(nullptr);
				continue;
			}

			LResourceHandle<Component> componentHandle(_engine, track->_resourceName);

			if (componentHandle.lock() && componentHandle._typed)
				_instances.push_back(componentHandle._typed->createInstance(_owner));
			else {
				if (_engine->isDemo())
					warning("ChoreInstance::bind(): no component named '%s' was found, that track will be ignored", track->_resourceName.c_str());

				_instances.push_back(nullptr);
			}

			componentHandle.unlock();
		}

		for (uint i = 0; i < chore->_tracks.size(); i++) {
			ChoreTrack *track = chore->_tracks[i];

			if (!track || track->_parentId < 0)
				continue;

			int parentIndex = chore->findTrackIndex(track->_parentId);

			if (parentIndex < 0) {
				if (_engine->isDemo())
					warning("ChoreInstance::bind(): the parent of component '%s' cannot be found, the child remains an orphan", track->_resourceName.c_str());

				continue;
			}

			if (i >= _instances.size() || (uint)parentIndex >= _instances.size() || !_instances[i]) {
				warning("ChoreInstance::bind(): missing instance for track %u", i);
				continue;
			}

			if (!_instances[i]->setParent(_instances[parentIndex]) && _engine->isDemo())
				warning("ChoreInstance::bind(): component '%s' failed to attach itself to its parent", track->_resourceName.c_str());
		}
	} else {
		_flags = kFinished;
	}

	unlock();
}

bool ChoreInstance::isPlaying() {
	return (_flags & kPlaying) != 0;
}

bool ChoreInstance::isFinished() {
	return (_flags & kFinished) != 0;
}

bool ChoreInstance::isLooping() {
	return (_flags & kLooping) != 0;
}

bool ChoreInstance::setPlaying() {
	_flags |= kPlaying;

	return true;
}

bool ChoreInstance::fadeIn(float seconds) {
	if (isPlaying())
		return false;

	if (!(seconds > 0.0f))
		return false;

	for (uint i = 0; i < _instances.size(); i++) {
		if (_instances[i])
			_instances[i]->fadeIn(seconds);
	}

	setPlaying();

	return true;
}

bool ChoreInstance::fadeOut(float seconds) {
	if (!isPlaying())
		return false;

	if (!(seconds > 0.0f))
		return false;

	ChoreSet *choreSet = nullptr;

	if (lock())
		choreSet = _typed;

	Chore *chore = nullptr;

	if (choreSet)
		chore = choreSet->findChore(_choreName);
	else
		warning("ChoreInstance::fadeOut(): no chore set");

	if (chore && !(chore->_length > seconds)) {
		stop();
		unlock();

		return true;
	}

	unlock();

	for (uint i = 0; i < _instances.size(); i++) {
		if (_instances[i])
			_instances[i]->fadeOut(seconds);
	}

	_stopTime = (float)((double)seconds + (double)_time);

	return true;
}

bool ChoreInstance::stop() {
	for (uint i = 0; i < _instances.size(); i++) {
		if (_instances[i])
			_instances[i]->reset();
	}

	_stopTime = -1.0f;
	_flags = kFinished;

	return true;
}

bool ChoreInstance::pause() {
	_flags &= ~kPlaying;

	return true;
}

bool ChoreInstance::complete() {
	_flags = kPlaying;

	ChoreSet *choreSet = nullptr;

	if (lock())
		choreSet = _typed;

	Chore *chore = nullptr;

	if (choreSet)
		chore = choreSet->findChore(_choreName);
	else
		warning("ChoreInstance::complete(): no chore set");

	if (chore)
		update((float)((double)chore->_length - (double)_time), false);

	unlock();

	return true;
}

bool ChoreInstance::update(float deltaTime, bool replay) {
	bool stopRequested = false;

	if (!(deltaTime >= 0.0f))
		return false;

	if (!isPlaying())
		return true;

	if (!lock())
		return false;

	if (!_typed) {
		if (_engine->isDemo())
			warning("ChoreInstance::update(): the chore set handle points to nothing");

		return false;
	}

	Chore *chore = _typed->findChore(_choreName);

	if (!chore) {
		if (_engine->isDemo())
			warning("ChoreInstance::update(): could not find a chore named '%s'", _choreName.c_str());

		return false;
	}

	float fullDeltaTime = deltaTime;

	if (_stopTime > 0.0f && (double)deltaTime + (double)_time > (double)_stopTime) {
		_stopTime = 0.0f;
		stopRequested = true;
	}

	while (true) {
		for (uint i = 0; i < chore->_tracks.size(); i++) {
			ChoreTrack *track = chore->_tracks[i];

			if (!track) {
				warning("ChoreInstance::update(): missing track %u", i);
				continue;
			}

			if (track->_keys.empty())
				continue;

			if (i >= _instances.size()) {
				warning("ChoreInstance::update(): missing instance for track %u", i);
				continue;
			}

			ComponentInstance *instance = _instances[i];

			if (!instance)
				continue;

			if (replay && !instance->isReplayable())
				continue;

			Common::List<ChoreKey *>::iterator current = track->_keys.begin();

			while (current != track->_keys.end() && !((*current)->time >= _time))
				++current;

			Common::List<ChoreKey *>::iterator next = current;

			if (next != track->_keys.end())
				++next;

			while (current != track->_keys.end()) {
				ChoreKey *key = *current;
				float limitTime = (float)((double)deltaTime + (double)_time);

				if ((double)key->time > (double)limitTime)
					break;

				if (next == track->_keys.end()) {
					if (isLooping()) {
						ChoreKey *firstKey = *track->_keys.begin();
						double loopEnd = (double)chore->_length + (double)firstKey->time;

						if (loopEnd > (double)limitTime)
							instance->setKey(key->value, (float)((double)limitTime - (double)key->time), firstKey->value, (float)(loopEnd - (double)limitTime), true);
						else
							instance->setKey(key->value, 0.0f, firstKey->value, (float)(loopEnd - (double)key->time), false);
					} else {
						double fullTime = (double)deltaTime + (double)_time;

						instance->setKey(key->value, (float)(fullTime - (double)key->time), key->value, (float)((double)key->time - fullTime), true);
					}
				} else {
					ChoreKey *nextKey = *next;

					if ((double)nextKey->time > (double)limitTime)
						instance->setKey(key->value, (float)((double)limitTime - (double)key->time), nextKey->value, (float)((double)nextKey->time - (double)limitTime), true);
					else
						instance->setKey(key->value, 0.0f, nextKey->value, (float)((double)nextKey->time - (double)key->time), false);
				}

				++current;
				++next;
			}
		}

		double newTime = (double)deltaTime + (double)_time;

		_time = (float)newTime;

		if (!(newTime >= (double)chore->_length))
			break;

		if (!isLooping()) {
			_flags = kFinished;
			break;
		}

		if (!(chore->_length > 0.0f)) {
			warning("ChoreInstance::update(): looping chore with no length");
			break;
		}

		deltaTime = (float)((double)_time - (double)chore->_length);
		_time = 0.0f;

		if (!(chore->_length >= _stopTime))
			_stopTime = (float)((double)_stopTime - (double)chore->_length);
	}

	for (uint i = 0; i < chore->_tracks.size(); i++) {
		if (i >= _instances.size()) {
			warning("ChoreInstance::update(): missing instance for track %u", i);
			continue;
		}

		if (_instances[i])
			_instances[i]->update(fullDeltaTime);
	}

	unlock();

	if (stopRequested)
		stop();

	return true;
}

bool ChoreInstance::setLooping(bool looping) {
	if (looping)
		_flags |= kLooping;
	else
		_flags &= ~kLooping;

	return true;
}

bool ChoreInstance::save(SaveRestoreAccess *accessFunc) {
	LString choreSetName;

	if (_resource)
		choreSetName = _resource->_name;
	else
		warning("ChoreInstance::save(): no chore set");

	int length = (int)choreSetName.size() + 1;

	accessFunc(&length, sizeof(length));
	accessFunc((void *)choreSetName.c_str(), length);

	length = (int)_choreName.size() + 1;

	accessFunc(&length, sizeof(length));
	accessFunc((void *)_choreName.c_str(), length);
	accessFunc(&_time, sizeof(_time));
	accessFunc(&_stopTime, sizeof(_stopTime));
	accessFunc(&_flags, sizeof(_flags));

	return true;
}

bool ChoreInstance::restore(SaveRestoreAccess *accessFunc) {
	char buffer[kRestoreBufferSize + 1];
	int length;
	LString choreSetName;
	LString restoredChoreName;
	float savedTime;
	uint savedFlags;

	accessFunc(&length, sizeof(length));

	if (length < 1 || length > kRestoreBufferSize) {
		warning("ChoreInstance::restore(): invalid chore set name length %d", length);

		return false;
	}

	accessFunc(buffer, length);
	buffer[length] = 0;
	choreSetName = buffer;

	accessFunc(&length, sizeof(length));

	if (length < 1 || length > kRestoreBufferSize) {
		warning("ChoreInstance::restore(): invalid chore name length %d", length);

		return false;
	}

	accessFunc(buffer, length);
	buffer[length] = 0;
	restoredChoreName = buffer;

	bind(restoredChoreName, choreSetName);

	accessFunc(&savedTime, sizeof(savedTime));
	accessFunc(&_stopTime, sizeof(_stopTime));
	accessFunc(&savedFlags, sizeof(savedFlags));

	_flags = kPlaying;

	bool result = update(savedTime, true);

	_flags = savedFlags;

	return result;
}

uint ChoreInstance::newInstanceWithId(EmiEngine *engine, Actor *actor, uint instanceId) {
	ChoreInstance *instance = new ChoreInstance(engine, actor, instanceId);

	return instance->_id;
}

uint ChoreInstance::newInstance(EmiEngine *engine, Actor *actor, const LString &newChoreName, const LString &choreSetName) {
	if (newChoreName.empty() || choreSetName.empty())
		return 0;

	ChoreInstance *instance = new ChoreInstance(engine, actor, newChoreName, choreSetName);

	return instance->_id;
}

ChoreInstance *ChoreInstance::getInstance(EmiEngine *engine, uint instanceId) {
	InstanceMap::iterator it = engine->_choreInstances.find(instanceId);

	if (it == engine->_choreInstances.end())
		return nullptr;

	return it->_value;
}

bool ChoreInstance::isInstanceValid(EmiEngine *engine, uint instanceId) {
	if (instanceId == 0)
		return false;

	InstanceMap::iterator it = engine->_choreInstances.find(instanceId);

	if (it == engine->_choreInstances.end())
		return false;

	if (it->_value)
		return true;

	engine->_choreInstances.erase(instanceId);

	return false;
}

bool ChoreInstance::startup(EmiEngine *engine) {
	if (engine->_choreInstancesStarted)
		return false;

	engine->_choreInstancesStarted = true;

	return true;
}

void ChoreInstance::shutdown(EmiEngine *engine) {
	engine->_choreInstancesStarted = false;
}

} // End of namespace Grim
