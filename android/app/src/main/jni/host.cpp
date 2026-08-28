// ============================================================================
// JNI host for the KisakCOD Android port.
//
// Owns: the EGL surface lifecycle, the engine/shell frame loop, and the wiring
// of Kotlin overlay events into the touch pipeline (and_touch.cpp).
//
// Two build modes (selected by -DKISAK_ENGINE_LINKED=0/1):
//   * shell (default in the gradle project): engine_api pointers are stubs,
//     each nativeStep renders the demo scene (gl_render_shell.cpp) — the APK
//     runs and the overlay can be tested on a device.
//   * linked (M2, see ENGINE_PORT.md): engine_api pointers are real engine
//     symbols; nativeStep runs Com_Frame() and swaps the EGL surface.
// ============================================================================

#include "jni_shim.h"  // <jni.h> on Android, minimal shim elsewhere
#include <string.h>
#include <stdio.h>

#include "gles_stub.h"
#include "egl_host.h"
#include "gl_render_shell.h"
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

// ---------------- global state ----------------------------------------------

static EglHost gEgl;
static ShellRender gShell;
static ShellInputState s_shell;
static bool gComStarted = false;
static char gCmdline[4096] = { 0 };

// ---------------- shell-mode hooks (visualize input) ------------------------

static void shellKeyHook(int keynum, bool down) {
    switch (keynum) {
        case 'W': if (down) s_shell.moveY = -1.f; else s_shell.moveY = 0.f; break;
        case 'S': if (down) s_shell.moveY = 1.f; else s_shell.moveY = 0.f; break;
        case 'A': if (down) s_shell.moveX = -1.f; else s_shell.moveX = 0.f; break;
        case 'D': if (down) s_shell.moveX = 1.f; else s_shell.moveX = 0.f; break;
        case 200: s_shell.fire = down; break;   // K_MOUSE1
        case 201: s_shell.ads = down; break;    // K_MOUSE2
        case 32: s_shell.jump = down; break;    // K_SPACE
        case 160: s_shell.sprint = down; break; // K_SHIFT
        case 'C': s_shell.crouch = down; break;
        case 'Z': s_shell.prone = down; break;
        case 'R': s_shell.reload = down; break;
        case 'V': s_shell.melee = down; break;
        case 'F': s_shell.use = down; break;
        case 'G': s_shell.nade = down; break;
        case 9: s_shell.score = down; break;    // K_TAB
        case 27: s_shell.pause = down; break;   // K_ESCAPE
        default: break;
    }
}

static void shellMouseHook(int x, int y, int dx, int dy) {
    (void)x; (void)y;
    s_shell.cursorX += (float)dx / 1920.0f;
    s_shell.cursorY += (float)dy / 1080.0f;
    if (s_shell.cursorX < 0.f) s_shell.cursorX = 0.f;
    if (s_shell.cursorX > 1.f) s_shell.cursorX = 1.f;
    if (s_shell.cursorY < 0.f) s_shell.cursorY = 0.f;
    if (s_shell.cursorY > 1.f) s_shell.cursorY = 1.f;
}

static void initTouchHooks() {
    TouchHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    if (kShellMode) {
        hooks.key = shellKeyHook;
        hooks.mouse = shellMouseHook;
        hooks.cmd = [](const char *) {};
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
    if (ok && kShellMode && !gShell.ready) {
        gShell.init(w, h);
    }
    AndroidSys_Log("kisakcod", "surface created %dx%d ok=%d", w, h, ok ? 1 : 0);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeSurfaceChanged(JNIEnv *, jobject,
                                                             jint w, jint h) {
    gEgl.resize((int)w, (int)h);
    gShell.resize((int)w, (int)h);
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeSurfaceDestroyed(JNIEnv *, jobject) {
    gEgl.destroySurface();
}

JNIEXPORT jboolean JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeStep(JNIEnv *, jobject, jlong dtMs) {
    if (!gEgl.isReady()) return JNI_TRUE;

    if (!kShellMode) {
        if (!gComStarted) {
            gEngine.com_init(gCmdline);
            gComStarted = true;
        }
        gEngine.com_frame();
    } else {
        gShell.draw((float)dtMs, s_shell);
    }

    gEgl.beginFrame();  // viewport
    // engine/shell already drew into the current GL context
    gEgl.endFrame();    // swap
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
    if (kShellMode) {
        if (side == 0) {
            s_shell.moveX = (float)x;
            s_shell.moveY = (float)y;
        } else {
            s_shell.lookX = (float)x;
            s_shell.lookY = (float)y;
        }
    }
}

JNIEXPORT void JNICALL
Java_com_kisakcod_android_app_JniBridge_nativeGamepadKey(JNIEnv *, jobject,
                                                         jint key,
                                                         jboolean down) {
    if (kShellMode) {
        shellKeyHook((int)key, down == JNI_TRUE);
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
            s_shell.touchEnabled = (value == JNI_TRUE);
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