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

namespace {
template <typename type_t> type_t *ptrShift(type_t *ptr, const uint shift) {
	return (type_t *)((uintptr_t)ptr + shift);
}
} // namespace

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
	// blit buffer to screen
	HDC dc = GetDC(_window);
	const BITMAPINFOHEADER &bih = _screen._bmp.bmiHeader;
	// blit backbuffer to the screen
	StretchDIBits(
		dc,              // hdc
		0,               // xDst
		bih.biHeight-1,  // yDst
		bih.biWidth,     // nDestWidth
		-bih.biHeight,   // nDestHeight
		0,               // xSrc
		0,               // ySrc
		bih.biWidth,     // nSrcWidth
		bih.biHeight,    // sSrcHeight
		_screen._data,   // lpBits
		&(_screen._bmp), // lpBitsInfo
		DIB_RGB_COLORS,  // iUsage
		SRCCOPY          // dwRop
	);
	// finished WM_PAINT
	ReleaseDC(_window, dc);
	ValidateRect(_window, NULL);
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
		return DefWindowProcA(hWnd, Msg, wParam, lParam);
	}
}

bool GDIDetail::windowCreate(uint w, uint h) {
	assert(_window == NULL && w && h);
	static const char *kClassName = "ScummVMClass";
	static const char *kWndName = "ScummVM";
	// create window class
	HINSTANCE hinstance = GetModuleHandle(NULL);
	WNDCLASSEXA wndClassEx = {
		sizeof(WNDCLASSEXA), // cbSize;
		0,                   // style;
		windowEventHandler,  // lpfnWndProc;
		0,                   // cbClsExtra;
		0,                   // cbWndExtra;
		hinstance,           // hInstance;
		NULL,                // hIcon;
		NULL,                // hCursor;
		NULL,                // hbrBackground;
		NULL,                // lpszMenuName;
		(LPCSTR)kClassName,  // lpszClassName;
		NULL                 // hIconSm
	};
	if (RegisterClassExA(&wndClassEx) == 0) {
		return false;
	}
	// create window
	_window = CreateWindowExA(
		_dwExStyle,       // dwExStyle
		kClassName,       // lpClassName
		(LPCSTR)kWndName, // lpWindowName
		_dwStyle,         // dwStyle
		CW_USEDEFAULT,    // x
		CW_USEDEFAULT,    // y
		w,                // nWidth
		h,                // nHeight
		NULL,             // hWndParent
		NULL,             // hMenu
		hinstance,        // hInstance
		NULL              // lpParam
	);
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
	// XXX: this will also move the window by some pixels
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
	InvalidateRect(_window, NULL, FALSE);
	return UpdateWindow(_window) == TRUE;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

GDIGraphicsManager::GDIGraphicsManager()
	: _detail(new GDIDetail) {
	// clear the palette
	memset(palette, 0, sizeof(palette));
}

GDIGraphicsManager::~GDIGraphicsManager() {
	if (_detail) {
		delete _detail;
		_detail = nullptr;
	}
	LOG_CALL();
}

bool GDIGraphicsManager::hasFeature(OSystem::Feature f) {
	// empty
	warning("has feature(%u)", (uint)f);
	//    if (f==OSystem::Feature::kFeatureOverlaySupportsAlpha)
	//        return true;
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
	Graphics::PixelFormat format(4, 8, 8, 8, 0, 16, 8, 0, 24);
	return format;
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
	// note: palette entries seem to be passed in as RGB byte triplets.
	for (uint i = 0; i < num; ++i) {
		const uint palIndex = start + i;
		assert(palIndex < 256);
		// pack RGB triplet
		uint color = 0;
		color |= colors[0] << 16;
		color |= colors[1] << 8;
		color |= colors[2] << 0;
		// insert into palette
		palette[palIndex] = color;
		colors += 3;
	}
}

void GDIGraphicsManager::grabPalette(byte *colors, uint start, uint num) {
	for (uint i = 0; i < num; ++i) {
		const uint palIndex = start + i;
		assert(palIndex < 256);
		// pack RGB triplet
		const uint color = palette[palIndex];
		colors[0] |= color >> 16;
		colors[1] |= color >> 8;
		colors[2] |= color >> 0;
		colors += 3;
	}
}

void GDIGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x,
										  int y, int w, int h) {
	GDIDetail::LockInfo lock;
	if (!_detail->screenLock(&lock)) {
		return;
	}

	uint *dst = lock._pixels;
	const byte *src = (const byte *)buf;

	const uint width = min(w, lock._width);
	const uint height = min(h, lock._height);

	// HACK! Note to self: Do clipping you lamer!
	dst += x + y * lock._pitch;

	for (uint y = 0; y < height; ++y) {
		memcpy(dst, src, width * 4);
		// copy row to screen
		for (uint x = 0; x < width; ++x) {
			// index palette and write pixel
			dst[x] = palette[src[x]];
		}
		// advance scanlines
		src += pitch;
		dst += lock._pitch;
	}
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
//	LOG_CALL();
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
	return getScreenFormat();
}

void GDIGraphicsManager::clearOverlay() {
	// empty
	LOG_CALL();
	GDIDetail::LockInfo info;
	if (!_detail->screenLock(&info)) {
		return;
	}
	// clear step
	uint *dst = info._pixels;
	for (uint y = 0; y < info._height; ++y) {
		uint *row = dst;
		for (uint x = 0; x < info._width; ++x) {
			dst[x] = 0xff212121;
		}
		// TODO: make _pitch consistent
		dst = ptrShift<uint>(dst, info._pitch * sizeof(uint));
	}
}

void GDIGraphicsManager::grabOverlay(void *buf, int pitch) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x,
										   int y, int w, int h) {
	LOG_CALL();
#if 1
	const uint *src = (const uint *)buf;
	GDIDetail::LockInfo lock;
	if (!_detail->screenLock(&lock)) {
		return;
	}
	uint *dst = lock._pixels;

	if (x != 0 || y != 0 || w != 320 || h != 200) {
		return;
	}

	memcpy(dst, buf, pitch * h);
#endif
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
	// TODO: this is likely the cross cursor!
//	LOG_CALL();
}

void GDIGraphicsManager::setCursorPalette(const byte *colors, uint start,
										  uint num) {
	// empty
	LOG_CALL();
}
