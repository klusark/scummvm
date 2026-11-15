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

#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/graphics/layer.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/resources/lresource.h"

namespace Grim {

static const int kQuadsFlags = 0x10001;
static const int kPs2QuadsFlags = 0x20001;

void Layer::set16BitFix(EmiEngine *engine, bool enable) {
	engine->_layer16BitFix = enable;
}

void Layer::saveRestoreNextId(EmiEngine *engine, SaveRestoreAccess *accessFunc) {
	accessFunc(&engine->_nextLayerId, sizeof(engine->_nextLayerId));
}

Layer::Layer(EmiEngine *engine, const char *layerName, int layerSortOrder, int layerFrame, int layerId) : _tiledScene(engine), _background(engine), _engine(engine) {
	if (layerId == -1) {
		_id = _engine->_nextLayerId;
		_engine->_nextLayerId++;
	} else {
		_id = layerId;
	}

	_frame = 0;
	_sortOrder = 0;

	if (strlen(layerName) >= kNameSize)
		warning("Layer::Layer(): the name is too long");

	Common::strlcpy(_name, layerName, kNameSize);
	_sortOrder = layerSortOrder;
	_frame = layerFrame;

	LString resourceName = LString::format("BG%s", layerName);

	_background._resource = _engine->getResourceManager()->findResource(resourceName.c_str());

	if (!_background._resource) {
		_background._resource = _engine->getResourceManager()->createResource(resourceName.c_str());

		if (!_background._resource)
			_background.allocateResource();

		if (_background._resource) {
			_background._resource->resSetName(resourceName.c_str());
			_engine->getResourceManager()->addResource(_background._resource);
		}
	}

	_background.setResource(_background._resource);

	resourceName = LString::format("TS%s", layerName);

	_tiledScene._resource = _engine->getResourceManager()->findResource(resourceName.c_str());

	if (!_tiledScene._resource) {
		_tiledScene._resource = _engine->getResourceManager()->createResource(resourceName.c_str());

		if (!_tiledScene._resource)
			_tiledScene.allocateResource();

		if (_tiledScene._resource) {
			_tiledScene._resource->resSetName(resourceName.c_str());
			_engine->getResourceManager()->addResource(_tiledScene._resource);
		}
	}

	_tiledScene.setResource(_tiledScene._resource);
}

Layer::~Layer() {
}

void *Layer::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void Layer::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

int Layer::draw(CDrawContext *context) {
	_engine->_layerDrawTimer.start();

	Background *backgroundData = nullptr;

	if (_background.lock())
		backgroundData = _background._typed;

	if (_tiledScene.lock()) {
		TiledScene *scene = _tiledScene._typed;

		if (scene) {
			context->setOrtho(-1.0f, 1.0f, -1.0f, 1.0f);

			int frameCount = scene->getFrameCount();

			if (frameCount > 0) {
				// The disasm wraps the frame in place
				while (_frame < 0)
					_frame += frameCount;

				int frameIndex = _frame % frameCount;
				int firstBatch = scene->_frames[2 * frameIndex];
				int batchCount = scene->_frames[2 * frameIndex + 1];

				if (batchCount > 0 && !backgroundData) {
					warning("Layer::draw(): the background is not available");
				} else if (batchCount > 0) {
					// PS2 alpha-tests the tiles instead of blending them
					int quadsFlags = kQuadsFlags;

					if (_engine->getGamePlatform() == Common::kPlatformPS2)
						quadsFlags = kPs2QuadsFlags;

					for (int batch = firstBatch; batch < firstBatch + batchCount; batch++) {
						if (batch < 0 || batch >= scene->_header[TiledScene::kHeaderNumBatches]) {
							warning("Layer::draw(): invalid batch %d", batch);
							break;
						}

						const int *entry = &scene->_batches[3 * batch];
						CTexture *texture = backgroundData->getTexture(entry[0]);

						context->drawQuads(quadsFlags, texture, entry[1], entry[2], &scene->_vertices);
					}
				}
			}

			_tiledScene.unlock();
			_background.unlock();
		}
	}

	_engine->_layerDrawTimer.stop();

	return 0;
}

} // End of namespace Grim
