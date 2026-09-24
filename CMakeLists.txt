cmake_minimum_required(VERSION 3.16)

project(KisakCOD)

# Set the default C++ standard
set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)

# Set this to match the folder name in scripts/platform
# This will handle the path overrides if needed
#   win32   - MSVC + DirectX SDK (the default, unchanged)
#   android - NDK + Vulkan via DXVK (see docs/ANDROID_PORT.md)
set(KISAK_PLATFORM win32)

# Radiant (the level editor) links D3D and is not portable to Android, so it is
# excluded from Android builds. Left unset on every other platform, which keeps
# the existing builds unchanged.
if (KISAK_PLATFORM STREQUAL "android")
    set(KISAK_NO_RADIANT ON)
endif()

# Set Address Sanitizer
# NOTE: If you use this you will need to copy to the game's directory
# clang_rt.asan_dbg_dynamic-i386.dll and clang_rt.asan_dynamic-i386.dll from the MSVC C directory located below (or similar)
# C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.44.35207\bin\Hostx86\x86
set(USE_ASAN OFF)

# Set to ON to build the sound system against OpenAL instead of Miles/MSS.
# When OFF (default), the original Miles Sound System (MSS/AIL) backend is used.
# Android cannot use Miles (closed 32-bit x86), so it is forced ON there by
# scripts/platform/android/platform.cmake.
if (NOT KISAK_PLATFORM STREQUAL "android")
    set(KISAK_OPENAL OFF)
endif()

# Set the gloabl directory variables
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(PLATFORM_DIR ${SCRIPTS_DIR}/platform)

# Set the output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

# Include global scripts/tricks
include(${SCRIPTS_DIR}/extern/increment_build.cmake)

# Include platform overrides
include(${SCRIPTS_DIR}/platform_override.cmake)

# Log out the KISAK_PLATFORM
message("Building KISAK_PLATFORM: ${KISAK_PLATFORM}")

# Include the subprojects
add_subdirectory(${SCRIPTS_DIR}/mp)
add_subdirectory(${SCRIPTS_DIR}/sp)
add_subdirectory(${SCRIPTS_DIR}/dedi)

# Radiant is a Win32/D3D tool with no Android build.
if (NOT KISAK_NO_RADIANT)
    add_subdirectory(${SCRIPTS_DIR}/radiant)
endif()
