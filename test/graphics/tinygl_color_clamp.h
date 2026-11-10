#include <cxxtest/TestSuite.h>

#include "common/scummsys.h"

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"

// The colours of a smooth shaded triangle are stepped in fixed point from pixel to pixel, which can end a little
// below 0 or above the maximum at its edge. They must be clamped there, and not wrap around to the other extreme.

class TinyGLColorClampTestSuite : public CxxTest::TestSuite {
	TinyGL::ContextHandle *_context = nullptr;
	int _w = 0, _h = 0;
	uint32 _rng = 1;

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

	float rnd() {
		_rng = _rng * 1664525u + 1013904223u;
		return (_rng >> 8) / 16777216.0f;
	}

	// Draws random triangles whose channels are each 0 or 1 at the vertices, so that many of their edges are at the
	// limits, and checks every drawn pixel against the exact interpolation at its centre: a channel that should be
	// near one limit must not come out near the other. With alphaAsColor the alpha is checked instead, as white
	// drawn with glBlendFunc(GL_SRC_ALPHA, GL_ZERO). Returns the number of wrapped pixels.
	int countWraps(bool textured, bool alphaAsColor, int count) {
		createContext(32, 32);
		int wraps = 0;
		for (int i = 0; i < count; i++) {
			begin(textured, alphaAsColor);
			float v[3][2], c[3][3];
			for (int k = 0; k < 3; k++) {
				v[k][0] = 1 + rnd() * (_w - 2);
				v[k][1] = 1 + rnd() * (_h - 2);
				for (int j = 0; j < 3; j++)
					c[k][j] = rnd() < 0.5f ? 0.0f : 1.0f;
				if (alphaAsColor)
					c[k][1] = c[k][2] = c[k][0];
			}
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

			const float d = (v[1][0] - v[0][0]) * (v[2][1] - v[0][1]) - (v[2][0] - v[0][0]) * (v[1][1] - v[0][1]);
			if (fabsf(d) < 1.0f)
				continue; // too thin to say where the edges are
			for (int y = 0; y < _h; y++) {
				for (int x = 0; x < _w; x++) {
					byte got[3];
					getPixel(s, x, y, got);
					if (!got[0] && !got[1] && !got[2])
						continue; // not drawn, or drawn black
					const float px = x + 0.5f, py = y + 0.5f;
					const float w1 = ((px - v[0][0]) * (v[2][1] - v[0][1]) - (v[2][0] - v[0][0]) * (py - v[0][1])) / d;
					const float w2 = ((v[1][0] - v[0][0]) * (py - v[0][1]) - (px - v[0][0]) * (v[1][1] - v[0][1])) / d;
					const float w0 = 1 - w1 - w2;
					for (int j = 0; j < 3; j++) {
						const float e = CLIP((w0 * c[0][j] + w1 * c[1][j] + w2 * c[2][j]) * 255.0f, 0.0f, 255.0f);
						if ((got[j] >= 200 && e <= 30.0f) || (got[j] <= 55 && e >= 225.0f)) {
							if (wraps < 5)
								TS_TRACE(Common::String::format("triangle %d, pixel (%d, %d), channel %d: %d, should be %.1f", i, x, y, j, got[j], e).c_str());
							wraps++;
							break;
						}
					}
				}
			}
		}
		return wraps;
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

	void testRandomUntexturedTrianglesDoNotWrap() {
		TS_ASSERT_EQUALS(countWraps(false, false, 3000), 0);
	}

	void testRandomTexturedTrianglesDoNotWrap() {
		TS_ASSERT_EQUALS(countWraps(true, false, 3000), 0);
	}

	void testRandomUntexturedAlphaDoesNotWrap() {
		TS_ASSERT_EQUALS(countWraps(false, true, 3000), 0);
	}

	void testRandomTexturedAlphaDoesNotWrap() {
		TS_ASSERT_EQUALS(countWraps(true, true, 3000), 0);
	}
};

#endif
