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

#include "engines/grim/remastered/look/look_renderer.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"

namespace Grim {

RdCamera *RenderDroid::CameraModule::create(float fov, int clipFar, float zNear, float zFar, float aspectRatio) {
	RdCamera *camera = new RdCamera;

	if (!newEntry(camera, fov, clipFar, zNear, zFar, aspectRatio))
		return nullptr;

	return camera;
}

int RenderDroid::CameraModule::newEntry(RdCamera *camera, float fov, int clipFar, float zNear, float zFar, float aspectRatio) {
	camera->clipFrustum = new RdClipFrustum;

	bool belowMinimum = fov < 5.0;
	if (_rd->_remastered)
		belowMinimum = !(fov >= 5.0);

	double clampedFov;
	if (belowMinimum)
		clampedFov = 5.0;
	else if (fov > 179.0)
		clampedFov = 179.0;
	else
		clampedFov = fov;

	camera->fov = (float)clampedFov;
	camera->clipFrustum->clipFar = clipFar;
	camera->clipFrustum->zNear = zNear;
	camera->clipFrustum->zFar = zFar;
	camera->aspectRatio = aspectRatio;
	camera->orthoScale = 1.0f;
	camera->canvas = nullptr;
	camera->ambientLight = 0.0f;
	camera->numLights = 0;
	camera->attenuationMin = 0.2f;
	camera->attenuationMax = 0.1f;
	setProjectType(camera, 1);

	return 1;
}

void RenderDroid::CameraModule::destroy(RdCamera *&camera) {
	if (camera) {
		if (_rd->_remastered && _curCamera == camera)
			_curCamera = nullptr;

		freeEntry(camera);
		delete camera;
		camera = nullptr;
	}
}

void RenderDroid::CameraModule::freeEntry(RdCamera *camera) {
	if (camera->clipFrustum) {
		delete camera->clipFrustum;
		camera->clipFrustum = nullptr;
	}

	if (_rd->_remastered)
		camera->canvas = nullptr;
}

int RenderDroid::CameraModule::setCanvas(RdCamera *camera, RdCanvas *canvas) {
	camera->canvas = canvas;
	buildFOV(camera);

	return 1;
}

int RenderDroid::CameraModule::setCurrent(RdCamera *camera) {
	if (_curCamera != camera) {
		_curCamera = camera;

		if (_rd->_remasteredLookRenderer)
			_rd->_remasteredLookRenderer->postCameraChange();
	}

	return 1;
}

int RenderDroid::CameraModule::setFOV(RdCamera *camera, float fov) {
	bool belowMinimum = fov < 5.0;
	if (_rd->_remastered)
		belowMinimum = !(fov >= 5.0);

	double clampedFov;

	if (belowMinimum)
		clampedFov = 5.0;
	else if (fov > 179.0)
		clampedFov = 179.0;
	else
		clampedFov = fov;

	camera->fov = (float)clampedFov;
	buildFOV(camera);

	return 1;
}

int RenderDroid::CameraModule::setProjectType(RdCamera *camera, int projectType) {
	camera->projectType = projectType;

	if (projectType == 0) {
		if (camera->aspectRatio == 1.0f) {
			camera->project = &CameraModule::orthoProjectSquare;
			camera->projectLst = &CameraModule::orthoProjectSquareLst;
		} else {
			camera->project = &CameraModule::orthoProject;
			camera->projectLst = &CameraModule::orthoProjectLst;
		}
	} else if (projectType == 1) {
		if (camera->aspectRatio == 1.0f) {
			camera->project = &CameraModule::perspProjectSquare;
			camera->projectLst = &CameraModule::perspProjectSquareLst;
		} else {
			camera->project = &CameraModule::perspProject;
			camera->projectLst = &CameraModule::perspProjectLst;
		}
	}

	if (camera->canvas)
		buildFOV(camera);

	return 1;
}

int RenderDroid::CameraModule::setAspectRatio(RdCamera *camera, float aspectRatio) {
	camera->aspectRatio = aspectRatio;

	if (camera->projectType == 0) {
		if (aspectRatio == 1.0) {
			camera->project = &CameraModule::orthoProjectSquare;
			camera->projectLst = &CameraModule::orthoProjectSquareLst;
		} else {
			camera->project = &CameraModule::orthoProject;
			camera->projectLst = &CameraModule::orthoProjectLst;
		}
	} else if (camera->projectType == 1) {
		if (aspectRatio == 1.0) {
			camera->project = &CameraModule::perspProjectSquare;
			camera->projectLst = &CameraModule::perspProjectSquareLst;
		} else {
			camera->project = &CameraModule::perspProject;
			camera->projectLst = &CameraModule::perspProjectLst;
		}
	}

	buildFOV(camera);

	return 1;
}

int RenderDroid::CameraModule::buildFOV(RdCamera *camera) {
	RdCanvas *canvas = camera->canvas;

	if (!canvas)
		return 0;

	if (camera->projectType == 0) {
		int height = canvas->yEnd - canvas->yStart;
		double halfWidth = (double)(canvas->xEnd - canvas->xStart) * 0.5;

		camera->clipFrustum->orthoLeft = (float)-(halfWidth / camera->orthoScale);
		float halfHeight = (float)((double)height * 0.5);
		camera->clipFrustum->orthoTop = (float)((double)halfHeight / camera->orthoScale / camera->aspectRatio);
		camera->clipFrustum->orthoRight = (float)(halfWidth / camera->orthoScale);
		camera->clipFrustum->orthoBottom = (float)(-((double)halfHeight / camera->orthoScale) / camera->aspectRatio);
		camera->focalLen = 0.0f;
		camera->clipFrustum->top = 0.0f;
		camera->clipFrustum->bottom = 0.0f;
		camera->clipFrustum->left = 0.0f;
		camera->clipFrustum->right = 0.0f;
	} else if (camera->projectType == 1) {
		float halfWidth = (float)((double)(canvas->xEnd - canvas->xStart) * 0.5);
		float halfHeight = (float)((double)(canvas->yEnd - canvas->yStart) * 0.5);
		float halfFov = (float)((double)camera->fov * 0.5);
		float tangent = _rd->_stdMath->tan(halfFov);
		float focalLen = (float)((double)halfWidth / tangent);

		camera->focalLen = focalLen;
		camera->clipFrustum->top = (float)((double)halfHeight / focalLen / camera->aspectRatio);
		camera->clipFrustum->left = (float)(-(double)halfWidth / camera->focalLen);
		camera->clipFrustum->bottom = (float)(-(double)halfHeight / camera->focalLen / camera->aspectRatio);
		camera->clipFrustum->right = (float)((double)halfWidth / camera->focalLen);
		camera->clipFrustum->guardTop = (float)(((double)halfHeight - -1.0) / camera->focalLen / camera->aspectRatio);
		camera->clipFrustum->guardLeft = (float)(-((double)halfWidth - -1.0) / camera->focalLen);
	}

	return 1;
}

void RenderDroid::CameraModule::update(RdMatrix34 mat) {
	_rd->invertOrtho34(_curCamera->viewMatrix, mat);
	memcpy(_camMatrix, mat, sizeof(RdMatrix34));
	_rd->extractAngles34(_camMatrix, _camPYR);
}

void RenderDroid::CameraModule::orthoProject(RdVector3 out, RdVector3 in) {
	RdCanvas *canvas = _curCamera->canvas;

	out[0] = (float)((double)_curCamera->orthoScale * in[0] + canvas->xCenter);
	out[1] = (float)(-((double)in[2] * _curCamera->orthoScale) * _curCamera->aspectRatio + canvas->yCenter);
	out[2] = in[1];
}

void RenderDroid::CameraModule::orthoProjectLst(RdVector3 *out, RdVector3 *in, int num) {
	RdCanvas *canvas = _curCamera->canvas;
	float orthoScale = _curCamera->orthoScale;
	float yCenter = canvas->yCenter;
	float xCenter = canvas->xCenter;
	float scaledAspect = (float)((double)_curCamera->aspectRatio * orthoScale);

	for (int i = 0; i < num; i++) {
		double scaledX = (double)orthoScale * in[i][0];

		out[i][0] = (float)(scaledX + xCenter);
		out[i][1] = (float)((double)yCenter - (double)in[i][2] * scaledAspect);
		out[i][2] = in[i][1];
	}
}

void RenderDroid::CameraModule::orthoProjectSquare(RdVector3 out, RdVector3 in) {
	RdCanvas *canvas = _curCamera->canvas;

	out[0] = (float)((double)_curCamera->orthoScale * in[0] + canvas->xCenter);
	out[1] = (float)((double)canvas->yCenter - (double)in[2] * _curCamera->orthoScale);
	out[2] = in[1];
}

void RenderDroid::CameraModule::orthoProjectSquareLst(RdVector3 *out, RdVector3 *in, int num) {
	RdCanvas *canvas = _curCamera->canvas;
	float orthoScale = _curCamera->orthoScale;
	float yCenter = canvas->yCenter;
	float xCenter = canvas->xCenter;

	for (int i = 0; i < num; i++) {
		double scaledX = (double)orthoScale * in[i][0];

		out[i][0] = (float)(scaledX + xCenter);
		out[i][1] = (float)((double)yCenter - (double)in[i][2] * orthoScale);
		out[i][2] = in[i][1];
	}
}

void RenderDroid::CameraModule::perspProject(RdVector3 out, RdVector3 in) {
	RdCanvas *canvas = _curCamera->canvas;

	if (_rd->_remastered) {
		float scale = (float)((double)_curCamera->focalLen / in[1]);
		float scaledY = (float)((double)_curCamera->aspectRatio * scale);

		out[0] = (float)((double)scale * in[0] + canvas->xCenter);
		out[1] = (float)((double)canvas->yCenter - (double)in[2] * scaledY);
		out[2] = in[1];
		return;
	}

	double scale = (double)_curCamera->focalLen / in[1];
	double scaledX = scale * in[0];
	double scaledY = scale * _curCamera->aspectRatio;

	out[0] = (float)(scaledX + canvas->xCenter);
	out[1] = (float)((double)canvas->yCenter - (double)in[2] * scaledY);
	out[2] = in[1];
}

void RenderDroid::CameraModule::perspProjectLst(RdVector3 *out, RdVector3 *in, int num) {
	RdCanvas *canvas = _curCamera->canvas;
	float xCenter = canvas->xCenter;
	float yCenter = canvas->yCenter;

	if (_rd->_remastered) {
		for (int i = 0; i < num; i++) {
			float scale = (float)((double)_curCamera->focalLen / in[i][1]);
			float scaledY = (float)((double)_curCamera->aspectRatio * scale);

			out[i][0] = (float)((double)scale * in[i][0] + xCenter);
			out[i][1] = (float)((double)yCenter - (double)in[i][2] * scaledY);
			out[i][2] = in[i][1];
		}

		return;
	}

	for (int i = 0; i < num; i++) {
		double scale = (double)_curCamera->focalLen / in[i][1];
		double scaledX = scale * in[i][0];
		double scaledY = scale * _curCamera->aspectRatio;

		out[i][0] = (float)(scaledX + xCenter);
		out[i][1] = (float)((double)yCenter - (double)in[i][2] * scaledY);
		out[i][2] = in[i][1];
	}
}

void RenderDroid::CameraModule::perspProjectSquare(RdVector3 out, RdVector3 in) {
	RdCanvas *canvas = _curCamera->canvas;

	if (_rd->_remastered) {
		float scale = (float)((double)_curCamera->focalLen / in[1]);

		out[0] = (float)((double)in[0] * scale + canvas->xCenter);
		out[1] = (float)((double)canvas->yCenter - (double)in[2] * scale);
		out[2] = in[1];
		return;
	}

	double scale = (double)_curCamera->focalLen / in[1];

	out[0] = (float)(scale * in[0] + canvas->xCenter);
	out[1] = (float)((double)canvas->yCenter - (double)in[2] * scale);
	out[2] = in[1];
}

void RenderDroid::CameraModule::perspProjectSquareLst(RdVector3 *out, RdVector3 *in, int num) {
	RdCanvas *canvas = _curCamera->canvas;
	float xCenter = canvas->xCenter;
	float yCenter = canvas->yCenter;

	if (_rd->_remastered) {
		for (int i = 0; i < num; i++) {
			float scale = (float)((double)_curCamera->focalLen / in[i][1]);

			out[i][0] = (float)((double)in[i][0] * scale + xCenter);
			out[i][1] = (float)((double)yCenter - (double)in[i][2] * scale);
			out[i][2] = in[i][1];
		}

		return;
	}

	for (int i = 0; i < num; i++) {
		double scale = (double)_curCamera->focalLen / in[i][1];

		out[i][0] = (float)(scale * in[i][0] + xCenter);
		out[i][1] = (float)((double)yCenter - (double)in[i][2] * scale);
		out[i][2] = in[i][1];
	}
}

void RenderDroid::CameraModule::setAmbientLight(RdCamera *camera, float ambient) {
	camera->ambientLight = ambient;
}

int RenderDroid::CameraModule::addLight(RdCamera *camera, RdLight *light, RdVector3 *pos) {
	if (camera->numLights >= 64) {
		warning("RenderDroid::CameraModule::addLight(): too many lights");
		return 0;
	}

	light->id = camera->numLights;
	camera->lights[camera->numLights] = light;
	camera->lightPos[camera->numLights][0] = (*pos)[0];
	camera->lightPos[camera->numLights][1] = (*pos)[1];
	camera->lightPos[camera->numLights][2] = (*pos)[2];
	light->falloffMin = (float)((double)light->intensity / camera->attenuationMin);
	light->falloffMax = (float)((double)light->intensity / camera->attenuationMax);
	camera->numLights++;

	return 1;
}

void RenderDroid::CameraModule::advanceFrame() {
	if (!_curCamera)
		return;

	RdCanvas *canvas = _curCamera->canvas;

	if ((_rd->_curRenderOptions & 0x100) && (canvas->flags & 2)) {
		if (_rd->_curAcceleration > 0) {
#if defined(GRIM_STD3D)
			_rd->_std3D->clearZBuffer();
#endif
			return;
		}

		if (canvas->flags & 1) {
			StdRect rect;

			rect.x = canvas->xStart;
			rect.y = canvas->yStart;
			rect.width = canvas->xEnd - canvas->xStart + 1;
			rect.height = canvas->yEnd - canvas->yStart + 1;
			_rd->_stdDisplay->vBufferFill(canvas->zBuffer, 0, &rect);
		} else {
			_rd->_stdDisplay->vBufferFill(canvas->zBuffer, 0, nullptr);
		}
	}
}

} // End of namespace Grim
