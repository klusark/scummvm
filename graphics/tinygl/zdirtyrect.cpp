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

#include "graphics/tinygl/tinygl.h"
#include "graphics/tinygl/zdirtyrect.h"
#include "graphics/tinygl/zgl.h"
#include "graphics/tinygl/gl.h"

#include "common/debug.h"

namespace TinyGL {

// BENCH: hashing helpers for the tile diff
static inline uint64 hashMix(uint64 h, uint64 v) {
	h ^= v;
	h *= 0xff51afd7ed558ccdULL;
	h ^= h >> 32;
	return h;
}

static inline uint64 hashFloat(uint64 h, float f) {
	uint32 v;
	memcpy(&v, &f, sizeof(v));
	return hashMix(h, v);
}

static inline uint64 hashVec3(uint64 h, const Vector3 &v) {
	return hashFloat(hashFloat(hashFloat(h, v.X), v.Y), v.Z);
}

static inline uint64 hashVec4(uint64 h, const Vector4 &v) {
	return hashFloat(hashFloat(hashFloat(hashFloat(h, v.X), v.Y), v.Z), v.W);
}

static inline uint64 hashPtr(uint64 h, const void *p) {
	return hashMix(h, (uint64)(uintptr)p);
}

static inline uint64 hashRect(uint64 h, const Common::Rect &r) {
	return hashMix(hashMix(h, ((uint64)(uint16)r.left << 16) | (uint16)r.top), ((uint64)(uint16)r.right << 16) | (uint16)r.bottom);
}

GLTextureEnvArgument::GLTextureEnvArgument()
	: sourceRGB(TGL_TEXTURE)
	, operandRGB(TGL_SRC_COLOR)
	, sourceAlpha(TGL_TEXTURE)
	, operandAlpha(TGL_SRC_ALPHA) {}

GLTextureEnv::GLTextureEnv()
	: envMode(TGL_MODULATE)
	, combineRGB(TGL_REPLACE)
	, combineAlpha(TGL_REPLACE)
	, constA(255), constR(255), constG(255), constB(255) {}

bool GLTextureEnv::isDefault() const {
	return envMode == TGL_MODULATE;
}

void GLContext::issueDrawCall(DrawCall *drawCall) {
	if (_enableDirtyRectangles && drawCall->getDirtyRegion().isEmpty())
		return;
	_drawCallsQueue.push_back(drawCall);
}

void GLContext::debugDrawRectangle(Common::Rect rect, int r, int g, int b) {
	int fbWidth = fb->getPixelBufferWidth();

	if (rect.left < 0)
		rect.left = 0;
	if (rect.right >= fbWidth)
		rect.right = fbWidth - 1;
	if (rect.top < 0)
		rect.top = 0;
	if (rect.bottom >= fb->getPixelBufferHeight())
		rect.bottom = fb->getPixelBufferHeight() - 1;

	for (int x = rect.left; x < rect.right; x++) {
		fb->writePixel(rect.top * fbWidth + x, 255, r, g, b);
		fb->writePixel((rect.bottom - 1) * fbWidth + x, 255, r, g, b);
	}
	for (int y = rect.top; y < rect.bottom; y++) {
		fb->writePixel(y * fbWidth + rect.left, 255, r, g, b);
		fb->writePixel(y * fbWidth + rect.right - 1, 255, r, g, b);
	}
}

struct DirtyRectangle {
	Common::Rect rectangle;
	int r, g, b;

	DirtyRectangle() {
		r = 0;
		g = 0;
		b = 0;
	}
	DirtyRectangle(Common::Rect rect, int red, int green, int blue) {
		rectangle = rect;
		r = red;
		g = green;
		b = blue;
	}
};

void GLContext::disposeResources() {
	// Dispose textures and resources.
	bool allDisposed = true;
	do {
		allDisposed = true;
		for (int i = 0; i < TEXTURE_HASH_TABLE_SIZE; i++) {
			GLTexture *t = shared_state.texture_hash_table[i];
			while (t) {
				if (t->disposed) {
					free_texture(t);
					allDisposed = false;
					break;
				}
				t = t->next;
			}
		}

	} while (allDisposed == false);

	Internal::tglCleanupImages();
}

void GLContext::disposeDrawCallLists() {
	for (const auto &drawCall : _previousFrameDrawCallsQueue) {
		delete drawCall;
	}
	_previousFrameDrawCallsQueue.clear();
	for (auto &drawCall : _drawCallsQueue) {
		delete drawCall;
	}
	_drawCallsQueue.clear();
}

static inline void _appendDirtyRectangle(const DrawCall &call, Common::List<DirtyRectangle> &rectangles, int r, int g, int b) {
	Common::Rect dirty_region = call.getDirtyRegion();
	if (rectangles.empty() || dirty_region != rectangles.back().rectangle)
		rectangles.push_back(DirtyRectangle(dirty_region, r, g, b));
}

// BENCH: dirty rect diff experiment
int g_dirtyDiffMode = 0;      // 0 = compare by position, 1 = realign after a mismatch
int g_dirtyDiffWindow = 16;   // how far (prev skip + cur skip) to look for the next match
bool g_dirtyVerify = false;   // redraw every frame from scratch and compare pixels
int g_dirtyTileSize = 32;     // tile diff: tile width and height in pixels
DirtyDiffStats g_dirtyDiffStats;
static Common::Array<byte> s_refColor, s_refZ, s_refStencil, s_saveColor, s_saveZ, s_saveStencil;

typedef Common::Array<DrawCall *> DrawCallArray;

// Cheap key checked before the full comparison: equal draw calls always have
// the same type and dirty region.
struct DrawCallKey {
	Common::Rect region;
	int type;
	bool operator==(const DrawCallKey &o) const { return type == o.type && region == o.region; }
};
typedef Common::Array<DrawCallKey> DrawCallKeyArray;

static inline bool callsEqual(const DrawCallArray &prev, const DrawCallKeyArray &prevKeys, uint i,
		const DrawCallArray &cur, const DrawCallKeyArray &curKeys, uint j) {
	return prevKeys[i] == curKeys[j] && *prev[i] == *cur[j];
}

static void diffByPosition(const DrawCallArray &prev, const DrawCallKeyArray &prevKeys,
		const DrawCallArray &cur, const DrawCallKeyArray &curKeys,
		Common::List<DirtyRectangle> &rectangles, uint &unmatched) {
	uint i = 0;
	for (; i < prev.size() && i < cur.size(); i++) {
		if (!callsEqual(prev, prevKeys, i, cur, curKeys, i)) {
			_appendDirtyRectangle(*prev[i], rectangles, 255, 255, 255);
			_appendDirtyRectangle(*cur[i], rectangles, 255, 0, 0);
			unmatched += 2;
		}
	}
	for (uint k = i; k < prev.size(); k++, unmatched++)
		_appendDirtyRectangle(*prev[k], rectangles, 255, 255, 255);
	for (uint k = i; k < cur.size(); k++, unmatched++)
		_appendDirtyRectangle(*cur[k], rectangles, 255, 0, 0);
}

// Matches equal draw calls in order. After a mismatch it looks for the nearest
// pair prev[i + a] == cur[j + b] (smallest a + b <= window); the calls skipped
// on either side are the changed ones. If there is none, prev[i] and cur[j]
// are both treated as changed, like the position diff does.
static void diffRealign(const DrawCallArray &prev, const DrawCallKeyArray &prevKeys,
		const DrawCallArray &cur, const DrawCallKeyArray &curKeys, uint window,
		Common::List<DirtyRectangle> &rectangles, uint &unmatched, uint &resyncs) {
	uint i = 0, j = 0;
	const uint np = prev.size(), nc = cur.size();
	while (i < np && j < nc) {
		if (callsEqual(prev, prevKeys, i, cur, curKeys, j)) {
			i++;
			j++;
			continue;
		}
		uint skipPrev = 1, skipCur = 1;
		bool found = false;
		for (uint k = 1; k <= window && !found; k++) {
			for (uint a = 0; a <= k; a++) {
				const uint b = k - a;
				if (i + a < np && j + b < nc && callsEqual(prev, prevKeys, i + a, cur, curKeys, j + b)) {
					skipPrev = a;
					skipCur = b;
					found = true;
					break;
				}
			}
		}
		if (found && (skipPrev == 0 || skipCur == 0))
			resyncs++;
		for (uint k = 0; k < skipPrev; k++, unmatched++)
			_appendDirtyRectangle(*prev[i + k], rectangles, 255, 255, 255);
		for (uint k = 0; k < skipCur; k++, unmatched++)
			_appendDirtyRectangle(*cur[j + k], rectangles, 255, 0, 0);
		i += skipPrev;
		j += skipCur;
	}
	for (; i < np; i++, unmatched++)
		_appendDirtyRectangle(*prev[i], rectangles, 255, 255, 255);
	for (; j < nc; j++, unmatched++)
		_appendDirtyRectangle(*cur[j], rectangles, 255, 0, 0);
}

static void mergeDirtyRectangles(Common::List<DirtyRectangle> &rectangles, const Common::Rect &renderRect) {
	typedef Common::List<DirtyRectangle>::iterator RectangleIterator;

	// This loop increases outer rectangle coordinates to favor merging of adjacent rectangles.
	for (auto &rect : rectangles) {
		rect.rectangle.right++;
		rect.rectangle.bottom++;
	}

	// Merge coalesce dirty rects.
	bool restartMerge;
	do {
		restartMerge = false;
		for (RectangleIterator it1 = rectangles.begin(); it1 != rectangles.end(); ++it1) {
			for (RectangleIterator it2 = rectangles.begin(); it2 != rectangles.end();) {
				if (it1 != it2) {
					if (it1->rectangle.intersects(it2->rectangle)) {
						it1->rectangle.extend(it2->rectangle);
						it2 = rectangles.erase(it2);
						restartMerge = true;
					} else {
						++it2;
					}
				} else {
					++it2;
				}
			}
		}
	} while(restartMerge);

	for (RectangleIterator it1 = rectangles.begin(); it1 != rectangles.end(); ++it1) {
		RectangleIterator it2 = it1;
		it2++;
		while (it2 != rectangles.end()) {
			if (it1->rectangle.contains(it2->rectangle)) {
				it2 = rectangles.erase(it2);
			} else {
				++it2;
			}
		}
	}

	for (auto &rect : rectangles) {
		rect.rectangle.clip(renderRect);
	}
}

static uint64 rectanglesArea(const Common::List<DirtyRectangle> &rectangles) {
	uint64 area = 0;
	for (const auto &rect : rectangles)
		area += (uint64)rect.rectangle.width() * rect.rectangle.height();
	return area;
}

// Tile diff: every tile keeps a hash of the calls touching it, in order. A
// tile is redrawn when its hash differs from the previous frame's.
static Common::Array<uint64> s_prevTileHash;
static int s_prevTilesX = 0, s_prevTilesY = 0, s_prevTileSize = 0;

static void diffTiles(const DrawCallArray &cur, const Common::Array<uint64> &callHashes, const Common::Rect &renderRect,
		int tileSize, Common::List<DirtyRectangle> &rectangles) {
	const int tilesX = (renderRect.right + tileSize - 1) / tileSize;
	const int tilesY = (renderRect.bottom + tileSize - 1) / tileSize;
	Common::Array<uint64> tileHash;
	tileHash.resize(tilesX * tilesY);
	for (uint i = 0; i < tileHash.size(); i++)
		tileHash[i] = 0x9e3779b97f4a7c15ULL;

	for (uint c = 0; c < cur.size(); c++) {
		Common::Rect r = cur[c]->getDirtyRegion();
		// Same one pixel of slack the rectangle merge adds.
		r.right++;
		r.bottom++;
		r.clip(renderRect);
		if (r.isEmpty())
			continue;
		const int x0 = r.left / tileSize, x1 = (r.right - 1) / tileSize;
		const int y0 = r.top / tileSize, y1 = (r.bottom - 1) / tileSize;
		const uint64 h = callHashes[c];
		for (int y = y0; y <= y1; y++) {
			uint64 *row = &tileHash[y * tilesX];
			for (int x = x0; x <= x1; x++)
				row[x] = hashMix(row[x] * 31 + 1, h);
		}
	}

	const bool comparable = s_prevTilesX == tilesX && s_prevTilesY == tilesY && s_prevTileSize == tileSize;
	Common::Array<bool> dirty;
	dirty.resize(tilesX * tilesY);
	for (uint i = 0; i < dirty.size(); i++)
		dirty[i] = !comparable || tileHash[i] != s_prevTileHash[i];

	// Join dirty tiles: runs along each row, then runs with the same span on
	// consecutive rows.
	struct Run { int x0, x1, y0, y1; };
	Common::Array<Run> open, next;
	for (int y = 0; y <= tilesY; y++) {
		next.clear();
		int x = 0;
		while (y < tilesY && x < tilesX) {
			if (!dirty[y * tilesX + x]) {
				x++;
				continue;
			}
			int x0 = x;
			while (x < tilesX && dirty[y * tilesX + x])
				x++;
			Run run = { x0, x - 1, y, y };
			for (uint k = 0; k < open.size(); k++) {
				if (open[k].x0 == run.x0 && open[k].x1 == run.x1) {
					run.y0 = open[k].y0;
					open.remove_at(k);
					break;
				}
			}
			next.push_back(run);
		}
		for (const Run &run : open) {
			Common::Rect rect(run.x0 * tileSize, run.y0 * tileSize, (run.x1 + 1) * tileSize, (run.y1 + 1) * tileSize);
			rect.clip(renderRect);
			rectangles.push_back(DirtyRectangle(rect, 0, 0, 255));
		}
		open = next;
	}

	s_prevTileHash = tileHash;
	s_prevTilesX = tilesX;
	s_prevTilesY = tilesY;
	s_prevTileSize = tileSize;
}

void GLContext::presentBufferDirtyRects(Common::List<Common::Rect> &dirtyAreas) {
	DirtyDiffStats &st = g_dirtyDiffStats;
	uint64 tp0 = benchRdtsc();

	DrawCallArray prev, cur;
	prev.reserve(_previousFrameDrawCallsQueue.size());
	cur.reserve(_drawCallsQueue.size());
	for (auto *call : _previousFrameDrawCallsQueue)
		prev.push_back(call);
	for (auto *call : _drawCallsQueue)
		cur.push_back(call);

	uint64 tk0 = benchRdtsc();
	DrawCallKeyArray prevKeys, curKeys;
	prevKeys.resize(prev.size());
	curKeys.resize(cur.size());
	for (uint i = 0; i < prev.size(); i++) {
		prevKeys[i].region = prev[i]->getDirtyRegion();
		prevKeys[i].type = prev[i]->getType();
	}
	for (uint i = 0; i < cur.size(); i++) {
		curKeys[i].region = cur[i]->getDirtyRegion();
		curKeys[i].type = cur[i]->getType();
	}
	uint64 keyTicks = benchRdtsc() - tk0;

	// Run both diffs every frame for the statistics; the selected one is used.
	Common::List<DirtyRectangle> byPosition, realigned;
	uint unmatchedPos = 0, unmatchedRealign = 0, resyncs = 0;

	uint64 t0 = benchRdtsc();
	diffByPosition(prev, prevKeys, cur, curKeys, byPosition, unmatchedPos);
	mergeDirtyRectangles(byPosition, renderRect);
	uint64 t1 = benchRdtsc();
	diffRealign(prev, prevKeys, cur, curKeys, g_dirtyDiffWindow, realigned, unmatchedRealign, resyncs);
	(void)resyncs;
	mergeDirtyRectangles(realigned, renderRect);
	uint64 t2 = benchRdtsc();

	uint64 t3 = benchRdtsc();
	Common::Array<uint64> callHashes;
	callHashes.resize(cur.size());
	for (uint i = 0; i < cur.size(); i++)
		callHashes[i] = cur[i]->hash();
	uint64 t4 = benchRdtsc();
	Common::List<DirtyRectangle> tiles;
	diffTiles(cur, callHashes, renderRect, g_dirtyTileSize, tiles);
	uint64 t5 = benchRdtsc();

	const uint64 screenArea = (uint64)renderRect.width() * renderRect.height();
	st.frames++;
	st.drawCalls += cur.size();
	st.unmatchedPos += unmatchedPos;
	st.unmatchedRealign += unmatchedRealign;
	st.screenArea += screenArea;
	st.hashTicks += t4 - t3;
	st.ticks[DirtyDiffStats::kPosition] += t1 - t0 + keyTicks;
	st.ticks[DirtyDiffStats::kRealign] += t2 - t1 + keyTicks;
	st.ticks[DirtyDiffStats::kTiles] += t5 - t3;
	const Common::List<DirtyRectangle> *results[DirtyDiffStats::kMethods] = { &byPosition, &realigned, &tiles };
	for (int m = 0; m < DirtyDiffStats::kMethods; m++) {
		const uint64 area = rectanglesArea(*results[m]);
		st.area[m] += area;
		st.rects[m] += results[m]->size();
		st.hist[m][MIN<uint64>(10, area * 10 / screenArea)]++;
	}

	Common::List<DirtyRectangle> &rectangles = g_dirtyDiffMode == 2 ? tiles : g_dirtyDiffMode == 1 ? realigned : byPosition;
	st.statsTicks += benchRdtsc() - tp0;

	// Verification: the reference buffers hold the frame drawn from scratch
	// every frame (execute all calls, no clipping), as presentBufferSimple does.
	const int fbW = fb->getPixelBufferWidth(), fbH = fb->getPixelBufferHeight();
	const size_t colorBytes = (size_t)fb->getPixelBufferPitch() * fbH;
	const size_t zBytes = (size_t)fbW * fbH * sizeof(uint);
	const size_t sBytes = fb->getStencilBuffer() ? (size_t)fbW * fbH : 0;
	if (g_dirtyVerify && s_refColor.empty()) {
		s_refColor.resize(colorBytes);
		s_refZ.resize(zBytes);
		s_refStencil.resize(sBytes);
		memcpy(s_refColor.data(), fb->getPixelBuffer(), colorBytes);
		memcpy(s_refZ.data(), fb->getZBufferMutable(), zBytes);
		if (sBytes)
			memcpy(s_refStencil.data(), fb->getStencilBuffer(), sBytes);
	}

	if (!rectangles.empty()) {
		for (auto &rect : rectangles) {
			dirtyAreas.push_back(rect.rectangle);
		}

		// Execute draw calls.
		uint64 te0 = benchRdtsc(); // BENCH
		for (auto &drawCall : _drawCallsQueue) {
			Common::Rect drawCallRegion = drawCall->getDirtyRegion();
			for (auto &rect : rectangles) {
				Common::Rect dirtyRegion = rect.rectangle;
				if (dirtyRegion.intersects(drawCallRegion)) {
					uint64 tx0 = benchRdtsc();
					drawCall->execute(true, &dirtyRegion);
					st.typeTicks[drawCall->getType()] += benchRdtsc() - tx0;
					st.typeExecutes[drawCall->getType()]++;
					st.executes++;
				}
			}
		}
		st.execTicks += benchRdtsc() - te0;

	}

	if (g_dirtyVerify) {
		verifyDirtyFrame(colorBytes, zBytes, sBytes);
	}

	if (!rectangles.empty()) {
		if (_debugRectsEnabled) {
			// Draw debug rectangles.
			// Note: white rectangles are rectangle that contained other rectangles
			// blue rectangles are rectangle merged from other rectangles
			// red rectangles are original dirty rects

			fb->enableBlending(false);
			fb->enableAlphaTest(false);

			for (auto &rect : rectangles) {
				debugDrawRectangle(rect.rectangle, rect.r, rect.g, rect.b);
			}

			fb->enableBlending(blending_enabled);
			fb->enableAlphaTest(alpha_test_enabled);
		}
	}

	uint64 td0 = benchRdtsc();
	// Dispose not necessary draw calls.
	for (auto &p : _previousFrameDrawCallsQueue) {
		delete p;
	}

	_previousFrameDrawCallsQueue = _drawCallsQueue;
	_drawCallsQueue.clear();

	disposeResources();
	st.disposeTicks += benchRdtsc() - td0;

	finishFrameAllocStats();
	_currentAllocatorIndex = (_currentAllocatorIndex + 1) & 0x1;
	resetFrameAllocator(_currentAllocatorIndex);
}

// BENCH: draws the frame from scratch on top of the reference buffers (the
// previous frame drawn from scratch) and compares it with the dirty rect result.
void GLContext::verifyDirtyFrame(size_t colorBytes, size_t zBytes, size_t sBytes) {
	DirtyDiffStats &st = g_dirtyDiffStats;
	byte *color = fb->getPixelBuffer();
	byte *z = (byte *)fb->getZBufferMutable();
	byte *stencil = fb->getStencilBuffer();

	s_saveColor.resize(colorBytes);
	s_saveZ.resize(zBytes);
	s_saveStencil.resize(sBytes);
	memcpy(s_saveColor.data(), color, colorBytes);
	memcpy(s_saveZ.data(), z, zBytes);
	if (sBytes)
		memcpy(s_saveStencil.data(), stencil, sBytes);

	memcpy(color, s_refColor.data(), colorBytes);
	memcpy(z, s_refZ.data(), zBytes);
	if (sBytes)
		memcpy(stencil, s_refStencil.data(), sBytes);
	for (const auto &drawCall : _drawCallsQueue)
		drawCall->execute(true);

	const int w = fb->getPixelBufferWidth(), h = fb->getPixelBufferHeight();
	const int bpp = fb->getPixelBufferBpp(), pitch = fb->getPixelBufferPitch();
	uint64 bad = 0;
	for (int y = 0; y < h; y++) {
		const byte *a = color + y * pitch, *b = s_saveColor.data() + y * pitch;
		if (memcmp(a, b, w * bpp) == 0)
			continue;
		for (int x = 0; x < w; x++)
			if (memcmp(a + x * bpp, b + x * bpp, bpp) != 0)
				bad++;
	}

	memcpy(s_refColor.data(), color, colorBytes);
	memcpy(s_refZ.data(), z, zBytes);
	if (sBytes)
		memcpy(s_refStencil.data(), stencil, sBytes);
	memcpy(color, s_saveColor.data(), colorBytes);
	memcpy(z, s_saveZ.data(), zBytes);
	if (sBytes)
		memcpy(stencil, s_saveStencil.data(), sBytes);

	st.verifyFrames++;
	if (bad) {
		if (!st.verifyBadFrames)
			st.verifyFirstBadFrame = st.frames;
		st.verifyBadFrames++;
		st.verifyBadPixels += bad;
		st.verifyMaxBadPixels = MAX<uint64>(st.verifyMaxBadPixels, bad);
	}
}

void GLContext::presentBufferSimple(Common::List<Common::Rect> &dirtyAreas) {
	dirtyAreas.push_back(Common::Rect(fb->getPixelBufferWidth(), fb->getPixelBufferHeight()));

	for (const auto &drawCall : _drawCallsQueue) {
		drawCall->execute(true);
		delete drawCall;
	}

	_drawCallsQueue.clear();

	disposeResources();

	finishFrameAllocStats();
	resetFrameAllocator(_currentAllocatorIndex);
}

void presentBuffer(Common::List<Common::Rect> &dirtyAreas) {
	GLContext *c = gl_get_context();
	if (c->_enableDirtyRectangles) {
		c->presentBufferDirtyRects(dirtyAreas);
	} else {
		c->presentBufferSimple(dirtyAreas);
	}
}

void presentBuffer() {
	Common::List<Common::Rect> dirtyAreas;
	presentBuffer(dirtyAreas);
}

bool DrawCall::operator==(const DrawCall &other) const {
	if (_type == other._type) {
		switch (_type) {
		case DrawCall_Rasterization:
			return *(const RasterizationDrawCall *)this == (const RasterizationDrawCall &)other;
			break;
		case DrawCall_Blitting:
			return *(const BlittingDrawCall *)this == (const BlittingDrawCall &)other;
			break;
		case DrawCall_Clear:
			return *(const ClearBufferDrawCall *)this == (const ClearBufferDrawCall &)other;
			break;
		default:
			return false;
		}
	} else {
		return false;
	}
}


RasterizationDrawCall::RasterizationDrawCall() : DrawCall(DrawCall_Rasterization) {
	GLContext *c = gl_get_context();
	_vertexCount = c->vertex_cnt;
	_vertex = (GLVertex *) Internal::allocateFrame(_vertexCount * sizeof(GLVertex), kFrameAllocRasterVertices);
	_drawTriangleFront = c->draw_triangle_front;
	_drawTriangleBack = c->draw_triangle_back;
	memcpy(_vertex, c->vertex, sizeof(GLVertex) * _vertexCount);
	_state = captureState();
	if (c->_enableDirtyRectangles) {
		computeDirtyRegion();
	}
}

void RasterizationDrawCall::computeDirtyRegion() {
	int clip_code = 0xf;

	for (int i = 0; i < _vertexCount; i++) {
		clip_code &= _vertex[i].clip_code;
	}

	if (!clip_code) {
		GLContext *c = gl_get_context();
		int xmax = c->fb->getPixelBufferWidth() - 1;
		int ymax = c->fb->getPixelBufferHeight() - 1;
		int left = xmax, right = 0, top = ymax, bottom = 0;
		for (int i = 0; i < _vertexCount; i++) {
			GLVertex *v = &_vertex[i];
			if (v->clip_code)
				c->gl_transform_to_viewport(v);
			left =   MIN(left,   v->clip_code & 0x1 ?    0 : v->zp.x);
			right =  MAX(right,  v->clip_code & 0x2 ? xmax : v->zp.x);
			bottom = MAX(bottom, v->clip_code & 0x4 ? ymax : v->zp.y);
			top =    MIN(top,    v->clip_code & 0x8 ?    0 : v->zp.y);
		}
		// Note: clipping outside of Rect is required despite above clip_code checks,
		// as vertices far on the Z axis will overflow X and/or Y coordinates.
		// This happens in EMI intro, for example.
		_dirtyRegion = Common::Rect(
			MAX(0, left),
			MAX(0, top),
			MIN(right, xmax) + 1,
			MIN(bottom, ymax) + 1
		);
	}
}

void RasterizationDrawCall::execute(bool restoreState, const Common::Rect *clippingRectangle) const {
	GLContext *c = gl_get_context();

	RasterizationDrawCall::RasterizationState backupState;
	if (restoreState) {
		backupState = captureState();
	}
	applyState(_state, clippingRectangle);

	GLVertex *prevVertex = c->vertex;
	int prevVertexCount = c->vertex_cnt;

	c->vertex = _vertex;
	c->vertex_cnt = _vertexCount;
	c->draw_triangle_front = (gl_draw_triangle_func)_drawTriangleFront;
	c->draw_triangle_back = (gl_draw_triangle_func)_drawTriangleBack;

	int cnt = c->vertex_cnt;

	switch (c->begin_type) {
	case TGL_POINTS:
		for(int i = 0; i < cnt; i++) {
			c->gl_draw_point(&c->vertex[i]);
		}
		break;
	case TGL_LINES:
		for(int i = 0; i < cnt / 2; i++) {
			c->gl_draw_line(&c->vertex[i * 2], &c->vertex[i * 2 + 1]);
		}
		break;
	case TGL_LINE_LOOP:
		c->gl_draw_line(&c->vertex[cnt - 1], &c->vertex[0]);
		// Fall through...
	case TGL_LINE_STRIP:
		for(int i = 0; i < cnt - 1; i++) {
			c->gl_draw_line(&c->vertex[i], &c->vertex[i + 1]);
		}
		break;
	case TGL_TRIANGLES:
		for(int i = 0; i < cnt; i += 3) {
			c->gl_draw_triangle(&c->vertex[i], &c->vertex[i + 1], &c->vertex[i + 2]);
		}
		break;
	case TGL_TRIANGLE_STRIP:
		while (cnt >= 3) {
			// needed to respect triangle orientation
			switch (cnt & 1) {
			case 0:
				c->gl_draw_triangle(&c->vertex[2], &c->vertex[1], &c->vertex[0]);
				break;
			case 1:
				c->gl_draw_triangle(&c->vertex[0], &c->vertex[1], &c->vertex[2]);
				break;
			}
			cnt--;
			c->vertex++;
		}
		break;
	case TGL_TRIANGLE_FAN:
		for(int i = 1; i < cnt - 1; i++) {
			c->gl_draw_triangle(&c->vertex[0], &c->vertex[i], &c->vertex[i + 1]);
		}
		break;
	case TGL_QUADS:
		for(int i = 0; i < cnt; i += 4) {
			c->vertex[i + 2].edge_flag = 0;
			c->gl_draw_triangle(&c->vertex[i], &c->vertex[i + 1], &c->vertex[i + 2]);
			c->vertex[i + 2].edge_flag = 1;
			c->vertex[i + 0].edge_flag = 0;
			c->gl_draw_triangle(&c->vertex[i], &c->vertex[i + 2], &c->vertex[i + 3]);
		}
		break;
	case TGL_QUAD_STRIP:
		for (int i = 0; i + 3 < cnt; i += 2) {
			c->gl_draw_triangle(&c->vertex[i], &c->vertex[i + 1], &c->vertex[i + 2]);
			c->gl_draw_triangle(&c->vertex[i + 1], &c->vertex[i + 3], &c->vertex[i + 2]);
		}
		break;
	case TGL_POLYGON: {
		for (int i = c->vertex_cnt; i >= 3; i--) {
			c->gl_draw_triangle(&c->vertex[i - 1], &c->vertex[0], &c->vertex[i - 2]);
		}
		break;
	}
	default:
		error("glBegin: type %x not handled", c->begin_type);
	}

	c->vertex = prevVertex;
	c->vertex_cnt = prevVertexCount;

	if (restoreState) {
		applyState(backupState, nullptr);
	}
}

RasterizationDrawCall::RasterizationState RasterizationDrawCall::captureState() const {
	RasterizationState state;
	GLContext *c = gl_get_context();
	state.enableScissor = c->scissor_test_enabled;
	state.enableBlending = c->blending_enabled;
	state.sfactor = c->source_blending_factor;
	state.dfactor = c->destination_blending_factor;
	state.alphaTestEnabled = c->alpha_test_enabled;
	state.alphaFunc = c->alpha_test_func;
	state.alphaRefValue = c->alpha_test_ref_val;
	state.depthTestEnabled = c->depth_test_enabled;
	state.depthFunction = c->depth_func;
	state.depthWriteMask = c->depth_write_mask;
	state.stencilTestEnabled = c->stencil_test_enabled;
	state.stencilTestFunc = c->stencil_test_func;
	state.stencilValue = c->stencil_ref_val;
	state.stencilMask = c->stencil_mask;
	state.stencilWriteMask = c->stencil_write_mask;
	state.stencilSfail = c->stencil_sfail;
	state.stencilDpfail = c->stencil_dpfail;
	state.stencilDppass = c->stencil_dppass;
	state.polygonStippleEnabled = c->polygon_stipple_enabled;
	state.offsetStates = c->offset_states;
	state.offsetFactor = c->offset_factor;
	state.offsetUnits = c->offset_units;

	state.cullFaceEnabled = c->cull_face_enabled;
	state.beginType = c->begin_type;
	state.colorMaskRed = c->color_mask_red;
	state.colorMaskGreen = c->color_mask_green;
	state.colorMaskBlue = c->color_mask_blue;
	state.colorMaskAlpha = c->color_mask_alpha;
	state.currentFrontFace = c->current_front_face;
	state.currentShadeModel = c->current_shade_model;
	state.polygonModeBack = c->polygon_mode_back;
	state.polygonModeFront = c->polygon_mode_front;
	state.texture2DEnabled = c->texture_2d_enabled;
	state.texture = c->current_texture;
	state.wrapS = c->texture_wrap_s;
	state.wrapT = c->texture_wrap_t;
	state.textureEnv = c->_texEnv;
	state.lightingEnabled = c->lighting_enabled;
	state.textureVersion = c->current_texture->versionNumber;
	state.fogEnabled = c->fog_enabled;
	state.fogColorR = c->fog_color.X;
	state.fogColorG = c->fog_color.Y;
	state.fogColorB = c->fog_color.Z;
	state.stippleColor = c->stippleColor;
	state.two_color_stipple_enabled = c->two_color_stipple_enabled;

	memcpy(state.scissor, c->scissor, sizeof(state.scissor));
	memcpy(state.viewportScaling, c->viewport.scale._v, sizeof(c->viewport.scale._v));
	memcpy(state.viewportTranslation, c->viewport.trans._v, sizeof(c->viewport.trans._v));
	memcpy(state.polygonStipplePattern, c->polygon_stipple_pattern, sizeof(c->polygon_stipple_pattern));

	return state;
}

void RasterizationDrawCall::applyState(const RasterizationDrawCall::RasterizationState &state, const Common::Rect *clippingRectangle) const {
	GLContext *c = gl_get_context();
	c->fb->setupScissor(state.enableScissor, state.scissor, clippingRectangle);
	c->fb->enableBlending(state.enableBlending);
	c->fb->setBlendingFactors(state.sfactor, state.dfactor);
	c->fb->enableAlphaTest(state.alphaTestEnabled);
	c->fb->setAlphaTestFunc(state.alphaFunc, state.alphaRefValue);
	c->fb->setDepthFunc(state.depthFunction);
	c->fb->enableDepthWrite(state.depthWriteMask);
	c->fb->enableDepthTest(state.depthTestEnabled);
	c->fb->enableStencilTest(state.stencilTestEnabled);
	c->fb->setStencilWriteMask(state.stencilWriteMask);
	c->fb->setStencilTestFunc(state.stencilTestFunc, state.stencilValue, state.stencilMask);
	c->fb->setStencilOp(state.stencilSfail, state.stencilDpfail, state.stencilDppass);
	c->fb->setOffsetStates(state.offsetStates);
	c->fb->setOffsetFactor(state.offsetFactor);
	c->fb->setOffsetUnits(state.offsetUnits);
	c->fb->setFogEnabled(state.fogEnabled);
	c->fb->setFogColor(state.fogColorR, state.fogColorG, state.fogColorB);
	c->fb->setPolygonStipplePattern(state.polygonStipplePattern);
	c->fb->setStippleColor(state.stippleColor >> 16, (state.stippleColor >> 8) & 0xff, state.stippleColor & 0xff);
	c->fb->enableTwoColorStipple(state.two_color_stipple_enabled);
	c->fb->enablePolygonStipple(state.polygonStippleEnabled);

	c->scissor_test_enabled = state.enableScissor;
	c->blending_enabled = state.enableBlending;
	c->source_blending_factor = state.sfactor;
	c->destination_blending_factor = state.dfactor;
	c->alpha_test_enabled = state.alphaTestEnabled;
	c->alpha_test_func = state.alphaFunc;
	c->alpha_test_ref_val = state.alphaRefValue;
	c->depth_test_enabled = state.depthTestEnabled;
	c->depth_func = state.depthFunction;
	c->depth_write_mask = state.depthWriteMask;
	c->stencil_test_enabled = state.stencilTestEnabled;
	c->stencil_test_func = state.stencilTestFunc;
	c->stencil_ref_val = state.stencilValue;
	c->stencil_mask = state.stencilMask;
	c->stencil_write_mask = state.stencilWriteMask;
	c->stencil_sfail = state.stencilSfail;
	c->stencil_dpfail = state.stencilDpfail;
	c->stencil_dppass = state.stencilDppass;
	c->offset_states = state.offsetStates;
	c->offset_factor = state.offsetFactor;
	c->offset_units = state.offsetUnits;

	c->lighting_enabled = state.lightingEnabled;
	c->cull_face_enabled = state.cullFaceEnabled;
	c->begin_type = state.beginType;
	c->color_mask_red = state.colorMaskRed;
	c->color_mask_green = state.colorMaskGreen;
	c->color_mask_blue = state.colorMaskBlue;
	c->color_mask_alpha = state.colorMaskAlpha;
	c->current_front_face = state.currentFrontFace;
	c->current_shade_model = state.currentShadeModel;
	c->polygon_mode_back = state.polygonModeBack;
	c->polygon_mode_front = state.polygonModeFront;
	c->texture_2d_enabled = state.texture2DEnabled;
	c->current_texture = state.texture;
	c->texture_wrap_s = state.wrapS;
	c->texture_wrap_t = state.wrapT;
	c->_texEnv = state.textureEnv;
	c->fog_enabled = state.fogEnabled;
	c->fog_color = Vector4(state.fogColorR, state.fogColorG, state.fogColorB, 1.0f);

	memcpy(c->scissor, state.scissor, sizeof(c->scissor));
	memcpy(c->viewport.scale._v, state.viewportScaling, sizeof(c->viewport.scale._v));
	memcpy(c->viewport.trans._v, state.viewportTranslation, sizeof(c->viewport.trans._v));
}

bool RasterizationDrawCall::operator==(const RasterizationDrawCall &other) const {
	if (_vertexCount == other._vertexCount &&
		_drawTriangleFront == other._drawTriangleFront &&
		_drawTriangleBack == other._drawTriangleBack &&
		_state == other._state) {
		for (int i = 0; i < _vertexCount; i++) {
			if ((_vertex[i] != other._vertex[i])) {
				return false;
			}
		}
		return true;
	}
	return false;
}


BlittingDrawCall::BlittingDrawCall(BlitImage *image, const BlitTransform &transform, BlittingMode blittingMode) : DrawCall(DrawCall_Blitting), _transform(transform), _mode(blittingMode), _image(image) {
	tglIncBlitImageRef(image);
	_blitState = captureState();
	_imageVersion = tglGetBlitImageVersion(image);
	if (gl_get_context()->_enableDirtyRectangles) {
		computeDirtyRegion();
	}
}

BlittingDrawCall::~BlittingDrawCall() {
	tglDeleteBlitImage(_image);
}

void BlittingDrawCall::execute(bool restoreState, const Common::Rect *clippingRectangle) const {
	BlittingState backupState;
	if (restoreState) {
		backupState = captureState();
	}
	applyState(_blitState, clippingRectangle);

	switch (_mode) {
	case BlittingDrawCall::BlitMode_Regular:
		Internal::tglBlit(_image, _transform);
		break;
	case BlittingDrawCall::BlitMode_Fast:
		Internal::tglBlitFast(_image, _transform._destinationRectangle.left, _transform._destinationRectangle.top);
		break;
	case BlittingDrawCall::BlitMode_ZBuffer:
		Internal::tglBlitZBuffer(_image, _transform._destinationRectangle.left, _transform._destinationRectangle.top);
		break;
	default:
		break;
	}
	if (restoreState) {
		applyState(backupState, nullptr);
	}
}

BlittingDrawCall::BlittingState BlittingDrawCall::captureState() const {
	BlittingState state;
	TinyGL::GLContext *c = gl_get_context();
	state.enableScissor = c->scissor_test_enabled;
	state.enableBlending = c->blending_enabled;
	state.sfactor = c->source_blending_factor;
	state.dfactor = c->destination_blending_factor;
	state.alphaTest = c->alpha_test_enabled;
	state.alphaFunc = c->alpha_test_func;
	state.alphaRefValue = c->alpha_test_ref_val;
	state.depthTestEnabled = c->depth_test_enabled;

	memcpy(state.scissor, c->scissor, sizeof(state.scissor));
	return state;
}

void BlittingDrawCall::applyState(const BlittingState &state, const Common::Rect *clippingRectangle) const {
	TinyGL::GLContext *c = gl_get_context();
	c->fb->setupScissor(state.enableScissor, state.scissor, clippingRectangle);
	c->fb->enableBlending(state.enableBlending);
	c->fb->setBlendingFactors(state.sfactor, state.dfactor);
	c->fb->enableAlphaTest(state.alphaTest);
	c->fb->setAlphaTestFunc(state.alphaFunc, state.alphaRefValue);
	c->fb->enableDepthTest(state.depthTestEnabled);

	c->scissor_test_enabled = state.enableScissor;
	c->blending_enabled = state.enableBlending;
	c->source_blending_factor = state.sfactor;
	c->destination_blending_factor = state.dfactor;
	c->alpha_test_enabled = state.alphaTest;
	c->alpha_test_func = state.alphaFunc;
	c->alpha_test_ref_val = state.alphaRefValue;
	c->depth_test_enabled = state.depthTestEnabled;

	memcpy(c->scissor, state.scissor, sizeof(c->scissor));
}

void BlittingDrawCall::computeDirtyRegion() {
	int blitWidth = _transform._destinationRectangle.width();
	int blitHeight = _transform._destinationRectangle.height();
	if (blitWidth == 0) {
		if (_transform._sourceRectangle.width() != 0) {
			blitWidth = _transform._sourceRectangle.width();
		} else {
			tglGetBlitImageSize(_image, blitWidth, blitHeight);
		}
	}
	if (blitHeight == 0) {
		if (_transform._sourceRectangle.height() != 0) {
			blitHeight = _transform._sourceRectangle.height();
		} else {
			tglGetBlitImageSize(_image, blitWidth, blitHeight);
		}
	}
	if (blitWidth == 0 || blitHeight == 0) {
		_dirtyRegion = Common::Rect();
	} else {
		_dirtyRegion = Common::Rect(
			_transform._destinationRectangle.left,
			_transform._destinationRectangle.top,
			_transform._destinationRectangle.left + blitWidth + 1,
			_transform._destinationRectangle.top + blitHeight + 1
		);
		_dirtyRegion.clip(gl_get_context()->renderRect);
	}
}

bool BlittingDrawCall::operator==(const BlittingDrawCall &other) const {
	return
		_mode == other._mode &&
		_image == other._image &&
		_transform == other._transform &&
		_blitState == other._blitState &&
		_imageVersion == tglGetBlitImageVersion(other._image);
}


ClearBufferDrawCall::ClearBufferDrawCall(bool clearZBuffer, int zValue,
	                                 bool clearColorBuffer, int rValue, int gValue, int bValue,
	                                 bool clearStencilBuffer, int stencilValue)
	: _clearZBuffer(clearZBuffer), _clearColorBuffer(clearColorBuffer), _zValue(zValue),
	  _rValue(rValue), _gValue(gValue), _bValue(bValue), _clearStencilBuffer(clearStencilBuffer),
	  _stencilValue(stencilValue), DrawCall(DrawCall_Clear) {
	_clearState = captureState();
	TinyGL::GLContext *c = gl_get_context();
	if (c->_enableDirtyRectangles) {
		_dirtyRegion = c->renderRect;
	}
}

void ClearBufferDrawCall::execute(bool restoreState, const Common::Rect *clippingRectangle) const {
	ClearBufferState backupState;
	if (restoreState) {
		backupState = captureState();
	}
	applyState(_clearState, clippingRectangle);

	TinyGL::GLContext *c = gl_get_context();
	c->fb->clear(_clearZBuffer, _zValue, _clearColorBuffer, _rValue, _gValue, _bValue, _clearStencilBuffer, _stencilValue);

	if (restoreState) {
		applyState(backupState, nullptr);
	}
}

ClearBufferDrawCall::ClearBufferState ClearBufferDrawCall::captureState() const {
	ClearBufferState state;
	TinyGL::GLContext *c = gl_get_context();
	state.enableScissor = c->scissor_test_enabled;
	memcpy(state.scissor, c->scissor, sizeof(state.scissor));
	return state;
}

void ClearBufferDrawCall::applyState(const ClearBufferState &state, const Common::Rect *clippingRectangle) const {
	TinyGL::GLContext *c = gl_get_context();
	c->fb->setupScissor(state.enableScissor, state.scissor, clippingRectangle);

	c->scissor_test_enabled = state.enableScissor;
	memcpy(c->scissor, state.scissor, sizeof(c->scissor));
}

bool ClearBufferDrawCall::operator==(const ClearBufferDrawCall &other) const {
	return
		_clearZBuffer == other._clearZBuffer &&
		_clearColorBuffer == other._clearColorBuffer &&
		_clearStencilBuffer == other._clearStencilBuffer &&
		_rValue == other._rValue &&
		_gValue == other._gValue &&
		_bValue == other._bValue &&
		_zValue == other._zValue &&
		_stencilValue == other._stencilValue &&
		_clearState == other._clearState;
}


// BENCH: draw call hashing for the tile diff. Hashes at least everything the
// operator== compare, so equal calls always give equal hashes.
uint64 DrawCall::hash() const {
	switch (_type) {
	case DrawCall_Rasterization:
		return ((const RasterizationDrawCall *)this)->hash();
	case DrawCall_Blitting:
		return ((const BlittingDrawCall *)this)->hash();
	case DrawCall_Clear:
		return ((const ClearBufferDrawCall *)this)->hash();
	default:
		return 0;
	}
}

uint64 ClearBufferDrawCall::hash() const {
	uint64 h = hashMix(0x1234567ULL, DrawCall_Clear);
	h = hashMix(h, _clearZBuffer | (_clearColorBuffer << 1) | (_clearStencilBuffer << 2) | (_clearState.enableScissor << 3));
	h = hashMix(h, (uint32)_rValue);
	h = hashMix(h, (uint32)_gValue);
	h = hashMix(h, (uint32)_bValue);
	h = hashMix(h, (uint32)_zValue);
	h = hashMix(h, (uint32)_stencilValue);
	for (int i = 0; i < 4; i++)
		h = hashMix(h, (uint32)_clearState.scissor[i]);
	return h;
}

uint64 BlittingDrawCall::hash() const {
	uint64 h = hashMix(0x7654321ULL, DrawCall_Blitting);
	h = hashMix(h, _mode);
	h = hashPtr(h, _image);
	h = hashMix(h, (uint32)_imageVersion);
	h = hashRect(h, _transform._sourceRectangle);
	h = hashRect(h, _transform._destinationRectangle);
	h = hashMix(h, (uint32)_transform._rotation);
	h = hashMix(h, (uint32)_transform._originX);
	h = hashMix(h, (uint32)_transform._originY);
	h = hashFloat(h, _transform._aTint);
	h = hashFloat(h, _transform._rTint);
	h = hashFloat(h, _transform._gTint);
	h = hashFloat(h, _transform._bTint);
	h = hashMix(h, _transform._flipHorizontally | (_transform._flipVertically << 1));
	const BlittingState &b = _blitState;
	h = hashMix(h, b.enableScissor | (b.enableBlending << 1) | (b.alphaTest << 2));
	for (int i = 0; i < 4; i++)
		h = hashMix(h, (uint32)b.scissor[i]);
	h = hashMix(h, (uint32)b.sfactor);
	h = hashMix(h, (uint32)b.dfactor);
	h = hashMix(h, (uint32)b.alphaFunc);
	h = hashMix(h, (uint32)b.alphaRefValue);
	h = hashMix(h, (uint32)b.depthTestEnabled);
	return h;
}

uint64 RasterizationDrawCall::hash() const {
	uint64 h = hashMix(0xabcdefULL, DrawCall_Rasterization);
	h = hashMix(h, (uint32)_vertexCount);
	h = hashPtr(h, (const void *)_drawTriangleFront);
	h = hashPtr(h, (const void *)_drawTriangleBack);

	const RasterizationState &st = _state;
	h = hashMix(h, st.enableScissor | (st.enableBlending << 1) | (st.alphaTestEnabled << 2) | (st.depthTestEnabled << 3) |
		(st.stencilTestEnabled << 4) | (st.colorMaskRed << 5) | (st.colorMaskGreen << 6) | (st.colorMaskBlue << 7) |
		(st.colorMaskAlpha << 8) | (st.texture2DEnabled << 9) | (st.fogEnabled << 10) | (st.polygonStippleEnabled << 11) |
		(st.two_color_stipple_enabled << 12));
	const int ints[] = { st.sfactor, st.dfactor, st.alphaFunc, st.alphaRefValue, st.depthFunction, st.depthWriteMask,
		st.stencilTestFunc, st.stencilValue, st.stencilMask, st.stencilWriteMask, st.stencilSfail, st.stencilDpfail,
		st.stencilDppass, st.offsetStates, st.lightingEnabled, st.cullFaceEnabled, st.beginType, st.currentFrontFace,
		st.currentShadeModel, st.polygonModeBack, st.polygonModeFront, st.textureVersion,
		st.scissor[0], st.scissor[1], st.scissor[2], st.scissor[3], (int)st.wrapS, (int)st.wrapT, (int)st.stippleColor };
	for (uint i = 0; i < ARRAYSIZE(ints); i++)
		h = hashMix(h, (uint32)ints[i]);
	h = hashFloat(h, st.offsetFactor);
	h = hashFloat(h, st.offsetUnits);
	for (int i = 0; i < 3; i++) {
		h = hashFloat(h, st.viewportTranslation[i]);
		h = hashFloat(h, st.viewportScaling[i]);
	}
	h = hashFloat(h, st.fogColorR);
	h = hashFloat(h, st.fogColorG);
	h = hashFloat(h, st.fogColorB);
	h = hashPtr(h, st.texture);
	const GLTextureEnv &env = st.textureEnv;
	h = hashMix(h, env.envMode);
	h = hashMix(h, env.combineRGB);
	h = hashMix(h, env.combineAlpha);
	h = hashMix(h, ((uint32)env.constA << 24) | (env.constR << 16) | (env.constG << 8) | env.constB);
	h = hashMix(h, ((uint64)env.arg0.sourceRGB << 32) | env.arg0.operandRGB);
	h = hashMix(h, ((uint64)env.arg0.sourceAlpha << 32) | env.arg0.operandAlpha);
	h = hashMix(h, ((uint64)env.arg1.sourceRGB << 32) | env.arg1.operandRGB);
	h = hashMix(h, ((uint64)env.arg1.sourceAlpha << 32) | env.arg1.operandAlpha);
	if (st.polygonStippleEnabled) {
		for (int i = 0; i < 128; i += 4)
			h = hashMix(h, READ_UINT32(st.polygonStipplePattern + i));
	}

	for (int i = 0; i < _vertexCount; i++) {
		const GLVertex &v = _vertex[i];
		h = hashMix(h, ((uint64)(uint32)v.edge_flag << 32) | (uint32)v.clip_code);
		h = hashVec3(h, v.normal);
		h = hashVec4(h, v.coord);
		h = hashVec4(h, v.tex_coord);
		h = hashVec4(h, v.color);
		h = hashVec4(h, v.ec);
		h = hashVec4(h, v.pc);
		const ZBufferPoint &zp = v.zp;
		h = hashMix(h, ((uint64)(uint32)zp.x << 32) | (uint32)zp.y);
		h = hashMix(h, ((uint64)(uint32)zp.z << 32) | (uint32)zp.s);
		h = hashMix(h, ((uint64)(uint32)zp.t << 32) | (uint32)zp.r);
		h = hashMix(h, ((uint64)(uint32)zp.g << 32) | (uint32)zp.b);
		h = hashMix(h, (uint32)zp.a);
		if (st.fogEnabled) {
			h = hashFloat(h, v.fog_factor);
			h = hashMix(h, (uint32)zp.f);
		}
	}
	return h;
}

bool RasterizationDrawCall::RasterizationState::operator==(const RasterizationState &other) const {
	return
		enableScissor == other.enableScissor &&
		enableBlending == other.enableBlending &&
		sfactor == other.sfactor &&
		dfactor == other.dfactor &&
		alphaTestEnabled == other.alphaTestEnabled &&
		alphaFunc == other.alphaFunc &&
		alphaRefValue == other.alphaRefValue &&
		depthTestEnabled == other.depthTestEnabled &&
		depthFunction == other.depthFunction &&
		depthWriteMask == other.depthWriteMask &&
		stencilTestEnabled == other.stencilTestEnabled &&
		stencilTestFunc == other.stencilTestFunc &&
		stencilValue == other.stencilValue &&
		stencilMask == other.stencilMask &&
		stencilWriteMask == other.stencilWriteMask &&
		stencilSfail == other.stencilSfail &&
		stencilDpfail == other.stencilDpfail &&
		stencilDppass == other.stencilDppass &&
		offsetStates == other.offsetStates &&
		offsetFactor == other.offsetFactor &&
		offsetUnits == other.offsetUnits &&
		lightingEnabled == other.lightingEnabled &&
		cullFaceEnabled == other.cullFaceEnabled &&
		beginType == other.beginType &&
		colorMaskRed == other.colorMaskRed &&
		colorMaskGreen == other.colorMaskGreen &&
		colorMaskBlue == other.colorMaskBlue &&
		colorMaskAlpha == other.colorMaskAlpha &&
		currentFrontFace == other.currentFrontFace &&
		currentShadeModel == other.currentShadeModel &&
		polygonModeBack == other.polygonModeBack &&
		polygonModeFront == other.polygonModeFront &&
		texture2DEnabled == other.texture2DEnabled &&
		texture == other.texture &&
		textureVersion == texture->versionNumber &&
		fogEnabled == other.fogEnabled &&
		fogColorR == other.fogColorR &&
		fogColorG == other.fogColorG &&
		fogColorB == other.fogColorB &&
		scissor[0] == other.scissor[0] &&
		scissor[1] == other.scissor[1] &&
		scissor[2] == other.scissor[2] &&
		scissor[3] == other.scissor[3] &&
		viewportTranslation[0] == other.viewportTranslation[0] &&
		viewportTranslation[1] == other.viewportTranslation[1] &&
		viewportTranslation[2] == other.viewportTranslation[2] &&
		viewportScaling[0] == other.viewportScaling[0] &&
		viewportScaling[1] == other.viewportScaling[1] &&
		viewportScaling[2] == other.viewportScaling[2];
}

static const char *const kFrameAllocCategoryNames[kFrameAllocCategoryCount] = {
	"ClearBufferDrawCall",
	"RasterizationDrawCall",
	"RasterizationDrawCall vertices",
	"BlittingDrawCall",
	"other"
};

static const char *const kPrimitiveNames[FrameAllocStats::kNumPrimitiveTypes + 1] = {
	"POINTS", "LINES", "LINE_LOOP", "LINE_STRIP", "TRIANGLES",
	"TRIANGLE_STRIP", "TRIANGLE_FAN", "QUADS", "QUAD_STRIP", "POLYGON", "unknown"
};

void FrameAllocStats::clear() {
	memset(this, 0, sizeof(*this));
}

int FrameAllocStats::getVertexBucket(int vertexCount) {
	if (vertexCount <= 3)
		return 0;
	if (vertexCount <= 4)
		return 1;
	if (vertexCount <= 16)
		return 2;
	if (vertexCount <= 64)
		return 3;
	if (vertexCount <= 256)
		return 4;
	if (vertexCount <= 1024)
		return 5;
	return 6;
}

const char *FrameAllocStats::getVertexBucketName(int bucket) {
	static const char *const names[kNumVertexBuckets] = {
		"1-3 verts", "4 verts", "5-16 verts", "17-64 verts", "65-256 verts", "257-1024 verts", ">1024 verts"
	};
	return names[bucket];
}

void FrameAllocStats::add(FrameAllocCategory category, const char *label, size_t size, int primitive, int vertexCount) {
	totalBytes += size;
	totalCount++;
	categories[category].add(size);

	if (category == kFrameAllocRasterVertices) {
		if (primitive < 0 || primitive >= kNumPrimitiveTypes)
			primitive = kNumPrimitiveTypes;
		primitives[primitive].add(size);
		vertexBuckets[getVertexBucket(vertexCount)].add(size);
	}

	int i = findOrAddLabel(label);
	if (i < 0) {
		overflowLabels.add(size);
		return;
	}
	labels[i].add(size);
	labelCategories[i][category].add(size);
}

void FrameAllocStats::addOffscreen(const char *label, size_t size) {
	offscreen.add(size);
	int i = findOrAddLabel(label);
	if (i < 0)
		overflowOffscreen.add(size);
	else
		labelOffscreen[i].add(size);
}

void FrameAllocStats::addScissored(const char *label, size_t size) {
	scissored.add(size);
	int i = findOrAddLabel(label);
	if (i < 0)
		overflowScissored.add(size);
	else
		labelScissored[i].add(size);
}

int FrameAllocStats::findOrAddLabel(const char *label) {
	if (!label)
		label = "(unlabeled)";
	// Labels are compared by content (truncated to the stored length), the pointer
	// is only a cache: the same buffer may hold different text over time.
	if (label == lastLabelPtr && strncmp(labelNames[lastLabelIndex], label, kMaxLabelLength - 1) == 0)
		return lastLabelIndex;
	int i;
	for (i = 0; i < numLabels; i++) {
		if (strncmp(labelNames[i], label, kMaxLabelLength - 1) == 0)
			break;
	}
	if (i == numLabels) {
		if (numLabels == kMaxLabels)
			return -1;
		Common::strlcpy(labelNames[numLabels++], label, kMaxLabelLength);
	}
	lastLabelPtr = label;
	lastLabelIndex = i;
	return i;
}

static Common::String formatBytes(size_t bytes) {
	if (bytes >= 1024 * 1024)
		return Common::String::format("%.2f MiB", bytes / (1024.0 * 1024.0));
	if (bytes >= 1024)
		return Common::String::format("%.1f KiB", bytes / 1024.0);
	return Common::String::format("%u B", (uint)bytes);
}

static Common::String formatEntry(const char *name, const FrameAllocStats::Entry &e, size_t total, int indent = 2) {
	return Common::String::format("%*s%-40s %8u allocs %12s %5.1f%%  largest %s\n", indent, "", name,
		e.count, formatBytes(e.bytes).c_str(), total ? 100.0 * e.bytes / total : 0.0, formatBytes(e.largest).c_str());
}

static Common::String formatRejected(const char *name, const FrameAllocStats::Entry &e, int indent = 2) {
	return Common::String::format("%*s%-40s %8u calls  %12s saved  largest %s\n", indent, "", name,
		e.count, formatBytes(e.bytes).c_str(), formatBytes(e.largest).c_str());
}

Common::String FrameAllocStats::format(const char *title, size_t capacity) const {
	Common::String out = Common::String::format("%s (frame %u): %s in %u allocations, capacity %s (%.1f%% used)\n",
		title, frameNumber, formatBytes(totalBytes).c_str(), totalCount, formatBytes(capacity).c_str(),
		capacity ? 100.0 * totalBytes / capacity : 0.0);

	out += "By category:\n";
	for (int i = 0; i < kFrameAllocCategoryCount; i++) {
		if (categories[i].count)
			out += formatEntry(kFrameAllocCategoryNames[i], categories[i], totalBytes);
	}

	// Labels, largest first.
	int order[kMaxLabels];
	for (int i = 0; i < numLabels; i++)
		order[i] = i;
	for (int i = 1; i < numLabels; i++) {
		int v = order[i], j = i;
		for (; j > 0 && labels[order[j - 1]].bytes < labels[v].bytes; j--)
			order[j] = order[j - 1];
		order[j] = v;
	}
	out += "By label:\n";
	for (int n = 0; n < numLabels; n++) {
		int i = order[n];
		out += formatEntry(labelNames[i], labels[i], totalBytes);
		for (int c = 0; c < kFrameAllocCategoryCount; c++) {
			if (labelCategories[i][c].count && labelCategories[i][c].bytes != labels[i].bytes)
				out += formatEntry(kFrameAllocCategoryNames[c], labelCategories[i][c], totalBytes, 6);
		}
		if (labelOffscreen[i].count)
			out += formatRejected("rejected: off-screen", labelOffscreen[i], 6);
		if (labelScissored[i].count)
			out += formatRejected("rejected: outside scissor", labelScissored[i], 6);
	}
	if (overflowLabels.count)
		out += formatEntry("(label table full)", overflowLabels, totalBytes);
	if (overflowOffscreen.count)
		out += formatRejected("(label table full) rejected: off-screen", overflowOffscreen, 6);
	if (overflowScissored.count)
		out += formatRejected("(label table full) rejected: outside scissor", overflowScissored, 6);
	if (offscreen.count || scissored.count) {
		out += "Rejected before allocation (draw call + vertices, not in the totals):\n";
		if (offscreen.count)
			out += formatRejected("off-screen (outside one clip plane)", offscreen);
		if (scissored.count)
			out += formatRejected("outside scissor box", scissored);
	}

	if (categories[kFrameAllocRasterVertices].count) {
		out += "Vertex arrays by primitive:\n";
		for (int i = 0; i <= kNumPrimitiveTypes; i++) {
			if (primitives[i].count)
				out += formatEntry(kPrimitiveNames[i], primitives[i], totalBytes);
		}
		out += "Vertex arrays by vertex count:\n";
		for (int i = 0; i < kNumVertexBuckets; i++) {
			if (vertexBuckets[i].count)
				out += formatEntry(getVertexBucketName(i), vertexBuckets[i], totalBytes);
		}
	}
	return out;
}

uint64 g_benchAllocTicks = 0, g_benchStatsTicks = 0, g_benchPresentTicks = 0, g_benchSceneTicks = 0; // BENCH

void *GLContext::allocateFrame(size_t size, FrameAllocCategory category) {
	LinearAllocator &allocator = _drawCallAllocator[_currentAllocatorIndex];
	FrameAllocStats &stats = _frameAllocStats[_currentAllocatorIndex];
	uint64 t0 = benchRdtsc(); // BENCH
	void *ptr = allocator.allocate(size);
	uint64 t1 = benchRdtsc();
	stats.add(category, _frameAllocLabel, size, begin_type, vertex_cnt);
	g_benchAllocTicks += t1 - t0;
	g_benchStatsTicks += benchRdtsc() - t1;
	return ptr;
}

void GLContext::finishFrameAllocStats() {
	FrameAllocStats &stats = _frameAllocStats[_currentAllocatorIndex];
	stats.frameNumber = _frameAllocFrameNumber++;
	_lastFrameAllocStats = stats;
	if (stats.totalBytes >= _peakFrameAllocStats.totalBytes)
		_peakFrameAllocStats = stats;
}

void GLContext::resetFrameAllocator(int index) {
	_drawCallAllocator[index].reset();
	_frameAllocStats[index].clear();
	_frameAllocStats[index].frameNumber = _frameAllocFrameNumber;
}

const char *setFrameAllocLabel(const char *label) {
	GLContext *c = gl_get_context();
	const char *previous = c->_frameAllocLabel;
	c->_frameAllocLabel = label;
	return previous;
}

void resetFrameAllocPeak() {
	GLContext *c = gl_get_context();
	c->_peakFrameAllocStats.clear();
}

Common::String getFrameAllocReport(FrameAllocReport which) {
	GLContext *c = gl_get_context();
	size_t capacity = c->_drawCallAllocator[c->_currentAllocatorIndex].getCapacity();
	Common::String out;
	switch (which) {
	case kFrameAllocReportCurrent:
		out = c->_frameAllocStats[c->_currentAllocatorIndex].format("Current frame", capacity);
		break;
	case kFrameAllocReportLastFrame:
		out = c->_lastFrameAllocStats.format("Last presented frame", capacity);
		break;
	case kFrameAllocReportPeakFrame:
		out = c->_peakFrameAllocStats.format("Peak frame", capacity);
		break;
	default:
		break;
	}
	out += Common::String::format("Allocator usage right now: [0] %s in %u blocks, [1] %s in %u blocks (current index %d%s)\n",
		formatBytes(c->_drawCallAllocator[0].getUsed()).c_str(), c->_drawCallAllocator[0].getBlockCount(),
		formatBytes(c->_drawCallAllocator[1].getUsed()).c_str(), c->_drawCallAllocator[1].getBlockCount(),
		c->_currentAllocatorIndex, c->_enableDirtyRectangles ? "; dirty rects keep the previous frame alive" : "");
	return out;
}

void *Internal::allocateFrame(int size, FrameAllocCategory category) {
	GLContext *c = gl_get_context();
	return c->allocateFrame(size, category);
}

} // end of namespace TinyGL
