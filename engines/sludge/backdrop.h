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

#ifndef SLUDGE_BACKDROP_H
#define SLUDGE_BACKDROP_H

#include "graphics/surface.h"

#include "sludge/variable.h"

namespace Sludge {

enum {
	LIGHTMAPMODE_NONE = -1,
	LIGHTMAPMODE_HOTSPOT,
	LIGHTMAPMODE_PIXEL,
	LIGHTMAPMODE_NUM
};

extern uint winWidth, winHeight, sceneWidth, sceneHeight;
extern int lightMapMode;


/**
 * parallax layers can scroll at different speeds
 * to the background image, giving the illusion of
 * depth to a scene as it moves.
 */
struct parallaxLayer {
	Graphics::Surface surface;
	int speedX, speedY;
	bool wrapS, wrapT;
	uint16 fileNum, fractionX, fractionY;
	int cameraX, cameraY;
	parallaxLayer *next;
	parallaxLayer *prev;
};

void killAllBackDrop();
bool resizeBackdrop(int x, int y);
bool killResizeBackdrop(int x, int y);
void killBackDrop();
void loadBackDrop(int fileNum, int x, int y);
void mixBackDrop(int fileNum, int x, int y);
void drawBackDrop();
void blankScreen(int x1, int y1, int x2, int y2);
void darkScreen();
void saveHSI(Common::WriteStream *stream);
bool loadHSI(Common::SeekableReadStream *stream, int, int, bool);
bool mixHSI(Common::SeekableReadStream *stream, int x = 0, int y = 0);
void drawHorizontalLine(uint, uint, uint);
void drawVerticalLine(uint, uint, uint);
void hardScroll(int distance);
bool getRGBIntoStack(uint x, uint y, stackHandler *sH);

// Also the light map stuff

void killLightMap();
bool loadLightMap(int v);

extern Graphics::Surface lightMap;

// And background parallax scrolling

void killParallax();
bool loadParallax(uint16 v, uint16 fracX, uint16 fracY);
void saveParallaxRecursive(parallaxLayer *me, Common::WriteStream *fp);

void nosnapshot();
bool snapshot();
void saveSnapshot(Common::WriteStream *stream);
bool restoreSnapshot(Common::SeekableReadStream *stream);

} // End of namespace Sludge

#endif
