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

#ifndef GRIM_EMI_RPRIMITIVEINSTANCE_H
#define GRIM_EMI_RPRIMITIVEINSTANCE_H

#include "common/hashmap.h"

#include "engines/grim/emi/nodes/rprimitive.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class Camera;
class RNode;

class RPrimitiveInstance {
public:
	enum {
		kAlphaOff = 0,
		kAlphaSet = 1,
		kAlphaModulate = 2
	};

	RPrimitiveInstance(RPrimitive *primitive);
	virtual ~RPrimitiveInstance();

	void *operator new(size_t size);
	void operator delete(void *block);

	virtual void output(RNode *node, Camera *camera) {}
	virtual void update(RNode *node, Camera *camera) {}
	virtual int getSaveSize();
	virtual bool saveState(SaveRestoreAccess *accessFunc);
	virtual bool restoreState(SaveRestoreAccess *accessFunc);
	virtual bool isVisible(RNode *node, Camera *camera);
	virtual void drawBounds(RNode *node, Camera *camera);

	RPrimitive *_prim;
	bool _visible;
	bool _stateRestored;
	int _globalAlphaMode;
	float _globalAlpha;
	int _matAlphaMode;
	Common::HashMap<int, float> _matAlpha;
};

} // End of namespace Grim

#endif
