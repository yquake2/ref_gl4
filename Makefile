# ------------------------------------------------------ #
# Makefile for the OpenGL 4.6 renderer lib for Quake II  #
#                                                        #
# Just type "make" to compile the                        #
#  - OpenGL 4.6 renderer lib (ref_gl4.so / rev_gl4.dll)  #
#                                                        #
# Dependencies:                                          #
#  - SDL 2.0                                             #
#  - libGL                                               #
#                                                        #
# Platforms:                                             #
#  - FreeBSD                                             #
#  - Linux                                               #
#  - NetBSD                                              #
#  - OpenBSD                                             #
#  - Windows                                             #
# ------------------------------------------------------ #

# Variables
# ---------

# - ASAN: Builds with address sanitizer, includes DEBUG.
# - DEBUG: Builds a debug build, forces -O0 and adds debug symbols.
# - VERBOSE: Prints full compile, linker and misc commands.
# - UBSAN: Builds with undefined behavior sanitizer, includes DEBUG.

# ----------

# User configurable options
# -------------------------

# Enables HTTP support through cURL. Used for
# HTTP download.
WITH_CURL:=yes

# Enables the optional OpenAL sound system.
# To use it your system needs libopenal.so.1
# or openal32.dll (we recommend openal-soft)
# installed
WITH_OPENAL:=yes

# Sets an RPATH to $ORIGIN/lib. It can be used to
# inject custom libraries, e.g. a patches libSDL.so
# or libopenal.so. Not supported on Windows.
WITH_RPATH:=yes

# Builds with SDL 3 instead of SDL 2.
WITH_SDL3:=no

# Enable systemwide installation of game assets.
WITH_SYSTEMWIDE:=no

# This will set the default SYSTEMDIR, a non-empty string
# would actually be used. On Windows normals slashes (/)
# instead of backslashed (\) should be used! The string
# MUST NOT be surrounded by quotation marks!
WITH_SYSTEMDIR:=""

# This will set the build options to create an MacOS .app-bundle.
# The app-bundle itself will not be created, but the runtime paths
# will be set to expect the game-data in *.app/
# Contents/Resources
OSX_APP:=yes

# This is an optional configuration file, it'll be used in
# case of presence.
CONFIG_FILE:=config.mk

# ----------

# In case a of a configuration file being present, we'll just use it
ifeq ($(wildcard $(CONFIG_FILE)), $(CONFIG_FILE))
include $(CONFIG_FILE)
endif

# Detect the OS
ifdef SystemRoot
YQ2_OSTYPE ?= Windows
else
YQ2_OSTYPE ?= $(shell uname -s)
endif

# Special case for MinGW
ifneq (,$(findstring MINGW,$(YQ2_OSTYPE)))
YQ2_OSTYPE := Windows
endif

# Detect the architecture
ifeq ($(YQ2_OSTYPE), Windows)
ifdef MINGW_CHOST
ifeq ($(MINGW_CHOST), x86_64-w64-mingw32)
YQ2_ARCH ?= x86_64
else # i686-w64-mingw32
YQ2_ARCH ?= i386
endif
else # windows, but MINGW_CHOST not defined
ifdef PROCESSOR_ARCHITEW6432
# 64 bit Windows
YQ2_ARCH ?= $(PROCESSOR_ARCHITEW6432)
else
# 32 bit Windows
YQ2_ARCH ?= $(PROCESSOR_ARCHITECTURE)
endif
endif # windows but MINGW_CHOST not defined
else
ifneq ($(YQ2_OSTYPE), Darwin)
# Normalize some abiguous YQ2_ARCH strings
YQ2_ARCH ?= $(shell uname -m | sed -e 's/i.86/i386/' -e 's/amd64/x86_64/' -e 's/arm64/aarch64/' -e 's/^arm.*/arm/')
else
YQ2_ARCH ?= $(shell uname -m)
endif
endif

# Detect the compiler
ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang version"), 1)
COMPILER := clang
COMPILERVER := $(shell $(CC)  -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/')
else ifeq ($(shell $(CC) -v 2>&1 | grep -c -E "(gcc version|gcc-Version)"), 1)
COMPILER := gcc
COMPILERVER := $(shell $(CC)  -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/')
else
COMPILER := unknown
endif

# ASAN includes DEBUG
ifdef ASAN
DEBUG=1
endif

# UBSAN includes DEBUG
ifdef UBSAN
DEBUG=1
endif

# ----------

# Base CFLAGS. These may be overridden by the environment.
# Highest supported optimizations are -O2, higher levels
# will likely break this crappy code.
ifdef DEBUG
CFLAGS ?= -O0 -g -Wall -pipe
ifdef ASAN
override CFLAGS += -fsanitize=address -DUSE_SANITIZER
endif
ifdef UBSAN
override CFLAGS += -fsanitize=undefined -DUSE_SANITIZER
endif
else
CFLAGS ?= -O2 -Wall -pipe -fomit-frame-pointer
endif

# Always needed are:
#  -fno-strict-aliasing since the source doesn't comply
#   with strict aliasing rules and it's next to impossible
#   to get it there...
#  -fwrapv for defined integer wrapping. MSVC6 did this
#   and the game code requires it.
#  -fvisibility=hidden to keep symbols hidden. This is
#   mostly best practice and not really necessary.
override CFLAGS += -fno-strict-aliasing -fwrapv -fvisibility=hidden

# -MMD to generate header dependencies. Unsupported by
#  the Clang shipped with OS X.
ifneq ($(YQ2_OSTYPE), Darwin)
override CFLAGS += -MMD
endif

# ----------

# ARM needs a sane minimum architecture. We need the `yield`
# opcode, arm6k is the first iteration that supports it. arm6k
# is also the first Raspberry PI generation and older hardware
# is likely too slow to run the game. We're not enforcing the
# minimum architecture, but if you're build for something older
# like arm5 the `yield` opcode isn't compiled in and the game
# (especially q2ded) will consume more CPU time than necessary.
ifeq ($(YQ2_ARCH), arm)
CFLAGS += -march=armv6k
endif

# ----------

# Switch of some annoying warnings.
ifeq ($(COMPILER), clang)
	# -Wno-missing-braces because otherwise clang complains
	#  about totally valid 'vec3_t bla = {0}' constructs.
	override CFLAGS += -Wno-missing-braces
else ifeq ($(COMPILER), gcc)
	# GCC 8.0 or higher.
	ifeq ($(shell test $(COMPILERVER) -ge 80000; echo $$?),0)
	    # -Wno-format-truncation and -Wno-format-overflow
		# because GCC spams about 50 false positives.
		override CFLAGS += -Wno-format-truncation -Wno-format-overflow
	endif
endif

# ----------

# Defines the operating system and architecture
override CFLAGS += -DYQ2OSTYPE=\"$(YQ2_OSTYPE)\" -DYQ2ARCH=\"$(YQ2_ARCH)\"

# ----------

# For reproduceable builds, look here for details:
# https://reproducible-builds.org/specs/source-date-epoch/
ifdef SOURCE_DATE_EPOCH
override CFLAGS += -DBUILD_DATE=\"$(shell date --utc --date="@${SOURCE_DATE_EPOCH}" +"%b %_d %Y" | sed -e 's/ /\\ /g')\"
endif

# ----------

# Using the default x87 float math on 32bit x86 causes rounding trouble
# -ffloat-store could work around that, but the better solution is to
# just enforce SSE - every x86 CPU since Pentium3 supports that
# and this should even improve the performance on old CPUs
ifeq ($(YQ2_ARCH), i386)
override CFLAGS += -msse -mfpmath=sse
endif

# Force SSE math on x86_64. All sane compilers should do this
# anyway, just to protect us from broken Linux distros.
ifeq ($(YQ2_ARCH), x86_64)
override CFLAGS += -mfpmath=sse
endif

# Disable floating-point expression contraction. While this shouldn't be
# a problem for C (only for C++) better be safe than sorry. See
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=100839 for details.
ifeq ($(COMPILER), gcc)
override CFLAGS += -ffp-contract=off
endif

# ----------

# Systemwide installation.
ifeq ($(WITH_SYSTEMWIDE),yes)
override CFLAGS += -DSYSTEMWIDE
ifneq ($(WITH_SYSTEMDIR),"")
override CFLAGS += -DSYSTEMDIR=\"$(WITH_SYSTEMDIR)\"
endif
endif

# ----------

# We don't support encrypted ZIP files.
ZIPCFLAGS := -DNOUNCRYPT

# Just set IOAPI_NO_64 on everything that's not Linux or Windows,
# otherwise minizip will use fopen64(), fseek64() and friends that
# may be unavailable. This is - of course - not really correct, in
# a better world we would set -DIOAPI_NO_64 to force everything to
# fopen(), fseek() and so on and -D_FILE_OFFSET_BITS=64 to let the
# libc headers do their work. Currently we can't do that because
# Quake II uses nearly everywere int instead of off_t...
#
# This may have the side effect that ZIP files larger than 2GB are
# unsupported. But I doubt that anyone has such large files, they
# would likely hit other internal limits.
ifneq ($(YQ2_OSTYPE),Windows)
ifneq ($(YQ2_OSTYPE),Linux)
ZIPCFLAGS += -DIOAPI_NO_64
endif
endif

# ----------

# Extra CFLAGS for SDL.
ifeq ($(WITH_SDL3),yes)
SDLCFLAGS := $(shell pkgconf --cflags sdl3)
SDLCFLAGS += -DUSE_SDL3
else
SDLCFLAGS := $(shell sdl2-config --cflags)
endif

# ----------

# Base include path.
ifeq ($(YQ2_OSTYPE),Linux)
INCLUDE ?= -I/usr/include
else ifeq ($(YQ2_OSTYPE),FreeBSD)
INCLUDE ?= -I/usr/local/include
else ifeq ($(YQ2_OSTYPE),NetBSD)
INCLUDE ?= -I/usr/X11R7/include -I/usr/pkg/include
else ifeq ($(YQ2_OSTYPE),OpenBSD)
INCLUDE ?= -I/usr/local/include
else ifeq ($(YQ2_OSTYPE),Windows)
INCLUDE ?= -I/usr/include
endif

# ----------

# Base LDFLAGS. This is just the library path.
ifeq ($(YQ2_OSTYPE),Linux)
LDFLAGS ?= -L/usr/lib
else ifeq ($(YQ2_OSTYPE),FreeBSD)
LDFLAGS ?= -L/usr/local/lib
else ifeq ($(YQ2_OSTYPE),NetBSD)
LDFLAGS ?= -L/usr/X11R7/lib -Wl,-R/usr/X11R7/lib -L/usr/pkg/lib -Wl,-R/usr/pkg/lib
else ifeq ($(YQ2_OSTYPE),OpenBSD)
LDFLAGS ?= -L/usr/local/lib
else ifeq ($(YQ2_OSTYPE),Windows)
LDFLAGS ?= -L/usr/lib
endif

# Link address sanitizer if requested.
ifdef ASAN
override LDFLAGS += -fsanitize=address
endif

# Link undefined behavior sanitizer if requested.
ifdef UBSAN
override LDFLAGS += -fsanitize=undefined
endif

# Required libraries.
ifeq ($(YQ2_OSTYPE),Linux)
LDLIBS ?= -lm -ldl -rdynamic
else ifeq ($(YQ2_OSTYPE),FreeBSD)
LDLIBS ?= -lm
else ifeq ($(YQ2_OSTYPE),NetBSD)
LDLIBS ?= -lm
else ifeq ($(YQ2_OSTYPE),OpenBSD)
LDLIBS ?= -lm
else ifeq ($(YQ2_OSTYPE),Windows)
LDLIBS ?= -lws2_32 -lwinmm -static-libgcc
else ifeq ($(YQ2_OSTYPE), Darwin)
LDLIBS ?= -arch $(YQ2_ARCH)
else ifeq ($(YQ2_OSTYPE), Haiku)
LDLIBS ?= -lm -lnetwork
else ifeq ($(YQ2_OSTYPE), SunOS)
LDLIBS ?= -lm -lsocket -lnsl
endif

# ASAN and UBSAN must not be linked
# with --no-undefined. OSX and OpenBSD
# don't support it at all.
ifndef ASAN
ifndef UBSAN
ifneq ($(YQ2_OSTYPE), Darwin)
ifneq ($(YQ2_OSTYPE), OpenBSD)
override LDFLAGS += -Wl,--no-undefined
endif
endif
endif
endif

# ----------

# Extra LDFLAGS for SDL
ifeq ($(WITH_SDL3),yes)
ifeq ($(YQ2_OSTYPE), Darwin)
SDLLDFLAGS := -lSDL3
else
SDLLDFLAGS := $(shell pkgconf --libs sdl3)
endif
else
ifeq ($(YQ2_OSTYPE), Darwin)
SDLLDFLAGS := -lSDL2
else
SDLLDFLAGS := $(shell sdl2-config --libs)
endif
endif

# The renderer libs don't need libSDL2main, libmingw32 or -mwindows.
ifeq ($(YQ2_OSTYPE), Windows)
DLL_SDLLDFLAGS = $(subst -mwindows,,$(subst -lmingw32,,$(subst -lSDL2main,,$(SDLLDFLAGS))))
endif

# ----------

# When make is invoked by "make VERBOSE=1" print
# the compiler and linker commands.
ifdef VERBOSE
Q :=
else
Q := @
endif

# ----------

# Phony targets
.PHONY : all ref_gl4

# ----------

# Builds everything
all: ref_gl4

# Cleanup
clean:
	@echo "===> CLEAN"
	${Q}rm -Rf build release/*

cleanall:
	@echo "===> CLEAN"
	${Q}rm -Rf build release

# ----------

# The OpenGL 4.6 renderer lib

ifeq ($(YQ2_OSTYPE), Windows)

ref_gl4:
	@echo "===> Building ref_gl4.dll"
	${Q}mkdir -p release
	$(MAKE) release/ref_gl4.dll

release/ref_gl4.dll : GLAD_INCLUDE = -Isrc/client/refresh/gl4/glad/include
release/ref_gl4.dll : LDFLAGS += -shared

else # not Windows or Darwin - macOS doesn't support OpenGL 4.6

ref_gl4:
	@echo "===> Building ref_gl4.so"
	${Q}mkdir -p release
	$(MAKE) release/ref_gl4.so

release/ref_gl4.so : GLAD_INCLUDE = -Isrc/client/refresh/gl4/glad/include
release/ref_gl4.so : CFLAGS += -fPIC
release/ref_gl4.so : LDFLAGS += -shared

endif # OS specific ref_gl4 stuff

build/ref_gl4/%.o: %.c
	@echo "===> CC $<"
	${Q}mkdir -p $(@D)
	${Q}$(CC) -c $(CFLAGS) $(SDLCFLAGS) $(INCLUDE) $(GLAD_INCLUDE) -o $@ $<

# ----------

REFGL4_OBJS_ := \
	src/client/refresh/gl4/gl4_draw.o \
	src/client/refresh/gl4/gl4_image.o \
	src/client/refresh/gl4/gl4_light.o \
	src/client/refresh/gl4/gl4_lightmap.o \
	src/client/refresh/gl4/gl4_main.o \
	src/client/refresh/gl4/gl4_mesh.o \
	src/client/refresh/gl4/gl4_misc.o \
	src/client/refresh/gl4/gl4_model.o \
	src/client/refresh/gl4/gl4_sdl.o \
	src/client/refresh/gl4/gl4_surf.o \
	src/client/refresh/gl4/gl4_warp.o \
	src/client/refresh/gl4/gl4_shaders.o \
	src/client/refresh/files/surf.o \
	src/client/refresh/files/models.o \
	src/client/refresh/files/pcx.o \
	src/client/refresh/files/stb.o \
	src/client/refresh/files/wal.o \
	src/client/refresh/files/pvs.o \
	src/common/shared/shared.o \
	src/common/md4.o

REFGL4_OBJS_GLADE_ := \
	src/client/refresh/gl4/glad/src/glad.o

ifeq ($(YQ2_OSTYPE), Windows)
REFGL4_OBJS_ += \
	src/backends/windows/shared/hunk.o
else # not Windows
REFGL4_OBJS_ += \
	src/backends/unix/shared/hunk.o
endif

# ----------

# Rewrite pathes to our object directory.
REFGL4_OBJS = $(patsubst %,build/ref_gl4/%,$(REFGL4_OBJS_))
REFGL4_OBJS += $(patsubst %,build/ref_gl4/%,$(REFGL4_OBJS_GLADE_))

# ----------

# Generate header dependencies.
REFGL4_DEPS= $(REFGL4_OBJS:.o=.d)

# Suck header dependencies in.
-include $(REFGL4_DEPS)

# ----------

# release/ref_gl4.so
ifeq ($(YQ2_OSTYPE), Windows)
release/ref_gl4.dll : $(REFGL4_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL4_OBJS) $(LDLIBS) $(DLL_SDLLDFLAGS) -o $@
	$(Q)strip $@

else
release/ref_gl4.so : $(REFGL4_OBJS)
	@echo "===> LD $@"
	${Q}$(CC) $(LDFLAGS) $(REFGL4_OBJS) $(LDLIBS) $(SDLLDFLAGS) -o $@
endif

# ----------
