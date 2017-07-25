#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "win32-events.h"

bool Win32EventSource::handleEvent(tagMSG &msg, Common::Event &event) {
	switch (msg.message) {
	case WM_QUIT:
		event.type = Common::EventType::EVENT_QUIT;
		return true;
	}
	return false;
}

bool Win32EventSource::pollEvent(Common::Event &event) {
	MSG msg;
	while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		// translate virtual key code
		TranslateMessage(&msg);

		if (handleEvent(msg, event)) {
			return true;
		} else {
			DispatchMessageA(&msg);
		}
	}
	return true;
}
