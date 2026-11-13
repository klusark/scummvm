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

#include "common/memstream.h"
#include "common/serializer.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/actor/sputactor.h"
#include "engines/grim/chore/cos.h"
#include "engines/grim/costume/sputcostume.h"
#include "engines/grim/grim.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/lua/lua.h"
#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/movie/sputsmush.h"
#include "engines/grim/remastered/dialoglog.h"
#include "engines/grim/remastered/hdcanvas.h"
#include "engines/grim/remastered/overlay.h"
#include "engines/grim/remastered/savestore.h"
#include "engines/grim/remastered/sputmouse.h"
#include "engines/grim/render/sputprimitive.h"
#include "engines/grim/render/sputrender.h"
#include "engines/grim/res.h"
#include "engines/grim/sputresource.h"
#include "engines/grim/sputroom.h"
#include "engines/grim/text/polyglot.h"
#include "engines/grim/text/sputtext.h"
#include "engines/grim/wuregistry.h"

namespace Grim {

enum {
	// The version of the format of the iMUSE data inside the Serializer
	kRemasteredSoundsSaveVersion = 30
};

static const char *const newGameSet = "mo.set";

int GrimEngine::saveWriteCallback(void *buffer, int size) {
	return g_grim->zwrite(buffer, size);
}

int GrimEngine::saveReadCallback(void *buffer, int size) {
	return g_grim->zread(buffer, size);
}

void GrimEngine::requestRemasteredSave(int slot, const char *mural, int setIndex, const char *title) {
	_remasteredSaveSlot = slot;
	_remasteredSaveSetIndex = setIndex;
	_remasteredSaveMural = mural;
	_remasteredSaveTitle = _polyglotV1->getTranslation(title);
	_remasteredSaveStatus = SaveStore::kStatusWaiting;
	_libSaveRequest = 1;
}

void GrimEngine::requestRemasteredLoad(int slot, bool loadLast) {
	_remasteredLoadSlot = slot;
	_remasteredLoadLast = loadLast;
	_remasteredSaveStatus = SaveStore::kStatusWaiting;
	_libLoadRequest = 1;
}

void GrimEngine::requestRemasteredErase(int slot) {
	_remasteredEraseSlot = slot;
	_remasteredSaveStatus = SaveStore::kStatusWaiting;
	_remasteredEraseRequest = 1;
}

void GrimEngine::requestRemasteredNewGame() {
	_remasteredNewGameRequest = 1;
}

// The blocks go one after the other, each one between a tag of its own: the one of the Lua
// callback, the costumes, the resources, the texts, the rooms, the commentary, the actors, the
// render state, the primitives, the movie, the overlays, the dialog log, the hotspots, the Lua
// state and the iMUSE state
void GrimEngine::saveRemastered() {
	Common::MemoryWriteStreamDynamic buffer(DisposeAfterUse::YES);

	_libSaveRequest = 0;
	_saveWriteStream = &buffer;

	buffer.writeUint32LE(SaveStore::kVersion);

	SaveStore::writeTag(&buffer, "LCBS");
	_luaV1->saveGameCallback();
	SaveStore::writeTag(&buffer, "LCBE");

	SaveStore::writeTag(&buffer, "COSS");
	_sputCostume->getCos()->save(&buffer);
	SaveStore::writeTag(&buffer, "COSE");

	SaveStore::writeTag(&buffer, "RESS");
	_res->sysSaveRemastered(&buffer);
	SaveStore::writeTag(&buffer, "RESE");

	SaveStore::writeTag(&buffer, "TEXS");
	_sputText->saveRemastered(&buffer);
	SaveStore::writeTag(&buffer, "TEXE");

	SaveStore::writeTag(&buffer, "ROMS");
	_sputRoom->saveRemastered(&buffer);
	SaveStore::writeTag(&buffer, "ROME");

	SaveStore::writeTag(&buffer, "COMS");
	SaveStore::writeCommentary(&buffer);
	SaveStore::writeTag(&buffer, "COME");

	SaveStore::writeTag(&buffer, "ACTS");
	_sputActor->saveRemastered(&buffer);
	SaveStore::writeTag(&buffer, "ACTE");

	SaveStore::writeTag(&buffer, "RNDS");
	_sputRender->save(&buffer);
	SaveStore::writeTag(&buffer, "RNDE");

	SaveStore::writeTag(&buffer, "PRMS");
	_sputPrimitive->saveRemastered(&buffer);
	SaveStore::writeTag(&buffer, "PRME");

	SaveStore::writeTag(&buffer, "SMHS");
	_sputSmush->saveRemastered(&buffer);
	SaveStore::writeTag(&buffer, "SMHE");

	SaveStore::writeTag(&buffer, "OVRS");
	_hdCanvas->getOverlays()->save(&buffer);
	SaveStore::writeTag(&buffer, "OVRE");

	SaveStore::writeTag(&buffer, "DLGS");
	_dialogLog->save(&buffer);
	SaveStore::writeTag(&buffer, "DLGE");

	SaveStore::writeTag(&buffer, "HOTS");
	_sputMouse->save(&buffer);
	SaveStore::writeTag(&buffer, "HOTE");

	buffer.writeUint32LE((uint32)_sputPrevMode);

	SaveStore::writeTag(&buffer, "LUAS");
	lua_SaveRemastered(saveWriteCallback);
	SaveStore::writeTag(&buffer, "LUAE");

	Common::Serializer ser(nullptr, &buffer);

	ser.setVersion(kRemasteredSoundsSaveVersion);
	SaveStore::writeTag(&buffer, "IMUS");
	_imuse->soundsSaveLoadRemastered(ser);
	SaveStore::writeTag(&buffer, "IMUE");

	_saveWriteStream = nullptr;

	if (buffer.err() || buffer.size() >= SaveStore::kMaxUncompressedSize) {
		warning("GrimEngine::saveRemastered(): the savegame is too big or could not be written");
		_remasteredSaveStatus = SaveStore::kStatusUnknownError;

		return;
	}

	Common::Array<byte> compressed;

	if (!SaveStore::compress(buffer.getData(), (uint32)buffer.size(), compressed)) {
		_remasteredSaveStatus = SaveStore::kStatusUnknownError;

		return;
	}

	if (compressed.size() >= SaveStore::kMaxCompressedSize)
		warning("GrimEngine::saveRemastered(): the compressed savegame is bigger than the original game can read");

	int status = SaveStore::writeSlot(_remasteredSaveSlot, compressed, _remasteredSaveSetIndex, _remasteredSaveMural.c_str(),
			_remasteredSaveTitle.c_str());

	if (status == SaveStore::kStatusOk) {
		_wuRegistry->saveInt("LastSavedGame", _remasteredSaveSlot);
		_wuRegistry->saveRegistry();
	}

	_remasteredSaveStatus = status;
}

void GrimEngine::restoreRemastered() {
	_libLoadRequest = 0;

	Common::Array<byte> compressed;
	int status = SaveStore::readSlot(_remasteredLoadSlot, compressed);

	if (status != SaveStore::kStatusOk) {
		// The slot of the last game is not there any more: a new game starts instead
		if (_remasteredLoadLast) {
			_luaV1->bootSecondStage(newGameSet);

			return;
		}

		_remasteredSaveStatus = status;

		return;
	}

	Common::Array<byte> data;

	if (!SaveStore::uncompress(&compressed[0], (uint32)compressed.size(), data)) {
		warning("GrimEngine::restoreRemastered(): the slot %d is not a valid savegame", _remasteredLoadSlot);
		_remasteredSaveStatus = SaveStore::kStatusCorrupt;

		return;
	}

	_sputRender->allowLoadImage(0);
	_imuse->imStopAllSounds();
	_sputSmush->stopMovie();
	_sputSmush->periodic();
	_res->freeAll();
	_sputActor->clearShadowMaterial();
	_sputResource->stopPurge(1);

	_saveReadStream = new Common::MemoryReadStream(&data[0], (uint32)data.size(), DisposeAfterUse::NO);

	Common::SeekableReadStream *stream = _saveReadStream;
	bool valid = true;

	// The version is not checked: the saves of the remastered are all of the current one
	stream->readUint32LE();

	valid = SaveStore::readTag(stream, "LCBS") && valid;

	uint32 blockSize = stream->readUint32LE();

	stream->seek((int)blockSize, SEEK_CUR);
	valid = SaveStore::readTag(stream, "LCBE") && valid;

	valid = SaveStore::readTag(stream, "COSS") && valid;
	_sputCostume->getCos()->restore(stream);
	valid = SaveStore::readTag(stream, "COSE") && valid;

	valid = SaveStore::readTag(stream, "RESS") && valid;
	valid = _res->sysLoadRemastered(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "RESE") && valid;

	valid = SaveStore::readTag(stream, "TEXS") && valid;
	valid = _sputText->restoreRemastered(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "TEXE") && valid;

	valid = SaveStore::readTag(stream, "ROMS") && valid;
	valid = _sputRoom->restoreRemastered(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "ROME") && valid;

	valid = SaveStore::readTag(stream, "COMS") && valid;
	valid = SaveStore::readCommentary(stream) && valid;
	valid = SaveStore::readTag(stream, "COME") && valid;

	valid = SaveStore::readTag(stream, "ACTS") && valid;
	valid = _sputActor->restoreRemastered(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "ACTE") && valid;

	valid = SaveStore::readTag(stream, "RNDS") && valid;
	valid = _sputRender->restore(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "RNDE") && valid;

	valid = SaveStore::readTag(stream, "PRMS") && valid;
	valid = _sputPrimitive->restoreRemastered(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "PRME") && valid;

	valid = SaveStore::readTag(stream, "SMHS") && valid;
	valid = _sputSmush->restoreRemastered(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "SMHE") && valid;

	valid = SaveStore::readTag(stream, "OVRS") && valid;
	valid = _hdCanvas->getOverlays()->restore(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "OVRE") && valid;

	valid = SaveStore::readTag(stream, "DLGS") && valid;
	valid = _dialogLog->restore(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "DLGE") && valid;

	valid = SaveStore::readTag(stream, "HOTS") && valid;
	valid = _sputMouse->restore(stream) == 0 && valid;
	valid = SaveStore::readTag(stream, "HOTE") && valid;

	_sputPrevMode = (int)stream->readUint32LE();

	valid = SaveStore::readTag(stream, "LUAS") && valid;
	lua_RestoreRemastered(saveReadCallback);
	valid = SaveStore::readTag(stream, "LUAE") && valid;

	Common::Serializer ser(stream, nullptr);

	ser.setVersion(kRemasteredSoundsSaveVersion);
	valid = SaveStore::readTag(stream, "IMUS") && valid;
	_imuse->soundsSaveLoadRemastered(ser);
	valid = SaveStore::readTag(stream, "IMUE") && valid;

	_libMode = kLibModeNormal;

	_sputResource->stopPurge(0);

	Room *room = _sputRoom->getCurrentRoom();
	int curSetup = 0;

	if (room)
		curSetup = room->curSetup;

	_sputRender->useSetup(room, curSetup);

	delete _saveReadStream;
	_saveReadStream = nullptr;

	_libJustLoaded = 1;
	_sputRender->allowLoadImage(1);

	StdRect rect = { 0, 0, 640, 480 };

	_sputRender->markAllPagesDirty(&rect, 1);

	_remasteredSaveStatus = valid ? SaveStore::kStatusOk : SaveStore::kStatusCorrupt;
}

// The files are emptied, which takes the slot out of the list
void GrimEngine::eraseRemastered() {
	_remasteredEraseRequest = 0;
	_remasteredSaveStatus = SaveStore::eraseSlot(_remasteredEraseSlot);
}

// A new game: the sounds, the movie, the resources and the actors of the game that is over go
// away, the scripts are read again and the boot function runs from the start. The overlays, the
// texts and the hotspots stay, because the loading screen that the scripts show is made of them.
void GrimEngine::newGameRemastered() {
	_remasteredNewGameRequest = 0;

	_sputRender->allowLoadImage(0);
	_imuse->imStopAllSounds();
	_sputSmush->stopMovie();
	_sputSmush->periodic();
	_res->freeAll();
	_sputActor->clearShadowMaterial();
	_sputResource->stopPurge(1);
	lua_collectgarbage(0);

	_sputActor->reset();
	_sputRender->open();

	_sputResource->stopPurge(0);
	_sputRender->allowLoadImage(1);

	_luaV1->findSystemFiles();
	_luaV1->boot(0, nullptr);
}

} // End of namespace Grim
