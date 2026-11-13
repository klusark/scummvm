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

#include "common/str.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/grim.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/std/std_conffile.h"
#include "engines/grim/walkplane.h"

namespace Grim {

int SputRoom::readSections() {
	readSectionFunc("colormaps", &SputRoom::parseColormaps);
	readSectionFunc("setups", &SputRoom::parseSetups);
	readSectionFunc("lights", &SputRoom::parseLights);
	readSectionFunc("sectors", &SputRoom::parseSectors);

	if (_grim->getGameFlags() & ADGF_DEMO)
		readSectionFunc("objectstates", &SputRoom::readObjectsSection);

	_started = 1;

	return 0;
}

int SputRoom::readSectionFunc(const char *name, SectionFunc read) {
	int index = sectionNameCompare(name);

	if (index == -1) {
		if (_numSections >= 32)
			return 0;

		index = _numSections++;
	}

	Common::strlcpy(_sections[index].name, name, 32);
	_sections[index].handler = read;

	return 1;
}

int SputRoom::sectionNameCompare(const char *name) {
	if (_numSections <= 0)
		return -1;

	int index = 0;

	while (scumm_stricmp(_sections[index].name, name)) {
		if (++index >= _numSections)
			return -1;
	}

	return index;
}

int SputRoom::loadSections(const char *fileName) {
	char name[1024];

	if (!_stdConffile->open(fileName))
		return 0;

	if (_stdConffile->readLine()) {
		while (sscanf(_stdConffile->getLine(), "section: %s", name) != 1 || findSectionHandler(name)) {
			if (!_stdConffile->readLine()) {
				_stdConffile->close();
				return 1;
			}
		}

		_stdConffile->close();
		return 0;
	}

	_stdConffile->close();

	return 1;
}

int SputRoom::findSectionHandler(const char *name) {
	int index = sectionNameCompare(name);

	return index == -1 || (this->*_sections[index].handler)() != 0;
}

int SputRoom::parseLights() {
	Room *room = _loadingRoom;
	RoomLight *light = nullptr;
	int lightIndex = -1;
	bool numLightsFlag = false;
	char name[1024];

	room->numLights = 0;

	if (!_stdConffile->readLine())
		return 1;

	while (true) {
		const char *line = _stdConffile->getLine();

		if (*line) {
			int numLights;

			name[0] = 0;

			if (sscanf(line, " section: %s\n", name)) {
				if (!findSectionHandler(name))
					return 0;
			} else if (sscanf(line, " numlights %d\n", &numLights) == 1) {
				if (numLights < 0 || numLights > 64) {
					warning("SputRoom::parseLights(): invalid number of lights %d", numLights);
					numLights = CLIP(numLights, 0, 64);
				}

				room->numLights = numLights;
				numLightsFlag = true;
				light = nullptr;
				lightIndex = -1;
				memset(room->lights, 0, sizeof(room->lights));
			} else if (numLightsFlag) {
				if (sscanf(line, " light %s", name) == 1) {
					lightIndex++;

					if (lightIndex >= 64) {
						warning("SputRoom::parseLights(): more than 64 lights");
						light = nullptr;
					} else {
						light = &room->lights[lightIndex];
						_rd->_light.newEntry(&light->light);

						if (strlen(name) >= 32)
							warning("SputRoom::parseLights(): light name %s is too long", name);

						Common::strlcpy(light->name, name, 32);
					}
				}

				if (!light) {
					warning("SputRoom::parseLights(): line outside a light ignored");
				} else {
					if (sscanf(line, " type %s", name) == 1) {
						if (!scumm_stricmp(name, "omni"))
							light->light.type = 2;
						else if (!strcmp(name, "spot"))
							light->light.type = 3;
						else
							light->light.type = 0;
					}

					if (sscanf(line, " direction %f %f %f", &light->light.direction[0], &light->light.direction[1], &light->light.direction[2]) == 3)
						_rd->normalize3Acc(light->light.direction);

					sscanf(line, " position %f %f %f", &light->position[0], &light->position[1], &light->position[2]);
					sscanf(line, " intensity %f", &light->light.intensity);
					sscanf(line, " umbraangle %f", &light->light.angleX);
					sscanf(line, " penumbraangle %f", &light->light.angleY);

					int red;
					int green;
					int blue;

					if (sscanf(line, " color %d %d %d", &red, &green, &blue) == 3)
						light->light.color = (uint)(blue | ((green | (red << 8)) << 8));
				}
			}
		}

		if (!_stdConffile->readLine())
			return 1;
	}
}

int SputRoom::parseSetups() {
	Room *room = _loadingRoom;
	bool demo = (_grim->getGameFlags() & ADGF_DEMO) != 0;
	int setupIndex = -1;
	char name[1024];
	char fileName[1024];

	room->setups = nullptr;

	if (!_stdConffile->readLine())
		return 1;

	while (true) {
		const char *line = _stdConffile->getLine();

		if (*line) {
			int numSetups;

			name[0] = 0;

			if (sscanf(line, " section: %s\n", name)) {
				if (!findSectionHandler(name))
					return 0;
			} else if (sscanf(line, " numsetups %d\n", &numSetups) == 1) {
				if (numSetups < 0) {
					warning("SputRoom::parseSetups(): invalid number of setups %d", numSetups);
					return 0;
				}

				room->numSetups = numSetups;
				room->setups = new RoomSetup[numSetups];
				memset(room->setups, 0, sizeof(RoomSetup) * numSetups);
				setupIndex = -1;

				if (demo && room->numObjects) {
					for (int i = 0; i < numSetups; i++) {
						room->setups[i].objectArt = new RoomObjectArt[room->numObjects];
						memset(room->setups[i].objectArt, 0, sizeof(RoomObjectArt) * room->numObjects);
					}
				}
			} else {
				if (!room->setups)
					return 0;

				if (sscanf(line, " setup %s", name) == 1) {
					setupIndex++;

					if (setupIndex >= (int)room->numSetups)
						return 0;

					if (strlen(name) >= 32)
						warning("SputRoom::parseSetups(): setup name %s is too long", name);

					Common::strlcpy(room->setups[setupIndex].name, name, 32);
				} else if (setupIndex < 0) {
					return 0;
				} else {
					RoomSetup *setup = &room->setups[setupIndex];

					if (demo && sscanf(line, " object_art %s %s", name, fileName) == 2) {
						for (uint i = 0; i < room->numObjects; i++) {
							if (strcmp(name, room->objects[i].name))
								continue;

							if (strlen(fileName) >= 32)
								warning("SputRoom::parseSetups(): object art name %s is too long", fileName);

							Common::strlcpy(setup->objectArt[i].name, name, 32);
							Common::strlcpy(setup->objectArt[i].bitmapName, fileName, 32);
						}
					} else if (demo && sscanf(line, " object_z %s %s", name, fileName) == 2) {
						for (uint i = 0; i < room->numObjects; i++) {
							if (strcmp(name, room->objects[i].name))
								continue;

							if (strlen(fileName) >= 32)
								warning("SputRoom::parseSetups(): object z buffer name %s is too long", fileName);

							Common::strlcpy(setup->objectArt[i].name, name, 32);
							Common::strlcpy(setup->objectArt[i].zBufferName, fileName, 32);
						}
					} else if (sscanf(line, " background %s", name) == 1) {
						if (strlen(name) >= 32)
							warning("SputRoom::parseSetups(): background name %s is too long", name);

						Common::strlcpy(setup->bitmapName, name, 32);
					} else if (sscanf(line, " zbuffer %s", name) == 1) {
						if (strlen(name) >= 32)
							warning("SputRoom::parseSetups(): zbuffer name %s is too long", name);

						Common::strlcpy(setup->zBufferName, name, 32);
					} else if (sscanf(line, " position %f %f %f", &setup->camera.position[0], &setup->camera.position[1], &setup->camera.position[2]) != 3 &&
							sscanf(line, " interest %f %f %f", &setup->camera.interest[0], &setup->camera.interest[1], &setup->camera.interest[2]) != 3 &&
							sscanf(line, " roll %f", &setup->camera.roll) != 1 &&
							sscanf(line, " fov %f", &setup->camera.fov) != 1 &&
							sscanf(line, " nclip %f", &setup->camera.nearClip) != 1 &&
							sscanf(line, " fclip %f", &setup->camera.farClip) != 1) {
						return 0;
					}
				}
			}
		}

		if (!_stdConffile->readLine())
			return 1;
	}
}

int SputRoom::readObjectsSection() {
	Room *room = _loadingRoom;
	RoomObject *object = nullptr;
	char name[1024];

	room->numObjects = 0;
	room->objects = nullptr;

	if (!_stdConffile->readLine())
		return 1;

	while (true) {
		const char *line = _stdConffile->getLine();

		if (*line) {
			int numObjects;

			name[0] = 0;

			if (sscanf(line, " section: %s\n", name)) {
				if (!findSectionHandler(name))
					return 0;
			} else {
				if (sscanf(line, " tot_objects %d\n", &numObjects) == 1) {
					delete[] room->objects;
					room->objects = nullptr;
					room->numObjects = 0;

					if (numObjects > 0) {
						room->numObjects = numObjects;
						room->objects = new RoomObject[numObjects];
						memset(room->objects, 0, sizeof(RoomObject) * numObjects);
					}

					object = room->objects;
				}

				if (object) {
					RdVector3 position;

					if (sscanf(line, " object %s %f %f %f\n", name, &position[0], &position[1], &position[2]) == 4) {
						if ((uint)(object - room->objects) >= room->numObjects) {
							warning("SputRoom::readObjectsSection(): more than %u objects", room->numObjects);
							return 0;
						}

						if (strlen(name) >= 32)
							warning("SputRoom::readObjectsSection(): object name %s is too long", name);

						Common::strlcpy(object->name, name, 32);
						memcpy(object->position, position, sizeof(RdVector3));
						object++;
					}
				}
			}
		}

		if (!_stdConffile->readLine())
			return 1;
	}
}

int SputRoom::parseColormaps() {
	Room *room = _loadingRoom;
	char name[1024];

	if (room->colormaps)
		warning("SputRoom::parseColormaps(): pRoom->apColormaps == NULL");

	if (!_stdConffile->readLine())
		return 1;

	while (true) {
		const char *line = _stdConffile->getLine();

		if (*line) {
			int numColormaps;
			bool readName = true;

			name[0] = 0;

			if (sscanf(line, " section: %s\n", name)) {
				if (!findSectionHandler(name))
					return 0;
			} else {
				if (sscanf(line, " numcolormaps %d\n", &numColormaps) == 1) {
					if (numColormaps < 0) {
						warning("SputRoom::parseColormaps(): invalid number of colormaps %d", numColormaps);
						return 0;
					}

					room->numcolormaps = numColormaps;

					if (numColormaps) {
						room->colormaps = new ResRef[numColormaps];
						memset(room->colormaps, 0, sizeof(ResRef) * numColormaps);
					} else {
						readName = false;
					}
				}

				if (readName && room->colormaps) {
					if (sscanf(line, " colormap %s", name) == 1) {
						if (strlen(name) >= 32)
							warning("SputRoom::parseColormaps(): colormap name %s is too long", name);

						Common::strlcpy(room->colormapName, name, 32);
					}
				}
			}
		}

		if (!_stdConffile->readLine())
			return 1;
	}
}

int SputRoom::parseSectors() {
	const char *nextSection = _walkSystem->parseCollection(_loadingRoom->name);

	if (nextSection)
		findSectionHandler(nextSection);

	_walkSystem->markPathFinderDirty();

	return 1;
}

} // End of namespace Grim
