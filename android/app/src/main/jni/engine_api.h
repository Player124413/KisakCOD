// The engine API surface the Android host calls into. The host never includes
// engine headers; on a linked build engine_real.cpp points these slots at the
// real symbols, on a shell build engine_shell.cpp provides inert stubs.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// sysEventType_t values (src/qcommon/qcommon.h)
enum {
    KISAK_SE_NONE = 0,
    KISAK_SE_KEY = 1,
    KISAK_SE_CHAR = 2,
    KISAK_SE_CONSOLE = 3,
};

typedef struct KisakEngineApi {
    void (*com_init)(char *commandLine);
    void (*com_frame)(void);
    void (*com_quit_f)(void);
    void (*cbuf_add_text)(int localClientNum, const char *text);
    int (*cl_mouse_event)(int x, int y, int dx, int dy);
    void (*sys_que_event)(uint32_t time, int type, int value, int value2,
                          int ptrLength, void *ptr);
    uint32_t (*sys_milliseconds)(void);
    bool (*ui_cursor_mode)(void); // true when the game UI wants a mouse cursor
} KisakEngineApi;

extern KisakEngineApi gEngine;
extern bool gEngineLinked; // true when compiled against the real engine

void KisakEngine_InitShell(void); // always available
void KisakEngine_InitReal(void);  // only meaningful when KISAK_ENGINE_LINKED

#ifdef __cplusplus
}
#endif