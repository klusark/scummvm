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

#include "engines/grim/remastered/look/look_glsl.h"
#include "engines/grim/remastered/look/look_programs.h"

#include <string.h>

namespace Grim {

bool RemasteredLookGlsl::isIdentifierChar(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool RemasteredLookGlsl::findToken(const Common::String &text, const char *token, uint from, uint &position) {
	uint length = strlen(token);

	for (uint i = from; i + length <= text.size(); i++) {
		if (strncmp(text.c_str() + i, token, length) != 0)
			continue;

		if (i > 0 && isIdentifierChar(text[i - 1]))
			continue;

		if (i + length < text.size() && isIdentifierChar(text[i + length]))
			continue;

		position = i;

		return true;
	}

	return false;
}

void RemasteredLookGlsl::skipSpaces(const Common::String &text, uint &position) {
	while (position < text.size() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\n' || text[position] == '\r'))
		position++;
}

Common::String RemasteredLookGlsl::trim(const Common::String &text) {
	uint start = 0;
	uint end = text.size();

	while (start < end && (text[start] == ' ' || text[start] == '\t' || text[start] == '\n' || text[start] == '\r'))
		start++;

	while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n' || text[end - 1] == '\r'))
		end--;

	return text.substr(start, end - start);
}

bool RemasteredLookGlsl::findCallEnd(const Common::String &text, uint open, uint &close) {
	int depth = 0;

	for (uint i = open; i < text.size(); i++) {
		if (text[i] == '(') {
			depth++;
		} else if (text[i] == ')') {
			depth--;

			if (depth == 0) {
				close = i;

				return true;
			}
		}
	}

	return false;
}

void RemasteredLookGlsl::splitArguments(const Common::String &text, uint open, uint close, Common::Array<Common::String> &args) {
	int depth = 0;
	uint start = open + 1;

	args.clear();

	for (uint i = open + 1; i < close; i++) {
		if (text[i] == '(') {
			depth++;
		} else if (text[i] == ')') {
			depth--;
		} else if (text[i] == ',' && depth == 0) {
			args.push_back(trim(text.substr(start, i - start)));
			start = i + 1;
		}
	}

	args.push_back(trim(text.substr(start, close - start)));
}

bool RemasteredLookGlsl::replaceVersion(Common::String &text) {
	uint position = 0;

	if (!findToken(text, "version", 0, position) || position == 0 || text[position - 1] != '#') {
		warning("RemasteredLookGlsl::replaceVersion(): the shader has no version line");
		return false;
	}

	uint lineEnd = position;

	while (lineEnd < text.size() && text[lineEnd] != '\n')
		lineEnd++;

	text = Common::String("#version 120") + text.substr(lineEnd);

	return true;
}

bool RemasteredLookGlsl::replaceUniformBlock(Common::String &text) {
	uint position = 0;

	while (findToken(text, "layout", position, position)) {
		uint scan = position + 6;

		skipSpaces(text, scan);

		if (scan >= text.size() || text[scan] != '(') {
			position = scan;
			continue;
		}

		uint close = 0;

		if (!findCallEnd(text, scan, close)) {
			warning("RemasteredLookGlsl::replaceUniformBlock(): unbalanced layout qualifier");
			return false;
		}

		Common::String qualifier = text.substr(scan + 1, close - scan - 1);

		if (qualifier.find("std140") == Common::String::npos) {
			position = close;
			continue;
		}

		uint cursor = close + 1;

		skipSpaces(text, cursor);

		if (strncmp(text.c_str() + cursor, "uniform", 7) != 0) {
			warning("RemasteredLookGlsl::replaceUniformBlock(): the block has no uniform keyword");
			return false;
		}

		cursor += 7;
		skipSpaces(text, cursor);

		uint nameStart = cursor;

		while (cursor < text.size() && isIdentifierChar(text[cursor]))
			cursor++;

		Common::String blockName = text.substr(nameStart, cursor - nameStart);

		skipSpaces(text, cursor);

		if (cursor >= text.size() || text[cursor] != '{') {
			warning("RemasteredLookGlsl::replaceUniformBlock(): the block has no body");
			return false;
		}

		uint bodyStart = cursor + 1;
		uint bodyEnd = bodyStart;

		while (bodyEnd < text.size() && text[bodyEnd] != '}')
			bodyEnd++;

		if (bodyEnd >= text.size()) {
			warning("RemasteredLookGlsl::replaceUniformBlock(): the block body is not closed");
			return false;
		}

		Common::String body = text.substr(bodyStart, bodyEnd - bodyStart);

		cursor = bodyEnd + 1;
		skipSpaces(text, cursor);

		uint instanceStart = cursor;

		while (cursor < text.size() && isIdentifierChar(text[cursor]))
			cursor++;

		Common::String instance = text.substr(instanceStart, cursor - instanceStart);

		skipSpaces(text, cursor);

		if (instance.empty() || cursor >= text.size() || text[cursor] != ';') {
			warning("RemasteredLookGlsl::replaceUniformBlock(): the block has no instance name");
			return false;
		}

		Common::String structName = Common::String("Look") + blockName;
		Common::String replacement = Common::String("struct ") + structName + " {" + body + "};\nuniform " + structName + " " + instance + ";";

		text = text.substr(0, position) + replacement + text.substr(cursor + 1);
		position += replacement.size();
	}

	return true;
}

bool RemasteredLookGlsl::replaceArrayCopies(Common::String &text) {
	static const char *const marker = " = constantsV.";
	uint search = 0;

	while (search < text.size()) {
		uint found = text.find(marker, search);

		if (found == Common::String::npos)
			break;

		uint nameEnd = found;
		uint nameStart = found;

		while (nameStart > 0 && isIdentifierChar(text[nameStart - 1]))
			nameStart--;

		uint sourceStart = found + 3;
		uint sourceEnd = sourceStart;

		while (sourceEnd < text.size() && (isIdentifierChar(text[sourceEnd]) || text[sourceEnd] == '.'))
			sourceEnd++;

		if (nameStart == nameEnd || sourceEnd >= text.size() || text[sourceEnd] != ';') {
			search = found + 1;
			continue;
		}

		Common::String local = text.substr(nameStart, nameEnd - nameStart);
		Common::String source = text.substr(sourceStart, sourceEnd - sourceStart);
		Common::String declaration = Common::String("vec4 ") + local + "[";
		uint declarationStart = text.find(declaration.c_str());

		if (declarationStart == Common::String::npos || declarationStart >= nameStart) {
			search = found + 1;
			continue;
		}

		uint declarationEnd = declarationStart;

		while (declarationEnd < text.size() && text[declarationEnd] != ';')
			declarationEnd++;

		if (declarationEnd >= text.size()) {
			warning("RemasteredLookGlsl::replaceArrayCopies(): unterminated array declaration");
			return false;
		}

		text = text.substr(0, nameStart) + text.substr(sourceEnd + 1);
		text = text.substr(0, declarationStart) + text.substr(declarationEnd + 1);

		uint position = 0;

		while (findToken(text, local.c_str(), position, position)) {
			text = text.substr(0, position) + source + text.substr(position + local.size());
			position += source.size();
		}

		search = 0;
	}

	return true;
}

bool RemasteredLookGlsl::replaceTextureCalls(Common::String &text) {
	uint i = 0;

	while (i < text.size()) {
		if (!isIdentifierChar(text[i]) || (i > 0 && isIdentifierChar(text[i - 1]))) {
			i++;
			continue;
		}

		uint end = i;

		while (end < text.size() && isIdentifierChar(text[end]))
			end++;

		Common::String identifier = text.substr(i, end - i);

		if (identifier != "texture" && identifier != "textureLod" && identifier != "textureLodOffset") {
			i = end;
			continue;
		}

		uint open = end;

		skipSpaces(text, open);

		if (open >= text.size() || text[open] != '(') {
			i = end;
			continue;
		}

		if (identifier == "texture") {
			text = text.substr(0, i) + "texture2D" + text.substr(open);
			i += 9;
			continue;
		}

		uint close = 0;

		if (!findCallEnd(text, open, close)) {
			warning("RemasteredLookGlsl::replaceTextureCalls(): unbalanced call");
			return false;
		}

		Common::Array<Common::String> args;

		splitArguments(text, open, close, args);

		Common::String replacement;

		if (identifier == "textureLod") {
			if (args.size() != 3 || args[2] != "0.0") {
				warning("RemasteredLookGlsl::replaceTextureCalls(): unsupported textureLod call");
				return false;
			}

			replacement = "texture2D(" + args[0] + ", " + args[1] + ")";
		} else {
			if (args.size() != 4 || args[2] != "0.0" || strncmp(args[3].c_str(), "ivec2", 5) != 0) {
				warning("RemasteredLookGlsl::replaceTextureCalls(): unsupported textureLodOffset call");
				return false;
			}

			if (text.find("fxaaQualityRcpFrame_1") == Common::String::npos) {
				warning("RemasteredLookGlsl::replaceTextureCalls(): the texel size variable is missing");
				return false;
			}

			replacement = "texture2D(" + args[0] + ", (" + args[1] + ") + " + args[3].substr(1) + " * fxaaQualityRcpFrame_1)";
		}

		text = text.substr(0, i) + replacement + text.substr(close + 1);
	}

	return true;
}

bool RemasteredLookGlsl::replaceQualifiers(Common::String &text, bool vertex) {
	Common::String result;
	uint lineStart = 0;

	while (lineStart <= text.size()) {
		uint lineEnd = lineStart;

		while (lineEnd < text.size() && text[lineEnd] != '\n')
			lineEnd++;

		Common::String line = text.substr(lineStart, lineEnd - lineStart);
		Common::String trimmed = trim(line);

		if (strncmp(trimmed.c_str(), "layout(location", 15) == 0) {
			uint close = trimmed.find(')');

			if (close == Common::String::npos) {
				warning("RemasteredLookGlsl::replaceQualifiers(): bad location qualifier");
				return false;
			}

			trimmed = trim(trimmed.substr(close + 1));
		}

		if (strncmp(trimmed.c_str(), "in ", 3) == 0) {
			line = (vertex ? "attribute " : "varying ") + trimmed.substr(3);
		} else if (strncmp(trimmed.c_str(), "out ", 4) == 0) {
			line = (vertex ? "varying " : "") + trimmed.substr(4);
		} else if (trimmed != trim(text.substr(lineStart, lineEnd - lineStart))) {
			line = trimmed;
		}

		result += line;

		if (lineEnd < text.size())
			result += '\n';

		lineStart = lineEnd + 1;
	}

	text = result;

	return true;
}

bool RemasteredLookGlsl::replaceMain(Common::String &text) {
	uint position = 0;

	if (!findToken(text, "main", 0, position)) {
		warning("RemasteredLookGlsl::replaceMain(): the shader has no main function");
		return false;
	}

	text = text.substr(0, position) + "look_main" + text.substr(position + 4);

	return true;
}

bool RemasteredLookGlsl::translateVertex(const Common::String &source, Common::String &out) {
	out = source;

	if (!replaceVersion(out) || !replaceUniformBlock(out) || !replaceArrayCopies(out) || !replaceTextureCalls(out) ||
			!replaceQualifiers(out, true))
		return false;

	return true;
}

bool RemasteredLookGlsl::translatePixel(const Common::String &source, int outputKind, Common::String &out) {
	out = source;

	if (!replaceVersion(out) || !replaceUniformBlock(out) || !replaceTextureCalls(out) || !replaceQualifiers(out, false))
		return false;

	if (!replaceMain(out))
		return false;

	out += "\nvoid main() {\n\tlook_main();\n";

	switch (outputKind) {
	case RemasteredLookPrograms::kOutputFloat:
		out += "\tgl_FragColor = vec4(ps_result, 0.0, 0.0, 1.0);\n";
		break;
	case RemasteredLookPrograms::kOutputThree:
		out += "\tgl_FragData[0] = ps_result[0];\n";
		out += "\tgl_FragData[1] = ps_result[1];\n";
		out += "\tgl_FragData[2] = ps_result[2];\n";
		break;
	default:
		out += "\tgl_FragColor = ps_result;\n";
		break;
	}

	out += "}\n";

	return true;
}

} // End of namespace Grim
