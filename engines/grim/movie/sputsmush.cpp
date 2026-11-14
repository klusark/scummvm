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

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "graphics/pixelformat.h"
#include "graphics/surface.h"

#ifdef USE_THEORADEC
#include "audio/mixer.h"
#include "video/theora_decoder.h"

#include "engines/grim/movie/theoraconverter.h"
#include "engines/grim/remastered/hdcanvas.h"
#endif

#include "engines/grim/grim.h"
#include "engines/grim/res.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

SputSmush::SputSmush(Res *res, StdDisplay *stdDisplay, SputRender *sputRender, StdVBuffer *mainGfxBuffer, WuRegistry *wuRegistry) :
		_res(res), _stdDisplay(stdDisplay), _sputRender(sputRender), _mainGfxBuffer(mainGfxBuffer),
		_wuRegistry(wuRegistry), _hs(nullptr), _started(0), _presentBuf(nullptr), _buf(0), _playing(0), _paused(0), _abort(0),
		_looping(0), _x(0), _y(0), _startTime(0), _lastPeriodicTime(0), _videoClip(nullptr), _movieSurface(nullptr),
		_releasedSurface(nullptr), _frameTime(0.0f), _isVideoVisible(false), _playFromMenu(false),
		_videoExtraFramesLeft(0), _videoDuration(0), _playingMovie(false), _wasPlayingMovie(false), _commentaryDucking(false),
		_videoPendingFrame(nullptr), _videoFpsNumerator(0), _videoFpsDenominator(1), _videoFrameDuration(0.0f),
		_videoEnded(false), _videoEndedTime(0), _movieSerial(0), _theoraConverter(nullptr), _theoraAudioConverter(nullptr) {
	memset(_curMovieFile, 0, sizeof(_curMovieFile));

	_smushPlay = new SmushPlay(g_grim->isRemastered());

	if (g_grim->isRemastered()) {
		const Common::FSNode gameDataDir(ConfMan.getPath("path"));

		SearchMan.addSubDirectoryMatching(gameDataDir, "MoviesHD");
	}
}

SputSmush::~SputSmush() {
	freeVideoResources();

	delete _smushPlay;
	_smushPlay = nullptr;
}

int SputSmush::getMovieTime() {
	int movieTime = _smushPlay->getMovieTime();

	if (movieTime == 0 && _started && videoIsPlayingMovie())
		return videoGetMovieTimeMs();

	return movieTime;
}

int SputSmush::initialize(SysStruct *hs) {
	_hs = hs;

	_smushPlay->sysInitialize(hs);
	_smushPlay->sysSetVideoCallbacks(this);

	_presentBuf = nullptr;
	_buf = 1;

	return 0;
}

int SputSmush::save(Common::WriteStream *stream) {
	stream->writeSint32LE(_playing);
	stream->writeSint32LE(_paused);
	stream->writeSint32LE(_abort);
	stream->write(_curMovieFile, sizeof(_curMovieFile));
	stream->writeSint32LE(_looping);
	stream->writeSint32LE(_x);
	stream->writeSint32LE(_y);

	if (stream->err()) {
		warning("SputSmush::save(): failed to write the movie state");
		return 1;
	}

	return 0;
}

int SputSmush::restore(Common::ReadStream *stream) {
	setTarget(_mainGfxBuffer);

	_playing = stream->readSint32LE();
	_paused = stream->readSint32LE();
	_abort = stream->readSint32LE();
	stream->read(_curMovieFile, sizeof(_curMovieFile));
	_looping = stream->readSint32LE();
	_x = stream->readSint32LE();
	_y = stream->readSint32LE();

	if (stream->err() || stream->eos()) {
		warning("SputSmush::restore(): failed to read the movie state");
		return 1;
	}

	if (!memchr(_curMovieFile, 0, sizeof(_curMovieFile))) {
		warning("SputSmush::restore(): the movie name is not terminated");
		_curMovieFile[sizeof(_curMovieFile) - 1] = '\0';
	}

	if (_playing)
		startMovie(_curMovieFile, _looping, _x, _y);

	return 0;
}

int SputSmush::startup() {
	char startSmush[10] = "";
	int found = _wuRegistry->getStr("StartSmush", startSmush, sizeof(startSmush), "");

	if (!found || scumm_stricmp(startSmush, "FALSE")) {
		if (_smushPlay->sysStartup())
			return 1;

		_started = 1;
	}

	return 0;
}

int SputSmush::shutdown() {
	if (_started) {
		freeVideoResources();
		_smushPlay->sysShutdown();
	}

	return 0;
}

int SputSmush::terminate() {
	_smushPlay->sysTerminate();

	return 0;
}

int SputSmush::periodic() {
	int result = 1;
	int finished = 0;

	if (!_started || !_playing)
		return 1;

	if (videoIsPlayingMovie()) {
		videoUpdate(0);

		if (!_abort) {
			_wasPlayingMovie = true;

			return !videoIsVisible();
		}

		_playing = 0;
		_smushPlay->stopMovie();
		videoStop();
		_wasPlayingMovie = false;

		return 1;
	}

	if (_wasPlayingMovie) {
		_playing = 0;
		_abort = 0;
		videoUpdate(1);
		_smushPlay->stopMovie();
		videoStop();
		_wasPlayingMovie = false;

		return 1;
	}

	if (_paused)
		return 0;

	_smushPlay->setTranslationTable(_sputRender->getColorLookup());

	if (_abort || !_smushPlay->updateMovie(&finished)) {
		if (finished || _abort) {
			_playing = 0;

			StdRect frameRect;

			_smushPlay->stopMovie();
			_smushPlay->getFrameRect(&frameRect, nullptr);
			_sputRender->markAllPagesDirty(&frameRect, 0);
		} else {
			result = 0;
		}
	}

	_smushPlay->sysConfigureColorLookup(0);
	_lastPeriodicTime = g_system->getMillis();

	return result;
}

int SputSmush::setTarget(StdVBuffer *buffer) {
	if (!_started)
		return 1;

	_presentBuf = buffer;

	return 0;
}

int SputSmush::startMovie(const char *name, int looping, int x, int y) {
	if (!_started)
		return 1;

	if (_playing) {
		_wasPlayingMovie = false;
		_smushPlay->stopMovie();
		videoStop();
		_playing = 0;
	}

	_smushPlay->setTranslationTable(_sputRender->getColorLookup());

	_looping = looping;
	_x = x;
	_y = y;

	int libMode = g_grim->getLibMode();

	if (!(g_grim->isRemastered() && videoPlay(name, libMode == GrimEngine::kLibModeMenu || libMode == 0))) {
		if (!_presentBuf) {
			warning("SputSmush::startMovie(): the target is not set");
			return 1;
		}

		if (_smushPlay->startMovie(name, looping != 0 ? 0x10 : 0, 100000, x, y, _presentBuf->tex.width, _presentBuf->tex.height, 0, 1))
			return 1;
	}

	_paused = 0;
	_abort = 0;
	_playing = 1;

	if (name != _curMovieFile) {
		if (strlen(name) >= sizeof(_curMovieFile))
			warning("SputSmush::startMovie(): the movie name is too long");

		strncpy(_curMovieFile, name, sizeof(_curMovieFile));
		_curMovieFile[sizeof(_curMovieFile) - 1] = '\0';
	}

	_startTime = g_system->getMillis();

	return 0;
}

int SputSmush::stopMovie() {
	int result = _started;

	if (_started) {
		_abort = 1;
		_paused = 0;
	}

	return result;
}

int SputSmush::getMovieLength(const char *name) {
	char path[128];
	int movieLength = 0;

	if (_res->findExternalFile(name, path, sizeof(path)) == 1)
		movieLength = _smushPlay->getMovieLength(path);

	return movieLength;
}

int SputSmush::startVideo(int width, int height, int depth, int pixelFormat) {
	if (!_started)
		return 1;

	if (!_presentBuf) {
		warning("SputSmush::startVideo(): the target is not set");
		return 1;
	}

	return _presentBuf->tex.width != width || _presentBuf->tex.height != height || _presentBuf->tex.format.bpp != depth;
}

int SputSmush::stopVideo() {
	return _started == 0;
}

int SputSmush::lockMovieBuffer(LecBitmap *bitmap, StdRect *rect) {
	if (!_started)
		return 1;

	if (!_presentBuf) {
		warning("SputSmush::lockMovieBuffer(): the target is not set");
		return 1;
	}

	_stdDisplay->vBufferLock(_presentBuf);

	bitmap->bytesPerPixel = _presentBuf->tex.format.bpp >> 3;
	bitmap->pixels = _presentBuf->pixels;
	bitmap->height = _presentBuf->tex.height;
	bitmap->width = _presentBuf->tex.width;
	bitmap->pitchInBytes = _presentBuf->tex.pitchInBytes;

	rect->y = 0;
	rect->x = 0;
	rect->height = _presentBuf->tex.height;
	rect->width = _presentBuf->tex.width;

	return 0;
}

int SputSmush::unlockMovieBuffer(LecBitmap *bitmap, StdRect *rect) {
	if (!_started)
		return 1;

	if (!_presentBuf) {
		warning("SputSmush::unlockMovieBuffer(): the target is not set");
		return 1;
	}

	_stdDisplay->notifyBufferWritten(_presentBuf);
	_stdDisplay->vBufferUnlock(_presentBuf);

	return 0;
}

int SputSmush::setVideoPalette(uint32 *palette, int start, int count) {
	return _started == 0;
}

int SputSmush::moviePlaying() {
	return _playing;
}

int SputSmush::pause(int pause) {
	_paused = pause;

	if (pause)
		_smushPlay->pauseMovie();
	else
		_smushPlay->resumeMovie();

	return 0;
}

int SputSmush::getVideoDurationMs(const char *name) {
	struct HdMovieDuration {
		const char *name;
		int duration;
	};

	static const HdMovieDuration durations[] = {
		{ "albinizod.snm", 4466 }, { "brunopk.snm", 115666 }, { "byebye.snm", 129533 }, { "byeruba.snm", 217266 },
		{ "coffrock.snm", 64800 }, { "copaldie.snm", 72733 }, { "credits.snm", 523000 }, { "crushed.snm", 100333 },
		{ "distbeav.snm", 5866 }, { "dooropen.snm", 7400 }, { "eatbird.snm", 72200 }, { "eldepot.snm", 60066 },
		{ "escape.snm", 35800 }, { "exodus.snm", 71600 }, { "falling.snm", 23133 }, { "getshcks.snm", 38200 },
		{ "glotdriv.snm", 3600 }, { "greenhse.snm", 98466 }, { "hecdie.snm", 14333 }, { "hecgetit.snm", 4266 },
		{ "heloruba.snm", 50266 }, { "heltrain.snm", 64400 }, { "hostage.snm", 60266 }, { "intro.snm", 187466 },
		{ "logos.snm", 3866 }, { "lol.snm", 102333 }, { "loladies.snm", 66733 }, { "lsahq.snm", 85466 },
		{ "mnycutfl.snm", 13266 }, { "neonlady.snm", 3466 }, { "normarae.snm", 73933 }, { "oldjob.snm", 68000 },
		{ "plunge.snm", 9400 }, { "poseidon.snm", 6600 }, { "repmec1c.snm", 51200 }, { "repmec3c.snm", 82333 },
		{ "reunion.snm", 84933 }, { "shanghai.snm", 82266 }, { "stocking.snm", 6666 }, { "stump1c.snm", 35600 },
		{ "stump3c.snm", 10000 }, { "subsaway.snm", 41933 }, { "swtchbrd.snm", 4466 }, { "swtchsuc.snm", 7800 },
		{ "swtchuns.snm", 9333 }, { "thepearl.snm", 13800 }, { "vivamaro.snm", 126133 }, { "yr2intro.snm", 62066 }
	};

	for (uint i = 0; i < ARRAYSIZE(durations); i++) {
		if (!strcmp(name, durations[i].name))
			return durations[i].duration;
	}

	return -1;
}

bool SputSmush::videoPlay(const char *name, bool playFromMenu) {
#ifdef USE_THEORADEC
	static const int languageHint[GrimEngine::kNumLanguages - 1] = { 2, 1, 4, 3, 5 };
	static const char *const languageCodes[6] = { "eng", "spa", "ger", "ita", "fre", "por" };

	HdCanvas *canvas = g_grim->getHdCanvas();

	if (!canvas || !canvas->isActive())
		return false;

	delete _videoClip;
	_videoClip = nullptr;
	_videoPendingFrame = nullptr;

	const char *extension = strstr(name, ".snm");

	if (!extension) {
		warning("SputSmush::videoPlay(): the movie name has no .snm extension");
		return false;
	}

	Common::String hdName(name, (uint32)(extension - name));

	hdName += ".ogv";

	Common::Path hdPath(hdName);

	if (!SearchMan.hasFile(hdPath))
		return false;

	Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(hdPath);

	if (!stream)
		return false;

	int language = g_grim->getLanguage();
	int hint = 0;

	if (language >= 1 && language <= 5)
		hint = languageHint[language - 1];

	if (!_theoraConverter)
		_theoraConverter = new TheoraConverter();

	if (!_theoraAudioConverter)
		_theoraAudioConverter = new TheoraAudioConverter();

	_theoraAudioConverter->setAudioGain(1.0f);

	Video::TheoraDecoder *decoder = new Video::TheoraDecoder();

	// The volume is applied by the sample converter, not by the mixer
	decoder->setSoundType(Audio::Mixer::kPlainSoundType);
	decoder->setAudioLanguage(languageCodes, ARRAYSIZE(languageCodes), hint);

#ifdef USE_TREMOR
	warning("SputSmush::videoPlay(): the decoder uses integer samples, the voice volume does not apply to the movies");
#else
	decoder->setSampleConverter(_theoraAudioConverter);
#endif

	decoder->setPostProcessing(false);
	decoder->setFrameConverter(_theoraConverter);

	if (!decoder->loadStream(stream) || !decoder->setOutputPixelFormat(Graphics::PixelFormat::createFormatRGBA32())) {
		warning("SputSmush::videoPlay(): cannot play %s, using the classic movie", hdName.c_str());
		delete decoder;
		return false;
	}

	Common::Rational frameRate = decoder->getFrameRate();

	if (frameRate.getNumerator() <= 0 || frameRate.getDenominator() <= 0) {
		warning("SputSmush::videoPlay(): %s has no frame rate, using the classic movie", hdName.c_str());
		delete decoder;
		return false;
	}

	_videoFpsNumerator = frameRate.getNumerator();
	_videoFpsDenominator = frameRate.getDenominator();

	float framesPerSecond = (float)((double)_videoFpsNumerator / (double)_videoFpsDenominator);

	_videoFrameDuration = (float)(1.0 / (double)framesPerSecond);

	decoder->start();

	_videoClip = decoder;
	_videoPendingFrame = nullptr;
	_videoEnded = false;
	_videoEndedTime = 0;
	_frameTime = 0.0f;
	_isVideoVisible = true;
	_playFromMenu = playFromMenu;
	_videoExtraFramesLeft = 0;
	_videoDuration = getVideoDurationMs(name);

	if (!_movieSurface) {
		_movieSurface = new Graphics::Surface();
		_movieSurface->create(decoder->getWidth(), decoder->getHeight(), Graphics::PixelFormat::createFormatRGBA32());
		_movieSerial++;
	}

	_playingMovie = true;

	return true;
#else
	return false;
#endif
}

void SputSmush::videoStop() {
#ifdef USE_THEORADEC
	if (_videoClip) {
		delete _videoClip;
		_videoClip = nullptr;
		_videoPendingFrame = nullptr;

		// The canvas may still read the frame of the last update, so the surface is freed later
		if (_movieSurface) {
			if (_releasedSurface) {
				_releasedSurface->free();
				delete _releasedSurface;
			}

			_releasedSurface = _movieSurface;
			_movieSurface = nullptr;
		}
	}
#endif

	_playingMovie = false;
}

bool SputSmush::videoIsPlayingMovie() const {
	if (_playFromMenu && !_isVideoVisible)
		return _videoExtraFramesLeft != 0;

	return _playingMovie;
}

int SputSmush::videoGetMovieTimeMs() const {
	return (int)(_frameTime * 1000.0f);
}

void SputSmush::freeVideoResources() {
#ifdef USE_THEORADEC
	delete _videoClip;
	_videoClip = nullptr;
	_videoPendingFrame = nullptr;

	delete _theoraConverter;
	_theoraConverter = nullptr;

	delete _theoraAudioConverter;
	_theoraAudioConverter = nullptr;

	if (_movieSurface) {
		_movieSurface->free();
		delete _movieSurface;
		_movieSurface = nullptr;
	}

	if (_releasedSurface) {
		_releasedSurface->free();
		delete _releasedSurface;
		_releasedSurface = nullptr;
	}
#endif
}

void SputSmush::videoUpdate(int force) {
#ifdef USE_THEORADEC
	if (_videoClip || _videoExtraFramesLeft || force) {
		float gain = (float)((double)_wuRegistry->getFloat("VoiceVolume", 127.0f) / 127.0);

		if (_commentaryDucking && gain > 0.05f)
			gain = 0.05f;

		if (_videoClip && _theoraAudioConverter)
			_theoraAudioConverter->setAudioGain(gain);

		if (_movieSurface && _videoClip) {
			float timePosition = (float)_videoClip->getTime() / 1000.0f;

			if ((double)(uint32)_videoDuration <= (double)timePosition * 1000.0) {
				if (_isVideoVisible)
					_videoExtraFramesLeft = 1;

				_isVideoVisible = false;
			}

			if (videoAudioDone()) {
				_isVideoVisible = false;
				_playingMovie = false;
			}

			// The decoder is read in every call, also when the frame is not shown, to feed the audio
			const Graphics::Surface *frame = videoGetNextFrame();

			if (frame && _isVideoVisible) {
				int rows = MIN<int>(frame->h, _movieSurface->h);
				int rowBytes = MIN<int>(frame->w, _movieSurface->w) * 4;

				for (int y = 0; y < rows; y++)
					memcpy(_movieSurface->getBasePtr(0, y), frame->getBasePtr(0, y), rowBytes);

				_movieSerial++;
			}

			if (_isVideoVisible || _videoExtraFramesLeft) {
				if (_videoExtraFramesLeft)
					_videoExtraFramesLeft--;

				queueHdFrame();
			} else if (_playFromMenu && (force & 1)) {
				queueHdFrame();
			}

			_frameTime = (float)_videoClip->getTime() / 1000.0f;
		}
	}
#endif
}

const Graphics::Surface *SputSmush::videoGetNextFrame() {
#ifdef USE_THEORADEC
	float clipTime = (float)_videoClip->getTime() / 1000.0f;

	for (;;) {
		if (!_videoPendingFrame) {
			_videoPendingFrame = videoDecodeNextFrame();

			if (!_videoPendingFrame)
				return nullptr;
		}

		float start = videoFrameStartTime(_videoClip->getCurFrame());

		if (clipTime < start)
			return nullptr;

		const Graphics::Surface *frame = _videoPendingFrame;

		if (!((double)start + (double)_videoFrameDuration < (double)clipTime)) {
			_videoPendingFrame = nullptr;

			return frame;
		}

		// A late frame is dropped when another one follows, the last one is shown anyway
		const Graphics::Surface *next = videoDecodeNextFrame();

		if (!next) {
			_videoPendingFrame = nullptr;

			return frame;
		}

		_videoPendingFrame = next;
	}
#else
	return nullptr;
#endif
}

const Graphics::Surface *SputSmush::videoDecodeNextFrame() {
#ifdef USE_THEORADEC
	int lastFrame = _videoClip->getCurFrame();
	const Graphics::Surface *frame = _videoClip->decodeNextFrame();

	// Without a further frame the decoder returns the picture it already has
	if (_videoClip->getCurFrame() == lastFrame)
		frame = nullptr;

	return frame;
#else
	return nullptr;
#endif
}

float SputSmush::videoFrameStartTime(int frame) const {
	float granuleTime = (float)((double)(frame + 1) * ((double)_videoFpsDenominator / (double)_videoFpsNumerator));

	return granuleTime - _videoFrameDuration;
}

bool SputSmush::videoAudioDone() {
#ifdef USE_THEORADEC
	if (!_videoClip->endOfVideo())
		return false;

	uint32 now = g_system->getMillis();

	if (!_videoEnded) {
		_videoEnded = true;
		_videoEndedTime = now;
	}

	return now - _videoEndedTime > 500;
#else
	return false;
#endif
}

void SputSmush::queueHdFrame() {
#ifdef USE_THEORADEC
	if (!_movieSurface)
		return;

	HdCanvas *canvas = g_grim->getHdCanvas();

	if (!canvas)
		return;

	const Graphics::Surface *frame = _movieSurface;

	canvas->queueMovie((const byte *)frame->getPixels(), frame->w, frame->h, _movieSerial);
#endif
}

} // End of namespace Grim
