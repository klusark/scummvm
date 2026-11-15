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
#include "engines/grim/emi/math/irect.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/scene/setup.h"
#include "engines/grim/emi/graphics/tiledscene.h"

namespace Grim {

static const int kViewportWidth = 640;
static const int kViewportHeight = 480;
static const float kViewportFov = 90.0f;
static const int kLayerSortStep = 10;

Setup::Setup(EmiEngine *engine) : _camera(engine->getDrawContext()), _engine(engine) {
	_name[0] = 0;
	_layersBuilt = false;

	IRect viewport;

	viewport.x = 0;
	viewport.y = 0;
	viewport.w = kViewportWidth;
	viewport.h = kViewportHeight;
	_camera.setViewport(&viewport, kViewportFov);
}

Setup::~Setup() {
	while (!_layers.empty())
		removeLayer(_layers.front());

	_backgroundName.clear();
	_layers.clear();
}

void *Setup::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void Setup::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

void *Setup::operator new(size_t, void *where) {
	return where;
}

void Setup::operator delete(void *, void *) {
}

void *Setup::operator new[](size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void Setup::operator delete[](void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

Setup *Setup::newArray(EmiEngine *engine, int count) {
	byte *block = static_cast<byte *>(Setup::operator new[](kArrayHeaderSize + (size_t)count * sizeof(Setup)));
	Setup *array = reinterpret_cast<Setup *>(block + kArrayHeaderSize);

	*reinterpret_cast<int *>(block) = count;

	for (int i = 0; i < count; i++)
		new (&array[i]) Setup(engine);

	return array;
}

void Setup::deleteArray(Setup *array) {
	byte *block = reinterpret_cast<byte *>(array) - kArrayHeaderSize;
	int count = *reinterpret_cast<int *>(block);

	for (int i = count; i > 0; i--)
		array[i - 1].~Setup();

	Setup::operator delete[](block);
}

void Setup::buildLayers() {
	if (_layersBuilt)
		return;

	Layer *firstLayer = new Layer(_engine, _backgroundName.c_str(), 0, 0, -1);
	TiledScene *scene = nullptr;

	if (firstLayer->_tiledScene.lock())
		scene = firstLayer->_tiledScene._typed;

	if (scene) {
		int frame = 0;
		int layerSortOrder = 0;

		while (frame < scene->getFrameCount()) {
			addLayer(_backgroundName.c_str(), layerSortOrder, frame, -1);
			frame++;
			layerSortOrder += kLayerSortStep;
		}
	} else {
		warning("Setup::buildLayers(): the tiled scene is not available");
	}

	firstLayer->_tiledScene.unlock();
	delete firstLayer;
	_layersBuilt = true;
}

int Setup::getSize() {
	return (int)(sizeof(Setup) + _layers.size() * sizeof(Layer *));
}

int Setup::addLayer(const char *layerName, int layerSortOrder, int layerFrame, int layerId) {
	Layer *layer = new Layer(_engine, layerName, layerSortOrder, layerFrame, layerId);

	_layers.push_back(layer);

	return layer->_id;
}

void Setup::removeLayer(Layer *layer) {
	Common::List<Layer *>::iterator it = _layers.begin();

	while (it != _layers.end()) {
		if (*it == layer)
			it = _layers.erase(it);
		else
			++it;
	}

	if (layer) {
		delete layer;
		layer = nullptr;
	}
}

void Setup::removeLayerById(int layerId) {
	Common::List<Layer *>::iterator it;

	for (it = _layers.begin(); it != _layers.end(); ++it) {
		if ((*it)->_id == layerId) {
			removeLayer(*it);

			return;
		}
	}
}

bool Setup::save(SaveRestoreAccess *accessFunc) {
	accessFunc(&_layersBuilt, sizeof(_layersBuilt));

	int count = (int)_layers.size();

	accessFunc(&count, sizeof(count));

	Common::List<Layer *>::iterator it;

	for (it = _layers.begin(); it != _layers.end(); ++it) {
		Layer *layer = *it;
		int value = layer->_id;

		accessFunc(&value, sizeof(value));
		value = layer->_sortOrder;
		accessFunc(&value, sizeof(value));
		value = layer->_frame;
		accessFunc(&value, sizeof(value));

		int length = (int)strlen(layer->_name) + 1;

		accessFunc(&length, sizeof(length));
		accessFunc(layer->_name, length);
	}

	return true;
}

bool Setup::restore(SaveRestoreAccess *accessFunc) {
	Common::List<Layer *>::iterator it;

	for (it = _layers.begin(); it != _layers.end(); ++it) {
		delete *it;
		*it = nullptr;
	}

	_layers.clear();

	byte built = 0;
	int count = 0;

	accessFunc(&built, sizeof(built));
	_layersBuilt = built != 0;
	accessFunc(&count, sizeof(count));

	for (int i = 0; i < count; i++) {
		char layerName[kNameSize];
		int layerId = 0;
		int layerSortOrder = 0;
		int layerFrame = 0;
		int length = 0;

		accessFunc(&layerId, sizeof(layerId));
		accessFunc(&layerSortOrder, sizeof(layerSortOrder));
		accessFunc(&layerFrame, sizeof(layerFrame));
		accessFunc(&length, sizeof(length));

		layerName[0] = 0;

		if (length > kNameSize) {
			warning("Setup::restore(): the layer name is too long");
			accessFunc(layerName, kNameSize);

			for (int skipped = kNameSize; skipped < length; skipped++) {
				char discard;

				accessFunc(&discard, sizeof(discard));
			}
		} else if (length > 0) {
			accessFunc(layerName, length);
		}

		layerName[kNameSize - 1] = 0;
		addLayer(layerName, layerSortOrder, layerFrame, layerId);
	}

	return true;
}

} // End of namespace Grim
