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

#ifndef BACKENDS_MIXER_WIN32_H
#define BACKENDS_MIXER_WIN32_H

#include "audio/mixer_intern.h"

class Win32MixerManager {
public:

    Win32MixerManager();
    
    ~Win32MixerManager();

    virtual void init();

    virtual void shutDown();
    
    Audio::Mixer *getMixer() {
        return (Audio::Mixer *)_mixer;
    }

protected:
    // audio callback handler
    static void audioProc(void * buffer, size_t bufferSize, void *self);

    Audio::MixerImpl *_mixer;
    struct PicoWave &_picoWave;
};

#endif
