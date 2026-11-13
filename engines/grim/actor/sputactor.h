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

#ifndef GRIM_SPUTACTOR_H
#define GRIM_SPUTACTOR_H

#include "common/scummsys.h"

#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/res.h"
#include "engines/grim/std/std_display.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class Cos;
class GrimEngine;
class LegacyImporter;
class Lua_V1;
class Puck;
class SputCostume;
class SputRender;
class SputResource;
class SputRoom;
class SputText;
class SputTime;
class Std3D;
class StdDisplay;
class StdMath;
class SysStruct;
class WalkPlane;
class WalkSystem;
struct ModelInst;
struct SputTextEvent;

struct ObjectCorners {
	RdVector3 corner[4];
	int valid[4];
};

struct CostumeNode {
	int costumeInstance;
	RdThing *headThing;
	ModelInst *headList;
	int headJoint[3];
	float headTurnRate;
	float headYawLimit;
	float headPitchLimit;
	float headLookRef;
	int walkChore;
	int turnLeftChore;
	int turnRightChore;
	int restChore;
	int chore[16];
	int mumbleChore;
	ResRef rCostume;
	CostumeNode *next;
};

struct ActorShadow {
	int active;
	int side;
	RdVector3 normal;
	float planeD;
	RdVector3 lightPos;
	WalkPlane *clipPlane[12];
};

enum {
	kExtraPathPoints = 128
};

struct Extra {
	RdVector3 lookCarrot;
	int walking;
	int turnDir;
	int resting;
	float walkRate;
	float turnRate;
	int aligned;
	float offsetYaw;
	float alignOff;
	RdVector3 walkTarget;
	RdVector3 goal;
	int numPathPoints;
	RdVector3 path[kExtraPathPoints];
	int pathIndex;
	Puck *puck;
	int inBoxes;
	int walkDominate;
	float reflectionAngle;
	int talking;
	int soundId;
	int talkTimerMs;
	char sayLineId[12];
	char voiceName[16];
	SputTextEvent *sayText;
	int phoneme;
	int activeShadow;
	ActorShadow shadows[6];
	int collisionMode;
	float collisionScale;
	int collided;
	int clipPlaneActive;
	RdVector3 clipPlaneNorm;
	RdVector3 clipPlanePt;
	int talkColor;
	float timeScale;
	int subClipNode;
};

struct SputActor {
	ResRef *colorMap;
	int flags;
	int mode;
	RdVector3 actorPos;
	RdVector3 prevPosition;
	RdVector3 actorRot;
	float scale;
	int matrixValid;
	RdMatrix34 orientMatrix;
	int onScreen;
	StdRect drawnRect;
	char curSetName[32];
	int inUse;
	CostumeNode *costumeStack;
	Extra *extra;
};

struct LipSyncEntry {
	int phoneme;
	int mouthAnim;
};

class SputActorModule {
	friend class LegacyImporter;

public:
	SputActorModule(Res *res, RenderDroid *rd, StdMath *stdMath, StdDisplay *stdDisplay, WalkSystem *walkSystem, SputResource *sputResource,
			SputRoom *sputRoom, SputRender *sputRender, SputCostume *sputCostume, SputText *sputText, SputTime *sputTime);

	void setGrimEngine(GrimEngine *grim) { _grim = grim; }
	void setLua(Lua_V1 *lua) { _lua = lua; }
	void setStd3D(Std3D *std3D) { _std3D = std3D; }
	SputActor *getActors() { return _actors; }
	int getNumActors() const { return _numActors; }
	SputActor *getSelectedActor() { return _selectedActor; }
	void clearShadowMaterial() { _shadowMaterial = nullptr; }

	void nukeCostumeStacks();
	int initialize(SysStruct *hs);
	int save(Common::WriteStream *stream);
	int restore(Common::ReadStream *stream);
	int saveRemastered(Common::WriteStream *stream);
	int restoreRemastered(Common::ReadStream *stream);
	int getNumActorsTalking();
	int setCostume(SputActor *actor, const char *costumeName, int instance);
	void setActorHead(SputActor *actor, uint joint1, uint joint2, uint joint3, float maxAngle, float yawLimit, float pitchLimit);
	int canActorSeePoint(SputActor *actor, float *point, float *distance);
	int canActorSeeActor(SputActor *actor, SputActor *target, float *distance);
	void turn(SputActor *actor, float dir);
	void forward(SputActor *actor);
	void updateSpeechOnly(float delta);
	void updateActors(float delta);
	void updateActorsDemo(float delta);
	void updateCostumesMain(SputActor *actor, float delta);
	void updateCostumesMainDemo(SputActor *actor, float delta);
	void stopActorDemo(SputActor *actor);
	void sayLineDemo(SputActor *actor, const char *msgId);
	void shutUpDemo(SputActor *actor);
	int isVoicePlayingDemo(SputActor *actor);
	void updateCostumes2(SputActor *actor, float delta);
	void updateActorSpeech(SputActor *actor, float delta);
	int setSelectedActor(int handle);
	SputActor *actorDeref(int handle);
	int getActorHandle(SputActor *actor);
	int allocateActor(int flag20 = 0, int flag80 = 0);
	void allocateActorExtra(SputActor *actor);
	void enableWalkSystem(SputActor *actor, int enable);
	void enablePuck(SputActor *actor, int enable);
	void setActorPosition(SputActor *actor, float *pos);
	void setActorRotation(SputActor *actor, float *rot, int flipTurn);
	void setReflection(SputActor *actor, float angle);
	void setWalkChore(SputActor *actor, int choreIndex, CostumeNode *node);
	void setTurnChores(SputActor *actor, int leftChore, int rightChore, CostumeNode *node);
	void setRestChore(SputActor *actor, int choreIndex, CostumeNode *node);
	int pushCostume(SputActor *actor, const char *costumeName, int instance);
	void popCostume(SputActor *actor, int freeInstance);
	int matchPhonemeWithAnim(SputActor *actor, int phoneme);
	static int compareAnimEntry(const void *key, const void *entry);
	int terminate();
	int startup();
	void shutdown();
	void updateCostumes3(SputActor *actor);
	void updatePuck(SputActor *actor);
	int alignToPuck(SputActor *actor, Puck *puck, int flip);
	void updateCostumes4(SputActor *actor);
	void actorWalkTo(SputActor *actor, float *pos, float *goal, SputActor *target);
	int actorWalkToImpl(SputActor *actor, SputActor *target, float *path, int numPoints, float *outPath);
	int actorWalkToImplRemastered(SputActor *actor, SputActor *target, float *path, int numPoints, float *outPath);
	void setPathfinderMode(int mode);
	void getCollisionBoxes(SputActor *actor, int *count, int maxPoints, float (*points)[3], SputActor *avoid);
	void getOuttaTheBox(float *point, const RdBound3 &box, RdMatrix34 matrix, float *origin);
	int checkCollisions(float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix, float *offset);
	void findObjectCorners(RdModel3 *model, float margin, RdMatrix34 mat, float z, ObjectCorners *out);
	int testCollision(float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix);
	int findClosestCandidate(float *from, float *to, ObjectCorners *corners);
	void getOuttaTheBoxRemastered(float *point, const RdBound3 &box, RdMatrix34 matrix, float *origin);
	int checkCollisionsRemastered(SputActor *actor, float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix, float *offset);
	void findObjectCornersRemastered(RdModel3 *model, float margin, RdMatrix34 mat, float z, ObjectCorners *out);
	int testCollisionRemastered(float *from, float *to, float *center, const RdBound3 &box, RdMatrix34 matrix);
	int findClosestCandidateRemastered(float *from, float *to, ObjectCorners *corners);
	void sayLine(SputActor *actor, const char *msgId, int voiceStream, int volume, int pan, int x, int y);
	void shutUp(SputActor *actor);
	void setSpeechMode(int mode);
	int getSpeechMode();
	void leaveSector(SputActor *actor, WalkPlane *sector);
	void updatePuckMovement(SputActor *actor);
	void setWalkDominate(SputActor *actor, int dominate);
	void setActorConstrain(SputActor *actor, int constrain);
	void fadeInChore(int inst, int choreIndex, int fadeMs);
	void fadeOutChore(int inst, int choreIndex, int fadeMs);
	int isVoicePlaying(SputActor *actor);
	int actorHasShadow(SputActor *actor);
	void activateActorShadow(SputActor *actor, int shadowIdx, int active);
	void setShadowPlane(SputActor *actor, const char *planeName);
	void setShadowPoint(SputActor *actor, float x, float y, float z);
	void setShadowValid(SputActor *actor, int shadowIdx, int side);
	void addShadowClipPlane(SputActor *actor, int shadowIdx, const char *planeName);
	void resetShadows(SputActor *actor);
	void loadShadowResources(SputActor *actor);
	float (*getOrientMatrix(SputActor *actor))[3];
	void drawToClean(SputActor *actor);
	void updatePositions();
	void updatePosition(SputActor *actor);
	int sweepIntoBox(float *from, float *to, const RdBound3 &box, float *outPos);
	int countCostumes(SputActor *actor);
	void setClipPlane(SputActor *actor, float *normal, float *point);
	void setClipPlaneActive(SputActor *actor, int active);
	int getActorTextRate();
	void setActorTextRate(int rate);
	void setFrustrumCull(SputActor *actor, int cull);
	void freeActor(SputActor *actor);
	void setWalkSystemFadeTime(int fadeMs);
	void initShadow(CostumeNode *nodePointer, ActorShadow *shadow);
	void drawShadowPolygon(const float *poly, int numPoly, float *minPointer, float *maxPointer, ActorShadow *shadow);
	CostumeNode *allocCostumeNode();
	void initCostumeNode(CostumeNode *node);
	void drawCostume(CostumeNode *node, RdMatrix34 matrix);
	void drawCostumeImpl(CostumeNode *node, RdMatrix34 matrix);
	void applyPendingScale(SputActor *actor, ModelInst *list);
	void syncCostumeComponents(int instHandle);
	void buildJointMatricesRec(ModelInst *inst, RdMatrix34 matrix);
	void enumCostumeComponents(int inst);

private:
	void publishActors();
	bool buildPath(SputActor *actor, float *start, float *goal, int tighten, unsigned *failFlag, SputActor *avoid);
	int pathCapacity() const;
	void transformVectorOrtho34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat);
	void transformVector34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat);
	void transformPoint34Remastered(RdVector3 out, RdVector3 vec, RdMatrix34 mat);
	void transformPoint34AccRemastered(RdVector3 vec, RdMatrix34 mat);
	float dot3Remastered(float *a, float *b);
	float normalize2Remastered(float *vec);
	float normalize3AccRemastered(float *vec);
	SputActor *getManny();
	int findSlidePoint(float *otherCenter, float *prevCenter, float *center, float reach);
	int findSlideCandidates(float *prevCenter, float *otherCenter, float radius, float moved, float *firstPoint, float *secondPoint);
	void sphereCollisionRemastered(SputActor *actor, SputActor *other, float *myCenterOffset, float *myCenterWorld, float *prevCenterWorld,
			float *otherCenterWorld, float myRadius, float otherRadius);

	enum {
		kMannyPositions = 40
	};

	Res *_res;
	RenderDroid *_rd;
	StdMath *_stdMath;
	StdDisplay *_stdDisplay;
	WalkSystem *_walkSystem;
	SputResource *_sputResource;
	SputRoom *_sputRoom;
	SputRender *_sputRender;
	SputCostume *_sputCostume;
	SputText *_sputText;
	SputTime *_sputTime;
	GrimEngine *_grim;
	Lua_V1 *_lua = nullptr;
	Std3D *_std3D = nullptr;
	Cos *_cos;

	SysStruct *_hs;
	int _initialized;
	int _started;

	SputActor *_actors;
	int _maxActors;
	int _numActors;
	SputActor *_selectedActor;
	int _numActorsTalking;
	SputActor *_talkingActors[4];
	SputActor *_collisionActors[256];
	int _numCollisionActors;
	float _collisionImpact[3];
	float _collisionOut[3];
	float _collisionIn[3];
	int _speechMode;
	int _textRate;
	int _walkSystemFadeTime;
	LipSyncEntry _lipSyncAnims[46];
	int _numLipSyncAnims;
	RdMaterial *_shadowMaterial;
	RdMaterial _demoShadowMaterial;
	RdMatCel _demoShadowCel;

	unsigned int _dynamicCollisionFrames = 0;
	bool _hadDynamicCollision = false;

	float _mannyOldDest[3] = { 0.0f, 0.0f, 0.0f };
	float _mannyOldPos[kMannyPositions][3] = {};
	int _mannyAvgIdx = 0;
	int _mannyAvgCnt = 0;
};

} // End of namespace Grim

#endif
