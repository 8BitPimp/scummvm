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
#undef min
#undef max

#include "common/system.h"
#include "common/config-manager.h"
#include "common/rect.h"

#include "gdi-graphics.h"

#define LOG_CALL()                                                             \
	{ /* warning("%s\n", __FUNCTION__); */ }

static const OSystem::GraphicsMode s_noGraphicsModes[] = {
	{"320x240x32", "Default Graphics Mode", 0}, //
	{NULL, NULL, NULL}};

enum { SCREEN_GAME = 0, SCREEN_OVERLAY, SCREEN_COUNT__ };

struct Point {

	Point() {}
	Point(int ax, int ay) : x(ax), y(ay) {}

	int x, y;
};

struct Rect {
	int x0, y0;
	int x1, y1;

	bool contains(const Point &point) const {
		return (point.x >= xMin() && point.x <= xMin()) &&
		       (point.y >= yMin() && point.y <= yMax());
	}

	int xMin() const { return min(x0, x1); }

	int xMax() const { return max(x0, x1); }

	int yMin() const { return min(y0, y1); }

	int yMax() const { return max(y0, y1); }

	int dx() const { return xMax() - xMin(); }

	int dy() const { return yMax() - yMin(); }

	static bool notOverlap(const Rect &a, const Rect &b) {
		return a.xMax() < b.xMin() || a.xMin() > b.xMax() ||
		       a.yMax() < b.yMin() || a.yMin() > b.yMax();
	}

	static Rect intersect(const Rect &a, const Rect &b) {
		const Rect out = {
			clip(a.xMin(), b.x0, a.xMax()), clip(a.yMin(), b.y0, a.yMax()),
			clip(a.xMin(), b.x1, a.xMax()), clip(a.yMin(), b.y1, a.yMax())
		};
		return out;
	}

	template <typename type_t>
	static type_t min(const type_t &a, const type_t &b) {
		return (a < b) ? a : b;
	}

	template <typename type_t>
	static type_t max(const type_t &a, const type_t &b) {
		return (a > b) ? a : b;
	}

	template <typename type_t>
	static type_t clip(const type_t &lo, const type_t &in, const type_t &hi) {
		return min(hi, (max(lo, in)));
	}
};

struct BlitBuffer {

	struct BlitInfo {
		const void *inData;
		int x, y;
		uint w, h;
		uint pitch;
		bool mask;
		uint maskKey;
	};

	BlitBuffer()
		: _data(NULL)
		, _width(0)
		, _height(0) {}

	BlitBuffer(uint width, uint height)
		: _data(new uint[width * height + 1])
		, _width(width)
		, _height(height) {
		assert(_data);
	}

	~BlitBuffer() {
		if (_data) {
			delete[] _data;
		}
	}

	void clear(uint val) {
		assert(_data && _width && _height);
		const uint *end = _data + (_width * _height);
		for (uint *ptr = _data; ptr < end; ++ptr) {
			*ptr = val;
		}
	}

	void rect(const Rect &rect, const uint rgb) {
		const Rect boarder = {0, 0, _width - 1, _height - 1};
		const Rect clip = Rect::intersect(boarder, rect);
		uint *x0 = _data + clip.yMin() * _width;
		uint *x1 = _data + clip.yMax() * _width;
		const int xMin = clip.xMin(), xMax = clip.xMax();
		const int yMin = clip.yMin(), yMax = clip.yMax();
		for (int x = xMin; x < xMax; ++x) {
			x0[x] = rgb;
			x1[x] = rgb;
		}
		uint *y0 = _data + clip.xMin() + clip.yMin() * _width;
		uint *y1 = _data + clip.xMax() + clip.yMin() * _width;
		for (int y = yMin; y <= yMax; ++y) {
			(*y0 = rgb), y0 += _width;
			(*y1 = rgb), y1 += _width;
		}
	}

	void blit(BlitInfo &info) {
		if (info.mask) {
			_blit<uint, true>(info);
		}
		else {
			_blit<uint, false>(info);
		}
	}

	void copyTo(void *vDst, uint pitch, uint height) {
		const uint cpyPitch = Rect::min(pitch, _width * sizeof(uint));
		const uint cpyHeight = Rect::min(height, _height);
		const uint *src = _data;
		byte *dst = (byte *)vDst;
		for (uint y = 0; y < cpyHeight; ++y) {
			memcpy(dst, src, cpyPitch);
			dst += pitch, src += _width;
		}
	}

	uint width() const { return _width; }

	uint height() const { return _height; }

	uint *data() const { return _data; }

	void resize(uint width, uint height) {
		if (_data) {
			delete[] _data;
		}
		_data = new uint[width * height];
		_width = width;
		_height = height;
	}

protected:
	template <typename SrcPix, bool Masked>
	void _blit(BlitInfo &info);

	uint *_data;
	uint _width, _height;
};

template <typename SrcPix, bool Masked>
void BlitBuffer::_blit(BlitBuffer::BlitInfo &info) {
	// access helpers
	const int x = info.x, y = info.y, w = info.w, h = info.h;
	const uint key = info.maskKey;
	// find clipped rect
	const Rect dstRect = {x, y, (x + w - 1), (y + h - 1)};
	const Rect srcRect = {0, 0, (int)_width - 1, (int)_height - 1};
	if (Rect::notOverlap(dstRect, srcRect)) {
		// no intersection; no blit required
		return;
	}
	// find intersection region
	const Rect clipRect = Rect::intersect(srcRect, dstRect);
	// find source start location
	const Point srcDelta = Point(clipRect.xMin() - x, clipRect.yMin() - y);
	assert(srcDelta.x >= 0 && srcDelta.y >= 0);
	const SrcPix *srcInData = (const SrcPix *)info.inData;
	const uint srcPitch = info.pitch / sizeof(SrcPix);
	const SrcPix *src = srcInData + srcDelta.x + srcDelta.y * srcPitch;
	// find destination start location
	const uint dstPitch = _width;
	uint *dst = _data + clipRect.xMin() + (clipRect.yMin() * dstPitch);
	// itteration range
	const Point diff = Point(clipRect.dx(), clipRect.dy());
	// end point for access checking
	const uint *const dstEnd = _data + _height * _width;
	assert(dstEnd >= (dst + dstPitch * diff.y + diff.x));
	// vertical loop
	for (int y = 0; y <= diff.y; ++y) {
		// row itteration pointers
		const SrcPix *srcRow = src;
		uint *dstRow = dst;
		// horizontal loop
		for (int x = 0; x <= diff.x; ++x) {
			if (Masked) {
				if (*srcRow != key) {
					*dstRow = *srcRow;
				}
			}
			else {
				*dstRow = *srcRow;
			}
			// increment pixel
			++srcRow, ++dstRow;
		}
		// increment scanlines
		src += srcPitch, dst += dstPitch;
	}
}

struct GDICursor {

	GDICursor()
		: _size(0, 0)
		, _allocated(0)
		, _data8(NULL)
		, _data32(NULL)
		, _dirty(true)
		, _offset(0, 0)
		, _key(0xff)
		, _visible(true)
	{
		memset(_palette, 0xff, sizeof(_palette));
	}

	~GDICursor() {
		if (_data8) {
			delete [] _data8;
			_data8 = NULL;
		}
		if (_data32) {
			delete [] _data32;
			_data32 = NULL;
		}
	}

	void show(bool visible) {
		_visible = visible;
	}

	void setCursor(
		const void *buf,
		Point size,
		Point hotspot,
		uint32 keycolor,
		bool dontScale,
		const Graphics::PixelFormat *format)
	{
		_resize(size);
		assert(_data8 && _allocated >= uint(size.x * size.y));
		_offset = hotspot;
		_key = keycolor;

		_dirty = true;
		const byte *src = (const byte*)buf;
		const uint elms = _size.x * _size.y;
		for (uint i = 0; i<elms; ++i) {
			_data8[i] = src[i];
		}
	}

	void setPalette(
		const byte *colors,
		uint start,
		uint num)
	{
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
			_palette[palIndex] = color;
			colors += 3;
		}
		_dirty = true;
	}

	void blit(Point pos, BlitBuffer & dst) {
		if (_visible && _data32) {
			_convert();
			BlitBuffer::BlitInfo info;
			info.w = _size.x;
			info.h = _size.y;
			info.x = pos.x - _offset.x;
			info.y = pos.y - _offset.y;
			info.pitch = _size.x * sizeof(uint);
			info.inData = _data32;
			info.mask = true;
			info.maskKey = _palette[_key & 0xff];
			dst.blit(info);
		}
	}

protected:
	void _resize(const Point & size) {
		const size_t elms = size.x * size.y;
		if (elms > _allocated) {
			if (_data8) {
				delete [] _data8;
			}
			if (_data32) {
				delete [] _data32;
			}
			_allocated = elms;
			_data8  = new byte[elms];
			_data32 = new uint[elms];
		}
		_size = size;
	}

	void _convert() {
		if (!_dirty) {
			return;
		}
		const uint elms = _size.x * _size.y;
		if (elms) {
			assert(_data8 && _data32 && _allocated);
		}
		const byte key = _key;
		for (uint i=0; i<elms; ++i) {
			const byte pix = _data8[i];
			const uint rgb = _palette[pix];
			_data32[i] = rgb;
		}
		_dirty = false;
	}

	Point _size;
	size_t _allocated;
	byte *_data8;
	uint *_data32;
	bool _dirty;
	Point _offset;
	uint _key;
	uint _palette[256];
	bool _visible;
};

// scumm buffer is a buffer in the target format
struct ScummBuffer {

	ScummBuffer()
		: _dirty(true)
		, _pix(NULL)
		, _width(0)
		, _height(0) {
		memset(_palette, 0, sizeof(_palette));
	}

	void release() {
		if (_pix) {
			delete[] _pix;
			_pix = NULL;
		}
	}

	void resize(uint width, uint height) {
		release();
		_width = width;
		_height = height;
		_pix = new byte[width * height];
	}

	void copyRectToScreen(
		const void *buf,
		int pitch,
		int x, int y,
		int w, int h) {
		assert(buf && _pix);
		// note: the docs say we dont have to care about clipping
		const byte *src = (const byte *)buf;
		byte *dst = _pix + x + y * _width;
		for (int iy = 0; iy < h; ++iy) {
			memcpy(dst, src, w);
			src += pitch;
			dst += _width;
		}
		_dirty = true;
	}

	void setPalette(const byte *colors, uint start, uint num) {
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
			_palette[palIndex] = color;
			colors += 3;
		}
		_dirty = true;
	}

	void grabPalette(byte *colors, uint start, uint num) {
		for (uint i = 0; i < num; ++i) {
			const uint palIndex = start + i;
			assert(palIndex < 256);
			// pack RGB triplet
			const uint color = _palette[palIndex];
			colors[0] |= color >> 16;
			colors[1] |= color >> 8;
			colors[2] |= color >> 0;
			colors += 3;
		}
	}

	void render(uint w, uint h, uint *dst) {
		byte *index = _pix;
		for (uint y = 0; y < h; ++y) {
			for (uint x = 0; x < w; ++x) {
				dst[x] = _palette[index[x]];
			}
			dst += w;
			index += _width;
		}
		// TODO: blit the cursor at this stage!
	}

	uint width() const { return _width; }

	uint height() const { return _height; }

	byte *pixels() { return _pix; }

	uint *palette() { return _palette; }

	void clear(byte index) { memset(_pix, index, _width * _height); }

	bool _dirty;

protected:
	byte *_pix;
	uint _width, _height;
	uint _palette[256];
};

struct GDIDetail {

	struct LockInfo {
		uint *_pixels;
		uint _width, _pitch;
		uint _height;
	};

	GDIDetail()
		: _window(NULL)
		, _dwExStyle(WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW)
		, _dwStyle(WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU)
		, _scale(1)
		, _activeScreen(NULL) {}

	~GDIDetail() {
		release();
	}

	bool windowCreate(uint w, uint h, uint scale);
	bool windowResize(uint w, uint h, uint scale);
	bool screenInvalidate();
	bool screenLock(LockInfo *out);

	HWND windowHandle() const { return _window; }

	uint screenWidth() const {
		assert(_activeScreen);
		return _activeScreen->width();
	}

	uint screenHeight() const {
		assert(_activeScreen);
		return _activeScreen->height();
	}

	uint screenScale() const { return _scale; }

	void screenActivate(const uint index) {
		assert(index < SCREEN_COUNT__);
		_activeScreen = _screenArray + index;
	}

	BlitBuffer &screen(const uint index) {
		assert(index < SCREEN_COUNT__);
		return _screenArray[index];
	}

	void release() {
		if (_window) {
			CloseWindow(_window);
			_window = NULL;
		}
	}

	// pallete based screen buffer
	ScummBuffer _scummBuffer;
	// mouse cursor
	GDICursor _cursor;

protected:
	static LRESULT CALLBACK windowEventHandler(HWND, UINT, WPARAM, LPARAM);

	// repaint the current window (WM_PAINT)
	LRESULT _windowRedraw();
	// create a back buffer of a specific size
	bool _screenCreate(uint w, uint h);
	// up scale ratio
	uint _scale;
	// window handle
	HWND _window;
	// window styles used by resizing code
	const DWORD _dwStyle, _dwExStyle;
	// screen buffer info
	BITMAPINFO _bmp;
	BlitBuffer _screenArray[SCREEN_COUNT__];
	BlitBuffer *_activeScreen;
};

bool GDIDetail::_screenCreate(uint w, uint h) {
	assert(w && h);
	for (int i = 0; i < SCREEN_COUNT__; ++i) {
		_screenArray[i].resize(w, h);
	}
	_activeScreen = _screenArray;
	_scummBuffer.resize(w, h);
	// create bitmap info
	BITMAPINFOHEADER &bih = _bmp.bmiHeader;
	ZeroMemory(&_bmp, sizeof(BITMAPINFO));
	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biBitCount = 32;
	bih.biWidth = w;
	bih.biHeight = h;
	bih.biPlanes = 1;
	bih.biCompression = BI_RGB;
	// success
	return true;
}

LRESULT GDIDetail::_windowRedraw() {
	if (_activeScreen == NULL) {
		// no screen; hand back to default handler
		return DefWindowProcA(_window, WM_PAINT, 0, 0);
	}
	// blit buffer to screen
	HDC dc = GetDC(_window);
	const BITMAPINFOHEADER &bih = _bmp.bmiHeader;

	RECT clientRect;
	if (!GetClientRect(_window, &clientRect)) {
		ZeroMemory(&clientRect, sizeof(RECT));
		clientRect.bottom = bih.biHeight;
		clientRect.right = bih.biWidth;
	}

	// SetDIBits(dc, ...)

	// blit backbuffer to the screen
	assert(_activeScreen);
	BlitBuffer &bb = *_activeScreen;
	StretchDIBits(
		dc,                    // hdc
		0,                     // xDst
		clientRect.bottom - 1, // yDst
		clientRect.right,      // nDestWidth
		-clientRect.bottom,    // nDestHeight
		0,                     // xSrc
		0,                     // ySrc
		bb.width(),            // nSrcWidth
		bb.height(),           // sSrcHeight
		bb.data(),             // lpBits
		&_bmp,                 // lpBitsInfo
		DIB_RGB_COLORS,        // iUsage
		SRCCOPY                // dwRop
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

bool GDIDetail::windowCreate(uint w, uint h, uint scale) {
	assert(w && h);
	if (_window) {
		// resize window and create the offscreen buffer
		return windowResize(w, h, scale);
	}
	static const char *kClassName = "ScummVMClass";
	static const char *kWndName = "ScummVM";
	//
	HCURSOR cursor = LoadCursor(NULL, IDC_CROSS);
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
		cursor,              // hCursor;
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
		32,               // nWidth
		32,               // nHeight
		NULL,             // hWndParent
		NULL,             // hMenu
		hinstance,        // hInstance
		NULL              // lpParam
	);
	if (_window == NULL) {
		return false;
	}
	// resize window and create the offscreen buffer
	if (!windowResize(w, h, scale)) {
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

	screenActivate(SCREEN_GAME);
	return true;
}

bool GDIDetail::windowResize(uint w, uint h, uint scale) {
	assert(w && h);
	// store scale parameter
	_scale = scale;
	// get current window rect
	RECT rect;
	GetWindowRect(_window, &rect);
	// adjust to expected client area
	rect.right = rect.left + (w * scale);
	rect.bottom = rect.top + (h * scale);
	// adjust so rect becomes client area
	if (AdjustWindowRectEx(&rect, _dwStyle, FALSE, _dwExStyle) == FALSE) {
		return false;
	}
	// XXX: this will also move the window by some pixels
	MoveWindow(
		_window,
		rect.left,
		rect.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		TRUE
	);
	// create a new screen buffer for out window size
	if (!_screenCreate(w, h)) {
		return false;
	}
	// force a screen redraw
	screenInvalidate();
	return true;
}

bool GDIDetail::screenLock(LockInfo *info) {
	assert(info && _activeScreen);
	info->_pixels = _activeScreen->data();
	info->_width  = _activeScreen->width();
	info->_height = _activeScreen->height();
	info->_pitch  = _activeScreen->width();
	return info->_pixels != NULL;
}

bool GDIDetail::screenInvalidate() {
	// invalidate entire window
	InvalidateRect(_window, NULL, FALSE);
	return UpdateWindow(_window) == TRUE;
}

GDIGraphicsManager::GDIGraphicsManager()
	: _detail(new GDIDetail) {
//	// clear the palette
//	memset(_detail->_scummBuffer.palette(), 0, sizeof(_palette));
}

GDIGraphicsManager::~GDIGraphicsManager() {
	if (_detail) {
		delete _detail;
		_detail = NULL;
	}
}

bool GDIGraphicsManager::hasFeature(OSystem::Feature f) {
	// warning("has feature(%u)", (uint)f);
	// if (f==OSystem::Feature::kFeatureOverlaySupportsAlpha)
	//     return true;

	using namespace Common;

	switch (f) {
	case OSystem::kFeatureCursorPalette:
		return true;
	default:
		return false;
	}

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
		// XXX: should this be 320x200 ?
		_detail->windowCreate(320, 240, 2);
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
	Graphics::PixelFormat format(
		4,  // BytesPerPixel
		8,  // RBits
		8,  // GBits
		8,  // BBits
		0,  // ABits
		16, // RShift
		8,  // GShift
		0,  // BShift
		24  // AShift
	);
	return format;
}

inline Common::List<Graphics::PixelFormat>
GDIGraphicsManager::getSupportedFormats() const {
	LOG_CALL();
	Common::List<Graphics::PixelFormat> list;
	list.push_back(getScreenFormat());
	return list;
}

void GDIGraphicsManager::initSize(uint width, uint height,
								  const Graphics::PixelFormat *format) {
	LOG_CALL();
	_detail->windowResize(width, height, 2);
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
	ScummBuffer &sb = _detail->_scummBuffer;
	sb.setPalette(colors, start, num);
	_detail->_cursor.setPalette(colors, start, num);
}

void GDIGraphicsManager::grabPalette(byte *colors, uint start, uint num) {
	ScummBuffer &sb = _detail->_scummBuffer;
	sb.grabPalette(colors, start, num);
}

void GDIGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x,
										  int y, int w, int h) {

	ScummBuffer &sb = _detail->_scummBuffer;
	sb.copyRectToScreen(buf, pitch, x, y, w, h);
}

Graphics::Surface *GDIGraphicsManager::lockScreen() {
	assert(_detail);
	BlitBuffer &bb = _detail->screen(SCREEN_GAME);
	GDIDetail::LockInfo lock;
	if (!_detail->screenLock(&lock)) {
		return NULL;
	}
	_surface.init(
		bb.width(),
		bb.height(),
		bb.width(),
		bb.data(),
		getScreenFormat()
	);
	return &_surface;
}

void GDIGraphicsManager::unlockScreen() {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::fillScreen(uint32 col) {
	assert(col < 256);
	ScummBuffer &sb = _detail->_scummBuffer;
	sb.clear(col);
}

void GDIGraphicsManager::updateScreen() {
	// TODO: only update when active screen
	ScummBuffer &sb = _detail->_scummBuffer;
	if (sb._dirty) {
		BlitBuffer &bb = _detail->screen(SCREEN_GAME);
		sb.render(bb.width(), bb.height(), bb.data());
		sb._dirty = false;
#if 1
		do {
			POINT mousePos;
			if (GetCursorPos(&mousePos) == FALSE) {
				break;
			}
			if (ScreenToClient(_detail->windowHandle(), &mousePos) == FALSE) {
				break;
			}
			mousePos.x /= (int)_detail->screenScale();
			mousePos.y /= (int)_detail->screenScale();

			_detail->_cursor.blit(Point(mousePos.x, mousePos.y), bb);
			// render over old mouse pointer next time
			// XXX: optimize with dirty regions!
			sb._dirty = true;
		} while (false);
#endif
	}
	_detail->screenInvalidate();
}

void GDIGraphicsManager::setShakePos(int shakeOffset) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::setFocusRectangle(const Common::Rect &rect) {
#if 0
	const Rect r = {rect.left, rect.top, rect.right, rect.bottom};
	BlitBuffer &bb = _detail->screen(SCREEN_GAME);
	bb.rect(r, 0xAABBFF);
#endif
}

void GDIGraphicsManager::clearFocusRectangle() {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::showOverlay() {
	LOG_CALL();
	_detail->screenActivate(SCREEN_OVERLAY);
}

void GDIGraphicsManager::hideOverlay() {
	LOG_CALL();
	_detail->screenActivate(SCREEN_GAME);
}

Graphics::PixelFormat GDIGraphicsManager::getOverlayFormat() const {
	return getScreenFormat();
}

void GDIGraphicsManager::clearOverlay() {
	BlitBuffer &bb = _detail->screen(SCREEN_OVERLAY);
	bb.clear(0x202020);
}

void GDIGraphicsManager::grabOverlay(void *buf, int pitch) {
	BlitBuffer &bb = _detail->screen(SCREEN_OVERLAY);
	bb.copyTo(buf, pitch, bb.height());
}

void GDIGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x,
										   int y, int w, int h) {
	BlitBuffer::BlitInfo info = {
		buf,         // input buffer
		(uint)x,     // x start coordinate
		(uint)y,     // y start coordinate
		(uint)w,     // copy width
		(uint)h,     // copy height
		(uint)pitch, // image pitch in bytes
	};
	BlitBuffer &bb = _detail->screen(SCREEN_OVERLAY);
	bb.blit(info);
#if 0
	const Rect rect = {x, y, x + w - 1, y + h - 1};
	bb.rect(rect, 0xa0a0a0);
#endif
}

int16 GDIGraphicsManager::getOverlayHeight() {
	assert(_detail);
	BlitBuffer &bb = _detail->screen(SCREEN_OVERLAY);
	return bb.height();
}

int16 GDIGraphicsManager::getOverlayWidth() {
	assert(_detail);
	BlitBuffer &bb = _detail->screen(SCREEN_OVERLAY);
	return bb.width();
}

bool GDIGraphicsManager::showMouse(bool visible) {
	assert(_detail);
	_detail->_cursor.show(visible);
	return true;
}

void GDIGraphicsManager::warpMouse(int x, int y) {
	// empty
	LOG_CALL();
}

void GDIGraphicsManager::setMouseCursor(
	const void *buf,
	uint w, uint h,
	int hx, int hy,
	uint32 key,
	bool dontScale,
	const Graphics::PixelFormat *format)
{
	assert(_detail);
	_detail->_cursor.setCursor(buf, Point(w, h), Point(hx, hy), key, dontScale, format);
}

void GDIGraphicsManager::setCursorPalette(
	const byte *colors,
	uint start,
	uint num)
{
	assert(_detail);
	_detail->_cursor.setPalette(colors, start, num);
}

uint GDIGraphicsManager::getScale() const {
	return _detail->screenScale();
}
