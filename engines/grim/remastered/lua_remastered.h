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

class Lua_Remastered : public Lua_V1 {
public:
	typedef Lua_Remastered LuaClass;

	Lua_Remastered();

	void recognizeOpcodes() override;
	void registerMainOpcodes() override;
	void registerCostumeOpcodes() override;
	void registerPrimitiveOpcodes() override;
	void registerHardwareOpcodes() override;

	int controlCallback(float frameTime) override;

protected:
	DECLARE_LUA_OPCODE(Load) override;
	DECLARE_LUA_OPCODE(Save) override;
	DECLARE_LUA_OPCODE(Is3DHardwareEnabled) override;

	DECLARE_LUA_OPCODE(PrintStack);

	DECLARE_LUA_OPCODE(New);
	DECLARE_LUA_OPCODE(Erase);
	DECLARE_LUA_OPCODE(GetSaveStatus);
	DECLARE_LUA_OPCODE(FindSaveGames);
	DECLARE_LUA_OPCODE(InitiateFindSaveGames);
	DECLARE_LUA_OPCODE(GetFindSaveGameStatus);

	DECLARE_LUA_OPCODE(SetMouseSpeedScale);
	DECLARE_LUA_OPCODE(SetKeyMappingMode);
	DECLARE_LUA_OPCODE(ResetKeyMappingToDefault);
	DECLARE_LUA_OPCODE(GetRemappedKeyName);
	DECLARE_LUA_OPCODE(GetRemappedKeyHint);

	DECLARE_LUA_OPCODE(NoOpPutActorAtOrigin);
	DECLARE_LUA_OPCODE(NoOpGetClippedPos);
	DECLARE_LUA_OPCODE(NoOpDriveActorTo);
	DECLARE_LUA_OPCODE(NoOpWalkActorToInterest);
	DECLARE_LUA_OPCODE(NoOpUnLockFont);
	DECLARE_LUA_OPCODE(NoOpLockSound);
	DECLARE_LUA_OPCODE(NoOpUnlockSound);
	DECLARE_LUA_OPCODE(NoOpLockCostume);
	DECLARE_LUA_OPCODE(NoOpUnlockCostume);
	DECLARE_LUA_OPCODE(NoOpPlaySound);
	DECLARE_LUA_OPCODE(NoOpPlaySoundAt);
	DECLARE_LUA_OPCODE(NoOpPlaySoundAttached);
	DECLARE_LUA_OPCODE(NoOpIsSoundPlaying);
	DECLARE_LUA_OPCODE(NoOpGetSectorVertices);
	DECLARE_LUA_OPCODE(NoOpLoadBundle);
	DECLARE_LUA_OPCODE(NoOpUnloadBundle);

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
