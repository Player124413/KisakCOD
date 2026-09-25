# KisakCOD -- Android native library.
#
# Two build modes:
#
#   KISAK_ANDROID_STUB=ON  (default)
#       Builds only the JNI bridge. The engine is not linked in, so the
#       launcher, the touch overlay and the whole input pipeline can be
#       shipped and tested while the POSIX port of the engine is in progress.
#
#   KISAK_ANDROID_STUB=OFF
#       Adds the engine sources. This requires the POSIX/Android platform
#       layer (src/_platform/android) and a DXVK build for Android to be in
#       place first; see docs/ANDROID_PORT.md.
#
# In both modes the output is libkisakcod.so, because the Kotlin side loads
# exactly one library.

cmake_minimum_required(VERSION 3.22.1)
project(kisakcod-android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(KISAK_ANDROID_STUB "Build the JNI bridge without the engine" ON)

set(BRIDGE_SRCS android_bridge.cpp)

# Declared before the stub branch so that engine.cmake (which runs inside that
# branch) can target_link_libraries() against it. Order matters here.
add_library(kisakcod SHARED ${BRIDGE_SRCS})

target_include_directories(kisakcod PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (KISAK_ANDROID_STUB)
    target_compile_definitions(kisakcod PRIVATE KISAK_ANDROID_STUB=1)
    message(STATUS "Android: STUB build (engine not linked)")
else()
    # scripts/platform/android/engine.cmake adds the engine as a nested
    # subdirectory, so there is one source list shared with the desktop builds.
    # KISAK_ENGINE_ROOT is the repository root (three levels up from here).
    if (NOT DEFINED KISAK_ENGINE_ROOT)
        get_filename_component(KISAK_ENGINE_ROOT
            "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
    endif()
    include("${KISAK_ENGINE_ROOT}/scripts/platform/android/engine.cmake")

    # DXVK provides d3d9 on top of Vulkan. It must be built for Android
    # separately (meson + NDK) and linked here.
    if (DEFINED KISAK_DXVK_LIB AND EXISTS "${KISAK_DXVK_LIB}")
        target_link_libraries(kisakcod PRIVATE ${KISAK_DXVK_LIB})
    endif()
endif()

target_link_libraries(kisakcod PRIVATE
    android
    log
    # OpenAL Soft is the sound backend on Android (Miles is a closed 32-bit x86
    # Windows binary and has no ARM build). Linked here because the engine's
    # snd_al.cpp resolves its symbols at dlopen time on some platforms.
    OpenSLES
)

# Guard against the mistakes that are invisible until runtime.
target_compile_options(kisakcod PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)
