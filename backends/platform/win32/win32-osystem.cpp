#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_fputs

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "win32-osystem.h"
#include "audio/mixer_intern.h"
#include "backends/events/default/default-events.h"
#include "backends/events/win32/win32-events.h"

win32OSystem::win32OSystem()
	: _events(NULL)
    , _mixerManager(NULL)
{
	// note: must be created immediately so we can open the config
    _fsFactory = new WindowsFilesystemFactory();
}

win32OSystem::~win32OSystem() {
	if (_events) {
		delete _events;
		_events = NULL;
	}
}

void win32OSystem::initBackend() {

	_mutexManager = new Win32MutexManager();

	_timerManager = new DefaultTimerManager();

	Common::EventSource *events = getDefaultEventSource();
    _eventManager = new DefaultEventManager(events);

	_savefileManager = new DefaultSaveFileManager();

	_graphicsManager = new GDIGraphicsManager();

    _mixerManager = new Win32MixerManager;
    _mixerManager->init();
	_mixer = _mixerManager->getMixer();

	ModularBackend::initBackend();
}

uint32 win32OSystem::getMillis(bool skipRecord) {
	return uint32(GetTickCount());
}

void win32OSystem::delayMillis(uint msecs) { Sleep(msecs); }

void win32OSystem::logMessage(LogMessageType::Type type, const char *message) {
	FILE *output = 0;
	if (type == LogMessageType::kInfo || type == LogMessageType::kDebug) {
		output = stdout;
	} else {
		output = stderr;
	}
	fputs(message, output);
	fflush(output);
}

Common::EventSource *win32OSystem::getDefaultEventSource() {
    if (!_events) {
        _events = new Win32EventSource();
    }
    assert(_events);
    return _events;
}

void win32OSystem::getTimeAndDate(::TimeDate &t) const {
    // TODO
}
