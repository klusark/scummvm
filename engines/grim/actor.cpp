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

#include "common/config-manager.h"

#include "math/line3d.h"
#include "math/rect2d.h"

#include "engines/grim/debug.h"
#include "engines/grim/actor.h"
#include "engines/grim/grim.h"
#include "engines/grim/costume.h"
#include "engines/grim/lipsync.h"
#include "engines/grim/movie/movie.h"
#include "engines/grim/sound.h"
#include "engines/grim/lua.h"
#include "engines/grim/resource.h"
#include "engines/grim/savegame.h"
#include "engines/grim/set.h"
#include "engines/grim/sincos.h"
#include "engines/grim/gfx_base.h"
#include "engines/grim/model.h"
#include "engines/grim/emi/emi.h"
#include "engines/grim/emi/costumeemi.h"
#include "engines/grim/emi/skeleton.h"
#include "engines/grim/emi/costume/emiskel_component.h"
#include "engines/grim/emi/modelemi.h"

namespace Grim {

Shadow::Shadow() :
		active(false), dontNegate(false), valid(0), planeDist(0.f), userData(nullptr) {
	planeNormal[0] = planeNormal[1] = planeNormal[2] = 0.f;
}

static int animTurn(float turnAmt, const Math::Angle &dest, Math::Angle *cur) {
	Math::Angle d = dest - *cur;
	d.normalize(-180);
	// If the actor won't turn because the rate is set to zero then
	// have the actor turn all the way to the destination yaw.
	// Without this some actors will lock the interface on changing
	// scenes, this affects the Bone Wagon in particular.
	if (turnAmt == 0 || turnAmt >= fabsf(d.getDegrees())) {
		*cur = dest;
	} else if (d > 0) {
		*cur += turnAmt;
	} else {
		*cur -= turnAmt;
	}
	if (d != 0) {
		return (d > 0 ? 1 : -1);
	}
	return 0;
}

// The original Grim engine turns by the turn rate plus a tenth of the angle
// that is left, so the turn eases out, and it stops once the actor is within
// half a degree of the target.
static int animTurnEased(float turnAmt, const Math::Angle &dest, Math::Angle *cur) {
	Math::Angle d = dest - *cur;
	d.normalize(-180);
	float remaining = fabsf(d.getDegrees());
	if (remaining < 0.5f)
		return 0;

	float step = remaining;
	if (turnAmt > 0.f && turnAmt < remaining)
		step = (remaining - turnAmt) * 0.1f + turnAmt;

	if (d > 0) {
		*cur += step;
		return 1;
	}
	*cur -= step;
	return -1;
}

// Switches for the rules of the original that were ported to the walk of the actors, to find which of them a script cannot take yet (development):
// exact_edges and turn_key_rule in the configuration, on unless set to false
static bool grimRuleOn(const char *key) {
	return !ConfMan.hasKey(key) || ConfMan.getBool(key);
}

bool Actor::_isTalkingBackground = false;
int Actor::s_wsDepth = 0;
const Actor *Actor::s_drawnActor = nullptr;

void Actor::saveStaticState(SaveGame *state) {
	state->writeBool(_isTalkingBackground);
}

void Actor::restoreStaticState(SaveGame *state) {
	_isTalkingBackground = state->readBool();
}

Actor::Actor() :
		_talkColor(255, 255, 255), _pos(0, 0, 0),
		_lookingMode(false), _followBoxes(false), _running(false),
		_pitch(0), _yaw(0), _roll(0), _walkRate(0.3f),
		_talkAnim(0),
		_reflectionAngle(80), _constrained(false), _wsDist(0.f), _turnKey(0), _scale(1.f), _timeScale(1.f),
		_visible(true), _lipSync(nullptr), _turning(false), _rotPending(false), _hPlane(false), _hAxisA(0), _hAxisB(1), _hSector(nullptr), _headingSet(false), _yawSnap(false), _singleTurning(false), _walking(false),
		_walkedLast(false), _walkedCur(false),
		_collisionMode(CollisionOff), _collisionScale(1.f),
		_lastTurnDir(0), _currTurnDir(0),
		_sayLineText(0), _talkDelay(0),
		_attachedActor(0), _attachedJoint(""),
		_globalAlpha(1.f), _alphaMode(AlphaOff),
		 _mustPlaceText(false),
		_puckOrient(false), _puck(true), _fovCos(0.5f), _talking(false),
		_inOverworld(false), _drawnToClean(false), _backgroundTalk(false),
		_sortOrder(0), _useParentSortOrder(false),
		_sectorSortOrder(0), _hasSectorSortOrder(false), _fakeUnbound(false), _lightMode(LightFastDyn),
		_hasFollowedBoxes(false), _lookAtActor(0) {

	// Some actors don't set walk and turn rates, so they use the defaults
	// of the original: Doug at the cat races, Glottis at the demon beaver
	// entrance and Chepito in su.set
	if (g_grim->getGameType() == GType_GRIM) {
		_walkRate = 0.4f;
		_turnRate = 135.0f;
	} else {
		_turnRate = 100.0f;
	}

	_clipActive = 0;
	_invClipNode = -1;
	_frustumCull = true;
	_hRight[0] = 1.f; _hRight[1] = 0.f; _hRight[2] = 0.f;
	_hFwd[0] = 0.f; _hFwd[1] = 1.f; _hFwd[2] = 0.f;
	_hUp[0] = 0.f; _hUp[1] = 0.f; _hUp[2] = 1.f;
	_hPlaneN[0] = _hPlaneN[1] = _hPlaneN[2] = 0.f;
	_hPlaneV0[0] = _hPlaneV0[1] = _hPlaneV0[2] = 0.f;

	_activeShadowSlot = -1;
	_shadowArray = new Shadow[MAX_SHADOWS];
}

Actor::~Actor() {
	if (_shadowArray) {
		clearShadowPlanes();
		delete[] _shadowArray;
	}
	while (!_costumeStack.empty()) {
		delete _costumeStack.back();
		_costumeStack.pop_back();
	}
	g_grim->immediatelyRemoveActor(this);
}

void Actor::saveState(SaveGame *savedState) const {
	// store actor name
	savedState->writeString(_name);
	savedState->writeString(_setName);

	savedState->writeColor(_talkColor);

	savedState->writeVector3d(_pos);

	savedState->writeFloat(_pitch.getDegrees());
	savedState->writeFloat(_yaw.getDegrees());
	savedState->writeFloat(_roll.getDegrees());
	savedState->writeFloat(_walkRate);
	savedState->writeFloat(_turnRate);
	savedState->writeBool(_followBoxes);
	savedState->writeFloat(_reflectionAngle);
	savedState->writeBool(_visible);
	savedState->writeBool(_lookingMode);
	savedState->writeFloat(_scale);
	savedState->writeFloat(_timeScale);
	savedState->writeBool(_puckOrient);

	savedState->writeString(_talkSoundName);
	savedState->writeBool(_talking);
	savedState->writeBool(_backgroundTalk);

	savedState->writeLEUint32((uint32)_collisionMode);
	savedState->writeFloat(_collisionScale);

	if (_lipSync) {
		savedState->writeBool(true);
		savedState->writeString(_lipSync->getFilename());
	} else {
		savedState->writeBool(false);
	}

	savedState->writeLEUint32(_costumeStack.size());
	for (Common::List<Costume *>::const_iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
		Costume *c = *i;
		savedState->writeString(c->getFilename());
		Costume *pc = c->getPreviousCostume();
		int depth = 0;
		while (pc) {
			++depth;
			pc = pc->getPreviousCostume();
		}
		savedState->writeLESint32(depth);
		pc = c->getPreviousCostume();
		for (int j = 0; j < depth; ++j) { //save the previousCostume hierarchy
			savedState->writeString(pc->getFilename());
			pc = pc->getPreviousCostume();
		}
		c->saveState(savedState);
	}

	savedState->writeBool(_turning);
	savedState->writeBool(_singleTurning);
	savedState->writeFloat(_moveYaw.getDegrees());
	savedState->writeFloat(_movePitch.getDegrees());
	savedState->writeFloat(_moveRoll.getDegrees());

	savedState->writeBool(_walking);
	savedState->writeVector3d(_destPos);

	_restChore.saveState(savedState);

	_walkChore.saveState(savedState);
	savedState->writeBool(_walkedLast);
	savedState->writeBool(_walkedCur);

	_leftTurnChore.saveState(savedState);
	_rightTurnChore.saveState(savedState);
	savedState->writeLESint32(_lastTurnDir);
	savedState->writeLESint32(_currTurnDir);

	for (int i = 0; i < 10; ++i) {
		_talkChore[i].saveState(savedState);
	}
	savedState->writeLESint32(_talkAnim);

	_mumbleChore.saveState(savedState);

	for (int i = 0; i < MAX_SHADOWS; ++i) {
		Shadow &shadow = _shadowArray[i];
		savedState->writeString(shadow.name);

		savedState->writeVector3d(shadow.pos);

		savedState->writeLEUint32(shadow.planeList.size());
		// Cannot use g_grim->getCurrSet() here because an actor can have walk planes
		// from other scenes. It happens e.g. when Membrillo calls Velasco to tell him
		// Naranja is dead.
		for (SectorListType::iterator j = shadow.planeList.begin(); j != shadow.planeList.end(); ++j) {
			Plane &p = *j;
			savedState->writeString(p.setName);
			savedState->writeString(p.sector->getName());
		}

		savedState->writeLESint32(0);
		savedState->writeBool(shadow.active);
		savedState->writeBool(shadow.dontNegate);
	}
	savedState->writeLESint32(_activeShadowSlot);

	savedState->writeLESint32(_sayLineText);

	savedState->writeVector3d(_lookAtVector);

	savedState->writeLEUint32(_path.size());
	for (Common::List<Math::Vector3d>::const_iterator i = _path.begin(); i != _path.end(); ++i) {
		savedState->writeVector3d(*i);
	}

	if (g_grim->getGameType() == GType_MONKEY4) {
		savedState->writeLEUint32(_alphaMode);
		savedState->writeFloat(_globalAlpha);

		savedState->writeBool(_inOverworld);
		savedState->writeLESint32(_sortOrder);
		savedState->writeBool(_useParentSortOrder);

		savedState->writeLESint32(_attachedActor);
		savedState->writeString(_attachedJoint);

		Common::List<MaterialPtr>::const_iterator it = _materials.begin();
		for (; it != _materials.end(); ++it) {
			if (*it) {
				warning("%s", (*it)->getFilename().c_str());
				savedState->writeLESint32((*it)->getActiveTexture());
			}
		}

		savedState->writeLESint32(_lookAtActor);

		savedState->writeLEUint32(_localAlpha.size());
		for (unsigned int i = 0; i < _localAlpha.size(); i++) {
			savedState->writeFloat(_localAlpha[i]);
		}
		savedState->writeLEUint32(_localAlphaMode.size());
		for (unsigned int i = 0; i < _localAlphaMode.size(); i++) {
			savedState->writeLESint32(_localAlphaMode[i]);
		}
	}

	savedState->writeBool(_drawnToClean);
	savedState->writeBool(_fakeUnbound);
	savedState->writeBool(_constrained);
	savedState->writeVector3d(_constrainDir);

}

bool Actor::restoreState(SaveGame *savedState) {
	for (Common::List<Costume *>::const_iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
		delete *i;
	}
	_costumeStack.clear();
	_materials.clear();

	// load actor name
	_name = savedState->readString();
	_setName = savedState->readString();

	_talkColor = savedState->readColor();

	_pos                = savedState->readVector3d();
	_pitch              = savedState->readFloat();
	_yaw                = savedState->readFloat();
	_roll               = savedState->readFloat();
	_walkRate           = savedState->readFloat();
	_turnRate           = savedState->readFloat();
	if (savedState->saveMinorVersion() > 6 && savedState->saveMinorVersion() < 30) {
		// Removed turn rate multiplier
		savedState->readFloat();
	}
	_followBoxes        = savedState->readBool();
	_reflectionAngle    = savedState->readFloat();
	_visible            = savedState->readBool();
	_lookingMode        = savedState->readBool();
	_scale              = savedState->readFloat();
	_timeScale          = savedState->readFloat();
	_puckOrient         = savedState->readBool();

	_talkSoundName      = savedState->readString();
	_talking = savedState->readBool();
	_backgroundTalk = savedState->readBool();
	if (isTalking()) {
		g_grim->addTalkingActor(this);
	}

	_collisionMode      = (CollisionMode)savedState->readLEUint32();
	_collisionScale     = savedState->readFloat();

	if (savedState->readBool()) {
		Common::String fn = savedState->readString();
		_lipSync = g_resourceloader->getLipSync(fn);
	} else {
		_lipSync = nullptr;
	}

	uint32 size = savedState->readLEUint32();
	for (uint32 i = 0; i < size; ++i) {
		Common::String fname = savedState->readString();
		const int depth = savedState->readLESint32();
		Costume *pc = nullptr;
		if (depth > 0) {    //build all the previousCostume hierarchy
			Common::String *names = new Common::String[depth];
			for (int j = 0; j < depth; ++j) {
				names[j] = savedState->readString();
			}
			for (int j = depth - 1; j >= 0; --j) {
				pc = findCostume(names[j]);
				if (!pc) {
					pc = g_resourceloader->loadCostume(names[j], this, pc);
				}
			}
			delete[] names;
		}

		Costume *c = g_resourceloader->loadCostume(fname, this, pc);
		c->restoreState(savedState);
		_costumeStack.push_back(c);
	}

	_turning = savedState->readBool();
	if (savedState->saveMinorVersion() > 25) {
		_singleTurning = savedState->readBool();
	}
	_moveYaw = savedState->readFloat();
	if (g_grim->getGameType() == GType_GRIM) {
		// (the frame is made again at its first use, from the angle that was saved, as the original makes the walker of a loaded actor)
		_headingSet = false;
	}
	if (savedState->saveMinorVersion() > 6) {
		_movePitch = savedState->readFloat();
		_moveRoll = savedState->readFloat();
	} else {
		_movePitch = _pitch;
		_moveRoll = _roll;
	}

	_walking = savedState->readBool();
	_destPos = savedState->readVector3d();

	_restChore.restoreState(savedState, this);

	_walkChore.restoreState(savedState, this);
	_walkedLast = savedState->readBool();
	_walkedCur = savedState->readBool();

	_leftTurnChore.restoreState(savedState, this);
	_rightTurnChore.restoreState(savedState, this);
	_lastTurnDir = savedState->readLESint32();
	_currTurnDir = savedState->readLESint32();

	for (int i = 0; i < 10; ++i) {
		_talkChore[i].restoreState(savedState, this);
	}
	_talkAnim = savedState->readLESint32();

	_mumbleChore.restoreState(savedState, this);

	clearShadowPlanes();

	int maxShadows = MAX_SHADOWS;
	if (savedState->saveMinorVersion() < 24)
		maxShadows = 5;
	for (int i = 0; i < maxShadows; ++i) {
		Shadow &shadow = _shadowArray[i];
		shadow.name = savedState->readString();

		shadow.pos = savedState->readVector3d();

		size = savedState->readLEUint32();
		Set *scene = nullptr;
		for (uint32 j = 0; j < size; ++j) {
			Common::String actorSetName = savedState->readString();
			Common::String secName = savedState->readString();
			if (!scene || scene->getName() != actorSetName) {
				scene = g_grim->findSet(actorSetName);
			}
			if (scene) {
				addShadowPlane(secName.c_str(), scene, i);
			} else {
				warning("%s: No scene \"%s\" found, cannot restore shadow on sector \"%s\"", getName().c_str(), actorSetName.c_str(), secName.c_str());
			}
		}

		int shadowMaskSize = savedState->readLESint32();
		for (int s = 0; s < shadowMaskSize; s++) {
			savedState->readByte();
		}
		shadow.active = savedState->readBool();
		shadow.dontNegate = savedState->readBool();
	}
	_activeShadowSlot = savedState->readLESint32();

	_sayLineText = savedState->readLESint32();

	_lookAtVector = savedState->readVector3d();

	size = savedState->readLEUint32();
	_path.clear();
	for (uint32 i = 0; i < size; ++i) {
		_path.push_back(savedState->readVector3d());
	}

	if (g_grim->getGameType() == GType_MONKEY4) {
		_alphaMode = (AlphaMode) savedState->readLEUint32();
		_globalAlpha = savedState->readFloat();

		_inOverworld  = savedState->readBool();
		_sortOrder    = savedState->readLESint32();
		if (savedState->saveMinorVersion() >= 22)
			_useParentSortOrder = savedState->readBool();

		if (savedState->saveMinorVersion() < 18)
			savedState->readBool(); // Used to be _shadowActive.

		_attachedActor = savedState->readLESint32();
		_attachedJoint = savedState->readString();

		// will be recalculated in next update()
		_sectorSortOrder = 0;
		_hasSectorSortOrder = false;

		if (savedState->saveMinorVersion() < 24) {
			// Used to be the wear chore.
			if (savedState->readBool()) {
				savedState->readString();
			}
			savedState->readLESint32();
		}

		if (savedState->saveMinorVersion() >= 13) {
			Common::List<MaterialPtr>::const_iterator it = _materials.begin();
			for (; it != _materials.end(); ++it) {
				if (*it) {
					(*it)->setActiveTexture(savedState->readLESint32());
				}
			}
		}

		if (savedState->saveMinorVersion() >= 17)
			_lookAtActor = savedState->readLESint32();

		if (savedState->saveMinorVersion() >= 25) {
			_localAlpha.resize(savedState->readLEUint32());
			for (unsigned int i = 0; i < _localAlpha.size(); i++) {
				_localAlpha[i] = savedState->readFloat();
			}

			_localAlphaMode.resize(savedState->readLEUint32());
			for (unsigned int i = 0; i < _localAlphaMode.size(); i++) {
				_localAlphaMode[i] = savedState->readLESint32();
			}
		}
	}

	if (savedState->saveMinorVersion() >= 4) {
		_drawnToClean = savedState->readBool();
	} else {
		_drawnToClean = false;
	}
	if (savedState->saveMinorVersion() >= 27) {
		_fakeUnbound = savedState->readBool();
	} else {
		// Note: actor will stay invisible until re-bound to set
		// by game scripts.
		_fakeUnbound = false;
	}
	if (savedState->saveMinorVersion() >= 34) {
		_constrained = savedState->readBool();
		_constrainDir = savedState->readVector3d();
	} else {
		_constrained = false;
	}

	return true;
}

void Actor::setRot(const Math::Vector3d &pos) {
	Math::Angle y, p, r;
	calculateOrientation(pos, &p, &y, &r);
	setRot(p, y, r);
}


// (the arithmetic of the original: every expression in extended precision, stored as a float where the original stores one)
typedef long double LD;

static inline float toF(LD v) {
	return (float)v;
}

// Timer_GetFrameSeconds (FUN_004a3c00): the time of the frame in seconds as the timer of the original keeps it, a float made from the milliseconds (at most 300) times
// 0.001f (FUN_004a3c80)
static inline LD origFrameSeconds() {
	unsigned ms = g_grim->getFrameTime();
	if (ms > 300)
		ms = 300;
	return (LD)(float)((LD)ms * (LD)0.001f);
}

static LD origNormalize3(float *v);
static void headingGround(const float *f, float &x, float &y);
static void origActorMatrix(float *m, float e0, float e1, float e2, const Math::Vector3d &pos);
static void origToLocal(float *o, const float *v, const float *m);
static void origTurn(float *o, const float *v, const float *m, bool withPos);
static void origAxisMatrix(float *m, const float *axis, float degrees);

void Actor::setRot(const Math::Angle &pitchParam, const Math::Angle &yawParam, const Math::Angle &rollParam) {
	_pitch = pitchParam;
	if (g_grim->getGameType() == GType_GRIM && !_headingSet && !_followBoxes) {
		// An actor that has never walked, turned or followed boxes has no walker in the original (FUN_00490730 makes it on the first
		// of those), and SetActorRot sets its angles as they are asked (the tumblers of the safe in vd read them back and compare them
		// as whole numbers)
		_yaw = yawParam;
		_moveYaw = _yaw;
	} else if (g_grim->getGameType() == GType_GRIM) {
		// (Actor_SetRot: the angle that is still to turn, IsActorTurning, is the one the frame was turned by)
		_turnLeft = faceHeading(yawParam, pitchParam, rollParam);
		// (the heading that is seen follows the frame of the walker in the next frame, not at once: GetActorRot reads the old angle until then)
		_yawSnap = true;
	} else {
		_yaw = yawParam;
		_moveYaw = _yaw;
	}
	_roll = rollParam;
	_turning = false;
}

void Actor::setPos(const Math::Vector3d &position) {
	_walking = false;
	_pos = position;

	// Don't allow positions outside the sectors.
	// This is necessary after solving the tree pump puzzle, when the bone
	// wagon returns to the signopost set.
	if (g_grim->getGameType() == GType_GRIM) {
		// Actor_SetPositionClamped (FUN_00490870): an actor that has a walker takes the sector for the place (FUN_004a82c0: moved onto it only when it is not
		// in it by the test of the sector), one that has none is put where it is asked
		if (_followBoxes && _headingSet)
			updateHeadingSector();
	} else if (_followBoxes) {
		g_grim->getCurrSet()->findClosestSector(_pos, nullptr, &_pos);
	}

	if (g_grim->getGameType() == GType_MONKEY4) {
		Math::Vector3d moveVec = position - _pos;
		for (Actor *a : g_grim->getActiveActors()) {
			handleCollisionWith(a, _collisionMode, &moveVec);
		}
	}
}

void Actor::calculateOrientation(const Math::Vector3d &pos, Math::Angle *pitch, Math::Angle *yaw, Math::Angle *roll) {
	Math::Vector3d actorForward(0.f, 1.f, 0.f);
	Math::Vector3d actorUp(0.f, 0.f, 1.f);
	Math::Vector3d lookVector = pos - _pos;
	lookVector.normalize();

	// EMI: Y is up-down, actors use an X-Z plane for movement
	if (g_grim->getGameType() == GType_MONKEY4) {
		float temp = lookVector.z();
		lookVector.x() = -lookVector.x();
		lookVector.z() = lookVector.y();
		lookVector.y() = temp;
	}

	Math::Vector3d up = actorUp;
	if (_puckOrient) {
		Sector *s = nullptr;
		g_grim->getCurrSet()->findClosestSector(_pos, &s, nullptr);
		if (s) {
			up = s->getNormal();
		}
	}

	Math::Matrix3 m;
	m.buildFromTargetDir(actorForward, lookVector, actorUp, up);

	if (_puckOrient) {
		m.getEuler(yaw, pitch, roll, Math::EO_ZXY);
	} else {
		*pitch = _movePitch;
		m.getEuler(yaw, nullptr, nullptr, Math::EO_ZXY);
		*roll = _moveRoll;
	}
}

void Actor::turnTo(const Math::Vector3d &pos) {
	Math::Angle y, p, r;
	calculateOrientation(pos, &p, &y, &r);
	turnTo(p, y, r);
}

bool Actor::singleTurnTo(const Math::Vector3d &pos) {
	Math::Angle y, p, r;
	calculateOrientation(pos, &p, &y, &r);

	float turnAmt = g_grim->getPerSecond(_turnRate) * _timeScale;
	bool done = animTurn(turnAmt, y, &_yaw) == 0;
	done = animTurn(turnAmt, p, &_pitch) == 0 && done;
	done = animTurn(turnAmt, r, &_roll) == 0 && done;
	if (g_grim->getGameType() == GType_GRIM)
		faceHeading(_yaw);
	else
		_moveYaw = _yaw;
	_movePitch = _pitch;
	_moveRoll = _roll;
	_singleTurning = !done;

	return done;
}

void Actor::turnTo(const Math::Angle &pitchParam, const Math::Angle &yawParam, const Math::Angle &rollParam) {
	_movePitch = pitchParam;
	_moveRoll = rollParam;
	if (g_grim->getGameType() == GType_GRIM)
		_turnLeft = faceHeading(yawParam, pitchParam, rollParam); // (Actor_SetRot without the negative turn rate: the angle that is still to turn is the one the frame was turned by)
	else
		_moveYaw = yawParam;
	if (_yaw != yawParam || _pitch != pitchParam || _roll != rollParam) {
		_turning = true;
	} else
		_turning = false;
}

void Actor::walkTo(const Math::Vector3d &p) {
	// The original ignores walk requests while an inventory set is current, so
	// scripts that wait for a walk do not block until the inventory is closed.
	const Set *activeSet = g_grim->getCurrSet();
	if (g_grim->getGameType() == GType_GRIM && activeSet && activeSet->getName().hasSuffix("_inv.set")) {
		_walking = false;
		return;
	}

	// EMI: an actor without a puck gets no path (the original's WalkActorTo does nothing for it)
	if (g_grim->getGameType() == GType_MONKEY4 && !_puck)
		_walking = false;
	else if (p == _pos && g_grim->getGameType() != GType_GRIM)
		_walking = false;
	else {
		_walking = true;
		_destPos = p;
		_path.clear();

		if (_followBoxes && g_grim->getGameType() == GType_GRIM && grimRuleOn("path_planner") && g_grim->getCurrSet()->getSearchCount() > 0) {
			// Walker_ReplanPath (FUN_00491650): the destination goes to the nearest place of the walk sectors, the path is the one of the planner of the
			// original (FUN_004a9430): the sectors searched depth first, the corners pulled straight. The first point is where the actor is.
			Set *currSet = g_grim->getCurrSet();
			Math::Vector3d dest = p;
			currSet->grimNearestWalkPoint(dest);
			Common::Array<Math::Vector3d> points;
			if (!currSet->grimPlanPath(_pos, dest, points)) {
				warning("Actor::walkTo(): No path found for %s", _name.c_str());
				_walking = false;
				return;
			}
			_destPos = dest;
			for (int i = (int)points.size() - 1; i >= 1; --i)
				_path.push_back(points[i]);
			if (Debug::isChannelEnabled(Debug::Actors)) {
				Common::String list;
				for (Common::List<Math::Vector3d>::const_iterator i = _path.begin(); i != _path.end(); ++i)
					list += Common::String::format(" (%.9g %.9g %.9g)", i->x(), i->y(), i->z());
				Debug::debug(Debug::Actors, "WALKTO %s from %.9g %.9g %.9g to %.9g %.9g %.9g path (next last):%s", _name.c_str(), _pos.x(), _pos.y(), _pos.z(), p.x(), p.y(), p.z(), list.c_str());
			}
			return;
		}

		if (_followBoxes) {
			bool pathFound = false;

			Set *currSet = g_grim->getCurrSet();

			// The original does not snap the start of a walk to the closest sector,
			// the actor has to be inside a visible walkbox. A script can leave an
			// actor outside of them, for example by enabling a sector while the
			// inventory is open, and relies on the walk being ignored.
			// An actor standing right on the edge of a box, such as Meche in set nq after her own
			// box is switched off, is still inside (a hand's breadth: the keys leave an actor a little off the plane of a thin sector).
			Math::Vector3d startPt;
			currSet->findClosestSector(_pos, nullptr, &startPt);
			if (!currSet->findPointSector(_pos, Sector::WalkType) && (startPt - _pos).getMagnitude() > 0.1f) {
				warning("Actor::walkTo(): %s is not inside a walkbox", _name.c_str());
				_walking = false;
				return;
			}

			currSet->findClosestSector(p, nullptr, &_destPos);

			Common::List<PathNode *> openList;
			Common::List<PathNode *> closedList;

			PathNode *start = new PathNode;
			start->parent = nullptr;
			start->pos = _pos;
			start->dist = 0.f;
			start->cost = 0.f;
			openList.push_back(start);
			currSet->findClosestSector(_pos, &start->sect, nullptr);

			Common::List<Sector *> sectors;
			for (int i = 0; i < currSet->getSectorCount(); ++i) {
				Sector *s = currSet->getSectorBase(i);
				int type = s->getType();
				if ((type == Sector::WalkType || type == Sector::HotType || type == Sector::FunnelType) && s->isVisible()) {
					sectors.push_back(s);
				}
			}

			do {
				PathNode *node = nullptr;
				float cost = -1.f;
				for (Common::List<PathNode *>::iterator j = openList.begin(); j != openList.end(); ++j) {
					PathNode *n = *j;
					float c = n->dist + n->cost;
					if (cost < 0.f || c < cost) {
						cost = c;
						node = n;
					}
				}
				closedList.push_back(node);
				openList.remove(node);
				Sector *sector = node->sect;

				if (sector && sector->isPointInSector(_destPos)) {
					PathNode *n = closedList.back();
					// Don't put the start position in the list, or else
					// the first angle calculated in updateWalk() will be
					// meaningless. The only node without parent is the start
					// one.
					while (n->parent) {
						_path.push_back(n->pos);
						n = n->parent;
					}

					pathFound = true;
					break;
				}

				for (Common::List<Sector *>::iterator i = sectors.begin(); i != sectors.end(); ++i) {
					Sector *s = *i;
					bool inClosed = false;
					for (Common::List<PathNode *>::iterator j = closedList.begin(); j != closedList.end(); ++j) {
						if ((*j)->sect == s) {
							inClosed = true;
							break;
						}
					}
					if (inClosed)
						continue;

					// Get "bridges" from the current sector to the other.
					Common::List<Math::Line3d> bridges = sector->getBridgesTo(s);
					if (bridges.empty())
						continue; // The sectors are not adjacent.

					Math::Vector3d closestPoint;
					if (g_grim->getGameType() == GType_GRIM)
						closestPoint = s->getClosestPoint(_destPos);
					else
						closestPoint = _destPos;
					Math::Vector3d best;
					float bestDist = 1e6f;
					Math::Line3d l(node->pos, closestPoint);

					// Pick a point on the boundary of the two sectors to walk towards.
					while (!bridges.empty()) {
						Math::Line3d bridge = bridges.back();
						Math::Vector3d pos;
						const bool useXZ = (g_grim->getGameType() == GType_MONKEY4);

						// Prefer points on the straight line from this node towards
						// the destination. Otherwise pick the middle point of a bridge
						// that is closest to the destination.
						if (!bridge.intersectLine2d(l, &pos, useXZ)) {
							pos = bridge.middle();
						} else {
							best = pos;
							break;
						}
						float dist = (pos - closestPoint).getMagnitude();
						if (dist < bestDist) {
							bestDist = dist;
							best = pos;
						}
						bridges.pop_back();
					}
					best = handleCollisionTo(node->pos, best);

					PathNode *n = nullptr;
					for (Common::List<PathNode *>::iterator j = openList.begin(); j != openList.end(); ++j) {
						if ((*j)->sect == s) {
							n = *j;
							break;
						}
					}
					if (n) {
						float newCost = node->cost + (best - node->pos).getMagnitude();
						if (newCost < n->cost) {
							n->cost = newCost;
							n->parent = node;
							n->pos = best;
							n->dist = (n->pos - _destPos).getMagnitude();
						}
					} else {
						n = new PathNode;
						n->parent = node;
						n->sect = s;
						n->pos = best;
						n->dist = (n->pos - _destPos).getMagnitude();
						n->cost = node->cost + (n->pos - node->pos).getMagnitude();
						openList.push_back(n);
					}
				}
			} while (!openList.empty());

			for (Common::List<PathNode *>::iterator j = closedList.begin(); j != closedList.end(); ++j) {
				delete *j;
			}
			for (Common::List<PathNode *>::iterator j = openList.begin(); j != openList.end(); ++j) {
				delete *j;
			}

			if (!pathFound) {
				// The original does not walk when the destination is unreachable,
				// scripts rely on that when a sector has been disabled.
				warning("Actor::walkTo(): No path found for %s", _name.c_str());
				_walking = false;
				return;
			}
		}

		_path.push_front(_destPos);
		if (g_grim->getGameType() == GType_GRIM && Debug::isChannelEnabled(Debug::Actors)) {
			// the points of the path, the next one last (the original keeps them from the start to the end of the walk)
			Common::String points;
			for (Common::List<Math::Vector3d>::const_iterator i = _path.begin(); i != _path.end(); ++i)
				points += Common::String::format(" (%.9g %.9g %.9g)", i->x(), i->y(), i->z());
			Debug::debug(Debug::Actors, "WALKTO %s from %.9g %.9g %.9g to %.9g %.9g %.9g path (next last):%s", _name.c_str(), _pos.x(), _pos.y(), _pos.z(), p.x(), p.y(), p.z(), points.c_str());
		}
	}
}

bool Actor::isWalking() const {
	return _walkedLast || _walkedCur || _walking;
}

bool Actor::isTurning() const {
	// IsActorTurning of the original (FUN_00499be0): the angle that is still to turn, which Walker_UpdateYaw makes zero below half a degree
	if (g_grim->getGameType() == GType_GRIM)
		return _turnLeft != 0.f;
	if (g_grim->getGameType() == GType_MONKEY4)
		if (_singleTurning)
			return true;
	if (_turning)
		return true;

	if (_rotPending && g_grim->getGameType() == GType_GRIM)
		return true;

	if (_lastTurnDir != 0 || _currTurnDir != 0)
		return true;

	return false;
}

void Actor::stopTurning() {
	_turning = false;
	if (_lastTurnDir != 0)
		getTurnChore(_lastTurnDir)->stop(true);

	_lastTurnDir = 0;
	_currTurnDir = 0;
}

void Actor::moveTo(const Math::Vector3d &pos) {
	// The walking actor doesn't always have the collision mode set, but it must however check
	// the collisions. E.g. the set hl doesn't set Manny's mode, but it must check for
	// collisions with Raoul.
	// On the other hand, it must not *set* the sphere mode, it must just use it for this call:
	// the set xb sets Manny's collision scale as 1 and mode as Off. If you then go to set tb
	// and talk to Doug at the photo window, Manny's mode *must be* Off, otherwise Doug will
	// collide, miss the target point and will walk away, leaving Manny waiting for him at
	// the window forever.
	CollisionMode mode = _collisionMode;
	if (_collisionMode == CollisionOff) {
		mode = CollisionSphere;
	}

	Math::Vector3d moveVec = pos - _pos;
	const Math::Vector3d askedVec = moveVec;
	// (not ported yet: an actor that walks a path and is pushed by another one makes the path again around the one that pushed it, from the place it
	// was pushed to: FUN_004939f0 -> Walker_ReplanPath with the actor, FUN_004917e0. Without that the push and the next step of the path repeat for ever
	// at a box that is in the way of the end of the path, as at the signpost, so the steps of a path are not pushed, as they were not before.)
	if (!(g_grim->getGameType() == GType_GRIM && _walking)) {
		for (Actor *a : g_grim->getActiveActors())
			handleCollisionWith(a, mode, &moveVec);
	}
	// (the original sets the place that was asked for; the sum of the position and the difference to it is not the same float)
	if (g_grim->getGameType() == GType_GRIM && moveVec == askedVec)
		_pos = pos;
	else
		_pos += moveVec;
}

void Actor::walkForward() {
	if (g_grim->getGameType() == GType_GRIM) {
		// Actor_WalkForward (FUN_0048fc80) only asks for the step (the flag at +0xc of the state); Actor_UpdateWalker makes it at the end of the
		// frame, when every script has run. A script that reads the position after the call gets the old one.
		_walking = false;
		_walkPending = true;
		return;
	}
	doWalkForward();
}

void Actor::doWalkForward() {
	float dist = g_grim->getPerSecond(_walkRate) * _timeScale;
	if (g_grim->getGameType() == GType_GRIM)
		dist = toF(origFrameSeconds() * _timeScale * _walkRate); // (Actor_UpdateWalker: seconds * time scale * walk rate)
	if (g_grim->getGameType() == GType_GRIM && Debug::isChannelEnabled(Debug::Actors)) {
		// the state the original's Walker_WalkSectors starts from (position, the frame), to be compared with a trace of its breakpoints
		ensureHeading();
		Debug::debug(Debug::Actors, "WSTATE %s dist %.9g pos %.9g %.9g %.9g right %.9g %.9g %.9g fwd %.9g %.9g %.9g up %.9g %.9g %.9g",
		             _name.c_str(), dist, _pos.x(), _pos.y(), _pos.z(), _hRight[0], _hRight[1], _hRight[2], _hFwd[0], _hFwd[1], _hFwd[2], _hUp[0], _hUp[1], _hUp[2]);
	}
	// Limit the amount of the movement per frame, otherwise with low fps
	// scripts that use WalkActorForward and proximity may break.
	const float maxDist = _walkRate / 5.f * _timeScale;
	if ((dist > 0 && dist > maxDist) || (dist < 0 && dist < maxDist))
		dist = maxDist;

	// Handle special case where actor is trying to walk but _walkRate is
	// currently set to 0.0f by _walkChore to simulate roboter-like walk style:
	// set _walkedCur to true to keep _walkChore playing in Actor::update()
	if (_walkRate == 0.0f) {
		_walkedCur = true;
	}

	if (g_grim->getGameType() != GType_GRIM)
		_walking = false;

	if (g_grim->getGameType() == GType_GRIM && _followBoxes && grimRuleOn("walk_sectors")) {
		// Actor_UpdateWalker (FUN_00491110): the step over the sectors, with the turn keys that forbid a turn at an edge
		ensureHeading();
		if (!_hSector)
			updateHeadingSector();
		const Math::Vector3d before = _pos;
		const bool keyA = _running || _turnKey != -1;
		const bool keyB = _running || _turnKey != 1;
		const bool done = grimWalkSectors(dist, keyA, keyB);
		if (done || (_pos - before).getMagnitude() > 0.0001f)
			_walkedCur = true;
		const Math::Vector3d after = _pos;
		_pos = before;
		moveTo(after);
		return;
	}

	if (_constrained) {
		// Along the line of the constraint, either way: a heading further off the line than the reflection angle does
		// not move the actor
		Sector *sector = nullptr;
		if (g_grim->getCurrSet())
			sector = g_grim->getCurrSet()->findPointSector(_pos, Sector::WalkType);
		Math::Vector3d up = sector ? sector->getNormal() : Math::Vector3d(0.f, 0.f, 1.f);
		Math::Vector3d forward(-moveSine(), moveCosine(), 0.f);
		if (sector) {
			forward = sector->getProjectionToPuckVector(forward);
			forward.normalize();
		}
		Math::Vector3d right = Math::Vector3d::crossProduct(forward, up);
		if (fabsf(Math::Angle::arcSine(CLIP(right.dotProduct(_constrainDir), -1.f, 1.f)).getDegrees()) > _reflectionAngle)
			return;
		Math::Vector3d dir = forward.dotProduct(_constrainDir) > 0 ? _constrainDir : -_constrainDir;
		if (g_grim->getGameType() == GType_GRIM)
			setHeadingVector(dir.x(), dir.y(), dir.z());
		else
			_moveYaw = Math::Angle::arcTangent2(-dir.x(), dir.y());
		_yaw = _moveYaw;
	}

	if (!_followBoxes) {
		Math::Vector3d forwardVec(-moveSine() * _pitch.getCosine(),
		                           moveCosine() * _pitch.getCosine(), _pitch.getSine());

		// EMI: Y is up-down, actors use an X-Z plane for movement
		if (g_grim->getGameType() == GType_MONKEY4) {
			float temp = forwardVec.z();
			forwardVec.x() = -forwardVec.x();
			forwardVec.z() = forwardVec.y();
			forwardVec.y() = temp;
		}

		_pos += forwardVec * dist;
		_walkedCur = true;
		return;
	}

	bool backwards = false;
	if (dist < 0) {
		dist = -dist;
		backwards = true;
	}

	int tries = 0;
	bool gapJumped = false; // (once in a call: two thin sectors a gap apart would send the walk back and forth for ever)
	// The edge rules of the original (FUN_004a8610) are ported for the boxes as the set file has them; the original shrinks the boxes in another way
	// (FUN_004a9d00: the shared vertices move together) than ScummVM does, and with them shrunk the rules of ScummVM are kept; so they are for an actor
	// that is constrained to a line (stairs, curbs: the first block of FUN_004a8610 aligns it with the line, not ported yet)
	const bool exactEdges = g_grim->getGameType() == GType_GRIM && grimRuleOn("exact_edges");
	while (dist > 0.0f) {
		Sector *currSector = nullptr, *prevSector = nullptr, *startSector = nullptr;
		// the sectors that were left in this step: the original keeps each switched off while the rest of the step is walked, so that the walk cannot go back
		// to one of them (FUN_004a8610, the sector flag at +0x2c)
		Common::Array<Sector *> left;
		Sector::ExitInfo ei;

		if (g_grim->getGameType() == GType_GRIM) {
			ensureHeading();
			// The walker of the original keeps the sector it is in (+0x5c) and changes it only where the walk leaves it: it does not look for
			// it again in every frame (sectors that lie over each other would give it another plane each time)
			if (_hSector && g_grim->getCurrSet()->grimOwnsSector(_hSector))
				currSector = const_cast<Sector *>(_hSector);
		}
		// EMI: the sector the actor is on is the one that holds its place (if there is none, the one under or over it, a step off in
		// height: a stair, a platform). The closest sector in space would put an actor that has just stepped over the edge of a
		// platform back on its edge.
		if (g_grim->getGameType() == GType_MONKEY4) {
			currSector = g_grim->getCurrSet()->findPointSector(_pos, Sector::WalkType);
			if (!currSector) {
				currSector = g_grim->getCurrSet()->findStepSector(_pos, Sector::WalkType, nullptr, 0.6f);
				if (currSector) {
					const float planeDist = currSector->distanceToPoint(_pos);
					_pos -= planeDist * currSector->getNormal();
				}
			}
		}
		if (!currSector) {
			g_grim->getCurrSet()->findClosestSector(_pos, &currSector, &_pos);
			if (g_grim->getGameType() == GType_GRIM && currSector)
				setHeadingSector(currSector);
		}
		if (!currSector) { // Shouldn't happen...
			Math::Vector3d forwardVec(-moveSine() * _pitch.getCosine(),
			                           moveCosine() * _pitch.getCosine(), _pitch.getSine());

			// EMI: Y is up-down, actors use an X-Z plane for movement
			if (g_grim->getGameType() == GType_MONKEY4) {
				float temp = forwardVec.z();
				forwardVec.x() = -forwardVec.x();
				forwardVec.z() = forwardVec.y();
				forwardVec.y() = temp;
			}

			if (backwards)
				forwardVec = -forwardVec;

			moveTo(_pos + forwardVec * dist);
			_walkedCur = true;
			return;
		}
		startSector = currSector;

		float oldDist = dist;
		while (currSector) {
			prevSector = currSector;
			Math::Vector3d forwardVec;
			const Math::Vector3d &normal = currSector->getNormal();
			if (g_grim->getGameType() == GType_GRIM) {
				Math::Angle ax = Math::Vector2d(normal.x(), normal.z()).getAngle();
				Math::Angle ay = Math::Vector2d(normal.y(), normal.z()).getAngle();

				float z1 = -moveCosine() * (ay - _pitch).getCosine();
				float z2 = moveSine() * (ax - _pitch).getCosine();
				forwardVec = Math::Vector3d(-moveSine() * ax.getSine() * _pitch.getCosine(),
				                             moveCosine() * ay.getSine() * _pitch.getCosine(), z1 + z2);
			} else {
				Math::Angle ax = Math::Vector2d(normal.x(), normal.y()).getAngle();
				Math::Angle az = Math::Vector2d(normal.z(), normal.y()).getAngle();

				float y1 = moveCosine() * (az - _pitch).getCosine();
				float y2 = moveSine() * (ax - _pitch).getCosine();
				forwardVec = Math::Vector3d(-moveSine() * ax.getSine() * _pitch.getCosine(), y1 + y2,
				                            -moveCosine() * az.getSine() * _pitch.getCosine());
			}

			if (backwards)
				forwardVec = -forwardVec;

			Math::Vector3d puckVec = currSector->getProjectionToPuckVector(forwardVec);
			puckVec /= puckVec.getMagnitude();
			if (exactEdges && _headingSet && _hSector == currSector) {
				// The walker of the original steps along the forward vector of its frame, which lies in the plane of the sector (FUN_004a8350: the
				// vector (0, step, 0) in the frame), not along a vector made from the heading on the ground
				puckVec = Math::Vector3d(_hFwd[0], _hFwd[1], _hFwd[2]);
				if (backwards)
					puckVec = -puckVec;
			}
			// The original first looks whether the whole step ends in the sector it is in, by the test of the sector (a point up to 2e-5 outside an edge is
			// inside: Sector_ContainsPoint): then it moves there at once, also along an edge (FUN_004a8610)
			if (exactEdges && currSector->isPointInSector(_pos + puckVec * dist, true)) {
				moveTo(_pos + puckVec * dist);
				_walkedCur = true;
				return;
			}
			currSector->getExitInfo(_pos, puckVec, &ei);
			float exitDist = (ei.exitPoint - _pos).getMagnitude();
			if (dist < exitDist) {
				moveTo(_pos + puckVec * dist);
				_walkedCur = true;
				return;
			}
			_pos = ei.exitPoint;
			dist -= exitDist;
			if (exitDist > 0.0001)
				_walkedCur = true;

			// Check for an adjacent sector which can continue
			// the path
			if (exactEdges) {
				// (the original asks for the sector at the exit point itself, with the sector it leaves switched off: FUN_004a8610)
				left.push_back(prevSector);
				currSector = g_grim->getCurrSet()->findPointSector(ei.exitPoint, Sector::WalkType, true, &left);
						} else {
			// (not the sector being left: with the tolerance of the point test its edge still counts as inside a little way on)
			currSector = g_grim->getCurrSet()->findPointSector(ei.exitPoint + (float)0.0001 * puckVec, Sector::WalkType, prevSector);

			// EMI: the floor goes up or down by a step (stairs, a platform in the Lua Bar): the next sector is the one under or over the
			// place, whose plane is a step away; the position is set onto it below.
			if (!currSector && g_grim->getGameType() == GType_MONKEY4)
				currSector = g_grim->getCurrSet()->findStepSector(ei.exitPoint + (float)0.0001 * puckVec, Sector::WalkType, prevSector, 0.6f);

			// EMI: neighbouring sectors of different heights can leave a gap of a few centimetres between them (the
			// way to Meathook's front door), the walk goes on into a sector that is that close
			if (!currSector && !gapJumped && g_grim->getGameType() == GType_MONKEY4) {
				gapJumped = true;
				Sector *near = nullptr;
				Math::Vector3d nearPt;
				const Math::Vector3d probe = ei.exitPoint + (float)0.0001 * puckVec;
				g_grim->getCurrSet()->findClosestSector(probe, &near, &nearPt, prevSector);
				if (near && (nearPt - probe).getMagnitude() < 0.1f) {
					currSector = near;
					_pos = nearPt;
				}
				}
			}

			// EMI: some sectors are significantly higher/lower than others.
			if (currSector && g_grim->getGameType() == GType_MONKEY4) {
				float planeDist = currSector->distanceToPoint(_pos);
				if (fabs(planeDist) < 1.f)
					_pos -= planeDist * currSector->getNormal();
			}

			if (currSector == prevSector || currSector == startSector)
				break;
			if (g_grim->getGameType() == GType_GRIM && currSector && _headingSet)
				setHeadingSector(currSector);
			// (the original goes on for no more than 2 cm into the next sector in a frame: FUN_004a8610)
			if (g_grim->getGameType() == GType_GRIM && currSector && dist > 0.02f)
				dist = 0.02f;
		}

		int turnDir = 1;
		if (ei.angleWithEdge > 90) {
			ei.angleWithEdge = 180 - ei.angleWithEdge;
			ei.edgeDir = -ei.edgeDir;
			turnDir = -1;
		}
		if (_constrained) {
			// A constrained actor is turned along the edge whatever the angle, the line of the constraint with it, and
			// stops for this frame
			if (g_grim->getGameType() == GType_GRIM)
				turnHeadingBy(((ei.angleWithEdge + (exactEdges ? 0.2f : 1.f)) * turnDir).getDegrees());
			else
				_moveYaw += (ei.angleWithEdge + 1.f) * turnDir;
			_yaw = _moveYaw;
			_constrainDir = Math::Vector3d(-moveSine(), moveCosine(), 0.f);
			return;
		}
		if (exactEdges) {
			// The original turns along the edge by 90.2 - asin(|cos|), the angle with the edge and 0.2 degrees (FUN_004a6d10), when that is below the reflection
			// angle, and not against a turn key that is held (unless it runs): toward the left (a positive turn) only if the key is not the right one, and the other way round
			// (FUN_004a8610). The rest of the step is walked in the new direction.
			float a = ei.angleWithEdge.getDegrees() + 0.2f;
			if (!(a < _reflectionAngle))
				return;
			float t = a * turnDir;
			// (an actor that runs, SetActorWalkDominate, is turned whichever key is held: the flag at +0x1bc in FUN_00491110)
			if (!_running && !((t > 0.f && _currTurnDir != -1) || (t < 0.f && _currTurnDir != 1)))
				return;
			turnHeadingBy(t);
			_movePitch = 0;
			_moveRoll = 0;
			_turning = true;
			if (oldDist <= dist + 0.001f) {
				tries++;
				if (tries > 3)
					break;
			}
			continue;
		}
		if (ei.angleWithEdge > _reflectionAngle)
			return;

		ei.angleWithEdge += (float)1.0f;

		if (g_grim->getGameType() == GType_MONKEY4) {
			ei.angleWithEdge = -ei.angleWithEdge;
		}
		if (g_grim->getGameType() == GType_GRIM) {
			turnHeadingBy((ei.angleWithEdge * turnDir).getDegrees());
			_movePitch = 0;
			_moveRoll = 0;
			_turning = true;
		} else {
			turnTo(0, _moveYaw + ei.angleWithEdge * turnDir, 0);
		}

		if (oldDist <= dist + 0.001f) {
			// If we didn't move at all, keep trying a couple more times
			// in case we can move in the new direction.
			tries++;
			if (tries > 3)
				break;
		}
	}
}

void Actor::walkTowards(const Math::Angle &heading) {
	Math::Angle diff = heading - _moveYaw;
	diff.normalize(-180);

	// The original turns the heading by twice the turn rate every frame and
	// does not wait until it is aligned before walking.
	const float maxTurn = fabsf(g_grim->getPerSecond(_turnRate)) * 2.f;
	if (g_grim->getGameType() == GType_GRIM) {
		if (fabsf(diff.getDegrees()) <= maxTurn)
			faceHeading(heading);
		else
			turnHeadingBy(diff > 0 ? maxTurn : -maxTurn);
	} else if (fabsf(diff.getDegrees()) <= maxTurn) {
		_moveYaw = heading;
	} else if (diff > 0) {
		_moveYaw += maxTurn;
	} else {
		_moveYaw -= maxTurn;
	}
	_turning = true;

	walkForward();
}

Math::Vector3d Actor::getSimplePuckVector() const {
	if (g_grim->getGameType() == GType_MONKEY4) {
		Math::Angle yaw = 0;
		const Actor *a = this;
		while (a) {
			yaw += a->_yaw;
			if (!a->isAttached())
				break;
			a = Actor::getPool().getObject(a->_attachedActor);
		}
		return Math::Vector3d(yaw.getSine(), 0, yaw.getCosine());
	} else if (_headingSet) {
		// (the way the walker faces, which is what the original gives: the angle that is seen follows it a frame later)
		float x, y;
		headingGround(_hFwd, x, y);
		return Math::Vector3d(x, y, 0);
	} else {
		return Math::Vector3d(-_yaw.getSine(), _yaw.getCosine(), 0);
	}
}

Math::Vector3d Actor::getPuckVector() const {
	Math::Vector3d forwardVec = getSimplePuckVector();

	Set *currSet = g_grim->getCurrSet();
	if (!currSet)
		return forwardVec;

	Sector *sector = currSet->findPointSector(_pos, Sector::WalkType);
	if (!sector)
		return forwardVec;
	else
		return sector->getProjectionToPuckVector(forwardVec);
}

void Actor::setFov(float degrees) {
	// As the original: the cosine of half the angle (SetActorFOV, 0x412ff0)
	_fovCos = cosf(degrees * (float)M_PI / 180.0f * 0.5f);
}

// What an actor sees, in the original (0x427eb0): the unit vector in the horizontal plane from the actor to the other one, dotted
// with the way the actor faces, is at least the cosine of half the field of view. (Without a field of view set an actor sees nothing
// there; the player's actor has one of 120 degrees from its script, which is the default here, as it is not saved.)
bool Actor::canSeeEmi(const Actor *other) const {
	Math::Vector3d delta = other->getWorldPos() - getWorldPos();
	delta.y() = 0.f;
	if (delta.getMagnitude() < Math::epsilon)
		return true;
	delta.normalize();
	Math::Vector3d forward = getSimplePuckVector();
	forward.y() = 0.f;
	forward.normalize();
	return forward.dotProduct(delta) >= _fovCos;
}

void Actor::setPuckOrient(bool orient) {
	// (calculateOrientation() lets the actor follow the normal of the sector it stands on)
	_puckOrient = orient;
}

void Actor::setRestChore(int chore, Costume *cost) {
	if (_restChore.equals(cost, chore))
		return;

	_restChore.stop(g_grim->getGameType() == GType_GRIM);

	if (!cost) {
		cost = _restChore._costume;
	}
	if (!cost) {
		cost = getCurrentCostume();
	}

	_restChore = ActionChore(cost, chore);

	_restChore.playLooping(g_grim->getGameType() == GType_GRIM);
}

int Actor::getRestChore() const {
	return _restChore._chore;
}

void Actor::setWalkChore(int chore, Costume *cost) {
	if (_walkChore.equals(cost, chore))
		return;

	if (_walkedLast && _walkChore.isPlaying()) {
		_walkChore.stop(true);

		if (g_grim->getGameType() == GType_GRIM) {
			_restChore.playLooping(true);
		}
	}

	if (!cost) {
		cost = _walkChore._costume;
	}
	if (!cost) {
		cost = getCurrentCostume();
	}

	_walkChore = ActionChore(cost, chore);
}

void Actor::setTurnChores(int left_chore, int right_chore, Costume *cost) {
	if (_leftTurnChore.equals(cost, left_chore) && _rightTurnChore.equals(cost, right_chore))
		return;

	if (!cost) {
		cost = _leftTurnChore._costume;
	}
	if (!cost) {
		cost = getCurrentCostume();
	}

	_leftTurnChore.stop(true);
	_rightTurnChore.stop(true);
	_lastTurnDir = 0;

	_leftTurnChore = ActionChore(cost, left_chore);
	_rightTurnChore = ActionChore(cost, right_chore);

	if ((left_chore >= 0 && right_chore < 0) || (left_chore < 0 && right_chore >= 0))
		error("Unexpectedly got only one turn chore");
}

void Actor::setTalkChore(int index, int chore, Costume *cost) {
	if (index < 1 || index > 10)
		error("Got talk chore index out of range (%d)", index);

	index--;

	if (!cost) {
		cost = _talkChore[index]._costume;
	}
	if (!cost) {
		cost = getCurrentCostume();
	}

	if (_talkChore[index].equals(cost, chore))
		return;

	_talkChore[index].stop();

	_talkChore[index] = ActionChore(cost, chore);
}

int Actor::getTalkChore(int index) const {
	return _talkChore[index]._chore;
}

Costume *Actor::getTalkCostume(int index) const {
	return _talkChore[index]._costume;
}

void Actor::setMumbleChore(int chore, Costume *cost) {
	if (_mumbleChore.isPlaying()) {
		_mumbleChore.stop();
	}

	if (!cost) {
		cost = _mumbleChore._costume;
	}
	if (!cost) {
		cost = getCurrentCostume();
	}

	_mumbleChore = ActionChore(cost, chore);
}

void Actor::stopAllChores(bool ignoreLoopingChores) {
	for (Common::List<Costume *>::iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
		Costume *costume = *i;
		costume->stopChores(ignoreLoopingChores);
		if (costume->isChoring(false) == -1) {
			freeCostume(costume);
			i = _costumeStack.erase(i);
			--i;
		}
	}
}

void Actor::turn(int dir) {
	_walking = false;
	if (g_grim->getGameType() == GType_GRIM) {
		// The key is taken only when the heading that is seen has caught up with the heading of the walker to within two steps of the turn: the angle
		// that is still to turn (the heading after a turn along an edge is far ahead of what is seen) makes the original ignore TurnActor (FUN_0048fbd0)
		ensureHeading();
		// Actor_TurnKey (FUN_0048fbd0): rate * (float)(seconds * time scale) * dir, taken when the angle still to turn is not more than twice that
		float step = toF((LD)_turnRate * (LD)(float)(origFrameSeconds() * _timeScale) * dir);
		if (step < 0.f)
			step = -step;
		step = step + step;
		if (grimRuleOn("turn_key_rule") && _turnLeft > step)
			return;
	}
	// (the turn rate of the walker is negative after an immediate SetActorRot until the heading that is seen has been brought to the frame (FUN_00491230), the
	// turn of the key is the other way until then)
	float delta = g_grim->getPerSecond(_turnRate) * dir * _timeScale;
	if (g_grim->getGameType() == GType_GRIM) {
		// (Actor_UpdateWalker: key * rate * (float)(seconds * time scale))
		delta = toF((LD)dir * _turnRate * (LD)(float)(origFrameSeconds() * _timeScale));
		if (_yawSnap)
			delta = -delta;
	}
	if (g_grim->getGameType() == GType_GRIM)
		_turnKey = dir > 0 ? 1 : -1;
	if (g_grim->getGameType() == GType_MONKEY4) {
		delta = -delta;
	}
	if (g_grim->getGameType() == GType_GRIM)
		turnHeadingBy(delta);
	else
		_moveYaw = _moveYaw + delta;
	_turning = true;
	_currTurnDir = dir;
}

Math::Angle Actor::getYawTo(const Actor *a) const {
	Math::Vector3d forwardVec = getSimplePuckVector();
	Math::Vector3d delta = a->getWorldPos() - getWorldPos();

	if (g_grim->getGameType() == GType_MONKEY4) {
		delta.y() = 0;
	} else {
		delta.z() = 0;
	}
	if (delta.getMagnitude() < Math::epsilon)
		return Math::Angle(0);
	return Math::Vector3d::angle(forwardVec, delta);
}

Math::Angle Actor::getYawTo(const Math::Vector3d &p) const {
	Math::Vector3d dpos = p - _pos;

	if (g_grim->getGameType() == GType_MONKEY4) {
		dpos.y() = dpos.z();
		// EMI yaws turn the other way (the puck vector is (sin, cos)), as in calculateOrientation()
		dpos.x() = -dpos.x();
	}
	if (dpos.x() == 0 && dpos.y() == 0)
		return 0;
	else
		return Math::Angle::arcTangent2(-dpos.x(), dpos.y());
}

// The subtitles of an actor that is not on the screen are shown at a position the
// scripts can change, they move it to the top while a dialog is open.
static void placeOffscreenText(TextObject *textObject) {
	textObject->setX(g_grim->_offscreenTextX);
	textObject->setY(g_grim->_offscreenTextY);
}

void Actor::sayLine(const char *msgId, bool background, float x, float y) {
	assert(msgId);

	if (msgId[0] == 0) {
		warning("Actor::sayLine: Empty message");
		return;
	}

	char id[50];
	Common::String msg = LuaBase::instance()->parseMsgText(msgId, id);

	if (id[0] == 0) {
		error("Actor::sayLine: No message ID for text");
		return;
	}

	// During Fullscreen movies SayLine is usually called for text display only.
	// The movie with Charlie screaming after Manny put the sheet on him instead
	// uses sayLine for the voice too.
	// However, normal SMUSH movies may call SayLine, for example:
	// When Domino yells at Manny (a SMUSH movie) he does it with
	// a SayLine request rather than as part of the movie!

	Common::String soundName = id;

	if (g_grim->getGameType() == GType_GRIM) {
		if (g_grim->isRemastered()) {
			soundName = g_grim->getLanguagePrefix() + "_" + soundName;
		}
		soundName += ".wav";
	} else if (g_grim->getGameType() == GType_MONKEY4 && g_grim->getGamePlatform() == Common::kPlatformPS2) {
		soundName += ".scx";
	} else {
		soundName += ".wVC";
	}
	if (_talkSoundName == soundName)
		return;

	if (_talking || msg.empty())
		shutUp();

	_talkSoundName = soundName;

	Set *currSet = g_grim->getCurrSet();

	if (g_grim->getSpeechMode() != GrimEngine::TextOnly) {
		// if there is no costume probably the character is drawn by a smush movie, so
		// we don't want to go out of sync with it.
		if (g_grim->getGameType() == GType_GRIM && getCurrentCostume()) {
			_talkDelay = 500;
		}
		g_sound->startVoice(_talkSoundName.c_str());
	}

	// If the actor is clearly not visible then don't try to play the lip sync
	if (_visible && (!g_movie->isPlaying() || g_grim->getMode() == GrimEngine::NormalMode)) {
		Common::String soundLip = id;
		soundLip += ".lip";

		if (!_talkChore[0].isPlaying()) {
			// _talkChore[0] is *_stop_talk
			_talkChore[0].setLastFrame();
		}
		// Sometimes actors speak offscreen before they, including their
		// talk chores are initialized.
		// For example, when reading the work order (a LIP file exists for no reason).
		// Also, some lip sync files have no entries
		// In these cases, revert to using the mumble chore.
		if (g_grim->getSpeechMode() != GrimEngine::TextOnly)
			_lipSync = g_resourceloader->getLipSync(soundLip);
		// If there's no lip sync file then load the mumble chore if it exists
		// (the mumble chore doesn't exist with the cat races announcer)
		if (!_lipSync)
			_mumbleChore.playLooping();

		_talkAnim = -1;
	}

	_talking = true;
	g_grim->addTalkingActor(this);

	_backgroundTalk = background;
	if (background)
		_isTalkingBackground = true;

	if (_sayLineText && g_grim->getMode() != GrimEngine::SmushMode) {
		delete TextObject::getPool().getObject(_sayLineText);
		_sayLineText = 0;
	}

	if (!msg.empty()) {
		GrimEngine::SpeechMode m = g_grim->getSpeechMode();
		if (!g_grim->_sayLineDefaults.getFont() || m == GrimEngine::VoiceOnly)
			return;

		if (background) {
			// if we're talking background draw the text object only if there are no no-background
			// talking actors. This prevents glottis and nick subtitles overlapping in the high roller lounge,
			// where glottis is background and nick isn't. (https://github.com/residualvm/residualvm/issues/685)
			for (Actor *a : g_grim->getTalkingActors()) {
				if (!a->_backgroundTalk && a->_sayLineText) {
					return;
				}
			}
		} else {
			// if we're not background then delete the TextObject of any talking background actor.
			for (Actor *a : g_grim->getTalkingActors()) {
				if (a->_backgroundTalk && a->_sayLineText) {
					delete TextObject::getPool().getObject(a->_sayLineText);
					a->_sayLineText = 0;
				}
			}
		}

		TextObject *textObject = new TextObject();
		textObject->setDefaults(&g_grim->_sayLineDefaults);
		textObject->setFGColor(_talkColor);
		textObject->setIsSpeech();
		if (m == GrimEngine::TextOnly || g_grim->getMode() == GrimEngine::SmushMode) {
			// (the original: 640 ms and 15 ms for each character, as many times as the text speed is below 11; FUN_00492b30)
			textObject->setDuration((g_grim->getGameType() == GType_GRIM ? 640 : 500) + msg.size() * 15 * (11 - g_grim->getTextSpeed()));
		}
		if (g_grim->getGameType() == GType_MONKEY4 && (x != -1 || y != -1)) {
			textObject->setX(320 * (x + 1));
			textObject->setY(240 * (y + 1));
		} else if (g_grim->getMode() == GrimEngine::SmushMode) {
			textObject->setX(640 / 2);
			textObject->setY(456);
			g_grim->setMovieSubtitle(textObject);
		} else {
			if (_visible && isInSet(currSet->getName()) && getEffectiveSortOrder() >= 0) {
				_mustPlaceText = true;
			} else {
				_mustPlaceText = false;
				placeOffscreenText(textObject);
			}
		}
		textObject->setText(msgId, _mustPlaceText);
		if (g_grim->getMode() != GrimEngine::SmushMode)
			_sayLineText = textObject->getId();
	}
}

void Actor::lineCleanup() {
	if (_sayLineText) {
		delete TextObject::getPool().getObject(_sayLineText);
		_sayLineText = 0;
	}
}

bool Actor::isTalking() {
	// This should return if the actor is actually talking, disregarding of the background status,
	// otherwise when Naranja is asleep Toto's lines will be cut sometimes. Naranja and Toto both
	// talk in background, and when Naranja lines stop toto:wait_for_message() should not return
	// until he actually stops talking.
	return _talking;
}

bool Actor::isTalkingForeground() const {
	if (!_talking) {
		return false;
	}

	if (_backgroundTalk)
		return _isTalkingBackground;

	return true;
}

void Actor::shutUp() {
	// While the call to stop the sound is usually made by the game,
	// we also need to handle when the user terminates the dialog.
	// Some warning messages will occur when the user terminates the
	// actor dialog but the game will continue alright.
	if (_talkSoundName != "") {
		g_sound->stopSound(_talkSoundName.c_str());
		_talkSoundName = "";
	}

	if (_lipSync) {
		if (_talkAnim != -1)
			_talkChore[_talkAnim].stop(g_grim->getGameType() == GType_MONKEY4, ActionChore::talkFadeTime);
		_lipSync = nullptr;
	}
	// having a lipsync is no guarantee the mumble chore is no running. the talk chores may be -1 (domino in do)
	stopMumbleChore();
	stopTalking();

	if (_sayLineText) {
		delete TextObject::getPool().getObject(_sayLineText);
		_sayLineText = 0;
	}

	// The actors talking in background have a special behaviour: if there are two or more of them
	// talking at the same time, after one of them finishes talking calling isTalking() an *all*
	// of them must return false. This is necessary for the angelitos in set fo: when they start crying
	// they talk in background, and the lua script that hangs on IsMessageGoing() must return before they
	// stop, since they can go on forever.
	if (_backgroundTalk)
		_isTalkingBackground = false;

	_talking = false;
}

void Actor::pushCostume(const char *n) {
	Costume *c = findCostume(n);
	if (c) {
		Debug::debug(Debug::Actors, "Trying to push a costume already in the stack. %s, %s", getName().c_str(), n);
		return;
	}

	Costume *newCost = g_resourceloader->loadCostume(n, this, getCurrentCostume());

	_costumeStack.push_back(newCost);
}

void Actor::setColormap(const char *map) {
	if (!_costumeStack.empty()) {
		Costume *cost = _costumeStack.back();
		cost->setColormap(map);
	} else {
		warning("Actor::setColormap: No costumes");
	}
}

void Actor::setCostume(const char *n) {
	if (!_costumeStack.empty())
		popCostume();

	pushCostume(n);
}

void Actor::popCostume() {
	if (!_costumeStack.empty()) {
		Costume *popped = _costumeStack.back();
		_costumeStack.pop_back();
		if (g_grim->getGameType() == GType_GRIM && !_costumeStack.empty())
			_costumeStack.back()->copyHeadLookState(popped);
		freeCostume(popped);

		if (_costumeStack.empty()) {
			Debug::debug(Debug::Actors, "Popped (freed) the last costume for an actor.\n");
		}
	} else {
		Debug::warning(Debug::Actors, "Attempted to pop (free) a costume when the stack is empty!");
	}
}

void Actor::clearCostumes() {
	// Make sure to destroy costume copies in reverse order
	while (!_costumeStack.empty())
		popCostume();
}

Costume *Actor::getCurrentCostume() const {
	if (g_grim->getGameType() == GType_MONKEY4) {
		// Return the first costume that has a model component.
		for (Common::List<Costume *>::const_iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
			EMICostume *costume = static_cast<EMICostume *>(*i);
			if (costume->getEMIModel()) {
				return costume;
			}
		}
		return nullptr;
	} else {
		if (_costumeStack.empty())
			return nullptr;
		else
			return _costumeStack.back();
	}
}

void Actor::setHead(int joint1, int joint2, int joint3, float maxRoll, float maxPitch, float maxYaw) {
	Costume *curCostume = getCurrentCostume();
	if (curCostume) {
		curCostume->setHead(joint1, joint2, joint3, maxRoll, maxPitch, maxYaw);
	}
}

void Actor::setHead(const char *joint, const Math::Vector3d &offset) {
	Costume *curCostume = getCurrentCostume();
	if (curCostume) {
		EMICostume *costume = static_cast<EMICostume *>(curCostume);
		costume->setHead(joint, offset);
	}
}

void Actor::setHeadLimits(float yawRange, float maxPitch, float minPitch) {
	Costume *curCostume = getCurrentCostume();
	if (curCostume) {
		EMICostume *costume = static_cast<EMICostume *>(curCostume);
		costume->setHeadLimits(yawRange, maxPitch, minPitch);
	}
}

void Actor::setLookAtRate(float rate) {
	getCurrentCostume()->setLookAtRate(rate);
}

float Actor::getLookAtRate() const {
	return getCurrentCostume()->getLookAtRate();
}

EMIModel *Actor::findModelWithMesh(const Common::String &mesh) {
	for (Common::List<Costume *>::iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
		EMICostume *costume = static_cast<EMICostume *>(*i);
		if (!costume) {
			continue;
		}
		for (int j = 0; j < costume->getNumChores(); j++) {
			EMIModel *model = costume->getEMIModel(j);
			if (!model) {
				continue;
			}
			if (mesh == model->_meshName) {
				return model;
			}
		}
	}
	return nullptr;
}

void Actor::setGlobalAlpha(float alpha, const Common::String &mesh) {
	if (mesh.empty()) {
		_globalAlpha = alpha;
	} else {
		EMIModel *model = findModelWithMesh(mesh);
		if (model != nullptr) {
			model->_meshAlpha = alpha;
		}
	}
}

void Actor::setAlphaMode(AlphaMode mode, const Common::String &mesh) {
	if (mesh.empty()) {
		_alphaMode = mode;
	} else {
		EMIModel *model = findModelWithMesh(mesh);
		if (model != nullptr) {
			model->_meshAlphaMode = mode;
		}
	}
}

Costume *Actor::findCostume(const Common::String &n) {
	for (Common::List<Costume *>::iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
		if ((*i)->getFilename().compareToIgnoreCase(n) == 0)
			return *i;
	}

	return nullptr;
}

void Actor::setFollowBoxes(bool follow) {
	_followBoxes = follow;
	if (follow && g_grim->getGameType() == GType_GRIM) {
		ensureHeading();
		updateHeadingSector();
	} else if (follow && g_grim->getCurrSet() && isInSet(g_grim->getCurrSet()->getName())) {
		// The originals (EMI: 0x428eb0 -> 0x444b90) put the actor into the walkboxes the moment it starts to follow them, as SetActorPos does for an actor
		// that already follows them. A script that places an actor with setpos and then calls follow_boxes relies on that: the walk of an actor that is
		// outside of them is ignored (see walkTo()).
		g_grim->getCurrSet()->findClosestSector(_pos, nullptr, &_pos);
	}
	if (follow)
		_hasFollowedBoxes = true;
}

// FUN_004a7520: the direction along the box the walker stands in. The heading goes back (or ahead, for a walk backwards) to the edge it came in through; from
// there a point of the opposite edge, at the same fraction along it but at least 15% from the corners, is where the line goes.
bool Actor::grimConstraintLine(float *out) const {
	const Sector *sector = _hSector;
	if (!sector)
		return false;
	const float pos[3] = { _pos.x(), _pos.y(), _pos.z() };
	const float sgn = _wsDist <= 0.f ? 100.f : -100.f;
	const float far[3] = { toF((LD)_hFwd[0] * sgn + pos[0]), toF((LD)_hFwd[1] * sgn + pos[1]), toF((LD)_hFwd[2] * sgn + pos[2]) };
	float ex[3] = { 0.f, 0.f, 0.f };
	sector->grimFindExitPoint(pos, far, ex);
	int idx = sector->grimExitEdge();
	if (idx == -1)
		return false;
	const Math::Vector3d *v = sector->getVertices();
	const int n = sector->getNumVertices();
	auto V = [&](int i) -> const Math::Vector3d & { return v[((i % n) + n) % n]; };
	auto d2 = [&](const Math::Vector3d &a, const float *b) {
		const LD dx = (LD)a.x() - b[0], dy = (LD)a.y() - b[1], dz = (LD)a.z() - b[2];
		return (float)(dz * dz + dy * dy + dx * dx);
	};
	auto edgeIsAlongHeading = [&](int from, int to) {
		// the edge from vertex 'to' to vertex 'from' against the forward vector of the walker: closer than 1 degree to it, or to its opposite
		float e[3] = { V(from).x() - V(to).x(), V(from).y() - V(to).y(), V(from).z() - V(to).z() };
		origNormalize3(e);
		const LD dot = (LD)e[0] * _hFwd[0] + (LD)_hFwd[2] * e[2] + (LD)e[1] * _hFwd[1];
		const LD ang = 90.0L - (LD)originalAsinDeg((float)dot);
		return ang < 1.0L || 179.0L < ang;
	};
	if (0.0025f <= d2(V(idx), ex)) {
		// the exit is not at the corner of the edge: when it is at the other corner of the edge and the edge runs along the heading, the edge before
		if (d2(V(idx - 1), ex) < 0.0025f && edgeIsAlongHeading(idx - 1, idx))
			idx = ((idx - 1) % n + n) % n;
	} else if (edgeIsAlongHeading(idx - 1, idx)) {
		idx = (idx + 1) % n;
	}
	const Math::Vector3d &vi = V(idx);
	const Math::Vector3d &vp = V(idx - 1);
	float dx = vp.x() - vi.x();
	float t;
	if (fabsf(dx) <= 1e-05f)
		dx = 0.f;
	if (dx == 0.f)
		t = (float)(((LD)ex[1] - vi.y()) / ((LD)vp.y() - vi.y()));
	else
		t = (float)(((LD)ex[0] - vi.x()) / dx);
	if (t < 0.15f)
		t = 0.15f;
	if (0.85f < t)
		t = 0.85f;
	const Math::Vector3d &o1 = V(idx + 1);
	const Math::Vector3d &o2 = V(n - 2 + idx);
	const float mx = (float)(((LD)o2.x() - o1.x()) * t + o1.x());
	const float my = (float)(((LD)o2.y() - o1.y()) * t + o1.y());
	const float mz = (float)(((LD)o2.z() - o1.z()) * t + o1.z());
	out[0] = mx - ex[0];
	out[1] = my - ex[1];
	out[2] = mz - ex[2];
	origNormalize3(out);
	if (_wsDist < 0.f) {
		out[0] = out[0] * -1.f;
		out[1] = out[1] * -1.f;
		out[2] = out[2] * -1.f;
	}
	return true;
}

// The scripts constrain the actor on stairs and curbs. Walker_SetConstraint (FUN_004a85b0): the walker is turned onto the line of the box it stands in and
// walks only along it.
void Actor::setConstrain(bool constrain) {
	_constrained = false;
	if (g_grim->getGameType() != GType_GRIM)
		return;
	// (the original ignores it during a walk along a path)
	if (!constrain || _walking)
		return;
	ensureHeading();
	float line[3];
	if (!_hSector || !grimConstraintLine(line))
		return;
	_cvec[0] = line[0];
	_cvec[1] = line[1];
	_cvec[2] = line[2];
	_hFwd[0] = line[0];
	_hFwd[1] = line[1];
	_hFwd[2] = line[2];
	rebuildHeadingRight();
	syncMoveYaw();
	_constrained = true;
	_constrainDir = Math::Vector3d(line[0], line[1], line[2]);
	_yaw = Math::Angle::arcTangent2(-line[0], line[1]);
}

// FUN_004a8310: the position of the walker is set as it is; the actors it runs into are looked at once after the whole step (FUN_004939f0)
void Actor::grimSetWalkerPos(const float *p) {
	_pos.set(p[0], p[1], p[2]);
}

// Walker_WalkSectors (FUN_004a8610): a step of the walk. The point the step ends at (in the frame of the walker: straight ahead by the distance) is the new
// position when it is in the sector. Else the walk goes on to the exit point of the sector, with the sector switched off so that it cannot come back to
// it; into a sector that is there (the rest of the step, but no more than 2 cm in one frame), or, at an edge, turned along it by the angle with the edge when
// that is below the reflection angle and the turn key that is held does not forbid it (the step goes on in the new direction).
bool Actor::grimWalkSectors(float dist, bool turnLeft, bool turnRight) {
	bool ret = false;
	if (Debug::isChannelEnabled(Debug::Actors))
		Debug::debug(Debug::Actors, "WALKSEC %s depth %d dist %.9g pos %.9g %.9g %.9g fwd %.9g %.9g %.9g constrained %d", _name.c_str(), s_wsDepth, dist, _pos.x(), _pos.y(), _pos.z(), _hFwd[0], _hFwd[1], _hFwd[2], _constrained);
	if (s_wsDepth == 0)
		_wsDist = dist;
	s_wsDepth++;
	if (_constrained) {
		const LD d = (LD)_hRight[0] * _cvec[0] + (LD)_cvec[2] * _hRight[2] + (LD)_hRight[1] * _cvec[1];
		const LD a = (90.0L - (LD)originalAsinDeg((float)d)) - 90.0L;
		float turn = (float)a;
		if ((LD)_reflectionAngle < fabsl(a))
			return false;
		const LD f = (LD)_hFwd[0] * _cvec[0] + (LD)_cvec[2] * _hFwd[2] + (LD)_hFwd[1] * _cvec[1];
		if (f <= 0.L)
			turn = -turn;
		turnHeadingBy(turn);
	}
	const float local[3] = { 0.f, dist, 0.f };
	const float frame[12] = { _hRight[0], _hRight[1], _hRight[2], _hFwd[0], _hFwd[1], _hFwd[2], _hUp[0], _hUp[1], _hUp[2], _pos.x(), _pos.y(), _pos.z() };
	float target[3];
	origTurn(target, local, frame, true);
	Sector *sector = const_cast<Sector *>(_hSector);
	if (!sector || sector->grimContains(target, true)) {
		grimSetWalkerPos(target);
		s_wsDepth = 0;
		return true;
	}
	if (s_wsDepth > 8) {
		s_wsDepth = 0;
		return false;
	}
	const float pos[3] = { _pos.x(), _pos.y(), _pos.z() };
	float exitPt[3];
	float angle = sector->grimFindExitPoint(pos, target, exitPt);
	const int exitEdge = sector->grimExitEdge();
	sector->grimSetFree(false);
	float rel[3] = { toF((LD)exitPt[0] - pos[0]), toF((LD)exitPt[1] - pos[1]), toF((LD)exitPt[2] - pos[2]) };
	float local2[3];
	origToLocal(local2, rel, frame);
	dist = toF((LD)dist - local2[1]);
	Sector *next = g_grim->getCurrSet()->grimSectorAt(exitPt);
	if (Debug::isChannelEnabled(Debug::Actors))
		Debug::debug(Debug::Actors, "WALKSEC2 sector %s exit %.9g %.9g %.9g angle %.9g edge %d next %s keys %d %d", sector->getName().c_str(), exitPt[0], exitPt[1], exitPt[2], angle, exitEdge, next ? next->getName().c_str() : "none", turnLeft, turnRight);
	if (!next) {
		grimSetWalkerPos(exitPt);
		if (!(sector->getType() & 0x100)) {
			const float aa = angle < 0.f ? -angle : angle;
			if (_reflectionAngle <= aa || _constrained) {
				_wsDist = _wsDist - dist;
			} else if ((angle > 0.f && turnLeft) || (angle < 0.f && turnRight)) {
				turnHeadingBy(angle);
				sector->grimSetFree(true);
				ret = grimWalkSectors(dist, true, true);
			}
			if (_constrained) {
				turnHeadingBy(angle);
				_cvec[0] = _hFwd[0];
				_cvec[1] = _hFwd[1];
				_cvec[2] = _hFwd[2];
			}
			sector->grimSetFree(true);
			return ret;
		}
		// a funnel sector: along the edge that is next to the one the walk left by
		const int n = sector->getNumVertices();
		const int fe = sector->grimFunnelEdge();
		if ((fe + 1) % n != exitEdge) {
			const Math::Vector3d &fv = sector->getVertices()[fe];
			const float crel[3] = { toF((LD)fv.x() - pos[0]), toF((LD)fv.y() - pos[1]), toF((LD)fv.z() - pos[2]) };
			float cl[3];
			origToLocal(cl, crel, frame);
			if (cl[0] <= 0.f) {
				if (angle < 3.0f)
					angle = toF(((LD)angle - -90.0L) - -90.2L);
			} else if (3.0f < angle) {
				angle = toF(-90.2L - (90.0L - (LD)angle));
			}
			turnHeadingBy(angle);
			ret = grimWalkSectors(dist, true, true);
			sector->grimSetFree(true);
			return ret;
		}
		return false;
	}
	grimSetWalkerPos(exitPt);
	setHeadingSector(next);
	if (0.02f <= dist)
		dist = 0.02f;
	ret = grimWalkSectors(dist, true, true);
	sector->grimSetFree(true);
	return ret;
}

// ---- The heading of a Grim actor, as the original keeps it ----
// The original does not keep an angle for the way an actor faces when it moves but the frame of its walker (a right, a forward and an up
// vector, and the plane of the sector it is in), and turns it only by relative angles: the rotation matrix of an angle comes from a table
// (sincos.cpp) and the angle that is needed to face a direction from an arcsine series. Both are a little off the true values, so that an
// actor that was told to face 90 degrees faces 89.98 and walks a line that drifts by 3e-4 of its length. Where that decides things (to which
// side an actor that walks head on into a box is slid) ScummVM has to do the same. In a sector that is tilted (a ramp, a stair) the frame is
// tilted with it, and the angles between the heading that is seen and the frame are those in the plane of the sector: they differ from the
// angles on the ground.

// The forward vector on the ground, as a unit vector: a frame that is flat gives it as it is, a tilted one has to be shortened
static void headingGround(const float *f, float &x, float &y) {
	x = f[0];
	y = f[1];
	if (fabsf(f[2]) > 1e-4f)
		originalNormalize2(x, y);
}

void Actor::syncMoveYaw() {
	if (_hFwd[0] == 0.f && _hFwd[1] == 0.f)
		return;
	_moveYaw = Math::Angle::arcTangent2(-_hFwd[0], _hFwd[1]);
}

// The frame of an actor that has none yet: the original makes the frame of the walker facing 0 and turns it to the direction the actor
// faces (the visible one) by a relative turn
void Actor::ensureHeading() const {
	if (_headingSet)
		return;
	_headingSet = true;
	_hFwd[0] = 0.f;
	_hFwd[1] = 1.f;
	_hFwd[2] = 0.f;
	_hRight[0] = 1.f;
	_hRight[1] = 0.f;
	_hRight[2] = 0.f;
	_hUp[0] = 0.f;
	_hUp[1] = 0.f;
	_hUp[2] = 1.f;
	_hPlane = false;
	_hSector = nullptr;
	Actor *self = const_cast<Actor *>(this);
	self->faceHeading(_moveYaw);
	if (_followBoxes)
		self->updateHeadingSector();
}

// Walker_RebuildRightVector (FUN_004a8a90): the forward vector normalized, the right vector is forward x up, normalized
void Actor::rebuildHeadingRight() const {
	origNormalize3(_hFwd);
	_hRight[0] = toF((LD)_hFwd[1] * _hUp[2] - (LD)_hUp[1] * _hFwd[2]);
	_hRight[1] = toF((LD)_hFwd[2] * _hUp[0] - (LD)_hFwd[0] * _hUp[2]);
	_hRight[2] = toF((LD)_hFwd[0] * _hUp[1] - (LD)_hFwd[1] * _hUp[0]);
	origNormalize3(_hRight);
}

// FUN_004a7000: the coordinate of a point that is not one of the two axes of the plane of the sector, as the plane has it
void Actor::headingFixPlane(float *p) const {
	const int a = _hAxisA, b = _hAxisB;
	int k;
	if (a == 1 && b == 0)
		k = 2;
	else if (a == 0 && b == 2)
		k = 1;
	else
		k = 0;
	const LD plane = (LD)_hPlaneN[2] * _hPlaneV0[2] + (LD)_hPlaneN[0] * _hPlaneV0[0] + (LD)_hPlaneN[1] * _hPlaneV0[1];
	p[k] = (float)(((LD)_hPlaneN[a] * p[a] + (LD)_hPlaneN[b] * p[b] + -plane) / -(LD)_hPlaneN[k]);
}

// Walker_SetSectorFrame (FUN_004a89b0): the up vector is the normal of the sector, the right vector forward x up (normalized), the forward
// vector up x right (normalized): the heading goes onto the plane of the sector
void Actor::setHeadingSector(const Sector *sector) {
	ensureHeading();
	_hSector = sector;
	if (!sector) {
		_hPlane = false;
		return;
	}
	sector->grimPlane(_hPlaneN, _hPlaneV0, _hAxisA, _hAxisB);
	_hPlane = true;
	_hUp[0] = _hPlaneN[0];
	_hUp[1] = _hPlaneN[1];
	_hUp[2] = _hPlaneN[2];
	_hRight[0] = toF((LD)_hUp[2] * _hFwd[1] - (LD)_hUp[1] * _hFwd[2]);
	_hRight[1] = toF((LD)_hFwd[2] * _hUp[0] - (LD)_hUp[2] * _hFwd[0]);
	_hRight[2] = toF((LD)_hUp[1] * _hFwd[0] - (LD)_hUp[0] * _hFwd[1]);
	origNormalize3(_hRight);
	_hFwd[0] = toF((LD)_hUp[1] * _hRight[2] - (LD)_hUp[2] * _hRight[1]);
	_hFwd[1] = toF((LD)_hRight[0] * _hUp[2] - (LD)_hUp[0] * _hRight[2]);
	_hFwd[2] = toF((LD)_hRight[1] * _hUp[0] - (LD)_hUp[1] * _hRight[0]);
	origNormalize3(_hFwd);
	syncMoveYaw();
}

// FUN_004a82c0: the sector the walker is in (the one that has the position, else the nearest) gives the frame its plane
// FUN_004a82c0: the sector for the place of the walker (the one that has it, else the nearest); a place that is not in it by the test of the sector is moved
// onto it, and the frame takes its plane
void Actor::updateHeadingSector() {
	Set *set = g_grim->getCurrSet();
	if (!set) {
		return;
	}
	Sector *sector = set->grimNearestSector(_pos);
	if (!sector) {
		_hSector = nullptr;
		return;
	}
	float p[3] = { _pos.x(), _pos.y(), _pos.z() };
	if (!sector->grimContains(p, true)) {
		sector->grimClampPoint(p);
		_pos.set(p[0], p[1], p[2]);
	}
	setHeadingSector(sector);
}

void Actor::setHeadingVector(float x, float y, float z) {
	ensureHeading();
	_hFwd[0] = x;
	_hFwd[1] = y;
	_hFwd[2] = z;
	rebuildHeadingRight();
	syncMoveYaw();
}

// FUN_004a83c0: the frame turned by an angle in degrees about its up vector
void Actor::turnHeadingBy(float degrees) {
	ensureHeading();
	float m[12];
	origAxisMatrix(m, _hUp, degrees);
	const float v[3] = { _hFwd[0], _hFwd[1], _hFwd[2] };
	// FUN_0044ffc0: the forward vector with the columns of the matrix
	float f[3];
	f[0] = toF((LD)v[0] * m[0] + (LD)v[1] * m[3] + (LD)v[2] * m[6]);
	f[1] = toF((LD)m[1] * v[0] + (LD)v[1] * m[4] + (LD)v[2] * m[7]);
	f[2] = toF((LD)m[2] * v[0] + (LD)v[2] * m[8] + (LD)m[5] * v[1]);
	// the vector goes through the position of the walker and the plane of the sector, which rounds it to the floats of the position
	const float pos[3] = { _pos.x(), _pos.y(), _pos.z() };
	float q[3];
	for (int i = 0; i < 3; i++)
		q[i] = toF((LD)f[i] + pos[i]);
	if (_hPlane) {
		// FUN_004a6fa0: the point onto the plane of the sector
		const float d = toF(((LD)q[1] - _hPlaneV0[1]) * _hPlaneN[1] + ((LD)q[2] - _hPlaneV0[2]) * _hPlaneN[2] + ((LD)q[0] - _hPlaneV0[0]) * _hPlaneN[0]);
		for (int i = 0; i < 3; i++)
			q[i] = toF((LD)q[i] - (LD)d * _hPlaneN[i]);
	}
	for (int i = 0; i < 3; i++)
		_hFwd[i] = toF((LD)q[i] - pos[i]);
	rebuildHeadingRight();
	Debug::debug(Debug::Actors, "TURNH %s deg %.9g in %.9g %.9g %.9g out %.9g %.9g %.9g", _name.c_str(), degrees, v[0], v[1], v[2], _hFwd[0], _hFwd[1], _hFwd[2]);
	syncMoveYaw();
}

// SetActorRot / TurnActorTo of the original: the frame is turned by the angle that takes its forward vector to the direction of the angle
float Actor::faceHeading(const Math::Angle &yaw, const Math::Angle &pitch, const Math::Angle &roll) {
	ensureHeading();
	// the forward row of the matrix of the three angles (FUN_0044f080) as a point next to the actor, in the frame of the walker (FUN_004a8370: the point
	// minus the position, then the transpose of the frame), flat, normalized
	const float px = _pos.x();
	const float py = _pos.y();
	const float pz = _pos.z();
	float am[12];
	origActorMatrix(am, pitch.getDegrees(), yaw.getDegrees(), roll.getDegrees(), Math::Vector3d());
	float v[3];
	v[0] = (float)((LD)(float)((LD)am[3] + px) - (LD)px);
	v[1] = (float)((LD)(float)((LD)am[4] + py) - (LD)py);
	v[2] = (float)((LD)(float)((LD)am[5] + pz) - (LD)pz);
	const float fr[9] = { _hRight[0], _hRight[1], _hRight[2], _hFwd[0], _hFwd[1], _hFwd[2], _hUp[0], _hUp[1], _hUp[2] };
	float l[3];
	origToLocal(l, v, fr);
	l[2] = 0.f;
	origNormalize3(l);
	float rel = (float)(90.0L - (LD)originalAsinDeg(l[1]));
	const float angle = rel;
	if (l[0] > 0.f)
		rel = -rel;
	turnHeadingBy(rel);
	return angle;
}

// FUN_004a8450: the frame is turned toward a point (put on the plane of the sector first), by the angle to it in the frame or by at most maxAngle
float Actor::turnHeadingToPoint(const Math::Vector3d &target, float maxAngle) {
	ensureHeading();
	float q[3] = { target.x(), target.y(), target.z() };
	if (_hPlane)
		headingFixPlane(q);
	float v[3] = { toF((LD)q[0] - _pos.x()), toF((LD)q[1] - _pos.y()), toF((LD)q[2] - _pos.z()) };
	const float fr[9] = { _hRight[0], _hRight[1], _hRight[2], _hFwd[0], _hFwd[1], _hFwd[2], _hUp[0], _hUp[1], _hUp[2] };
	float l[3];
	origToLocal(l, v, fr);
	origNormalize3(l);
	float rel = (float)(90.0L - (LD)originalAsinDeg(l[1]));
	if (maxAngle < rel)
		rel = maxAngle;
	if (0.f < l[0])
		rel = rel * -1.f;
	turnHeadingBy(rel);
	return rel;
}

// Actor_GetYawToPoint (FUN_004a8500): the angle in the frame of the walker to the point, added to the yaw that the frame has (FUN_0044f650: from its forward vector on
// the ground)
float Actor::grimYawToPoint(const Math::Vector3d &target) {
	ensureHeading();
	float q[3] = { target.x(), target.y(), target.z() };
	if (_hSector && _hPlane)
		headingFixPlane(q);
	const float v[3] = { toF((LD)q[0] - _pos.x()), toF((LD)q[1] - _pos.y()), toF((LD)q[2] - _pos.z()) };
	const float fr[9] = { _hRight[0], _hRight[1], _hRight[2], _hFwd[0], _hFwd[1], _hFwd[2], _hUp[0], _hUp[1], _hUp[2] };
	float l[3];
	origToLocal(l, v, fr);
	origNormalize3(l);
	float rel = (float)(90.0L - (LD)originalAsinDeg(l[1]));
	if (0.f < l[0])
		rel = rel * -1.f;
	const float m3 = _hFwd[0], m4 = _hFwd[1];
	const float hyp = toF(sqrtl((LD)toF((LD)m4 * m4 + (LD)m3 * m3)));
	float yaw = 0.f;
	if (0.001f <= hyp) {
		LD a = 90.0L - (LD)originalAsinDeg(toF((LD)m4 / hyp));
		if (0.f < m3)
			a = -a;
		yaw = (float)a;
	}
	return (float)((LD)yaw + (LD)rel);
}

// FUN_004931f0 (GetSectorOppositeEdge): FUN_004a7390 puts the walker on the opposite edge of a sector of four sides (the place where the line from the walker, 10 m
// along its backward direction, leaves the sector, on the edge two further, at the same distance along it from the other end); Actor_SetPositionClamped then
// takes the place for the actor. The builtin gives no values to the script.
void Actor::grimOppositeEdge(Sector *sector) {
	if (!_headingSet)
		return;
	Math::Vector3d newPos = _pos;
	if (sector->getNumVertices() == 4) {
		const float pos[3] = { _pos.x(), _pos.y(), _pos.z() };
		float target[3];
		for (int i = 0; i < 3; i++)
			target[i] = toF((LD)_hFwd[i] * -10.0f + pos[i]);
		float exitPt[3];
		sector->grimFindExitPoint(pos, target, exitPt);
		const int edge = sector->grimExitEdge();
		if (edge != -1) {
			const int n = sector->getNumVertices();
			const Math::Vector3d *vs = sector->getVertices();
			const Math::Vector3d &v0 = vs[edge];
			const Math::Vector3d &v1 = vs[edge - 1 > -1 ? edge - 1 : n - 1];
			const LD dx = (LD)v1.x() - v0.x();
			LD frac;
			if (fabsl(dx) > (LD)1e-05f)
				frac = ((LD)exitPt[0] - v0.x()) / dx;
			else
				frac = ((LD)exitPt[1] - v0.y()) / ((LD)v1.y() - v0.y());
			frac = 1.0L - frac;
			const int e2 = (edge + 2) % n;
			const Math::Vector3d &a = vs[e2];
			const Math::Vector3d &b = vs[e2 - 1 > -1 ? e2 - 1 : n - 1];
			newPos.set(toF(((LD)b.x() - a.x()) * frac + a.x()), toF(((LD)b.y() - a.y()) * frac + a.y()), toF(((LD)b.z() - a.z()) * frac + a.z()));
		}
	}
	setPos(newPos);
}

// TurnActorTo (FUN_0049fc60): the frame of the walker is turned toward the point by at most the turn rate of the frame (FUN_004a8450), at once; true when that
// was all the turn rate allowed (there may be more to turn), else it is done
bool Actor::grimTurnActorTo(const Math::Vector3d &target) {
	ensureHeading();
	const float maxAngle = toF(origFrameSeconds() * fabsf(_turnRate));
	float rel = turnHeadingToPoint(target, maxAngle);
	if (rel < 0.f)
		rel = -rel;
	return maxAngle <= rel;
}

// Walker_UpdateYaw (FUN_00491230): the forward vector of the heading that is seen (from its three angles) in the frame of the walker, flat: the
// angle between that and the forward vector of the frame is what is left to turn. A turn of less than half a degree is none. Else the heading that
// is seen turns by the turn rate of the frame plus a tenth of the rest (all of it when snap), toward the frame.
int Actor::grimUpdateYaw(bool snap) {
	ensureHeading();
	float m[12];
	origActorMatrix(m, _pitch.getDegrees(), _yaw.getDegrees(), _roll.getDegrees(), _pos);
	const float pos[3] = { _pos.x(), _pos.y(), _pos.z() };
	float v[3];
	for (int i = 0; i < 3; i++)
		v[i] = (float)((LD)(float)((LD)m[3 + i] + pos[i]) - (LD)pos[i]);
	const float fr[9] = { _hRight[0], _hRight[1], _hRight[2], _hFwd[0], _hFwd[1], _hFwd[2], _hUp[0], _hUp[1], _hUp[2] };
	float l[3];
	origToLocal(l, v, fr);
	l[2] = 0.f;
	const float l0pre = l[0], l1pre = l[1];
	origNormalize3(l);
	if (Debug::isChannelEnabled(Debug::Actors)) {
		uint32 bits;
		memcpy(&bits, &l[1], 4);
		Debug::debug(Debug::Actors, "UPDYAW %s l1 %08x yaw %.9g pos %.9g %.9g pre %.9g %.9g v %.9g %.9g %.9g m %.9g %.9g %.9g sum %.9g %.9g %.9g right %.9g %.9g %.9g fwd %.9g %.9g %.9g", _name.c_str(), bits, _yaw.getDegrees(), _pos.x(), _pos.y(), l0pre, l1pre, v[0], v[1], v[2], m[3], m[4], m[5], (float)((LD)m[3] + pos[0]), (float)((LD)m[4] + pos[1]), (float)((LD)m[5] + pos[2]), fr[0], fr[1], fr[2], fr[3], fr[4], fr[5]);
	}
	const LD angle = 90.0L - (LD)originalAsinDeg(l[1]);
	if (angle < 0.5L) {
		_turnLeft = 0.f;
		return 0;
	}
	const float a = (float)angle;
	_turnLeft = a;
	// (the turn rate of the walker is negative after an immediate SetActorRot, until an update gets here: the step of that one is unlimited and makes the rate
	// positive again. An update that finds less than half a degree to turn returns before that, and leaves the rate negative.)
	LD step;
	if (_yawSnap) {
		step = (LD)3.4028235e+38f;
		_yawSnap = false;
	} else {
		// (seconds * time scale * rate, kept in extended precision)
		step = origFrameSeconds() * _timeScale * _turnRate;
	}
	float turn = a;
	if (step < (LD)a)
		turn = (float)(((LD)a - step) * (LD)0.1f + step);
	if (l[0] <= 0.f) {
		_yaw -= turn;
		return -1;
	}
	_yaw += turn;
	return 1;
}

float Actor::moveSine() const {
	if (g_grim->getGameType() != GType_GRIM)
		return _moveYaw.getSine();
	ensureHeading();
	float x, y;
	headingGround(_hFwd, x, y);
	return -x;
}

float Actor::moveCosine() const {
	if (g_grim->getGameType() != GType_GRIM)
		return _moveYaw.getCosine();
	ensureHeading();
	float x, y;
	headingGround(_hFwd, x, y);
	return y;
}

// Actor_SetPositionClamped (FUN_00490870) for a waypoint: the walker is put there, and takes the plane of the sector there
void Actor::placeOnWaypoint(const Math::Vector3d &p) {
	_pos = p;
	if (_followBoxes && _headingSet)
		updateHeadingSector();
}

// FUN_00493230: the walk along the points of a path. A point is reached when the actor is nearer to it than a step and a quarter (the actor is
// put on it then, and the next point is looked at in the same way); the frame is turned toward the next point at once and a step is asked for.
void Actor::updateWalkGrim() {
	const float rate = fabsf(_walkRate);
	const float step = toF(origFrameSeconds() * _timeScale * rate * 1.25L);
	Math::Vector3d wp = _path.back();
	auto nearer = [&](const Math::Vector3d &point) {
		// (the points of a path lie on the plane of the sector of the walker, as the planner of the original makes them; a script's own point
		// can have any height, and the actor stands on the plane)
		Math::Vector3d w = point;
		if (_hPlane) {
			float q[3] = { point.x(), point.y(), point.z() };
			headingFixPlane(q);
			w.set(q[0], q[1], q[2]);
		}
		const float dx = _pos.x() - w.x();
		const float dy = _pos.y() - w.y();
		const float dz = _pos.z() - w.z();
		return (float)sqrtl((LD)dz * dz + (LD)dy * dy + (LD)dx * dx) < step;
	};
	if (nearer(wp)) {
		do {
			_path.pop_back();
			placeOnWaypoint(wp);
			if (_path.empty())
				break;
			wp = _path.back();
		} while (nearer(wp));
		if (_path.empty()) {
			// (the heading that is seen goes on turning to the frame; the last point is where the actor is)
			_walking = false;
			return;
		}
	}
	_walking = true;
	turnHeadingToPoint(wp, 3.4028235e+38f);
	_walkPending = true;
}

void Actor::updateWalk() {
	if (_path.empty()) {
		return;
	}

	if (g_grim->getGameType() == GType_GRIM) {
		updateWalkGrim();
		return;
	}

	Math::Vector3d destPos = _path.back();
	Math::Vector3d dir = destPos - _pos;
	float dist = dir.getMagnitude();

	_walkedCur = true;
	float walkAmt = g_grim->getPerSecond(_walkRate) * _timeScale;
	if (walkAmt >= dist) {
		walkAmt = dist;
		_path.pop_back();
		if (_path.empty()) {
			_walking = false;
			_pos = destPos;
			// It seems that we need to allow an already active turning motion to
			// continue or else turning actors away from barriers won't work right
			_turning = false;
			return;
		}
	}

	if (g_grim->getGameType() == GType_GRIM) {
		Math::Angle y, p, r;
		calculateOrientation(destPos, &p, &y, &r);
		_movePitch = p;
		_moveRoll = r;
		turnHeadingToPoint(destPos, 3.4028235e+38f);
		_turning = _yaw != y || _pitch != p || _roll != r;
	} else {
		turnTo(destPos);
	}

	// The EMI original only moves the actor while the body is within 80 degrees
	// of the walking direction, otherwise it turns on the spot first.
	if (g_grim->getGameType() == GType_MONKEY4) {
		Math::Angle remaining = _moveYaw - _yaw;
		remaining.normalize(-180);
		if (fabsf(remaining.getDegrees()) >= 80.f)
			return;
	}

	dir = destPos - _pos;
	dir.normalize();
	_pos += dir * walkAmt;
}

// FUN_0044fc60: the matrix m times the matrix p (the rows of m turned by p; the position is not needed here)
static void origMatrixMul(float *m, const float *p) {
	const float o[9] = { m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8] };
	for (int r = 0; r < 3; r++) {
		const float *q = o + r * 3;
		m[r * 3] = toF((LD)q[0] * p[0] + (LD)p[6] * q[2] + (LD)p[3] * q[1]);
		m[r * 3 + 1] = toF((LD)p[4] * q[1] + (LD)p[1] * q[0] + (LD)p[7] * q[2]);
		m[r * 3 + 2] = toF((LD)p[8] * q[2] + (LD)p[2] * q[0] + (LD)p[5] * q[1]);
	}
}

// FUN_0044f650: the three angles (pitch, yaw, roll) of the matrix
static void origMatrixAngles(const float *m, float *a) {
	const float m3sq = toF((LD)m[3] * m[3]);
	const float f6 = toF((LD)m[4] * m[4] + m3sq);
	const float f7 = toF(sqrtl((LD)f6));
	const float f8 = -m[1];
	if (0.001f <= f7) {
		LD y = 90.0L - (LD)originalAsinDeg(toF((LD)m[4] / f7));
		if (0.f < m[3])
			y = -y;
		a[1] = toF(y);
	} else {
		LD r = 90.0L - (LD)originalAsinDeg(m[0]);
		if ((0.f < f8 && 0.f < m[5]) || (f8 < 0.f && m[5] < 0.f))
			r = -r;
		a[2] = toF(r);
		a[1] = 0.f;
	}
	if (0.001f <= f7) {
		const float q = toF((LD)f6 / f7);
		a[0] = q < 1.f ? toF(90.0L - (LD)originalAsinDeg(q)) : 0.f;
	} else {
		a[0] = 90.f;
	}
	if (m[5] < 0.f)
		a[0] = -a[0];
	const float f5 = -m[4];
	if (0.001f <= f7) {
		const float q = toF(((LD)f8 * m[3] + (LD)f5 * -m[0]) / sqrtl((LD)f5 * f5 + m3sq));
		if (q < 1.f)
			a[2] = -1.f < q ? toF(90.0L - (LD)originalAsinDeg(q)) : 180.f;
		else
			a[2] = 0.f;
		if (-m[2] < 0.f)
			a[2] = -a[2];
	}
}

// FUN_00490f50: an actor that is oriented to the sector (ActorPuckOrient) turns its up row toward the up vector of the walker, at the turn rate of the
// frame, when they are more than two degrees apart; the three angles are then those of the turned matrix
void Actor::grimPuckOrient() {
	ensureHeading();
	float m[12];
	origActorMatrix(m, _pitch.getDegrees(), _yaw.getDegrees(), _roll.getDegrees(), _pos);
	const float up[3] = { m[6], m[7], m[8] };
	const float n[3] = { _hUp[0], _hUp[1], _hUp[2] };
	const LD dot = (LD)up[0] * n[0] + (LD)up[2] * n[2] + (LD)up[1] * n[1];
	const float rel = toF(90.0L - (LD)originalAsinDeg((float)dot));
	if (!((LD)2.0 < (rel < 0.f ? -(LD)rel : (LD)rel)))
		return;
	const LD maxStep = origFrameSeconds() * _turnRate;
	float step = rel;
	if (0.f < rel && maxStep < (LD)rel)
		step = (float)maxStep;
	if (step < 0.f && (LD)step < -maxStep)
		step = (float)-maxStep;
	float axis[3] = { toF((LD)n[2] * up[1] - (LD)n[1] * up[2]), toF((LD)n[0] * up[2] - (LD)n[2] * up[0]), toF((LD)n[1] * up[0] - (LD)n[0] * up[1]) };
	origNormalize3(axis);
	float p[12];
	origAxisMatrix(p, axis, step);
	origMatrixMul(m, p);
	float a[3] = { _pitch.getDegrees(), _yaw.getDegrees(), _roll.getDegrees() };
	origMatrixAngles(m, a);
	_pitch = Math::Angle(a[0]);
	_yaw = Math::Angle(a[1]);
	_roll = Math::Angle(a[2]);
}

void Actor::update(uint frameTime) {
	_rotPending = false;
	Set *set = g_grim->getCurrSet();

	if (g_grim->getGameType() == GType_MONKEY4) {
		if (_followBoxes) {
			// Check for sort order information in the current sector
			int oldSortOrder = getEffectiveSortOrder();
			Common::Pair<bool, int> sortInfo = set->findSectorSortOrder(_pos, Sector::WalkType);
			_hasSectorSortOrder = sortInfo.first;
			_sectorSortOrder = sortInfo.second;

			if (oldSortOrder != getEffectiveSortOrder())
				g_emi->invalidateSortOrder();
		} else if (_hasSectorSortOrder) {
			_hasSectorSortOrder = false;
			_sectorSortOrder = 0;
			g_emi->invalidateSortOrder();
		}
	}

	if (g_grim->getGameType() == GType_GRIM) {
		// The order of the original (FUN_0048fd90): the walk along a path turns the frame toward the next point and asks for a step, the step is made
		// (Actor_UpdateWalker), then the heading that is seen follows the frame (Walker_UpdateYaw)
		if (_walking)
			updateWalk();
		if (_walkPending) {
			_walkPending = false;
			doWalkForward();
		}
		if (_headingSet) {
			// (the update runs in every frame for an actor that has a walker: what turns the heading that is seen is the frame, not a request)
			// (the original never eases the pitch and the roll toward a target: they stay as they were set, apart from ActorPuckOrient)
			_currTurnDir = grimUpdateYaw(false);
			_turning = _currTurnDir != 0;
		} else {
			_turnLeft = 0.f;
		}
		// (the key of TurnActor is for one frame: FUN_004913f0 clears it at the end of the update)
		_turnKey = 0;
		if (_puckOrient)
			grimPuckOrient();
	} else {
		if (_turning) {
			float turnAmt = g_grim->getPerSecond(_turnRate) * _timeScale;
			_currTurnDir = animTurnEased(turnAmt, _moveYaw, &_yaw);
			_currTurnDir = -_currTurnDir;
			int p = animTurn(turnAmt, _movePitch, &_pitch);
			int r = animTurn(turnAmt, _moveRoll, &_roll);
			if (_currTurnDir == 0 && p == 0 && r == 0) {
				_turning = false;
			}
		}

		if (_walking) {
			updateWalk();
		}
	}

	if (_walkChore.isValid()) {
		if (_walkedCur) {
			if (!_walkChore.isPlaying()) {
				_walkChore.playLooping(true);
			}
			if (g_grim->getGameType() == GType_GRIM && _restChore.isPlaying()) {
				_restChore.stop(true);
			}
		} else {
			if (_walkedLast && _walkChore.isPlaying()) {
				_walkChore.stop(true);
				if (!_restChore.isPlaying()) {
					_restChore.playLooping(true);
				}
			}
		}
	}

	if (_leftTurnChore.isValid()) {
		if (_walkedCur || _walkedLast)
			_currTurnDir = 0;

		if (g_grim->getGameType() == GType_GRIM && _restChore.isValid()) {
			if (_currTurnDir != 0) {
				if (getTurnChore(_currTurnDir)->isPlaying() && _restChore.isPlaying()) {
					_restChore.stop(true, 500);
				}
			} else if (_lastTurnDir != 0) {
				if (!_walkedCur && getTurnChore(_lastTurnDir)->isPlaying()) {
					_restChore.playLooping(true);
				}
			}
		}

		if (_lastTurnDir != 0 && _lastTurnDir != _currTurnDir) {
			getTurnChore(_lastTurnDir)->stop(true);
		}
		if (_currTurnDir != 0 && _currTurnDir != _lastTurnDir) {
			getTurnChore(_currTurnDir)->playLooping(true, 500);
			if (_currTurnDir == 1) {
				// When turning to the left, ensure that the components of the right turn chore
				// are fading out (or stopped).
				// This is necessary because the left turn chore typically contains both the
				// left turn and right turn keyframe components. The above call to playLooping
				// will thus start fading in both of the components, overriding the right turn's
				// fade out that was started before.
				// The left turn chore's keys will eventually stop the right turn keyframe from
				// playing, but the stopping will be instantaneous. To get a smooth transition,
				// we want to keep fading out the right turn. The chore's "stop" key will be
				// ignored when the keyframe is fading out (see KeyframeComponent::stop()).
				_rightTurnChore.stop(true);
			}
		}
	} else {
		_currTurnDir = 0;
	}

	// The rest chore might have been stopped because of a
	// StopActorChore(nil).  Restart it if so.
	if (!_walkedCur && _currTurnDir == 0 && !_restChore.isPlaying()) {
		_restChore.playLooping(g_grim->getGameType() == GType_GRIM);
	}

	_walkedLast = _walkedCur;
	_walkedCur = false;
	_lastTurnDir = _currTurnDir;
	_currTurnDir = 0;

	// Update lip syncing
	if (_lipSync) {
		int posSound;

		// While getPosIn16msTicks will return "-1" to indicate that the
		// sound is no longer playing, it is more appropriate to check first
		if (g_grim->getSpeechMode() != GrimEngine::TextOnly && g_sound->getSoundStatus(_talkSoundName.c_str()))
			posSound = g_sound->getPosIn16msTicks(_talkSoundName.c_str());
		else
			posSound = -1;
		if (posSound != -1) {
			int anim = _lipSync->getAnim(posSound);
			if (_talkAnim != anim) {
				if (anim != -1) {
					if (_talkChore[anim].isValid()) {
						stopMumbleChore();
						if (_talkAnim != -1) {
							_talkChore[_talkAnim].stop(g_grim->getGameType() == GType_MONKEY4, ActionChore::talkFadeTime);
						}
						if (g_grim->getGameType() == GType_GRIM) {
							// Run the stop_talk chore so that it resets the components
							// to the right visibility.
							stopTalking();
						} else {
							// Make sure the talk rest chore isn't playing.
							_talkChore[0].stop();
						}
						_talkAnim = anim;
						_talkChore[_talkAnim].play(g_grim->getGameType() == GType_MONKEY4, ActionChore::talkFadeTime);
					} else if (_mumbleChore.isValid() && !_mumbleChore.isPlaying()) {
						_mumbleChore.playLooping();
						_talkAnim = -1;
					}
				} else {
					stopMumbleChore();
					if (_talkAnim != -1)
						_talkChore[_talkAnim].stop(true, ActionChore::talkFadeTime);

					_talkAnim = 0;
					stopTalking();
				}
			}
		}
	}

	frameTime = (uint)(frameTime * _timeScale);
	for (Common::List<Costume *>::iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
		Costume *c = *i;
		c->setPosRotate(_pos, _pitch, _yaw, _roll);
		int marker = c->update(frameTime);
		if (marker > 0) {
			costumeMarkerCallback(marker);
		}
		if (g_grim->getGameType() == GType_MONKEY4 && c->isChoring(false) == -1) {
			freeCostume(c);
			i = _costumeStack.erase(i);
			--i;
		}
	}

	Costume *c = getCurrentCostume();
	if (c) {
		c->animate();
	}

	if (_lookingMode && _lookAtActor != 0) {
		Actor *actor = Actor::getPool().getObject(_lookAtActor);
		if (actor)
			_lookAtVector = actor->getHeadPos();
	}

	// Only the costume on top moves the head, as the ones below may share its model
	if (c)
		c->moveHead(_lookingMode, _lookAtVector);

}

// Not all the talking actors are in the current set, and so on not all the talking actors
// update() is called. For example, Don when he comes out of his office after reaping Meche.
bool Actor::updateTalk(uint frameTime) {
	if (_talking) {
		// If there's no sound file then we're obviously not talking
		GrimEngine::SpeechMode m = g_grim->getSpeechMode();
		TextObject *textObject = nullptr;
		if (_sayLineText)
			textObject = TextObject::getPool().getObject(_sayLineText);
		if (m == GrimEngine::TextOnly && !textObject) {
			shutUp();
			return false;
		} else if (m != GrimEngine::TextOnly && (_talkSoundName.empty() || !g_sound->getSoundStatus(_talkSoundName.c_str()))) {
			_talkDelay -= frameTime;
			if (_talkDelay <= 0) {
				_talkDelay = 0;
				shutUp();
				return false;
			}
		}
		return true;
	}

	return false;
}

bool Actor::canSee(const Actor *other) const {
	if (g_grim->getGameType() != GType_GRIM || _setName.empty() || other->_setName != _setName)
		return false;

	Costume *costume = getCurrentCostume();
	if (!costume)
		return false;
	int joint = costume->getHeadFirstJoint();
	if (joint < 0 || !costume->getModelNodes())
		return false;

	ModelNode *node = costume->getModelNodes() + joint;
	node->_needsUpdate = true;
	ModelNode *root = node;
	while (root->_parent) {
		root = root->_parent;
		root->_needsUpdate = true;
	}

	Math::Matrix4 matrix;
	matrix.setPosition(_pos);
	matrix.buildFromEuler(_yaw, _pitch, _roll, Math::EO_ZXY);
	root->setMatrix(matrix);
	root->update();

	Math::Vector3d delta = other->_pos - node->_pivotMatrix.getPosition();
	Math::Vector3d direction(delta.x(), delta.y(), 0.0f);
	direction.normalize();
	// The original takes the second axis of the orientation matrix of the actor (so a pitched actor sees less in front of
	// it: the x and y of the axis are scaled by the cosine of the pitch) and the test is on x and y only
	Math::Vector3d forward = getSimplePuckVector();
	float pitchScale = _pitch.getCosine();
	float dot = pitchScale * (forward.x() * direction.x() + forward.y() * direction.y());
	return dot >= cosf(fabsf(costume->getHeadMaxRoll()) * 0.5f * (float)M_PI / 180.0f);
}

bool Actor::getScreenRect(int *x1, int *y1, int *x2, int *y2) {
	if (_costumeStack.empty() || !_visible || !g_grim->getCurrSet() || !isInSet(g_grim->getCurrSet()->getName()))
		return false;

	*x1 = 1000;
	*y1 = 1000;
	*x2 = -1000;
	*y2 = -1000;
	g_driver->startActorDraw(this);
	_costumeStack.back()->getBoundingBox(x1, y1, x2, y2);
	g_driver->finishActorDraw();
	return *x1 != 1000 && *x2 != -1000 && *y1 != 1000 && *y2 != -1000;
}

void Actor::draw() {
	// FIXME: if isAttached(), factor in the joint rotation as well.
	const Math::Vector3d &absPos = getWorldPos();
	if (!_costumeStack.empty()) {
		g_grim->getCurrSet()->setupLights(absPos, _inOverworld);
		if (g_grim->getGameType() == GType_GRIM) {
			Costume *costume = _costumeStack.back();
			drawCostume(costume);
		} else {
			for (Common::List<Costume *>::iterator it = _costumeStack.begin(); it != _costumeStack.end(); ++it) {
				Costume *costume = *it;
				drawCostume(costume);
			}
		}
	}

	if (_mustPlaceText) {
		Common::Point p1, p2;
		if (g_grim->getGameType() == GType_GRIM) {
			if (!_costumeStack.empty()) {
				int x1 = 1000, y1 = 1000, x2 = -1000, y2 = -1000;
				g_driver->startActorDraw(this);
				_costumeStack.back()->getBoundingBox(&x1, &y1, &x2, &y2);
				g_driver->finishActorDraw();
				p1.x = x1;
				p1.y = y1;
				p2.x = x2;
				p2.y = y2;
			}
		} else {
			g_driver->getActorScreenBBox(this, p1, p2);
		}

		TextObject *textObject = TextObject::getPool().getObject(_sayLineText);
		if (textObject) {
			if (p1.x == 1000 || p2.x == -1000 || p2.x == -1000) {
				placeOffscreenText(textObject);
			} else {
				textObject->setX((p1.x + p2.x) / 2);
				textObject->setY(p1.y);
			}
			// Deletes the original text and rebuilds it with the newly placed text
			textObject->reset();
		}
		_mustPlaceText = false;
	}
}

void Actor::drawCostume(Costume *costume) {
	for (int l = 0; l < MAX_SHADOWS; l++) {
		if (!shouldDrawShadow(l))
			continue;
		g_driver->setShadow(&_shadowArray[l]);
		g_driver->setShadowMode();
		g_driver->drawShadowPlanes();
		g_driver->startActorDraw(this);
		costume->draw();
		g_driver->finishActorDraw();
		g_driver->clearShadowMode();
		g_driver->setShadow(nullptr);
	}

	// normal draw actor
	g_driver->startActorDraw(this);
	const bool clipped = g_grim->getGameType() == GType_GRIM && _clipActive != 0 && grimRuleOn("clip_plane");
	if (clipped)
		g_driver->setActorClip(true, _clipNormal, _clipPoint);
	s_drawnActor = this;
	costume->draw();
	s_drawnActor = nullptr;
	if (clipped)
		g_driver->setActorClip(false, _clipNormal, _clipPoint);
	g_driver->finishActorDraw();
}

// SetActorClipPlane: the normal is made a unit vector (FUN_004469a0)
void Actor::setClipPlane(const Math::Vector3d &normal, const Math::Vector3d &point) {
	typedef long double LD;
	float n[3] = { normal.x(), normal.y(), normal.z() };
	const LD len = sqrtl((LD)n[0] * n[0] + (LD)n[2] * n[2] + (LD)n[1] * n[1]);
	if (len != 0.L) {
		const LD inv = 1.L / len;
		n[0] = (float)(inv * n[0]);
		n[1] = (float)(n[1] * inv);
		n[2] = (float)(n[2] * inv);
	}
	_clipNormal.set(n[0], n[1], n[2]);
	_clipPoint = point;
}

// The plane for the item that is hung on the node of SetActorInvClipNode: through the actor, with the way it faces (the second row of its matrix) as the normal
bool Actor::getInvClipPlane(Math::Vector3d &normal, Math::Vector3d &point) const {
	if (_invClipNode < 0 || !grimRuleOn("clip_plane"))
		return false;
	float m[12];
	origActorMatrix(m, _pitch.getDegrees(), _yaw.getDegrees(), _roll.getDegrees(), _pos);
	normal.set(m[3], m[4], m[5]);
	point = _pos;
	return true;
}

void Actor::setShadowPlane(const char *n) {
	assert(_activeShadowSlot != -1);

	_shadowArray[_activeShadowSlot].name = n;

	if (g_grim->getGameType() != GType_GRIM)
		return;
	// (the plane of the sector the shadow is cast on, kept for the side test of SetActorShadowValid: FUN_00493570; the test is off again)
	Set *set = g_grim->getCurrSet();
	Sector *sector = set ? set->getSectorByName(n) : nullptr;
	if (sector) {
		typedef long double LD;
		Shadow &shadow = _shadowArray[_activeShadowSlot];
		float origin[3];
		int axisA, axisB;
		sector->grimPlane(shadow.planeNormal, origin, axisA, axisB);
		shadow.planeDist = (float)-((LD)origin[0] * shadow.planeNormal[0] + (LD)origin[1] * shadow.planeNormal[1] + (LD)origin[2] * shadow.planeNormal[2]);
		shadow.valid = 0;
	}
}

void Actor::addShadowPlane(const char *n, Set *scene, int shadowId) {
	assert(shadowId != -1);

	// This needs to be an exact match, because with a substring search a search for sector
	// "shadow1" would return sector "shadow10" in set st, causing shadows not to be cast
	// on the street.
	Sector *sector = scene->getSectorByName(n);
	if (sector) {
		// Create a copy so we are sure it will not be deleted by the Set destructor
		// behind our back. This is important when Membrillo phones Velasco to tell him
		// Naranja is dead, because the scene changes back and forth few times and so
		// the scenes' sectors are deleted while they are still keeped by the actors.
		Plane p = { scene->getName(), new Sector(*sector) };
		_shadowArray[shadowId].planeList.push_back(p);
		g_grim->flagRefreshShadowMask(true);
	}
}

bool Actor::shouldDrawShadow(int shadowId) {
	Shadow *shadow = &_shadowArray[shadowId];
	if (!shadow->active)
		return false;

	// Don't draw a shadow if the shadow caster and the actor are on different sides
	// of the shadow plane.

	if (shadow->planeList.size() == 0)
		return false;

	if (g_grim->getGameType() == GType_GRIM && grimRuleOn("shadow_valid")) {
		// (the original only looks at the side of the plane the actor stands on, when SetActorShadowValid asked for it: FUN_004936e0)
		if (shadow->valid == 0)
			return true;
		const float f = _pos.y() * shadow->planeNormal[1] + _pos.z() * shadow->planeNormal[2] + _pos.x() * shadow->planeNormal[0] + shadow->planeDist;
		return (shadow->valid != -1 || f <= 0.f) && (shadow->valid != 1 || f >= 0.f);
	}

	Sector *sector = shadow->planeList.front().sector;
	Math::Vector3d n = sector->getNormal();
	Math::Vector3d p = sector->getVertices()[0];
	float d = -(n.x() * p.x() + n.y() * p.y() + n.z() * p.z());

	Math::Vector3d bboxPos, bboxSize;
	getBBoxInfo(bboxPos, bboxSize);
	Math::Vector3d centerOffset = bboxPos + bboxSize * 0.5f;
	p = getPos() + centerOffset;

	bool actorSide = n.x() * p.x() + n.y() * p.y() + n.z() * p.z() + d < 0.f;
	p = shadow->pos;
	bool shadowSide = n.x() * p.x() + n.y() * p.y() + n.z() * p.z() + d < 0.f;

	if (actorSide == shadowSide)
		return true;
	return false;
}

void Actor::addShadowPlane(const char *n) {
	addShadowPlane(n, g_grim->getCurrSet(), _activeShadowSlot);
}

void Actor::setActiveShadow(int shadowId) {
	assert(shadowId >= 0 && shadowId < MAX_SHADOWS);

	_activeShadowSlot = shadowId;
	_shadowArray[_activeShadowSlot].active = true;
}

void Actor::setShadowValid(int valid) {
	_shadowArray[_activeShadowSlot].valid = valid;
	if (valid == -1)
		_shadowArray[_activeShadowSlot].dontNegate = true;
	else
		_shadowArray[_activeShadowSlot].dontNegate = false;
}

void Actor::setActivateShadow(int shadowId, bool state) {
	assert(shadowId >= 0 && shadowId < MAX_SHADOWS);

	_shadowArray[shadowId].active = state;
}

void Actor::setShadowPoint(const Math::Vector3d &p) {
	assert(_activeShadowSlot != -1);

	_shadowArray[_activeShadowSlot].pos = p;
}

void Actor::setShadowColor(const Color &color) {
	assert(_activeShadowSlot != -1);

	_shadowArray[_activeShadowSlot].color = color;
}

void Actor::clearShadowPlanes() {
	for (int i = 0; i < MAX_SHADOWS; i++) {
		clearShadowPlane(i);
	}
}

void Actor::clearShadowPlane(int i) {
	Shadow *shadow = &_shadowArray[i];
	while (!shadow->planeList.empty()) {
		delete shadow->planeList.back().sector;
		shadow->planeList.pop_back();
	}
	shadow->active = false;
	shadow->dontNegate = false;
	shadow->valid = 0;

	g_driver->destroyShadow(shadow);
}

void Actor::putInSet(const Common::String &set) {
	if (_drawnToClean) {
		// actor was frozen and...
		if (set == "") {
			// ...is getting unbound from set.
			// This is done in game scripts to reduce the drawing
			// workload in original engine, and we should not need
			// it here. And implementing off-screen buffers is
			// highly non-trivial. Disobey so it is still drawn.
			_fakeUnbound = true;
			return;
		} else {
			// ...is getting bound to a set. Thaw and continue.
			_drawnToClean = false;
		}
	}
	_fakeUnbound = false;
	// EMI: an actor put into a set gets a puck there (the scripts destroy it when they take the actor out, e.g. the raft of Mysts o' Tyme)
	if (g_grim->getGameType() == GType_MONKEY4 && set != "")
		_puck = true;
	// The set should change immediately, otherwise a very rapid set change
	// for an actor will be recognized incorrectly and the actor will be lost.
	_setName = set;

	g_grim->invalidateActiveActorsList();
}

bool Actor::isDrawableInSet(const Common::String &set) const {
	return _setName == set;
}

bool Actor::isInSet(const Common::String &set) const {
	return !_fakeUnbound && _setName == set;
}

void Actor::freeCostume(Costume *costume) {
	Debug::debug(Debug::Actors, "Freeing costume %s", costume->getFilename().c_str());
	freeCostumeChore(costume, &_restChore);
	freeCostumeChore(costume, &_walkChore);
	freeCostumeChore(costume, &_leftTurnChore);
	freeCostumeChore(costume, &_rightTurnChore);
	freeCostumeChore(costume, &_mumbleChore);
	for (int i = 0; i < 10; i++)
		freeCostumeChore(costume, &_talkChore[i]);
	delete costume;
}

void Actor::freeCostumeChore(const Costume *toFree, ActionChore *chore) {
	if (chore->_costume == toFree) {
		*chore = ActionChore();
	}
}

void Actor::stopTalking() {
	// _talkChore[0] is *_stop_talk
	// Don't playLooping it, or else manny's mouth will flicker when he smokes.
	_talkChore[0].setLastFrame();
}

bool Actor::stopMumbleChore() {
	if (_mumbleChore.isPlaying()) {
		_mumbleChore.stop();
		return true;
	}

	return false;
}

void Actor::setCollisionMode(CollisionMode mode) {
	_collisionMode = mode;
}

void Actor::setCollisionScale(float scale) {
	_collisionScale = scale;
}

Math::Vector3d Actor::handleCollisionTo(const Math::Vector3d &from, const Math::Vector3d &pos) const {
	if (_collisionMode == CollisionOff) {
		return pos;
	}

	Math::Vector3d p = pos;
	Math::Vector3d moveVec = pos - _pos;
	for (Actor *a : Actor::getPool()) {
		if (a != this && a->isInSet(_setName) && a->isVisible()) {
			p = a->getTangentPos(from, p);
			handleCollisionWith(a, _collisionMode, &moveVec);
		}
	}
	return p;
}

Math::Vector3d Actor::getTangentPos(const Math::Vector3d &pos, const Math::Vector3d &dest) const {
	if (_collisionMode == CollisionOff) {
		return dest;
	}
	if (pos.getDistanceTo(dest) < Math::epsilon) {
		return dest;
	}
	Math::Vector3d p;
	float size;
	if (!getSphereInfo(false, size, p))
		return dest;
	Math::Vector2d p1(pos.x(), pos.y());
	Math::Vector2d p2(dest.x(), dest.y());
	if (p1.getDistanceTo(p2) < Math::epsilon) {
		return dest;
	}
	Math::Segment2d segment(p1, p2);

	// TODO: collision with Box
//	if (_collisionMode == CollisionSphere) {
	Math::Vector2d center(p.x(), p.y());

	Math::Vector2d inter;
	float distance = segment.getLine().getDistanceTo(center, &inter);

	if (distance < size && segment.containsPoint(inter)) {
		Math::Vector2d v(inter - center);
		v.normalize();
		v *= size;
		v += center;

		return Math::Vector3d(v.getX(), v.getY(), dest.z());
	}
//	} else {
//	}

	return dest;
}

void Actor::setLocalAlpha(unsigned int vertex, float alpha) {
	if (vertex >= _localAlpha.size()) {
		_localAlpha.resize(MAX(MAX_LOCAL_ALPHA_VERTICES, vertex + 1));
	}
	_localAlpha[vertex] = alpha;
}

void Actor::setLocalAlphaMode(unsigned int vertex, AlphaMode alphaMode) {
	if (vertex >= _localAlphaMode.size()) {
		_localAlphaMode.resize(MAX(MAX_LOCAL_ALPHA_VERTICES, vertex + 1));
	}
	_localAlphaMode[vertex] = alphaMode;
}

bool Actor::hasLocalAlpha() const {
	return !_localAlphaMode.empty();
}

float Actor::getLocalAlpha(unsigned int vertex) const {
	if (vertex < _localAlphaMode.size() && vertex < _localAlpha.size() && _localAlphaMode[vertex] == Actor::AlphaReplace) {
		return _localAlpha[vertex];
	} else {
		return 1.0f;
	}
}

void Actor::getBBoxInfo(Math::Vector3d &bboxPos, Math::Vector3d &bboxSize) const {
	if (g_grim->getGameType() == GType_MONKEY4) {
		EMICostume *costume = static_cast<EMICostume *>(getCurrentCostume());
		if (!costume) {
			bboxPos = Math::Vector3d(0, 0, 0);
			bboxSize = Math::Vector3d(0, 0, 0);
			return;
		}
		EMIModel *model = costume->getEMIModel();
		bboxPos = *model->_center;
		bboxSize = *model->_boxData2 - *model->_boxData;
	} else {
		Model *model = getCurrentCostume()->getModel();
		bboxPos = model->_bboxPos;
		bboxSize = model->_bboxSize;
	}
}

bool Actor::getSphereInfo(bool adjustZ, float &size, Math::Vector3d &p) const {
	if (g_grim->getGameType() == GType_MONKEY4) {
		EMICostume *costume = static_cast<EMICostume *>(getCurrentCostume());
		if (!costume) {
			Debug::warning(Debug::Actors, "Actor::getSphereInfo: actor \"%s\" has no costume", getName().c_str());
			return false;
		}
		EMIModel *model = costume->getEMIModel();
		assert(model);
		p = _pos + *(model->_center);
		// pre-scale factor of 0.8 was guessed by comparing with the original game
		size = model->_radius * _collisionScale * 0.8f;
	} else {
		Model *model = getCurrentCostume()->getModel();
		assert(model);

		// The sphere is centred on the middle of the box of the model, which is turned with the actor.
		Math::Vector2d c(model->_center.x(), model->_center.y());
		c.rotateAround(Math::Vector2d(0.f, 0.f), _yaw);
		p = _pos + Math::Vector3d(c.getX(), c.getY(), model->_center.z());
		size = model->_radius * _collisionScale;
	}
	return true;
}

// The way the original keeps a walking actor (a sphere) out of the box of another one (FUN_004939f0, FUN_00494100, FUN_004922d0),
// with its arithmetic: the floats of the matrices of the two actors (from the angles, by the tables of the original), every
// expression kept in extended precision until it is stored. A head-on move into a face has no side to slide to but the one that the
// last digits of the floats give (the axis of the turn is the cross product of two nearly parallel vectors), so the digits are the
// behaviour.
// FUN_0044eeb0: the matrix of an actor, the three rows of its angles (pitch, yaw, roll) and its position
static void origActorMatrix(float *m, float e0, float e1, float e2, const Math::Vector3d &pos) {
	float s0, c0, s1, c1, s2, c2;
	originalSinCos(e0, s0, c0);
	originalSinCos(e1, s1, c1);
	originalSinCos(e2, s2, c2);
	m[0] = toF(-((LD)s2 * s1) * s0 + (LD)c2 * c1);
	m[1] = toF((LD)s2 * c1 * s0 + (LD)c2 * s1);
	m[2] = toF(-(LD)s2 * c0);
	m[3] = toF(-(LD)s1 * c0);
	m[4] = toF((LD)c1 * c0);
	m[5] = s0;
	m[6] = toF((LD)c2 * s1 * s0 + (LD)s2 * c1);
	m[7] = toF(-(LD)s0 * c2 * c1 + (LD)s2 * s1);
	m[8] = toF((LD)c2 * c0);
	m[9] = pos.x();
	m[10] = pos.y();
	m[11] = pos.z();
}

// FUN_0044fee0 (without the position) and FUN_00450060 / FUN_004500d0 (with it): a vector turned by the matrix
static void origTurn(float *o, const float *v, const float *m, bool withPos) {
	const float x = toF((LD)v[0] * m[0] + (LD)v[1] * m[3] + (LD)v[2] * m[6] + (withPos ? (LD)m[9] : 0.L));
	const float y = toF((LD)m[1] * v[0] + (LD)m[4] * v[1] + (LD)v[2] * m[7] + (withPos ? (LD)m[10] : 0.L));
	const float z = toF((LD)m[2] * v[0] + (LD)m[8] * v[2] + (LD)v[1] * m[5] + (withPos ? (LD)m[11] : 0.L));
	o[0] = x;
	o[1] = y;
	o[2] = z;
}

// FUN_0044ff50: the rows of the matrix with the vector (to the frame of the actor)
static void origToLocal(float *o, const float *v, const float *m) {
	const float x = toF((LD)v[0] * m[0] + (LD)v[1] * m[1] + (LD)v[2] * m[2]);
	const float y = toF((LD)m[3] * v[0] + (LD)m[4] * v[1] + (LD)v[2] * m[5]);
	const float z = toF((LD)m[6] * v[0] + (LD)m[8] * v[2] + (LD)v[1] * m[7]);
	o[0] = x;
	o[1] = y;
	o[2] = z;
}

// FUN_004469a0: the vector divided by its length, which is returned
static LD origNormalize3(float *v) {
	const LD len = sqrtl((LD)v[0] * v[0] + (LD)v[2] * v[2] + (LD)v[1] * v[1]);
	if (len != 0.L) {
		const LD inv = 1.L / len;
		v[0] = toF(inv * v[0]);
		v[1] = toF(v[1] * inv);
		v[2] = toF(v[2] * inv);
	}
	return len;
}

// FUN_0044f210: the matrix of a turn about an axis
static void origAxisMatrix(float *m, const float *axis, float degrees) {
	float s, c;
	originalSinCos(degrees, s, c);
	float d1 = toF(1.L - axis[2]);
	if (fabsf(d1) <= 1e-05f)
		d1 = 0.f;
	if (d1 == 0.f) {
		m[0] = c;
		m[4] = c;
		m[3] = -s;
		m[1] = s;
	} else {
		float d2 = toF((LD)axis[2] - -1.L);
		if (fabsf(d2) <= 1e-05f)
			d2 = 0.f;
		if (d2 != 0.f) {
			const float f6 = toF((LD)axis[0] * axis[0]);
			const float f8 = toF((LD)axis[1] * axis[1]);
			const float f7 = toF((LD)f6 * c);
			const float f9 = toF((1.L - f6) - f8);
			const float f10 = toF((LD)f8 * c);
			const float oc = toF(1.L - c);
			const float f11 = toF((LD)axis[2] * axis[0] * oc);
			const float f12 = toF((LD)axis[1] * axis[2] * oc);
			m[0] = toF(((LD)f7 * f9 + f10) / (1.L - f9) + f6);
			const float t = toF((LD)axis[1] * axis[0] * oc);
			m[9] = 0.f;
			m[10] = 0.f;
			m[4] = toF(((LD)f10 * f9 + f7) / (1.L - f9) + f8);
			m[8] = toF((LD)f10 + f7 + f9);
			m[1] = toF((LD)axis[2] * s + t);
			m[3] = toF((LD)t - (LD)axis[2] * s);
			m[2] = toF((LD)f11 - (LD)axis[1] * s);
			m[5] = toF((LD)f12 + (LD)s * axis[0]);
			m[6] = toF((LD)f11 + (LD)axis[1] * s);
			m[7] = toF((LD)f12 - (LD)s * axis[0]);
			m[11] = 0.f;
			return;
		}
		m[0] = c;
		m[4] = c;
		m[1] = -s;
		m[3] = s;
	}
	m[2] = 0.f;
	m[5] = 0.f;
	m[6] = 0.f;
	m[7] = 0.f;
	m[8] = 1.f;
	m[9] = 0.f;
	m[10] = 0.f;
	m[11] = 0.f;
}

// FUN_00494100: the point where the move came in through a face of the box, and the end of the move along that face. The box is
// bmin[3], bmax[3]; cur is the end of the move, prev its start (both in the frame of the box).
static bool origBoxSlide(const float *cur, const float *prev, const float *bmin, const float *bmax, float *out) {
	float d[3], far[3];
	for (int i = 0; i < 3; i++) {
		d[i] = cur[i] - prev[i];
		far[i] = toF((LD)d[i] * -10.0 + prev[i]);
	}
	float best = 3.4028235e+38f;
	int face = 0;
	float normal[3] = { 0.f, 0.f, 0.f };
	for (int k = 0; k < 6; k++) {
		const int axis = k / 2;
		const bool isMax = (k % 2) == 0;
		const float limit = isMax ? bmax[axis] : bmin[axis];
		const bool beyond = isMax ? ((LD)limit - 0.002 < far[axis]) : ((LD)far[axis] < (LD)limit - -0.002);
		if (!beyond)
			continue;
		const float ad = d[axis] < 0.f ? -d[axis] : d[axis];
		const float t = toF(((LD)limit - cur[axis]) / ad);
		const float at = t < 0.f ? -t : t;
		const float ab = best < 0.f ? -best : best;
		const bool better = face == 0 ? at < 3.4028235e+38f : at < ab;
		if (better) {
			best = isMax ? -t : t;
			face = k + 1;
			normal[0] = normal[1] = normal[2] = 0.f;
			normal[axis] = isMax ? 1.f : -1.f;
		}
	}
	if (face == 0)
		return false;
	for (int i = 0; i < 3; i++)
		out[i] = toF((LD)best * d[i] + cur[i]);
	const float ab = best < 0.f ? -best : best;
	float rest[3];
	for (int i = 0; i < 3; i++)
		rest[i] = toF((LD)d[i] * ab);
	const float len = (float)origNormalize3(rest);
	const LD dot = (LD)rest[0] * normal[0] + (LD)normal[2] * rest[2] + (LD)rest[1] * normal[1];
	const LD asinDot = originalAsinDeg((float)dot);
	if (90.L - asinDot < 90.L)
		return false;
	float axis[3];
	axis[0] = toF((LD)normal[2] * rest[1] - (LD)normal[1] * rest[2]);
	axis[1] = toF((LD)normal[0] * rest[2] - (LD)normal[2] * rest[0]);
	axis[2] = toF((LD)normal[1] * rest[0] - (LD)normal[0] * rest[1]);
	// (a head-on move with no noise at all in the floats has no axis: the original goes on with a zero vector and ends up with a position that
	// is not a number; the actor is put out through the nearest side then)
	if (axis[0] == 0.f && axis[1] == 0.f && axis[2] == 0.f)
		return false;
	origNormalize3(axis);
	float m[12];
	origAxisMatrix(m, axis, toF((LD)toF(90.L - asinDot) - 90.L));
	// FUN_0044ffc0: the vector with the columns of the matrix
	const float x = toF((LD)rest[0] * m[0] + (LD)rest[1] * m[3] + (LD)rest[2] * m[6]);
	const float y = toF((LD)m[1] * rest[0] + (LD)rest[1] * m[4] + (LD)rest[2] * m[7]);
	const float z = toF((LD)m[2] * rest[0] + (LD)rest[2] * m[8] + (LD)m[5] * rest[1]);
	out[0] = toF((LD)len * x + out[0]);
	out[1] = toF((LD)len * y + out[1]);
	out[2] = toF((LD)len * z + out[2]);
	return true;
}

bool Actor::handleBoxCollision(Actor *actor, const Math::Vector3d &p1, float size1, const Math::Vector3d &p2, float size2,
                               Math::Vector3d *vec) const {
	if (!actor->getCurrentCostume() || !getCurrentCostume())
		return false;
	Model *model = actor->getCurrentCostume()->getModel();
	Model *model1 = getCurrentCostume()->getModel();
	if (!model || !model1)
		return false;

	float m1[12], m2[12];
	const Math::Vector3d curPos = _pos + *vec;
	origActorMatrix(m1, _pitch.getDegrees(), _yaw.getDegrees(), _roll.getDegrees(), curPos);
	origActorMatrix(m2, actor->_pitch.getDegrees(), actor->_yaw.getDegrees(), actor->_roll.getDegrees(), actor->_pos);

	// the centres of the two spheres: the middle of the box of each model, turned with its actor
	const float c1m[3] = { model1->_center.x(), model1->_center.y(), model1->_center.z() };
	const float c2m[3] = { model->_center.x(), model->_center.y(), model->_center.z() };
	float off1[3], c2w[3];
	origTurn(off1, c1m, m1, false);
	origTurn(c2w, c2m, m2, true);
	const float cur1[3] = { toF((LD)off1[0] + curPos.x()), toF((LD)off1[1] + curPos.y()), toF((LD)off1[2] + curPos.z()) };
	const float diff[3] = { cur1[0] - c2w[0], cur1[1] - c2w[1], cur1[2] - c2w[2] };
	const float r1 = model1->_radius * _collisionScale;
	const float r2 = model->_radius * actor->_collisionScale;
	const LD dist = sqrtl((LD)diff[2] * diff[2] + (LD)diff[1] * diff[1] + (LD)diff[0] * diff[0]);
	// (the spheres of the two do not touch: the box is not looked at)
	if (!(dist < (LD)toF((LD)r2 + r1)))
		return false;

	const float scale = actor->_collisionScale;
	const Math::Vector3d boxMin = model->_hasFileBox ? model->_fileMin : model->_bboxPos;
	const Math::Vector3d boxMax = model->_hasFileBox ? model->_fileMax : model->_bboxPos + model->_bboxSize;
	const float bmin[3] = { toF((LD)boxMin.x() * scale - r1), toF((LD)boxMin.y() * scale - r1), toF((LD)boxMin.z() * scale - r1) };
	const float bmax[3] = { toF((LD)boxMax.x() * scale + r1), toF((LD)boxMax.y() * scale + r1), toF((LD)boxMax.z() * scale + r1) };

	float loc[3];
	origToLocal(loc, diff, m2);
	for (int i = 0; i < 3; i++)
		loc[i] = loc[i] + c2m[i];
	if (loc[2] <= bmin[2] || loc[1] <= bmin[1] || bmax[1] <= loc[1] || bmax[0] <= loc[0] || loc[0] <= bmin[0] || bmax[2] <= loc[2])
		return false;

	// the start of the move, in the same frame
	const float prev1[3] = { toF((LD)off1[0] + _pos.x()), toF((LD)off1[1] + _pos.y()), toF((LD)off1[2] + _pos.z()) };
	const float pdiff[3] = { prev1[0] - c2w[0], prev1[1] - c2w[1], prev1[2] - c2w[2] };
	float prev[3];
	origToLocal(prev, pdiff, m2);
	for (int i = 0; i < 3; i++)
		prev[i] = prev[i] + c2m[i];

	float res[3];
	if (!origBoxSlide(loc, prev, bmin, bmax, res)) {
		// FUN_004922d0: out through the nearest of the four sides, a centimetre past it
		res[0] = loc[0];
		res[1] = loc[1];
		res[2] = loc[2];
		float best = 3.4028235e+38f;
		float v = fabsf(bmax[1] - loc[1]);
		if (v < best) {
			best = v;
			res[1] = toF(bmax[1] - -0.01);
		}
		v = fabsf(bmin[1] - loc[1]);
		if (v < best) {
			best = v;
			res[1] = toF(bmin[1] - 0.01);
		}
		v = fabsf(bmax[0] - loc[0]);
		if (v < best) {
			best = v;
			res[0] = toF(bmax[0] - -0.01);
			res[1] = loc[1];
		}
		v = fabsf(bmin[0] - loc[0]);
		if (v < best) {
			res[0] = toF(bmin[0] - 0.01);
			res[1] = loc[1];
		}
	}

	// back to the world, and from the centre of the sphere to the position of the actor
	float world[3];
	origTurn(world, res, m2, true);
	const Math::Vector3d newPos(world[0] - off1[0], world[1] - off1[1], world[2] - off1[2]);
	*vec = newPos - _pos;
	collisionHandlerCallback(actor);
	return true;
}

bool Actor::handleCollisionWith(Actor *actor, CollisionMode mode, Math::Vector3d *vec) const {
	if (actor->_collisionMode == CollisionOff || actor == this) {
		return false;
	}

	if (!actor->getCurrentCostume()) {
		return false;
	}

	Math::Vector3d p1, p2;
	float size1, size2;
	// Both spheres are centred on the middle of the box of the model, as the original does (the rocks in lb and bv, and the poles in xb,
	// are the places that tell if this is wrong).
	if (!this->getSphereInfo(false, size1, p1) || !actor->getSphereInfo(false, size2, p2)) {
		return false;
	}

	CollisionMode mode1 = mode;
	CollisionMode mode2 = actor->_collisionMode;

	// (FUN_004939f0: the actor that moves is a sphere whatever its own mode is, only the mode of the other one counts)
	if (g_grim->getGameType() == GType_GRIM)
		mode1 = CollisionSphere;

	if (g_grim->getGameType() == GType_GRIM && mode2 == CollisionBox)
		return handleBoxCollision(actor, p1, size1, p2, size2, vec);

	if (mode1 == CollisionSphere && mode2 == CollisionSphere) {
		Math::Vector3d pos = p1 + *vec;
		float distance = (pos - p2).getMagnitude();
		if (distance < size1 + size2) {
			// Move the destination point so that its distance from the
			// center of the circle is size1+size2.
			Math::Vector3d v = pos - p2;
			v.normalize();
			v *= size1 + size2;
			*vec = v + p2 - p1;

			collisionHandlerCallback(actor);
			return true;
		}
	} else if (mode1 == CollisionBox && mode2 == CollisionBox) {
		warning("Collision between box and box not implemented!");
		return false;
	} else {
		Math::Vector3d bboxSize1, bboxSize2;
		Math::Vector3d bboxPos1, bboxPos2;

		// get bboxSize and bboxPos for the current and the colliding actor
		this->getBBoxInfo(bboxPos1, bboxSize1);
		actor->getBBoxInfo(bboxPos2, bboxSize2);

		Math::Rect2d rect;

		Math::Vector3d bboxPos;
		Math::Vector3d size;
		float scale;
		Math::Vector3d pos;
		Math::Vector3d circlePos;
		Math::Angle yaw;

		Math::Vector2d circle;
		float radius;

		if (mode1 == CollisionBox) {
			pos = p1 + *vec;
			bboxPos = pos + bboxPos1;
			size =  bboxSize1;
			scale = _collisionScale;
			yaw = _yaw;

			circle.setX(p2.x());
			circle.setY(p2.y());
			circlePos = p2;
			radius = size2;
		} else {
			pos = p2;
			bboxPos = p2  + bboxPos2;
			size = bboxSize2;
			scale = actor->_collisionScale;
			yaw = actor->_yaw;

			circle.setX(p1.x() + vec->x());
			circle.setY(p1.y() + vec->y());
			circlePos = p1;
			radius = size1;
		}

		rect._topLeft = Math::Vector2d(bboxPos.x(), bboxPos.y() + size.y());
		rect._topRight = Math::Vector2d(bboxPos.x() + size.x(), bboxPos.y() + size.y());
		rect._bottomLeft = Math::Vector2d(bboxPos.x(), bboxPos.y());
		rect._bottomRight = Math::Vector2d(bboxPos.x() + size.x(), bboxPos.y());

		rect.scale(scale);
		rect.rotateAround(Math::Vector2d(pos.x(), pos.y()), yaw);

		if (rect.intersectsCircle(circle, radius)) {
			Math::Vector2d center = rect.getCenter();
			// Draw a line from the center of the rect to the place the character
			// would go to.
			Math::Vector2d v = circle - center;
			v.normalize();

			Math::Segment2d edge;
			// That line intersects (usually) an edge
			rect.getIntersection(center, v, &edge);
			// Take the perpendicular of that edge
			Math::Line2d perpendicular = edge.getPerpendicular(circle);

			Math::Vector3d point;
			Math::Vector2d p;
			// If that perpendicular intersects the edge
			if (edge.intersectsLine(perpendicular, &p)) {
				Math::Vector2d direction = perpendicular.getDirection();
				direction.normalize();

				// Move from the intersection until we are at a safe distance
				Math::Vector2d point1(p - direction * radius);
				Math::Vector2d point2(p + direction * radius);

				if (center.getDistanceTo(point1) < center.getDistanceTo(point2)) {
					point = point2.toVector3d();
				} else {
					point = point1.toVector3d();
				}
			} else { //if not we're around a corner
				// Find the nearest vertex of the rect
				Math::Vector2d vertex = rect.getTopLeft();
				float distance = vertex.getDistanceTo(circle);

				Math::Vector2d other = rect.getTopRight();
				float otherDist = other.getDistanceTo(circle);
				if (otherDist < distance) {
					distance = otherDist;
					vertex = other;
				}

				other = rect.getBottomLeft();
				otherDist = other.getDistanceTo(circle);
				if (otherDist < distance) {
					distance = otherDist;
					vertex = other;
				}

				other = rect.getBottomRight();
				if (other.getDistanceTo(circle) < distance) {
					vertex = other;
				}

				// and move on a circle around it
				Math::Vector2d dst = circle - vertex;
				dst.normalize();
				dst = dst * radius;
				point = (vertex + dst).toVector3d();
			}

			float z = vec->z();
			*vec = point - circlePos;
			vec->z() = z;
			collisionHandlerCallback(actor);
			return true;
		}
	}

	return false;
}

void Actor::costumeMarkerCallback(int marker) {
	LuaObjects objects;
	objects.add(this);
	objects.add(marker);

	LuaBase::instance()->callback("costumeMarkerHandler", objects);
}

void Actor::collisionHandlerCallback(Actor *other) const {
	LuaObjects objects;
	objects.add(this);
	objects.add(other);

	LuaBase::instance()->callback("collisionHandler", objects);

	LuaObjects objects2;
	objects2.add(other);
	objects2.add(this);
	LuaBase::instance()->callback("collisionHandler", objects2);
}

const Math::Matrix4 Actor::getFinalMatrix() const {
	// Defaults to identity (no rotation)
	Math::Matrix4 m;

	// Add the rotation and translation from the parent actor, if this actor is attached
	if (isAttached()) {
		Actor *attachedActor = Actor::getPool().getObject(_attachedActor);
		m = attachedActor->getFinalMatrix();

		// If this actor is attached to a joint, add that rotation
		EMICostume *cost = static_cast<EMICostume *>(attachedActor->getCurrentCostume());
		if (cost && cost->_emiSkel && cost->_emiSkel->_obj) {
			Joint *j = cost->_emiSkel->_obj->getJointNamed(_attachedJoint);
			m = m * j->_finalMatrix;
		}
	}

	// Translate by this actor's position
	Math::Vector3d pp = getPos();
	Math::Matrix4 t;
	t.setToIdentity();
	t.setPosition(pp);
	m = m * t;

	// Scaling could be applied here as follows:
	//
	// const float &scale = getScale();
	// t.setToIdentity();
	// t.setValue(3, 3, 1.0 / scale);
	// m = m * t;
	//
	// However, actor's _scale is only changed via the lua call SetActorScale
	// which is not used in EMI. Actor::getFinalMatrix() is only used for EMI
	// so the additional scaling can be omitted.

	// Finish with this actor's rotation
	Math::Matrix4 rotMat(getRoll(), getYaw(), getPitch(), Math::EO_ZYX);
	m = m * rotMat;
	return  m;
}

Math::Vector3d Actor::getWorldPos() const {
	if (! isAttached())
		return getPos();

	return getFinalMatrix().getPosition();
}

Math::Quaternion Actor::getRotationQuat() const {
	if (g_grim->getGameType() == GType_MONKEY4) {
		const Math::Matrix4 m = getFinalMatrix();
		return Math::Quaternion(m).inverse();
	} else {
		return Math::Quaternion::fromEuler(_yaw, _pitch, _roll, Math::EO_ZXY).inverse();
	}
}

Math::Vector3d Actor::getHeadPos() const {
	if (g_grim->getGameType() == GType_GRIM) {
		for (Common::List<Costume *>::const_iterator i = _costumeStack.begin(); i != _costumeStack.end(); ++i) {
			int headJoint = (*i)->getHeadJoint();
			if (headJoint == -1)
				continue;

			ModelNode *allNodes = (*i)->getModelNodes();
			ModelNode *node = allNodes + headJoint;

			node->_needsUpdate = true;
			ModelNode *root = node;
			while (root->_parent) {
				root = root->_parent;
				root->_needsUpdate = true;
			}

			Math::Matrix4 matrix;
			matrix.setPosition(_pos);
			matrix.buildFromEuler(_yaw, _pitch, _roll, Math::EO_ZXY);
			root->setMatrix(matrix);
			root->update();

			return node->_pivotMatrix.getPosition();
		}
	}

	return getWorldPos();
}

void Actor::setSortOrder(const int order) {
	_sortOrder = order;

	// If this actor is attached to another actor, we'll use the
	// explicitly specified sort order instead of the parent actor's
	// sort order from now on.
	if (_useParentSortOrder)
		_useParentSortOrder = false;
}

int Actor::getSortOrder() const {
	return _sortOrder;
}

int Actor::getEffectiveSortOrder() const {
	if (_useParentSortOrder && _attachedActor != 0) {
		Actor *attachedActor = Actor::getPool().getObject(_attachedActor);
		return attachedActor->getEffectiveSortOrder();
	}
	return _hasSectorSortOrder ? _sectorSortOrder : getSortOrder();
}

void Actor::activateShadow(bool active, const char *shadowName) {
	Set *set = g_grim->getCurrSet();
	if (!set) {
		warning("Actor %s trying to activate shadow to null Set", getName().c_str());
		return;
	}
	if (!shadowName) {
		for (int i = 0; i < set->getShadowCount(); ++i) {
			activateShadow(active, set->getShadow(i));
		}
	} else {
		SetShadow *shadow = set->getShadowByName(shadowName);
		if (shadow)
			activateShadow(active, shadow);
	}
}

void Actor::activateShadow(bool active, SetShadow *setShadow) {
	int shadowId = -1;
	for (int i = 0; i < MAX_SHADOWS; i++) {
		if (setShadow->_name.equals(_shadowArray[i].name)) {
			shadowId = i;
			break;
		}
	}

	if (shadowId == -1) {
		for (int i = 0; i < MAX_SHADOWS; i++) {
			if (!_shadowArray[i].active) {
				shadowId = i;
				break;
			}
		}
	}

	if (shadowId == -1) {
		warning("Actor %s trying to activate shadow %s, but all shadow slots are in use", getName().c_str(), setShadow->_name.c_str());
		return;
	}

	clearShadowPlane(shadowId);
	setActivateShadow(shadowId, active);

	if (active) {
		setActiveShadow(shadowId);
		setShadowPoint(setShadow->_shadowPoint);
		setShadowPlane(setShadow->_name.c_str());
		setShadowColor(setShadow->_color);
		_shadowArray[_activeShadowSlot].dontNegate = true; // Don't negate the normal.

		Common::List<Common::String>::iterator it;
		for (it = setShadow->_sectorNames.begin(); it != setShadow->_sectorNames.end(); ++it) {
			addShadowPlane(it->c_str(), g_grim->getCurrSet(), shadowId);
		}
	}
}

Joint *Actor::getAttachedJoint(Actor *parent) const {
	EMICostume *cost = static_cast<EMICostume *>(parent->getCurrentCostume());

	if (!cost || !cost->_emiSkel || !cost->_emiSkel->_obj)
		return nullptr;

	assert(cost->_emiSkel->_obj->hasJoint(_attachedJoint));
	return cost->_emiSkel->_obj->getJointNamed(_attachedJoint);
}

void Actor::attachToActor(Actor *parent, const char *joint) {
	assert(parent != nullptr);
	// No need to attach if we're already attached to this parent
	if (parent->getId() == _attachedActor)
		return;
	// If we're attached to a different parent, detach first
	if (_attachedActor != 0)
		detach();

	// Find the new position coordinates
	Math::Matrix4 parentMatrix = parent->getFinalMatrix();

	// If the parent has a skeleton and the requested joint exists, use the joint transform.
	// Some models (pile o' boulders) don't have a skeleton.
	Common::String jointStr = joint ? joint : "";
	const Joint *attachedJoint = getAttachedJoint(parent);

	Math::Quaternion newRot;

	// A joint attachment already provides the final joint orientation calculated by
	// Skeleton::commitAnim(). Use it directly instead of applying an additional
	// quaternion adjustment, which can introduce an incorrect rotation.
	if (attachedJoint) {
		// Use the joint's final calculated orientation as the attached object's starting rotation.
		newRot = attachedJoint->_finalQuat;

		// Convert position into the joint's local space.
		_pos = _pos - attachedJoint->_finalMatrix.getPosition();

		Math::Matrix4 jointMatrix = attachedJoint->_finalMatrix;
		jointMatrix.transpose();
		jointMatrix.transform(&_pos, true);
	} else {
		// Otherwise, use the actor's rotation relative to its parent.
		newRot = getRotationQuat().inverse() * parent->getRotationQuat();
	}

	// Get the final rotation Euler coordinates
	newRot.getEuler(&_roll, &_yaw, &_pitch, Math::EO_ZYX);

	// Convert position into the parent actor's local space.
	_pos = _pos - parentMatrix.getPosition();
	parentMatrix.transpose();
	parentMatrix.transform(&_pos, true);

	// Save the attachment info
	_attachedActor = parent->getId();
	_attachedJoint = jointStr;

	// Use the parent actor's sort order.
	_useParentSortOrder = true;
}

void Actor::detach() {
	if (!isAttached())
		return;

	// Replace our sort order with the parent actor's sort order. Note
	// that we do this even if a sort order was explicitly specified for
	// the actor during the time it was attached (in which case the actor
	// doesn't respect the parent actor's sort order). This seems weird,
	// but matches the behavior of the original engine.
	Actor *attachedActor = Actor::getPool().getObject(_attachedActor);
	_sortOrder = attachedActor->getEffectiveSortOrder();
	_useParentSortOrder = false;

	// Position and rotate the actor in relation to the world coords
	setPos(getWorldPos());
	Math::Quaternion q = getRotationQuat();
	q.inverse().getEuler(&_roll, &_yaw, &_pitch, Math::EO_ZYX);

	// Remove the attached actor
	_attachedActor = 0;
	_attachedJoint = "";
}

void Actor::drawToCleanBuffer() {
	_drawnToClean = true;
}

MaterialPtr Actor::findMaterial(const Common::String &name) {
	Common::String fixedName = g_resourceloader->fixFilename(name, false);
	Common::List<MaterialPtr>::iterator it = _materials.begin();
	for (; it != _materials.end(); ++it) {
		if (*it) {
			if ((*it)->getFilename() == fixedName) {
				return *it;
			}
		} else {
			it = _materials.erase(it);
			--it;
		}
	}
	return (MaterialPtr)nullptr;
}

MaterialPtr Actor::loadMaterial(const Common::String &name, bool clamp) {
	MaterialPtr mat = findMaterial(name);
	if (!mat) {
		mat = g_resourceloader->loadMaterial(name.c_str(), nullptr, clamp);
		// Note: We store a weak reference.
		_materials.push_back(mat);
		mat->dereference();
	}
	return mat;
}

unsigned const int Actor::ActionChore::fadeTime = 150;
unsigned const int Actor::ActionChore::talkFadeTime = 50;

Actor::ActionChore::ActionChore() :
	_costume(nullptr),
	_chore(-1) {
}

Actor::ActionChore::ActionChore(Costume *cost, int chore) :
	_costume(cost),
	_chore(chore) {
}

void Actor::ActionChore::play(bool fade, unsigned int time) {
	if (isValid()) {
		if (fade) {
			_costume->playChore(_chore, time);
		} else {
			_costume->playChore(_chore);
		}
	}
}

void Actor::ActionChore::playLooping(bool fade, unsigned int time) {
	if (isValid()) {
		if (fade) {
			_costume->playChoreLooping(_chore, time);
		} else {
			_costume->playChoreLooping(_chore);
		}
	}
}

void Actor::ActionChore::stop(bool fade, unsigned int time) {
	if (isValid()) {
		if (fade) {
			_costume->stopChore(_chore, time);
		} else {
			_costume->stopChore(_chore);
		}
	}
}

void Actor::ActionChore::setLastFrame() {
	if (isValid()) {
		_costume->setChoreLastFrame(_chore);
	}
}

bool Actor::ActionChore::isPlaying() const {
	return (isValid() && _costume->isChoring(_chore, false) >= 0);
}

void Actor::ActionChore::saveState(SaveGame *savedState) const {
	if (_costume) {
		savedState->writeBool(true);
		savedState->writeString(_costume->getFilename());
	} else {
		savedState->writeBool(false);
	}
	savedState->writeLESint32(_chore);
}

void Actor::ActionChore::restoreState(SaveGame *savedState, Actor *actor) {
	if (savedState->readBool()) {
		Common::String fname = savedState->readString();
		_costume = actor->findCostume(fname);
	} else {
		_costume = nullptr;
	}
	_chore = savedState->readLESint32();
}

} // end of namespace Grim
