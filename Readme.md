# Falconnect

A work-in-progress fork of the Dolphin emulator, exclusively for use with F-Zero GX, that adds online
multiplayer functionality.

## Status
Basic core multiplayer with up to 30 players is working. After I've finished testing and done some UI and logic flow work to make
it usable, I'll open up alpha testing. No exact timeframe on this as I'm currently moving house.

## Planned features
- Full 30 player online multiplayer races
- Human and CPU players in multiplayer
- Many more race options than vanilla
  - Unlocked lap count
  - Choose lap on which boost power is granted
  - Story mode tracks
  - Restrict usable machines (e.g. only mid-tier machines, Space Angler only, randomised)
  - Restrict machine settings (e.g. snaking only or max speed only)
  - Choose stats (e.g. vanilla, Unleashed, GXtreme, custom)
- Online Grands Prix

## Ideas under consideration
- Ranking system
- Expanded offline practice mode
- Custom track support

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

Falconnect is being tested on Windows 11 (x86) and macOS Golden Gate (Apple Silicon) - other operating systems
and CPU architectures will likely work but haven't been tested.

Work in progress docs for the reverse engineering involved can be found at https://falconnect.net/.

## With thanks to
- Ghidra, [Ghidra-GameCube-Loader](https://github.com/Cuyler36/Ghidra-GameCube-Loader), and Dolphin's debugging tools - reverse engineering and debugging
- [fzerogx-docs](https://github.com/JoselleAstrid/fzerogx-docs) - amazing work documenting internal memory structures and addresses
- F-Zero Nexus Discord - loads of modding information and documentation

