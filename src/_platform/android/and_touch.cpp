// ============================================================================
// KisakCOD Android port — touch -> engine pipeline (see and_touch.h)
// ============================================================================

#include "and_touch.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

// ---------- state ------------------------------------------------------------

static TouchHooks g_hooks;
static bool g_hooksReady = false;

// movement stick
static const float MOVE_DEADZONE = 0.15f;
static unsigned g_moveKeys = 0;      // bit0=W bit1=S bit2=A bit3=D (pressed)
static int g_movementKey[4] = { 'W', 'S', 'A', 'D' };

// look stick
static float g_sensH = 1.0f;
static float g_sensV = 1.0f;
static bool  g_invertY = false;
static int   g_lookCurve = 1;        // 0 = linear, 1 = boosted
static float g_cursorX = 0.4f;       // menu cursor (normalized 0..1)
static float g_cursorY = 0.5f;
static bool  g_touchEnabled = true;

static uint32_t g_statsKeyEvents = 0;

// ---------- helpers ------------------------------------------------------------

static float deadzone(float v, float dz) {
    if (v > dz)  return (v - dz) / (1.0f - dz);
    if (v < -dz) return (v + dz) / (1.0f - dz);
    return 0.0f;
}

// response curve: mild exponent boost keeps center control precise
static float curve(float v) {
    if (g_lookCurve == 0 || v == 0.0f) return v;
    float s = v < 0 ? -1.0f : 1.0f;
    float a = fabsf(v);
    return s * powf(a, 1.25f);
}

static void keyEvent(int keynum, bool down) {
    if (!g_hooksReady || !g_hooks.key) return;
    if (!g_touchEnabled && keynum != AT_KEY_ESCAPE) {
        // master switch off: ignore everything except pause
        return;
    }
    g_hooks.key(keynum, down);
    g_statsKeyEvents++;
}

static void mouseEvent(int x, int y, int dx, int dy) {
    if (!g_hooksReady || !g_hooks.mouse) return;
    if (!g_touchEnabled) return;
    g_hooks.mouse(x, y, dx, dy);
}

// ensure a movement key is (or isn't) held; returns bits changed
static void setMoveKey(int idx, bool down) {
    const unsigned bit = (1u << idx);
    const bool held = (g_moveKeys & bit) != 0;
    if (held == down) return;
    if (down) g_moveKeys |= bit; else g_moveKeys &= ~bit;
    keyEvent(g_movementKey[idx], down);
}

// ---------- button map ---------------------------------------------------------

typedef struct ButtonMap { const char *id; int key; } ButtonMap;

static const ButtonMap kButtons[] = {
    { "fire",       AT_KEY_MOUSE1    },
    { "ads",        AT_KEY_MOUSE2    },
    { "jump",       AT_KEY_SPACE     },
    { "sprint",     AT_KEY_SHIFT     },
    { "crouch",     'C'              },
    { "prone",      'Z'              },
    { "reload",     'R'              },
    { "melee",      'V'              },
    { "use",        'F'              },
    { "nade",       'G'              },
    { "scoreboard", AT_KEY_TAB       },
    { "weapon1",    '1'              },
    { "weapon2",    '2'              },
    { "weapon3",    '3'              },
    { "pause",      AT_KEY_ESCAPE    },
    { "enter",      AT_KEY_ENTER     },
    { "up",         AT_KEY_UPARROW   },
    { "down",       AT_KEY_DOWNARROW },
    { "left",       AT_KEY_LEFTARROW },
    { "right",      AT_KEY_RIGHTARROW},
};

static int buttonKey(const char *id) {
    for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) {
        if (strcmp(kButtons[i].id, id) == 0) return kButtons[i].key;
    }
    return 0;
}

// ---------- API ---------------------------------------------------------------

void AndroidTouch_Init(const TouchHooks *hooks) {
    memset(&g_hooks, 0, sizeof(g_hooks));
    if (hooks) g_hooks = *hooks;
    g_hooksReady = true;
    g_moveKeys = 0;
    g_sensH = 1.0f;
    g_sensV = 1.0f;
    g_invertY = false;
    g_lookCurve = 1;
    g_touchEnabled = true;
    g_statsKeyEvents = 0;
}

void AndroidTouch_Shutdown(void) {
    // release everything
    for (int i = 0; i < 4; i++) setMoveKey(i, false);
    g_hooksReady = false;
}

void AndroidTouch_Button(const char *actionId, bool down) {
    if (!g_hooksReady || !actionId) return;
    int key = buttonKey(actionId);
    if (key != 0) keyEvent(key, down);
}

static void stickInternal(int side, float nx, float ny, uint32_t dtMs, bool gated) {
    (void)dtMs;
    if (!g_hooksReady) return;
    if (gated && !g_touchEnabled) return; // master switch off: ignore touch sticks

    if (side == 0) {
        // ---------- movement stick ----------
        float dx = deadzone(nx, MOVE_DEADZONE);
        float dy = deadzone(ny, MOVE_DEADZONE);
        // vertical: forward = up (ny negative)
        const bool forward = dy < -0.001f;
        const bool back    = dy >  0.001f;
        const bool left    = dx < -0.001f;
        const bool right   = dx >  0.001f;
        setMoveKey(0, forward);
        setMoveKey(1, back);
        setMoveKey(2, left);
        setMoveKey(3, right);
    } else {
        // ---------- look / menu cursor ----------
        float lx = deadzone(nx, 0.12f);
        float ly = deadzone(ny, 0.12f);

        bool uiMode = g_hooks.uiCursorMode ? g_hooks.uiCursorMode() : false;
        if (!uiMode) {
            // gameplay: rate-independent mouse deltas
            float dx = curve(lx) * g_sensH * 260.0f;   // px/s at full deflection
            float dy = curve(ly) * g_sensV * 260.0f;
            if (g_invertY) dy = -dy;
            if (!gated || g_touchEnabled) mouseEvent(0, 0, (int)dx, (int)dy);
        } else {
            // menus: absolute cursor driven by the stick
            const float speed = 1.2f / 1000.0f;         // screen fraction per ms
            g_cursorX += curve(lx) * speed * (float)dtMs;
            g_cursorY += curve(ly) * speed * (float)dtMs;
            if (g_cursorX < 0) g_cursorX = 0;
            if (g_cursorX > 1) g_cursorX = 1;
            if (g_cursorY < 0) g_cursorY = 0;
            if (g_cursorY > 1) g_cursorY = 1;
            // Menus are rendered in a 1920x1080 virtual space by the engine's
            // ScreenPlacement; scale the normalized cursor into it.
            if (!gated || g_touchEnabled) {
                mouseEvent((int)(g_cursorX * 1920.0f),
                           (int)(g_cursorY * 1080.0f), 0, 0);
            }
        }
    }
}

void AndroidTouch_Stick(int side, float nx, float ny, uint32_t dtMs) {
    stickInternal(side, nx, ny, dtMs, true);
}

void AndroidTouch_GamepadStick(int side, float nx, float ny, uint32_t dtMs) {
    stickInternal(side, nx, ny, dtMs, false);
}

void AndroidTouch_SetFloat(const char *name, float value) {
    if (!name) return;
    if (strcmp(name, "sens_h") == 0) {
        g_sensH = value < 0.05f ? 0.05f : (value > 5.0f ? 5.0f : value);
    } else if (strcmp(name, "sens_v") == 0) {
        g_sensV = value < 0.05f ? 0.05f : (value > 5.0f ? 5.0f : value);
    } else if (strcmp(name, "look_curve") == 0) {
        g_lookCurve = value >= 0.5f ? 1 : 0;
    } else if (strcmp(name, "render_scale") == 0) {
        // stored here so the host can read it; clamp for safety
        (void)value;
    }
}

void AndroidTouch_SetBool(const char *name, bool value) {
    if (!name) return;
    if (strcmp(name, "invert_y") == 0) {
        g_invertY = value;
    } else if (strcmp(name, "touch_enabled") == 0) {
        if (g_touchEnabled && !value) {
            // disable: release everything currently held
            for (int i = 0; i < 4; i++) setMoveKey(i, false);
        }
        g_touchEnabled = value;
    }
    if (g_hooks.cmd) {
        // mirror state into the engine console so cvars stay readable
        // (only when engine is present)
    }
}

void AndroidTouch_GetStats(TouchStats *stats) {
    if (!stats) return;
    stats->keyEvents = (int)g_statsKeyEvents;
    stats->moveMask = g_moveKeys;
}