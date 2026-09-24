// KisakCOD -- Android platform bridge
//
// This header is the seam between the Android shell (Java/Kotlin) and the
// engine. It is deliberately free of D3D9/DXVK concepts: the engine keeps
// calling plain D3D9, and DXVK is what turns that into Vulkan underneath.
//
// THREADING
//   Java UI thread  -> queued/staged here, never touches engine state.
//   Game thread     -> Android_PumpInput() + Com_Frame(), owns the engine.
//
// Build modes:
//   KISAK_ANDROID_STUB=1  Build without the engine. Lets the launcher and the
//                         touch layer be shipped and tested while the POSIX
//                         port of the engine is still in progress. Every
//                         engine call is replaced by a log line.

#pragma once

#include <jni.h>
#include <android/native_window.h>

#include <atomic>

namespace kisak::android {

// ---------------------------------------------------------------- lifecycle

// Start the engine on a dedicated thread. Returns false if already running.
bool EngineStart(const char *gameDir, const char *args);
void EngineStop();
bool EngineIsRunning();

// Called by the game thread once per frame, BEFORE Com_Frame().
// Drains queued console commands and applies accumulated look deltas.
void PumpInput();

// Surface (ANativeWindow) lifecycle. The engine is not started until a surface
// exists, because DXVK needs a real window to create its Vulkan swapchain.
bool SurfaceCreated(ANativeWindow *window);
void SurfaceChanged(ANativeWindow *window, int width, int height);
void SurfaceDestroyed();

void SetPaused(bool paused);

// ---------------------------------------------------------------- input (UI thread side)

// Queue a console command ("+attack"). Lock-free SPSC; drops on overflow so a
// stalled game thread can never block the UI thread.
void QueueCommand(const char *cmd);

// Accumulate look delta in engine mouse units.
void AddLook(float dx, float dy);

// Analogue stick deflection in [-1,1].
void SetMoveAxis(float x, float y);

// Read-back used by PumpInput (game thread).
float TakeLookX();
float TakeLookY();
float GetMoveAxisX();
float GetMoveAxisY();

// ---------------------------------------------------------------- misc

const char *LastError();
void SetLastError(const char *msg);

}  // namespace kisak::android
