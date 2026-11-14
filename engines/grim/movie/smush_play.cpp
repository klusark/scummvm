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

#include "common/compression/deflate.h"
#include "common/endian.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/timer.h"

#include "graphics/surface.h"

#include "engines/grim/grim.h"
#include "engines/grim/movie/codecs/smush_decoder.h"
#include "engines/grim/movie/smush_animator.h"
#include "engines/grim/movie/smush_mixer.h"
#include "engines/grim/movie/smush_play.h"
#include "engines/grim/movie/smush_sound.h"
#include "engines/grim/movie/smush_timer.h"
#include "engines/grim/movie/smush_wave.h"

namespace Grim {

SmushPlay::SmushPlay(bool remastered) :
		_hs(nullptr), _video(nullptr), _initialized(0), _started(0), _timerInstalled(0), _movieIsPlaying(0), _movieFlags(0),
		_internalBitmapPointer(nullptr), _translationTable(nullptr), _colorLookupConfig(0) {
	memset(&_movieInfo, 0, sizeof(_movieInfo));
	memset(_curFileName, 0, sizeof(_curFileName));
	memset(&_frameRect, 0, sizeof(_frameRect));
	memset(&_srcRect, 0, sizeof(_srcRect));
	memset(&_internalBitmap, 0, sizeof(_internalBitmap));

	_decoder = new SmushDecoder();
	_timer = new SmushTimer();
	_driver = new SmushDriver(_timer, remastered);
	_sound = new SmushSound(_timer, _driver);
	_wave = new SmushWave(_sound);
	_animPlay = new SmushAnimPlay(_timer, _sound);
	_animator = new SmushAnimator(_decoder);
	_decoder->setWave(_wave);
}

SmushPlay::~SmushPlay() {
	if (_started)
		sysShutdown();

	Common::StackLock lock(_mutex);

	delete _animPlay;
	_animPlay = nullptr;
	delete _animator;
	_animator = nullptr;
	delete _wave;
	_wave = nullptr;
	delete _sound;
	_sound = nullptr;
	delete _driver;
	_driver = nullptr;
	delete _timer;
	_timer = nullptr;
	delete _decoder;
	_decoder = nullptr;
}

int SmushPlay::sysInitialize(SysStruct *hs) {
	_hs = hs;

	if (_initialized)
		return 1;

	_initialized = 1;

	return 0;
}

void SmushPlay::sysTerminate() {
	_initialized = 0;
}

int SmushPlay::sysStartup() {
	if (!_initialized || _started)
		return 1;

	{
		Common::StackLock lock(_mutex);

		if (_timer->moduleStartup())
			return 1;

		if (_sound->moduleStartup()) {
			_timer->moduleShutdown();
			return 1;
		}

		if (_wave->moduleStartup()) {
			_sound->moduleShutdown();
			_timer->moduleShutdown();
			return 1;
		}
	}

	g_system->getTimerManager()->installTimerProc(&timerCallback, 10000, this, "smushPlay");
	_timerInstalled = 1;
	_started = 1;

	return 0;
}

void SmushPlay::sysShutdown() {
	if (!_started)
		return;

	if (_timerInstalled) {
		g_system->getTimerManager()->removeTimerProc(&timerCallback);
		_timerInstalled = 0;
	}

	Common::StackLock lock(_mutex);

	_wave->moduleShutdown();
	_sound->moduleShutdown();
	_timer->moduleShutdown();
	_started = 0;
}

void SmushPlay::sysSetVideoCallbacks(SmushPlayVideo *video) {
	_video = video;
}

void SmushPlay::sysConfigureColorLookup(int config) {
	_colorLookupConfig = config;
}

void SmushPlay::timerCallback(void *context) {
	SmushPlay *play = static_cast<SmushPlay *>(context);
	Common::StackLock lock(play->_mutex);

	play->_timer->timerThread();
}

int SmushPlay::startMovie(const char *name, int flags, int bufferBytes, int x, int y, int width, int height, int offset, int pixelFormat) {
	Common::StackLock lock(_mutex);

	if (!_video)
		return 1;

	Common::SeekableReadStream *stream = openFile(name);

	if (!stream)
		return 1;

	if (strlen(name) >= sizeof(_curFileName))
		warning("SmushPlay::startMovie(): the movie name is too long");

	strncpy(_curFileName, name, sizeof(_curFileName) - 1);
	_curFileName[sizeof(_curFileName) - 1] = '\0';

	stream->seek(offset, SEEK_CUR);

	if (getInfo(stream, &_movieInfo)) {
		delete stream;
		return 1;
	}

	delete stream;
	stream = nullptr;

	if (_video->startVideo(width, height, 16, pixelFormat))
		return 1;

	_movieFlags = flags & 0xFFFFFFF;

	if (openAnim(name, 15)) {
		_video->stopVideo();
		return 1;
	}

	if (x < 0)
		_frameRect.x = _movieInfo.x;
	else
		_frameRect.x = x;

	if (y < 0)
		_frameRect.y = _movieInfo.y;
	else
		_frameRect.y = y;

	_frameRect.width = _movieInfo.width;
	_frameRect.height = _movieInfo.height;
	_movieIsPlaying = 1;

	return 0;
}

int SmushPlay::stopMovie() {
	Common::StackLock lock(_mutex);

	if (_movieIsPlaying) {
		closeAnim();
		_sound->souEngineStopAll();
		_internalBitmapPointer = nullptr;
	}

	_movieIsPlaying = 0;

	if (_video)
		_video->stopVideo();

	return 0;
}

int SmushPlay::updateMovie(int *finished) {
	Common::StackLock lock(_mutex);
	LecBitmap bitmap;
	StdRect rect;

	memset(&bitmap, 0, sizeof(bitmap));
	memset(&rect, 0, sizeof(rect));

	if (!_video) {
		warning("SmushPlay::updateMovie(): the video callbacks are not set");
		*finished = 1;
		return 1;
	}

	int state = _animPlay->anmRender(this);

	if (!_video->lockMovieBuffer(&bitmap, &rect)) {
		blit(&bitmap, _internalBitmapPointer, &rect);
		_video->unlockMovieBuffer(&bitmap, &rect);
	}

	if (state < 0 || state == 4) {
		_animPlay->anmStop();
		_internalBitmapPointer = nullptr;

		if (_movieFlags & 0x10) {
			*finished = 0;

			int fps = 15;

			if (_movieInfo.frameDelay && !g_grim->isRemastered())
				fps = (_movieInfo.frameDelay / 2 + 1000000) / _movieInfo.frameDelay;

			restartAnim(fps);
			_movieIsPlaying = 1;
		} else {
			*finished = 1;
			_decoder->close();
			_movieIsPlaying = 0;
		}
	} else {
		*finished = 0;
		_movieIsPlaying = 1;
	}

	return 0;
}

int SmushPlay::pauseMovie() {
	Common::StackLock lock(_mutex);

	if (_movieIsPlaying)
		_animator->animatorPause(true);

	return 0;
}

int SmushPlay::resumeMovie() {
	Common::StackLock lock(_mutex);

	if (_movieIsPlaying)
		_animator->animatorPause(false);

	return 0;
}

int SmushPlay::getMovieTime() {
	Common::StackLock lock(_mutex);

	if (_initialized && _started && _movieIsPlaying)
		return _sound->souEngineQueryTime(0);

	return 0;
}

void SmushPlay::setTranslationTable(const uint16 *table) {
	_translationTable = table;
}

void SmushPlay::getFrameRect(StdRect *frameRect, StdRect *srcRect) {
	if (frameRect)
		*frameRect = _frameRect;

	if (srcRect)
		*srcRect = _srcRect;
}

int SmushPlay::getMovieLength(const char *path) {
	Common::SeekableReadStream *stream = openFile(path);

	if (!stream)
		return 0;

	SmushMovieInfo info;

	memset(&info, 0, sizeof(info));
	getInfo(stream, &info);
	delete stream;
	stream = nullptr;

	if (g_grim->useRemasterFixes())
		return (int)((double)info.frames * (double)66.666664f);

	return (int)((double)info.frames * 66.666664);
}

int SmushPlay::animatorRender(int flags) {
	int state = _animator->animatorRender(flags);

	if (state < 0)
		return state;

	const Graphics::Surface *surface = _decoder->getFrameSurface();

	_internalBitmap.pixels = (byte *)surface->getPixels();
	_internalBitmap.width = surface->w;
	_internalBitmap.height = surface->h;
	_internalBitmap.bytesPerPixel = surface->format.bytesPerPixel;
	_internalBitmap.bitsPerPixel = 16;
	_internalBitmap.pitchInBytes = surface->pitch;
	fakeBlit(&_internalBitmap, nullptr);

	return state;
}

void SmushPlay::resetFrameCounter() {
	_decoder->resetBlocky16Frames();
}

int SmushPlay::getFrameCounter() {
	return _decoder->getBlocky16Frames();
}

Common::SeekableReadStream *SmushPlay::openFile(const char *path) {
	if (!_hs) {
		warning("SmushPlay::openFile(): the module is not initialized");
		return nullptr;
	}

	Common::SeekableReadStream *stream = _hs->openFile(path);

	if (!stream)
		return nullptr;

	return Common::wrapCompressedReadStream(stream);
}

int SmushPlay::getInfo(Common::SeekableReadStream *stream, SmushMovieInfo *info) {
	if (!stream)
		return 0;

	int32 position = stream->pos();

	stream->seek(16, SEEK_SET);
	info->version0 = stream->readByte();
	info->version1 = stream->readByte();
	info->frames = stream->readSint16LE();
	info->x = stream->readSint16LE();
	info->y = stream->readSint16LE();
	info->width = stream->readSint16LE();
	info->height = stream->readSint16LE();
	info->reserved12 = stream->readSint16LE();
	info->frameDelay = stream->readSint32LE();
	info->maxFrameSize = stream->readSint32LE();
	stream->read(info->palette, sizeof(info->palette));
	info->reserved1046 = stream->readSint16LE();

	if (info->width <= 0 || info->height <= 0 || info->maxFrameSize <= 0)
		return -2;

	stream->seek(position, SEEK_SET);

	return 0;
}

int SmushPlay::openAnim(const char *path, int fps) {
	Common::SeekableReadStream *stream = openFile(path);

	if (!stream)
		return 1;

	if (!_decoder->loadStream(stream))
		return 1;

	_decoder->start();

	if (_animPlay->anmStart(fps)) {
		_decoder->close();
		return 1;
	}

	_animator->animatorNewAnim();

	return 0;
}

void SmushPlay::restartAnim(int fps) {
	_decoder->rewind();
	_decoder->start();
	_animPlay->anmStart(fps);
	_animator->animatorNewAnim();
}

void SmushPlay::closeAnim() {
	_animPlay->anmStop();
	_decoder->close();
}

int SmushPlay::fakeBlit(LecBitmap *bitmap, const uint16 *table) {
	_internalBitmapPointer = bitmap;

	if (!_translationTable)
		_translationTable = table;

	return 0;
}

int SmushPlay::blit(LecBitmap *dest, LecBitmap *source, StdRect *rect) {
	if (!dest || !source)
		return 0;

	if (_movieFlags & 1) {
		_movieFlags &= ~6;

		if (_frameRect.width <= rect->width / 2)
			_movieFlags |= 4;
		else if (_frameRect.height < rect->height)
			_movieFlags |= 2;
	}

	int x = _frameRect.x;
	int y = _frameRect.y;
	int rowScale = ((_movieFlags & 6) != 0) + 1;
	int columnScale = ((_movieFlags & 4) != 0) + 1;
	int width = _frameRect.width * columnScale;
	int height = rowScale * _frameRect.height;

	if (x < rect->x) {
		width += x - rect->x;
		x = rect->x;
	}

	if (x + width > rect->x + rect->width)
		width = rect->x + rect->width - x;

	if (y < rect->y) {
		height += y - rect->y;
		y = rect->y;
	}

	if (y + height > rect->y + rect->height)
		height = rect->y + rect->height - y;

	int rows = height / rowScale;
	int rowBytes = source->bytesPerPixel * (width / columnScale);

	if (rows < 0 || rowBytes < 0) {
		warning("SmushPlay::blit(): the movie is outside of the target");
		return 0;
	}

	byte *destPixels = dest->pixels + dest->pitchInBytes * rowScale * y + columnScale * x * dest->bytesPerPixel;

	if (_movieFlags & 4)
		blitDouble(source->pixels, source->pitchInBytes, destPixels, dest->pitchInBytes, rowBytes, rows);
	else if (_movieFlags & 2)
		blitDoubleRows(source->pixels, source->pitchInBytes, destPixels, dest->pitchInBytes, rowBytes, rows);
	else
		blitCopy(source->pixels, source->pitchInBytes, destPixels, dest->pitchInBytes, rowBytes, rows);

	return 0;
}

void SmushPlay::blitDouble(const byte *source, int sourcePitch, byte *dest, int destPitch, int rowBytes, int rows) {
	if (rows == 0)
		return;

	for (int row = 0; row < rows; row++) {
		const byte *sourceEnd = source + rowBytes;

		do {
			uint32 value = READ_LE_UINT32(source);
			uint32 low = (value & 0xFF) | ((value << 8) & 0xFF0000);
			uint32 high = (value >> 8) & 0xFF00;
			uint32 top = value & 0xFF000000;
			uint32 first = low | (low << 8);
			uint32 second = ((high | top) >> 8) | high | top;

			WRITE_LE_UINT32(dest, first);
			WRITE_LE_UINT32(dest + destPitch, first);
			WRITE_LE_UINT32(dest + 4, second);
			WRITE_LE_UINT32(dest + destPitch + 4, second);
			source += 4;
			dest += 8;
		} while (source < sourceEnd);

		source += sourcePitch - rowBytes;
		dest += 2 * destPitch - 2 * rowBytes;
	}
}

void SmushPlay::blitDoubleRows(const byte *source, int sourcePitch, byte *dest, int destPitch, int rowBytes, int rows) {
	int numDwords = rowBytes >> 2;

	for (int row = 0; row < rows; row++) {
		for (int i = 0; i < numDwords; i++) {
			uint32 value = READ_UINT32(source + 4 * i);

			WRITE_UINT32(dest + 4 * i, value);
			WRITE_UINT32(dest + destPitch + 4 * i, value);
		}

		source += sourcePitch;
		dest += 2 * destPitch;
	}
}

void SmushPlay::blitCopy(const byte *source, int sourcePitch, byte *dest, int destPitch, int rowBytes, int rows) {
	int numPixels = (rowBytes >> 2) * 2;

	for (int row = 0; row < rows; row++) {
		const uint16 *sourceRow = (const uint16 *)(source + sourcePitch * row);
		uint16 *destRow = (uint16 *)(dest + destPitch * row);

		if (_translationTable) {
			for (int i = 0; i < numPixels; i++)
				destRow[i] = _translationTable[sourceRow[i]];
		} else {
			for (int i = 0; i < numPixels; i++)
				destRow[i] = sourceRow[i];
		}
	}
}

} // End of namespace Grim
