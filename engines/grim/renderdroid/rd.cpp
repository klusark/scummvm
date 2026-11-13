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

#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/std/std_3d.h"
#include "engines/grim/std/std_3d_config.h"

namespace Grim {

RenderDroid::RenderDroid(StdMath *stdMath, StdDisplay *stdDisplay, StdConffile *stdConffile, StdFile *stdFile, bool isDemo, bool isRemastered) :
		_camera(this), _canvas(this), _clip(this), _colormap(this, isDemo), _colorEffects(this),
		_face(this), _keyframe(this), _light(this), _material(this), _model3(this),
		_primit2(this), _primit3(this, isDemo), _puppet(this, isDemo), _sprite(this), _thing(this),
		_active(this), _cache(this), _raster(this), _afRaster(this), _nRaster(this),
		_zRaster(this), _stdMath(stdMath), _stdDisplay(stdDisplay), _stdConffile(stdConffile), _stdFile(stdFile), _std3D(nullptr),
		_hs(nullptr), _started(0), _opened(0),
		_frameTrue(0), _curAcceleration(0), _curRenderOptions(0), _curGeometryMode(0),
		_curLightingMode(0), _curTextureMode(0), _curSortingMethod(0), _curOcclusionMethod(0),
		_curZBufferMethod(0), _curVertexColorMode(0), _curCullFlags(0), _curProcFaceUserData(0), _demo(isDemo), _remastered(isRemastered) {
	memset(&_colorInfo, 0, sizeof(StdColorFormat));
	memset(&_curColorEffects, 0, sizeof(RdColorEffects));

	_mipDistances[0] = 10.0f;
	_mipDistances[1] = 20.0f;
	_mipDistances[2] = 40.0f;
	_mipDistances[3] = 80.0f;

	memset(_identMatrix34, 0, sizeof(RdMatrix34));
	_identMatrix34[0][0] = 1.0f;
	_identMatrix34[1][1] = 1.0f;
	_identMatrix34[2][2] = 1.0f;
}

RenderDroid::~RenderDroid() {
}

void RenderDroid::setColorInfo(const StdColorFormat &colorInfo) {
	_colorInfo = colorInfo;
}

void RenderDroid::setStd3D(Std3D *std3D) {
	_std3D = std3D;
}

int RenderDroid::getAcceleration() const {
	return _curAcceleration;
}

void RenderDroid::setAcceleration(int acceleration) {
	if (acceleration > 0 && !_std3D) {
		warning("RenderDroid::setAcceleration(): no 3D device set");
		acceleration = 0;
	}

	_curAcceleration = acceleration;
}

int RenderDroid::startup(SysStruct *hs) {
	if (_started)
		return 1;

	_hs = hs;
	_active.startup();
	_cache.startup();
	_raster.startup();
	_started = 1;

	return 1;
}

void RenderDroid::shutdown() {
	if (_started == 1)
		_started = 0;
}

int RenderDroid::open(int acceleration) {
	if (!_opened) {
		_curGeometryMode = 5;
		_curLightingMode = 5;
		_curTextureMode = 3;
		_curRenderOptions = 1;
		_curSortingMethod = 0;
		_curOcclusionMethod = 0;
		_curCullFlags = _demo ? 3 : 1;
		_curProcFaceUserData = 0;
		_colorEffects.clear(&_curColorEffects);
		_curVertexColorMode = 0;
		_frameTrue = 0;
		clearPreStatistics();
		_opened = 1;

		if (acceleration > 0 && !_std3D) {
			warning("RenderDroid::open(): no 3D device set");
			acceleration = 0;
		}

		_curAcceleration = acceleration;
	}

	return 1;
}

int RenderDroid::close() {
	int wasOpened = _opened;

	if (_opened)
		_opened = 0;

	return wasOpened;
}

int RenderDroid::setRenderOptions(int options) {
	_curRenderOptions = options;
	return options;
}

int RenderDroid::setGeometryMode(int mode) {
	_curGeometryMode = mode;
	return mode;
}

int RenderDroid::setLightingMode(int mode) {
	_curLightingMode = mode;
	return mode;
}

int RenderDroid::setTextureMode(int mode) {
	_curTextureMode = mode;
	return mode;
}

int RenderDroid::setSortingMethod(int method) {
	_curSortingMethod = method;
	return method;
}

int RenderDroid::setOcclusionMethod(int method) {
	_curOcclusionMethod = method;
	return method;
}

int RenderDroid::setZBufferMethod(int method) {
	_curZBufferMethod = method;
	return method;
}

int RenderDroid::advanceFrame() {
	clearPreStatistics();
	_frameTrue++;
	_camera.advanceFrame();

	return beginHardwareScene();
}

void RenderDroid::finishFrame() {
	_cache.flush();
	_cache.finishFrame();
	_primit3.clearFrameCounters();
}

void RenderDroid::clearPreStatistics() {
	_cache.clearFrameCounters();
	_active.clearFrameCounters();
	_model3.clearFrameCounters();
}

int RenderDroid::beginHardwareScene() {
	int result = _curAcceleration;

#if defined(GRIM_STD3D)
	if (result > 0)
		return _std3D->beginScene();
#endif

	return result;
}

} // End of namespace Grim
