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

#include "audio/mixer.h"

#include "common/config-manager.h"
#include "common/endian.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/util.h"

#include "engines/grim/grim.h"
#include "engines/grim/std/std_sort.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

static Common::ConfigManager::Domain *getConfDomain() {
	Common::ConfigManager::Domain *domain = ConfMan.getActiveDomain();

	if (!domain)
		domain = ConfMan.getDomain(Common::ConfigManager::kApplicationDomain);

	return domain;
}

static bool isStorableName(const char *valueName) {
	uint length = strlen(valueName);

	if (!length || valueName[0] == ' ' || valueName[length - 1] == ' ' || valueName[0] == '#' || valueName[0] == '[')
		return false;

	for (uint i = 0; i < length; i++) {
		if (valueName[i] == '=' || (byte)valueName[i] < 0x20)
			return false;
	}

	return true;
}

static bool isStorableString(const char *data) {
	uint length = strlen(data);

	if (length && (data[0] == ' ' || data[0] == '\t' || data[length - 1] == ' ' || data[length - 1] == '\t'))
		return false;

	for (uint i = 0; i < length; i++) {
		if (data[i] == '\r' || data[i] == '\n')
			return false;
	}

	return true;
}

static bool canParseInt(const Common::String &text) {
	char *end;

	if (text.empty())
		return true;

	strtol(text.c_str(), &end, 0);

	return end != text.c_str();
}

static bool canParseFloat(const Common::String &text) {
	char *end;

	if (text.empty())
		return true;

	strtof(text.c_str(), &end);

	return end != text.c_str();
}

static bool isNumberText(const Common::String &text) {
	char *end;

	if (text.empty())
		return false;

	strtod(text.c_str(), &end);

	return *end == '\0';
}

static bool queryText(const Common::String &text, byte *data, uint *size) {
	if (text.size() > *size) {
		*size = text.size();
		return false;
	}

	if (text.size())
		memcpy(data, text.c_str(), text.size());

	if (text.size() < *size)
		data[text.size()] = 0;

	*size = text.size();

	return true;
}

enum {
	kMapString,
	kMapInt,
	kMapVolume,
	kMapTalkSpeed,
	kMapSpeechMode,
	kMapBool,
	kMapPath,
	kMapFloat,
	kMapOptionalInt,
	kMapOptionalVolume,
	kMapLastSet
};

struct WuRegistryMapping {
	const char *name;
	const char *key;
	int kind;
};

static const WuRegistryMapping mappings[] = {
	{ "good_times", "game_devel_mode", kMapBool },
	{ "GrimDeveloper", "game_devel_mode", kMapBool },
	{ "GrimDataDir", "path", kMapPath },
	{ "GrimLastSet", "last_set", kMapLastSet },
	{ "MusicVolume", "music_volume", kMapVolume },
	{ "SfxVolume", "sfx_volume", kMapVolume },
	{ "VoiceVolume", "speech_volume", kMapVolume },
	{ "LastSavedGame", "last_saved_game", kMapString },
	{ "Gamma", "grim_gamma", kMapFloat },
	{ "GammaCorrection", "grim_gamma", kMapFloat },
	{ "VoiceEffects", "voice_effects", kMapString },
	{ "TextSpeed", "talkspeed", kMapTalkSpeed },
	{ "TextMode", "subtitles", kMapSpeechMode },
	{ "SpeechMode", "subtitles", kMapSpeechMode },
	{ "MovementMode", "movement", kMapString },
	{ "JoystickEnabled", "joystick", kMapString },
	{ "SpewOnError", "spew_on_error", kMapString },
	{ "Transcript", "transcript", kMapString },
	{ "DirectorsCommentary", "directors_commentary", kMapOptionalInt },
	{ "Widescreen", "widescreen", kMapOptionalInt },
	{ "Language", "grim_language", kMapOptionalInt },
	{ "ResolutionScaling", "resolution_scaling", kMapOptionalInt },
	{ "MouseSpeed", "mouse_speed", kMapOptionalInt },
	{ "AdvancedLighting", "advanced_lighting", kMapOptionalInt },
	{ "DirectorsCommentaryVolume", "directors_commentary_volume", kMapOptionalVolume },
	{ "RenderingMode", "rendering_mode", kMapOptionalInt },
	{ "Fullscreen", "grim_fullscreen", kMapOptionalInt },
	{ "ClassicLighting", "classic_lighting", kMapOptionalInt },
	{ "ResolutionX", "resolution_x", kMapOptionalInt },
	{ "ResolutionY", "resolution_y", kMapOptionalInt },
	{ "CrossSaveAutoSync", "cross_save_auto_sync", kMapOptionalInt }
};

// The remastered stores the last slot as a number; the original games store a string
static const WuRegistryMapping remasteredLastSavedGame = { "LastSavedGame", "last_saved_game", kMapOptionalInt };

static uint convertVolumeToMixer(uint volume) {
	return CLIP<uint>(volume * 2, 0, Audio::Mixer::kMaxMixerVolume);
}

static uint convertVolumeFromMixer(uint volume) {
	return CLIP<uint>(volume / 2, 0, 127);
}

static uint convertTalkSpeedToGUI(uint talkSpeed) {
	return CLIP<uint>(talkSpeed * 255 / 10, 0, 255);
}

static uint convertTalkSpeedFromGUI(uint talkSpeed) {
	return CLIP<uint>(talkSpeed * 10 / 255, 1, 10);
}

static bool convertSubtitlesToGUI(uint speechMode) {
	return speechMode == 1 || speechMode == 3;
}

static bool convertSpeechMuteToGUI(uint speechMode) {
	return speechMode == 1;
}

static uint convertSpeechModeFromGUI(bool subtitles, bool speechMute) {
	if (!subtitles && !speechMute)
		return 2;

	if (subtitles && !speechMute)
		return 3;

	if (subtitles && speechMute)
		return 1;

	warning("WuRegistry: both subtitles and speech are off, assuming subtitles only");

	return 1;
}

static const WuRegistryMapping *findMapping(const char *valueName) {
	if (g_grim->isRemastered() && !scumm_stricmp(remasteredLastSavedGame.name, valueName))
		return &remasteredLastSavedGame;

	for (uint i = 0; i < ARRAYSIZE(mappings); i++) {
		if (!scumm_stricmp(mappings[i].name, valueName))
			return &mappings[i];
	}

	return nullptr;
}

static bool isStringKind(int kind) {
	return kind == kMapString || kind == kMapPath || kind == kMapFloat || kind == kMapLastSet;
}

static uint getMappedType(const WuRegistryMapping *mapping) {
	if (mapping->kind == kMapFloat || mapping->kind == kMapOptionalInt || mapping->kind == kMapOptionalVolume)
		return ConfMan.hasKey(mapping->key) ? WuRegistry::kValueBinary : 0;

	if (isStringKind(mapping->kind))
		return WuRegistry::kValueString;

	if (mapping->kind == kMapBool && !ConfMan.getBool(mapping->key))
		return 0;

	return WuRegistry::kValueBinary;
}

static bool readMappedText(const WuRegistryMapping *mapping, Common::String *text);

static bool readMappedInt(const WuRegistryMapping *mapping, int *value) {
	switch (mapping->kind) {
	case kMapInt:
	case kMapOptionalInt:
		*value = ConfMan.getInt(mapping->key);
		return true;
	case kMapVolume:
	case kMapOptionalVolume:
		*value = convertVolumeFromMixer(ConfMan.getInt(mapping->key));
		return true;
	case kMapTalkSpeed:
		*value = convertTalkSpeedFromGUI(ConfMan.getInt(mapping->key));
		return true;
	case kMapSpeechMode:
		*value = convertSpeechModeFromGUI(ConfMan.getBool("subtitles"), ConfMan.getBool("speech_mute"));
		return true;
	case kMapBool:
		*value = ConfMan.getBool(mapping->key) ? 1 : 0;
		return *value != 0;
	default:
		break;
	}

	Common::String text;

	readMappedText(mapping, &text);
	*value = atoi(text.c_str());

	return true;
}

static bool readMappedText(const WuRegistryMapping *mapping, Common::String *text) {
	if (mapping->kind == kMapPath) {
		*text = ConfMan.getPath(mapping->key).toString(Common::Path::kNativeSeparator);
		return true;
	}

	if (isStringKind(mapping->kind)) {
		*text = ConfMan.get(mapping->key);
		return true;
	}

	int value;

	if (!readMappedInt(mapping, &value))
		return false;

	*text = Common::String::format("%d", value);

	return true;
}

static void writeMappedInt(const WuRegistryMapping *mapping, int value);

static void writeMappedText(const WuRegistryMapping *mapping, const char *text) {
	switch (mapping->kind) {
	case kMapString:
	case kMapFloat:
		ConfMan.set(mapping->key, text);
		break;
	case kMapPath:
		ConfMan.setPath(mapping->key, Common::Path(text, Common::Path::kNativeSeparator));
		break;
	case kMapLastSet:
		if (ConfMan.hasKey(mapping->key))
			ConfMan.set(mapping->key, text);

		break;
	case kMapBool:
		ConfMan.setBool(mapping->key, !scumm_stricmp(text, "true"));
		break;
	default:
		writeMappedInt(mapping, atoi(text));
		break;
	}
}

static void writeMappedInt(const WuRegistryMapping *mapping, int value) {
	switch (mapping->kind) {
	case kMapInt:
	case kMapOptionalInt:
		ConfMan.setInt(mapping->key, value);
		break;
	case kMapVolume:
	case kMapOptionalVolume:
		ConfMan.setInt(mapping->key, convertVolumeToMixer(value));
		break;
	case kMapTalkSpeed:
		ConfMan.setInt(mapping->key, convertTalkSpeedToGUI(value));
		break;
	case kMapSpeechMode:
		ConfMan.setBool("subtitles", convertSubtitlesToGUI(value));
		ConfMan.setBool("speech_mute", convertSpeechMuteToGUI(value));
		break;
	case kMapBool:
		ConfMan.setBool(mapping->key, value != 0);
		break;
	default:
		writeMappedText(mapping, Common::String::format("%d", value).c_str());
		break;
	}
}

WuRegistry::WuRegistry() : _started(0), _valueNames(nullptr), _numValues(0) {
}

WuRegistry::~WuRegistry() {
	if (_valueNames)
		freeIndex();
}

int WuRegistry::startup(const char *version) {
	_started = 1;

	ConfMan.registerDefault("subtitles", true);
	ConfMan.registerDefault("talkspeed", 179);
	ConfMan.registerDefault("game_devel_mode", false);

	if (ConfMan.getInt("talkspeed") == 60)
		ConfMan.setInt("talkspeed", 179);

	ConfMan.set("joystick", "false");

	// The remastered does not keep a version
	if (!g_grim->isRemastered()) {
		char storedVersion[128];

		if (getStr("Version", storedVersion, sizeof(storedVersion), nullptr) && scumm_stricmp(storedVersion, version))
			warning("WuRegistry::startup(): the stored version '%s' differs from '%s', the stored values are kept", storedVersion, version);

		saveStr("Version", version);
	}

	return loadIndex();
}

int WuRegistry::loadIndex() {
	_numValues = _values.size();
	_valueNames = new char *[_numValues];

	for (uint i = 0; i < _numValues; i++) {
		Common::String name = _values[i].name;

		name.toLowercase();
		_valueNames[i] = new char[name.size() + 1];
		memcpy(_valueNames[i], name.c_str(), name.size() + 1);
	}

	sort(_valueNames, _numValues, sizeof(char *), compareNames);

	return 0;
}

int WuRegistry::compareNames(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

void WuRegistry::shutdown() {
	freeIndex();
	_started = 0;

	// The remastered writes the registry only when the scripts ask for it
	if (!g_grim->isRemastered())
		ConfMan.flushToDisk();
}

void WuRegistry::freeIndex() {
	for (uint i = 0; i < _numValues; i++)
		delete[] _valueNames[i];

	delete[] _valueNames;
	_valueNames = nullptr;
	_numValues = 0;
}

int WuRegistry::saveInt(const char *valueName, int value) {
	Common::String key;
	const WuRegistryMapping *mapping = findMapping(valueName);

	if (mapping) {
		writeMappedInt(mapping, value);

		return 1;
	}

	if (claimKey(valueName, kValueBinary, &key)) {
		addIndex(valueName);
		ConfMan.setInt(key, value);

		return 1;
	}

	byte data[4];

	WRITE_LE_UINT32(data, (uint32)value);
	setValue(valueName, kValueBinary, data, 4);
	addIndex(valueName);

	return 1;
}

int WuRegistry::addIndex(const char *valueName) {
	int index = findIndex(valueName);

	if (!index) {
		Common::String name = valueName;

		name.toLowercase();

		char **newNames = new char *[_numValues + 1];

		for (uint i = 0; i < _numValues; i++)
			newNames[i] = _valueNames[i];

		delete[] _valueNames;
		_valueNames = newNames;
		_numValues++;

		_valueNames[_numValues - 1] = new char[name.size() + 1];
		memcpy(_valueNames[_numValues - 1], name.c_str(), name.size() + 1);

		sort(_valueNames, _numValues, sizeof(char *), compareNames);
	}

	return index;
}

int WuRegistry::findIndex(const char *valueName) {
	const WuRegistryMapping *mapping = findMapping(valueName);

	if (mapping)
		return getMappedType(mapping) ? 1 : 0;

	if (!_valueNames)
		return 1;

	Common::String name = valueName;

	name.toLowercase();

	const char *key = name.c_str();
	void *found = binarySearch(&key, _valueNames, _numValues, sizeof(char *), compareNames);

	if (found)
		return 1;

	return ConfMan.hasKey(valueName) ? 1 : 0;
}

int WuRegistry::saveFloat(const char *valueName, float value) {
	Common::String key;
	const WuRegistryMapping *mapping = findMapping(valueName);

	if (mapping && mapping->kind == kMapFloat) {
		ConfMan.set(mapping->key, Common::String::format("%.9g", value));
		addIndex(valueName);

		return 1;
	}

	if (mapping) {
		if (!(value > -2147483648.0f && value < 2147483648.0f)) {
			warning("WuRegistry::saveFloat(): the value of '%s' does not fit an integer", valueName);
			return 1;
		}

		writeMappedInt(mapping, (int)value);

		return 1;
	}

	if (claimKey(valueName, kValueBinary, &key)) {
		addIndex(valueName);
		ConfMan.set(key, Common::String::format("%.9g", value));

		return 1;
	}

	byte data[4];
	uint32 bits;

	memcpy(&bits, &value, sizeof(bits));
	WRITE_LE_UINT32(data, bits);
	setValue(valueName, kValueBinary, data, 4);
	addIndex(valueName);

	return 1;
}

int WuRegistry::getInt(const char *valueName, int defaultValue) {
	byte data[4] = { 0, 0, 0, 0 };
	uint size = 4;
	Common::String key;
	const WuRegistryMapping *mapping = findMapping(valueName);

	if (!findIndex(valueName))
		return defaultValue;

	if (mapping) {
		int mappedValue;

		if (readMappedInt(mapping, &mappedValue))
			return mappedValue;

		return defaultValue;
	}

	if (findConfKey(valueName, &key)) {
		if (!canParseInt(ConfMan.get(key))) {
			warning("WuRegistry::getInt(): the value of '%s' is not a number", valueName);
			return defaultValue;
		}

		return ConfMan.getInt(key);
	}

	if (queryValue(valueName, nullptr, data, &size)) {
		if (size < 4)
			warning("WuRegistry::getInt(): the value is shorter than 4 bytes");

		return (int)READ_LE_UINT32(data);
	}

	return defaultValue;
}

float WuRegistry::getFloat(const char *valueName, float defaultValue) {
	byte data[4] = { 0, 0, 0, 0 };
	uint size = 4;
	Common::String key;
	const WuRegistryMapping *mapping = findMapping(valueName);

	if (!findIndex(valueName))
		return defaultValue;

	if (mapping && mapping->kind == kMapFloat) {
		if (!canParseFloat(ConfMan.get(mapping->key))) {
			warning("WuRegistry::getFloat(): the value of '%s' is not a number", valueName);
			return defaultValue;
		}

		return ConfMan.getFloat(mapping->key);
	}

	if (mapping) {
		int mappedValue;

		if (readMappedInt(mapping, &mappedValue))
			return (float)mappedValue;

		return defaultValue;
	}

	if (findConfKey(valueName, &key)) {
		if (!canParseFloat(ConfMan.get(key))) {
			warning("WuRegistry::getFloat(): the value of '%s' is not a number", valueName);
			return defaultValue;
		}

		return ConfMan.getFloat(key);
	}

	if (queryValue(valueName, nullptr, data, &size)) {
		if (size < 4)
			warning("WuRegistry::getFloat(): the value is shorter than 4 bytes");

		uint32 bits = READ_LE_UINT32(data);
		float value;

		memcpy(&value, &bits, sizeof(value));

		return value;
	}

	return defaultValue;
}

int WuRegistry::saveBinary(const char *valueName, const byte *data, uint size) {
	// TODO: binary values are not persisted
	setValue(valueName, kValueBinary, data, size);
	addIndex(valueName);

	return 1;
}

int WuRegistry::getBinary(const char *valueName, byte *data, uint size) {
	if (findIndex(valueName) && queryValue(valueName, nullptr, data, &size))
		return 1;

	return 0;
}

int WuRegistry::saveStr(const char *valueName, const char *data) {
	Common::String key;
	const WuRegistryMapping *mapping = findMapping(valueName);

	if (mapping) {
		int mappedResult = addIndex(valueName);

		writeMappedText(mapping, data);

		return mappedResult;
	}

	bool storable = isStorableString(data);

	if (!storable)
		warning("WuRegistry::saveStr(): the string of '%s' cannot be stored in the configuration", valueName);

	if (storable && claimKey(valueName, kValueString, &key)) {
		int result = addIndex(valueName);

		ConfMan.set(key, data);

		return result;
	}

	setValue(valueName, kValueString, (const byte *)data, strlen(data));

	return addIndex(valueName);
}

int WuRegistry::getStr(const char *valueName, char *data, uint size, const char *defaultValue) {
	int result = findIndex(valueName);

	if (!result)
		return 0;

	if (!size) {
		warning("WuRegistry::getStr(): empty buffer");
		return 0;
	}

	Common::String key;
	uint type = kValueString;
	uint storedSize = size;
	bool queried;
	const WuRegistryMapping *mapping = findMapping(valueName);
	Common::String text;

	if (mapping)
		queried = readMappedText(mapping, &text) && queryText(text, (byte *)data, &storedSize);
	else if (findConfKey(valueName, &key))
		queried = queryText(ConfMan.get(key), (byte *)data, &storedSize);
	else
		queried = queryValue(valueName, &type, (byte *)data, &storedSize);

	if (!queried) {
		if (defaultValue) {
			strncpy(data, defaultValue, size - 1);
			data[size - 1] = 0;
		}

		return 0;
	}

	if (type == kValueString && storedSize >= size) {
		warning("WuRegistry::getStr(): the string has no room for its terminator");
		data[size - 1] = 0;
	}

	return 1;
}

uint WuRegistry::queryType(const char *valueName) {
	const WuRegistryMapping *mapping = findMapping(valueName);

	if (mapping)
		return getMappedType(mapping);

	int index = findValue(valueName);
	Common::String key;

	if (index >= 0 && !_values[index].inConfMan)
		return _values[index].type;

	if (!findConfKey(valueName, &key))
		return 0;

	if (index >= 0)
		return _values[index].type;

	return isNumberText(ConfMan.get(key)) ? kValueBinary : kValueString;
}

void WuRegistry::saveRegistry() {
	_unlocks.saveFile();
	ConfMan.flushToDisk();
}

// The folders of the saves have no way to tell when a file was written, so the time is kept here
// as the date of the C locale followed by hours and minutes
static Common::String getSlotTimeKey(int slot) {
	return Common::String::format("save_time_%d", slot);
}

void WuRegistry::saveSlotTime(int slot) {
	TimeDate now;

	g_system->getTimeAndDate(now);
	ConfMan.set(getSlotTimeKey(slot), Common::String::format("%02d/%02d/%02d %02d:%02d", now.tm_mon + 1, now.tm_mday,
			now.tm_year % 100, now.tm_hour, now.tm_min));
}

bool WuRegistry::getSlotTime(int slot, Common::String *time) {
	Common::String key = getSlotTimeKey(slot);

	if (!ConfMan.hasKey(key))
		return false;

	*time = ConfMan.get(key);

	return true;
}

void WuRegistry::removeSlotTime(int slot) {
	Common::String key = getSlotTimeKey(slot);

	if (ConfMan.hasKey(key))
		ConfMan.removeKey(key, ConfMan.getActiveDomainName());
}

void WuRegistry::unlockCutscene(int index) {
	_unlocks.unlockCutscene(index);
}

bool WuRegistry::isCutsceneUnlocked(int index) {
	return _unlocks.isCutsceneUnlocked(index);
}

void WuRegistry::unlockConcept(int index) {
	_unlocks.unlockConcept(index);
}

bool WuRegistry::isConceptUnlocked(int index) {
	return _unlocks.isConceptUnlocked(index);
}

void WuRegistry::resetUnlocks() {
	_unlocks.resetUnlocks();
}

bool WuRegistry::importRegistrySav(Common::SeekableReadStream *stream) {
	return _unlocks.importRegistrySav(stream);
}

int WuRegistry::findValue(const char *valueName) const {
	for (uint i = 0; i < _values.size(); i++) {
		if (_values[i].name.equalsIgnoreCase(valueName))
			return i;
	}

	return -1;
}

void WuRegistry::setValue(const char *valueName, uint type, const byte *data, uint size) {
	int index = findValue(valueName);

	if (index < 0) {
		WuRegistryValue value;

		value.name = valueName;
		value.type = type;
		value.inConfMan = false;
		_values.push_back(value);
		index = _values.size() - 1;
	}

	WuRegistryValue &value = _values[index];

	if (value.inConfMan) {
		Common::ConfigManager::Domain *domain = getConfDomain();

		if (domain)
			domain->erase(value.name);

		value.inConfMan = false;
	}

	value.type = type;
	value.data.resize(size);

	if (size)
		memcpy(value.data.data(), data, size);
}

bool WuRegistry::claimKey(const char *valueName, uint type, Common::String *key) {
	if (!isStorableName(valueName)) {
		warning("WuRegistry::claimKey(): the value name '%s' cannot be stored in the configuration", valueName);
		return false;
	}

	int index = findValue(valueName);

	if (index < 0) {
		WuRegistryValue value;

		value.name = valueName;
		value.type = type;
		value.inConfMan = true;
		_values.push_back(value);
		index = _values.size() - 1;
	}

	WuRegistryValue &value = _values[index];

	value.type = type;
	value.inConfMan = true;
	value.data.clear();

	*key = value.name;

	return true;
}

bool WuRegistry::findConfKey(const char *valueName, Common::String *key) const {
	int index = findValue(valueName);

	if (index >= 0) {
		if (!_values[index].inConfMan)
			return false;

		*key = _values[index].name;
	} else {
		*key = valueName;
	}

	return ConfMan.hasKey(*key);
}

bool WuRegistry::queryValue(const char *valueName, uint *type, byte *data, uint *size) const {
	int index = findValue(valueName);

	if (index < 0 || _values[index].inConfMan)
		return false;

	const WuRegistryValue &value = _values[index];
	uint storedSize = value.data.size();

	if (type)
		*type = value.type;

	if (!data)
		return true;

	if (storedSize > *size) {
		*size = storedSize;
		return false;
	}

	if (storedSize)
		memcpy(data, value.data.data(), storedSize);

	if (value.type == kValueString && storedSize < *size)
		data[storedSize] = 0;

	*size = storedSize;

	return true;
}

} // End of namespace Grim
