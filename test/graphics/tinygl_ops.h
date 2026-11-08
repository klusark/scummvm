#include <cxxtest/TestSuite.h>

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"
#include "graphics/tinygl/zgl.h"

// Checks that every op's parameter count in opinfo.h matches what its tgl*
// wrapper writes and its glop* handler reads. Display lists copy exactly
// OP_<name>_SIZE slots per op, so a mismatch shows up as wrong state after
// tglCallList even when immediate mode looks fine.

class TinyGLOpsTestSuite : public CxxTest::TestSuite {
	TinyGL::ContextHandle *_context = nullptr;

	static TinyGL::GLContext *ctx() {
		return TinyGL::gl_get_context();
	}

public:
	void setUp() {
		_context = TinyGL::createContext(2, 2, Graphics::PixelFormat::createFormatARGB32(), 2, false, false);
		TinyGL::setContext(_context);
	}

	void tearDown() {
		if (_context != nullptr) {
			TinyGL::destroyContext(_context);
			_context = nullptr;
		}
	}

	void testOpSizes() {
		TS_ASSERT_EQUALS((int)TinyGL::OP_Color_SIZE, 5);
		TS_ASSERT_EQUALS((int)TinyGL::OP_Vertex_SIZE, 5);
		TS_ASSERT_EQUALS((int)TinyGL::OP_ColorMask_SIZE, 5);
		TS_ASSERT_EQUALS((int)TinyGL::OP_Fog_SIZE, 6);
		TS_ASSERT_EQUALS((int)TinyGL::OP_LoadMatrix_SIZE, 17);
		TS_ASSERT_EQUALS((int)TinyGL::OP_PolygonStipple_SIZE, 129);
		TS_ASSERT_EQUALS((int)TinyGL::OP_End_SIZE, 1);
	}

	void testOpParamsMacro() {
		TGL_OP_PARAMS(p, Light);
		TS_ASSERT_EQUALS(sizeof(p) / sizeof(p[0]), (size_t)TinyGL::OP_Light_SIZE);
		TS_ASSERT_EQUALS(p[0].op, (int)TinyGL::OP_Light);
		for (int i = 1; i < TinyGL::OP_Light_SIZE; i++)
			TS_ASSERT_EQUALS(p[i].i, 0);
	}

	void testColorMaskImmediate() {
		tglColorMask(TGL_TRUE, TGL_FALSE, TGL_TRUE, TGL_FALSE);
		TS_ASSERT(ctx()->color_mask_red);
		TS_ASSERT(!ctx()->color_mask_green);
		TS_ASSERT(ctx()->color_mask_blue);
		TS_ASSERT(!ctx()->color_mask_alpha);
	}

	void testColorMaskDisplayList() {
		TGLuint list = tglGenLists(1);
		tglNewList(list, TGL_COMPILE);
		tglColorMask(TGL_FALSE, TGL_TRUE, TGL_FALSE, TGL_TRUE);
		tglEndList();

		// compiling must not execute
		TS_ASSERT(ctx()->color_mask_red);
		TS_ASSERT(ctx()->color_mask_green);
		TS_ASSERT(ctx()->color_mask_blue);
		TS_ASSERT(ctx()->color_mask_alpha);

		tglCallList(list);
		TS_ASSERT(!ctx()->color_mask_red);
		TS_ASSERT(ctx()->color_mask_green);
		TS_ASSERT(!ctx()->color_mask_blue);
		TS_ASSERT(ctx()->color_mask_alpha);
	}

	void testColorDisplayList() {
		TGLuint list = tglGenLists(1);
		tglNewList(list, TGL_COMPILE);
		tglColor4f(0.25f, 0.5f, 0.75f, 0.125f);
		tglEndList();

		tglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		tglCallList(list);
		TS_ASSERT_EQUALS(ctx()->current_color.X, 0.25f);
		TS_ASSERT_EQUALS(ctx()->current_color.Y, 0.5f);
		TS_ASSERT_EQUALS(ctx()->current_color.Z, 0.75f);
		TS_ASSERT_EQUALS(ctx()->current_color.W, 0.125f);
	}

	void testFogDisplayList() {
		const TGLfloat color[] = { 0.1f, 0.2f, 0.3f, 0.4f };
		TGLuint list = tglGenLists(1);
		tglNewList(list, TGL_COMPILE);
		tglFogf(TGL_FOG_START, 2.5f);
		tglFogf(TGL_FOG_END, 7.0f);
		tglFogi(TGL_FOG_MODE, TGL_EXP2);
		tglFogfv(TGL_FOG_COLOR, color);
		tglEndList();

		tglCallList(list);
		TS_ASSERT_EQUALS(ctx()->fog_start, 2.5f);
		TS_ASSERT_EQUALS(ctx()->fog_end, 7.0f);
		TS_ASSERT_EQUALS(ctx()->fog_mode, (int)TGL_EXP2);
		TS_ASSERT_EQUALS(ctx()->fog_color.X, 0.1f);
		TS_ASSERT_EQUALS(ctx()->fog_color.Y, 0.2f);
		TS_ASSERT_EQUALS(ctx()->fog_color.Z, 0.3f);
		TS_ASSERT_EQUALS(ctx()->fog_color.W, 0.4f);
	}

	// Exercises glopArrayElement's on-stack Color/Vertex params.
	void testDrawArraysColor() {
		const float S = 10.0f;
		const TGLfloat verts[] = { -S, -S, +S, -S, 0, +S };
		const TGLubyte colors[] = { 255, 127, 0, 255,  255, 127, 0, 255,  255, 127, 0, 255 };

		tglDisable(TGL_TEXTURE_2D);
		tglDisable(TGL_BLEND);
		tglDisable(TGL_DEPTH_TEST);
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglViewport(0, 0, 2, 2);

		tglEnableClientState(TGL_VERTEX_ARRAY);
		tglEnableClientState(TGL_COLOR_ARRAY);
		tglVertexPointer(2, TGL_FLOAT, 0, verts);
		tglColorPointer(4, TGL_UNSIGNED_BYTE, 0, colors);
		tglDrawArrays(TGL_TRIANGLES, 0, 3);
		tglDisableClientState(TGL_COLOR_ARRAY);
		tglDisableClientState(TGL_VERTEX_ARRAY);

		byte a, r, g, b;
		Graphics::Surface surface;
		TinyGL::presentBuffer();
		TinyGL::getSurfaceRef(surface);
		surface.format.colorToARGB(surface.getPixel(0, 0), a, r, g, b);
		TS_ASSERT_EQUALS(r, 255);
		TS_ASSERT_EQUALS(g, 127);
		TS_ASSERT_EQUALS(b, 0);
		TS_ASSERT_EQUALS(a, 255);
	}
};

#endif
