# OpenGL 4.6 Renderer Library for Yamagi Quake II

This is an OpenGL 4.6 renderer library for Yamagi Quake II.

## Compilation

You'll need:
* clang or gcc,
* GNU Make,
* SDL2 with `sdl2-config`,
* opengl-headers
* libpthread (unix-like systems only)

Type `make` to compile the library. If the compilation is successfull,
the library can be found under *release/ref_gl4.dll* (Windows) or
*release/ref_gl4.so* (unixoid systems).

## Usage

Copy the library next to your Yamagi Quake II executable. You can select
OpenGL 4.6 through the video menu.

## Why Should I Use This?

If your hardware supports it, then generally, the updated and more efficient OpenGL calls will provide much better performance.
Additionally, the GL4 renderer has several enhancements.

* Multithreaded rendering over rendering images and particles
* Optimised calls, structure and buffer enhancements
* Shader UBO efficiency enhancements

Generally, you'll find the performance over the last version is around 100% better due to these changes.  There will be no stutters, micro-stutters or frame drops of any kind.
I tested it on a 10 year old mid-level laptop and still got over 250fps whereas before it used to dip down to the 50's with explosions and lots of particles.

Below are examples:

Before:
<img width="1270" height="956" alt="Screenshot 2025-08-14 002602" src="https://github.com/user-attachments/assets/c3f45e2a-b6af-46c3-84d1-fdf06cbe8c92" />

After:
<img width="1275" height="954" alt="Screenshot 2025-08-14 002703" src="https://github.com/user-attachments/assets/9ac9fa45-5dc5-4ef0-ae4e-2df06a741897" />

If you have run into issues, please attach output logs with OS/driver version
and device name to the bug report. [List](https://openbenchmarking.org/test/pts/yquake2)
of currently tested devices for the reference.
