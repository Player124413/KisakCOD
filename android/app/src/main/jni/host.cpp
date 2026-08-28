// ============================================================================
// JNI host for the KisakCOD Android port.
//
// Owns: the EGL surface lifecycle, the engine/shell frame loop, and the wiring
// of Kotlin overlay events into the touch pipeline (and_touch.cpp).
//
// Two build modes (selected by -DKISAK_ENGINE_LINKED=0/1):
//   * shell (default in the gradle project): engine_api pointers are stubs,
//     each nativeStep calls the GLES backend's R_* API directly— the APK
//     runs and the overlay can be tested on a device with real renderer code.
//   * linked (M3, see ENGINE_PORT.md): engine_api pointers are real engine
//     symbols; nativeStep runs Com_Frame() which internally calls R_*.
// ============================================================================

#include "jni_shim.h"  // <jni.h> on Android, minimal shim elsewhere
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gles_stub.h"
#include "egl_host.h"
#include "gl_renderer.h"   // R_* API — the real GLES3 backend
#include "engine_api.h"
#include "and_touch.h"
#include "and_sys.h"

#if defined(__ANDROID__)
#include <android/native_window.h>
#endif

#if KISAK_ENGINE_LINKED
static const bool kShellMode = false;
#else
static const bool kShellMode = true;
#endif

// refdef_s — only used in shell mode (engine not linked), so no ODR conflict
// with the engine's definition when KISAK_ENGINE_LINKED=1. Mirrors the subset
// of fields that R_RenderScene (gl_renderer.cpp) reads: x, y, width, height.
struct refdef_s {
    uint32_t x, y;
    uint32_t width, height;
    float tanHalfFovX;
    float tanHalfFovY;
    float vieworg[3];
    float viewaxis[3][3];
    float viewOffset[3];
    int time;
    float zNear;
    float blurRadius;
    char _pad[0x4098 - 64]; // mirroring full engine struct size
};

// ---------------- global state ----------------------------------------------

static EglHost gEgl;
static bool gComStarted = false;
static char gCmdline[4096] = { 0 };
static int gWidth = 0, gHeight = 0;

// ---------------- shell-mode hooks (visualize input via dummy cmds) ---------
// In shell mode we don't have the engine's CL_* input pipeline, so buttons
// are echoed to the shell-only console. The GLES backend's command buffer
// is populated by the shell's own test commands (gfx_gles_test pattern).

static void initTouchHooks() {
    TouchHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    if (kShellMode) {
        hooks.key = [](int key, bool) {
            AndroidSys_Log("kisakcod", "[shell] key %d %s",
                           key, key < 32 ? "ctrl" : "press");
        };
        hooks.mouse = [](int, int, int dx, int dy) {
            AndroidSys_Log("kisakcod", "[shell] mouse %+d %+d", dx, dy);
        };
        hooks.cmd = [](const char *t) { AndroidSys_Log("kisakcod", "[shell] cmd: %s", t); };
        hooks.uiCursorMode = []() { return false; };
    } else {
        hooks.key = [](int key, bool down) {
            gEngine.sys_que_event(gEngine.sys_milliseconds(), KISAK_SE_KEY,
                                  key, down ? 1 : 0, 0, NULL);
        };
        hooks.mouse = [](int x, int y, int dx, int dy) {
            gEngine.cl_mouse_event(x, y, dx, dy);
        };
        hooks.cmd = [](const char *t) { gEngine.cbuf_add_text(0, t); };
        hooks.uiCursorMode = []() {
            return gEngine.ui_cursor_mode ? gEngine.ui_cursor_mode() : false;
        };
    }
    AndroidTouch_Init(&hooks);
}

// ---------------- JNI --------------------------------------------------------

static const char *jstr(JNIEnv *env, jstring js) {
#if defined(__ANDROID__)
    return js ? env->GetStringUTFChars(js, NULL) : NULL;
#else
    (void)env;
    return js ? js->c : NULL;
#endif
}
static void jstrFree(JNIEnv *env, jstring js, const char *c) {
#if defined(__ANDROID__)
    if (js && c) env->ReleaseStringUTFChars(js, c);
#else
    (void)env; (void)js; (void)c;
#endif
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_kisakcod_android_app_JniBridge_isEngineLinked(JNIEnv *, jobject) {
    return kShellMode ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeCreate(JNIEnv *env, jobject,
                                                     jstring gameRootDir) {
    const char *dir = jstr(env, gameRootDir);
    if (dir) {
        AndroidSys_SetDataDir(dir);
        snprintf(gCmdline, sizeof(gCmdline),
                 "+set fs_basepath %s +exec android_launcher.cfg", dir);
        AndroidSys_Log("kisakcod", "data dir: %s (engine linked: %s)",
                       dir, kShellMode ? "no" : "yes");
        jstrFree(env, gameRootDir, dir);
    }
    if (!kShellMode) {
        KisakEngine_InitReal();
    } else {
        KisakEngine_InitShell();
    }
    initTouchHooks();
    AndroidSys_Log("kisakcod", "nativeCreate done (shell=%d)", kShellMode ? 1 : 0);
}

JNIEXPORT jboolean JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeSurfaceCreated(
    JNIEnv *env, jobject, jobject surface) {
#if defined(__ANDROID__)
    ANativeWindow *win = surface ? ANativeWindow_fromSurface(env, surface) : NULL;
    if (!win) return JNI_FALSE;
    int w = ANativeWindow_getWidth(win);
    int h = ANativeWindow_getHeight(win);
    bool ok = gEgl.initSurface(win, w, h);
    ANativeWindow_release(win);
#else
    bool ok = false;
    int w = 0, h = 0;
#endif
    if (ok) {
        gWidth = w;
        gHeight = h;
        if (kShellMode) {
            // Shell mode: initialize the full GLES backend directly.
            // This exercises the same R_Init -> R_BeginFrame -> R_RenderScene
            // -> R_EndFrame path that the engine-linked build uses.
            R_Init();
            AndroidSys_Log("kisakcod", "shell GLES backend initialized");
        }
    }
    AndroidSys_Log("kisakcod", "surface created %dx%d ok=%d", w, h, ok ? 1 : 0);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeSurfaceChanged(JNIEnv *, jobject,
                                                             jint w, jint h) {
    gWidth = (int)w;
    gHeight = (int)h;
    gEgl.resize(gWidth, gHeight);
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeSurfaceDestroyed(JNIEnv *, jobject) {
    if (kShellMode) {
        R_Shutdown(0);
    }
    gEgl.destroySurface();
}

JNIEXPORT jboolean JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeStep(JNIEnv *, jobject, jlong dtMs) {
    if (!gEgl.isReady()) return JNI_TRUE;

    if (!kShellMode) {
        // ---- Engine-linked mode ----
        if (!gComStarted) {
            gEngine.com_init(gCmdline);
            gComStarted = true;
        }
        // The engine calls R_BeginFrame/R_RenderScene/R_EndFrame internally
        // through Com_Frame -> SCR_UpdateFrame -> R_RenderScene.
        gEngine.com_frame();
    } else {
        // ---- Shell mode: exercise the real GLES backend ----
        // Each frame we drive the full R_* pipeline. The command buffer
        // is currently empty (no engine front-end), so we get a clear/load
        // screen only. This validates the R_Init -> BeginFrame -> EndFrame
        // lifecycle and GL context management.
        R_BeginFrame();
        // R_RenderScene needs a valid refdef; provide minimal defaults.
        refdef_s ref;
        memset(&ref, 0, sizeof(ref));
        ref.width = gWidth > 0 ? (uint32_t)gWidth : 640;
        ref.height = gHeight > 0 ? (uint32_t)gHeight : 480;
        R_RenderScene(&ref);
        R_EndFrame();
    }

    gEgl.endFrame(); // swap buffers
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeButton(JNIEnv *env, jobject,
                                                     jstring actionId,
                                                     jboolean down) {
    const char *id = jstr(env, actionId);
    if (id) {
        AndroidTouch_Button(id, down == JNI_TRUE);
        jstrFree(env, actionId, id);
    }
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeStick(JNIEnv *, jobject, jint side,
                                                    jfloat x, jfloat y,
                                                    jlong dtMs) {
    AndroidTouch_Stick((int)side, (float)x, (float)y, (uint32_t)dtMs);
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeGamepadKey(JNIEnv *, jobject,
                                                         jint key,
                                                         jboolean down) {
    if (kShellMode) {
        AndroidSys_Log("kisakcod", "[shell] gamepad key %d %s",
                       (int)key, down ? "down" : "up");
        return;
    }
    gEngine.sys_que_event(gEngine.sys_milliseconds(), KISAK_SE_KEY,
                          (int)key, down ? 1 : 0, 0, NULL);
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeGamepadStick(JNIEnv *, jobject,
                                                           jint side, jfloat x,
                                                           jfloat y) {
    AndroidTouch_GamepadStick((int)side, (float)x, (float)y, 16u);
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativePause(JNIEnv *, jobject) {
    AndroidSys_Log("kisakcod", "pause (frame loop stops in Java)");
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeResume(JNIEnv *, jobject) {
    AndroidSys_Log("kisakcod", "resume");
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeDestroy(JNIEnv *, jobject) {
    gEgl.destroySurface();
    AndroidTouch_Shutdown();
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeSetFloat(JNIEnv *env, jobject,
                                                       jstring name,
                                                       jfloat value) {
    const char *n = jstr(env, name);
    if (n) {
        AndroidTouch_SetFloat(n, (float)value);
        jstrFree(env, name, n);
    }
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeSetBool(JNIEnv *env, jobject,
                                                      jstring name,
                                                      jboolean value) {
    const char *n = jstr(env, name);
    if (n) {
        AndroidTouch_SetBool(n, value == JNI_TRUE);
        if (kShellMode && strcmp(n, "touch_enabled") == 0) {
            AndroidSys_Log("kisakcod", "[shell] touch_enabled = %d",
                           value == JNI_TRUE ? 1 : 0);
        }
        jstrFree(env, name, n);
    }
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeCmd(JNIEnv *env, jobject,
                                                  jstring text) {
    const char *t = jstr(env, text);
    if (t) {
        if (!kShellMode && gEngine.cbuf_add_text) {
            gEngine.cbuf_add_text(0, t);
        }
        jstrFree(env, text, t);
    }
}

} // extern "C"