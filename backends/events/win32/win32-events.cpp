#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "win32-events.h"

namespace {
Common::KeyCode keyMap[256];

#define MAP(FROM, TO)                                                          \
	{ keyMap[FROM] = TO; }

void keyMapInit() {
    using namespace Common;
    MAP(VK_ESCAPE, KEYCODE_ESCAPE);
    MAP(VK_SPACE, KEYCODE_SPACE);
}

Common::KeyCode keyMapLookup(uint vk) {
	assert(vk < 256);
	return keyMap[vk];
}

template <uint from, uint to>
uint getBits(const uint in) {
	return (in & ((1u << to) - 1u)) >> from;
}

bool on_WM_QUIT(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	out.type = EventType::EVENT_QUIT;
	return true;
}

bool on_WM_KEYDOWN(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	// The previous key state. The value is 1 if the key is down before the
	// message is sent, or it is zero if the key is up.
	const uint pState = getBits<30, 30>(msg.lParam);
	out.type = EventType::EVENT_KEYDOWN;
	const uint vKey = msg.wParam;
	out.kbd.keycode = keyMapLookup(vKey);
	return out.kbd.keycode != KEYCODE_INVALID;
}

bool on_WM_KEYUP(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	out.type = EventType::EVENT_KEYUP;
	const uint vKey = msg.wParam;
	out.kbd.keycode = keyMapLookup(vKey);
	return out.kbd.keycode != KEYCODE_INVALID;
}

bool on_WM_MOUSEMOVE(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	out.type = EventType::EVENT_MOUSEMOVE;
	out.mouse.x = short(msg.lParam & 0xffffu);
	out.mouse.y = short(msg.lParam >> 16);
	return true;
}

bool on_WM_LBUTTONDOWN(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	out.type = EventType::EVENT_LBUTTONDOWN;
	out.mouse.x = short(msg.lParam & 0xffffu);
	out.mouse.y = short(msg.lParam >> 16);
	return true;
}

bool on_WM_RBUTTONDOWN(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	out.type = EventType::EVENT_RBUTTONDOWN;
	out.mouse.x = short(msg.lParam & 0xffffu);
	out.mouse.y = short(msg.lParam >> 16);
	return true;
}
} // namespace

Win32EventSource::Win32EventSource() {
	// generate windows VK code to ScummVM key code table
	keyMapInit();
}

bool Win32EventSource::handleEvent(tagMSG &msg, Common::Event &event) {
	event.synthetic = false;
	using namespace Common;
	switch (msg.message) {
	case WM_QUIT:
		return on_WM_QUIT(msg, event);
	case WM_MOUSEMOVE:
		return on_WM_MOUSEMOVE(msg, event);
	case WM_KEYDOWN:
		return on_WM_KEYDOWN(msg, event);
	case WM_KEYUP:
		return on_WM_KEYUP(msg, event);
	case WM_LBUTTONDOWN:
		return on_WM_LBUTTONDOWN(msg, event);
	case WM_RBUTTONDOWN:
		return on_WM_RBUTTONDOWN(msg, event);
	default:
		event.type = EventType::EVENT_INVALID;
		return false;
	}
}

bool Win32EventSource::pollEvent(Common::Event &event) {
	MSG msg;
	while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		// translate virtual key code
		TranslateMessage(&msg);

		//         if (msg.hwnd==_window) {
		//         }

		if (handleEvent(msg, event)) {
			return true;
		} else {
			DispatchMessageA(&msg);
		}
	}
	// no event generated
	return false;
}
