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

#ifndef GRAPHICS_TINYGL_H
#define GRAPHICS_TINYGL_H

#include "common/str.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"
#include "graphics/tinygl/gl.h"
#include "graphics/tinygl/zblit_public.h"

namespace TinyGL {

typedef void *ContextHandle;

ContextHandle *createContext(int screenW, int screenH, Graphics::PixelFormat pixelFormat,
                   int textureSize, bool enableStencilBuffer, bool dirtyRectsEnable,
                   uint32 drawCallMemorySize = 5 * 1024 * 1024);
void destroyContext();
void destroyContext(ContextHandle *handle);
void setContext(ContextHandle *handle);
void presentBuffer();
void presentBuffer(Common::List<Common::Rect> &dirtyAreas);
void getSurfaceRef(Graphics::Surface &surface);
Graphics::Surface *copyFromFrameBuffer(const Graphics::PixelFormat &dstFormat);

/**
 * Draw call memory (linear frame allocator) accounting.
 *
 * Every allocation made from the per-frame draw call allocator is tagged with
 * a category (which draw call structure it belongs to) and with the label that
 * is current at the time of the allocation. Labels are set by the caller with
 * setFrameAllocLabel() and let an engine attribute memory to its own code
 * (e.g. "model", "bitmap", "text"). The label text is copied into the
 * statistics, so it only has to stay valid while it is the current label.
 */
const char *setFrameAllocLabel(const char *label); // returns the previous label

enum FrameAllocReport {
	kFrameAllocReportCurrent,   // the frame being built right now
	kFrameAllocReportLastFrame, // the last presented frame
	kFrameAllocReportPeakFrame  // the presented frame with the highest usage so far
};

Common::String getFrameAllocReport(FrameAllocReport which = kFrameAllocReportLastFrame);
void resetFrameAllocPeak();

/** RAII helper that sets a frame allocation label for the current scope. */
class FrameAllocLabelScope {
public:
	FrameAllocLabelScope(const char *label) : _previous(setFrameAllocLabel(label)) {}
	~FrameAllocLabelScope() { setFrameAllocLabel(_previous); }
private:
	const char *_previous;
};

} // end of namespace TinyGL

#endif
