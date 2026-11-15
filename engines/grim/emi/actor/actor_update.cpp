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

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/animation/anode.h"
#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/scene/overworld.h"

namespace Grim {

void Actor::updateAllActors(EmiEngine *engine, float deltaTime) {
	engine->_updateAllActorsTimer.start();

	engine->_collisionCount = 0;

	Room_v2 *currentRoom = Room_v2::getCurrentRoom(engine);

	if (!currentRoom) {
		engine->_updateAllActorsTimer.stop();

		return;
	}

	updateSpeechOnly(engine, deltaTime);

	Common::List<Actor *>::iterator it;

	for (it = currentRoom->_actors.begin(); it != currentRoom->_actors.end(); ++it) {
		Actor *current = *it;

		if (!current)
			continue;

		if (current->_extra)
			current->_extra->_inCollision = false;

		if (!current->_instances.empty()) {
			if (engine->_collisionCount < kMaxCollisionActors) {
				engine->_collisionActors[engine->_collisionCount] = current;
				engine->_collisionCount++;
			} else {
				warning("Actor::updateAllActors(): too many actors in the collision list");
			}
		}

		if (!engine->isDemo()) {
			Camera *setupCamera = currentRoom->getSetupCamera(-1);

			if (setupCamera)
				current->_onScreen = current->calcOnScreen(setupCamera);
		}

		current->update(deltaTime);
	}

	Overworld *overworld = engine->getOverworld();

	for (it = overworld->_actors.begin(); it != overworld->_actors.end(); ++it) {
		Actor *current = *it;

		if (!current)
			continue;

		if (!current->_instances.empty()) {
			if (engine->_collisionCount < kMaxCollisionActors) {
				engine->_collisionActors[engine->_collisionCount] = current;
				engine->_collisionCount++;
			} else {
				warning("Actor::updateAllActors(): too many actors in the collision list");
			}
		}

		if (!engine->isDemo())
			current->_onScreen = true;

		current->update(deltaTime);
	}

	updatePositions(engine);
	engine->_updateAllActorsTimer.stop();
}

void Actor::updateSpeechOnly(EmiEngine *engine, float deltaTime) {
	for (int i = 0; i < kNumSpeakers; i++) {
		if (engine->_speakers[i])
			engine->_speakers[i]->updateActorSpeech(deltaTime);
	}
}

void Actor::updateMovement(float frameDelta, float scaledDelta) {
	if ((_mode & 1) != 0)
		followPath(scaledDelta);

	if (_extra && _extra->_puck)
		updateWalk(scaledDelta);
}

void Actor::update(float deltaTime) {
	float frameDelta = deltaTime;

	if (_extra)
		deltaTime = deltaTime * _extra->_timeScale;

	updateMovement(frameDelta, deltaTime);

	removeFinishedChores();
	updateWalkChores();

	Common::List<uint>::iterator choreIt;

	for (choreIt = _choreIds.begin(); choreIt != _choreIds.end(); ++choreIt) {
		if (ChoreInstance::isInstanceValid(_engine, *choreIt)) {
			ChoreInstance *instance = ChoreInstance::getInstance(_engine, *choreIt);

			instance->update(deltaTime, false);
		}
	}

	if (_engine->isDemo()) {
		// The demo updates the animation nodes of the other actors when it draws them
		if (_extra && _extra->_lookAtActive) {
			updateAnimNodes();
			setLookAtJoints(deltaTime);
		}
	} else if (_onScreen) {
		updateAnimNodes();

		if (_extra && _extra->_lookAtActive)
			setLookAtJoints(deltaTime);
	}
}

void Actor::updateAnimNodes() {
	Common::List<ANode *>::iterator nodeIt;

	for (nodeIt = _animNodes.begin(); nodeIt != _animNodes.end(); ++nodeIt)
		(*nodeIt)->updateANode();
}

bool Actor::updatePositions(EmiEngine *engine) {
	for (int i = 0; i < engine->_collisionCount; i++) {
		Actor *current = engine->_collisionActors[i];

		if (current->_extra) {
			if ((current->_nodeFlags & kWorldValid) == 0)
				current->updateWorldTransform();

			// The disasm treats an unordered comparison as equal
			bool moved = (current->_world._pos._x < current->_prevPos._x || current->_world._pos._x > current->_prevPos._x)
				|| (current->_world._pos._y < current->_prevPos._y || current->_world._pos._y > current->_prevPos._y)
				|| (current->_world._pos._z < current->_prevPos._z || current->_world._pos._z > current->_prevPos._z);

			if (moved)
				current->resolveCollisions();
		}
	}

	return true;
}

} // End of namespace Grim
