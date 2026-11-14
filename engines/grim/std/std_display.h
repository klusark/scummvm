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

#ifndef GRIM_STD_STD_DISPLAY_H
#define GRIM_STD_STD_DISPLAY_H

#include "common/scummsys.h"

namespace Grim {

class StdBitmap;
class Std3D;
class RemasteredLookRenderer;
struct Std3DSurface;

enum StdDisplayLimits {
	kStdDisplayMaxDevices = 16,
	kStdDisplayMaxModes = 64
};

struct StdRect {
	int x;
	int y;
	int width;
	int height;
};

struct StdColorFormat {
	int colorMode;
	int bpp;
	int redBits;
	int greenBits;
	int blueBits;
	int redShift;
	int greenShift;
	int blueShift;
	int redLoss;
	int greenLoss;
	int blueLoss;
	int alphaBits;
	int alphaShift;
	int alphaLoss;
};

struct StdVBufferTexFmt {
	int width;
	int height;
	int sizeInBytes;
	int pitchInBytes;
	int pitchInPixels;
	StdColorFormat format;
};

struct StdVBuffer {
	int type;
	uint lockCount;
	int videoMemory;
	StdVBufferTexFmt tex;
	byte *paletteRGB;
	byte *pixels;
	int colorKey;
	int allocSize;
	Std3DSurface *surface;
	int surfaceColorKey;
};

enum StdDisplayBackend {
	kStdDisplayBackendAny = -1,
	kStdDisplayBackendNone = 0,
	kStdDisplayBackendGl = 1,
	kStdDisplayBackendTinyGl = 2
};

struct StdDisplayDevice {
	char name[128];
	char description[128];
	int type;
	int hasGuid;
	int b3D;
	int primary;
	int hasHardware;
	uint vidMemTotal;
	uint vidMemFree;
	byte guid[16];
	int backend;
	int usable;
};

struct StdDisplayDeviceFlags {
	int type;
	int hasGuid;
	int b3D;
	int primary;
	int hasHardware;
	int backend;
};

struct StdDisplayMode {
	int modeX;
	float aspect;
	StdVBufferTexFmt tex;
};

struct StdDisplayModeDesc {
	int width;
	int height;
	int pitch;
	uint pixelFlags;
	uint rgbBitCount;
	uint rBitMask;
	uint gBitMask;
	uint bBitMask;
};

class StdDisplay {
public:
	StdDisplay();

	void setStdBitmap(StdBitmap *stdBitmap) { _stdBitmap = stdBitmap; }
	void setStd3D(Std3D *std3D) { _std3D = std3D; }
	void setBuffers(StdVBuffer *mainGfxBuffer, StdVBuffer *backBuffer) { _mainGfxBuffer = mainGfxBuffer; _backBuffer = backBuffer; }
	void notifyBufferWritten(StdVBuffer *buffer);
	void setRemasteredLookRenderer(RemasteredLookRenderer *remasteredLookRenderer) { _remasteredLookRenderer = remasteredLookRenderer; }

	int isDisplayOpen() { return _displayOpen; }
	int isSurfacesCreated() { return _surfacesCreated; }
	int getFullscreen() { return _fullscreen; }
	int getNumDrawBuffers() { return _numDrawBuffers; }
	uint getNumDevices() { return _numDevices; }
	StdDisplayDevice *getDevices() { return _devices; }
	StdDisplayDevice *getCurDevice() { return _curDevice; }
	uint getCurDeviceIndex() { return _curDeviceIndex; }
	uint getNumModes() { return _numModes; }
	StdDisplayMode *getModes() { return _modes; }
	StdDisplayMode *getDisplayMode() { return _displayMode; }

	int startup();
	void shutdown();
	int enumerateDevices(uint backendMask);
	int enumDevicesCallback(const byte *guid, const char *description, const char *name, int backend);
	void setDeviceUsable(uint deviceIndex, int usable);
	int isBackendEnabled(int backend);
	int createBackend(int backend);
	int openBackend(int backend, int width, int height);
	void restoreDisplayMode();
	int enumerateModes(uint deviceIndex);
	int createDirectDraw();
	int enumModesCallback(const StdDisplayModeDesc *desc);
	int enumModesCallback2(const StdDisplayModeDesc *desc);
	uint findClosestDevice(const StdDisplayDeviceFlags *wanted);
	uint findClosestMode(const StdDisplayMode *wanted, const StdDisplayMode *modes, uint numModes);
	int openDisplay(uint modeIndex, byte *palette, int fullscreen);
	void closeDisplay();
	int initDisplay(StdDisplayMode *mode, byte *palette);
	void releaseSurfaces();
	void freeBuffers();
	int prepareWindowForDraw();
	void prepareOutputForDraw();
	int update();
	byte *surfaceLock(StdVBuffer *vBuffer);
	int surfaceUnlock(StdVBuffer *vBuffer, int upload);
	int surfaceBlt(StdVBuffer *vBuffer, int color, const StdRect *rect);
	int copyBltB(StdVBuffer *dst, StdVBuffer *src, int x, int y, StdRect *rect, char colorKey);
	int copyBltC(StdVBuffer *dst, StdVBuffer *src, int x, int y, StdRect *rect, char colorKey);
	int copyBltD(StdVBuffer *dst, StdVBuffer *src, int x, int y, StdRect *rect, char colorKey);

	StdVBuffer *vBufferNew(StdVBufferTexFmt *format, int videoMemory, int videoMemoryOnly, byte *palette);
	void vBufferFree(StdVBuffer *&vBuffer);
	void vBufferFreeEntry(StdVBuffer *vBuffer);
	int vBufferLock(StdVBuffer *vBuffer);
	int vBufferUnlock(StdVBuffer *vBuffer);
	void vBufferFill(StdVBuffer *vBuffer, int color, StdRect *rect);
	int vBufferCopy(StdVBuffer *dst, StdVBuffer *src, int x, int y, StdRect *rect, char colorKey);
	int vBufferSetColorKey(StdVBuffer *vBuffer, int colorKey);
	void vBufferDeCompress(StdVBuffer *vBuffer);
	int vBufferDeCompressTo(StdVBuffer *src, StdVBuffer *dst);
	StdVBuffer *vBufferConvertColorFormat(StdColorFormat *format, StdVBuffer *vBuffer);
	int copyBltA(StdVBuffer *dstPointer, StdVBuffer *srcPointer, int x, int y, StdRect *rect, char colorKey);
	int lzssDecompress(byte *dst, byte *src, int size, int outputSize);

private:
	void decompressImpl();
	int readFlagBit(const byte *&in, uint &flagBits, int &flagCount);
	int isZBuffer(StdVBuffer *vBuffer);
	void presentIfPrimary(StdVBuffer *vBuffer);
	int copyToZBuffer(StdVBuffer *src, int x, int y, const StdRect *srcRectPointer);
	void unlockForRead(StdVBuffer *vBuffer);
	int syncShadow(StdVBuffer *vBuffer);

	StdBitmap *_stdBitmap;
	byte *_inputBuffer;
	int _inputBufferIndex;
	int _inputBufferMax;
	byte *_outputBuffer;
	int _outputBufferIndex;
	int _outputBufferMax;

	Std3D *_std3D;
	StdVBuffer *_mainGfxBuffer;
	StdVBuffer *_backBuffer;
	RemasteredLookRenderer *_remasteredLookRenderer = nullptr;
	int _startup;
	int _enumerated;
	int _displayOpen;
	int _surfacesCreated;
	int _fullscreen;
	int _numDrawBuffers;
	uint _backendMask;
	uint _numDevices;
	StdDisplayDevice _devices[kStdDisplayMaxDevices];
	StdDisplayDevice *_curDevice;
	uint _curDeviceIndex;
	uint _numModes;
	StdDisplayMode _modes[kStdDisplayMaxModes];
	uint _curModeIndex;
	StdDisplayMode *_displayMode;
};

} // End of namespace Grim

#endif
