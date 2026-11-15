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
#include "common/file.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "engines/grim/emi/emiengine.h"
#include "engines/grim/emi/core/lconsoleservices.h"
#include "engines/grim/emi/core/lmemoryservices.h"

namespace Grim {

LConsoleServices::LConsoleServices(EmiEngine *engine) : _engine(engine) {
	if (!_engine->_consoleServices)
		_engine->registerConsole(this);
}

LConsoleServices::~LConsoleServices() {
	if (_engine->_consoleServices == this)
		_engine->_consoleServices = nullptr;
}

void *LConsoleServices::operator new(size_t size) {
	return LMemoryServices::getCurrent()->mallocImpl(size);
}

void LConsoleServices::operator delete(void *block) {
	LMemoryServices::getCurrent()->freeImpl(block);
}

int LConsoleServices::logMessage(int level, const char *format, va_list args) {
	Common::String message = Common::String::vformat(format, args);
	int length = message.size();

	if (length > kMaxMessageLength) {
		warning("LConsoleServices::logMessage(): message too long");
		message = Common::String(message.c_str(), kMaxMessageLength);
	}

	logToFile("%s", message.c_str());

	while (!message.empty() && message.lastChar() == '\n')
		message.deleteLastChar();

	if (level == kLevelWarning)
		warning("%s", message.c_str());
	else if (level == kLevelError)
		warning("ERROR: %s", message.c_str());
	else
		debug("%s", message.c_str());

	return length;
}

int LConsoleServices::logStatus(const char *format, ...) {
	va_list args;

	va_start(args, format);
	int length = logMessage(kLevelStatus, format, args);
	va_end(args);

	return length;
}

int LConsoleServices::logWarning(const char *format, ...) {
	va_list args;

	va_start(args, format);
	int length = logMessage(kLevelWarning, format, args);
	va_end(args);

	return length;
}

int LConsoleServices::logError(const char *format, ...) {
	va_list args;

	va_start(args, format);
	int length = logMessage(kLevelError, format, args);
	va_end(args);

	return length;
}

int LConsoleServices::logDebug(const char *format, ...) {
	va_list args;

	va_start(args, format);
	int length = logMessage(kLevelDebug, format, args);
	va_end(args);

	return length;
}

int LConsoleServices::logToFile(const char *format, ...) {
	va_list args;

	if (!_engine->_logToFileEnabled)
		return 0;

	Common::DumpFile file;
	if (file.open(Common::Path(_engine->_logToFileName))) {
		va_start(args, format);
		Common::String text = Common::String::vformat(format, args);
		va_end(args);

		file.writeString(text);
		file.close();
	}

	return 0;
}

} // End of namespace Grim
