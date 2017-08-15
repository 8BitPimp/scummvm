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

#ifndef BACKEND_EVENTS_WIN32_H
#define BACKEND_EVENTS_WIN32_H

#include "common/events.h"
#include "common/queue.h"
#include "common/system.h"

class Win32EventSource : public Common::EventSource {
public:
	Win32EventSource(class GDIGraphicsManager *window);
	virtual ~Win32EventSource(){};
	virtual bool pollEvent(Common::Event &event);
protected:
	class GDIGraphicsManager *_window;
	bool handleEvent(struct tagMSG &msg, Common::Event &event);
};

#endif // BACKEND_EVENTS_WIN32_H
