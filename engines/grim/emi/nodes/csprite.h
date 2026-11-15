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

#ifndef GRIM_EMI_CSPRITE_H
#define GRIM_EMI_CSPRITE_H

#include "engines/grim/emi/graphics/cmaterial.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/math/cvector.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/nodes/rprimitive.h"
#include "engines/grim/emi/nodes/rprimitiveinstance.h"

namespace Grim {

class Camera;
class EmiEngine;
class RNode;

class CSprite : public RPrimitive {
public:
	enum {
		kNumCorners = 4
	};

	explicit CSprite(EmiEngine *engine);
	virtual ~CSprite();

	virtual RPrimitiveInstance *createInstance();
	virtual bool restore(LFile *file);

	bool loadBinary(const char *path);
	bool saveBinary(const char *path);

	EmiEngine *getEngine() const;

	LResourceHandle<CMaterial> _material;
	float _width;
	float _height;
	CVector _offset;
	Color _corners[kNumCorners];
	float _uv[kNumCorners * 2];
	int _drawFlags;

protected:
	EmiEngine *_engine;
};

class CSpriteInstance : public RPrimitiveInstance {
public:
	CSpriteInstance(CSprite *spritePrimitive);
	virtual ~CSpriteInstance();

	virtual void output(RNode *node, Camera *camera);
	virtual int getSaveSize();
	virtual bool saveState(SaveRestoreAccess *accessFunc);
	virtual bool restoreState(SaveRestoreAccess *accessFunc);

	int _textureIndex;

private:
	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
