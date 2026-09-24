// KisakCOD -- Android platform bridge implementation.
//
// This file is the ONLY place where the Android UI thread and the engine meet.
// The UI thread never calls into the engine; it stages input here, and the game
// thread drains it once per frame from Android_PumpInput().
//
// Graphics: nothing in this file knows about Vulkan. The engine keeps calling
// plain Direct3D 9 and is linked against DXVK, which translates D3D9 (including
// the DXSO shader bytecode embedded in the game's fastfiles) into Vulkan.

#include "android_bridge.h"

#include <android/log.h>
#include <android/native_window_jni.h>

#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#ifndef KISAK_ANDROID_STUB
// Engine side. Include paths come from src/ via the CMake target.
#include "qcommon/qcommon.h"  // Com_Init_Try_Block_Function, Com_Frame_Try_Block_Function, Dvar_Init
#include "qcommon/cmd.h"      // Cbuf_AddText
#include "client/cl_input.h"  // CL_MouseEvent

// TODO(android): the Sys_* platform contract is currently declared in
// src/win32/win_local.h. When src/_platform/android/ lands it must provide the
// same declarations and this include becomes the platform-neutral header.
#include "win32/win_local.h"  // Sys_InitializeCriticalSections
#endif

namespace {

constexpr const char *kTag = "KisakCOD";

#define KLOGI(...) __android_log_print(ANDROID_LOG_INFO, kTag, __VA_ARGS__)
#define KLOGW(...) __android_log_print(ANDROID_LOG_WARN, kTag, __VA_ARGS__)
#define KLOGE(...) __android_log_print(ANDROID_LOG_ERROR, kTag, __VA_ARGS__)

// ---------------------------------------------------------------- input staging

// Single-producer (UI thread) / single-consumer (game thread) ring buffer.
// Dropping on overflow is the correct failure mode: the UI thread must never
// block on a game thread that is mid-load or has crashed.
constexpr int kCmdSlots = 64;
constexpr int kCmdLen = 128;

struct CommandRing {
    char slots[kCmdSlots][kCmdLen];
    std::atomic<int> head{0};  // producer
    std::atomic<int> tail{0};  // consumer
} g_commands;

std::atomic<float> g_lookX{0.0f};
std::atomic<float> g_lookY{0.0f};
std::atomic<float> g_moveX{0.0f};
std::atomic<float> g_moveY{0.0f};

std::atomic<bool> g_running{false};
std::atomic<bool> g_paused{false};
std::atomic<bool> g_hasSurface{false};

std::string g_lastError;
std::thread g_gameThread;

}  // namespace

namespace kisak::android {

// ---------------------------------------------------------------- input API

void QueueCommand(const char *cmd) {
    if (!cmd || !*cmd) return;

    const int h = g_commands.head.load(std::memory_order_relaxed);
    const int next = (h + 1) % kCmdSlots;
    if (next == g_commands.tail.load(std::memory_order_acquire)) {
        KLOGW("command queue full, dropping: %s", cmd);
        return;  // full: drop rather than block
    }

    char *dst = g_commands.slots[h];
    std::strncpy(dst, cmd, kCmdLen - 1);
    dst[kCmdLen - 1] = '\0';
    g_commands.head.store(next, std::memory_order_release);
}

void AddLook(float dx, float dy) {
    // exchange() is simpler and adequate here: we only need the accumulated
    // total, not per-event ordering.
    g_lookX.store(g_lookX.load(std::memory_order_relaxed) + dx, std::memory_order_relaxed);
    g_lookY.store(g_lookY.load(std::memory_order_relaxed) + dy, std::memory_order_relaxed);
}

void SetMoveAxis(float x, float y) {
    g_moveX.store(x, std::memory_order_relaxed);
    g_moveY.store(y, std::memory_order_relaxed);
}

float TakeLookX() { return g_lookX.exchange(0.0f, std::memory_order_relaxed); }
float TakeLookY() { return g_lookY.exchange(0.0f, std::memory_order_relaxed); }
float GetMoveAxisX() { return g_moveX.load(std::memory_order_relaxed); }
float GetMoveAxisY() { return g_moveY.load(std::memory_order_relaxed); }

void SetPaused(bool paused) { g_paused.store(paused, std::memory_order_relaxed); }

const char *LastError() { return g_lastError.c_str(); }
void SetLastError(const char *msg) { g_lastError = msg ? msg : ""; }

// ---------------------------------------------------------------- pump

void PumpInput() {
    // 1. Console commands.
    const int head = g_commands.head.load(std::memory_order_acquire);
    int tail = g_commands.tail.load(std::memory_order_relaxed);
    while (tail != head) {
        const char *cmd = g_commands.slots[tail];
#ifdef KISAK_ANDROID_STUB
        KLOGI("cmd: %s", cmd);
#else
        // localClientNum 0 is correct for the SP build.
        Cbuf_AddText(0, cmd);
        // Cbuf_AddText expects commands to be newline terminated.
        Cbuf_AddText(0, "\n");
#endif
        tail = (tail + 1) % kCmdSlots;
    }
    g_commands.tail.store(tail, std::memory_order_release);

    // 2. Look. CL_MouseEvent(x, y, dx, dy) is the same entry point the Win32
    //    mouse path uses (src/win32/win_input.cpp -> IN_MouseMove), so touch
    //    look goes through exactly the code the game already trusts.
    const float dx = TakeLookX();
    const float dy = TakeLookY();
    if (dx != 0.0f || dy != 0.0f) {
#ifdef KISAK_ANDROID_STUB
        KLOGI("look: %.2f %.2f", dx, dy);
#else
        CL_MouseEvent(0, 0, static_cast<int>(dx), static_cast<int>(dy));
#endif
    }

#ifndef KISAK_ANDROID_STUB
    // 3. Analogue movement.
    //
    // NOT WIRED YET, deliberately. The engine has a gamepad path
    // (CL_GamepadMove + CL_GamepadAxisValue in src/client/cl_input.cpp) but the
    // call site at cl_input.cpp:1609 is commented out in this build, so feeding
    // it would do nothing. Until that is re-enabled, the touch layer derives
    // digital +forward/+back/+moveleft/+moveright commands from the stick
    // (see TouchControlsView.updateMovement), which works today.
    //
    // To finish this later:
    //   1. re-enable CL_GamepadMove(cmd) in CL_CreateCmd,
    //   2. provide CL_GamepadAxisValue()/GPad_GetButton() on the Android side
    //      reading GetMoveAxisX()/GetMoveAxisY() below,
    //   3. switch the stick to analogue output.
    (void)GetMoveAxisX();
    (void)GetMoveAxisY();
#endif
}

// ---------------------------------------------------------------- engine thread

namespace {

void GameThreadMain(std::string gameDir, std::string args) {
    KLOGI("game thread start (dir=%s args=%s)", gameDir.c_str(), args.c_str());

#ifdef KISAK_ANDROID_STUB
    // Stub mode: emulate a frame loop so the shell, input plumbing and HUD can
    // be exercised end to end without the engine.
    KLOGI("KISAK_ANDROID_STUB: engine not linked in");
    g_running.store(true, std::memory_order_relaxed);
    while (g_running.load(std::memory_order_relaxed)) {
        if (!g_paused.load(std::memory_order_relaxed)) {
            PumpInput();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
#else
    // Mirror the Win32 entry point (src/win32/win_main.cpp WinMain) minus the
    // window/splash/console scaffolding, which Android does not have.
    //
    // Order is significant: critical sections and Dvar_Init must precede
    // Com_Init, exactly as in WinMain.
    Sys_InitializeCriticalSections();

    Dvar_Init();

    // Com_Init_Try_Block_Function / Com_Frame_Try_Block_Function are the
    // declared, exception-wrapped entry points (src/qcommon/qcommon.h).
    char *cmdline = const_cast<char *>(args.c_str());
    Com_Init_Try_Block_Function(cmdline);

    g_running.store(true, std::memory_order_relaxed);

    while (g_running.load(std::memory_order_relaxed)) {
        if (!g_paused.load(std::memory_order_relaxed) &&
            g_hasSurface.load(std::memory_order_relaxed)) {
            PumpInput();
            Com_Frame_Try_Block_Function();
        } else {
            // Not rendering: still drain input so no command is ever stranded
            // (a stranded "-attack" would leave the trigger held).
            PumpInput();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
#endif

    KLOGI("game thread exit");
}

}  // namespace

bool EngineStart(const char *gameDir, const char *args) {
    if (g_running.load(std::memory_order_relaxed) || g_gameThread.joinable()) {
        KLOGW("EngineStart: already running");
        return false;
    }

    g_lastError.clear();
    g_paused.store(false, std::memory_order_relaxed);

    try {
        g_gameThread = std::thread(GameThreadMain,
                                   gameDir ? std::string(gameDir) : std::string(),
                                   args ? std::string(args) : std::string());
    } catch (const std::exception &e) {
        SetLastError(e.what());
        KLOGE("failed to start game thread: %s", e.what());
        return false;
    }
    return true;
}

void EngineStop() {
    g_running.store(false, std::memory_order_relaxed);
    if (g_gameThread.joinable()) g_gameThread.join();
    g_hasSurface.store(false, std::memory_order_relaxed);
}

bool EngineIsRunning() { return g_running.load(std::memory_order_relaxed); }

// ---------------------------------------------------------------- surface

// The engine must not touch the GPU until a real ANativeWindow exists: DXVK
// creates its Vulkan swapchain from it, and there is no off-screen fallback.
bool SurfaceCreated(ANativeWindow *window) {
    if (!window) {
        SetLastError("SurfaceCreated: null ANativeWindow");
        return false;
    }
    g_hasSurface.store(true, std::memory_order_relaxed);

#ifndef KISAK_ANDROID_STUB
    // TODO(android): hand the ANativeWindow to the renderer.
    //
    // DXVK needs a WSI backend for VK_KHR_android_surface. dxvk-native's WSI
    // layer (src/wsi/) currently ships SDL2 and GLFW backends; an ANativeWindow
    // backend is the one piece that must be added there. Until it lands, this
    // is where the window pointer is stashed for the backend to pick up.
    (void)window;
#else
    (void)window;
#endif
    return true;
}

void SurfaceChanged(ANativeWindow *window, int width, int height) {
    (void)window;
    KLOGI("surface changed %dx%d", width, height);
#ifndef KISAK_ANDROID_STUB
    // TODO(android): forward a resize to the renderer (D3D device reset path).
#endif
}

void SurfaceDestroyed() {
    g_hasSurface.store(false, std::memory_order_relaxed);
    KLOGI("surface destroyed");
}

}  // namespace kisak::android

// ================================================================ JNI

namespace {

jclass g_bridgeClass = nullptr;
JavaVM *g_vm = nullptr;

template <typename Fn>
void WithJniEnv(Fn &&fn) {
    if (!g_vm) return;
    JNIEnv *env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) return;
    fn(env);
}

}  // namespace

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    g_vm = vm;
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) return -1;

    jclass cls = env->FindClass("com/kisak/cod/input/InputBridge");
    if (cls) g_bridgeClass = reinterpret_cast<jclass>(env->NewGlobalRef(cls));
    env->DeleteLocalRef(cls);

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *) {
    WithJniEnv([](JNIEnv *env) {
        if (g_bridgeClass) env->DeleteGlobalRef(g_bridgeClass);
        g_bridgeClass = nullptr;
    });
    g_vm = nullptr;
}

JNIEXPORT jboolean JNICALL
Java_com_kisak_cod_input_InputBridge_nativeInit(JNIEnv *env, jclass, jstring gameDir, jstring args) {
    const char *d = gameDir ? env->GetStringUTFChars(gameDir, nullptr) : nullptr;
    const char *a = args ? env->GetStringUTFChars(args, nullptr) : nullptr;
    const bool ok = kisak::android::EngineStart(d ? d : "", a ? a : "");
    if (d) env->ReleaseStringUTFChars(gameDir, d);
    if (a) env->ReleaseStringUTFChars(args, a);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativeShutdown(JNIEnv *, jclass) {
    kisak::android::EngineStop();
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativePumpInput(JNIEnv *, jclass) {
    kisak::android::PumpInput();
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativeSetPaused(JNIEnv *, jclass, jboolean paused) {
    kisak::android::SetPaused(paused == JNI_TRUE);
}

namespace {

// ANativeWindow_fromSurface() acquires a reference that must be released with
// ANativeWindow_release(). Forgetting this leaks one window per rotation.
ANativeWindow *g_window = nullptr;
std::mutex g_windowMutex;

ANativeWindow *AcquireWindow(JNIEnv *env, jobject surface) {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    ANativeWindow *next = ANativeWindow_fromSurface(env, surface);
    if (g_window && g_window != next) {
        ANativeWindow_release(g_window);
    }
    g_window = next;
    return g_window;
}

void ReleaseWindow() {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    if (g_window) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }
}

}  // namespace

JNIEXPORT jboolean JNICALL
Java_com_kisak_cod_input_InputBridge_nativeSurfaceCreated(JNIEnv *env, jclass, jobject surface) {
    ANativeWindow *window = AcquireWindow(env, surface);
    if (!window) {
        kisak::android::SetLastError("ANativeWindow_fromSurface failed");
        return JNI_FALSE;
    }
    return kisak::android::SurfaceCreated(window) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativeSurfaceChanged(
    JNIEnv *env, jclass, jobject surface, jint width, jint height) {
    ANativeWindow *window = AcquireWindow(env, surface);
    kisak::android::SurfaceChanged(window, width, height);
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativeSurfaceDestroyed(JNIEnv *, jclass) {
    kisak::android::SurfaceDestroyed();
    ReleaseWindow();
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativeQueueCommand(JNIEnv *env, jclass, jstring command) {
    if (!command) return;
    const char *c = env->GetStringUTFChars(command, nullptr);
    if (c) {
        kisak::android::QueueCommand(c);
        env->ReleaseStringUTFChars(command, c);
    }
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativeAddLook(JNIEnv *, jclass, jfloat dx, jfloat dy) {
    kisak::android::AddLook(dx, dy);
}

JNIEXPORT void JNICALL
Java_com_kisak_cod_input_InputBridge_nativeSetMoveAxis(JNIEnv *, jclass, jfloat x, jfloat y) {
    kisak::android::SetMoveAxis(x, y);
}

JNIEXPORT jboolean JNICALL
Java_com_kisak_cod_input_InputBridge_nativeIsRunning(JNIEnv *, jclass) {
    return kisak::android::EngineIsRunning() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_kisak_cod_input_InputBridge_nativeLastError(JNIEnv *env, jclass) {
    const char *err = kisak::android::LastError();
    if (!err || !*err) return nullptr;
    return env->NewStringUTF(err);
}

}  // extern "C"
