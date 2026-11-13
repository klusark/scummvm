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

#include "common/scummsys.h"	// for USE_THEORADEC

#ifdef USE_THEORADEC

#ifndef VIDEO_THEORA_DECODER_H
#define VIDEO_THEORA_DECODER_H

#include "common/rational.h"
#include "common/str.h"
#include "video/video_decoder.h"
#include "audio/mixer.h"
#include "graphics/surface.h"

#include <theora/theoradec.h>

#ifdef USE_TREMOR
#include <tremor/ivorbiscodec.h>
#else
#include <vorbis/codec.h>
#endif

namespace Common {
class SeekableReadStream;
}

namespace Audio {
class AudioStream;
class QueuingAudioStream;
}

namespace Video {

/**
 *
 * Decoder for Theora videos.
 * Video decoder used in engines:
 *  - ags
 *  - asylum
 *  - grim
 *  - pegasus
 *  - sword25
 *  - tetraedge
 *  - wintermute
 */
class TheoraDecoder : public VideoDecoder {
public:
	TheoraDecoder();
	virtual ~TheoraDecoder();

	/**
	 * Load a video file
	 * @param stream  the stream to load
	 */
	bool loadStream(Common::SeekableReadStream *stream) override;
	void close() override;

	/** Frames per second of the loaded video. */
	Common::Rational getFrameRate() const;

	/**
	 * Select the Vorbis audio stream by language, for files that carry
	 * several audio streams. A user comment of a stream is matched against
	 * the codes in order (for example "eng", "spa"): its language is the
	 * index of the first code that the comment contains. The first stream
	 * that has a comment with the language @p hint is played.
	 * If no stream matches, the first one is used, as without this call.
	 * Only the first six audio streams of the file are considered.
	 * The array of codes must stay valid until loadStream() returns.
	 * Must be called before loadStream().
	 */
	void setAudioLanguage(const char *const *codes, int numCodes, int hint) {
		_languageCodes = codes;
		_numLanguageCodes = numCodes;
		_languageHint = hint;
	}

	/**
	 * Converts the planes of a decoded frame into the pixels of the output
	 * surface, in place of the YUV to RGB conversion of the decoder.
	 */
	class FrameConverter {
	public:
		virtual ~FrameConverter() {}

		/**
		 * @param dest         the output surface: it has the size of the Y plane
		 *                     and the pixel format of the video track
		 * @param pixelFormat  the chroma subsampling of the stream
		 * @param y            the luma plane
		 * @param u            the Cb plane
		 * @param v            the Cr plane
		 */
		virtual void convert(Graphics::Surface *dest, th_pixel_fmt pixelFormat,
		                     const th_img_plane &y, const th_img_plane &u, const th_img_plane &v) = 0;
	};

	/**
	 * Turn the post-processing of libtheora on or off. It is on by default
	 * (the maximum level); off leaves the decoded frames as they are coded.
	 * Must be called before loadStream().
	 */
	void setPostProcessing(bool enable) { _postProcessing = enable; }

	/**
	 * Use @p converter in place of the standard YUV to RGB conversion.
	 * The converter is not owned and must outlive the decoder. Null, the
	 * default, selects the standard conversion.
	 * Must be called before loadStream().
	 */
	void setFrameConverter(FrameConverter *converter) { _frameConverter = converter; }

	/**
	 * Converts a decoded Vorbis sample into a 16 bit sample, in place of the
	 * standard conversion. It is called for every sample, in the order of the
	 * output (channels interleaved), when the sample is decoded.
	 */
	class SampleConverter {
	public:
		virtual ~SampleConverter() {}

		/** @param sample  a decoded sample, nominally in the range -1.0 to 1.0 */
		virtual int16 convert(float sample) = 0;
	};

	/**
	 * Use @p converter in place of the standard sample conversion.
	 * The converter is not owned and must outlive the decoder. Null, the
	 * default, selects the standard conversion. It is not used with Tremor,
	 * whose samples are integers.
	 * Must be called before loadStream().
	 */
	void setSampleConverter(SampleConverter *converter) { _sampleConverter = converter; }

protected:
	void readNextPacket() override;

private:
	class TheoraVideoTrack : public VideoTrack {
	public:
		TheoraVideoTrack(th_info &theoraInfo, th_setup_info *theoraSetup, bool postProcessing, FrameConverter *frameConverter);
		~TheoraVideoTrack();

		bool endOfTrack() const override { return _endOfVideo; }
		uint16 getWidth() const override { return _width; }
		uint16 getHeight() const override { return _height; }
		Graphics::PixelFormat getPixelFormat() const override { return _pixelFormat; }
		bool setOutputPixelFormat(const Graphics::PixelFormat &format) override {
			if (format.bytesPerPixel != 2 && format.bytesPerPixel != 4)
				return false;
			_pixelFormat = format;
			return true;
		}

		int getCurFrame() const override { return _curFrame; }
		const Common::Rational &getFrameRate() const { return _frameRate; }
		uint32 getNextFrameStartTime() const override { return (uint32)(_nextFrameStartTime * 1000); }
		const Graphics::Surface *decodeNextFrame() override { return _displaySurface; }

		bool decodePacket(ogg_packet &oggPacket);
		void setEndOfVideo() { _endOfVideo = true; }

	private:
		int _curFrame;
		bool _endOfVideo;
		Common::Rational _frameRate;
		double _nextFrameStartTime;

		Graphics::Surface *_surface;
		Graphics::Surface *_displaySurface;
		Graphics::PixelFormat _pixelFormat;
		int _x;
		int _y;
		uint16 _width;
		uint16 _height;
		uint16 _surfaceWidth;
		uint16 _surfaceHeight;

		th_dec_ctx *_theoraDecode;
		th_pixel_fmt _theoraPixelFormat;
		FrameConverter *_frameConverter;

		void translateYUVtoRGBA(th_ycbcr_buffer &YUVBuffer);
	};

	class VorbisAudioTrack : public AudioTrack {
	public:
		VorbisAudioTrack(Audio::Mixer::SoundType soundType, vorbis_info &vorbisInfo, SampleConverter *sampleConverter);
		~VorbisAudioTrack();

		bool decodeSamples();
		bool hasAudio() const;
		bool needsAudio() const;
		void synthesizePacket(ogg_packet &oggPacket);
		void setEndOfAudio() { _endOfAudio = true; }

	protected:
		Audio::AudioStream *getAudioStream() const override;

	private:
		// single audio fragment audio buffering
		int _audioBufferFill;
		ogg_int16_t *_audioBuffer;

		Audio::QueuingAudioStream *_audStream;

		vorbis_block _vorbisBlock;
		vorbis_dsp_state _vorbisDSP;

		bool _endOfAudio;
		SampleConverter *_sampleConverter;
	};

	void queuePage(ogg_page *page);
	int bufferData();
	bool queueAudio();
	void ensureAudioBufferSize();
	bool findAudioStreamSerial(int &serial);

	Common::SeekableReadStream *_fileStream;
	const char *const *_languageCodes;
	int _numLanguageCodes;
	int _languageHint;
	bool _postProcessing;
	FrameConverter *_frameConverter;
	SampleConverter *_sampleConverter;

	ogg_sync_state _oggSync;
	ogg_page _oggPage;
	ogg_packet _oggPacket;

	ogg_stream_state _theoraOut, _vorbisOut;
	bool _hasVideo, _hasAudio;

	vorbis_info _vorbisInfo;

	TheoraVideoTrack *_videoTrack;
	VorbisAudioTrack *_audioTrack;
};

} // End of namespace Video

#endif

#endif
