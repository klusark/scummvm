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

#include "engines/grim/emi/actor/actor.h"
#include "engines/grim/emi/scene/camera.h"
#include "engines/grim/emi/graphics/cdcvertexarray.h"
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/graphics/color.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/nodes/rmesh.h"
#include "engines/grim/emi/nodes/rmeshinstance.h"
#include "engines/grim/emi/nodes/rprimitiveinstance.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/wpvertex.h"

namespace Grim {

static const int kShadowTriangleFlags = 0x301;
static const int kShadowVertexCapacity = 2000;
static const float kMinClipLength = 1e-20f;

uint Actor::findShadowPrimitive(RPrimitiveInstance *instance) {
	for (uint i = 0; i < _shadowMeshes.size(); i++) {
		if (_shadowMeshes[i] == instance)
			return i;
	}

	return (uint)-1;
}

void Actor::removeShadowPrimitive(int index) {
	if (index >= 0 && (uint)index < _shadowMeshes.size())
		_shadowMeshes.remove_at(index);
}

void Actor::activateShadow(bool active, Shadow_v2 *shadow) {
	if (active) {
		if (shadow) {
			if (findShadowIndex(shadow) < 0)
				_shadows.push_back(shadow);
		} else {
			_shadows.clear();

			bool locked = _room.lock();
			Room_v2 *currentRoom = locked ? _room._typed : nullptr;

			if (currentRoom) {
				Common::List<Shadow_v2 *>::iterator it;

				for (it = currentRoom->_shadows.begin(); it != currentRoom->_shadows.end(); ++it) {
					if (*it)
						activateShadow(active, *it);
				}
			} else {
				warning("Actor::activateShadow(): the room could not be locked");
			}

			_room.unlock();
		}
	} else if (shadow) {
		int index = findShadowIndex(shadow);

		if (index >= 0)
			_shadows.remove_at(index);
	} else {
		_shadows.clear();
	}
}

int Actor::findShadowIndex(Shadow_v2 *shadow) {
	for (uint i = 0; i < _shadows.size(); i++) {
		if (_shadows[i] == shadow)
			return i;
	}

	return -1;
}

int Actor::drawShadow(Camera *camera, Shadow_v2 *shadow) {
	_engine->_actorDrawShadowTimer.start();

	CDrawContext *drawContext = _engine->getDrawContext();

	if (!_onScreen && !_engine->isDemo()) {
		_engine->_actorDrawShadowTimer.stop();

		return 0;
	}

	if (!drawContext || !_engine->_shadowVertices) {
		warning("Actor::drawShadow(): no draw context or shadow vertices");
		_engine->_actorDrawShadowTimer.stop();

		return 0;
	}

	drawContext->setCameraMatrix(reinterpret_cast<const float *>(&camera->_view));

	float shadowPoint[3];
	float *position = shadow->getPosition(shadowPoint);
	float shadowX = position[0];
	float shadowY = position[1];
	float shadowZ = position[2];

	Color shadowColor(shadow->_color);

	Common::Array<RPrimitiveInstance *> *meshInstances = &_instances;

	if (_shadowMeshes.size() > 0)
		meshInstances = &_shadowMeshes;

	if (shadow->_sectors.empty() || !shadow->_sectors[0]) {
		warning("Actor::drawShadow(): shadow without sectors");
		drawContext->resetCameraMatrix();
		_engine->_actorDrawShadowTimer.stop();

		return 0;
	}

	WalkPlane_v2 *firstSector = shadow->_sectors[0];
	float planeNormalX = firstSector->_normal[0];
	float planeNormalY = firstSector->_normal[1];
	float planeD = firstSector->_d;
	float planeNormalZ = firstSector->_normal[2];

	for (uint i = 0; i < meshInstances->size(); i++) {
		RPrimitiveInstance *instance = (*meshInstances)[i];

		if (!instance || !instance->_prim) {
			warning("Actor::drawShadow(): primitive instance without primitive");

			continue;
		}

		RMesh *mesh = dynamic_cast<RMesh *>(instance->_prim);

		if (!mesh)
			continue;

		instance->update(this, camera);

		RMeshInstance *meshInstance = static_cast<RMeshInstance *>((*meshInstances)[i]);

		if (!meshInstance->_vertexArray) {
			warning("Actor::drawShadow(): mesh instance without vertex array");

			continue;
		}

		if (meshInstance->_vertexArray->_numVerts > kShadowVertexCapacity) {
			warning("Actor::drawShadow(): too many vertices for the shadow vertex array");

			continue;
		}

		_engine->_shadowVertices->_numVerts = meshInstance->_vertexArray->_numVerts;

		CDCVertex *destination = static_cast<CDCVertex *>(_engine->_shadowVertices->lock());
		CDCVertex *source = static_cast<CDCVertex *>(meshInstance->_vertexArray->lock());

		if (!destination || !source) {
			warning("Actor::drawShadow(): could not lock the vertex arrays");

			continue;
		}

		for (int v = 0; v < _engine->_shadowVertices->_numVerts; v++) {
			CVector localPoint;
			CVector worldPoint;

			localPoint._x = source[v].x;
			localPoint._y = source[v].y;
			localPoint._z = source[v].z;

			localToWorld(&localPoint, &worldPoint);

			destination[v].x = worldPoint._x;
			destination[v].y = worldPoint._y;
			destination[v].z = worldPoint._z;

			double deltaX = (double)destination[v].x - shadowX;
			float deltaY = (float)((double)destination[v].y - shadowY);
			float deltaZ = (float)((double)destination[v].z - shadowZ);

			double numerator = (((double)planeNormalY * destination[v].y + (double)planeNormalZ * destination[v].z) + (double)planeNormalX * destination[v].x) + planeD;
			double denominator = ((double)planeNormalZ * deltaZ + (double)planeNormalY * deltaY) + deltaX * planeNormalX;
			float projectionScale = (float)(-(numerator / denominator));

			float offsetY = (float)((double)deltaY * projectionScale);
			float offsetZ = (float)((double)deltaZ * projectionScale);

			destination[v].x = (float)(deltaX * projectionScale + destination[v].x);
			destination[v].y = (float)(offsetY + destination[v].y);
			destination[v].z = (float)(offsetZ + destination[v].z);

			uint red = (uint)shadowColor._r <= 0xFFu ? (uint)shadowColor._r : 0xFFu;
			uint green = (uint)shadowColor._g <= 0xFFu ? (uint)shadowColor._g : 0xFFu;
			uint blue = (uint)shadowColor._b <= 0xFFu ? (uint)shadowColor._b : 0xFFu;
			uint alpha = (uint)shadowColor._a <= 0xFFu ? (uint)shadowColor._a : 0xFFu;

			destination[v].color = (alpha << 24) | (red << 16) | (green << 8) | blue;
		}

		meshInstance->_vertexArray->unlock();
		_engine->_shadowVertices->unlock();

		for (uint s = 0; s < shadow->_sectors.size(); s++) {
			WalkPlane_v2 *sector = shadow->_sectors[s];

			if (!sector) {
				warning("Actor::drawShadow(): unresolved shadow sector");

				continue;
			}

			if (sector->_numVerts > drawContext->getMaxClipPlanes())
				continue;

			int numVerts = sector->_numVerts;

			for (int e = 0; e < numVerts; e++) {
				wpVertex *start = sector->_verts[e];
				wpVertex *end = sector->_verts[(e + 1) % numVerts];

				double toShadowX = (double)shadowX - start->x;
				double toShadowY = (double)shadowY - start->y;
				double toShadowZ = (double)shadowZ - start->z;

				float edgeX = (float)((double)end->x - start->x);
				float edgeY = (float)((double)end->y - start->y);
				double edgeZ = (double)end->z - start->z;

				float clipX = (float)(edgeY * toShadowZ - edgeZ * toShadowY);
				float clipY = (float)(edgeZ * toShadowX - toShadowZ * edgeX);
				float clipZ = (float)(toShadowY * edgeX - edgeY * toShadowX);

				double length = sqrt(((double)clipX * clipX + (double)clipZ * clipZ) + (double)clipY * clipY);

				if (length >= kMinClipLength) {
					double inverse = 1.0 / length;

					clipX = (float)(clipX * inverse);
					clipY = (float)(inverse * clipY);
					clipZ = (float)(inverse * clipZ);
				}

				float clipD = (float)(-(((double)shadowY * clipY + (double)shadowZ * clipZ) + (double)shadowX * clipX));

				drawContext->addClipPlane(clipX, clipY, clipZ, clipD);
			}

			drawContext->submitVertices(_engine->_shadowVertices);

			for (uint t = 0; t < mesh->_trisets.size(); t++) {
				TriSet *triset = mesh->_trisets[t];

				if (triset)
					drawContext->drawTriangles(kShadowTriangleFlags, nullptr, (int)triset->indices.size(), triset->indices.data());
			}

			drawContext->releaseVertices();
			drawContext->clearClipPlanes();
		}
	}

	drawContext->resetCameraMatrix();

	_engine->_actorDrawShadowTimer.stop();

	return 0;
}

} // End of namespace Grim
