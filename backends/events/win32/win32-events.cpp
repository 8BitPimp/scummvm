#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "win32-events.h"
#include "backends/graphics/gdi/gdi-graphics.h"
#include "backends/timer/default/default-timer.h"

namespace {
struct keyInfo {
	Common::KeyCode keyCode;
	uint16 ascii;
};
keyInfo keyMap[256];

#define MAP(VK, KEYCODE, ASCII)                                                \
	{                                                                          \
		keyMap[(VK)].keyCode = (KEYCODE);                                      \
		keyMap[(VK)].ascii = (ASCII);                                          \
	}

void keyMapInit() {
	using namespace Common;
	MAP(VK_ESCAPE,  KEYCODE_ESCAPE,    ASCII_ESCAPE);
	MAP(VK_SPACE,   KEYCODE_SPACE,     ASCII_SPACE);
	MAP(VK_RETURN,  KEYCODE_RETURN,    ASCII_RETURN);
	MAP(VK_UP,      KEYCODE_UP,        0);
	MAP(VK_DOWN,    KEYCODE_DOWN,      0);
	MAP(VK_LEFT,    KEYCODE_LEFT,      0);
	MAP(VK_RIGHT,   KEYCODE_RIGHT,     0);
	MAP(VK_TAB,     KEYCODE_TAB,       ASCII_TAB);
	MAP(VK_BACK,    KEYCODE_BACKSPACE, ASCII_BACKSPACE);
	for (int i = 0; i < 26; ++i) {
		MAP('A' + i, Common::KeyCode(KEYCODE_a + i), 'a' + i);
	}
	for (int i = 0; i < 10; ++i) {
		MAP('0' + i, Common::KeyCode(KEYCODE_0 + i), '0' + i);
	}
	for (int i = 0; i < 12; ++i) {
		MAP(VK_F1 + i, Common::KeyCode(KEYCODE_F1 + i), ASCII_F1 + i);
	}
}

const keyInfo &keyMapLookup(uint vk) {
	assert(vk < 256);
	return keyMap[vk];
}

template <uint from, uint to> uint getBits(const uint in) {
	return (in & ((1u << to) - 1u)) >> from;
}

bool on_WM_QUIT(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	out.type = Common::EVENT_QUIT;
	return true;
}

bool on_WM_KEYDOWN(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	// The previous key state. The value is 1 if the key is down before the
	// message is sent, or it is zero if the key is up.
	const uint pState = getBits<30, 30>(msg.lParam);
	out.type = Common::EVENT_KEYDOWN;
	const uint vKey = msg.wParam;
	const keyInfo &key = keyMapLookup(vKey);
	out.kbd.keycode = key.keyCode;
	out.kbd.ascii = key.ascii;
	return out.kbd.keycode != KEYCODE_INVALID;
}

bool on_WM_KEYUP(const ::tagMSG &msg, Common::Event &out) {
	using namespace Common;
	out.type = Common::EVENT_KEYUP;
	const uint vKey = msg.wParam;
	const keyInfo &key = keyMapLookup(vKey);
	out.kbd.keycode = key.keyCode;
	out.kbd.ascii = key.ascii;
	return out.kbd.keycode != KEYCODE_INVALID;
}

bool on_WM_MOUSE_X(const ::tagMSG &msg, Common::Event &out, uint scale) {
	using namespace Common;
	switch (msg.message) {
	case WM_MOUSEMOVE:
		out.type = Common::EVENT_MOUSEMOVE;
		break;
	case WM_LBUTTONDOWN:
		out.type = Common::EVENT_LBUTTONDOWN;
		break;
	case WM_LBUTTONUP:
		out.type = Common::EVENT_LBUTTONUP;
		break;
	case WM_RBUTTONDOWN:
		out.type = Common::EVENT_RBUTTONDOWN;
		break;
	case WM_RBUTTONUP:
		out.type = Common::EVENT_RBUTTONUP;
		break;
	default:
		return false;
	}
	out.mouse.x = short(msg.lParam & 0xffffu) / scale;
	out.mouse.y = short(msg.lParam >> 16) / scale;
	return true;
}
} // namespace

Win32EventSource::Win32EventSource(class GDIGraphicsManager *window)
	: _window(window) {
	// generate windows VK code to ScummVM key code table
	keyMapInit();
}

bool Win32EventSource::handleEvent(tagMSG &msg, Common::Event &event) {
	using namespace Common;
	memset(&event, 0, sizeof(event));
	// get the window scale for mouse scaling
	const uint wndScale = this->_window->getScale();
	assert(wndScale >= 1);
	switch (msg.message) {
	case WM_QUIT:
		return on_WM_QUIT(msg, event);
	case WM_KEYDOWN:
		return on_WM_KEYDOWN(msg, event);
	case WM_KEYUP:
		return on_WM_KEYUP(msg, event);
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		return on_WM_MOUSE_X(msg, event, wndScale);
	default:
		event.type = Common::EVENT_INVALID;
		return false;
	}
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

	//XXX: this is a little wedged in here, can we clean this up?
	// call the timer handler reguarly
	Common::TimerManager *timer = g_system->getTimerManager();
	assert(timer);
	DefaultTimerManager * defTimer = static_cast<DefaultTimerManager*>(timer);
	defTimer->handler();

	// no event generated
	return false;
}
