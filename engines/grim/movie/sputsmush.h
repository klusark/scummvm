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

#ifndef GRIM_SPUTSMUSH_H
#define GRIM_SPUTSMUSH_H

#include "common/scummsys.h"

#include "engines/grim/movie/smush_play.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/std/std_sys.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Graphics {
struct Surface;
}

namespace Video {
class TheoraDecoder;
}

namespace Grim {

class LegacyImporter;
class Res;
class SputRender;
class TheoraAudioConverter;
class TheoraConverter;
class WuRegistry;

class SputSmush : public SmushPlayVideo {
	friend class LegacyImporter;

public:
	SputSmush(Res *res, StdDisplay *stdDisplay, SputRender *sputRender, StdVBuffer *mainGfxBuffer, WuRegistry *wuRegistry);
	virtual ~SputSmush();

	virtual int getMovieTime();
	virtual int initialize(SysStruct *hs);
	virtual int save(Common::WriteStream *stream);
	virtual int restore(Common::ReadStream *stream);
	int saveRemastered(Common::WriteStream *stream);
	int restoreRemastered(Common::ReadStream *stream);
	virtual int startup();
	virtual int shutdown();
	virtual int terminate();
	virtual int periodic();
	virtual int setTarget(StdVBuffer *buffer);
	virtual int startMovie(const char *name, int looping, int x, int y);
	virtual int stopMovie();
	virtual int getMovieLength(const char *name);
	int startVideo(int width, int height, int depth, int pixelFormat) override;
	int stopVideo() override;
	int lockMovieBuffer(LecBitmap *bitmap, StdRect *rect) override;
	int unlockMovieBuffer(LecBitmap *bitmap, StdRect *rect) override;
	int setVideoPalette(uint32 *palette, int start, int count) override;
	virtual int moviePlaying();
	virtual int pause(int pause);
	virtual void blitFrame() {}

	bool isHdMoviePlaying() const { return videoIsPlayingMovie(); }
	bool videoIsPlayingMovie() const;
	bool videoIsVisible() const { return _isVideoVisible; }

	void setCommentaryDucking(bool ducking) { _commentaryDucking = ducking; }

protected:
	bool videoPlay(const char *name, bool playFromMenu);
	void videoStop();
	void videoUpdate(int force);
	const Graphics::Surface *videoGetNextFrame();
	const Graphics::Surface *videoDecodeNextFrame();
	float videoFrameStartTime(int frame) const;
	bool videoAudioDone();
	int videoGetMovieTimeMs() const;
	static int getVideoDurationMs(const char *name);
	static bool isHdMovieName(const char *name);
	void queueHdFrame();
	void freeVideoResources();

	SmushPlay *_smushPlay;
	Res *_res;
	StdDisplay *_stdDisplay;
	SputRender *_sputRender;
	StdVBuffer *_mainGfxBuffer;
	WuRegistry *_wuRegistry;
	SysStruct *_hs;
	int _started;
	StdVBuffer *_presentBuf;
	int _buf;
	int _playing;
	int _paused;
	int _abort;
	char _curMovieFile[128];
	int _looping;
	int _x;
	int _y;
	uint _startTime;
	uint _lastPeriodicTime;

	Video::TheoraDecoder *_videoClip;
	Graphics::Surface *_movieSurface;
	Graphics::Surface *_releasedSurface;
	float _frameTime;
	bool _isVideoVisible;
	bool _playFromMenu;
	int _videoExtraFramesLeft;
	int _videoDuration;
	bool _playingMovie;
	bool _wasPlayingMovie;
	bool _commentaryDucking;

	const Graphics::Surface *_videoPendingFrame;
	int _videoFpsNumerator;
	int _videoFpsDenominator;
	float _videoFrameDuration;
	bool _videoEnded;
	uint32 _videoEndedTime;

	int _movieSerial;

	TheoraConverter *_theoraConverter;

	TheoraAudioConverter *_theoraAudioConverter;
};

} // End of namespace Grim

#endif
