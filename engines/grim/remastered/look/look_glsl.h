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

#ifndef GRIM_REMASTERED_LOOK_LOOK_GLSL_H
#define GRIM_REMASTERED_LOOK_LOOK_GLSL_H

#include "common/array.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/remastered/look/look_types.h"

namespace Grim {

class RemasteredLookGlsl {
public:
	static bool translateVertex(const Common::String &source, Common::String &out);
	static bool translatePixel(const Common::String &source, int outputKind, Common::String &out);

private:
	static bool isIdentifierChar(char c);
	static bool findToken(const Common::String &text, const char *token, uint from, uint &position);
	static void skipSpaces(const Common::String &text, uint &position);
	static bool findCallEnd(const Common::String &text, uint open, uint &close);
	static void splitArguments(const Common::String &text, uint open, uint close, Common::Array<Common::String> &args);
	static Common::String trim(const Common::String &text);
	static bool replaceTextureCalls(Common::String &text);
	static bool replaceUniformBlock(Common::String &text);
	static bool replaceArrayCopies(Common::String &text);
	static bool replaceQualifiers(Common::String &text, bool vertex);
	static bool replaceVersion(Common::String &text);
	static bool replaceMain(Common::String &text);
	static bool replaceResultDeclaration(Common::String &text);
};

} // End of namespace Grim

#endif
