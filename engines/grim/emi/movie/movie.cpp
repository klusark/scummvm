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

#include "common/events.h"
#include "common/memstream.h"
#include "common/stream.h"
#include "common/substream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "audio/mixer.h"

#include "graphics/surface.h"

#ifdef USE_BINK
#include "video/bink_decoder.h"
#endif

#include "engines/grim/grim.h"
#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/resources/cbundle.h"
#include "engines/grim/emi/graphics/cdcvertexarray.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/input/joystick.h"
#include "engines/grim/emi/images/labitmap.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/movie/movie.h"
#include "engines/grim/emi/movie/pss/pssdecoder.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/sound/sounds.h"
#include "engines/grim/emi/input/sputcontrol_v2.h"
#include "engines/grim/emi/text/cjkstring.h"
#include "engines/grim/emi/text/textevent.h"

namespace Grim {

static const byte kXorKey = 0xD2;
static const int kSmushHeaderSize = 16;
static const int kSmushBufferSize = 32;
static const int kSubtitleLineSize = 400;
static const int kVoiceCategory = 2;
static const int kStandardWidth = 640;
static const int kMaxClearedFrames = 2;
static const int kSubtitleX = 320;
static const int kSubtitleY = 468;
static const byte kSubtitleJustify = 10;
static const int kEventsMask = 3;
static const int kPs2MinFramesBeforeSkip = 4;
static const int kPs2LoopDelayMs = 4;
static const uint kPs2LoadingIconMinMs = 500;
static const float kMovieInputGain = 0.85f;
static const int kLoadingIconVertices = 4;
static const int kLoadingIconQuadsFlags = 0x10004;
static const float kLoadingIconNearClip = 0.05f;
static const float kLoadingIconFarClip = 20.0f;

static const float kLoadingIconPositions[] = {
	1.624f, -1.143f, 1.983f,
	1.924f, -1.143f, 1.983f,
	1.924f, -1.443f, 1.983f,
	1.624f, -1.443f, 1.983f
};

static const float kLoadingIconUvs[] = {
	0.0f, 0.0f,
	1.0f, 0.0f,
	1.0f, 1.0f,
	0.0f, 1.0f
};

Movie::Movie(EmiEngine *engine, const char *name, bool newSubtitles, bool newFlag6194, bool newFlag6195, int frame) : EventHandler(engine, 0) {
	bool isDemo = _engine->isDemo();

	// The demo keeps the master gain and ignores the subtitle, drawing and start frame flags
	if (!isDemo)
		_engine->getSounds()->lowerMasterGainForMovie();

	_fileName = name;

	size_t extension = _fileName.rfind('.');

	if (extension != LString::npos) {
		_fileName.erase(extension);
		_fileName += isDemo ? ".lab" : ".m4b";
	}

	_typeMask = 0;
	_finished = false;
	_frameReady = false;
	memset(_subtitles, 0, sizeof(_subtitles));
	_numSubtitles = 0;
	_subtitleIndex = 0;
	_subtitleEvent = nullptr;
	_i9nBundleOpen = _engine->getBundleServices()->isI9nBundleOpen();
	_subtitlesOn = isDemo ? false : newSubtitles;
	_flag6194 = isDemo ? false : newFlag6194;
	_flag6195 = isDemo ? false : newFlag6195;
	_engine->_movieFirstFrame = isDemo ? 1 : frame;
	// The disasm draws on the surface directly only on its full screen Direct3D path, the one reproduced here
	_useDirectDraw = !_subtitlesOn && !_flag6194;
	_clearCount = 0;
	_bink = nullptr;
	_frameSurface = nullptr;
	_paused = false;
}

Movie::~Movie() {
	if (_subtitleEvent) {
		_subtitleEvent->_flags &= ~1u;
		_subtitleEvent = nullptr;
	}

#ifdef USE_BINK
	delete _bink;
#endif

	_bink = nullptr;
	_frameSurface = nullptr;

	if (!_engine->isDemo())
		_engine->getSounds()->restoreMasterGain();
}

bool Movie::handleEvent(const Event *event) {
	if (event->type != Event::kKeyUp || event->vk != 27)
		return false;

	_finished = true;

	return true;
}

void Movie::pause() {
#ifdef USE_BINK
	if (_bink && !_paused) {
		_bink->pauseVideo(true);
		_paused = true;
	}
#endif

	_typeMask = 0;
}

void Movie::resume() {
#ifdef USE_BINK
	if (_bink && _paused) {
		_bink->pauseVideo(false);
		_paused = false;
	}
#endif

	_typeMask = kEventsMask;
}

bool Movie::update() {
#ifdef USE_BINK
	if (!_bink) {
		warning("Movie::update(): the movie is not open");

		return true;
	}

	// The frame number of the disasm does not move until the frame is presented
	if (_frameReady || !_bink->needsUpdate())
		return _finished;

	_frameSurface = _bink->decodeNextFrame();

	if (!_frameSurface) {
		warning("Movie::update(): cannot decode the next frame");
		_finished = true;

		return true;
	}

	_frameReady = true;

	return _finished;
#else
	return true;
#endif
}

void Movie::presentFrame() {
	if (!_frameReady)
		return;

	_frameReady = false;
	drawFrame();

#ifdef USE_BINK
	if (!_bink)
		return;

	int frameNumber = _bink->getCurFrame() + 1;

	if (_subtitlesOn)
		updateSubtitle(frameNumber);

	if (frameNumber == (int)_bink->getFrameCount())
		_finished = true;
#endif
}

uint Movie::readHeader() {
	LFile *file = _engine->getFileServices()->open(_fileName.c_str(), "rb");

	if (!file)
		return 0;

	uint size = 0;
	int count = 0;
	char header[kSmushBufferSize];

	memset(header, 0, sizeof(header));
	file->read(header, kSmushHeaderSize);

	if (!strncmp(header, "SMUSH", 5)) {
		xorDecryptD2((byte *)header, kSmushHeaderSize);
		strtok(header, "\n");

		char *sizeText = strtok(nullptr, "\n");
		int value = 0;
		int parsed = 0;

		if (sizeText) {
			sizeText[4] = 0;
			parsed = sscanf(sizeText, "%d", &value);
		} else {
			warning("Movie::readHeader(): the header has no size");
		}

		if (value < 0) {
			warning("Movie::readHeader(): the header size is negative");
			value = 0;
		}

		if (parsed != 0) {
			size = value;

			if (_subtitlesOn) {
				file->seek(0, 0);

				char *data = new char[size + 1];

				memset(data, 0, size + 1);
				file->read(data, size);
				xorDecryptD2((byte *)data, size);

				char *line = strtok(data, "\n");

				while (line) {
					if (!strncmp(line, "BEGINDATA", 9))
						break;

					line = strtok(nullptr, "\n");
				}

				line = strtok(nullptr, "\n");

				while (line) {
					if (!strcmp(line, "ENDOFDATA"))
						break;

					if (count >= kMaxSubtitles)
						break;

					// The disasm reads the key without a size limit
					if (sscanf(line, "%d\t%d\t%15s\t", &_subtitles[count].start, &_subtitles[count].end, _subtitles[count].text) != 3)
						break;

					count++;
					line = strtok(nullptr, "\n");
				}

				delete[] data;
			}
		}
	}

	_engine->getFileServices()->close(file, true);
	_numSubtitles = count;

	return size;
}

bool Movie::loadSubtitles() {
	LString path = _fileName;
	size_t extension = path.rfind('.');

	if (extension == LString::npos)
		return false;

	path.erase(extension);
	path += ".sub";

	LFile *file = _engine->getFileServices()->open(path.c_str(), "r");

	if (!file)
		return false;

	int count = 0;
	char line[kSubtitleLineSize];

	if (file->getString(line, kSubtitleLineSize)) {
		while (count < kMaxSubtitles) {
			if (line[0] != '#') {
				// The disasm reads the key without a size limit
				if (sscanf(line, "%d\t%d\t%15s\t", &_subtitles[count].start, &_subtitles[count].end, _subtitles[count].text) != 3) {
					_engine->getFileServices()->close(file, true);

					return false;
				}

				count++;
			}

			if (!file->getString(line, kSubtitleLineSize))
				break;
		}
	}

	_numSubtitles = count;
	_engine->getFileServices()->close(file, true);

	return true;
}

void Movie::updateSubtitle(int frame) {
	if (_subtitleIndex == _numSubtitles)
		return;

	SubtitleEntry *entry = &_subtitles[_subtitleIndex];

	if (frame < entry->start)
		return;

	if (frame > entry->end) {
		if (_subtitleEvent) {
			_subtitleEvent->_flags &= ~1u;
			_subtitleEvent = nullptr;
		}

		_subtitleIndex++;

		return;
	}

	if (_subtitleEvent)
		return;

	if (_engine->isCjkMode()) {
		CjkString cjkText = TextEvent::translateCjk(_engine, entry->text, nullptr);

		_subtitleEvent = new TextEvent(_engine, cjkText);
	} else {
		const char *translated = TextEvent::translate(_engine, entry->text, nullptr);
		LString text;

		if (translated)
			text = translated;
		else
			warning("Movie::updateSubtitle(): no text for the subtitle");

		_subtitleEvent = new TextEvent(_engine, text);
	}

	if (_i9nBundleOpen)
		_subtitleEvent->setFont("subtitle.laf");

	_subtitleEvent->_x = kSubtitleX;
	_subtitleEvent->_y = kSubtitleY;
	_subtitleEvent->_geometryReady = false;
	_subtitleEvent->_justify = kSubtitleJustify;
	_subtitleEvent->_geometryReady = false;
	_subtitleEvent->activate(0.0f, 1);
}

bool Movie::open() {
#ifdef USE_BINK
	CDrawContext *drawContext = _engine->getDrawContext();

	if (!drawContext) {
		warning("Movie::open(): the movies are not started");

		return false;
	}

	size_t slash = _fileName.rfind('\\');

	if (slash != LString::npos) {
		LString movieName = LString(".\\Movies\\") + _fileName.substr(slash + 1);

		if (fileExists(_engine, movieName.c_str()))
			_fileName = movieName;
	}

	// The demo movies are plain Bink files, without the encrypted header
	uint header = 0;

	if (!_engine->isDemo()) {
		header = readHeader();

		if (_subtitlesOn && !header && !loadSubtitles())
			_subtitlesOn = false;
	}

	LFile *file = _engine->getFileServices()->open(_fileName.c_str(), "rb");

	if (!file)
		return false;

	Common::SeekableReadStream *stream = file->_readStream;

	file->_readStream = nullptr;
	_engine->getFileServices()->close(file, true);

	if (!stream || header > stream->size()) {
		warning("Movie::open(): the movie header is bigger than the file");
		delete stream;

		return false;
	}

	Common::SeekableReadStream *binkStream = new Common::SeekableSubReadStream(stream, header, stream->size(), DisposeAfterUse::YES);

	_bink = new Video::BinkDecoder();

	if (!_bink->loadStream(binkStream)) {
		delete _bink;
		_bink = nullptr;

		return false;
	}

	if (!_bink->setOutputPixelFormat(drawContext->getMovieFormat()))
		warning("Movie::open(): the decoder cannot use the format of the draw context");

	// The demo plays the movie at full volume
	if (!_engine->isDemo()) {
		float gain = Sounds::soundVolumeToGain(_engine->getSounds()->getCategoryVolume(kVoiceCategory));

		// The disasm turns NaN into 0
		if (gain > 1.0f)
			gain = 1.0f;
		else if (!(gain >= 0.0f))
			gain = 0.0f;

		_bink->setVolume((byte)(gain * 255.0f + 0.5f));
	}

	_bink->setSoundType(Audio::Mixer::kSpeechSoundType);

	_clearCount = 0;
	_typeMask = kEventsMask;
	_finished = false;
	_frameReady = false;

	if (_engine->_movieFirstFrame != 1) {
		if (_engine->_movieFirstFrame < 1 || _engine->_movieFirstFrame > (int)_bink->getFrameCount())
			warning("Movie::open(): the first frame %d is not in the movie", _engine->_movieFirstFrame);
		else if (!_bink->seekToFrame(_engine->_movieFirstFrame - 1))
			warning("Movie::open(): cannot go to the frame %d", _engine->_movieFirstFrame);
	}

	_bink->start();

	return true;
#else
	warning("Movie::open(): the Bink decoder is not built in");

	return false;
#endif
}

void Movie::drawFrame() {
	CDrawContext *drawContext = _engine->getDrawContext();

	if (!drawContext || !_frameSurface) {
		warning("Movie::drawFrame(): nothing to draw");

		return;
	}

	if (!_useDirectDraw && (_flag6194 || _frameSurface->w != kStandardWidth)) {
		bool clearScreen = _subtitlesOn;

		if (!clearScreen) {
			clearScreen = _clearCount < kMaxClearedFrames;
			_clearCount++;
		}

		if (clearScreen)
			drawContext->clear(true, false);
	}

	if (!drawContext->beginScene())
		return;

	drawContext->drawMovieFrame(*_frameSurface);

	if (!_useDirectDraw)
		TextEvent::update(_engine, drawContext, true);

	drawContext->endScene();
	drawContext->present(true);
}

bool Movie::startMovie(EmiEngine *engine, const char *fileName, bool subtitles, bool flag6194, bool flag6195, int frame) {
	if (engine->_currentMovie) {
		warning("Movie::startMovie(): a movie is already playing");
		stopMovie(engine);
	}

	engine->_currentMovie = new Movie(engine, fileName, subtitles, flag6194, flag6195, frame);

	if (!engine->_currentMovie->open()) {
		stopMovie(engine);

		return false;
	}

	return true;
}

void Movie::stopMovie(EmiEngine *engine) {
	if (!engine->_currentMovie)
		return;

	pauseCurrent(engine);
	delete engine->_currentMovie;
	engine->_currentMovie = nullptr;
}

bool Movie::updateMovie(EmiEngine *engine) {
	if (!engine->_currentMovie) {
		warning("Movie::updateMovie(): no movie is playing");

		return true;
	}

	bool movieFinished = engine->_currentMovie->update();

	if (movieFinished)
		stopMovie(engine);

	return movieFinished;
}

void Movie::presentCurrent(EmiEngine *engine) {
	if (engine->_currentMovie)
		engine->_currentMovie->presentFrame();
}

void Movie::pauseCurrent(EmiEngine *engine) {
	if (engine->_currentMovie)
		engine->_currentMovie->pause();
}

void Movie::resumeCurrent(EmiEngine *engine) {
	if (engine->_currentMovie)
		engine->_currentMovie->resume();
}

void Movie::skipCurrent(EmiEngine *engine) {
	if (engine->_currentMovie)
		engine->_currentMovie->_finished = true;
}

void Movie::startupMovies(EmiEngine *engine) {
	engine->_ps2MovieRequested = false;
	engine->_ps2MovieName.clear();
	engine->_loadingIconPs2 = nullptr;
}

void Movie::releaseSubtitleFont() {
	// The subtitles are text events, there is no font to release
}

bool Movie::fileExists(EmiEngine *engine, const char *path) {
	LFile *file = engine->getFileServices()->open(path, "rb");

	if (!file)
		return false;

	engine->getFileServices()->close(file, true);

	return true;
}

void Movie::xorDecryptD2(byte *data, int size) {
	for (int i = 0; i < size; i++)
		data[i] ^= kXorKey;
}

void Movie::requestMoviePs2(EmiEngine *engine, const char *name) {
	engine->_ps2MovieRequested = true;
	engine->_ps2MovieName = name;
}

bool Movie::isMovieRequestedPs2(EmiEngine *engine) {
	return engine->_ps2MovieRequested;
}

bool Movie::playRequestedMoviePs2(EmiEngine *engine) {
	Sounds *sounds = engine->getSounds();

	engine->_ps2MovieRequested = false;

	// PS2 saves the game state to RAM and reloads it after the movie; we only restore sound and music
	Common::MemoryWriteStreamDynamic state(DisposeAfterUse::YES);

	engine->_ps2StateWriter = &state;
	sounds->saveSounds(writePs2State);
	sounds->saveMusicState(writePs2State);
	engine->_ps2StateWriter = nullptr;

	sounds->stopAllForLoad();

	// Unloading the room waits for the music, like a set change
	if (Room_v2::getCurrentRoom(engine))
		sounds->killMusicForSet(nullptr);

	bool played = playMoviePs2(engine, engine->_ps2MovieName.c_str());
	bool iconShown = drawLoadingIconPs2(engine);
	uint iconMs = g_system->getMillis();

	// The reload unloads everything again, but the room is already gone by then
	sounds->stopAllForLoad();

	// Our reload is instant while PS2 takes a while, so the icon stays on screen for at least 500 ms
	if (iconShown) {
		uint elapsedMs = g_system->getMillis() - iconMs;

		if (elapsedMs < kPs2LoadingIconMinMs)
			g_system->delayMillis(kPs2LoadingIconMinMs - elapsedMs);
	}

	Common::MemoryReadStream reader(state.getData(), state.size());

	engine->_ps2StateReader = &reader;
	sounds->restoreSounds(readPs2State);
	sounds->restoreMusicState(readPs2State);
	engine->_ps2StateReader = nullptr;

	return played;
}

// The sound code takes plain function pointers, so these two reach the engine through g_grim
int Movie::writePs2State(void *data, int bytes) {
	EmiEngine *engine = static_cast<EmiEngine *>(g_grim);

	if (bytes && engine->_ps2StateWriter)
		engine->_ps2StateWriter->write(data, bytes);

	return 1;
}

int Movie::readPs2State(void *data, int bytes) {
	EmiEngine *engine = static_cast<EmiEngine *>(g_grim);

	if (bytes && engine->_ps2StateReader)
		engine->_ps2StateReader->read(data, bytes);

	return 1;
}

bool Movie::playMoviePs2(EmiEngine *engine, const char *name) {
	LConsoleServices *console = engine->getConsoleServices();

	if (console)
		console->logDebug("Movie::Startup\n");

	float gain = Sounds::soundVolumeToGain(engine->getSounds()->getCategoryVolume(kVoiceCategory));

	if (gain < 0.0f || !(gain <= 1.0f))
		return false;

	if (console)
		console->logDebug("Movie::PlayMovie(%s, volume: %f)\n", name, gain);

	if (!name[0])
		return false;

	CDrawContext *drawContext = engine->getDrawContext();

	if (!drawContext) {
		warning("Movie::playMoviePs2(): movies are not started");

		return false;
	}

	LString path = LString("VIDEO\\") + name + ".PSS";
	Common::SeekableReadStream *stream = openMoviePs2(engine, path);

	if (!stream) {
		if (console)
			console->logError("Could not find Video: %s\n", path.c_str());

		path = "VIDEO\\DEFAULT.PSS";
		stream = openMoviePs2(engine, path);

		if (!stream) {
			if (console)
				console->logError("Could not find DEFAULT Video: %s\n", path.c_str());

			return false;
		}
	}

#ifdef USE_MPEG2
	PssDecoder decoder;

	// The decoder owns the stream, also when it cannot open it
	if (!decoder.loadStream(stream, drawContext->getMovieFormat())) {
		warning("Movie::playMoviePs2(): cannot open %s", path.c_str());

		return false;
	}

	Common::EventManager *eventManager = g_system->getEventManager();
	Common::Event event;
	Graphics::Surface screen;
	int framesShown = 0;
	bool skipped = false;
	uint startMs = g_system->getMillis();

	decoder.start((byte)(gain * kMovieInputGain * 255.0f + 0.5f));

	while (!engine->shouldQuit() && !skipped && !decoder.isFinished()) {
		const Graphics::Surface *frame = decoder.update();

		if (frame) {
			drawFramePs2(engine, *frame, screen);
			framesShown++;
		}

		while (eventManager->pollEvent(event)) {
			if (isSkipEventPs2(engine, event) && framesShown >= kPs2MinFramesBeforeSkip)
				skipped = true;
		}

		g_system->delayMillis(kPs2LoopDelayMs);
	}

	if (console)
		console->logDebug("Movie ended after %u frames: clock %u ms, wall clock %u ms\n", decoder.getFramesDecoded(), decoder.getTime(), g_system->getMillis() - startMs);

	if (skipped || engine->shouldQuit())
		decoder.stop();
	else
		decoder.detachAudio();

	screen.free();

	return true;
#else
	delete stream;
	warning("Movie::playMoviePs2(): the MPEG-2 decoder is not built in");

	return false;
#endif
}

Common::SeekableReadStream *Movie::openMoviePs2(EmiEngine *engine, const LString &path) {
	LFile *file = engine->getFileServices()->open(path.c_str(), "rb");

	if (!file)
		return nullptr;

	Common::SeekableReadStream *stream = file->_readStream;

	file->_readStream = nullptr;
	engine->getFileServices()->close(file, true);

	return stream;
}

void Movie::drawFramePs2(EmiEngine *engine, const Graphics::Surface &frame, Graphics::Surface &screen) {
	CDrawContext *drawContext = engine->getDrawContext();
	int width = drawContext->_displayMode.width;
	int height = drawContext->_displayMode.height;
	const Graphics::Surface *output = &frame;

	// 640x448 frames are centered on taller screens, with black borders
	if (frame.w <= width && frame.h <= height && (frame.w != width || frame.h != height)) {
		if (!screen.getPixels() || screen.w != width || screen.h != height || screen.format != frame.format) {
			screen.free();
			screen.create(width, height, frame.format);
			screen.fillRect(Common::Rect(width, height), frame.format.RGBToColor(0, 0, 0));
		}

		screen.copyRectToSurface(frame, (width - frame.w) / 2, (height - frame.h) / 2, Common::Rect(frame.w, frame.h));
		output = &screen;
	}

	if (!drawContext->beginScene())
		return;

	drawContext->drawMovieFrame(*output);
	drawContext->endScene();
	drawContext->present(true);
}

void Movie::loadLoadingIconPs2(EmiEngine *engine) {
	LABitmap bitmap(engine);

	// PS2 creates the texture even when the file is missing, and then divides by zero
	if (!bitmap.loadFile("FX\\LOADING.TGA")) {
		warning("Movie::loadLoadingIconPs2(): cannot load FX\\LOADING.TGA");

		return;
	}

	engine->_loadingIconPs2 = engine->getDrawContext()->createTexture(&bitmap);
}

void Movie::releaseLoadingIconPs2(EmiEngine *engine) {
	engine->getDrawContext()->freeTexture(engine->_loadingIconPs2);
	engine->_loadingIconPs2 = nullptr;
}

bool Movie::drawLoadingIconPs2(EmiEngine *engine) {
	if (!engine->_loadingIconPs2)
		return false;

	CDrawContext *drawContext = engine->getDrawContext();

	drawContext->copyFrontToBack();

	Camera camera(drawContext);

	camera.setLocalPosition(CVector());
	camera._nearClip = kLoadingIconNearClip;
	camera._farClip = kLoadingIconFarClip;
	camera.applyProjection();

	VertexStreams streams;

	streams.count = kLoadingIconVertices;
	streams.positions = kLoadingIconPositions;
	streams.normals = nullptr;
	streams.colors = nullptr;
	streams.uvs = kLoadingIconUvs;

	drawContext->beginScene();
	drawContext->drawQuads(kLoadingIconQuadsFlags, engine->_loadingIconPs2, 0, kLoadingIconVertices, &streams);
	drawContext->endScene();
	drawContext->present(true);

	return true;
}

bool Movie::isSkipEventPs2(EmiEngine *engine, const Common::Event &event) {
	// SELECT skips the movie on PS2; Escape does too, for keyboard players
	if (event.type == Common::EVENT_KEYDOWN)
		return event.kbd.keycode == Common::KEYCODE_ESCAPE && !event.kbdRepeat;

	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START)
		return (int)event.customType == EmiEngine::kPadSelect;

	if (event.type == Common::EVENT_JOYBUTTON_DOWN)
		return engine->mapPadButton(event.joystick.button) == EmiEngine::kPadSelect;

	return false;
}

} // End of namespace Grim
