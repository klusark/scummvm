#include <cxxtest/TestSuite.h>

#ifdef USE_TINYGL

#include "common/array.h"

#include "graphics/tinygl/tinygl.h"

// Which pixels TinyGL draws.
//
// Triangles follow OpenGL: a pixel is drawn when its centre is inside the triangle, or on a left or a top
// edge of it. The window coordinates are rounded to 1/16 of a pixel, so the tests give their vertices in
// those units (16 = one pixel, y down) and compare the buffer with that rule worked out in integers.

class TinyGLRasterTestSuite : public CxxTest::TestSuite {
	enum {
		kSub = 16 // sub-pixel units per pixel
	};

	struct Pt {
		int x, y;
	};

	TinyGL::ContextHandle *_context = nullptr;
	int _w = 0, _h = 0;
	uint32 _seed = 1;

	// the buffer sizes must be powers of two so that the sub-pixel coordinates are exact in float
	void makeContext(int w, int h, int textureSize = 2) {
		destroy();
		_w = w;
		_h = h;
		_context = TinyGL::createContext(w, h, Graphics::PixelFormat::createFormatARGB32(), textureSize, false, false);
		TinyGL::setContext(_context);

		tglDisable(TGL_TEXTURE_2D);
		tglDisable(TGL_BLEND);
		tglDisable(TGL_DEPTH_TEST);
		tglDisable(TGL_LIGHTING);
		tglDisable(TGL_CULL_FACE);
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglViewport(0, 0, w, h);
		tglClearColor(0, 0, 0, 1);
		tglColor4f(1, 1, 1, 1);
	}

	void destroy() {
		if (_context != nullptr) {
			TinyGL::destroyContext(_context);
			_context = nullptr;
		}
	}

	void clear() {
		tglClear(TGL_COLOR_BUFFER_BIT);
	}

	// a vertex at a window position in sub-pixel units
	void vertex(int sx, int sy) {
		tglVertex2f(sx / (float)(kSub / 2 * _w) - 1.0f, 1.0f - sy / (float)(kSub / 2 * _h));
	}

	void vertex(const Pt &p) {
		vertex(p.x, p.y);
	}

	void triangle(const Pt &a, const Pt &b, const Pt &c) {
		tglBegin(TGL_TRIANGLES);
		vertex(a);
		vertex(b);
		vertex(c);
		tglEnd();
	}

	// the red channel of every pixel
	void readRed(Common::Array<byte> &out) {
		Graphics::Surface surface;
		TinyGL::presentBuffer();
		TinyGL::getSurfaceRef(surface);
		out.resize(_w * _h);
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				byte a, r, g, b;
				surface.format.colorToARGB(surface.getPixel(x, y), a, r, g, b);
				out[y * _w + x] = r;
			}
		}
	}

	void readPixel(int x, int y, byte &r, byte &g, byte &b) {
		Graphics::Surface surface;
		byte a;
		TinyGL::presentBuffer();
		TinyGL::getSurfaceRef(surface);
		surface.format.colorToARGB(surface.getPixel(x, y), a, r, g, b);
	}

	static int64 cross(const Pt &a, const Pt &b, int px, int py) {
		return (int64)(b.x - a.x) * (py - a.y) - (int64)(b.y - a.y) * (px - a.x);
	}

	static int64 area2(const Pt &a, const Pt &b, const Pt &c) {
		return cross(a, b, c.x, c.y);
	}

	// OpenGL's rule, exactly: the centre is inside, or on a left or a top edge
	static bool covers(Pt a, Pt b, Pt c, int x, int y) {
		const int64 area = area2(a, b, c);
		if (area == 0)
			return false;
		if (area < 0) {
			const Pt t = b;
			b = c;
			c = t;
		}
		const int px = x * kSub + kSub / 2;
		const int py = y * kSub + kSub / 2;
		const Pt *e[4] = { &a, &b, &c, &a };
		for (int i = 0; i < 3; i++) {
			const int64 v = cross(*e[i], *e[i + 1], px, py);
			if (v < 0)
				return false;
			if (v == 0) {
				// (with this winding and y down, a top edge runs right and a left edge runs up)
				const int dx = e[i + 1]->x - e[i]->x;
				const int dy = e[i + 1]->y - e[i]->y;
				if (!((dy == 0 && dx > 0) || dy < 0))
					return false;
			}
		}
		return true;
	}

	int countCovered(const Pt &a, const Pt &b, const Pt &c) const {
		int n = 0;
		for (int y = 0; y < _h; y++)
			for (int x = 0; x < _w; x++)
				n += covers(a, b, c, x, y) ? 1 : 0;
		return n;
	}

	// draws one white triangle on black and compares every pixel with the rule; returns the wrong pixels
	int wrongPixels(const Pt &a, const Pt &b, const Pt &c) {
		Common::Array<byte> red;
		clear();
		triangle(a, b, c);
		readRed(red);
		int wrong = 0;
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				const bool expected = covers(a, b, c, x, y);
				const bool actual = red[y * _w + x] != 0;
				if (expected != actual)
					wrong++;
			}
		}
		return wrong;
	}

	void checkTriangle(const Pt &a, const Pt &b, const Pt &c) {
		const int wrong = wrongPixels(a, b, c);
		if (wrong != 0) {
			Common::String msg = Common::String::format("%d wrong pixels for (%d, %d) (%d, %d) (%d, %d)",
			                                            wrong, a.x, a.y, b.x, b.y, c.x, c.y);
			TS_FAIL(msg.c_str());
		}
	}

	uint32 rnd() {
		_seed = _seed * 1664525u + 1013904223u;
		return _seed >> 8;
	}

	int rnd(int lo, int hi) {
		return lo + (int)(rnd() % (uint32)(hi - lo + 1));
	}

	Pt rndPt() {
		Pt p;
		p.x = rnd(0, _w * kSub);
		p.y = rnd(0, _h * kSub);
		return p;
	}

	// additive drawing: every draw of a pixel adds 64 to it
	void beginCounting() {
		clear();
		tglEnable(TGL_BLEND);
		tglBlendFunc(TGL_ONE, TGL_ONE);
		tglColor4f(64 / 255.0f, 64 / 255.0f, 64 / 255.0f, 1.0f);
	}

	// how often a pixel was drawn, from its red channel
	static int drawCount(byte red) {
		return (red + 32) / 64;
	}

public:
	void setUp() {
		_seed = 1;
	}

	void tearDown() {
		destroy();
	}

	// A rectangle on whole pixels covers exactly those pixels: [x0, x1) x [y0, y1)
	void testIntegerQuad() {
		makeContext(64, 64);
		const int x0 = 10, y0 = 20, x1 = 30, y1 = 25;
		Common::Array<byte> red;
		clear();
		tglBegin(TGL_QUADS);
		vertex(x0 * kSub, y0 * kSub);
		vertex(x1 * kSub, y0 * kSub);
		vertex(x1 * kSub, y1 * kSub);
		vertex(x0 * kSub, y1 * kSub);
		tglEnd();
		readRed(red);
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				const bool inside = x >= x0 && x < x1 && y >= y0 && y < y1;
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, inside);
			}
		}
	}

	// The whole buffer, from corner to corner
	void testFullScreenQuad() {
		makeContext(64, 32);
		Common::Array<byte> red;
		beginCounting();
		tglBegin(TGL_QUADS);
		vertex(0, 0);
		vertex(_w * kSub, 0);
		vertex(_w * kSub, _h * kSub);
		vertex(0, _h * kSub);
		tglEnd();
		readRed(red);
		for (int i = 0; i < _w * _h; i++)
			TS_ASSERT_EQUALS(drawCount(red[i]), 1);
	}

	// A quad larger than the buffer is clipped: still every pixel once
	void testClippedQuad() {
		makeContext(64, 32);
		Common::Array<byte> red;
		beginCounting();
		tglBegin(TGL_QUADS);
		vertex(-37 * kSub, -11 * kSub);
		vertex((_w + 23) * kSub, -5 * kSub);
		vertex((_w + 9) * kSub, (_h + 17) * kSub);
		vertex(-3 * kSub, (_h + 40) * kSub);
		tglEnd();
		readRed(red);
		for (int i = 0; i < _w * _h; i++)
			TS_ASSERT_EQUALS(drawCount(red[i]), 1);
	}

	// A vertex within the pixel moves the edge: the centre decides
	void testPixelCentreDecides() {
		makeContext(16, 16);
		// the right edge at 4 + 8/16: centre of pixel 4 is on it, so out; one sub-pixel more and it is in
		for (int sub = 0; sub <= kSub; sub++) {
			Common::Array<byte> red;
			clear();
			tglBegin(TGL_QUADS);
			vertex(2 * kSub + sub, 2 * kSub + sub);
			vertex(4 * kSub + sub, 2 * kSub + sub);
			vertex(4 * kSub + sub, 4 * kSub + sub);
			vertex(2 * kSub + sub, 4 * kSub + sub);
			tglEnd();
			readRed(red);
			// centres at n + 8/16: the quad [2 + s, 4 + s) holds those of pixels 2 and 3 up to s = 8/16,
			// and of pixels 3 and 4 after
			const int first = sub <= kSub / 2 ? 2 : 3;
			for (int y = 0; y < _h; y++) {
				for (int x = 0; x < _w; x++) {
					const bool inside = x >= first && x < first + 2 && y >= first && y < first + 2;
					TS_ASSERT_EQUALS(red[y * _w + x] != 0, inside);
				}
			}
		}
	}

	// The cases of the rule one by one, on a triangle whose edges go through pixel centres
	void testTopLeftRule() {
		makeContext(16, 16);
		const int c = kSub / 2;
		// a right triangle with its corners on the centres of (2, 2), (6, 2) and (2, 6)
		const Pt a = { 2 * kSub + c, 2 * kSub + c };
		const Pt b = { 6 * kSub + c, 2 * kSub + c };
		const Pt d = { 2 * kSub + c, 6 * kSub + c };
		Common::Array<byte> red;
		clear();
		triangle(a, b, d);
		readRed(red);
		// top edge (row 2) and left edge (column 2) are in, the diagonal x + y = 8 is a right edge: out
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				const bool inside = x >= 2 && y >= 2 && x + y < 8;
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, inside);
			}
		}

		// the other half of the square takes the diagonal, as its left edge, and not its bottom or right
		const Pt e = { 6 * kSub + c, 6 * kSub + c };
		clear();
		triangle(b, e, d);
		readRed(red);
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				const bool inside = x < 6 && y < 6 && x + y >= 8;
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, inside);
			}
		}
	}

	// The order of the vertices and the winding change nothing
	void testVertexOrder() {
		makeContext(32, 32);
		const Pt a = { 37, 21 }, b = { 451, 133 }, c = { 190, 480 };
		const Pt *order[6][3] = {
			{ &a, &b, &c }, { &b, &c, &a }, { &c, &a, &b },
			{ &a, &c, &b }, { &c, &b, &a }, { &b, &a, &c }
		};
		TS_ASSERT(countCovered(a, b, c) > 100);
		for (int i = 0; i < 6; i++)
			checkTriangle(*order[i][0], *order[i][1], *order[i][2]);
	}

	void testFlatTopAndBottom() {
		makeContext(32, 32);
		for (int i = 0; i < 300; i++) {
			Pt a = rndPt(), b = rndPt(), c = rndPt();
			b.y = a.y; // a horizontal edge, above or below c
			checkTriangle(a, b, c);
			checkTriangle(c, b, a);
		}
	}

	void testVerticalEdge() {
		makeContext(32, 32);
		for (int i = 0; i < 300; i++) {
			Pt a = rndPt(), b = rndPt(), c = rndPt();
			b.x = a.x;
			checkTriangle(a, b, c);
		}
	}

	// No area: nothing is drawn
	void testDegenerate() {
		makeContext(32, 32);
		const Pt a = { 40, 40 }, b = { 200, 120 }, c = { 360, 200 };
		TS_ASSERT_EQUALS(area2(a, b, c), 0);
		Common::Array<byte> red;
		clear();
		triangle(a, b, c);
		triangle(a, a, b);
		triangle(a, a, a);
		readRed(red);
		for (int i = 0; i < _w * _h; i++)
			TS_ASSERT_EQUALS(red[i], 0);
	}

	void testRandomTriangles() {
		makeContext(64, 64);
		for (int i = 0; i < 1500; i++)
			checkTriangle(rndPt(), rndPt(), rndPt());
	}

	// Vertices on pixel centres: the edges go through many more of them
	void testRandomTrianglesOnPixelCentres() {
		makeContext(64, 64);
		for (int i = 0; i < 1500; i++) {
			Pt p[3];
			for (int j = 0; j < 3; j++) {
				p[j].x = rnd(0, _w - 1) * kSub + kSub / 2;
				p[j].y = rnd(0, _h - 1) * kSub + kSub / 2;
			}
			checkTriangle(p[0], p[1], p[2]);
		}
	}

	// Triangles smaller than a pixel: drawn when they hold the centre
	void testRandomSmallTriangles() {
		makeContext(32, 32);
		int drawn = 0;
		for (int i = 0; i < 1500; i++) {
			const Pt o = rndPt();
			Pt p[3];
			for (int j = 0; j < 3; j++) {
				p[j].x = CLIP(o.x + rnd(-20, 20), 0, _w * kSub);
				p[j].y = CLIP(o.y + rnd(-20, 20), 0, _h * kSub);
			}
			drawn += countCovered(p[0], p[1], p[2]);
			checkTriangle(p[0], p[1], p[2]);
		}
		TS_ASSERT(drawn > 100);
	}

	// Long, thin triangles
	void testRandomSlivers() {
		makeContext(64, 64);
		for (int i = 0; i < 1500; i++) {
			const Pt a = rndPt(), b = rndPt();
			Pt c;
			c.x = CLIP((a.x + b.x) / 2 + rnd(-3, 3), 0, _w * kSub);
			c.y = CLIP((a.y + b.y) / 2 + rnd(-3, 3), 0, _h * kSub);
			checkTriangle(a, b, c);
		}
	}

	// Long edges: the position of the edge must not drift over the rows
	void testRandomLargeTriangles() {
		makeContext(1024, 512);
		for (int i = 0; i < 40; i++)
			checkTriangle(rndPt(), rndPt(), rndPt());
	}

	// A triangle of almost no area with a pixel centre on its left edge is one pixel, and it must not be
	// thrown away as having no area (the products of the coordinates do not fit a float)
	void testTinyAreaLargeTriangle() {
		makeContext(1024, 1024);
		static const int cases[4][8] = {
			// a, b, c, the pixel
			{ 11948, 4733, 14756, 10483, 12891, 6664, 834, 475 },
			{ 11855, 111, 5409, 10113, 4867, 10954, 539, 319 },
			{ 4202, 4525, 12358, 14147, 2711, 2766, 517, 583 },
			{ 2947, 3134, 14925, 15762, 17, 45, 558, 590 }
		};
		for (int i = 0; i < 4; i++) {
			const Pt a = { cases[i][0], cases[i][1] };
			const Pt b = { cases[i][2], cases[i][3] };
			const Pt c = { cases[i][4], cases[i][5] };
			TS_ASSERT_EQUALS(ABS<int64>(area2(a, b, c)), 2);
			TS_ASSERT(covers(a, b, c, cases[i][6], cases[i][7]));
			TS_ASSERT(countCovered(a, b, c) >= 1);
			checkTriangle(a, b, c);
			checkTriangle(a, c, b);
		}
	}

	// Triangles around a point leave no hole and draw no pixel twice
	void testFanDrawsEveryPixelOnce() {
		makeContext(64, 64);
		for (int run = 0; run < 100; run++) {
			// a ring of points on the border of the buffer, in order
			Common::Array<Pt> ring;
			const int perSide = rnd(1, 4);
			for (int side = 0; side < 4; side++) {
				int last = 0;
				for (int k = 0; k < perSide; k++) {
					const int max = _w * kSub;
					const int t = k == 0 ? 0 : rnd(last + 1, max - (perSide - k));
					last = t;
					Pt p;
					switch (side) {
					case 0: p.x = t; p.y = 0; break;
					case 1: p.x = max; p.y = t; break;
					case 2: p.x = max - t; p.y = max; break;
					default: p.x = 0; p.y = max - t; break;
					}
					ring.push_back(p);
				}
			}
			Pt centre;
			centre.x = rnd(1, _w * kSub - 1);
			centre.y = rnd(1, _h * kSub - 1);

			Common::Array<byte> red;
			beginCounting();
			for (uint k = 0; k < ring.size(); k++) {
				// (alternate the winding and the first vertex: neither may matter)
				const Pt &p = ring[k];
				const Pt &q = ring[(k + 1) % ring.size()];
				if ((k + run) % 3 == 0)
					triangle(centre, p, q);
				else if ((k + run) % 3 == 1)
					triangle(q, p, centre);
				else
					triangle(p, centre, q);
			}
			readRed(red);
			int wrong = 0;
			for (int i = 0; i < _w * _h; i++) {
				if (drawCount(red[i]) != 1)
					wrong++;
			}
			TS_ASSERT_EQUALS(wrong, 0);
			tglDisable(TGL_BLEND);
		}
	}

	// A mesh of random points: every pixel once
	void testMeshDrawsEveryPixelOnce() {
		makeContext(64, 64);
		enum { kN = 6 };
		for (int run = 0; run < 100; run++) {
			// a grid whose inner points are moved about within their cells
			Pt grid[kN + 1][kN + 1];
			const int cell = _w * kSub / kN;
			for (int j = 0; j <= kN; j++) {
				for (int i = 0; i <= kN; i++) {
					grid[j][i].x = i == kN ? _w * kSub : i * cell;
					grid[j][i].y = j == kN ? _h * kSub : j * cell;
					if (i > 0 && i < kN)
						grid[j][i].x += rnd(-cell / 3, cell / 3);
					if (j > 0 && j < kN)
						grid[j][i].y += rnd(-cell / 3, cell / 3);
				}
			}
			Common::Array<byte> red;
			beginCounting();
			for (int j = 0; j < kN; j++) {
				for (int i = 0; i < kN; i++) {
					if ((i + j + run) & 1) {
						triangle(grid[j][i], grid[j][i + 1], grid[j + 1][i + 1]);
						triangle(grid[j + 1][i + 1], grid[j + 1][i], grid[j][i]);
					} else {
						triangle(grid[j][i], grid[j][i + 1], grid[j + 1][i]);
						triangle(grid[j][i + 1], grid[j + 1][i], grid[j + 1][i + 1]);
					}
				}
			}
			readRed(red);
			int wrong = 0;
			for (int i = 0; i < _w * _h; i++) {
				if (drawCount(red[i]) != 1)
					wrong++;
			}
			TS_ASSERT_EQUALS(wrong, 0);
			tglDisable(TGL_BLEND);
		}
	}

	// The colour of a pixel is the colour at its centre
	void testSmoothColourAtPixelCentre() {
		makeContext(64, 64);
		tglShadeModel(TGL_SMOOTH);
		clear();
		// red grows to the right, green grows downwards
		tglBegin(TGL_QUADS);
		tglColor4f(0, 0, 0, 1);
		vertex(0, 0);
		tglColor4f(1, 0, 0, 1);
		vertex(_w * kSub, 0);
		tglColor4f(1, 1, 0, 1);
		vertex(_w * kSub, _h * kSub);
		tglColor4f(0, 1, 0, 1);
		vertex(0, _h * kSub);
		tglEnd();
		for (int y = 0; y < _h; y += 7) {
			for (int x = 0; x < _w; x += 5) {
				byte r, g, b;
				readPixel(x, y, r, g, b);
				TS_ASSERT_DELTA((int)r, (x + 0.5) * 255 / _w, 1.5);
				TS_ASSERT_DELTA((int)g, (y + 0.5) * 255 / _h, 1.5);
				TS_ASSERT_EQUALS(b, 0);
			}
		}
	}

	// The same for a triangle that does not start on a pixel: the colour does not depend on where the
	// vertices are within their pixels
	void testSmoothColourIndependentOfSubPixel() {
		makeContext(64, 64);
		tglShadeModel(TGL_SMOOTH);
		for (int sub = 0; sub < kSub; sub += 3) {
			// the colour is a function of the window position, whatever the triangle is
			const Pt p[3] = { { 3 * kSub + sub, 2 * kSub + sub }, { 60 * kSub - sub, 9 * kSub + sub }, { 21 * kSub + sub, 61 * kSub - sub } };
			clear();
			tglBegin(TGL_TRIANGLES);
			for (int i = 0; i < 3; i++) {
				tglColor4f(p[i].x / (float)(_w * kSub), p[i].y / (float)(_h * kSub), 0, 1);
				vertex(p[i]);
			}
			tglEnd();
			int checked = 0;
			for (int y = 0; y < _h; y += 3) {
				for (int x = 0; x < _w; x += 3) {
					if (!covers(p[0], p[1], p[2], x, y))
						continue;
					byte r, g, b;
					readPixel(x, y, r, g, b);
					TS_ASSERT_DELTA((int)r, (x + 0.5) * 255 / _w, 1.5);
					TS_ASSERT_DELTA((int)g, (y + 0.5) * 255 / _h, 1.5);
					checked++;
				}
			}
			TS_ASSERT(checked > 50);
		}
	}

	// A texture drawn one texel per pixel, and two pixels per texel, gives its texels back
	void testTextureAtPixelCentre() {
		enum { kTex = 4 };
		makeContext(32, 32, kTex);
		byte texData[kTex * kTex * 4];
		for (int t = 0; t < kTex; t++) {
			for (int s = 0; s < kTex; s++) {
				byte *px = texData + (t * kTex + s) * 4;
				px[0] = 40 + 60 * s;
				px[1] = 40 + 60 * t;
				px[2] = 0;
				px[3] = 255;
			}
		}
		TGLuint texture;
		tglGenTextures(1, &texture);
		tglBindTexture(TGL_TEXTURE_2D, texture);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_S, TGL_CLAMP);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_T, TGL_CLAMP);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MAG_FILTER, TGL_NEAREST);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MIN_FILTER, TGL_NEAREST);
		tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGBA, kTex, kTex, 0, TGL_RGBA, TGL_UNSIGNED_BYTE, texData);
		tglEnable(TGL_TEXTURE_2D);
		tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_REPLACE);

		for (int scale = 1; scale <= 4; scale++) {
			const int x0 = 5, y0 = 9, size = kTex * scale;
			clear();
			tglBegin(TGL_QUADS);
			tglTexCoord2f(0, 0);
			vertex(x0 * kSub, y0 * kSub);
			tglTexCoord2f(1, 0);
			vertex((x0 + size) * kSub, y0 * kSub);
			tglTexCoord2f(1, 1);
			vertex((x0 + size) * kSub, (y0 + size) * kSub);
			tglTexCoord2f(0, 1);
			vertex(x0 * kSub, (y0 + size) * kSub);
			tglEnd();
			for (int y = 0; y < size; y++) {
				for (int x = 0; x < size; x++) {
					byte r, g, b;
					readPixel(x0 + x, y0 + y, r, g, b);
					TS_ASSERT_EQUALS((int)r, 40 + 60 * (x / scale));
					TS_ASSERT_EQUALS((int)g, 40 + 60 * (y / scale));
				}
			}
		}
		tglDisable(TGL_TEXTURE_2D);
		tglDeleteTextures(1, &texture);
	}

	// With the depth test, a triangle drawn twice passes TGL_LEQUAL and TGL_EQUAL whatever the order
	// of its vertices: the depth of a pixel depends on the pixel only
	void testDepthIndependentOfVertexOrder() {
		makeContext(64, 64);
		tglEnable(TGL_DEPTH_TEST);
		tglDepthMask(TGL_TRUE);
		const Pt p[3] = { { 37, 21 }, { 951, 333 }, { 290, 980 } };
		const float z[3] = { -0.5f, 0.1f, 0.7f };
		for (int order = 0; order < 6; order++) {
			static const int perm[6][3] = { { 0, 1, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 0, 2, 1 }, { 2, 1, 0 }, { 1, 0, 2 } };
			tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT);
			tglDepthFunc(TGL_LESS);
			tglColor4f(0, 0, 0, 1);
			tglBegin(TGL_TRIANGLES);
			for (int i = 0; i < 3; i++)
				tglVertex3f(p[i].x / (float)(kSub / 2 * _w) - 1.0f, 1.0f - p[i].y / (float)(kSub / 2 * _h), z[i]);
			tglEnd();
			tglDepthFunc(TGL_EQUAL);
			tglColor4f(1, 1, 1, 1);
			tglBegin(TGL_TRIANGLES);
			for (int i = 0; i < 3; i++) {
				const int k = perm[order][i];
				tglVertex3f(p[k].x / (float)(kSub / 2 * _w) - 1.0f, 1.0f - p[k].y / (float)(kSub / 2 * _h), z[k]);
			}
			tglEnd();
			Common::Array<byte> red;
			readRed(red);
			int wrong = 0, covered = 0;
			for (int y = 0; y < _h; y++) {
				for (int x = 0; x < _w; x++) {
					const bool expected = covers(p[0], p[1], p[2], x, y);
					covered += expected ? 1 : 0;
					if (expected != (red[y * _w + x] != 0))
						wrong++;
				}
			}
			TS_ASSERT(covered > 500);
			TS_ASSERT_EQUALS(wrong, 0);
		}
		tglDisable(TGL_DEPTH_TEST);
	}

	// A point is drawn on the pixel it is in
	void testPoints() {
		makeContext(16, 16);
		static const int cases[][4] = {
			// position (sub-pixels), pixel
			{ 5 * kSub, 7 * kSub, 5, 7 },
			{ 5 * kSub + 8, 7 * kSub + 8, 5, 7 },
			{ 5 * kSub + 15, 7 * kSub + 15, 5, 7 },
			{ 6 * kSub, 8 * kSub, 6, 8 },
			{ 0, 0, 0, 0 },
			// (the far edges belong to no pixel: the last one is used)
			{ 16 * kSub, 16 * kSub, 15, 15 },
			{ 16 * kSub, 3 * kSub, 15, 3 },
			{ 3 * kSub, 16 * kSub, 3, 15 }
		};
		for (uint i = 0; i < ARRAYSIZE(cases); i++) {
			Common::Array<byte> red;
			clear();
			tglBegin(TGL_POINTS);
			vertex(cases[i][0], cases[i][1]);
			tglEnd();
			readRed(red);
			for (int y = 0; y < _h; y++)
				for (int x = 0; x < _w; x++)
					TS_ASSERT_EQUALS(red[y * _w + x] != 0, x == cases[i][2] && y == cases[i][3]);
		}
	}

	// A line runs from the pixel of its first vertex up to, and without, the pixel of its second
	void testLines() {
		makeContext(16, 16);
		Common::Array<byte> red;

		// horizontal, on whole pixels and anywhere within them
		for (int sub = 0; sub < kSub; sub += 5) {
			clear();
			tglBegin(TGL_LINES);
			vertex(2 * kSub + sub, 5 * kSub + sub);
			vertex(10 * kSub + sub, 5 * kSub + sub);
			tglEnd();
			readRed(red);
			for (int y = 0; y < _h; y++)
				for (int x = 0; x < _w; x++)
					TS_ASSERT_EQUALS(red[y * _w + x] != 0, y == 5 && x >= 2 && x < 10);
		}

		// vertical, upwards
		clear();
		tglBegin(TGL_LINES);
		vertex(3 * kSub, 12 * kSub);
		vertex(3 * kSub, 4 * kSub);
		tglEnd();
		readRed(red);
		for (int y = 0; y < _h; y++)
			for (int x = 0; x < _w; x++)
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, x == 3 && y > 4 && y <= 12);

		// diagonal
		clear();
		tglBegin(TGL_LINES);
		vertex(1 * kSub, 1 * kSub);
		vertex(9 * kSub, 9 * kSub);
		tglEnd();
		readRed(red);
		for (int y = 0; y < _h; y++)
			for (int x = 0; x < _w; x++)
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, x == y && x >= 1 && x < 9);

		// along the far edges of the buffer: the last column and row
		clear();
		tglBegin(TGL_LINES);
		vertex(16 * kSub, 0);
		vertex(16 * kSub, 16 * kSub);
		vertex(0, 16 * kSub);
		vertex(16 * kSub, 16 * kSub);
		tglEnd();
		readRed(red);
		for (int y = 0; y < _h; y++)
			for (int x = 0; x < _w; x++)
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, (x == 15 && y < 15) || (y == 15 && x < 15));
	}

	// The outline of a rectangle (as a line loop) has its four sides, corners included
	void testLineLoop() {
		makeContext(16, 16);
		const int x0 = 3, y0 = 4, x1 = 11, y1 = 9;
		Common::Array<byte> red;
		clear();
		tglBegin(TGL_LINE_LOOP);
		vertex(x0 * kSub, y0 * kSub);
		vertex(x1 * kSub, y0 * kSub);
		vertex(x1 * kSub, y1 * kSub);
		vertex(x0 * kSub, y1 * kSub);
		tglEnd();
		readRed(red);
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				const bool inBox = x >= x0 && x <= x1 && y >= y0 && y <= y1;
				const bool onSide = x == x0 || x == x1 || y == y0 || y == y1;
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, inBox && onSide);
			}
		}
	}

	// A viewport smaller than the buffer: its corners are the corners of its first and last pixels
	void testViewportOffset() {
		makeContext(64, 64);
		const int vx = 8, vy = 16, vw = 32, vh = 16; // (tglViewport's y is from the bottom)
		Common::Array<byte> red;
		clear();
		tglViewport(vx, vy, vw, vh);
		tglBegin(TGL_QUADS);
		tglVertex2f(-1, -1);
		tglVertex2f(1, -1);
		tglVertex2f(1, 1);
		tglVertex2f(-1, 1);
		tglEnd();
		readRed(red);
		const int top = _h - vy - vh;
		for (int y = 0; y < _h; y++) {
			for (int x = 0; x < _w; x++) {
				const bool inside = x >= vx && x < vx + vw && y >= top && y < top + vh;
				TS_ASSERT_EQUALS(red[y * _w + x] != 0, inside);
			}
		}
		tglViewport(0, 0, _w, _h);
	}
};

#endif
