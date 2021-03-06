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
#ifndef SLUDGE_SPRITES_H
#define SLUDGE_SPRITES_H

#include "graphics/surface.h"
#include "graphics/transparent_surface.h"

namespace Sludge {

struct onScreenPerson;
struct zBufferData;

struct sprite {
	int xhot, yhot;
	Graphics::Surface surface;
	Graphics::Surface burnSurface;
};

class spritePalette {
public:
	uint16 *pal;
	byte *r;
	byte *g;
	byte *b;
	byte originalRed, originalGreen, originalBlue, total;

	spritePalette() : pal(0), r(0), g(0), b(0), total(0) {
		originalRed = originalGreen = originalBlue = 255;
	}

	~spritePalette() {
		delete[] pal;
		delete[] r;
		delete[] g;
		delete[] b;
	}
};

struct spriteBank {
	int total;
	int type;
	sprite *sprites;
	spritePalette myPalette;
	bool isFont;
};

void forgetSpriteBank(spriteBank &forgetme);
bool loadSpriteBank(char *filename, spriteBank &loadhere);
bool loadSpriteBank(int fileNum, spriteBank &loadhere, bool isFont);

void fontSprite(int x1, int y1, sprite &single, const spritePalette &fontPal);
void flipFontSprite(int x1, int y1, sprite &single, const spritePalette &fontPal);

bool scaleSprite(sprite &single, const spritePalette &fontPal, onScreenPerson *thisPerson, bool mirror);
void pasteSpriteToBackDrop(int x1, int y1, sprite &single, const spritePalette &fontPal);
bool reserveSpritePal(spritePalette &sP, int n);
void fixScaleSprite(int x1, int y1, sprite &single, const spritePalette &fontPal, onScreenPerson *thisPerson, const int camX, const int camY, bool);
void burnSpriteToBackDrop(int x1, int y1, sprite &single, const spritePalette &fontPal);

void resetSpriteLayers(zBufferData *ptrZBuffer, int x, int y, bool upsidedown);
void addSpriteDepth(Graphics::Surface *ptr, int depth, int x, int y, Graphics::FLIP_FLAGS flip, int width = -1, int height = -1, uint32 color = TS_ARGB(255, 255, 255, 255));
void displaySpriteLayers();
void killSpriteLayers();

} // End of namespace Sludge

#endif
