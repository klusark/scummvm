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

#ifndef GRIM_DETECTION_H
#define GRIM_DETECTION_H

#include "engines/advancedDetector.h"
#include "engines/obsolete.h"

static const Engines::ObsoleteGameID obsoleteGameIDsTable[] = {
	{"grimdemo", "grim", Common::kPlatformWindows},
	{nullptr, nullptr, Common::kPlatformUnknown}
};

namespace Grim {

enum GrimGameType {
	GType_GRIM,
	GType_MONKEY4
};

struct GrimGameDescription {
	AD_GAME_DESCRIPTION_HELPERS(desc);

	ADGameDescription desc;
	GrimGameType gameType;
};

#define GAMEOPTION_SHOW_FPS GUIO_GAMEOPTIONS2
#define GAMEOPTION_3D_ACCELERATION GUIO_GAMEOPTIONS3
#define GAMEOPTION_REMASTER_BUGFIXES GUIO_GAMEOPTIONS4
#define GAMEOPTION_DEVELOPER_MODE GUIO_GAMEOPTIONS5

#define GUI_OPTIONS_GRIME GUIO4(GAMEOPTION_SHOW_FPS, GAMEOPTION_3D_ACCELERATION, GAMEOPTION_REMASTER_BUGFIXES, GAMEOPTION_DEVELOPER_MODE)

#define GUI_OPTIONS_GRIME_DEMO GUIO2(GAMEOPTION_SHOW_FPS, GAMEOPTION_DEVELOPER_MODE)
#define GUI_OPTIONS_GRIME_REMASTERED GUIO2(GAMEOPTION_SHOW_FPS, GAMEOPTION_DEVELOPER_MODE)
#define GAMEOPTION_EMI_MOVIE_SUBTITLES GUIO_GAMEOPTIONS8
#define GAMEOPTION_EMI_PS2_STRETCH_MOVIES GUIO_GAMEOPTIONS9

#define GUI_OPTIONS_EMI GUIO1(GAMEOPTION_EMI_MOVIE_SUBTITLES)
#define GAMEOPTION_EMI_PS2_TANK_CONTROLS GUIO_GAMEOPTIONS10

#define GUI_OPTIONS_EMI_PS2 GUIO3(GAMEOPTION_EMI_PS2_STRETCH_MOVIES, GAMEOPTION_EMI_PS2_TANK_CONTROLS, GAMEOPTION_DEVELOPER_MODE)
#define GUI_OPTIONS_EMI_DEMO GUIO0()

} // End of namespace Grim

#endif // GRIM_DETECTION_H
