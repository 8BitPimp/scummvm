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

#ifndef BACKENDS_GRAPHICS_GDI_H
#define BACKENDS_GRAPHICS_GDI_H

#include "backends/graphics/graphics.h"
#include "graphics/surface.h"

class GDIGraphicsManager : public GraphicsManager {
public:
    GDIGraphicsManager();
	virtual ~GDIGraphicsManager();

	bool hasFeature(OSystem::Feature f);
	void setFeatureState(OSystem::Feature f, bool enable);
	bool getFeatureState(OSystem::Feature f);
	const OSystem::GraphicsMode *getSupportedGraphicsModes() const;
	int getDefaultGraphicsMode() const;
	bool setGraphicsMode(int mode);
	void resetGraphicsScale();
	int getGraphicsMode() const;
	inline Graphics::PixelFormat getScreenFormat() const;
	inline Common::List<Graphics::PixelFormat> getSupportedFormats() const;
	void initSize(uint width, uint height,
				  const Graphics::PixelFormat *format = NULL);
	virtual int getScreenChangeID() const;
	void beginGFXTransaction();
	OSystem::TransactionError endGFXTransaction();
	int16 getHeight();
	int16 getWidth();
	void setPalette(const byte *colors, uint start, uint num);
	void grabPalette(byte *colors, uint start, uint num);
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w,
						  int h);
	Graphics::Surface *lockScreen();
	void unlockScreen();
	void fillScreen(uint32 col);
	void updateScreen();
	void setShakePos(int shakeOffset);
	void setFocusRectangle(const Common::Rect &rect);
	void clearFocusRectangle();
	void showOverlay();
	void hideOverlay();
	Graphics::PixelFormat getOverlayFormat() const;
	void clearOverlay();
	void grabOverlay(void *buf, int pitch);
	void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w,
						   int h);
	int16 getOverlayHeight();
	int16 getOverlayWidth();
	bool showMouse(bool visible);
	void warpMouse(int x, int y);
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX,
						int hotspotY, uint32 keycolor, bool dontScale = false,
						const Graphics::PixelFormat *format = NULL);
	void setCursorPalette(const byte *colors, uint start, uint num);

protected:
	Graphics::Surface _surface;
	struct GDIDetail *_detail;
	uint palette[256];
};

#endif
