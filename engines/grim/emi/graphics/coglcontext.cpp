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

#include "common/scummsys.h"

#if defined(USE_OPENGL_GAME)

#include "common/system.h"
#include "common/textconsole.h"

#include "engines/util.h"

#include "graphics/opengl/context.h"
#include "graphics/surface.h"

#include "engines/grim/emi/graphics/coglcontext.h"
#include "engines/grim/emi/graphics/cogltexture.h"
#include "engines/grim/emi/graphics/coglvertexarray.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/core/profilertimer.h"

namespace Grim {

static const float kPi = 3.1415927f;
static const double kDegreesToHalfRadians = 0.002777777777777778;
static const uint kRawReadWidth = 640;
static const uint kRawReadHeight = 480;

COGLContext::COGLContext(EmiEngine *engine) : CDrawContext(engine) {
	_vertexColors = nullptr;
	_extensionsQueried = false;
	_hasPackedPixels = false;
	_currentVertexArray = nullptr;
	_numClipPlanes = 0;
	_swapCount = 0;
	_depthTestEnabled = false;
	_blendEnabled = false;
	_alphaTestEnabled = false;
	_shadeModel = 0;
	_texture2dEnabled = false;
	_texEnvMode = 0;
	_frameCopy = nullptr;
	_gammaFrame = nullptr;
	_frameSize = 0;
	_gammaActive = false;

	for (int i = 0; i < 256; i++)
		_gammaRamp[i] = i * 257;
}

COGLContext::~COGLContext() {
	close();
}

bool COGLContext::open(const DisplayMode *mode) {
	_displayMode = *mode;

	initGraphics3d(_displayMode.width, _displayMode.height);

	if (OpenGLContext.type != OpenGL::kContextGL) {
		warning("COGLContext::open(): the OpenGL context is not available");

		return false;
	}

	_frameSize = _displayMode.width * _displayMode.height * 3;
	delete[] _frameCopy;
	delete[] _gammaFrame;
	_frameCopy = new byte[_frameSize]();
	_gammaFrame = new byte[_frameSize]();

	setViewport(0, 0, _displayMode.width, _displayMode.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1.0f, 1.0f, -1.0f);

	queryGLInfo();
	checkExtensions();
	initGLState();

	// The frame buffer is never written with alpha later, and the ScummVM backend may alpha test it when it blits the frame
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	_drawListA._ctx = this;
	_drawListB._ctx = this;

	return true;
}

bool COGLContext::close() {
	_gammaActive = false;

	delete[] _frameCopy;
	_frameCopy = nullptr;
	delete[] _gammaFrame;
	_gammaFrame = nullptr;
	_frameSize = 0;

	return true;
}

bool COGLContext::beginScene() {
	bool result = CDrawContext::beginScene();

	// Not in the disasm, the window of the original has no alpha: the vertex alpha must not reach the framebuffer
	if (result)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

	return result;
}

bool COGLContext::endScene() {
	bool result = CDrawContext::endScene();

	if (result) {
		glFinish();

		return true;
	}

	return result;
}

int COGLContext::present(bool vsync) {
	captureFrame();

	if (_gammaActive)
		applyGamma();

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	g_system->updateScreen();

	if (_gammaActive)
		restoreFrame();

	_swapCount++;

	return _swapCount;
}

void COGLContext::clear(bool color, bool depth) {
	GLboolean mask[4];

	glGetBooleanv(GL_COLOR_WRITEMASK, mask);

	// The cleared alpha is opaque, as the framebuffer has no alpha in the original
	if (color) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}

	glClear((depth ? GL_DEPTH_BUFFER_BIT : 0) | (color ? GL_COLOR_BUFFER_BIT : 0));
	glColorMask(mask[0], mask[1], mask[2], mask[3]);
}

void COGLContext::setCameraMatrix(const float *matrix) {
	GLfloat m[16];

	if (_engine->isDemo()) {
		double determinant = ((double)matrix[1] * matrix[5] - (double)matrix[4] * matrix[2]) * matrix[6] +
			((double)matrix[4] * matrix[0] - (double)matrix[3] * matrix[1]) * matrix[8] +
			((double)matrix[3] * matrix[2] - (double)matrix[0] * matrix[5]) * matrix[7];

		glFrontFace(determinant >= 0.0 ? GL_CCW : GL_CW);
	} else {
		glFrontFace(GL_CW);
	}

	glMatrixMode(GL_MODELVIEW);

	m[3] = 0.0f;
	m[7] = 0.0f;
	m[11] = 0.0f;
	m[0] = matrix[0];
	m[1] = matrix[1];
	m[2] = -matrix[2];
	m[4] = matrix[3];
	m[5] = matrix[4];
	m[6] = -matrix[5];
	m[8] = matrix[6];
	m[9] = matrix[7];
	m[15] = 1.0f;
	m[10] = -matrix[8];
	m[14] = -matrix[11];
	m[12] = matrix[9];
	m[13] = matrix[10];

	glLoadMatrixf(m);
}

void COGLContext::resetCameraMatrix() {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1.0f, 1.0f, -1.0f);
}

void COGLContext::setViewport(int x, int y, int width, int height) {
	glViewport(x, y, width, height);
}

void COGLContext::setPerspective(float fovX, float fovY, float nearZ, float farZ) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	double top = tan((double)kPi * fovY * kDegreesToHalfRadians) * nearZ;
	double right = tan((double)kPi * fovX * kDegreesToHalfRadians) * nearZ;

	glFrustum(-right, right, -top, top, nearZ, farZ);
}

void COGLContext::setOrtho(float left, float right, float bottom, float top) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(left, right, bottom, top, 0.0, 1.0);
}

void COGLContext::setGamma(float gamma) {
	if (!_hasGammaCtl)
		return;

	for (int i = 0; i < 256; i++) {
		double value = pow((double)i * 0.00392156862745098, (double)gamma) * 65535.0 + 0.5;

		if (value > 65535.0)
			value = 65535.0;

		_gammaRamp[i] = (uint16)(int64)value;
	}

	_gammaActive = gamma != 1.0f;
}

int COGLContext::getMaxClipPlanes() {
	GLint planes = 0;

	glGetIntegerv(GL_MAX_CLIP_PLANES, &planes);

	return planes;
}

void COGLContext::addClipPlane(float a, float b, float c, float d) {
	GLdouble equation[4];
	GLenum plane = GL_CLIP_PLANE0 + _numClipPlanes;

	equation[0] = a;
	equation[1] = b;
	equation[2] = c;
	equation[3] = d;
	_numClipPlanes++;

	glClipPlane(plane, equation);
	glEnable(plane);
}

void COGLContext::clearClipPlanes() {
	for (int i = 0; i < _numClipPlanes; i++)
		glDisable(GL_CLIP_PLANE0 + i);

	_numClipPlanes = 0;
}

void COGLContext::submitVertices(CDCVertexArray *vertexArray) {
	_engine->_oglSubmitVerticesTimer.start();

	CDCVertex *vertices = (CDCVertex *)vertexArray->lock();

	glVertexPointer(3, GL_FLOAT, sizeof(CDCVertex), vertices);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	_vertexColors = (byte *)_engine->getMemoryServices()->mallocImpl(vertexArray->_numVerts * 4 * sizeof(byte));

	if (_vertexColors) {
		for (int i = 0; i < vertexArray->_numVerts; i++) {
			uint32 color = vertices[i].color;

			_vertexColors[4 * i] = (color >> 16) & 0xFF;
			_vertexColors[4 * i + 1] = (color >> 8) & 0xFF;
			_vertexColors[4 * i + 2] = color & 0xFF;
			_vertexColors[4 * i + 3] = color >> 24;
		}
	}

	glColorPointer(4, GL_UNSIGNED_BYTE, 0, _vertexColors);
	glTexCoordPointer(2, GL_FLOAT, sizeof(CDCVertex), &vertices->u);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	_currentVertexArray = vertexArray;
	_counter8 += vertexArray->_numVerts;

	_engine->_oglSubmitVerticesTimer.stop();
}

void COGLContext::releaseVertices() {
	if (!_currentVertexArray) {
		warning("COGLContext::releaseVertices(): no vertex array submitted");

		return;
	}

	_currentVertexArray->unlock();
	_engine->getMemoryServices()->freeImpl(_vertexColors);
	_vertexColors = nullptr;

	// The ScummVM backend pipelines do not set these pointers, so leaving the arrays enabled makes them read freed memory
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void COGLContext::drawTriangles(int flags, CTexture *tex, int numIndices, const uint16 *indices) {
	_engine->_oglDrawTrianglesTimer.start();

	if (numIndices) {
		setPolyOptions(flags);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
		setTextures(tex, flags);

		_engine->_oglDrawElementsTimer.start();
		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, indices);
		_engine->_oglDrawElementsTimer.stop();

		_trianglesDrawn += numIndices / 3;
	}

	_engine->_oglDrawTrianglesTimer.stop();
}

void COGLContext::drawQuads(int flags, CTexture *tex, int first, int count, const VertexStreams *streams) {
	_engine->_oglDrawQuadsTimer.start();

	if (count) {
		setPolyOptions(flags);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);

		if (tex)
			setTextures(tex, flags);
		else
			setTextures(nullptr, 0);

		glBegin(GL_QUADS);

		for (int i = first; i < first + count; i++) {
			if (streams->uvs)
				glTexCoord2f(streams->uvs[2 * i], streams->uvs[2 * i + 1]);

			if (streams->colors) {
				uint32 color = streams->colors[i];

				glColor4ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, color >> 24);
			} else {
				glColor4ub(255, 255, 255, 255);
			}

			glVertex3fv(&streams->positions[3 * i]);
		}

		glEnd();

		_trianglesDrawn += count / 4;
	}

	_engine->_oglDrawQuadsTimer.stop();
}

int COGLContext::outputDrawPoly(DrawPoly *poly) {
	_engine->_oglOutputDrawPolyTimer.start();

	poly->flags &= 0xFFFFFF0F;

	if (poly->tex)
		poly->flags |= 0x20;
	else
		poly->flags |= 0x10;

	setPolyOptions(poly->flags);

	if (poly->flags & 0x10) {
		setTextures(nullptr, poly->flags);
		glBegin(GL_TRIANGLE_FAN);

		for (int i = 0; i < poly->numVerts; i++) {
			DrawVertex *vertex = poly->verts[i];

			if ((poly->flags & 3) == 0) {
				uint32 color = vertex->color;

				glColor4ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, color >> 24);
			}

			glVertex3fv(&vertex->x);
		}
	} else {
		setTextures(poly->tex, poly->flags);
		glBegin(GL_TRIANGLE_FAN);

		for (int i = 0; i < poly->numVerts; i++) {
			DrawVertex *vertex = poly->verts[i];

			glTexCoord2f(vertex->u, vertex->v);

			if ((poly->flags & 3) == 0) {
				uint32 color = vertex->color;

				glColor4ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, color >> 24);
			}

			glVertex3fv(&vertex->x);
		}
	}

	glEnd();
	_trianglesDrawn++;

	_engine->_oglOutputDrawPolyTimer.stop();

	return 1;
}

void COGLContext::drawPrim(int flags, DrawVertex *verts, int count, bool filled) {
	setPolyOptions(flags);
	setTextures(nullptr, flags);

	glBegin(filled ? GL_POLYGON : GL_LINE_LOOP);

	for (int i = 0; i < count; i++) {
		glVertex3fv(&verts[i].x);

		if ((flags & 3) == 0) {
			uint32 color = verts[i].color;

			glColor4ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, color >> 24);
		}
	}

	glEnd();
}

void COGLContext::drawPrim2D(int flags, DrawVertex *verts, int count, bool filled) {
	glDisable(GL_CULL_FACE);
	setOrtho(-1.0f, 1.0f, -1.0f, 1.0f);
	drawPrim(flags, verts, count, filled);
}

void COGLContext::setColor(const Color *color) {
	glColor4ub(color->_r, color->_g, color->_b, color->_a);
}

void COGLContext::setPolyOptions(int flags) {
	_engine->_oglSetPolyOptionsTimer.start();

	if (flags & 0x100) {
		if (!_depthTestEnabled) {
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LEQUAL);
			_depthTestEnabled = true;
		}
	} else if (_depthTestEnabled) {
		glDisable(GL_DEPTH_TEST);
		_depthTestEnabled = false;
	}

	if ((flags & 3) != 0 && _shadeModel != 1) {
		_shadeModel = 1;
		glShadeModel(GL_FLAT);
	}

	if (flags & 1)
		glColor3ub(255, 255, 255);

	if ((flags & 4) != 0 && _shadeModel != 2) {
		_shadeModel = 2;
		glShadeModel(GL_SMOOTH);
	}

	if (flags & 0x10000) {
		if (!_blendEnabled) {
			glEnable(GL_BLEND);
			_blendEnabled = true;
		}

		if (_blendMode != 1) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			_blendMode = 1;
		}
	} else if (_blendEnabled) {
		glDisable(GL_BLEND);
		_blendEnabled = false;
	}

	if (flags & 0x20000) {
		if (!_alphaTestEnabled) {
			glEnable(GL_ALPHA_TEST);
			_alphaTestEnabled = true;
		}
	} else if (_alphaTestEnabled) {
		glDisable(GL_ALPHA_TEST);
		_alphaTestEnabled = false;
	}

	_polyOptions = flags;

	_engine->_oglSetPolyOptionsTimer.stop();
}

void COGLContext::setTextures(CTexture *tex, int flags) {
	_engine->_oglSetTexturesTimer.start();

	if (!tex) {
		_texture2dEnabled = false;
		glDisable(GL_TEXTURE_2D);
		_engine->_oglSetTexturesTimer.stop();

		return;
	}

	uint textureFlags = tex->_flags;

	glBindTexture(GL_TEXTURE_2D, static_cast<COGLTexture *>(tex)->_glId);

	if (_engine->isDemo() && (flags & 1)) {
		if (_texEnvMode != 1) {
			_texEnvMode = 1;
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
	} else if (_texEnvMode != 2) {
		_texEnvMode = 2;
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	if ((textureFlags & 0x80) || (flags & 0x10000)) {
		if (!_blendEnabled) {
			glEnable(GL_BLEND);
			_blendEnabled = true;
		}

		if (_blendMode != 1) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			_blendMode = 1;
		}
	}

	if (textureFlags & 0x400) {
		if (!_blendEnabled) {
			glEnable(GL_BLEND);
			_blendEnabled = true;
		}

		if (_blendMode != 4) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			_blendMode = 4;
		}
	}

	if (!_texture2dEnabled) {
		_texture2dEnabled = true;
		glEnable(GL_TEXTURE_2D);
	}

	_engine->_oglSetTexturesTimer.stop();
}

void COGLContext::readBackBuffer(LABitmap *bitmap) {
	int width = _displayMode.width;
	int height = _displayMode.height;
	LColorFormat format(6);

	bitmap->create(width, height, format, nullptr);

	if (!_frameCopy) {
		warning("COGLContext::readBackBuffer(): no frame available");

		return;
	}

	byte *destination = (byte *)bitmap->getPixels();

	for (int row = 0; row < height; row++)
		memcpy(destination + row * bitmap->getStride(), _frameCopy + (height - 1 - row) * width * 3, width * 3);
}

void COGLContext::readBackBufferRaw(char *dest, uint size) {
	uint bytesPerPixel = _displayMode.bpp >> 3;

	if (!_frameCopy || _displayMode.width < (int)kRawReadWidth || _displayMode.height < (int)kRawReadHeight) {
		warning("COGLContext::readBackBufferRaw(): no frame available");

		return;
	}

	if (size < kRawReadWidth * kRawReadHeight * bytesPerPixel) {
		warning("COGLContext::readBackBufferRaw(): buffer too small");

		return;
	}

	for (int y = kRawReadHeight - 1; y >= 0; y--) {
		const byte *source = _frameCopy + y * _displayMode.width * 3;
		byte *destination = (byte *)dest;

		for (uint x = 0; x < kRawReadWidth; x++) {
			byte red = source[3 * x];
			byte green = source[3 * x + 1];
			byte blue = source[3 * x + 2];

			if (bytesPerPixel == 4) {
				destination[4 * x] = red;
				destination[4 * x + 1] = green;
				destination[4 * x + 2] = blue;
				destination[4 * x + 3] = 255;
			} else {
				uint16 packed = (((red * 31 + 127) / 255) << 11) | (((green * 63 + 127) / 255) << 5) | ((blue * 31 + 127) / 255);

				memcpy(destination + 2 * x, &packed, sizeof(packed));
			}
		}

		dest += kRawReadWidth * bytesPerPixel;
	}
}

void COGLContext::copyFrontToBack() {
	setOrtho(-1.0f, 1.0f, -1.0f, 1.0f);
	setPolyOptions(18);
	setTextures(nullptr, 18);

	if (!_frameCopy) {
		warning("COGLContext::copyFrontToBack(): no frame available");

		return;
	}

	drawFrame(_frameCopy);
}

CDCVertexArray *COGLContext::createVertexArray(int numVerts, const float *positions, const uint32 *colors, const float *uvs) {
	return new COGLVertexArray(numVerts, positions, colors, uvs);
}

CDCVertexArray *COGLContext::createVertexArray(const VertexStreams *streams) {
	return new COGLVertexArray(streams);
}

CDCVertexArray *COGLContext::createVertexArray(CDCVertexArray *source) {
	return new COGLVertexArray(source);
}

CTexture *COGLContext::createTexture(LABitmap *bitmap) {
	if (!bitmap)
		return nullptr;

	int width = bitmap->getWidth();

	if (((width - 1) & bitmap->getWidth()) != 0) {
		if (_engine->getConsoleServices())
			_engine->getConsoleServices()->logError("Texture not a power of two in width: %s\n", bitmap->_bitmapName.c_str());
		else
			warning("COGLContext::createTexture(): texture not a power of two in width: %s", bitmap->_bitmapName.c_str());

		return _defaultTexture;
	}

	int height = bitmap->getHeight();

	if (((height - 1) & bitmap->getHeight()) != 0) {
		if (_engine->getConsoleServices())
			_engine->getConsoleServices()->logError("Texture not a power of two in height: %s\n", bitmap->_bitmapName.c_str());
		else
			warning("COGLContext::createTexture(): texture not a power of two in height: %s", bitmap->_bitmapName.c_str());

		return _defaultTexture;
	}

	if (width == 0 || height == 0) {
		// The original could divide by zero here, let's avoid that...
		warning("COGLContext::createTexture(): empty texture: %s", bitmap->_bitmapName.c_str());

		return _defaultTexture;
	}

	if ((uint)width / (uint)height > 8 || (uint)height / (uint)width > 8) {
		if (_engine->getConsoleServices())
			_engine->getConsoleServices()->logError("Texture aspect greater than 8:1 : %s\n", bitmap->_bitmapName.c_str());
		else
			warning("COGLContext::createTexture(): texture aspect greater than 8:1: %s", bitmap->_bitmapName.c_str());

		return _defaultTexture;
	}

	return new COGLTexture(this, bitmap, _textureFlag);
}

CTexture *COGLContext::createTexture(int width, int height, const LColorFormat &format, byte *pixels, const char *name) {
	LABitmap bitmap(_engine, width, height, format, pixels, true);

	return createTexture(&bitmap);
}

int COGLContext::getTextureCost(CTexture *tex) {
	if (!tex || tex == _defaultTexture)
		return 0;

	return sizeof(COGLTexture);
}

void COGLContext::prepareDialog() {
	if (_swapCount & 1)
		present(false);
}

bool COGLContext::drawMovieFrame(const Graphics::Surface &frame) {
	if (!frame.getPixels() || frame.w <= 0 || frame.h <= 0) {
		warning("COGLContext::drawMovieFrame(): no frame");

		return false;
	}

	const byte *pixels = (const byte *)frame.getPixels();
	byte *converted = nullptr;
	int rowLength = frame.pitch / 4;

	if (frame.format != getMovieFormat()) {
		converted = new byte[frame.w * frame.h * 4];
		rowLength = frame.w;

		for (int y = 0; y < frame.h; y++) {
			for (int x = 0; x < frame.w; x++) {
				uint8 red;
				uint8 green;
				uint8 blue;
				byte *destination = converted + (y * frame.w + x) * 4;

				frame.format.colorToRGB(frame.getPixel(x, y), red, green, blue);
				destination[0] = red;
				destination[1] = green;
				destination[2] = blue;
				destination[3] = 255;
			}
		}

		pixels = converted;
	}

	setOrtho(-1.0f, 1.0f, -1.0f, 1.0f);
	resetCameraMatrix();
	setPolyOptions(18);
	setTextures(nullptr, 18);

	// The frame is drawn from its top left corner, down the screen
	glRasterPos2f(-1.0f, 1.0f);
	glPixelZoom(1.0f, -1.0f);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
	glDrawPixels(frame.w, frame.h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelZoom(1.0f, 1.0f);

	delete[] converted;

	return true;
}

void COGLContext::initGLState() {
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0);
	glDepthFunc(GL_LEQUAL);
	glAlphaFunc(GL_GEQUAL, 0.1f);

	_depthTestEnabled = true;
	_blendEnabled = false;
	_alphaTestEnabled = false;
	_blendMode = 0;
	_shadeModel = 2;
	glShadeModel(GL_SMOOTH);
	_texture2dEnabled = false;
	_texEnvMode = 0;
	_hasGammaCtl = true;
}

void COGLContext::queryGLInfo() {
	const char *text = (const char *)glGetString(GL_RENDERER);

	if (text)
		_glRenderer = text;

	text = (const char *)glGetString(GL_VENDOR);

	if (text)
		_glVendor = text;

	text = (const char *)glGetString(GL_VERSION);

	if (text)
		_glVersion = text;

	text = (const char *)glGetString(GL_EXTENSIONS);

	if (text)
		_glExtensions = text;
}

void COGLContext::checkExtensions() {
	const char *extensions = (const char *)glGetString(GL_EXTENSIONS);

	if (extensions) {
		_extensionsQueried = true;

		if (strstr(extensions, "GL_EXT_packed_pixels"))
			_hasPackedPixels = true;
	}
}

void COGLContext::captureFrame() {
	if (!_frameCopy)
		return;

	g_system->presentBuffer();

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, _displayMode.width, _displayMode.height, GL_RGB, GL_UNSIGNED_BYTE, _frameCopy);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
}

void COGLContext::applyGamma() {
	if (!_frameCopy || !_gammaFrame)
		return;

	for (uint i = 0; i < _frameSize; i++)
		_gammaFrame[i] = _gammaRamp[_frameCopy[i]] >> 8;

	drawFlatFrame(_gammaFrame);
}

void COGLContext::restoreFrame() {
	if (!_frameCopy)
		return;

	drawFlatFrame(_frameCopy);
}

void COGLContext::drawFlatFrame(const byte *rgbPixels) {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);

	drawFrame(rgbPixels);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

void COGLContext::drawFrame(const byte *rgbPixels) {
	glRasterPos2f(-1.0f, -1.0f);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glDrawPixels(_displayMode.width, _displayMode.height, GL_RGB, GL_UNSIGNED_BYTE, rgbPixels);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

} // End of namespace Grim

#endif
