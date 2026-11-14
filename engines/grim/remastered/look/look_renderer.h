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

#ifndef GRIM_REMASTERED_LOOK_LOOK_RENDERER_H
#define GRIM_REMASTERED_LOOK_LOOK_RENDERER_H

#include "common/array.h"
#include "common/random.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/remastered/look/look_device.h"
#include "engines/grim/remastered/look/look_pipeline.h"
#include "engines/grim/remastered/look/look_scene.h"
#include "engines/grim/remastered/look/look_textures.h"
#include "engines/grim/remastered/look/setx.h"

namespace Grim {

class RenderDroid;
class SputRoom;
class SputActorModule;
class SputRender;
class SputTime;
class WuRegistry;
struct RdModel3;
struct RdModel3Mesh;
struct RdThing;
struct StdVBuffer;
struct StdRect;

class RemasteredLookRenderer : public RemasteredLookScene {
public:
	enum {
		kMaxExtraLights = 16
	};

	RemasteredLookRenderer(RenderDroid *rd, SputRoom *room, SputActorModule *actors, SputRender *render, SputTime *time, WuRegistry *registry);
	~RemasteredLookRenderer() override;

	bool open(int hostWidth, int hostHeight);
	void close();
	bool isOpen() const { return _open; }

	void setBuffers(const StdVBuffer *drawBuffer, const StdVBuffer *zBuffer, const StdVBuffer *frontBuffer);

	void beginFrame();
	bool isLookEnabled() const { return _open && _deferredEnabled; }
	bool isSoftwareEnabled() const { return !_open || _softwareEnabled; }

	void drawBeginModel3(RdModel3 *model, const float *worldCenter, float radius, const float *matrix12, const char *parentName);
	void drawModel3Mesh(RdModel3Mesh *mesh, const float *matrix12);
	void drawEndModel3();
	void drawSprite(RdThing *thing, float (*matrix)[3]);
	bool drawShadowGeometry(const float *vertices, uint count, uint colour565);
	void bufferCopy(const StdVBuffer *destination, const StdVBuffer *source, int x, int y, const StdRect *rect, bool colorKeyFlag);
	void postCameraChange();
	void notifyChange(const void *key);
	void notifyBufferWritten(const StdVBuffer *buffer);
	void notifyFree(const void *key);
	void notifyMaterialLoad(const void *pixels, const char *materialName, int cellIndex);

	void present(const byte *software565, int softwarePitch, const byte *overlayRgba, bool overlayChanged);

	uint getCharacterLightCount() const override;
	uint getShadowCharacterLightCount() const override;
	uint getCutsceneLightCount() const override { return 0; }
	bool getCharacterLightInfo(uint index, int type, LookLightInfo &info) const override;
	bool getCutsceneLightInfo(uint index, LookCutsceneLight &light) const override;
	uint getShadowCasterCount() const override;
	bool getShadowCasterInfo(uint index, float matrix[12]) const override;
	uint getZShapeCount() const override;
	bool getZShapeInfo(uint index, int type, int drawPass, float matrix[12], int &tessellation) const override;
	bool getCompositor(LookCompositorInfo &info) const override;
	uint getActorAoCount() const override;
	bool getActorAoInfo(uint index, float matrix[12]) const override;
	uint getRenderThingCount() const override { return 0; }
	void getRenderThingSphere(uint index, float center[3], float &radius) const override;
	void getIris(float &size, float &centerX, float &centerY) const override;
	float getGamma() const override;
	void getCamera(LookCamera &camera) const override;
	uint getAdvancedLighting() const override;
	void setLightLodType(int type) override { _lightLodType = type; }
	void getCurrentSetupName(Common::String &name) const override;
	const void *getCameraIdentity() const override;
	void resetCharacterLightKeylightStates() override;
	void setCharacterLightKeylightState(uint index, int value) override;

private:
	struct VertexEntry {
		const void *vertices;
		const void *set;
		int index;
		RemasteredLookBuffer *buffer;
	};

	struct ExtraLightSpec {
		Common::String name;
		int type;
		float position[3];
		float rotation[3];
		float scale[3];
		float shadingBias;
		float shadingLimit;
		float innerRadius;
		float falloffPower;
		float verticalInner;
		float verticalPower;
		int animationMode;
		float animationFrequency;
		int shadowMode;
		int shadowQuality;
		float shadowBias;
		float shadowSoftness;
		float shadowNear;
		float shadowMaxDistance;
		float innerA[3];
		float outerA[3];
		float intensityA;
		float innerB[3];
		float outerB[3];
		float intensityB;

		ExtraLightSpec();
	};

	void updateAlpha();
	void recoverContext();
	bool getLightStatus(const char *name, int &value) const;
	void updateRoom();
	void updateExtraLights(float step);
	void addExtraLight(const ExtraLightSpec &spec);
	void fillMatrix(const float *position, const float *rotation, const float *scale, float matrix[12]) const;
	bool getCameraRelativeValue(const float *center, float radius, float &value) const;
	RemasteredLookBuffer *getVertexBuffer(RdModel3Mesh *mesh, const void *set, int index, float scaleU, float scaleV, int &count);
	void releaseVertexBuffers(const void *key);
	void releaseAllVertexBuffers();

	RenderDroid *_rd;
	SputRoom *_room;
	SputActorModule *_actors;
	SputRender *_render;
	SputTime *_time;
	WuRegistry *_registry;

	RemasteredLookDevice *_remasteredLookDevice;
	RemasteredLookPipeline *_remasteredLookPipeline;
	RemasteredLookTextureCache *_remasteredLookTextureCache;
	RemasteredLookTexture *_softwareTexture;
	RemasteredLookTexture *_overlayTexture;
	bool _open;
	int _hostWidth;
	int _hostHeight;

	const StdVBuffer *_drawBuffer;
	const StdVBuffer *_zBuffer;
	const StdVBuffer *_frontBuffer;
	bool _frontBufferVisible;

	double _previousFrameTime;
	bool _hasPreviousFrame;
	float _softwareAlpha;
	float _softwareAlphaEnd;
	float _softwareAlphaStart;
	float _softwareAlphaBlendTime;
	bool _deferredEnabled;
	bool _softwareEnabled;
	bool _graphicsChanged;
	bool _widescreen;
	float _resolutionScaling;

	SetX _setx;
	Common::String _loadedRoom;
	Common::RandomSource _random;
	CharacterLight _extraLights[kMaxExtraLights];
	uint _extraCount;
	int _lightLodType;
	Common::Array<VertexEntry> _vertexEntries;
};

} // End of namespace Grim

#endif
