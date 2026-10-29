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

#include "common/archive.h"

#include "engines/grim/debug.h"
#include "engines/grim/set.h"
#include "engines/grim/textsplit.h"
#include "engines/grim/colormap.h"
#include "engines/grim/grim.h"
#include "engines/grim/savegame.h"
#include "engines/grim/resource.h"
#include "engines/grim/bitmap.h"
#include "engines/grim/gfx_base.h"
#include "engines/grim/sound.h"
#include "engines/grim/sincos.h"
#include "engines/grim/imuse/imuse.h"
#include "engines/grim/emi/sound/emisound.h"

#include "math/frustum.h"

namespace Grim {

Set::Set(const Common::String &sceneName, Common::SeekableReadStream *data) :
		_locked(false), _name(sceneName), _enableLights(false) {

	char header[7];
	data->read(header, 7);
	data->seek(0, SEEK_SET);
	if (memcmp(header, "section", 7) == 0) {
		TextSplitter ts(_name, data);
		loadText(ts);
	} else {
		loadBinary(data);
	}
	setupOverworldLights();
}

Set::Set() :
		_cmaps(nullptr), _locked(false), _enableLights(false), _numSetups(0),
		_numLights(0), _numSectors(0), _numObjectStates(0), _minVolume(0),
		_maxVolume(0), _numCmaps(0), _numShadows(0), _currSetup(nullptr),
		_setups(nullptr), _lights(nullptr), _sectors(nullptr), _shadows(nullptr) {
	setupOverworldLights();
}

Set::~Set() {
	if (_cmaps || g_grim->getGameType() == GType_MONKEY4) {
		delete[] _cmaps;
		for (int i = 0; i < _numSetups; ++i) {
			delete _setups[i]._bkgndBm;
			delete _setups[i]._bkgndZBm;
		}
		delete[] _setups;
		turnOffLights();
		delete[] _lights;
		for (int i = 0; i < _numSectors; ++i) {
			delete _sectors[i];
		}
		delete[] _sectors;
		while (!_states.empty()) {
			ObjectState *s = _states.front();
			_states.pop_front();
			delete s;
		}
		delete[] _shadows;
	}
	for (Light *l : _overworldLightsList) {
		delete l;
	}
}

void Set::setupOverworldLights() {
	Light *l;

	l = new Light();
	l->_name = "Overworld Light 1";
	l->_enabled = true;
	l->_type = Light::Ambient;
	l->_pos = Math::Vector3d(0, 0, 0);
	l->_dir = Math::Vector3d(0, 0, 0);
	l->_color = Color(255, 255, 255);
	l->setIntensity(0.5f);
	_overworldLightsList.push_back(l);

	l = new Light();
	l->_name = "Overworld Light 2";
	l->_enabled = true;
	l->_type = Light::Direct;
	l->_pos = Math::Vector3d(0, 0, 0);
	l->_dir = Math::Vector3d(0, 0, -1);
	l->_color = Color(255, 255, 255);
	l->setIntensity(0.6f);
	_overworldLightsList.push_back(l);
}

void Set::loadText(TextSplitter &ts) {
	char tempBuf[256];

	ts.expectString("section: colormaps");
	ts.scanString(" numcolormaps %d", 1, &_numCmaps);
	_cmaps = new ObjectPtr<CMap>[_numCmaps];
	char cmap_name[256];
	for (int i = 0; i < _numCmaps; i++) {
		ts.scanString(" colormap %256s", 1, cmap_name);
		_cmaps[i] = g_resourceloader->getColormap(cmap_name);
	}

	if (ts.checkString("section: objectstates") || ts.checkString("sections: object_states")) {
		ts.nextLine();
		ts.scanString(" tot_objects %d", 1, &_numObjectStates);
		char object_name[256];
		for (int l = 0; l < _numObjectStates; l++) {
			ts.scanString(" object %256s", 1, object_name);
		}
	} else {
		_numObjectStates = 0;
	}

	ts.expectString("section: setups");
	ts.scanString(" numsetups %d", 1, &_numSetups);
	_setups = new Setup[_numSetups];
	for (int i = 0; i < _numSetups; i++)
		_setups[i].load(this, i, ts);
	_currSetup = _setups;

	_numShadows = 0;
	_numSectors = -1;
	_numLights = -1;
	_lights = nullptr;
	_sectors = nullptr;
	_shadows = nullptr;

	_minVolume = 0;
	_maxVolume = 0;

	// Lights are optional
	if (ts.isEof())
		return;

	ts.expectString("section: lights");
	ts.scanString(" numlights %d", 1, &_numLights);
	_lights = new Light[_numLights];
	for (int i = 0; i < _numLights; i++) {
		_lights[i].load(ts);
		_lights[i]._id = i;
		_lightsList.push_back(&_lights[i]);
	}

	// Calculate the number of sectors
	ts.expectString("section: sectors");
	if (ts.isEof()) // Sectors are optional, but section: doesn't seem to be
		return;

	int sectorStart = ts.getLineNumber();
	_numSectors = 0;
	// Find the number of sectors (while the sectors usually
	// count down from the highest number there are a few
	// cases where they count up, see hh.set for example)
	while (!ts.isEof()) {
		ts.scanString(" %s", 1, tempBuf);
		if (!scumm_stricmp(tempBuf, "sector"))
			_numSectors++;
	}
	// Allocate and fill an array of sector info
	_sectors = new Sector*[_numSectors];
	ts.setLineNumber(sectorStart);
	Common::Array<Math::Vector3d> &weldPool = _weldPool;
	weldPool.clear();
	_searchOrder.clear();
	for (int i = 0; i < _numSectors; i++) {
		// Use the ids as index for the sector in the array.
		// This way when looping they are checked from the id 0 sto the last,
		// which seems important for sets with overlapping camera sectors, like ga.set.
		Sector *s = new Sector();
		s->load(ts);
		// The original joins the vertices that lie closer than 1e-4 to one another (the first one loaded is used) and searches the sectors from the
		// last one loaded to the first
		s->weldVertices(weldPool);
		_sectors[s->getSectorId()] = s;
		if (g_grim->getGameType() == GType_GRIM)
			_searchOrder.insert_at(0, s);
	}
}

void Set::loadBinary(Common::SeekableReadStream *data) {
	// yes, an array of size 0
	_cmaps = nullptr;//new CMapPtr[0];
	_numCmaps = 0;
	_numObjectStates = 0;


	_numSetups = data->readUint32LE();
	_setups = new Setup[_numSetups];
	for (int i = 0; i < _numSetups; i++)
		_setups[i].loadBinary(data);
	_currSetup = _setups;

	_numSectors = 0;
	_numLights = 0;
	_lights = nullptr;
	_sectors = nullptr;
	_shadows = nullptr;

	_minVolume = 0;
	_maxVolume = 0;

	// the rest may or may not be optional. Might be a good idea to check if there is no more data.

	_numLights = data->readUint32LE();
	_lights = new Light[_numLights];
	for (int i = 0; i < _numLights; i++) {
		_lights[i].loadBinary(data);
		_lights[i]._id = i;
		_lightsList.push_back(&_lights[i]);
	}

	_numSectors = data->readUint32LE();
	// Allocate and fill an array of sector info
	_sectors = new Sector*[_numSectors];
	for (int i = 0; i < _numSectors; i++) {
		_sectors[i] = new Sector();
		_sectors[i]->loadBinary(data);
	}

	_numShadows = data->readUint32LE();
	_shadows = new SetShadow[_numShadows];

	for (int i = 0; i < _numShadows; ++i) {
		_shadows[i].loadBinary(data, this);
	}

	// Enable lights by default
	_enableLights = true;
}

void Set::saveState(SaveGame *savedState) const {
	savedState->writeString(_name);
	if (g_grim->getGameType() == GType_GRIM) {
		savedState->writeLESint32(_numCmaps);
		for (int i = 0; i < _numCmaps; ++i) {
			savedState->writeString(_cmaps[i]->getFilename());
		}
	}
	savedState->writeLEUint32((uint32)(_currSetup - _setups)); // current setup id
	savedState->writeBool(_locked);
	savedState->writeBool(_enableLights);
	savedState->writeLESint32(_minVolume);
	savedState->writeLESint32(_maxVolume);

	savedState->writeLEUint32(_states.size());
	for (StateList::const_iterator i = _states.begin(); i != _states.end(); ++i) {
		savedState->writeLESint32((*i)->getId());
	}

	// Setups
	savedState->writeLESint32(_numSetups);
	for (int i = 0; i < _numSetups; ++i) {
		_setups[i].saveState(savedState);
	}

	// Sectors
	savedState->writeLESint32(_numSectors);
	for (int i = 0; i < _numSectors; ++i) {
		_sectors[i]->saveState(savedState);
	}

	// Lights
	savedState->writeLESint32(_numLights);
	for (int i = 0; i < _numLights; ++i) {
		_lights[i].saveState(savedState);
	}

	// Shadows
	savedState->writeLESint32(_numShadows);
	for (int i = 0; i < _numShadows; ++i) {
		_shadows[i].saveState(savedState);
	}
}

// The sectors are searched from the last one of the set file to the first (SectorList_FindByPoint), the order that loadText keeps: a restored set has the
// sectors by their ids only, so the order is read from the set file again (the ids in the order of the file); a set file that is not there gives the ids
// from the last to the first, the order of the files of the game
void Set::rebuildSearchOrder() {
	_searchOrder.clear();
	Common::Array<int> ids;
	Common::SeekableReadStream *stream = g_resourceloader->openNewStreamFile(_name.c_str(), true);
	if (stream) {
		bool inSector = false;
		while (!stream->eos()) {
			Common::String line = stream->readLine();
			line.trim();
			if (line.hasPrefix("sector") && !line.hasPrefix("sectors") && line.size() > 6 && Common::isSpace(line[6])) {
				inSector = true;
			} else if (inSector && line.hasPrefix("ID")) {
				ids.push_back(atoi(line.c_str() + 2));
				inSector = false;
			}
		}
		delete stream;
	}
	if (ids.size() != (uint)_numSectors) {
		ids.clear();
		for (int i = _numSectors - 1; i >= 0; i--)
			ids.push_back(i);
	}
	for (uint i = 0; i < ids.size(); i++) {
		const int id = ids[i];
		if (id >= 0 && id < _numSectors && _sectors[id])
			_searchOrder.insert_at(0, _sectors[id]);
	}
	if (_searchOrder.size() != (uint)_numSectors)
		_searchOrder.clear();
}

bool Set::restoreState(SaveGame *savedState) {
	_name = savedState->readString();
	if (g_grim->getGameType() == GType_GRIM) {
		_numCmaps = savedState->readLESint32();
		_cmaps = new CMapPtr[_numCmaps];
		for (int i = 0; i < _numCmaps; ++i) {
			Common::String str = savedState->readString();
			_cmaps[i] = g_resourceloader->getColormap(str);
		}
	}

	int32 currSetupId = savedState->readLEUint32();
	_locked           = savedState->readBool();
	_enableLights     = savedState->readBool();
	_minVolume        = savedState->readLESint32();
	_maxVolume        = savedState->readLESint32();

	_numObjectStates = savedState->readLESint32();
	_states.clear();
	for (int i = 0; i < _numObjectStates; ++i) {
		int32 id = savedState->readLESint32();
		ObjectState *o = ObjectState::getPool().getObject(id);
		_states.push_back(o);
	}

	//Setups
	_numSetups = savedState->readLESint32();
	_setups = new Setup[_numSetups];
	_currSetup = _setups + currSetupId;
	for (int i = 0; i < _numSetups; ++i) {
		_setups[i].restoreState(savedState);
	}

	//Sectors
	_numSectors = savedState->readLESint32();
	if (_numSectors > 0) {
		_sectors = new Sector*[_numSectors];
		for (int i = 0; i < _numSectors; ++i) {
			_sectors[i] = new Sector();
			_sectors[i]->restoreState(savedState);
		}
	} else {
		_sectors = nullptr;
	}
	if (g_grim->getGameType() == GType_GRIM)
		rebuildSearchOrder();

	_numLights = savedState->readLESint32();
	_lights = new Light[_numLights];
	for (int i = 0; i < _numLights; i++) {
		_lights[i].restoreState(savedState);
		_lights[i]._id = i;
		_lightsList.push_back(&_lights[i]);
	}

	if (savedState->saveMinorVersion() >= 19) {
		_numShadows = savedState->readLESint32();
		_shadows = new SetShadow[_numShadows];
		for (int i = 0; i < _numShadows; ++i) {
			_shadows[i].restoreState(savedState);
		}
	}

	return true;
}

void Set::Setup::load(Set *set, int id, TextSplitter &ts) {
	char buf[256];

	ts.scanString(" setup %256s", 1, buf);
	_name = buf;

	ts.scanString(" background %256s", 1, buf);
	_bkgndBm = loadBackground(buf);

	// ZBuffer is optional
	_bkgndZBm = nullptr;
	if (ts.checkString("zbuffer")) {
		ts.scanString(" zbuffer %256s", 1, buf);
		// Don't even try to load if it's the "none" bitmap
		if (strcmp(buf, "<none>.lbm") != 0) {
			if (SearchMan.hasFile(Common::Path(buf))) {
				_bkgndZBm = Bitmap::create(buf);
				Debug::debug(Debug::Bitmaps | Debug::Sets,
							 "Loading scene z-buffer bitmap: %s\n", buf);
			} else {
				Debug::debug(Debug::Bitmaps | Debug::Sets,
							 "Scene z-buffer bitmap %s is missing\n", buf);
			}
		}
	}

	ts.scanString(" position %f %f %f", 3, &_pos.x(), &_pos.y(), &_pos.z());
	ts.scanString(" interest %f %f %f", 3, &_interest.x(), &_interest.y(), &_interest.z());
	ts.scanString(" roll %f", 1, &_roll);
	ts.scanString(" fov %f", 1, &_fov);
	ts.scanString(" nclip %f", 1, &_nclip);
	ts.scanString(" fclip %f", 1, &_fclip);
	for (;;) {
		char name[256], zname[256];
		char bitmap[256], zbitmap[256];
		zbitmap[0] = '\0';
		if (ts.checkString("object_art"))
			ts.scanString(" object_art %256s %256s", 2, name, bitmap);
		else
			break;
		if (ts.checkString("object_z"))
			ts.scanString(" object_z %256s %256s", 2, zname, zbitmap);

		if (zbitmap[0] == '\0' || strcmp(name, zname) == 0) {
			set->addObjectState(id, ObjectState::OBJSTATE_BACKGROUND, bitmap, zbitmap, true);
		}
	}
}

void Set::Setup::loadBinary(Common::SeekableReadStream *data) {
	char name[128];
	data->read(name, 128);
	_name = Common::String(name);

	// Skip an unknown number (this is the stringlength of the following string)
	int fNameLen = 0;
	fNameLen = data->readUint32LE();

	char *fileName = new char[fNameLen];
	data->read(fileName, fNameLen);

	_bkgndZBm = nullptr;
	_bkgndBm = loadBackground(fileName);

	_pos.readFromStream(data);

	Math::Quaternion q;
	q.readFromStream(data);
	q.toMatrix(_rot);

	_fov   = data->readFloatLE();
	_nclip = data->readFloatLE();
	_fclip = data->readFloatLE();

	delete[] fileName;
}

void Set::Setup::saveState(SaveGame *savedState) const {
	//name
	savedState->writeString(_name);

	//bkgndBm
	if (_bkgndBm) {
		savedState->writeLESint32(_bkgndBm->getId());
	} else {
		savedState->writeLESint32(0);
	}

	// bkgndZBm
	if (_bkgndZBm) {
		savedState->writeLESint32(_bkgndZBm->getId());
	} else {
		savedState->writeLESint32(0);
	}

	savedState->writeVector3d(_pos);
	if (g_grim->getGameType() == GType_MONKEY4) {
		// Get the rotation matrix as a quaternion and write it out
		Math::Quaternion q(_rot);
		savedState->writeFloat(q.x());
		savedState->writeFloat(q.y());
		savedState->writeFloat(q.z());
		savedState->writeFloat(q.w());
	} else {
		savedState->writeVector3d(_interest);
		savedState->writeFloat(_roll);
	}
	savedState->writeFloat(_fov);
	savedState->writeFloat(_nclip);
	savedState->writeFloat(_fclip);
}

bool Set::Setup::restoreState(SaveGame *savedState) {
	_name = savedState->readString();

	_bkgndBm = Bitmap::getPool().getObject(savedState->readLESint32());
	_bkgndZBm = Bitmap::getPool().getObject(savedState->readLESint32());

	_pos      = savedState->readVector3d();
	if (g_grim->getGameType() == GType_MONKEY4) {
		float x = savedState->readFloat();
		float y = savedState->readFloat();
		float z = savedState->readFloat();
		float w = savedState->readFloat();
		Math::Quaternion q(x, y, z, w);
		_rot = q.toMatrix();
	} else {
		_interest = savedState->readVector3d();
		_roll     = savedState->readFloat();
	}
	_fov      = savedState->readFloat();
	_nclip    = savedState->readFloat();
	_fclip    = savedState->readFloat();

	return true;
}

Light::Light() : _falloffNear(0.0f), _falloffFar(0.0f), _enabled(false), _id(0) {
	setIntensity(0.0f);
	setUmbraDegrees(0.0f);
	setPenumbraDegrees(0.0f);
}

void Set::Setup::getRotation(float *x, float *y, float *z) {
	Math::Angle aX, aY, aZ;
	if (g_grim->getGameType() == GType_MONKEY4)
		_rot.getEuler(&aX, &aY, &aZ, Math::EO_ZYX);
	else
		_rot.getEuler(&aX, &aY, &aZ, Math::EO_ZXY);

	if (x != nullptr)
		*x = aX.getDegrees();
	if (y != nullptr)
		*y = aY.getDegrees();
	if (z != nullptr)
		*z = aZ.getDegrees();
}

void Set::Setup::setPitch(Math::Angle pitch) {
	Math::Angle oldYaw, oldRoll;
	if (g_grim->getGameType() == GType_MONKEY4) {
		_rot.getEuler(&oldRoll, &oldYaw, nullptr, Math::EO_ZYX);
		_rot.buildFromEuler(oldRoll, oldYaw, pitch, Math::EO_ZYX);
	} else {
		_rot.getEuler(&oldYaw, nullptr, &oldRoll, Math::EO_ZXY);
		_rot.buildFromEuler(oldYaw, pitch, oldRoll, Math::EO_ZXY);
	}
}

void Set::Setup::setYaw(Math::Angle yaw) {
	Math::Angle oldPitch, oldRoll;
	if (g_grim->getGameType() == GType_MONKEY4) {
		_rot.getEuler(&oldRoll, nullptr, &oldPitch, Math::EO_ZYX);
		_rot.buildFromEuler(oldRoll, yaw, oldPitch, Math::EO_ZYX);
	} else {
		_rot.getEuler(nullptr, &oldPitch, &oldRoll, Math::EO_ZXY);
		_rot.buildFromEuler(yaw, oldPitch, oldRoll, Math::EO_ZXY);
	}
}

void Set::Setup::setRoll(Math::Angle roll) {
	Math::Angle oldPitch, oldYaw;
	if (g_grim->getGameType() == GType_MONKEY4) {
		_rot.getEuler(nullptr, &oldYaw, &oldPitch, Math::EO_ZYX);
		_rot.buildFromEuler(roll, oldYaw, oldPitch, Math::EO_ZYX);
	} else {
		_rot.getEuler(&oldYaw, &oldPitch, nullptr, Math::EO_ZXY);
		_rot.buildFromEuler(oldYaw, oldPitch, roll, Math::EO_ZXY);
	}
}

void Light::setUmbraDegrees(float angle) {
	_umbraangle = angle;
	_cosumbraangle = cosf(angle * (float)M_PI / 180.0f);
}

void Light::setUmbraRadians(float angle) {
	_umbraangle = angle;
	_cosumbraangle = cosf(angle);
}

void Light::setPenumbraDegrees(float angle) {
	_penumbraangle = angle;
	_cospenumbraangle = cosf(angle * (float)M_PI / 180.0f);
}

void Light::setPenumbraRadians(float angle) {
	_penumbraangle = angle;
	_cospenumbraangle = cosf(angle);
}

void Light::setIntensity(float intensity) {
	_intensity = intensity;
	if (g_grim->getGameType() == GType_MONKEY4) {
		_scaledintensity = intensity / 255;
	} else {
		_scaledintensity = intensity / 15;
	}
}

void Light::load(TextSplitter &ts) {
	char buf[256];
	float tmp;

	// Light names can be null, but ts doesn't seem flexible enough to allow this
	if (strlen(ts.getCurrentLine()) > strlen(" light"))
		ts.scanString(" light %256s", 1, buf);
	else {
		ts.nextLine();
		buf[0] = '\0';
	}
	_name = buf;

	ts.scanString(" type %256s", 1, buf);
	Common::String type = buf;
	if (type == "spot") {
		_type = Spot;
	} else if (type == "omni") {
		_type = Omni;
	} else if (type == "direct") {
		_type = Direct;
	} else {
		error("Light::load() Unknown type of light: %s", buf);
	}

	ts.scanString(" position %f %f %f", 3, &_pos.x(), &_pos.y(), &_pos.z());
	ts.scanString(" direction %f %f %f", 3, &_dir.x(), &_dir.y(), &_dir.z());
	ts.scanString(" intensity %f", 1, &tmp);
	setIntensity(tmp);
	ts.scanString(" umbraangle %f", 1, &tmp);
	setUmbraDegrees(tmp);
	ts.scanString(" penumbraangle %f", 1, &tmp);
	setPenumbraDegrees(tmp);

	int r, g, b;
	ts.scanString(" color %d %d %d", 3, &r, &g, &b);
	_color.getRed() = r;
	_color.getGreen() = g;
	_color.getBlue() = b;

	_enabled = true;
}

void Light::loadBinary(Common::SeekableReadStream *data) {
	char name[32];
	data->read(name, 32);
	_name = name;

	_pos.readFromStream(data);

	Math::Quaternion quat;
	quat.readFromStream(data);

	_dir.set(0, 0, -1);
	Math::Matrix4 rot = quat.toMatrix();
	rot.transform(&_dir, false);

	// This relies on the order of the LightType enum.
	_type = (LightType)data->readSint32LE();

	setIntensity(data->readFloatLE());

	int j = data->readSint32LE();
	// This always seems to be 0
	if (j != 0) {
		warning("Light::loadBinary j != 0");
	}

	_color.getRed() = data->readSint32LE();
	_color.getGreen() = data->readSint32LE();
	_color.getBlue() = data->readSint32LE();

	_falloffNear = data->readFloatLE();
	_falloffFar = data->readFloatLE();
	// the cone angles of the binary sets are in radians
	setUmbraRadians(data->readFloatLE());
	setPenumbraRadians(data->readFloatLE());

	_enabled = true;
}

void Light::saveState(SaveGame *savedState) const {
	// name
	savedState->writeString(_name);
	savedState->writeBool(_enabled);

	// type
	savedState->writeLEUint32(_type);

	savedState->writeVector3d(_pos);
	savedState->writeVector3d(_dir);

	savedState->writeColor(_color);

	savedState->writeFloat(_intensity);
	savedState->writeFloat(_umbraangle);
	savedState->writeFloat(_penumbraangle);

	savedState->writeFloat(_falloffNear);
	savedState->writeFloat(_falloffFar);
}

bool Light::restoreState(SaveGame *savedState) {
	_name = savedState->readString();
	_enabled = savedState->readBool();
	if (savedState->saveMinorVersion() > 7) {
		if (savedState->saveMinorVersion() >= 12) {
			_type = (LightType)savedState->readLEUint32();
		} else {
			int type = savedState->readLEUint32();
			if (type == 1) {
				_type = Spot;
			} else if (type == 2) {
				_type = Direct;
			} else if (type == 3) {
				_type = Omni;
			} else if (type == 4) {
				_type = Ambient;
			}
		}
	} else {
		Common::String type = savedState->readString();
		if (type == "spot") {
			_type = Spot;
		} else if (type == "omni") {
			_type = Omni;
		} else if (type == "direct") {
			_type = Direct;
		}
	}

	_pos           = savedState->readVector3d();
	_dir           = savedState->readVector3d();

	_color         = savedState->readColor();

	setIntensity(    savedState->readFloat());
	// the angles are saved as they are stored: in radians for EMI, in degrees for Grim
	if (g_grim->getGameType() == GType_MONKEY4) {
		setUmbraRadians(savedState->readFloat());
		setPenumbraRadians(savedState->readFloat());
	} else {
		setUmbraDegrees(savedState->readFloat());
		setPenumbraDegrees(savedState->readFloat());
	}

	if (savedState->saveMinorVersion() >= 20) {
		_falloffNear = savedState->readFloat();
		_falloffFar = savedState->readFloat();
	}

	return true;
}

SetShadow::SetShadow() : _numSectors(0) {
}

void SetShadow::loadBinary(Common::SeekableReadStream *data, Set *set) {
	uint32 nameLen = data->readUint32LE();
	char *name = new char[nameLen];
	data->read(name, nameLen);
	_name = Common::String(name);

	int lightNameLen = data->readSint32LE();
	char *lightName = new char[lightNameLen];
	data->read(lightName, lightNameLen);

	_shadowPoint.readFromStream(data);

	if (lightNameLen > 0) {
		for (Common::List<Light *>::const_iterator it = set->getLights(false).begin(); it != set->getLights(false).end(); ++it) {
			if ((*it)->_name.equals(lightName)) {
				_shadowPoint = (*it)->_pos;
				break;
			}
		}
	}

	int numSectors = data->readSint32LE();
	for (int i = 0; i < numSectors; ++i) {
		uint32 sectorNameLen = data->readUint32LE();
		char *sectorName = new char[sectorNameLen];
		data->read(sectorName, sectorNameLen);
		_sectorNames.push_back(sectorName);
		delete[] sectorName;
	}

	data->skip(4); // Unknown
	_color._vals[0] = (byte)data->readSint32LE();
	_color._vals[1] = (byte)data->readSint32LE();
	_color._vals[2] = (byte)data->readSint32LE();
	delete[] lightName;
	delete[] name;
}

void SetShadow::saveState(SaveGame *savedState) const {
	savedState->writeString(_name);
	savedState->writeVector3d(_shadowPoint);
	savedState->writeLESint32(_numSectors);
	savedState->writeLEUint32(_sectorNames.size());
	for (Common::List<Common::String>::const_iterator it = _sectorNames.begin(); it != _sectorNames.end(); ++it) {
		savedState->writeString(*it);
	}
	savedState->writeColor(_color);
}

void SetShadow::restoreState(SaveGame *savedState) {
	_name = savedState->readString();
	_shadowPoint = savedState->readVector3d();
	_numSectors = savedState->readLESint32();
	uint numSectors = savedState->readLEUint32();
	for (uint i = 0; i < numSectors; ++i) {
		_sectorNames.push_back(savedState->readString());
	}
	_color = savedState->readColor();
}

void Set::Setup::setupCamera() const {
	// Ignore nclip_ and fclip_ for now in Grim.  This fixes:
	// (a) Nothing was being displayed in the Land of the Living
	// diner because lr.set set nclip to 0.
	// (b) The zbuffers for setups with different nclip or
	// fclip values.  If it turns out that the clipping planes
	// are important at some point, we'll need to modify the
	// zbuffer transformation in bitmap.cpp to take nclip_ and
	// fclip_ into account.
	if (g_grim->getGameType() == GType_GRIM) {
		g_driver->setupCameraFrustum(_fov, 0.01f, 3276.8f);
		g_driver->positionCamera(_pos, _interest, _roll);
	} else {
		g_driver->setupCameraFrustum(_fov, _nclip, _fclip);
		g_driver->positionCamera(_pos, _rot);
	}
}

class Sorter {
public:
	Sorter(const Math::Vector3d &pos) {
		_pos = pos;
	}

	bool operator()(Light *l1, Light *l2) const {
		float d1 = (l1->_pos - _pos).getSquareMagnitude();
		float d2 = (l2->_pos - _pos).getSquareMagnitude();
		if (d1 == d2) {
			return l1->_id < l2->_id;
		}

		return d1 < d2;
	}

	Math::Vector3d _pos;
};

void Set::setupLights(const Math::Vector3d &pos, bool inOverworld) {
	if (g_grim->getGameType() == GType_MONKEY4 && !g_driver->supportsShaders()) {
		// If shaders are not available, we do lighting in software for EMI.
		g_driver->disableLights();
		return;
	}

	if (!_enableLights) {
		g_driver->disableLights();
		return;
	}

	// Sort the ligths from the nearest to the farthest to the pos.
	Sorter sorter(pos);
	Common::List<Light *>* lightsList = inOverworld ? &_overworldLightsList : &_lightsList;
	Common::sort(lightsList->begin(), lightsList->end(), sorter);

	int count = 0;
	for (Light *l : *lightsList) {
		if (l->_enabled) {
			g_driver->setupLight(l, count);
			++count;
		}
	}
}

void Set::turnOffLights() {
	_enableLights = false;
	int count = 0;
	for (int i = 0; i < _numLights; i++) {
		Light *l = &_lights[i];
		if (l->_enabled) {
			g_driver->turnOffLight(count);
			++count;
		}
	}
}

void Set::setSetup(int num) {
	// Looks like num is zero-based so >= should work to find values
	// that are out of the range of valid setups

	// Quite weird, but this is what the original does when the setup id is above
	// the upper bound.
	if (num >= _numSetups)
		num %= _numSetups;

	if (num < 0) {
		error("Failed to change scene setup, value out of range");
		return;
	}
	_currSetup = _setups + num;
	g_grim->flagRefreshShadowMask(true);
	if (g_emiSound) {
		g_emiSound->updateSoundPositions();
	}
}

Bitmap::Ptr Set::loadBackground(const char *fileName) {
	// Some sets have setups without any art that the game never selects, the
	// original only loads the setup in use, so don't warn about the missing files.
	Bitmap::Ptr bg;
	if (SearchMan.hasFile(Common::Path(fileName)))
		bg = Bitmap::create(fileName);
	if (!bg) {
		Debug::warning(Debug::Bitmaps | Debug::Sets,
					   "Unable to load scene bitmap: %s, loading dfltroom instead", fileName);
		if (g_grim->getGameType() == GType_MONKEY4) {
			bg = Bitmap::create("dfltroom.til");
		} else {
			bg = Bitmap::create("dfltroom.bm");
		}
		if (!bg) {
			Debug::error(Debug::Bitmaps | Debug::Sets, "Unable to load dfltroom");
		}
	} else {
		Debug::debug(Debug::Bitmaps | Debug::Sets,
					 "Loaded scene bitmap: %s", fileName);
	}
	return bg;
}

void Set::drawBackground() const {
	if (_currSetup->_bkgndZBm) // Some screens have no zbuffer mask (eg, Alley)
		_currSetup->_bkgndZBm->draw();

	if (!_currSetup->_bkgndBm) {
		// This should fail softly, for some reason jumping to the signpost (sg) will load
		// the scene in such a way that the background isn't immediately available
		warning("Background hasn't loaded yet for setup %s in %s!", _currSetup->_name.c_str(), _name.c_str());
	} else {
		_currSetup->_bkgndBm->draw();
	}
}

void Set::drawBitmaps(ObjectState::Position stage) {
	for (StateList::iterator i = _states.reverse_begin(); i != _states.end(); --i) {
		if ((*i)->getPos() == stage && _currSetup == _setups + (*i)->getSetupID())
			(*i)->draw();
	}
}

void Set::setupCamera() {
	_currSetup->setupCamera();
	_frustum.setup(g_driver->getProjection() * g_driver->getModelView());
}

Sector *Set::findPointSector(const Math::Vector3d &p, Sector::SectorType type, bool exact, const Common::Array<Sector *> *exclude) {
	// (the scripts see the sectors in the order of the original, the last one loaded first, and by its test; the walk of the actors by the order of the ids)
	const int n = exact ? getSearchCount() : _numSectors;
	for (int i = 0; i < n; i++) {
		Sector *sector = exact ? getSearchSector(i) : _sectors[i];
		bool skip = false;
		for (uint e = 0; exclude && e < exclude->size(); ++e) {
			if ((*exclude)[e] == sector)
				skip = true;
		}
		if (sector && !skip && (sector->getType() & type) && sector->isVisible() && sector->isPointInSector(p, exact))
			return sector;
	}
	return nullptr;
}

Sector *Set::findPointSector(const Math::Vector3d &p, Sector::SectorType type, const Sector *exclude) {
	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		if (sector && sector != exclude && (sector->getType() & type) && sector->isVisible() && sector->isPointInSector(p))
			return sector;
	}
	return nullptr;
}

// A sector that the point lies over (or under) seen from above, whose plane is at most maxStep away in height: the next piece of floor
// when the floor goes up or down by a step. The test of findPointSector wants the point on the plane of the sector, which it never is
// at the foot of a stair.
Sector *Set::findStepSector(const Math::Vector3d &p, Sector::SectorType type, const Sector *exclude, float maxStep) {
	Sector *best = nullptr;
	float bestDist = maxStep + 1.f;
	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		float planeY = 0.f;
		if (!sector || sector == exclude || !(sector->getType() & type) || !sector->isVisible() || !sector->containsXZ(p, &planeY))
			continue;
		float d = fabsf(planeY - p.y());
		if (d <= maxStep && d < bestDist) {
			bestDist = d;
			best = sector;
		}
	}
	return best;
}

Common::Pair<bool, int> Set::findSectorSortOrder(const Math::Vector3d &p, Sector::SectorType type) {
	int setup = getSetup();
	float minDist = 0.01f;
	int sortOrder = 0;
	bool found = false;

	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		if (!sector || (sector->getType() & type) == 0 || !sector->isVisible())
			continue;

		Math::Vector3d closestPt = sector->getClosestPoint(p);
		float thisDist = (closestPt - p).getMagnitude();
		if (thisDist < minDist) {
			minDist = thisDist;
			sortOrder = sector->getSortplane(setup);
			found = true;
		}
	}
	return Common::Pair<bool, int>(found, sortOrder);
}

void Set::findClosestSector(const Math::Vector3d &p, Sector **sect, Math::Vector3d *closestPoint, const Sector *exclude) {
	Sector *resultSect = nullptr;
	Math::Vector3d resultPt = p;
	float minDist = 0.0;

	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		if ((sector->getType() & Sector::WalkType) == 0 || !sector->isVisible() || sector == exclude)
			continue;
		Math::Vector3d closestPt = sector->getClosestPoint(p);
		float thisDist = (closestPt - p).getMagnitude();
		if (!resultSect || thisDist < minDist) {
			resultSect = sector;
			resultPt = closestPt;
			minDist = thisDist;
		}
	}

	if (sect)
		*sect = resultSect;

	if (closestPoint)
		*closestPoint = resultPt;
}

bool Set::boxesShrunk() const {
	for (int i = 0; i < _numSectors; i++) {
		if (_sectors[i] && _sectors[i]->isShrunk())
			return true;
	}
	return false;
}

// ShrinkBoxes of the original (FUN_004a9d00): every walk sector that is on moves each of its vertices inward along the bisector of its two edges until it is the
// radius away from both; a sector in which a vertex does not fit has its shortest edge joined (the vertices between its ends become one) and is tried again,
// and one that cannot be made smaller is switched off. The vertices are shared by the sectors of a set (the pool): the shift of a shared vertex is the mean of
// the shifts that its sectors asked for, and it moves in all of them, also in the ones that are not walk sectors.
void Set::shrinkBoxesGrim(float radius) {
	if (radius < 0.f)
		return;
	for (uint i = 0; i < _searchOrder.size(); i++)
		_searchOrder[i]->grimSaveForShrink(radius);

	struct Shift {
		float key[3];
		float sum[3];
		int count;
	};
	Common::Array<Shift> shifts;

	for (uint i = 0; i < _searchOrder.size(); i++) {
		Sector *sector = _searchOrder[i];
		if (!(sector->getType() & Sector::WalkType) || !sector->isActive())
			continue;
		for (;;) {
			float offsets[3 * 64];
			if (sector->getNumVertices() > 64) {
				sector->grimSwitchOff();
				break;
			}
			if (sector->grimShrinkOffsets(radius, offsets)) {
				for (int v = 0; v < sector->getNumVertices(); v++) {
					const Math::Vector3d &vert = sector->getVertices()[v];
					uint e = 0;
					while (e < shifts.size() && !(shifts[e].key[0] == vert.x() && shifts[e].key[1] == vert.y() && shifts[e].key[2] == vert.z()))
						e++;
					if (e == shifts.size()) {
						Shift fresh;
						fresh.key[0] = vert.x();
						fresh.key[1] = vert.y();
						fresh.key[2] = vert.z();
						fresh.sum[0] = fresh.sum[1] = fresh.sum[2] = 0.f;
						fresh.count = 0;
						shifts.push_back(fresh);
					}
					for (int c = 0; c < 3; c++)
						shifts[e].sum[c] = shifts[e].sum[c] + offsets[v * 3 + c];
					shifts[e].count++;
				}
				break;
			}
			if (!sector->grimCollapseShortestEdge(_weldPool)) {
				sector->grimSwitchOff();
				break;
			}
		}
	}

	// the shared vertices move: by the mean of the shifts, in every sector that has them
	for (uint i = 0; i < _searchOrder.size(); i++) {
		Sector *sector = _searchOrder[i];
		for (int v = 0; v < sector->getNumVertices(); v++) {
			const Math::Vector3d vert = sector->getVertices()[v];
			for (uint e = 0; e < shifts.size(); e++) {
				if (shifts[e].key[0] != vert.x() || shifts[e].key[1] != vert.y() || shifts[e].key[2] != vert.z())
					continue;
				const float inv = 1.0f / (float)shifts[e].count;
				Math::Vector3d moved((float)((long double)shifts[e].sum[0] * inv + vert.x()),
				                     (float)((long double)shifts[e].sum[1] * inv + vert.y()),
				                     (float)((long double)shifts[e].sum[2] * inv + vert.z()));
				sector->grimSetVertex(v, moved);
				break;
			}
		}
	}
}

void Set::shrinkBoxes(float radius) {
	if (g_grim->getGameType() == GType_GRIM && !_searchOrder.empty()) {
		unshrinkBoxes();
		shrinkBoxesGrim(radius);
		return;
	}
	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		sector->shrink(radius);
	}
}

void Set::unshrinkBoxes() {
	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		if (sector)
			sector->unshrink();
	}
}

void Set::setLightIntensity(const char *light, float intensity) {
	for (int i = 0; i < _numLights; ++i) {
		Light &l = _lights[i];
		if (l._name == light) {
			l.setIntensity(intensity);
			return;
		}
	}
}

void Set::setLightIntensity(int light, float intensity) {
	Light &l = _lights[light];
	l.setIntensity(intensity);
}

void Set::setLightEnabled(const char *light, bool enabled) {
	for (int i = 0; i < _numLights; ++i) {
		Light &l = _lights[i];
		if (l._name == light) {
			l._enabled = enabled;
			return;
		}
	}
}

void Set::setLightEnabled(int light, bool enabled) {
	Light &l = _lights[light];
	l._enabled = enabled;
}

void Set::setLightPosition(const char *light, const Math::Vector3d &pos) {
	for (int i = 0; i < _numLights; ++i) {
		Light &l = _lights[i];
		if (l._name == light) {
			l._pos = pos;
			return;
		}
	}
}

void Set::setLightPosition(int light, const Math::Vector3d &pos) {
	Light &l = _lights[light];
	l._pos = pos;
}

// The sounds that were put in space (SetSoundPosition) and play on: the original keeps them in a list of its own (newest first), moves their volume and pan
// when they are set and again when the setup changes, and drops the ones that have stopped (FUN_004a4e20, FUN_004a3fe0, FUN_004a4dc0)
namespace {

struct PositionedSound {
	Common::String name;
	Math::Vector3d pos;
	int minVol;
	int maxVol;
	float scale;
};

Common::List<PositionedSound> g_positionedSounds;

void dropFinishedSounds() {
	for (Common::List<PositionedSound>::iterator it = g_positionedSounds.begin(); it != g_positionedSounds.end();) {
		if (g_imuse->getSoundStatus(it->name.c_str()))
			++it;
		else
			it = g_positionedSounds.erase(it);
	}
}

// FUN_004469a0
void normalize3(float *v) {
	typedef long double LD;
	const LD len = sqrtl((LD)v[0] * v[0] + (LD)v[2] * v[2] + (LD)v[1] * v[1]);
	if (len != 0.L) {
		const LD inv = 1.L / len;
		v[0] = (float)(inv * v[0]);
		v[1] = (float)(v[1] * inv);
		v[2] = (float)(v[2] * inv);
	}
}

// The unit vector to the right of a camera, as the original builds it (FUN_0044f490 with the rotation of FUN_0044f210 about the way it looks by the roll)
void cameraRight(const Math::Vector3d &pos, const Math::Vector3d &interest, float rollDegrees, float *right) {
	typedef long double LD;
	float f[3] = { interest.x() - pos.x(), interest.y() - pos.y(), interest.z() - pos.z() };
	normalize3(f);
	float s, c;
	originalSinCos(rollDegrees, s, c);
	float m[12] = { 0.f };
	float t = 1.f - f[2];
	if (fabsf(t) <= 1e-05f)
		t = 0.f;
	if (t == 0.f) {
		m[0] = c;
		m[4] = c;
		m[3] = -s;
		m[1] = s;
		m[8] = 1.f;
	} else {
		t = f[2] - -1.f;
		if (fabsf(t) <= 1e-05f)
			t = 0.f;
		if (t != 0.f) {
			const float xx = f[0] * f[0];
			const float yy = f[1] * f[1];
			const float xxc = xx * c;
			const float zz = (1.f - xx) - yy;
			const float yyc = yy * c;
			const float omc = 1.f - c;
			const float xyo = f[1] * f[0] * omc;
			const float zxo = f[2] * f[0] * omc;
			const float yzo = f[1] * f[2] * omc;
			m[0] = (float)(((LD)xxc * zz + yyc) / (1.f - zz) + xx);
			m[4] = (float)(((LD)yyc * zz + xxc) / (1.f - zz) + yy);
			m[8] = yyc + xxc + zz;
			m[1] = f[2] * s + xyo;
			m[3] = xyo - f[2] * s;
			m[2] = zxo - f[1] * s;
			m[5] = yzo + s * f[0];
			m[6] = zxo + f[1] * s;
			m[7] = yzo - s * f[0];
		} else {
			m[0] = c;
			m[4] = c;
			m[1] = -s;
			m[3] = s;
			m[8] = 1.f;
		}
	}
	float up[3] = { 0.f, 0.f, 1.f };
	float az = fabsf(f[2]);
	if (az > 0.999f) {
		if (f[2] <= 0.f) {
			up[1] = 1.f;
			up[2] = 0.f;
		} else {
			up[0] = -0.f;
			up[1] = -1.f;
			up[2] = -0.f;
		}
	}
	float u[3];
	u[0] = (float)((LD)up[0] * m[0] + (LD)up[1] * m[3] + (LD)up[2] * m[6]);
	u[1] = (float)((LD)m[1] * up[0] + (LD)m[4] * up[1] + (LD)up[2] * m[7]);
	u[2] = (float)((LD)m[2] * up[0] + (LD)m[8] * up[2] + (LD)up[1] * m[5]);
	right[0] = (float)((LD)f[1] * u[2] - (LD)f[2] * u[1]);
	right[1] = (float)((LD)f[2] * u[0] - (LD)f[0] * u[2]);
	right[2] = (float)((LD)f[0] * u[1] - (LD)f[1] * u[0]);
	normalize3(right);
}

} // end of anonymous namespace

void Set::setSoundPosition(const char *soundName, const Math::Vector3d &pos) {
	setSoundPosition(soundName, pos, _minVolume, _maxVolume);
}

void Set::setSoundPosition(const char *soundName, const Math::Vector3d &pos, int minVol, int maxVol, float scale) {
	if (!g_imuse->getSoundStatus(soundName))
		return;
	dropFinishedSounds();
	PositionedSound s;
	s.name = soundName;
	s.pos = pos;
	s.minVol = minVol;
	s.maxVol = maxVol;
	s.scale = scale;
	g_positionedSounds.push_front(s);
	int volume, balance;
	calculateSoundPosition(pos, minVol, maxVol, scale, volume, balance);
	if (volume >= 0)
		g_sound->setVolume(soundName, volume);
	g_sound->setPan(soundName, balance);
}

void Set::updateSoundPositions() {
	dropFinishedSounds();
	for (Common::List<PositionedSound>::const_iterator it = g_positionedSounds.begin(); it != g_positionedSounds.end(); ++it) {
		int volume, balance;
		calculateSoundPosition(it->pos, it->minVol, it->maxVol, it->scale, volume, balance);
		if (volume >= 0)
			g_sound->setVolume(it->name.c_str(), volume);
		g_sound->setPan(it->name.c_str(), balance);
	}
}

// FUN_004a4e80: the distance is the original's rough one (FUN_00446a10, which also takes the vector to the unit length), the volume falls off
// in a straight line from the loudest at 0.8 from the camera over `scale`, the pan is the part of that direction along the right of the camera
void Set::calculateSoundPosition(const Math::Vector3d &pos, int minVol, int maxVol, float scale, int &volume, int &balance) const {
	typedef long double LD;
	volume = -1;
	balance = 64;
	if (!_currSetup)
		return;
	const float nearDist = 0.8f;
	const float panScale = 1.5f;
	float d[3] = { pos.x() - _currSetup->_pos.x(), pos.y() - _currSetup->_pos.y(), pos.z() - _currSetup->_pos.z() };
	float ax = fabsf(d[0]), ay = fabsf(d[1]), az = fabsf(d[2]);
	float mx = ax, md = ay, mn = az;
	if (az <= ay) {
		if (ax < ay) {
			md = ax;
			mx = ay;
			if (ax < az) {
				mn = ax;
				md = az;
			}
		}
	} else if (az <= ax) {
		mn = ay;
		md = az;
	} else {
		mn = ax;
		mx = az;
		if (ay < ax) {
			mn = ay;
			md = ax;
		}
	}
	const float len = (float)((LD)mn * 0.25 + (LD)md * 0.34375 + mx);
	const float inv = 1.f / len;
	for (int i = 0; i < 3; i++)
		d[i] = (float)((LD)d[i] * inv);
	const LD t = (LD)len - nearDist;
	if (t >= 0.L) {
		const LD range = (LD)(uint32)(maxVol - minVol);
		const LD v = ((LD)scale - t) * (range / scale) + minVol;
		uint32 vol = (uint32)(int64)v;
		if (vol < (uint32)minVol)
			vol = minVol;
		else if (vol > (uint32)maxVol)
			vol = maxVol;
		volume = vol;
	}
	float right[3];
	cameraRight(_currSetup->_pos, _currSetup->_interest, _currSetup->_roll, right);
	LD x = ((LD)right[0] * d[0] + (LD)right[1] * d[1] + (LD)right[2] * d[2]) * panScale;
	if (x < -1.L)
		x = -1.L;
	else if (!(x <= 1.L))
		x = 1.L;
	uint32 pan = (uint32)(int64)((x - -1.L) * 64.L);
	balance = pan > 0x7f ? 0x7f : pan;
}

Sector *Set::getSectorBase(int id) {
	if ((_numSectors >= 0) && (id < _numSectors))
		return _sectors[id];
	else
		return nullptr;
}

Sector *Set::getSectorByName(const Common::String &name) {
	// (the original searches the list of its sectors, the last one loaded first: FUN_004a7230; two sectors of a set can have the same name)
	if (g_grim->getGameType() == GType_GRIM && !_searchOrder.empty()) {
		for (uint i = 0; i < _searchOrder.size(); i++) {
			if (_searchOrder[i]->getName() == name)
				return _searchOrder[i];
		}
		return nullptr;
	}
	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		if (sector->getName() == name) {
			return sector;
		}
	}
	return nullptr;
}

Sector *Set::getSectorBySubstring(const Common::String &str) {
	for (int i = 0; i < _numSectors; i++) {
		Sector *sector = _sectors[i];
		if (strstr(sector->getName().c_str(), str.c_str())) {
			return sector;
		}
	}
	return nullptr;
}

Sector *Set::getSectorBySubstring(const Common::String &str, const Math::Vector3d &pos, bool exact) {
	const int n = exact ? getSearchCount() : _numSectors;
	for (int i = 0; i < n; i++) {
		Sector *sector = exact ? getSearchSector(i) : _sectors[i];
		// like the original (FUN_004a6980 with flag 1) a sector that is switched off never contains a point
		if (strstr(sector->getName().c_str(), str.c_str()) && sector->isVisible() && sector->isPointInSector(pos, exact)) {
			return sector;
		}
	}
	return nullptr;
}

void Set::setSoundParameters(int minVolume, int maxVolume) {
	_minVolume = minVolume;
	_maxVolume = maxVolume;
}

void Set::getSoundParameters(int *minVolume, int *maxVolume) {
	*minVolume = _minVolume;
	*maxVolume = _maxVolume;
}

void Set::addObjectState(const ObjectState::Ptr &s) {
	_states.push_front(s);
}

ObjectState *Set::addObjectState(int setupID, ObjectState::Position pos, const char *bitmap, const char *zbitmap, bool transparency) {
	ObjectState *state = findState(bitmap);

	if (state) {
		return state;
	}

	state = new ObjectState(setupID, pos, bitmap, zbitmap, transparency);
	addObjectState(state);

	return state;
}

ObjectState *Set::findState(const Common::String &filename) {
	// Check the different state objects for the bitmap
	for (StateList::iterator i = _states.begin(); i != _states.end(); ++i) {
		const Common::String &file = (*i)->getBitmapFilename();

		if (file == filename)
			return *i;
		if (file.compareToIgnoreCase(filename) == 0) {
			Debug::warning(Debug::Sets, "State object request '%s' matches object '%s' but is the wrong case", filename.c_str(), file.c_str());
			return *i;
		}
	}
	return nullptr;
}

void Set::moveObjectStateToFront(const ObjectState::Ptr &s) {
	_states.remove(s);
	_states.push_front(s);
	// Make the state invisible. This hides the deadbolt when brennis closes the switcher door
	// in the server room (tu), and therefore fixes https://github.com/residualvm/residualvm/issues/24
	s->setActiveImage(0);
}

void Set::moveObjectStateToBack(const ObjectState::Ptr &s) {
	_states.remove(s);
	_states.push_back(s);
}

SetShadow *Set::getShadow(int i) {
	return &_shadows[i];
}

SetShadow *Set::getShadowByName(const Common::String &name) {
	for (int i = 0; i < _numShadows; ++i) {
		SetShadow *shadow = &_shadows[i];
		if (shadow->_name.equalsIgnoreCase(name))
			return shadow;
	}
	return nullptr;
}

} // end of namespace Grim
