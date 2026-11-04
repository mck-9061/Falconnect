# Falconnect

Thank you to those who have helped and supported this project.

A certain other GX netplay solution has been made available, the first version of which was vibecoded in a matter of days, using nothing
but AI to reverse engineer the game. 

In my efforts in creating Falconnect, AI was never used to reverse engineer, write PowerPC code, or design netcode. The project was driven by 
my own love for the game and interest in learning reverse engineering and the internals of GameCube games. Yet it has been lumped in
with the other project as if the use of AI in both is comparable.

It's fairly clear I can't keep up with the slopcoding doing this, and my motivation to spend my limited free time on this project
is very low when the community has taken to the other tool so much. So, I have decided to stop future development of Falconnect. 

I've really enjoyed working on this. Having never done any reverse engineering before, working on Falconnect has taught me so much,
not just about GX but about low-level programming and modding as a whole. It started with just wanting to see if I could get machines to show
up in another instance of the game using documentation of memory addresses put together by members of the community, and ended
with some fairly successful alpha tests with all sorts of heavily modded parts of the game that I had manually reverse engineered.

The server and client programs will remain available should you want to host private games.

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

## Notes

Falconnect, this repository, reverse engineering efforts, and developers, are not affiliated with Nintendo, Sega, 
Amusement Vision, or the original development staff of F-Zero GX. F-Zero is a trademark of Nintendo. Falconnect
contains none of the code, textures, audio, models, or any other copyrighted materials of F-Zero GX, and requires a
legally obtained copy of the original game to function.

## With thanks to
- Ghidra, [Ghidra-GameCube-Loader](https://github.com/Cuyler36/Ghidra-GameCube-Loader), and Dolphin's debugging tools - reverse engineering and debugging
- [fzerogx-docs](https://github.com/JoselleAstrid/fzerogx-docs) - amazing work documenting internal memory structures and addresses
- F-Zero Nexus Discord - loads of modding information and documentation

