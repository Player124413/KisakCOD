#!/usr/bin/env bash
# Builds and runs the native logic tests for the KisakCOD Android port on a
# plain POSIX host (no NDK needed). Also syntax-checks the JNI/GL sources with
# the desktop shim headers so the port's C++ stays clean.
set -euo pipefail
cd "$(dirname "$0")/../.."   # repo root

PLAT=src/_platform/android
JNI=android/app/src/main/jni
OUT=android/tests/build

mkdir -p "$OUT"

echo "== compiling native tests =="
g++ -std=c++17 -Wall -Wextra -O1 \
    -I"$PLAT" \
    android/tests/native_touch_test.cpp \
    "$PLAT/and_touch.cpp" "$PLAT/and_sys.cpp" \
    -pthread -lm -o "$OUT/native_touch_test"

echo "== compiling dds decoder tests =="
g++ -std=c++17 -Wall -Wextra -O1 \
    -Isrc/gfx_gles \
    android/tests/dds_test.cpp src/gfx_gles/r_image_dds.cpp \
    -o "$OUT/dds_test"

echo "== running native tests =="
"$OUT/native_touch_test"
echo
echo "== running dds tests =="
"$OUT/dds_test"

echo "== syntax-checking gfx_gles =="
for f in src/gfx_gles/gl_renderer.cpp src/gfx_gles/gl_backend.cpp src/gfx_gles/r_image_dds.cpp; do
    echo "  checking $f"
    g++ -std=c++17 -Wall -Wextra -fsyntax-only -Wno-ignored-attributes \
        -I"$JNI" -Isrc/gfx_gles \
        "$f"
done

echo "== syntax-checking host + GL sources (desktop shims) =="
for f in "$JNI/host.cpp" "$JNI/egl_host.cpp" "$JNI/gl_shader.cpp" \
         "$JNI/gl_render_shell.cpp" "$JNI/engine_shell.cpp"; do
    echo "  checking $f"
    g++ -std=c++17 -Wall -Wextra -fsyntax-only -DKISAK_ENGINE_LINKED=0 \
        -include "$PLAT/force_include.h" \
        -I"$JNI" -I"$PLAT" -Isrc/gfx_gles "$f"
done

echo "== syntax-checking engine universal/qcommon sources (Android) =="
INCS="-Isrc/_platform/android -Isrc/_platform/android/gfx_d3d -Isrc/_platform/android/win32"
INCS="$INCS -Isrc -Isrc/qcommon -Isrc/universal -Isrc/gfx_gles"
for f in src/universal/timing.cpp src/universal/profile.cpp \
         src/universal/physicalmemory.cpp src/universal/win_shared.cpp; do
    echo "  checking $f"
    g++ -std=c++20 -Wall -Wextra -fsyntax-only -D__ANDROID__ -DKISAK_ANDROID -DKISAK_MP \
        -include "$PLAT/force_include.h" $INCS "$f" 2>&1 | head -5
done

echo
echo "ALL CHECKS DONE"