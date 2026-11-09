#include <cxxtest/TestSuite.h>

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"
#include "graphics/tinygl/zgl.h"

class TinyGLLinearAllocatorTestSuite : public CxxTest::TestSuite {
	static bool isAligned(const void *ptr) {
		return ((uintptr)ptr % TinyGL::LinearAllocator::kAlignment) == 0;
	}

	static bool inRange(const byte *p, const byte *start, size_t size) {
		return p >= start && p < start + size;
	}

public:
	void testSingleBlockIsContiguous() {
		TinyGL::LinearAllocator a;
		a.initialize(256);
		byte *p0 = (byte *)a.allocate(16);
		byte *p1 = (byte *)a.allocate(32);
		byte *p2 = (byte *)a.allocate(8);
		TS_ASSERT_EQUALS(p1, p0 + 16);
		TS_ASSERT_EQUALS(p2, p1 + 32);
		TS_ASSERT_EQUALS(a.getBlockCount(), 1u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)256);
	}

	void testAlignment() {
		TinyGL::LinearAllocator a;
		a.initialize(256);
		const size_t sizes[] = { 1, 3, 7, 8, 9, 13, 24, 31 };
		const size_t aligned[] = { 8, 8, 8, 8, 16, 16, 24, 32 };
		byte *prev = nullptr;
		for (uint i = 0; i < ARRAYSIZE(sizes); i++) {
			byte *p = (byte *)a.allocate(sizes[i]);
			TS_ASSERT(isAligned(p));
			if (prev)
				TS_ASSERT_EQUALS(p, prev + aligned[i - 1]);
			prev = p;
		}
		TS_ASSERT_EQUALS(a.getBlockCount(), 1u);
	}

	void testExactFitStaysInBlock() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *p0 = (byte *)a.allocate(32);
		byte *p1 = (byte *)a.allocate(32);
		TS_ASSERT_EQUALS(p1, p0 + 32);
		TS_ASSERT_EQUALS(a.getBlockCount(), 1u);
	}

	// A request that doesn't fit goes to the next block; the space left in
	// the current block is still used by the smaller requests after it.
	void testLargeRequestSkipsToNextBlock() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *p0 = (byte *)a.allocate(40);  // 24 bytes left in block 0
		byte *p1 = (byte *)a.allocate(32);  // doesn't fit: block 1
		TS_ASSERT_EQUALS(a.getBlockCount(), 2u);
		TS_ASSERT(!inRange(p1, p0, 64));
		byte *p2 = (byte *)a.allocate(16);  // fits in what is left of block 0
		TS_ASSERT_EQUALS(p2, p0 + 40);
		byte *p3 = (byte *)a.allocate(8);   // still block 0, now full
		TS_ASSERT_EQUALS(p3, p0 + 56);
		byte *p4 = (byte *)a.allocate(16);  // block 0 is full: block 1, after p1
		TS_ASSERT_EQUALS(p4, p1 + 32);
		TS_ASSERT_EQUALS(a.getBlockCount(), 2u);
	}

	// When a request fits in neither the current nor the next block, the next
	// block becomes the current one.
	void testAdvancesWhenNeitherFits() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *b0 = (byte *)a.allocate(40);  // block 0: 24 left
		byte *b1 = (byte *)a.allocate(48);  // block 1: 16 left
		byte *b2 = (byte *)a.allocate(32);  // fits in neither: block 1 becomes current, block 2 added
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);
		TS_ASSERT(!inRange(b2, b0, 64));
		TS_ASSERT(!inRange(b2, b1, 64));
		// Block 0 is no longer used this frame; block 1 is current.
		TS_ASSERT_EQUALS((byte *)a.allocate(16), b1 + 48);
		TS_ASSERT_EQUALS((byte *)a.allocate(8), b2 + 32);
	}

	void testOversizedRequestUsesLargeBlock() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *p0 = (byte *)a.allocate(8);
		byte *big = (byte *)a.allocate(1000);
		TS_ASSERT(isAligned(big));
		memset(big, 0xAB, 1000);
		TS_ASSERT_EQUALS(a.getBlockCount(), 1u);
		TS_ASSERT_EQUALS(a.getLargeBlockCount(), 1u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)(64 + 1000));
		// Regular allocations continue where they were.
		TS_ASSERT_EQUALS((byte *)a.allocate(8), p0 + 8);
	}

	void testLargeBlocksAreReused() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *big1 = (byte *)a.allocate(1000);
		byte *big2 = (byte *)a.allocate(500);
		TS_ASSERT_DIFFERS(big1, big2);
		TS_ASSERT_EQUALS(a.getLargeBlockCount(), 2u);

		a.reset();
		// First free large block that is big enough.
		TS_ASSERT_EQUALS((byte *)a.allocate(400), big1);
		TS_ASSERT_EQUALS((byte *)a.allocate(400), big2);
		TS_ASSERT_EQUALS(a.getLargeBlockCount(), 2u);
		// None free: a new one.
		a.allocate(100);
		TS_ASSERT_EQUALS(a.getLargeBlockCount(), 3u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)(64 + 1000 + 504 + 104));
	}

	void testResetReusesBlocks() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *first[3];
		for (int i = 0; i < 3; i++)
			first[i] = (byte *)a.allocate(64);
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);

		a.reset();
		for (int i = 0; i < 3; i++)
			TS_ASSERT_EQUALS((byte *)a.allocate(64), first[i]);
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)192);
	}

	// The same sequence of requests gets the same addresses every frame and
	// needs no new blocks after the first.
	void testRepeatedFramesAreStable() {
		TinyGL::LinearAllocator a;
		a.initialize(1024);
		const int kCount = 300;
		byte *ptrs[kCount];
		for (int frame = 0; frame < 3; frame++) {
			a.reset();
			for (int i = 0; i < kCount; i++) {
				byte *p = (byte *)a.allocate(16 + (i * 37) % 200);
				if (frame == 0)
					ptrs[i] = p;
				else
					TS_ASSERT_EQUALS(p, ptrs[i]);
			}
		}
		TS_ASSERT_EQUALS(a.getLargeBlockCount(), 0u);
	}

	void testContentsSurviveGrowth() {
		TinyGL::LinearAllocator a;
		a.initialize(100);
		const int kCount = 500;
		uint32 *ptrs[kCount];
		for (int i = 0; i < kCount; i++) {
			size_t n = 1 + (i * 7) % 40;  // up to 160 bytes: some go to large blocks
			ptrs[i] = (uint32 *)a.allocate(n * sizeof(uint32));
			TS_ASSERT(isAligned(ptrs[i]));
			for (size_t j = 0; j < n; j++)
				ptrs[i][j] = (uint32)(i * 1000 + j);
		}
		TS_ASSERT(a.getBlockCount() > 1u);
		TS_ASSERT(a.getLargeBlockCount() > 0u);
		for (int i = 0; i < kCount; i++) {
			size_t n = 1 + (i * 7) % 40;
			for (size_t j = 0; j < n; j++)
				TS_ASSERT_EQUALS(ptrs[i][j], (uint32)(i * 1000 + j));
		}
	}

	// A context whose draw call blocks are far too small for one frame keeps
	// drawing correctly instead of running out of memory.
	void testContextWithTinyBlocks() {
		TinyGL::ContextHandle *context = TinyGL::createContext(2, 2, Graphics::PixelFormat::createFormatARGB32(), 2, false, false, 512);
		TinyGL::setContext(context);
		tglDisable(TGL_TEXTURE_2D);
		tglDisable(TGL_BLEND);
		tglDisable(TGL_DEPTH_TEST);
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglViewport(0, 0, 2, 2);

		for (int frame = 0; frame < 3; frame++) {
			const float S = 10.0f;
			for (int i = 0; i < 200; i++) {
				tglColor4ub(i, 255 - i, frame * 50, 255);
				tglBegin(TGL_TRIANGLES);
				tglVertex2f(-S, -S);
				tglVertex2f(+S, -S);
				tglVertex2f(0, +S);
				tglEnd();
			}
			TinyGL::GLContext *c = TinyGL::gl_get_context();
			TS_ASSERT(c->_drawCallAllocator[c->_currentAllocatorIndex].getBlockCount() > 1u);

			byte a, r, g, b;
			Graphics::Surface surface;
			TinyGL::presentBuffer();
			TinyGL::getSurfaceRef(surface);
			surface.format.colorToARGB(surface.getPixel(0, 0), a, r, g, b);
			TS_ASSERT_EQUALS(r, 199);
			TS_ASSERT_EQUALS(g, 56);
			TS_ASSERT_EQUALS(b, frame * 50);
			TS_ASSERT_EQUALS(a, 255);
		}
		TinyGL::destroyContext(context);
	}
};

#endif
