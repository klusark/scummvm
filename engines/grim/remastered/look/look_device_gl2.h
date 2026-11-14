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

#ifndef GRIM_REMASTERED_LOOK_LOOK_DEVICE_GL2_H
#define GRIM_REMASTERED_LOOK_LOOK_DEVICE_GL2_H

#include "common/array.h"
#include "common/fs.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/str.h"

#include "engines/grim/remastered/look/look_device.h"

namespace Grim {

class RemasteredLookTextureGl;
class RemasteredLookTargetGl;
class RemasteredLookBufferGl;

class RemasteredLookDeviceGl2 : public RemasteredLookDevice {
public:
	RemasteredLookDeviceGl2();
	~RemasteredLookDeviceGl2() override;

	bool open(int hostWidth, int hostHeight) override;
	void close() override;
	const LookCaps &getCaps() const override { return _caps; }

	void beginFrame() override;
	void endFrame() override;

	RemasteredLookTarget *createTarget(uint width, uint height, LookFormat format) override;
	RemasteredLookTexture *getTargetTexture(RemasteredLookTarget *target) override;
	void destroyTarget(RemasteredLookTarget *&target) override;

	RemasteredLookTexture *createTexture(const LookImage &image) override;
	void updateTexture(RemasteredLookTexture *texture, const byte *pixels, uint width, uint height) override;
	void destroyTexture(RemasteredLookTexture *&texture) override;

	RemasteredLookBuffer *createVertexBuffer(LookVertexFormat format, const void *data, uint numVertices) override;
	RemasteredLookBuffer *createIndexBuffer(const uint16 *indices, uint count) override;
	void destroyBuffer(RemasteredLookBuffer *&buffer) override;

	void setTargets(const RemasteredLookTargetSetup &setup) override;
	void setDepth(int func, bool write) override;
	void setCull(int mode) override;
	void setColorMask(bool red, bool green, bool blue, bool alpha) override;
	void clear(const float rgba[4], float depth) override;
	void drawCall(const LookCall &call) override;

	bool readTarget(RemasteredLookTarget *target, byte *rgba, uint width, uint height) override;
	bool isContextLost() override;
	void abandon() override;

private:
	enum {
		kSentinelWidth = 7,
		kSentinelHeight = 5
	};

	struct PhysicalFormat {
		int internalFormat;
		int format;
		int type;
		bool valid;
	};

	struct LayoutBinding {
		const LookConstLayout *layout;
		Common::Array<int> locations;
	};

	struct Program {
		uint id;
		LayoutBinding vertexBinding;
		LayoutBinding pixelBinding;
	};

	typedef Common::HashMap<uint32, Program *> ProgramMap;
	typedef Common::HashMap<Common::String, Common::String> SourceMap;

	void setFormats();
	bool findShaderDirectory();
	bool loadSource(const Common::String &name, Common::String &source);
	uint compileShader(int stage, const Common::String &name, const Common::String &text);
	Program *getProgram(int program);
	bool precompilePrograms();
	void freePrograms();
	void bindLayout(uint programId, const char *instance, const LookConstLayout *layout, LayoutBinding &binding);
	void applyConstants(uint programId, const char *instance, const LookConstBlock &block, LayoutBinding &binding);

	void flushDeferred();
	void applyTargets();
	void applyDepth();
	void applyCull();
	void applyColorMask();
	void applyBlend(bool enable, int mode);
	void applySampler(RemasteredLookTextureGl *texture, const LookSampler &sampler);
	void bindVertexArrays(const RemasteredLookBuffer *vertices);
	void issueDraw(const LookCall &call);
	void uploadLevel(RemasteredLookTextureGl *texture, uint level, uint width, uint height, const byte *data, uint size, LookFormat format);
	void setPhysical(LookFormat format, int internalFormat, int glFormat, int type);

	LookCaps _caps;
	bool _open;
	int _hostWidth;
	int _hostHeight;
	int _hostFramebuffer;
	int _hostViewport[4];
	int _hostProgram;
	int _hostArrayBuffer;
	int _hostElementBuffer;

	uint _framebuffer;
	PhysicalFormat _physical[kLookFormatDepth + 1];
	Common::FSNode _shaderDirectory;
	SourceMap _sources;
	ProgramMap _programs;

	RemasteredLookTargetSetup _setup;
	bool _targetsDirty;
	int _depthFunc;
	bool _depthWrite;
	int _cullMode;
	bool _colorMask[4];
	uint _currentProgram;
	uint _enabledAttributes;
	bool _depthStateKnown;
	bool _frameActive;
	bool _incompleteReported;
	bool _abandoned;
	uint _sentinel;
	Common::Array<RemasteredLookTextureGl *> _deferredTextures;
	Common::Array<RemasteredLookBufferGl *> _deferredBuffers;
};

} // End of namespace Grim

#endif
