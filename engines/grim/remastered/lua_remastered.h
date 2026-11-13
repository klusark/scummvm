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

#ifndef GRIM_LUA_REMASTERED
#define GRIM_LUA_REMASTERED

#include "common/str-array.h"

#include "engines/grim/lua_v1_opcodes/lua_v1.h"
#include "engines/grim/remastered/savestore.h"

namespace Grim {

/**
 * The Lua layer of the remastered: the main table with its own order, plus the commentary,
 * overlay and mouse tables. The opcodes that only read or write engine state are Lua_V1
 * members (lua_v1_remastered.cpp). The ones whose module does not exist yet are stubs here:
 * each one reports once that it is not implemented and answers with a neutral value, and
 * is meant to be replaced by a call into its module.
 */
class Lua_Remastered : public Lua_V1 {
public:
	typedef Lua_Remastered LuaClass;

	Lua_Remastered();

	void registerMainOpcodes() override;
	void registerCostumeOpcodes() override;
	void registerPrimitiveOpcodes() override;
	void registerHardwareOpcodes() override;

protected:
	// The remastered calls these with other arguments than the original (slot numbers)
	DECLARE_LUA_OPCODE(Load) override;
	DECLARE_LUA_OPCODE(Save) override;

	// Debugging
	DECLARE_LUA_OPCODE(PrintStack);

	// Save module, through the requests of the engine
	DECLARE_LUA_OPCODE(New);
	DECLARE_LUA_OPCODE(Erase);
	DECLARE_LUA_OPCODE(GetSaveStatus);
	DECLARE_LUA_OPCODE(FindSaveGames);
	DECLARE_LUA_OPCODE(InitiateFindSaveGames);
	DECLARE_LUA_OPCODE(GetFindSaveGameStatus);

	// Input module
	DECLARE_LUA_OPCODE(SetMouseSpeedScale);
	DECLARE_LUA_OPCODE(SetKeyMappingMode);
	DECLARE_LUA_OPCODE(ResetKeyMappingToDefault);
	DECLARE_LUA_OPCODE(GetRemappedKeyName);
	DECLARE_LUA_OPCODE(GetRemappedKeyHint);
	DECLARE_LUA_OPCODE(LoadRemappedKeys);
	DECLARE_LUA_OPCODE(SaveRemappedKeys);

	// Overlay module
	DECLARE_LUA_OPCODE(OverlayCreate);
	DECLARE_LUA_OPCODE(OverlayMove);
	DECLARE_LUA_OPCODE(OverlaySetLayer);
	DECLARE_LUA_OPCODE(OverlayDestroy);
	DECLARE_LUA_OPCODE(OverlayDimensions);
	DECLARE_LUA_OPCODE(OverlayClearCache);
	DECLARE_LUA_OPCODE(OverlayFade);
	DECLARE_LUA_OPCODE(OverlayGetScreenSize);
	DECLARE_LUA_OPCODE(OverlayScreenBoxingMode);
	DECLARE_LUA_OPCODE(OverlayShowLoadSpinner);
	DECLARE_LUA_OPCODE(WidescreenCorrectionFactor);

	// Commentary module
	DECLARE_LUA_OPCODE(SetCommentary);
	DECLARE_LUA_OPCODE(ClearCommentary);
	DECLARE_LUA_OPCODE(PlayCurrentCommentary);
	DECLARE_LUA_OPCODE(EnableCommentary);
	DECLARE_LUA_OPCODE(StopCommentaryImmediately);
	DECLARE_LUA_OPCODE(HasHeardCommentary);
	DECLARE_LUA_OPCODE(IsPlayingCommentary);

private:
	void reportStub(const char *opcode, const char *module);

	Common::StringArray _reportedStubs;
	Common::Array<SaveStore::SlotInfo> _foundSaves;
	int _findSaveGamesStatus = 0;
};

} // end of namespace Grim

#endif
