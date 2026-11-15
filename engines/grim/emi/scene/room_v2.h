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

#ifndef GRIM_EMI_ROOM_V2_H
#define GRIM_EMI_ROOM_V2_H

#include "common/list.h"

#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/scene/clightnode.h"
#include "engines/grim/emi/graphics/layer.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/emi/resources/lresource.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/scene/setup.h"
#include "engines/grim/emi/actor/shadow_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class Actor;
class CDrawContext;
class EmiEngine;

class Room_v2 : public LResource {
public:
	Room_v2();
	virtual ~Room_v2();

	virtual uint resSizeOfItem();

	void setSetup(int setupIndex);
	Camera *getSetupCamera(int setupIndex);
	int draw(CDrawContext *drawContext);
	void addActor(Actor *actor);
	int removeActor(Actor *actor);
	void setLightsActive(bool active);
	void saveLightStates();
	void restoreLightStates();
	CLightNode *findLight(const char *lightName);
	void addLight(CLightNode *light);
	void removeLight(CLightNode *light);
	Shadow_v2 *findShadow(const LString &shadowName);
	Shadow_v2 *newShadow();
	void removeShadow(Shadow_v2 *shadow);
	void setSoundParameters(float newMaxDist, char newMinVol, char newMaxVol, float newPanFactor, float newMaxRadius);
	bool save(SaveRestoreAccess *accessFunc);
	bool restore(SaveRestoreAccess *accessFunc);
	void freeSetups();
	bool loadText(const char *path);
	bool parseSection(LFile *file, const char *sectionName);
	bool parseSetups(LFile *file);
	bool parseLights(LFile *file);
	bool parseShadows(LFile *file);
	bool parseSectors(LFile *file);
	bool loadBinary(const char *path);
	bool saveBinary(const char *path);

	static bool startup(EmiEngine *engine);
	static void shutdown(EmiEngine *engine);
	static void saveCurrentRoom(EmiEngine *engine, SaveRestoreAccess *accessFunc);
	static void restoreCurrentRoom(EmiEngine *engine, SaveRestoreAccess *accessFunc);
	static Layer *findLayerById(EmiEngine *engine, int layerId);
	static void loadRoom(EmiEngine *engine, const char *roomName);
	static Room_v2 *getCurrentRoom(EmiEngine *engine);
	static const char *getCurrentRoomName(EmiEngine *engine);

	Setup *_setups;
	int _numSetups;
	int _currentSetup;
	Common::List<Shadow_v2 *> _shadows;
	Common::List<CLightNode *> _lights;
	Common::List<Actor *> _actors;
	WalkPlaneCollection *_walkPlanes;
	char _minVol;
	char _maxVol;
	float _maxDist;
	float _panFactor;
	float _maxRadius;

protected:
	virtual bool resLoadItem(const char *itemName);

private:
	enum {
		kLineBufferSize = 256
	};

	void logLoaderError(const LString &message);
	bool failBinaryLoad(LFile *file, const char *reason);
};

} // End of namespace Grim

#endif
