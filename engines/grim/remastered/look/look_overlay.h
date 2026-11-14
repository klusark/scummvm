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

#ifndef GRIM_REMASTERED_LOOK_LOOK_OVERLAY_H
#define GRIM_REMASTERED_LOOK_LOOK_OVERLAY_H

#include "common/array.h"
#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_device.h"
#include "engines/grim/remastered/look/look_types.h"

namespace Grim {

struct LookOverlayTextQuad {
	float x;
	float y;
	int width;
	int height;
	int atlasX;
	int atlasY;
};

class RemasteredLookOverlay {
public:
	enum {
		kAtlasSize = 2048
	};

	explicit RemasteredLookOverlay(RemasteredLookDevice *device);
	~RemasteredLookOverlay();

	void beginFrame();

	void queueFill(float x, float y, float width, float height, const float *color);
	void queueImage(const byte *pixels, int srcWidth, int srcHeight, float x, float y, float width, float height, int mirror,
			float alpha, float desaturate, float gamma);
	void queueText(const void *fontKey, const LookOverlayTextQuad *quads, uint numQuads, const float *textColor, float alpha,
			float originX, float originY);
	void queueMovie(const byte *rgba, int width, int height, int serial, float halfWidth, float halfHeight, float gamma);

	bool hasFont(const void *fontKey, uint generation) const;
	void setFont(const void *fontKey, uint generation, const byte *atlas, uint usedHeight);
	void dropFont(const void *fontKey);
	void dropImage(const byte *pixels);

	void issue(int hostWidth, int hostHeight);

private:
	enum Kind {
		kKindFill,
		kKindImage,
		kKindText,
		kKindMovie
	};

	struct Command {
		int kind;
		float color[4];
		float x;
		float y;
		float width;
		float height;
		const byte *pixels;
		int srcWidth;
		int srcHeight;
		int mirror;
		float alpha;
		float desaturate;
		float gamma;
		const void *fontKey;
		uint firstQuad;
		uint numQuads;
		int serial;
	};

	struct ImageEntry {
		const byte *pixels;
		int width;
		int height;
		RemasteredLookTexture *texture;
	};

	struct FontEntry {
		const void *key;
		uint generation;
		uint usedHeight;
		RemasteredLookTexture *texture;
	};

	void clearCommand(Command *command) const;
	RemasteredLookTexture *getImageTexture(const byte *pixels, int width, int height);
	RemasteredLookTexture *getFontTexture(const void *fontKey);
	void uploadMovie(const Command &command);
	void createQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1, LookCall *call);
	void issueFill(const Command &command, float kx, float ky);
	void issueImage(const Command &command, float kx, float ky);
	void issueText(const Command &command, float kx, float ky);
	void issueMovie(const Command &command);
	void freeFrameBuffers();
	static void scaleRect(float x, float y, float width, float height, float kx, float ky, float *ndcX, float *ndcY, float *ndcWidth,
			float *ndcHeight);
	static float clampAlpha(float value);

	RemasteredLookDevice *_device;
	Common::Array<Command> _commands;
	Common::Array<LookOverlayTextQuad> _quads;
	Common::Array<ImageEntry> _images;
	Common::Array<FontEntry> _fonts;
	Common::Array<RemasteredLookBuffer *> _frameBuffers;
	RemasteredLookTexture *_movieTexture;
	int _movieWidth;
	int _movieHeight;
	int _movieSerial;
	bool _movieQueued;
};

} // End of namespace Grim

#endif
