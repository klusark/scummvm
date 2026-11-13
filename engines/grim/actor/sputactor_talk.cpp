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

#include "common/file.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/chore/cos.h"
#include "engines/grim/grim.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/std/std_sys.h"

namespace Grim {

void SputActorModule::sayLineDemo(SputActor *actor, const char *msgId) {
	int speechText = _speechMode & 1;
	int speechVoice = _speechMode & 2;
	char translation[32];
	char voiceFile[36];

	if (!actor->extra)
		allocateActorExtra(actor);

	Extra *extra = actor->extra;

	translation[0] = 0;
	voiceFile[0] = 0;

	const char *text = _sputText->extractSayLineData(msgId, translation);

	if (translation[0]) {
		static const char *const extensions[4] = { "wvc", "wav", "imc", "imu" };

		for (int i = 0; i < 4 && !voiceFile[0]; i++) {
			Common::String fileName = Common::String::format("%s.%s", translation, extensions[i]);
			Common::SeekableReadStream *file = _hs->openFile(fileName.c_str());

			if (file) {
				delete file;
				Common::strlcpy(voiceFile, fileName.c_str(), sizeof(voiceFile));
			}
		}
	}

	if (extra->talking) {
		if (translation[0]) {
			char currentId[13];

			memcpy(currentId, extra->sayLineId, 12);
			currentId[12] = 0;

			if (!strcmp(currentId, translation))
				return;
		}

		shutUpDemo(actor);
	}

	extra->talkTimerMs = 0;
	extra->talking = 1;
	strncpy(extra->sayLineId, translation, 12);
	_numActorsTalking++;
	extra->phoneme = 0;

	if (speechVoice) {
		if (voiceFile[0]) {
			g_grim->getImuse()->soundStartVoiceStream(voiceFile, 127, 64);
			Common::strlcpy(extra->voiceName, voiceFile, sizeof(extra->voiceName));
		} else {
			extra->voiceName[0] = 0;
		}

		extra->talkTimerMs = -1;
	}

	if (speechText || !extra->voiceName[0]) {
		SputTextEvent *event = _sputText->createActorTextDemo(actor, text);

		extra->sayText = event;

		if (!extra->voiceName[0])
			extra->talkTimerMs = 48 * (int)strlen(text) + 640;

		if (event)
			_sputText->addToQueue(event, -1, 5);
	}

	// There is no lip sync data in the demo, so the mumble chore always plays
	CostumeNode *node = actor->costumeStack;

	if (node) {
		RdThing *headThing = node->headThing;

		do {
			if (node->headThing != headThing)
				break;

			if (node->mumbleChore >= 0)
				break;

			node = node->next;
		} while (node);

		if (node && node->headThing == headThing && node->mumbleChore >= 0)
			_cos->instPlayChore(node->costumeInstance, node->mumbleChore, 1, 0);
	}
}

void SputActorModule::shutUpDemo(SputActor *actor) {
	if (!actor) {
		warning("SputActorModule::shutUpDemo(): no actor");
		return;
	}

	CostumeNode *node = actor->costumeStack;
	Extra *extra = actor->extra;

	if (!node || !extra)
		return;

	if (extra->voiceName[0])
		g_grim->getImuse()->soundStopVoiceStream();

	if (extra->sayText)
		_sputText->killEvent(extra->sayText);

	RdThing *headThing = node->headThing;

	do {
		if (node->headThing != headThing)
			break;

		if (node->mumbleChore >= 0)
			break;

		node = node->next;
	} while (node);

	if (node && node->headThing == headThing && node->mumbleChore >= 0 && _cos->instChoreStatus(node->costumeInstance, node->mumbleChore))
		_cos->instStopChore(node->costumeInstance, node->mumbleChore);

	node = actor->costumeStack;
	headThing = node->headThing;

	do {
		if (node->headThing != headThing)
			break;

		if (node->chore[0] >= 0)
			break;

		node = node->next;
	} while (node);

	if (node && node->headThing == headThing && node->chore[0] >= 0)
		_cos->instCompleteChore(node->costumeInstance, node->chore[0]);

	extra->phoneme = 0;
	extra->voiceName[0] = 0;
	extra->sayText = nullptr;
	extra->sayLineId[0] = 0;
	extra->talking = 0;

	if (_numActorsTalking > 0)
		_numActorsTalking--;
}

int SputActorModule::isVoicePlayingDemo(SputActor *actor) {
	return actor->extra->voiceName[0] && g_grim->getImuse()->imGetParam(kTalkSoundID, IMUSE_P_SND_TRACK_NUM) > 0;
}

void SputActorModule::stopActorDemo(SputActor *actor) {
	CostumeNode *node = actor->costumeStack;

	if (node) {
		if (node->rCostume.valid)
			_cos->instRescue(node->costumeInstance, node->rCostume.serial);

		_sputCostume->resetCostume(node->costumeInstance);
	}

	if (actor->extra && actor->extra->talking)
		shutUpDemo(actor);
}

void SputActorModule::updateSpeechOnly(float delta) {
	if (_grim->getGameFlags() & ADGF_DEMO)
		return;

	for (int i = 0; i < 4; i++) {
		if (_talkingActors[i])
			updateActorSpeech(_talkingActors[i], delta);
	}
}

void SputActorModule::updateActorSpeech(SputActor *actor, float delta) {
	Extra *extra = actor->extra;

	if (!extra->talking)
		return;

	if (extra->voiceName[0] && extra->phoneme && actor->costumeStack) {
		int phoneme = g_grim->getImuse()->iMUSEGetPhoneme(extra->soundId, 0);

		if (phoneme) {
			int mouthChore = matchPhonemeWithAnim(actor, phoneme);
			CostumeNode *node = actor->costumeStack;
			RdThing *headThing = node->headThing;

			do {
				if (node->headThing != headThing)
					break;

				if (node->chore[mouthChore] >= 0)
					break;

				node = node->next;
			} while (node);

			if (node && node->headThing == headThing)
				_cos->instCompleteChore(node->costumeInstance, node->chore[mouthChore]);
		} else {
			shutUp(actor);
		}
	}

	double elapsed = (double)delta * -1000.0;

	if (elapsed > -9.0e18 && elapsed < 9.0e18)
		actor->extra->talkTimerMs += (int)(int64)elapsed;
	else
		warning("SputActorModule::updateActorSpeech(): the time delta is out of range");

	if ((actor->extra->voiceName[0] && !isVoicePlaying(actor)) || (!actor->extra->voiceName[0] && actor->extra->talkTimerMs < 0))
		shutUp(actor);
}

int SputActorModule::matchPhonemeWithAnim(SputActor *actor, int phoneme) {
	if (phoneme == 95)
		return 0;

	LipSyncEntry key;

	key.phoneme = phoneme;
	key.mouthAnim = 0;

	LipSyncEntry *entry = (LipSyncEntry *)bsearch(&key, _lipSyncAnims, _numLipSyncAnims, sizeof(LipSyncEntry), compareAnimEntry);

	if (entry)
		return entry->mouthAnim;

	return 1;
}

int SputActorModule::compareAnimEntry(const void *key, const void *entry) {
	return *(const int *)key - *(const int *)entry;
}

void SputActorModule::sayLine(SputActor *actor, const char *msgId, int voiceStream, int volume, int pan, int x, int y) {
	if (!actor->extra)
		allocateActorExtra(actor);

	int speechText = _speechMode & 1;
	int speechVoice = _speechMode & 2;
	char translation[32];

	translation[0] = 0;

	const char *text = _sputText->extractSayLineData(msgId, translation);
	char voiceFile[36];

	voiceFile[0] = 0;

	if (translation[0]) {
		static const char *const extensions[4] = { "wvc", "wav", "imc", "imu" };
		Common::String baseNames[2];
		int numBaseNames = 0;
		bool found = false;

		// The remastered has a voice per language: the prefixed name first, then the plain one
		if (g_grim->isRemastered())
			baseNames[numBaseNames++] = Common::String::format("%s_%s", g_grim->getLanguageCode(), translation);

		baseNames[numBaseNames++] = translation;

		for (int n = 0; n < numBaseNames && !found; n++) {
			for (int i = 0; i < 4 && !found; i++) {
				Common::String fileName = Common::String::format("%s.%s", baseNames[n].c_str(), extensions[i]);
				Common::SeekableReadStream *file = _hs->openFile(fileName.c_str());

				if (file) {
					delete file;
					Common::strlcpy(voiceFile, fileName.c_str(), sizeof(voiceFile));
					found = true;
				}
			}
		}

		if (!found)
			voiceFile[0] = 0;
	}

	if (actor->extra->talking) {
		if (translation[0]) {
			char currentId[13];

			memcpy(currentId, actor->extra->sayLineId, 12);
			currentId[12] = 0;

			if (!strcmp(currentId, translation))
				return;
		}

		shutUp(actor);
	}

	actor->extra->talkTimerMs = 0;
	actor->extra->talking = 1;
	strncpy(actor->extra->sayLineId, translation, 12);
	_numActorsTalking++;

	for (int i = 0; i < 4; i++) {
		if (!_talkingActors[i]) {
			_talkingActors[i] = actor;
			break;
		}
	}

	if (speechVoice) {
		if (voiceFile[0]) {
			if (!voiceStream) {
				actor->extra->soundId = g_grim->getImuse()->soundsLoadProcessStartSound(voiceFile, 127, 2);

				if (!actor->extra->soundId) {
					actor->extra->voiceName[0] = 0;
				} else {
					g_grim->getImuse()->soundsSetParam((const char *)(intptr)actor->extra->soundId, IMUSE_P_VOLUME, volume);

					if (actor->flags & 8)
						_sputRoom->setSoundPosition((const char *)(intptr)actor->extra->soundId, actor->actorPos, 20, 127, 8.0f, 0.80000001f);

					Common::strlcpy(actor->extra->voiceName, voiceFile, sizeof(actor->extra->voiceName));
					actor->extra->phoneme = g_grim->getImuse()->iMUSEGetPhoneme(actor->extra->soundId, 0);
				}
			} else if (!g_grim->getImuse()->soundStartVoiceStream(voiceFile, volume, pan)) {
				actor->extra->soundId = kTalkSoundID;

				if (actor->flags & 8)
					_sputRoom->setSoundPosition((const char *)(intptr)kTalkSoundID, actor->actorPos, 20, 127, 8.0f, 0.80000001f);

				Common::strlcpy(actor->extra->voiceName, voiceFile, sizeof(actor->extra->voiceName));
				actor->extra->phoneme = g_grim->getImuse()->iMUSEGetPhoneme(actor->extra->soundId, 0);
			} else {
				actor->extra->voiceName[0] = 0;
			}
		} else {
			actor->extra->voiceName[0] = 0;
		}

		actor->extra->talkTimerMs = -1;
	}

	if (speechText || !actor->extra->voiceName[0]) {
		SputTextEvent *event = _sputText->createActorText(actor, text, x, y);

		actor->extra->sayText = event;

		if (!actor->extra->voiceName[0])
			actor->extra->talkTimerMs = 15 * (11 - _textRate) * (int)strlen(text) + 640;

		if (event)
			_sputText->addToQueue(event, -1, 13);

		if (voiceStream) {
			for (int i = 0; i < 4; i++) {
				SputActor *other = _talkingActors[i];

				if (other && other != actor && other->extra->sayText)
					other->extra->sayText->flags |= 2;
			}
		} else {
			for (int i = 0; i < 4; i++) {
				SputActor *other = _talkingActors[i];

				if (other && other != actor && other->extra->sayText && other->extra->soundId == kTalkSoundID) {
					if (actor->extra->sayText)
						actor->extra->sayText->flags |= 2;

					break;
				}
			}
		}
	}

	Extra *extra = actor->extra;
	CostumeNode *node = actor->costumeStack;

	if (node && extra->phoneme) {
		RdThing *headThing = node->headThing;

		do {
			if (node->headThing != headThing)
				break;

			int i = 1;

			while (i <= 9 && node->chore[i] < 0)
				i++;

			if (i <= 9)
				break;

			node = node->next;
		} while (node);

		if (!node || node->headThing != headThing)
			extra->phoneme = 0;
	}

	if (!actor->extra->voiceName[0] || !actor->extra->phoneme) {
		node = actor->costumeStack;

		if (node) {
			RdThing *headThing = node->headThing;

			do {
				if (node->headThing != headThing)
					break;

				if (node->mumbleChore >= 0)
					break;

				node = node->next;
			} while (node);

			if (node && node->headThing == headThing) {
				if (node->mumbleChore >= 0)
					_cos->instPlayChore(node->costumeInstance, node->mumbleChore, 1, 0);
			}
		}
	}
}

void SputActorModule::shutUp(SputActor *actor) {
	if (!actor)
		return;

	Extra *extra = actor->extra;

	if (!extra)
		return;

	if (extra->voiceName[0]) {
		if (extra->soundId == kTalkSoundID)
			g_grim->getImuse()->soundStopVoiceStream();
		else
			g_grim->getImuse()->soundsStopSound((const char *)(intptr)extra->soundId);
	}

	if (actor->extra->sayText)
		_sputText->killEvent(actor->extra->sayText);

	CostumeNode *node = actor->costumeStack;

	if (node) {
		RdThing *headThing = node->headThing;

		do {
			if (node->headThing != headThing)
				break;

			if (node->mumbleChore >= 0)
				break;

			node = node->next;
		} while (node);

		if (node && node->headThing == headThing && node->mumbleChore >= 0 && _cos->instChoreStatus(node->costumeInstance, node->mumbleChore))
			_cos->instStopChore(node->costumeInstance, node->mumbleChore);

		node = actor->costumeStack;
		headThing = node->headThing;

		do {
			if (node->headThing != headThing)
				break;

			if (node->chore[0] >= 0)
				break;

			node = node->next;
		} while (node);

		if (node && node->headThing == headThing && node->chore[0] >= 0)
			_cos->instCompleteChore(node->costumeInstance, node->chore[0]);
	}

	for (int i = 0; i < 4; i++) {
		if (_talkingActors[i] == actor)
			_talkingActors[i] = nullptr;
	}

	actor->extra->phoneme = 0;
	actor->extra->voiceName[0] = 0;
	actor->extra->sayText = nullptr;
	actor->extra->sayLineId[0] = 0;
	actor->extra->talking = 0;

	if (_numActorsTalking > 0)
		_numActorsTalking--;
}

int SputActorModule::isVoicePlaying(SputActor *actor) {
	Extra *extra = actor->extra;

	if (!extra->voiceName[0])
		return 0;

	if (extra->soundId == kTalkSoundID)
		return g_grim->getImuse()->imGetParam(kTalkSoundID, IMUSE_P_SND_TRACK_NUM) > 0;

	return g_grim->getImuse()->soundsIsSoundPlaying(extra->voiceName);
}

} // End of namespace Grim
