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
g++ -std=c++17 -Wall -Wextra -fsyntax-only \
    -I"$JNI" -Isrc/gfx_gles \
    src/gfx_gles/gl_renderer.cpp src/gfx_gles/r_image_dds.cpp

echo "== syntax-checking host + GL sources (desktop shims) =="
for f in "$JNI/host.cpp" "$JNI/egl_host.cpp" "$JNI/gl_shader.cpp" \
         "$JNI/gl_render_shell.cpp" "$JNI/engine_shell.cpp"; do
    echo "  checking $f"
    g++ -std=c++17 -Wall -Wextra -fsyntax-only -DKISAK_ENGINE_LINKED=0 \
        -I"$JNI" -I"$PLAT" "$f"
done

echo
echo "ALL CHECKS DONE"