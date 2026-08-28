# KisakCOD Android port — native library
#
# Two modes (option KISAK_ENGINE_LINKED):
#   OFF (default): shell mode — the APK boots, the touch overlay + launcher are
#                  fully functional and testable on a device without the engine.
#   ON  (M2):      links the KisakCOD engine sources (see ENGINE_PORT.md for the
#                  exact wiring) into the same library.
cmake_minimum_required(VERSION 3.22.1)
project(kisakcod_game LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(KISAK_ENGINE_LINKED "Link against the KisakCOD engine (M2)" OFF)

# jni dir = <repo>/android/app/src/main/jni, so ../../../../ lands at repo root
set(KISAK_PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/_platform/android)

add_library(kisakcod_game SHARED
    host.cpp
    egl_host.cpp
    gl_shader.cpp
    gl_render_shell.cpp
    engine_shell.cpp
    ${KISAK_PLATFORM_DIR}/and_touch.cpp
    ${KISAK_PLATFORM_DIR}/and_sys.cpp
)

target_include_directories(kisakcod_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${KISAK_PLATFORM_DIR}
)

if (KISAK_ENGINE_LINKED)
    message(STATUS "KisakCOD: ENGINE-LINKED build (M2) — see ENGINE_PORT.md")
    target_sources(kisakcod_game PRIVATE engine_real.cpp)
    target_compile_definitions(kisakcod_game PRIVATE KISAK_ENGINE_LINKED=1)
    # Engine include dirs must be provided by the M2 build harness
    # (scripts/common_files.cmake); add them via CMAKE_CXX_FLAGS.
else()
    message(STATUS "KisakCOD: SHELL build — engine not linked")
    target_compile_definitions(kisakcod_game PRIVATE KISAK_ENGINE_LINKED=0)
endif()

target_link_libraries(kisakcod_game
    android
    log
    EGL
    GLESv2
)

if (KISAK_ENGINE_LINKED)
    # The engine needs pthread + dl + m on Android (and later OpenAL for sound,
    # see KISAK_OPENAL in the engine build).
    target_link_libraries(kisakcod_game pthread dl m)
endif()