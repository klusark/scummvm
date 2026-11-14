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

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

int RenderDroid::LightModule::newEntry(RdLight *light) {
	light->type = 2;
	light->active = 1;
	light->direction[0] = 0.0f;
	light->direction[1] = 0.0f;
	light->direction[2] = 0.0f;
	light->intensity = 1.0f;
	light->color = 0xFFFFFF;
	light->angleX = 0.0f;
	light->cosAngleX = 0.0f;
	light->angleY = 0.0f;
	light->cosAngleY = 0.0f;

	return 1;
}

void RenderDroid::LightModule::setAngles(RdLight *light, float angleX, float angleY) {
	float unusedSin;

	light->angleX = angleX;
	light->angleY = angleY;
	_rd->_stdMath->sinCos(angleX, &unusedSin, &light->cosAngleX);
	_rd->_stdMath->sinCos(angleY, &unusedSin, &light->cosAngleY);

	double difference = (double)light->cosAngleX - light->cosAngleY;
	if (difference > 0.0)
		light->lux = (float)(1.0 / difference);
}

float RenderDroid::LightModule::calcVertexIntensities(RdLight **lights, RdVector3 *lightPos, RdVector3 *lightDir, uint numLights, RdVector3 *normals, RdVector3 *vertices, float *staticIntensities, float *dynamicIntensities, uint numVertices, float attenuation) {
	float total = 0.0f;

	if (_rd->_remastered) {
		for (uint i = 0; i < numVertices; i++) {
			dynamicIntensities[i] = staticIntensities[i];

			for (uint j = 0; j < numLights; j++) {
				RdLight *light = lights[j];
				RdVector3 vec;
				float distanceSquared = 0.0f;

				if (light->type >= 2) {
					vec[0] = (float)((double)lightPos[j][0] - vertices[i][0]);
					vec[1] = (float)((double)lightPos[j][1] - vertices[i][1]);
					vec[2] = (float)((double)lightPos[j][2] - vertices[i][2]);
					distanceSquared = (float)(((double)vec[1] * vec[1] + (double)vec[0] * vec[0]) + (double)vec[2] * vec[2]);
				}

				if ((double)distanceSquared < (double)light->falloffMin * light->falloffMin) {
					switch (light->type) {
					case 0:
					case 1: {
						float dot = (float)(((double)normals[i][0] * lightDir[j][0] + (double)normals[i][1] * lightDir[j][1]) + (double)normals[i][2] * lightDir[j][2]);

						if (dot > 0.0f)
							dynamicIntensities[i] = (float)((double)dot * light->intensity + dynamicIntensities[i]);

						break;
					}
					case 2: {
						float distance = _rd->normalize3Acc(vec);
						float dot = (float)(((double)normals[i][1] * vec[1] + (double)normals[i][0] * vec[0]) + (double)normals[i][2] * vec[2]);

						if (dot > 0.0f)
							dynamicIntensities[i] = (float)((((double)light->intensity - (double)distance * attenuation) * dot) + dynamicIntensities[i]);

						break;
					}
					case 3: {
						float distance = _rd->normalize3Acc(vec);
						float cosine = (float)(((double)lightDir[j][0] * vec[0] + (double)lightDir[j][1] * vec[1]) + (double)lightDir[j][2] * vec[2]);

						if (light->cosAngleY < cosine) {
							float intensity = light->intensity;

							if (light->cosAngleX > cosine)
								intensity = (float)((1.0 - ((double)light->cosAngleX - cosine) * light->lux) * intensity);

							float dot = (float)(((double)normals[i][0] * lightDir[j][0] + (double)normals[i][1] * lightDir[j][1]) + (double)normals[i][2] * lightDir[j][2]);

							dynamicIntensities[i] = (float)(((double)dot * ((double)intensity - (double)distance * attenuation)) + dynamicIntensities[i]);
						}

						break;
					}
					default:
						break;
					}
				}

				if (dynamicIntensities[i] > 1.0) {
					dynamicIntensities[i] = 1.0f;
					break;
				}

				if (dynamicIntensities[i] == 1.0f)
					break;
			}

			total = (float)((double)total + dynamicIntensities[i]);
		}

		return (float)((double)total / (double)numVertices);
	}

	for (uint i = 0; i < numVertices; i++) {
		dynamicIntensities[i] = staticIntensities[i];

		for (uint j = 0; j < numLights; j++) {
			RdLight *light = lights[j];
			RdVector3 vec;
			double distanceSquared;

			if (light->type >= 2) {
				double dy = (double)lightPos[j][1] - vertices[i][1];
				double dx = (double)lightPos[j][0] - vertices[i][0];
				double dz = (double)lightPos[j][2] - vertices[i][2];

				vec[0] = (float)dx;
				vec[1] = (float)dy;
				vec[2] = (float)dz;
				distanceSquared = dx * vec[0] + dy * vec[1] + dz * vec[2];
			} else {
				distanceSquared = 0.0;
			}

			if (distanceSquared < (double)light->falloffMin * light->falloffMin) {
				double contribution = 0.0;
				bool lit = false;

				switch (light->type) {
				case 0:
				case 1: {
					double dot = (double)normals[i][0] * lightDir[j][0] + (double)normals[i][1] * lightDir[j][1] + (double)normals[i][2] * lightDir[j][2];

					if (dot > 0.0) {
						contribution = light->intensity * dot;
						lit = true;
					}
					break;
				}
				case 2: {
					float distance = _rd->normalize3Acc(vec);
					double dot = (double)normals[i][0] * vec[0] + (double)normals[i][1] * vec[1] + (double)normals[i][2] * vec[2];

					if (dot > 0.0) {
						contribution = ((double)light->intensity - (double)distance * attenuation) * dot;
						lit = true;
					}
					break;
				}
				case 3: {
					float distance = _rd->normalize3Acc(vec);
					float cosine = (float)((double)lightDir[j][1] * vec[1] + (double)lightDir[j][0] * vec[0] + (double)lightDir[j][2] * vec[2]);

					if (cosine > light->cosAngleY) {
						double intensity = light->intensity;

						if (cosine < light->cosAngleX)
							intensity = intensity * (1.0 - ((double)light->cosAngleX - cosine) * light->lux);

						contribution = ((double)normals[i][1] * lightDir[j][1] + (double)normals[i][0] * lightDir[j][0] + (double)normals[i][2] * lightDir[j][2]) * (intensity - (double)distance * attenuation);
						lit = true;
					}
					break;
				}
				default:
					break;
				}

				if (lit)
					dynamicIntensities[i] = (float)(contribution + dynamicIntensities[i]);
			}

			if (dynamicIntensities[i] > 1.0)
				dynamicIntensities[i] = 1.0f;

			if (dynamicIntensities[i] == 1.0f)
				break;
		}

		total = (float)((double)total + dynamicIntensities[i]);
	}

	return (float)((double)total / (double)numVertices);
}

float RenderDroid::LightModule::calcFaceIntensity(RdLight **lights, RdVector3 *lightPos, RdVector3 *lightDir, uint numLights, RdFace3 *face, RdVector3 normal, RdVector3 *vertices, float attenuation) {
	float total = 0.0f;

	if (_rd->_remastered) {
		for (uint i = 0; i < numLights; i++) {
			RdLight *light = lights[i];

			if (light->active) {
				RdVector3 vec;
				int vertex = face->vertices[0];

				vec[0] = (float)((double)lightPos[i][0] - vertices[vertex][0]);
				vec[1] = (float)((double)lightPos[i][1] - vertices[vertex][1]);
				vec[2] = (float)((double)lightPos[i][2] - vertices[vertex][2]);

				float distanceSquared = (float)(((double)vec[1] * vec[1] + (double)vec[0] * vec[0]) + (double)vec[2] * vec[2]);

				if ((double)distanceSquared < (double)light->falloffMin * light->falloffMin) {
					float distance = _rd->normalize3Acc(vec);
					float dot = (float)(((double)normal[1] * vec[1] + (double)normal[0] * vec[0]) + (double)normal[2] * vec[2]);

					if (dot > 0.0f)
						total = (float)((((double)light->intensity - (double)distance * attenuation) * dot) + total);
				}
			}

			if (total >= 1.0)
				break;
		}

		return total;
	}

	for (uint i = 0; i < numLights; i++) {
		RdLight *light = lights[i];

		if (light->active) {
			RdVector3 vec;
			int vertex = face->vertices[0];

			vec[0] = (float)((double)lightPos[i][0] - vertices[vertex][0]);
			vec[1] = (float)((double)lightPos[i][1] - vertices[vertex][1]);
			vec[2] = (float)((double)lightPos[i][2] - vertices[vertex][2]);

			if ((double)light->falloffMin * light->falloffMin > (double)vec[0] * vec[0] + (double)vec[1] * vec[1] + (double)vec[2] * vec[2]) {
				float distance = _rd->normalize3Acc(vec);
				double dot = (double)normal[2] * vec[2] + (double)normal[1] * vec[1] + (double)vec[0] * normal[0];

				if (dot > 0.0)
					total = (float)(((double)light->intensity - (double)distance * attenuation) * dot + total);
			}
		}

		if (total >= 1.0)
			break;
	}

	return total;
}

} // End of namespace Grim
