#include <cxxtest/TestSuite.h>

#ifdef USE_TINYGL

#include "graphics/tinygl/tinygl.h"

// every test sets up some texture environment
// then draws a single pixel and checks the resulting output pixel

class TinyGLTexEnvTestSuite : public CxxTest::TestSuite {
    TinyGL::ContextHandle *_context = nullptr;
public:
    void setUp() {
        _context = TinyGL::createContext(2, 2, Graphics::PixelFormat::createFormatARGB32(), 2, false, false);
        TinyGL::setContext(_context);

        tglEnable(TGL_TEXTURE_2D);
        tglDisable(TGL_BLEND);
        tglDisable(TGL_DEPTH_TEST);
        tglMatrixMode(TGL_PROJECTION);
        tglLoadIdentity();
        tglMatrixMode(TGL_MODELVIEW);
        tglLoadIdentity();
		tglViewport(0, 0, 2, 2);
    }

    void tearDown() {
        if (_context != nullptr) {
            TinyGL::destroyContext(_context);
            _context = nullptr;
        }
    }

	// these three functions use RGBA order instead of ARGB to make it consistent with tglColor4ub which we also call

    void setConstant(byte r, byte g, byte b, byte a) {
        const float values[] = { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        tglTexEnvfv(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_COLOR, values);
    }

    void drawPixel(byte texR, byte texG, byte texB, byte texA) {
		const float S = 10.0f;
        const byte texData[] = { texR, texG, texB, texA };
        TGLuint texture;
        tglGenTextures(1, &texture);
        tglBindTexture(TGL_TEXTURE_2D, texture);
        tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_S, TGL_CLAMP);
        tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_T, TGL_CLAMP);
        tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGBA, 1, 1, 0, TGL_RGBA, TGL_UNSIGNED_BYTE, &texData);
        tglBegin(TGL_TRIANGLES);
        tglTexCoord2f(0.0f, 0.0f); tglVertex2f(-S, -S);
        tglTexCoord2f(1.0f, 0.0f); tglVertex2f(+S, -S);
        tglTexCoord2f(0.5f, 1.0f); tglVertex2f(0, +S);
        tglEnd();
    }

    void checkOutput(byte expR, byte expG, byte expB, byte expA) {
        byte actA, actR, actG, actB;
        Graphics::Surface surface;
        TinyGL::presentBuffer();
        TinyGL::getSurfaceRef(surface);
        surface.format.colorToARGB(surface.getPixel(0, 0), actA, actR, actG, actB);

        TS_ASSERT_EQUALS(expA, actA);
        TS_ASSERT_EQUALS(expR, actR);
        TS_ASSERT_EQUALS(expG, actG);
        TS_ASSERT_EQUALS(expB, actB);
    }

    void testModulate() {
        // no tglTexEnvi setup because TGL_MODULATE should be the default
        tglColor4ub(255, 255, 255, 127);
        drawPixel(255, 127, 0, 255);
        checkOutput(255, 127, 0, 127);
    }

    void testReplace() {
        tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_REPLACE);
        tglColor4ub(255, 255, 255, 127);
        drawPixel(255, 127, 0, 255);
        checkOutput(255, 127, 0, 255);
    }

    void testDecal() {
        tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_DECAL);
        tglColor4ub(100, 200, 255, 123);
        drawPixel(200, 100, 0, 192);
        checkOutput(175, 125, 63, 123); // exactly 175.3, 124.7, 63
    }

    void testAdd() {
        tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_ADD);
        tglColor4ub(50, 100, 150, 127);
        drawPixel(25, 50, 150, 200);
        checkOutput(75, 150, 255, 100);
        // attention: TGL_ADD still modulates alpha
    }

    void testBlend() {
        tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_BLEND);
        tglColor4ub(210, 210, 200, 127);
        setConstant(123, 123, 100, 42);
        drawPixel(0, 255, 128, 200);
        checkOutput(210, 123, 150, 100);
    }

	// a(1 - t) + bt rounded to the nearest, as the GL computes it exactly
	static byte exactLerp(int a, int b, int t) {
		return (byte)((2 * (a * (255 - t) + b * t) + 255) / 510);
	}

	// Values at and around the limits and the middle, where rounding goes wrong
	static const byte *lerpValues(int &count) {
		static const byte values[] = { 0, 1, 2, 63, 64, 100, 127, 128, 129, 192, 200, 253, 254, 255 };
		count = ARRAYSIZE(values);
		return values;
	}

	// Every byte colour must come out unchanged when modulated with a white texture. The 16 bit colour used to be
	// rounded to a byte where it should have been truncated, which made every value from 128 one too high.
	void testModulateKeepsEveryColour() {
		for (int n = 0; n < 256; n++) {
			tglColor4ub(n, 255 - n, n, n);
			drawPixel(255, 255, 255, 255);
			checkOutput(n, 255 - n, n, n);
		}
	}

	void testReplaceKeepsEveryAlpha() {
		tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_REPLACE);
		for (int n = 0; n < 256; n++) {
			tglColor4ub(0, 0, 0, n);
			const byte texData[] = { 10, 20, 30 };
			TGLuint texture;
			tglGenTextures(1, &texture);
			tglBindTexture(TGL_TEXTURE_2D, texture);
			tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGB, 1, 1, 0, TGL_RGB, TGL_UNSIGNED_BYTE, texData);
			tglBegin(TGL_TRIANGLES);
			tglTexCoord2f(0.0f, 0.0f); tglVertex2f(-10, -10);
			tglTexCoord2f(1.0f, 0.0f); tglVertex2f(+10, -10);
			tglTexCoord2f(0.5f, 1.0f); tglVertex2f(0, +10);
			tglEnd();
			checkOutput(10, 20, 30, n);
			tglDeleteTextures(1, &texture);
		}
	}

	// Cp(1 - As) + CsAs, rounded once and not per half, against the exact result
	void testDecalExact() {
		tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_DECAL);
		int count;
		const byte *values = lerpValues(count);
		for (int i = 0; i < count; i++) {
			for (int j = 0; j < count; j++) {
				for (int k = 0; k < count; k++) {
					const byte p = values[i], t = values[j], a = values[k];
					tglColor4ub(p, 255 - p, p, 77);
					drawPixel(t, 255 - t, 128, a);
					checkOutput(exactLerp(p, t, a), exactLerp(255 - p, 255 - t, a), exactLerp(p, 128, a), 77);
				}
			}
		}
	}

	// Cp(1 - Cs) + CcCs, rounded once and not per half, against the exact result
	void testBlendExact() {
		tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_BLEND);
		int count;
		const byte *values = lerpValues(count);
		for (int i = 0; i < count; i++) {
			for (int j = 0; j < count; j++) {
				for (int k = 0; k < count; k++) {
					const byte p = values[i], c = values[j], t = values[k];
					tglColor4ub(p, 255 - p, p, 255);
					setConstant(c, 255 - c, 255, 0);
					drawPixel(t, t, 255 - t, 255);
					checkOutput(exactLerp(p, c, t), exactLerp(255 - p, 255 - c, t), exactLerp(p, 255, 255 - t), 255);
				}
			}
		}
	}

    void setCombineMode(TGLuint combineRGB, TGLuint combineAlpha) {
        tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_COMBINE);
        tglTexEnvi(TGL_TEXTURE_ENV, TGL_COMBINE_RGB, combineRGB);
        tglTexEnvi(TGL_TEXTURE_ENV, TGL_COMBINE_ALPHA, combineAlpha);
    }

    void setCombineArg(int arg, TGLuint rgbSource, TGLuint rgbOperand, TGLuint alphaSource, TGLuint alphaOperand) {
        assert(arg >= 0 && arg <= 1);
        tglTexEnvi(TGL_TEXTURE_ENV, arg ? TGL_SOURCE1_RGB : TGL_SOURCE0_RGB, rgbSource);
        tglTexEnvi(TGL_TEXTURE_ENV, arg ? TGL_SOURCE1_ALPHA : TGL_SOURCE0_ALPHA, alphaSource);
        tglTexEnvi(TGL_TEXTURE_ENV, arg ? TGL_OPERAND1_RGB : TGL_OPERAND0_RGB, rgbOperand);
        tglTexEnvi(TGL_TEXTURE_ENV, arg ? TGL_OPERAND1_ALPHA : TGL_OPERAND0_ALPHA, alphaOperand);
    }

    void testCombineArgTexture() {
        setCombineMode(TGL_REPLACE, TGL_REPLACE);
        setCombineArg(0, TGL_TEXTURE, TGL_SRC_COLOR, TGL_TEXTURE, TGL_SRC_ALPHA);
        drawPixel(13, 37, 42, 24);
        checkOutput(13, 37, 42, 24);
    }

    void testCombineArgPrimaryColor() {
        setCombineMode(TGL_REPLACE, TGL_REPLACE);
        setCombineArg(0, TGL_PRIMARY_COLOR, TGL_SRC_COLOR, TGL_PRIMARY_COLOR, TGL_SRC_ALPHA);
        tglColor4ub(12, 34, 56, 78);
        drawPixel(13, 37, 42, 24);
        checkOutput(12, 34, 56, 78);
    }

    void testCombineArgConstant() {
        setCombineMode(TGL_REPLACE, TGL_REPLACE);
        setCombineArg(0, TGL_CONSTANT, TGL_SRC_COLOR, TGL_CONSTANT, TGL_SRC_ALPHA);
        setConstant(12, 34, 56, 78);
        drawPixel(13, 37, 42, 24);
        checkOutput(12, 34, 56, 78);
    }

	void testCombineArgMixed() {
		setCombineMode(TGL_REPLACE, TGL_REPLACE);
		setCombineArg(0, TGL_PRIMARY_COLOR, TGL_SRC_ALPHA, TGL_TEXTURE, TGL_SRC_ALPHA);
		tglColor4ub(12, 34, 56, 78);
		drawPixel(13, 37, 42, 24);
		checkOutput(78, 78, 78, 24);
	}

    void testCombineOpOneMinus() {
        setCombineMode(TGL_REPLACE, TGL_REPLACE);
        setCombineArg(0, TGL_TEXTURE, TGL_ONE_MINUS_SRC_COLOR, TGL_TEXTURE, TGL_ONE_MINUS_SRC_ALPHA);
        drawPixel(13, 37, 42, 24);
        checkOutput(242, 218, 213, 231);
    }

    void testCombineOpSrcAlpha() {
        setCombineMode(TGL_REPLACE, TGL_REPLACE);
        setCombineArg(0, TGL_TEXTURE, TGL_SRC_ALPHA, TGL_TEXTURE, TGL_ONE_MINUS_SRC_ALPHA);
        drawPixel(13, 37, 42, 24);
        checkOutput(24, 24, 24, 231);
    }

    void testCombineReplace() {
        setCombineMode(TGL_REPLACE, TGL_REPLACE);
        setCombineArg(0, TGL_TEXTURE, TGL_SRC_COLOR, TGL_TEXTURE, TGL_SRC_ALPHA);
        setCombineArg(1, TGL_PRIMARY_COLOR, TGL_SRC_COLOR, TGL_PRIMARY_COLOR, TGL_SRC_ALPHA);
        tglColor4ub(12, 34, 56, 78); // just to confuse the implementation
        drawPixel(13, 37, 42, 24);
        checkOutput(13, 37, 42, 24);
    }

    void testCombineModulate() {
        setCombineMode(TGL_MODULATE, TGL_MODULATE);
        setCombineArg(0, TGL_TEXTURE, TGL_SRC_COLOR, TGL_TEXTURE, TGL_SRC_ALPHA);
        setCombineArg(1, TGL_PRIMARY_COLOR, TGL_SRC_COLOR, TGL_PRIMARY_COLOR, TGL_SRC_ALPHA);
        tglColor4ub(255, 255, 255, 127);
        drawPixel(255, 127, 0, 255);
        checkOutput(255, 127, 0, 127);
    }

    void testCombineAdd() {
        setCombineMode(TGL_ADD, TGL_ADD);
        setCombineArg(0, TGL_TEXTURE, TGL_SRC_COLOR, TGL_TEXTURE, TGL_SRC_ALPHA);
        setCombineArg(1, TGL_PRIMARY_COLOR, TGL_SRC_COLOR, TGL_PRIMARY_COLOR, TGL_SRC_ALPHA);
        tglColor4ub(50, 100, 150, 100);
        drawPixel(25, 50, 150, 200);
        checkOutput(75, 150, 255, 255);
        // attention: TGL_COMBINE TGL_ADD does *not* modulate alpha
    }

	void testSaveTexEnvState() {
		tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_REPLACE);
		tglColor4ub(0, 0, 0, 0);
		drawPixel(255, 127, 0, 255);
		tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_MODULATE); // before executing the drawcall
		checkOutput(255, 127, 0, 255);
	}
};

#endif
