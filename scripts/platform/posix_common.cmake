# Shared POSIX build configuration.
#
# Included by scripts/platform/android/platform.cmake and
# scripts/platform/linux/platform.cmake. Everything that is true of every
# non-Windows build of this engine lives here; only the toolchain and the
# dependency set differ between Android and Linux.
#
# ---------------------------------------------------------------------------
# WHY THIS FILE EXISTS
#
# The engine is a decompilation of a 32-bit Windows game. It calls the Win32
# API from ~40 files spread across the tree, not just from src/win32/, so
# patching every call site was never an option. Instead src/_platform/posix
# implements the Win32 functions the engine actually uses on top of POSIX, and
# the *declarations* come from a vendored subset of the MinGW-w64 headers in
# deps/mingw-headers -- which is also the only remaining source of the real
# d3d9.h and d3dx9.h, since DXVK no longer ships them.
#
# The consequence is that a POSIX build defines _WIN32. That is deliberate and
# load-bearing: without it MinGW's headers take their 32-bit branch for every
# pointer-sized type (UINT_PTR, WPARAM, LPARAM, HANDLE, SIZE_T and every struct
# holding one), and the result is not a compile error but silent ABI corruption
# -- WPARAM measures 4 bytes on a 64-bit target without it. _WIN64 is derived
# from the real pointer width inside win32_posix.h, before <windows.h>.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Source overrides
#
# The override mechanism in scripts/platform_override.cmake swaps any file at
# ${PLATFORM_OVERRIDE_DIR}/<path relative to src/> for the original. Our
# replacements live at src/_platform/posix/win32/, so src/win32/win_main.cpp
# becomes src/_platform/posix/win32/win_main.cpp.
# ---------------------------------------------------------------------------
set(PLATFORM_OVERRIDE_DIR "${SRC_DIR}/_platform/posix")

# WIN32_SRC is the list the subprojects compile. Swapping it is what replaces
# the whole Windows platform layer in one step.
apply_platform_overrides(WIN32_SRC "${PLATFORM_OVERRIDE_DIR}")

# kisakcod.rc is a Windows resource script (icon + version info). There is no
# resource compiler on POSIX and nothing to embed, so it is dropped rather than
# left for the linker to choke on.
list(FILTER WIN32_SRC EXCLUDE REGEX "\\.rc$")

# ---------------------------------------------------------------------------
# Include paths
#
# Order matters. src/_platform/posix MUST come before deps/mingw-headers: both
# directories contain a corecrt.h, and if MinGW's wins it redefines ssize_t and
# time_t against glibc. Our corecrt.h shadows it and forwards to the host
# headers instead.
# ---------------------------------------------------------------------------
include_directories(
    "${SRC_DIR}/_platform/posix"     # the shim: win32_posix.h, winsock2.h, corecrt.h, Windows.h, intrin.h
    "${DEPS_DIR}/mingw-headers"      # MinGW-w64: the Win32 + D3D9 + D3DX9 declarations
    "${SRC_DIR}"                     # <universal/...>, <qcommon/...>, <win32/...>
    "${DEPS_DIR}"                    # deps/ headers
)

# ---------------------------------------------------------------------------
# Definitions
# ---------------------------------------------------------------------------
add_compile_definitions(
    # Selects the POSIX branch in q_shared.h, which is what pulls msvc_compat.h
    # in ahead of everything else. Without this the MSVC-isms the decompiled
    # sources spell (__int8..__int64, __cdecl, __forceinline, __declspec, the
    # Interlocked* family) are undefined.
    KISAK_POSIX

    # Needed by the MinGW headers. See the note at the top of this file.
    _WIN32
    __MINGW32__

    # Trims <windows.h> down to what the engine uses; winsock.h and wsipx.h are
    # explicitly excluded because they collide with the POSIX socket headers.
    WIN32_LEAN_AND_MEAN

    # Windows 7. Reported by GetVersionExA; it is the most permissive value that
    # keeps the engine's legacy code paths enabled, and the newer paths were
    # never decompiled.
    _WIN32_WINNT=0x0601

    # The engine's own headers expect the POSIX feature set (realpath,
    # strcasecmp, M_PI, ...).
    _GNU_SOURCE

    # The decompiled sources rely on char being signed (x86/MSVC default). On
    # ARM this is a behavioural difference, not a style choice.
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
)

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------
# dl:     LoadLibrary/GetProcAddress in win32_posix.cpp
# pthread: every thread, mutex and condition variable in the shim
# m:      the engine's maths
link_libraries(dl pthread m)

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------

# The decompiled sources violate strict aliasing all over the place (type-punning
# through char* and uint32_t* is the norm), and the Win32 build never had to
# care. Turning this on produces wrong-code bugs, not warnings.
add_compile_options(-fno-strict-aliasing)

# Warnings that are noise during a port and hide the real problems.
add_compile_options(
    -Wno-attributes                    # __attribute__((aligned)) on a struct with a ctor
    -Wno-builtin-declaration-mismatch  # the shim declares Win32 functions the host libc also has
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare
    -Wno-reorder
    -Wno-invalid-offsetof               # offsetof on non-standard-layout engine structs
)

message(STATUS "POSIX platform overrides: ${PLATFORM_OVERRIDE_DIR}")
message(STATUS "POSIX: _WIN32 defined, MinGW headers from ${DEPS_DIR}/mingw-headers")
