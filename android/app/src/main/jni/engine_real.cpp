// Engine-linked implementations of the engine API slots.
//
// This TU is ONLY compiled when KISAK_ENGINE_LINKED=1 (the M2 engine build —
// see ENGINE_PORT.md). It is compiled together with the engine sources using
// the same platform defines (KISAK_MP etc.), so the symbols below resolve to
// the engine's real C++ functions.
#include "engine_api.h"

#if !KISAK_ENGINE_LINKED
#error "engine_real.cpp must only be compiled with KISAK_ENGINE_LINKED=1"
#endif

// The engine declares these in its own headers; matching the declarations here
// keeps the linkage natural (C++ mangling, __cdecl on win32 builds).
void Com_Init(char *commandLine);
void Com_Frame();
void Com_Quit_f();
void Cbuf_AddText(int localClientNum, const char *text);
int CL_MouseEvent(int x, int y, int dx, int dy);
void Sys_QueEvent(uint32_t time, int type, int value, int value2,
                  int ptrLength, void *ptr);
uint32_t Sys_Milliseconds(void);

// Provided by the android platform layer (src/_platform/android/and_main.cpp)
bool AndroidEngine_IsUIMode(void);

static void re_com_init(char *cmd) { Com_Init(cmd); }
static void re_com_frame(void) { Com_Frame(); }
static void re_com_quit_f(void) { Com_Quit_f(); }
static void re_cbuf_add_text(int lcn, const char *t) { Cbuf_AddText(lcn, t); }
static int  re_cl_mouse_event(int x, int y, int dx, int dy) {
    return CL_MouseEvent(x, y, dx, dy);
}
static void re_sys_que_event(uint32_t t, int ty, int v, int v2, int pl, void *p) {
    Sys_QueEvent(t, ty, v, v2, pl, p);
}
static uint32_t re_sys_milliseconds(void) { return Sys_Milliseconds(); }
static bool re_ui_cursor_mode(void) { return AndroidEngine_IsUIMode(); }

KisakEngineApi gEngine = {
    re_com_init,
    re_com_frame,
    re_com_quit_f,
    re_cbuf_add_text,
    re_cl_mouse_event,
    re_sys_que_event,
    re_sys_milliseconds,
    re_ui_cursor_mode,
};

bool gEngineLinked = true;

void KisakEngine_InitReal(void) {
    // table already points at the real engine
}