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

#include "common/fs.h"
#include "common/path.h"
#include "common/str.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/images/ctgafile.h"
#include "engines/grim/emi/graphics/dimregion.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/input/demoreplay.h"
#include "engines/grim/emi/input/inputrecorder.h"
#include "engines/grim/emi/images/labitmap.h"
#include "engines/grim/emi/graphics/layer.h"
#include "engines/grim/emi/graphics/lcolorformat.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/core/ltimeservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/movie/movie.h"
#include "engines/grim/emi/scene/overworld.h"
#include "engines/grim/emi/graphics/primitivequeue_v2.h"
#include "engines/grim/emi/core/profilertimer.h"
#include "engines/grim/emi/resources/resourcemanager.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/emi/input/sputcontrol_v2.h"
#include "engines/grim/emi/text/textevent.h"
#include "engines/grim/emi/files/zfile.h"

namespace Grim {

static const int kPrimitiveLayers = 2;
static const float kMaxFrameTime = 0.125f;
static const float kMaxFrameTimePs2 = 0.066666998f;
static const float kMaxFrameTimeDemo = 0.066666998f;
static const double kMillisecondsToSeconds = 0.001f;
static const int kStatsTextX = 10;
static const int kStatsTextFirstY = 20;
static const int kStatsTextRowHeight = 20;
static const byte kStatsTextJustify = 16;
static const int kNumStatsEventsDemo = 4;
static const int kStatsTextDemoTexY = 100;
static const int kMovieControlSkipFrames = 5;
static const int kReplayControlSkipFrames = 3;
static const float kMaxReplayRealTime = 120.0f;

bool EmiEngine::runFrame() {
	updateManual(_drawContext, true, 0);

	return !_quit;
}

int EmiEngine::updateManual(CDrawContext *drawContext, bool draw, int manualMs) {
	// PS2 updates sounds once per frame, before Lua
	bool isPs2 = getGamePlatform() == Common::kPlatformPS2;

	_updateManualTimer.start();

	if (!isPs2)
		updateSounds(false);

	// The demo updates the resources first, without the frame time
	if (_isDemo) {
		_gameResourceManager->update(0.0f);
		updateSounds(false);
	}

	if (_libSaveRequest)
		saveGame(drawContext);

	if (_libLoadRequest)
		loadGame(drawContext);

	// The disasm does not stop the timer here
	if (_mode == kModeNone)
		return 0;

	// Not in the disasm, the options changed from the ScummVM menu
	applyPendingGameSettings(manualMs);

	float frameTime;

	if (isPs2) {
		if (_demoReplay && _demoReplay->_mode == DemoReplay::kModeFinished)
			setReplayRequest(0, nullptr);

		frameTime = updateFrameTime(manualMs);

		if (!_demoReplay || _demoReplay->_mode == DemoReplay::kModeRecord) {
			if (_controlSkipFrames != 0)
				_controlSkipFrames--;
			else
				_control->update();
		} else {
			_demoReplay->deliverEvents();
		}

		if (_replayRequest != -1) {
			applyReplayRequest();
			_controlSkipFrames = kReplayControlSkipFrames;
		}
	} else {
		if (_inputRecorder && _inputRecorder->_mode == InputRecorder::kModeReplay)
			_inputRecorder->replayKeys();
		else if (_controlSkipFrames != 0)
			_controlSkipFrames--;
		else
			_control->update();

		frameTime = updateFrameTime(manualMs);
	}

	if (!_isDemo)
		_gameResourceManager->update(frameTime);

	if (!isPs2)
		updateSounds(false);

	if (_mode != kModePaused || manualMs != 0) {
		_frameCount++;

		// PS2 applies the music state requested by the previous frame before Lua runs
		if (isPs2) {
			updateSounds(true);
			applyPendingMusicState();
		}

		_lua->update(frameTime);
		syncMovementOption();

		if (!isPs2)
			updateSounds(false);

		// PS2 plays the requested movie here and skips the rest of the frame
		if (isPs2 && Movie::isMovieRequestedPs2(this)) {
			Movie::playRequestedMoviePs2(this);
			_controlSkipFrames = kMovieControlSkipFrames;
			_updateManualTimer.stop();

			return 0;
		}

		if (isPs2 && _soundRestartFrames != 0) {
			_soundRestartFrames--;

			if (_soundRestartFrames == 0)
				_sounds->resumeAll();
		}

		if (_mode == kModeMovie) {
			if (updateMovie(frameTime))
				_mode = kModeGame;
		} else if (_mode != kModeOverworld) {
			Actor::updateAllActors(this, frameTime);

			if (!isPs2)
				updateSounds(false);
		}

		if (isPs2 && _setChangeFrames > 0) {
			_setChangeFrames++;

			if (_setChangeFrames == 3)
				_setChangeFrames = 0;
		}
	}

	if (!isPs2) {
		applyPendingMusicState();
		updateSounds(false);
	}

	int result = 0;

	if (draw && (_mode != kModePaused || manualMs != 0))
		result = renderUpdate(drawContext);

	_updateManualTimer.stop();
	drawStatsOverlay(drawContext);

	if (_dumpingFrames)
		dumpFrame();

	return result;
}

void EmiEngine::applyPendingMusicState() {
	if (_pendingMusicState == -1)
		return;

	// After a replay change the new music state waits a few frames
	if (_replayMusicFrames != 0 && _pendingMusicState != 0) {
		_replayMusicFrames--;

		return;
	}

	setMusicState(_pendingMusicState, -1);

	// On PS2, a new music state fades out the loading loop
	if (getGamePlatform() == Common::kPlatformPS2 && _pendingMusicState != 0)
		_sounds->fadeLoadingLoop(-1);

	_pendingMusicState = -1;
}

float EmiEngine::updateFrameTime(int manualMs) {
	LTimeStamp stamp;

	stamp.stamp();

	if (_inputRecorder && _inputRecorder->_mode == InputRecorder::kModeReplay) {
		_frameTime = _inputRecorder->replayFrame();

		return _frameTime;
	}

	if (_demoReplay && _demoReplay->_mode == DemoReplay::kModePlayback) {
		float realTime = (float)(stamp._seconds - _lastStamp);

		if (realTime > kMaxReplayRealTime || realTime < 0.0f)
			realTime = kMaxReplayRealTime;

		_rawFrameTime = realTime;
		_frameTime = _demoReplay->consumeFrame();

		// Wait until the recorded frame time has passed
		while (_rawFrameTime < _frameTime) {
			g_system->delayMillis(1);
			stamp.stamp();

			_rawFrameTime = (float)(stamp._seconds - _lastStamp);

			if (_rawFrameTime < realTime) {
				_rawFrameTime = realTime;

				break;
			}
		}

		if (_rawFrameTime > kMaxReplayRealTime)
			_rawFrameTime = realTime;

		_lastStamp = stamp._seconds;

		return _frameTime;
	}

	float elapsed;

	if (manualMs != 0)
		elapsed = (float)((double)(uint)manualMs * kMillisecondsToSeconds);
	else
		elapsed = (float)(stamp._seconds - _lastStamp);

	if (_firstFrame) {
		elapsed = 0.0f;
		_firstFrame = false;
	}

	_rawFrameTime = elapsed;

	float maxFrameTime = kMaxFrameTime;

	if (getGamePlatform() == Common::kPlatformPS2)
		maxFrameTime = kMaxFrameTimePs2;
	else if (_isDemo)
		maxFrameTime = kMaxFrameTimeDemo;

	if (elapsed > maxFrameTime)
		elapsed = maxFrameTime;

	if (_dumpingFrames)
		elapsed = _isDemo ? kMaxFrameTimeDemo : kMaxFrameTime;

	_lastStamp = stamp._seconds;
	_frameTime = elapsed;

	if (_inputRecorder)
		_inputRecorder->recordFrame(elapsed);

	if (_demoReplay)
		_demoReplay->recordFrame(elapsed);

	return elapsed;
}

int EmiEngine::renderUpdate(CDrawContext *drawContext) {
	_renderUpdateTimer.start();

	int result = 0;

	if (getMode() == kModeNone) {
		_renderUpdateTimer.stop();

		return 0;
	}

	if (getMode() == kModeMovie) {
		Movie::presentCurrent(this);

		_renderUpdateTimer.stop();

		return 0;
	}

	Room_v2 *currentRoom = Room_v2::getCurrentRoom(this);

	if (!currentRoom) {
		_renderUpdateTimer.stop();

		return 0;
	}

	Overworld *overworld = getOverworld();

	// The demo has no overworld mode branch, every other mode draws the room
	if (_isDemo || getMode() != kModeOverworld) {
		_frontCopyCount = 0;

		if (drawContext->beginScene()) {
			int roomResult = currentRoom->draw(drawContext);

			if (roomResult < 0) {
				drawContext->endScene();
			} else {
				_primitiveQueue->drawQueue(drawContext, kPrimitiveLayers);
				result = DimRegion::drawDimRegions(this, drawContext) + roomResult;

				if (overworld->_enabled) {
					currentRoom->saveLightStates();
					currentRoom->setLightsActive(false);
					overworld->setLightsActive(true);
					result += overworld->draw(drawContext);
					overworld->setLightsActive(false);
					currentRoom->restoreLightStates();
				}

				result += TextEvent::update(this, drawContext, true);
				drawContext->endScene();
				drawContext->present(true);
			}
		}
	} else if (drawContext->beginScene()) {
		int copyCount = _frontCopyCount;

		_frontCopyCount++;

		if (copyCount < 1)
			drawContext->copyFrontToBack();

		if (overworld->_enabled) {
			currentRoom->saveLightStates();
			currentRoom->setLightsActive(false);
			overworld->setLightsActive(true);
			result = overworld->draw(drawContext);
			overworld->setLightsActive(false);
			currentRoom->restoreLightStates();
		}

		result += TextEvent::update(this, drawContext, true);
		drawContext->endScene();
		drawContext->present(true);
	}

	_renderUpdateTimer.stop();

	return result;
}

void EmiEngine::showSplash(CDrawContext *drawContext) {
	if (!drawContext->beginScene())
		return;

	Layer *layer = new Layer(this, "splash.til", 0, 0, -1);

	layer->draw(drawContext);
	drawContext->endScene();
	drawContext->present(true);
	delete layer;
}

void EmiEngine::pixelTest() {
	CDrawContext *drawContext = _drawContext;

	if (!drawContext->beginScene())
		return;

	if (!_isDemo)
		Layer::set16BitFix(this, false);

	Layer *layer = new Layer(this, "pixeltest.til", 0, 0, -1);

	layer->draw(drawContext);
	drawContext->endScene();

	if (!_isDemo)
		Layer::set16BitFix(this, true);

	LABitmap bitmap(this);

	drawContext->readBackBuffer(&bitmap);
	bitmap.convertFormat(LColorFormat(6));

	uint pixelCount = (uint)bitmap._sizeBytes / 3;
	const byte *pixels = (const byte *)bitmap._pixels;
	uint rowWidth = (uint)bitmap._width;

	if (pixelCount != 0 && (!pixels || rowWidth == 0)) {
		warning("EmiEngine::pixelTest(): the back buffer has no pixels");
		pixelCount = 0;
	}

	for (uint i = 0; i < pixelCount; i++) {
		byte expected = ((i + i / rowWidth) & 1) != 0 ? 0 : 255;

		if (pixels[i * 3] != expected) {
			drawContext->setPixelCenter(!drawContext->getPixelCenter());

			if (_isDemo)
				_consoleServices->logDebug("The pixel center has to change.\n");

			break;
		}
	}

	delete layer;

	// The demo leaves the test picture on screen until the splash replaces it
	if (!_isDemo)
		drawContext->clear(true, false);
}

void EmiEngine::saveScreenshot() {
	Common::String path = Common::String::format("./%s%d.tga", "shot", _screenshotCount);

	_screenshotCount++;

	// TODO write the screenshot to the ScummVM screenshot path
	saveTga(path.c_str());
}

void EmiEngine::saveTga(const char *path) {
	CDrawContext *drawContext = _drawContext;

	if (!drawContext) {
		warning("EmiEngine::saveTga(): no draw context");

		return;
	}

	LABitmap bitmap(this);

	drawContext->readBackBuffer(&bitmap);
	bitmap.convertFormat(LColorFormat(6));

	CTGAFile tgaFile(this);
	ZFile *file = _zFileServices->open(path, "wb");

	if (!file) {
		warning("EmiEngine::saveTga(): could not create %s", path);

		return;
	}

	tgaFile.writeImage(file, &bitmap, nullptr);
	_zFileServices->closeZFile(file);
}

void EmiEngine::dumpFrame() {
	Common::String path = Common::String::format("./%s/%s%02d.tga", _frameDumpPath.c_str(), _frameDumpPath.c_str(), _frameDumpCount);

	_frameDumpCount++;
	saveTga(path.c_str());
}

void EmiEngine::beginFrameDump(const char *path) {
	_frameDumpCount = 0;
	_frameDumpPath = path;
	_dumpingFrames = true;

	// TODO write the frames to the ScummVM screenshot path
	Common::FSNode(Common::Path(path, '/')).createDirectory();
	_sounds->suspendAudio();
}

void EmiEngine::endFrameDump() {
	_dumpingFrames = false;
	_sounds->resumeAudio();
}

void EmiEngine::drawStatsOverlay(CDrawContext *drawContext) {
	// The disasm of the Korean and Chinese versions replaces the whole function with an empty one
	if (_cjkMode)
		return;

	Room_v2 *currentRoom = Room_v2::getCurrentRoom(this);
	bool skipTexts = false;
	int numStatsEvents = _isDemo ? kNumStatsEventsDemo : kNumStatsEvents;

	if (currentRoom && (_lua->_statsOverlayFlags & Lua_V2::kStatsOverlayFlag) != 0) {
		double framesPerSecond = 0.0;

		// The demo has no unclamped frame time
		float shownFrameTime = _isDemo ? _frameTime : _rawFrameTime;

		// The original could divide by zero here, let's avoid that...
		if (shownFrameTime != 0.0f)
			framesPerSecond = (double)1.0f / (double)shownFrameTime;

		Common::String framesText = Common::String::format("FPS: %6.2f", framesPerSecond);
		Common::String frameTimeText = Common::String::format("FT:  %6.2fms", (double)shownFrameTime * (double)1000.0f);
		TextEvent *firstEvent = _statsEvents[0];

		if (firstEvent && (firstEvent->_flags & 1) != 0) {
			firstEvent->_text = framesText;
			firstEvent->_geometryReady = false;
			_statsEvents[1]->_text = frameTimeText;
			_statsEvents[1]->_geometryReady = false;
		} else {
			for (int i = 0; i < numStatsEvents; i++) {
				TextEvent *event;

				if (i == 1)
					event = new TextEvent(this, frameTimeText);
				else
					event = new TextEvent(this, framesText);

				_statsEvents[i] = event;
				event->setFont("comicsans18.laf");
				event->_x = kStatsTextX;
				event->_y = kStatsTextFirstY + i * kStatsTextRowHeight;

				if (_isDemo && i == 3)
					event->_y = kStatsTextDemoTexY;

				event->_geometryReady = false;
				event->_justify = kStatsTextJustify;
				event->_geometryReady = false;
				event->_color = Color::kWhite;
				event->_geometryReady = false;
				event->activate(0.0f, 1);
			}
		}
	} else if (currentRoom && (_lua->_statsOverlayFlags & Lua_V2::kStatsOverlayFlag) == 0) {
		if (_statsEvents[0]) {
			for (int i = 0; i < numStatsEvents; i++) {
				_statsEvents[i]->_flags &= ~1u;
				_statsEvents[i] = nullptr;
			}

			skipTexts = true;
		}
	}

	if (!skipTexts) {
		if (_statsEvents[2]) {
			_statsEvents[2]->_text = Common::String::format("mem: %d, polys: %d", (int)_memoryServices->_bytesAllocated, drawContext->_trianglesDrawn);
			_statsEvents[2]->_geometryReady = false;
		}

		if (_isDemo) {
			// The texture counters are not tracked here, they stay at zero
			if (_statsEvents[3]) {
				_statsEvents[3]->_text = Common::String::format("Tex: (U) %d, %d (L) %d, %d", 0, 0, 0, 0);
				_statsEvents[3]->_geometryReady = false;
			}
		} else {
			// The D3D statistics do not exist here and the disasm leaves the last value uninitialized
			if (_statsEvents[3]) {
				_statsEvents[3]->_text = Common::String::format("%s EV: %4d CR: %4d UT: %4d UV: %4d", " ", 0, 0, 0, 0);
				_statsEvents[3]->_geometryReady = false;
			}
		}

		if (!_isDemo && _statsEvents[4]) {
			_statsEvents[4]->_text = Common::String::format("RE: %4d (%10d) MN: %4d (%10d)", 0, 0, 0, 0);
			_statsEvents[4]->_geometryReady = false;
		}
	}

	ProfilerTimer::endFrame(this);

	if (currentRoom && (_lua->_statsOverlayFlags & Lua_V2::kStatsProfilerFlag) != 0) {
		ProfilerTimer::printAllStats(this);
		_lua->_statsOverlayFlags &= ~(uint)Lua_V2::kStatsProfilerFlag;
	}
}

} // End of namespace Grim
