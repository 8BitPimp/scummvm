/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef WIN32_OSYSTEM_H
#define WIN32_OSYSTEM_H

#if defined(USE_WIN32_DRIVER)

#include "backends/modular-backend.h"
#include "base/main.h"

#include "backends/fs/windows/windows-fs-factory.h"
#include "backends/graphics/gdi/gdi-graphics.h"
#include "backends/mixer/win32/win32-mixer.h"
#include "backends/mutex/win32/win32-mutex.h"
#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"

#include "common/scummsys.h"

class win32OSystem : public ModularBackend {
public:
	win32OSystem();
	virtual ~win32OSystem();

	virtual void initBackend();

	virtual Common::EventSource *getDefaultEventSource();

	virtual uint32 getMillis(bool skipRecord = false);
	virtual void delayMillis(uint msecs);
	virtual void getTimeAndDate(::TimeDate &t) const;

	virtual void logMessage(LogMessageType::Type type, const char *message);

protected:
	Win32MixerManager *_mixerManager;
	Common::EventSource *_events;

	class GDIGraphicsManager *_gdiGraphics;
};

#endif // USE_WIN32_DRIVER
#endif // WIN32_OSYSTEM_H
