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
#include "common/config-manager.h"

win32OSystem::win32OSystem()
	: _events(NULL)
	, _mixerManager(NULL) {
	// note: must be created immediately so we can open the config
	_fsFactory = new WindowsFilesystemFactory();
}

win32OSystem::~win32OSystem() {
	if (_mixerManager) {
		_mixer->stopAll();
		_mixerManager->shutDown();
		delete _mixerManager;
		_mixerManager = NULL;
	}
	if (_events) {
		delete _events;
		_events = NULL;
	}
	// delete this here as it must be destroyed before the mutex manager
	if (_timerManager) {
		delete _timerManager;
		_timerManager = NULL;
	}
	if (_gdiGraphics) {
		delete _gdiGraphics;
		_gdiGraphics = NULL;
		// Note: this was done by the modular backend
		_graphicsManager = NULL;
	}
}

void win32OSystem::initBackend() {
	_mutexManager = new Win32MutexManager();
	_timerManager = new DefaultTimerManager();
	_savefileManager = new DefaultSaveFileManager();
	// init graphics
	{
		_gdiGraphics = new GDIGraphicsManager();
		_graphicsManager = _gdiGraphics;
	}
	// init events
	{
		_events = new Win32EventSource(_gdiGraphics);
		_eventManager = new DefaultEventManager(_events);
	}
	// init audio mixer
	{
		// determine the desired output sampling frequency.
		uint32 samplesPerSec = 0;
		if (ConfMan.hasKey("output_rate"))
			samplesPerSec = ConfMan.getInt("output_rate");
		if (samplesPerSec <= 0)
			samplesPerSec = 22050;
		// open the mixing device
		Audio::MixerImpl *mixImpl = new Audio::MixerImpl(g_system, samplesPerSec);
		_mixerManager = new Win32MixerManager;
		_mixerManager->init(mixImpl);
		_mixer = mixImpl;
	}
	ModularBackend::initBackend();
}

uint32 win32OSystem::getMillis(bool skipRecord) {
	return uint32(GetTickCount());
}

void win32OSystem::delayMillis(uint msecs) {
	const uint ticks = GetTickCount();
	for (;;) {
		/* break if delay time has elapsed */
		const uint interval = GetTickCount() - ticks;
		if (interval >= msecs) {
			break;
		}
		/* yield process time slice */
		Sleep(0);
	}
}

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
	assert(_events);
	return _events;
}

void win32OSystem::getTimeAndDate(::TimeDate &t) const {
	// TODO
}
