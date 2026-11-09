#include <cxxtest/TestSuite.h>

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"
#include "graphics/tinygl/zgl.h"

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

	// Spotlights. The material only reflects diffuse light and there is no
	// ambient, so the red channel is the light's diffuse term times the spot
	// attenuation. Identity matrices: object space == eye space.

	void setUpLighting() {
		const TGLfloat zero[] = { 0, 0, 0, 1 };
		const TGLfloat white[] = { 1, 1, 1, 1 };

		tglDisable(TGL_TEXTURE_2D);
		tglDisable(TGL_BLEND);
		tglDisable(TGL_DEPTH_TEST);
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglViewport(0, 0, 2, 2);

		tglLightModelfv(TGL_LIGHT_MODEL_AMBIENT, zero);
		tglMaterialfv(TGL_FRONT_AND_BACK, TGL_AMBIENT, zero);
		tglMaterialfv(TGL_FRONT_AND_BACK, TGL_DIFFUSE, white);
		tglMaterialfv(TGL_FRONT_AND_BACK, TGL_SPECULAR, zero);
		tglMaterialfv(TGL_FRONT_AND_BACK, TGL_EMISSION, zero);
		tglLightfv(TGL_LIGHT0, TGL_AMBIENT, zero);
		tglLightfv(TGL_LIGHT0, TGL_SPECULAR, zero);
		tglEnable(TGL_LIGHTING);
		tglEnable(TGL_LIGHT0);
	}

	// Draws a lit triangle in the z = 0 plane facing +z, covering the whole
	// 2x2 viewport, and returns pixel (0, 0)'s red channel.
	byte drawLitTriangle() {
		const float S = 10.0f;
		tglBegin(TGL_TRIANGLES);
		tglNormal3f(0, 0, 1);
		tglVertex3f(-S, -S, 0);
		tglVertex3f(+S, -S, 0);
		tglVertex3f(0, +S, 0);
		tglEnd();

		byte a, r, g, b;
		Graphics::Surface surface;
		TinyGL::presentBuffer();
		TinyGL::getSurfaceRef(surface);
		surface.format.colorToARGB(surface.getPixel(0, 0), a, r, g, b);
		return r;
	}

	// Positional light 100 units above the plane. The vertices are at most
	// atan(10 * sqrt(2) / 100) ~= 8.05 degrees off the spot axis.
	void setPositionalSpot(const TGLfloat *dir, TGLfloat cutoff) {
		const TGLfloat pos[] = { 0, 0, 100, 1 };
		const TGLfloat diffuse[] = { 2, 2, 2, 1 };	// saturates when lit
		tglLightfv(TGL_LIGHT0, TGL_DIFFUSE, diffuse);
		tglLightfv(TGL_LIGHT0, TGL_POSITION, pos);
		tglLightfv(TGL_LIGHT0, TGL_SPOT_DIRECTION, dir);
		tglLightf(TGL_LIGHT0, TGL_SPOT_CUTOFF, cutoff);
	}

	void testSpotDirectionStored() {
		const TGLfloat dir[] = { 3, 0, -4 };
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglLightfv(TGL_LIGHT0, TGL_SPOT_DIRECTION, dir);
		const TinyGL::GLLight &l = ctx()->lights[0];
		TS_ASSERT_EQUALS(l.spot_direction.X, 3.0f);
		TS_ASSERT_EQUALS(l.spot_direction.Y, 0.0f);
		TS_ASSERT_EQUALS(l.spot_direction.Z, -4.0f);
		TS_ASSERT_DELTA(l.norm_spot_direction.X, 0.6f, 1e-6f);
		TS_ASSERT_DELTA(l.norm_spot_direction.Y, 0.0f, 1e-6f);
		TS_ASSERT_DELTA(l.norm_spot_direction.Z, -0.8f, 1e-6f);
	}

	// Like GL_POSITION, the direction is transformed by the modelview matrix
	// current at the time of the call.
	void testSpotDirectionModelview() {
		const TGLfloat dir[] = { 0, 0, -1 };
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglRotatef(90, 0, 1, 0);	// -z rotates to -x
		tglLightfv(TGL_LIGHT0, TGL_SPOT_DIRECTION, dir);
		const TinyGL::GLLight &l = ctx()->lights[0];
		TS_ASSERT_DELTA(l.norm_spot_direction.X, -1.0f, 1e-6f);
		TS_ASSERT_DELTA(l.norm_spot_direction.Y, 0.0f, 1e-6f);
		TS_ASSERT_DELTA(l.norm_spot_direction.Z, 0.0f, 1e-6f);
		tglLoadIdentity();
	}

	void testSpotCutoffStored() {
		tglLightf(TGL_LIGHT0, TGL_SPOT_CUTOFF, 60);
		TS_ASSERT_EQUALS(ctx()->lights[0].spot_cutoff, 60.0f);
		TS_ASSERT_DELTA(ctx()->lights[0].cos_spot_cutoff, 0.5f, 1e-6f);
		tglLightf(TGL_LIGHT0, TGL_SPOT_EXPONENT, 7);
		TS_ASSERT_EQUALS(ctx()->lights[0].spot_exponent, 7.0f);
	}

	void testSpotlightInsideCone() {
		const TGLfloat down[] = { 0, 0, -1 };
		setUpLighting();
		setPositionalSpot(down, 20);
		TS_ASSERT_EQUALS(drawLitTriangle(), 255);
	}

	void testSpotlightOutsideNarrowCone() {
		const TGLfloat down[] = { 0, 0, -1 };
		setUpLighting();
		setPositionalSpot(down, 5);
		TS_ASSERT_EQUALS(drawLitTriangle(), 0);
	}

	// Same cone as testSpotlightInsideCone but tilted 45 degrees away from
	// the triangle, so it must honour the direction, not just the cutoff.
	void testSpotlightPointingAway() {
		const TGLfloat tilted[] = { 1, 0, -1 };
		setUpLighting();
		setPositionalSpot(tilted, 20);
		TS_ASSERT_EQUALS(drawLitTriangle(), 0);
	}

	void testSpotlightNoCutoffIgnoresDirection() {
		const TGLfloat up[] = { 0, 0, 1 };
		setUpLighting();
		setPositionalSpot(up, 180);
		TS_ASSERT_EQUALS(drawLitTriangle(), 255);
	}

	// Directional light along +z so every vertex sees the same angle: the
	// spot axis is 60 degrees off the light vector, cos = 0.5, and the
	// attenuation is 0.5^exponent (OpenGL 1.x spec, section 2.14.1).
	byte drawDirectionalSpot(TGLfloat cutoff, TGLfloat exponent) {
		const TGLfloat pos[] = { 0, 0, 1, 0 };
		const TGLfloat dir[] = { 0, 0.8660254f, -0.5f };
		const TGLfloat white[] = { 1, 1, 1, 1 };
		setUpLighting();
		tglLightfv(TGL_LIGHT0, TGL_DIFFUSE, white);
		tglLightfv(TGL_LIGHT0, TGL_POSITION, pos);
		tglLightfv(TGL_LIGHT0, TGL_SPOT_DIRECTION, dir);
		tglLightf(TGL_LIGHT0, TGL_SPOT_CUTOFF, cutoff);
		tglLightf(TGL_LIGHT0, TGL_SPOT_EXPONENT, exponent);
		return drawLitTriangle();
	}

	void testSpotExponent0() {
		TS_ASSERT_EQUALS(drawDirectionalSpot(70, 0), 255);
	}

	void testSpotExponent1() {
		TS_ASSERT_DELTA(drawDirectionalSpot(70, 1), 0.5f * 255, 1);
	}

	void testSpotExponent2() {
		TS_ASSERT_DELTA(drawDirectionalSpot(70, 2), 0.25f * 255, 1);
	}

	void testSpotExponent8() {
		TS_ASSERT_DELTA(drawDirectionalSpot(70, 8), 255.0f / 256, 1);
	}

	void testSpotOutsideCutoffDirectional() {
		TS_ASSERT_EQUALS(drawDirectionalSpot(50, 0), 0);	// cos 50 ~= 0.643 > 0.5
	}

	void testSpotlightDisplayList() {
		const TGLfloat pos[] = { 0, 0, 100, 1 };
		const TGLfloat down[] = { 0, 0, -1 };
		const TGLfloat diffuse[] = { 2, 2, 2, 1 };
		setUpLighting();

		TGLuint list = tglGenLists(1);
		tglNewList(list, TGL_COMPILE);
		tglLightfv(TGL_LIGHT0, TGL_DIFFUSE, diffuse);
		tglLightfv(TGL_LIGHT0, TGL_POSITION, pos);
		tglLightfv(TGL_LIGHT0, TGL_SPOT_DIRECTION, down);
		tglLightf(TGL_LIGHT0, TGL_SPOT_CUTOFF, 20);
		tglEndList();

		TS_ASSERT_EQUALS(ctx()->lights[0].spot_cutoff, 180.0f);	// not executed yet

		tglCallList(list);
		TS_ASSERT_EQUALS(ctx()->lights[0].spot_cutoff, 20.0f);
		TS_ASSERT_DELTA(ctx()->lights[0].norm_spot_direction.Z, -1.0f, 1e-6f);
		TS_ASSERT_EQUALS(drawLitTriangle(), 255);
	}

	// Array pointers with stride 0 are tightly packed: stride = size *
	// sizeof(type). Each check first sets a different explicit stride so a
	// stale value can't pass by accident (gl_ColorPointer used to multiply the
	// old stride, uninitialised on a fresh context).

	void checkColorStride(TGLint size, TGLenum type, int expected) {
		static const TGLubyte dummy[64] = {};
		tglColorPointer(4, TGL_UNSIGNED_BYTE, 100, dummy);
		tglColorPointer(size, type, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->color_array_size, size);
		TS_ASSERT_EQUALS(ctx()->color_array_type, (int)type);
		TS_ASSERT_EQUALS(ctx()->color_array_stride, expected);
		tglColorPointer(size, type, 7, dummy);
		TS_ASSERT_EQUALS(ctx()->color_array_stride, 7);
	}

	void testColorPointerPackedStride() {
		checkColorStride(3, TGL_BYTE, 3);
		checkColorStride(4, TGL_BYTE, 4);
		checkColorStride(3, TGL_UNSIGNED_BYTE, 3);
		checkColorStride(4, TGL_UNSIGNED_BYTE, 4);
		checkColorStride(3, TGL_SHORT, 6);
		checkColorStride(4, TGL_UNSIGNED_SHORT, 8);
		checkColorStride(3, TGL_INT, 12);
		checkColorStride(4, TGL_UNSIGNED_INT, 16);
		checkColorStride(3, TGL_FLOAT, 12);
		checkColorStride(4, TGL_FLOAT, 16);
		checkColorStride(3, TGL_DOUBLE, 24);
		checkColorStride(4, TGL_DOUBLE, 32);
	}

	void testColorPointerPackedStrideFreshContext() {
		static const TGLubyte dummy[16] = {};
		tglColorPointer(4, TGL_UNSIGNED_BYTE, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->color_array_stride, 4);
	}

	void testVertexPointerPackedStride() {
		static const TGLfloat dummy[16] = {};
		tglVertexPointer(4, TGL_FLOAT, 100, dummy);
		tglVertexPointer(2, TGL_FLOAT, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->vertex_array_stride, 8);
		tglVertexPointer(3, TGL_SHORT, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->vertex_array_stride, 6);
		tglVertexPointer(3, TGL_DOUBLE, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->vertex_array_stride, 24);
	}

	void testNormalPointerPackedStride() {
		static const TGLfloat dummy[16] = {};
		tglNormalPointer(TGL_FLOAT, 100, dummy);
		tglNormalPointer(TGL_FLOAT, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->normal_array_stride, 12);
		tglNormalPointer(TGL_SHORT, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->normal_array_stride, 6);
	}

	void testTexCoordPointerPackedStride() {
		static const TGLfloat dummy[16] = {};
		tglTexCoordPointer(4, TGL_FLOAT, 100, dummy);
		tglTexCoordPointer(2, TGL_FLOAT, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->texcoord_array_stride, 8);
		tglTexCoordPointer(3, TGL_SHORT, 0, dummy);
		TS_ASSERT_EQUALS(ctx()->texcoord_array_stride, 6);
	}

	// Rendering version: three packed orange vertices followed by blue. A
	// stale stride of 8 makes vertex 2 read the blue bytes.
	void testDrawArraysPackedColorAfterStride() {
		const float S = 10.0f;
		const TGLfloat verts[] = { -S, -S, +S, -S, 0, +S };
		const TGLubyte colors[] = {
			255, 127, 0, 255,  255, 127, 0, 255,  255, 127, 0, 255,
			0, 0, 255, 255,  0, 0, 255, 255
		};

		tglDisable(TGL_TEXTURE_2D);
		tglDisable(TGL_BLEND);
		tglDisable(TGL_DEPTH_TEST);
		tglDisable(TGL_LIGHTING);
		tglMatrixMode(TGL_PROJECTION);
		tglLoadIdentity();
		tglMatrixMode(TGL_MODELVIEW);
		tglLoadIdentity();
		tglViewport(0, 0, 2, 2);

		tglEnableClientState(TGL_VERTEX_ARRAY);
		tglEnableClientState(TGL_COLOR_ARRAY);
		tglVertexPointer(2, TGL_FLOAT, 0, verts);
		tglColorPointer(4, TGL_UNSIGNED_BYTE, 8, colors);
		tglColorPointer(4, TGL_UNSIGNED_BYTE, 0, colors);
		tglDrawArrays(TGL_TRIANGLES, 0, 3);
		tglDisableClientState(TGL_COLOR_ARRAY);
		tglDisableClientState(TGL_VERTEX_ARRAY);

		byte a, r, g, b;
		Graphics::Surface surface;
		TinyGL::presentBuffer();
		TinyGL::getSurfaceRef(surface);
		for (int y = 0; y < 2; y++) {
			for (int x = 0; x < 2; x++) {
				surface.format.colorToARGB(surface.getPixel(x, y), a, r, g, b);
				TS_ASSERT_EQUALS(r, 255);
				TS_ASSERT_EQUALS(g, 127);
				TS_ASSERT_EQUALS(b, 0);
				TS_ASSERT_EQUALS(a, 255);
			}
		}
	}

	// Context defaults. GLContext comes from plain `new`, so a member that
	// init() forgets keeps whatever was in the heap block. Dirty the state,
	// recreate the context (a same-size allocation usually reuses the freed
	// block) and check every default is back to the GL 1.x value.
	void recreateDirtyContext() {
		tglEnable(TGL_NORMALIZE);
		tglEnable(TGL_TWO_COLOR_STIPPLE);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_S, TGL_CLAMP);
		tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_T, TGL_CLAMP);
		TS_ASSERT(ctx()->normalize_enabled);
		TS_ASSERT(ctx()->two_color_stipple_enabled);
		TS_ASSERT_EQUALS(ctx()->texture_wrap_s, (int)TGL_CLAMP);
		TS_ASSERT_EQUALS(ctx()->texture_wrap_t, (int)TGL_CLAMP);
		tearDown();
		setUp();
	}

	void testDefaultNormalizeDisabled() {
		recreateDirtyContext();
		TS_ASSERT(!ctx()->normalize_enabled);
		TGLint enabled = 1;
		tglGetIntegerv(TGL_NORMALIZE, &enabled);
		TS_ASSERT_EQUALS(enabled, 0);
	}

	void testDefaultTwoColorStippleDisabled() {
		recreateDirtyContext();
		TS_ASSERT(!ctx()->two_color_stipple_enabled);
	}

	void testDefaultTextureWrapRepeat() {
		recreateDirtyContext();
		TS_ASSERT_EQUALS(ctx()->texture_wrap_s, (int)TGL_REPEAT);
		TS_ASSERT_EQUALS(ctx()->texture_wrap_t, (int)TGL_REPEAT);
	}

	// Without GL_NORMALIZE a half-length normal halves the diffuse term
	// (the light is straight overhead, so N.L = |N|). With it, full brightness.
	byte drawHalfNormalTriangle() {
		const TGLfloat pos[] = { 0, 0, 1, 0 };
		const TGLfloat white[] = { 1, 1, 1, 1 };
		setUpLighting();
		tglLightfv(TGL_LIGHT0, TGL_DIFFUSE, white);
		tglLightfv(TGL_LIGHT0, TGL_POSITION, pos);

		const float S = 10.0f;
		tglBegin(TGL_TRIANGLES);
		tglNormal3f(0, 0, 0.5f);
		tglVertex3f(-S, -S, 0);
		tglVertex3f(+S, -S, 0);
		tglVertex3f(0, +S, 0);
		tglEnd();

		byte a, r, g, b;
		Graphics::Surface surface;
		TinyGL::presentBuffer();
		TinyGL::getSurfaceRef(surface);
		surface.format.colorToARGB(surface.getPixel(0, 0), a, r, g, b);
		return r;
	}

	void testNormalizeOffByDefaultRendering() {
		recreateDirtyContext();
		TS_ASSERT_DELTA(drawHalfNormalTriangle(), 0.5f * 255, 1);
	}

	void testNormalizeGet() {
		TGLint enabled = 0;
		tglEnable(TGL_NORMALIZE);
		tglGetIntegerv(TGL_NORMALIZE, &enabled);
		TS_ASSERT_EQUALS(enabled, 1);
		tglDisable(TGL_NORMALIZE);
		tglGetIntegerv(TGL_NORMALIZE, &enabled);
		TS_ASSERT_EQUALS(enabled, 0);
	}

	void testNormalizeEnabledRendering() {
		tglEnable(TGL_NORMALIZE);
		TS_ASSERT_EQUALS(drawHalfNormalTriangle(), 255);
	}
};

#endif
