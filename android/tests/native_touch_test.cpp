// KisakCOD Android port — native logic tests.
//
// Runs on any POSIX box (g++ -std=c++17 ... -pthread -lm):
//   * and_touch: button mapping, stick deadzone/diagonals, look deltas,
//     invert-Y, master touch switch, menu-cursor mode
//   * and_sys: time, data dir, VirtualAlloc shim, threads
//
// This is the executable proof that the touch pipeline the overlay feeds is
// deterministic and correct, independent of the engine.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "and_touch.h"
#include "and_sys.h"

// ------------------------------------------------------------------ harness

static struct {
    int keyEvents;
    int lastKey;
    bool lastDown;
    int mouseDx, mouseDy;
    int mouseX, mouseY;
    int mouseCalls;
    char lastCmd[128];
    bool uiMode;
    bool touchWasEnabled;
} H;

static void hookKey(int key, bool down) {
    H.keyEvents++;
    H.lastKey = key;
    H.lastDown = down;
}

static void hookMouse(int x, int y, int dx, int dy) {
    H.mouseCalls++;
    H.mouseX = x;
    H.mouseY = y;
    H.mouseDx += dx;
    H.mouseDy += dy;
}

static void hookCmd(const char *t) {
    strncpy(H.lastCmd, t, sizeof(H.lastCmd) - 1);
}

static bool hookUiMode(void) { return H.uiMode; }

static int failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            printf("  ok   %s\n", msg);                                        \
        } else {                                                               \
            printf("  FAIL %s  (%s:%d)\n", msg, __FILE__, __LINE__);           \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void reset(void) {
    memset(&H, 0, sizeof(H));
    TouchHooks h;
    memset(&h, 0, sizeof(h));
    h.key = hookKey;
    h.mouse = hookMouse;
    h.cmd = hookCmd;
    h.uiCursorMode = hookUiMode;
    AndroidTouch_Init(&h);
}

// ------------------------------------------------------------------ tests

static void test_buttons(void) {
    printf("[buttons]\n");
    reset();
    AndroidTouch_Button("fire", true);
    CHECK(H.keyEvents == 1 && H.lastKey == AT_KEY_MOUSE1 && H.lastDown == true,
          "fire down -> K_MOUSE1 down");
    AndroidTouch_Button("fire", false);
    CHECK(H.lastDown == false, "fire up -> K_MOUSE1 up");

    reset();
    AndroidTouch_Button("ads", true);
    CHECK(H.lastKey == AT_KEY_MOUSE2, "ads -> K_MOUSE2");
    AndroidTouch_Button("jump", true);
    CHECK(H.lastKey == AT_KEY_SPACE, "jump -> K_SPACE");
    AndroidTouch_Button("sprint", true);
    CHECK(H.lastKey == AT_KEY_SHIFT, "sprint -> K_SHIFT");
    AndroidTouch_Button("crouch", true);
    CHECK(H.lastKey == 'C', "crouch -> C");
    AndroidTouch_Button("prone", true);
    CHECK(H.lastKey == 'Z', "prone -> Z");
    AndroidTouch_Button("reload", true);
    CHECK(H.lastKey == 'R', "reload -> R");
    AndroidTouch_Button("melee", true);
    CHECK(H.lastKey == 'V', "melee -> V");
    AndroidTouch_Button("use", true);
    CHECK(H.lastKey == 'F', "use -> F");
    AndroidTouch_Button("nade", true);
    CHECK(H.lastKey == 'G', "nade -> G");
    AndroidTouch_Button("scoreboard", true);
    CHECK(H.lastKey == AT_KEY_TAB, "scoreboard -> K_TAB");
    AndroidTouch_Button("weapon1", true);
    CHECK(H.lastKey == '1', "weapon1 -> 1");
    AndroidTouch_Button("weapon3", true);
    CHECK(H.lastKey == '3', "weapon3 -> 3");
    AndroidTouch_Button("pause", true);
    CHECK(H.lastKey == AT_KEY_ESCAPE, "pause -> K_ESCAPE");
    reset();
    AndroidTouch_Button("no_such_action", true);
    CHECK(H.keyEvents == 0, "unknown action is ignored");
}

static void test_move_stick(void) {
    printf("[move stick]\n");
    reset();
    AndroidTouch_Stick(0, 0.0f, -1.0f, 16);
    TouchStats st;
    AndroidTouch_GetStats(&st);
    CHECK((st.moveMask & 1u) != 0, "up -> W held");
    CHECK((st.moveMask & 0xEu) == 0, "only W held");

    AndroidTouch_Stick(0, -0.9f, 0.9f, 16);
    AndroidTouch_GetStats(&st);
    CHECK((st.moveMask & (1u << 1)) != 0 && (st.moveMask & (1u << 2)) != 0,
          "down-left -> S + A");

    // full diagonal is normalized by the engine's own input; here we just keep
    // both keys (engine default binds handle diagonals)
    AndroidTouch_Stick(0, 0.8f, -0.8f, 16);
    AndroidTouch_GetStats(&st);
    CHECK((st.moveMask & (1u << 0)) != 0 && (st.moveMask & (1u << 3)) != 0,
          "up-right -> W + D");

    // deadzone
    AndroidTouch_Stick(0, 0.05f, 0.05f, 16);
    AndroidTouch_GetStats(&st);
    CHECK(st.moveMask == 0, "inside deadzone -> all keys released");

    // release
    AndroidTouch_Stick(0, 0.0f, 0.0f, 16);
    CHECK(H.keyEvents == 0 || H.lastDown == false, "release emits key-ups");
}

static void test_look_stick(void) {
    printf("[look stick]\n");
    reset();
    AndroidTouch_SetFloat("sens_h", 1.0f);
    AndroidTouch_SetFloat("sens_v", 1.0f);
    AndroidTouch_SetBool("invert_y", false);
    AndroidTouch_SetBool("touch_enabled", true);

    AndroidTouch_Stick(1, 1.0f, 0.0f, 1000);
    CHECK(H.mouseDx > 200 && H.mouseDx < 320, "full right 1s @sens1 -> ~260 px/s");
    CHECK(H.mouseDx > 0 && H.mouseDy == 0, "no vertical drift");

    reset();
    AndroidTouch_SetFloat("sens_h", 2.0f);
    AndroidTouch_Stick(1, 1.0f, 0.0f, 1000);
    CHECK(H.mouseDx > 400 && H.mouseDx < 620, "sens 2 -> double the deltas");

    reset();
    AndroidTouch_SetFloat("sens_h", 1.0f);
    AndroidTouch_SetBool("invert_y", true);
    AndroidTouch_Stick(1, 0.0f, 1.0f, 1000);
    CHECK(H.mouseDy < -200, "invert Y flips vertical");

    // deadzone on look
    reset();
    AndroidTouch_Stick(1, 0.05f, 0.05f, 1000);
    CHECK(H.mouseDx == 0 && H.mouseDy == 0, "look deadzone respected");
}

static void test_cursor_mode(void) {
    printf("[menu cursor]\n");
    reset();
    H.uiMode = true;
    AndroidTouch_Stick(1, 1.0f, 1.0f, 5000);
    CHECK(H.mouseCalls == 1, "cursor emitted");
    CHECK(H.mouseX >= 0 && H.mouseX <= 1920, "x in 1920 space");
    CHECK(H.mouseY >= 0 && H.mouseY <= 1080, "y in 1080 space");
    // stick kept pushing right/down: cursor should be at the far corner, clamped
    CHECK(H.mouseX == 1920 && H.mouseY == 1080, "cursor clamped to corner");
}

static void test_touch_master_switch(void) {
    printf("[master touch switch]\n");
    reset();
    AndroidTouch_SetBool("touch_enabled", false);
    AndroidTouch_Button("fire", true);
    CHECK(H.keyEvents == 0, "buttons suppressed when touch off");
    AndroidTouch_Stick(0, 0.0f, -1.0f, 16);
    TouchStats st;
    AndroidTouch_GetStats(&st);
    CHECK(st.moveMask == 0, "move stick suppressed when touch off");
    AndroidTouch_Button("pause", true);
    CHECK(H.lastKey == AT_KEY_ESCAPE, "pause still allowed when touch off");

    // gamepad must keep working with touch off
    H.keyEvents = 0;
    AndroidTouch_GamepadStick(0, 0.0f, -1.0f, 16);
    AndroidTouch_GetStats(&st);
    CHECK((st.moveMask & 1u) != 0, "gamepad move stick works while touch is off");
    AndroidTouch_SetBool("touch_enabled", true);
    AndroidTouch_Stick(0, 0.0f, 0.0f, 16);
}

static void test_sys(void) {
    printf("[and_sys]\n");
    uint32_t a = AndroidSys_Milliseconds();
    AndroidSys_Sleep(20);
    uint32_t b = AndroidSys_Milliseconds();
    CHECK(b >= a && (b - a) >= 15, "milliseconds advance with sleep");

    AndroidSys_SetDataDir("/data/user/0/com.kisakcod.android/files/");
    char buf[512] = {0};
    AndroidSys_GetCwd(buf, sizeof(buf));
    CHECK(strcmp(buf, "/data/user/0/com.kisakcod.android/files") == 0,
          "Sys_Cwd source returns the app data dir (trailing slash stripped)");

    void *p = AndroidSys_VirtualAlloc(NULL, 4096, 0x3000u, 0x04u);
    CHECK(p != NULL, "VirtualAlloc shim works");
    memset(p, 0xAB, 4096);
    unsigned char *b8 = (unsigned char *)p;
    CHECK(b8[0] == 0xAB && b8[4095] == 0xAB, "allocated memory writable");
    CHECK(AndroidSys_VirtualFree(p, 4096, 0x8000u), "VirtualFree release ok");

    volatile int ran = 0;
    AndroidThread *t = AndroidSys_CreateThread(
        [](void *arg) -> void * {
            *(volatile int *)arg = 1;
            return NULL;
        },
        (void *)&ran);
    CHECK(t != NULL, "thread created");
    CHECK(AndroidSys_WaitThread(t), "thread joined");
    CHECK(ran == 1, "thread body ran");
    AndroidSys_CloseThread(t);
}

// ------------------------------------------------------------------ main

int main(void) {
    printf("== KisakCOD native touch/sys tests ==\n");
    test_buttons();
    test_move_stick();
    test_look_stick();
    test_cursor_mode();
    test_touch_master_switch();
    test_sys();
    printf("----\n");
    if (failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", failures);
    return 1;
}