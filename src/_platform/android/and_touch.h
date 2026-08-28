// ============================================================================
// KisakCOD Android port — touch -> engine pipeline
//
// Pure C++, no Android/engine includes, so this file is compiled by BOTH:
//   * the shell build (android/app gradle project, KISAK_ENGINE_LINKED=OFF)
//   * the engine build (M2, linked against KisakCOD)
// All engine interaction goes through the injected TouchHooks table.
//
// Semantics:
//   * buttons  -> engine key events (SE_KEY) so player key binds keep working
//   * left  stick -> WASD movement keys (deadzone + diagonal normalization)
//   * right stick -> mouse look deltas (sensitivity, curve, invert-Y) or an
//                    absolute menu cursor when the engine is in UI mode
// ============================================================================
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Engine key numbers (see src/ui/keycodes.h in the engine).
#define AT_KEY_TAB        9
#define AT_KEY_ENTER      13
#define AT_KEY_ESCAPE     27
#define AT_KEY_SPACE      32

#define AT_KEY_UPARROW    154
#define AT_KEY_DOWNARROW  155
#define AT_KEY_LEFTARROW  156
#define AT_KEY_RIGHTARROW 157
#define AT_KEY_CTRL       159
#define AT_KEY_SHIFT      160

#define AT_KEY_MOUSE1     200
#define AT_KEY_MOUSE2     201
#define AT_KEY_MWHEELUP   206
#define AT_KEY_MWHEELDOWN 205

typedef struct TouchHooks {
    // Emit an engine key event (down=true presses, down=false releases).
    void (*key)(int keynum, bool down);
    // Emit a mouse event: absolute (x,y) + relative (dx,dy).
    void (*mouse)(int x, int y, int dx, int dy);
    // Execute an engine console command (e.g. "toggle cl_whatever").
    void (*cmd)(const char *text);
    // Returns true when the engine is NOT in gameplay look mode
    // (true = right stick should drive an absolute menu cursor).
    bool (*uiCursorMode)(void);
} TouchHooks;

// ---- lifecycle -------------------------------------------------------------

void AndroidTouch_Init(const TouchHooks *hooks);
void AndroidTouch_Shutdown(void);

// ---- input (called from JNI / overlay) -------------------------------------

// actionId: "fire","ads","jump","sprint","crouch","prone","reload","melee",
//           "use","nade","scoreboard","weapon1".."weapon3","pause"
void AndroidTouch_Button(const char *actionId, bool down);

// side: 0 = move stick (WASD), 1 = look stick (mouse/cursor)
// (nx, ny) in [-1..1], dtMs = ms since last sample (for rate-independent look)
void AndroidTouch_Stick(int side, float nx, float ny, uint32_t dtMs);

// Same as AndroidTouch_Stick but ignores the master touch switch: gamepad
// (Bluetooth/USB HID) input must keep working when touch controls are off.
void AndroidTouch_GamepadStick(int side, float nx, float ny, uint32_t dtMs);

// ---- settings (from the launcher) -------------------------------------------

void AndroidTouch_SetFloat(const char *name, float value);
void AndroidTouch_SetBool(const char *name, bool value);

// ---- introspection (used by tests) ------------------------------------------

typedef struct TouchStats {
    // for tests: how many key events queued
    int keyEvents;
    // current pressed movement keys (bit0=W,1=S,2=A,3=D)
    unsigned moveMask;
} TouchStats;

void AndroidTouch_GetStats(TouchStats *stats);

#ifdef __cplusplus
}
#endif