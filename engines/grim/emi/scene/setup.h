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

#ifndef GRIM_EMI_SETUP_H
#define GRIM_EMI_SETUP_H

#include "common/list.h"

#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/layer.h"
#include "engines/grim/emi/files/lfile.h"
#include "engines/grim/lua/lua.h"

namespace Grim {

class EmiEngine;

class Setup {
public:
	enum {
		kNameSize = 128
	};

	Setup(EmiEngine *engine);
	~Setup();

	void *operator new(size_t size);
	void operator delete(void *block);
	void *operator new(size_t size, void *where);
	void operator delete(void *block, void *where);
	void *operator new[](size_t size);
	void operator delete[](void *block);

	static Setup *newArray(EmiEngine *engine, int count);
	static void deleteArray(Setup *array);

	void buildLayers();
	int getSize();
	int addLayer(const char *layerName, int layerSortOrder, int layerFrame, int layerId);
	void removeLayer(Layer *layer);
	void removeLayerById(int layerId);
	bool save(SaveRestoreAccess *accessFunc);
	bool restore(SaveRestoreAccess *accessFunc);

	char _name[kNameSize];
	Camera _camera;
	Common::List<Layer *> _layers;
	LString _backgroundName;
	bool _layersBuilt;

private:
	enum {
		kArrayHeaderSize = 16
	};

	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
