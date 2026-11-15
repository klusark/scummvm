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
#include "engines/grim/emi/graphics/cdrawcontext.h"
#include "engines/grim/emi/nodes/cnode.h"
#include "engines/grim/emi/math/cquaternion.h"
#include "engines/grim/emi/math/cvector.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/math/irect.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/lua_v2_opcodes/lua_v2.h"
#include "engines/grim/emi/scene/room_v2.h"
#include "engines/grim/emi/scene/walkplane_v2.h"
#include "engines/grim/emi/scene/walkplanecollection.h"

namespace Grim {

static const float kMinVectorLength = 1e-20f;
static const float kFlatDotLimit = 1.000001f;
static const float kPs2ScreenWidth = 640.0f;
static const float kPs2ScreenHeight = 480.0f;
static const float kPs2HalfScreenWidth = 320.0f;
static const float kPs2HalfScreenHeight = 240.0f;

void Lua_V2::WorldToScreen() {
	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object zObj = lua_getparam(3);

	bool isPs2 = _engine->getGamePlatform() == Common::kPlatformPS2;

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		if ((isPs2 || _engine->isDemo()) && _engine->getConsoleServices())
			_engine->getConsoleServices()->logDebug("WorldToScreen: Parameters must be x, y, z coords.\n");

		return;
	}

	float z = lua_getnumber(zObj);
	float y = lua_getnumber(yObj);
	float x = lua_getnumber(xObj);
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom)
		return;

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::WorldToScreen(): the room has no camera");

		return;
	}

	camera->updateViewMatrix();

	// PS2 computes in floats on a fixed 640x480 screen
	if (isPs2) {
		const float *m = camera->_view._rot._m;
		float pointX = ((x * m[0] + y * m[3]) + z * m[6]) + camera->_view._pos._x;
		float pointY = ((x * m[1] + y * m[4]) + z * m[7]) + camera->_view._pos._y;
		float depth = ((x * m[2] + y * m[5]) + z * m[8]) + camera->_view._pos._z;

		if (depth == 0.0f) {
			warning("Lua_V2::WorldToScreen(): point is on the camera plane");
			lua_pushnil();
			lua_pushnil();

			return;
		}

		float inverseDepth = 1.0f / depth;
		float ps2ScreenX = camera->_halfWidth + inverseDepth * (pointX * camera->_focalX);
		float ps2ScreenY = camera->_halfHeight + inverseDepth * (-pointY * camera->_focalY);

		if (!_engine->getDrawContext() || ps2ScreenX < 0.0f || !(ps2ScreenX <= kPs2ScreenWidth) || ps2ScreenY < 0.0f || !(ps2ScreenY <= kPs2ScreenHeight)) {
			lua_pushnil();
			lua_pushnil();

			return;
		}

		lua_pushnumber((ps2ScreenX - kPs2HalfScreenWidth) / kPs2HalfScreenWidth);
		lua_pushnumber((kPs2HalfScreenHeight - ps2ScreenY) / kPs2HalfScreenHeight);

		return;
	}

	float view[12];

	for (int i = 0; i < 9; i++)
		view[i] = camera->_view._rot._m[i];

	view[9] = camera->_view._pos._x;
	view[10] = camera->_view._pos._y;
	view[11] = camera->_view._pos._z;

	double viewX = ((double)view[3] * y + (double)view[6] * z) + (double)view[0] * x;
	float viewY = (float)(((double)view[4] * y + (double)view[7] * z) + (double)view[1] * x);
	float viewDepth = (float)(((double)view[5] * y + (double)view[8] * z) + (double)view[2] * x);
	float screenYTerm = (float)((double)view[10] + viewY);
	float inverseDepth = (float)(1.0 / ((double)view[11] + viewDepth));
	float screenX = (float)((((viewX + view[9]) * camera->_focalX) * inverseDepth) + camera->_halfWidth);
	float screenY = (float)((double)camera->_halfHeight - (((double)screenYTerm * camera->_focalY) * inverseDepth));

	if (!_engine->getDrawContext()) {
		lua_pushnil();
		lua_pushnil();

		return;
	}

	int rectBuffer[4];
	int *rect = _engine->getDrawContext()->getViewportRect(rectBuffer);
	int width = rect[2];
	int height = rect[3];

	if (!(screenX >= 0.0f) || !((double)width >= (double)screenX) || !(screenY >= 0.0f) || !((double)height >= (double)screenY)) {
		lua_pushnil();
		lua_pushnil();

		return;
	}

	double halfWidth = (double)(width / 2);
	double halfHeight = (double)(height / 2);

	lua_pushnumber((float)(((double)screenX - halfWidth) / halfWidth));
	lua_pushnumber((float)((halfHeight - (double)screenY) / halfHeight));
}

void Lua_V2::GetCameraPosition() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "GetCameraPosition: there is no camera.\n");
		lua_pushnil();

		return;
	}

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::GetCameraPosition(): the room has no camera");
		lua_pushnil();

		return;
	}

	if ((camera->_nodeFlags & CNode::kWorldValid) == 0)
		camera->updateWorldTransform();

	lua_pushnumber(camera->_world._pos._x);

	if ((camera->_nodeFlags & CNode::kWorldValid) == 0)
		camera->updateWorldTransform();

	lua_pushnumber(camera->_world._pos._y);

	if ((camera->_nodeFlags & CNode::kWorldValid) == 0)
		camera->updateWorldTransform();

	lua_pushnumber(camera->_world._pos._z);
}

void Lua_V2::SetCameraPosition() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "SetCameraPosition: there is no camera.\n");
		lua_pushnil();

		return;
	}

	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object zObj = lua_getparam(3);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		logDemo(kConsoleStatus, "SetCameraPosition: the parameters must be numbers.\n");
		lua_pushnil();

		return;
	}

	CVector position;

	position._z = lua_getnumber(zObj);
	position._y = lua_getnumber(yObj);
	position._x = lua_getnumber(xObj);

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::SetCameraPosition(): the room has no camera");

		return;
	}

	camera->setLocalPosition(position);
}

void Lua_V2::CameraLookAt() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "CameraLookAt: there is no camera.\n");
		lua_pushnil();

		return;
	}

	lua_Object xObj = lua_getparam(1);
	lua_Object yObj = lua_getparam(2);
	lua_Object zObj = lua_getparam(3);

	if (!lua_isnumber(xObj) || !lua_isnumber(yObj) || !lua_isnumber(zObj)) {
		logDemo(kConsoleStatus, "CameraLookAt: the parameters must be numbers.\n");
		lua_pushnil();

		return;
	}

	CVector target;

	target._z = lua_getnumber(zObj);
	target._y = lua_getnumber(yObj);
	target._x = lua_getnumber(xObj);

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::CameraLookAt(): the room has no camera");

		return;
	}

	camera->lookAt(target);
}

void Lua_V2::GetCameraFOV() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "GetCameraFOV: there is no camera.\n");
		lua_pushnil();

		return;
	}

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::GetCameraFOV(): the room has no camera");
		lua_pushnil();

		return;
	}

	lua_pushnumber(camera->_fovX);
}

void Lua_V2::SetCameraFOV() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "SetCameraFOV: there is no camera.\n");
		lua_pushnil();

		return;
	}

	lua_Object fovObj = lua_getparam(1);

	if (!lua_isnumber(fovObj)) {
		logDemo(kConsoleStatus, "SetCameraFOV: the parameter must be a number.\n");
		lua_pushnil();

		return;
	}

	CDrawContext *drawContext = _engine->getDrawContext();

	if (!drawContext) {
		warning("Lua_V2::SetCameraFOV(): there is no draw context");

		return;
	}

	currentRoom->getSetupCamera(-1);

	int rectBuffer[4];
	int *rect = drawContext->getViewportRect(rectBuffer);
	IRect viewport;

	viewport.x = rect[0];
	viewport.y = rect[1];
	viewport.w = rect[2];
	viewport.h = rect[3];

	float fov = lua_getnumber(fovObj);
	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::SetCameraFOV(): the room has no camera");

		return;
	}

	camera->setViewport(&viewport, fov);
}

void Lua_V2::GetCameraYaw() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "GetCameraYaw: there is no camera.\n");
		lua_pushnil();

		return;
	}

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::GetCameraYaw(): the room has no camera");
		lua_pushnil();

		return;
	}

	if ((camera->_nodeFlags & CNode::kEulerValid) == 0)
		camera->computeLocalMatrixFromWorld();

	lua_pushnumber(camera->_eulerDeg._y);
}

void Lua_V2::YawCamera() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "YawCamera: there is no camera.\n");
		lua_pushnil();

		return;
	}

	lua_Object angleObj = lua_getparam(1);

	if (!lua_isnumber(angleObj)) {
		logDemo(kConsoleStatus, "YawCamera: the parameter must be a number.\n");
		lua_pushnil();

		return;
	}

	CVector axis;

	axis._x = 0.0f;
	axis._y = 1.0f;
	axis._z = 0.0f;

	float angle = (float)((double)lua_getnumber(angleObj) * Actor::kDegreesToRadians);
	CQuaternion rotation(axis, angle);
	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::YawCamera(): the room has no camera");

		return;
	}

	if ((camera->_nodeFlags & CNode::kQuaternionValid) == 0)
		camera->updateLocalQuaternion();

	double rotationX = rotation._x;
	double rotationY = rotation._y;
	double rotationZ = rotation._z;
	double rotationW = rotation._w;
	float resultW = (float)(((rotationW * camera->_quat._w - rotationX * camera->_quat._x) - rotationY * camera->_quat._y) - rotationZ * camera->_quat._z);
	float resultZ = (float)(((rotationY * camera->_quat._x + rotationZ * camera->_quat._w) + rotationW * camera->_quat._z) - rotationX * camera->_quat._y);
	float resultY = (float)(((rotationY * camera->_quat._w + rotationW * camera->_quat._y) + rotationX * camera->_quat._z) - rotationZ * camera->_quat._x);
	float resultX = (float)(((rotationW * camera->_quat._x + rotationZ * camera->_quat._y) + rotationX * camera->_quat._w) - rotationY * camera->_quat._z);

	camera->_quat = CQuaternion(resultX, resultY, resultZ, resultW);
	camera->_nodeFlags = (camera->_nodeFlags & ~CNode::kEulerValid) | CNode::kQuaternionValid | CNode::kLocalSet;
	camera->updateLocalState();
}

void Lua_V2::GetCameraPitch() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "GetCameraPitch: there is no camera.\n");
		lua_pushnil();

		return;
	}

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::GetCameraPitch(): the room has no camera");
		lua_pushnil();

		return;
	}

	if ((camera->_nodeFlags & CNode::kEulerValid) == 0)
		camera->computeLocalMatrixFromWorld();

	lua_pushnumber(camera->_eulerDeg._x);
}

void Lua_V2::PitchCamera() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "PitchCamera: there is no camera.\n");
		lua_pushnil();

		return;
	}

	lua_Object angleObj = lua_getparam(1);

	if (!lua_isnumber(angleObj)) {
		logDemo(kConsoleStatus, "PitchCamera: the parameter must be a number.\n");
		lua_pushnil();

		return;
	}

	CVector axis;

	axis._x = 1.0f;
	axis._y = 0.0f;
	axis._z = 0.0f;

	float angle = (float)((double)lua_getnumber(angleObj) * Actor::kDegreesToRadians);
	CQuaternion rotation(axis, angle);
	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::PitchCamera(): the room has no camera");

		return;
	}

	if ((camera->_nodeFlags & CNode::kQuaternionValid) == 0)
		camera->updateLocalQuaternion();

	double rotationX = rotation._x;
	double rotationY = rotation._y;
	double rotationZ = rotation._z;
	double rotationW = rotation._w;
	float resultW = (float)(((rotationW * camera->_quat._w - rotationX * camera->_quat._x) - rotationY * camera->_quat._y) - rotationZ * camera->_quat._z);
	float resultZ = (float)(((rotationY * camera->_quat._x + rotationZ * camera->_quat._w) + rotationW * camera->_quat._z) - rotationX * camera->_quat._y);
	float resultY = (float)(((rotationY * camera->_quat._w + rotationW * camera->_quat._y) + rotationX * camera->_quat._z) - rotationZ * camera->_quat._x);
	float resultX = (float)(((rotationW * camera->_quat._x + rotationZ * camera->_quat._y) + rotationX * camera->_quat._w) - rotationY * camera->_quat._z);

	camera->_quat = CQuaternion(resultX, resultY, resultZ, resultW);
	camera->_nodeFlags = (camera->_nodeFlags & ~CNode::kEulerValid) | CNode::kQuaternionValid | CNode::kLocalSet;
	camera->updateLocalState();
}

void Lua_V2::GetCameraRoll() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "GetCameraRoll: there is no camera.\n");
		lua_pushnil();

		return;
	}

	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::GetCameraRoll(): the room has no camera");
		lua_pushnil();

		return;
	}

	if ((camera->_nodeFlags & CNode::kEulerValid) == 0)
		camera->computeLocalMatrixFromWorld();

	lua_pushnumber(camera->_eulerDeg._z);
}

void Lua_V2::RollCamera() {
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleStatus, "RollCamera: there is no camera.\n");
		lua_pushnil();

		return;
	}

	lua_Object angleObj = lua_getparam(1);

	if (!lua_isnumber(angleObj)) {
		logDemo(kConsoleStatus, "RollCamera: the parameter must be a number.\n");
		lua_pushnil();

		return;
	}

	CVector axis;

	axis._x = 0.0f;
	axis._y = 0.0f;
	axis._z = 1.0f;

	float angle = (float)((double)lua_getnumber(angleObj) * Actor::kDegreesToRadians);
	CQuaternion rotation(axis, angle);
	Camera *camera = currentRoom->getSetupCamera(-1);

	if (!camera) {
		warning("Lua_V2::RollCamera(): the room has no camera");

		return;
	}

	if ((camera->_nodeFlags & CNode::kQuaternionValid) == 0)
		camera->updateLocalQuaternion();

	double rotationX = rotation._x;
	double rotationY = rotation._y;
	double rotationZ = rotation._z;
	double rotationW = rotation._w;
	float resultW = (float)(((rotationW * camera->_quat._w - rotationX * camera->_quat._x) - rotationY * camera->_quat._y) - rotationZ * camera->_quat._z);
	float resultZ = (float)(((rotationY * camera->_quat._x + rotationZ * camera->_quat._w) + rotationW * camera->_quat._z) - rotationX * camera->_quat._y);
	float resultY = (float)(((rotationY * camera->_quat._w + rotationW * camera->_quat._y) + rotationX * camera->_quat._z) - rotationZ * camera->_quat._x);
	float resultX = (float)(((rotationW * camera->_quat._x + rotationZ * camera->_quat._y) + rotationX * camera->_quat._w) - rotationY * camera->_quat._z);

	camera->_quat = CQuaternion(resultX, resultY, resultZ, resultW);
	camera->_nodeFlags = (camera->_nodeFlags & ~CNode::kEulerValid) | CNode::kQuaternionValid | CNode::kLocalSet;
	camera->updateLocalState();
}

void Lua_V2::GetCameraLookVector() {
	lua_Object setupObj = lua_getparam(1);
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom) {
		logDemo(kConsoleWarning, "GetCameraLookVector: there is no current set.\n");
		lua_pushnil();

		return;
	}

	int setupIndex;

	if (lua_isnumber(setupObj))
		setupIndex = luaFtol(lua_getnumber(setupObj));
	else
		setupIndex = currentRoom->_currentSetup;

	Camera *camera = currentRoom->getSetupCamera(setupIndex);

	if (!camera) {
		warning("Lua_V2::GetCameraLookVector(): the room has no camera");
		lua_pushnil();

		return;
	}

	if ((camera->_nodeFlags & CNode::kWorldValid) == 0)
		camera->updateWorldTransform();

	double lookX = camera->_world._rot._m[6];
	double lookY = camera->_world._rot._m[7];
	double lookZ = camera->_world._rot._m[8];
	float resultX = camera->_world._rot._m[6];
	float resultY = camera->_world._rot._m[7];
	float resultZ = camera->_world._rot._m[8];
	double length = sqrt((lookX * lookX + lookY * lookY) + lookZ * lookZ);

	if (length >= (double)kMinVectorLength) {
		double inverse = 1.0 / length;

		resultX = (float)(lookX * inverse);
		resultY = (float)(lookY * inverse);
		resultZ = (float)(lookZ * inverse);
	}

	lua_Object table = lua_createtable();

	lua_pushobject(table);
	lua_pushstring("x");
	lua_pushnumber(resultX);
	lua_settable();
	lua_pushobject(table);
	lua_pushstring("y");
	lua_pushnumber(resultY);
	lua_settable();
	lua_pushobject(table);
	lua_pushstring("z");
	lua_pushnumber(resultZ);
	lua_settable();
	lua_pushobject(table);
}

void Lua_V2::PerSecond() {
	lua_Object valueObj = lua_getparam(1);

	// The disasm does not return here, so a second value is pushed
	if (!lua_isnumber(valueObj))
		lua_pushnil();

	float frameTime = _engine->getFrameTime();

	lua_pushnumber((float)((double)lua_getnumber(valueObj) * frameTime));
}

void Lua_V2::SnapToPlane() {
	lua_Object planeObj = lua_getparam(1);
	lua_Object xObj = lua_getparam(2);
	lua_Object yObj = lua_getparam(3);
	lua_Object zObj = lua_getparam(4);
	Room_v2 *currentRoom = Room_v2::getCurrentRoom(_engine);

	if (!currentRoom || !currentRoom->_walkPlanes || !lua_isstring(planeObj)) {
		warning("Lua_V2::SnapToPlane(): missing walk planes or plane name");
		lua_pushnil();

		return;
	}

	LString planeName = lua_getstring(planeObj);
	WalkPlane_v2 *plane = currentRoom->_walkPlanes->planeNamed(planeName);

	if (!plane) {
		logDemo(kConsoleWarning, "SnapToPlane: the plane %s was not found.\n", planeName.c_str());
		lua_pushnil();

		return;
	}

	float point[3];

	point[0] = lua_getnumber(xObj);
	point[1] = lua_getnumber(yObj);
	point[2] = lua_getnumber(zObj);
	plane->project(point);
	lua_pushnumber(point[0]);
	lua_pushnumber(point[1]);
	lua_pushnumber(point[2]);
}

void Lua_V2::GetAngleBetweenVectors() {
	lua_Object firstObj = lua_getparam(1);
	lua_Object secondObj = lua_getparam(2);

	if (!lua_istable(firstObj) || !lua_istable(secondObj)) {
		logDemo(kConsoleWarning, "GetAngleBetweenVectors: both arguments must be tables.\n");
		lua_pushnil();

		return;
	}

	float firstVector[3];
	float secondVector[3];
	const char *const components[3] = { "x", "y", "z" };

	for (int i = 0; i < 3; i++) {
		lua_pushobject(firstObj);
		lua_pushstring(components[i]);
		firstVector[i] = lua_getnumber(lua_gettable());
	}

	for (int i = 0; i < 3; i++) {
		lua_pushobject(secondObj);
		lua_pushstring(components[i]);
		secondVector[i] = lua_getnumber(lua_gettable());
	}

	double firstLength = sqrt(((double)firstVector[1] * firstVector[1] + (double)firstVector[2] * firstVector[2]) + (double)firstVector[0] * firstVector[0]);
	double firstZ = firstVector[2];

	if (firstLength >= (double)kMinVectorLength) {
		double inverse = 1.0 / firstLength;

		firstVector[0] = (float)(inverse * firstVector[0]);
		firstVector[1] = (float)(inverse * firstVector[1]);
		firstZ = firstVector[2] * inverse;
	}

	double secondLength = sqrt(((double)secondVector[1] * secondVector[1] + (double)secondVector[2] * secondVector[2]) + (double)secondVector[0] * secondVector[0]);
	double secondZ = secondVector[2];

	if (secondLength >= (double)kMinVectorLength) {
		double inverse = 1.0 / secondLength;

		secondVector[0] = (float)(inverse * secondVector[0]);
		secondVector[1] = (float)(inverse * secondVector[1]);
		secondZ = secondVector[2] * inverse;
	}

	float dot = (float)(((secondZ * firstZ) + (double)secondVector[1] * firstVector[1]) + (double)secondVector[0] * firstVector[0]);

	if (!(dot > 1.0f) || !(dot < kFlatDotLimit))
		lua_pushnumber((float)(acos((double)dot) * WalkPlane_v2::kRadiansToDegrees));
	else
		lua_pushnumber(0.0f);
}

} // end of namespace Grim
