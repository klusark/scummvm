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

#include "common/endian.h"
#include "common/textconsole.h"

#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"
#include "engines/grim/std/std_bitmap.h"
#include "engines/grim/std/std_display.h"

namespace Grim {

StdDisplay::StdDisplay() : _stdBitmap(nullptr), _inputBuffer(nullptr), _inputBufferIndex(0), _inputBufferMax(0), _outputBuffer(nullptr), _outputBufferIndex(0), _outputBufferMax(0),
		_std3D(nullptr), _mainGfxBuffer(nullptr), _backBuffer(nullptr), _startup(0), _enumerated(0), _displayOpen(0), _surfacesCreated(0), _fullscreen(0),
		_numDrawBuffers(1), _backendMask(0), _numDevices(0), _curDevice(nullptr), _curDeviceIndex(0), _numModes(0), _curModeIndex(0), _displayMode(nullptr) {
	memset(_devices, 0, sizeof(_devices));
	memset(_modes, 0, sizeof(_modes));
}

StdVBuffer *StdDisplay::vBufferNew(StdVBufferTexFmt *format, int videoMemory, int videoMemoryOnly, byte *palette) {
	if (((uint)format->format.bpp >> 3) == 0) {
		warning("StdDisplay::vBufferNew(): unsupported bits per pixel");
		return nullptr;
	}

	StdVBuffer *buffer = new StdVBuffer;

	buffer->paletteRGB = palette;
	buffer->pixels = nullptr;
	buffer->lockCount = 0;
	buffer->tex = *format;
	buffer->allocSize = 0;
	buffer->surface = nullptr;
	buffer->surfaceColorKey = -1;

	int width = buffer->tex.width;
	uint bytesPerPixel = (uint)buffer->tex.format.bpp >> 3;

	buffer->tex.pitchInBytes = width * bytesPerPixel;
	buffer->colorKey = 0;
	buffer->tex.pitchInPixels = width * bytesPerPixel / bytesPerPixel;

	int sizeInBytes = buffer->tex.height * width * bytesPerPixel;
	buffer->tex.sizeInBytes = sizeInBytes;

#if defined(GRIM_STD3D)
	if (videoMemory && _displayOpen && _curDevice && _curDevice->type == 1 && _std3D) {
		if (buffer->tex.format.bpp != 16) {
			warning("StdDisplay::vBufferNew(): only 16 bpp video memory buffers are supported, using system memory");
		} else {
			buffer->videoMemory = 0;
			buffer->type = 1;
			buffer->surface = _std3D->createSurface(buffer->tex.width, buffer->tex.height, 0);

			if (!buffer->surface) {
				delete buffer;
				return nullptr;
			}

			if (videoMemoryOnly)
				buffer->videoMemory = 1;

			return buffer;
		}
	}
#endif

	buffer->type = 0;
	buffer->videoMemory = 0;
	buffer->pixels = new byte[sizeInBytes];
	buffer->allocSize = sizeInBytes;
	buffer->lockCount = 1;

	return buffer;
}

void StdDisplay::vBufferFree(StdVBuffer *&vBuffer) {
	if (vBuffer) {
		vBufferFreeEntry(vBuffer);
		delete vBuffer;
		vBuffer = nullptr;
	}
}

void StdDisplay::vBufferFreeEntry(StdVBuffer *vBuffer) {
	if (vBuffer->type == 0 || vBuffer->type == 2) {
		if (vBuffer->pixels) {
			if (_remasteredLookRenderer)
				_remasteredLookRenderer->notifyFree(vBuffer->pixels);

			delete[] vBuffer->pixels;
			vBuffer->pixels = nullptr;
		}
	}
#if defined(GRIM_STD3D)
	else if (vBuffer->type == 1) {
		if (vBuffer->surface && _std3D)
			_std3D->destroySurface(vBuffer->surface);

		vBuffer->surface = nullptr;

		if (vBuffer->allocSize) {
			delete[] vBuffer->pixels;
			vBuffer->pixels = nullptr;
			vBuffer->allocSize = 0;
		}
	}
#endif
}

int StdDisplay::vBufferLock(StdVBuffer *vBuffer) {
	if (vBuffer->type == 0) {
		vBuffer->lockCount++;
		return 1;
	}

	if (vBuffer->type == 1) {
#if defined(GRIM_STD3D)
		if (!surfaceLock(vBuffer))
			return 0;

		vBuffer->lockCount++;
		return 1;
#else
		return 0;
#endif
	}

	return 1;
}

int StdDisplay::vBufferUnlock(StdVBuffer *vBuffer) {
	if (vBuffer->type == 0) {
		if (vBuffer->lockCount)
			vBuffer->lockCount--;

		return 1;
	}

	if (vBuffer->type != 1)
		return 1;

	if (!vBuffer->lockCount)
		return 1;

#if defined(GRIM_STD3D)
	int result = surfaceUnlock(vBuffer, 1);

	if (result)
		vBuffer->lockCount--;

	return result;
#else
	return 0;
#endif
}

void StdDisplay::vBufferFill(StdVBuffer *vBuffer, int color, StdRect *rect) {
	if (vBuffer->type != 0) {
#if defined(GRIM_STD3D)
		if (vBuffer->type == 1)
			surfaceBlt(vBuffer, color, rect);
#endif
		return;
	}

	switch (vBuffer->tex.format.bpp) {
	case 8:
		if (rect) {
			byte *row = vBuffer->pixels + rect->y * vBuffer->tex.pitchInPixels + rect->x;

			for (int i = 0; i < rect->height; i++) {
				memset(row, color, rect->width);
				row += vBuffer->tex.pitchInBytes;
			}
		} else {
			memset(vBuffer->pixels, color, vBuffer->tex.sizeInBytes);
		}
		break;
	case 16:
		if (rect) {
			byte *row = vBuffer->pixels + 2 * rect->x + 2 * rect->y * vBuffer->tex.pitchInPixels;

			for (int i = 0; i < rect->height; i++) {
				uint16 *pixel = (uint16 *)row;

				for (int j = 0; j < rect->width; j++)
					pixel[j] = (uint16)color;

				row += vBuffer->tex.pitchInBytes;
			}
		} else {
			uint16 *pixel = (uint16 *)vBuffer->pixels;

			for (int i = 0; i < vBuffer->tex.sizeInBytes >> 1; i++)
				pixel[i] = (uint16)color;
		}
		break;
	case 32:
		if (rect) {
			byte *row = vBuffer->pixels + 4 * rect->x + 4 * rect->y * vBuffer->tex.pitchInPixels;

			for (int i = 0; i < rect->height; i++) {
				uint32 *pixel = (uint32 *)row;

				for (int j = 0; j < rect->width; j++)
					pixel[j] = (uint32)color;

				row += vBuffer->tex.pitchInBytes;
			}
		} else {
			uint32 *pixel = (uint32 *)vBuffer->pixels;

			for (int i = 0; i < vBuffer->tex.sizeInBytes >> 2; i++)
				pixel[i] = (uint32)color;
		}
		break;
	default:
		break;
	}
}

void StdDisplay::notifyBufferWritten(StdVBuffer *buffer) {
	if (_remasteredLookRenderer)
		_remasteredLookRenderer->notifyBufferWritten(buffer);
}

int StdDisplay::vBufferCopy(StdVBuffer *dst, StdVBuffer *src, int x, int y, StdRect *rect, char colorKey) {
	if (_remasteredLookRenderer)
		_remasteredLookRenderer->bufferCopy(dst, src, x, y, rect, colorKey != 0);

	if (src->type == 0 && dst->type == 0) {
		copyBltA(dst, src, x, y, rect, colorKey);
		return 1;
	}

#if defined(GRIM_STD3D)
	if (src->type != 0) {
		if (dst->type == 1)
			copyBltB(dst, src, x, y, rect, colorKey);
		else
			copyBltD(dst, src, x, y, rect, colorKey);

		return 1;
	}

	copyBltC(dst, src, x, y, rect, colorKey);
	return 1;
#else
	return 0;
#endif
}

int StdDisplay::vBufferSetColorKey(StdVBuffer *vBuffer, int colorKey) {
	if (vBuffer->type == 0)
		vBuffer->colorKey = colorKey;
	else if (vBuffer->type == 1)
		vBuffer->surfaceColorKey = colorKey;

	return 1;
}

void StdDisplay::vBufferDeCompress(StdVBuffer *vBuffer) {
	if (vBuffer->type != 2)
		return;

	byte *pixels = new byte[vBuffer->tex.height * vBuffer->tex.pitchInBytes];

	int size = lzssDecompress(pixels, vBuffer->pixels, vBuffer->tex.sizeInBytes, vBuffer->tex.height * vBuffer->tex.pitchInBytes);
	delete[] vBuffer->pixels;
	vBuffer->tex.sizeInBytes = size;
	vBuffer->type = 0;
	vBuffer->allocSize = vBuffer->tex.height * vBuffer->tex.pitchInBytes;
	vBuffer->pixels = pixels;
}

int StdDisplay::vBufferDeCompressTo(StdVBuffer *src, StdVBuffer *dst) {
	vBufferLock(dst);
	lzssDecompress(dst->pixels, src->pixels, src->tex.sizeInBytes, dst->allocSize);

	if (_remasteredLookRenderer)
		_remasteredLookRenderer->notifyChange(dst->pixels);

	return vBufferUnlock(dst);
}

StdVBuffer *StdDisplay::vBufferConvertColorFormat(StdColorFormat *format, StdVBuffer *vBuffer) {
	if (!memcmp(format, &vBuffer->tex.format, sizeof(StdColorFormat)))
		return vBuffer;

	if (!vBuffer->tex.format.colorMode && !format->colorMode)
		return vBuffer;

	if (vBuffer->type == 2) {
		warning("StdDisplay::vBufferConvertColorFormat(): buffer is compressed");
		return vBuffer;
	}

	StdVBuffer *result;

	if (vBuffer->tex.format.bpp == format->bpp) {
		result = vBuffer;
	} else {
		StdVBufferTexFmt texFormat = vBuffer->tex;

		texFormat.format = *format;
		result = vBufferNew(&texFormat, 0, 0, nullptr);

		if (!result)
			return result;
	}

	vBufferLock(vBuffer);
	vBufferLock(result);

	for (int i = 0; i < result->tex.height; i++)
		_stdBitmap->convertPixelRow(&result->pixels[result->tex.pitchInBytes * i], format, &vBuffer->pixels[vBuffer->tex.pitchInBytes * i], &vBuffer->tex.format, result->tex.width);

	if (_remasteredLookRenderer)
		_remasteredLookRenderer->notifyChange(result->pixels);

	vBufferUnlock(vBuffer);
	vBufferUnlock(result);

	result->colorKey = _stdBitmap->convertColor(format, vBuffer->colorKey, &vBuffer->tex.format);
	result->tex.format = *format;

	if (result != vBuffer)
		vBufferFree(vBuffer);

	return result;
}

int StdDisplay::copyBltA(StdVBuffer *dstPointer, StdVBuffer *srcPointer, int x, int y, StdRect *rect, char colorKey) {
	int srcX;
	int srcY;
	int width;
	int height;

	if (rect) {
		srcX = rect->x;
		srcY = rect->y;
		width = rect->width;
		height = rect->height;
	} else {
		width = srcPointer->tex.width;
		height = srcPointer->tex.height;
		srcX = 0;
		srcY = 0;
	}

	if (x >= dstPointer->tex.width)
		return 0;

	int endX = x + width;
	if (!endX)
		return 0;

	uint firstRow = y;
	if (y >= dstPointer->tex.height || !(height + y))
		return 0;

	uint lastRow = height + y - 1;
	uint lastColumn = endX - 1;
	int srcStartX = srcX;

	if (y < 0) {
		srcY -= y;
		firstRow = 0;
	}

	if (lastRow >= (uint)dstPointer->tex.height)
		lastRow = dstPointer->tex.height - 1;

	if (x < 0) {
		srcStartX = srcX - x;
		x = 0;
	}

	if (lastColumn >= (uint)dstPointer->tex.width)
		lastColumn = dstPointer->tex.width - 1;

	int copyWidth = lastColumn - x + 1;
	if (!copyWidth)
		return 1;

	uint rowBytes = copyWidth * ((uint)srcPointer->tex.format.bpp >> 3);
	uint bppCase = srcPointer->tex.format.bpp - 8;
	if (bppCase > 0x18)
		return 1;

	if (firstRow <= lastRow) {
		int numRows = lastRow - firstRow + 1;

		if (srcStartX < 0 || srcY < 0 || srcStartX + copyWidth > srcPointer->tex.width || srcY + numRows > srcPointer->tex.height) {
			warning("StdDisplay::copyBltA(): source rectangle outside the source buffer");
			return 0;
		}
	}

	switch (srcPointer->tex.format.bpp) {
	case 8: {
		byte *src = &srcPointer->pixels[srcPointer->tex.pitchInPixels * srcY + srcStartX];
		byte *dst = &dstPointer->pixels[dstPointer->tex.pitchInPixels * firstRow + x];

		if (firstRow > lastRow)
			return 1;

		int rows = lastRow - firstRow + 1;

		if ((colorKey & 1) == 0) {
			do {
				memcpy(dst, src, rowBytes);
				dst += dstPointer->tex.pitchInPixels;
				src += srcPointer->tex.pitchInPixels;
				rows--;
			} while (rows);
		} else {
			int srcSkip = srcPointer->tex.pitchInPixels - copyWidth;
			int dstSkip = dstPointer->tex.pitchInPixels - copyWidth;

			do {
				if ((uint)x <= lastColumn) {
					int count = copyWidth;

					do {
						if (*src != (byte)srcPointer->colorKey)
							*dst = *src;
						src++;
						dst++;
						count--;
					} while (count);
				}
				dst += dstSkip;
				src += srcSkip;
				rows--;
			} while (rows);
		}
		return 1;
	}
	case 16: {
		byte *src = &srcPointer->pixels[2 * srcStartX + 2 * srcPointer->tex.pitchInPixels * srcY];
		byte *dst = &dstPointer->pixels[2 * x + 2 * dstPointer->tex.pitchInPixels * firstRow];

		if (firstRow > lastRow)
			return 1;

		int rows = lastRow - firstRow + 1;

		if ((colorKey & 1) == 0) {
			do {
				memcpy(dst, src, rowBytes);
				src += 2 * srcPointer->tex.pitchInPixels;
				dst += 2 * dstPointer->tex.pitchInPixels;
				rows--;
			} while (rows);
		} else {
			int srcSkip = srcPointer->tex.pitchInPixels - copyWidth;
			int dstSkip = dstPointer->tex.pitchInPixels - copyWidth;

			do {
				if ((uint)x <= lastColumn) {
					int count = copyWidth;

					do {
						if (*(uint16 *)src != (uint16)srcPointer->colorKey)
							*(uint16 *)dst = *(uint16 *)src;
						src += 2;
						dst += 2;
						count--;
					} while (count);
				}
				rows--;
				dst += 2 * dstSkip;
				src += 2 * srcSkip;
			} while (rows);
		}
		return 1;
	}
	case 24: {
		byte *src = &srcPointer->pixels[3 * srcStartX + srcPointer->tex.pitchInBytes * srcY];
		byte *dst = &dstPointer->pixels[3 * x + dstPointer->tex.pitchInBytes * firstRow];

		if (firstRow > lastRow)
			return 1;

		int rows = lastRow - firstRow + 1;

		do {
			memcpy(dst, src, rowBytes);
			dst += dstPointer->tex.pitchInBytes;
			rows--;
			src += srcPointer->tex.pitchInBytes;
		} while (rows);
		return 1;
	}
	case 32: {
		byte *src = &srcPointer->pixels[4 * srcStartX + 4 * srcPointer->tex.pitchInPixels * srcY];
		byte *dst = &dstPointer->pixels[4 * x + 4 * dstPointer->tex.pitchInPixels * firstRow];

		if (firstRow > lastRow)
			return 1;

		int rows = lastRow - firstRow + 1;

		if ((colorKey & 1) == 0) {
			do {
				memcpy(dst, src, rowBytes);
				src += 4 * srcPointer->tex.pitchInPixels;
				rows--;
				dst += 4 * dstPointer->tex.pitchInPixels;
			} while (rows);
		} else {
			int srcSkip = srcPointer->tex.pitchInPixels - copyWidth;
			int dstSkip = dstPointer->tex.pitchInPixels - copyWidth;

			do {
				if ((uint)x <= lastColumn) {
					int count = copyWidth;

					do {
						if (*(uint32 *)src != (uint32)srcPointer->colorKey)
							*(uint32 *)dst = *(uint32 *)src;
						src += 4;
						dst += 4;
						count--;
					} while (count);
				}
				rows--;
				dst += 4 * dstSkip;
				src += 4 * srcSkip;
			} while (rows);
		}
		return 1;
	}
	default:
		return 1;
	}
}

int StdDisplay::lzssDecompress(byte *dst, byte *src, int size, int outputSize) {
	byte *padded = new byte[size + 16];

	memcpy(padded, src, size);
	memset(padded + size, 0, 16);

	_inputBuffer = padded;
	_inputBufferIndex = 0;
	_outputBufferIndex = 0;
	_inputBufferMax = size;
	_outputBuffer = dst;
	_outputBufferMax = outputSize;
	decompressImpl();

	delete[] padded;
	_inputBuffer = nullptr;

	return _outputBufferIndex;
}

int StdDisplay::readFlagBit(const byte *&in, uint &flagBits, int &flagCount) {
	int bit = flagBits & 1;

	flagBits >>= 1;
	if (!--flagCount) {
		flagCount = 16;
		flagBits = READ_LE_UINT16(in);
		in += 2;
	}

	return bit;
}

void StdDisplay::decompressImpl() {
	byte *out = _outputBuffer;
	byte *outEnd = _outputBuffer + _outputBufferMax;
	const byte *inLimit = _inputBuffer + _inputBufferMax + 8;
	uint flagBits = READ_LE_UINT16(_inputBuffer);
	const byte *in = _inputBuffer + 2;
	int flagCount = 16;

	while (true) {
		bool literal = true;

		while (literal) {
			if (in > inLimit) {
				warning("StdDisplay::decompressImpl(): compressed data ends unexpectedly");
				_outputBufferIndex = out - _outputBuffer;
				_inputBufferIndex = in - _inputBuffer;
				return;
			}

			literal = readFlagBit(in, flagBits, flagCount) == 1;

			if (literal) {
				if (out >= outEnd) {
					warning("StdDisplay::decompressImpl(): decompressed data does not fit the buffer");
					_outputBufferIndex = out - _outputBuffer;
					_inputBufferIndex = in - _inputBuffer;
					return;
				}

				*out++ = *in++;
			}
		}

		int copyLen;
		int offset;

		if (readFlagBit(in, flagBits, flagCount) == 0) {
			int high = readFlagBit(in, flagBits, flagCount);
			int low = readFlagBit(in, flagBits, flagCount);

			copyLen = (low | 2 * high) + 3;
			offset = *in++ | 0xFFFFFF00;
		} else {
			int first = *in++;
			int second = *in++;

			offset = (16 * ((second & 0xF0) - 256)) | first;
			copyLen = (second & 0xF) + 3;
			if ((second & 0xF) == 0) {
				copyLen = *in++ + 1;
				if (copyLen == 1)
					break;
			}
		}

		const byte *copy = out + offset;

		if (copy < _outputBuffer || out + copyLen > outEnd || in > inLimit) {
			warning("StdDisplay::decompressImpl(): invalid back reference");
			break;
		}

		for (int i = 0; i < copyLen; i++)
			*out++ = *copy++;
	}

	_outputBufferIndex = out - _outputBuffer;
	_inputBufferIndex = in - _inputBuffer;
}

} // End of namespace Grim
