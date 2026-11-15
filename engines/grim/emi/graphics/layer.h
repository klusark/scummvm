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

#ifndef GRIM_EMI_LAYER_H
#define GRIM_EMI_LAYER_H

#include "engines/grim/emi/graphics/background.h"
#include "engines/grim/emi/resources/lresourcehandle.h"
#include "engines/grim/emi/graphics/tiledscene.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class CDrawContext;
class EmiEngine;

class Layer {
public:
	enum {
		kNameSize = 128
	};

	Layer(EmiEngine *engine, const char *layerName, int layerSortOrder, int layerFrame, int layerId);
	~Layer();

	void *operator new(size_t size);
	void operator delete(void *block);

	int draw(CDrawContext *context);

	static void set16BitFix(EmiEngine *engine, bool enable);
	static void saveRestoreNextId(EmiEngine *engine, SaveRestoreAccess *accessFunc);

	int _id;
	int _sortOrder;
	int _frame;
	char _name[kNameSize];
	LResourceHandle<TiledScene> _tiledScene;
	LResourceHandle<Background> _background;

private:
	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
