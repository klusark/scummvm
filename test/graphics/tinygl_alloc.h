#include <cxxtest/TestSuite.h>

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"
#include "graphics/tinygl/zgl.h"

// Checks the draw call allocator accounting (TinyGL::getFrameAllocReport and friends).

class TinyGLAllocTestSuite : public CxxTest::TestSuite {
	TinyGL::ContextHandle *_context = nullptr;

	static TinyGL::GLContext *ctx() { return TinyGL::gl_get_context(); }

	static int findLabel(const TinyGL::FrameAllocStats &stats, const char *label) {
		for (int i = 0; i < stats.numLabels; i++) {
			if (strcmp(stats.labelNames[i], label) == 0)
				return i;
		}
		return -1;
	}

	void createContext(bool dirtyRects, uint32 memorySize = 5 * 1024 * 1024) {
		_context = TinyGL::createContext(16, 16, Graphics::PixelFormat::createFormatARGB32(), 2, false, dirtyRects, memorySize);
		TinyGL::setContext(_context);
		tglDisable(TGL_TEXTURE_2D);
		tglDisable(TGL_DEPTH_TEST);
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglViewport(0, 0, 16, 16);
	}

	void drawTriangle() {
		tglBegin(TGL_TRIANGLES);
		tglVertex2f(-1.0f, -1.0f);
		tglVertex2f(1.0f, -1.0f);
		tglVertex2f(0.0f, 1.0f);
		tglEnd();
	}

	void drawQuad() {
		tglBegin(TGL_QUADS);
		tglVertex2f(-1.0f, -1.0f);
		tglVertex2f(1.0f, -1.0f);
		tglVertex2f(1.0f, 1.0f);
		tglVertex2f(-1.0f, 1.0f);
		tglEnd();
	}

	void drawPolygon(int vertexCount) {
		tglBegin(TGL_POLYGON);
		for (int i = 0; i < vertexCount; i++) {
			float a = 6.2831853f * i / vertexCount;
			tglVertex2f(cosf(a), sinf(a));
		}
		tglEnd();
	}

public:
	void tearDown() {
		if (_context != nullptr) {
			TinyGL::destroyContext(_context);
			_context = nullptr;
		}
	}

	void testCategoriesAndTotals() {
		createContext(false);
		{
			TinyGL::FrameAllocLabelScope label("clear");
			tglClear(TGL_COLOR_BUFFER_BIT);
		}
		{
			TinyGL::FrameAllocLabelScope label("tri");
			drawTriangle();
		}

		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		const size_t clearSize = sizeof(TinyGL::ClearBufferDrawCall);
		const size_t rasterSize = sizeof(TinyGL::RasterizationDrawCall);
		const size_t vertSize = 3 * sizeof(TinyGL::GLVertex);

		TS_ASSERT_EQUALS(cur.totalCount, 3u);
		TS_ASSERT_EQUALS(cur.totalBytes, clearSize + rasterSize + vertSize);
		TS_ASSERT_EQUALS(cur.totalBytes, ctx()->_drawCallAllocator[ctx()->_currentAllocatorIndex].getUsed());
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocClearCall].count, 1u);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocClearCall].bytes, clearSize);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocRasterCall].count, 1u);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocRasterCall].bytes, rasterSize);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocRasterVertices].count, 1u);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocRasterVertices].bytes, vertSize);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocBlitCall].count, 0u);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocOther].count, 0u);

		// Labels
		TS_ASSERT_EQUALS(cur.numLabels, 2);
		int clear = findLabel(cur, "clear");
		int tri = findLabel(cur, "tri");
		TS_ASSERT(clear >= 0);
		TS_ASSERT(tri >= 0);
		TS_ASSERT_EQUALS(cur.labels[clear].bytes, clearSize);
		TS_ASSERT_EQUALS(cur.labels[tri].count, 2u);
		TS_ASSERT_EQUALS(cur.labels[tri].bytes, rasterSize + vertSize);
		TS_ASSERT_EQUALS(cur.labelCategories[tri][TinyGL::kFrameAllocRasterVertices].bytes, vertSize);

		// Vertex breakdowns
		TS_ASSERT_EQUALS(cur.primitives[TGL_TRIANGLES].count, 1u);
		TS_ASSERT_EQUALS(cur.primitives[TGL_TRIANGLES].bytes, vertSize);
		TS_ASSERT_EQUALS(cur.vertexBuckets[0].count, 1u);
		TS_ASSERT_EQUALS(cur.vertexBuckets[0].largest, vertSize);
	}

	void testPresentSnapshotsAndResets() {
		createContext(false);
		drawTriangle();
		const size_t frameBytes = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex].totalBytes;
		TS_ASSERT(frameBytes > 0);

		TinyGL::presentBuffer();

		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.totalBytes, 0u);
		TS_ASSERT_EQUALS(cur.totalCount, 0u);
		TS_ASSERT_EQUALS(cur.numLabels, 0);
		TS_ASSERT_EQUALS(cur.frameNumber, 1u);
		TS_ASSERT_EQUALS(ctx()->_lastFrameAllocStats.totalBytes, frameBytes);
		TS_ASSERT_EQUALS(ctx()->_lastFrameAllocStats.frameNumber, 0u);
		TS_ASSERT_EQUALS(ctx()->_peakFrameAllocStats.totalBytes, frameBytes);
		// Unlabeled allocations are grouped together.
		TS_ASSERT(findLabel(ctx()->_lastFrameAllocStats, "(unlabeled)") >= 0);
	}

	void testPeakKeepsBiggestFrame() {
		createContext(false);
		drawPolygon(100);
		drawTriangle();
		TinyGL::presentBuffer();
		const size_t bigFrame = ctx()->_lastFrameAllocStats.totalBytes;

		drawTriangle();
		TinyGL::presentBuffer();
		const size_t smallFrame = ctx()->_lastFrameAllocStats.totalBytes;

		TS_ASSERT(smallFrame < bigFrame);
		TS_ASSERT_EQUALS(ctx()->_peakFrameAllocStats.totalBytes, bigFrame);
		TS_ASSERT_EQUALS(ctx()->_peakFrameAllocStats.frameNumber, 0u);
		TS_ASSERT_EQUALS(ctx()->_lastFrameAllocStats.frameNumber, 1u);
		TS_ASSERT_EQUALS(ctx()->_peakFrameAllocStats.primitives[TGL_POLYGON].count, 1u);
		TS_ASSERT_EQUALS(ctx()->_peakFrameAllocStats.vertexBuckets[4].count, 1u); // 65-256 verts

		TinyGL::resetFrameAllocPeak();
		TS_ASSERT_EQUALS(ctx()->_peakFrameAllocStats.totalBytes, 0u);
		drawTriangle();
		TinyGL::presentBuffer();
		TS_ASSERT_EQUALS(ctx()->_peakFrameAllocStats.totalBytes, smallFrame);
	}

	void testVertexBuckets() {
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(1), 0);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(3), 0);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(4), 1);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(5), 2);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(16), 2);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(17), 3);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(64), 3);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(65), 4);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(256), 4);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(257), 5);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(1024), 5);
		TS_ASSERT_EQUALS(TinyGL::FrameAllocStats::getVertexBucket(1025), 6);

		createContext(false);
		drawQuad();
		drawPolygon(8);
		drawPolygon(8);
		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.primitives[TGL_QUADS].count, 1u);
		TS_ASSERT_EQUALS(cur.primitives[TGL_QUADS].bytes, 4 * sizeof(TinyGL::GLVertex));
		TS_ASSERT_EQUALS(cur.vertexBuckets[1].count, 1u);
		TS_ASSERT_EQUALS(cur.primitives[TGL_POLYGON].count, 2u);
		TS_ASSERT_EQUALS(cur.vertexBuckets[2].count, 2u);
		TS_ASSERT_EQUALS(cur.vertexBuckets[2].bytes, 16 * sizeof(TinyGL::GLVertex));
	}

	void testLabelScopeRestoresPrevious() {
		createContext(false);
		TS_ASSERT(TinyGL::setFrameAllocLabel("outer") == nullptr);
		{
			TinyGL::FrameAllocLabelScope label("inner");
			TS_ASSERT_EQUALS(strcmp(ctx()->_frameAllocLabel, "inner"), 0);
		}
		TS_ASSERT_EQUALS(strcmp(ctx()->_frameAllocLabel, "outer"), 0);
		TS_ASSERT_EQUALS(strcmp(TinyGL::setFrameAllocLabel(nullptr), "outer"), 0);
	}

	void testLabelsMatchByContent() {
		createContext(false);
		// Same text from different buffers ends up in one entry.
		char a[] = "same";
		char b[] = "same";
		TinyGL::setFrameAllocLabel(a);
		drawTriangle();
		TinyGL::setFrameAllocLabel(b);
		drawTriangle();
		TinyGL::setFrameAllocLabel(nullptr);
		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.numLabels, 1);
		TS_ASSERT_EQUALS(cur.labels[0].count, 4u);
	}

	void testLabelTableOverflow() {
		createContext(false);
		static char names[TinyGL::FrameAllocStats::kMaxLabels + 2][8];
		for (int i = 0; i < TinyGL::FrameAllocStats::kMaxLabels + 2; i++) {
			snprintf(names[i], sizeof(names[i]), "l%d", i);
			TinyGL::setFrameAllocLabel(names[i]);
			drawTriangle();
		}
		TinyGL::setFrameAllocLabel(nullptr);
		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.numLabels, (int)TinyGL::FrameAllocStats::kMaxLabels);
		TS_ASSERT_EQUALS(cur.overflowLabels.count, 4u); // 2 labels x (draw call + vertices)
		TS_ASSERT_EQUALS(cur.totalCount, (uint32)(TinyGL::FrameAllocStats::kMaxLabels + 2) * 2);
	}

	void testDirtyRectsAlternateAllocators() {
		createContext(true);
		drawTriangle();
		const int first = ctx()->_currentAllocatorIndex;
		const size_t frameBytes = ctx()->_frameAllocStats[first].totalBytes;
		TS_ASSERT(frameBytes > 0);
		TinyGL::presentBuffer();
		// The previous frame's draw calls stay alive in the other allocator.
		TS_ASSERT_DIFFERS(ctx()->_currentAllocatorIndex, first);
		TS_ASSERT_EQUALS(ctx()->_drawCallAllocator[first].getUsed(), frameBytes);
		TS_ASSERT_EQUALS(ctx()->_frameAllocStats[first].totalBytes, frameBytes);
		TS_ASSERT_EQUALS(ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex].totalBytes, 0u);
		TS_ASSERT_EQUALS(ctx()->_lastFrameAllocStats.totalBytes, frameBytes);
	}

	void drawTriangleAt(float x, float y) {
		tglBegin(TGL_TRIANGLES);
		tglVertex2f(x - 0.5f, y - 0.5f);
		tglVertex2f(x + 0.5f, y - 0.5f);
		tglVertex2f(x, y + 0.5f);
		tglEnd();
	}

	void checkOffscreen(bool dirtyRects) {
		createContext(dirtyRects);
		const size_t triBytes = sizeof(TinyGL::RasterizationDrawCall) + 3 * sizeof(TinyGL::GLVertex);
		{
			TinyGL::FrameAllocLabelScope label("visible");
			drawTriangleAt(0.0f, 0.0f);   // fully on screen
			drawTriangleAt(1.0f, 0.0f);   // straddles the right edge
		}
		{
			TinyGL::FrameAllocLabelScope label("hidden");
			drawTriangleAt(3.0f, 0.0f);   // right
			drawTriangleAt(-3.0f, 0.0f);  // left
			drawTriangleAt(0.0f, 3.0f);   // top
			drawTriangleAt(0.0f, -3.0f);  // bottom
		}
		{
			// Each vertex outside a different plane, but no single plane rejects all:
			// conservatively counted as visible.
			TinyGL::FrameAllocLabelScope label("corner");
			tglBegin(TGL_TRIANGLES);
			tglVertex2f(3.0f, 0.0f);
			tglVertex2f(-3.0f, 0.0f);
			tglVertex2f(0.0f, 3.0f);
			tglEnd();
		}
		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.offscreen.count, 4u);
		TS_ASSERT_EQUALS(cur.offscreen.bytes, 4 * triBytes);
		TS_ASSERT_EQUALS(cur.offscreen.largest, triBytes);
		// Off-screen calls are rejected before allocation.
		TS_ASSERT_EQUALS(cur.totalBytes, 3 * triBytes);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocRasterCall].count, 3u);
		TS_ASSERT_EQUALS(ctx()->_drawCallAllocator[ctx()->_currentAllocatorIndex].getUsed(), 3 * triBytes);
		TS_ASSERT_EQUALS(cur.scissored.count, 0u);
		int visible = findLabel(cur, "visible");
		int hidden = findLabel(cur, "hidden");
		int corner = findLabel(cur, "corner");
		TS_ASSERT(visible >= 0 && hidden >= 0 && corner >= 0);
		TS_ASSERT_EQUALS(cur.labelOffscreen[visible].count, 0u);
		TS_ASSERT_EQUALS(cur.labelOffscreen[corner].count, 0u);
		TS_ASSERT_EQUALS(cur.labelOffscreen[hidden].count, 4u);
		TS_ASSERT_EQUALS(cur.labelOffscreen[hidden].bytes, 4 * triBytes);
		TS_ASSERT_EQUALS(cur.labels[hidden].bytes, 0u);
		TS_ASSERT_EQUALS(cur.labels[visible].bytes, 2 * triBytes);

		Common::String report = TinyGL::getFrameAllocReport(TinyGL::kFrameAllocReportCurrent);
		TS_ASSERT(report.contains("Rejected before allocation"));
		TS_ASSERT(report.contains("off-screen (outside one clip plane)"));
		TS_ASSERT(report.contains("rejected: off-screen"));
		TS_ASSERT(!report.contains("outside scissor"));

		TinyGL::presentBuffer();
		TS_ASSERT_EQUALS(ctx()->_lastFrameAllocStats.offscreen.count, 4u);
		TS_ASSERT_EQUALS(ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex].offscreen.count, 0u);
	}

	void testOffscreenNoDirtyRects() {
		checkOffscreen(false);
	}

	void testOffscreenDirtyRects() {
		checkOffscreen(true);
	}

	void testOffscreenNearFar() {
		createContext(false);
		tglMatrixMode(TGL_PROJECTION);
		tglOrtho(-1, 1, -1, 1, -1, 1);
		tglMatrixMode(TGL_MODELVIEW);
		tglBegin(TGL_TRIANGLES);
		tglVertex3f(0.0f, 0.0f, 5.0f); // behind the near plane
		tglVertex3f(0.5f, 0.0f, 5.0f);
		tglVertex3f(0.0f, 0.5f, 5.0f);
		tglEnd();
		TS_ASSERT_EQUALS(ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex].offscreen.count, 1u);
		TS_ASSERT_EQUALS(ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex].totalCount, 0u);
	}

	// Pixel coordinates (top-left origin) to NDC for the 16x16 test buffer, matching
	// TinyGL's viewport transform: x = (ndc + 1) * (16 - 0.5) / 2.
	static float pxToNdcX(float px) { return px / 7.75f - 1.0f; }
	static float pxToNdcY(float py) { return 1.0f - py / 7.75f; }

	void drawPixelTriangle(float x0, float y0, float x1, float y1, float x2, float y2) {
		tglBegin(TGL_TRIANGLES);
		tglVertex2f(pxToNdcX(x0), pxToNdcY(y0));
		tglVertex2f(pxToNdcX(x1), pxToNdcY(y1));
		tglVertex2f(pxToNdcX(x2), pxToNdcY(y2));
		tglEnd();
	}

	static uint32 readPixel(int x, int y) {
		Graphics::Surface surface;
		TinyGL::getSurfaceRef(surface);
		return surface.getPixel(x, y);
	}

	void testScissorRejectsOutside() {
		createContext(false);
		// Scissor box: pixels x in [4, 12), y in [4, 12) (bottom-left origin in GL, symmetric here).
		tglScissor(4, 4, 8, 8);
		tglEnable(TGL_SCISSOR_TEST);
		const size_t triBytes = sizeof(TinyGL::RasterizationDrawCall) + 3 * sizeof(TinyGL::GLVertex);
		{
			TinyGL::FrameAllocLabelScope label("in");
			drawPixelTriangle(5, 5, 10, 5, 7, 10);      // inside
			drawPixelTriangle(1, 1, 6, 1, 3, 6);        // overlaps the top-left corner
		}
		{
			TinyGL::FrameAllocLabelScope label("out");
			drawPixelTriangle(0, 0, 2, 0, 1, 2);        // top-left, outside
			drawPixelTriangle(13.5f, 5, 15, 5, 14, 9);  // right, outside
			drawPixelTriangle(5, 13.5f, 9, 13.5f, 7, 15); // below, outside
		}
		{
			// Within the one pixel of slack next to the box: kept (conservative).
			TinyGL::FrameAllocLabelScope label("edge");
			drawPixelTriangle(12, 5, 12.5f, 5, 12, 8);
		}
		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.scissored.count, 3u);
		TS_ASSERT_EQUALS(cur.scissored.bytes, 3 * triBytes);
		TS_ASSERT_EQUALS(cur.offscreen.count, 0u);
		TS_ASSERT_EQUALS(cur.totalBytes, 3 * triBytes);
		TS_ASSERT_EQUALS(cur.labelScissored[findLabel(cur, "out")].count, 3u);
		TS_ASSERT_EQUALS(cur.labelScissored[findLabel(cur, "in")].count, 0u);
		TS_ASSERT_EQUALS(cur.labelScissored[findLabel(cur, "edge")].count, 0u);
		Common::String report = TinyGL::getFrameAllocReport(TinyGL::kFrameAllocReportCurrent);
		TS_ASSERT(report.contains("outside scissor box"));
		TS_ASSERT(report.contains("rejected: outside scissor"));

		// Without the scissor test nothing is rejected.
		tglDisable(TGL_SCISSOR_TEST);
		drawPixelTriangle(0, 0, 2, 0, 1, 2);
		TS_ASSERT_EQUALS(cur.scissored.count, 3u);
		TS_ASSERT_EQUALS(cur.totalBytes, 4 * triBytes);
	}

	void testScissorNotUsedForClippedVertices() {
		createContext(false);
		tglScissor(4, 4, 8, 8);
		tglEnable(TGL_SCISSOR_TEST);
		// One vertex off-screen: window coordinates aren't known, the call is kept.
		drawPixelTriangle(-20, 0, 2, 0, 1, 2);
		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.scissored.count, 0u);
		TS_ASSERT_EQUALS(cur.offscreen.count, 0u);
		TS_ASSERT_EQUALS(cur.categories[TinyGL::kFrameAllocRasterCall].count, 1u);
	}

	// Rejection must never change the image: every triangle of a sweep around the
	// scissor box (and the screen edges) is drawn with the scissor on, and compared
	// with the same triangle drawn without scissor, masked to the box.
	void checkRejectionKeepsImage(bool dirtyRects) {
		createContext(dirtyRects);
		const Common::Rect box(4, 5, 11, 12); // pixel buffer coordinates
		uint32 black = 0;
		{
			Graphics::Surface s;
			TinyGL::getSurfaceRef(s);
			black = s.format.ARGBToColor(255, 0, 0, 0);
		}
		int rejectedTotal = 0;
		for (float oy = -3.0f; oy <= 16.0f; oy += 0.75f) {
			for (float ox = -3.0f; ox <= 16.0f; ox += 0.75f) {
				for (int pass = 0; pass < 2; pass++) {
					tglDisable(TGL_SCISSOR_TEST);
					tglClearColor(0, 0, 0, 1);
					tglClear(TGL_COLOR_BUFFER_BIT);
					if (pass == 1) {
						tglScissor(box.left, 16 - box.bottom, box.width(), box.height());
						tglEnable(TGL_SCISSOR_TEST);
					}
					tglColor4f(1, 1, 1, 1);
					drawPixelTriangle(ox, oy, ox + 1.6f, oy + 0.3f, ox + 0.4f, oy + 1.4f);
					rejectedTotal += ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex].scissored.count;
					TinyGL::presentBuffer();
					for (int y = 0; y < 16; y++) {
						for (int x = 0; x < 16; x++) {
							uint32 p = readPixel(x, y);
							if (pass == 0) {
								_reference[y][x] = p;
							} else {
								uint32 expected = box.contains(x, y) ? _reference[y][x] : black;
								if (p != expected) {
									TS_FAIL(Common::String::format("pixel (%d,%d) differs for triangle at (%.2f,%.2f)", x, y, ox, oy).c_str());
									return;
								}
							}
						}
					}
				}
			}
		}
		// The sweep must actually exercise the rejection, and draw something.
		TS_ASSERT(rejectedTotal > 100);
	}

	uint32 _reference[16][16];

	void testRejectionKeepsImage() {
		checkRejectionKeepsImage(false);
	}

	void testRejectionKeepsImageDirtyRects() {
		checkRejectionKeepsImage(true);
	}

	void testLabelTextIsCopied() {
		createContext(false);
		char buf[32];
		Common::strlcpy(buf, "first", sizeof(buf));
		TinyGL::setFrameAllocLabel(buf);
		drawTriangle();
		// Reuse the same buffer with other text: must become a separate label.
		Common::strlcpy(buf, "second", sizeof(buf));
		drawTriangle();
		Common::strlcpy(buf, "garbage", sizeof(buf));
		TinyGL::setFrameAllocLabel(nullptr);
		TinyGL::presentBuffer();
		const TinyGL::FrameAllocStats &last = ctx()->_lastFrameAllocStats;
		TS_ASSERT_EQUALS(last.numLabels, 2);
		TS_ASSERT(findLabel(last, "first") >= 0);
		TS_ASSERT(findLabel(last, "second") >= 0);
		TS_ASSERT_EQUALS(findLabel(last, "garbage"), -1);
	}

	void testLongLabelTruncated() {
		createContext(false);
		Common::String longName;
		for (int i = 0; i < 100; i++)
			longName += 'a';
		TinyGL::setFrameAllocLabel(longName.c_str());
		drawTriangle();
		TinyGL::setFrameAllocLabel(nullptr);
		const TinyGL::FrameAllocStats &cur = ctx()->_frameAllocStats[ctx()->_currentAllocatorIndex];
		TS_ASSERT_EQUALS(cur.numLabels, 1);
		TS_ASSERT_EQUALS(strlen(cur.labelNames[0]), (size_t)TinyGL::FrameAllocStats::kMaxLabelLength - 1);
	}

	void testReportText() {
		createContext(false);
		{
			TinyGL::FrameAllocLabelScope label("myLabel");
			drawTriangle();
		}
		Common::String current = TinyGL::getFrameAllocReport(TinyGL::kFrameAllocReportCurrent);
		TS_ASSERT(current.hasPrefix("Current frame (frame 0): "));
		TS_ASSERT(current.contains("2 allocations"));
		TS_ASSERT(current.contains("RasterizationDrawCall vertices"));
		TS_ASSERT(current.contains("myLabel"));
		TS_ASSERT(current.contains("TRIANGLES"));
		TS_ASSERT(current.contains("1-3 verts"));
		TS_ASSERT(!current.contains("ClearBufferDrawCall"));
		TS_ASSERT(current.contains("Allocator usage right now"));

		TinyGL::presentBuffer();
		Common::String last = TinyGL::getFrameAllocReport(TinyGL::kFrameAllocReportLastFrame);
		TS_ASSERT(last.hasPrefix("Last presented frame (frame 0): "));
		TS_ASSERT(last.contains("myLabel"));
		Common::String peak = TinyGL::getFrameAllocReport(TinyGL::kFrameAllocReportPeakFrame);
		TS_ASSERT(peak.hasPrefix("Peak frame (frame 0): "));
		Common::String empty = TinyGL::getFrameAllocReport(TinyGL::kFrameAllocReportCurrent);
		TS_ASSERT(empty.contains("0 B in 0 allocations"));
		TS_ASSERT(!empty.contains("myLabel"));
	}
};

#endif
