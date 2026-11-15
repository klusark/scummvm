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

#include "common/scummsys.h"

#ifdef USE_MPEG2

#include "common/endian.h"
#include "common/memstream.h"
#include "common/rect.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "audio/audiostream.h"
#include "audio/decoders/raw.h"

#include "image/codecs/mpeg.h"

#include "engines/grim/emi/movie/pss/pssdecoder.h"

namespace Grim {

// Frame periods of the MPEG-2 frame rate codes, in ticks of the 27 MHz clock
const uint32 PssDecoder::kFramePeriods[kNumFrameRates] = {
	kDefaultFramePeriod, 1126125, 1125000, 1080000, 900900, 900000, 540000, 450450, 450000
};

PssDecoder::PssDecoder() {
	_stream = nullptr;
	_mpegDecoder = nullptr;
	_endOfStream = false;
	_videoFinished = false;
	_needsEndCode = false;
	_packetData = nullptr;

	_videoData = nullptr;
	_videoSize = 0;
	_videoCapacity = 0;
	_videoSearchPos = 1;
	_videoBase = 0;
	_demuxTicks = 0;
	_audioAccepted = 0;
	_chunkTicks = 0;

	_ringPending = 0;
	_ringDisplayOn = false;
	_ringHasDrawn = false;
	_ringTime = 0;
	_ringLastDraw = 0;
	_framesDrawn = 0;

	_width = 0;
	_height = 0;
	_framePeriod = kDefaultFramePeriod;
	_framesCreated = false;
	memset(_frameNumbers, 0, sizeof(_frameNumbers));
	_queueHead = 0;
	_numQueued = 0;
	_nextFrameNumber = 0;

	_audioStream = nullptr;
	_audioHeaderRead = false;
	_audioStarted = false;
	_volume = Audio::Mixer::kMaxChannelVolume;
	_audioRate = 0;
	_audioChannels = 0;
	_audioInterleave = 0;
	_audioBlock = nullptr;
	_audioBlockFill = 0;
	_audioFramesQueued = 0;

	_clockTime = 0;
	_clockMillis = 0;
}

PssDecoder::~PssDecoder() {
	stop();

	for (int i = 0; i < kMaxQueuedFrames; i++)
		_frames[i].free();

	_shownFrame.free();

	delete _mpegDecoder;
	delete _stream;
	free(_packetData);
	free(_videoData);
	delete[] _audioBlock;
}

bool PssDecoder::loadStream(Common::SeekableReadStream *stream, const Graphics::PixelFormat &format) {
	_stream = stream;

	if (_stream->readUint32BE() != kStartCodePack) {
		warning("PssDecoder::loadStream(): the movie is not an MPEG program stream");

		return false;
	}

	_stream->seek(0);

	_mpegDecoder = new Image::MPEGDecoder();
	_format = format;

	if (!_mpegDecoder->setOutputPixelFormat(format)) {
		warning("PssDecoder::loadStream(): the decoder cannot use the draw context format");
		_format = _mpegDecoder->getPixelFormat();
	}

	_packetData = (byte *)malloc(kMaxPacketSize);
	_videoCapacity = kInitialVideoBuffer;
	_videoData = (byte *)malloc(_videoCapacity);

	return true;
}

void PssDecoder::start(byte volume) {
	_volume = volume;
	_clockTime = 0;
	_clockMillis = g_system->getMillis();
}

void PssDecoder::stop() {
	if (_audioStarted)
		g_system->getMixer()->stopHandle(_audioHandle);

	// The mixer owns the audio stream and deletes it with the handle
	_audioStream = nullptr;
	_audioStarted = false;
}

void PssDecoder::detachAudio() {
	// The queue is already finished, the mixer plays it to the end and then deletes it
	_audioStream = nullptr;
	_audioStarted = false;
}

const Graphics::Surface *PssDecoder::update() {
	uint32 time = getTime();
	bool frameShown = false;

	for (;;) {
		fillQueue();

		if (!_numQueued)
			break;

		// Pictures still in the ring when the decoder ends are never drawn
		if (_frameNumbers[_queueHead] >= _framesDrawn) {
			if (_videoFinished)
				_numQueued = 0;

			break;
		}

		if (getFrameTime(_frameNumbers[_queueHead]) > time)
			break;

		int next = (_queueHead + 1) % kMaxQueuedFrames;
		bool nextDue = _numQueued > 1 && _frameNumbers[next] < _framesDrawn && getFrameTime(_frameNumbers[next]) <= time;

		// When the video is late, the frames in between are decoded but not drawn
		if (!nextDue) {
			_shownFrame.copyRectToSurface(_frames[_queueHead], 0, 0, Common::Rect(_width, _height));
			frameShown = true;
		}

		_queueHead = next;
		_numQueued--;
	}

	if (!frameShown)
		return nullptr;

	return &_shownFrame;
}

bool PssDecoder::isFinished() {
	return _videoFinished && !_numQueued;
}

uint32 PssDecoder::getTime() {
	uint32 now = g_system->getMillis();

	if (isAudioPlaying()) {
		uint32 elapsed = g_system->getMixer()->getSoundElapsedTime(_audioHandle);

		if (elapsed > 0) {
			_clockTime = elapsed;
			_clockMillis = now;

			return elapsed;
		}
	}

	// The system clock runs the movie until the audio starts and after it ends
	return _clockTime + (now - _clockMillis);
}

uint64 PssDecoder::getArrival(uint64 offset) const {
	for (uint i = 0; i < _packets.size(); i++) {
		if (_packets[i].esEnd > offset)
			return _packets[i].arrival;
	}

	return _demuxTicks;
}

bool PssDecoder::canDraw() const {
	return !_ringHasDrawn || _ringTime >= _ringLastDraw + kVblankTicks;
}

void PssDecoder::drawOldest() {
	// A draw ends at the next vblank and the next one can start one vblank later
	_ringTime = (_ringTime / kVblankTicks + 1) * kVblankTicks;
	_ringPending--;
	_framesDrawn++;
	_ringLastDraw = _ringTime;
	_ringHasDrawn = true;
}

void PssDecoder::simulateOutput(uint64 availTicks) {
	while (_ringPending == kRingSize) {
		if (!canDraw())
			_ringTime = _ringLastDraw + kVblankTicks;

		drawOldest();
	}

	if (availTicks > _ringTime)
		_ringTime = availTicks;

	_ringPending++;

	// The drawing starts when the ring is full
	if (_ringPending == kRingSize)
		_ringDisplayOn = true;

	if (_ringDisplayOn && canDraw())
		drawOldest();
}

uint32 PssDecoder::getFramesDecoded() const {
	return _nextFrameNumber;
}

void PssDecoder::fillQueue() {
	for (;;) {
		// The sequence end can return two frames, so a picture is decoded only when two slots are free
		while (_numQueued + 2 <= kMaxQueuedFrames && decodeNextChunk()) {
		}

		if (_endOfStream)
			return;

		if (_numQueued + 2 > kMaxQueuedFrames && !needsAudio())
			return;

		readNextPacket();
	}
}

bool PssDecoder::needsAudio() {
	if (_videoSize >= kMaxBufferedVideo)
		return false;

	// Keep reading until the first audio packet shows up
	if (!_audioHeaderRead)
		return true;

	if (!_audioStream || !isAudioPlaying())
		return false;

	uint32 queuedMs = (uint32)((uint64)_audioFramesQueued * 1000 / _audioRate);
	uint32 playedMs = g_system->getMixer()->getSoundElapsedTime(_audioHandle);

	return queuedMs < playedMs + kAudioLeadMs;
}

bool PssDecoder::isAudioPlaying() {
	return _audioStarted && g_system->getMixer()->isSoundHandleActive(_audioHandle);
}

bool PssDecoder::readNextPacket() {
	uint32 startCode = findNextStartCode();

	if (!startCode || startCode == kStartCodeProgramEnd) {
		endStream();

		return false;
	}

	if (startCode == kStartCodePack) {
		skipPackHeader();

		return true;
	}

	// Only the system header and the packets have a length, anything else is skipped until the next start code
	if (startCode < kStartCodeSystemHeader)
		return true;

	uint32 length = _stream->readUint16BE();

	if (_stream->eos()) {
		endStream();

		return false;
	}

	if (startCode != kStartCodeVideo && startCode != kStartCodePrivateStream1) {
		_stream->skip(length);

		return true;
	}

	if (_stream->read(_packetData, length) != length) {
		endStream();

		return false;
	}

	// The PTS is skipped with the rest of the PES header, the frames follow the audio clock
	if (length < kPesHeaderSize || (_packetData[0] & 0xC0) != 0x80) {
		warning("PssDecoder::readNextPacket(): the packet has no MPEG-2 header");

		return true;
	}

	uint32 offset = kPesHeaderSize + _packetData[kPesHeaderSize - 1];

	if (offset > length)
		return true;

	if (startCode == kStartCodeVideo)
		appendVideo(_packetData + offset, length - offset);
	else
		readAudio(_packetData + offset, length - offset);

	return true;
}

uint32 PssDecoder::findNextStartCode() {
	uint32 state = 0xFFFFFFFF;

	for (;;) {
		byte value = _stream->readByte();

		if (_stream->eos())
			return 0;

		state = (state << 8) | value;

		if ((state & 0xFFFFFF00) == 0x100)
			return state;
	}
}

void PssDecoder::skipPackHeader() {
	byte value = _stream->readByte();

	if ((value & 0xC0) == 0x40) {
		// The last byte of an MPEG-2 pack header holds the stuffing length
		_stream->skip(8);
		_stream->skip(_stream->readByte() & 7);
	} else {
		_stream->skip(7);
	}
}

void PssDecoder::endStream() {
	if (_endOfStream)
		return;

	_endOfStream = true;

	// The mixer can delete the stream once it is finished
	if (_audioStream && isAudioPlaying())
		_audioStream->finish();

	_audioStream = nullptr;
}

void PssDecoder::appendVideo(const byte *data, uint32 size) {
	if (_videoSize + size > _videoCapacity) {
		while (_videoSize + size > _videoCapacity)
			_videoCapacity *= 2;

		_videoData = (byte *)realloc(_videoData, _videoCapacity);

		if (!_videoData)
			error("PssDecoder::appendVideo(): cannot grow the video buffer to %u bytes", _videoCapacity);
	}

	memcpy(_videoData + _videoSize, data, size);
	_videoSize += size;

	VideoPacket packet;

	packet.esEnd = _videoBase + _videoSize;
	packet.arrival = _demuxTicks;
	_packets.push_back(packet);
}

uint32 PssDecoder::findChunkEnd() {
	// A chunk ends where the next picture, sequence header, group or sequence end starts
	uint32 pos = _videoSearchPos;

	while (pos + 3 < _videoSize) {
		if (!_videoData[pos] && !_videoData[pos + 1] && _videoData[pos + 2] == 1) {
			byte code = _videoData[pos + 3];

			if (code == kCodePicture || code == kCodeSequenceHeader || code == kCodeGroup || code == kCodeSequenceEnd)
				return pos;
		}

		pos++;
	}

	_videoSearchPos = pos;

	return 0;
}

bool PssDecoder::decodeNextChunk() {
	if (_videoFinished)
		return false;

	uint32 end = findChunkEnd();

	if (!end) {
		if (!_endOfStream)
			return false;

		if (!_videoSize) {
			if (_needsEndCode) {
				// libmpeg2 holds back the last reference picture until the sequence ends
				byte endCode[4] = { 0, 0, 1, kCodeSequenceEnd };

				decodeChunk(endCode, sizeof(endCode), _demuxTicks);

				return true;
			}

			_videoFinished = true;

			return false;
		}

		end = _videoSize;
	}

	// The picture before this chunk is complete when the 4th byte of the start code has arrived
	decodeChunk(_videoData, end, getArrival(_videoBase + 3));

	memmove(_videoData, _videoData + end, _videoSize - end);
	_videoSize -= end;
	_videoBase += end;
	_videoSearchPos = 1;

	while (_packets.size() && _packets[0].esEnd <= _videoBase)
		_packets.remove_at(0);

	return true;
}

void PssDecoder::decodeChunk(const byte *data, uint32 size, uint64 availTicks) {
	_chunkTicks = availTicks;

	if (size >= 4 && !data[0] && !data[1] && data[2] == 1) {
		byte code = data[3];

		if (code == kCodeSequenceHeader)
			readSequenceHeader(data, size);
		else if (code == kCodePicture)
			_needsEndCode = true;
		else if (code == kCodeSequenceEnd)
			_needsEndCode = false;
	}

	Common::MemoryReadStream packet(data, size);
	uint32 framePeriod = 0;
	int slot = (_queueHead + _numQueued) % kMaxQueuedFrames;
	Graphics::Surface *dst = nullptr;

	if (_framesCreated)
		dst = &_frames[slot];

	if (!_mpegDecoder->decodePacket(packet, framePeriod, dst) || !dst)
		return;

	queueFrame(slot);

	// The sequence end returns two frames in one call, and the second one overwrites the first
	uint32 count = framePeriod / _framePeriod;

	for (uint32 i = 1; i < count && _numQueued < kMaxQueuedFrames; i++) {
		int copySlot = (_queueHead + _numQueued) % kMaxQueuedFrames;

		_frames[copySlot].copyRectToSurface(_frames[slot], 0, 0, Common::Rect(_width, _height));
		queueFrame(copySlot);
	}
}

void PssDecoder::readSequenceHeader(const byte *data, uint32 size) {
	if (size < kSequenceHeaderSize || _framesCreated)
		return;

	_width = (data[4] << 4) | (data[5] >> 4);
	_height = ((data[5] & 0x0F) << 8) | data[6];

	int rateCode = data[7] & 0x0F;

	if (rateCode > 0 && rateCode < kNumFrameRates)
		_framePeriod = kFramePeriods[rateCode];

	for (int i = 0; i < kMaxQueuedFrames; i++)
		_frames[i].create(_width, _height, _format);

	_shownFrame.create(_width, _height, _format);
	_framesCreated = true;
}

void PssDecoder::queueFrame(int slot) {
	_frameNumbers[slot] = _nextFrameNumber++;
	_numQueued++;

	simulateOutput(_chunkTicks);
}

uint32 PssDecoder::getFrameTime(uint32 frame) const {
	return (uint32)((uint64)frame * _framePeriod / kClockTicksPerMs);
}

void PssDecoder::readAudio(const byte *data, uint32 size) {
	if (size < kAudioMarkerSize)
		return;

	uint32 bodyOffset = kAudioMarkerSize;

	if (!_audioHeaderRead) {
		_audioHeaderRead = true;

		if (!readAudioHeader(data, size, bodyOffset))
			return;
	}

	// The mixer deletes the stream if somebody else stops the handle
	if (_audioStream && !isAudioPlaying())
		_audioStream = nullptr;

	if (!_audioStream)
		return;

	// The stream is read as fast as the audio buffers drain, which they do at the audio rate
	_audioAccepted += size - bodyOffset;

	if (_audioAccepted > kAudioBufferBytes) {
		uint64 due = (_audioAccepted - kAudioBufferBytes) * kTicksPerSecond / ((uint64)_audioRate * _audioChannels * 2);

		if (due > _demuxTicks)
			_demuxTicks = due;
	}

	queueAudio(data + bodyOffset, size - bodyOffset);
}

bool PssDecoder::readAudioHeader(const byte *data, uint32 size, uint32 &bodyOffset) {
	// The first packet starts with an SShd header, followed by the SSbd tag and the body size
	if (size < kAudioHeaderDataOffset || READ_BE_UINT32(data + kAudioMarkerSize) != MKTAG('S', 'S', 'h', 'd')) {
		warning("PssDecoder::readAudioHeader(): the audio has no SShd header");

		return false;
	}

	uint32 headerSize = READ_LE_UINT32(data + kAudioHeaderSizeOffset);

	if (headerSize < kAudioHeaderMinSize || headerSize > size || kAudioHeaderDataOffset + headerSize + kAudioBodyHeaderSize > size) {
		warning("PssDecoder::readAudioHeader(): the SShd header size %u is not valid", headerSize);

		return false;
	}

	bodyOffset = kAudioHeaderDataOffset + headerSize + kAudioBodyHeaderSize;

	if (READ_BE_UINT32(data + bodyOffset - kAudioBodyHeaderSize) != MKTAG('S', 'S', 'b', 'd')) {
		warning("PssDecoder::readAudioHeader(): the audio has no SSbd tag");

		return false;
	}

	uint32 type = READ_LE_UINT32(data + kAudioTypeOffset);

	_audioRate = READ_LE_UINT32(data + kAudioRateOffset);
	_audioChannels = READ_LE_UINT32(data + kAudioChannelsOffset);
	_audioInterleave = READ_LE_UINT32(data + kAudioInterleaveOffset);

	if (type != kAudioTypePcm || !_audioRate || (_audioChannels != 1 && _audioChannels != 2) ||
			!_audioInterleave || (_audioInterleave & 1) || _audioInterleave > kMaxPacketSize) {
		warning("PssDecoder::readAudioHeader(): unsupported audio (type %u, %u Hz, %u channels, interleave %u)",
				type, _audioRate, _audioChannels, _audioInterleave);

		return false;
	}

	_audioBlock = new byte[_audioInterleave * _audioChannels];
	_audioBlockFill = 0;
	_audioStream = Audio::makeQueuingAudioStream(_audioRate, _audioChannels == 2);

	g_system->getMixer()->playStream(Audio::Mixer::kPlainSoundType, &_audioHandle, _audioStream, -1, _volume, 0, DisposeAfterUse::YES);
	_audioStarted = true;

	return true;
}

void PssDecoder::queueAudio(const byte *data, uint32 size) {
	uint32 blockSize = _audioInterleave * _audioChannels;
	uint32 numBlocks = (_audioBlockFill + size) / blockSize;

	if (!numBlocks) {
		memcpy(_audioBlock + _audioBlockFill, data, size);
		_audioBlockFill += size;

		return;
	}

	byte *output = (byte *)malloc(numBlocks * blockSize);
	uint32 outputSize = 0;

	while (size > 0) {
		uint32 copy = MIN<uint32>(size, blockSize - _audioBlockFill);

		memcpy(_audioBlock + _audioBlockFill, data, copy);
		_audioBlockFill += copy;
		data += copy;
		size -= copy;

		if (_audioBlockFill < blockSize)
			break;

		// Each block holds the samples of the left channel first, then those of the right one
		for (uint32 i = 0; i < _audioInterleave; i += 2) {
			for (uint32 channel = 0; channel < _audioChannels; channel++) {
				memcpy(output + outputSize, _audioBlock + channel * _audioInterleave + i, 2);
				outputSize += 2;
			}
		}

		_audioBlockFill = 0;
	}

	byte flags = Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN;

	if (_audioChannels == 2)
		flags |= Audio::FLAG_STEREO;

	_audioStream->queueBuffer(output, outputSize, DisposeAfterUse::YES, flags);
	_audioFramesQueued += outputSize / (2 * _audioChannels);
}

} // End of namespace Grim

#endif
