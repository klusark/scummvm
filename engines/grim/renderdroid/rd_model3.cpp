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

#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/renderdroid/rd.h"

#include <string.h>

namespace Grim {

void RenderDroid::Model3Module::clearFrameCounters() {
	_numDrawnModels = 0;
}

void RenderDroid::Model3Module::activateClipPlane(int onOff) {
	_clipPlaneActive = onOff;
}

void RenderDroid::Model3Module::setClipPlane(RdVector3 norm, RdVector3 pt) {
	_clipNorm[0] = norm[0];
	_clipNorm[1] = norm[1];
	_clipNorm[2] = norm[2];
	_clipPt[0] = pt[0];
	_clipPt[1] = pt[1];
	_clipPt[2] = pt[2];
}

int RenderDroid::Model3Module::newEntry(RdModel3 *model3) {
	memset(model3, 0, sizeof(RdModel3));
	Common::strlcpy(model3->name, "UNKNOWN", 32);
	model3->curGeoSet = 0;

	return 0;
}

void RenderDroid::Model3Module::calcBoundingBoxes(RdModel3 *model3) {
	model3->bound.min[0] = 3.4e38f;
	model3->bound.min[1] = 3.4e38f;
	model3->bound.min[2] = 3.4e38f;
	model3->bound.max[0] = -3.4e38f;
	model3->bound.max[1] = -3.4e38f;
	model3->bound.max[2] = -3.4e38f;

	for (uint i = 0; i < model3->numGeos; i++) {
		RdModel3Geo *geo = &model3->geo[i];

		for (uint j = 0; j < geo->numMeshes; j++) {
			RdModel3Mesh *mesh = &geo->meshes[j];

			mesh->bound.min[0] = 3.4e38f;
			mesh->bound.min[1] = 3.4e38f;
			mesh->bound.min[2] = 3.4e38f;
			mesh->bound.max[0] = -3.4e38f;
			mesh->bound.max[1] = -3.4e38f;
			mesh->bound.max[2] = -3.4e38f;

			for (uint k = 0; k < mesh->numVertices; k++) {
				float *vertex = mesh->vertices[k];

				if (vertex[0] < (double)mesh->bound.min[0])
					mesh->bound.min[0] = vertex[0];
				if (vertex[1] < (double)mesh->bound.min[1])
					mesh->bound.min[1] = vertex[1];
				if (vertex[2] < (double)mesh->bound.min[2])
					mesh->bound.min[2] = vertex[2];
				if (vertex[0] > (double)mesh->bound.max[0])
					mesh->bound.max[0] = vertex[0];
				if (vertex[1] > (double)mesh->bound.max[1])
					mesh->bound.max[1] = vertex[1];
				if (vertex[2] > (double)mesh->bound.max[2])
					mesh->bound.max[2] = vertex[2];
			}

			// The demo takes the box of the model from the boxes of the meshes
			if (_rd->isDemo()) {
				if (mesh->bound.min[0] < (double)model3->bound.min[0])
					model3->bound.min[0] = mesh->bound.min[0];
				if (mesh->bound.min[1] < (double)model3->bound.min[1])
					model3->bound.min[1] = mesh->bound.min[1];
				if (mesh->bound.min[2] < (double)model3->bound.min[2])
					model3->bound.min[2] = mesh->bound.min[2];
				if (mesh->bound.max[0] > (double)model3->bound.max[0])
					model3->bound.max[0] = mesh->bound.max[0];
				if (mesh->bound.max[1] > (double)model3->bound.max[1])
					model3->bound.max[1] = mesh->bound.max[1];
				if (mesh->bound.max[2] > (double)model3->bound.max[2])
					model3->bound.max[2] = mesh->bound.max[2];
			}
		}
	}

	// The demo has no center of the geometry and no radius from the box
	if (_rd->isDemo())
		return;

	model3->bound.min[0] = 3.4e38f;
	model3->bound.min[1] = 3.4e38f;
	model3->bound.min[2] = 3.4e38f;
	model3->bound.max[0] = -3.4e38f;
	model3->bound.max[1] = -3.4e38f;
	model3->bound.max[2] = -3.4e38f;
	calcModelBoundingBox(model3, model3->hNodes, _rd->_identMatrix34);

	double dy = (double)model3->bound.min[1] - model3->bound.max[1];
	double dx = (double)model3->bound.min[0] - model3->bound.max[0];
	double dz = (double)model3->bound.min[2] - model3->bound.max[2];
	float dxStored = (float)dx;
	double dxSquared = dx * dxStored;
	float dzStored = (float)dz;
	double centerY = ((double)model3->bound.max[1] - model3->bound.min[1]) * 0.5 + model3->bound.min[1];
	double centerZ = ((double)model3->bound.max[2] - model3->bound.min[2]) * 0.5 + model3->bound.min[2];

	model3->geomCenter[0] = (float)(((double)model3->bound.max[0] - model3->bound.min[0]) * 0.5 + model3->bound.min[0]);
	model3->geomCenter[1] = (float)centerY;
	model3->geomCenter[2] = (float)centerZ;

	if (_rd->_remastered) {
		float diagonal = (float)sqrt((dy * dy + dx * dx) + dz * dz);

		model3->radius = (float)(diagonal * 0.5);
	} else {
		model3->radius = (float)(sqrt(dxSquared + dy * dy + dz * dzStored) * 0.5);
	}
}

void RenderDroid::Model3Module::calcRadii(RdModel3 *model3) {
	for (uint i = 0; i < model3->geo[0].numMeshes; i++) {
		RdModel3Mesh *mesh = &model3->geo[0].meshes[i];
		double maxLength = 0.0;

		for (uint j = 0; j < mesh->numVertices; j++) {
			float *vertex = mesh->vertices[j];
			double length;

			if (_rd->_remastered)
				length = sqrt(((double)vertex[1] * vertex[1] + (double)vertex[0] * vertex[0]) + (double)vertex[2] * vertex[2]);
			else
				length = sqrt((double)vertex[1] * vertex[1] + (double)vertex[2] * vertex[2] + (double)vertex[0] * vertex[0]);

			if (length > maxLength) {
				float stored = (float)length;
				maxLength = stored;
			}
		}

		if (_rd->_remastered)
			mesh->radius = (float)(maxLength * 0.1f + maxLength);
		else
			mesh->radius = (float)(maxLength * 0.1 + maxLength);
	}

	_maxGeoRadius = 0.0f;
	buildExpandedRadius(model3, model3->hNodes, _rd->_identMatrix34);

	// The demo replaces the radius of the model with the expanded one
	if (_rd->isDemo())
		model3->radius = (float)((double)_maxGeoRadius * 0.1 + _maxGeoRadius);
}

void RenderDroid::Model3Module::calcNodeDepths(RdModel3 *model3) {
	for (uint i = 0; i < model3->numHNodes; i++) {
		RdModel3HNode *hNode = &model3->hNodes[i];

		hNode->depth = 0;
		for (RdModel3HNode *parent = hNode->parentNode; parent; parent = parent->parentNode)
			hNode->depth++;
	}
}

void RenderDroid::Model3Module::calcFaceNormals(RdModel3 *model3) {
	for (uint i = 0; i < model3->numGeos; i++) {
		RdModel3Geo *geo = &model3->geo[i];

		for (uint j = 0; j < geo->numMeshes; j++) {
			RdModel3Mesh *mesh = &geo->meshes[j];

			for (uint k = 0; k < mesh->numFaces; k++) {
				RdFace3 *face = &mesh->faces[k];
				int numVertices = face->numVertices;
				int vertex = 0;
				uint previous = 0;
				uint next = 0;
				int following = 0;

				if (numVertices > 0) {
					do {
						previous = vertex - 1;
						if (vertex - 1 < 0)
							previous = numVertices - 1;

						following = vertex + 1;
						next = following % numVertices;

						if (!_rd->pointsCollinear(mesh->vertices[face->vertices[vertex]], mesh->vertices[face->vertices[next]], mesh->vertices[face->vertices[previous]]))
							break;

						numVertices = face->numVertices;
						vertex++;
					} while (following < (int)face->numVertices);
				}

				if (vertex < (int)face->numVertices)
					_rd->calcSurfaceNormal(face->normal, mesh->vertices[face->vertices[vertex]], mesh->vertices[face->vertices[next]], mesh->vertices[face->vertices[previous]]);
			}
		}
	}
}

void RenderDroid::Model3Module::calcVertexNormals(RdModel3 *model3) {
	for (uint i = 0; i < model3->numGeos; i++) {
		RdModel3Geo *geo = &model3->geo[i];

		for (uint j = 0; j < geo->numMeshes; j++) {
			RdModel3Mesh *mesh = &geo->meshes[j];
			float *normal = mesh->vertexNormals[0];

			for (uint vertex = 0; vertex < mesh->numVertices; vertex++) {
				double sumX = 0.0;
				double sumY = 0.0;
				double sumZ = 0.0;
				double count = 0.0;
				float countStored = 0.0f;
				float sumYStored = 0.0f;
				float sumZStored = 0.0f;

				if (mesh->numFaces) {
					for (uint k = 0; k < mesh->numFaces; k++) {
						RdFace3 *face = &mesh->faces[k];

						for (uint m = 0; m < face->numVertices; m++) {
							if ((uint)face->vertices[m] == vertex) {
								if (_rd->_remastered) {
									sumX = (float)((double)face->normal[0] + sumX);
									sumY = (float)((double)face->normal[1] + sumY);
									sumZ = (float)((double)face->normal[2] + sumZ);
									count = (float)(count + 1.0);
								} else {
									sumX = sumX + face->normal[0];
									sumY = sumY + face->normal[1];
									sumZ = sumZ + face->normal[2];
									count = count - -1.0;
								}
								break;
							}
						}
					}

					countStored = (float)count;
					sumZStored = (float)sumZ;
					sumYStored = (float)sumY;
				}

				if (countStored == 0.0) {
					normal[0] = 1.0f;
					normal[1] = 0.0f;
					normal[2] = 0.0f;
				} else {
					if (countStored == 1.0) {
						RdFace3 *firstFace = &mesh->faces[0];
						uint numFaceVertices = firstFace->numVertices;
						float *previous = mesh->vertices[firstFace->vertices[numFaceVertices - 1]];
						float *next = mesh->vertices[firstFace->vertices[1 % numFaceVertices]];
						float *first = mesh->vertices[0];

						if (_rd->_remastered) {
							float dxPreviousStored = (float)((double)first[0] - previous[0]);
							float dyPreviousStored = (float)((double)first[1] - previous[1]);
							float dzPreviousStored = (float)((double)first[2] - previous[2]);
							float dxNextStored = (float)((double)first[0] - next[0]);
							float dyNextStored = (float)((double)first[1] - next[1]);
							float dzNextStored = (float)((double)first[2] - next[2]);

							normal[0] = (float)((double)dxNextStored + dxPreviousStored);
							normal[1] = (float)((double)dyNextStored + dyPreviousStored);
							normal[2] = (float)((double)dzNextStored + dzPreviousStored);
						} else {
							float dyPrevious = (float)((double)first[1] - previous[1]);
							double dzNext = (double)first[2] - next[2];
							double dxNext = (double)first[0] - next[0];
							float dzPrevious = (float)((double)first[2] - previous[2]);
							double dxPrevious = (double)first[0] - previous[0];

							normal[1] = (float)(((double)first[1] - next[1]) + dyPrevious);
							normal[2] = (float)(dzNext + dzPrevious);
							normal[0] = (float)(dxNext + dxPrevious);
						}
					} else {
						normal[0] = (float)(sumX / countStored);
						normal[1] = (float)((double)sumYStored / countStored);
						normal[2] = (float)((double)sumZStored / countStored);
					}

					_rd->normalize3Acc(normal);
				}

				normal += 3;
			}
		}
	}
}

int RenderDroid::Model3Module::getMeshMatrix(RdThing *thing, RdMatrix34 orient, uint nodeNum, RdMatrix34 meshOrient) {
	if (nodeNum >= thing->data.model3->numHNodes)
		return 0;

	memcpy(meshOrient, thing->jointMatrices[nodeNum], sizeof(RdMatrix34));

	return 1;
}

void RenderDroid::Model3Module::buildExpandedRadius(RdModel3 *model3, RdModel3HNode *hNode, RdMatrix34 parentMatrix) {
	RdMatrix34 nodeMatrix;
	RdMatrix34 translation;
	RdMatrix34 accumulated;

	_rd->build34(nodeMatrix, hNode->pyr, hNode->xyz);
	_rd->buildTranslate34(translation, hNode->pivot);
	_rd->postMultiply34(translation, nodeMatrix);

	if (hNode->parentNode) {
		RdVector3 negatedPivot;
		negatedPivot[0] = -hNode->parentNode->pivot[0];
		negatedPivot[1] = -hNode->parentNode->pivot[1];
		negatedPivot[2] = -hNode->parentNode->pivot[2];
		_rd->postTranslate34(translation, negatedPivot);
	}

	_rd->multiply34(accumulated, parentMatrix, translation);

	if (hNode->meshNodeIndex != -1) {
		RdModel3Mesh *mesh = &model3->geo[0].meshes[hNode->meshNodeIndex];

		for (uint i = 0; i < mesh->numVertices; i++) {
			RdVector3 transformed;

			_rd->transformPoint34(transformed, mesh->vertices[i], accumulated);

			double length = sqrt((double)transformed[0] * transformed[0] + (double)transformed[1] * transformed[1] + (double)transformed[2] * transformed[2]);
			if (length > _maxGeoRadius)
				_maxGeoRadius = (float)length;
		}
	}

	if (hNode->numChildren) {
		RdModel3HNode *child = hNode->firstChildNode;

		for (uint i = 0; i < hNode->numChildren; i++) {
			buildExpandedRadius(model3, child, accumulated);
			child = child->siblingNode;
		}
	}
}

void RenderDroid::Model3Module::calcModelBoundingBox(RdModel3 *model3, RdModel3HNode *hNode, RdMatrix34 parentMatrix) {
	RdMatrix34 nodeMatrix;
	RdMatrix34 translation;
	RdMatrix34 accumulated;

	_rd->build34(nodeMatrix, hNode->pyr, hNode->xyz);
	_rd->buildTranslate34(translation, hNode->pivot);
	_rd->postMultiply34(translation, nodeMatrix);

	if (hNode->parentNode) {
		RdVector3 negatedPivot;
		negatedPivot[0] = -hNode->parentNode->pivot[0];
		negatedPivot[1] = -hNode->parentNode->pivot[1];
		negatedPivot[2] = -hNode->parentNode->pivot[2];
		_rd->postTranslate34(translation, negatedPivot);
	}

	_rd->multiply34(accumulated, parentMatrix, translation);

	if (hNode->meshNodeIndex != -1) {
		RdModel3Mesh *mesh = &model3->geo[0].meshes[hNode->meshNodeIndex];

		for (uint i = 0; i < mesh->numVertices; i++) {
			RdVector3 transformed;

			_rd->transformPoint34(transformed, mesh->vertices[i], accumulated);

			if (transformed[0] > (double)model3->bound.max[0])
				model3->bound.max[0] = transformed[0];
			if (transformed[1] > (double)model3->bound.max[1])
				model3->bound.max[1] = transformed[1];
			if (transformed[2] > (double)model3->bound.max[2])
				model3->bound.max[2] = transformed[2];
			if (transformed[0] < (double)model3->bound.min[0])
				model3->bound.min[0] = transformed[0];
			if (transformed[1] < (double)model3->bound.min[1])
				model3->bound.min[1] = transformed[1];
			if (transformed[2] < (double)model3->bound.min[2])
				model3->bound.min[2] = transformed[2];
		}
	}

	if (hNode->numChildren) {
		RdModel3HNode *child = hNode->firstChildNode;

		for (uint i = 0; i < hNode->numChildren; i++) {
			calcModelBoundingBox(model3, child, accumulated);
			child = child->siblingNode;
		}
	}
}

int RenderDroid::Model3Module::hasBytes(Common::SeekableReadStream *file, uint count, uint elementSize) {
	return (uint64)count * elementSize <= (uint64)(file->size() - file->pos());
}

RdModel3 *RenderDroid::Model3Module::load(char *filename) {
	RdModel3 *model3 = new RdModel3;
	memset(model3, 0, sizeof(RdModel3));

	if (loadEntry(filename, model3)) {
		loadPostProcess(model3);
		return model3;
	}

	destroy(model3);
	return nullptr;
}

int RenderDroid::Model3Module::loadEntry(char *filename, RdModel3 *model3) {
	Common::SeekableReadStream *file = _rd->_hs->openFile(filename);

	if (!file)
		return 0;

	uint32 magic = file->readUint32LE();

	if (magic == 0x4D4F444C) {
		int result = loadEntryBinary(file, model3);
		delete file;
		return result;
	}

	delete file;
	return loadEntryText(filename, model3);
}

int RenderDroid::Model3Module::loadEntryBinary(Common::SeekableReadStream *file, RdModel3 *model3) {
	Common::Array<int> faceMaterialIndices;
	char *materialNames = nullptr;
	bool ok = true;

	newEntry(model3);

	uint numMaterials = file->readUint32LE();

	if (!hasBytes(file, numMaterials, 32)) {
		warning("RenderDroid::Model3Module::loadEntryBinary(): invalid material count");
		return 0;
	}

	model3->numMaterials = numMaterials;
	model3->materials = nullptr;

	if (numMaterials) {
		materialNames = new char[32 * numMaterials];
		file->read(materialNames, 32 * numMaterials);
	}

	file->read(model3->name, 32);
	model3->name[31] = 0;
	model3->index = file->readSint32LE();
	uint numGeos = file->readUint32LE();

	if (numGeos > 4) {
		warning("RenderDroid::Model3Module::loadEntryBinary(): invalid geoset count");
		ok = false;
		numGeos = 0;
	}

	model3->numGeos = numGeos;

	for (uint g = 0; g < numGeos && ok; g++) {
		RdModel3Geo *geo = &model3->geo[g];
		uint numMeshes = file->readUint32LE();

		if (!hasBytes(file, numMeshes, 60)) {
			warning("RenderDroid::Model3Module::loadEntryBinary(): invalid mesh count");
			ok = false;
			break;
		}

		geo->meshes = new RdModel3Mesh[numMeshes];
		memset(geo->meshes, 0, sizeof(RdModel3Mesh) * numMeshes);
		geo->numMeshes = numMeshes;

		for (uint m = 0; m < numMeshes && ok; m++) {
			RdModel3Mesh *mesh = &geo->meshes[m];

			file->read(mesh->name, 32);
			mesh->name[31] = 0;
			mesh->num = file->readUint32LE();
			mesh->geometryMode = file->readSint32LE();
			mesh->lightingMode = file->readSint32LE();
			mesh->textureMode = file->readSint32LE();
			uint numVertices = file->readUint32LE();
			uint numTexVertices = file->readUint32LE();
			uint numFaces = file->readUint32LE();

			if (!hasBytes(file, numVertices, 32) || !hasBytes(file, numTexVertices, 8) || !hasBytes(file, numFaces, 76)) {
				warning("RenderDroid::Model3Module::loadEntryBinary(): invalid mesh size");
				ok = false;
				break;
			}

			mesh->numVertices = numVertices;
			mesh->numTexVertices = numTexVertices;

			if (numVertices) {
				mesh->vertices = new RdVector3[numVertices];
				mesh->staticVertexIntensities = new float[numVertices];
				mesh->dynamicVertexIntensities = new float[numVertices];
			}

			if (numTexVertices)
				mesh->texVertices = new RdVector2[numTexVertices];

			for (uint i = 0; i < numVertices; i++) {
				mesh->vertices[i][0] = file->readFloatLE();
				mesh->vertices[i][1] = file->readFloatLE();
				mesh->vertices[i][2] = file->readFloatLE();
			}

			for (uint i = 0; i < numTexVertices; i++) {
				mesh->texVertices[i][0] = file->readFloatLE();
				mesh->texVertices[i][1] = file->readFloatLE();
			}

			for (uint i = 0; i < numVertices; i++)
				mesh->staticVertexIntensities[i] = file->readFloatLE();

			for (uint i = 0; i < numVertices; i++)
				mesh->dynamicVertexIntensities[i] = file->readFloatLE();

			if (numFaces) {
				mesh->faces = new RdFace3[numFaces];
				memset(mesh->faces, 0, sizeof(RdFace3) * numFaces);
				mesh->numFaces = numFaces;
			}

			for (uint f = 0; f < numFaces && ok; f++) {
				RdFace3 *face = &mesh->faces[f];

				face->num = file->readUint32LE();
				face->type = file->readUint32LE();
				face->geometryMode = file->readSint32LE();
				face->lightingMode = file->readSint32LE();
				face->textureMode = file->readSint32LE();
				uint faceVertices = file->readUint32LE();
				file->skip(4);
				uint hasTexVertices = file->readUint32LE();
				uint hasMaterial = file->readUint32LE();
				face->curCel = file->readSint32LE();
				face->uvDelta[0] = file->readFloatLE();
				face->uvDelta[1] = file->readFloatLE();
				face->extraLight = file->readFloatLE();
				face->reserved52 = file->readSint32LE();
				face->reserved56 = file->readSint32LE();
				face->reserved60 = file->readSint32LE();
				face->normal[0] = file->readFloatLE();
				face->normal[1] = file->readFloatLE();
				face->normal[2] = file->readFloatLE();

				if (!hasBytes(file, faceVertices, hasTexVertices ? 8 : 4)) {
					warning("RenderDroid::Model3Module::loadEntryBinary(): invalid face size");
					ok = false;
					break;
				}

				face->numVertices = faceVertices;

				if (faceVertices) {
					face->vertices = new int[faceVertices];

					for (uint i = 0; i < faceVertices; i++)
						face->vertices[i] = file->readSint32LE();
				}

				if (hasTexVertices) {
					face->texVertices = new int[faceVertices];

					for (uint i = 0; i < faceVertices; i++)
						face->texVertices[i] = file->readSint32LE();
				}

				if (hasMaterial)
					faceMaterialIndices.push_back(file->readSint32LE());
				else
					faceMaterialIndices.push_back(-1);
			}

			if (!ok)
				break;

			if (numVertices) {
				mesh->vertexNormals = new RdVector3[numVertices];

				for (uint i = 0; i < numVertices; i++) {
					mesh->vertexNormals[i][0] = file->readFloatLE();
					mesh->vertexNormals[i][1] = file->readFloatLE();
					mesh->vertexNormals[i][2] = file->readFloatLE();
				}
			}

			mesh->castShadow = file->readSint32LE();
			mesh->userLightValue = file->readFloatLE();
			mesh->radius = file->readFloatLE();

			for (uint i = 0; i < 3; i++)
				mesh->bound.min[i] = file->readFloatLE();

			for (uint i = 0; i < 3; i++)
				mesh->bound.max[i] = file->readFloatLE();
		}
	}

	if (ok) {
		model3->curGeoSet = file->readUint32LE();
		uint numHNodes = file->readUint32LE();

		if (!hasBytes(file, numHNodes, 184)) {
			warning("RenderDroid::Model3Module::loadEntryBinary(): invalid hierarchy node count");
			ok = false;
		} else {
			model3->hNodes = new RdModel3HNode[numHNodes];
			memset(model3->hNodes, 0, sizeof(RdModel3HNode) * numHNodes);
			model3->numHNodes = numHNodes;
		}

		for (uint i = 0; i < model3->numHNodes && ok; i++) {
			RdModel3HNode *hNode = &model3->hNodes[i];

			file->read(hNode->name, 64);
			hNode->name[63] = 0;
			hNode->flags = file->readUint32LE();
			hNode->num = file->readUint32LE();
			hNode->type = file->readUint32LE();
			hNode->meshNodeIndex = file->readSint32LE();
			hNode->depth = file->readUint32LE();
			uint hasParent = file->readUint32LE();
			hNode->numChildren = file->readUint32LE();
			uint hasFirstChild = file->readUint32LE();
			uint hasSibling = file->readUint32LE();

			for (uint k = 0; k < 3; k++)
				hNode->pivot[k] = file->readFloatLE();

			for (uint k = 0; k < 3; k++)
				hNode->xyz[k] = file->readFloatLE();

			for (uint k = 0; k < 3; k++)
				hNode->pyr[k] = file->readFloatLE();

			for (uint row = 0; row < 4; row++) {
				for (uint col = 0; col < 3; col++)
					hNode->mat[row][col] = file->readFloatLE();
			}

			if (hasParent) {
				uint index = file->readUint32LE();

				if (index >= model3->numHNodes) {
					warning("RenderDroid::Model3Module::loadEntryBinary(): invalid parent node index");
					ok = false;
					break;
				}

				hNode->parentNode = &model3->hNodes[index];
			}

			if (hasFirstChild) {
				uint index = file->readUint32LE();

				if (index >= model3->numHNodes) {
					warning("RenderDroid::Model3Module::loadEntryBinary(): invalid child node index");
					ok = false;
					break;
				}

				hNode->firstChildNode = &model3->hNodes[index];
			}

			if (hasSibling) {
				uint index = file->readUint32LE();

				if (index >= model3->numHNodes) {
					warning("RenderDroid::Model3Module::loadEntryBinary(): invalid sibling node index");
					ok = false;
					break;
				}

				hNode->siblingNode = &model3->hNodes[index];
			}
		}
	}

	if (ok) {
		model3->radius = file->readFloatLE();

		for (uint i = 0; i < 3; i++)
			model3->bound.min[i] = file->readFloatLE();

		for (uint i = 0; i < 3; i++)
			model3->bound.max[i] = file->readFloatLE();

		for (uint i = 0; i < 3; i++)
			model3->geomCenter[i] = file->readFloatLE();

		for (uint i = 0; i < 3; i++)
			model3->insertOffset[i] = file->readFloatLE();

		if (numMaterials) {
			model3->materials = new RdMaterial *[numMaterials];
			memset(model3->materials, 0, sizeof(RdMaterial *) * numMaterials);
		}

		for (uint i = 0; i < numMaterials; i++) {
			char materialName[33];
			memcpy(materialName, materialNames + 32 * i, 32);
			materialName[32] = 0;
			model3->materials[i] = _rd->_material.load(materialName, 0, 0);
		}

		uint faceIndex = 0;

		for (uint g = 0; g < model3->numGeos; g++) {
			RdModel3Geo *geo = &model3->geo[g];

			for (uint m = 0; m < geo->numMeshes; m++) {
				RdModel3Mesh *mesh = &geo->meshes[m];

				for (uint f = 0; f < mesh->numFaces; f++) {
					int materialIndex = faceMaterialIndices[faceIndex++];
					RdFace3 *face = &mesh->faces[f];

					if (materialIndex >= 0 && (uint)materialIndex < numMaterials) {
						face->material = model3->materials[materialIndex];
					} else {
						if (materialIndex >= 0)
							warning("RenderDroid::Model3Module::loadEntryBinary(): invalid face material index");

						face->material = nullptr;
					}
				}
			}
		}
	}

	delete[] materialNames;

	return ok ? 1 : 0;
}

int RenderDroid::Model3Module::loadEntryText(char *filename, RdModel3 *model3) {
	StdConffile *conffile = _rd->_stdConffile;
	char section[1024];
	char name[1024];
	char faceLine[1024];
	bool success = false;

	newEntry(model3);
	Common::strlcpy(model3->name, _rd->_stdFile->fromPath(filename), 32);

	if (!conffile->open(filename))
		return 0;

	do {
		if (!conffile->readLine() || sscanf(conffile->getLine(), " section: %s", section) != 1)
			break;

		int versionMajor;
		int versionMinor;

		if (!conffile->readLine())
			break;

		sscanf(conffile->getLine(), " 3do %d.%d", &versionMajor, &versionMinor);

		uint numMaterials;

		if (!conffile->readLine() || sscanf(conffile->getLine(), " section: %s", section) != 1 ||
				!conffile->readLine() || sscanf(conffile->getLine(), " materials %d", &numMaterials) != 1)
			break;

		if (numMaterials) {
			if (numMaterials >= 0x80) {
				warning("RenderDroid::Model3Module::loadEntryText(): too many materials in %s", filename);
				break;
			}

			model3->materials = new RdMaterial *[numMaterials];
			memset(model3->materials, 0, sizeof(RdMaterial *) * numMaterials);
			model3->numMaterials = numMaterials;

			char *paths = new char[1024 * numMaterials];
			memset(paths, 0, 1024 * numMaterials);
			bool pathsOk = true;

			for (uint i = 0; i < numMaterials; i++) {
				int index;

				if (!conffile->readLine() || sscanf(conffile->getLine(), " %d: %s", &index, name) != 2) {
					pathsOk = false;
					break;
				}

				if (index < 0 || (uint)index >= numMaterials) {
					warning("RenderDroid::Model3Module::loadEntryText(): invalid material index in %s", filename);
					pathsOk = false;
					break;
				}

				Common::strlcpy(&paths[1024 * index], name, 1024);
			}

			for (uint i = 0; i < numMaterials && pathsOk; i++) {
				model3->materials[i] = _rd->_material.load(&paths[1024 * i], 0, 0);

				if (!model3->materials[i])
					pathsOk = false;
			}

			delete[] paths;

			if (!pathsOk)
				break;
		}

		model3->numMaterials = numMaterials;

		float radius;

		if (!conffile->readLine() || sscanf(conffile->getLine(), " section: %s", section) != 1 ||
				!conffile->readLine() || sscanf(conffile->getLine(), " radius %f", &radius) != 1)
			break;

		model3->radius = radius;

		float offsetX;
		float offsetY;
		float offsetZ;

		if (!conffile->readLine() || sscanf(conffile->getLine(), " insert offset %f %f %f", &offsetX, &offsetY, &offsetZ) != 3)
			break;

		model3->insertOffset[0] = offsetX;
		model3->insertOffset[1] = offsetY;
		model3->insertOffset[2] = offsetZ;

		uint numGeos;

		if (!conffile->readLine() || sscanf(conffile->getLine(), " geosets %d", &numGeos) != 1)
			break;

		if (numGeos > 4) {
			warning("RenderDroid::Model3Module::loadEntryText(): too many geosets in %s", filename);
			break;
		}

		model3->numGeos = numGeos;
		bool ok = true;

		for (uint g = 0; g < numGeos && ok; g++) {
			RdModel3Geo *geo = &model3->geo[g];
			int geosetIndex;
			uint numMeshes;

			if (!conffile->readLine() || sscanf(conffile->getLine(), " geoset %d", &geosetIndex) != 1 ||
					!conffile->readLine() || sscanf(conffile->getLine(), " meshes %d", &numMeshes) != 1) {
				ok = false;
				break;
			}

			geo->meshes = new RdModel3Mesh[numMeshes];
			memset(geo->meshes, 0, sizeof(RdModel3Mesh) * numMeshes);
			geo->numMeshes = numMeshes;

			for (uint m = 0; m < numMeshes && ok; m++) {
				RdModel3Mesh *mesh = &geo->meshes[m];
				int meshIndex;
				int castShadow;
				uint numVertices;
				uint numTexVertices;
				uint numFaces;

				mesh->num = m;

				if (!conffile->readLine() || sscanf(conffile->getLine(), " mesh %d", &meshIndex) != 1 ||
						!conffile->readLine() || sscanf(conffile->getLine(), " name %s", name) != 1) {
					ok = false;
					break;
				}

				Common::strlcpy(mesh->name, name, 32);

				if (!conffile->readLine() || sscanf(conffile->getLine(), " radius %f", &mesh->radius) != 1 ||
						!conffile->readLine()) {
					ok = false;
					break;
				}

				if (sscanf(conffile->getLine(), " shadow %d", &castShadow) == 1) {
					mesh->castShadow = castShadow;

					if (!conffile->readLine()) {
						ok = false;
						break;
					}
				}

				if (sscanf(conffile->getLine(), " geometrymode %d", &mesh->geometryMode) != 1 ||
						!conffile->readLine() || sscanf(conffile->getLine(), " lightingmode %d", &mesh->lightingMode) != 1 ||
						!conffile->readLine() || sscanf(conffile->getLine(), " texturemode %d", &mesh->textureMode) != 1 ||
						!conffile->readLine() || sscanf(conffile->getLine(), " vertices %d", &numVertices) != 1 ||
						numVertices > 0x200) {
					ok = false;
					break;
				}

				if (numVertices) {
					mesh->vertices = new RdVector3[numVertices];
					mesh->staticVertexIntensities = new float[numVertices];
					mesh->dynamicVertexIntensities = new float[numVertices];
					memset(mesh->dynamicVertexIntensities, 0, numVertices);
				}

				for (uint i = 0; i < numVertices; i++) {
					int index;
					float x;
					float y;
					float z;
					float intensity;

					if (!conffile->readLine() || sscanf(conffile->getLine(), " %d: %f %f %f %f", &index, &x, &y, &z, &intensity) != 5) {
						ok = false;
						break;
					}

					mesh->vertices[i][0] = x;
					mesh->vertices[i][1] = y;
					mesh->vertices[i][2] = z;
					mesh->staticVertexIntensities[i] = intensity;
				}

				if (!ok)
					break;

				mesh->numVertices = numVertices;

				if (!conffile->readLine() || sscanf(conffile->getLine(), " texture vertices %d", &numTexVertices) != 1 ||
						numTexVertices > 0x300) {
					ok = false;
					break;
				}

				if (numTexVertices)
					mesh->texVertices = new RdVector2[numTexVertices];

				for (uint i = 0; i < numTexVertices; i++) {
					int index;
					float u;
					float v;

					if (!conffile->readLine() || sscanf(conffile->getLine(), " %d: %f %f", &index, &u, &v) != 3) {
						ok = false;
						break;
					}

					mesh->texVertices[i][0] = u;
					mesh->texVertices[i][1] = v;
				}

				if (!ok)
					break;

				mesh->numTexVertices = numTexVertices;

				if (!conffile->readLine()) {
					ok = false;
					break;
				}

				if (numVertices)
					mesh->vertexNormals = new RdVector3[numVertices];

				for (uint i = 0; i < numVertices; i++) {
					int index;
					float x;
					float y;
					float z;

					if (!conffile->readLine() || sscanf(conffile->getLine(), " %d: %f %f %f", &index, &x, &y, &z) != 4) {
						ok = false;
						break;
					}

					mesh->vertexNormals[i][0] = x;
					mesh->vertexNormals[i][1] = y;
					mesh->vertexNormals[i][2] = z;
				}

				if (!ok)
					break;

				if (!conffile->readLine() || sscanf(conffile->getLine(), " faces %d", &numFaces) != 1 || numFaces > 0x200) {
					ok = false;
					break;
				}

				if (numFaces) {
					mesh->faces = new RdFace3[numFaces];
					memset(mesh->faces, 0, sizeof(RdFace3) * numFaces);
				}

				mesh->numFaces = numFaces;

				for (uint f = 0; f < numFaces && ok; f++) {
					RdFace3 *face = &mesh->faces[f];
					int materialIndex;
					int extraLightValid;
					float extraLight = 0.0f;
					char *token;

					if (!conffile->readLine()) {
						ok = false;
						break;
					}

					_rd->_face.newEntry(face);
					Common::strlcpy(faceLine, conffile->getLine(), 1024);

					strtok(faceLine, " \t");
					token = strtok(nullptr, " \t");

					if (!token) {
						ok = false;
						break;
					}

					materialIndex = atoi(token);
					face->num = f;

					if (materialIndex == -1) {
						face->material = nullptr;
					} else if (materialIndex < 0 || (uint)materialIndex >= model3->numMaterials) {
						warning("RenderDroid::Model3Module::loadEntryText(): invalid face material index in %s", filename);
						ok = false;
						break;
					} else {
						face->material = model3->materials[materialIndex];
					}

					token = strtok(nullptr, " \t");
					if (!token || sscanf(token, "%x", &face->type) != 1) {
						ok = false;
						break;
					}

					token = strtok(nullptr, " \t");
					if (!token || sscanf(token, "%d", &face->geometryMode) != 1) {
						ok = false;
						break;
					}

					token = strtok(nullptr, " \t");
					if (!token || sscanf(token, "%d", &face->lightingMode) != 1) {
						ok = false;
						break;
					}

					token = strtok(nullptr, " \t");
					if (!token || sscanf(token, "%d", &face->textureMode) != 1) {
						ok = false;
						break;
					}

					token = strtok(nullptr, " \t");
					extraLightValid = token ? sscanf(token, "%f", &extraLight) : 0;
					if (extraLightValid != 1) {
						ok = false;
						break;
					}

					face->extraLight = extraLight;

					token = strtok(nullptr, " \t");
					if (!token) {
						ok = false;
						break;
					}

					uint faceVertices = atoi(token);
					if (faceVertices == 0 || faceVertices > 0x18) {
						ok = false;
						break;
					}

					face->numVertices = faceVertices;
					face->vertices = new int[faceVertices];

					if (face->material && (face->material->flags & 2)) {
						face->texVertices = new int[faceVertices];

						for (uint i = 0; i < faceVertices; i++) {
							token = strtok(nullptr, " \t,");
							if (!token) {
								ok = false;
								break;
							}
							face->vertices[i] = atoi(token);

							token = strtok(nullptr, " \t,");
							if (!token) {
								ok = false;
								break;
							}
							face->texVertices[i] = atoi(token);
						}
					} else {
						for (uint i = 0; i < faceVertices; i++) {
							token = strtok(nullptr, " \t,");
							if (!token) {
								ok = false;
								break;
							}
							face->vertices[i] = atoi(token);

							if (!strtok(nullptr, " \t,")) {
								ok = false;
								break;
							}
						}
					}
				}

				if (!ok)
					break;

				if (!conffile->readLine()) {
					ok = false;
					break;
				}

				for (uint f = 0; f < mesh->numFaces; f++) {
					int index;
					float x;
					float y;
					float z;

					if (!conffile->readLine() || sscanf(conffile->getLine(), " %d: %f %f %f", &index, &x, &y, &z) != 4) {
						ok = false;
						break;
					}

					mesh->faces[f].normal[0] = x;
					mesh->faces[f].normal[1] = y;
					mesh->faces[f].normal[2] = z;
				}
			}
		}

		if (!ok)
			break;

		uint numHNodes;

		if (!conffile->readLine() || sscanf(conffile->getLine(), " section: %s", section) != 1 ||
				!conffile->readLine() || sscanf(conffile->getLine(), " hierarchy nodes %d", &numHNodes) != 1)
			break;

		model3->hNodes = new RdModel3HNode[numHNodes];
		memset(model3->hNodes, 0, sizeof(RdModel3HNode) * numHNodes);
		model3->numHNodes = numHNodes;

		for (uint i = 0; i < numHNodes && ok; i++) {
			RdModel3HNode *hNode = &model3->hNodes[i];
			int index;
			int parentIndex;
			int firstChildIndex;
			int siblingIndex;
			float x;
			float y;
			float z;
			float pitch;
			float yaw;
			float roll;
			float pivotX;
			float pivotY;
			float pivotZ;

			hNode->num = i;

			if (!conffile->readLine() || sscanf(conffile->getLine(), " %d: %x %x %d %d %d %d %d %f %f %f %f %f %f %f %f %f %63s",
					&index, &hNode->flags, &hNode->type, &hNode->meshNodeIndex, &parentIndex, &firstChildIndex,
					&siblingIndex, &hNode->numChildren, &x, &y, &z, &pitch, &yaw, &roll, &pivotX, &pivotY, &pivotZ,
					hNode->name) != 18) {
				ok = false;
				break;
			}

			if (parentIndex < -1 || parentIndex >= (int)numHNodes || firstChildIndex < -1 || firstChildIndex >= (int)numHNodes ||
					siblingIndex < -1 || siblingIndex >= (int)numHNodes) {
				warning("RenderDroid::Model3Module::loadEntryText(): invalid node index in %s", filename);
				ok = false;
				break;
			}

			hNode->parentNode = parentIndex == -1 ? nullptr : &model3->hNodes[parentIndex];
			hNode->firstChildNode = firstChildIndex == -1 ? nullptr : &model3->hNodes[firstChildIndex];
			hNode->siblingNode = siblingIndex == -1 ? nullptr : &model3->hNodes[siblingIndex];

			hNode->xyz[0] = x;
			hNode->xyz[1] = y;
			hNode->xyz[2] = z;
			hNode->pyr[0] = pitch;
			hNode->pyr[1] = yaw;
			hNode->pyr[2] = roll;
			hNode->pivot[0] = pivotX;
			hNode->pivot[1] = pivotY;
			hNode->pivot[2] = pivotZ;
		}

		if (!ok)
			break;

		calcNodeDepths(model3);
		success = true;
	} while (false);

	conffile->close();

	return success ? 1 : 0;
}

void RenderDroid::Model3Module::loadPostProcess(RdModel3 *model3) {
	calcBoundingBoxes(model3);
	calcRadii(model3);
	calcFaceNormals(model3);
	calcVertexNormals(model3);
	calcNodeDepths(model3);
	buildShadowModel(model3);

	if (_rd->isDemo() && !validate(model3))
		error("RenderDroid::Model3Module::loadPostProcess(): rdModel3_Validate(pModel3)");
}

int RenderDroid::Model3Module::validate(RdModel3 *model3) {
	for (uint g = 0; g < model3->numGeos; g++) {
		RdModel3Geo *geo = &model3->geo[g];

		for (uint m = 0; m < geo->numMeshes; m++) {
			RdModel3Mesh *mesh = &geo->meshes[m];

			for (uint f = 0; f < mesh->numFaces; f++) {
				RdFace3 *face = &mesh->faces[f];

				if (face->material) {
					if (!(face->material->flags & 2) && face->geometryMode > 3)
						return 0;
				} else if (face->geometryMode > 0) {
					return 0;
				}
			}
		}
	}

	return 1;
}

void RenderDroid::Model3Module::destroy(RdModel3 *&model3) {
	if (model3) {
		freeEntry(model3);
		delete model3;
		model3 = nullptr;
	} else {
		warning("RenderDroid::Model3Module::destroy(): attempt to free NULL model ptr");
	}
}

void RenderDroid::Model3Module::freeEntry(RdModel3 *model3) {
	for (uint g = 0; g < model3->numGeos; g++) {
		RdModel3Geo *geo = &model3->geo[g];

		for (uint m = 0; m < geo->numMeshes; m++) {
			RdModel3Mesh *mesh = &geo->meshes[m];

			if (_rd->_remasteredLookRenderer)
				_rd->_remasteredLookRenderer->notifyFree(mesh->vertices);

			delete[] mesh->vertices;
			mesh->vertices = nullptr;

			delete[] mesh->texVertices;
			mesh->texVertices = nullptr;

			if (mesh->faces) {
				for (uint f = 0; f < mesh->numFaces; f++)
					_rd->_face.freeEntry(&mesh->faces[f]);

				delete[] mesh->faces;
				mesh->faces = nullptr;
			}

			delete[] mesh->staticVertexIntensities;
			mesh->staticVertexIntensities = nullptr;

			delete[] mesh->dynamicVertexIntensities;
			mesh->dynamicVertexIntensities = nullptr;

			delete[] mesh->vertexNormals;
			mesh->vertexNormals = nullptr;

			delete[] mesh->shadowVerts;
			mesh->shadowVerts = nullptr;
		}

		delete[] geo->meshes;
		geo->meshes = nullptr;
	}

	delete[] model3->hNodes;
	model3->hNodes = nullptr;

	if (model3->materials) {
		for (uint i = 0; i < model3->numMaterials; i++) {
			if (model3->materials[i])
				_rd->_material.destroy(model3->materials[i]);
		}

		delete[] model3->materials;
		model3->materials = nullptr;
	}
}

int RenderDroid::Model3Module::draw(RdThing *thing, RdMatrix34 mat, RdThing *parentThing) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 center;
	RdVector3 worldCenter = { 0.0f, 0.0f, 0.0f };

	_curThing = thing;
	_curModel3 = thing->data.model3;
	if (_rd->isDemo()) {
		// The demo tests the origin of the model, not its geometric center
		_rd->transformPoint34(center, mat[3], camera->viewMatrix);
	} else {
		_rd->transformPoint34(center, _curModel3->geomCenter, mat);
		memcpy(worldCenter, center, sizeof(worldCenter));
		_rd->transformPoint34Acc(center, camera->viewMatrix);
	}

	// The demo uses the culling flags of the renderer, not the field of the thing
	bool cull = _rd->isDemo() ? (_rd->_curCullFlags & 2) != 0 : thing->frustrumCull != 0;

	if (cull)
		_thingFrustrumCull = _rd->_clip.sphereInFrustrum(camera->clipFrustum, center, _curModel3->radius);
	else
		_thingFrustrumCull = 1;

	if (_thingFrustrumCull == 2)
		return 0;

	if (_rd->_remasteredLookRenderer) {
		const char *parentName = parentThing ? parentThing->data.model3->name : nullptr;

		_rd->_remasteredLookRenderer->drawBeginModel3(_curModel3, worldCenter, _curModel3->radius, &mat[0][0], parentName);
	}

	_thingFrustrumCull = 1;

	uint curGeoSet;

	if (thing->curGeoSet == -1)
		curGeoSet = _curModel3->curGeoSet;
	else
		curGeoSet = _curThing->curGeoSet;

	if (curGeoSet >= 4) {
		warning("RenderDroid::Model3Module::draw(): invalid geoset %u", curGeoSet);
		return 0;
	}

	_curGeo = &_curModel3->geo[curGeoSet];

	// The demo builds the joint matrices once per frame
	if (!_rd->isDemo() || _curThing->jointsFrame != _rd->getFrameTrue())
		_rd->_puppet.buildJointMatrices(_curThing, mat);

	_curGeometryMode = _curThing->curGeometryMode;

	if (_curGeometryMode >= _rd->_curGeometryMode)
		_curGeometryMode = _rd->_curGeometryMode;

	if ((_rd->_curRenderOptions & 2) && camera->ambientLight >= 1.0f) {
		_curLightingMode = 0;
	} else {
		_curLightingMode = _curThing->curLightingMode;

		if (_curLightingMode >= _rd->_curLightingMode)
			_curLightingMode = _rd->_curLightingMode;
	}

	_curTextureMode = _curThing->curTextureMode;

	if (_curTextureMode >= _rd->_curTextureMode)
		_curTextureMode = _rd->_curTextureMode;

	if (_rd->_remasteredLookRenderer && !_rd->_remasteredLookRenderer->isSoftwareEnabled())
		_curLightingMode = 1;
	else if (_curLightingMode > 1) {
		_numGeoLights = 0;

		for (uint i = 0; i < camera->numLights; i++) {
			RdLight *light = camera->lights[i];

			if (!light->active)
				continue;

			double distSquared;

			if (light->type >= 2 && _rd->_remastered) {
				float dxStored = (float)((double)mat[3][0] - camera->lightPos[i][0]);
				float dyStored = (float)((double)mat[3][1] - camera->lightPos[i][1]);
				float dzStored = (float)((double)mat[3][2] - camera->lightPos[i][2]);

				distSquared = (float)(((double)dyStored * dyStored + (double)dxStored * dxStored) + (double)dzStored * dzStored);
			} else if (light->type >= 2) {
				double dy = (double)mat[3][1] - camera->lightPos[i][1];
				double dz = (double)mat[3][2] - camera->lightPos[i][2];
				double dx = (double)mat[3][0] - camera->lightPos[i][0];
				distSquared = dx * dx + dy * dy + dz * dz;
			} else {
				distSquared = 0.0;
			}

			double range = (double)light->falloffMin + _curModel3->radius;

			if (distSquared < range * range) {
				_geoLights[_numGeoLights] = light;
				_numGeoLights++;
			}
		}
	}

	drawHNode(_curModel3->hNodes);
	_numDrawnModels++;

	if (_rd->_remasteredLookRenderer)
		_rd->_remasteredLookRenderer->drawEndModel3();

	return 1;
}

void RenderDroid::Model3Module::drawHNode(RdModel3HNode *hNode) {
	int meshNodeIndex = hNode->meshNodeIndex;

	if (meshNodeIndex != -1 && _curThing->abMeshVisible[hNode->num]) {
		if (hNode->flags & 2) {
			RdModel3HNode *ancestor = hNode->parentNode;

			while (ancestor->flags & 2)
				ancestor = ancestor->parentNode;

			_curGeo->meshes[meshNodeIndex].lightingMode = 6;
			_curGeo->meshes[hNode->meshNodeIndex].userLightValue = _curGeo->meshes[ancestor->meshNodeIndex].userLightValue;
		}

		if (_rd->_remasteredLookRenderer) {
			_rd->_remasteredLookRenderer->drawModel3Mesh(&_curGeo->meshes[hNode->meshNodeIndex], &_curThing->jointMatrices[hNode->num][0][0]);

			if (_rd->_remasteredLookRenderer->isSoftwareEnabled())
				drawMesh(&_curGeo->meshes[hNode->meshNodeIndex], _curThing->jointMatrices[hNode->num]);
		} else {
			drawMesh(&_curGeo->meshes[hNode->meshNodeIndex], _curThing->jointMatrices[hNode->num]);
		}
	}

	if (hNode->numChildren) {
		RdModel3HNode *child = hNode->firstChildNode;

		for (uint i = 0; i < hNode->numChildren; i++) {
			if (!_curThing->abJointAmputate[child->num])
				drawHNode(child);

			child = child->siblingNode;
		}
	}
}

void RenderDroid::Model3Module::drawMesh(RdModel3Mesh *mesh, RdMatrix34 mat) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 center;
	RdMatrix34 modelView;
	RdMatrix34 inverse;

	_curMesh = mesh;

	if (!mesh->geometryMode)
		return;

	if (mesh->numVertices > 512) {
		warning("RenderDroid::Model3Module::drawMesh(): too many vertices in mesh %s", mesh->name);
		return;
	}

	_rd->transformPoint34(center, mat[3], camera->viewMatrix);

	if (_thingFrustrumCull)
		_meshFrustrumCull = (_rd->_curCullFlags & 1) ? _rd->_clip.sphereInFrustrum(camera->clipFrustum, center, _curMesh->radius) : 1;
	else
		_meshFrustrumCull = 0;

	if (_meshFrustrumCull == 2)
		return;

	if (_clipPlaneActive) {
		if (_rd->_remastered) {
			float sideStored = (float)((((double)mat[3][1] - _clipPt[1]) * _clipNorm[1] + ((double)mat[3][0] - _clipPt[0]) * _clipNorm[0]) + ((double)mat[3][2] - _clipPt[2]) * _clipNorm[2]);

			if (-_curMesh->radius > sideStored)
				return;
		} else {
			double side = ((double)mat[3][1] - _clipPt[1]) * _clipNorm[1] + ((double)mat[3][2] - _clipPt[2]) * _clipNorm[2] + ((double)mat[3][0] - _clipPt[0]) * _clipNorm[0];

			if (!(-_curMesh->radius <= side))
				return;
		}
	}

	_rd->multiply34(modelView, camera->viewMatrix, mat);
	_rd->transformPointLst34(modelView, _curMesh->vertices, _view, _curMesh->numVertices);
	_rd->invertOrtho34(inverse, mat);

	_geometryMode = _curMesh->geometryMode;

	if (_geometryMode >= _curGeometryMode)
		_geometryMode = _curGeometryMode;

	_textureMode = _curMesh->textureMode;

	if (_textureMode >= _curTextureMode)
		_textureMode = _curTextureMode;

	int lightingMode = _curMesh->lightingMode;

	if (lightingMode != 6 && lightingMode >= _curLightingMode)
		lightingMode = _curLightingMode;

	_lightingMode = lightingMode;

	_vertexSrc.vertices = _view;
	_vertexSrc.texVertices = _curMesh->texVertices;
	_vertexSrc.intensities = _curMesh->dynamicVertexIntensities;
	_vertexSrc.addedIntensities = nullptr;
	_vertexDst.vertices = _faceVerts;

	if (lightingMode == 2 || lightingMode == 3) {
		_numMeshLights = 0;

		for (uint i = 0; i < _numGeoLights; i++) {
			RdLight *light = _geoLights[i];

			if (!light->active)
				continue;

			const float *lightPos = camera->lightPos[light->id];
			bool inRange;

			if (lightingMode == 2 && _rd->_remastered) {
				float dxStored = (float)((double)lightPos[0] - mat[3][0]);
				float dyStored = (float)((double)lightPos[1] - mat[3][1]);
				float dzStored = (float)((double)lightPos[2] - mat[3][2]);
				float distance = (float)sqrt(((double)dyStored * dyStored + (double)dxStored * dxStored) + (double)dzStored * dzStored);
				double range = (double)light->falloffMin + _curMesh->radius;

				inRange = range > distance;
			} else if (lightingMode == 2) {
				double dx = (double)lightPos[0] - mat[3][0];
				double dy = (double)lightPos[1] - mat[3][1];
				float dyStored = (float)dy;
				float dzStored = (float)((double)lightPos[2] - mat[3][2]);
				double distance = sqrt(dx * dx + dy * dyStored + (double)dzStored * dzStored);
				double range = (double)light->falloffMin + _curMesh->radius;

				inRange = range > distance;
			} else {
				double distSquared;

				if (light->type >= 2 && _rd->_remastered) {
					float dxStored = (float)((double)lightPos[0] - mat[3][0]);
					float dyStored = (float)((double)lightPos[1] - mat[3][1]);
					float dzStored = (float)((double)lightPos[2] - mat[3][2]);

					distSquared = (float)(((double)dyStored * dyStored + (double)dxStored * dxStored) + (double)dzStored * dzStored);
				} else if (light->type >= 2) {
					double dx = (double)lightPos[0] - mat[3][0];
					double dy = (double)lightPos[1] - mat[3][1];
					double dz = (double)lightPos[2] - mat[3][2];
					distSquared = dx * dx + dy * dy + dz * dz;
				} else {
					distSquared = 0.0;
				}

				double range = (double)light->falloffMin + _curMesh->radius;

				inRange = distSquared < range * range;
			}

			if (!inRange)
				continue;

			_meshLights[_numMeshLights] = light;
			_rd->transformPoint34(_localLightPos[_numMeshLights], camera->lightPos[light->id], inverse);

			if (light->type != 2) {
				float translation[3];
				RdVector3 direction;

				translation[0] = inverse[3][0];
				translation[1] = inverse[3][1];
				translation[2] = inverse[3][2];
				inverse[3][0] = 0.0f;
				inverse[3][1] = 0.0f;
				inverse[3][2] = 0.0f;
				direction[0] = -light->direction[0];
				direction[1] = -light->direction[1];
				direction[2] = -light->direction[2];
				_rd->transformPoint34(_localLightDir[_numMeshLights], direction, inverse);
				inverse[3][0] = translation[0];
				inverse[3][1] = translation[1];
				inverse[3][2] = translation[2];
			}

			_numMeshLights++;
		}

		if (lightingMode == 3) {
			mesh->userLightValue = _rd->_light.calcVertexIntensities(_meshLights, _localLightPos, _localLightDir, _numMeshLights, _curMesh->vertexNormals, _curMesh->vertices, _curMesh->staticVertexIntensities, _curMesh->dynamicVertexIntensities, _curMesh->numVertices, camera->attenuationMin);
		}
	} else if (lightingMode == 6) {
		for (uint i = 0; i < mesh->numFaces; i++)
			mesh->faces[i].extraLight = mesh->userLightValue;
	}

	RdVector3 cameraPosition;

	_rd->_camera.getPosition(cameraPosition);
	_rd->transformPoint34(_localCamera, cameraPosition, inverse);

	for (uint i = 0; i < mesh->numFaces; i++) {
		RdFace3 *face = &mesh->faces[i];
		const float *vertex = _curMesh->vertices[face->vertices[0]];
		double facing;
		bool facingBack;

		if (_rd->_remastered) {
			float dxStored = (float)((double)_localCamera[0] - vertex[0]);
			float dyStored = (float)((double)_localCamera[1] - vertex[1]);
			float dzStored = (float)((double)_localCamera[2] - vertex[2]);

			facing = (float)(((double)face->normal[1] * dyStored + (double)face->normal[0] * dxStored) + (double)face->normal[2] * dzStored);
			facingBack = !(facing > 0.0);
		} else {
			// The demo adds the terms in z, y, x order
			if (_rd->isDemo())
				facing = ((double)_localCamera[2] - vertex[2]) * face->normal[2] + ((double)_localCamera[1] - vertex[1]) * face->normal[1] + ((double)_localCamera[0] - vertex[0]) * face->normal[0];
			else
				facing = ((double)_localCamera[2] - vertex[2]) * face->normal[2] + ((double)_localCamera[0] - vertex[0]) * face->normal[0] + ((double)_localCamera[1] - vertex[1]) * face->normal[1];

			facingBack = facing <= 0.0;
		}

		int backface = 0;

		if (facingBack) {
			backface = 1;

			if (!(face->type & 1) && (_rd->_curRenderOptions & 1))
				continue;
		}

		drawFace(face, backface);
	}
}

int RenderDroid::Model3Module::drawFace(RdFace3 *face, int backface) {
	RdCamera *camera = _rd->_camera.getCurrent();
	RdProcFace *procFace = _rd->_cache.getProcEntry();

	if (!procFace)
		return 0;

	int geometryMode = face->geometryMode;

	if (_geometryMode < geometryMode)
		geometryMode = _geometryMode;

	procFace->geometryMode = geometryMode;

	int lightingMode = face->lightingMode;

	if (_lightingMode < lightingMode)
		lightingMode = _lightingMode;

	procFace->lightingMode = lightingMode;

	int textureMode = face->textureMode;

	if (_textureMode < textureMode)
		textureMode = _textureMode;

	procFace->textureMode = textureMode;

	if (face->type & 0x10)
		procFace->lightingMode = 1;

	_vertexDst.screen = procFace->vertices;
	_vertexDst.texVertices = procFace->texVertices;
	_vertexDst.intensities = procFace->intensities;
	_vertexSrc.numVertices = face->numVertices;
	_vertexSrc.vertIndices = face->vertices;
	_vertexSrc.texIndices = face->texVertices;

	if (_meshFrustrumCull)
		_rd->_primit3.clipFace(camera->clipFrustum, procFace->geometryMode, procFace->lightingMode, procFace->textureMode, &_vertexSrc, &_vertexDst, face->uvDelta);
	else
		_rd->_primit3.noClipFace(procFace->geometryMode, procFace->lightingMode, procFace->textureMode, &_vertexSrc, &_vertexDst, face->uvDelta);

	if (_vertexDst.numVertices < 3)
		return 0;

	if (_clipPlaneActive) {
		RdVector3 planeNormal;
		RdVector3 planePoint;

		_rd->transformVector34(planeNormal, _clipNorm, camera->viewMatrix);
		_rd->transformPoint34(planePoint, _clipPt, camera->viewMatrix);

		int numClipped = _rd->_clip.faceToPlane(planeNormal, planePoint, _vertexDst.vertices, _vertexDst.texVertices, _vertexDst.intensities, _vertexDst.numVertices);

		if (numClipped < 2)
			return 0;

		_vertexDst.numVertices = numClipped;
		procFace->numVertices = numClipped;
	}

	if (procFace->lightingMode == 2) {
		RdVector3 normal;

		if (backface) {
			normal[0] = -face->normal[0];
			normal[1] = -face->normal[1];
			normal[2] = -face->normal[2];
		} else {
			normal[0] = face->normal[0];
			normal[1] = face->normal[1];
			normal[2] = face->normal[2];
		}

		procFace->diffuseLight = _rd->_light.calcFaceIntensity(_meshLights, _localLightPos, _localLightDir, _numMeshLights, face, normal, _curMesh->vertices, camera->attenuationMin);
	}

	(_rd->_camera.*camera->projectLst)(_vertexDst.screen, _vertexDst.vertices, _vertexDst.numVertices);

	if (_rd->_curRenderOptions & 2)
		procFace->ambientLight = camera->ambientLight;
	else
		procFace->ambientLight = 0.0f;

	int curCel = face->curCel;

	if (curCel == -1 && face->material)
		curCel = face->material->curCel;

	procFace->curCel = curCel;

	RdColormap *curMap = _rd->_colormap.getCurrent();
	RdColormap *identityMap = _rd->_colormap.getIdentity();

	if (procFace->ambientLight < 1.0f) {
		int procLightingMode = procFace->lightingMode;

		if (procLightingMode == 2) {
			if (procFace->diffuseLight >= 1.0f && curMap == identityMap)
				procFace->lightingMode = 0;
			else if (procFace->diffuseLight <= 0.0f)
				procFace->lightingMode = 1;
		} else if (procLightingMode == 3) {
			float first = procFace->intensities[0];
			uint same = 1;

			while (same < _vertexDst.numVertices) {
				double difference = (double)procFace->intensities[same] - first;

				if (difference < 0.0)
					difference = -difference;

				if (difference > 0.015625)
					break;

				same++;
			}

			if (same == _vertexDst.numVertices) {
				if (first != 1.0f) {
					if (first == 0.0f) {
						procFace->lightingMode = 1;
						procFace->diffuseLight = 0.0f;
					} else {
						procFace->lightingMode = 2;
						procFace->diffuseLight = first;
					}
				} else if (curMap == identityMap) {
					procFace->lightingMode = 0;
				} else {
					procFace->lightingMode = 2;
					procFace->diffuseLight = 1.0f;
				}
			}
		}
	} else if (curMap != identityMap) {
		procFace->lightingMode = 2;
		procFace->diffuseLight = 1.0f;
	} else {
		procFace->lightingMode = 0;
	}

	char bufferFlags = 1;

	if (procFace->geometryMode >= 4)
		bufferFlags = 3;

	if (procFace->lightingMode >= 3)
		bufferFlags |= 4;

	procFace->backface = backface;
	procFace->type = face->type;
	procFace->extraLight = face->extraLight;
	procFace->material = face->material;
	_rd->_cache.addProcFace(0, _vertexDst.numVertices, bufferFlags);

	return 1;
}

int RenderDroid::Model3Module::drawPolyline(RdThing *thing, RdMatrix34 mat) {
	RdPolyline *polyline = thing->data.polyline;
	RdCamera *camera = _rd->_camera.getCurrent();
	RdMatrix34 modelView;
	RdVector3 localEnd;
	RdVector3 end;
	RdVertexLists src;

	memset(&src, 0, sizeof(RdVertexLists));
	src.vertices = (RdVector3 *)_polylineVerts;

	_rd->multiply34(modelView, camera->viewMatrix, mat);

	localEnd[0] = 0.0f;
	localEnd[1] = polyline->length;
	localEnd[2] = 0.0f;
	_rd->transformPoint34(end, localEnd, modelView);

	float endY = (float)((double)end[1] - -0.001f);

	_polylineVerts[0] = (float)((double)end[0] - polyline->endWidth);
	_polylineVerts[1] = endY;
	_polylineVerts[2] = (float)((double)end[2] - polyline->endWidth);
	_polylineVerts[3] = (float)((double)end[0] + polyline->endWidth);
	_polylineVerts[4] = endY;
	_polylineVerts[5] = (float)((double)end[2] - polyline->endWidth);
	_polylineVerts[6] = (float)((double)end[0] + polyline->endWidth);
	_polylineVerts[7] = endY;
	_polylineVerts[8] = (float)((double)end[2] + polyline->endWidth);
	_polylineVerts[9] = (float)((double)end[0] - polyline->endWidth);
	_polylineVerts[10] = endY;
	_polylineVerts[11] = (float)((double)end[2] + polyline->endWidth);
	src.texVertices = polyline->texVertsEnd;
	_rd->_primit3.drawPolylineFace(thing, &polyline->faceEnd, (RdVector3 *)_polylineVerts, &src);

	float startY = (float)((double)modelView[3][1] - -0.001f);

	_polylineVerts[0] = (float)((double)modelView[3][0] - polyline->startWidth);
	_polylineVerts[1] = startY;
	_polylineVerts[2] = (float)((double)modelView[3][2] - polyline->startWidth);
	_polylineVerts[3] = (float)((double)modelView[3][0] + polyline->startWidth);
	_polylineVerts[4] = startY;
	_polylineVerts[5] = (float)((double)modelView[3][2] - polyline->startWidth);
	_polylineVerts[6] = (float)((double)modelView[3][0] + polyline->startWidth);
	_polylineVerts[7] = startY;
	_polylineVerts[8] = (float)((double)modelView[3][2] + polyline->startWidth);
	_polylineVerts[9] = (float)((double)modelView[3][0] - polyline->startWidth);
	_polylineVerts[10] = startY;
	_polylineVerts[11] = (float)((double)modelView[3][2] + polyline->startWidth);
	src.texVertices = polyline->texVertsEnd;
	_rd->_primit3.drawPolylineFace(thing, &polyline->faceEnd, (RdVector3 *)_polylineVerts, &src);

	float dx = (float)((double)end[0] - modelView[3][0]);
	double dz = (double)end[2] - modelView[3][2];
	float length = (float)sqrt(dz * dz + (double)dx * dx);
	float sine = (float)(-(double)dx / length);
	float angle = _rd->_stdMath->arcSin3(sine);

	if ((float)dz < 0.0f) {
		if (dx <= 0.0f)
			angle = (float)-((double)angle - -180.0);
		else
			angle = (float)(180.0 - angle);
	}

	float sinAngle;
	float cosAngle;

	_rd->_stdMath->sinCos(angle, &sinAngle, &cosAngle);

	float sinLength = (float)((double)sinAngle * length);
	float cosLength = (float)((double)cosAngle * length);
	double endWidth = polyline->endWidth;
	double startWidth = polyline->startWidth;

	_polylineVerts[1] = end[1];
	_polylineVerts[0] = (float)(endWidth * cosAngle - sinLength + modelView[3][0]);
	_polylineVerts[2] = (float)(endWidth * sinAngle + cosLength + modelView[3][2]);
	_polylineVerts[3] = (float)(-endWidth * cosAngle - sinLength + modelView[3][0]);
	_polylineVerts[4] = end[1];
	_polylineVerts[5] = (float)(-endWidth * sinAngle + cosLength + modelView[3][2]);
	_polylineVerts[6] = (float)(-startWidth * cosAngle - 0.0 + modelView[3][0]);
	_polylineVerts[7] = modelView[3][1];
	_polylineVerts[8] = (float)(-startWidth * sinAngle + 0.0 + modelView[3][2]);
	_polylineVerts[9] = (float)(startWidth * cosAngle - 0.0 + modelView[3][0]);
	_polylineVerts[10] = modelView[3][1];
	_polylineVerts[11] = (float)(startWidth * sinAngle + 0.0 + modelView[3][2]);
	src.texVertices = polyline->texVertsBody;
	_rd->_primit3.drawPolylineFace(thing, &polyline->faceBody, (RdVector3 *)_polylineVerts, &src);

	return 1;
}

int RenderDroid::Model3Module::drawParticle(RdThing *thing, RdMatrix34 mat) {
	RdParticleCloud *particlePointer = thing->data.particle;
	RdCamera *camera = _rd->_camera.getCurrent();
	RdVector3 center;
	RdMatrix34 modelView;
	int frustrumCull;

	if (particlePointer->numParticles > 256) {
		warning("RenderDroid::Model3Module::drawParticle(): too many particles (%u)", particlePointer->numParticles);
		return 0;
	}

	_rd->transformPoint34(center, mat[3], camera->viewMatrix);

	if (_rd->_curCullFlags & 2)
		frustrumCull = _rd->_clip.sphereInFrustrum(camera->clipFrustum, center, particlePointer->radius);
	else
		frustrumCull = thing->frustrumCull;

	if (frustrumCull == 2)
		return 0;

	_rd->multiply34(modelView, camera->viewMatrix, mat);

	float ambientLight = 0.0f;

	if (_rd->_curRenderOptions & 2)
		ambientLight = camera->ambientLight;

	int lightingMode;

	if (ambientLight < 1.0f) {
		if (ambientLight > 0.0f)
			lightingMode = particlePointer->lightingMode;
		else
			lightingMode = 1;
	} else {
		lightingMode = 0;
	}

	if (lightingMode >= particlePointer->lightingMode)
		lightingMode = particlePointer->lightingMode;

	_rd->transformPointLst34(modelView, particlePointer->positions, _particleTransformed, particlePointer->numParticles);

	for (uint i = 0; i < particlePointer->numParticles; i++) {
		RdProcFace *procFace = _rd->_cache.getProcEntry();

		if (!procFace)
			return 0;

		const float *particle = _particleTransformed[i];
		double size = particlePointer->size;

		_particleVerts[0] = (float)((double)particle[0] - size);
		_particleVerts[1] = particle[1];
		_particleVerts[2] = (float)((double)particle[2] - size);
		_particleVerts[3] = (float)((double)particle[0] + size);
		_particleVerts[4] = particle[1];
		_particleVerts[5] = (float)((double)particle[2] - size);
		_particleVerts[6] = (float)((double)particle[0] + size);
		_particleVerts[7] = particle[1];
		_particleVerts[8] = (float)((double)particle[2] + size);
		_particleVerts[9] = (float)((double)particle[0] - size);
		_particleVerts[10] = particle[1];
		_particleVerts[11] = (float)((double)particle[2] + size);

		int numClipped = _rd->_clip.face3S(camera->clipFrustum, (RdVector3 *)_particleVerts, 4);

		if (numClipped >= 3) {
			(_rd->_camera.*camera->projectLst)(procFace->vertices, (RdVector3 *)_particleVerts, numClipped);
			procFace->lightingMode = lightingMode;
			procFace->ambientLight = ambientLight;
			procFace->geometryMode = 3;
			procFace->type = 0;
			procFace->material = particlePointer->material;
			procFace->curCel = particlePointer->cels[i];
			procFace->backface = 0;
			_rd->_cache.addProcFace(0, numClipped, 1);
		}
	}

	return 1;
}

void RenderDroid::Model3Module::buildShadowModel(RdModel3 *model3) {
	static const byte corners[16][3] = {
		{1, 1, 1}, {1, 0, 1}, {0, 0, 1}, {0, 1, 1},
		{1, 1, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0},
		{0, 0, 0}, {1, 1, 0}, {1, 1, 1}, {0, 0, 1},
		{0, 1, 0}, {1, 0, 0}, {1, 0, 1}, {0, 1, 1}
	};

	for (uint g = 0; g < model3->numGeos; g++) {
		RdModel3Geo *geo = &model3->geo[g];

		for (uint m = 0; m < geo->numMeshes; m++) {
			RdModel3Mesh *mesh = &geo->meshes[m];

			if (!mesh->castShadow)
				continue;

			delete[] mesh->shadowVerts;
			mesh->shadowVerts = new RdVector3[16];

			for (uint v = 0; v < 16; v++) {
				for (uint k = 0; k < 3; k++)
					mesh->shadowVerts[v][k] = corners[v][k] ? mesh->bound.max[k] : mesh->bound.min[k];
			}
		}
	}
}

} // End of namespace Grim
