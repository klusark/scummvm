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

#ifndef GRIM_RENDERDROID_RD_H
#define GRIM_RENDERDROID_RD_H

#include "common/scummsys.h"

#include "engines/grim/std/std_conffile.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/std/std_file.h"
#include "engines/grim/std/std_math.h"
#include "engines/grim/std/std_sys.h"

namespace Grim {

typedef float RdVector3[3];
typedef float RdVector2[2];
typedef float RdMatrix34[4][3];

struct RdModel3;
struct RdModel3Mesh;
struct RdModel3HNode;
struct RdFace3;
struct RdMaterial;
struct RdTexture;
struct RdMatCel;
struct RdThing;
struct RdPuppet;
struct RdKeyframe;
struct RdCamera;
struct RdCanvas;
struct RdLight;
struct RdSprite;
struct RdPolyline;
struct RdParticleCloud;
struct RdProcFace;
struct RdActiveEdge;
struct RdActiveFace;
struct RdActiveSpan;

class Std3D;
class RemasteredLookRenderer;
struct Std3DVertex;
struct Std3DTri;

struct RdBound3 {
	RdVector3 min;
	RdVector3 max;
};

struct RdModel3Geo {
	uint numMeshes;
	RdModel3Mesh *meshes;
};

struct RdFace3 {
	uint num;
	uint type;
	int geometryMode;
	int lightingMode;
	int textureMode;
	uint numVertices;
	int *vertices;
	int *texVertices;
	RdMaterial *material;
	int curCel;
	RdVector2 uvDelta;
	float extraLight;
	int reserved52;
	int reserved56;
	int reserved60;
	RdVector3 normal;
};

struct RdModel3Mesh {
	char name[32];
	uint num;
	int geometryMode;
	int lightingMode;
	int textureMode;
	RdVector3 *vertices;
	RdVector2 *texVertices;
	float *staticVertexIntensities;
	float *dynamicVertexIntensities;
	RdFace3 *faces;
	RdVector3 *vertexNormals;
	uint numVertices;
	uint numTexVertices;
	uint numFaces;
	int castShadow;
	RdVector3 *shadowVerts;
	float userLightValue;
	float radius;
	RdBound3 bound;
};

struct RdModel3HNode {
	char name[64];
	uint flags;
	uint num;
	uint type;
	int meshNodeIndex;
	uint depth;
	RdModel3HNode *parentNode;
	uint numChildren;
	RdModel3HNode *firstChildNode;
	RdModel3HNode *siblingNode;
	RdVector3 pivot;
	RdVector3 xyz;
	RdVector3 pyr;
	RdMatrix34 mat;
};

struct RdModel3 {
	char name[32];
	int index;
	RdModel3Geo geo[4];
	uint numGeos;
	RdMaterial **materials;
	uint numMaterials;
	uint curGeoSet;
	uint numHNodes;
	RdModel3HNode *hNodes;
	float radius;
	RdBound3 bound;
	RdVector3 geomCenter;
	RdVector3 insertOffset;
};

struct RdMatCel {
	uint flags;
	int colorIndex;
	int reserved8;
	int reserved12;
	int reserved16;
	int reserved20;
	int reserved24;
	RdTexture *texture;
};

struct RdTexCacheEntry {
	uint texture;
	int inCache;
	int colorKey;
	uint width;
	uint height;
	uint size;
	uint lastFrame;
	RdTexCacheEntry *prev;
	RdTexCacheEntry *next;
};

struct RdTexture {
	uint flags;
	int reserved4;
	int colorKey;
	int widthLog2;
	int widthMask;
	int heightMask;
	uint numMips;
	StdVBuffer *mips[4];
	RdTexCacheEntry cache[2][4];
};

struct RdMaterial {
	uint flags;
	char name[32];
	int lockCount;
	StdColorFormat format;
	uint numCels;
	int curCel;
	RdMatCel *cels[16];
	uint numTextures;
	RdTexture *textures;
};

struct RdLight {
	int id;
	int type;
	int active;
	RdVector3 direction;
	float intensity;
	uint color;
	float angleX;
	float cosAngleX;
	float angleY;
	float cosAngleY;
	float lux;
	float falloffMin;
	float falloffMax;
};

struct RdVertexLists {
	uint numVertices;
	int *vertIndices;
	int *texIndices;
	RdVector3 *vertices;
	RdVector2 *texVertices;
	float *intensities;
	float *addedIntensities;
	float *redIntensities;
	float *greenIntensities;
	float *blueIntensities;
	RdVector3 *screen;
};

struct RdClipPoly {
	RdVector3 *verts;
	RdVector2 *texVerts;
	float *intensities;
};

struct RdKeyframeEntry {
	float frame;
	uint flags;
	RdVector3 pos;
	RdVector3 pyr;
	RdVector3 dpos;
	RdVector3 dpyr;
};

struct RdKeyframeNode {
	char name[32];
	uint nodeNum;
	uint numEntries;
	RdKeyframeEntry *paEntries;
};

struct RdKeyframe {
	char name[32];
	int reserved32;
	uint flags;
	uint numJoints;
	uint type;
	float fps;
	uint numFrames;
	uint numNodes;
	RdKeyframeNode *paNodes;
	uint numMarkers;
	float markerFrame[8];
	int markerType[8];
};

typedef void (*RdPuppetTrackCallback)(void *userArg, int trackNum, int markerType);

struct RdPuppetTrack {
	uint status;
	int reserved4;
	int priority;
	int priorityMasked;
	float fps;
	float noise;
	float fade;
	float fadeSpeed;
	int entryIdx[64];
	float curFrame;
	float prevFrame;
	RdKeyframe *keyframe;
	RdPuppetTrackCallback callback;
	int reserved304;
};

struct RdPuppet {
	int paused;
	RdThing *thing;
	RdPuppetTrack tracks[6];
};

struct RdCanvas {
	uint flags;
	StdVBuffer *vBuffer;
	float xCenter;
	float yCenter;
	StdVBuffer *zBuffer;
	int zScaleShift;
	int xStart;
	int yStart;
	int xEnd;
	int yEnd;
};

struct RdClipFrustum {
	int clipFar;
	float zNear;
	float zFar;
	float orthoLeft;
	float orthoTop;
	float orthoRight;
	float orthoBottom;
	float top;
	float bottom;
	float left;
	float right;
	float guardTop;
	float guardLeft;
};

struct RdSprite {
	char name[32];
	int type;
	float radius;
	byte reserved40[36];
	float width;
	float height;
	float halfWidth;
	float halfHeight;
	RdFace3 face;
	float *texVerts;
	RdVector3 offset;
};

struct RdPolyline {
	char name[32];
	float length;
	float startWidth;
	float endWidth;
	byte reserved44[12];
	RdFace3 faceBody;
	RdFace3 faceEnd;
	RdVector2 *texVertsBody;
	RdVector2 *texVertsEnd;
};

struct RdParticleCloud {
	char name[32];
	int lightingMode;
	uint numParticles;
	RdVector3 *positions;
	int *cels;
	int reserved48;
	float size;
	RdMaterial *material;
	float radius;
};

struct RdThing {
	int type;
	union {
		RdModel3 *model3;
		RdCamera *camera;
		RdLight *light;
		RdSprite *sprite;
		RdPolyline *polyline;
		RdParticleCloud *particle;
		void *dataPointer;
	} data;
	int geometryMode;
	int lightingMode;
	int textureMode;
	RdPuppet *puppet;
	int jointsLocked;
	int jointsFrame;
	RdMatrix34 *jointMatrices;
	RdVector3 *jointPyr;
	int *abJointAmputate;
	int *abMeshVisible;
	int curCel;
	int curGeoSet;
	int curGeometryMode;
	int curLightingMode;
	int curTextureMode;
	int frustrumCull;
	void *userArg;
};

struct RdColormap {
	char name[32];
	uint flags;
	RdVector3 tint;
	byte colors[768];
	byte *lightLevel;
	byte *lightLevelAlloc;
	byte *transparency;
	byte *transparencyAlloc;
	int reserved832;
	int reserved836;
	byte *rgb16;
	byte *rgb16Alpha;
};

typedef void (*RdProcFaceDrawFunc)(RdProcFace *face, RdMatCel *cel, float light);
typedef void (*RdProcFaceSetupFunc)(RdActiveFace *face, RdMatCel *cel, float light);

struct RdProcFace {
	uint flags;
	int type;
	int geometryMode;
	int lightingMode;
	int textureMode;
	RdProcFaceDrawFunc userDraw;
	RdProcFaceSetupFunc userSetup;
	uint numVertices;
	RdVector3 *vertices;
	RdVector2 *texVertices;
	float *intensities;
	float *redIntensities;
	float *greenIntensities;
	float *blueIntensities;
	RdMaterial *material;
	int curCel;
	float ambientLight;
	float diffuseLight;
	float extraLight;
	RdColormap *colormap;
	int backface;
	int xMin;
	int xMax;
	int yMin;
	int yMax;
	float zMin;
	float zMax;
	int yMinIdx;
	int yMaxIdx;
	int userData;
};

struct RdColorEffects {
	int tint[3];
	float filter[3];
	int add[3];
	float fade;
};

typedef RdMaterial *(*RdMaterialLoaderFunc)(char *path, int a2, int a3);
typedef int (*RdMaterialUnloaderFunc)(RdMaterial *material);

struct RdNRasterEdge {
	int x;
	int dx;
	int numLines;
	uint vertIdx;
	float oneOverZ;
	float dOneOverZ;
	float uOverZ;
	float dUOverZ;
	float vOverZ;
	float dVOverZ;
	int u;
	int du;
	int v;
	int dv;
	int intensity;
	int dIntensity;
	int y;
};

typedef RdNRasterEdge RdZRasterEdge;

class RenderDroid {
public:
	RenderDroid(StdMath *stdMath, StdDisplay *stdDisplay, StdConffile *stdConffile, StdFile *stdFile, bool isDemo, bool isRemastered = false);
	~RenderDroid();

	int startup(SysStruct *hs);
	void shutdown();
	int open(int acceleration);
	int close();
	int setRenderOptions(int options);
	int setGeometryMode(int mode);
	int setLightingMode(int mode);
	int setTextureMode(int mode);
	int setSortingMethod(int method);
	int setOcclusionMethod(int method);
	int setZBufferMethod(int method);
	int advanceFrame();
	int getFrameTrue() const { return _frameTrue; }
	bool isDemo() const { return _demo; }
	SysStruct *getHs() const { return _hs; }
	RemasteredLookRenderer *getRemasteredLookRenderer() const { return _remasteredLookRenderer; }
	void setRemasteredLookRenderer(RemasteredLookRenderer *remasteredLookRenderer) { _remasteredLookRenderer = remasteredLookRenderer; }
	void finishFrame();
	void clearPreStatistics();
	int beginHardwareScene();
	void setColorInfo(const StdColorFormat &colorInfo);
	void setStd3D(Std3D *std3D);
	int getAcceleration() const;
	void setAcceleration(int acceleration);

	void cross3(RdVector3 out, RdVector3 a, RdVector3 b);
	float dot2(RdVector2 a, RdVector2 b);
	float dot3(RdVector3 a, RdVector3 b);
	void extractAngles(RdVector3 vec, RdVector3 pyr);
	float normalize3(RdVector3 out, RdVector3 in);
	float normalize3Acc(RdVector3 vec);
	float normalize3QuickAcc(RdVector3 vec);
	void rotate3Acc(RdVector3 vec, RdVector3 pyr);

	void calcSurfaceNormal(RdVector3 out, RdVector3 p0, RdVector3 p1, RdVector3 p2);
	void clampVector(RdVector3 vec, float eps);
	int pointsCollinear(RdVector3 a, RdVector3 b, RdVector3 c);

	void build34(RdMatrix34 out, RdVector3 pyr, RdVector3 pos);
	void buildFromVectorAngle34(RdMatrix34 out, RdVector3 axis, float angle);
	void buildRotate34(RdMatrix34 out, RdVector3 pyr);
	void buildScale34(RdMatrix34 out, RdVector3 scale);
	void buildTranslate34(RdMatrix34 out, RdVector3 pos);
	void extractAngles34(RdMatrix34 mat, RdVector3 pyr);
	void identity34(RdMatrix34 out);
	void invertOrtho34(RdMatrix34 out, RdMatrix34 in);
	void lookAt(RdMatrix34 out, RdVector3 pos, RdVector3 target, float roll);
	void multiply34(RdMatrix34 out, RdMatrix34 a, RdMatrix34 b);
	void postMultiply34(RdMatrix34 dst, RdMatrix34 src);
	void postTranslate34(RdMatrix34 mat, RdVector3 vec);
	void preMultiply34(RdMatrix34 dst, RdMatrix34 src);
	void preRotate34(RdMatrix34 mat, RdVector3 pyr);
	void preScale34(RdMatrix34 mat, RdVector3 scale);
	void transformPoint34(RdVector3 out, RdVector3 pt, RdMatrix34 mat);
	void transformPoint34Acc(RdVector3 pt, RdMatrix34 mat);
	void transformPointLst34(RdMatrix34 mat, RdVector3 *in, RdVector3 *out, uint num);
	void transformVector34(RdVector3 out, RdVector3 vec, RdMatrix34 mat);
	void transformVector34Acc(RdVector3 vec, RdMatrix34 mat);
	void transformVectorOrtho34(RdVector3 out, RdVector3 vec, RdMatrix34 mat);

	void cross3Remastered(RdVector3 out, RdVector3 a, RdVector3 b);
	float dot2Remastered(RdVector2 a, RdVector2 b);
	float dot3Remastered(RdVector3 a, RdVector3 b);
	float normalize2Remastered(float *vec);
	float normalize3Remastered(RdVector3 out, RdVector3 in);
	float normalize3AccRemastered(RdVector3 vec);
	int pointsCollinearRemastered(RdVector3 a, RdVector3 b, RdVector3 c);

	void buildFromVectorAngle34Remastered(RdMatrix34 out, RdVector3 axis, float angle);
	void extractAngles34Remastered(RdMatrix34 mat, RdVector3 pyr);
	void transformPoint34Remastered(RdVector3 out, RdVector3 pt, RdMatrix34 mat);
	void transformPoint34AccRemastered(RdVector3 pt, RdMatrix34 mat);
	void transformVector34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat);
	void transformVector34AccRemastered(RdVector3 vec, RdMatrix34 mat);
	void transformVectorOrtho34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat);

	class Module {
	protected:
		explicit Module(RenderDroid *rd) : _rd(rd) {}
		RenderDroid *_rd;
	};

	class CameraModule : public Module {
	public:
		explicit CameraModule(RenderDroid *rd) : Module(rd), _curCamera(nullptr) {}
		RdCamera *getCurrent() const { return _curCamera; }
		void getPosition(RdVector3 out) const { out[0] = _camMatrix[3][0]; out[1] = _camMatrix[3][1]; out[2] = _camMatrix[3][2]; }
		void getMatrix(RdMatrix34 out) const { memcpy(out, _camMatrix, sizeof(RdMatrix34)); }
		RdCamera *create(float fov, int clipFar, float zNear, float zFar, float aspectRatio);
		int newEntry(RdCamera *camera, float fov, int clipFar, float zNear, float zFar, float aspectRatio);
		void destroy(RdCamera *&camera);
		void freeEntry(RdCamera *camera);
		int setCanvas(RdCamera *camera, RdCanvas *canvas);
		int setCurrent(RdCamera *camera);
		int setFOV(RdCamera *camera, float fov);
		int setProjectType(RdCamera *camera, int projectType);
		int setAspectRatio(RdCamera *camera, float aspectRatio);
		int buildFOV(RdCamera *camera);
		void update(RdMatrix34 mat);
		void orthoProject(RdVector3 out, RdVector3 in);
		void orthoProjectLst(RdVector3 *out, RdVector3 *in, int num);
		void orthoProjectSquare(RdVector3 out, RdVector3 in);
		void orthoProjectSquareLst(RdVector3 *out, RdVector3 *in, int num);
		void perspProject(RdVector3 out, RdVector3 in);
		void perspProjectLst(RdVector3 *out, RdVector3 *in, int num);
		void perspProjectSquare(RdVector3 out, RdVector3 in);
		void perspProjectSquareLst(RdVector3 *out, RdVector3 *in, int num);
		void setAmbientLight(RdCamera *camera, float ambient);
		int addLight(RdCamera *camera, RdLight *light, RdVector3 *pos);
		void advanceFrame();

	private:
		friend class RenderDroid;
		RdCamera *_curCamera;
		RdMatrix34 _camMatrix;
		RdVector3 _camPYR;
	};

	class CanvasModule : public Module {
	public:
		explicit CanvasModule(RenderDroid *rd) : Module(rd) {}
		RdCanvas *create(uint flags, StdVBuffer *vBuffer, StdVBuffer *zBuffer, int xStart, int yStart, int xEnd, int yEnd, int a8);
		int newEntry(RdCanvas *canvas, uint flags, StdVBuffer *vBuffer, StdVBuffer *zBuffer, int xStart, int yStart, int xEnd, int yEnd, int a9);
		void destroy(RdCanvas *&canvas);
	};

	class ClipModule : public Module {
	public:
		explicit ClipModule(RenderDroid *rd) : Module(rd), _faceStatus(0) {}
		int line2(RdCanvas *canvas, int *x0, int *y0, int *x1, int *y1);
		int calcOutcode2(RdCanvas *canvas, int x, int y);
		int point3(RdClipFrustum *clip, RdVector3 pt);
		int point3Remastered(RdClipFrustum *clip, RdVector3 pt);
		int line3(RdClipFrustum *clip, RdVector3 v0, RdVector3 v1, int *clipped0, int *clipped1);
		int face3W(RdClipFrustum *clip, RdVector3 *verts, int numVerts);
		int face3WOrtho(RdClipFrustum *clip, RdVector3 *verts, int numVerts);
		int face3S(RdClipFrustum *clip, RdVector3 *verts, int numVerts);
		int face3SOrtho(RdClipFrustum *clip, RdVector3 *verts, int numVerts);
		int face3GS(RdClipFrustum *clip, RdVector3 *verts, float *intensities, int numVerts);
		int face3GSOrtho(RdClipFrustum *clip, RdVector3 *verts, float *intensities, int numVerts);
		int face3T(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, int numVerts);
		int face3TOrtho(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, int numVerts);
		int face3GT(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts);
		int face3GTOrtho(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts);
		int sphereInFrustrum(RdClipFrustum *clip, RdVector3 center, float radius);
		int faceToPlane(RdVector3 norm, RdVector3 pt, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts);

	private:
		friend class RenderDroid;
		int clipFace(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts, bool guard, bool ortho);
		int clipPerspectiveX(const RdClipPoly *src, int count, RdClipPoly *dst, float slope, bool keepGreater, uint status);
		int clipPerspectiveZ(const RdClipPoly *src, int count, RdClipPoly *dst, float slope, bool keepGreater, uint status);
		int clipOrtho(const RdClipPoly *src, int count, RdClipPoly *dst, int axis, float bound, bool keepGreater, uint status);
		int clipDepth(const RdClipPoly *src, int count, RdClipPoly *dst, float plane, bool keepGreater, uint status);
		void copyVertex(const RdClipPoly *src, int srcIdx, RdClipPoly *dst, int dstIdx);
		void interpolateAttributes(const RdClipPoly *src, int prevIdx, int curIdx, RdClipPoly *dst, int dstIdx, double t);
		bool canAddVertex(int count);

		RdVector3 _vertBufferA[64];
		RdVector2 _workTVerts[64];
		float _workIVerts[64];
		uint _faceStatus;
	};

	class ColormapModule : public Module {
	public:
		// The colormaps of the demo have a light level table after the colors
		ColormapModule(RenderDroid *rd, bool isDemo) : Module(rd), _curMap(nullptr), _identityMap(nullptr), _demoFormat(isDemo) {}
		RdColormap *getCurrent() const { return _curMap; }
		RdColormap *getIdentity() const { return _identityMap; }
		RdColormap *load(char *path);
		int loadEntry(char *path, RdColormap *colormap);
		void destroy(RdColormap *&colormap);
		void freeEntry(RdColormap *colormap);
		int setCurrent(RdColormap *colormap);
		int setIdentity(RdColormap *colormap);
		int buildRGB16(byte *dst, byte *palette, byte r, byte g, byte b, StdColorFormat *colorFormat);
		int buildGrayRamp(RdColormap *colormap);
		int size(RdColormap *colormap);

	private:
		friend class RenderDroid;
		RdColormap *_curMap;
		RdColormap *_identityMap;
		bool _demoFormat;
	};

	class ColorEffectsModule : public Module {
	public:
		explicit ColorEffectsModule(RenderDroid *rd) : Module(rd) {}
		void clear(RdColorEffects *effects);
	};

	class FaceModule : public Module {
	public:
		explicit FaceModule(RenderDroid *rd) : Module(rd) {}
		int newEntry(RdFace3 *face);
		void freeEntry(RdFace3 *face);
	};

	class KeyframeModule : public Module {
	public:
		explicit KeyframeModule(RenderDroid *rd) : Module(rd) {}
		RdKeyframe *load(char *path);
		int loadEntry(char *path, RdKeyframe *keyframe);
		int loadHeader(Common::SeekableReadStream *file, RdKeyframe *keyframe);
		void newEntry(RdKeyframe *keyframe);
		void destroy(RdKeyframe *&keyframe);
		void freeEntry(RdKeyframe *keyframe);
	};

	class LightModule : public Module {
	public:
		explicit LightModule(RenderDroid *rd) : Module(rd) {}
		int newEntry(RdLight *light);
		void setAngles(RdLight *light, float angleX, float angleY);
		float calcVertexIntensities(RdLight **lights, RdVector3 *lightPos, RdVector3 *lightDir, uint numLights, RdVector3 *normals, RdVector3 *vertices, float *staticIntensities, float *dynamicIntensities, uint numVertices, float attenuation);
		float calcFaceIntensity(RdLight **lights, RdVector3 *lightPos, RdVector3 *lightDir, uint numLights, RdFace3 *face, RdVector3 normal, RdVector3 *vertices, float attenuation);
	};

	class MaterialModule : public Module {
	public:
		explicit MaterialModule(RenderDroid *rd) : Module(rd), _loader(nullptr), _unloader(nullptr) {}
		RdMaterialLoaderFunc registerLoader(RdMaterialLoaderFunc loader);
		RdMaterialUnloaderFunc registerUnloader(RdMaterialUnloaderFunc unloader);
		RdMaterial *load(char *path, int videoMemory, int videoMemoryOnly);
		int loadEntry(char *path, RdMaterial *material, int videoMemory, int videoMemoryOnly);
		int destroy(RdMaterial *&material);
		void freeEntry(RdMaterial *material);
		int addToTextureCache(RdMaterial *material, RdTexture *texture, int mipLevel, int alt);

	private:
		RdMaterialLoaderFunc _loader;
		RdMaterialUnloaderFunc _unloader;
	};

	class Model3Module : public Module {
	public:
		explicit Model3Module(RenderDroid *rd) : Module(rd), _curThing(nullptr), _curModel3(nullptr), _curGeo(nullptr), _curMesh(nullptr), _thingFrustrumCull(0), _meshFrustrumCull(0),
				_curGeometryMode(0), _curLightingMode(0), _curTextureMode(0), _geometryMode(0), _lightingMode(0), _textureMode(0),
				_numGeoLights(0), _numMeshLights(0), _numDrawnModels(0), _clipPlaneActive(0), _maxGeoRadius(0.0f) {
			memset(_geoLights, 0, sizeof(_geoLights));
			memset(_meshLights, 0, sizeof(_meshLights));
			memset(&_vertexSrc, 0, sizeof(_vertexSrc));
			memset(&_vertexDst, 0, sizeof(_vertexDst));
			memset(_clipNorm, 0, sizeof(_clipNorm));
			memset(_clipPt, 0, sizeof(_clipPt));
			memset(_localCamera, 0, sizeof(_localCamera));
			_clipNorm[2] = 1.0f;
		}
		void clearFrameCounters();
		void activateClipPlane(int onOff);
		void setClipPlane(RdVector3 norm, RdVector3 pt);
		int newEntry(RdModel3 *model3);
		RdModel3 *load(char *filename);
		int loadEntry(char *filename, RdModel3 *model3);
		int loadEntryText(char *filename, RdModel3 *model3);
		int loadEntryBinary(Common::SeekableReadStream *file, RdModel3 *model3);
		void loadPostProcess(RdModel3 *model3);
		int validate(RdModel3 *model3);
		void destroy(RdModel3 *&model3);
		void freeEntry(RdModel3 *model3);
		void buildShadowModel(RdModel3 *model3);
		void calcBoundingBoxes(RdModel3 *model3);
		void calcModelBoundingBox(RdModel3 *model3, RdModel3HNode *hNode, RdMatrix34 parentMatrix);
		void calcRadii(RdModel3 *model3);
		void buildExpandedRadius(RdModel3 *model3, RdModel3HNode *hNode, RdMatrix34 parentMatrix);
		void calcNodeDepths(RdModel3 *model3);
		void calcFaceNormals(RdModel3 *model3);
		void calcVertexNormals(RdModel3 *model3);
		int getMeshMatrix(RdThing *thing, RdMatrix34 orient, uint nodeNum, RdMatrix34 meshOrient);
		int draw(RdThing *thing, RdMatrix34 mat, RdThing *parentThing = nullptr);
		void drawHNode(RdModel3HNode *hNode);
		void drawMesh(RdModel3Mesh *mesh, RdMatrix34 mat);
		int drawFace(RdFace3 *face, int backface);
		int drawPolyline(RdThing *thing, RdMatrix34 mat);
		int drawParticle(RdThing *thing, RdMatrix34 mat);

	private:
		friend class RenderDroid;
		int hasBytes(Common::SeekableReadStream *file, uint count, uint elementSize);

		RdThing *_curThing;
		RdModel3 *_curModel3;
		RdModel3Geo *_curGeo;
		RdModel3Mesh *_curMesh;
		int _thingFrustrumCull;
		int _meshFrustrumCull;
		int _curGeometryMode;
		int _curLightingMode;
		int _curTextureMode;
		int _geometryMode;
		int _lightingMode;
		int _textureMode;
		uint _numGeoLights;
		uint _numMeshLights;
		RdLight *_geoLights[64];
		RdLight *_meshLights[64];
		RdVector3 _localLightPos[64];
		RdVector3 _localLightDir[64];
		RdVector3 _view[512];
		RdVector3 _faceVerts[32];
		RdVector3 _localCamera;
		RdVertexLists _vertexSrc;
		RdVertexLists _vertexDst;
		uint _numDrawnModels;
		int _clipPlaneActive;
		RdVector3 _clipNorm;
		RdVector3 _clipPt;
		float _maxGeoRadius;
		RdVector3 _particleTransformed[256];
		float _particleVerts[12];
		float _polylineVerts[12];
	};

	class Primit2Module : public Module {
	public:
		explicit Primit2Module(RenderDroid *rd) : Module(rd) {}
		void drawLine(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern);
		int drawClippedLine(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern);
		void drawCircle(RdCanvas *canvas, int cx, int cy, float radius, float angleStep, uint16 color, uint pattern);
		void drawRectangle(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern);

	private:
		void plot(RdCanvas *canvas, int x, int y, uint16 color);
		void drawBresenham(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern);
	};

	class Primit3Module : public Module {
	public:
		Primit3Module(RenderDroid *rd, bool isDemo) : Module(rd), _demo(isDemo) {}
		RdProcFace *drawPolygon(RdVector3 *verts, int numVerts, RdMaterial *material);
		int getScreenCoord(RdVector3 pt, int *screen);
		int drawLine(RdVector3 p0, RdVector3 p1, int *screen0, int *screen1, int *clipped0, int *clipped1, uint16 color, uint pattern);
		void drawPoint(RdVector3 pt, float radius, float angleStep, uint16 color, uint pattern);
		int clipFace(RdClipFrustum *clip, int geometryMode, int lightingMode, int textureMode, RdVertexLists *src, RdVertexLists *dst, RdVector2 uvDelta);
		int noClipFace(int geometryMode, int lightingMode, int textureMode, RdVertexLists *src, RdVertexLists *dst, RdVector2 uvDelta);
		void clearFrameCounters();
		int drawPolylineFace(RdThing *thing, RdFace3 *face, RdVector3 *verts, RdVertexLists *src);

	private:
		friend class RenderDroid;
		RdVector3 _work[32];
		bool _demo;
	};

	class PuppetModule : public Module {
	public:
		// The demo puppets have 4 tracks, the retail ones 6
		PuppetModule(RenderDroid *rd, bool isDemo) : Module(rd), _numTracks(isDemo ? 4 : 6) {}
		int getNumTracks() const { return _numTracks; }
		RdPuppet *create(RdThing *thing);
		int newEntry(RdPuppet *puppet, RdThing *thing);
		int addTrack(RdPuppet *puppet, RdKeyframe *keyframe, int priority, int priorityMasked);
		int removeTrack(RdPuppet *puppet, int trackNum);
		int setStatus(RdPuppet *puppet, int trackNum, uint status);
		void setCallback(RdPuppet *puppet, int trackNum, RdPuppetTrackCallback callback);
		int playTrack(RdPuppet *puppet, int trackNum);
		int fadeInTrack(RdPuppet *puppet, int trackNum, float time);
		int fadeOutTrack(RdPuppet *puppet, int trackNum, float time);
		void advanceTrack(RdPuppet *puppet, int trackNum, float delta);
		int updateTracks(RdPuppet *puppet, float delta);
		int resetTrack(RdPuppet *puppet, int trackNum);
		void buildJointMatrices(RdThing *thing, RdMatrix34 base);

	private:
		int _numTracks;
	};

	class SpriteModule : public Module {
	public:
		explicit SpriteModule(RenderDroid *rd) : Module(rd) {}
		RdSprite *create(int type, char *name, char *matFilename, float width, float height, int geometryMode, int lightingMode, int textureMode, float extraLight, RdVector3 *offset);
		int newEntry(RdSprite *sprite, char *name, int type, char *matFilename, float width, float height, int geometryMode, int lightingMode, int textureMode, float extraLight, RdVector3 *offset);
		void destroy(RdSprite *&sprite);
		void freeEntry(RdSprite *sprite);
		int draw(RdThing *thing, RdMatrix34 mat);

	private:
		friend class RenderDroid;
		float _spriteVerts[12];
		RdVector3 _spriteWork[32];
	};

	class ThingModule : public Module {
	public:
		explicit ThingModule(RenderDroid *rd) : Module(rd) {}
		RdThing *create(void *userArg);
		int newEntry(RdThing *thing, void *userArg);
		void destroy(RdThing *&thing);
		void freeEntry(RdThing *thing);
		int setModel3(RdThing *thing, RdModel3 *model3);
		int setCamera(RdThing *thing, RdCamera *camera);
		int setSprite3(RdThing *thing, RdSprite *sprite);
		int draw(RdThing *thing, RdMatrix34 mat, RdThing *parentThing = nullptr);
		void accumulateMatrices(RdThing *thing, RdModel3HNode *hNode, RdMatrix34 parent);
	};

	class ActiveModule : public Module {
	public:
		explicit ActiveModule(RenderDroid *rd);
		~ActiveModule();
		int startup();
		int advanceFrame();
		void clearFrameCounters();
		uint drawScene();
		int buildSpans();
		uint flushSpans();
		uint buildEdges();
		int addActiveFace(RdProcFace *procFace);

	private:
		friend class RenderDroid;
		void addSpan(RdActiveFace *face, uint alignedEndX);
		RdActiveEdge *_activeEdges;
		RdActiveFace *_activeFaces;
		RdActiveSpan *_activeSpans;
		RdActiveEdge *_activeEdgeHead;
		RdActiveEdge *_activeEdgeTail;
		RdActiveEdge *_newActiveEdges[1024];
		RdActiveEdge *_removeActiveEdges[1024];
		uint _drawnFaces;
		uint _numActiveEdges;
		uint _numActiveFaces;
		uint _numActiveSpans;
		uint _yCurScanLine;
		uint _yMaxEdge;
		uint _yMinEdge;
	};

	class CacheModule : public Module {
	public:
		explicit CacheModule(RenderDroid *rd);
		~CacheModule();
		int *getViewportRect() { return _viewportRect; }
		RdProcFace *getProcFaces() { return _procFaces; }
		size_t getNumProcFaces() const { return _numProcFaces; }
		RdMatCel *selectCel(const RdProcFace *procFace);
		float calcFaceLight(const RdProcFace *procFace, int lightingMode, float ambientLight);
		void calcVertexLights(RdProcFace *procFace, float ambientLight);
		int startup();
		int finishFrame();
		int reset();
		void clearFrameCounters();
		RdProcFace *getProcEntry();
		int flush();
		int addProcFace(uint flags, uint numVertices, char bufferFlags);
		int drawFaceN(RdProcFace *procFace);
		int drawFaceZ(RdProcFace *procFace);
		int drawFaceUser(RdProcFace *procFace);
		static int procFaceCompare(const void *a, const void *b);
		int sendFaceListToHardware();
		int resetRenderList();
		uint drawRenderList();
		static int hwTriCompare(const void *a, const void *b);

	private:
		friend class RenderDroid;
		float _vertices[98304];
		float _texVertices[65536];
		float _intensities[32768];
		float _greenIntensities[32768];
		float _blueIntensities[32768];
		RdProcFace _procFaces[1024];
		size_t _numProcFaces;
		uint _numUsedVertices;
		uint _numUsedTexVertices;
		uint _numUsedIntensities;
		uint _drawnFaces;
		int _viewportRect[4];
		Std3DVertex *_hwVertices;
		Std3DTri *_hwNormalTris;
		Std3DTri *_hwSolidTris;
		uint _totalNormalTris;
		uint _totalSolidTris;
		uint _totalVerts;
	};

	class RasterModule : public Module {
	public:
		explicit RasterModule(RenderDroid *rd) : Module(rd), _fixedScale(65536.0f) {}
		void startup();
		float getOneOverNFlex(int index) const { return _oneOverNFlex[index]; }
		int getOneOverNFixed(int index) const { return _oneOverNFixed[index]; }

	private:
		friend class RenderDroid;
		int _oneOverNFixed[2048];
		float _oneOverNFixedF[16];
		float _oneOverNFlex[2048];
		float _fixedScale;
	};

	class AFRasterModule : public Module {
	public:
		typedef void (AFRasterModule::*DrawSpanFunc)(RdActiveSpan *span);
		typedef void (AFRasterModule::*DrawFaceFunc)(RdActiveFace *face);
		typedef void (AFRasterModule::*StepEdgeFunc)(RdActiveEdge *edge);
		typedef int (AFRasterModule::*SetupEdgeFunc)(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);

		explicit AFRasterModule(RenderDroid *rd) : Module(rd), _mipMapNum(0), _texRowShift(0), _texPitch(0), _uWrap(0), _vWrap(0), _dUdXModifier(0), _dVdXModifier(0), _dOneOverZdXFixed(0), _dOneOverZdX(0.0f), _dUOverZdX(0.0f), _dVOverZdX(0.0f), _oneOverZStep(0.0f), _uOverZStep(0.0f), _vOverZStep(0.0f), _pixels(nullptr), _monoMap(nullptr), _rgbMap(nullptr), _color(0), _RGBcolor(0), _drawSpan(nullptr), _lowPrecision(false), _spanU(0), _spanV(0), _spanDU(0), _spanDV(0) {}

		void calcAffineGradients(RdActiveFace *face, int refVert);
		void calcPerspGradients(RdActiveFace *face, int refVert);

		void setupNGonLW(RdActiveFace *face, RdMatCel *cel);
		void setupNGonLS(RdActiveFace *face, RdMatCel *cel);
		void setupNGonFS(RdActiveFace *face, RdMatCel *cel, float light);
		void setupNGonGS(RdActiveFace *face, RdMatCel *cel);
		void setupNGonLAT(RdActiveFace *face, RdMatCel *cel);
		void setupNGonMLAT(RdActiveFace *face, RdMatCel *cel);
		void setupNGonFAT(RdActiveFace *face, RdMatCel *cel, float light);
		void setupNGonMFAT(RdActiveFace *face, RdMatCel *cel, float light);
		void setupNGonGAT(RdActiveFace *face, RdMatCel *cel);
		void setupNGonMGAT(RdActiveFace *face, RdMatCel *cel);
		void setupNGonLIT(RdActiveFace *face, RdMatCel *cel);
		void setupNGonMLIT(RdActiveFace *face, RdMatCel *cel);
		void setupNGonFIT(RdActiveFace *face, RdMatCel *cel, float light);
		void setupNGonMFIT(RdActiveFace *face, RdMatCel *cel, float light);
		void setupNGonGIT(RdActiveFace *face, RdMatCel *cel);
		void setupNGonMGIT(RdActiveFace *face, RdMatCel *cel);

		void drawNGonLW(RdActiveFace *face);
		void drawNGonLS(RdActiveFace *face);
		void drawNGonFS(RdActiveFace *face);
		void drawNGonGS(RdActiveFace *face);
		void drawNGonLAT(RdActiveFace *face);
		void drawNGonMLAT(RdActiveFace *face);
		void drawNGonFAT(RdActiveFace *face);
		void drawNGonMFAT(RdActiveFace *face);
		void drawNGonGAT(RdActiveFace *face);
		void drawNGonMGAT(RdActiveFace *face);
		void drawNGonLIT(RdActiveFace *face);
		void drawNGonMLIT(RdActiveFace *face);
		void drawNGonFIT(RdActiveFace *face);
		void drawNGonMFIT(RdActiveFace *face);
		void drawNGonGIT(RdActiveFace *face);
		void drawNGonMGIT(RdActiveFace *face);

		int setupEdgeNGonLW(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonLS(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonFS(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonGS(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonLAT(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonFAT(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonGAT(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonPerspective(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1, int variant);
		void setEdgePerspectiveValues(RdActiveEdge *edge, RdActiveFace *face, int v0, float dy, double xOffset, int variant);
		int setupEdgeNGonLIT(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonFIT(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
		int setupEdgeNGonGIT(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);

		void advanceEdgeNGonLS(RdActiveEdge *edge);
		void advanceEdgeNGonGS(RdActiveEdge *edge);
		void advanceLeftEdgeNGonFAT(RdActiveEdge *edge);
		void advanceLeftEdgeNGonGAT(RdActiveEdge *edge);
		void advanceLeftEdgeNGonFIT(RdActiveEdge *edge);
		void advanceRightEdgeNGonLIT(RdActiveEdge *edge);
		void advanceEdgeNGonGIT(RdActiveEdge *edge);
		void advanceLeftEdgeNGonGIT(RdActiveEdge *edge);

		void drawSpanNGonLW_8(RdActiveSpan *span);
		void drawSpanNGonLW_16(RdActiveSpan *span);
		void drawSpanNGonMLAT_8(RdActiveSpan *span);
		void drawSpanNGonMLAT_16(RdActiveSpan *span);
		void drawSpanNGonMFAT_8(RdActiveSpan *span);
		void drawSpanNGonMFAT_16(RdActiveSpan *span);
		void drawSpanNGonMGAT_8(RdActiveSpan *span);
		void drawSpanNGonMGAT_16(RdActiveSpan *span);
		void drawSpanNGonMLIT_8(RdActiveSpan *span);
		void drawSpanNGonMLIT_16(RdActiveSpan *span);
		void drawSpanNGonMFIT_8(RdActiveSpan *span);
		void drawSpanNGonMFIT_16(RdActiveSpan *span);
		void drawSpanNGonMGIT_8(RdActiveSpan *span);
		void drawSpanNGonMGIT_16(RdActiveSpan *span);

		void drawSpanNGonLAT_16(RdActiveSpan *span);
		void drawSpanNGonLAT_16_W8(RdActiveSpan *span);
		void drawSpanNGonLAT_16_W16(RdActiveSpan *span);
		void drawSpanNGonLAT_16_W32(RdActiveSpan *span);
		void drawSpanNGonLAT_16_W64(RdActiveSpan *span);
		void drawSpanNGonLAT_16_W128(RdActiveSpan *span);
		void drawSpanNGonLAT_16_W256(RdActiveSpan *span);
		void drawSpanNGonFAT_16(RdActiveSpan *span);
		void drawSpanNGonFAT_16_W8(RdActiveSpan *span);
		void drawSpanNGonFAT_16_W16(RdActiveSpan *span);
		void drawSpanNGonFAT_16_W32(RdActiveSpan *span);
		void drawSpanNGonFAT_16_W64(RdActiveSpan *span);
		void drawSpanNGonFAT_16_W128(RdActiveSpan *span);
		void drawSpanNGonFAT_16_W256(RdActiveSpan *span);
		void drawSpanNGonGAT_16(RdActiveSpan *span);
		void drawSpanNGonGAT_16_W8(RdActiveSpan *span);
		void drawSpanNGonGAT_16_W16(RdActiveSpan *span);
		void drawSpanNGonGAT_16_W32(RdActiveSpan *span);
		void drawSpanNGonGAT_16_W64(RdActiveSpan *span);
		void drawSpanNGonGAT_16_W128(RdActiveSpan *span);
		void drawSpanNGonGAT_16_W256(RdActiveSpan *span);
		void drawSpanNGonLIT_16(RdActiveSpan *span);
		void drawSpanNGonLIT_16_W8(RdActiveSpan *span);
		void drawSpanNGonLIT_16_W16(RdActiveSpan *span);
		void drawSpanNGonLIT_16_W32(RdActiveSpan *span);
		void drawSpanNGonLIT_16_W64(RdActiveSpan *span);
		void drawSpanNGonLIT_16_W128(RdActiveSpan *span);
		void drawSpanNGonLIT_16_W256(RdActiveSpan *span);
		void drawSpanNGonFIT_16(RdActiveSpan *span);
		void drawSpanNGonFIT_16_W8(RdActiveSpan *span);
		void drawSpanNGonFIT_16_W16(RdActiveSpan *span);
		void drawSpanNGonFIT_16_W32(RdActiveSpan *span);
		void drawSpanNGonFIT_16_W64(RdActiveSpan *span);
		void drawSpanNGonFIT_16_W128(RdActiveSpan *span);
		void drawSpanNGonFIT_16_W256(RdActiveSpan *span);
		void drawSpanNGonGIT_16(RdActiveSpan *span);
		void drawSpanNGonGIT_16_W8(RdActiveSpan *span);
		void drawSpanNGonGIT_16_W16(RdActiveSpan *span);
		void drawSpanNGonGIT_16_W32(RdActiveSpan *span);
		void drawSpanNGonGIT_16_W64(RdActiveSpan *span);
		void drawSpanNGonGIT_16_W128(RdActiveSpan *span);
		void drawSpanNGonGIT_16_W256(RdActiveSpan *span);

		void drawSpanNGonLAT_8(RdActiveSpan *span);
		void drawSpanNGonLAT_8_W8(RdActiveSpan *span);
		void drawSpanNGonLAT_8_W16(RdActiveSpan *span);
		void drawSpanNGonLAT_8_W32(RdActiveSpan *span);
		void drawSpanNGonLAT_8_W64(RdActiveSpan *span);
		void drawSpanNGonLAT_8_W128(RdActiveSpan *span);
		void drawSpanNGonLAT_8_W256(RdActiveSpan *span);
		void drawSpanNGonFAT_8(RdActiveSpan *span);
		void drawSpanNGonFAT_8_W8(RdActiveSpan *span);
		void drawSpanNGonFAT_8_W16(RdActiveSpan *span);
		void drawSpanNGonFAT_8_W32(RdActiveSpan *span);
		void drawSpanNGonFAT_8_W64(RdActiveSpan *span);
		void drawSpanNGonFAT_8_W128(RdActiveSpan *span);
		void drawSpanNGonFAT_8_W256(RdActiveSpan *span);
		void drawSpanNGonGAT_8(RdActiveSpan *span);
		void drawSpanNGonGAT_8_W8(RdActiveSpan *span);
		void drawSpanNGonGAT_8_W16(RdActiveSpan *span);
		void drawSpanNGonGAT_8_W32(RdActiveSpan *span);
		void drawSpanNGonGAT_8_W64(RdActiveSpan *span);
		void drawSpanNGonGAT_8_W128(RdActiveSpan *span);
		void drawSpanNGonGAT_8_W256(RdActiveSpan *span);
		void drawSpanNGonLIT_8(RdActiveSpan *span);
		void drawSpanNGonLIT_8_W8(RdActiveSpan *span);
		void drawSpanNGonLIT_8_W16(RdActiveSpan *span);
		void drawSpanNGonLIT_8_W32(RdActiveSpan *span);
		void drawSpanNGonLIT_8_W64(RdActiveSpan *span);
		void drawSpanNGonLIT_8_W128(RdActiveSpan *span);
		void drawSpanNGonLIT_8_W256(RdActiveSpan *span);
		void drawSpanNGonFIT_8(RdActiveSpan *span);
		void drawSpanNGonFIT_8_W8(RdActiveSpan *span);
		void drawSpanNGonFIT_8_W16(RdActiveSpan *span);
		void drawSpanNGonFIT_8_W32(RdActiveSpan *span);
		void drawSpanNGonFIT_8_W64(RdActiveSpan *span);
		void drawSpanNGonFIT_8_W128(RdActiveSpan *span);
		void drawSpanNGonFIT_8_W256(RdActiveSpan *span);
		void drawSpanNGonGIT_8(RdActiveSpan *span);
		void drawSpanNGonGIT_8_W8(RdActiveSpan *span);
		void drawSpanNGonGIT_8_W16(RdActiveSpan *span);
		void drawSpanNGonGIT_8_W32(RdActiveSpan *span);
		void drawSpanNGonGIT_8_W64(RdActiveSpan *span);
		void drawSpanNGonGIT_8_W128(RdActiveSpan *span);
		void drawSpanNGonGIT_8_W256(RdActiveSpan *span);

		void drawZSpan(RdActiveSpan *span);
		void drawZSpanShifted(RdActiveSpan *span);

	private:
		friend class RenderDroid;

		struct SpanRun {
			byte *dst;
			uint16 *zPointer;
			int z;
			int dz;
			int zShift;
			int u;
			int v;
			int du;
			int dv;
			int intensity;
			int dIntensity;
			int uMask;
			int vShift;
		};

		int wrapVertex(int index, int lastVertex);
		int selectRefVertex(RdProcFace *procFace, int *yMinVertPointer, int *yMaxVertPointer);
		void selectMipLevel(RdActiveFace *face, int refVert);
		int getZShift(RdCanvas *canvas);
		void drawZSpans(RdActiveFace *face);
		void drawTexturedSpans(RdActiveFace *face);
		DrawSpanFunc selectWidthSpan(int texPitch, const DrawSpanFunc *widthSpans, DrawSpanFunc defaultFunction);
		void setupFlat(RdActiveFace *face, RdMatCel *cel, DrawFaceFunc drawSpans, SetupEdgeFunc setupEdge);
		void setupTextured(RdActiveFace *face, RdMatCel *cel, bool persp, int lightKind, float light, DrawFaceFunc drawSpans, SetupEdgeFunc setupEdge);
		int setupEdgeAffine(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1, bool texture, bool intensity, StepEdgeFunc step);
		void loadPerspState(RdActiveFace *face);
		void plotMaskedRun(SpanRun &run, int count, bool bpp16, int lightKind);
		void drawSpanMaskedAffine(RdActiveSpan *span, bool bpp16, int lightKind);
		void drawSpanMaskedPersp(RdActiveSpan *span, bool bpp16, int lightKind);
		void plotRun16(SpanRun &run, int count, bool intensity);
		void plotRun8(SpanRun &run, int count, int lightKind, bool pairedIntensity);
		void plotPerspRun(SpanRun &run, int count, int lightKind, bool bpp16, bool chunk);
		void drawSpanAffine16(RdActiveSpan *span, int uMask, int vShift, bool intensity);
		void drawSpanAffine8(RdActiveSpan *span, int vShift, int lightKind);
		void drawSpanPersp16(RdActiveSpan *span, int uMask, int vShift, bool intensity);
		void drawSpanPersp(RdActiveSpan *span, int uMask, int vShift, int lightKind, bool bpp16);
		double fpuRound(double value);
		double fpuAdd(double a, double b);
		double fpuSub(double a, double b);
		double fpuMul(double a, double b);
		double fpuDiv(double a, double b);

		int _mipMapNum;
		int _texRowShift;
		int _texPitch;
		int _uWrap;
		int _vWrap;
		int _dUdXModifier;
		int _dVdXModifier;
		int _dOneOverZdXFixed;
		float _dOneOverZdX;
		float _dUOverZdX;
		float _dVOverZdX;
		float _oneOverZStep;
		float _uOverZStep;
		float _vOverZStep;
		byte *_pixels;
		byte *_monoMap;
		byte *_rgbMap;
		byte _color;
		uint16 _RGBcolor;
		DrawSpanFunc _drawSpan;
		bool _lowPrecision;
		int _spanU;
		int _spanV;
		int _spanDU;
		int _spanDV;
	};

	class NRasterModule : public Module {
	public:
		explicit NRasterModule(RenderDroid *rd);

		void calcAffineGradients(float z);
		void calcPerspGradients(float z);

		int drawNGonLV(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonLW(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonLS(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonTLS(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonFS(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonTFS(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonGS(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonTGS(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonLAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonTLAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMLAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMTLAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonTFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonMFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonMTFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonGAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonTGAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMGAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMTGAT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonLIT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonTLIT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMLIT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMTLIT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonTFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonMFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonMTFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		int drawNGonGIT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonTGIT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMGIT(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonMTGIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonLS_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonFS_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonLS_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTLS_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTLS_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTFS_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonGS_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonGS_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTGS_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTGS_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonLAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonLAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTLAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTLAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMLAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMLAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTLAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTLAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonFAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTFAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTFAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMFAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMFAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTFAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTFAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonGAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTGAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTGAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMGAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMGAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTGAT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTGAT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonLIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTLIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTLIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMLIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMLIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTLIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTLIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonFIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTFIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTFIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMFIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMFIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTFIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTFIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonGIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTGIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonTGIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMGIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMGIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTGIT_8(RdNRasterEdge *left, RdNRasterEdge *right);
		void drawScanLineNGonMTGIT_16(RdNRasterEdge *left, RdNRasterEdge *right);
		int setupLeftEdgeNGonLS(RdNRasterEdge *edge);
		int setupRightEdgeNGonLS(RdNRasterEdge *edge);
		int setupLeftEdgeNGonFS(RdNRasterEdge *edge);
		int setupRightEdgeNGonFS(RdNRasterEdge *edge);
		int setupLeftEdgeNGonGS(RdNRasterEdge *edge);
		int setupRightEdgeNGonGS(RdNRasterEdge *edge);
		int setupLeftEdgeNGonLAT(RdNRasterEdge *edge);
		int setupRightEdgeNGonLAT(RdNRasterEdge *edge);
		int setupLeftEdgeNGonFAT(RdNRasterEdge *edge);
		int setupRightEdgeNGonFAT(RdNRasterEdge *edge);
		int setupLeftEdgeNGonGAT(RdNRasterEdge *edge);
		int setupRightEdgeNGonGAT(RdNRasterEdge *edge);
		int setupLeftEdgeNGonLIT(RdNRasterEdge *edge);
		int setupRightEdgeNGonLIT(RdNRasterEdge *edge);
		int setupLeftEdgeNGonFIT(RdNRasterEdge *edge);
		int setupRightEdgeNGonFIT(RdNRasterEdge *edge);
		int setupLeftEdgeNGonGIT(RdNRasterEdge *edge);
		int setupRightEdgeNGonGIT(RdNRasterEdge *edge);

	private:
		friend class RenderDroid;

		enum Geometry {
			kGeometrySolid,
			kGeometryAffine,
			kGeometryPerspective
		};

		enum Lighting {
			kLightingNone,
			kLightingFlat,
			kLightingGouraud
		};

		enum Blend {
			kBlendOpaque,
			kBlendAlpha,
			kBlendMasked,
			kBlendMaskedAlpha
		};

		enum EdgeKind {
			kEdgeFlat,
			kEdgeGouraud,
			kEdgeAffine,
			kEdgeAffineFlat,
			kEdgeAffineGouraud,
			kEdgePerspective,
			kEdgePerspectiveFlat,
			kEdgePerspectiveGouraud
		};

		struct SpanPlot {
			Lighting lighting;
			Blend blend;
			int bpp;
			bool perspective;
			byte *dest8;
			uint16 *dest16;
			const byte *backdrop;
		};

		struct SpanRun {
			int u;
			int v;
			int uStep;
			int vStep;
			int intensity;
			int dIntensity;
		};

		int ftol(double value);
		int floorToInt(double value);
		int fixedMultiply(int a, int b);
		int ceilToPixel(int x);
		int wrapVertex(int index);
		float lowRound(double value);
		float lowAdd(float a, float b);
		float lowSub(float a, float b);
		float lowMul(float a, float b);
		float lowDiv(float a, float b);

		int selectGradientVertex();
		void selectMipLevel(float z);

		int setupEdge(RdNRasterEdge *edge, int step, EdgeKind kind);
		EdgeKind getEdgeKind(Geometry geometry, Lighting lighting, bool isLeft);
		void advanceEdge(RdNRasterEdge *edge, Geometry geometry, Lighting lighting, bool isLeft);

		void setupFace(RdProcFace *procFace);
		void setupSolidColors(RdProcFace *procFace, RdMatCel *cel, float light, int bpp, Lighting lighting, Blend blend);
		bool setupTexture(RdProcFace *procFace, RdMatCel *cel, float light, int bpp, Geometry geometry, Lighting lighting, Blend blend);
		int drawNGon(RdProcFace *procFace, RdMatCel *cel, float light, Geometry geometry, Lighting lighting, Blend blend);

		void drawScanLine(RdNRasterEdge *left, RdNRasterEdge *right, Geometry geometry, Lighting lighting, Blend blend, int bpp);
		void drawScanLineSolid(RdNRasterEdge *left, RdNRasterEdge *right, Lighting lighting, Blend blend, int bpp);
		void drawScanLineAffine(RdNRasterEdge *left, RdNRasterEdge *right, Lighting lighting, Blend blend, int bpp);
		void drawScanLinePerspective(RdNRasterEdge *left, RdNRasterEdge *right, Lighting lighting, Blend blend, int bpp);
		int calcSpan(RdNRasterEdge *left, RdNRasterEdge *right, int *xStart);
		void calcSpanIntensity(RdNRasterEdge *left, RdNRasterEdge *right, int width, int subPixel, int *intensity, int *dIntensityPointer);
		bool beginSpanPlot(SpanPlot *plot, int y, int xStart, Lighting lighting, Blend blend, int bpp, bool perspective, bool bufferedDestination);
		void plotPixel(SpanPlot *plot, byte texel, int intensity);
		byte lightTexel(Lighting lighting, byte texel, int intensity);
		byte fetchTexel(int u, int v);
		byte fetchTexelByRows(int u, int v);
		void startRun(SpanRun *run, float u, float v, float uStepFixed, float vStepFixed);
		void plotRun(SpanPlot *plot, SpanRun *run, int count);

		uint _numVertices;
		int _yMinVert;
		int _yMaxVert;
		int _leftVertexStep;
		int _rightVertexStep;
		float _oneOverZ[32];
		float _uOverZ[32];
		float _vOverZ[32];
		RdVector3 *_vertices;
		RdVector2 *_texVertices;
		float *_intensities;
		byte *_dest;
		uint16 *_wDest;
		byte *_pixels;
		int _mipMapNum;
		int _maxMipMap;
		int _texPitch;
		int _texRowShift;
		int _uWrap;
		int _vWrap;
		int _uStep;
		int _vStep;
		int _fixU;
		int _fixV;
		float _dOneOverZdX;
		float _dOneOverZdY;
		float _dUOverZdX;
		float _dUOverZdY;
		float _dVOverZdX;
		float _dVOverZdY;
		int _dUdXModifier;
		int _dVdXModifier;
		float _oneOverZStep;
		float _uOverZStep;
		float _vOverZStep;
		byte *_alphaMap;
		byte *_monoMap;
		uint16 *_rgbMap;
		byte *_rgbToPalette;
		uint16 _RGBcolor;
		byte _color;
	};

	class ZRasterModule : public Module {
	public:
		explicit ZRasterModule(RenderDroid *rd);
		void calcAffineGradients(float z);
		void calcPerspGradients(float z);
		int drawNGonLV(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonLW(RdProcFace *procFace, RdMatCel *cel);
		int drawNGonLS(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonLS_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonLS_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonLS(RdZRasterEdge *edge);
		int setupRightEdgeNGonLS(RdZRasterEdge *edge);
		int setupLeftEdgeNGonFS(RdZRasterEdge *edge);
		int setupRightEdgeNGonFS(RdZRasterEdge *edge);
		int drawNGonTLS(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonTLS_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTLS_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonFS(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonFS_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonFS_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonTFS(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonTFS_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTFS_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonGS(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonGS_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonGS_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonGS(RdZRasterEdge *edge);
		int setupRightEdgeNGonGS(RdZRasterEdge *edge);
		int drawNGonTGS(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonTGS_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTGS_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonLAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonLAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonLAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonTLAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonTLAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTLAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMLAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMLAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMLAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMTLAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMTLAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMTLAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonLAT(RdZRasterEdge *edge);
		int setupRightEdgeNGonLAT(RdZRasterEdge *edge);
		int drawNGonFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonFAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonFAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonTFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonTFAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTFAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonMFAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMFAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMTFAT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonMTFAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMTFAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonFAT(RdZRasterEdge *edge);
		int setupRightEdgeNGonFAT(RdZRasterEdge *edge);
		int drawNGonGAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonGAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonGAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonTGAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonTGAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTGAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMGAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMGAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMGAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMTGAT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMTGAT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMTGAT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonGAT(RdZRasterEdge *edge);
		int setupRightEdgeNGonGAT(RdZRasterEdge *edge);
		int drawNGonLIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonLIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonLIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonTLIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonTLIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTLIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMLIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMLIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMLIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMTLIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMTLIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMTLIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonLIT(RdZRasterEdge *edge);
		int setupRightEdgeNGonLIT(RdZRasterEdge *edge);
		int drawNGonFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonFIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonFIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonTFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonTFIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTFIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonMFIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMFIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMTFIT(RdProcFace *procFace, RdMatCel *cel, float light);
		void drawScanLineNGonMTFIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMTFIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonFIT(RdZRasterEdge *edge);
		int setupRightEdgeNGonFIT(RdZRasterEdge *edge);
		int drawNGonGIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonGIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonGIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonTGIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonTGIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonTGIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMGIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMGIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMGIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int drawNGonMTGIT(RdProcFace *procFace, RdMatCel *cel);
		void drawScanLineNGonMTGIT_8(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawScanLineNGonMTGIT_16(RdZRasterEdge *left, RdZRasterEdge *right);
		int setupLeftEdgeNGonGIT(RdZRasterEdge *edge);
		int setupRightEdgeNGonGIT(RdZRasterEdge *edge);
		void drawNGonLAT_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_Case7(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_Case7(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_Case6(RdZRasterEdge *left, RdZRasterEdge *right);

		void drawNGonLAT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLAT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFAT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGAT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonLIT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonFIT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_8_Case0(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_8_Case1(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_8_Case2(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_8_Case3(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_8_Case4(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_8_Case5(RdZRasterEdge *left, RdZRasterEdge *right);
		void drawNGonGIT_8_Case6(RdZRasterEdge *left, RdZRasterEdge *right);

	private:
		friend class RenderDroid;

		enum Mapping {
			kSolid,
			kAffine,
			kPerspective
		};

		enum Shading {
			kUnlit,
			kFlat,
			kGouraud
		};

		enum {
			kTransparent = 1,
			kMasked = 2
		};

		struct PixelMode {
			int bpp;
			int shading;
			int flags;
			bool perspective;
		};

		struct ScanParams {
			PixelMode mode;
			int uMask;
			int vShift;
			int lightMask;
			bool caseMode;
		};

		struct ScanState {
			int xLeft;
			int xStart;
			int x;
			int numPixels;
			byte *dest;
			uint16 *zPointer;
			int z;
			int zStep;
			int intensity;
			int intensityStep;
			int u;
			int v;
			int uStep;
			int vStep;
			bool dither;
			int ditherX;
			int ditherY;
		};

		bool setZScaleShift();
		float truncToFloat(double value);
		int toFixed(double value);
		int wrapVertex(int index, int last);
		void selectGradientVertices(int *vertA, int *vertB, int *refVertPointer);
		void selectMipLevel(float z);
		int gradientModifier(double dX, double dY);
		bool findEdgeSegment(RdZRasterEdge *edge, int vertexStep, uint *vertIdxPointer, uint *nextIdxPointer, float *yStartPointer, int *numLinesPointer);
		int interpolateEdgeValue(float from, float to, float invDy, float yOffset, int *step);
		int setupEdgeLinear(RdZRasterEdge *edge, int vertexStep, bool textured, bool gouraud);
		int setupLeftEdgePerspective(RdZRasterEdge *edge, int shading);
		int setupRightEdgePerspective(RdZRasterEdge *edge, bool gouraud);
		int setupLeftEdge(RdZRasterEdge *edge, int mapping, int shading);
		int setupRightEdge(RdZRasterEdge *edge, int mapping, int shading);
		void stepEdge(RdZRasterEdge *edge, int mapping, int shading, bool isLeft);
		bool beginScanLine(RdZRasterEdge *left, RdZRasterEdge *right, int bpp, bool signedEnd, ScanState *state);
		void setupLinearZ(RdZRasterEdge *left, RdZRasterEdge *right, ScanState *state);
		void setupLinearIntensity(RdZRasterEdge *left, RdZRasterEdge *right, ScanState *state);
		int lightIndex(int index, bool gouraud);
		int readRGB(int index);
		bool plotTexel(const PixelMode &mode, byte *dest, byte texel, int lightBase);
		void drawTextureRun(int count, ScanState *state, const ScanParams &params);
		void scanLineSolid(RdZRasterEdge *left, RdZRasterEdge *right, int bpp, int shading, int flags);
		ScanParams makeScanParams(int bpp, int shading, int flags, bool perspective);
		void setupAffineRun(RdZRasterEdge *left, RdZRasterEdge *right, ScanState *state);
		void scanLineAffine(RdZRasterEdge *left, RdZRasterEdge *right, const ScanParams &params);
		void setRunSteps(ScanState *state, float uEnd, float vEnd, float uStart, float vStart, double stepScale);
		void setIdleRunCoords(ScanState *state, const ScanParams &params, float oneOverZ, float uStart, float vStart);
		void scanLinePerspective(RdZRasterEdge *left, RdZRasterEdge *right, const ScanParams &params);
		void scanLineCase(RdZRasterEdge *left, RdZRasterEdge *right, int mapping, int shading, int pitch);
		int roundToInt(double value);
		void setCaseRun(float uStart, float vStart, double uEnd, double vEnd, double stepScale, int shift);
		uint zTestMask(ScanState *state, int count);
		byte fetchTexel8(int u, int v, int vShift);
		byte shadeTexel8(int shading, byte texel, uint intensity);
		void drawPixels8(ScanState *state, int count, uint mask, int shading, int vShift);
		void drawBlock8(ScanState *state, uint mask, int shading, int vShift);
		void scanLineAffineCase8(RdZRasterEdge *left, RdZRasterEdge *right, int shading, int vShift);
		void scanLinePerspectiveCase8(RdZRasterEdge *left, RdZRasterEdge *right, int shading, int vShift, bool blockShiftFromTable);
		void scanLineCase8(RdZRasterEdge *left, RdZRasterEdge *right, int mapping, int shading, int pitch);
		bool setupGeometry(RdProcFace *procFace);
		bool setupSolidFace(RdProcFace *procFace, RdMatCel *cel, float light, int bpp, int shading, int flags);
		bool setupTexturedFace(RdProcFace *procFace, RdMatCel *cel, float light, int bpp, int mapping, int shading, int flags);
		int selectCasePitch(int mapping, int shading);
		int selectCasePitch8();
		int drawNGon(RdProcFace *procFace, RdMatCel *cel, float light, int mapping, int shading, int flags);

		uint _numVertices;
		int _maxMipMap;
		int _yMinVert;
		int _yMaxVert;
		int _leftVertexStep;
		int _rightVertexStep;
		float *_intensities;
		RdVector3 *_vertices;
		RdVector2 *_texVertices;
		byte *_rgbToPalette;
		byte *_rgbMap;
		byte *_monoMap;
		byte *_alphaMap;
		byte *_pixels;
		int _uWrap;
		int _vWrap;
		int _mipMapNum;
		int _texPitch;
		int _texRowShift;
		int _zScaleShift;
		int _dUdXModifier;
		int _dVdXModifier;
		int _dOneOverZdXFixed;
		float _oneOverZStep;
		float _uOverZStep;
		float _vOverZStep;
		float _dOneOverZdX;
		float _dOneOverZdY;
		float _dUOverZdX;
		float _dUOverZdY;
		float _dVOverZdX;
		float _dVOverZdY;
		uint16 _RGBcolor;
		byte _color;
		int _caseU;
		int _caseV;
		int _caseUStep;
		int _caseVStep;
		float _oneOverZ[32];
		float _uOverZ[32];
		float _vOverZ[32];
	};

	CameraModule _camera;
	CanvasModule _canvas;
	ClipModule _clip;
	ColormapModule _colormap;
	ColorEffectsModule _colorEffects;
	FaceModule _face;
	KeyframeModule _keyframe;
	LightModule _light;
	MaterialModule _material;
	Model3Module _model3;
	Primit2Module _primit2;
	Primit3Module _primit3;
	PuppetModule _puppet;
	SpriteModule _sprite;
	ThingModule _thing;
	ActiveModule _active;
	CacheModule _cache;
	RasterModule _raster;
	AFRasterModule _afRaster;
	NRasterModule _nRaster;
	ZRasterModule _zRaster;

private:
	StdMath *_stdMath;
	StdDisplay *_stdDisplay;
	StdConffile *_stdConffile;
	StdFile *_stdFile;
	Std3D *_std3D;
	SysStruct *_hs;
	int _started;
	int _opened;
	int _frameTrue;
	int _curAcceleration;
	int _curRenderOptions;
	int _curGeometryMode;
	int _curLightingMode;
	int _curTextureMode;
	int _curSortingMethod;
	int _curOcclusionMethod;
	int _curZBufferMethod;
	int _curVertexColorMode;
	int _curCullFlags;
	int _curProcFaceUserData;
	bool _demo;
	bool _remastered;
	RemasteredLookRenderer *_remasteredLookRenderer = nullptr;
	StdColorFormat _colorInfo;
	RdColorEffects _curColorEffects;
	float _mipDistances[4];
	RdMatrix34 _identMatrix34;
};

typedef void (RenderDroid::CameraModule::*RdProjectFunc)(RdVector3 out, RdVector3 in);
typedef void (RenderDroid::CameraModule::*RdProjectLstFunc)(RdVector3 *out, RdVector3 *in, int num);

typedef void (RenderDroid::AFRasterModule::*RdActiveEdgeStepFunc)(RdActiveEdge *edge);
typedef int (RenderDroid::AFRasterModule::*RdActiveSetupEdgeFunc)(RdActiveEdge *edge, RdActiveFace *face, int v0, int v1);
typedef void (RenderDroid::AFRasterModule::*RdActiveSpanFunc)(RdActiveFace *face);

struct RdActiveEdge {
	int left;
	RdActiveFace *face;
	int yStart;
	int numLines;
	float oneOverZ;
	float dOneOverZ;
	float uOverZ;
	float dUOverZ;
	float vOverZ;
	float dVOverZ;
	int x;
	int dx;
	int u;
	int du;
	int v;
	int dv;
	int intensity;
	int dIntensity;
	RdActiveEdgeStepFunc step;
	RdActiveEdge *prev;
	RdActiveEdge *next;
	RdActiveEdge *nextRemove;
};

struct RdActiveSpan {
	int x;
	int width;
	int y;
	union {
		int u;
		float uOverZ;
	};
	union {
		int du;
		float vOverZ;
	};
	int v;
	int dv;
	float oneOverZ;
	int intensity;
	int dIntensity;
	RdActiveSpan *next;
};

struct RdActiveFace {
	RdProcFace *procFace;
	float oneOverZ[32];
	float uOverZ[32];
	float vOverZ[32];
	float dOneOverZdX;
	float dOneOverZdY;
	float dUOverZdX;
	float dUOverZdY;
	float dVOverZdX;
	float dVOverZdY;
	int dOneOverZdXFixed;
	int uModifier;
	int vModifier;
	float dOneOverZdX16;
	float dUOverZdX16;
	float dVOverZdX16;
	RdActiveSpanFunc drawSpans;
	RdActiveSpan *firstSpan;
	RdActiveSpan *lastSpan;
	int mipLevel;
	byte *texPixels;
	int texShift;
	int texPitch;
	int colorIndex;
	int uWrap;
	int vWrap;
	byte *lightLevel;
	byte *rgb16;
	int reserved484;
	RdActiveSetupEdgeFunc setupEdge;
	RdActiveEdge *leftEdge;
	RdActiveEdge *rightEdge;
	int curX;
	int activeEdgeCount;
	RdActiveFace *prevFace;
	RdActiveFace *nextFace;
};

struct RdCamera {
	int projectType;
	RdCanvas *canvas;
	RdMatrix34 viewMatrix;
	float fov;
	float focalLen;
	float aspectRatio;
	float orthoScale;
	RdClipFrustum *clipFrustum;
	RdProjectFunc project;
	RdProjectLstFunc projectLst;
	float ambientLight;
	uint numLights;
	RdLight *lights[64];
	RdVector3 lightPos[64];
	float attenuationMin;
	float attenuationMax;
};

} // End of namespace Grim

#endif
