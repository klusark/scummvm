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

#include "engines/grim/chore/cos.h"
#include "engines/grim/grim.h"
#include "engines/grim/legacy_savegames/legacy_savegames_import.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/res.h"
#include "engines/grim/actor/sputactor.h"
#include "engines/grim/costume/sputcostume_components.h"
#include "engines/grim/sputobjectstate.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/sputtime.h"
#include "engines/grim/std/std_bitmap.h"
#include "engines/grim/text/text.h"
#include "engines/grim/walkplane.h"

namespace Grim {

static Common::String getFontName(Res *res, BlastTextFont *font) {
	ResNode *node = res->checkAllocatedRes(font);

	if (node && node->tag == kSputTagFont)
		return node->name;

	return Common::String();
}

static BlastTextFont *loadFontByName(Res *res, const char *name, BlastTextFont *systemFont) {
	if (name && name[0]) {
		BlastTextFont *font = static_cast<BlastTextFont *>(res->getResource(kSputTagFont, name));

		if (font)
			return font;

		warning("LegacyImporter::setDefaultFonts(): font %s not found, using the system font", name);
	}

	return systemFont;
}

int LegacyImporter::setChoreInstance(Cos *cos, int inst, int choreIndex, uint32 state, int time) {
	int index = cos->instHandleToIndex(inst);

	if (index < 0 || index >= cos->_maxInstances || cos->_instances[index].free) {
		warning("LegacyImporter::setChoreInstance(): invalid instance handle %d", inst);
		return 0;
	}

	CosInstance *instance = &cos->_instances[index];

	if (choreIndex < 0 || choreIndex >= cos->_costumes[instance->costume].numChores) {
		warning("LegacyImporter::setChoreInstance(): invalid chore %d", choreIndex);
		return 0;
	}

	instance->choreInstances[choreIndex].state = state;
	instance->choreInstances[choreIndex].time = time;

	return 1;
}

void LegacyImporter::resetActors(SputActorModule *actors) {
	for (int i = 0; i < 4; i++)
		actors->_talkingActors[i] = nullptr;

	actors->_numActorsTalking = 0;
	actors->_selectedActor = nullptr;

	for (int i = 0; i < actors->_maxActors; i++) {
		if (actors->_actors[i].inUse)
			actors->freeActor(&actors->_actors[i]);
	}

	actors->_numActors = 0;
}

int LegacyImporter::addPuppetTrack(SputActorModule *actors, ModelInst *modelInst, RdKeyframe *keyframe, int priority, int priorityMasked, uint status, float frame, float fade, float fadeSpeed) {
	RdPuppet *puppet = modelInst->thing->puppet;

	if (!puppet) {
		warning("LegacyImporter::addPuppetTrack(): the model has no puppet for the track %s", keyframe->name);
		return -1;
	}

	int trackNum = actors->_rd->_puppet.addTrack(puppet, keyframe, priority, priorityMasked);

	if (trackNum < 0 || trackNum >= 6) {
		warning("LegacyImporter::addPuppetTrack(): the keyframe %s could not be added to the puppet", keyframe->name);
		return -1;
	}

	puppet->tracks[trackNum].status = status;
	puppet->tracks[trackNum].curFrame = frame;
	puppet->tracks[trackNum].prevFrame = frame;
	puppet->tracks[trackNum].fade = fade;
	puppet->tracks[trackNum].fadeSpeed = fadeSpeed;

	return trackNum;
}

void LegacyImporter::beginRoom(SputRoom *sputRoom, Room *room, int curSetup, int roomRef) {
	room->curSetup = curSetup;

	if (sputRoom->_currentRoomRef.valid)
		sputRoom->_res->removeReference(&sputRoom->_currentRoomRef);

	sputRoom->_res->setReference(&sputRoom->_currentRoomRef, room);

	if (!sputRoom->_currentRoomRef.valid)
		error("LegacyImporter::beginRoom(): Bad room Reference");

	sputRoom->loadResources(static_cast<Room *>(sputRoom->_currentRoomRef.handle));
	Common::strlcpy(sputRoom->_currentRoomName, room->name, 32);

	if (roomRef)
		sputRoom->_res->setReference(&room->roomRef, room);
}

SputObjectState *LegacyImporter::addRoomObjectState(SputRoom *sputRoom, int setupIndex, int objectType, const char *bitmapName, const char *zbufferName, int colorFrame, int zFrame) {
	SputObjectState *state = sputRoom->addObjectState(setupIndex, objectType, bitmapName, zbufferName, -1);

	if (!state)
		return nullptr;

	if (state->colorImage.bitmapRef.valid) {
		StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(state->colorImage.bitmapRef.handle);

		if (colorFrame >= (int)bitmap->numFrames) {
			warning("LegacyImporter::addRoomObjectState(): frame %d of %s is out of range", colorFrame, bitmapName);
			colorFrame = -1;
		}

		bitmap->curFrame = colorFrame;
	}

	if (state->zImage.bitmapRef.valid) {
		StdBitmapStruct *bitmap = static_cast<StdBitmapStruct *>(state->zImage.bitmapRef.handle);

		if (zFrame >= (int)bitmap->numFrames) {
			warning("LegacyImporter::addRoomObjectState(): frame %d of %s is out of range", zFrame, zbufferName);
			zFrame = -1;
		}

		bitmap->curFrame = zFrame;
	}

	return state;
}

void LegacyImporter::setCurrentRoom(SputRoom *sputRoom, Room *room, byte minVol, byte maxVol, float maxDist, float panFactor, float maxRadius) {
	sputRoom->setCurrentRoom(room, false);

	sputRoom->_walkSystem->setCollection(room->name);

	room->minVol = minVol;
	room->maxVol = maxVol;
	room->maxDist = maxDist;
	room->panFactor = panFactor;
	room->maxRadius = maxRadius;
}

void LegacyImporter::getDefaultFontNames(SputText *sputText, Common::String *printLine, Common::String *defaultValue, Common::String *blastText) {
	*printLine = getFontName(sputText->_res, sputText->_printLineParams.font);
	*defaultValue = getFontName(sputText->_res, sputText->_defaultTextParams.font);
	*blastText = getFontName(sputText->_res, sputText->_blastTextParams.font);
}

void LegacyImporter::setDefaultFonts(SputText *sputText, const char *printLineFont, const char *defaultFont, const char *blastTextFont) {
	while (sputText->_textQueue)
		sputText->killEvent(sputText->_textQueue);

	sputText->_font = sputText->_text->getSystemFont();
	sputText->_printLineParams.font = loadFontByName(sputText->_res, printLineFont, sputText->_font);
	sputText->_defaultTextParams.font = loadFontByName(sputText->_res, defaultFont, sputText->_font);
	sputText->_blastTextParams.font = loadFontByName(sputText->_res, blastTextFont, sputText->_font);
}

void LegacyImporter::setSayLineDefaults(SputText *sputText, int x, int y, int fgColor, int justify) {
	int baseLine = 0;

	if (sputText->_defaultTextParams.font)
		baseLine = sputText->_defaultTextParams.font->baseLine;

	sputText->_defaultTextParams.x = x;
	sputText->_defaultTextParams.y = y + baseLine;
	sputText->_defaultTextParams.fgColor = fgColor;

	if (justify == 1)
		sputText->_defaultTextParams.flags = 5;
	else if (justify == 2)
		sputText->_defaultTextParams.flags = 4;
	else if (justify == 3)
		sputText->_defaultTextParams.flags = 6;
}

void LegacyImporter::setRenderState(SputRender *sputRender, int irisMode, int irisCenterX, int irisCenterY, int irisDuration, int irisElapsed, int camActorIndex) {
	sputRender->_irisStart = 0;
	sputRender->_irisDuration = 0.0f;
	sputRender->_irisCenterX = 320;
	sputRender->_irisCenterY = 240;
	sputRender->_renderFlags = 0;
	sputRender->_pageFlipEnable = 1;
	sputRender->_movieJustFinished = 0;
	sputRender->_camActorIndex = camActorIndex;

	if (irisMode == 1) {
		sputRender->irisDown(320, 240, 1);
		sputRender->_irisStart = sputRender->_sputTime->getGameTime() - 2;
	} else if (irisMode == 2 || irisMode == 3) {
		if (irisMode == 2)
			sputRender->irisDown(irisCenterX, irisCenterY, irisDuration);
		else
			sputRender->irisUp(irisCenterX, irisCenterY, irisDuration);

		sputRender->_irisStart = sputRender->_sputTime->getGameTime() - irisElapsed;
	}
}

void LegacyImporter::startMovie(SputSmush *sputSmush, const char *name, int looping, int x, int y) {
	sputSmush->setTarget(sputSmush->_mainGfxBuffer);

	sputSmush->_playing = 0;
	sputSmush->_paused = 0;
	sputSmush->_abort = 0;
	sputSmush->_looping = 0;
	sputSmush->_x = 0;
	sputSmush->_y = 0;
	memset(sputSmush->_curMovieFile, 0, sizeof(sputSmush->_curMovieFile));

	if (name)
		sputSmush->startMovie(name, looping, x, y);
}

void LegacyImporter::getEngineRefs(LegacyLuaRefs *refs) {
	Lua_V1 *lua = _grim->_luaV1;
	SputText *sputText = _grim->_sputText;

	refs->systemTable = lua->_refSystemTable;
	refs->typeOverride = lua->_refTypeOverride;
	refs->oldConcatFallback = lua->_refOldConcatFallback;
	refs->textObject[0] = sputText->_luaTextObjXRef;
	refs->textObject[1] = sputText->_luaTextObjYRef;
	refs->textObject[2] = sputText->_luaTextObjFontRef;
	refs->textObject[3] = sputText->_luaTextObjWidthRef;
	refs->textObject[4] = sputText->_luaTextObjHeightRef;
	refs->textObject[5] = sputText->_luaTextObjFGColorRef;
	refs->textObject[6] = sputText->_luaTextObjBGColorRef;
	refs->textObject[7] = sputText->_luaTextObjFXColorRef;
	refs->textObject[8] = sputText->_luaTextObjHIColorRef;
	refs->textObject[9] = sputText->_luaTextObjDurationRef;
	refs->textObject[10] = sputText->_luaTextObjCenterRef;
	refs->textObject[11] = sputText->_luaTextObjLJustifyRef;
	refs->textObject[12] = sputText->_luaTextObjRJustifyRef;
	refs->textObject[13] = sputText->_luaTextObjVolumeRef;
	refs->textObject[14] = sputText->_luaTextObjBufferRef;
	refs->textObject[15] = sputText->_luaTextObjBackgroundRef;
}

} // End of namespace Grim
