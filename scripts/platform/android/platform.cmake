# Platform overrides for the Android (NDK) build.
#
# Selected from the top-level CMakeLists.txt with:
#     set(KISAK_PLATFORM android)
#
# This mirrors scripts/platform/win32/platform.cmake: it validates the
# platform name, points the override mechanism at src/_platform/android, and
# sets the toolchain flags. There is intentionally no MSVC flag here.

if (NOT KISAK_PLATFORM STREQUAL "android")
    message(FATAL_ERROR "KISAK_PLATFORM is incorrect for building android.")
endif()

# Where platform-specific file replacements live.
# Any file at src/_platform/android/<same relative path> shadows the original.
set(PLATFORM_OVERRIDE_DIR "${SRC_DIR}/_platform/android")

# ---------------------------------------------------------------------------
# Dependencies that cannot be built for ARM64.
# ---------------------------------------------------------------------------

# Miles Sound System (deps/msslib/mss32.lib) is a closed 32-bit x86 Windows
# binary. There is no Android build and there never will be. OpenAL Soft is the
# only viable backend, so force it on rather than let someone hit a link error
# three hours into a build. This is read by src/sound/snd_driver.cpp, which is
# guarded by `#ifndef KISAK_OPENAL`.
set(KISAK_OPENAL ON CACHE BOOL "Force OpenAL on Android (Miles is x86-only)" FORCE)

# Bink (deps/binklib/binkw32.lib) is the same story. These are consumed by the
# engine source lists that stage 2/3 of the port adds; they are declared here
# so the intent is recorded next to the rest of the platform contract.
set(KISAK_NO_BINK ON)
set(KISAK_NO_STEAM ON)

# Radiant is excluded in the top-level CMakeLists.txt (it is a Win32/D3D tool),
# so it is deliberately not re-declared here.

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------

add_compile_definitions(
    KISAK_ANDROID
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    # The engine is full of 32-bit assumptions inherited from the decompilation.
    # This does not fix them, but it makes the compiler shout about the ones
    # that matter while the port is in progress.
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The decompiled sources rely on char being signed (x86/MSVC default). On ARM
# this is a real behavioural difference, not a style choice.
add_compile_options(-fsigned-char)

# Long-path / large translation unit tolerance for the generated files.
add_compile_options(-fno-strict-aliasing)

# Warnings that are noise during a port and hide real problems.
add_compile_options(
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-reorder
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -fomit-frame-pointer)
else()
    add_compile_options(-O0 -g)
endif()

message(STATUS "Android platform overrides: ${PLATFORM_OVERRIDE_DIR}")
message(STATUS "Android: OpenAL=ON, Bink=OFF, Steam=OFF (no closed x86 binaries available)")
