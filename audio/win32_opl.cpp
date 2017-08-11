#if defined(WIN32)

#include "fmopl.h"

struct OPLDriver {
	virtual ~OPLDriver() {};
	virtual void mute() = 0;
	virtual void write(uint reg, uint val) = 0;
	virtual byte read(uint reg) = 0;
};

// x86_64 visual studio disabled inline assembly
#if defined(_MSC_VER) && !defined(_WIN64)

struct OPLDriverASM : public OPLDriver {
	virtual void mute();
	virtual void write(uint reg, uint val);
	virtual byte read(uint reg);

	// TODO: cache OPL registers for fast reading without bus delays
protected:
	static void _portWrite(uint port, uint data);
	static void _portDelay(uint cycles);
};

// base port for soundblaster card
#define SBBASE 0x220

void OPLDriverASM::mute() {
	for (uint i=0; i<0xff; ++i) {
		_portWrite(SBBASE + 0x8, i);
		_portDelay(6);
		_portWrite(SBBASE + 0x9, 0);
		_portDelay(36);
	}
}

void OPLDriverASM::write(uint reg, uint val) {
	// delay timings come from:
	//   'SoundBlaster Series - Hardware Programming Guide'
	_portWrite(SBBASE + 0x8, reg);
	_portDelay(6);
	_portWrite(SBBASE + 0x9, val);
	_portDelay(36);
}

byte OPLDriverASM::read(uint reg) {
	return 0;
}

// write to the OPL chip on ISA bus
void OPLDriverASM::_portWrite(uint port, uint data) {
	__asm {
		push edx
		mov  edx, port
		mov  eax, data
		out  dx,  al
		pop  edx
	};
}

// delay using ISA port read cycles
void OPLDriverASM::_portDelay(uint cycles) {
	const uint port = SBBASE + 0x7; // whats at 0x7 again?
	__asm {
		push edx
		push ecx
		mov edx, port
		mov ecx, cycles
	top:
		in al, dx
		dec ecx
		test ecx, ecx
		jnz top
		pop ecx
		pop edx
	};
}

static OPLDriver * createOPLDriverASM() {
	// TODO: check windows version as PORT instructions are privelaged for WinNT and above
	return new OPLDriverASM;
}
#else
static OPLDriver * createOPLDriverASM() {
	return NULL;
}
#endif // defined(_MSC_VER) && !defined(_WIN64)

// TODO: add INPOUT driver
OPLDriver * CreateOPLDriverINPOUT() {
	return NULL;
}

class Win32OPL : public OPL::RealOPL {
public:
	Win32OPL();
	virtual ~Win32OPL();

	// OPL interface
	virtual bool init();
	virtual void reset();
	virtual void write(int a, int v);
	virtual byte read(int a);
	virtual void writeReg(int r, int v);
	virtual void setCallbackFrequency(int timerFrequency);
protected:

	OPLDriver *_driver;

	virtual void startCallbacks(int timerFrequency);
	virtual void stopCallbacks();
};

Win32OPL::Win32OPL()
	: _driver(NULL)
{
}

Win32OPL::~Win32OPL() {
	if (_driver) {
		delete _driver;
		_driver = NULL;
	}
}

bool Win32OPL::init() {
	// try to create a direct assembly driver
	_driver = createOPLDriverASM();
	if (_driver) {
		return true;
	}
	// try to create an INPOUT driver
	_driver = CreateOPLDriverINPOUT();
	if (_driver) {
		return true;
	}
	return false;
}

void Win32OPL::reset() {
	if (_driver) {
		_driver->mute();
	}
}

void Win32OPL::write(int a, int v) {
	if (_driver) {
		_driver->write(a, v);
	}
}

byte Win32OPL::read(int a) {
	return _driver ? _driver->read(a) : 0;
}

void Win32OPL::writeReg(int r, int v) {
	if (_driver) {
		_driver->write(r, v);
	}
}

void Win32OPL::setCallbackFrequency(int timerFrequency) {
}

void Win32OPL::startCallbacks(int timerFrequency) {
}

void Win32OPL::stopCallbacks() {
}

OPL::OPL *CreateWin32OPL(uint type) {
	return new Win32OPL;
}

#endif // defined(WIN32)
