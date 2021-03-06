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

#include "graphics/surface.h"
#include "graphics/transparent_surface.h"

#include "sludge/allfiles.h"
#include "sludge/fileset.h"
#include "sludge/people.h"
#include "sludge/sprites.h"
#include "sludge/moreio.h"
#include "sludge/newfatal.h"
#include "sludge/backdrop.h"
#include "sludge/sludger.h"
#include "sludge/zbuffer.h"
#include "sludge/imgloader.h"
#include "sludge/sludge.h"

namespace Sludge {

// Sprite display informations
struct SpriteDisplay {
	int x, y;
	int width, height;
	uint32 color;
	Graphics::FLIP_FLAGS flip;
	Graphics::Surface *surface;

	SpriteDisplay(int xpos, int ypos, Graphics::FLIP_FLAGS f, Graphics::Surface *ptr, int w = -1, int h = 1, uint32 c = TS_ARGB(255, 255, 255, 255)) :
			x(xpos), y(ypos), flip(f), surface(ptr), width(w), height(h), color(c) {
	}
};

// All sprites are sorted into different "layers" (up to 16) according to their relative y position to z-buffer zones
struct SpriteLayers {
	int numLayers;
	Common::List<SpriteDisplay> layer[16];
};

SpriteLayers spriteLayers;

extern zBufferData zBuffer;

extern inputType input;
extern int cameraX, cameraY;
extern float cameraZoom;
extern Graphics::Surface renderSurface;
extern Graphics::Surface backdropSurface;

byte currentBurnR = 0, currentBurnG = 0, currentBurnB = 0;

void forgetSpriteBank(spriteBank &forgetme) {
	if (forgetme.myPalette.pal) {
		delete[] forgetme.myPalette.pal;
		forgetme.myPalette.pal = NULL;
		delete[] forgetme.myPalette.r;
		forgetme.myPalette.r = NULL;
		delete[] forgetme.myPalette.g;
		forgetme.myPalette.g = NULL;
		delete[] forgetme.myPalette.b;
		forgetme.myPalette.b = NULL;
	}

	for (int i = 0; i < forgetme.total; ++i) {
		forgetme.sprites[i].surface.free();
		forgetme.sprites[i].burnSurface.free();
	}

	delete []forgetme.sprites;
	forgetme.sprites = NULL;

	// TODO: also remove sprite bank from allLoadedBanks
	// And add a function call for this function to the scripting language
}

bool reserveSpritePal(spritePalette &sP, int n) {
	if (sP.pal) {
		delete[] sP.pal;
		delete[] sP.r;
		delete[] sP.g;
		delete[] sP.b;
	}

	sP.pal = new uint16[n];
	if (!checkNew(sP.pal))
		return false;

	sP.r = new byte[n];
	if (!checkNew(sP.r))
		return false;
	sP.g = new byte[n];
	if (!checkNew(sP.g))
		return false;
	sP.b = new byte[n];
	if (!checkNew(sP.b))
		return false;
	sP.total = n;
	return (bool)(sP.pal != NULL) && (sP.r != NULL) && (sP.g != NULL) && (sP.b != NULL);
}

bool loadSpriteBank(int fileNum, spriteBank &loadhere, bool isFont) {

	int total, spriteBankVersion = 0, howmany = 0, startIndex = 0;
	byte *data;

	setResourceForFatal(fileNum);
	if (!openFileFromNum(fileNum))
		return fatal("Can't open sprite bank / font");

	loadhere.isFont = isFont;

	total = bigDataFile->readUint16BE();
	if (!total) {
		spriteBankVersion = bigDataFile->readByte();
		if (spriteBankVersion == 1) {
			total = 0;
		} else {
			total = bigDataFile->readUint16BE();
		}
	}

	if (total <= 0)
		return fatal("No sprites in bank or invalid sprite bank file");
	if (spriteBankVersion > 3)
		return fatal("Unsupported sprite bank file format");

	loadhere.total = total;
	loadhere.sprites = new sprite[total];
	if (!checkNew(loadhere.sprites))
		return false;
	byte **spriteData = new byte *[total];
	if (!checkNew(spriteData))
		return false;

	// version 1, 2, read how many now
	if (spriteBankVersion && spriteBankVersion < 3) {
		howmany = bigDataFile->readByte();
		startIndex = 1;
	}

	// version 3, sprite is png
	if (spriteBankVersion == 3) {
		debug(kSludgeDebugGraphics, "png sprite");
		for (int i = 0; i < total; i++) {
			loadhere.sprites[i].xhot = bigDataFile->readSint16LE();
			loadhere.sprites[i].yhot = bigDataFile->readSint16LE();
			if (!ImgLoader::loadPNGImage(bigDataFile, &loadhere.sprites[i].surface, false)) {
				return fatal("fail to read png sprite");
			}
		}
		finishAccess();
		setResourceForFatal(-1);
		return true;
	}

	// version 0, 1, 2
	for (int i = 0; i < total; i++) {
		uint picwidth, picheight;
		// load sprite width, height, relative position
		if (spriteBankVersion == 2) {
			picwidth = bigDataFile->readUint16BE();
			picheight = bigDataFile->readUint16BE();
			loadhere.sprites[i].xhot = bigDataFile->readSint16LE();
			loadhere.sprites[i].yhot = bigDataFile->readSint16LE();
		} else {
			picwidth = (byte)bigDataFile->readByte();
			picheight = (byte)bigDataFile->readByte();
			loadhere.sprites[i].xhot = bigDataFile->readByte();
			loadhere.sprites[i].yhot = bigDataFile->readByte();
		}

		// init data
		loadhere.sprites[i].surface.create(picwidth, picheight, *g_sludge->getScreenPixelFormat());
		if (isFont) {
			loadhere.sprites[i].burnSurface.create(picwidth, picheight, *g_sludge->getScreenPixelFormat());
		}
		data = (byte *)new byte[picwidth * (picheight + 1)];
		if (!checkNew(data))
			return false;
		memset(data + picwidth * picheight, 0, picwidth);
		spriteData[i] = data;

		// read color
		if (spriteBankVersion == 2) { // RUN LENGTH COMPRESSED DATA
			uint size = picwidth * picheight;
			uint pip = 0;

			while (pip < size) {
				byte col = bigDataFile->readByte();
				int looper;
				if (col > howmany) {
					col -= howmany + 1;
					looper = bigDataFile->readByte() + 1;
				} else
					looper = 1;

				while (looper--) {
					data[pip++] = col;
				}
			}
		} else { // RAW DATA
			uint bytes_read = bigDataFile->read(data, picwidth * picheight);
			if (bytes_read != picwidth * picheight && bigDataFile->err()) {
				warning("Reading error in loadSpriteBank.");
			}
		}
	}

	// read howmany for version 0
	if (!spriteBankVersion) {
		howmany = bigDataFile->readByte();
		startIndex = bigDataFile->readByte();
	}

	// Make palette for version 0, 1, 2
	if (!reserveSpritePal(loadhere.myPalette, howmany + startIndex))
		return false;
	for (int i = 0; i < howmany; i++) {
		loadhere.myPalette.r[i + startIndex] = (byte)bigDataFile->readByte();
		loadhere.myPalette.g[i + startIndex] = (byte)bigDataFile->readByte();
		loadhere.myPalette.b[i + startIndex] = (byte)bigDataFile->readByte();
		loadhere.myPalette.pal[i + startIndex] =
				(uint16)g_sludge->getOrigPixelFormat()->RGBToColor(
						loadhere.myPalette.r[i + startIndex],
						loadhere.myPalette.g[i + startIndex],
						loadhere.myPalette.b[i + startIndex]);
	}
	loadhere.myPalette.originalRed = loadhere.myPalette.originalGreen = loadhere.myPalette.originalBlue = 255;

	// convert
	for (int i = 0; i < total; i++) {
		int fromhere = 0;
		int transColour = -1;
		int size = loadhere.sprites[i].surface.w * loadhere.sprites[i].surface.h;
		while (fromhere < size) {
			byte s = spriteData[i][fromhere++];
			if (s) {
				transColour = s;
				break;
			}
		}
		fromhere = 0;
		for (int y = 0; y < loadhere.sprites[i].surface.h; y++) {
			for (int x = 0; x < loadhere.sprites[i].surface.w; x++) {
				byte *target = (byte *)loadhere.sprites[i].surface.getBasePtr(x, y);
				byte s = spriteData[i][fromhere++];
				if (s) {
					target[0] = (byte)255;
					target[1] = (byte)loadhere.myPalette.b[s];
					target[2] = (byte)loadhere.myPalette.g[s];
					target[3] = (byte)loadhere.myPalette.r[s];
					transColour = s;
				} else if (transColour >= 0) {
					target[0] = (byte)0;
					target[1] = (byte)loadhere.myPalette.b[transColour];
					target[2] = (byte)loadhere.myPalette.g[transColour];
					target[3] = (byte)loadhere.myPalette.r[transColour];
				}
				if (isFont) {
					target = (byte *)loadhere.sprites[i].burnSurface.getBasePtr(x, y);
					if (s)
						target[0] = loadhere.myPalette.r[s];
					target[1] = (byte)255;
					target[2] = (byte)255;
					target[3] = (byte)255;
				}
			}
		}
		delete[] spriteData[i];
	}
	delete[] spriteData;
	spriteData = NULL;

	finishAccess();

	setResourceForFatal(-1);

	return true;
}

// pasteSpriteToBackDrop uses the colour specified by the setPasteColour (or setPasteColor)
void pasteSpriteToBackDrop(int x1, int y1, sprite &single, const spritePalette &fontPal) {
	//TODO: shader: useLightTexture
	x1 -= single.xhot;
	y1 -= single.yhot;
	Graphics::TransparentSurface tmp(single.surface, false);
	tmp.blit(backdropSurface, x1, y1, Graphics::FLIP_NONE, nullptr,
			TS_RGB(fontPal.originalRed, fontPal.originalGreen, fontPal.originalBlue));
}

// burnSpriteToBackDrop adds text in the colour specified by setBurnColour
// using the differing brightness levels of the font to achieve an anti-aliasing effect.
void burnSpriteToBackDrop(int x1, int y1, sprite &single, const spritePalette &fontPal) {
	//TODO: shader: useLightTexture
	x1 -= single.xhot;
	y1 -= single.yhot - 1;
	Graphics::TransparentSurface tmp(single.surface, false);
	tmp.blit(backdropSurface, x1, y1, Graphics::FLIP_NONE, nullptr,
			TS_RGB(currentBurnR, currentBurnG, currentBurnB));
}

void fontSprite(bool flip, int x, int y, sprite &single, const spritePalette &fontPal) {
	float x1 = (float)x - (float)single.xhot / cameraZoom;
	float y1 = (float)y - (float)single.yhot / cameraZoom;

	// Use Transparent surface to scale and blit
	Graphics::TransparentSurface tmp(single.surface, false);
	tmp.blit(renderSurface, x1, y1, (flip ? Graphics::FLIP_H : Graphics::FLIP_NONE), 0, TS_RGB(fontPal.originalRed, fontPal.originalGreen, fontPal.originalBlue));

	if (single.burnSurface.getPixels() != nullptr) {
		Graphics::TransparentSurface tmp2(single.burnSurface, false);
		tmp2.blit(renderSurface, x1, y1, (flip ? Graphics::FLIP_H : Graphics::FLIP_NONE), 0, TS_RGB(fontPal.originalRed, fontPal.originalGreen, fontPal.originalBlue));

	}
}

void fontSprite(int x, int y, sprite &single, const spritePalette &fontPal) {
	fontSprite(false, x, y, single, fontPal);
}

void flipFontSprite(int x, int y, sprite &single, const spritePalette &fontPal) {
	fontSprite(true, x, y, single, fontPal);
}

byte curLight[3];

uint32 getDrawColor(onScreenPerson *thisPerson) {
//TODO: how to mix secondary color
#if 0
	if (thisPerson->colourmix) {
		//glEnable(GL_COLOR_SUM); FIXME: replace line?
		setSecondaryColor(curLight[0]*thisPerson->r * thisPerson->colourmix / 65025 / 255.f, curLight[1]*thisPerson->g * thisPerson->colourmix / 65025 / 255.f, curLight[2]*thisPerson->b * thisPerson->colourmix / 65025 / 255.f, 1.0f);
	}
#endif

	return TS_ARGB((uint8)(255 - thisPerson->transparency),
			(uint8)(curLight[0] * (255 - thisPerson->colourmix) / 255.f),
			(uint8)(curLight[1] * (255 - thisPerson->colourmix) / 255.f),
			(uint8)(curLight[2] * (255 - thisPerson->colourmix) / 255.f));
}

bool scaleSprite(sprite &single, const spritePalette &fontPal, onScreenPerson *thisPerson, bool mirror) {
	float x = thisPerson->x;
	float y = thisPerson->y;

	float scale = thisPerson->scale;
	bool light = !(thisPerson->extra & EXTRA_NOLITE);

	if (scale <= 0.05)
		return false;

	int diffX = (int)(((float)single.surface.w) * scale);
	int diffY = (int)(((float)single.surface.h) * scale);

	float x1, y1, x2, y2;

	if (thisPerson->extra & EXTRA_FIXTOSCREEN) {
		x = x / cameraZoom;
		y = y / cameraZoom;
		if (single.xhot < 0)
			x1 = x - (int)((mirror ? (float)(single.surface.w - single.xhot) : (float)(single.xhot + 1)) * scale / cameraZoom);
		else
			x1 = x - (int)((mirror ? (float)(single.surface.w - (single.xhot + 1)) : (float)single.xhot) * scale / cameraZoom);
		y1 = y - (int)((single.yhot - thisPerson->floaty) * scale / cameraZoom);
		x2 = x1 + (int)(diffX / cameraZoom);
		y2 = y1 + (int)(diffY / cameraZoom);
	} else {
		x -= cameraX;
		y -= cameraY;
		if (single.xhot < 0)
			x1 = x - (int)((mirror ? (float)(single.surface.w - single.xhot) : (float)(single.xhot + 1)) * scale);
		else
			x1 = x - (int)((mirror ? (float)(single.surface.w - (single.xhot + 1)) : (float)single.xhot) * scale);
		y1 = y - (int)((single.yhot - thisPerson->floaty) * scale);
		x2 = x1 + diffX;
		y2 = y1 + diffY;
	}

	// set light map color
	if (light && lightMap.getPixels()) {
		if (lightMapMode == LIGHTMAPMODE_HOTSPOT) {
			int lx = x + cameraX;
			int ly = y + cameraY;
			if (lx < 0 || ly < 0 || lx >= (int)sceneWidth || ly >= (int)sceneHeight) {
				curLight[0] = curLight[1] = curLight[2] = 255;
			} else {
				byte *target = (byte *)lightMap.getBasePtr(lx, ly);
				curLight[0] = target[3];
				curLight[1] = target[2];
				curLight[2] = target[1];
			}
		} else if (lightMapMode == LIGHTMAPMODE_PIXEL) {
			curLight[0] = curLight[1] = curLight[2] = 255;
		}
	} else {
		curLight[0] = curLight[1] = curLight[2] = 255;
	}

	// Use Transparent surface to scale and blit
	uint32 spriteColor = getDrawColor(thisPerson);
	if (!zBuffer.numPanels) {
		Graphics::TransparentSurface tmp(single.surface, false);
		tmp.blit(renderSurface, x1, y1, (mirror ? Graphics::FLIP_H : Graphics::FLIP_NONE), nullptr, spriteColor, diffX, diffY);
	} else {
		int d = ((!(thisPerson->extra & EXTRA_NOZB)) && zBuffer.numPanels) ? y + cameraY : sceneHeight + 1;
		addSpriteDepth(&single.surface, d, x1, y1, (mirror ? Graphics::FLIP_H : Graphics::FLIP_NONE), diffX, diffY, spriteColor);
	}

	// Are we pointing at the sprite?
	if (input.mouseX >= x1 && input.mouseX <= x2 && input.mouseY >= y1 && input.mouseY <= y2) {
		if (thisPerson->extra & EXTRA_RECTANGULAR)
			return true;

		// check if point to non transparent part
		int pixelx = (int)(single.surface.w * (input.mouseX - x1) / (x2 - x1));
		int pixely = (int)(single.surface.h * (input.mouseY - y1) / (y2 - y1));
		uint32 *colorPtr = (uint32 *)single.surface.getBasePtr(pixelx, pixely);

		uint8 a, r, g, b;
		g_sludge->getScreenPixelFormat()->colorToARGB(*colorPtr, a, r, g, b);
		return a != 0;
	}
	return false;
}

void resetSpriteLayers(zBufferData *pz, int x, int y, bool upsidedown) {
	if (spriteLayers.numLayers > 0)
		killSpriteLayers();
	spriteLayers.numLayers = pz->numPanels;
	for (int i = 0; i < spriteLayers.numLayers; ++i) {
		SpriteDisplay node(x, y, (upsidedown ? Graphics::FLIP_V : Graphics::FLIP_NONE), &pz->sprites[i], pz->sprites[i].w, pz->sprites[i].h);
		spriteLayers.layer[i].push_back(node);
	}
}

void addSpriteDepth(Graphics::Surface *ptr, int depth, int x, int y, Graphics::FLIP_FLAGS flip, int width, int height, uint32 color) {
	int i;
	for (i = 1; i < zBuffer.numPanels; ++i) {
		if (zBuffer.panel[i] >= depth) {
			break;
		}
	}
	--i;
	SpriteDisplay node(x, y, flip, ptr, width, height, color);
	spriteLayers.layer[i].push_back(node);
}

void displaySpriteLayers() {
	for (int i = 0; i < spriteLayers.numLayers; ++i) {
		Common::List<SpriteDisplay>::iterator it;
		for (it = spriteLayers.layer[i].begin(); it != spriteLayers.layer[i].end(); ++it) {
			Graphics::TransparentSurface tmp(*it->surface, false);
			tmp.blit(renderSurface, it->x, it->y, it->flip, nullptr, it->color, it->width, it->height);
		}
	}
	killSpriteLayers();
}

void killSpriteLayers() {
	for (int i = 0; i < spriteLayers.numLayers; ++i) {
		spriteLayers.layer[i].clear();
	}
	spriteLayers.numLayers = 0;
}

// Paste a scaled sprite onto the backdrop
void fixScaleSprite(int x, int y, sprite &single, const spritePalette &fontPal, onScreenPerson *thisPerson, int camX, int camY, bool mirror) {

	float scale = thisPerson->scale;
	bool useZB = !(thisPerson->extra & EXTRA_NOZB);
	bool light = !(thisPerson->extra & EXTRA_NOLITE);

	if (scale <= 0.05)
		return;

	int diffX = (int)(((float)single.surface.w) * scale);
	int diffY = (int)(((float)single.surface.h) * scale);
	int x1;
	if (single.xhot < 0)
		x1 = x - (int)((mirror ? (float)(single.surface.w - single.xhot) : (float)(single.xhot + 1)) * scale);
	else
		x1 = x - (int)((mirror ? (float)(single.surface.w - (single.xhot + 1)) : (float)single.xhot) * scale);
	int y1 = y - (int)((single.yhot - thisPerson->floaty) * scale);

	// set light map color
	if (light && lightMap.getPixels()) {
		if (lightMapMode == LIGHTMAPMODE_HOTSPOT) {
			int lx = x + cameraX;
			int ly = y + cameraY;
			if (lx < 0 || ly < 0 || lx >= (int)sceneWidth || ly >= (int)sceneHeight) {
				curLight[0] = curLight[1] = curLight[2] = 255;
			} else {
				byte *target = (byte *)lightMap.getBasePtr(lx, ly);
				curLight[0] = target[3];
				curLight[1] = target[2];
				curLight[2] = target[1];
			}
		} else if (lightMapMode == LIGHTMAPMODE_PIXEL) {
			curLight[0] = curLight[1] = curLight[2] = 255;
		}
	} else {
		curLight[0] = curLight[1] = curLight[2] = 255;
	}

	// draw backdrop
	drawBackDrop();

	// draw zBuffer
	if (zBuffer.numPanels) {
		drawZBuffer((int)(x1 + camX), (int)(y1 + camY), false);
	}

	// draw sprite
	uint32 spriteColor = getDrawColor(thisPerson);
	if (!zBuffer.numPanels) {
		Graphics::TransparentSurface tmp(single.surface, false);
		tmp.blit(renderSurface, x1, y1, (mirror ? Graphics::FLIP_H : Graphics::FLIP_NONE), nullptr, spriteColor, diffX, diffY);
	} else {
		int d = useZB ? y + cameraY : sceneHeight + 1;
		addSpriteDepth(&single.surface, d, x1, y1, (mirror ? Graphics::FLIP_H : Graphics::FLIP_NONE), diffX, diffY, spriteColor);
	}

	// draw all
	displaySpriteLayers();

	// copy screen to backdrop
	backdropSurface.copyFrom(renderSurface);

	// reset zBuffer with the new backdrop
	if (zBuffer.numPanels) {
		setZBuffer(zBuffer.originalNum);
	}
}

} // End of namespace Sludge
