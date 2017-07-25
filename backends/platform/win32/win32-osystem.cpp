#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "win32-osystem.h"
#include "backends/events/default/default-events.h"
#include "backends/events/win32/win32-events.h"

OSystem_WIN32::OSystem_WIN32()
	: _events(NULL) {
	// note: must be created immediately so we can open the config
	_fsFactory = new WindowsFilesystemFactory();
}

OSystem_WIN32::~OSystem_WIN32() {
	if (_events) {
		delete _events;
		_events = NULL;
	}
}

void OSystem_WIN32::initBackend() {

	//XXX: create a WIN32MutexManager
	_mutexManager = new NullMutexManager();

	_timerManager = new DefaultTimerManager();

	Common::EventSource *events = getDefaultEventSource();
	_eventManager = new DefaultEventManager(events);

	_savefileManager = new DefaultSaveFileManager();

	_graphicsManager = new GDIGraphicsManager();

	//XXX: use winmm
	static const uint sample_rate = 22050;
	_mixer = new Audio::MixerImpl(this, sample_rate);
	((Audio::MixerImpl *)_mixer)->setReady(false);
	//XXX: Create thread to pump audio via winmm

	ModularBackend::initBackend();
}

uint32 OSystem_WIN32::getMillis(bool skipRecord) {
	return uint32(GetTickCount());
}

void OSystem_WIN32::delayMillis(uint msecs) { Sleep(msecs); }

void OSystem_WIN32::logMessage(LogMessageType::Type type, const char *message) {
	FILE *output = 0;
	if (type == LogMessageType::kInfo || type == LogMessageType::kDebug) {
		output = stdout;
	} else {
		output = stderr;
	}
	fputs(message, output);
	fflush(output);
}

Common::EventSource *OSystem_WIN32::getDefaultEventSource() {
    if (!_events) {
        _events = new Win32EventSource();
    }
    assert(_events);
    return _events;
}
