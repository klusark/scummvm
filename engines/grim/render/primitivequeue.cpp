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

#include "common/util.h"

#include "engines/grim/grim.h"
#include "engines/grim/render/primitivequeue.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/std/std_display.h"

namespace Grim {

SputPrimitiveObject::SputPrimitiveObject(int id, int shape) : next(nullptr), prev(nullptr), shape(shape), id(id),
		layer(2), x1(0), y1(0), x2(0), y2(0), color(0), pattern(-1), filled(0), colorRgb(0), alpha(1.0f) {
}

PrimitiveQueue::PrimitiveQueue(RenderDroid *rd) : _rd(rd), _sputRender(nullptr), _head(nullptr), _tail(nullptr), _nextId(1) {
}

void PrimitiveQueue::purge() {
	if (_head) {
		SputPrimitiveObject *next;

		do {
			next = _head->next;
			markDirty(_head);
			delete _head;
			_head = next;
		} while (next);
	}

	_tail = nullptr;
}

int PrimitiveQueue::addLine(int x1, int y1, int x2, int y2, int color, int layer) {
	int id = _nextId++;
	SputPrimitiveObject *obj = addObject(id, layer);

	obj->x1 = x1;
	obj->y1 = y1;
	obj->x2 = x2;
	obj->y2 = y2;
	obj->color = color;

	return obj->id;
}

int PrimitiveQueue::addRect(int x1, int y1, int x2, int y2, int color, int filled, int layer) {
	int id = _nextId++;
	SputPrimitiveObject *obj = addObject(id, layer);

	obj->shape = kSputPrimitiveRect;
	obj->x1 = x1;
	obj->y1 = y1;
	obj->x2 = x2;
	obj->y2 = y2;
	obj->color = color;
	obj->filled = filled;

	return obj->id;
}

int PrimitiveQueue::addPolygon(int numPoints, int *points, int color, int layer) {
	int id = _nextId++;

	for (int i = 0; i < numPoints - 1; i++) {
		SputPrimitiveObject *obj = addObject(id, layer);

		obj->shape = kSputPrimitivePolygon;
		obj->x1 = points[i * 2];
		obj->y1 = points[i * 2 + 1];
		obj->x2 = points[i * 2 + 2];
		obj->y2 = points[i * 2 + 3];
		obj->color = color;
	}

	return id;
}

SputPrimitiveObject *PrimitiveQueue::addObject(int id, int layer) {
	if (_tail) {
		SputPrimitiveObject *obj = new SputPrimitiveObject(id, kSputPrimitiveLine);

		_tail->next = obj;
		_tail->next->prev = _tail;
		_tail = _tail->next;
	} else {
		SputPrimitiveObject *obj = new SputPrimitiveObject(id, kSputPrimitiveLine);

		_tail = obj;
		_head = obj;
	}

	// The remastered also has the passes of the menus (4) and of the dialogs (8); the other values are the overlay pass
	if (g_grim->isRemastered()) {
		if (layer != 1 && layer != 4 && layer != 8)
			layer = 2;
	} else if (layer != 2 && layer != 1) {
		layer = 2;
	}

	_tail->layer = layer;

	return _tail;
}

void PrimitiveQueue::deletePrimitive(int id) {
	SputPrimitiveObject *obj = _head;

	while (obj) {
		if (obj->id == id) {
			SputPrimitiveObject *prev = obj->prev;

			if (prev)
				prev->next = obj->next;

			if (obj->next)
				obj->next->prev = obj->prev;

			if (_tail == obj)
				_tail = obj->prev;

			if (obj == _head)
				_head = _head->next;

			SputPrimitiveObject *next = obj->next;

			markDirty(obj);
			delete obj;
			obj = next;
		} else {
			obj = obj->next;
		}
	}
}

void PrimitiveQueue::changeColor(int id, int color) {
	for (SputPrimitiveObject *obj = findPrimitive(id, nullptr); obj; obj = findPrimitive(id, obj))
		obj->color = color;
}

void PrimitiveQueue::changeColorRgb(int id, uint colorRgb) {
	for (SputPrimitiveObject *obj = findPrimitive(id, nullptr); obj; obj = findPrimitive(id, obj))
		obj->colorRgb = colorRgb;
}

void PrimitiveQueue::setAlpha(int id, float alpha) {
	for (SputPrimitiveObject *obj = findPrimitive(id, nullptr); obj; obj = findPrimitive(id, obj))
		obj->alpha = alpha;
}

void PrimitiveQueue::changeLayer(int id, int layer) {
	for (SputPrimitiveObject *obj = findPrimitive(id, nullptr); obj; obj = findPrimitive(id, obj)) {
		markDirty(obj);

		if (g_grim->isRemastered()) {
			if (layer != 1 && layer != 4 && layer != 8)
				layer = 2;
		} else if (layer != 2 && layer != 1) {
			layer = 2;
		}

		obj->layer = layer;
	}
}

void PrimitiveQueue::move(int id, int dx, int dy) {
	for (SputPrimitiveObject *obj = findPrimitive(id, nullptr); obj; obj = findPrimitive(id, obj)) {
		markDirty(obj);

		obj->x1 += dx;
		obj->y2 += dy;
		obj->y1 += dy;
		obj->x2 += dx;
	}
}

void PrimitiveQueue::moveTo(int id, int x, int y) {
	int dx = 0;
	int dy = 0;
	SputPrimitiveObject *obj = findPrimitive(id, nullptr);

	if (obj) {
		markDirty(obj);

		if (x != -1) {
			dx = x - obj->x1;
			obj->x1 = x;
		}

		if (y != -1) {
			dy = y - obj->y1;
			obj->y1 = y;
		}

		obj->y2 += dy;
		obj->x2 += dx;

		for (obj = findPrimitive(id, obj); obj; obj = findPrimitive(id, obj)) {
			markDirty(obj);

			obj->x1 += dx;
			obj->y1 += dy;
			obj->x2 += dx;
			obj->y2 += dy;
		}
	}
}

void PrimitiveQueue::moveEndpoint(int id, int x2, int y2) {
	SputPrimitiveObject *obj = findPrimitive(id, nullptr);

	if (obj) {
		markDirty(obj);

		if (x2 != -1)
			obj->x2 = x2;

		if (y2 != -1)
			obj->y2 = y2;
	}
}

void PrimitiveQueue::changeDimensions(int id, int width, int height) {
	SputPrimitiveObject *obj = findPrimitive(id, nullptr);

	if (obj) {
		markDirty(obj);

		if (width != -1)
			obj->x2 = width + obj->x1;

		if (height != -1)
			obj->y2 = height + obj->y1;
	}
}

void PrimitiveQueue::draw(RdCanvas *canvas, int layerMask) {
	for (SputPrimitiveObject *obj = _head; obj; obj = obj->next) {
		if (layerMask & obj->layer)
			drawPrimitive(obj, canvas);
	}
}

void PrimitiveQueue::drawPrimitive(SputPrimitiveObject *obj, RdCanvas *canvas) {
	switch (obj->shape) {
	case kSputPrimitiveLine:
	case kSputPrimitivePolygon:
		_rd->_primit2.drawClippedLine(canvas, obj->x1, obj->y1, obj->x2, obj->y2, (uint16)obj->color, (uint)obj->pattern);
		break;
	case kSputPrimitiveRect:
		if (obj->filled) {
			int left = MIN(obj->x1, obj->x2);
			int top = MIN(obj->y1, obj->y2);
			int right = MAX(obj->x1, obj->x2);
			int bottom = MAX(obj->y1, obj->y2);

			for (int y = top; y <= bottom; y++)
				_rd->_primit2.drawClippedLine(canvas, left, y, right, y, (uint16)obj->color, (uint)obj->pattern);
		} else {
			_rd->_primit2.drawRectangle(canvas, obj->x1, obj->y1, obj->x2, obj->y2, (uint16)obj->color, (uint)obj->pattern);
		}
		break;
	default:
		break;
	}
}

void PrimitiveQueue::markAllDirty(int layerMask) {
	for (SputPrimitiveObject *obj = _head; obj; obj = obj->next) {
		if (layerMask & obj->layer)
			markDirty(obj);
	}
}

void PrimitiveQueue::markDirty(SputPrimitiveObject *obj) {
	StdRect rect;

	rect.x = MIN(obj->x1, obj->x2);
	rect.y = MIN(obj->y1, obj->y2);
	rect.width = MAX(obj->x1, obj->x2) - rect.x + 1;
	rect.height = MAX(obj->y1, obj->y2) - rect.y + 1;

	_sputRender->markDirtyRect(&rect, 0);
}

void PrimitiveQueue::resetId() {
	_nextId = 1;
}

SputPrimitiveObject *PrimitiveQueue::findPrimitive(int id, SputPrimitiveObject *prev) {
	SputPrimitiveObject *obj;

	if (prev)
		obj = prev->next;
	else
		obj = _head;

	for (; obj; obj = obj->next) {
		if (obj->id == id)
			break;
	}

	return obj;
}

} // End of namespace Grim
