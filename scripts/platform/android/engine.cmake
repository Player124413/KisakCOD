# Adds the engine to the Android build.
#
# Included from android/app/src/main/cpp/CMakeLists.txt when KISAK_ANDROID_STUB
# is OFF. It pulls in the repository's own top-level CMakeLists as a
# subdirectory, so there is exactly one source list for the engine -- the same
# one the desktop builds use. Duplicating it here would guarantee the two drift.
#
# KISAK_PLATFORM=android selects scripts/platform/android/platform.cmake, which
# includes posix_common.cmake, which swaps src/win32/* for
# src/_platform/posix/win32/* and sets up the MinGW headers.
#
# KISAK_BUILD_SHARED=ON turns the three engine targets into shared libraries
# (see the top-level CMakeLists.txt), because Android loads .so files.
#
# The bridge in this project links against KisakCOD-sp and the Kotlin side
# loads both libraries, in dependency order.

set(KISAK_PLATFORM android)
set(KISAK_BUILD_SHARED ON)
set(KISAK_NO_RADIANT ON)
set(KISAK_OPENAL ON)

# The engine's own build expects to be the top-level project; nesting it keeps
# its targets and the bridge's target in one CMake invocation, which is what
# Gradle's externalNativeBuild drives.
add_subdirectory(
    "${KISAK_ENGINE_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}/engine"
)

# Singleplayer only: the user's scope is SP, and MP/dedi would drag in
# Steamworks and a networking path that has no Android story.
target_link_libraries(kisakcod PRIVATE KisakCOD-sp)

message(STATUS "Android: engine linked (KisakCOD-sp, shared, armeabi-v7a)")
