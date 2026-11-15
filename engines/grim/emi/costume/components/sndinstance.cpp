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

#include "common/textconsole.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/costume/components/sndinstance.h"
#include "engines/grim/emi/sound/sounds.h"

namespace Grim {

SndInstance::SndInstance(EmiEngine *engine, const LString &componentName, Actor *actor) : ComponentInstance(engine, componentName) {
	_engine = engine;
	_owner = actor;
	_soundHandle = 0;

	if (componentName.size() >= Component::kNamePrefixSize)
		_soundHandle = _engine->getSounds()->loadSound(componentName.c_str() + Component::kNamePrefixSize, 1);
	else
		warning("SndInstance::SndInstance(): invalid component name %s", componentName.c_str());
}

SndInstance::~SndInstance() {
	_engine->getSounds()->stopSound(_soundHandle);
}

bool SndInstance::isReplayable() {
	return false;
}

void SndInstance::setKey(float value, float elapsed, float nextValue, float remaining, bool partial) {
	if (!_soundHandle)
		return;

	Sounds *sounds = _engine->getSounds();
	bool loop = false;

	switch ((int)value) {
	case 0:
		sounds->stopSoundPlayback(_soundHandle);
		break;
	case 1:
		loop = true;
		// Fall through
	case 2:
		if (sounds->isSoundPaused(_soundHandle)) {
			sounds->resumeSound(_soundHandle);
		} else {
			bool isPlaying;

			if (_engine->getGamePlatform() == Common::kPlatformPS2)
				isPlaying = sounds->isSoundPlayingPs2(_soundHandle, "a costume sound");
			else
				isPlaying = sounds->isSoundPlaying(_soundHandle);

			if (!isPlaying)
				sounds->startSound(_soundHandle, 0, loop, 100);
		}

		break;
	case 3:
		sounds->pauseSound(_soundHandle);
		break;
	default:
		break;
	}
}

} // End of namespace Grim
