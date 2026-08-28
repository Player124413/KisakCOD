# ============================================================================
# KisakCOD Android port — engine-side platform configuration (M2/M3)
#
# Status: M2 harness. The dedicated server / full client cannot be wired into
# this CMake until a few engine headers are made Android-clean (see
# ENGINE_PORT.md). This file defines the *target shape* of what the Android
# build compiles so the remaining work is incremental:
#
#   1. drop the win32 layer,
#   2. swap gfx_d3d (D3D9) for gfx_gles (GLES), once the backend is ready,
#   3. add the android platform sources,
#   4. keep OpenAL for sound (KISAK_OPENAL=ON).
# ============================================================================

if (NOT KISAK_PLATFORM STREQUAL "android")
    message(FATAL_ERROR "KISAK_PLATFORM is incorrect for building android.")
endif()

# Platform override directory (files in here shadow same-named engine files)
set(PLATFORM_OVERRIDE_DIR "${SRC_DIR}/_platform/android")
apply_platform_overrides(CLIENT_MP "${PLATFORM_OVERRIDE_DIR}")   # as win32 does
apply_platform_overrides(CGAME_MP   "${PLATFORM_OVERRIDE_DIR}")

message(STATUS "Android platform: win32 sources excluded, android platform sources added")

# ---- 1) never compile the Windows layer ------------------------------------
set(WIN32_SRC "")

# ---- 2) renderer: gfx_d3d -> gfx_gles (checked in at M3) -------------------
# Until gfx_gles implements the full R_* surface, the client build target is
# disabled; the dedicated server can go first (no renderer needed).
set(GFX_D3D "")   # exclude DirectX9 backend
if (NOT KISAK_GLES_ENABLED)
    message(STATUS "gfx_gles not enabled yet: client targets stay disabled (M3).")
endif()

# ---- 3) android platform sources -------------------------------------------
set(ANDROID_GLUE_SRC
    "${SRC_DIR}/_platform/android/and_main.cpp"
    "${SRC_DIR}/_platform/android/and_touch.cpp"
    "${SRC_DIR}/_platform/android/and_sys.cpp"
)

if (KISAK_GLES_ENABLED)
    # The C++ files of the GLES backend replace gfx_d3d/gfx_gles:...
    set(GFX_D3D
        "${SRC_DIR}/gfx_gles/gl_renderer.cpp"
        "${SRC_DIR}/gfx_gles/r_image_dds.cpp"
    )
endif()

# ---- 4) sound -----------------------------------------------------------------
# KISAK_OPENAL=ON in the root CMakeLists selects the OpenAL backend already;
# Android links libopenal via scripts/extern/openal.cmake (M2/M3).

# ---- Android toolchain ---------------------------------------------------------
# The shared library is produced by the Android Gradle project (android/)
# which invokes this CMake tree via the NDK toolchain; nothing MSVC-ish here.
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
set(CMAKE_CXX_STANDARD 20)