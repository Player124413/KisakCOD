# Platform overrides for the Android (NDK) build.
#
# Selected from the top-level CMakeLists.txt with:
#     set(KISAK_PLATFORM android)
#
# This includes scripts/platform/posix_common.cmake, which does the real work:
# it swaps src/win32/* for src/_platform/posix/win32/*, adds the MinGW header
# include path, defines _WIN32, and links pthread/dl. Everything Android
# specific on top of that is here.
#
# ---------------------------------------------------------------------------
# 32-BIT IS NOT OPTIONAL
#
# The engine is a 32-bit codebase and cannot be built 64-bit. The fastfile
# loader streams hardcoded 32-bit struct sizes with 4-byte pointer slots --
# Load_Stream(atStreamStart, (uint8_t *)varGfxImage, 36) for GfxImage, then
# DB_PushStreamPos(4) to skip the 4-byte name pointer -- and
# DB_ConvertOffsetToPointer(uint32_t *) turns 32-bit offsets back into
# pointers. 315 static_asserts pin 32-bit layouts, and an arm64-v8a build
# fails 3429 of them across 60 translation units, every one of them
# pointer-width drift. See docs/ANDROID_PORT.md.
#
# Hence armeabi-v7a and NOT arm64-v8a, here and in android/app/build.gradle.kts.
# ---------------------------------------------------------------------------

if (NOT KISAK_PLATFORM STREQUAL "android")
    message(FATAL_ERROR "KISAK_PLATFORM is incorrect for building android.")
endif()

include(${PLATFORM_DIR}/posix_common.cmake)

# ---------------------------------------------------------------------------
# Dependencies that cannot be built for ARM.
# ---------------------------------------------------------------------------

# Miles Sound System (deps/msslib/mss32.lib) is a closed 32-bit x86 Windows
# binary. There is no Android build and there never will be. OpenAL Soft is the
# only viable backend, so force it on rather than let someone hit a link error
# three hours into a build. This is read by src/sound/snd_driver.cpp, which is
# guarded by `#ifndef KISAK_OPENAL`.
set(KISAK_OPENAL ON CACHE BOOL "Force OpenAL on Android (Miles is x86-only)" FORCE)

# Bink (deps/binklib/binkw32.lib) is the same story. Cinematics are skipped;
# an FFmpeg Bink decoder is the eventual replacement.
set(KISAK_NO_BINK ON)
set(KISAK_NO_STEAM ON)

# Radiant is excluded in the top-level CMakeLists.txt (it is a Win32/D3D tool),
# so it is deliberately not re-declared here.

# ---------------------------------------------------------------------------
# Android-specific definitions
# ---------------------------------------------------------------------------
add_compile_definitions(
    KISAK_ANDROID
    # logcat, not stderr: __android_log_print is what the engine's Com_Printf
    # has to reach. The shim's Sys_Print writes to stderr, which the NDK
    # redirects to logcat by default, so this is belt-and-braces.
    KISAK_ANDROID_LOG
)

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -fomit-frame-pointer)
else()
    add_compile_options(-O0 -g)
endif()

# The engine allocates several 8 MB thread stacks and a large zone; the NDK
# default is fine but being explicit documents the expectation.
add_compile_options(-pthread)

message(STATUS "Android platform overrides: ${PLATFORM_OVERRIDE_DIR}")
message(STATUS "Android: OpenAL=ON, Bink=OFF, Steam=OFF (no closed x86 binaries available)")
message(STATUS "Android: 32-bit armeabi-v7a (the engine cannot be built 64-bit -- see docs/ANDROID_PORT.md)")
