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

#include "engines/grim/chore/cos.h"
#include "engines/grim/puck.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/costume/sputcostume_components.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/walkplane.h"

namespace Grim {

int SputActorModule::saveRemastered(Common::WriteStream *stream) {
	stream->writeSint32LE(_maxActors);
	stream->writeSint32LE(_numActors);

	for (int actorIndex = 0; actorIndex < _maxActors; actorIndex++) {
		SputActor *actor = &_actors[actorIndex];

		if (!actor->inUse)
			continue;

		stream->writeSint32LE(actorIndex);
		stream->writeSint32LE(actor->colorMap->valid ? _res->serialFromHandle(actor->colorMap->handle) : actor->colorMap->serial);
		stream->writeSint32LE(0);
		stream->writeSint32LE(actor->flags);
		stream->writeSint32LE(actor->mode);

		for (int i = 0; i < 3; i++)
			stream->writeFloatLE(actor->actorPos[i]);

		for (int i = 0; i < 3; i++)
			stream->writeFloatLE(actor->prevPosition[i]);

		for (int i = 0; i < 3; i++)
			stream->writeFloatLE(actor->actorRot[i]);

		stream->writeFloatLE(actor->scale);
		stream->writeSint32LE(actor->matrixValid);

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 3; j++)
				stream->writeFloatLE(actor->orientMatrix[i][j]);
		}

		stream->writeSint32LE(actor->onScreen);
		stream->writeSint32LE(actor->drawnRect.x);
		stream->writeSint32LE(actor->drawnRect.y);
		stream->writeSint32LE(actor->drawnRect.width);
		stream->writeSint32LE(actor->drawnRect.height);
		stream->write(actor->curSetName, 32);
		stream->writeSint32LE(actor->extra != nullptr);

		int numCostumes = countCostumes(actor);

		stream->writeSint32LE(numCostumes);

		CostumeNode **nodes = new CostumeNode *[numCostumes + 1];
		CostumeNode *node = actor->costumeStack;

		for (int i = numCostumes - 1; i >= 0; i--) {
			nodes[i] = node;
			node = node->next;
		}

		for (int i = 0; i < numCostumes; i++) {
			CostumeNode *saved = nodes[i];

			stream->writeSint32LE(saved->costumeInstance);
			stream->writeSint32LE(0);

			for (int j = 0; j < 3; j++)
				stream->writeSint32LE(saved->headJoint[j]);

			stream->writeFloatLE(saved->headTurnRate);
			stream->writeFloatLE(saved->headYawLimit);
			stream->writeFloatLE(saved->headPitchLimit);
			stream->writeFloatLE(saved->headLookRef);
			stream->writeSint32LE(saved->walkChore);
			stream->writeSint32LE(saved->turnLeftChore);
			stream->writeSint32LE(saved->turnRightChore);
			stream->writeSint32LE(saved->restChore);

			for (int j = 0; j < 16; j++)
				stream->writeSint32LE(saved->chore[j]);

			stream->writeSint32LE(saved->mumbleChore);

			if (!saved->rCostume.valid)
				warning("SputActorModule::saveRemastered(): the costume reference is not valid");

			stream->writeSint32LE(saved->rCostume.valid ? _res->serialFromHandle(saved->rCostume.handle) : saved->rCostume.serial);
			stream->writeSint32LE(0);
			_cos->saveInstance(saved->costumeInstance, stream);
		}

		delete[] nodes;

		if (actor->costumeStack) {
			int numModels = 0;

			for (ModelInst *modelInst = actor->costumeStack->headList; modelInst; modelInst = modelInst->next)
				numModels++;

			stream->writeSint32LE(numModels);

			for (ModelInst *modelInst = actor->costumeStack->headList; modelInst; modelInst = modelInst->next) {
				stream->writeSint32LE(modelInst->visible);
				stream->writeSint32LE(modelInst->resolveParentOffset);
				stream->writeSint32LE(modelInst->parentMeshIdx);

				if (modelInst->thing->type == 1) {
					if (!modelInst->modelRef.valid)
						warning("SputActorModule::saveRemastered(): Bad model Reference");

					RdModel3 *model3 = static_cast<RdModel3 *>(modelInst->modelRef.handle);

					for (uint i = 0; i < model3->numHNodes; i++)
						stream->writeSint32LE(modelInst->thing->abMeshVisible[i]);
				}

				int numTracks = 0;
				RdPuppet *puppet = modelInst->thing->puppet;

				if (puppet) {
					for (int i = 0; i < 6; i++) {
						if (puppet->tracks[i].keyframe)
							numTracks++;
					}

					stream->writeSint32LE(numTracks);

					for (int i = 0; i < 6; i++) {
						RdPuppetTrack *track = &puppet->tracks[i];

						if (!track->keyframe)
							continue;

						stream->write(track->keyframe->name, 32);
						stream->writeSint32LE(track->priority);
						stream->writeSint32LE(track->priorityMasked);
						stream->writeUint32LE(track->status);
						stream->writeFloatLE(track->curFrame);
						stream->writeFloatLE(track->prevFrame);
						stream->writeFloatLE(track->fade);
						stream->writeFloatLE(track->fadeSpeed);
					}
				} else {
					stream->writeSint32LE(numTracks);
				}
			}
		}

		Extra *extra = actor->extra;

		if (extra) {
			for (int i = 0; i < 3; i++)
				stream->writeFloatLE(extra->lookCarrot[i]);

			stream->writeSint32LE(extra->walking);
			stream->writeSint32LE(extra->turnDir);
			stream->writeSint32LE(extra->resting);
			stream->writeFloatLE(extra->walkRate);
			stream->writeFloatLE(extra->turnRate);
			stream->writeSint32LE(extra->aligned);
			stream->writeFloatLE(extra->offsetYaw);
			stream->writeFloatLE(extra->alignOff);

			for (int i = 0; i < 3; i++)
				stream->writeFloatLE(extra->walkTarget[i]);

			for (int i = 0; i < 3; i++)
				stream->writeFloatLE(extra->goal[i]);

			stream->writeSint32LE(extra->numPathPoints);

			for (int i = 0; i < kExtraPathPoints; i++) {
				for (int j = 0; j < 3; j++)
					stream->writeFloatLE(extra->path[i][j]);
			}

			stream->writeSint32LE(extra->pathIndex);
			stream->writeSint32LE(extra->puck != nullptr);
			stream->writeSint32LE(0);
			stream->writeSint32LE(extra->inBoxes);
			stream->writeSint32LE(extra->walkDominate);
			stream->writeFloatLE(extra->reflectionAngle);
			stream->writeSint32LE(extra->talking);
			stream->writeSint32LE(extra->soundId);
			stream->writeSint32LE(extra->soundId < 0 ? -1 : 0);
			stream->writeSint32LE(extra->talkTimerMs);
			stream->write(extra->sayLineId, 12);
			stream->write(extra->voiceName, 15);

			if (extra->sayText)
				stream->writeSint32LE(extra->sayText->serial);
			else
				stream->writeSint32LE(0);

			stream->writeSint32LE(0);
			stream->writeSint32LE(extra->phoneme);
			stream->writeSint32LE(extra->activeShadow);
			stream->writeSint32LE(extra->collisionMode);
			stream->writeFloatLE(extra->collisionScale);
			stream->writeSint32LE(extra->collided);
			stream->writeSint32LE(extra->clipPlaneActive);

			for (int i = 0; i < 3; i++)
				stream->writeFloatLE(extra->clipPlaneNorm[i]);

			for (int i = 0; i < 3; i++)
				stream->writeFloatLE(extra->clipPlanePt[i]);

			stream->writeSint32LE(extra->talkColor);
			stream->writeFloatLE(extra->timeScale);
			stream->writeSint32LE(extra->subClipNode);

			if (!_sputRoom->getCurrentRoom())
				resetShadows(actor);

			for (int i = 0; i < 6; i++) {
				ActorShadow *shadow = &extra->shadows[i];
				int numClipPlanes = 0;

				stream->writeSint32LE(shadow->active);
				stream->writeSint32LE(shadow->side);

				for (int j = 0; j < 3; j++)
					stream->writeFloatLE(shadow->normal[j]);

				stream->writeFloatLE(shadow->planeD);

				for (int j = 0; j < 3; j++)
					stream->writeFloatLE(shadow->lightPos[j]);

				for (int j = 0; j < 12; j++) {
					if (shadow->clipPlane[j])
						numClipPlanes++;
				}

				stream->writeSint32LE(numClipPlanes);

				for (int j = 0; j < 12; j++) {
					if (shadow->clipPlane[j])
						stream->write(shadow->clipPlane[j]->_name, 32);
				}
			}
		}
	}

	int selectedIndex = -1;

	if (_selectedActor)
		selectedIndex = (int)(_selectedActor - _actors);

	stream->writeSint32LE(selectedIndex);

	if (stream->err()) {
		warning("SputActorModule::saveRemastered(): failed to write the actors");
		return 1;
	}

	return 0;
}

int SputActorModule::restoreRemastered(Common::ReadStream *stream) {
	for (int i = 0; i < 4; i++)
		_talkingActors[i] = nullptr;

	_numActorsTalking = 0;

	for (int i = 0; i < _maxActors; i++)
		freeActor(&_actors[i]);

	int savedMaxActors = stream->readSint32LE();

	if (savedMaxActors > _maxActors) {
		SputActor *newActors = new SputActor[savedMaxActors]();

		memcpy(newActors, _actors, _maxActors * sizeof(SputActor));
		delete[] _actors;
		_actors = newActors;
		_maxActors = savedMaxActors;
		publishActors();
	}

	_numActors = stream->readSint32LE();

	for (int actorCount = 0; actorCount < _numActors; actorCount++) {
		int actorIndex = stream->readSint32LE();

		if (stream->err() || stream->eos() || actorIndex < 0 || actorIndex >= _maxActors) {
			warning("SputActorModule::restoreRemastered(): invalid actor index %d", actorIndex);
			return 1;
		}

		SputActor *actor = &_actors[actorIndex];

		actor->inUse = 1;

		if (!actor->colorMap)
			actor->colorMap = new ResRef();

		actor->colorMap->serial = stream->readSint32LE();
		stream->readSint32LE();
		actor->flags = stream->readSint32LE();
		actor->mode = stream->readSint32LE();

		for (int i = 0; i < 3; i++)
			actor->actorPos[i] = stream->readFloatLE();

		for (int i = 0; i < 3; i++)
			actor->prevPosition[i] = stream->readFloatLE();

		for (int i = 0; i < 3; i++)
			actor->actorRot[i] = stream->readFloatLE();

		actor->scale = stream->readFloatLE();
		actor->matrixValid = stream->readSint32LE();
		actor->matrixValid = 0;

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 3; j++)
				actor->orientMatrix[i][j] = stream->readFloatLE();
		}

		actor->onScreen = stream->readSint32LE();
		actor->drawnRect.x = stream->readSint32LE();
		actor->drawnRect.y = stream->readSint32LE();
		actor->drawnRect.width = stream->readSint32LE();
		actor->drawnRect.height = stream->readSint32LE();
		stream->read(actor->curSetName, 32);
		actor->curSetName[31] = 0;

		int hasExtra = stream->readSint32LE();

		_res->resolveToSerialNumber(actor->colorMap);

		if (actor->curSetName[0]) {
			char roomName[64];

			Common::strlcpy(roomName, actor->curSetName, sizeof(roomName));
			actor->curSetName[0] = 0;
			_sputRoom->addActor(roomName, actor);
		}

		actor->costumeStack = nullptr;

		int numCostumes = stream->readSint32LE();

		if (stream->err() || stream->eos() || numCostumes < 0) {
			warning("SputActorModule::restoreRemastered(): invalid costume count");
			return 1;
		}

		for (int costumeIndex = 0; costumeIndex < numCostumes; costumeIndex++) {
			int headJoint[3];
			float headTurnRate;
			float headYawLimit;
			float headPitchLimit;
			float headLookRef;
			int walkChore;
			int turnLeftChore;
			int turnRightChore;
			int restChore;
			int chore[16];
			int mumbleChore;
			ResRef costumeRef;

			stream->readSint32LE();
			stream->readSint32LE();

			for (int i = 0; i < 3; i++)
				headJoint[i] = stream->readSint32LE();

			headTurnRate = stream->readFloatLE();
			headYawLimit = stream->readFloatLE();
			headPitchLimit = stream->readFloatLE();
			headLookRef = stream->readFloatLE();
			walkChore = stream->readSint32LE();
			turnLeftChore = stream->readSint32LE();
			turnRightChore = stream->readSint32LE();
			restChore = stream->readSint32LE();

			for (int i = 0; i < 16; i++)
				chore[i] = stream->readSint32LE();

			mumbleChore = stream->readSint32LE();

			memset(&costumeRef, 0, sizeof(costumeRef));
			costumeRef.serial = stream->readSint32LE();
			stream->readSint32LE();
			_res->resolveToSerialNumber(&costumeRef);

			if (!costumeRef.valid) {
				warning("SputActorModule::restoreRemastered(): Bad costume Reference");
				return 1;
			}

			CosCostume *costume = _cos->ptrFromHandle(costumeRef.serial);
			int pushed;

			if (costumeIndex)
				pushed = pushCostume(actor, costume->name, 0);
			else
				pushed = setCostume(actor, costume->name, 0);

			if (!pushed) {
				warning("SputActorModule::restoreRemastered(): failed to load the costume %s", costume->name);
				_res->removeReference(&costumeRef);
				return 1;
			}

			_cos->restoreInstance(actor->costumeStack->costumeInstance, stream);

			CostumeNode *node = actor->costumeStack;

			node->headJoint[0] = headJoint[0];
			node->headJoint[1] = headJoint[1];
			node->headJoint[2] = headJoint[2];
			node->headTurnRate = headTurnRate;
			node->headYawLimit = headYawLimit;
			node->headPitchLimit = headPitchLimit;
			node->headLookRef = headLookRef;
			node->walkChore = walkChore;
			node->turnLeftChore = turnLeftChore;
			node->turnRightChore = turnRightChore;
			node->restChore = restChore;
			memcpy(node->chore, chore, sizeof(node->chore));
			node->mumbleChore = mumbleChore;
			_res->removeReference(&costumeRef);
		}

		if (actor->costumeStack) {
			ModelInst *modelInst = actor->costumeStack->headList;
			int numModels = stream->readSint32LE();

			if (numModels > 0) {
				while (modelInst) {
					modelInst->visible = stream->readSint32LE();
					modelInst->resolveParentOffset = stream->readSint32LE();
					modelInst->parentMeshIdx = stream->readSint32LE();

					if (modelInst->thing->type == 1) {
						if (!modelInst->modelRef.valid) {
							warning("SputActorModule::restoreRemastered(): Bad model Reference");
							return 1;
						}

						RdModel3 *model3 = static_cast<RdModel3 *>(modelInst->modelRef.handle);

						for (uint i = 0; i < model3->numHNodes; i++)
							modelInst->thing->abMeshVisible[i] = stream->readSint32LE();
					}

					int numTracks = stream->readSint32LE();

					for (int i = 0; i < numTracks; i++) {
						RdPuppet *puppet = modelInst->thing->puppet;
						char keyframeName[33];

						stream->read(keyframeName, 32);
						keyframeName[32] = 0;

						if (!puppet) {
							warning("SputActorModule::restoreRemastered(): the model has no puppet for the track %s", keyframeName);
							return 1;
						}

						RdKeyframe *keyframe = static_cast<RdKeyframe *>(_sputResource->alloc(kSputTagKeyf, keyframeName));

						if (keyframe) {
							int priority = stream->readSint32LE();
							int priorityMasked = stream->readSint32LE();
							int trackNum = _rd->_puppet.addTrack(puppet, keyframe, priority, priorityMasked);
							uint status = stream->readUint32LE();
							float curFrame = stream->readFloatLE();
							float prevFrame = stream->readFloatLE();
							float fade = stream->readFloatLE();
							float fadeSpeed = stream->readFloatLE();

							if (trackNum >= 0 && trackNum < 6) {
								puppet->tracks[trackNum].status = status;
								puppet->tracks[trackNum].curFrame = curFrame;
								puppet->tracks[trackNum].prevFrame = prevFrame;
								puppet->tracks[trackNum].fade = fade;
								puppet->tracks[trackNum].fadeSpeed = fadeSpeed;
							} else {
								warning("SputActorModule::restoreRemastered(): the keyframe %s could not be added to the puppet", keyframeName);
							}
						}
					}

					modelInst = modelInst->next;
				}
			}
		}

		if (hasExtra) {
			actor->extra = nullptr;
			allocateActorExtra(actor);

			Extra *extra = actor->extra;

			for (int i = 0; i < 3; i++)
				extra->lookCarrot[i] = stream->readFloatLE();

			extra->walking = stream->readSint32LE();
			extra->turnDir = stream->readSint32LE();
			extra->resting = stream->readSint32LE();
			extra->walkRate = stream->readFloatLE();
			extra->turnRate = stream->readFloatLE();
			extra->aligned = stream->readSint32LE();
			extra->offsetYaw = stream->readFloatLE();
			extra->alignOff = stream->readFloatLE();

			for (int i = 0; i < 3; i++)
				extra->walkTarget[i] = stream->readFloatLE();

			for (int i = 0; i < 3; i++)
				extra->goal[i] = stream->readFloatLE();

			extra->numPathPoints = stream->readSint32LE();

			for (int i = 0; i < kExtraPathPoints; i++) {
				for (int j = 0; j < 3; j++)
					extra->path[i][j] = stream->readFloatLE();
			}

			if (extra->numPathPoints < 0 || extra->numPathPoints > kExtraPathPoints) {
				warning("SputActorModule::restoreRemastered(): the path of the actor %d has %d points", actorIndex, extra->numPathPoints);
				extra->numPathPoints = kExtraPathPoints;
			}

			extra->pathIndex = stream->readSint32LE();

			int hasPuck = stream->readSint32LE();

			stream->readSint32LE();

			extra->inBoxes = stream->readSint32LE();
			extra->walkDominate = stream->readSint32LE();
			extra->reflectionAngle = stream->readFloatLE();
			extra->talking = stream->readSint32LE();
			extra->soundId = stream->readSint32LE();
			stream->readSint32LE();
			extra->talkTimerMs = stream->readSint32LE();
			stream->read(extra->sayLineId, 12);
			stream->read(extra->voiceName, 15);
			extra->voiceName[15] = 0;

			int sayTextSerial = stream->readSint32LE();

			stream->readSint32LE();

			extra->phoneme = stream->readSint32LE();
			extra->activeShadow = stream->readSint32LE();
			extra->collisionMode = stream->readSint32LE();
			extra->collisionScale = stream->readFloatLE();
			extra->collided = stream->readSint32LE();
			extra->clipPlaneActive = stream->readSint32LE();

			for (int i = 0; i < 3; i++)
				extra->clipPlaneNorm[i] = stream->readFloatLE();

			for (int i = 0; i < 3; i++)
				extra->clipPlanePt[i] = stream->readFloatLE();

			extra->talkColor = stream->readSint32LE();
			extra->timeScale = stream->readFloatLE();
			extra->subClipNode = stream->readSint32LE();

			if (hasPuck) {
				extra->puck = nullptr;
				enablePuck(actor, 1);

				if (actor->extra->inBoxes)
					actor->extra->puck->validate();
			}

			if (actor->extra->talking) {
				for (int i = 0; i < 4; i++) {
					if (!_talkingActors[i]) {
						_talkingActors[i] = actor;
						_numActorsTalking++;
						break;
					}
				}
			}

			if (sayTextSerial)
				actor->extra->sayText = _sputText->findSerialNumber(sayTextSerial);

			resetShadows(actor);

			for (int i = 0; i < 6; i++) {
				ActorShadow *shadow = &actor->extra->shadows[i];

				shadow->active = stream->readSint32LE();
				shadow->side = stream->readSint32LE();

				for (int j = 0; j < 3; j++)
					shadow->normal[j] = stream->readFloatLE();

				shadow->planeD = stream->readFloatLE();

				for (int j = 0; j < 3; j++)
					shadow->lightPos[j] = stream->readFloatLE();

				int numClipPlanes = stream->readSint32LE();

				if (numClipPlanes < 0 || numClipPlanes >= 12) {
					warning("SputActorModule::restoreRemastered(): invalid shadow clip plane count %d", numClipPlanes);
					return 1;
				}

				for (int j = 0; j < numClipPlanes; j++) {
					char planeName[33];

					planeName[0] = 0;
					stream->read(planeName, 32);
					planeName[32] = 0;
					shadow->clipPlane[j] = _walkSystem->planeNamed(planeName);
				}
			}
		}

		if (stream->err() || stream->eos()) {
			warning("SputActorModule::restoreRemastered(): failed to read the actor %d", actorIndex);
			return 1;
		}
	}

	_selectedActor = nullptr;

	int selectedIndex = stream->readSint32LE();

	if (selectedIndex != -1) {
		if (selectedIndex >= 0 && selectedIndex < _maxActors)
			_selectedActor = &_actors[selectedIndex];
		else
			warning("SputActorModule::restoreRemastered(): invalid selected actor %d", selectedIndex);
	}

	return 0;
}

} // End of namespace Grim
