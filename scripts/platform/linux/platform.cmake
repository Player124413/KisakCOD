# Platform overrides for the Linux build.
#
# Selected from the top-level CMakeLists.txt with:
#     set(KISAK_PLATFORM linux)
#
# This is the same POSIX layer Android uses (src/_platform/posix), built with
# the host toolchain and linked against SDL2 instead of an ANativeWindow. It is
# the fastest way to iterate on the engine port itself: no NDK, no APK, no
# device -- just ./build/kisakcod-sp in front of a game directory.
#
# DXVK-native still runs on Linux, so the renderer path is identical to
# Android's: the engine calls plain D3D9 and DXVK translates it to Vulkan.

if (NOT KISAK_PLATFORM STREQUAL "linux")
    message(FATAL_ERROR "KISAK_PLATFORM is incorrect for building linux.")
endif()

include(${PLATFORM_DIR}/posix_common.cmake)

# ---------------------------------------------------------------------------
# Dependencies that cannot be built for ARM.
#
# Same reasoning as Android: Miles is a closed 32-bit x86 Windows binary and
# Bink likewise, so both are off. OpenAL Soft is the only viable sound backend.
# ---------------------------------------------------------------------------
set(KISAK_OPENAL ON CACHE BOOL "Force OpenAL on Linux (Miles is x86-only)" FORCE)
set(KISAK_NO_BINK ON)
set(KISAK_NO_STEAM ON)

# Radiant is a Win32/D3D tool and is not portable.
set(KISAK_NO_RADIANT ON)

# ---------------------------------------------------------------------------
# SDL2
#
# The Linux front end needs a window, an input device and a Vulkan surface.
# SDL2 is the least invasive choice: it is already the backend DXVK's own
# examples use, and it keeps the host code (which drives the engine through
# Sys_SetDisplaySize / Sys_HostEvent / Sys_HostFrame) identical in shape to
# the Android bridge.
# ---------------------------------------------------------------------------
find_package(SDL2 QUIET)
if (SDL2_FOUND)
    include_directories(${SDL2_INCLUDE_DIRS})
    message(STATUS "Linux: SDL2 found, window/input host available")
else()
    message(STATUS "Linux: SDL2 not found -- engine builds headless (no window/input host)")
endif()

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
else()
    add_compile_options(-O0 -g)
endif()

message(STATUS "Linux platform overrides: ${PLATFORM_OVERRIDE_DIR}")
message(STATUS "Linux: OpenAL=ON, Bink=OFF, Steam=OFF, Radiant=OFF")
