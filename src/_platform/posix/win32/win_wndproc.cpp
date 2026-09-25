// win_wndproc.cpp -- key translation for the POSIX/Android build.
//
// On Windows this file owns MainWndProc and the Win32 virtual-key translation
// tables. The host replaces the window proc entirely, but the *translation* is
// still needed: Android delivers KEYCODE_* values, Linux/SDL delivers SDL_Scancodes,
// and the engine wants its own key numbers from ui/keycodes.h.
//
// So this keeps the two pure functions -- AdustKeyForNumericKeypad and MapKey --
// and adds the host-facing entry points that turn a platform key code into an
// engine key event. MapKey is unchanged from the Win32 build because the engine
// still uses Win32 VK_* numbering internally; only the *source* of the VK code
// changed.

#include <universal/q_shared.h>
#include "win_local.h"
#include "win_input.h"

#include <ui/keycodes.h>
#include <client/client.h>
#include <qcommon/qcommon.h>

// ---------------------------------------------------------------------------
// VK_* subset
//
// ui/keycodes.h and the client's key state arrays are indexed by these, so the
// numbering has to stay the Win32 one. Only the codes the engine actually
// binds are listed; the rest are never produced by the host.
// ---------------------------------------------------------------------------

#ifndef VK_LBUTTON
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_BACK    0x08
#define VK_TAB     0x09
#define VK_RETURN  0x0D
#define VK_SHIFT   0x10
#define VK_CONTROL 0x11
#define VK_MENU    0x12
#define VK_PAUSE   0x13
#define VK_ESCAPE  0x1B
#define VK_SPACE   0x20
#define VK_PRIOR   0x21
#define VK_NEXT    0x22
#define VK_END     0x23
#define VK_HOME    0x24
#define VK_LEFT    0x25
#define VK_UP      0x26
#define VK_RIGHT   0x27
#define VK_DOWN    0x28
#define VK_INSERT  0x2D
#define VK_DELETE  0x2E
#define VK_F1      0x70
#endif

// ---------------------------------------------------------------------------
// Key translation
// ---------------------------------------------------------------------------

int AdustKeyForNumericKeypad(int key)
{
    // Verbatim from the Win32 build: the numpad has to be distinguishable from
    // the main block, and the engine binds them separately.
    switch (key)
    {
    case VK_HOME:  return '7';
    case VK_UP:    return '8';
    case VK_PRIOR: return '9';
    case VK_LEFT:  return '4';
    case VK_INSERT:return '5';
    case VK_RIGHT: return '6';
    case VK_END:   return '1';
    case VK_DOWN:  return '2';
    case VK_NEXT:  return '3';
    case VK_DELETE:return ',';
    default:       return key;
    }
}

int MapKey(int key)
{
    // Verbatim from the Win32 build. This is the last step before the engine
    // sees the key, and it is what makes the numpad behave like the Win32
    // console does.
    return AdustKeyForNumericKeypad(key);
}

// ---------------------------------------------------------------------------
// Host key translation
//
// The host passes its own key code plus a flag for whether the key is a
// "character" (in which case the engine wants the character value, not the
// platform code). These are the only two shapes the host needs to know about.
// ---------------------------------------------------------------------------

// Android's KEYCODE_* values, from android/view/KeyEvent.java. Only the ones
// the engine binds are mapped; anything unmapped is dropped rather than
// producing a phantom keypress.
enum KisakAndroidKeycode
{
    AKC_A = 29, AKC_B = 30, AKC_C = 31, AKC_D = 32, AKC_E = 33, AKC_F = 34,
    AKC_G = 35, AKC_H = 36, AKC_I = 37, AKC_J = 38, AKC_K = 39, AKC_L = 40,
    AKC_M = 41, AKC_N = 42, AKC_O = 43, AKC_P = 44, AKC_Q = 45, AKC_R = 46,
    AKC_S = 47, AKC_T = 48, AKC_U = 49, AKC_V = 50, AKC_W = 51, AKC_X = 52,
    AKC_Y = 53, AKC_Z = 54,
    AKC_0 = 7, AKC_1 = 8, AKC_2 = 9, AKC_3 = 10, AKC_4 = 11,
    AKC_5 = 12, AKC_6 = 13, AKC_7 = 14, AKC_8 = 15, AKC_9 = 16,
    AKC_GRAVE = 68,
    AKC_MINUS = 69, AKC_EQUALS = 70, AKC_LEFT_BRACKET = 71, AKC_RIGHT_BRACKET = 72,
    AKC_BACKSLASH = 73, AKC_SEMICOLON = 74, AKC_APOSTROPHE = 75,
    AKC_SLASH = 76, AKC_AT = 77, AKC_PLUS = 81,
    AKC_NUM = 78, AKC_PERIOD = 56, AKC_COMMA = 55, AKC_SPACE = 62,
    AKC_TAB = 61, AKC_ENTER = 66, AKC_DEL = 67, AKC_FORWARD_DEL = 112,
    AKC_ESCAPE = 111, AKC_MOVE_HOME = 122, AKC_MOVE_END = 123,
    AKC_PAGE_UP = 92, AKC_PAGE_DOWN = 93,
    AKC_DPAD_UP = 19, AKC_DPAD_DOWN = 20, AKC_DPAD_LEFT = 21, AKC_DPAD_RIGHT = 22,
    AKC_F1 = 131, AKC_F2 = 132, AKC_F3 = 133, AKC_F4 = 134,
    AKC_F5 = 135, AKC_F6 = 136, AKC_F7 = 137, AKC_F8 = 138,
    AKC_F9 = 139, AKC_F10 = 140, AKC_F11 = 141, AKC_F12 = 142,
    AKC_SHIFT_LEFT = 59, AKC_SHIFT_RIGHT = 60,
    AKC_CTRL_LEFT = 113, AKC_CTRL_RIGHT = 114,
    AKC_ALT_LEFT = 57, AKC_ALT_RIGHT = 58,
};

static int Kisak_AndroidToWin32(int code)
{
    // Letters
    if (code >= AKC_A && code <= AKC_Z)
        return 'A' + (code - AKC_A);
    // Digits
    if (code >= AKC_0 && code <= AKC_9)
        return '0' + (code - AKC_0);

    switch (code)
    {
    case AKC_GRAVE:         return '`';
    case AKC_MINUS:         return '-';
    case AKC_EQUALS:        return '=';
    case AKC_LEFT_BRACKET:  return '[';
    case AKC_RIGHT_BRACKET: return ']';
    case AKC_BACKSLASH:     return '\\';
    case AKC_SEMICOLON:     return ';';
    case AKC_APOSTROPHE:    return '\'';
    case AKC_SLASH:         return '/';
    case AKC_AT:            return '@';
    case AKC_PLUS:          return '+';
    case AKC_NUM:           return '#';
    case AKC_PERIOD:        return '.';
    case AKC_COMMA:         return ',';
    case AKC_SPACE:         return VK_SPACE;
    case AKC_TAB:           return VK_TAB;
    case AKC_ENTER:         return VK_RETURN;
    case AKC_DEL:           return VK_BACK;
    case AKC_FORWARD_DEL:   return VK_DELETE;
    case AKC_ESCAPE:        return VK_ESCAPE;
    case AKC_MOVE_HOME:     return VK_HOME;
    case AKC_MOVE_END:      return VK_END;
    case AKC_PAGE_UP:       return VK_PRIOR;
    case AKC_PAGE_DOWN:     return VK_NEXT;
    case AKC_DPAD_UP:       return VK_UP;
    case AKC_DPAD_DOWN:     return VK_DOWN;
    case AKC_DPAD_LEFT:     return VK_LEFT;
    case AKC_DPAD_RIGHT:    return VK_RIGHT;
    case AKC_F1:  return VK_F1;
    case AKC_F2:  return VK_F1 + 1;
    case AKC_F3:  return VK_F1 + 2;
    case AKC_F4:  return VK_F1 + 3;
    case AKC_F5:  return VK_F1 + 4;
    case AKC_F6:  return VK_F1 + 5;
    case AKC_F7:  return VK_F1 + 6;
    case AKC_F8:  return VK_F1 + 7;
    case AKC_F9:  return VK_F1 + 8;
    case AKC_F10: return VK_F1 + 9;
    case AKC_F11: return VK_F1 + 10;
    case AKC_F12: return VK_F1 + 11;
    case AKC_SHIFT_LEFT:
    case AKC_SHIFT_RIGHT:   return VK_SHIFT;
    case AKC_CTRL_LEFT:
    case AKC_CTRL_RIGHT:    return VK_CONTROL;
    case AKC_ALT_LEFT:
    case AKC_ALT_RIGHT:     return VK_MENU;
    default:                return 0;
    }
}

// The engine wants a character for printable keys and a VK code otherwise.
// `pressed` selects between SE_KEY and SE_CHAR.
extern "C" void kisak_host_key_event(int platformCode, bool pressed)
{
    int vk = Kisak_AndroidToWin32(platformCode);
    if (!vk) return;

    bool printable = (vk >= ' ' && vk <= '~');
    if (printable)
    {
        Sys_QueEvent(0, pressed ? SE_CHAR : SE_CHAR, MapKey(vk), 0, 0, 0);
        // SE_CHAR is edge-triggered on press only; release is implicit.
        if (pressed)
            Sys_QueEvent(0, SE_KEY, MapKey(vk), 1, 0, 0);
        else
            Sys_QueEvent(0, SE_KEY, MapKey(vk), 0, 0, 0);
        return;
    }

    Sys_QueEvent(0, SE_KEY, MapKey(vk), pressed ? 1 : 0, 0, 0);
}

// Mouse buttons, for a desktop build with a real pointer. Button is 0/1/2.
extern "C" void kisak_host_mouse_button(int button, bool pressed)
{
    static const int vkForButton[3] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
    if (button < 0 || button > 2) return;
    Sys_QueEvent(0, SE_KEY, vkForButton[button], pressed ? 1 : 0, 0, 0);
}

// ---------------------------------------------------------------------------
// MainWndProc
//
// Kept so the engine's declaration in win_local.h resolves and any code that
// takes its address still links. It is never called: there is no window to
// receive messages from.
// ---------------------------------------------------------------------------

LRESULT WINAPI MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd; (void)uMsg; (void)wParam; (void)lParam;
    return 0;
}

bool IsNumLockAffectedVK(int vk) { (void)vk; return false; }

void VID_AppActivate(bool active)
{
    // The renderer is told through Sys_HostSetActive; nothing to do here.
    (void)active;
}
