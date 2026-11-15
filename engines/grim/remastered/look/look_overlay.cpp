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

#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/remastered/look/look_consts.h"
#include "engines/grim/remastered/look/look_overlay.h"

namespace Grim {

static const LookSampler overlayImageSampler = { kLookWrapClamp, 0, 3, 0 };
static const LookSampler textSampler = { kLookWrapClamp, 1, 1, 0 };
static const LookSampler movieSampler = { kLookWrapClamp, 1, 1, 1 };

RemasteredLookOverlay::RemasteredLookOverlay(RemasteredLookDevice *device) : _device(device), _movieTexture(nullptr), _movieWidth(0),
		_movieHeight(0), _movieSerial(-1), _movieQueued(false) {
}

RemasteredLookOverlay::~RemasteredLookOverlay() {
	freeFrameBuffers();

	for (uint i = 0; i < _images.size(); i++)
		_device->destroyTexture(_images[i].texture);

	for (uint i = 0; i < _fonts.size(); i++)
		_device->destroyTexture(_fonts[i].texture);

	if (_movieTexture)
		_device->destroyTexture(_movieTexture);
}

void RemasteredLookOverlay::clearCommand(Command *command) const {
	memset(command, 0, sizeof(Command));
	command->serial = -1;
}

void RemasteredLookOverlay::beginFrame() {
	_commands.clear();
	_quads.clear();

	if (!_movieQueued && _movieTexture) {
		_device->destroyTexture(_movieTexture);
		_movieWidth = 0;
		_movieHeight = 0;
		_movieSerial = -1;
	}

	_movieQueued = false;
}

void RemasteredLookOverlay::queueFill(float x, float y, float width, float height, const float *color) {
	Command command;

	clearCommand(&command);
	command.kind = kKindFill;
	command.x = x;
	command.y = y;
	command.width = width;
	command.height = height;
	memcpy(command.color, color, sizeof(command.color));
	_commands.push_back(command);
}

void RemasteredLookOverlay::queueImage(const byte *pixels, int srcWidth, int srcHeight, float x, float y, float width, float height,
		int mirror, float alpha, float desaturate, float gamma) {
	if (!pixels || srcWidth <= 0 || srcHeight <= 0)
		return;

	Command command;

	clearCommand(&command);
	command.kind = kKindImage;
	command.pixels = pixels;
	command.srcWidth = srcWidth;
	command.srcHeight = srcHeight;
	command.x = x;
	command.y = y;
	command.width = width;
	command.height = height;
	command.mirror = mirror;
	command.alpha = alpha;
	command.desaturate = desaturate;
	command.gamma = gamma;
	_commands.push_back(command);
}

void RemasteredLookOverlay::queueText(const void *fontKey, const LookOverlayTextQuad *quads, uint numQuads, const float *textColor,
		float alpha, float originX, float originY) {
	if (numQuads == 0)
		return;

	Command command;

	clearCommand(&command);
	command.kind = kKindText;
	command.fontKey = fontKey;
	command.firstQuad = _quads.size();
	command.numQuads = numQuads;
	command.x = originX;
	command.y = originY;
	command.alpha = alpha;
	memcpy(command.color, textColor, sizeof(command.color));

	for (uint i = 0; i < numQuads; i++)
		_quads.push_back(quads[i]);

	_commands.push_back(command);
}

void RemasteredLookOverlay::queueMovie(const byte *rgba, int width, int height, int serial, float halfWidth, float halfHeight,
		float gamma) {
	if (!rgba || width <= 0 || height <= 0)
		return;

	Command command;

	clearCommand(&command);
	command.kind = kKindMovie;
	command.pixels = rgba;
	command.srcWidth = width;
	command.srcHeight = height;
	command.serial = serial;
	command.width = halfWidth;
	command.height = halfHeight;
	command.gamma = gamma;
	_commands.push_back(command);
	_movieQueued = true;
}

bool RemasteredLookOverlay::hasFont(const void *fontKey, uint generation) const {
	for (uint i = 0; i < _fonts.size(); i++) {
		if (_fonts[i].key == fontKey)
			return _fonts[i].generation == generation;
	}

	return false;
}

void RemasteredLookOverlay::setFont(const void *fontKey, uint generation, const byte *atlas, uint usedHeight) {
	if (usedHeight > (uint)kAtlasSize)
		usedHeight = kAtlasSize;

	for (uint i = 0; i < _fonts.size(); i++) {
		FontEntry &entry = _fonts[i];

		if (entry.key != fontKey)
			continue;

		uint rows = MAX(entry.usedHeight, usedHeight);

		if (rows > 0)
			_device->updateTextureRegion(entry.texture, atlas, kAtlasSize, 0, 0, kAtlasSize, rows);

		entry.generation = generation;
		entry.usedHeight = usedHeight;

		return;
	}

	LookImage image;

	image.format = kLookFormatR8;
	image.width = kAtlasSize;
	image.height = kAtlasSize;
	image.numLevels = 1;
	image.levels[0].data = atlas;
	image.levels[0].size = (uint)kAtlasSize * kAtlasSize;
	image.levels[0].width = kAtlasSize;
	image.levels[0].height = kAtlasSize;

	FontEntry entry;

	entry.key = fontKey;
	entry.generation = generation;
	entry.usedHeight = usedHeight;
	entry.texture = _device->createTexture(image);
	_fonts.push_back(entry);
}

void RemasteredLookOverlay::dropFont(const void *fontKey) {
	for (uint i = 0; i < _fonts.size(); i++) {
		if (_fonts[i].key == fontKey) {
			_device->destroyTexture(_fonts[i].texture);
			_fonts.remove_at(i);

			return;
		}
	}
}

void RemasteredLookOverlay::dropImage(const byte *pixels) {
	for (uint i = 0; i < _commands.size(); i++) {
		if (_commands[i].pixels == pixels && _commands[i].kind == kKindImage)
			_commands[i].pixels = nullptr;
	}

	for (uint i = 0; i < _images.size(); i++) {
		if (_images[i].pixels == pixels) {
			_device->destroyTexture(_images[i].texture);
			_images.remove_at(i);

			return;
		}
	}
}

RemasteredLookTexture *RemasteredLookOverlay::getImageTexture(const byte *pixels, int width, int height) {
	for (uint i = 0; i < _images.size(); i++) {
		ImageEntry &entry = _images[i];

		if (entry.pixels != pixels)
			continue;

		if (entry.width == width && entry.height == height)
			return entry.texture;

		_device->destroyTexture(entry.texture);
		_images.remove_at(i);
		break;
	}

	LookImage image;

	image.format = kLookFormatRgba8;
	image.width = width;
	image.height = height;
	image.numLevels = 1;
	image.levels[0].data = pixels;
	image.levels[0].size = (uint)width * height * 4;
	image.levels[0].width = width;
	image.levels[0].height = height;

	ImageEntry entry;

	entry.pixels = pixels;
	entry.width = width;
	entry.height = height;
	entry.texture = _device->createTexture(image);
	_images.push_back(entry);

	return entry.texture;
}

RemasteredLookTexture *RemasteredLookOverlay::getFontTexture(const void *fontKey) {
	for (uint i = 0; i < _fonts.size(); i++) {
		if (_fonts[i].key == fontKey)
			return _fonts[i].texture;
	}

	return nullptr;
}

void RemasteredLookOverlay::uploadMovie(const Command &command) {
	if (_movieTexture && (_movieWidth != command.srcWidth || _movieHeight != command.srcHeight)) {
		_device->destroyTexture(_movieTexture);
		_movieSerial = -1;
	}

	if (!_movieTexture) {
		LookImage image;

		image.format = kLookFormatRgba8;
		image.width = command.srcWidth;
		image.height = command.srcHeight;
		image.numLevels = 1;
		image.levels[0].data = command.pixels;
		image.levels[0].size = (uint)command.srcWidth * command.srcHeight * 4;
		image.levels[0].width = command.srcWidth;
		image.levels[0].height = command.srcHeight;
		_movieTexture = _device->createTexture(image);
		_movieWidth = command.srcWidth;
		_movieHeight = command.srcHeight;
		_movieSerial = command.serial;

		return;
	}

	if (command.serial >= 0 && command.serial == _movieSerial)
		return;

	_device->updateTextureRegion(_movieTexture, command.pixels, (uint)command.srcWidth * 4, 0, 0, command.srcWidth, command.srcHeight);
	_movieSerial = command.serial;
}

float RemasteredLookOverlay::clampAlpha(float value) {
	if (value <= 1.0f) {
		if (value < 0.0f)
			return 0.0f;

		return value;
	}

	return 1.0f;
}

void RemasteredLookOverlay::scaleRect(float x, float y, float width, float height, float kx, float ky, float *ndcX, float *ndcY,
		float *ndcWidth, float *ndcHeight) {
	*ndcX = (float)((double)x * (double)kx - 1.0);
	*ndcY = (float)((double)y * (double)ky + 1.0);
	*ndcWidth = (float)((double)kx * (double)width);
	*ndcHeight = (float)((double)ky * (double)height);
}

void RemasteredLookOverlay::createQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1,
		LookCall *call) {
	float data[6 * 5] = {
		x, y + height, 0.0f, u0, v0,
		x + width, y + height, 0.0f, u1, v0,
		x, y, 0.0f, u0, v1,
		x + width, y + height, 0.0f, u1, v0,
		x + width, y, 0.0f, u1, v1,
		x, y, 0.0f, u0, v1
	};

	RemasteredLookBuffer *buffer = _device->createVertexBuffer(kLookVertexQuad, data, 6);

	_frameBuffers.push_back(buffer);
	call->vertices = buffer;
	call->indices = nullptr;
	call->count = -2;
	call->first = 0;
}

void RemasteredLookOverlay::freeFrameBuffers() {
	for (uint i = 0; i < _frameBuffers.size(); i++)
		_device->destroyBuffer(_frameBuffers[i]);

	_frameBuffers.clear();
}

void RemasteredLookOverlay::issueFill(const Command &command, float kx, float ky) {
	float ndcX;
	float ndcY;
	float ndcWidth;
	float ndcHeight;

	scaleRect(command.x, command.y, command.width, command.height, kx, ky, &ndcX, &ndcY, &ndcWidth, &ndcHeight);

	LookCall call;

	call.reset();
	call.program = kLookProgramColorQuad;
	call.depthFunc = kLookDepthAlways;
	call.depthWrite = false;
	call.blendEnable = true;
	call.blendMode = kLookBlendPremultiplied;
	call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutColorQuad));
	call.pixelConstants.setFloats(RemasteredLookConsts::kColorQuadFill, command.color, 4);
	call.hasPixelConstants = true;
	call.vertexConstants = call.pixelConstants;
	call.hasVertexConstants = true;
	createQuad(ndcX, ndcY, ndcWidth, ndcHeight, 0.0f, 0.0f, 1.0f, 1.0f, &call);
	_device->drawCall(call);
}

void RemasteredLookOverlay::issueImage(const Command &command, float kx, float ky) {
	if (!command.pixels)
		return;

	RemasteredLookTexture *texture = getImageTexture(command.pixels, command.srcWidth, command.srcHeight);

	if (!texture)
		return;

	float imageWidth = (float)command.srcWidth;
	float imageHeight = (float)command.srcHeight;
	float u0Pixels = 0.0f;
	float v0Pixels = 0.0f;
	float u1Pixels = 0.0f;
	float v1Pixels = 0.0f;

	switch (command.mirror) {
	case 0:
		u1Pixels = imageWidth;
		v1Pixels = imageHeight;
		break;
	case 1:
		u0Pixels = imageWidth;
		v1Pixels = imageHeight;
		break;
	case 2:
		v0Pixels = imageHeight;
		u1Pixels = imageWidth;
		break;
	case 3:
		u0Pixels = imageWidth;
		v0Pixels = imageHeight;
		break;
	default:
		u0Pixels = command.width;
		v0Pixels = command.height;
		u1Pixels = u0Pixels;
		v1Pixels = v0Pixels;
		break;
	}

	float invWidth = (float)(1.0 / (double)command.srcWidth);
	float invHeight = (float)(1.0 / (double)command.srcHeight);
	float u0 = u0Pixels * invWidth;
	float v0 = v1Pixels * invHeight;
	float u1 = u1Pixels * invWidth;
	float v1 = v0Pixels * invHeight;
	float ndcX;
	float ndcY;
	float ndcWidth;
	float ndcHeight;

	scaleRect(command.x, command.y, command.width, command.height, kx, ky, &ndcX, &ndcY, &ndcWidth, &ndcHeight);

	float multiply = clampAlpha(command.alpha);
	float multiplyColor[4] = { multiply, multiply, multiply, multiply };
	float texel[4] = { invWidth, invHeight, clampAlpha(command.desaturate), command.gamma };

	LookCall call;

	call.reset();
	call.program = kLookProgramTexturedQuad;
	call.depthFunc = kLookDepthAlways;
	call.depthWrite = false;
	call.blendEnable = true;
	call.blendMode = kLookBlendPremultiplied;
	call.numTextures = 1;
	call.textures[0] = texture;
	call.numSamplers = 1;
	call.samplers[0] = overlayImageSampler;
	call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutQuad));
	call.pixelConstants.setFloats(RemasteredLookConsts::kQuadMultiplyColor, multiplyColor, 4);
	call.pixelConstants.setFloats(RemasteredLookConsts::kQuadTexelDesaturateGamma, texel, 4);
	call.hasPixelConstants = true;
	createQuad(ndcX, ndcY, ndcWidth, ndcHeight, u0, v0, u1, v1, &call);
	_device->drawCall(call);
}

void RemasteredLookOverlay::issueText(const Command &command, float kx, float ky) {
	RemasteredLookTexture *texture = getFontTexture(command.fontKey);

	if (!texture || command.numQuads == 0)
		return;

	float texelSize = (float)(1.0 / (double)kAtlasSize);
	Common::Array<float> vertices;

	vertices.resize(command.numQuads * 30);

	for (uint i = 0; i < command.numQuads; i++) {
		const LookOverlayTextQuad &quad = _quads[command.firstQuad + i];
		int right = quad.atlasX + quad.width - 4;
		int bottom = quad.atlasY + quad.height - 4;
		int leftEdge = quad.atlasX - 2 > 0 ? quad.atlasX - 2 : 0;
		int topEdge = quad.atlasY - 2 > 0 ? quad.atlasY - 2 : 0;
		int rightEdge = right + 2 < kAtlasSize - 1 ? right + 2 : kAtlasSize - 1;
		int bottomEdge = bottom + 2 < kAtlasSize - 1 ? bottom + 2 : kAtlasSize - 1;
		float u0 = (float)leftEdge * texelSize;
		float v0 = (float)topEdge * texelSize;
		float u1 = texelSize * (float)rightEdge;
		float v1 = (float)bottomEdge * texelSize;
		float x = quad.x;
		float y = quad.y;
		float quadRight = (float)quad.width + x;
		float quadBottom = (float)quad.height + y;
		float *v = &vertices[i * 30];
		float data[30] = {
			x, quadBottom, 0.0f, u0, v1,
			quadRight, quadBottom, 0.0f, u1, v1,
			x, y, 0.0f, u0, v0,
			quadRight, quadBottom, 0.0f, u1, v1,
			quadRight, y, 0.0f, u1, v0,
			x, y, 0.0f, u0, v0
		};

		memcpy(v, data, sizeof(data));
	}

	RemasteredLookBuffer *buffer = _device->createVertexBuffer(kLookVertexText, &vertices[0], command.numQuads * 6);

	_frameBuffers.push_back(buffer);

	float shadowAlpha = command.alpha;
	float shadowColor[4] = { 0.0f * shadowAlpha, 0.0f * shadowAlpha, 0.0f * shadowAlpha, shadowAlpha };
	float origin[2] = { command.x, command.y };
	float pixelSize[2] = { kx, ky };
	float texel[2] = { texelSize, texelSize };

	LookCall call;

	call.reset();
	call.program = kLookProgramText;
	call.depthFunc = kLookDepthAlways;
	call.depthWrite = false;
	call.blendEnable = true;
	call.blendMode = kLookBlendPremultiplied;
	call.numTextures = 1;
	call.textures[0] = texture;
	call.numSamplers = 1;
	call.samplers[0] = textSampler;
	call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutText));
	call.pixelConstants.setFloats(RemasteredLookConsts::kTextColor, command.color, 4);
	call.pixelConstants.setFloats(RemasteredLookConsts::kTextShadowColor, shadowColor, 4);
	call.pixelConstants.setFloats(RemasteredLookConsts::kTextOrigin, origin, 2);
	call.pixelConstants.setFloats(RemasteredLookConsts::kTextPixelSize, pixelSize, 2);
	call.pixelConstants.setFloats(RemasteredLookConsts::kTextTexelSize, texel, 2);
	call.hasPixelConstants = true;
	call.vertexConstants = call.pixelConstants;
	call.hasVertexConstants = true;
	call.vertices = buffer;
	call.indices = nullptr;
	call.count = (int)(command.numQuads * 6);
	call.first = 0;
	_device->drawCall(call);
}

void RemasteredLookOverlay::issueMovie(const Command &command) {
	uploadMovie(command);

	if (!_movieTexture)
		return;

	float halfWidth = command.width;
	float halfHeight = command.height;
	float multiplyColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float texel[4] = { 1.0f, 1.0f, 0.0f, command.gamma };

	LookCall call;

	call.reset();
	call.program = kLookProgramTexturedQuad;
	call.depthFunc = kLookDepthAlways;
	call.depthWrite = false;
	call.blendEnable = false;
	call.numTextures = 1;
	call.textures[0] = _movieTexture;
	call.numSamplers = 1;
	call.samplers[0] = movieSampler;
	call.pixelConstants.setLayout(RemasteredLookConsts::getLayout(RemasteredLookConsts::kLayoutQuad));
	call.pixelConstants.setFloats(RemasteredLookConsts::kQuadMultiplyColor, multiplyColor, 4);
	call.pixelConstants.setFloats(RemasteredLookConsts::kQuadTexelDesaturateGamma, texel, 4);
	call.hasPixelConstants = true;
	createQuad(-halfWidth, -halfHeight, halfWidth + halfWidth, halfHeight + halfHeight, 0.0f, 0.0f, 1.0f, 1.0f, &call);
	_device->drawCall(call);
}

void RemasteredLookOverlay::issue(int hostWidth, int hostHeight) {
	float kx = (float)(2.0 / (double)(uint32)hostWidth);
	float ky = (float)(-2.0 / (double)(uint32)hostHeight);

	for (uint i = 0; i < _commands.size(); i++) {
		const Command &command = _commands[i];

		switch (command.kind) {
		case kKindFill:
			issueFill(command, kx, ky);
			break;
		case kKindImage:
			issueImage(command, kx, ky);
			break;
		case kKindText:
			issueText(command, kx, ky);
			break;
		case kKindMovie:
			issueMovie(command);
			break;
		default:
			break;
		}
	}

	freeFrameBuffers();
	_commands.clear();
	_quads.clear();
}

} // End of namespace Grim
