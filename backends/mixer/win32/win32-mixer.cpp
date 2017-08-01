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
#include <Windows.h>

#include <array>
#include <cassert>

#include "win32-mixer.h"

// check for winmm errors
#define MMOK(EXP) ((EXP) == MMSYSERR_NOERROR)

enum {
	PW_OK,
	PW_ALREADY_OPEN,
	PW_WAVEINFO_ERROR,
	PW_THREAD_ABORT,
	PW_WAVEOUTOPEN_ERROR,
	PW_CREATETHREAD_ERROR,
	PW_CREATEEVENT_ERROR,
	PW_WAVEOUTCLOSE_ERROR,
	PW_WAVEOUTWRITE_ERROR,
	PW_WAVEOUTPREPHDR_ERROR,
	PW_CLOSEHANDLE_ERROR,
};

typedef void (*WaveProc)(void *buffer,		// audio buffer data pointer
						 size_t bufferSize, // audio buffer size in bytes
						 void *user			// opaque user data pointer
);

struct WaveInfo {
	uint32_t sampleRate; // sample rate in hz  (44100, 22050, ...)
	uint32_t bitDepth;   // bit depth in bits  (16, 8)
	uint32_t channels;   // number of channels (2, 1)
	uint32_t bufferSize; // audio buffer size in bytes
	WaveProc callback;   // audio rendering callback function
	void *callbackData;  // user data passed to callback
};

struct PicoWave {

	PicoWave()
		: _hwo(NULL)
		, _alive(0)
		, _waveEvent(NULL)
		, _waveThread(NULL)
		, _rawAlloc(NULL)
		, _error(PW_OK) {
		memset(&_wavehdr, 0, sizeof(_wavehdr));
		memset(&_info, 0, sizeof(_info));
	}

	~PicoWave() { close(); }

	bool open(const WaveInfo &info);
	bool start();
	bool pause();
	bool close();

	uint32_t lastError() const { return _error; }

protected:
	bool _prepare();
	bool _validate(const WaveInfo &info);

	static DWORD WINAPI _threadProc(LPVOID param);

	// internal wave info
	std::array<WAVEHDR, 4> _wavehdr;
	HWAVEOUT _hwo;
	LONG volatile _alive;
	HANDLE _waveEvent;
	HANDLE _waveThread;
	// allocation used for all buffers
	uint8_t *_rawAlloc;
	// user supplied info
	WaveInfo _info;
	// error code
	uint32_t _error;
};

namespace {
uintptr_t alignPtr(uintptr_t ptr, const uintptr_t align) {
	const uintptr_t offset = align - 1;
	const uintptr_t mask = ~(align - 1);
	return (ptr + offset) & mask;
}

bool isPowerOfTwo(size_t in) { return 0 == (in & (in - 1)); }
} // namespace

bool PicoWave::_prepare() {
	assert(_hwo);
	// 128 bits of alignment
	const size_t alignment = 16;
	// full number of samples required
	const size_t numSamples = _info.bufferSize * _info.channels;
	// full buffer amount requested in bytes
	const size_t numBytes = numSamples * _info.bitDepth / 8;
	// allocate with room for alignment
	_rawAlloc = new uint8_t[numBytes + alignment];
	// align the allocation
	uint8_t *ptr = (uint8_t *)alignPtr((uintptr_t)_rawAlloc, alignment);
	memset(ptr, 0, numBytes);
	// number of samples for each waveheader
	const size_t hdrSamples = numBytes / _wavehdr.size();

	for (WAVEHDR &hdr : _wavehdr) {
		// check alignment holds
		assert(0 == ((uintptr_t)ptr & (alignment - 1)));
		// allocate the wave header object
		memset(&hdr, 0, sizeof(hdr));
		hdr.lpData = (LPSTR)ptr;
		hdr.dwBufferLength = hdrSamples;
		// prepare the header for the device
		if (!MMOK(waveOutPrepareHeader(_hwo, &hdr, sizeof(hdr)))) {
			_error = PW_WAVEOUTPREPHDR_ERROR;
			return false;
		}
		// write the buffer to the device
		if (!MMOK(waveOutWrite(_hwo, &hdr, sizeof(hdr)))) {
			_error = PW_WAVEOUTWRITE_ERROR;
			return false;
		}
		// next chunk of samples for the next waveheader
		ptr += hdrSamples;
	}
	return true;
}

DWORD WINAPI PicoWave::_threadProc(LPVOID param) {
	assert(param);
	PicoWave &self = *(PicoWave *)param;
	assert(self._hwo);
	// loop while this thread is alive
	while (self._alive) {
		// wait for a wave event
		const DWORD ret = WaitForSingleObject(self._waveEvent, INFINITE);
		// poll waveheaders for a free block
		for (WAVEHDR &hdr : self._wavehdr) {
			if ((hdr.dwFlags & WHDR_DONE) == 0) {
				// buffer is not free for use
				continue;
			}
			if (!MMOK(waveOutUnprepareHeader(self._hwo, &hdr, sizeof(hdr)))) {
				return 1;
			}
			// call user function to fill with new self
			WaveProc callback = self._info.callback;
			if (callback) {
				void *cbData = self._info.callbackData;
				callback(hdr.lpData, hdr.dwBufferLength, cbData);
			}
			if (!MMOK(waveOutPrepareHeader(self._hwo, &hdr, sizeof(hdr)))) {
				return 1;
			}
			if (!MMOK(waveOutWrite(self._hwo, &hdr, sizeof(WAVEHDR)))) {
				return 1;
			}
		}
	}
	return 0;
}

bool PicoWave::_validate(const WaveInfo &info) {
	if (!isPowerOfTwo(info.bufferSize)) {
		return false;
	}
	if (info.callback == nullptr) {
		return false;
	}
	if (info.bitDepth != 16 || info.bitDepth != 8) {
		return false;
	}
	switch (info.sampleRate) {
	case 44100:
	case 22050:
	case 11025:
		break;
	default:
		return false;
	}
	if (info.channels != 1 && info.channels != 2) {
		return false;
	}
	return true;
}

bool PicoWave::open(const WaveInfo &info) {
	// check if already running
	if (_hwo || _waveThread || _waveEvent) {
		_error = PW_ALREADY_OPEN;
		return false;
	}
	if (_validate(info)) {
		_error = PW_WAVEINFO_ERROR;
		return false;
	}
	// mark callback thread as alive
	InterlockedExchange(&_alive, 1);
	// copy wave info structure to internal data for reference
	_info = info;
	// create waitable wave event
	_waveEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (_waveEvent == NULL) {
		_error = PW_CREATEEVENT_ERROR;
		return false;
	}
	// prepare output wave format
	WAVEFORMATEX waveformat;
	memset(&waveformat, 0, sizeof(waveformat));
	waveformat.cbSize = 0;
	waveformat.wFormatTag = WAVE_FORMAT_PCM;
	waveformat.nChannels = info.channels;
	waveformat.nSamplesPerSec = info.sampleRate;
	waveformat.wBitsPerSample = info.bitDepth;
	waveformat.nBlockAlign = (info.channels * waveformat.wBitsPerSample) / 8;
	waveformat.nAvgBytesPerSec = info.sampleRate * waveformat.nBlockAlign;
	// create wave output
	memset(&_hwo, 0, sizeof(_hwo));
	if (!MMOK(waveOutOpen(&_hwo, WAVE_MAPPER, &waveformat,
						  (DWORD_PTR)_waveEvent, NULL, CALLBACK_EVENT))) {
		_error = PW_WAVEOUTOPEN_ERROR;
		return false;
	}
	// create the wave thread
	_waveThread = CreateThread(NULL, 0, _threadProc, this, CREATE_SUSPENDED, 0);
	if (_waveThread == NULL) {
		_error = PW_CREATETHREAD_ERROR;
		return false;
	}
	// prepare waveout for playback
	return _prepare();
}

bool PicoWave::close() {
	InterlockedExchange(&_alive, 0);
	if (_waveThread) {
		bool hardKill = true;
		const DWORD timeout = 1000;
		if (WaitForSingleObject(_waveThread, timeout)) {
			// thread join failed :'(
		}
		DWORD exitCode = 0;
		if (GetExitCodeThread(_waveThread, &exitCode)) {
			if (exitCode != STILL_ACTIVE) {
				hardKill = false;
			}
		}
		if (hardKill) {
			_error = PW_THREAD_ABORT;
			// note: hard kills are not good as they could leave the client process
			//       in an inconsistent state
			if (TerminateThread(_waveThread, 0) == FALSE) {
				// XXX: how to handle a thread that wont die?
			}
		}
		_waveThread = NULL;
	}
	if (_hwo) {
		int maxTries = 100;
		while (!MMOK(waveOutClose(_hwo))) {
			Sleep(100);
			if (--maxTries == 0) {
				_error = PW_WAVEOUTCLOSE_ERROR;
				break;
			}
		}
		_hwo = NULL;
	}
	if (_waveEvent) {
		if (CloseHandle(_waveEvent) == FALSE) {
			_error = PW_CLOSEHANDLE_ERROR;
		}
		_waveEvent = NULL;
	}
	if (_rawAlloc) {
		delete[] _rawAlloc;
		_rawAlloc = NULL;
	}
	memset(&_wavehdr, 0, sizeof(_wavehdr));
	memset(&_info, 0, sizeof(_info));
	return true;
}

bool PicoWave::start() {
	if (!_waveThread) {
		return false;
	}
	ResumeThread(_waveThread);
	return true;
}

bool PicoWave::pause() {
	if (!_waveThread) {
		return false;
	}
	SuspendThread(_waveThread);
	return true;
}

Win32MixerManager::Win32MixerManager()
	: _mixer(NULL)
	, _picoWave(*(new PicoWave)) {
	assert(&_picoWave);
}

Win32MixerManager::~Win32MixerManager() {
	assert(&_picoWave);
	delete &_picoWave;
}

void Win32MixerManager::audioProc(void *buffer, size_t bufferSize, void *user) {
	assert(user);
	Win32MixerManager &self = *(Win32MixerManager *)user;
	int16 *output = (int16 *)buffer;
	uint bailout = 100;
	while (bufferSize && --bailout) {
		const size_t numSamples =
			self._mixer->mixCallback((byte *)output, bufferSize);
		const size_t numBytes = numSamples * sizeof(uint);
		assert(numBytes <= bufferSize);
		output += numBytes;
		bufferSize -= numBytes;
	}
}

void Win32MixerManager::init(Audio::MixerImpl *mixer) {
	assert(mixer);
	const uint sampleRate = mixer->getOutputRate();
	const WaveInfo info = {
		sampleRate,                   // sample rate
		16,                           // bit depth
		2,                            // channels
		1024 * 4,                     // buffer size
		Win32MixerManager::audioProc, // callback
		this                          // callback user data
	};
	if (!_picoWave.open(info)) {
		return;
	}
	// open new mixer device
	_mixer = mixer;
	_mixer->setReady(true);
	_picoWave.start();
}

void Win32MixerManager::shutDown() {
	_picoWave.close();
}
