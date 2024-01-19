# OpenGL 4.6 Renderer Library for Yamagi Quake II

This is an OpenGL 4.6 renderer library for Yamagi Quake II.


## Compilation

You'll need:
* clang or gcc,
* GNU Make,
* SDL2 with `sdl2-config`,
* opengl-headers

Type `make` to compile the library. If the compilation is successfull,
the library can be found under *release/ref_gl4.dll* (Windows) or
*release/ref_gl4.so* (unixoid systems).

## Usage

Copy the library next to your Yamagi Quake II executable. You can select
OpenGL 4.6 through the video menu.

If you have run into issues, please attach output logs with OS/driver version
and device name to the bug report. [List](https://openbenchmarking.org/test/pts/yquake2)
of currently tested devices for the reference.
