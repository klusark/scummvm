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

#include "common/algorithm.h"
#include "common/util.h"

#include "engines/grim/actor/sputactor.h"
#include "engines/grim/grim.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/remastered/sputmouse.h"
#include "engines/grim/render/primitivequeue.h"
#include "engines/grim/render/sputprimitive.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/sputtime.h"

namespace Grim {

// Layers of the overlays and pages of the text that the remastered scripts use
enum {
	kOverlayBack = 1,
	kOverlayUi = 2,
	kOverlayOvertext = 3,
	kOverlayCursor = 4,
	kOverlayMotherDuck = 5,
	kOverlayMenuBack = 11,
	kOverlayMenuUi = 12,
	kOverlayDialogUi = 22,
	kOverlayMovieUi = 32,
	kOverlayBorders = 40,
	kTextPageDefault = 0,
	kTextPageMenu = 1,
	kTextPageMasked = 2,
	kTextPageDialog = 3,
	kTextPageMotherDuck = 4
};

// The overlays that have the whole canvas for themselves do not follow the 4:3 area
enum {
	kBoxingNone = 0,
	kBoxingFull = 1
};

// Rows of the 640x480 software surface that the movies and the menus black out
void SputRender::clearSceneBuffer() {
	if (!_backBuffer->pixels)
		return;

	_stdDisplay->vBufferLock(_backBuffer);
	memset(_backBuffer->pixels, 0, (size_t)_backBuffer->tex.pitchInBytes * _backBuffer->tex.height);
	_stdDisplay->vBufferUnlock(_backBuffer);
}

// The 3D scene is not drawn in the menus, and not in the movies unless a movie is playing
bool SputRender::disable3DRendering() {
	int mode = _grim->getLibMode();

	return (mode == 3 && !_sputSmush->moviePlaying()) || mode == GrimEngine::kLibModeMenu;
}

void SputRender::updateTextPage(int force, int page) {
	_sputText->update(force, page);
}

// The movie frame of the remastered: true when the movie just finished and the frame goes on as a normal one
bool SputRender::updateMovieRemastered() {
	OverlayManager *overlays = _grim->getHdCanvas()->getOverlays();

	_sputSmush->setTarget(_backBuffer);

	int movieFinished = _sputSmush->periodic();

	if (movieFinished) {
		_grim->setLibMode(GrimEngine::kLibModeNormal);
		_movieJustFinished = 1;
	}

	// The scene buffer is black behind a movie in its first two frames of the whole run and in the frame where a
	// movie ends. The counter is never reset.
	if (_clearedBehindMovie < 2 || _movieJustFinished) {
		StdRect rect;

		rect.x = 0;
		rect.y = 0;
		rect.width = 640;
		rect.height = 480;

		for (int i = 0; i < _numPages; i++)
			markDirtyRectInternal(&rect, 1, i);

		clearSceneBuffer();
		forceRefresh();
		_clearedBehindMovie++;
	}

	_sputActor->updateSpeechOnly(_sputTime->getFrameTimeFlex());
	updateTextPage(0, kTextPageDefault);
	overlays->render(kOverlayBorders, kBoxingNone);
	overlays->render(kOverlayMovieUi, kBoxingNone);
	updateTextPage(1, kTextPageMenu);

	SputMouse *mouse = _grim->getSputMouse();

	if (mouse && mouse->isVisibleInMovie()) {
		mouse->update();
		overlays->render(kOverlayCursor, kBoxingNone);
	}

	_irisDuration = 0.0f;

	return _movieJustFinished != 0;
}

// The layers drawn over the scene in a normal or menu frame, in the order of the original
void SputRender::drawOverlayLayers(int mode) {
	HdCanvas *canvas = _grim->getHdCanvas();
	OverlayManager *overlays = canvas->getOverlays();

	if (_grim->getSputMouse())
		_grim->getSputMouse()->update();

	if (mode != GrimEngine::kLibModeMenu)
		overlays->render(kOverlayBorders, kBoxingNone);

	overlays->render(kOverlayBack, kBoxingNone);
	overlays->render(kOverlayUi, kBoxingNone);
	updateTextPage(1, kTextPageDefault);
	overlays->render(kOverlayOvertext, kBoxingNone);

	if (mode == GrimEngine::kLibModeMenu) {
		clearSceneBuffer();
		canvas->clearBuffer();
		overlays->render(kOverlayMenuBack, kBoxingFull);
		drawPrimitivesCanvas(kPrimitiveLayerMenu);
		updateTextPage(1, kTextPageMasked);
		overlays->render(kOverlayMenuUi, kBoxingFull);
		updateTextPage(1, kTextPageMenu);
		drawPrimitivesCanvas(kPrimitiveLayerDialog);
		overlays->render(kOverlayDialogUi, kBoxingFull);
		updateTextPage(1, kTextPageDialog);
	}

	drawLastLayers();
}

// The layers that every frame with a render mode of the menus has: the loading screen and the cursor
void SputRender::drawLastLayers() {
	OverlayManager *overlays = _grim->getHdCanvas()->getOverlays();

	overlays->render(kOverlayMotherDuck, kBoxingFull);
	updateTextPage(1, kTextPageMotherDuck);
	overlays->render(kOverlayCursor, kBoxingNone);
}

// The frame when no room is set: the loading screen of the boot
void SputRender::drawNoRoomRemastered() {
	OverlayManager *overlays = _grim->getHdCanvas()->getOverlays();

	clearSceneBuffer();
	overlays->render(kOverlayMotherDuck, kBoxingFull);
	updateTextPage(1, kTextPageMotherDuck);
}

// The filled rectangles of a pass are also put on the canvas, in its own pixels and in the order of the
// original. The other rectangles, the lines and the polygons are only drawn in the 640x480 surface.
void SputRender::drawPrimitivesCanvas(int layerMask) {
	if (_skipPrimitives)
		return;

	HdCanvas *canvas = _grim->getHdCanvas();

	for (SputPrimitiveObject *object = _sputPrimitive->getCurrent()->getHead(); object; object = object->next) {
		if (!(layerMask & object->layer) || object->shape != kSputPrimitiveRect || !object->filled)
			continue;

		int left = MIN(object->x1, object->x2);
		int top = MIN(object->y1, object->y2);
		int right = MAX(object->x1, object->x2);
		int bottom = MAX(object->y1, object->y2);

		canvas->queueFilledRectangle(left, top, right, bottom, object->colorRgb, object->alpha);
	}
}

} // End of namespace Grim
