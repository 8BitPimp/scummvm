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
#ifndef SLUDGE_TALK_H
#define SLUDGE_TALK_H

#include "sludge/sprites.h"

namespace Sludge {

struct speechLine {
	Common::String textLine;
	speechLine *next;
	int x;
};

struct speechStruct {
	onScreenPerson *currentTalker;
	speechLine *allSpeech;
	int speechY, lastFile, lookWhosTalking;
	spritePalette talkCol;
};

int wrapSpeech(const Common::String &theText, int objT, int sampleFile, bool);
void viewSpeech();
void killAllSpeech();
int isThereAnySpeechGoingOn();
void initSpeech();
void saveSpeech(speechStruct *sS, Common::WriteStream *stream);
bool loadSpeech(speechStruct *sS, Common::SeekableReadStream *stream);

} // End of namespace Sludge

#endif
