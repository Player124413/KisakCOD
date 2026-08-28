// Shell (no-engine) implementations of the engine API slots.
#include "engine_api.h"

#include "and_sys.h"

static void sh_com_init(char *) {
    AndroidSys_Log("kisakcod", "[shell] Com_Init (no engine linked)");
}

static void sh_com_frame(void) {
    // nothing to step in shell mode
}

static void sh_com_quit_f(void) {
}

static void sh_cbuf_add_text(int, const char *) {
}

static int sh_cl_mouse_event(int, int, int, int) {
    return 0;
}

static void sh_sys_que_event(uint32_t, int, int, int, int, void *) {
}

static uint32_t sh_sys_milliseconds(void) {
    return AndroidSys_Milliseconds();
}

static bool sh_ui_cursor_mode(void) {
    return false;
}

KisakEngineApi gEngine = {
    sh_com_init,
    sh_com_frame,
    sh_com_quit_f,
    sh_cbuf_add_text,
    sh_cl_mouse_event,
    sh_sys_que_event,
    sh_sys_milliseconds,
    sh_ui_cursor_mode,
};

bool gEngineLinked = false;

void KisakEngine_InitShell(void) {
    // table already points at shell stubs
}