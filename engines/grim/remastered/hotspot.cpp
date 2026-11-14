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

#include "common/stream.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/actor/sputactor.h"
#include "engines/grim/remastered/hotspot.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/std/std_conffile.h"
#include "engines/grim/std/std_display.h"
#include "engines/grim/std/std_sys.h"

namespace Grim {

enum {
	kLegacyWidth = 640,
	kLegacyHeight = 480,
	kPlayWidth = 1440,
	kPlayOffset = 240,
	kCanvasHeight = 1080,
	kPaddingByte = 0xFF
};

HotspotManager::HotspotManager(SysStruct *hs, SputRoom *sputRoom, SputActorModule *sputActor, RenderDroid *rd) :
		_hs(hs), _sputRoom(sputRoom), _sputActor(sputActor), _rd(rd), _overlays(nullptr), _numHotspots(0), _serial(0) {
	memset(_hotspots, 0, sizeof(_hotspots));
	memset(_setName, 0, sizeof(_setName));
	memset(_argument, 0, sizeof(_argument));
}

HotspotManager::~HotspotManager() {
	clear();
}

void HotspotManager::convertCoords1080(int *x, int *y, int mode, bool isSize) {
	if (mode == kCoordsLegacy) {
		*x = kPlayWidth * *x / kLegacyWidth + (isSize ? 0 : kPlayOffset);
		*y = kCanvasHeight * *y / kLegacyHeight;
	} else if (mode == kCoordsGameScreen) {
		if (!isSize)
			*x += kPlayOffset;
	}
}

void HotspotManager::convertCoordsNative(int *x, int *y, int mode, bool isSize) {
	if (mode == kCoordsLegacy) {
		*x = kLegacyWidth * (*x - (isSize ? 0 : kPlayOffset)) / kPlayWidth;
		*y = kLegacyHeight * *y / kCanvasHeight;
	} else if (mode == kCoordsGameScreen) {
		if (!isSize)
			*x -= kPlayOffset;
	}
}

void HotspotManager::convertRectNative(StdRect *rect, int mode) {
	if (mode == kCoordsLegacy) {
		rect->x = kLegacyWidth * (rect->x - kPlayOffset) / kPlayWidth;
		rect->y = kLegacyHeight * rect->y / kCanvasHeight;
		rect->width = kLegacyWidth * rect->width / kPlayWidth;
		rect->height = kLegacyHeight * rect->height / kCanvasHeight;
	} else if (mode == kCoordsGameScreen) {
		rect->x -= kPlayOffset;
	}
}

bool HotspotManager::isCompatible(int mode, int type, int cursor) {
	switch (mode) {
	case kModeNormal:
		return type != kTypeSpecial && type != kTypeMenu;
	case kModeSpecial:
		return type == kTypeSpecial || type == kTypeOverlay;
	case kModeMenu:
		return type == kTypeMenu;
	case kModeOnlyPath:
		return type == kTypeWalk || type == kTypeSpecial || type == kTypeOverlay || (type == kTypeObject && cursor == kCursorWalk);
	case kModeQuery:
		return type == kTypeMenuDialog;
	default:
		return false;
	}
}

bool HotspotManager::isPriorityCompatible(int mode, int type) {
	if (mode == kModeOnlyPath || mode == kModeSpecial)
		return true;

	return mode == kModeNormal && type != kTypeSpecial;
}

bool HotspotManager::isInside(int (*poly)[2], int numPoints, int x, int y) {
	bool inside = false;

	for (int i = 0, j = numPoints - 1; i < numPoints; j = i++) {
		int xi = poly[i][0];
		int yi = poly[i][1];
		int xj = poly[j][0];
		int yj = poly[j][1];

		if ((yi > y) != (yj > y) && x < xi + (y - yi) * (xj - xi) / (yj - yi))
			inside = !inside;
	}

	return inside;
}

void HotspotManager::clear() {
	for (int i = 0; i < _numHotspots; i++) {
		delete _hotspots[i];
		_hotspots[i] = nullptr;
	}

	_numHotspots = 0;
	_setName[0] = 0;
}

int HotspotManager::newHandle() {
	for (;;) {
		_serial = (_serial + 1) % kNumHandles;

		bool used = false;

		for (int i = 0; i < _numHotspots; i++) {
			if (_hotspots[i]->handle == _serial)
				used = true;
		}

		if (!used)
			return _serial;
	}
}

void HotspotManager::discardLastHotspot() {
	warning("HotspotManager::parseSetup(): incomplete hotspot, the rest of the file is ignored");

	_numHotspots--;
	delete _hotspots[_numHotspots];
	_hotspots[_numHotspots] = nullptr;
}

void HotspotManager::setHoverHidden(const Hotspot *hotspot, int hidden) {
	if (hotspot->overlayHandle >= 0 && _overlays)
		_overlays->setHidden(hotspot->overlayHandle, hidden);
}

void HotspotManager::getCentroid(int (*poly)[2], int numPoints, int *x, int *y) const {
	int sumX = 0;
	int sumY = 0;

	for (int i = 0; i < numPoints; i++) {
		sumX += poly[i][0];
		sumY += poly[i][1];
	}

	*x = numPoints > 0 ? sumX / numPoints : 0;
	*y = numPoints > 0 ? sumY / numPoints : 0;
}

bool HotspotManager::updateDynamicPolygon(Hotspot *hotspot) {
	if (hotspot->actorIndex == -1)
		return false;

	SputActor *actor = _sputActor->actorDeref(hotspot->actorIndex);

	if (!actor)
		return false;

	RdVector3 offset;
	RdVector3 world;
	int screen[3];

	offset[0] = hotspot->offset[0];
	offset[1] = hotspot->offset[1];
	offset[2] = hotspot->offset[2];

	_rd->transformPoint34(world, offset, actor->orientMatrix);

	if (!_rd->_primit3.getScreenCoord(world, screen))
		return false;

	int pointX = screen[0];
	int pointY = screen[1];
	int centerX;
	int centerY;

	convertCoords1080(&pointX, &pointY, kCoordsLegacy, false);
	convertCoordsNative(&pointX, &pointY, hotspot->coordMode, false);
	getCentroid(hotspot->poly, hotspot->polyNum, &centerX, &centerY);

	for (int i = 0; i < hotspot->polyNum; i++) {
		hotspot->poly[i][0] += pointX - centerX;
		hotspot->poly[i][1] += pointY - centerY;
	}

	return true;
}

void HotspotManager::load(const char *setName) {
	if (!setName) {
		warning("HotspotManager::load(): no set name");
		return;
	}

	if (strcmp(_setName, setName) == 0)
		return;

	char fileName[144];

	Common::strlcpy(fileName, setName, sizeof(fileName));

	char *extension = strstr(fileName, ".set");

	if (!extension)
		extension = strstr(fileName, ".SET");

	if (!extension)
		return;

	extension[1] = 'h';
	extension[2] = 'o';
	extension[3] = 't';

	StdConffile *conffile = new StdConffile(_hs);

	if (conffile->open(fileName)) {
		Common::strlcpy(_setName, setName, sizeof(_setName));

		int i = 0;

		while (i < _numHotspots) {
			if (_hotspots[i]->added == 0) {
				delete _hotspots[i];
				_numHotspots--;
				_hotspots[i] = _hotspots[_numHotspots];
				_hotspots[_numHotspots] = nullptr;
			} else {
				i++;
			}
		}

		int total = 0;
		const char *argument = parseLine(conffile, "hotspots_total:");

		if (!argument || sscanf(argument, "%d", &total) <= 0)
			warning("HotspotManager::load(): invalid hotspot file %s", fileName);

		if (total >= kMaxHotspots)
			warning("HotspotManager::load(): too many hotspots in %s", fileName);

		while (conffile->readLine()) {
			int setup = 0;

			if (sscanf(conffile->getLine(), "setup %d:", &setup) > 0)
				parseSetup(conffile, setup);
		}

		conffile->close();
	}

	delete conffile;
}

const char *HotspotManager::parseLine(StdConffile *conffile, const char *key) {
	if (!conffile->readLine())
		return nullptr;

	int keyLength = (int)strlen(key);

	for (;;) {
		const char *text = conffile->getLine();

		while (*text == '\t' || *text == ' ')
			text++;

		if (strncmp(text, key, keyLength) == 0 && (int)strlen(text) > keyLength) {
			const char *after = text + keyLength;
			int blanks = 0;

			while (after[blanks] == '\t' || after[blanks] == ' ')
				blanks++;

			if (blanks != 0) {
				Common::strlcpy(_argument, after + blanks, sizeof(_argument));

				int length = (int)strlen(_argument);

				while (length > 0 && (_argument[length - 1] == '\t' || _argument[length - 1] == '\n' ||
						_argument[length - 1] == '\r' || _argument[length - 1] == ' '))
					_argument[--length] = 0;

				return _argument;
			}
		}

		if (!conffile->readLine())
			return nullptr;
	}
}

void HotspotManager::parseSetup(StdConffile *conffile, int setup) {
	int num = 0;
	const char *argument = parseLine(conffile, "num_hotspots:");

	if (!argument || sscanf(argument, "%d", &num) <= 0) {
		warning("HotspotManager::parseSetup(): invalid hotspot file, no number of hotspots");
		return;
	}

	// A failed scan leaves the previous values
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float offsetZ = 0.0f;
	float pointX = 0.0f;
	float pointY = 0.0f;
	float pointZ = 0.0f;
	int numRead = 0;

	for (int k = 0; k < num; k++) {
		numRead = 0;

		if (_numHotspots >= kMaxHotspots) {
			warning("HotspotManager::parseSetup(): more than %d hotspots", (int)kMaxHotspots);
			return;
		}

		Hotspot *hotspot = new Hotspot;

		memset(hotspot, 0, sizeof(Hotspot));
		hotspot->setup = setup;
		hotspot->handle = newHandle();
		_hotspots[_numHotspots++] = hotspot;
		hotspot->overlayHandle = -1;
		hotspot->added = 0;
		hotspot->actorIndex = -1;
		hotspot->wait = 0.0f;

		argument = parseLine(conffile, "hotspot");

		if (!argument) {
			discardLastHotspot();
			return;
		}

		Common::strlcpy(hotspot->name, argument, sizeof(hotspot->name));
		argument = parseLine(conffile, "id");

		if (!argument) {
			discardLastHotspot();
			return;
		}

		Common::strlcpy(hotspot->id, argument, sizeof(hotspot->id));
		argument = parseLine(conffile, "type");

		if (!argument) {
			discardLastHotspot();
			return;
		}

		hotspot->type = kTypeNone;

		if (!strcmp(argument, "object"))
			hotspot->type = kTypeObject;
		else if (!strcmp(argument, "walk"))
			hotspot->type = kTypeWalk;
		else if (!strcmp(argument, "dynamic"))
			hotspot->type = kTypeDynamic;
		else if (!strcmp(argument, "special"))
			hotspot->type = kTypeSpecial;
		else if (!strcmp(argument, "dialog"))
			hotspot->type = kTypeDialog;
		else if (!strcmp(argument, "options"))
			hotspot->type = kTypeOptions;

		argument = parseLine(conffile, "cursor");

		if (!argument) {
			discardLastHotspot();
			return;
		}

		hotspot->cursor = kCursorNone;

		if (!strcmp(argument, "hotspot"))
			hotspot->cursor = kCursorHotspot;
		else if (!strcmp(argument, "walk"))
			hotspot->cursor = kCursorWalk;
		else if (!strcmp(argument, "arrow_left"))
			hotspot->cursor = kCursorArrowLeft;
		else if (!strcmp(argument, "arrow_right"))
			hotspot->cursor = kCursorArrowRight;
		else if (!strcmp(argument, "arrow_up"))
			hotspot->cursor = kCursorArrowUp;
		else if (!strcmp(argument, "arrow_down"))
			hotspot->cursor = kCursorArrowDown;

		if (hotspot->type == kTypeDynamic) {
			hotspot->pathLen = 0;
			argument = parseLine(conffile, "offset");

			if (!argument) {
				discardLastHotspot();
				return;
			}

			if (sscanf(argument, " (%f %f %f)%n", &offsetX, &offsetY, &offsetZ, &numRead) <= 0)
				warning("HotspotManager::parseSetup(): invalid hotspot file, offset of %s", hotspot->id);

			hotspot->offset[0] = offsetX;
			hotspot->offset[1] = offsetY;
			hotspot->offset[2] = offsetZ;
		} else {
			hotspot->offset[0] = 0.0f;
			hotspot->offset[1] = 0.0f;
			hotspot->offset[2] = 0.0f;
			argument = parseLine(conffile, "path");

			if (!argument) {
				discardLastHotspot();
				return;
			}

			if (sscanf(argument, "%d:%n", &hotspot->pathLen, &numRead) <= 0)
				warning("HotspotManager::parseSetup(): invalid hotspot file, path of %s", hotspot->id);

			const char *point = argument + numRead;

			if (hotspot->pathLen < 0 || hotspot->pathLen > Hotspot::kMaxPath) {
				warning("HotspotManager::parseSetup(): path too long in %s", hotspot->id);
				hotspot->pathLen = hotspot->pathLen < 0 ? 0 : (int)Hotspot::kMaxPath;
			}

			for (int i = 0; i < hotspot->pathLen; i++) {
				sscanf(point, " (%f %f %f)%n", &pointX, &pointY, &pointZ, &numRead);
				hotspot->path[i][0] = pointX;
				hotspot->path[i][1] = pointY;
				hotspot->path[i][2] = pointZ;
				point += numRead;
			}

			float wait = 0.0f;

			if (sscanf(point, " wait %f%n", &wait, &numRead) == 1)
				hotspot->wait = wait;
		}

		argument = parseLine(conffile, "region");

		if (!argument) {
			discardLastHotspot();
			return;
		}

		if (sscanf(argument, "%d:%n", &hotspot->polyNum, &numRead) <= 0)
			warning("HotspotManager::parseSetup(): invalid hotspot file, region of %s", hotspot->id);

		const char *vertex = argument + numRead;

		if (hotspot->polyNum < 0 || hotspot->polyNum > Hotspot::kMaxPolygon) {
			warning("HotspotManager::parseSetup(): too many vertices in a polygon of %s", hotspot->id);
			hotspot->polyNum = hotspot->polyNum < 0 ? 0 : (int)Hotspot::kMaxPolygon;
		}

		for (int i = 0; i < hotspot->polyNum; i++) {
			sscanf(vertex, " (%d %d)%n", &hotspot->poly[i][0], &hotspot->poly[i][1], &numRead);
			convertCoords1080(&hotspot->poly[i][0], &hotspot->poly[i][1], kCoordsLegacy, false);
			convertCoordsNative(&hotspot->poly[i][0], &hotspot->poly[i][1], kCoordsGameScreen, false);
			vertex += numRead;
		}

		hotspot->coordMode = kCoordsGameScreen;
	}
}

int HotspotManager::getActiveHotspots(int canvasX, int canvasY, const Hotspot **out, int max, int mode) {
	Room *room = _sputRoom->getCurrentRoom();

	if (!room || _numHotspots <= 0)
		return 0;

	int setup = room->curSetup;
	int count = 0;

	for (int i = 0; i < _numHotspots; i++) {
		Hotspot *hotspot = _hotspots[i];

		setHoverHidden(hotspot, 2);

		if (hotspot->setup != setup && hotspot->setup != -1)
			continue;

		if (hotspot->type != kTypeSpecial && hotspot->type != kTypeOverlay)
			continue;

		if (!isPriorityCompatible(mode, hotspot->type))
			continue;

		int pointX = canvasX;
		int pointY = canvasY;

		convertCoordsNative(&pointX, &pointY, hotspot->coordMode, false);

		if (isInside(hotspot->poly, hotspot->polyNum, pointX, pointY) && count < max) {
			out[count++] = hotspot;
			setHoverHidden(hotspot, 1);
		}
	}

	// A hover image of these stays hidden, only its layer grows
	for (int i = 0; i < _numHotspots; i++) {
		Hotspot *hotspot = _hotspots[i];

		if (hotspot->setup != setup && hotspot->setup != -1)
			continue;

		if (hotspot->type == kTypeSpecial || hotspot->type == kTypeOverlay)
			continue;

		if (!isCompatible(mode, hotspot->type, hotspot->cursor) || count >= max)
			continue;

		bool unplaced = hotspot->type == kTypeDynamic;

		if (unplaced && updateDynamicPolygon(hotspot))
			unplaced = false;

		int pointX = canvasX;
		int pointY = canvasY;

		convertCoordsNative(&pointX, &pointY, hotspot->coordMode, false);

		if (!isInside(hotspot->poly, hotspot->polyNum, pointX, pointY) || unplaced)
			continue;

		out[count++] = hotspot;

		int layer;

		if (hotspot->overlayHandle >= 0 && _overlays && _overlays->getLayer(hotspot->overlayHandle, &layer))
			_overlays->setLayer(hotspot->overlayHandle, layer + 2);
	}

	return count;
}

int HotspotManager::addHotspot(const char *id, int x, int y, int width, int height, int cursor, int boost, int menuType,
		const char *hoverImage, int hoverLayer, int coords) {
	if ((int)strlen(id) >= Hotspot::kIdSize)
		warning("HotspotManager::addHotspot(): id too long");

	Hotspot *hotspot = new Hotspot;

	memset(hotspot, 0, sizeof(Hotspot));

	hotspot->added = 1;
	hotspot->setup = -1;
	Common::strlcpy(hotspot->name, id, sizeof(hotspot->name));
	Common::strlcpy(hotspot->id, id, sizeof(hotspot->id));

	if (menuType == 0)
		hotspot->type = kTypeOverlay;
	else if (menuType == 2)
		hotspot->type = kTypeMenuDialog;
	else
		hotspot->type = kTypeMenu;

	hotspot->cursor = cursor;
	hotspot->actorIndex = -1;
	hotspot->polyNum = 4;
	hotspot->poly[0][0] = x - boost;
	hotspot->poly[0][1] = y - boost;
	hotspot->poly[1][0] = x + width + boost;
	hotspot->poly[1][1] = y - boost;
	hotspot->poly[2][0] = x + width + boost;
	hotspot->poly[2][1] = y + height + boost;
	hotspot->poly[3][0] = x - boost;
	hotspot->poly[3][1] = y + height + boost;
	hotspot->coordMode = coords;
	hotspot->overlayHandle = -1;

	if (hoverImage && hoverImage[0] && _overlays) {
		int layer = hoverLayer;

		if (menuType != 0 && layer < 10)
			layer += 10;

		OverlayProps props;

		props.cache = OverlayManager::kCacheDefault;
		props.layer = layer;
		props.coords = coords;

		int overlay = _overlays->create(hoverImage, x, y, props);

		if (overlay != OverlayManager::kNoHandle) {
			hotspot->overlayHandle = overlay;
			_overlays->setHidden(overlay, 2);
		}
	}

	if (_numHotspots + 1 >= kMaxHotspots)
		warning("HotspotManager::addHotspot(): too many hotspots");

	if (_numHotspots > kMaxHotspots - 2) {
		delete hotspot;

		return -1;
	}

	hotspot->handle = newHandle();
	_hotspots[_numHotspots++] = hotspot;

	int handle = _serial;

	_serial = handle + 1;

	return handle;
}

// The last hotspot takes the place of the removed one
void HotspotManager::removeHotspot(int handle) {
	for (int i = 0; i < _numHotspots; i++) {
		if (_hotspots[i]->handle != handle)
			continue;

		Hotspot *hotspot = _hotspots[i];

		if (hotspot->overlayHandle >= 0 && _overlays)
			_overlays->destroy(hotspot->overlayHandle);

		delete hotspot;

		_numHotspots--;
		_hotspots[i] = _hotspots[_numHotspots];
		_hotspots[_numHotspots] = nullptr;

		return;
	}
}

void HotspotManager::updateHotspot(const char *id, int x, int y, int coords) {
	int pointX = x;
	int pointY = y;

	convertCoords1080(&pointX, &pointY, coords, false);

	for (int i = 0; i < _numHotspots; i++) {
		Hotspot *hotspot = _hotspots[i];

		if (strcmp(hotspot->id, id) != 0)
			continue;

		int targetX = pointX;
		int targetY = pointY;
		int centerX;
		int centerY;

		convertCoordsNative(&targetX, &targetY, hotspot->coordMode, false);
		getCentroid(hotspot->poly, hotspot->polyNum, &centerX, &centerY);

		for (int j = 0; j < hotspot->polyNum; j++) {
			hotspot->poly[j][0] += targetX - centerX;
			hotspot->poly[j][1] += targetY - centerY;
		}
	}
}

void HotspotManager::linkHotspot(const char *id, int actorHandle) {
	for (int i = 0; i < _numHotspots; i++) {
		if (strcmp(_hotspots[i]->id, id) == 0)
			_hotspots[i]->actorIndex = actorHandle;
	}
}

int HotspotManager::serialize(Common::WriteStream *stream) const {
	byte padding[20];

	memset(padding, kPaddingByte, sizeof(padding));

	stream->writeSint32LE(_numHotspots);
	stream->writeSint32LE(_serial);
	stream->write(padding, 20);
	stream->write(_setName, kSetNameSize);

	for (int i = 0; i < _numHotspots; i++) {
		const Hotspot *hotspot = _hotspots[i];

		stream->writeSint32LE(hotspot->added);
		stream->writeSint32LE(hotspot->handle);
		stream->writeSint32LE(hotspot->setup);
		stream->write(hotspot->name, Hotspot::kNameSize);
		stream->write(hotspot->id, Hotspot::kIdSize);
		stream->writeSint32LE(hotspot->type);
		stream->writeSint32LE(hotspot->cursor);
		stream->writeSint32LE(hotspot->actorIndex);

		for (int j = 0; j < 3; j++)
			stream->writeFloatLE(hotspot->offset[j]);

		stream->writeSint32LE(hotspot->pathLen);

		for (int j = 0; j < hotspot->pathLen; j++) {
			for (int k = 0; k < 3; k++)
				stream->writeFloatLE(hotspot->path[j][k]);
		}

		stream->writeSint32LE(hotspot->polyNum);
		stream->writeSint32LE(hotspot->coordMode);

		for (int j = 0; j < hotspot->polyNum; j++) {
			stream->writeSint32LE(hotspot->poly[j][0]);
			stream->writeSint32LE(hotspot->poly[j][1]);
		}

		stream->writeFloatLE(hotspot->wait);
		stream->write(padding, 16);
	}

	if (stream->err()) {
		warning("HotspotManager::serialize(): failed to write the hotspots");
		return 1;
	}

	return 0;
}

int HotspotManager::deserialize(Common::ReadStream *stream) {
	byte padding[20];

	clear();

	int num = stream->readSint32LE();

	_serial = stream->readSint32LE();
	stream->read(padding, 20);
	stream->read(_setName, kSetNameSize);
	_setName[kSetNameSize - 1] = 0;

	if (stream->err() || stream->eos() || num < 0 || num > kMaxHotspots || _serial < 0 || _serial > kNumHandles) {
		warning("HotspotManager::deserialize(): invalid hotspot header");
		return 1;
	}

	for (int i = 0; i < num; i++) {
		Hotspot *hotspot = new Hotspot;

		memset(hotspot, 0, sizeof(Hotspot));
		hotspot->overlayHandle = -1;

		hotspot->added = stream->readSint32LE();
		hotspot->handle = stream->readSint32LE();
		hotspot->setup = stream->readSint32LE();
		stream->read(hotspot->name, Hotspot::kNameSize);
		stream->read(hotspot->id, Hotspot::kIdSize);
		hotspot->name[Hotspot::kNameSize - 1] = 0;
		hotspot->id[Hotspot::kIdSize - 1] = 0;
		hotspot->type = stream->readSint32LE();
		hotspot->cursor = stream->readSint32LE();
		hotspot->actorIndex = stream->readSint32LE();

		for (int j = 0; j < 3; j++)
			hotspot->offset[j] = stream->readFloatLE();

		hotspot->pathLen = stream->readSint32LE();

		if (hotspot->pathLen < 0 || hotspot->pathLen > Hotspot::kMaxPath) {
			warning("HotspotManager::deserialize(): invalid path length %d", hotspot->pathLen);
			delete hotspot;
			clear();
			return 1;
		}

		for (int j = 0; j < hotspot->pathLen; j++) {
			for (int k = 0; k < 3; k++)
				hotspot->path[j][k] = stream->readFloatLE();
		}

		hotspot->polyNum = stream->readSint32LE();
		hotspot->coordMode = stream->readSint32LE();

		if (hotspot->polyNum < 0 || hotspot->polyNum > Hotspot::kMaxPolygon) {
			warning("HotspotManager::deserialize(): invalid polygon size %d", hotspot->polyNum);
			delete hotspot;
			clear();
			return 1;
		}

		for (int j = 0; j < hotspot->polyNum; j++) {
			hotspot->poly[j][0] = stream->readSint32LE();
			hotspot->poly[j][1] = stream->readSint32LE();
		}

		hotspot->wait = stream->readFloatLE();
		stream->read(padding, 16);

		if (stream->err() || stream->eos()) {
			warning("HotspotManager::deserialize(): failed to read hotspot %d", i);
			delete hotspot;
			clear();
			return 1;
		}

		_hotspots[_numHotspots++] = hotspot;
	}

	return 0;
}

} // End of namespace Grim
