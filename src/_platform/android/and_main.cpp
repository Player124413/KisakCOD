// ============================================================================
// KisakCOD Android port — engine-build platform glue (M2)
//
// This file is ONLY compiled against the real engine (KISAK_ENGINE_LINKED).
// It implements the pieces the engine expects from its platform layer:
//   * Sys_Init                 — engine bootstrap on Android
//   * Sys_QueEvent/Sys_GetEvent — the input event queue (same contract as
//                                  win32/win_main.cpp)
//   * IN_Frame                 — per-frame input hook (called by the engine's
//                                  Debug_Frame in qcommon/common.cpp)
//   * AndroidEngine_IsUIMode   — menu/cursor state for the touch pipeline
//   * AndroidEngine_Main       — entry point hook used by the JNI host
//
// Touch translation itself lives in and_touch.cpp (shared with the shell
// build); this file only adapts the event types to engine headers.
// ============================================================================

#include "and_touch.h"
#include "and_sys.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <qcommon/qcommon.h>
#include <qcommon/threads.h>

// reuses win_local.h's event contract; we define the struct locally so no
// Windows headers are pulled in
struct sysEvent_t {
    int evTime;
    sysEventType_t evType;
    int evValue;
    int evValue2;
    int evPtrLength;
    void *evPtr;
};

static sysEvent_t s_eventQueue[256];
static int s_eventHead = 0;
static int s_eventTail = 0;

static bool s_uiCursorMode = false;

// ------------------------------------------------------------------ events

extern "C" void Sys_QueEvent(uint32_t time, int type, int value, int value2,
                             int ptrLength, void *ptr) {
    if (ptrLength && ptr) {
        // the engine expects the caller to keep the pointer alive; on Android
        // no queued event uses a payload yet (SE_KEY / SE_CHAR only)
        ptrLength = 0;
        ptr = NULL;
    }
    const int next = (s_eventTail + 1) & 0xFF;
    if (next == s_eventHead) {
        return; // full — drop (same behavior as win32)
    }
    sysEvent_t *ev = &s_eventQueue[s_eventTail];
    ev->evTime = (int)time;
    ev->evType = (sysEventType_t)type;
    ev->evValue = value;
    ev->evValue2 = value2;
    ev->evPtrLength = ptrLength;
    ev->evPtr = ptr;
    s_eventTail = next;
}

extern "C" sysEvent_t *Sys_GetEvent(sysEvent_t *result) {
    if (s_eventHead == s_eventTail) {
        return NULL;
    }
    *result = s_eventQueue[s_eventHead];
    s_eventHead = (s_eventHead + 1) & 0xFF;
    return result;
}

extern "C" void Sys_ShutdownEvents() {
    s_eventHead = s_eventTail = 0;
}

// ------------------------------------------------------------------ input

// The engine calls IN_Frame() every frame (qcommon/common.cpp).
extern "C" void IN_Frame() {
    // look deltas are delivered to CL_MouseEvent immediately by the touch
    // pipeline hooks (and_touch.cpp), so there is nothing to poll here.
}

// The touch pipeline asks whether the UI wants an absolute cursor.
extern "C" bool AndroidEngine_IsUIMode() {
    return s_uiCursorMode;
}

// Hooks the touch pipeline uses to reach into the engine (adapter so
// and_touch.cpp stays engine-free).
static void engineKeyHook(int keynum, bool down) {
    Sys_QueEvent(Sys_Milliseconds(), SE_KEY, keynum, down ? 1 : 0, 0, NULL);
}

static void engineMouseHook(int x, int y, int dx, int dy) {
    extern int CL_MouseEvent(int x, int y, int dx, int dy);
    CL_MouseEvent(x, y, dx, dy);
}

static void engineCmdHook(const char *text) {
    extern void Cbuf_AddText(int localClientNum, const char *text);
    Cbuf_AddText(0, text);
}

extern "C" void AndroidEngine_InstallTouchHooks() {
    static bool installed = false;
    if (installed) return;
    installed = true;
    TouchHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.key = engineKeyHook;
    hooks.mouse = engineMouseHook;
    hooks.cmd = engineCmdHook;
    hooks.uiCursorMode = []() { return AndroidEngine_IsUIMode(); };
    AndroidTouch_Init(&hooks);
}

// ------------------------------------------------------------------ engine bootstrap

extern "C" void Com_Init(char *commandLine);
extern "C" void Sys_InitializeCriticalSections();
extern "C" void Sys_InitMainThread();

// Called by the JNI host once, before the first Com_Frame.
extern "C" void AndroidEngine_Main(char *commandLine) {
    Sys_InitializeCriticalSections();
    Sys_InitMainThread();
    AndroidEngine_InstallTouchHooks();
    Com_Init(commandLine);
}

// ---- stubs the engine references; behave like their win32 counterparts ----

extern "C" void Sys_ShowConsole(int /*show*/, int /*forced*/) {}
extern "C" void Sys_SetWindowTitle(const char *) {}
extern "C" void Sys_Print(const char *msg) { fputs(msg, stderr); }
extern "C" void Sys_Error(const char *error, ...) {
    // engine fatal error — surface in logcat and abort the host loop
    va_list ap;
    va_start(ap, error);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), error, ap);
    va_end(ap);
    AndroidSys_Log("kisakcod", "FATAL: %s", buf);
    abort();
}