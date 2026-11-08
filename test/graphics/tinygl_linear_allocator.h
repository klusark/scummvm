#include <cxxtest/TestSuite.h>

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"
#include "graphics/tinygl/zgl.h"

class TinyGLLinearAllocatorTestSuite : public CxxTest::TestSuite {
	static size_t align(size_t size) {
		return (size + TinyGL::LinearAllocator::kAlignment - 1) & ~(TinyGL::LinearAllocator::kAlignment - 1);
	}

	static bool isAligned(const void *ptr) {
		return ((uintptr)ptr % TinyGL::LinearAllocator::kAlignment) == 0;
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
		TS_ASSERT_EQUALS(a.getUsed(), (size_t)56);
		TS_ASSERT_EQUALS(a.getBlockCount(), 1u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)256);
	}

	void testAlignment() {
		TinyGL::LinearAllocator a;
		a.initialize(256);
		const size_t sizes[] = { 1, 3, 7, 8, 9, 13, 24, 31 };
		byte *prev = nullptr;
		size_t prevSize = 0;
		size_t used = 0;
		for (size_t s : sizes) {
			byte *p = (byte *)a.allocate(s);
			TS_ASSERT(isAligned(p));
			if (prev)
				TS_ASSERT_EQUALS(p, prev + align(prevSize));
			prev = p;
			prevSize = s;
			used += align(s);
		}
		TS_ASSERT_EQUALS(a.getUsed(), used);
	}

	void testExactFitStaysInBlock() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *p0 = (byte *)a.allocate(32);
		byte *p1 = (byte *)a.allocate(32);
		TS_ASSERT_EQUALS(p1, p0 + 32);
		TS_ASSERT_EQUALS(a.getBlockCount(), 1u);
	}

	void testSpillsToNewBlock() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *p0 = (byte *)a.allocate(48);
		byte *p1 = (byte *)a.allocate(24);   // 16 bytes left: needs a new block
		TS_ASSERT_EQUALS(a.getBlockCount(), 2u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)128);
		TS_ASSERT(p1 < p0 || p1 >= p0 + 64);
		byte *p2 = (byte *)a.allocate(40);   // fits right after p1
		TS_ASSERT_EQUALS(p2, p1 + 24);
		TS_ASSERT_EQUALS(a.getBlockCount(), 2u);
		TS_ASSERT_EQUALS(a.getUsed(), (size_t)(48 + 24 + 40));
	}

	void testOversizedRequestGetsItsOwnBlock() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		a.allocate(8);
		byte *big = (byte *)a.allocate(1000);
		TS_ASSERT(isAligned(big));
		TS_ASSERT_EQUALS(a.getBlockCount(), 2u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)(64 + 1000));
		memset(big, 0xAB, 1000);
		// The oversized block is full, so the next request needs a normal sized block.
		a.allocate(8);
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)(64 + 1000 + 64));
	}

	void testResetReusesBlocks() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *first[3];
		first[0] = (byte *)a.allocate(64);
		first[1] = (byte *)a.allocate(64);
		first[2] = (byte *)a.allocate(64);
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);

		a.reset();
		TS_ASSERT_EQUALS(a.getUsed(), (size_t)0);
		for (int i = 0; i < 3; i++)
			TS_ASSERT_EQUALS((byte *)a.allocate(64), first[i]);
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)192);
	}

	void testSkipsExistingBlockThatIsTooSmall() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		byte *blockA = (byte *)a.allocate(64);
		byte *blockB = (byte *)a.allocate(64);
		byte *blockBig = (byte *)a.allocate(256);
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);

		a.reset();
		TS_ASSERT_EQUALS((byte *)a.allocate(32), blockA);
		// Doesn't fit in what is left of A nor in B: lands in the big block.
		TS_ASSERT_EQUALS((byte *)a.allocate(100), blockBig);
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);
		// B was skipped and is not revisited; the big block continues.
		TS_ASSERT_EQUALS((byte *)a.allocate(8), blockBig + 104);
		(void)blockB;
	}

	void testNoExistingBlockFitsAddsOne() {
		TinyGL::LinearAllocator a;
		a.initialize(64);
		a.allocate(64);
		a.allocate(64);
		a.reset();
		a.allocate(8);
		byte *p = (byte *)a.allocate(100);
		TS_ASSERT(isAligned(p));
		TS_ASSERT_EQUALS(a.getBlockCount(), 3u);
		TS_ASSERT_EQUALS(a.getCapacity(), (size_t)(64 + 64 + 104));
	}

	void testContentsSurviveGrowth() {
		TinyGL::LinearAllocator a;
		a.initialize(100);
		const int kCount = 500;
		uint32 *ptrs[kCount];
		for (int i = 0; i < kCount; i++) {
			size_t n = 1 + (i * 7) % 13;
			ptrs[i] = (uint32 *)a.allocate(n * sizeof(uint32));
			for (size_t j = 0; j < n; j++)
				ptrs[i][j] = (uint32)(i * 1000 + j);
		}
		TS_ASSERT(a.getBlockCount() > 1u);
		for (int i = 0; i < kCount; i++) {
			size_t n = 1 + (i * 7) % 13;
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
