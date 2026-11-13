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

#ifndef GRIM_SPUTROOM_H
#define GRIM_SPUTROOM_H

#include "common/scummsys.h"

#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/res.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Grim {

class GrimEngine;
class LegacyImporter;
class RenderDroid;
class SputActorModule;
class SputRender;
class SputResource;
class StdBitmap;
class StdConffile;
class StdDisplay;
class StdLbm;
class WalkSystem;
struct SputActor;
struct SputObjectState;
struct StdVBuffer;

struct RoomCamera {
	RdVector3 position;
	RdVector3 interest;
	float roll;
	float fov;
	float nearClip;
	float farClip;
	RdCamera *prdCamera;
	RdThing *thing;
	RdMatrix34 posMatrix;
};

// Only the demo has these. Its set files list the objects of the room and the art
// of each object in every setup, the retail ones create the object states from Lua
struct RoomObject {
	char name[32];
	RdVector3 position;
};

struct RoomObjectArt {
	char name[32];
	char bitmapName[32];
	char zBufferName[32];
};

struct RoomSetup {
	char name[32];
	RoomCamera camera;
	char bitmapName[32];
	char zBufferName[32];
	ResRef bitmapRef;
	ResRef zbufferRef;
	SputObjectState *ObjectStates;
	RoomObjectArt *objectArt;
};

struct RoomLight {
	char name[32];
	RdVector3 position;
	RdLight light;
};

struct RoomActorLink {
	int actorIndex;
	RoomActorLink *prev;
	RoomActorLink *next;
};

struct RoomActorNode {
	char name[128];
	RoomActorLink *actors;
	RoomActorNode *next;
};

struct Room {
	char name[128];
	ResRef *colormaps;
	int numcolormaps;
	int curColormap;
	char colormapName[32];
	RoomSetup *setups;
	uint numSetups;
	int curSetup;
	uint numObjects;
	RoomObject *objects;
	uint numLights;
	RoomLight lights[64];
	RoomActorNode *actorNode;
	ResRef roomRef;
	int structSize;
	byte minVol;
	byte maxVol;
	float maxDist;
	float panFactor;
	float maxRadius;
};

struct RoomSound {
	RoomSound *next;
	const char *soundName;
	RdVector3 pos;
	int minVol;
	int maxVol;
	float maxDist;
	float radius;
};

class SputRoom {
	friend class LegacyImporter;

public:
	SputRoom(RenderDroid *rd, Res *res, SputResource *sputResource, StdDisplay *stdDisplay, StdBitmap *stdBitmap, StdLbm *stdLbm,
			StdConffile *stdConffile, WalkSystem *walkSystem);

	void setSputRender(SputRender *sputRender) { _sputRender = sputRender; }
	void setGrimEngine(GrimEngine *grim) { _grim = grim; }
	void setActors(SputActor *actors) { _actors = actors; }
	void setSputActor(SputActorModule *sputActor) { _sputActor = sputActor; }

	StdVBuffer *getSetupBackground(Room *room, int setupIndex);
	StdVBuffer *getSetupZBackground(Room *room, int setupIndex);
	void relocateSounds();
	void setSoundParameters(Room *room, float maxDist, byte minVol, byte maxVol, float panFactor, float maxRadius);
	void getSoundParameters(Room *room, float *maxDist, byte *minVol, byte *maxVol, float *panFactor, float *maxRadius);
	int setSoundPosition(const char *soundName, float *pos, int minVol, int maxVol, float maxDist, float radius);
	void setCurrentRoom(Room *room);
	Room *getCurrentRoom();
	const char *getCurrentRoomName();
	int save(Common::WriteStream *stream);
	int restore(Common::ReadStream *stream);
	int saveRemastered(Common::WriteStream *stream);
	int restoreRemastered(Common::ReadStream *stream);
	int initialize(SysStruct *hs);
	StdVBuffer *loadZBuffer(const char *fileName);
	void freeZBuffer(StdVBuffer *&zBuffer);
	Room *loadRoom(const char *fileName);
	void freeRoom(Room *&room);
	int loadResources(Room *room);
	int terminate();
	int startup();
	int shutdown();
	RoomLight *getLight(Room *room, int index);
	int findLightByName(Room *room, const char *name);
	SputObjectState *addObjectState(int setupIndex, int objectType, const char *bitmapName, const char *zbufferName, int copyMode);
	void removeObjectState(SputObjectState *obj);
	void removeActor(const char *roomName, SputActor *actor);
	void addActor(const char *roomName, SputActor *actor);
	RoomActorLink *getRoomActors(const char *roomName);
	RoomActorNode *findActorNode(const char *roomName);

private:
	typedef int (SputRoom::*SectionFunc)();

	struct Section {
		char name[32];
		SectionFunc handler;
	};

	StdVBuffer *loadZBufferEntry(const char *fileName);
	void freeZBufferEntry(StdVBuffer *&zBuffer);
	void newRoomEntry(Room *room);
	int loadRoomEntry(const char *fileName, Room *room);
	void unReference(Room *room);
	void freeRoomEntry(Room *room);
	void freeRoomSetups(Room *room);
	void freeSetupEntry(Room *room, RoomSetup *setup);
	void freeSoundTable();
	RoomSound *allocSoundTable(const char *soundName, float *pos, int minVol, int maxVol, float maxDist, float radius);
	int setupSoundPosition(RoomSound *sound);
	int readSections();
	int readSectionFunc(const char *name, SectionFunc read);
	int sectionNameCompare(const char *name);
	int loadSections(const char *fileName);
	int findSectionHandler(const char *name);
	int parseLights();
	int parseSetups();
	int readObjectsSection();
	void createObjectStates(Room *room, RoomSetup *setup, int setupIndex);
	int parseColormaps();
	int parseSectors();
	RoomActorNode *allocActorNode(const char *roomName);
	void freeActorNode(RoomActorNode *node);

	RenderDroid *_rd;
	Res *_res;
	SputResource *_sputResource;
	StdDisplay *_stdDisplay;
	StdBitmap *_stdBitmap;
	StdLbm *_stdLbm;
	StdConffile *_stdConffile;
	WalkSystem *_walkSystem;
	SputRender *_sputRender;
	GrimEngine *_grim;
	SputActorModule *_sputActor;
	SputActor *_actors;
	SysStruct *_hs;
	int _initialized;
	int _started;
	ResRef _currentRoomRef;
	char _currentRoomName[32];
	RoomSound *_soundList;
	RoomActorNode *_actorNodeList;
	Room *_loadingRoom;
	int _updateLoop;
	int _numSections;
	Section _sections[32];
};

} // End of namespace Grim

#endif
