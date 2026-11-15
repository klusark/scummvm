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
#include "common/textconsole.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/actor/actor_ps2.h"
#include "engines/grim/emi/animation/anode.h"
#include "engines/grim/emi/graphics/cdcvertexarray.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/sound/lipdata.h"
#include "engines/grim/emi/resources/lresource.h"
#include "engines/grim/emi/scene/overworld.h"
#include "engines/grim/emi/actor/puck_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

const float Actor::kDegreesToRadians = 3.14159274f * 0.00555555569f;

Actor::Actor(EmiEngine *engine, const char *newActorName) : _room(engine), _engine(engine) {
	_prevPos._x = 0.0f;
	_prevPos._y = 0.0f;
	_prevPos._z = 0.0f;
	_drawnRect[0] = 0;
	_drawnRect[1] = 0;
	_drawnRect[2] = 0;
	_drawnRect[3] = 0;
	memset(_choreSlotIds, 0, kNumChoreSlots * sizeof(uint));

	_id = _engine->_nextActorId++;
	_engine->_actors.push_back(this);

	if (newActorName)
		_actorName = newActorName;
	else
		warning("Actor::Actor(): the actor name is null");

	_actorFlags = 0;
	_mode = 0;
	_reserved312 = 0.0f;
	_onScreen = false;
	_inOverworld = false;
	_room.setResource(nullptr);
	_sortOrder = 0;
	_parentActorId = 0;
	memset(_parentNodeName, 0, sizeof(_parentNodeName));
	_drawFlags |= 0x21000;

	for (int i = 0; i < kNumChoreSlots; i++) {
		_choreSlotIds[i] = 0;
		_choreSlotNames[i].clear();
		_choreSlotSetNames[i].clear();
	}

	_extra = nullptr;
	Common::sprintf_s(_name, "Actor %d", _id);
}

Actor::~Actor() {
	if (_room._resource) {
		Room_v2 *currentRoom = nullptr;

		if (_room.lock())
			currentRoom = _room._typed;

		if (currentRoom)
			currentRoom->removeActor(this);
		else
			warning("Actor::~Actor(): the room could not be locked");

		_room.unlock();
		_room.unlock();
	}

	Common::List<uint>::iterator choreIt;

	for (choreIt = _choreIds.begin(); choreIt != _choreIds.end(); ++choreIt) {
		if (ChoreInstance::isInstanceValid(_engine, *choreIt)) {
			ChoreInstance *instance = ChoreInstance::getInstance(_engine, *choreIt);

			if (instance)
				delete instance;
		}
	}

	_choreIds.clear();

	if (_extra) {
		if (_extra->_puck) {
			delete _extra->_puck;
			_extra->_puck = nullptr;
		}

		delete _extra;
		_extra = nullptr;
	}

	Common::List<Actor *>::iterator actorIt = _engine->_actors.begin();

	while (actorIt != _engine->_actors.end()) {
		if (*actorIt == this)
			actorIt = _engine->_actors.erase(actorIt);
		else
			++actorIt;
	}

	// The disasm leaves the actor in the overworld list and in the selected/speaker slots
}

Actor *Actor::createActor(EmiEngine *engine, const char *newActorName) {
	if (engine->getGamePlatform() == Common::kPlatformPS2)
		return new ActorPs2(engine, newActorName);

	return new Actor(engine, newActorName);
}

Puck_v2 *Actor::createPuck(float newReflectionAngle) {
	return new Puck_v2(_engine, newReflectionAngle);
}

int Actor::allocateActor(EmiEngine *engine, const char *newActorName) {
	Actor *actor = createActor(engine, newActorName);

	return actor->_id;
}

void Actor::freeActor(EmiEngine *engine, int actorId) {
	Actor *actor = actorDeref(engine, actorId);

	if (actor)
		delete actor;
}

void Actor::allocateActorExtra() {
	_extra = new Extra_v2(_engine);
}

bool Actor::putInSet(const char *roomName) {
	if (_room._resource) {
		activateShadow(false, nullptr);

		for (int removedBefore = 0;; removedBefore++) {
			Room_v2 *currentRoom = nullptr;

			if (_room.lock())
				currentRoom = _room._typed;

			int removed = 0;

			if (currentRoom)
				removed = currentRoom->removeActor(this);
			else
				warning("Actor::putInSet(): the room could not be locked");

			bool removedAgain = removedBefore < removed;

			if (currentRoom)
				_room.unlock();

			if (!removedAgain)
				break;

			_room.unlock();
		}

		_room.setResource(nullptr);
	}

	if (!roomName) {
		removeFinishedChores();

		return true;
	}

	_room._resource = _engine->getResourceManager()->findResource(roomName);

	if (!_room._resource) {
		_room._resource = _engine->getResourceManager()->createResource(roomName);

		if (!_room._resource)
			_room.allocateResource();

		if (!_room._resource) {
			warning("Actor::putInSet(): could not create the room resource '%s'", roomName);

			return false;
		}

		_room._resource->resSetName(roomName);
		_engine->getResourceManager()->addResource(_room._resource);
	}

	_room.setResource(_room._resource);

	if (!_room._resource)
		return false;

	if (!_room.lock())
		return false;

	Room_v2 *newRoom = _room._typed;

	if (!newRoom)
		return false;

	newRoom->addActor(this);

	if (_extra && _extra->_puck && _extra->_inBoxes) {
		_extra->_puck->reset();
		_extra->_puck->validate(newRoom->_walkPlanes);
	}

	return true;
}

bool Actor::startup(EmiEngine *engine, CDrawContext *drawContext) {
	if (drawContext)
		engine->_shadowVertices = drawContext->createVertexArray(2000, nullptr, nullptr, nullptr);

	for (int i = 0; i < kNumSpeakers; i++)
		engine->_speakers[i] = nullptr;

	LipData::sortMouthTable(engine);

	return true;
}

void Actor::shutdown(EmiEngine *engine) {
	// The disasm leaves dangling actor pointers at shutdown, let's clear them first
	engine->getOverworld()->clearActors();

	engine->_selectedActor = nullptr;

	for (int i = 0; i < kNumSpeakers; i++)
		engine->_speakers[i] = nullptr;

	engine->_numSpeakers = 0;

	freeAllActors(engine);

	// The vertex array only exists when startup() got a draw context
	if (engine->_shadowVertices)
		delete engine->_shadowVertices;

	engine->_shadowVertices = nullptr;

	engine->destroyOverworld();
}

void Actor::freeAllActors(EmiEngine *engine) {
	Common::List<Actor *>::iterator it;

	for (it = engine->_actors.begin(); it != engine->_actors.end(); ++it)
		(*it)->detachFromParent();

	it = engine->_actors.begin();

	while (it != engine->_actors.end()) {
		Actor *actor = *it;

		++it;

		if (actor)
			delete actor;
	}

	engine->_actors.clear();
}

int Actor::setSelectedActor(EmiEngine *engine, int actorId) {
	int previousId = -1;

	if (engine->_selectedActor)
		previousId = engine->_selectedActor->_id;

	if (actorId >= 0)
		engine->_selectedActor = actorDeref(engine, actorId);

	return previousId;
}

Actor *Actor::getSelectedActor(EmiEngine *engine) {
	return engine->_selectedActor;
}

Actor *Actor::actorDeref(EmiEngine *engine, int actorId) {
	Common::List<Actor *>::iterator it;

	for (it = engine->_actors.begin(); it != engine->_actors.end(); ++it) {
		if (*it && (*it)->_id == actorId)
			return *it;
	}

	return nullptr;
}

int Actor::getActorHandle(EmiEngine *, Actor *actor) {
	if (actor)
		return actor->_id;

	return 0;
}

void Actor::attachNode(CNode *node) {
	if (node) {
		registerANodes(node);
		node->setParent(this);
	}
}

void Actor::detachNode(CNode *node) {
	if (node) {
		unregisterANodes(node);
		node->detachFromParent();
	}
}

void Actor::registerANodes(CNode *node) {
	for (CNode *current = node; current; current = current->_nextSibling) {
		registerANodes(current->_firstChild);

		ANode *animNode = dynamic_cast<ANode *>(current);

		if (animNode)
			_animNodes.push_back(animNode);
	}
}

void Actor::unregisterANodes(CNode *node) {
	for (CNode *current = node; current; current = current->_nextSibling) {
		unregisterANodes(current->_firstChild);

		ANode *animNode = dynamic_cast<ANode *>(current);

		if (animNode) {
			Common::List<ANode *>::iterator it = _animNodes.begin();

			while (it != _animNodes.end()) {
				if (*it == animNode)
					it = _animNodes.erase(it);
				else
					++it;
			}
		}
	}
}

} // End of namespace Grim
