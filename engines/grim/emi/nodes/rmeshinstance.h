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

#ifndef GRIM_EMI_RMESHINSTANCE_H
#define GRIM_EMI_RMESHINSTANCE_H

#include "common/array.h"

#include "engines/grim/emi/graphics/cdcvertexarray.h"
#include "engines/grim/emi/nodes/cnode.h"
#include "engines/grim/emi/nodes/rprimitiveinstance.h"

namespace Grim {

class Camera;
class EmiEngine;
class RMesh;
class RNode;

class RMeshInstance : public RPrimitiveInstance {
public:
	RMeshInstance(RMesh *meshPrimitive);
	~RMeshInstance() override;

	void output(RNode *node, Camera *camera) override;
	void update(RNode *node, Camera *camera) override;
	int getSaveSize() override;
	bool saveState(SaveRestoreAccess *accessFunc) override;
	bool restoreState(SaveRestoreAccess *accessFunc) override;

	void setMaterialTextureIndex(int materialIndex, int textureIndex);

	Common::Array<int> _texIndex;
	CDCVertexArray *_vertexArray;
	bool _ownsVertexBuf;
	float *_vertexBuf;
	CNode **_boneNodes;
	bool _staticLightingDone;
	int _lastFrame;

private:
	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
