# Falconnect

A work-in-progress fork of the Dolphin emulator, exclusively for use with F-Zero GX, that adds online
multiplayer functionality.

## Status
I am currently porting the [prototype code I made in Python](https://github.com/mck-9061/Falconnect-prototype) to C++, so it can run internally in Dolphin
with no external tools and with multi-platform support.

## System Requirements

### Desktop

* OS
    * Windows (10 1903 or higher).
    * macOS (26.0 Tahoe or higher).
* Processor
    * A CPU with SSE2 support.
    * A modern CPU (3 GHz and Dual Core, not older than 2008) is highly recommended.
* Graphics
    * A reasonably modern graphics card (Direct3D 11.1 / OpenGL 3.3).
    * A graphics card that supports Direct3D 11.1 / OpenGL 4.4 is recommended.

Falconnect is being tested on Windows (x86) and macOS (Apple Silicon) - other operating systems
and CPU architectures will likely work but haven't been tested.

Work in progress docs for the reverse engineering involved can be found at https://falconnect.net/.

## With thanks to
- Ghidra, [Ghidra-GameCube-Loader](https://github.com/Cuyler36/Ghidra-GameCube-Loader), and Dolphin's debugging tools - reverse engineering and debugging
- [fzerogx-docs](https://github.com/JoselleAstrid/fzerogx-docs) - amazing work documenting internal memory structures and addresses
- F-Zero Nexus Discord - loads of modding information and documentation

