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

#include "common/system.h"
#include "common/config-manager.h"

#include "gdi-graphics.h"

#define LOG_CALL()                                                             \
	{ warning("%s\n", __FUNCTION__); }

static const OSystem::GraphicsMode s_noGraphicsModes[] = {
	{"320x240x32", "Default Graphics Mode", 0}, //
	{NULL, NULL, NULL}};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

struct GDIDetail {

	struct LockInfo {
		uint *_pixels;
		uint _width, _pitch;
		uint _height;
	};

	GDIDetail()
		: _window(NULL)
		, _dwExStyle(WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW)
		, _dwStyle(WS_CAPTION | WS_OVERLAPPED) {
		ZeroMemory(&_screen, sizeof(_screen));
	}

	bool windowCreate(uint w, uint h);
	bool windowResize(uint w, uint h);
	bool screenInvalidate();
	bool screenLock(LockInfo *out);

	HWND windowHandle() const { return _window; }

	uint screenWidth() const { return _screen._bmp.bmiHeader.biWidth; }

	uint screenHeight() const { return _screen._bmp.bmiHeader.biHeight; }

protected:
	static LRESULT CALLBACK windowEventHandler(HWND, UINT, WPARAM, LPARAM);

	// repaint the current window (WM_PAINT)
	LRESULT _windowRedraw();
	// create a back buffer of a specific size
	bool _screenCreate(uint w, uint h);

	// window handle
	HWND _window;
	// window styles used by resizing code
	const DWORD _dwStyle, _dwExStyle;
	// screen buffer info
	struct {
		BITMAPINFO _bmp;
		uint *_data;
	} _screen;
};

bool GDIDetail::_screenCreate(uint w, uint h) {
	assert(w && h);
	// create screen data
	if (_screen._data) {
		delete[] _screen._data;
		_screen._data = NULL;
	}
	_screen._data = new uint[w * h];
	assert(_screen._data);
	// create bitmap info
	ZeroMemory(&_screen._bmp, sizeof(BITMAPINFO));
	BITMAPINFOHEADER &b = _screen._bmp.bmiHeader;
	b.biSize = sizeof(BITMAPINFOHEADER);
	b.biBitCount = 32;
	b.biWidth = w;
	b.biHeight = h;
	b.biPlanes = 1;
	b.biCompression = BI_RGB;
	// success
	return true;
}

LRESULT GDIDetail::_windowRedraw() {
	if (!_screen._data) {
		// no screen; hand back to default handler
		return DefWindowProcA(_window, WM_PAINT, 0, 0);
	}
	// flip blitted image
	static const int xFlip = false ? -1 : 1;
	static const int yFlip = false ? -1 : 1;
	// blit buffer to screen
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(_window, &ps);
	const BITMAPINFOHEADER &bih = _screen._bmp.bmiHeader;
	// do the bit blit
	StretchDIBits(hdc, 0, 0, bih.biWidth * xFlip, bih.biHeight * yFlip, 0, 0,
				  bih.biWidth, bih.biHeight, _screen._data, &(_screen._bmp),
				  DIB_RGB_COLORS, SRCCOPY);
	// finished WM_PAINT
	EndPaint(_window, &ps);
	return 0;
}

LRESULT CALLBACK GDIDetail::windowEventHandler(HWND hWnd, UINT Msg,
											   WPARAM wParam, LPARAM lParam) {
	GDIDetail *self;
	switch (Msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT: {
		// dispatch redraw to window instance
		if (self = (GDIDetail *)GetWindowLongPtrA(hWnd, GWLP_USERDATA)) {
			return self->_windowRedraw();
		}
		// fallthrough
	}
	default:
		//        printf("- %x\n", Msg);
		return DefWindowProcA(hWnd, Msg, wParam, lParam);
	}
}

bool GDIDetail::windowCreate(uint w, uint h) {
	assert(_window == NULL && w && h);
	static const char *kClassName = "ScummVMClass";
	static const char *kWndName = "ScummVM";
	// create window class
	HINSTANCE hinstance = GetModuleHandle(NULL);
	WNDCLASSEXA wndClassEx = {sizeof(WNDCLASSEXA),
							  CS_OWNDC,
							  windowEventHandler,
							  0,
							  0,
							  hinstance,
							  NULL,
							  NULL,
							  NULL,
							  NULL,
							  (LPCSTR)kClassName,
							  NULL};
	if (RegisterClassExA(&wndClassEx) == 0) {
		return false;
	}
	// create window
	_window = CreateWindowExA(_dwExStyle, kClassName, (LPCSTR)kWndName,
							  _dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, w, h,
							  NULL, NULL, hinstance, NULL);
	if (_window == NULL) {
		return false;
	}
	// resize window and create the offscreen buffer
	if (!windowResize(w, h)) {
		return false;
	}
	// pass class instance to window user data to that the static window proc
	// can dispatch to the GDIDetail instance.
	SetLastError(0);
	if (SetWindowLongPtrA(_window, GWLP_USERDATA, (LONG)this) == 0 &&
		GetLastError()) {
		// unable to set window user data
		return false;
	}
	// force a screen redraw
	screenInvalidate();
	// display window
	ShowWindow(_window, SW_SHOW);
	return true;
}

bool GDIDetail::windowResize(uint w, uint h) {
	assert(w && h);
	// get current window rect
	RECT rect;
	GetWindowRect(_window, &rect);
	// adjust to expected client area
	rect.right = rect.left + w;
	rect.bottom = rect.top + h;
	// adjust so rect becomes client area
	if (AdjustWindowRectEx(&rect, _dwStyle, FALSE, _dwExStyle) == FALSE) {
		return false;
	}
	// XXX: this will move the window by [5,5] pixels
	MoveWindow(_window, rect.left, rect.top, rect.right - rect.left,
			   rect.bottom - rect.top, TRUE);
	// create a new screen buffer for out window size
	if (!_screenCreate(w, h)) {
		return false;
	}
	// force a screen redraw
	screenInvalidate();
	return true;
}

bool GDIDetail::screenLock(LockInfo *info) {
	assert(info);
	info->_pixels = _screen._data;
	info->_width = _screen._bmp.bmiHeader.biWidth;
	info->_height = _screen._bmp.bmiHeader.biHeight;
	info->_pitch = _screen._bmp.bmiHeader.biWidth;
	return info->_pixels != NULL;
}

bool GDIDetail::screenInvalidate() {
	// invalidate entire window
	return InvalidateRect(_window, NULL, FALSE) != 0;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

GDIGraphicsManager::GDIGraphicsManager()
	: _detail(new GDIDetail) {}

GDIGraphicsManager::~GDIGraphicsManager() {
	if (_detail) {
		delete _detail;
		_detail = nullptr;
	}
	LOG_CALL();
}

bool GDIGraphicsManager::hasFeature(OSystem::Feature f) {
	// empty
	LOG_CALL();
	return false;
}

void GDIGraphicsManager::setFeatureState(OSystem::Feature f, bool enable) {
	// empty
	LOG_CALL();
}

bool GDIGraphicsManager::getFeatureState(OSystem::Feature f) {
	// empty
	LOG_CALL();
	return false;
}

const OSystem::GraphicsMode *
GDIGraphicsManager::getSupportedGraphicsModes() const {
	LOG_CALL();
	return s_noGraphicsModes;
}

int GDIGraphicsManager::getDefaultGraphicsMode() const {
	// empty
	LOG_CALL();
	return 0;
}

bool GDIGraphicsManager::setGraphicsMode(int mode) {
	LOG_CALL();
	switch (mode) {
	case 0:
		_detail->windowCreate(320, 240);
		return true;
	default:
		warning("%s: invalid mode %s", __FUNCTION__, mode);
		return false;
	}
}

void GDIGraphicsManager::resetGraphicsScale() {
	// empty
	LOG_CALL();
}

int GDIGraphicsManager::getGraphicsMode() const {
	//
	LOG_CALL();
	return 0;
}

inline Graphics::PixelFormat GDIGraphicsManager::getScreenFormat() const {
	LOG_CALL();
	// BytesPerPixel,
	// RBits, GBits, BBits, ABits,
	// RShift, GShift, BShift, AShift
	Graphics::PixelFormat format(4, 32, 32, 32, 32, 0, 8, 16, 32);
	return Graphics::PixelFormat::createFormatCLUT8();
}

inline Common::List<Graphics::PixelFormat>
GDIGraphicsManager::getSupportedFormats() const {
	LOG_CALL();
	Common::List<Graphics::PixelFormat> list;
	// BytesPerPixel,
	// RBits, GBits, BBits, ABits,
	// RShift, GShift, BShift, AShift
	list.push_back(getScreenFormat());
	return list;
}

void GDIGraphicsManager::initSize(uint width, uint height,
								  const Graphics::PixelFormat *format) {
	LOG_CALL();
    _detail->windowResize(width, height);
}

int GDIGraphicsManager::getScreenChangeID() const {
	// empty
	LOG_CALL();
	return 0;
}

void GDIGraphicsManager::beginGFXTransaction() {
	// empty
	LOG_CALL();
}

OSystem::TransactionError GDIGraphicsManager::endGFXTransaction() {
	LOG_CALL();
	return OSystem::kTransactionSuccess;
}

int16 GDIGraphicsManager::getHeight() {
	LOG_CALL();
	return _detail->screenHeight();
}

int16 GDIGraphicsManager::getWidth() {
	LOG_CALL();
	return _detail->screenWidth();
}

void GDIGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::grabPalette(byte *colors, uint start, uint num) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x,
										  int y, int w, int h) {
	// empty
	LOG_CALL();
}

Graphics::Surface *GDIGraphicsManager::lockScreen() {
	LOG_CALL();
	GDIDetail::LockInfo lock;
	if (!_detail->screenLock(&lock)) {
		return NULL;
	}
	_surface.init(lock._width, lock._height, lock._pitch, lock._pixels,
				  getScreenFormat());
	return &_surface;
}

void GDIGraphicsManager::unlockScreen() {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::fillScreen(uint32 col) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::updateScreen() {
	LOG_CALL();
    _detail->screenInvalidate();
}

void GDIGraphicsManager::setShakePos(int shakeOffset) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::setFocusRectangle(const Common::Rect &rect) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::clearFocusRectangle() {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::showOverlay() {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::hideOverlay() {
	// empty
	LOG_CALL();
}

Graphics::PixelFormat GDIGraphicsManager::getOverlayFormat() const {
	LOG_CALL();
	return Graphics::PixelFormat();
}

void GDIGraphicsManager::clearOverlay() {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::grabOverlay(void *buf, int pitch) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x,
										   int y, int w, int h) {
	// empty
	LOG_CALL();
}

int16 GDIGraphicsManager::getOverlayHeight() {
	// empty
	LOG_CALL();
    return _detail->screenHeight();
}

int16 GDIGraphicsManager::getOverlayWidth() {
	// empty
	LOG_CALL();
    return _detail->screenWidth();
}

bool GDIGraphicsManager::showMouse(bool visible) {
	// empty
	LOG_CALL();
	return !visible;
}

void GDIGraphicsManager::warpMouse(int x, int y) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::setMouseCursor(const void *buf, uint w, uint h,
										int hotspotX, int hotspotY,
										uint32 keycolor, bool dontScale,
										const Graphics::PixelFormat *format) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::setCursorPalette(const byte *colors, uint start,
										  uint num) {
	// empty
	LOG_CALL();
}
