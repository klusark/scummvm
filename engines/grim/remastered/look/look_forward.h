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

#ifndef GRIM_REMASTERED_LOOK_LOOK_FORWARD_H
#define GRIM_REMASTERED_LOOK_LOOK_FORWARD_H

#include "common/scummsys.h"

#include "engines/grim/remastered/look/look_device.h"
#include "engines/grim/remastered/look/look_lightprobe.h"
#include "engines/grim/remastered/look/look_scene.h"

namespace Grim {

class RemasteredLookPipeline;

class RemasteredLookForward {
public:
	explicit RemasteredLookForward(RemasteredLookPipeline *pipeline);

	void setScene(RemasteredLookScene *scene);
	bool isActive() const { return _active; }
	RemasteredLookLightprobe *getLightprobe() { return &_lightprobe; }

	void beginFrame();
	void postCameraChange();
	void drawSubmesh(RemasteredLookBuffer *vertices, RemasteredLookTexture *texture, const float *matrix12, float red, float green, float blue);
	void renderScene();

private:
	void updateFlag();
	void flush();
	bool sceneShadows();
	void resolve(RemasteredLookTarget *target, RemasteredLookTarget *depth);

	RemasteredLookPipeline *_pipeline;
	RemasteredLookScene *_scene;
	RemasteredLookLightprobe _lightprobe;
	bool _active;
	bool _missingLatticeReported;
};

} // End of namespace Grim

#endif
