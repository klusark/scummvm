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

#ifndef GRIM_EMI_MOVIE_PSS_PSSDECODER_H
#define GRIM_EMI_MOVIE_PSS_PSSDECODER_H

#include "common/scummsys.h"

#ifdef USE_MPEG2

#include "common/array.h"

#include "audio/mixer.h"

#include "graphics/pixelformat.h"
#include "graphics/surface.h"

namespace Common {
class SeekableReadStream;
}

namespace Audio {
class QueuingAudioStream;
}

namespace Image {
class MPEGDecoder;
}

namespace Grim {

// Plays PSS movies (MPEG program streams with MPEG-2 video and PCM audio), one frame per picture
class PssDecoder {
public:
	enum {
		kMaxQueuedFrames = 12,
		kAudioLeadMs = 500,
		kMaxBufferedVideo = 8 * 1024 * 1024,
		kInitialVideoBuffer = 256 * 1024,
		kMaxPacketSize = 65535,
		kPesHeaderSize = 3,
		kSequenceHeaderSize = 8,
		kClockTicksPerMs = 27000,
		kDefaultFramePeriod = 1080000,
		kNumFrameRates = 9,
		kTicksPerSecond = 27000000,
		kVblankTicks = 540000,
		kRingSize = 5,
		kAudioBufferBytes = 98304
	};

	enum {
		kStartCodeProgramEnd = 0x1B9,
		kStartCodePack = 0x1BA,
		kStartCodeSystemHeader = 0x1BB,
		kStartCodePrivateStream1 = 0x1BD,
		kStartCodeVideo = 0x1E0
	};

	enum {
		kCodePicture = 0x00,
		kCodeSequenceHeader = 0xB3,
		kCodeSequenceEnd = 0xB7,
		kCodeGroup = 0xB8
	};

	enum {
		kAudioMarkerSize = 4,
		kAudioHeaderSizeOffset = 8,
		kAudioHeaderDataOffset = 12,
		kAudioTypeOffset = 12,
		kAudioRateOffset = 16,
		kAudioChannelsOffset = 20,
		kAudioInterleaveOffset = 24,
		kAudioHeaderMinSize = 16,
		kAudioBodyHeaderSize = 8,
		kAudioTypePcm = 1
	};

	PssDecoder();
	~PssDecoder();

	bool loadStream(Common::SeekableReadStream *stream, const Graphics::PixelFormat &format);
	void start(byte volume);
	void stop();
	void detachAudio();
	const Graphics::Surface *update();
	bool isFinished();
	uint32 getTime();
	uint32 getFramesDecoded() const;

private:
	void fillQueue();
	bool needsAudio();
	bool isAudioPlaying();
	bool readNextPacket();
	uint32 findNextStartCode();
	void skipPackHeader();
	void endStream();
	void appendVideo(const byte *data, uint32 size);
	uint32 findChunkEnd();
	bool decodeNextChunk();
	void decodeChunk(const byte *data, uint32 size, uint64 availTicks);
	void readSequenceHeader(const byte *data, uint32 size);
	void queueFrame(int slot);
	uint32 getFrameTime(uint32 frame) const;
	uint64 getArrival(uint64 offset) const;
	void simulateOutput(uint64 availTicks);
	bool canDraw() const;
	void drawOldest();
	void readAudio(const byte *data, uint32 size);
	bool readAudioHeader(const byte *data, uint32 size, uint32 &bodyOffset);
	void queueAudio(const byte *data, uint32 size);

	struct VideoPacket {
		uint64 esEnd;
		uint64 arrival;
	};

	static const uint32 kFramePeriods[kNumFrameRates];

	Common::SeekableReadStream *_stream;
	Image::MPEGDecoder *_mpegDecoder;
	Graphics::PixelFormat _format;
	bool _endOfStream;
	bool _videoFinished;
	bool _needsEndCode;
	byte *_packetData;

	byte *_videoData;
	uint32 _videoSize;
	uint32 _videoCapacity;
	uint32 _videoSearchPos;
	uint64 _videoBase;
	Common::Array<VideoPacket> _packets;
	uint64 _demuxTicks;
	uint64 _audioAccepted;
	uint64 _chunkTicks;

	int _ringPending;
	bool _ringDisplayOn;
	bool _ringHasDrawn;
	uint64 _ringTime;
	uint64 _ringLastDraw;
	uint32 _framesDrawn;

	int _width;
	int _height;
	uint32 _framePeriod;
	bool _framesCreated;
	Graphics::Surface _frames[kMaxQueuedFrames];
	uint32 _frameNumbers[kMaxQueuedFrames];
	int _queueHead;
	int _numQueued;
	uint32 _nextFrameNumber;
	Graphics::Surface _shownFrame;

	Audio::QueuingAudioStream *_audioStream;
	Audio::SoundHandle _audioHandle;
	bool _audioHeaderRead;
	bool _audioStarted;
	byte _volume;
	uint32 _audioRate;
	uint32 _audioChannels;
	uint32 _audioInterleave;
	byte *_audioBlock;
	uint32 _audioBlockFill;
	uint32 _audioFramesQueued;

	uint32 _clockTime;
	uint32 _clockMillis;
};

} // End of namespace Grim

#endif

#endif
