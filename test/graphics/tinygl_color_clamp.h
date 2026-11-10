#include <cxxtest/TestSuite.h>

#include "common/scummsys.h"
#include "common/random.h"

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"
#include "graphics/tinygl/zbuffer.h"
#include "math/vector3d.h"

#include "../system/null_osystem.h"

// The colours of a smooth shaded triangle are stepped in fixed point from pixel to pixel, which can end a little
// below 0 or above the maximum at its edge. They must be clamped there, and not wrap around to the other extreme.

class TinyGLColorClampTestSuite : public CxxTest::TestSuite {
	TinyGL::ContextHandle *_context = nullptr;
	int _w = 0, _h = 0;

	void createContext(int w, int h) {
		_w = w;
		_h = h;
		_context = TinyGL::createContext(w, h, Graphics::PixelFormat::createFormatARGB32(), 256, false, false);
		TinyGL::setContext(_context);
	}

	// Pixel coordinates, (0, 0) at the top left. With alphaAsColor the source alpha is drawn as the colour.
	void begin(bool textured, bool alphaAsColor = false) {
		tglViewport(0, 0, _w, _h);
		tglClearColor(0, 0, 0, 0);
		tglClear(TGL_COLOR_BUFFER_BIT);
		tglDisable(TGL_DEPTH_TEST);
		if (alphaAsColor) {
			tglEnable(TGL_BLEND);
			tglBlendFunc(TGL_SRC_ALPHA, TGL_ZERO);
		} else {
			tglDisable(TGL_BLEND);
		}
		tglShadeModel(TGL_SMOOTH);
		if (textured) {
			// white, so that the default TGL_MODULATE gives the interpolated colour
			static byte white[4 * 4 * 4];
			memset(white, 255, sizeof(white));
			TGLuint texture;
			tglGenTextures(1, &texture);
			tglBindTexture(TGL_TEXTURE_2D, texture);
			tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MIN_FILTER, TGL_NEAREST);
			tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MAG_FILTER, TGL_NEAREST);
			tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGBA, 4, 4, 0, TGL_RGBA, TGL_UNSIGNED_BYTE, white);
			tglEnable(TGL_TEXTURE_2D);
		} else {
			tglDisable(TGL_TEXTURE_2D);
		}
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglOrthof(0, _w, _h, 0, -1, 1);
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
	}

	void getPixel(const Graphics::Surface &s, int x, int y, byte out[3]) {
		byte a, r, g, b;
		s.format.colorToARGB(s.getPixel(x, y), a, r, g, b);
		out[0] = r;
		out[1] = g;
		out[2] = b;
	}

	// in [0, 1)
	static float rnd(Common::RandomSource &rng) {
		return rng.getRandomNumber(0xFFFFFF) / 16777216.0f;
	}

	// Draws a smooth shaded triangle and checks every drawn pixel against the exact interpolation at its centre: a
	// channel that should be near one limit must not come out near the other. With alphaAsColor the alpha (c[k][0]) is
	// checked instead, as white drawn with glBlendFunc(GL_SRC_ALPHA, GL_ZERO). Returns the number of wrapped pixels,
	// and adds the number of channels checked to checked.
	//
	// This does not rely on where exactly the rasterizer samples the colour or which pixels on an edge it draws: a
	// pixel may be up to a pixel away from where it should be in x and in y, so a channel may be off by up to its
	// gradient over that, and is only counted as wrapped if it is further off than that. Channels too steep for that
	// to tell a wrap from a shifted sample are skipped.
	int drawAndCountWraps(bool textured, bool alphaAsColor, const float v[3][2], const float c[3][3], int &checked) {
		begin(textured, alphaAsColor);
		tglBegin(TGL_TRIANGLES);
		for (int k = 0; k < 3; k++) {
			if (alphaAsColor)
				tglColor4f(1, 1, 1, c[k][0]);
			else
				tglColor3f(c[k][0], c[k][1], c[k][2]);
			tglTexCoord2f(0.5f, 0.5f);
			tglVertex2f(v[k][0], v[k][1]);
		}
		tglEnd();
		TinyGL::presentBuffer();
		Graphics::Surface s;
		TinyGL::getSurfaceRef(s);

		// Each channel is a plane over the triangle, with (x, y, c) at each vertex. The cross product of two edges is its
		// normal n, with n.z the 2D cross product of the edges (twice the signed area), and the gradient -(n.x, n.y) / n.z.
		const int channels = alphaAsColor ? 1 : 3;
		float dcdx[3], dcdy[3], tolerance[3];
		for (int j = 0; j < channels; j++) {
			const Math::Vector3d e1(v[1][0] - v[0][0], v[1][1] - v[0][1], c[1][j] - c[0][j]);
			const Math::Vector3d e2(v[2][0] - v[0][0], v[2][1] - v[0][1], c[2][j] - c[0][j]);
			const Math::Vector3d n = Math::Vector3d::crossProduct(e1, e2);
			if (fabsf(n.z()) < 1.0f)
				return 0; // too thin to say where the edges are
			dcdx[j] = -n.x() / n.z();
			dcdy[j] = -n.y() / n.z();
			tolerance[j] = (fabsf(dcdx[j]) + fabsf(dcdy[j])) * 255.0f + 4.0f;
		}
		int wraps = 0;
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				byte got[3];
				getPixel(s, x, y, got);
				if (!got[0] && !got[1] && !got[2])
					continue; // not drawn, or drawn black
				// from the first vertex to the pixel centre
				const float dx = x + 0.5f - v[0][0], dy = y + 0.5f - v[0][1];
				for (int j = 0; j < channels; j++) {
					if (tolerance[j] > 150.0f)
						continue;
					checked++;
					const float e = CLIP((c[0][j] + dcdx[j] * dx + dcdy[j] * dy) * 255.0f, 0.0f, 255.0f);
					const bool farOff = fabsf(got[j] - e) > tolerance[j];
					if (farOff && ((got[j] >= 200 && e <= 30.0f) || (got[j] <= 55 && e >= 225.0f))) {
						if (wraps < 5)
							TS_TRACE(Common::String::format("pixel (%d, %d), channel %d: %d, should be %.1f", x, y, j, got[j], e).c_str());
						wraps++;
						break;
					}
				}
			}
		}
		return wraps;
	}

	// Random triangles whose channels are each 0 or 1 at the vertices, so that many of their edges are at the limits.
	// The checked count makes sure that the tolerance did not skip most of them.
	void checkRandomTriangles(bool textured, bool alphaAsColor) {
		createContext(32, 32);
		// RandomSource needs a g_system, which the test runner does not have
		Common::install_null_g_system();
		Common::RandomSource rng("tinygl_color_clamp");
		rng.setSeed(1); // the same triangles every run
		Common::uninstall_null_g_system();
		int wraps = 0, checked = 0;
		for (int i = 0; i < 3000; i++) {
			float v[3][2], c[3][3];
			for (int k = 0; k < 3; k++) {
				v[k][0] = 1 + rnd(rng) * (_w - 2);
				v[k][1] = 1 + rnd(rng) * (_h - 2);
				for (int j = 0; j < 3; j++)
					c[k][j] = rng.getRandomBit();
			}
			wraps += drawAndCountWraps(textured, alphaAsColor, v, c, checked);
		}
		TS_ASSERT_EQUALS(wraps, 0);
		TS_ASSERT_LESS_THAN(100000, checked);
	}

	// Triangles from the random ones that wrapped before the clamp, with the channel that wrapped as red (or alpha)
	void checkKnownTriangles(bool textured, bool alphaAsColor) {
		static const float triangles[][3][3] = {
			{ { 6.14139223f, 6.12538671f, 0 }, { 19.7666359f, 16.3052292f, 0 }, { 25.0644283f, 2.10356808f, 1 } },
			{ { 3.45627403f, 20.4762764f, 0 }, { 3.98850656f, 3.08420515f, 1 }, { 25.9176044f, 23.1767406f, 0 } },
			{ { 27.300045f, 10.7614746f, 1 }, { 17.4126263f, 6.79631805f, 0 }, { 8.91143608f, 23.2624035f, 0 } },
			{ { 18.134573f, 25.2837238f, 0 }, { 13.4490871f, 14.6478195f, 0 }, { 5.02274847f, 5.93278265f, 1 } },
			{ { 13.5307283f, 9.57697105f, 0 }, { 1.81269145f, 25.9223995f, 1 }, { 17.2807808f, 24.2938519f, 0 } },
		};
		createContext(32, 32);
		for (int i = 0; i < ARRAYSIZE(triangles); i++) {
			float v[3][2], c[3][3];
			for (int k = 0; k < 3; k++) {
				v[k][0] = triangles[i][k][0];
				v[k][1] = triangles[i][k][1];
				c[k][0] = triangles[i][k][2];
				c[k][1] = c[k][2] = 0;
			}
			int checked = 0;
			TSM_ASSERT_EQUALS(Common::String::format("triangle %d", i).c_str(), drawAndCountWraps(textured, alphaAsColor, v, c, checked), 0);
			TS_ASSERT_LESS_THAN(20, checked); // black pixels (0 or not drawn) are not checked
		}
	}

public:
	void tearDown() {
		if (_context != nullptr) {
			TinyGL::destroyContext(_context);
			_context = nullptr;
		}
	}

	// A red, green and blue triangle: on the edge from green to blue the red is 0, and a pixel whose centre is just
	// inside that edge was drawn with red 255 (as yellow) where it stepped below 0
	void testGouraudEdgeRedDoesNotWrap() {
		createContext(96, 72);
		begin(false);
		tglBegin(TGL_TRIANGLES);
		tglColor3f(1, 0, 0); tglVertex2f(5.5f, 66.2f);
		tglColor3f(0, 1, 0); tglVertex2f(48.3f, 3.7f);
		tglColor3f(0, 0, 1); tglVertex2f(91.1f, 60.4f);
		tglEnd();
		TinyGL::presentBuffer();
		Graphics::Surface s;
		TinyGL::getSurfaceRef(s);
		byte got[3];
		getPixel(s, 62, 22, got);
		TS_ASSERT_LESS_THAN_EQUALS(got[0], 1);
		// and anywhere along that edge (red at most a few steps above 0 from the interpolation, never close to 255)
		for (int y = 4; y < 60; y++) {
			for (int x = _w - 1; x >= 0; x--) {
				getPixel(s, x, y, got);
				if (got[0] || got[1] || got[2]) {
					TSM_ASSERT_LESS_THAN_EQUALS(Common::String::format("row %d", y).c_str(), got[0], 8);
					break;
				}
			}
		}
	}

	// Constant colours: nothing to step, so the colour must be exact everywhere, at 0 and at the maximum
	void testFlatLimitsExact() {
		createContext(32, 32);
		begin(false);
		tglBegin(TGL_TRIANGLES);
		tglColor3f(1, 0, 1); tglVertex2f(1.3f, 2.1f);
		tglColor3f(1, 0, 1); tglVertex2f(30.7f, 5.9f);
		tglColor3f(1, 0, 1); tglVertex2f(12.2f, 29.4f);
		tglEnd();
		TinyGL::presentBuffer();
		Graphics::Surface s;
		TinyGL::getSurfaceRef(s);
		int drawn = 0;
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				byte got[3];
				getPixel(s, x, y, got);
				if (!got[0] && !got[2])
					continue;
				drawn++;
				TS_ASSERT_EQUALS(got[0], 255);
				TS_ASSERT_EQUALS(got[1], 0);
				TS_ASSERT_EQUALS(got[2], 255);
			}
		}
		TS_ASSERT_LESS_THAN(200, drawn);
	}

	void testRandomUntexturedTrianglesDoNotWrap() { checkRandomTriangles(false, false); }
	void testRandomTexturedTrianglesDoNotWrap() { checkRandomTriangles(true, false); }
	void testRandomUntexturedAlphaDoesNotWrap() { checkRandomTriangles(false, true); }
	void testRandomTexturedAlphaDoesNotWrap() { checkRandomTriangles(true, true); }

	void testKnownUntexturedTrianglesDoNotWrap() { checkKnownTriangles(false, false); }
	void testKnownTexturedTrianglesDoNotWrap() { checkKnownTriangles(true, false); }
	void testKnownUntexturedAlphaDoesNotWrap() { checkKnownTriangles(false, true); }
	void testKnownTexturedAlphaDoesNotWrap() { checkKnownTriangles(true, true); }

	// The conversions themselves, from ZB_POINT_*_BITS fixed point (16 bits, so 256 per level) to a byte
	void testColorToByteLimits() {
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(INT_MIN), 0);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(-0x10000), 0);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(-129), 0);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(-128), 0);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(-1), 0);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0), 0);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0xFF), 0);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0x100), 1);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0x8000), 128);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0xFEFF), 254);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0xFF00), 255);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0xFFFF), 255);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0x10000), 255);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0x10100), 255);
		TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(0x7FFFFFFF), 255);
	}

	// Every value around the limits against the clamped exact conversion
	void testConversionMatchesExact() {
		for (int v = -0x20000; v <= 0x20000; v++) {
			const int expected = CLIP(v < 0 ? 0 : v / 256, 0, 255);
			if (TinyGL::colorToByte<16>(v) != expected) {
				TS_FAIL(Common::String::format("%d: %d, should be %d", v, TinyGL::colorToByte<16>(v), expected).c_str());
				break;
			}
		}
	}

	// A byte colour n is scaled to n * 257 by tglColor (c * ZB_POINT_RED_MAX), which must convert back to exactly n
	void testConversionKeepsEveryByte() {
		for (int n = 0; n < 256; n++) {
			TS_ASSERT_EQUALS(TinyGL::colorToByte<16>(n * 257), n);
			TS_ASSERT_EQUALS(TinyGL::colorToByte<16>((int)((n / 255.0f) * 65535)), n);
		}
	}
};

#endif
