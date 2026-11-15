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

#include "common/scummsys.h"
#include "common/textconsole.h"

#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/nodes/rbox.h"
#include "engines/grim/emi/nodes/rnode.h"

namespace Grim {

struct BoxFace {
	int corners[4];
	float normal[3];
};

static const int kNumBoxCorners = 8;
static const int kNumBoxFaces = 6;
static const int kFacePolyFlags = 0x304;
static const BoxFace kBoxFaces[kNumBoxFaces] = {
	{ { 2, 3, 7, 6 }, { 0.0f, 1.0f, 0.0f } },
	{ { 4, 5, 1, 0 }, { 0.0f, -1.0f, 0.0f } },
	{ { 0, 1, 3, 2 }, { -1.0f, 0.0f, 0.0f } },
	{ { 6, 7, 5, 4 }, { 1.0f, 0.0f, 0.0f } },
	{ { 2, 6, 4, 0 }, { 0.0f, 0.0f, -1.0f } },
	{ { 1, 5, 7, 3 }, { 0.0f, 0.0f, 1.0f } }
};

RBox::RBox() {
	_sizeX = 0.0f;
	_sizeY = 0.0f;
	_sizeZ = 0.0f;
}

RBox::~RBox() {
}

void RBox::setSize(float width, float height, float depth) {
	float halfWidth = width * 0.5f;
	float halfHeight = height * 0.5f;
	float halfDepth = depth * 0.5f;

	_sizeX = width;
	_sizeY = height;
	_sizeZ = depth;

	_box._min._x = -halfWidth;
	_box._min._y = -halfHeight;
	_box._min._z = -halfDepth;
	_box._max._x = halfWidth;
	_box._max._y = halfHeight;
	_box._max._z = halfDepth;

	float sumY = (float)((double)_box._max._y + _box._min._y);
	float sumZ = (float)((double)_box._max._z + _box._min._z);

	_center._x = (float)(((double)_box._min._x + _box._max._x) * 0.5);
	_center._y = (float)((double)sumY * 0.5);
	_center._z = (float)((double)sumZ * 0.5);

	double extentX = (double)_box._max._x - _box._min._x;
	float extentY = (float)((double)_box._max._y - _box._min._y);
	float extentZ = (float)((double)_box._max._z - _box._min._z);
	double halfExtentX = extentX * 0.5;
	double halfExtentY = (double)extentY * 0.5;
	double halfExtentZ = (double)extentZ * 0.5;

	_radius = (float)sqrt(halfExtentZ * halfExtentZ + halfExtentY * halfExtentY + halfExtentX * halfExtentX);
}

RPrimitiveInstance *RBox::createInstance() {
	return new RBoxInstance(this);
}

RBoxInstance::RBoxInstance(RBox *boxPrimitive) : RPrimitiveInstance(boxPrimitive) {
}

RBoxInstance::~RBoxInstance() {
}

void RBoxInstance::output(RNode *node, Camera *camera) {
	if (!_visible)
		return;

	CDrawContext *ctx = camera->getDrawContext();

	if (!ctx) {
		warning("RBoxInstance::output(): no draw context");
		return;
	}

	const Color *faceColors[kNumBoxFaces] = {
		&Color::kWhite, &Color::kGreen, &Color::kBlue, &Color::kRed, &Color::kYellow, &Color::kCyan
	};

	if ((node->_nodeFlags & CNode::kWorldValid) == 0)
		node->updateWorldTransform();

	const float *v = camera->_view._rot._m;
	const CVector &nodePos = node->_world._pos;
	const CVector &viewPos = camera->_view._pos;

	double nodeViewX = (double)v[6] * nodePos._z + (double)v[3] * nodePos._y + (double)v[0] * nodePos._x;
	float nodeViewY = (float)((double)v[7] * nodePos._z + (double)v[1] * nodePos._x + (double)v[4] * nodePos._y);
	float nodeViewZ = (float)((double)v[8] * nodePos._z + (double)v[2] * nodePos._x + (double)v[5] * nodePos._y);
	float originX = (float)(nodeViewX + viewPos._x);
	float originY = (float)((double)nodeViewY + viewPos._y);
	float originZ = (float)((double)nodeViewZ + viewPos._z);

	CMatrix combined;

	node->_world._rot.multiply(&combined, &camera->_view._rot);

	const CVector &boxMin = _prim->_box._min;
	const CVector &boxMax = _prim->_box._max;
	float corners[kNumBoxCorners][3] = {
		{ boxMin._x, boxMin._y, boxMin._z },
		{ boxMin._x, boxMin._y, boxMax._z },
		{ boxMin._x, boxMax._y, boxMin._z },
		{ boxMin._x, boxMax._y, boxMax._z },
		{ boxMax._x, boxMin._y, boxMin._z },
		{ boxMax._x, boxMin._y, boxMax._z },
		{ boxMax._x, boxMax._y, boxMin._z },
		{ boxMax._x, boxMax._y, boxMax._z }
	};
	float transformed[kNumBoxCorners][3];
	const float *a = combined._m;

	for (int i = 0; i < kNumBoxCorners; i++) {
		float x = corners[i][0];
		float y = corners[i][1];
		float z = corners[i][2];
		double pendingX = (double)a[6] * z + (double)a[0] * x + (double)a[3] * y;
		float rotatedY = (float)((double)a[7] * z + (double)a[1] * x + (double)a[4] * y);
		float rotatedZ = (float)((double)a[8] * z + (double)a[2] * x + (double)a[5] * y);

		transformed[i][0] = (float)(pendingX + originX);
		transformed[i][1] = (float)((double)rotatedY + originY);
		transformed[i][2] = (float)((double)rotatedZ + originZ);
	}

	int clipResult = ctx->_clipVolume.testPoints(&transformed[0][0], kNumBoxCorners);

	if (clipResult == CClipVolume::kOutside)
		return;

	CDrawList *drawList = ctx->getDrawList(0);
	CVector cameraLocal;

	if ((camera->_nodeFlags & CNode::kWorldValid) == 0)
		camera->updateWorldTransform();

	node->worldToLocal(&camera->_world._pos, &cameraLocal);

	for (int face = 0; face < kNumBoxFaces; face++) {
		const BoxFace &boxFace = kBoxFaces[face];
		const float *anchor = corners[boxFace.corners[0]];
		double deltaX = (double)cameraLocal._x - anchor[0];
		double deltaY = (double)cameraLocal._y - anchor[1];
		double deltaZ = (double)cameraLocal._z - anchor[2];
		double facing = deltaZ * boxFace.normal[2] + deltaX * boxFace.normal[0] + deltaY * boxFace.normal[1];

		if (facing < 0.0)
			continue;

		DrawPoly *poly = drawList->newPoly(4);
		byte red;
		byte green;
		byte blue;

		poly->flags = kFacePolyFlags;
		faceColors[face]->getRGB(&red, &green, &blue);

		for (int i = 0; i < 4; i++) {
			DrawVertex *vertex = poly->verts[i];
			const float *point = transformed[boxFace.corners[i]];

			vertex->x = point[0];
			vertex->y = point[1];
			vertex->z = point[2];
			vertex->color = (vertex->color & 0xFF000000) | ((uint32)red << 16) | ((uint32)green << 8) | blue;
		}

		if (clipResult == CClipVolume::kPartial)
			ctx->_clipVolume.computeVertexOutcodes(4, poly->verts);

		poly->reserved8 = 0;
		drawList->addPoly(poly);
	}
}

} // End of namespace Grim
