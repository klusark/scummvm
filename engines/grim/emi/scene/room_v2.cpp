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

#include "common/endian.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "common/array.h"

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/resources/cbundle.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/scene/clight.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/math/cquaternion.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/input/demoreplay.h"
#include "engines/grim/emi/input/inputrecorder.h"
#include "engines/grim/emi/math/irect.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/sound/sounds.h"

namespace Grim {

static const uint kSavedRoomNameSize = 128;

enum {
	kDrawLayer = 0,
	kDrawActor = 1,
	kDrawShadow = 2,
	kNumSortBins = 16
};

struct DrawItem {
	int key;
	float depth;
	int type;
	void *object;
	int index;
};

static void mergeDrawItems(Common::Array<DrawItem> &first, Common::Array<DrawItem> &second) {
	Common::Array<DrawItem> merged;
	uint firstIndex = 0;
	uint secondIndex = 0;

	while (firstIndex < first.size() && secondIndex < second.size()) {
		const DrawItem &left = first[firstIndex];
		const DrawItem &right = second[secondIndex];
		bool takeRight;

		if (right.key == left.key)
			takeRight = (double)right.depth > (double)left.depth;
		else
			takeRight = right.key > left.key;

		if (takeRight) {
			merged.push_back(right);
			secondIndex++;
		} else {
			merged.push_back(left);
			firstIndex++;
		}
	}

	while (firstIndex < first.size()) {
		merged.push_back(first[firstIndex]);
		firstIndex++;
	}

	while (secondIndex < second.size()) {
		merged.push_back(second[secondIndex]);
		secondIndex++;
	}

	first = merged;
	second.clear();
}

static void sortDrawItems(Common::Array<DrawItem> &items) {
	Common::Array<DrawItem> bins[kNumSortBins];
	Common::Array<DrawItem> carry;
	uint maxBin = 0;
	uint next = 0;

	while (next < items.size()) {
		carry.clear();
		carry.push_back(items[next]);
		next++;

		uint bin = 0;

		while (bin < maxBin && !bins[bin].empty()) {
			mergeDrawItems(bins[bin], carry);
			SWAP(bins[bin], carry);
			bin++;
		}

		if (bin == kNumSortBins - 1) {
			mergeDrawItems(bins[bin], carry);
		} else {
			SWAP(bins[bin], carry);

			if (bin == maxBin)
				maxBin++;
		}
	}

	items.clear();

	for (uint bin = maxBin; bin > 0; bin--)
		mergeDrawItems(items, bins[bin - 1]);
}

Room_v2::Room_v2() {
	_setups = nullptr;
	_numSetups = 0;
	_currentSetup = 0;
	_walkPlanes = nullptr;
	_minVol = 0;
	_maxVol = 0;
	_maxDist = 0.0f;
	_panFactor = 0.0f;
	_maxRadius = 0.0f;
}

Room_v2::~Room_v2() {
	freeSetups();

	if (_walkPlanes) {
		delete _walkPlanes;
		_walkPlanes = nullptr;
	}

	while (!_lights.empty()) {
		CLightNode *lightNode = _lights.front();

		if (lightNode) {
			removeLight(lightNode);

			if (lightNode->_light) {
				delete lightNode->_light;
				lightNode->_light = nullptr;
			}

			delete lightNode;
		} else {
			warning("Room_v2::~Room_v2(): null light");
			_lights.pop_front();
		}
	}

	while (!_shadows.empty())
		removeShadow(_shadows.front());

	_actors.clear();
}

uint Room_v2::resSizeOfItem() {
	uint size = sizeof(Room_v2);

	for (int i = 0; i < _numSetups; i++)
		size += _setups[i].getSize();

	size += sizeof(Shadow_v2 *) * _shadows.size();

	Common::List<Shadow_v2 *>::iterator shadowIt;

	for (shadowIt = _shadows.begin(); shadowIt != _shadows.end(); ++shadowIt)
		size += (*shadowIt)->getSize();

	size += sizeof(CLightNode *) * _lights.size();

	Common::List<CLightNode *>::iterator lightIt;

	for (lightIt = _lights.begin(); lightIt != _lights.end(); ++lightIt) {
		if (*lightIt) {
			size += sizeof(CLightNode);

			if ((*lightIt)->_light)
				size += sizeof(CLight);
		}
	}

	size += sizeof(Actor *) * _actors.size();

	if (_walkPlanes)
		size += _walkPlanes->getSize();

	return size;
}

bool Room_v2::resLoadItem(const char *itemName) {
	LString textPath(itemName);
	LString binaryPath = textPath + "b";
	LFileServices *fileServices = _engine->getFileServices();
	LConsoleServices *consoleServices = _engine->getConsoleServices();
	bool binaryExists = fileServices->exists(binaryPath.c_str());
	bool textExists = fileServices->exists(textPath.c_str());
	bool useText = true;

	if (binaryExists) {
		if (!textExists)
			useText = false;
		else if (fileServices->getModTime(binaryPath.c_str()) >= fileServices->getModTime(textPath.c_str()))
			useText = false;
	}

	if (useText) {
		if (!loadText(itemName)) {
			if (consoleServices)
				consoleServices->logError("Parse problem in file '%s'.\n", itemName);
			else
				warning("Parse problem in file '%s'.", itemName);

			return false;
		}

		if (_walkPlanes) {
			Common::List<Shadow_v2 *>::iterator it;

			for (it = _shadows.begin(); it != _shadows.end(); ++it)
				(*it)->resolveSectors(_walkPlanes);
		} else if (!_shadows.empty()) {
			warning("Room_v2::resLoadItem(): shadows without sectors");
		}

		saveBinary(binaryPath.c_str());
	} else if (!loadBinary(binaryPath.c_str())) {
		if (consoleServices)
			consoleServices->logError("Parse problem in file '%s'.\n", binaryPath.c_str());
		else
			warning("Parse problem in file '%s'.", binaryPath.c_str());

		return false;
	}

	return true;
}

void Room_v2::setSetup(int setupIndex) {
	int index = setupIndex;

	if (_engine->isDemo() && setupIndex < 0)
		warning("Room_v2::setSetup(): setup %d is negative", setupIndex);

	if (_numSetups <= 0) {
		warning("Room_v2::setSetup(): the room has no setups");

		return;
	}

	while (index < 0)
		index += _numSetups;

	if (index >= _numSetups)
		index %= _numSetups;

	int oldSetup = _currentSetup;

	if (index != oldSetup) {
		_currentSetup = index;
		_engine->getLua()->callCameraChange(oldSetup, index);
	}
}

Camera *Room_v2::getSetupCamera(int setupIndex) {
	int index = setupIndex;

	if (index < 0 || index >= _numSetups)
		index = _currentSetup;

	if (!_setups || index < 0 || index >= _numSetups) {
		warning("Room_v2::getSetupCamera(): invalid setup %d", index);

		return nullptr;
	}

	return &_setups[index]._camera;
}

int Room_v2::draw(CDrawContext *drawContext) {
	_engine->_roomDrawTimer.start();

	if (!_setups || _currentSetup < 0 || _currentSetup >= _numSetups) {
		warning("Room_v2::draw(): invalid setup %d", _currentSetup);
		_engine->_roomDrawTimer.stop();

		return 0;
	}

	int layersDrawn = 0;
	Camera *camera = &_setups[_currentSetup]._camera;
	camera->updateViewMatrix();
	CTransform view = camera->_view;

	if (_engine->_lastDrawnRoom == this && _engine->_lastDrawnSetup == _currentSetup) {
		Setup *setup = &_setups[_engine->_lastDrawnSetup];
		drawContext->clear(false, true);

		Common::Array<DrawItem> items;
		Common::List<Layer *>::iterator layerIt;

		for (layerIt = setup->_layers.begin(); layerIt != setup->_layers.end(); ++layerIt) {
			DrawItem item;
			item.key = (*layerIt)->_sortOrder;
			item.depth = 100000.0f;
			item.type = kDrawLayer;
			item.object = *layerIt;
			item.index = 0;
			items.push_back(item);
		}

		Common::List<Actor *>::iterator actorIt;

		for (actorIt = _actors.begin(); actorIt != _actors.end(); ++actorIt) {
			Actor *actor = *actorIt;

			if ((actor->_nodeFlags & CNode::kWorldValid) == 0)
				actor->updateWorldTransform();

			float x = actor->_world._pos._x;
			float y = actor->_world._pos._y;
			float z = actor->_world._pos._z;
			double product = (double)view._rot._m[5] * y + (double)view._rot._m[2] * x;
			product = product + (double)view._rot._m[8] * z;
			float depth = (float)((double)view._pos._z + product);

			if (!actor->_instances.empty()) {
				DrawItem item;
				item.key = actor->getSortOrder(_engine->_lastDrawnSetup);
				item.depth = depth;
				item.type = kDrawActor;
				item.object = actor;
				item.index = 0;
				items.push_back(item);
			}

			for (uint i = 0; i < actor->_shadows.size(); i++) {
				DrawItem item;
				item.key = actor->getSortOrder(_engine->_lastDrawnSetup);
				item.type = kDrawShadow;
				item.depth = depth;
				item.object = actor;
				item.index = (int)i;
				items.push_back(item);
			}
		}

		if (items.size() >= 2)
			sortDrawItems(items);

		for (uint i = 0; i < items.size(); i++) {
			DrawItem &item = items[i];

			if (item.key < 0)
				continue;

			if (item.type == kDrawLayer) {
				layersDrawn += ((Layer *)item.object)->draw(drawContext);
			} else if (item.type == kDrawActor) {
				setup->_camera.applyProjection();
				((Actor *)item.object)->drawActor(&setup->_camera);

				if (_engine->isDemo())
					drawContext->flushDrawLists();
			} else if (item.type == kDrawShadow) {
				Actor *actor = (Actor *)item.object;
				setup->_camera.applyProjection();
				actor->drawShadow(&setup->_camera, actor->_shadows[item.index]);

				if (_engine->isDemo())
					drawContext->flushDrawLists();
			}
		}

		_engine->_roomDrawTimer.stop();

		return layersDrawn;
	}

	_engine->_lastDrawnRoom = this;
	_engine->_lastDrawnSetup = _currentSetup;
	_setups[_engine->_lastDrawnSetup].buildLayers();
	_engine->getSounds()->setSoundListener(camera);
	_engine->getLua()->callPostCameraChange(_engine->_lastDrawnSetup);
	_engine->_roomDrawTimer.stop();

	return -1;
}

void Room_v2::addActor(Actor *actor) {
	if (_engine->isDemo()) {
		Common::List<Actor *>::iterator it;

		for (it = _actors.begin(); it != _actors.end(); ++it) {
			if (*it == actor) {
				warning("Room_v2::addActor(): the actor was already in the room");

				break;
			}
		}
	}

	_actors.push_back(actor);
}

int Room_v2::removeActor(Actor *actor) {
	int count = 0;
	Common::List<Actor *>::iterator it = _actors.begin();

	while (it != _actors.end()) {
		if (*it == actor) {
			count++;
			it = _actors.erase(it);
		} else {
			++it;
		}
	}

	if (_engine->isDemo() && count > 1)
		warning("Room_v2::removeActor(): the actor was in the room %d extra times", count - 1);

	return count;
}

void Room_v2::setLightsActive(bool active) {
	Common::List<CLightNode *>::iterator it;

	for (it = _lights.begin(); it != _lights.end(); ++it) {
		if (*it)
			(*it)->setActive(active);
	}
}

void Room_v2::saveLightStates() {
	Common::List<CLightNode *>::iterator it;

	for (it = _lights.begin(); it != _lights.end(); ++it) {
		if (*it)
			(*it)->_savedActive = (*it)->_active;
	}
}

void Room_v2::restoreLightStates() {
	Common::List<CLightNode *>::iterator it;

	for (it = _lights.begin(); it != _lights.end(); ++it) {
		if (*it)
			(*it)->setActive((*it)->_savedActive);
	}
}

CLightNode *Room_v2::findLight(const char *lightName) {
	Common::List<CLightNode *>::iterator it;

	for (it = _lights.begin(); it != _lights.end(); ++it) {
		if (*it && scumm_stricmp(lightName, (*it)->_name) == 0)
			return *it;
	}

	return nullptr;
}

void Room_v2::addLight(CLightNode *light) {
	Common::List<CLightNode *>::iterator it;

	for (it = _lights.begin(); it != _lights.end(); ++it) {
		if (*it == light)
			return;
	}

	_lights.push_back(light);
}

void Room_v2::removeLight(CLightNode *light) {
	light->setActive(false);

	Common::List<CLightNode *>::iterator it = _lights.begin();

	while (it != _lights.end()) {
		if (*it == light)
			it = _lights.erase(it);
		else
			++it;
	}
}

Shadow_v2 *Room_v2::findShadow(const LString &shadowName) {
	Common::List<Shadow_v2 *>::iterator it;

	for (it = _shadows.begin(); it != _shadows.end(); ++it) {
		if (*it && (*it)->_name == shadowName)
			return *it;
	}

	return nullptr;
}

Shadow_v2 *Room_v2::newShadow() {
	Shadow_v2 *shadow = new Shadow_v2(_engine);

	_shadows.push_back(shadow);

	return shadow;
}

void Room_v2::removeShadow(Shadow_v2 *shadow) {
	Common::List<Shadow_v2 *>::iterator it = _shadows.begin();

	while (it != _shadows.end()) {
		if (*it == shadow)
			it = _shadows.erase(it);
		else
			++it;
	}

	delete shadow;
}

void Room_v2::setSoundParameters(float newMaxDist, char newMinVol, char newMaxVol, float newPanFactor, float newMaxRadius) {
	_maxDist = newMaxDist;
	_minVol = newMinVol;
	_maxVol = newMaxVol;
	_panFactor = newPanFactor;
	_maxRadius = newMaxRadius;
}

bool Room_v2::save(SaveRestoreAccess *accessFunc) {
	accessFunc(&_currentSetup, sizeof(_currentSetup));

	if (_engine->getGamePlatform() != Common::kPlatformPS2) {
		accessFunc(&_minVol, sizeof(_minVol));
		accessFunc(&_maxVol, sizeof(_maxVol));
		accessFunc(&_maxDist, sizeof(_maxDist));
		accessFunc(&_panFactor, sizeof(_panFactor));
		accessFunc(&_maxRadius, sizeof(_maxRadius));
	}

	Common::List<CLightNode *>::iterator it;

	for (it = _lights.begin(); it != _lights.end(); ++it) {
		CLightNode *lightNode = *it;

		if (lightNode) {
			if ((lightNode->_nodeFlags & CNode::kWorldValid) == 0)
				lightNode->updateWorldTransform();

			float position[3];

			position[0] = lightNode->_world._pos._x;
			position[1] = lightNode->_world._pos._y;
			position[2] = lightNode->_world._pos._z;
			accessFunc(position, sizeof(position));

			byte activeByte = lightNode->_active ? 1 : 0;

			accessFunc(&activeByte, sizeof(activeByte));
		}
	}

	for (int i = 0; i < _numSetups; i++)
		_setups[i].save(accessFunc);

	if (_walkPlanes)
		_walkPlanes->save(accessFunc);
	else
		warning("Room_v2::save(): the room has no sectors");

	return true;
}

bool Room_v2::restore(SaveRestoreAccess *accessFunc) {
	accessFunc(&_currentSetup, sizeof(_currentSetup));

	if (_engine->getGamePlatform() != Common::kPlatformPS2) {
		accessFunc(&_minVol, sizeof(_minVol));
		accessFunc(&_maxVol, sizeof(_maxVol));
		accessFunc(&_maxDist, sizeof(_maxDist));
		accessFunc(&_panFactor, sizeof(_panFactor));
		accessFunc(&_maxRadius, sizeof(_maxRadius));
	}

	Common::List<CLightNode *>::iterator it;

	for (it = _lights.begin(); it != _lights.end(); ++it) {
		CLightNode *lightNode = *it;

		if (lightNode) {
			float position[3] = { 0.0f, 0.0f, 0.0f };

			accessFunc(position, sizeof(position));

			CVector newPosition;

			newPosition._x = position[0];
			newPosition._y = position[1];
			newPosition._z = position[2];
			lightNode->setLocalPosition(newPosition);

			byte activeByte = 0;

			accessFunc(&activeByte, sizeof(activeByte));
			lightNode->setActive(activeByte != 0);
		}
	}

	for (int i = 0; i < _numSetups; i++)
		_setups[i].restore(accessFunc);

	if (_walkPlanes)
		_walkPlanes->restore(accessFunc);
	else
		warning("Room_v2::restore(): the room has no sectors");

	return true;
}

void Room_v2::freeSetups() {
	if (_numSetups > 0 && _setups) {
		Setup::deleteArray(_setups);
		_setups = nullptr;
		_numSetups = 0;
	} else {
		_setups = nullptr;
		_numSetups = 0;
	}
}

bool Room_v2::startup(EmiEngine *engine) {
	if (engine->_roomsStarted)
		return false;

	engine->_currentRoomHandle.bindEngine(engine);
	engine->_currentRoomHandle.setResource(nullptr);
	engine->_roomsStarted = true;

	return true;
}

void Room_v2::shutdown(EmiEngine *engine) {
	engine->_roomsStarted = false;
}

void Room_v2::saveCurrentRoom(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	if (engine->_currentRoom) {
		uint length = engine->_currentRoom->_name.size() + 1;

		accessFunc(&length, sizeof(length));
		accessFunc((void *)engine->_currentRoom->_name.c_str(), length);
		engine->_currentRoom->save(accessFunc);
		Layer::saveRestoreNextId(engine, accessFunc);
	} else {
		int noRoom = 0;

		accessFunc(&noRoom, sizeof(noRoom));
		Layer::saveRestoreNextId(engine, accessFunc);
	}
}

void Room_v2::restoreCurrentRoom(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	char buffer[kSavedRoomNameSize + 1];
	uint length = 0;

	accessFunc(&length, sizeof(length));

	if (length) {
		if (length > kSavedRoomNameSize) {
			warning("Room_v2::restoreCurrentRoom(): invalid room name length %u", length);

			return;
		}

		accessFunc(buffer, length);
		buffer[length] = 0;
		loadRoom(engine, buffer);

		if (engine->_currentRoom)
			engine->_currentRoom->restore(accessFunc);
		else
			warning("Room_v2::restoreCurrentRoom(): the room could not be loaded");
	}

	Layer::saveRestoreNextId(engine, accessFunc);
}

Layer *Room_v2::findLayerById(EmiEngine *engine, int layerId) {
	Room_v2 *room = engine->_currentRoom;

	if (!room || !room->_setups)
		return nullptr;

	for (int i = 0; i < room->_numSetups; i++) {
		Common::List<Layer *>::iterator it;

		for (it = room->_setups[i]._layers.begin(); it != room->_setups[i]._layers.end(); ++it) {
			if ((*it)->_id == layerId)
				return *it;
		}
	}

	return nullptr;
}

void Room_v2::loadRoom(EmiEngine *engine, const char *roomName) {
	if (engine->_currentRoom) {
		if (roomName && engine->_currentRoom->_name == roomName)
			return;

		engine->_currentRoom->setLightsActive(false);
		engine->_currentRoomHandle.unlock();

		Common::List<Actor *>::iterator it;

		for (it = engine->_currentRoom->_actors.begin(); it != engine->_currentRoom->_actors.end(); ++it) {
			if (engine->getConsoleServices())
				engine->getConsoleServices()->logError("Actor '%s' left in room.\n", (*it)->_actorName.c_str());
			else
				warning("Actor '%s' left in room.", (*it)->_actorName.c_str());
		}

		engine->_currentRoomHandle.setResource(nullptr);
		engine->_currentRoom = nullptr;
		engine->getSounds()->setSoundListener(nullptr);

		if (engine->getGamePlatform() == Common::kPlatformPS2)
			engine->getSounds()->killMusicForSet(roomName);
	}

	if (!roomName)
		return;

	if (engine->getGamePlatform() == Common::kPlatformPS2) {
		engine->getBundleServices()->openSetBundle(roomName);
		engine->startSetChangeFrames();
	}

	engine->_currentRoomHandle._resource = engine->getResourceManager()->findResource(roomName);

	if (!engine->_currentRoomHandle._resource) {
		engine->_currentRoomHandle._resource = engine->getResourceManager()->createResource(roomName);

		if (!engine->_currentRoomHandle._resource)
			engine->_currentRoomHandle.allocateResource();

		if (engine->_currentRoomHandle._resource) {
			engine->_currentRoomHandle._resource->resSetName(roomName);
			engine->getResourceManager()->addResource(engine->_currentRoomHandle._resource);
		}
	}

	engine->_currentRoomHandle.setResource(engine->_currentRoomHandle._resource);

	if (engine->_currentRoomHandle.lock())
		engine->_currentRoom = engine->_currentRoomHandle._typed;
	else
		engine->_currentRoom = nullptr;

	if (!engine->_currentRoom) {
		warning("Room_v2::loadRoom(): could not load room '%s'", roomName);

		return;
	}

	engine->_currentRoom->setSoundParameters(8.0f, 20, 127, 1.5f, 0.8f);
	engine->_currentRoom->setLightsActive(true);

	if (engine->getDemoReplay())
		engine->getDemoReplay()->consumeSet();
	else if (engine->getInputRecorder())
		engine->getInputRecorder()->recordRoom(roomName);
	else if (engine->getGamePlatform() != Common::kPlatformPS2)
		warning("Room_v2::loadRoom(): there is no input recorder");
}

Room_v2 *Room_v2::getCurrentRoom(EmiEngine *engine) {
	return engine->_currentRoom;
}

const char *Room_v2::getCurrentRoomName(EmiEngine *engine) {
	if (!engine->_currentRoom)
		return nullptr;

	return engine->_currentRoom->_name.c_str();
}

static const float kDegToRad = 3.14159274f * 0.00555555569f;
static const int kTokenSize = 256;
static const int kBinaryStringSize = 256;
static const int kMaxSetups = 4096;

void Room_v2::logLoaderError(const LString &message) {
	if (_engine->getConsoleServices())
		_engine->getConsoleServices()->logError("%s", message.c_str());
	else
		warning("%s", message.c_str());
}

static void setNodeRotation(CNode *node, const CQuaternion &rotation) {
	node->_quat = rotation;
	node->_nodeFlags = (node->_nodeFlags & ~(CNode::kEulerValid | CNode::kQuaternionValid | CNode::kLocalSet)) | CNode::kQuaternionValid | CNode::kLocalSet;
	node->updateLocalState();
}

static void setNodeEuler(CNode *node, const CVector &euler) {
	node->_eulerDeg = euler;
	node->_nodeFlags = (node->_nodeFlags & ~CNode::kQuaternionValid) | CNode::kEulerValid | CNode::kLocalSet;
	node->updateLocalState();
}

static void setLightNodeName(CLightNode *lightNode, const char *source) {
	if (strlen(source) >= CNode::kNameSize)
		warning("Room_v2::setLightNodeName(): the light name is too long");

	memset(lightNode->_name, 0, CNode::kNameSize);
	Common::strlcpy(lightNode->_name, source, CNode::kNameSize);
}

static bool readBinaryBytes(LFile *file, void *buffer, uint size) {
	return file->read(buffer, size) == size;
}

static bool readBinaryInt(LFile *file, int *value) {
	byte bytes[4];

	if (file->read(bytes, sizeof(bytes)) != sizeof(bytes))
		return false;

	*value = READ_LE_INT32(bytes);

	return true;
}

static bool readBinaryFloat(LFile *file, float *value) {
	byte bytes[4];

	if (file->read(bytes, sizeof(bytes)) != sizeof(bytes))
		return false;

	*value = READ_LE_FLOAT32(bytes);

	return true;
}

static bool readBinaryVector(LFile *file, CVector *vector) {
	return readBinaryFloat(file, &vector->_x) && readBinaryFloat(file, &vector->_y) && readBinaryFloat(file, &vector->_z);
}

static bool readBinaryQuaternion(LFile *file, CQuaternion *quaternion) {
	return readBinaryFloat(file, &quaternion->_x) && readBinaryFloat(file, &quaternion->_y) && readBinaryFloat(file, &quaternion->_z) && readBinaryFloat(file, &quaternion->_w);
}

static bool readBinaryColor(LFile *file, Color *color) {
	return readBinaryInt(file, &color->_a) && readBinaryInt(file, &color->_r) && readBinaryInt(file, &color->_g) && readBinaryInt(file, &color->_b);
}

static bool readBinaryString(LFile *file, char *buffer) {
	int length;

	if (!readBinaryInt(file, &length))
		return false;

	if (length < 0 || length > kBinaryStringSize) {
		warning("Room_v2::loadBinary(): invalid string length %d", length);

		return false;
	}

	if (!readBinaryBytes(file, buffer, length))
		return false;

	buffer[length] = 0;

	return true;
}

bool Room_v2::failBinaryLoad(LFile *file, const char *reason) {
	warning("Room_v2::loadBinary(): %s", reason);
	_engine->getFileServices()->close(file, true);

	return false;
}

static void writeBinaryInt(LFile *file, int value) {
	byte bytes[4];

	WRITE_LE_INT32(bytes, value);
	file->write(bytes, sizeof(bytes));
}

static void writeBinaryFloat(LFile *file, float value) {
	byte bytes[4];

	WRITE_LE_FLOAT32(bytes, value);
	file->write(bytes, sizeof(bytes));
}

static void writeBinaryVector(LFile *file, const CVector &vector) {
	writeBinaryFloat(file, vector._x);
	writeBinaryFloat(file, vector._y);
	writeBinaryFloat(file, vector._z);
}

static void writeBinaryQuaternion(LFile *file, const CQuaternion &quaternion) {
	writeBinaryFloat(file, quaternion._x);
	writeBinaryFloat(file, quaternion._y);
	writeBinaryFloat(file, quaternion._z);
	writeBinaryFloat(file, quaternion._w);
}

static void writeBinaryColor(LFile *file, const Color &color) {
	writeBinaryInt(file, color._a);
	writeBinaryInt(file, color._r);
	writeBinaryInt(file, color._g);
	writeBinaryInt(file, color._b);
}

static void writeBinaryString(LFile *file, const LString &string) {
	writeBinaryInt(file, (int)string.size() + 1);
	file->write(string.c_str(), string.size() + 1);
}

bool Room_v2::loadText(const char *path) {
	char section[128];
	char lineBuffer[kLineBufferSize];
	LFile *file = _engine->getFileServices()->open(path, "rt");

	if (!file || !file->isOpen()) {
		logLoaderError(LString::format("Error: Invalid load filename '%s'.\n", path));

		return false;
	}

	section[0] = 0;

	while (file->readLine(lineBuffer, kLineBufferSize)) {
		if (sscanf(lineBuffer, "section: %127s", section) == 1) {
			if (!parseSection(file, section)) {
				logLoaderError(LString::format("Load of file %s failed due to syntax errors.\n", path));
				_engine->getFileServices()->close(file, true);

				return false;
			}
		}
	}

	_engine->getFileServices()->close(file, true);

	return true;
}

bool Room_v2::parseSection(LFile *file, const char *sectionName) {
	bool parsed;

	if (scumm_stricmp("setups", sectionName) == 0) {
		parsed = parseSetups(file);
	} else if (scumm_stricmp("lights", sectionName) == 0) {
		parsed = parseLights(file);
	} else if (scumm_stricmp("shadows", sectionName) == 0) {
		parsed = parseShadows(file);
	} else if (scumm_stricmp("sectors", sectionName) == 0) {
		parsed = parseSectors(file);
	} else {
		logLoaderError(LString::format("Warning: Unknown Section '%s'.\n", sectionName));

		return true;
	}

	if (!parsed) {
		logLoaderError(LString::format("Error while parsing section %s.\n", sectionName));

		return false;
	}

	return true;
}

bool Room_v2::parseSetups(LFile *file) {
	char token[kTokenSize];
	char backgroundToken[Setup::kNameSize];
	char lineBuffer[kLineBufferSize];
	CVector interest;
	int currentIndex = -1;

	token[0] = 0;
	backgroundToken[0] = 0;
	_setups = nullptr;

	if (!file->readLine(lineBuffer, kLineBufferSize))
		return true;

	while (true) {
		int count;

		if (sscanf(lineBuffer, " section: %255s\n", token) != 0) {
			if (!parseSection(file, token))
				return false;
		} else if (sscanf(lineBuffer, " numsetups %d\n", &count) == 1) {
			if (count < 0 || count > kMaxSetups) {
				warning("Room_v2::parseSetups(): invalid number of setups %d", count);

				return false;
			}

			_numSetups = count;
			_setups = Setup::newArray(_engine, count);
			currentIndex = -1;
		} else {
			if (!_setups)
				return false;

			if (sscanf(lineBuffer, " setup %255s", token) == 1) {
				currentIndex++;

				if (currentIndex >= _numSetups) {
					logLoaderError("Too many setups in file!\n");

					return false;
				}

				if (strlen(token) >= Setup::kNameSize)
					warning("Room_v2::parseSetups(): the setup name is too long");

				Common::strlcpy(_setups[currentIndex]._name, token, Setup::kNameSize);
			} else if (currentIndex < 0) {
				logLoaderError(LString::format("Error parsing setups: Expected 'setup <name>', but got '%s'.\n", lineBuffer));

				return false;
			} else {
				Setup *setup = &_setups[currentIndex];
				Camera *camera = &setup->_camera;
				CVector vector;
				float roll;
				float x;
				float y;
				float z;
				float w;
				float fov;

				// The disasm reads the background name and then ignores it
				if (sscanf(lineBuffer, " background %127s", backgroundToken) == 1) {
				} else if (sscanf(lineBuffer, " tiledscene %127s", backgroundToken) == 1) {
					setup->_backgroundName = backgroundToken;
				} else if (sscanf(lineBuffer, " position %f %f %f", &x, &y, &z) == 3) {
					vector._x = x;
					vector._y = y;
					vector._z = z;
					camera->setLocalPosition(vector);
				} else if (sscanf(lineBuffer, " interest %f %f %f", &x, &y, &z) == 3) {
					interest._x = x;
					interest._y = y;
					interest._z = z;
					camera->lookAt(interest);
				} else if (sscanf(lineBuffer, " roll %f", &roll) == 1) {
					float angle = -(kDegToRad * roll);
					CVector axis;

					axis._x = 0.0f;
					axis._y = 0.0f;
					axis._z = 1.0f;

					CQuaternion rollRotation(axis, angle);

					if ((camera->_nodeFlags & CNode::kQuaternionValid) == 0)
						camera->updateLocalQuaternion();

					const CQuaternion &current = camera->_quat;
					float newW = (float)((double)rollRotation._w * current._w - (double)rollRotation._x * current._x - (double)rollRotation._y * current._y - (double)rollRotation._z * current._z);
					float newZ = (float)((double)rollRotation._w * current._z + (double)rollRotation._z * current._w + (double)rollRotation._y * current._x - (double)rollRotation._x * current._y);
					float newY = (float)((double)rollRotation._x * current._z + (double)rollRotation._y * current._w + (double)rollRotation._w * current._y - (double)rollRotation._z * current._x);
					float newX = (float)((double)rollRotation._z * current._y + (double)rollRotation._x * current._w + (double)rollRotation._w * current._x - (double)rollRotation._y * current._z);

					setNodeRotation(camera, CQuaternion(newX, newY, newZ, newW));
				} else if (sscanf(lineBuffer, " direction %f %f %f %f", &x, &y, &z, &w) == 4) {
					setNodeRotation(camera, CQuaternion(x, y, z, w));
				} else if (sscanf(lineBuffer, " fov %f", &fov) == 1) {
					IRect viewport;

					viewport.x = 0;
					viewport.y = 0;
					viewport.w = 640;
					viewport.h = 480;
					camera->setViewport(&viewport, fov);
				} else if (sscanf(lineBuffer, " nclip %f", &camera->_nearClip) == 1) {
				} else if (sscanf(lineBuffer, " fclip %f", &camera->_farClip) == 1) {
				} else {
					logLoaderError(LString::format("Unrecognized line in set file: '%s'\n", lineBuffer));

					return false;
				}
			}
		}

		if (!file->readLine(lineBuffer, kLineBufferSize))
			return true;
	}
}

bool Room_v2::parseLights(LFile *file) {
	char token[kTokenSize];
	char lineBuffer[kLineBufferSize];
	CLightNode *lightNode = nullptr;
	CLight *light = nullptr;
	float x;
	float y;
	float z;
	float w;
	int red;
	int green;
	int blue;

	token[0] = 0;

	if (!file->readLine(lineBuffer, kLineBufferSize))
		return true;

	while (true) {
		if (sscanf(lineBuffer, " section: %255s\n", token) != 0) {
			if (!parseSection(file, token))
				return false;
		} else if (sscanf(lineBuffer, " light %255s", token) == 1) {
			lightNode = new CLightNode(_engine);
			addLight(lightNode);
			setLightNodeName(lightNode, token);
			light = new CLight(Color(1u), 1.0f, 10.0f, 10.0f);
			lightNode->setLight(light);
			lightNode->setActive(true);
		} else if (!lightNode || !light) {
			warning("Room_v2::parseLights(): the line '%s' comes before any light", lineBuffer);
		} else if (sscanf(lineBuffer, " type %255s", token) == 1) {
			if (strcmp(token, "omni") == 0)
				light->_type = CLight::kTypeOmni;
			else if (strcmp(token, "spot") == 0)
				light->_type = CLight::kTypeSpot;
			else if (strcmp(token, "direct") == 0)
				light->_type = CLight::kTypeDirect;
			else
				light->_type = CLight::kTypeAmbient;
		} else if (sscanf(lineBuffer, " position %f %f %f", &x, &y, &z) == 3) {
			CVector position;

			position._x = x;
			position._y = y;
			position._z = z;
			lightNode->setLocalPosition(position);
		} else if (sscanf(lineBuffer, " direction %f %f %f", &x, &y, &z) == 3) {
			CVector euler;

			euler._x = x;
			euler._y = y;
			euler._z = z;
			setNodeEuler(lightNode, euler);
		} else if (sscanf(lineBuffer, " orientation %f %f %f %f", &x, &y, &z, &w) == 4) {
			setNodeRotation(lightNode, CQuaternion(x, y, z, w));
		} else if (sscanf(lineBuffer, " intensity %f", &light->_intensity) == 1) {
		} else if (sscanf(lineBuffer, " umbraangle %f", &x) == 1) {
			light->_umbraAngle = kDegToRad * x;
		} else if (sscanf(lineBuffer, " penumbraangle %f", &x) == 1) {
			light->_penumbraAngle = kDegToRad * x;
		} else if (sscanf(lineBuffer, " color %d %d %d", &red, &green, &blue) == 3) {
			light->setColor(red, green, blue);
		} else if (sscanf(lineBuffer, " focusdistance %f", &light->_focusDistance) != 1) {
			sscanf(lineBuffer, " spreaddistance %f", &light->_spreadDistance);
		}

		if (!file->readLine(lineBuffer, kLineBufferSize))
			return true;
	}
}

bool Room_v2::parseShadows(LFile *file) {
	char token[kTokenSize];
	char lineBuffer[kLineBufferSize];
	Shadow_v2 *shadow = nullptr;
	float x;
	float y;
	float z;
	int red;
	int green;
	int blue;

	token[0] = 0;

	if (!file->readLine(lineBuffer, kLineBufferSize))
		return true;

	while (true) {
		if (sscanf(lineBuffer, " section: %255s\n", token) != 0) {
			if (!parseSection(file, token))
				return false;
		} else if (sscanf(lineBuffer, " shadow %255s", token) == 1) {
			shadow = newShadow();
			shadow->setName(LString(token));
		} else if (!shadow) {
			warning("Room_v2::parseShadows(): the line '%s' comes before any shadow", lineBuffer);
		} else if (sscanf(lineBuffer, " light %255s", token) == 1) {
			shadow->setLightName(LString(token));
		} else if (sscanf(lineBuffer, " point %f %f %f", &x, &y, &z) == 3) {
			shadow->_point[0] = x;
			shadow->_point[1] = y;
			shadow->_point[2] = z;
		} else if (sscanf(lineBuffer, " color %d %d %d", &red, &green, &blue) == 3) {
			Color shadowColor;

			shadowColor.setRGB(red, green, blue);
			shadow->setColor(shadowColor);
		} else if (sscanf(lineBuffer, " sector %255s", token) == 1) {
			shadow->addSector(LString(token));
		}

		if (!file->readLine(lineBuffer, kLineBufferSize))
			return true;
	}
}

bool Room_v2::parseSectors(LFile *file) {
	_walkPlanes = new WalkPlaneCollection(_engine);

	char *nextSection = _walkPlanes->parseCollection(file);

	// The disasm ignores the result of parsing the following section
	if (nextSection)
		parseSection(file, nextSection);

	_walkPlanes->buildNeighbors();

	return true;
}

bool Room_v2::loadBinary(const char *path) {
	LFile *file = _engine->getFileServices()->open(path, "rb");

	if (!file)
		return false;

	char buffer[kBinaryStringSize + 1];
	int count;

	if (!readBinaryInt(file, &count))
		return failBinaryLoad(file, "unexpected end of file");

	if (count < 0 || count > kMaxSetups)
		return failBinaryLoad(file, "invalid number of setups");

	_numSetups = count;
	_setups = Setup::newArray(_engine, count);

	for (int i = 0; i < _numSetups; i++) {
		Setup *setup = &_setups[i];
		Camera *camera = &setup->_camera;
		CVector position;
		CQuaternion rotation;
		float fov;
		IRect viewport;

		if (!readBinaryBytes(file, setup->_name, Setup::kNameSize) || !readBinaryString(file, buffer))
			return failBinaryLoad(file, "invalid setup");

		if (!memchr(setup->_name, 0, Setup::kNameSize)) {
			warning("Room_v2::loadBinary(): the setup name is not terminated");
			setup->_name[Setup::kNameSize - 1] = 0;
		}

		setup->_backgroundName = buffer;

		if (!readBinaryVector(file, &position))
			return failBinaryLoad(file, "invalid setup position");

		camera->setLocalPosition(position);

		if (!readBinaryQuaternion(file, &rotation))
			return failBinaryLoad(file, "invalid setup rotation");

		setNodeRotation(camera, rotation);

		if (!readBinaryFloat(file, &fov))
			return failBinaryLoad(file, "invalid setup field of view");

		viewport.x = 0;
		viewport.y = 0;
		viewport.w = 640;
		viewport.h = 480;
		camera->setViewport(&viewport, fov);

		if (!readBinaryFloat(file, &camera->_nearClip) || !readBinaryFloat(file, &camera->_farClip))
			return failBinaryLoad(file, "invalid setup clipping planes");
	}

	if (!readBinaryInt(file, &count))
		return failBinaryLoad(file, "unexpected end of file");

	for (int i = 0; i < count; i++) {
		CLightNode *lightNode = new CLightNode(_engine);
		CVector position;
		CQuaternion rotation;

		addLight(lightNode);

		if (!readBinaryBytes(file, lightNode->_name, CNode::kNameSize))
			return failBinaryLoad(file, "invalid light name");

		if (!memchr(lightNode->_name, 0, CNode::kNameSize)) {
			warning("Room_v2::loadBinary(): the light name is not terminated");
			lightNode->_name[CNode::kNameSize - 1] = 0;
		}

		CLight *light = new CLight(Color(1u), 1.0f, 10.0f, 10.0f);

		lightNode->setLight(light);
		lightNode->setActive(true);

		if (!readBinaryVector(file, &position))
			return failBinaryLoad(file, "invalid light position");

		lightNode->setLocalPosition(position);

		if (!readBinaryQuaternion(file, &rotation))
			return failBinaryLoad(file, "invalid light rotation");

		setNodeRotation(lightNode, rotation);

		if (!readBinaryInt(file, &light->_type) || !readBinaryFloat(file, &light->_intensity) || !readBinaryColor(file, &light->_color) ||
			!readBinaryFloat(file, &light->_focusDistance) || !readBinaryFloat(file, &light->_spreadDistance) ||
			!readBinaryFloat(file, &light->_umbraAngle) || !readBinaryFloat(file, &light->_penumbraAngle))
			return failBinaryLoad(file, "invalid light");
	}

	if (!readBinaryInt(file, &count))
		return failBinaryLoad(file, "unexpected end of file");

	if (count > 0) {
		float vertices[WalkPlane_v2::kMaxVertices * 3];

		_walkPlanes = new WalkPlaneCollection(_engine);
		memset(vertices, 0, sizeof(vertices));

		for (int i = 0; i < count; i++) {
			int vertexCount;

			if (!readBinaryInt(file, &vertexCount))
				return failBinaryLoad(file, "unexpected end of file");

			if (vertexCount < 2 || vertexCount > WalkPlane_v2::kMaxVertices)
				return failBinaryLoad(file, "invalid number of sector vertices");

			for (int j = 0; j < vertexCount * 3; j++) {
				if (!readBinaryFloat(file, &vertices[j]))
					return failBinaryLoad(file, "invalid sector vertices");
			}

			WalkPlane_v2 *plane = _walkPlanes->createPlane(vertices, vertexCount);
			int planeType;
			byte activeByte;
			float tolerance;

			_walkPlanes->_planes.push_back(plane);

			if (!readBinaryString(file, buffer))
				return failBinaryLoad(file, "invalid sector name");

			plane->_name = buffer;

			if (!readBinaryInt(file, &plane->_number) || !readBinaryBytes(file, &activeByte, 1) || !readBinaryInt(file, &planeType))
				return failBinaryLoad(file, "invalid sector");

			plane->_active = activeByte != 0;
			plane->setType(planeType);

			if (!readBinaryInt(file, &plane->_numSortOrders))
				return failBinaryLoad(file, "invalid sector sort orders");

			if (plane->_numSortOrders < 0 || plane->_numSortOrders > WalkPlane_v2::kMaxSortOrders) {
				plane->_numSortOrders = 0;

				return failBinaryLoad(file, "invalid number of sector sort orders");
			}

			for (int j = 0; j < plane->_numSortOrders; j++) {
				if (!readBinaryInt(file, &plane->_sortOrder[j]))
					return failBinaryLoad(file, "invalid sector sort orders");
			}

			if (!readBinaryFloat(file, &tolerance))
				return failBinaryLoad(file, "invalid sector tolerance");

			if (!(tolerance < 0.0f || tolerance > 0.0f))
				plane->_tolerance = 0.005f;
			else
				plane->_tolerance = tolerance;
		}

		_walkPlanes->buildNeighbors();
	}

	if (!readBinaryInt(file, &count))
		return failBinaryLoad(file, "unexpected end of file");

	for (int i = 0; i < count; i++) {
		Shadow_v2 *shadow = newShadow();
		int sectorCount;

		if (!readBinaryString(file, buffer))
			return failBinaryLoad(file, "invalid shadow name");

		shadow->_name = buffer;

		if (!readBinaryString(file, buffer))
			return failBinaryLoad(file, "invalid shadow light name");

		shadow->_lightName = buffer;

		if (!readBinaryFloat(file, &shadow->_point[0]) || !readBinaryFloat(file, &shadow->_point[1]) || !readBinaryFloat(file, &shadow->_point[2]) ||
			!readBinaryInt(file, &sectorCount))
			return failBinaryLoad(file, "invalid shadow");

		for (int j = 0; j < sectorCount; j++) {
			WalkPlane_v2 *sector = nullptr;

			if (!readBinaryString(file, buffer))
				return failBinaryLoad(file, "invalid shadow sector name");

			if (_walkPlanes)
				sector = _walkPlanes->planeNamed(LString(buffer));
			else
				warning("Room_v2::loadBinary(): the shadow sector '%s' has no sectors to be found in", buffer);

			shadow->_sectors.push_back(sector);
		}

		if (!readBinaryColor(file, &shadow->_color))
			return failBinaryLoad(file, "invalid shadow color");
	}

	_engine->getFileServices()->close(file, true);

	return true;
}

bool Room_v2::saveBinary(const char *path) {
	LFile *file = _engine->getFileServices()->open(path, "wb");

	if (!file)
		return false;

	writeBinaryInt(file, _numSetups);

	for (int i = 0; i < _numSetups; i++) {
		Camera *camera = &_setups[i]._camera;

		file->write(_setups[i]._name, Setup::kNameSize);
		writeBinaryString(file, _setups[i]._backgroundName);

		if ((camera->_nodeFlags & CNode::kWorldValid) == 0)
			camera->updateWorldTransform();

		writeBinaryVector(file, camera->_world._pos);
		writeBinaryQuaternion(file, camera->getWorldOrientation());
		writeBinaryFloat(file, camera->_fovX);
		writeBinaryFloat(file, camera->_nearClip);
		writeBinaryFloat(file, camera->_farClip);
	}

	writeBinaryInt(file, (int)_lights.size());

	Common::List<CLightNode *>::iterator lightIt;

	for (lightIt = _lights.begin(); lightIt != _lights.end(); ++lightIt) {
		CLightNode *lightNode = *lightIt;
		CLight defaultLight(Color(1u), 1.0f, 10.0f, 10.0f);
		CLight *light = lightNode->_light;

		if (!light) {
			warning("Room_v2::saveBinary(): a light node has no light");
			light = &defaultLight;
		}

		file->write(lightNode->_name, CNode::kNameSize);

		if ((lightNode->_nodeFlags & CNode::kWorldValid) == 0)
			lightNode->updateWorldTransform();

		writeBinaryVector(file, lightNode->_world._pos);
		writeBinaryQuaternion(file, lightNode->getWorldOrientation());
		writeBinaryInt(file, light->_type);
		writeBinaryFloat(file, light->_intensity);
		writeBinaryColor(file, light->_color);
		writeBinaryFloat(file, light->_focusDistance);
		writeBinaryFloat(file, light->_spreadDistance);
		writeBinaryFloat(file, light->_umbraAngle);
		writeBinaryFloat(file, light->_penumbraAngle);
	}

	int planeCount = 0;

	if (_walkPlanes)
		planeCount = (int)_walkPlanes->_planes.size();

	writeBinaryInt(file, planeCount);

	if (planeCount > 0) {
		Common::List<WalkPlane_v2 *>::iterator planeIt;

		for (planeIt = _walkPlanes->_planes.begin(); planeIt != _walkPlanes->_planes.end(); ++planeIt) {
			WalkPlane_v2 *plane = *planeIt;

			if (plane->_numVerts < 0 || plane->_numVerts > WalkPlane_v2::kMaxVertices) {
				warning("Room_v2::saveBinary(): invalid number of sector vertices");
				plane->_numVerts = 0;
			}

			if (plane->_numSortOrders < 0 || plane->_numSortOrders > WalkPlane_v2::kMaxSortOrders) {
				warning("Room_v2::saveBinary(): invalid number of sector sort orders");
				plane->_numSortOrders = 0;
			}

			writeBinaryInt(file, plane->_numVerts);

			for (int i = 0; i < plane->_numVerts; i++) {
				if (plane->_verts[i]) {
					writeBinaryFloat(file, plane->_verts[i]->x);
					writeBinaryFloat(file, plane->_verts[i]->y);
					writeBinaryFloat(file, plane->_verts[i]->z);
				} else {
					warning("Room_v2::saveBinary(): missing sector vertex");
					writeBinaryFloat(file, 0.0f);
					writeBinaryFloat(file, 0.0f);
					writeBinaryFloat(file, 0.0f);
				}
			}

			writeBinaryString(file, plane->_name);
			writeBinaryInt(file, plane->_number);

			byte activeByte = plane->_active ? 1 : 0;

			file->write(&activeByte, 1);
			writeBinaryInt(file, plane->_type);
			writeBinaryInt(file, plane->_numSortOrders);

			for (int i = 0; i < plane->_numSortOrders; i++)
				writeBinaryInt(file, plane->_sortOrder[i]);

			writeBinaryFloat(file, plane->_tolerance);
		}
	}

	writeBinaryInt(file, (int)_shadows.size());

	Common::List<Shadow_v2 *>::iterator shadowIt;

	for (shadowIt = _shadows.begin(); shadowIt != _shadows.end(); ++shadowIt) {
		Shadow_v2 *shadow = *shadowIt;

		writeBinaryString(file, shadow->_name);
		writeBinaryString(file, shadow->_lightName);
		writeBinaryFloat(file, shadow->_point[0]);
		writeBinaryFloat(file, shadow->_point[1]);
		writeBinaryFloat(file, shadow->_point[2]);
		writeBinaryInt(file, (int)shadow->_sectors.size());

		for (uint i = 0; i < shadow->_sectors.size(); i++) {
			if (shadow->_sectors[i]) {
				writeBinaryString(file, shadow->_sectors[i]->_name);
			} else {
				warning("Room_v2::saveBinary(): unresolved shadow sector");
				writeBinaryString(file, LString());
			}
		}

		writeBinaryColor(file, shadow->_color);
	}

	_engine->getFileServices()->close(file, true);

	return true;
}

} // End of namespace Grim
