cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

# Print a message that using the Makefiles is recommended.
message(NOTICE: " The CMakeLists.txt is unmaintained. Use the Makefile if possible.")

# Enforce "Debug" as standard build type.
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

# CMake project configuration.
project(yquake2 C)

# Cmake module search path.
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/stuff/cmake/modules ${CMAKE_MODULE_PATH})
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED OFF)

if(YQUAKE2LIBS)
	if(CMAKE_CROSSCOMPILING)
		set(CMAKE_FIND_ROOT_PATH ${YQUAKE2LIBS})
	else()
		set(ENV{CMAKE_PREFIX_PATH} ${YQUAKE2LIBS})
	endif()

	set(ENV{SDL2DIR} ${YQUAKE2LIBS})
endif()

# Add extended path for FreeBSD and Homebrew on OS X.
list(APPEND CMAKE_PREFIX_PATH /usr/local)

if (MSVC)
	add_compile_options(/MP) # parallel build (use all cores, or as many as configured in VS)

	# ignore some compiler warnings
	add_compile_options(/wd4244 /wd4305) # possible loss of data/truncation (double to float etc; ignore)
	add_compile_options(/wd4018) # signed/unsigned missmatch
	add_compile_options(/wd4996) # 'function': was declared deprecated (like all that secure CRT stuff)
	# don't show me warnings for system headers, why the fuck isn't this default
	add_compile_options(/experimental:external /external:W0)
else() # GCC/clang/mingw
# Enforce compiler flags:
#  -Wall                -> More warnings
#  -fno-strict-aliasing -> Quake 2 is far away from strict aliasing
#  -fwrapv              -> Make signed integer overflows defined
#  -fvisibility=hidden	-> Force defaultsymbol visibility to hidden
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fno-strict-aliasing -fwrapv -fvisibility=hidden")

# Use -O2 as maximum optimization level. -O3 has it's problems with yquake2.
string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif() # MSVC'S else-case

# Switch off some annoying warnings
if (${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
	if (CMAKE_C_COMPILER_VERSION GREATER 7.99)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format-truncation -Wno-format-overflow")
	endif()
endif()

set(SYSTEMDIR "" CACHE STRING "Override the system default directory")

# These variables will act as our list of include folders and linker flags.
set(yquake2OpenGLLinkerFlags)
set(yquake2SDLLinkerFlags)

# Set directory locations (allowing us to move directories easily)
set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(REF_SRC_DIR ${SOURCE_DIR}/client/refresh)

# Operating system.
set(YQ2OSTYPE "${CMAKE_SYSTEM_NAME}" CACHE STRING "Override operation system type")
add_definitions(-DYQ2OSTYPE="${YQ2OSTYPE}")

# Architecture string
# work around CMake's useless/broken CMAKE_SYSTEM_PROCESSOR (taken from dhewm3)

set(cpu ${CMAKE_SYSTEM_PROCESSOR})

# Originally, ${CMAKE_SYSTEM_PROCESSOR} was supposed to contain the *target* CPU, according to CMake's documentation.
# As far as I can tell this has always been broken (always returns host CPU) at least on Windows
# (see e.g. https://cmake.org/pipermail/cmake-developers/2014-September/011405.html) and wasn't reliable on
# other systems either, for example on Linux with 32bit userland but 64bit kernel it returned the kernel CPU type
# (e.g. x86_64 instead of i686). Instead of fixing this, CMake eventually updated their documentation in 3.20,
# now it's officially the same as CMAKE_HOST_SYSTEM_PROCESSOR except when cross-compiling (where it's explicitly set)
# So we gotta figure out the actual target CPU type ourselves..
if(NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_HOST_SYSTEM_PROCESSOR))
	# special case: cross-compiling, here CMAKE_SYSTEM_PROCESSOR should be correct, hopefully
	# (just leave cpu at ${CMAKE_SYSTEM_PROCESSOR})
elseif(MSVC)
	# because all this wasn't ugly enough, it turned out that, unlike standalone CMake, Visual Studio's
	# integrated CMake doesn't set CMAKE_GENERATOR_PLATFORM, so I gave up on guessing the CPU arch here
	# and moved the CPU detection to MSVC-specific code in neo/sys/platform.h
else() # not MSVC and not cross-compiling, assume GCC or clang (-compatible), seems to work for MinGW as well
	execute_process(COMMAND ${CMAKE_C_COMPILER} "-dumpmachine"
	                RESULT_VARIABLE cc_dumpmachine_res
	                OUTPUT_VARIABLE cc_dumpmachine_out)
	if(cc_dumpmachine_res EQUAL 0)
		string(STRIP ${cc_dumpmachine_out} cc_dumpmachine_out) # get rid of trailing newline
		message(DEBUG "`${CMAKE_C_COMPILER} -dumpmachine` says: \"${cc_dumpmachine_out}\"")
		# gcc -dumpmachine and clang -dumpmachine seem to print something like "x86_64-linux-gnu" (gcc)
		# or "x64_64-pc-linux-gnu" (clang) or "i686-w64-mingw32" (32bit mingw-w64) i.e. starting with the CPU,
		# then "-" and then OS or whatever - so use everything up to first "-"
		string(REGEX MATCH "^[^-]+" cpu ${cc_dumpmachine_out})
		message(DEBUG "  => CPU architecture extracted from that: \"${cpu}\"")
	else()
		message(WARNING "${CMAKE_C_COMPILER} -dumpmachine failed with error (code) ${cc_dumpmachine_res}")
		message(WARNING "will use the (sometimes incorrect) CMAKE_SYSTEM_PROCESSOR (${cpu}) to determine YQ2ARCH")
	endif()
endif()

if(cpu STREQUAL "powerpc")
	set(cpu "ppc")
elseif(cpu STREQUAL "aarch64")
	# "arm64" is more obvious, and some operating systems (like macOS) use it instead of "aarch64"
	set(cpu "arm64")
elseif(cpu MATCHES "[aA][mM][dD]64" OR cpu MATCHES "[xX].*64")
	set(cpu "x86_64")
elseif(cpu MATCHES "i.86" OR cpu MATCHES "[xX]86")
	set(cpu "i386")
elseif(cpu MATCHES "[aA][rR][mM].*") # some kind of arm..
	# On 32bit Raspbian gcc -dumpmachine returns sth starting with "arm-",
	# while clang -dumpmachine says "arm6k-..." - try to unify that to "arm"
	if(CMAKE_SIZEOF_VOID_P EQUAL 8) # sizeof(void*) == 8 => must be arm64
		set(cpu "arm64")
	else() # should be 32bit arm then (probably "armv7l" "armv6k" or sth like that)
		set(cpu "arm")
	endif()
endif()

if(MSVC)
	# for MSVC YQ2ARCH is set in code (in src/common/header/common.h)
	message(STATUS "Setting YQ2OSTYPE to \"${YQ2OSTYPE}\" - NOT setting YQ2ARCH, because we're targeting MSVC (VisualC++)")
else()
	set(ARCH "${cpu}")
	add_definitions(-DYQ2ARCH="${ARCH}")
	message(STATUS "Setting YQ2OSTYPE to \"${YQ2OSTYPE}\" and YQ2ARCH to \"${ARCH}\".")
endif()
# make sure that ${cpu} isn't used below - if at all use ${ARCH}, but not when compiling with MSVC!
unset(cpu)

# END OF workarounds for CMake's poor choices regarding CPU architecture detection


# Systemwide installation of game assets.
if(${SYSTEMWIDE_SUPPORT})
    add_definitions(-DSYSTEMWIDE)
    if(NOT ${SYSTEMDIR} STREQUAL "")
      add_definitions(-DSYSTEMDIR="${SYSTEMDIR}")
    endif()
endif()

# We need to pass some options to minizip / unzip.
add_definitions(-DNOUNCRYPT)

if(NOT (CMAKE_SYSTEM_NAME MATCHES "Linux") AND NOT (CMAKE_SYSTEM_NAME MATCHES "Windows"))
	add_definitions(-DIOAPI_NO_64)
endif()

# Required libraries to build the different components of the binaries. Find
# them and add the include/linker directories and flags (in case the package
# manager find it in a weird place).
find_package(SDL2 REQUIRED)
list(APPEND yquake2IncludeDirectories "${SDL2_INCLUDE_DIR}/..")
list(APPEND yquake2SDLLinkerFlags ${SDL2_LIBRARY})

# We need an OpenGL implementation.
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
list(APPEND yquake2IncludeDirectories ${OPENGL_INCLUDE_DIR})
list(APPEND yquake2OpenGLLinkerFlags ${OPENGL_LIBRARIES})

# General linker flags.
if(NOT MSVC)
	list(APPEND yquake2LinkerFlags m)
endif()
list(APPEND yquake2LinkerFlags ${CMAKE_DL_LIBS})

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	if(!MSVC)
		list(APPEND yquake2LinkerFlags "-static-libgcc")
	endif()
else()
	if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Haiku")
		list(APPEND yquake2LinkerFlags "-rdynamic")
	else()
		list(APPEND yquake2LinkerFlags "-lnetwork")
                set(CMAKE_POSITION_INDEPENDENT_CODE ON)
	endif()
	if (${CMAKE_SYSTEM_NAME} MATCHES "SunOS")
		list(APPEND yquake2LinkerFlags "-lsocket -lnsl")
	endif()
endif()

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD" AND NOT WIN32)
	list(APPEND yquake2LinkerFlags "-Wl,--no-undefined")
endif()

# With all of those libraries and user defined paths
# added, lets give them to the compiler and linker.
include_directories(${yquake2IncludeDirectories})
link_directories(${yquake2LinkerDirectories})

# these settings only work for GCC and clang
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
	# If we're building with gcc for i386 let's define -ffloat-store.
	# This helps the old and crappy x87 FPU to produce correct values.
	# Would be nice if Clang had something comparable.
	if ("${ARCH}" STREQUAL "i386" AND ${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffloat-store")
	endif()

	# Force SSE math on x86_64. All sane compilers should do this
	# anyway, just to protect us from broken Linux distros.
	if ("${ARCH}" STREQUAL "x86_64")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpmath=sse")
	endif()

	if ("${ARCH}" STREQUAL "arm")
		if (CMAKE_SIZEOF_VOID_P EQUAL 4)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv6k")
		endif()
	endif()
endif()


if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
set(REF-Windows-Source
	${SOURCE_DIR}/backends/windows/shared/hunk.c
	)
else()
set(REF-Unix-Source
	${SOURCE_DIR}/backends/unix/shared/hunk.c
	)
endif()

set(GL4-Source
	${REF_SRC_DIR}/gl4/gl4_draw.c
	${REF_SRC_DIR}/gl4/gl4_image.c
	${REF_SRC_DIR}/gl4/gl4_light.c
	${REF_SRC_DIR}/gl4/gl4_lightmap.c
	${REF_SRC_DIR}/gl4/gl4_main.c
	${REF_SRC_DIR}/gl4/gl4_mesh.c
	${REF_SRC_DIR}/gl4/gl4_misc.c
	${REF_SRC_DIR}/gl4/gl4_model.c
	${REF_SRC_DIR}/gl4/gl4_sdl.c
	${REF_SRC_DIR}/gl4/gl4_surf.c
	${REF_SRC_DIR}/gl4/gl4_warp.c
	${REF_SRC_DIR}/gl4/gl4_shaders.c
	${REF_SRC_DIR}/files/models.c
	${REF_SRC_DIR}/files/pcx.c
	${REF_SRC_DIR}/files/stb.c
	${REF_SRC_DIR}/files/surf.c
	${REF_SRC_DIR}/files/wal.c
	${REF_SRC_DIR}/files/pvs.c
	${SOURCE_DIR}/common/shared/shared.c
	${SOURCE_DIR}/common/md4.c
	)

set(Glad-GL4-Source ${REF_SRC_DIR}/gl4/glad/src/glad.c)

set(GL4-Header
	${REF_SRC_DIR}/ref_shared.h
	${REF_SRC_DIR}/constants/anorms.h
	${REF_SRC_DIR}/constants/anormtab.h
	${REF_SRC_DIR}/constants/warpsin.h
	${REF_SRC_DIR}/files/stb_image.h
	${REF_SRC_DIR}/gl4/header/DG_dynarr.h
	${REF_SRC_DIR}/gl4/header/HandmadeMath.h
	${REF_SRC_DIR}/gl4/header/local.h
	${REF_SRC_DIR}/gl4/header/model.h
	${SOURCE_DIR}/common/header/shared.h
	)

set(Glad-GL4-Header
	${REF_SRC_DIR}/gl4/glad/include/glad/glad.h
	${REF_SRC_DIR}/gl4/glad/include/KHR/khrplatform.h
	)

# Build the GL4 dynamic library
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
add_library(ref_gl4 MODULE ${GL4-Source} ${Glad-GL4-Source} ${GL4-Header} ${Glad-GL4-Header} ${REF-Windows-Source})
else()
add_library(ref_gl4 MODULE ${GL4-Source} ${Glad-GL4-Source} ${GL4-Header} ${Glad-GL4-Header} ${REF-Unix-Source})
endif()

set_target_properties(ref_gl4 PROPERTIES
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release
		SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}
		)
target_include_directories(ref_gl4 PRIVATE ${CMAKE_SOURCE_DIR}/src/client/refresh/gl4/glad/include)
target_link_libraries(ref_gl4 ${yquake2LinkerFlags} ${yquake2SDLLinkerFlags})
