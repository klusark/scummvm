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

#ifndef GRIM_EMI_LCONSOLESERVICES_H
#define GRIM_EMI_LCONSOLESERVICES_H

#include "common/scummsys.h"

namespace Grim {

class EmiEngine;

class LConsoleServices {
public:
	explicit LConsoleServices(EmiEngine *engine);
	virtual ~LConsoleServices();

	void *operator new(size_t size);
	void operator delete(void *block);

	virtual int logStatus(const char *format, ...);
	virtual int logWarning(const char *format, ...);
	virtual int logError(const char *format, ...);
	virtual int logDebug(const char *format, ...);
	virtual int logToFile(const char *format, ...);

protected:
	enum {
		kLevelStatus,
		kLevelWarning,
		kLevelError,
		kLevelDebug
	};

	static const int kMaxMessageLength = 1021;

	int logMessage(int level, const char *format, va_list args);

	EmiEngine *_engine;
};

} // End of namespace Grim

#endif
