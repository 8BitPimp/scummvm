#if defined(WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fmopl.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/textconsole.h"
#include "common/str.h"

// compile with inpout driver support for port access on windows NT and above
#define USE_INPOUT

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

struct OPLDriver {

	OPLDriver(uint type, uint port)
		: _oplType(type)
		, _oplRegMask((type == OPL::Config::kFlagOpl2) ? 0xff : 0x1ff)
		, _oplPort(port)
	{
	}

	virtual ~OPLDriver() {};
	virtual void mute() = 0;
	virtual void write(uint reg, uint val) = 0;
	virtual byte read(uint reg) = 0;
protected:
	const uint _oplType;
	const uint _oplRegMask;
	const uint _oplPort;
	// register cache
	byte _reg[512];
};

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// x86_64 visual studio disabled inline assembly
#if defined(_MSC_VER) && !defined(_WIN64)

struct OPLDriverASM : public OPLDriver {

	OPLDriverASM(uint oplType, uint port)
		: OPLDriver(oplType, port)
	{}

	virtual void mute();
	virtual void write(uint reg, uint val);
	virtual byte read(uint reg);

	// TODO: cache OPL registers for fast reading without bus delays
protected:
	static void _portWrite(uint port, uint data);
	static void _portDelay(uint base, uint cycles);
};

void OPLDriverASM::mute() {
	for (uint i=0; i<0xff; ++i) {
		write(i, 0);
	}
}

void OPLDriverASM::write(uint reg, uint val) {
	reg &= _oplRegMask;
	val &= 0xff;
	// delay timings come from:
	//   'SoundBlaster Series - Hardware Programming Guide'
	_portWrite(_oplPort + 0x8, reg);
	_portDelay(_oplPort + 0x7, 6);
	_portWrite(_oplPort + 0x9, val);
	_portDelay(_oplPort + 0x7, 36);
	// cache this write
	_reg[reg] = val;
}

byte OPLDriverASM::read(uint reg) {
	return _reg[reg & _oplRegMask];
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
void OPLDriverASM::_portDelay(uint base, uint cycles) {
	const uint port = base;
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

static OPLDriver * createOPLDriverASM(uint oplType, uint port) {
	_OSVERSIONINFOA vers;
	ZeroMemory(&vers, sizeof(vers));
	vers.dwOSVersionInfoSize = sizeof(vers);
	// get windows version info
	if (GetVersionExA(&vers) == FALSE) {
		return NULL;
	}
	// check for Windows 95/98/Me since port instructions are privelaged for
	// WinNT and above and cant be used.
	if (vers.dwPlatformId == 1 && vers.dwMajorVersion == 4) {
		return new OPLDriverASM(oplType, port);
	}
	return NULL;
}
#else
static OPLDriver * createOPLDriverASM(uint, uint) {
	// dummy; no inline port access compiled
	return NULL;
}
#endif // defined(_MSC_VER) && !defined(_WIN64)

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

#if defined(USE_INPOUT)

typedef void (_stdcall *InpoutOut32)(short PortAddress, short data);
typedef short (_stdcall *InpoutInp32)(short PortAddress);
typedef BOOL (_stdcall *InpoutIsDriverOpen)();

struct OPLDriverINPOUT : public OPLDriver {

	OPLDriverINPOUT(uint oplType, uint port)
		: OPLDriver(oplType, port)
	{}

	virtual void mute();
	virtual void write(uint reg, uint val);
	virtual byte read(uint reg);

	HMODULE _inpout;
	InpoutInp32 _inp32;
	InpoutOut32 _out32;
};

void OPLDriverINPOUT::mute() {
	assert(_out32);
}

void OPLDriverINPOUT::write(uint reg, uint val) {
	assert(_out32 && _inp32);
	reg &= _oplRegMask;
	_reg[reg] = val;
	_out32(_oplPort + 0x8, reg);
	// delay
	for (int i=0; i<6; ++i) {
		_inp32(_oplPort + 0x7);
	}
	_out32(_oplPort + 0x9, val);
	// delay
	for (int i=0; i<36; ++i) {
		_inp32(_oplPort + 0x7);
	}
}

byte OPLDriverINPOUT::read(uint reg) {
	reg &= _oplRegMask;
	return _reg[reg];
}

OPLDriver *CreateOPLDriverINPOUT(uint oplType, uint port) {
	struct OPLDriverINPOUT *oplDriver = new OPLDriverINPOUT(oplType, port);
	// load inpout driver userspace library
	HMODULE inpout = LoadLibraryA("inpout32.dll");
	oplDriver->_inpout = inpout;
	if (inpout == NULL) {
		return (delete oplDriver), NULL;
	}
	// access INPOUT driver functions
	oplDriver->_inp32 =
		(InpoutInp32)GetProcAddress(inpout, "Inp32");
	oplDriver->_out32 =
		(InpoutOut32)GetProcAddress(inpout, "Out32");
	InpoutIsDriverOpen isInpOutDriverOpen =
		(InpoutIsDriverOpen)GetProcAddress(inpout, "IsInpOutDriverOpen");
	// check if loading was a success
	if (oplDriver->_inp32 && oplDriver->_out32 && isInpOutDriverOpen) {
		if (isInpOutDriverOpen() != FALSE) {
			return oplDriver;
		}
	}
	// error; unable to open driver or access export functions
	return (delete oplDriver), NULL;
}
#else
OPLDriver * CreateOPLDriverINPOUT(uint, uint) {
	// dummy; no inpout support compiled
	return NULL;
}
#endif // defined(USE_INPOUT)

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

class Win32OPL : public OPL::RealOPL {
public:
	Win32OPL(uint type);
	virtual ~Win32OPL();

	// OPL interface
	virtual bool init();
	virtual void reset();
	virtual void write(int a, int v);
	virtual byte read(int a);
	virtual void writeReg(int r, int v);
protected:

	OPLDriver *_driver;
	const uint _oplType;
};

Win32OPL::Win32OPL(uint type)
	: RealOPL()
	, _driver(NULL)
	, _oplType(type)
{
}

Win32OPL::~Win32OPL() {
	if (_driver) {
		delete _driver;
		_driver = NULL;
	}
}

bool Win32OPL::init() {
	// default OPL port
	uint port = true ? 0x220 : 0x380;
	// load OPL port from config
	const char *key = "opl_port";
	Common::ConfigManager::Domain *dom = ConfMan.getDomain("scummvm");
	if (dom) {
		const Common::String confPort = dom->getVal(key);
		if (!confPort.empty()) {
			port = strtol(confPort.c_str(), NULL, 16);
		}
		else {
			warning("'%s' not set in config; using default of %x", key, port);
		}
	}
	debug("Win32OPL using port '%s'=%x", key, port);
	// try to create a direct assembly driver
	_driver = createOPLDriverASM(_oplType, port);
	if (_driver) {
		debug("using OPLDriverASM");
		_driver->mute();
		return true;
	}
	// try to create an INPOUT driver
	_driver = CreateOPLDriverINPOUT(_oplType, port);
	if (_driver) {
		debug("using OPLDriverINPOUT");
		_driver->mute();
		return true;
	}
	// error; unable to access host ports
	warning("win32_opl device unable gain port access");
	return false;
}

void Win32OPL::reset() {
	if (_driver) {
		_driver->mute();
	}
}

void Win32OPL::write(int a, int v) {
	if (!_driver) {
		return;
	}
	// XXX: fixme (this is a horrible hack >:D)
	const uint port = min(
		uint16(a - 0x220) /* opl2 */,
		uint16(a - 0x380) /* opl3 */);
	_driver->write(port, v & 0xff);
}

byte Win32OPL::read(int a) {
	// XXX: fixme
	const uint port = min(
		uint16(a - 0x220) /* opl2 */,
		uint16(a - 0x380) /* opl3 */);
	return _driver ? _driver->read(port) : 0;
}

void Win32OPL::writeReg(int r, int v) {
	if (_driver) {
		_driver->write(r, v);
	}
}

OPL::OPL *CreateWin32OPL(uint type) {
	warning("CreateWin32OPL(%u)", type);
	return new Win32OPL(type);
}

#endif // defined(WIN32)
