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

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

RdPuppet *RenderDroid::PuppetModule::create(RdThing *thing) {
	RdPuppet *puppet = new RdPuppet;

	memset(puppet, 0, sizeof(RdPuppet));
	if (!newEntry(puppet, thing))
		return nullptr;

	thing->puppet = puppet;
	return puppet;
}

int RenderDroid::PuppetModule::newEntry(RdPuppet *puppet, RdThing *thing) {
	puppet->paused = 0;
	puppet->thing = thing;

	for (int i = 0; i < _numTracks; i++) {
		puppet->tracks[i].curFrame = 0.0f;
		puppet->tracks[i].prevFrame = 0.0f;
		removeTrack(puppet, i);
	}

	return 1;
}

int RenderDroid::PuppetModule::addTrack(RdPuppet *puppet, RdKeyframe *keyframe, int priority, int priorityMasked) {
	int slot = 0;

	while (slot < _numTracks && puppet->tracks[slot].status != 0)
		slot++;

	if (slot >= _numTracks) {
		slot = 0;
		while (slot < _numTracks && ((puppet->tracks[slot].status & 8) == 0 || (puppet->tracks[slot].status & 0x140) != 0))
			slot++;

		if (slot >= _numTracks)
			return -1;

		removeTrack(puppet, slot);
	}

	RdPuppetTrack *track = &puppet->tracks[slot];
	track->keyframe = keyframe;
	track->priority = priority;
	track->fps = keyframe->fps;
	track->fade = 0.0f;
	track->priorityMasked = priorityMasked;
	track->status |= 1;
	resetTrack(puppet, slot);

	return slot;
}

int RenderDroid::PuppetModule::removeTrack(RdPuppet *puppet, int trackNum) {
	RdPuppetTrackCallback callback = puppet->tracks[trackNum].callback;

	if (callback)
		callback(puppet->thing->userArg, trackNum, 0);

	puppet->tracks[trackNum].status = 0;
	puppet->tracks[trackNum].keyframe = nullptr;
	puppet->tracks[trackNum].callback = nullptr;

	return 1;
}

int RenderDroid::PuppetModule::setStatus(RdPuppet *puppet, int trackNum, uint status) {
	puppet->tracks[trackNum].status |= status;
	return 1;
}

void RenderDroid::PuppetModule::setCallback(RdPuppet *puppet, int trackNum, RdPuppetTrackCallback callback) {
	puppet->tracks[trackNum].callback = callback;
}

int RenderDroid::PuppetModule::playTrack(RdPuppet *puppet, int trackNum) {
	RdPuppetTrack *track = &puppet->tracks[trackNum];

	track->status = (track->status & 0xFFFFFFED) | 2;
	track->fade = 1.0f;

	return 1;
}

int RenderDroid::PuppetModule::fadeInTrack(RdPuppet *puppet, int trackNum, float time) {
	RdPuppetTrack *track = &puppet->tracks[trackNum];

	track->status = (track->status & 0xFFFFFFF1) | 6;
	if (time <= 0.0)
		track->fadeSpeed = 1.0f;
	else
		track->fadeSpeed = (float)(1.0 / time);

	return 1;
}

int RenderDroid::PuppetModule::fadeOutTrack(RdPuppet *puppet, int trackNum, float time) {
	RdPuppetTrack *track = &puppet->tracks[trackNum];

	track->status = (track->status & 0xFFFFFFF3) | 8;
	if (time <= 0.0)
		track->fadeSpeed = 1.0f;
	else
		track->fadeSpeed = (float)(1.0 / time);

	return 1;
}

void RenderDroid::PuppetModule::advanceTrack(RdPuppet *puppet, int trackNum, float delta) {
	RdPuppetTrack *track = &puppet->tracks[trackNum];
	RdKeyframe *keyframe = track->keyframe;
	float wraps = 0.0f;
	int finished = 0;

	if (!keyframe || delta == 0.0)
		return;

	float newFrame = (float)((double)delta + track->prevFrame);
	track->curFrame = newFrame;

	double numFrames = (double)keyframe->numFrames;

	if (_rd->_remastered)
		numFrames = (float)keyframe->numFrames;

	if (newFrame >= numFrames) {
		uint status = track->status;

		if (status & 0x20) {
			track->curFrame = (float)numFrames;
			finished = 1;
		} else if (status & 0x40) {
			track->curFrame = (float)numFrames;
			track->status = status | 0x10;
		} else if ((status & 0x80) == 0) {
			if (_rd->_remastered) {
				wraps = (float)floor((double)newFrame / numFrames);
				track->curFrame = (float)((double)newFrame - numFrames * wraps);
			} else {
				float turns = (float)((double)newFrame / numFrames);
				wraps = _rd->_stdMath->rint(turns);
				track->curFrame = (float)((double)track->curFrame - (double)track->keyframe->numFrames * wraps);
			}

			uint numHNodes = puppet->thing->data.model3->numHNodes;
			if (numHNodes >= 64)
				numHNodes = 64;
			memset(track->entryIdx, 0, 4 * numHNodes);
		} else {
			track->curFrame = (float)numFrames;
			fadeOutTrack(puppet, trackNum, 0.25f);
			track->status |= 0x10;
		}
	}

	if (track->callback) {
		keyframe = track->keyframe;

		if (keyframe->numMarkers) {
			if (wraps == 0.0) {
				for (uint i = 0; i < keyframe->numMarkers; i++) {
					if (keyframe->markerFrame[i] > (double)track->curFrame)
						break;

					if (keyframe->markerFrame[i] > (double)track->prevFrame || track->prevFrame == 0.0)
						track->callback(puppet->thing->userArg, trackNum, keyframe->markerType[i]);

					keyframe = track->keyframe;
				}
			} else if (wraps <= 1.0) {
				for (uint i = 0; i < keyframe->numMarkers; i++) {
					if (keyframe->markerFrame[i] > (double)track->prevFrame || keyframe->markerFrame[i] <= (double)track->curFrame)
						track->callback(puppet->thing->userArg, trackNum, keyframe->markerType[i]);

					keyframe = track->keyframe;
				}
			} else {
				for (uint i = 0; i < keyframe->numMarkers; i++) {
					track->callback(puppet->thing->userArg, trackNum, keyframe->markerType[i]);
					keyframe = track->keyframe;
				}
			}
		}
	}

	if (finished)
		removeTrack(puppet, trackNum);
	else
		track->prevFrame = track->curFrame;
}

int RenderDroid::PuppetModule::updateTracks(RdPuppet *puppet, float delta) {
	int activeTracks = 0;

	if (puppet->paused)
		return activeTracks;

	for (int i = 0; i < _numTracks; i++) {
		RdPuppetTrack *track = &puppet->tracks[i];

		if (!track->status)
			continue;

		activeTracks++;
		if (track->status & 0x200)
			continue;

		if ((track->status & 0x10) == 0)
			advanceTrack(puppet, i, (float)((double)track->fps * delta));

		if (track->status & 4) {
			double fade = (double)track->fadeSpeed * delta + track->fade;
			track->fade = (float)fade;

			bool faded;

			if (_rd->_remastered)
				faded = track->fade >= 1.0f;
			else
				faded = !(fade < 1.0);

			if (faded) {
				track->fade = 1.0f;
				track->status &= 0xFFFFFFFB;
			}
		} else if (track->status & 8) {
			double fade = (double)track->fade - (double)track->fadeSpeed * delta;
			track->fade = (float)fade;

			bool faded;

			if (_rd->_remastered)
				faded = track->fade <= 0.0f;
			else
				faded = fade <= 0.0;

			if (faded) {
				if (track->status & 0x100)
					track->status = (track->status & 0xFFFFFFF7) | 0x10;
				else
					removeTrack(puppet, i);
			}
		}
	}

	return activeTracks;
}

int RenderDroid::PuppetModule::resetTrack(RdPuppet *puppet, int trackNum) {
	RdPuppetTrack *track = &puppet->tracks[trackNum];
	uint numHNodes = puppet->thing->data.model3->numHNodes;

	if (numHNodes >= 64)
		numHNodes = 64;

	memset(track->entryIdx, 0, 4 * numHNodes);
	track->curFrame = 0.0f;
	track->prevFrame = 0.0f;
	track->status = 3;

	return 0;
}

void RenderDroid::PuppetModule::buildJointMatrices(RdThing *thing, RdMatrix34 base) {
	RdModel3 *model3 = thing->data.model3;
	RdPuppet *puppet = thing->puppet;

	if (thing->jointsLocked)
		return;

	if (!puppet || puppet->paused) {
		for (uint i = 0; i < model3->numHNodes; i++)
			memcpy(thing->jointMatrices[i], model3->hNodes[i].mat, sizeof(RdMatrix34));
	} else {
		for (int t = 0; t < _numTracks; t++) {
			RdPuppetTrack *track = &puppet->tracks[t];

			if (!track->status)
				continue;

			RdKeyframe *keyframe = track->keyframe;

			for (uint n = 0; n < keyframe->numNodes; n++) {
				RdKeyframeNode *node = &keyframe->paNodes[n];

				if (!node->numEntries)
					continue;

				int index = node->nodeNum >= 0x40 ? 0 : track->entryIdx[node->nodeNum];
				int last = node->numEntries - 1;

				if (index < 0 || index > last) {
					warning("RenderDroid::PuppetModule::buildJointMatrices(): stale entry index %d for node %s", index, node->name);
					continue;
				}

				if (index == last)
					continue;

				int next = index + 1;

				if (track->curFrame >= (double)node->paEntries[next].frame) {
					while (next != last && track->curFrame >= (double)node->paEntries[next + 1].frame)
						next++;

					if (n < 0x40)
						track->entryIdx[n] = next;
				}
			}
		}

		for (uint i = 0; i < model3->numHNodes; i++) {
			RdModel3HNode *hNode = &model3->hNodes[i];
			float topPos[3] = { 0.0f, 0.0f, 0.0f };
			float topPyr[3] = { 0.0f, 0.0f, 0.0f };
			float secondPos[3] = { 0.0f, 0.0f, 0.0f };
			float secondPyr[3] = { 0.0f, 0.0f, 0.0f };
			float topFade = 0.0f;
			float secondFade = 0.0f;
			int topPriority = 0;
			int secondPriority = 0;

			for (int t = 0; t < _numTracks; t++) {
				RdPuppetTrack *track = &puppet->tracks[t];
				RdKeyframe *keyframe = track->keyframe;

				if (!keyframe)
					continue;

				int priority;

				if (keyframe->type & hNode->type)
					priority = track->priorityMasked;
				else
					priority = track->priority;

				if (!(track->status & 2))
					continue;

				if (hNode->num >= keyframe->numNodes)
					continue;

				RdKeyframeNode *node = &keyframe->paNodes[hNode->num];

				if (!node->numEntries)
					continue;

				if (priority < secondPriority)
					continue;

				if (!(priority >= topPriority || topFade < 1.0f))
					continue;

				if (hNode->num >= 0x40) {
					warning("RenderDroid::PuppetModule::buildJointMatrices(): node index %u out of range", hNode->num);
					continue;
				}

				RdKeyframeEntry *entry = &node->paEntries[track->entryIdx[hNode->num]];
				double delta;

				if (keyframe->flags & 0x100)
					delta = 0.0;
				else
					delta = (double)track->curFrame - entry->frame;

				if (_rd->_remastered)
					delta = (float)delta;

				float curPos[3];
				float curPyr[3];

				if (entry->flags & 1) {
					curPos[0] = (float)(entry->dpos[0] * delta + entry->pos[0]);
					curPos[1] = (float)(entry->dpos[1] * delta + entry->pos[1]);
					curPos[2] = (float)(entry->dpos[2] * delta + entry->pos[2]);
				} else {
					curPos[0] = entry->pos[0];
					curPos[1] = entry->pos[1];
					curPos[2] = entry->pos[2];
				}

				if (entry->flags & 2) {
					curPyr[0] = (float)(entry->dpyr[0] * delta + entry->pyr[0]);
					curPyr[1] = (float)(entry->dpyr[1] * delta + entry->pyr[1]);
					curPyr[2] = (float)(entry->dpyr[2] * delta + entry->pyr[2]);
				} else {
					curPyr[0] = entry->pyr[0];
					curPyr[1] = entry->pyr[1];
					curPyr[2] = entry->pyr[2];
				}

				curPos[0] = (float)((double)curPos[0] - hNode->xyz[0]);
				curPos[1] = (float)((double)curPos[1] - hNode->xyz[1]);
				curPos[2] = (float)((double)curPos[2] - hNode->xyz[2]);
				curPyr[0] = (float)((double)curPyr[0] - hNode->pyr[0]);
				curPyr[1] = (float)((double)curPyr[1] - hNode->pyr[1]);
				curPyr[2] = (float)((double)curPyr[2] - hNode->pyr[2]);
				curPyr[0] = _rd->_stdMath->normalizeAngleAcute(curPyr[0]);
				curPyr[1] = _rd->_stdMath->normalizeAngleAcute(curPyr[1]);
				curPyr[2] = _rd->_stdMath->normalizeAngleAcute(curPyr[2]);

				if (track->fade < 1.0f) {
					for (int k = 0; k < 3; k++) {
						curPos[k] = (float)((double)track->fade * curPos[k]);
						curPyr[k] = (float)((double)track->fade * curPyr[k]);
					}
				}

				if (priority == topPriority) {
					for (int k = 0; k < 3; k++) {
						topPos[k] = (float)((double)curPos[k] + topPos[k]);
						topPyr[k] = (float)((double)curPyr[k] + topPyr[k]);
					}

					topFade = (float)((double)topFade + track->fade);
				} else if (priority < topPriority) {
					if (priority <= secondPriority) {
						for (int k = 0; k < 3; k++) {
							secondPos[k] = (float)((double)curPos[k] + secondPos[k]);
							secondPyr[k] = (float)((double)curPyr[k] + secondPyr[k]);
						}

						secondFade = (float)((double)secondFade + track->fade);
					} else {
						for (int k = 0; k < 3; k++) {
							secondPos[k] = curPos[k];
							secondPyr[k] = curPyr[k];
						}

						secondFade = track->fade;
						secondPriority = priority;
					}
				} else {
					int previousTopPriority = topPriority;

					for (int k = 0; k < 3; k++) {
						secondPos[k] = topPos[k];
						secondPyr[k] = topPyr[k];
						topPos[k] = curPos[k];
						topPyr[k] = curPyr[k];
					}

					secondFade = topFade;
					topPriority = priority;
					secondPriority = previousTopPriority;
					topFade = track->fade;
				}
			}

			bool topOnly;

			if (_rd->_remastered)
				topOnly = !(topFade < 1.0f) || !(secondFade > 0.0f);
			else
				topOnly = topFade >= 1.0f || secondFade <= 0.0f;

			if (topOnly) {
				if (topFade > 1.0f) {
					double scale = 1.0 / topFade;

					if (_rd->_remastered)
						scale = (float)scale;

					for (int k = 0; k < 3; k++) {
						topPos[k] = (float)(topPos[k] * scale);
						topPyr[k] = (float)(topPyr[k] * scale);
					}
				}
			} else {
				if (secondFade > 1.0f) {
					double scale = 1.0 / secondFade;

					if (_rd->_remastered)
						scale = (float)scale;

					for (int k = 0; k < 3; k++) {
						secondPos[k] = (float)(secondPos[k] * scale);
						secondPyr[k] = (float)(secondPyr[k] * scale);
					}
				}

				double remaining = 1.0 - topFade;

				if (_rd->_remastered)
					remaining = (float)remaining;

				for (int k = 0; k < 3; k++) {
					topPos[k] = (float)(secondPos[k] * remaining + topPos[k]);
					topPyr[k] = (float)(secondPyr[k] * remaining + topPyr[k]);
				}
			}

			topPyr[0] = _rd->_stdMath->normalizeAngleAcute(topPyr[0]);
			topPyr[1] = _rd->_stdMath->normalizeAngleAcute(topPyr[1]);
			topPyr[2] = _rd->_stdMath->normalizeAngleAcute(topPyr[2]);

			for (int k = 0; k < 3; k++) {
				topPos[k] = (float)((double)hNode->xyz[k] + topPos[k]);
				topPyr[k] = (float)((double)hNode->pyr[k] + topPyr[k]);
			}

			_rd->build34(thing->jointMatrices[i], topPyr, topPos);

			const float *jointPyr = thing->jointPyr[i];

			if (jointPyr[0] != 0.0f || jointPyr[1] != 0.0f || jointPyr[2] != 0.0f)
				_rd->preRotate34(thing->jointMatrices[i], thing->jointPyr[i]);
		}
	}

	_rd->_thing.accumulateMatrices(thing, model3->hNodes, base);
	thing->jointsFrame = _rd->_frameTrue;
}

} // End of namespace Grim
