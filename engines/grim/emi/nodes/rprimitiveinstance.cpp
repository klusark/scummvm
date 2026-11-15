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

#include "common/algorithm.h"
#include "common/array.h"
#include "common/textconsole.h"

#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/core/lmemoryservices.h"
#include "engines/grim/emi/nodes/rnode.h"
#include "engines/grim/emi/nodes/rprimitiveinstance.h"

namespace Grim {

static const float kInitialGlobalAlpha = 255.0f;
static const int kSaveBaseSize = 17;
static const int kSaveAlphaEntrySize = 8;
static const int kNumBoxCorners = 8;
static const int kNumBoxEdges = 12;
static const int kBoxEdges[kNumBoxEdges][2] = {
	{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
	{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
	{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
};

RPrimitiveInstance::RPrimitiveInstance(RPrimitive *primitive) {
	_prim = primitive;
	_visible = true;
	_stateRestored = false;
	_globalAlphaMode = kAlphaOff;
	_globalAlpha = kInitialGlobalAlpha;
	_matAlphaMode = kAlphaOff;
}

RPrimitiveInstance::~RPrimitiveInstance() {
}

void *RPrimitiveInstance::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void RPrimitiveInstance::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

int RPrimitiveInstance::getSaveSize() {
	return kSaveAlphaEntrySize * (int)_matAlpha.size() + kSaveBaseSize;
}

bool RPrimitiveInstance::saveState(SaveRestoreAccess *accessFunc) {
	byte visibleByte = _visible ? 1 : 0;

	accessFunc(&visibleByte, sizeof(visibleByte));
	accessFunc(&_globalAlphaMode, sizeof(_globalAlphaMode));
	accessFunc(&_globalAlpha, sizeof(_globalAlpha));
	accessFunc(&_matAlphaMode, sizeof(_matAlphaMode));

	uint32 numEntries = _matAlpha.size();

	accessFunc(&numEntries, sizeof(numEntries));

	Common::Array<int> keys;

	for (Common::HashMap<int, float>::const_iterator it = _matAlpha.begin(); it != _matAlpha.end(); ++it)
		keys.push_back(it->_key);

	Common::sort(keys.begin(), keys.end());

	for (uint i = 0; i < keys.size(); i++) {
		int key = keys[i];
		float value = _matAlpha[key];

		accessFunc(&key, sizeof(key));
		accessFunc(&value, sizeof(value));
	}

	return true;
}

bool RPrimitiveInstance::restoreState(SaveRestoreAccess *accessFunc) {
	byte visibleByte = 0;

	accessFunc(&visibleByte, sizeof(visibleByte));
	_visible = visibleByte != 0;
	accessFunc(&_globalAlphaMode, sizeof(_globalAlphaMode));
	accessFunc(&_globalAlpha, sizeof(_globalAlpha));
	accessFunc(&_matAlphaMode, sizeof(_matAlphaMode));

	int numEntries = 0;

	accessFunc(&numEntries, sizeof(numEntries));

	for (int i = 0; i < numEntries; i++) {
		int key = 0;
		float value = 0.0f;

		accessFunc(&key, sizeof(key));
		accessFunc(&value, sizeof(value));
		_matAlpha[key] = value;
	}

	_stateRestored = true;

	return true;
}

bool RPrimitiveInstance::isVisible(RNode *node, Camera *camera) {
	CDrawContext *ctx = camera->getDrawContext();

	if (!ctx) {
		warning("RPrimitiveInstance::isVisible(): no draw context");
		return false;
	}

	if ((node->_nodeFlags & CNode::kWorldValid) == 0)
		node->updateWorldTransform();

	const float *w = node->_world._rot._m;
	const float *v = camera->_view._rot._m;
	const CVector &nodePos = node->_world._pos;
	const CVector &viewPos = camera->_view._pos;
	const CVector &center = _prim->_center;

	double sphereX = (double)w[6] * center._z + (double)w[3] * center._y + (double)w[0] * center._x;
	float sphereY = (float)((double)w[7] * center._z + (double)w[1] * center._x + (double)w[4] * center._y);
	float sphereZ = (float)((double)w[8] * center._z + (double)w[2] * center._x + (double)w[5] * center._y);
	float worldX = (float)(sphereX + nodePos._x);
	float worldY = (float)((double)sphereY + nodePos._y);
	float worldZ = (float)((double)sphereZ + nodePos._z);
	double viewX = (double)worldZ * v[6] + (double)worldY * v[3] + (double)worldX * v[0];
	float viewY = (float)((double)worldZ * v[7] + (double)worldY * v[4] + (double)worldX * v[1]);
	float viewZ = (float)((double)worldZ * v[8] + (double)worldY * v[5] + (double)worldX * v[2]);
	float sphere[4];

	sphere[0] = (float)(viewX + viewPos._x);
	sphere[1] = (float)((double)viewY + viewPos._y);
	sphere[2] = (float)((double)viewZ + viewPos._z);
	sphere[3] = _prim->_radius;

	int result = ctx->_clipVolume.testSphere(sphere);

	if (result == CClipVolume::kOutside)
		return false;

	if (result == CClipVolume::kInside)
		return true;

	if ((node->_nodeFlags & CNode::kWorldValid) == 0)
		node->updateWorldTransform();

	double nodeViewX = (double)nodePos._x * v[0] + (double)v[6] * nodePos._z + (double)nodePos._y * v[3];
	float nodeViewY = (float)((double)nodePos._y * v[4] + (double)v[7] * nodePos._z + (double)nodePos._x * v[1]);
	float nodeViewZ = (float)((double)nodePos._x * v[2] + (double)v[8] * nodePos._z + (double)nodePos._y * v[5]);
	float transform[12];

	transform[0] = (float)((double)v[6] * w[2] + (double)v[0] * w[0] + (double)v[3] * w[1]);
	transform[1] = (float)((double)v[1] * w[0] + (double)v[7] * w[2] + (double)v[4] * w[1]);
	transform[2] = (float)((double)v[8] * w[2] + (double)v[5] * w[1] + (double)v[2] * w[0]);
	transform[3] = (float)((double)w[4] * v[3] + (double)v[0] * w[3] + (double)v[6] * w[5]);
	transform[4] = (float)((double)w[5] * v[7] + (double)w[4] * v[4] + (double)v[1] * w[3]);
	transform[5] = (float)((double)v[2] * w[3] + (double)w[5] * v[8] + (double)w[4] * v[5]);
	transform[6] = (float)((double)v[6] * w[8] + (double)w[6] * v[0] + (double)v[3] * w[7]);
	transform[7] = (float)((double)v[7] * w[8] + (double)v[4] * w[7] + (double)v[1] * w[6]);
	transform[8] = (float)((double)w[6] * v[2] + (double)w[8] * v[8] + (double)w[7] * v[5]);
	transform[9] = (float)(nodeViewX + viewPos._x);
	transform[10] = (float)((double)nodeViewY + viewPos._y);
	transform[11] = (float)((double)nodeViewZ + viewPos._z);

	return ctx->_clipVolume.testBox(&_prim->_box, transform) != CClipVolume::kOutside;
}

void RPrimitiveInstance::drawBounds(RNode *node, Camera *camera) {
	if ((node->_nodeFlags & CNode::kWorldValid) == 0)
		node->updateWorldTransform();

	const float *v = camera->_view._rot._m;
	const CVector &nodePos = node->_world._pos;
	const CVector &viewPos = camera->_view._pos;

	float nodeViewY = (float)((double)v[4] * nodePos._y + (double)v[1] * nodePos._x + (double)v[7] * nodePos._z);
	float nodeViewZ = (float)((double)v[5] * nodePos._y + (double)v[2] * nodePos._x + (double)v[8] * nodePos._z);
	double nodeViewX = (double)v[3] * nodePos._y + (double)v[6] * nodePos._z + (double)v[0] * nodePos._x;
	float originX = (float)(nodeViewX + viewPos._x);
	float originY = (float)((double)nodeViewY + viewPos._y);
	float originZ = (float)((double)nodeViewZ + viewPos._z);

	CMatrix combined;

	node->_world._rot.multiply(&combined, &camera->_view._rot);

	const CVector &boxMin = _prim->_box._min;
	const CVector &boxMax = _prim->_box._max;
	float corners[kNumBoxCorners][3] = {
		{ boxMin._x, boxMin._y, boxMin._z },
		{ boxMax._x, boxMin._y, boxMin._z },
		{ boxMax._x, boxMin._y, boxMax._z },
		{ boxMin._x, boxMin._y, boxMax._z },
		{ boxMin._x, boxMax._y, boxMin._z },
		{ boxMax._x, boxMax._y, boxMin._z },
		{ boxMax._x, boxMax._y, boxMax._z },
		{ boxMin._x, boxMax._y, boxMax._z }
	};
	const float *a = combined._m;

	for (int i = 0; i < kNumBoxCorners; i++) {
		float x = corners[i][0];
		float y = corners[i][1];
		float z = corners[i][2];
		float transformedX = (float)((double)a[6] * z + (double)a[3] * y + (double)a[0] * x);
		float transformedY = (float)((double)a[1] * x + (double)a[7] * z + (double)a[4] * y);
		float transformedZ = (float)((double)a[8] * z + (double)a[5] * y + (double)a[2] * x);

		corners[i][0] = (float)((double)originX + transformedX);
		corners[i][1] = (float)((double)originY + transformedY);
		corners[i][2] = (float)((double)originZ + transformedZ);
	}

	byte red;
	byte green;
	byte blue;

	Color::kWhite.getRGB(&red, &green, &blue);

	DrawVertex line[2];

	for (int i = 0; i < 2; i++)
		line[i].color = (line[i].color & 0xFF000000) | ((uint32)red << 16) | ((uint32)green << 8) | blue;

	for (int i = 0; i < kNumBoxEdges; i++) {
		line[0].x = corners[kBoxEdges[i][0]][0];
		line[0].y = corners[kBoxEdges[i][0]][1];
		line[0].z = corners[kBoxEdges[i][0]][2];
		line[1].x = corners[kBoxEdges[i][1]][0];
		line[1].y = corners[kBoxEdges[i][1]][1];
		line[1].z = corners[kBoxEdges[i][1]][2];

		camera->drawLineLoop(line, 2);
	}
}

} // End of namespace Grim
