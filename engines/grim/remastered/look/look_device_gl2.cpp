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

#include "engines/grim/std/std_3d_config.h"

#include "engines/grim/remastered/look/look_device_gl2.h"

#if defined(GRIM_STD3D_GL)

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "graphics/opengl/context.h"
#include "graphics/opengl/system_headers.h"

#include "engines/grim/remastered/look/look_device_gl2_types.h"
#include "engines/grim/remastered/look/look_glsl.h"
#include "engines/grim/remastered/look/look_programs.h"
#include "engines/grim/remastered/look/s3tc.h"

#include <string.h>

namespace Grim {

static const int depthFuncTable[6] = { GL_ALWAYS, GL_LESS, GL_LEQUAL, GL_EQUAL, GL_GEQUAL, GL_GREATER };

RemasteredLookDeviceGl2::RemasteredLookDeviceGl2() : _open(false), _hostWidth(0), _hostHeight(0), _hostFramebuffer(0), _hostProgram(0),
		_hostArrayBuffer(0), _hostElementBuffer(0), _framebuffer(0), _targetsDirty(true), _depthFunc(kLookDepthAlways),
		_depthWrite(true), _cullMode(kLookCullNone), _currentProgram(0), _enabledAttributes(0), _depthStateKnown(false),
		_frameActive(false), _incompleteReported(false), _abandoned(false), _sentinel(0) {
	memset(_hostViewport, 0, sizeof(_hostViewport));
	memset(_physical, 0, sizeof(_physical));

	for (int i = 0; i < 4; i++)
		_colorMask[i] = true;
}

RemasteredLookDeviceGl2::~RemasteredLookDeviceGl2() {
	close();
}

void RemasteredLookDeviceGl2::setPhysical(LookFormat format, int internalFormat, int glFormat, int type) {
	_physical[format].internalFormat = internalFormat;
	_physical[format].format = glFormat;
	_physical[format].type = type;
	_physical[format].valid = true;
}

void RemasteredLookDeviceGl2::setFormats() {
	setPhysical(kLookFormatRgba8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
	setPhysical(kLookFormatFloat1x32, GL_R32F, GL_RED, GL_FLOAT);
	setPhysical(kLookFormatFloat1x16, GL_R16F, GL_RED, GL_FLOAT);
	setPhysical(kLookFormatFloat2x16, GL_RG16F, GL_RG, GL_FLOAT);
	setPhysical(kLookFormatFloat4x16, GL_RGBA16F, GL_RGBA, GL_FLOAT);
}

bool RemasteredLookDeviceGl2::findShaderDirectory() {
	Common::FSNode game(ConfMan.getPath("path"));
	Common::FSNode compiled = game.getChild("x86").getChild("shaders").getChild("compiled");

	if (!compiled.exists() || !compiled.isDirectory()) {
		warning("RemasteredLookDeviceGl2::findShaderDirectory(): the shader folder x86/shaders/compiled was not found in the game folder");
		return false;
	}

	_shaderDirectory = compiled;

	return true;
}

bool RemasteredLookDeviceGl2::loadSource(const Common::String &name, Common::String &source) {
	SourceMap::iterator it = _sources.find(name);

	if (it != _sources.end()) {
		source = it->_value;

		return true;
	}

	Common::FSNode file = _shaderDirectory.getChild(name + ".glsl");
	Common::SeekableReadStream *stream = file.createReadStream();

	if (!stream) {
		warning("RemasteredLookDeviceGl2::loadSource(): cannot open shader %s", name.c_str());
		return false;
	}

	uint size = (uint)stream->size();
	char *buffer = new char[size + 1];
	uint read = stream->read(buffer, size);

	buffer[read] = '\0';
	delete stream;

	source = Common::String(buffer, read);
	delete[] buffer;
	_sources[name] = source;

	return true;
}

uint RemasteredLookDeviceGl2::compileShader(int stage, const Common::String &name, const Common::String &text) {
	GLuint shader = glCreateShader(stage == kLookStageVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
	const char *sourceText = text.c_str();

	glShaderSource(shader, 1, &sourceText, nullptr);
	glCompileShader(shader);

	GLint status = 0;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	if (!status) {
		GLint length = 0;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

		Common::String log;

		if (length > 1) {
			char *buffer = new char[length + 1];

			glGetShaderInfoLog(shader, length, nullptr, buffer);
			buffer[length] = '\0';
			log = buffer;
			delete[] buffer;
		}

		warning("RemasteredLookDeviceGl2::compileShader(): %s did not compile: %s", name.c_str(), log.c_str());
		glDeleteShader(shader);

		return 0;
	}

	return shader;
}

RemasteredLookDeviceGl2::Program *RemasteredLookDeviceGl2::getProgram(int program) {
	uint32 key = (uint32)program;
	ProgramMap::iterator it = _programs.find(key);

	if (it != _programs.end())
		return it->_value;

	Common::String vertexName = RemasteredLookPrograms::getVertexName(program);
	Common::String pixelName = RemasteredLookPrograms::getPixelName(program);
	Common::String vertexSource;
	Common::String pixelSource;

	if (!loadSource(vertexName, vertexSource) || !loadSource(pixelName, pixelSource)) {
		_programs[key] = nullptr;
		return nullptr;
	}

	Common::String vertexText;
	Common::String pixelText;

	if (!RemasteredLookGlsl::translateVertex(vertexSource, vertexText) ||
			!RemasteredLookGlsl::translatePixel(pixelSource, RemasteredLookPrograms::getOutputKind(program), pixelText)) {
		warning("RemasteredLookDeviceGl2::getProgram(): cannot translate %s / %s", vertexName.c_str(), pixelName.c_str());
		_programs[key] = nullptr;
		return nullptr;
	}

	GLuint vertexShader = compileShader(kLookStageVertex, vertexName, vertexText);
	GLuint pixelShader = compileShader(kLookStagePixel, pixelName, pixelText);

	if (!vertexShader || !pixelShader) {
		if (vertexShader)
			glDeleteShader(vertexShader);

		if (pixelShader)
			glDeleteShader(pixelShader);

		_programs[key] = nullptr;
		return nullptr;
	}

	GLuint id = glCreateProgram();

	glAttachShader(id, vertexShader);
	glAttachShader(id, pixelShader);
	glBindAttribLocation(id, 0, "vs_Position");
	glBindAttribLocation(id, 0, "vs_PositionPx");
	glBindAttribLocation(id, 1, "vs_Normal");
	glBindAttribLocation(id, 2, "vs_TexCoord");
	glLinkProgram(id);
	glDeleteShader(vertexShader);
	glDeleteShader(pixelShader);

	GLint status = 0;

	glGetProgramiv(id, GL_LINK_STATUS, &status);

	if (!status) {
		warning("RemasteredLookDeviceGl2::getProgram(): %s / %s did not link", vertexName.c_str(), pixelName.c_str());
		glDeleteProgram(id);
		_programs[key] = nullptr;
		return nullptr;
	}

	Program *result = new Program();

	result->id = id;
	result->vertexBinding.layout = nullptr;
	result->pixelBinding.layout = nullptr;

	glUseProgram(id);

	for (int i = 0; i < kLookMaxTextures; i++) {
		Common::String name = Common::String::format("ps_Texture%d", i);
		GLint location = glGetUniformLocation(id, name.c_str());

		if (location >= 0)
			glUniform1i(location, i);
	}

	_currentProgram = id;
	_programs[key] = result;

	return result;
}

bool RemasteredLookDeviceGl2::precompilePrograms() {
	static const int deferredPrograms[] = {
		kLookProgramTexturedLit, kLookProgramUntexturedLit, kLookProgramSmoothLighting, kLookProgramCopy,
		kLookProgramSmoothLightingResolve, kLookProgramShadowMesh, kLookProgramShadow, kLookProgramZQuad565,
		kLookProgramDeferredZShape, kLookProgramTexturedQuad, kLookProgramTexturedQuad565Linear,
		kLookProgramCharLightGradient, kLookProgramCharLightPoint, kLookProgramCharLightSpot,
		kLookProgramCharLightSpotShadow, kLookProgramCharLightSpotShadowEnv, kLookProgramCharLightDirectional,
		kLookProgramCharLightDirectionalShadow, kLookProgramCharLightDirectionalShadowEnv, kLookProgramCharAo,
		kLookProgramCharResolve, kLookProgramDeferredResolve, kLookProgramPostProcess, kLookProgramTexturedGammaQuad,
		kLookProgramDeferredLightDirect, kLookProgramDeferredLightOmni, kLookProgramDeferredLightSpot
	};

	for (uint i = 0; i < ARRAYSIZE(deferredPrograms); i++) {
		if (!getProgram(deferredPrograms[i]))
			return false;
	}

	return true;
}

void RemasteredLookDeviceGl2::freePrograms() {
	for (ProgramMap::iterator it = _programs.begin(); it != _programs.end(); ++it) {
		if (it->_value) {
			if (!_abandoned)
				glDeleteProgram(it->_value->id);

			delete it->_value;
		}
	}

	_programs.clear();
	_sources.clear();
	_currentProgram = 0;
}

bool RemasteredLookDeviceGl2::open(int hostWidth, int hostHeight) {
	if (_open)
		return true;

	Common::String missing;

	if (OpenGLContext.type != OpenGL::kContextGL)
		missing += " desktop OpenGL context";

	if (!OpenGLContext.enginesShadersSupported || OpenGLContext.glslVersion < 120)
		missing += " GLSL 1.20";

	if (!OpenGLContext.framebufferObjectSupported)
		missing += " framebuffer objects";

	if (!OpenGLContext.isGLVersionOrHigher(3, 0))
		missing += " OpenGL 3.0 (floating point and RG render targets)";

	if (!glDrawBuffers || !glBlendEquationSeparate || !glBlendFuncSeparate || !glGenFramebuffers)
		missing += " OpenGL entry points (draw buffers, separate blending, framebuffers)";

	GLint framebuffer = 0;
	GLint drawBuffers = 0;

	if (missing.empty()) {
		glGetIntegerv(GL_MAX_DRAW_BUFFERS, &drawBuffers);

		if (drawBuffers < 3)
			missing += " three draw buffers";
	}

	if (!missing.empty()) {
		warning("RemasteredLookDeviceGl2::open(): the OpenGL context lacks:%s", missing.c_str());
		return false;
	}

	_hostWidth = hostWidth;
	_hostHeight = hostHeight;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);

	_hostFramebuffer = framebuffer;
	_caps = LookCaps();
	_caps.maxDrawBuffers = (uint)drawBuffers;
	_caps.maxTextureSize = OpenGLContext.maxTextureSize;
#if defined(USE_GLAD)
	_caps.compressedTextures = GLAD_GL_EXT_texture_compression_s3tc != 0;
#endif

	glGenFramebuffers(1, &_framebuffer);
	setFormats();

	if (!findShaderDirectory()) {
		glDeleteFramebuffers(1, &_framebuffer);
		_framebuffer = 0;
		return false;
	}

	GLint previousSentinelTexture = 0;

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousSentinelTexture);
	glGenTextures(1, &_sentinel);
	glBindTexture(GL_TEXTURE_2D, _sentinel);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSentinelWidth, kSentinelHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D, previousSentinelTexture);

	_open = true;

	if (!precompilePrograms()) {
		warning("RemasteredLookDeviceGl2::open(): the shaders could not be prepared");
		close();
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glUseProgram(0);
	_currentProgram = 0;
	debug(1, "RemasteredLookDeviceGl2: draw buffers %u, S3TC %d", _caps.maxDrawBuffers, _caps.compressedTextures);

	return true;
}

void RemasteredLookDeviceGl2::close() {
	if (!_open && !_framebuffer)
		return;

	freePrograms();
	flushDeferred();

	if (_framebuffer) {
		if (!_abandoned)
			glDeleteFramebuffers(1, &_framebuffer);

		_framebuffer = 0;
	}

	if (_sentinel) {
		if (!_abandoned)
			glDeleteTextures(1, &_sentinel);

		_sentinel = 0;
	}

	_abandoned = false;
	_open = false;
}

bool RemasteredLookDeviceGl2::isContextLost() {
	if (!_open || !_sentinel)
		return false;

	GLint previous = 0;
	GLint width = 0;
	GLint height = 0;

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

	if (glIsTexture(_sentinel)) {
		glBindTexture(GL_TEXTURE_2D, _sentinel);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	}

	glBindTexture(GL_TEXTURE_2D, previous);

	return width != kSentinelWidth || height != kSentinelHeight;
}

void RemasteredLookDeviceGl2::abandon() {
	_abandoned = true;
}

void RemasteredLookDeviceGl2::beginFrame() {
	if (_frameActive)
		return;

	GLint value = 0;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
	_hostFramebuffer = value;
	glGetIntegerv(GL_VIEWPORT, _hostViewport);
	glGetIntegerv(GL_CURRENT_PROGRAM, &value);
	_hostProgram = value;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
	_hostArrayBuffer = value;
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
	_hostElementBuffer = value;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glFrontFace(GL_CCW);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glActiveTexture(GL_TEXTURE0);

	_currentProgram = (uint)_hostProgram;
	_enabledAttributes = 0;
	_targetsDirty = true;
	_frameActive = true;
}

void RemasteredLookDeviceGl2::endFrame() {
	if (!_frameActive)
		return;

	flushDeferred();

	for (int i = 0; i < 4; i++)
		glDisableVertexAttribArray(i);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glBindFramebuffer(GL_FRAMEBUFFER, _hostFramebuffer);
	glViewport(_hostViewport[0], _hostViewport[1], _hostViewport[2], _hostViewport[3]);
	glUseProgram(_hostProgram);
	glBindBuffer(GL_ARRAY_BUFFER, _hostArrayBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _hostElementBuffer);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	_currentProgram = (uint)_hostProgram;
	_frameActive = false;
}

RemasteredLookTarget *RemasteredLookDeviceGl2::createTarget(uint width, uint height, LookFormat format) {
	RemasteredLookTargetGl *target = new RemasteredLookTargetGl();

	target->configure(format, width, height);

	if (format == kLookFormatDepth) {
		GLuint renderbuffer = 0;
		GLint previous = 0;

		glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);
		glGenRenderbuffers(1, &renderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, previous);
		target->renderbuffer = renderbuffer;

		return target;
	}

	if (!_physical[format].valid) {
		warning("RemasteredLookDeviceGl2::createTarget(): format %d is not available", (int)format);
		delete target;
		return nullptr;
	}

	RemasteredLookTextureGl *texture = new RemasteredLookTextureGl();
	GLint previous = 0;
	GLuint id = 0;

	texture->configure(format, width, height, 1, true);

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, _physical[format].internalFormat, width, height, 0, _physical[format].format, _physical[format].type, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glBindTexture(GL_TEXTURE_2D, previous);

	texture->id = id;
	texture->wrap = GL_CLAMP_TO_EDGE;
	texture->minFilter = GL_NEAREST;
	texture->magFilter = GL_NEAREST;
	target->texture = texture;

	return target;
}

RemasteredLookTexture *RemasteredLookDeviceGl2::getTargetTexture(RemasteredLookTarget *target) {
	RemasteredLookTargetGl *glTarget = static_cast<RemasteredLookTargetGl *>(target);

	return glTarget ? glTarget->texture : nullptr;
}

void RemasteredLookDeviceGl2::destroyTarget(RemasteredLookTarget *&target) {
	RemasteredLookTargetGl *glTarget = static_cast<RemasteredLookTargetGl *>(target);

	if (!glTarget)
		return;

	if (glTarget->texture) {
		GLuint id = glTarget->texture->id;

		if (!_abandoned)
			glDeleteTextures(1, &id);

		delete glTarget->texture;
	}

	if (glTarget->renderbuffer && !_abandoned) {
		GLuint renderbuffer = glTarget->renderbuffer;

		glDeleteRenderbuffers(1, &renderbuffer);
	}

	delete glTarget;
	target = nullptr;
	_targetsDirty = true;
}

void RemasteredLookDeviceGl2::uploadLevel(RemasteredLookTextureGl *texture, uint level, uint width, uint height, const byte *data, uint size, LookFormat format) {
	switch (format) {
	case kLookFormatRgba8:
		glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		break;
	case kLookFormatRgb565:
		glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
		break;
	case kLookFormatZ16:
		glTexImage2D(GL_TEXTURE_2D, level, GL_R16, width, height, 0, GL_RED, GL_UNSIGNED_SHORT, data);
		break;
	case kLookFormatDxt1:
	case kLookFormatDxt3:
	case kLookFormatDxt5: {
		static const int compressedFormats[3] = { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT };
		static const S3tc::Format decodeFormats[3] = { S3tc::kDxt1, S3tc::kDxt3, S3tc::kDxt5 };
		int index = (int)format - (int)kLookFormatDxt1;

		if (_caps.compressedTextures) {
			glCompressedTexImage2D(GL_TEXTURE_2D, level, compressedFormats[index], width, height, 0, size, data);
		} else {
			Common::Array<byte> decoded;

			decoded.resize((uint)width * height * 4);

			if (!S3tc::decodeLevel(decodeFormats[index], width, height, data, size, decoded.data())) {
				warning("RemasteredLookDeviceGl2::uploadLevel(): cannot decode the compressed level");
				memset(decoded.data(), 0, decoded.size());
			}

			glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, decoded.data());
			texture->uploadedAsRgba = true;
		}
		break;
	}
	default:
		warning("RemasteredLookDeviceGl2::uploadLevel(): unsupported texture format %d", (int)format);
		break;
	}
}

RemasteredLookTexture *RemasteredLookDeviceGl2::createTexture(const LookImage &image) {
	if (image.numLevels == 0 || image.numLevels > kLookMaxLevels)
		return nullptr;

	RemasteredLookTextureGl *texture = new RemasteredLookTextureGl();
	GLint previous = 0;
	GLuint id = 0;

	texture->configure(image.format, image.width, image.height, image.numLevels, false);

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, image.numLevels - 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	texture->id = id;

	while (glGetError() != GL_NO_ERROR) {
	}

	for (uint level = 0; level < image.numLevels; level++)
		uploadLevel(texture, level, image.levels[level].width, image.levels[level].height, image.levels[level].data, image.levels[level].size, image.format);

	GLenum uploadError = glGetError();

	if (uploadError != GL_NO_ERROR || !id)
		warning("RemasteredLookDeviceGl2::createTexture(): the upload of %ux%u (format %d, %u levels, texture %u) failed with error 0x%x", image.width,
				image.height, (int)image.format, image.numLevels, id, (uint)uploadError);

	glBindTexture(GL_TEXTURE_2D, previous);

	texture->wrap = GL_CLAMP_TO_EDGE;
	texture->minFilter = GL_NEAREST;
	texture->magFilter = GL_NEAREST;

	return texture;
}

void RemasteredLookDeviceGl2::updateTexture(RemasteredLookTexture *texture, const byte *pixels, uint width, uint height) {
	RemasteredLookTextureGl *glTexture = static_cast<RemasteredLookTextureGl *>(texture);

	if (!glTexture || glTexture->isRenderTarget())
		return;

	GLint previous = 0;

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
	glBindTexture(GL_TEXTURE_2D, glTexture->id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	while (glGetError() != GL_NO_ERROR) {
	}

	uploadLevel(glTexture, 0, width, height, pixels, 0, glTexture->getFormat());

	GLenum error = glGetError();

	if (error != GL_NO_ERROR)
		warning("RemasteredLookDeviceGl2::updateTexture(): the upload of %ux%u (format %d, texture %u) failed with error 0x%x", width, height,
				(int)glTexture->getFormat(), glTexture->id, (uint)error);

	glBindTexture(GL_TEXTURE_2D, previous);
}

void RemasteredLookDeviceGl2::destroyTexture(RemasteredLookTexture *&texture) {
	RemasteredLookTextureGl *glTexture = static_cast<RemasteredLookTextureGl *>(texture);

	if (!glTexture)
		return;

	if (_abandoned)
		delete glTexture;
	else
		_deferredTextures.push_back(glTexture);

	texture = nullptr;
}

RemasteredLookBuffer *RemasteredLookDeviceGl2::createVertexBuffer(LookVertexFormat format, const void *data, uint numVertices) {
	static const uint strides[kLookNumVertexFormats] = { 32, 12, 20, 20, 12 };
	RemasteredLookBufferGl *buffer = new RemasteredLookBufferGl();
	uint size = strides[format] * numVertices;

	buffer->configure(format, numVertices, false);
	buffer->data.resize(size);

	if (size)
		memcpy(buffer->data.data(), data, size);

	return buffer;
}

RemasteredLookBuffer *RemasteredLookDeviceGl2::createIndexBuffer(const uint16 *indices, uint count) {
	RemasteredLookBufferGl *buffer = new RemasteredLookBufferGl();

	buffer->configure(kLookVertexVector3, count, true);
	buffer->data.resize(count * 2);

	if (count)
		memcpy(buffer->data.data(), indices, count * 2);

	return buffer;
}

void RemasteredLookDeviceGl2::destroyBuffer(RemasteredLookBuffer *&buffer) {
	RemasteredLookBufferGl *glBuffer = static_cast<RemasteredLookBufferGl *>(buffer);

	if (glBuffer)
		_deferredBuffers.push_back(glBuffer);

	buffer = nullptr;
}

void RemasteredLookDeviceGl2::flushDeferred() {
	for (uint i = 0; i < _deferredTextures.size(); i++) {
		GLuint id = _deferredTextures[i]->id;

		if (!_abandoned)
			glDeleteTextures(1, &id);

		delete _deferredTextures[i];
	}

	_deferredTextures.clear();

	for (uint i = 0; i < _deferredBuffers.size(); i++)
		delete _deferredBuffers[i];

	_deferredBuffers.clear();
}

void RemasteredLookDeviceGl2::setTargets(const RemasteredLookTargetSetup &setup) {
	_setup = setup;
	_targetsDirty = true;
}

void RemasteredLookDeviceGl2::setDepth(int func, bool write) {
	_depthFunc = func;
	_depthWrite = write;
}

void RemasteredLookDeviceGl2::setCull(int mode) {
	_cullMode = mode;
}

void RemasteredLookDeviceGl2::setColorMask(bool red, bool green, bool blue, bool alpha) {
	_colorMask[0] = red;
	_colorMask[1] = green;
	_colorMask[2] = blue;
	_colorMask[3] = alpha;
}

void RemasteredLookDeviceGl2::applyTargets() {
	if (!_targetsDirty)
		return;

	_targetsDirty = false;

	RemasteredLookTargetGl *first = static_cast<RemasteredLookTargetGl *>(_setup.color[0]);

	if (!first || !first->texture) {
		glBindFramebuffer(GL_FRAMEBUFFER, _hostFramebuffer);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, _hostWidth, _hostHeight);

		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);

	GLenum buffers[kLookMaxTargets];
	int numBuffers = 0;

	for (int i = 0; i < kLookMaxTargets; i++) {
		RemasteredLookTargetGl *target = static_cast<RemasteredLookTargetGl *>(_setup.color[i]);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, target && target->texture ? target->texture->id : 0, 0);

		if (target && target->texture)
			buffers[numBuffers++] = GL_COLOR_ATTACHMENT0 + i;
	}

	RemasteredLookTargetGl *depth = static_cast<RemasteredLookTargetGl *>(_setup.depth);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth ? depth->renderbuffer : 0);
	glDrawBuffers(numBuffers, buffers);

	if (!_incompleteReported && glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		warning("RemasteredLookDeviceGl2::applyTargets(): the framebuffer is not complete");
		_incompleteReported = true;
	}

	glViewport(0, 0, first->getWidth(), first->getHeight());
}

void RemasteredLookDeviceGl2::applyDepth() {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(depthFuncTable[_depthFunc < 0 || _depthFunc > 5 ? 0 : _depthFunc]);
	glDepthMask(_depthWrite ? GL_TRUE : GL_FALSE);
}

void RemasteredLookDeviceGl2::applyCull() {
	if (_cullMode == kLookCullNone) {
		glDisable(GL_CULL_FACE);
	} else {
		glEnable(GL_CULL_FACE);
		glCullFace(_cullMode == kLookCullBack ? GL_BACK : GL_FRONT);
	}
}

void RemasteredLookDeviceGl2::applyColorMask() {
	glColorMask(_colorMask[0] ? GL_TRUE : GL_FALSE, _colorMask[1] ? GL_TRUE : GL_FALSE, _colorMask[2] ? GL_TRUE : GL_FALSE, _colorMask[3] ? GL_TRUE : GL_FALSE);
}

void RemasteredLookDeviceGl2::applyBlend(bool enable, int mode) {
	if (!enable) {
		glDisable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ZERO);

		return;
	}

	GLenum equation = GL_FUNC_ADD;
	GLenum destinationColor = GL_ONE_MINUS_SRC_ALPHA;
	GLenum destinationAlpha = GL_ZERO;

	switch (mode) {
	case kLookBlendAdd:
		destinationColor = GL_ONE;
		destinationAlpha = GL_ONE;
		break;
	case kLookBlendMin:
		equation = GL_MIN;
		destinationColor = GL_ONE;
		break;
	case kLookBlendMax:
		equation = GL_MAX;
		destinationColor = GL_ONE;
		break;
	default:
		destinationAlpha = GL_ONE;
		break;
	}

	glEnable(GL_BLEND);
	glBlendEquationSeparate(equation, GL_FUNC_ADD);
	glBlendFuncSeparate(GL_ONE, destinationColor, GL_ONE, destinationAlpha);
}

void RemasteredLookDeviceGl2::clear(const float rgba[4], float depth) {
	applyTargets();
	applyColorMask();
	applyDepth();

	GLbitfield mask = GL_COLOR_BUFFER_BIT;

	glClearColor(rgba[0], rgba[1], rgba[2], rgba[3]);

	if (_setup.depth && _depthWrite) {
		glClearDepth(depth);
		mask |= GL_DEPTH_BUFFER_BIT;
	}

	glClear(mask);
}

void RemasteredLookDeviceGl2::applySampler(RemasteredLookTextureGl *texture, const LookSampler &sampler) {
	int wrap = sampler.wrap;
	int filter = sampler.filter;
	GLint wrapMode = wrap == kLookWrapRepeat ? GL_REPEAT : (wrap == kLookWrapMirror ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE);
	GLint magFilter = filter != 0 ? GL_LINEAR : GL_NEAREST;
	GLint minFilter;
	bool linear = (filter & 1) != 0;

	if (!texture->hasMips() || sampler.mip == 0)
		minFilter = linear ? GL_LINEAR : GL_NEAREST;
	else if (sampler.mip == 1)
		minFilter = linear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
	else
		minFilter = linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;

	if (texture->wrap != wrapMode) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
		texture->wrap = wrapMode;
	}

	if (texture->minFilter != minFilter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
		texture->minFilter = minFilter;
	}

	if (texture->magFilter != magFilter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
		texture->magFilter = magFilter;
	}
}

void RemasteredLookDeviceGl2::bindLayout(uint programId, const char *instance, const LookConstLayout *layout, LayoutBinding &binding) {
	binding.layout = layout;
	binding.locations.clear();

	for (uint i = 0; i < layout->numMembers; i++) {
		Common::String name = Common::String(instance) + "." + layout->members[i].name;
		GLint location = glGetUniformLocation(programId, name.c_str());

		if (location < 0 && layout->members[i].kind == kLookConstVec4Array)
			location = glGetUniformLocation(programId, (name + "[0]").c_str());

		binding.locations.push_back(location);
	}
}

void RemasteredLookDeviceGl2::applyConstants(uint programId, const char *instance, const LookConstBlock &block, LayoutBinding &binding) {
	if (!block.layout)
		return;

	if (binding.layout != block.layout)
		bindLayout(programId, instance, block.layout, binding);

	for (uint i = 0; i < block.layout->numMembers; i++) {
		int location = binding.locations[i];

		if (location < 0)
			continue;

		const LookConstMember &member = block.layout->members[i];
		const float *values = (const float *)(block.data + member.offset);

		switch (member.kind) {
		case kLookConstFloat:
			glUniform1f(location, values[0]);
			break;
		case kLookConstVec2:
			glUniform2fv(location, 1, values);
			break;
		case kLookConstVec4:
			glUniform4fv(location, 1, values);
			break;
		case kLookConstMat4:
			glUniformMatrix4fv(location, 1, GL_FALSE, values);
			break;
		case kLookConstVec4Array:
			glUniform4fv(location, member.count, values);
			break;
		default:
			break;
		}
	}
}

void RemasteredLookDeviceGl2::bindVertexArrays(const RemasteredLookBuffer *vertices) {
	const RemasteredLookBufferGl *buffer = static_cast<const RemasteredLookBufferGl *>(vertices);
	const byte *base = buffer->data.data();
	uint wanted = 0;

	switch (buffer->getVertexFormat()) {
	case kLookVertexTextured:
		wanted = 7;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, base);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, base + 12);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, base + 24);
		break;
	case kLookVertexQuad:
	case kLookVertexText:
		wanted = 5;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, base);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 20, base + 12);
		break;
	default:
		wanted = 1;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, base);
		break;
	}

	for (int i = 0; i < 4; i++) {
		bool shouldEnable = (wanted & (1u << i)) != 0;
		bool enabled = (_enabledAttributes & (1u << i)) != 0;

		if (shouldEnable && !enabled)
			glEnableVertexAttribArray(i);
		else if (!shouldEnable && enabled)
			glDisableVertexAttribArray(i);
	}

	_enabledAttributes = wanted;
}

void RemasteredLookDeviceGl2::issueDraw(const LookCall &call) {
	const RemasteredLookBufferGl *vertices = static_cast<const RemasteredLookBufferGl *>(call.vertices);
	int count = call.count;

	if (call.indices) {
		const RemasteredLookBufferGl *indices = static_cast<const RemasteredLookBufferGl *>(call.indices);

		if (count < 0)
			count = (int)indices->getNumElements();

		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices->data.data());
	} else {
		if (count < 0)
			count = (int)vertices->getNumElements() - (int)call.first;

		glDrawArrays(GL_TRIANGLES, call.first, count);
	}
}

void RemasteredLookDeviceGl2::drawCall(const LookCall &call) {
	if (!_open || !call.vertices || !RemasteredLookPrograms::isKnown(call.program))
		return;

	Program *program = getProgram(call.program);

	if (!program)
		return;

	applyTargets();

	if (_currentProgram != program->id) {
		glUseProgram(program->id);
		_currentProgram = program->id;
	}

	if (call.hasVertexConstants)
		applyConstants(program->id, "constantsV", call.vertexConstants, program->vertexBinding);

	if (call.hasPixelConstants)
		applyConstants(program->id, "constantsP", call.pixelConstants, program->pixelBinding);

	bindVertexArrays(call.vertices);

	for (uint i = 0; i < call.numTextures && i < kLookMaxTextures; i++) {
		RemasteredLookTextureGl *texture = static_cast<RemasteredLookTextureGl *>(call.textures[i]);

		if (!texture && i == 0)
			warning("RemasteredLookDeviceGl2::drawCall(): program %s is drawn without a texture on unit 0 (%u textures), texture 0 is bound",
					RemasteredLookPrograms::getPixelName(call.program), call.numTextures);
		else if (texture && !glIsTexture(texture->id))
			warning("RemasteredLookDeviceGl2::drawCall(): program %s uses the deleted texture %u (%ux%u) on unit %u",
					RemasteredLookPrograms::getPixelName(call.program), texture->id, texture->getWidth(), texture->getHeight(), i);

		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, texture ? texture->id : 0);

		if (texture && i < call.numSamplers)
			applySampler(texture, call.samplers[i]);
	}

	applyBlend(call.blendEnable, call.blendMode);

	int savedFunc = _depthFunc;
	bool savedWrite = _depthWrite;

	_depthFunc = call.depthFunc;
	_depthWrite = call.depthWrite;
	applyDepth();
	applyCull();
	applyColorMask();

	issueDraw(call);

	_depthFunc = savedFunc;
	_depthWrite = savedWrite;
}

bool RemasteredLookDeviceGl2::readTarget(RemasteredLookTarget *target, byte *rgba, uint width, uint height) {
	RemasteredLookTargetGl *glTarget = static_cast<RemasteredLookTargetGl *>(target);

	if (!glTarget || !glTarget->texture || glTarget->getFormat() != kLookFormatRgba8 || glTarget->getWidth() != width || glTarget->getHeight() != height)
		return false;

	GLint previous = 0;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
	glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTarget->texture->id, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glBindFramebuffer(GL_FRAMEBUFFER, previous);
	_targetsDirty = true;

	return true;
}

RemasteredLookDevice *createRemasteredLookDevice() {
	return new RemasteredLookDeviceGl2();
}

} // End of namespace Grim

#else

namespace Grim {

RemasteredLookDeviceGl2::RemasteredLookDeviceGl2() : _open(false) {
}

RemasteredLookDeviceGl2::~RemasteredLookDeviceGl2() {
}

bool RemasteredLookDeviceGl2::open(int hostWidth, int hostHeight) {
	return false;
}

void RemasteredLookDeviceGl2::close() {
}

void RemasteredLookDeviceGl2::beginFrame() {
}

void RemasteredLookDeviceGl2::endFrame() {
}

RemasteredLookTarget *RemasteredLookDeviceGl2::createTarget(uint width, uint height, LookFormat format) {
	return nullptr;
}

RemasteredLookTexture *RemasteredLookDeviceGl2::getTargetTexture(RemasteredLookTarget *target) {
	return nullptr;
}

void RemasteredLookDeviceGl2::destroyTarget(RemasteredLookTarget *&target) {
}

RemasteredLookTexture *RemasteredLookDeviceGl2::createTexture(const LookImage &image) {
	return nullptr;
}

void RemasteredLookDeviceGl2::updateTexture(RemasteredLookTexture *texture, const byte *pixels, uint width, uint height) {
}

void RemasteredLookDeviceGl2::destroyTexture(RemasteredLookTexture *&texture) {
}

RemasteredLookBuffer *RemasteredLookDeviceGl2::createVertexBuffer(LookVertexFormat format, const void *data, uint numVertices) {
	return nullptr;
}

RemasteredLookBuffer *RemasteredLookDeviceGl2::createIndexBuffer(const uint16 *indices, uint count) {
	return nullptr;
}

void RemasteredLookDeviceGl2::destroyBuffer(RemasteredLookBuffer *&buffer) {
}

void RemasteredLookDeviceGl2::setTargets(const RemasteredLookTargetSetup &setup) {
}

void RemasteredLookDeviceGl2::setDepth(int func, bool write) {
}

void RemasteredLookDeviceGl2::setCull(int mode) {
}

void RemasteredLookDeviceGl2::setColorMask(bool red, bool green, bool blue, bool alpha) {
}

void RemasteredLookDeviceGl2::clear(const float rgba[4], float depth) {
}

void RemasteredLookDeviceGl2::drawCall(const LookCall &call) {
}

bool RemasteredLookDeviceGl2::readTarget(RemasteredLookTarget *target, byte *rgba, uint width, uint height) {
	return false;
}

bool RemasteredLookDeviceGl2::isContextLost() {
	return false;
}

void RemasteredLookDeviceGl2::abandon() {
}

RemasteredLookDevice *createRemasteredLookDevice() {
	return nullptr;
}

} // End of namespace Grim

#endif
