# KisakCOD Android port — native library
#
# Two modes (option KISAK_ENGINE_LINKED):
#   OFF (default): shell mode — the APK boots, the touch overlay + launcher are
#                  fully functional and testable on a device without the engine.
#   ON  (M3):      links the KisakCOD engine sources (see ENGINE_PORT.md for the
#                  exact wiring) into the same library including the GLES3
#                  renderer backend.
cmake_minimum_required(VERSION 3.22.1)
project(kisakcod_game LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(KISAK_ENGINE_LINKED "Link against the KisakCOD engine (M3)" OFF)

# jni dir = <repo>/android/app/src/main/jni, so ../../../../ lands at repo root
set(KISAK_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(KISAK_PLATFORM_DIR ${KISAK_REPO_ROOT}/src/_platform/android)
set(KISAK_GFX_GLES_DIR ${KISAK_REPO_ROOT}/src/gfx_gles)

# ---- source files by mode ---------------------------------------------------
set(CORE_SRC
    host.cpp
    egl_host.cpp
    gl_shader.cpp
    ${KISAK_PLATFORM_DIR}/and_touch.cpp
    ${KISAK_PLATFORM_DIR}/and_sys.cpp
)

set(SHELL_SRC
    gl_render_shell.cpp
    engine_shell.cpp
)

set(ENGINE_SRC
    engine_real.cpp
    ${KISAK_GFX_GLES_DIR}/gl_renderer.cpp
    ${KISAK_GFX_GLES_DIR}/gl_backend.cpp
    ${KISAK_GFX_GLES_DIR}/r_image_dds.cpp
)

add_library(kisakcod_game SHARED
    ${CORE_SRC}
    # Shell sources included in both modes (shell renderer used until engine
    # mode is fully integrated); they don't conflict with gfx_gles because
    # they are in a separate TU.
    ${SHELL_SRC}
)

target_include_directories(kisakcod_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${KISAK_PLATFORM_DIR}
    ${KISAK_GFX_GLES_DIR}
)

target_compile_definitions(kisakcod_game PRIVATE KISAK_ENGINE_LINKED=0)

if (KISAK_ENGINE_LINKED)
    message(STATUS "KisakCOD: ENGINE-LINKED build (M3)")
    target_sources(kisakcod_game PRIVATE ${ENGINE_SRC})
    target_compile_definitions(kisakcod_game PRIVATE KISAK_ENGINE_LINKED=1)
    # Engine include dirs must be provided by the M3 build harness
    # (scripts/common_files.cmake); add them via CMAKE_CXX_FLAGS.
    target_include_directories(kisakcod_game PRIVATE
        ${KISAK_REPO_ROOT}/src
        ${KISAK_REPO_ROOT}/src/qcommon
        ${KISAK_REPO_ROOT}/src/universal
    )
else()
    message(STATUS "KisakCOD: SHELL build — engine not linked")
endif()

# GLES3 backend uses glGenVertexArrays, glBindVertexArray (GLES3) plus
# fallback to GLES2 otherwise. Link GLESv3; fall back to GLESv2.
find_library(GLES_LIBRARY
    NAMES GLESv3 GLESv2
    PATHS ${CMAKE_SYSROOT}/usr/lib
)
if (NOT GLES_LIBRARY)
    set(GLES_LIBRARY GLESv2)
endif()

target_link_libraries(kisakcod_game
    android
    log
    EGL
    ${GLES_LIBRARY}
)

if (KISAK_ENGINE_LINKED)
    target_link_libraries(kisakcod_game pthread dl m)
endif()