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
		_hs(hs), _sputRoom(sputRoom), _sputActor(sputActor), _rd(rd), _overlays(nullptr), _numHotspots(0), _nextHandle(0) {
	memset(_hotspots, 0, sizeof(_hotspots));
	memset(_setName, 0, sizeof(_setName));
}

HotspotManager::~HotspotManager() {
	clear();
}

// Legacy and game screen coordinates to the canvas, the sizes have no offset
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

// Special and overlay hotspots are tested before the others
bool HotspotManager::isPriorityCompatible(int mode, int type) {
	if (mode == kModeOnlyPath || mode == kModeSpecial)
		return true;

	return mode == kModeNormal && type != kTypeSpecial;
}

// Even-odd test with integer math
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
		if (_hotspots[i]->overlayHandle >= 0 && _overlays)
			_overlays->destroy(_hotspots[i]->overlayHandle);

		delete _hotspots[i];
		_hotspots[i] = nullptr;
	}

	_numHotspots = 0;
}

void HotspotManager::removeAt(int index) {
	Hotspot *hotspot = _hotspots[index];

	if (hotspot->overlayHandle >= 0 && _overlays)
		_overlays->destroy(hotspot->overlayHandle);

	delete hotspot;

	for (int i = index; i < _numHotspots - 1; i++)
		_hotspots[i] = _hotspots[i + 1];

	_numHotspots--;
	_hotspots[_numHotspots] = nullptr;
}

// The hotspots of the previous set go, the ones that the scripts added stay
void HotspotManager::removeFileHotspots() {
	int i = 0;

	while (i < _numHotspots) {
		if (_hotspots[i]->added)
			i++;
		else
			removeAt(i);
	}
}

int HotspotManager::newHandle() {
	for (;;) {
		_nextHandle = (_nextHandle + 1) % kNumHandles;

		if (_nextHandle == 0)
			continue;

		bool used = false;

		for (int i = 0; i < _numHotspots; i++) {
			if (_hotspots[i]->handle == _nextHandle)
				used = true;
		}

		if (!used)
			return _nextHandle;
	}
}

bool HotspotManager::isInCurrentSetup(const Hotspot *hotspot) const {
	if (hotspot->setup == -1)
		return true;

	Room *room = _sputRoom->getCurrentRoom();

	return room && hotspot->setup == room->curSetup;
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

// The polygon of a hotspot linked to an actor follows the projected offset point
bool HotspotManager::getPolygon(const Hotspot *hotspot, int (*poly)[2]) const {
	for (int i = 0; i < hotspot->polyNum; i++) {
		poly[i][0] = hotspot->poly[i][0];
		poly[i][1] = hotspot->poly[i][1];
	}

	if (hotspot->type != kTypeDynamic || hotspot->actorIndex < 0)
		return true;

	SputActor *actor = _sputActor->actorDeref(hotspot->actorIndex);

	if (!actor || !actor->inUse)
		return false;

	RdVector3 offset;
	RdVector3 world;
	int screen[3];

	offset[0] = hotspot->offset[0];
	offset[1] = hotspot->offset[1];
	offset[2] = hotspot->offset[2];

	_rd->transformPoint34(world, offset, _sputActor->getOrientMatrix(actor));

	if (!_rd->_primit3.getScreenCoord(world, screen))
		return false;

	int pointX = screen[0];
	int pointY = screen[1];
	int centerX;
	int centerY;

	convertCoords1080(&pointX, &pointY, kCoordsLegacy, false);
	convertCoordsNative(&pointX, &pointY, hotspot->coordMode, false);
	getCentroid(poly, hotspot->polyNum, &centerX, &centerY);

	for (int i = 0; i < hotspot->polyNum; i++) {
		poly[i][0] += pointX - centerX;
		poly[i][1] += pointY - centerY;
	}

	return true;
}

void HotspotManager::loadCurrentRoom() {
	Room *room = _sputRoom->getCurrentRoom();

	if (room)
		load(room->name);
}

void HotspotManager::load(const char *setName) {
	if (!setName || !setName[0] || !scumm_stricmp(_setName, setName))
		return;

	removeFileHotspots();
	Common::strlcpy(_setName, setName, sizeof(_setName));

	Common::String fileName(setName);
	uint dot = fileName.findLastOf('.');

	if (dot != Common::String::npos)
		fileName.erase(dot);

	fileName += ".hot";

	Common::SeekableReadStream *stream = _hs->openFile(fileName.c_str());

	if (!stream) {
		warning("HotspotManager::load(): can't open %s", fileName.c_str());
		return;
	}

	TextBuffer text;

	text.size = (int)stream->size();
	text.data = new char[text.size + 1];
	text.pos = 0;
	text.size = (int)stream->read(text.data, text.size);
	text.data[text.size] = 0;
	delete stream;

	if (!parseSet(&text))
		warning("HotspotManager::load(): %s is not complete", fileName.c_str());

	delete[] text.data;
}

// Finds the next line that starts with the key and returns the rest of it
const char *HotspotManager::parseLine(TextBuffer *text, const char *key) {
	int keyLength = (int)strlen(key);

	while (text->pos < text->size) {
		char *line = text->data + text->pos;
		char *end = line;

		while (*end && *end != '\n')
			end++;

		text->pos = (int)(end - text->data);

		if (*end) {
			*end = 0;
			text->pos++;
		}

		while (*line == ' ' || *line == '\t')
			line++;

		if (strncmp(line, key, keyLength) != 0)
			continue;

		char next = line[keyLength];

		if (next != ' ' && next != '\t' && next != '\r' && next != 0)
			continue;

		char *rest = line + keyLength;

		while (*rest == ' ' || *rest == '\t')
			rest++;

		int length = (int)strlen(rest);

		while (length > 0 && (rest[length - 1] == '\r' || rest[length - 1] == ' ' || rest[length - 1] == '\t'))
			rest[--length] = 0;

		return rest;
	}

	return nullptr;
}

bool HotspotManager::parseSet(TextBuffer *text) {
	const char *totalLine = parseLine(text, "hotspots_total:");
	int total = 0;

	if (!totalLine || sscanf(totalLine, "%d", &total) != 1)
		return false;

	if (total < 0 || total >= kMaxHotspots) {
		warning("HotspotManager::parseSet(): invalid hotspot count %d", total);
		return false;
	}

	int loaded = 0;

	while (loaded < total) {
		const char *setupLine = parseLine(text, "setup");
		int setup = 0;

		if (!setupLine || sscanf(setupLine, "%d", &setup) != 1)
			return false;

		int num = parseSetup(text, setup);

		if (num < 0)
			return false;

		loaded += num;
	}

	return true;
}

// Returns the number of hotspots read, or -1
int HotspotManager::parseSetup(TextBuffer *text, int setup) {
	const char *numLine = parseLine(text, "num_hotspots:");
	int num = 0;

	if (!numLine || sscanf(numLine, "%d", &num) != 1 || num < 0)
		return -1;

	for (int i = 0; i < num; i++) {
		if (_numHotspots >= kMaxHotspots) {
			warning("HotspotManager::parseSetup(): more than %d hotspots", (int)kMaxHotspots);
			return -1;
		}

		Hotspot *hotspot = new Hotspot;

		memset(hotspot, 0, sizeof(Hotspot));

		if (!parseHotspot(text, setup, hotspot)) {
			delete hotspot;
			return -1;
		}

		_hotspots[_numHotspots++] = hotspot;
	}

	return num;
}

bool HotspotManager::parseHotspot(TextBuffer *text, int setup, Hotspot *hotspot) {
	const char *line = parseLine(text, "hotspot");

	if (!line)
		return false;

	Common::strlcpy(hotspot->name, line, sizeof(hotspot->name));

	line = parseLine(text, "id");

	if (!line)
		return false;

	Common::strlcpy(hotspot->id, line, sizeof(hotspot->id));

	line = parseLine(text, "type");

	if (!line)
		return false;

	if (!strcmp(line, "object"))
		hotspot->type = kTypeObject;
	else if (!strcmp(line, "walk"))
		hotspot->type = kTypeWalk;
	else if (!strcmp(line, "dynamic"))
		hotspot->type = kTypeDynamic;
	else if (!strcmp(line, "special"))
		hotspot->type = kTypeSpecial;
	else if (!strcmp(line, "dialog"))
		hotspot->type = kTypeDialog;
	else if (!strcmp(line, "options"))
		hotspot->type = kTypeOptions;
	else
		hotspot->type = kTypeNone;

	line = parseLine(text, "cursor");

	if (!line)
		return false;

	if (!strcmp(line, "hotspot"))
		hotspot->cursor = kCursorHotspot;
	else if (!strcmp(line, "walk"))
		hotspot->cursor = kCursorWalk;
	else if (!strcmp(line, "arrow_left"))
		hotspot->cursor = kCursorArrowLeft;
	else if (!strcmp(line, "arrow_right"))
		hotspot->cursor = kCursorArrowRight;
	else if (!strcmp(line, "arrow_up"))
		hotspot->cursor = kCursorArrowUp;
	else if (!strcmp(line, "arrow_down"))
		hotspot->cursor = kCursorArrowDown;
	else
		hotspot->cursor = kCursorNone;

	hotspot->actorIndex = -1;
	hotspot->overlayHandle = -1;

	if (hotspot->type == kTypeDynamic) {
		line = parseLine(text, "offset");

		if (!line || sscanf(line, " (%f %f %f)", &hotspot->offset[0], &hotspot->offset[1], &hotspot->offset[2]) != 3)
			return false;
	} else {
		line = parseLine(text, "path");

		if (!line || sscanf(line, "%d", &hotspot->pathLen) != 1 || hotspot->pathLen < 0 || hotspot->pathLen > Hotspot::kMaxPath)
			return false;

		const char *point = strchr(line, ':');

		for (int i = 0; i < hotspot->pathLen; i++) {
			point = point ? strchr(point, '(') : nullptr;

			if (!point || sscanf(point, "(%f %f %f)", &hotspot->path[i][0], &hotspot->path[i][1], &hotspot->path[i][2]) != 3)
				return false;

			point = strchr(point, ')');
		}

		const char *wait = point ? strstr(point, "wait") : nullptr;

		if (wait)
			sscanf(wait + 4, "%f", &hotspot->wait);
	}

	line = parseLine(text, "region");

	if (!line || sscanf(line, "%d", &hotspot->polyNum) != 1 || hotspot->polyNum < 0 || hotspot->polyNum > Hotspot::kMaxPolygon)
		return false;

	const char *vertex = strchr(line, ':');

	for (int i = 0; i < hotspot->polyNum; i++) {
		vertex = vertex ? strchr(vertex, '(') : nullptr;

		if (!vertex || sscanf(vertex, "(%d %d)", &hotspot->poly[i][0], &hotspot->poly[i][1]) != 2)
			return false;

		convertCoords1080(&hotspot->poly[i][0], &hotspot->poly[i][1], kCoordsLegacy, false);
		convertCoordsNative(&hotspot->poly[i][0], &hotspot->poly[i][1], kCoordsGameScreen, false);
		vertex = strchr(vertex, ')');
	}

	hotspot->added = 0;
	hotspot->handle = newHandle();
	hotspot->setup = setup;
	hotspot->coordMode = kCoordsGameScreen;

	return true;
}

int HotspotManager::getActiveHotspots(int canvasX, int canvasY, const Hotspot **out, int max, int mode) {
	loadCurrentRoom();

	for (int i = 0; i < _numHotspots; i++)
		setHoverHidden(_hotspots[i], 2);

	int count = 0;

	for (int pass = 0; pass < 2; pass++) {
		for (int i = 0; i < _numHotspots && count < max; i++) {
			const Hotspot *hotspot = _hotspots[i];
			bool priority = hotspot->type == kTypeSpecial || hotspot->type == kTypeOverlay;

			if (priority != (pass == 0) || !isInCurrentSetup(hotspot))
				continue;

			if (pass == 0) {
				if (!isPriorityCompatible(mode, hotspot->type))
					continue;
			} else if (!isCompatible(mode, hotspot->type, hotspot->cursor)) {
				continue;
			}

			int poly[Hotspot::kMaxPolygon][2];

			if (!getPolygon(hotspot, poly))
				continue;

			int pointX = canvasX;
			int pointY = canvasY;

			convertCoordsNative(&pointX, &pointY, hotspot->coordMode, false);

			if (!isInside(poly, hotspot->polyNum, pointX, pointY))
				continue;

			out[count++] = hotspot;

			if (pass == 0)
				setHoverHidden(hotspot, 1);
		}
	}

	return count;
}

int HotspotManager::addHotspot(const char *id, int x, int y, int width, int height, int cursor, int boost, int menuType,
		const char *hoverImage, int hoverLayer, int coords) {
	if (_numHotspots >= kMaxHotspots) {
		warning("HotspotManager::addHotspot(): too many hotspots");
		return -1;
	}

	Hotspot *hotspot = new Hotspot;

	memset(hotspot, 0, sizeof(Hotspot));

	hotspot->added = 1;
	hotspot->handle = newHandle();
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

	if (hoverImage && _overlays) {
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

	_hotspots[_numHotspots++] = hotspot;

	return hotspot->handle;
}

void HotspotManager::removeHotspot(int handle) {
	for (int i = 0; i < _numHotspots; i++) {
		if (_hotspots[i]->handle == handle) {
			removeAt(i);
			return;
		}
	}
}

// Moves the polygon so that its center is on the point
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

void HotspotManager::linkHotspot(const char *id, int actorIndex) {
	for (int i = 0; i < _numHotspots; i++) {
		if (strcmp(_hotspots[i]->id, id) == 0)
			_hotspots[i]->actorIndex = actorIndex;
	}
}

int HotspotManager::serialize(Common::WriteStream *stream) const {
	byte padding[20];

	memset(padding, kPaddingByte, sizeof(padding));

	stream->writeSint32LE(_numHotspots);
	stream->writeSint32LE(_nextHandle);
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

	_nextHandle = stream->readSint32LE();
	stream->read(padding, 20);
	stream->read(_setName, kSetNameSize);
	_setName[kSetNameSize - 1] = 0;

	if (stream->err() || stream->eos() || num < 0 || num > kMaxHotspots || _nextHandle < 0 || _nextHandle >= kNumHandles) {
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
