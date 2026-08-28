#!/usr/bin/env bash
# Builds the KisakCOD Android port APK(s) from the command line.
#
# Requirements: JDK 17, Android SDK (ANDROID_HOME set), NDK r26d, CMake 3.22.1.
# If you have Android Studio, simply open android/ and press Build instead.
set -euo pipefail
cd "$(dirname "$0")/.."

if [ -z "${ANDROID_HOME:-}" ]; then
  echo "ANDROID_HOME is not set (point it at your Android SDK)." >&2
  exit 1
fi

if [ ! -x ./gradlew ]; then
  echo "No gradlew wrapper found. Open the project once in Android Studio to generate it," >&2
  echo "or run with a system gradle:  gradle :app:assembleDebug" >&2
  exit 1
fi

./gradlew :app:assembleDebug "$@"
echo
echo "APK: app/build/outputs/apk/debug/app-debug.apk"