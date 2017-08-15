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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "backends/mutex/mutex.h"
#include "backends/mutex/win32/win32-mutex.h"
#include "common/hashmap.h"

struct Win32Mutex {
	HANDLE _mutex;
};

struct MutexHashFunc {
	uint operator()(const OSystem::MutexRef &val) const { return (uint)val; }
};

struct Win32MutexDetail {
	typedef Common::HashMap<OSystem::MutexRef,
							Win32Mutex *,
							MutexHashFunc> HashMap;
	HashMap _hashMap;

	OSystem::MutexRef createMutex() {
		// create mutex object
		Win32Mutex *mux = new Win32Mutex;
		assert(mux);
		HANDLE hMutex = CreateMutexA(NULL, FALSE, NULL);
		assert(hMutex);
		mux->_mutex = hMutex;
		// insert into hash map
		OSystem::MutexRef key = (OSystem::MutexRef)mux;
		_hashMap[key] = mux;
		return key;
	}

	void lockMutex(OSystem::MutexRef mutex) {
		HashMap::iterator itt = _hashMap.find(mutex);
		if (itt == _hashMap.end()) {
			return;
		}
		Win32Mutex *mux = itt->_value;
		if (!(mux && mux->_mutex)) {
			return;
		}
		assert(mux && mux->_mutex);
		switch (WaitForSingleObject(mux->_mutex, INFINITE)) {
		case WAIT_OBJECT_0:
			break;
		default:
			assert(!"failed to wait for mutex");
		}
	}

	void unlockMutex(OSystem::MutexRef mutex) {
		HashMap::iterator itt = _hashMap.find(mutex);
		if (itt != _hashMap.end()) {
			Win32Mutex *mux = itt->_value;
			if (mux && mux->_mutex) {
				ReleaseMutex(mux->_mutex);
			}
		}
	}

	void deleteMutex(OSystem::MutexRef mutex) {
		HashMap::iterator itt = _hashMap.find(mutex);
		if (itt == _hashMap.end()) {
			return;
		}
		Win32Mutex *mux = itt->_value;
		if (!(mux && mux->_mutex)) {
			return;
		}
		switch (WaitForSingleObject(mux->_mutex, INFINITE)) {
		case WAIT_OBJECT_0:
			_hashMap.erase(itt);
			CloseHandle(mux->_mutex);
			mux->_mutex = NULL;
			delete mux;
			break;
		default:
			assert(!"failed to wait for mutex");
		}
	}
};

Win32MutexManager::Win32MutexManager()
	: _detail(*new Win32MutexDetail) {}

Win32MutexManager::~Win32MutexManager() {
	// delete the implementation
	delete &_detail;
}

OSystem::MutexRef Win32MutexManager::createMutex() {
	return _detail.createMutex();
}

void Win32MutexManager::lockMutex(OSystem::MutexRef mutex) {
	_detail.lockMutex(mutex);
}

void Win32MutexManager::unlockMutex(OSystem::MutexRef mutex) {
	_detail.unlockMutex(mutex);
}

void Win32MutexManager::deleteMutex(OSystem::MutexRef mutex) {
	_detail.deleteMutex(mutex);
}
