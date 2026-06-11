# OpenGL 4.6 Renderer Library for Yamagi Quake II

This is an OpenGL 4.6 renderer library for Yamagi Quake II.

## Compilation

You'll need (use the buildenv package from yq2 compilation instructions):
* clang or gcc,
* GNU Make,
* SDL3 with `sdl3-config`,
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
Additionally, the GL4 renderer has several enhancements, both graphical and performance.

* Multithreaded rendering over rendering images and particles
* Optimised calls, structure and buffer enhancements
* Shader UBO efficiency enhancements
* Bloom lighting

As stated, the GL4 renderer is multithreaded and extremely well optimised against any form of micro stuttering.  More advanced features will be coming in the future also.

If you have run into issues, please attach output logs with OS/driver version
and device name to the bug report. [List](https://openbenchmarking.org/test/pts/yquake2)
of currently tested devices for the reference.
