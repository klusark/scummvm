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
#include "common/ustr.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/costume/choreinstance.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/scene/overworld.h"
#include "engines/grim/emi/actor/puck_v2.h"
#include "engines/grim/emi/nodes/rprimitive.h"
#include "engines/grim/emi/nodes/rprimitiveinstance.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

static const int kSavedStringBufferSize = 512;
static const uint kMaxSkippedStateSize = 65536;
static const char *const kOverworldRoomName = "[OW]";

static void writeSavedString(SaveRestoreAccess *accessFunc, const LString &string) {
	int length = (int)string.size() + 1;

	accessFunc(&length, sizeof(length));
	accessFunc((void *)string.c_str(), length);
}

static bool readSavedString(SaveRestoreAccess *accessFunc, LString &string) {
	char buffer[kSavedStringBufferSize + 1];
	uint length = 0;

	accessFunc(&length, sizeof(length));

	if (length > (uint)kSavedStringBufferSize) {
		warning("Actor::restore(): invalid string length %u", length);

		return false;
	}

	accessFunc(buffer, length);
	buffer[length] = 0;
	string = buffer;

	return true;
}

static void writeSavedFlag(SaveRestoreAccess *accessFunc, bool value) {
	byte raw = value ? 1 : 0;

	accessFunc(&raw, sizeof(raw));
}

static bool readSavedFlag(SaveRestoreAccess *accessFunc) {
	byte raw = 0;

	accessFunc(&raw, sizeof(raw));

	return raw != 0;
}

static void restoreNodeEuler(CNode *node, const CVector &angles) {
	node->_eulerDeg = angles;
	node->_nodeFlags = (node->_nodeFlags & ~CNode::kQuaternionValid) | CNode::kEulerValid | CNode::kLocalSet;
	node->updateLocalState();
}

bool Actor::save(SaveRestoreAccess *accessFunc) {
	bool isPs2 = _engine->getGamePlatform() == Common::kPlatformPS2;

	writeSavedString(accessFunc,_actorName);
	accessFunc(&_id, sizeof(_id));
	accessFunc(&_actorFlags, sizeof(_actorFlags));
	accessFunc(&_mode, sizeof(_mode));

	if ((_nodeFlags & kPositionValid) == 0)
		computeLocalPositionFromWorld();

	accessFunc(&_pos, sizeof(_pos));

	if ((_nodeFlags & kEulerValid) == 0)
		computeLocalMatrixFromWorld();

	accessFunc(&_eulerDeg, sizeof(_eulerDeg));
	accessFunc(&_prevPos, sizeof(_prevPos));
	accessFunc(&_reserved312, sizeof(_reserved312));
	writeSavedFlag(accessFunc,_onScreen);
	accessFunc(&_sortOrder, sizeof(_sortOrder));

	if (_parentActorId && !actorDeref(_engine, _parentActorId))
		_parentActorId = 0;

	accessFunc(&_parentActorId, sizeof(_parentActorId));

	if (_parentActorId) {
		if (_parent) {
			accessFunc(_parent->_name, CNode::kNameSize);
		} else {
			char emptyName[CNode::kNameSize];

			warning("Actor::save(): the parent node is missing");
			memset(emptyName, 0, sizeof(emptyName));
			accessFunc(emptyName, CNode::kNameSize);
		}
	}

	for (int i = 0; i < 4; i++)
		accessFunc(&_drawnRect[i], sizeof(int));

	LString roomName;

	if (_room._resource) {
		bool locked = _room.lock();

		if (locked && _room._typed)
			roomName = _room._typed->_name;
		else
			warning("Actor::save(): the room could not be locked");

		if (locked)
			_room.unlock();
	} else if (_inOverworld) {
		roomName = kOverworldRoomName;
	}

	writeSavedString(accessFunc,roomName);

	removeFinishedChores();

	int choreCount = (int)_choreIds.size();

	accessFunc(&choreCount, sizeof(choreCount));

	Common::List<uint>::iterator choreIt;

	for (choreIt = _choreIds.begin(); choreIt != _choreIds.end(); ++choreIt) {
		uint choreId = *choreIt;

		accessFunc(&choreId, sizeof(choreId));

		if (ChoreInstance::isInstanceValid(_engine, choreId)) {
			ChoreInstance *instance = ChoreInstance::getInstance(_engine, choreId);

			if (instance)
				instance->save(accessFunc);
		}
	}

	for (int i = 0; i < kNumChoreSlots; i++) {
		writeSavedString(accessFunc,_choreSlotNames[i]);
		writeSavedString(accessFunc,_choreSlotSetNames[i]);
		accessFunc(&_choreSlotIds[i], sizeof(uint));
	}

	int primitiveCount = (int)_instances.size();

	accessFunc(&primitiveCount, sizeof(primitiveCount));

	for (int i = 0; i < primitiveCount; i++) {
		RPrimitiveInstance *instance = _instances[i];

		if (instance->_prim) {
			writeSavedString(accessFunc,instance->_prim->_name);
		} else {
			warning("Actor::save(): the primitive instance has no primitive");
			writeSavedString(accessFunc,LString());
		}

		int stateSize = instance->getSaveSize();

		accessFunc(&stateSize, sizeof(stateSize));
		instance->saveState(accessFunc);
	}

	int shadowCount = (int)_shadows.size();

	accessFunc(&shadowCount, sizeof(shadowCount));

	for (int i = 0; i < shadowCount; i++)
		writeSavedString(accessFunc,_shadows[i]->_name);

	writeSavedFlag(accessFunc,_extra != nullptr);

	if (_extra) {
		writeSavedFlag(accessFunc,_extra->_lookAtActive);
		writeSavedFlag(accessFunc,_extra->_lookAtHasTarget);
		accessFunc(&_extra->_lookTarget, sizeof(CVector));
		accessFunc(&_extra->_lookOffset, sizeof(CVector));
		accessFunc(&_extra->_lookTargetActorId, sizeof(int));
		accessFunc(&_extra->_lookGoalQuat, sizeof(CQuaternion));
		accessFunc(&_extra->_lookDirection, sizeof(CVector));
		accessFunc(&_extra->_lookCurQuat, sizeof(CQuaternion));
		accessFunc(_extra->_lookLimits, Extra_v2::kNumLookLimits * sizeof(CVector));
		writeSavedString(accessFunc,_extra->_lookJointName);
		accessFunc(&_extra->_lookCosFov, sizeof(float));
		accessFunc(&_extra->_lookRate, sizeof(float));
		writeSavedFlag(accessFunc,_extra->_walking);

		// The disasm saves only the first byte of turnDir
		byte turnDirLow = (byte)(_extra->_turnDir & 0xFF);

		accessFunc(&turnDirLow, sizeof(turnDirLow));
		writeSavedFlag(accessFunc,_extra->_resting);
		accessFunc(&_extra->_walkRate, sizeof(float));
		accessFunc(&_extra->_turnRate, sizeof(float));
		writeSavedFlag(accessFunc,_extra->_aligned);
		writeSavedFlag(accessFunc,_extra->_snapTurn);
		accessFunc(&_extra->_offsetYaw, sizeof(float));
		accessFunc(&_extra->_alignOff, sizeof(float));
		accessFunc(&_extra->_walkTarget, sizeof(CVector));
		accessFunc(&_extra->_goal, sizeof(CVector));
		accessFunc(&_extra->_numPathPoints, sizeof(int));
		accessFunc(_extra->_path, Extra_v2::kMaxPathPoints * sizeof(CVector));
		accessFunc(&_extra->_pathIndex, sizeof(int));
		writeSavedFlag(accessFunc,_extra->_puck != nullptr);
		writeSavedFlag(accessFunc,_extra->_inBoxes);
		writeSavedFlag(accessFunc,_extra->_walkDominate);

		if (!isPs2)
			accessFunc(&_extra->_reflectionAngle, sizeof(float));

		writeSavedFlag(accessFunc,_extra->_talking);
		accessFunc(&_extra->_talkTimerMs, sizeof(int));
		accessFunc(_extra->_sayLineId, Extra_v2::kSayLineIdSize);
		accessFunc(&_extra->_textEventId, sizeof(int));
		writeSavedFlag(accessFunc,_extra->_lipsyncActive);
		accessFunc(&_extra->_soundHandle, sizeof(int));
		accessFunc(&_extra->_visemeCurrent, sizeof(int));
		accessFunc(&_extra->_collisionMode, sizeof(int));
		accessFunc(&_extra->_collisionScale, sizeof(float));
		writeSavedFlag(accessFunc,_extra->_inCollision);
		accessFunc(&_extra->_talkColor, sizeof(Color));
		accessFunc(&_extra->_timeScale, sizeof(float));

		if (_extra->_puck) {
			Puck_v2 *puck = _extra->_puck;

			if ((puck->_nodeFlags & kPositionValid) == 0)
				puck->computeLocalPositionFromWorld();

			accessFunc(&puck->_pos, sizeof(CVector));

			if ((puck->_nodeFlags & kEulerValid) == 0)
				puck->computeLocalMatrixFromWorld();

			accessFunc(&puck->_eulerDeg, sizeof(CVector));

			if (!isPs2) {
				// The disasm saves only the first byte of the reflection angle
				byte reflectionRaw[sizeof(float)];

				memcpy(reflectionRaw, &puck->_reflectionAngle, sizeof(reflectionRaw));
				accessFunc(&reflectionRaw[0], sizeof(byte));
			}

			writeSavedFlag(accessFunc,puck->_constrained);
			accessFunc(puck->_constraint, sizeof(puck->_constraint));
			accessFunc(puck->_lineA, sizeof(puck->_lineA));
			accessFunc(puck->_lineB, sizeof(puck->_lineB));
		}
	}

	accessFunc(&_drawFlags, sizeof(_drawFlags));

	return true;
}

bool Actor::restore(SaveRestoreAccess *accessFunc) {
	bool isPs2 = _engine->getGamePlatform() == Common::kPlatformPS2;
	CVector savedVector;

	if (!readSavedString(accessFunc,_actorName))
		return false;

	accessFunc(&_id, sizeof(_id));
	accessFunc(&_actorFlags, sizeof(_actorFlags));
	accessFunc(&_mode, sizeof(_mode));
	accessFunc(&savedVector, sizeof(savedVector));
	setLocalPosition(savedVector);
	accessFunc(&savedVector, sizeof(savedVector));
	restoreNodeEuler(this, savedVector);
	accessFunc(&_prevPos, sizeof(_prevPos));
	accessFunc(&_reserved312, sizeof(_reserved312));
	_onScreen = readSavedFlag(accessFunc);
	accessFunc(&_sortOrder, sizeof(_sortOrder));
	accessFunc(&_parentActorId, sizeof(_parentActorId));

	if (_parentActorId)
		accessFunc(_parentNodeName, CNode::kNameSize);

	for (int i = 0; i < 4; i++)
		accessFunc(&_drawnRect[i], sizeof(int));

	LString savedRoomName;

	if (!readSavedString(accessFunc,savedRoomName))
		return false;

	if (savedRoomName == kOverworldRoomName)
		_engine->getOverworld()->putActor(this);
	else if (!savedRoomName.empty())
		putInSet(savedRoomName.c_str());
	else
		putInSet(nullptr);

	uint choreCount = 0;

	accessFunc(&choreCount, sizeof(choreCount));

	for (uint i = 0; i < choreCount; i++) {
		uint choreId = 0;

		accessFunc(&choreId, sizeof(choreId));

		if (ChoreInstance::isInstanceValid(_engine, choreId)) {
			ChoreInstance *existing = ChoreInstance::getInstance(_engine, choreId);

			if (existing)
				delete existing;
		}

		choreId = ChoreInstance::newInstanceWithId(_engine, this, choreId);

		if (ChoreInstance::isInstanceValid(_engine, choreId)) {
			ChoreInstance *instance = ChoreInstance::getInstance(_engine, choreId);

			if (instance)
				instance->restore(accessFunc);
		}

		_choreIds.push_back(choreId);
	}

	for (int i = 0; i < kNumChoreSlots; i++) {
		if (!readSavedString(accessFunc,_choreSlotNames[i]))
			return false;

		if (!readSavedString(accessFunc,_choreSlotSetNames[i]))
			return false;

		accessFunc(&_choreSlotIds[i], sizeof(uint));
	}

	uint primitiveCount = 0;

	accessFunc(&primitiveCount, sizeof(primitiveCount));

	for (uint i = 0; i < primitiveCount; i++) {
		LString primitiveName;
		bool found = false;

		if (!readSavedString(accessFunc,primitiveName))
			return false;

		for (uint j = 0; j < _instances.size() && !found; j++) {
			RPrimitiveInstance *instance = _instances[j];

			if (instance->_prim && instance->_prim->_name == primitiveName && !instance->_stateRestored) {
				int stateSize = 0;

				accessFunc(&stateSize, sizeof(stateSize));
				instance->restoreState(accessFunc);
				found = true;
			}
		}

		if (!found) {
			uint stateSize = 0;

			Common::String message = Common::String::format("Could not find file %s.", primitiveName.c_str());

			warning("%s", message.c_str());
			_engine->showMessage(message.c_str(), "Error!");
			accessFunc(&stateSize, sizeof(stateSize));

			if (stateSize > kMaxSkippedStateSize) {
				warning("Actor::restore(): invalid primitive state size %u", stateSize);

				return false;
			}

			byte discard[256];

			while (stateSize > 0) {
				uint chunk = stateSize < sizeof(discard) ? stateSize : (uint)sizeof(discard);

				accessFunc(discard, chunk);
				stateSize -= chunk;
			}
		}
	}

	uint shadowCount = 0;

	accessFunc(&shadowCount, sizeof(shadowCount));

	for (uint i = 0; i < shadowCount; i++) {
		LString shadowName;

		if (!readSavedString(accessFunc,shadowName))
			return false;

		bool locked = _room.lock();
		Room_v2 *lockedRoom = locked ? _room._typed : nullptr;

		if (lockedRoom)
			activateShadow(true, lockedRoom->findShadow(shadowName));
		else
			warning("Actor::restore(): the room could not be locked");

		if (locked)
			_room.unlock();
	}

	if (readSavedFlag(accessFunc)) {
		_extra = nullptr;
		allocateActorExtra();

		_extra->_lookAtActive = readSavedFlag(accessFunc);
		_extra->_lookAtHasTarget = readSavedFlag(accessFunc);
		accessFunc(&_extra->_lookTarget, sizeof(CVector));
		accessFunc(&_extra->_lookOffset, sizeof(CVector));
		accessFunc(&_extra->_lookTargetActorId, sizeof(int));
		accessFunc(&_extra->_lookGoalQuat, sizeof(CQuaternion));
		accessFunc(&_extra->_lookDirection, sizeof(CVector));
		accessFunc(&_extra->_lookCurQuat, sizeof(CQuaternion));
		accessFunc(_extra->_lookLimits, Extra_v2::kNumLookLimits * sizeof(CVector));

		if (!readSavedString(accessFunc,_extra->_lookJointName))
			return false;

		accessFunc(&_extra->_lookCosFov, sizeof(float));
		accessFunc(&_extra->_lookRate, sizeof(float));
		_extra->_walking = readSavedFlag(accessFunc);

		// The disasm restores only the first byte of turnDir
		byte turnDirLow = 0;

		accessFunc(&turnDirLow, sizeof(turnDirLow));
		_extra->_turnDir = (_extra->_turnDir & ~0xFF) | turnDirLow;
		_extra->_resting = readSavedFlag(accessFunc);
		accessFunc(&_extra->_walkRate, sizeof(float));
		accessFunc(&_extra->_turnRate, sizeof(float));
		_extra->_aligned = readSavedFlag(accessFunc);
		_extra->_snapTurn = readSavedFlag(accessFunc);
		accessFunc(&_extra->_offsetYaw, sizeof(float));
		accessFunc(&_extra->_alignOff, sizeof(float));
		accessFunc(&_extra->_walkTarget, sizeof(CVector));
		accessFunc(&_extra->_goal, sizeof(CVector));
		accessFunc(&_extra->_numPathPoints, sizeof(int));
		accessFunc(_extra->_path, Extra_v2::kMaxPathPoints * sizeof(CVector));
		accessFunc(&_extra->_pathIndex, sizeof(int));

		bool hasPuck = readSavedFlag(accessFunc);

		_extra->_inBoxes = readSavedFlag(accessFunc);
		_extra->_walkDominate = readSavedFlag(accessFunc);

		if (!isPs2)
			accessFunc(&_extra->_reflectionAngle, sizeof(float));

		_extra->_talking = readSavedFlag(accessFunc);
		accessFunc(&_extra->_talkTimerMs, sizeof(int));
		accessFunc(_extra->_sayLineId, Extra_v2::kSayLineIdSize);
		accessFunc(&_extra->_textEventId, sizeof(int));
		_extra->_lipsyncActive = readSavedFlag(accessFunc);
		accessFunc(&_extra->_soundHandle, sizeof(int));
		accessFunc(&_extra->_visemeCurrent, sizeof(int));
		accessFunc(&_extra->_collisionMode, sizeof(int));
		accessFunc(&_extra->_collisionScale, sizeof(float));
		_extra->_inCollision = readSavedFlag(accessFunc);
		accessFunc(&_extra->_talkColor, sizeof(Color));
		accessFunc(&_extra->_timeScale, sizeof(float));

		if (hasPuck) {
			Puck_v2 *puck = createPuck(_extra->_reflectionAngle);
			CVector puckVector;

			_extra->_puck = puck;
			puck->_actorName = _actorName;

			accessFunc(&puckVector, sizeof(puckVector));
			puck->setLocalPosition(puckVector);
			accessFunc(&puckVector, sizeof(puckVector));
			restoreNodeEuler(puck, puckVector);

			if (!isPs2) {
				byte reflectionRaw[sizeof(float)];

				memcpy(reflectionRaw, &puck->_reflectionAngle, sizeof(reflectionRaw));
				accessFunc(&reflectionRaw[0], sizeof(byte));
				memcpy(&puck->_reflectionAngle, reflectionRaw, sizeof(reflectionRaw));
			}

			puck->_constrained = readSavedFlag(accessFunc);
			accessFunc(puck->_constraint, sizeof(puck->_constraint));
			accessFunc(puck->_lineA, sizeof(puck->_lineA));
			accessFunc(puck->_lineB, sizeof(puck->_lineB));

			if (_extra->_inBoxes && _room._resource) {
				bool locked = _room.lock();
				Room_v2 *lockedRoom = locked ? _room._typed : nullptr;

				if (lockedRoom)
					puck->validate(lockedRoom->_walkPlanes);
				else
					warning("Actor::restore(): the room could not be locked");

				if (locked)
					_room.unlock();
			}
		}

		if (_extra->_talking) {
			for (int i = 0; i < kNumSpeakers; i++) {
				if (!_engine->_speakers[i]) {
					_engine->_speakers[i] = this;
					_engine->_numSpeakers++;

					break;
				}
			}
		}
	}

	accessFunc(&_drawFlags, sizeof(_drawFlags));

	return true;
}

void Actor::saveActors(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	int selectedId = -1;

	if (engine->_selectedActor)
		selectedId = engine->_selectedActor->_id;

	accessFunc(&selectedId, sizeof(selectedId));

	int actorCount = (int)engine->_actors.size();

	accessFunc(&actorCount, sizeof(actorCount));

	Common::List<Actor *>::iterator it;

	for (it = engine->_actors.begin(); it != engine->_actors.end(); ++it)
		(*it)->save(accessFunc);

	accessFunc(&engine->_nextChoreInstanceId, sizeof(engine->_nextChoreInstanceId));
	accessFunc(&engine->_nextActorId, sizeof(engine->_nextActorId));
}

void Actor::restoreActors(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	for (int i = 0; i < kNumSpeakers; i++)
		engine->_speakers[i] = nullptr;

	engine->_numSpeakers = 0;

	int selectedId = 0;
	int actorCount = 0;

	accessFunc(&selectedId, sizeof(selectedId));
	accessFunc(&actorCount, sizeof(actorCount));

	for (int i = 0; i < actorCount; i++) {
		Actor *actor = createActor(engine, "Restored");

		actor->restore(accessFunc);
	}

	accessFunc(&engine->_nextChoreInstanceId, sizeof(engine->_nextChoreInstanceId));
	accessFunc(&engine->_nextActorId, sizeof(engine->_nextActorId));

	engine->_selectedActor = nullptr;

	if (selectedId >= 0)
		engine->_selectedActor = actorDeref(engine, selectedId);

	Common::List<Actor *>::iterator it;

	for (it = engine->_actors.begin(); it != engine->_actors.end(); ++it) {
		Actor *actor = *it;

		if (!actor->_parentActorId)
			continue;

		Actor *parentActor = actorDeref(engine, actor->_parentActorId);

		if (parentActor) {
			char nodeName[CNode::kNameSize + 1];

			memcpy(nodeName, actor->_parentNodeName, CNode::kNameSize);
			nodeName[CNode::kNameSize] = 0;

			CNode *parentNode = parentActor->findNode(nodeName);

			if (!parentNode)
				parentNode = parentActor;

			actor->setParent(parentNode);
		} else {
			actor->_parentActorId = 0;
			actor->_parentNodeName[0] = 0;
		}
	}
}

} // End of namespace Grim
