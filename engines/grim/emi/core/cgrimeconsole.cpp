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

#include "common/debug.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/emi/core/cgrimeconsole.h"
#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/files/lfile.h"

namespace Grim {

CGrimeConsole::CGrimeConsole(EmiEngine *engine) : LConsoleServices(engine), _debugLogEnabled(false), _debugLogFile(nullptr), _debugConsoleReady(false) {
}

int CGrimeConsole::logPrintfStub(const char *text) {
	Common::String message(text + 2);

	while (!message.empty() && message.lastChar() == '\n')
		message.deleteLastChar();

	if (text[0] == 'E')
		warning("ERROR: %s", message.c_str());
	else if (text[0] == 'W')
		warning("%s", message.c_str());
	else
		debug("%s", message.c_str());

	return 0;
}

int CGrimeConsole::logStatus(const char *format, ...) {
	va_list args;

	va_start(args, format);
	Common::String message = Common::String::vformat(format, args);
	va_end(args);

	int length = message.size();
	if (length > 1021) {
		warning("CGrimeConsole::logStatus(): message too long");
		message = Common::String(message.c_str(), 1021);
	}

	Common::String output = "S:" + message;
	logPrintfStub(output.c_str());
	writeDebugLog(output.c_str());

	return length;
}

int CGrimeConsole::logDebug(const char *format, ...) {
	va_list args;

	va_start(args, format);
	Common::String message = Common::String::vformat(format, args);
	va_end(args);

	int length = message.size();
	if (length > 1021) {
		warning("CGrimeConsole::logDebug(): message too long");
		message = Common::String(message.c_str(), 1021);
	}

	Common::String output = "D:" + message;
	logPrintfStub(output.c_str());
	writeDebugLog(output.c_str());

	return length;
}

int CGrimeConsole::logWarning(const char *format, ...) {
	va_list args;

	va_start(args, format);
	Common::String message = Common::String::vformat(format, args);
	va_end(args);

	int length = message.size();
	if (length > 1021) {
		warning("CGrimeConsole::logWarning(): message too long");
		message = Common::String(message.c_str(), 1021);
	}

	Common::String output = "W:" + message;
	logPrintfStub(output.c_str());
	writeDebugLog(output.c_str());

	return length;
}

int CGrimeConsole::logError(const char *format, ...) {
	va_list args;

	va_start(args, format);
	Common::String message = Common::String::vformat(format, args);
	va_end(args);

	int length = message.size();
	if (length > 1021) {
		warning("CGrimeConsole::logError(): message too long");
		message = Common::String(message.c_str(), 1021);
	}

	Common::String output = "E:" + message;
	logPrintfStub(output.c_str());
	writeDebugLog(output.c_str());

	return length;
}

void CGrimeConsole::writeDebugLog(const char *text) {
	if (_debugLogEnabled && _debugLogFile)
		_debugLogFile->writeFormat("%s", text);
}

void CGrimeConsole::setDebugLog(bool enable) {
	if (enable) {
		if (_debugConsoleReady)
			return;

		if (_debugLogEnabled) {
			_debugLogFile = _engine->getFileServices()->open("debug.log", "w");

			if (!_debugLogFile)
				return;
		}

		initDebugConsole();
		_debugConsoleReady = true;
	} else if (_debugConsoleReady && _debugLogFile) {
		_engine->getFileServices()->close(_debugLogFile, true);
		_debugLogFile = nullptr;
	}
}

bool CGrimeConsole::initDebugConsole() {
	_engine->registerConsole(this);

	return true;
}

} // End of namespace Grim
