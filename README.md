# ScummVM Win32 Backend

This is a work in progress ScummVM Win32 backend.
There are several goals for this backend:
- Remove SDL dependancy
- Use no external libraries
- Compile with Visual C++ 6.0
- Run on Windows 95
- Support all LEC games
- Use direct port writes for Adlib audio

Writing this backend is my way of learning more about ScummVM and getting started contributing to this open source project.
An additional benefit that comes from having a non SDL backend is that it will help isolate SDL only to the systems that really need it.


### Status
Much of the core is now in place, and as such already some LEC games are playable.
Graphics are implemented using GDI calls and audio via the winmm system.
The backend needs a lot of testing and additional features to bring it up to the standard of other ScummVM backends.

### Building

Requirements:
- CMake
- Visual Studio 2015

Use the CMakeLists.txt in the root directory.
Make sure that ENABLE_WIN32_BACKEND is enabled.


### Compatability

Games known to work with this backend:
- Monkey Island 1
- Monkey Island 2
- Indy 3
- Day of the Tentacle
- Sam and Max Demo
- Indy (Fate of atlantis) Demo

Games still to test:
- The Dig
- Full Throttle
- Curse of Monkey Island
