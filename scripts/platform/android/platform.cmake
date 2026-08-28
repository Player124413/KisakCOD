# ============================================================================
# KisakCOD Android port — engine-side platform configuration (M2)
#
# This file is included by each subproject (mp, sp, dedi). For the Android
# build, only the dedicated server (KisakCOD-dedi) is practical at M2; the
# client targets (mp, sp) will fail at link time because the GLES renderer
# backend is not yet fully implemented (see ENGINE_PORT.md).
#
# What M2 provides:
#   - Android platform headers (src/_platform/android/*) override the Windows
#     headers the engine expects
#   - A comprehensive Win32 API compatibility shim (win32_compat.h) maps
#     CreateThread, VirtualAlloc, etc. to POSIX equivalents
#   - Stub gfx_d3d headers let the non-renderer modules (EffectsCore, DynEntity,
#     cgame, ui, …) compile without the D3D9 SDK — the stubs provide just the
#     type declarations the source references
#   - The GLES backend (gfx_gles) is wired in at M3
# ============================================================================

if (NOT KISAK_PLATFORM STREQUAL "android")
    message(FATAL_ERROR "KISAK_PLATFORM is incorrect for building android.")
endif()

# ---- 0) Add the Android platform include path ---------------------------------
# Must come FIRST so that <win32/win_local.h>, <Windows.h>, <gfx_d3d/…> etc.
# resolve to the Android stubs in src/_platform/android instead of the originals.
include_directories(BEFORE "${SRC_DIR}/_platform/android")

# ---- 1) never compile the Windows layer; use Android platform sources -------
set(WIN32_SRC
    "${SRC_DIR}/_platform/android/and_main.cpp"
    "${SRC_DIR}/_platform/android/and_touch.cpp"
    "${SRC_DIR}/_platform/android/and_sys.cpp"
)

# ---- 4) sound: OpenAL backend (optional for dedi, static for now) -----------
# KISAK_OPENAL=ON in the root CMakeLists selects the OpenAL backend.
# On Android, OpenAL is linked via scripts/extern/openal.cmake (M2).

# ---- 5) Compiler flags -----------------------------------------------------
add_compile_definitions(_CRT_SECURE_NO_WARNINGS KISAK_ANDROID)
set(CMAKE_CXX_STANDARD 20)

# ---- 6) Diagnostics ---------------------------------------------------------
message(STATUS "Android platform: WIN32_SRC excluded, GFX_D3D excluded")
message(STATUS "Android platform: include path: ${SRC_DIR}/_platform/android")
message(STATUS "Android platform: platform sources: ${ANDROID_GLUE_SRC}")