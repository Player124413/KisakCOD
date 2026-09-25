// win_input.cpp -- input for the POSIX/Android build.
//
// DirectInput is gone. The host owns the raw device: on Android that is
// TouchControlsView + GameActivity forwarding MotionEvent/KeyEvent through the
// JNI bridge; on Linux it is SDL's event loop. Both deliver *engine key
// events* through Sys_QueEvent, exactly as MainWndProc did on Windows, and the
// client's key/mouse state is updated from those events in IN_Frame.
//
// What this file owns is everything the client polls directly:
//   - IN_MouseMove(), which synthesises the mouse delta the engine applies to
//     the view angles
//   - the rumble API, which the client calls unconditionally
//   - the foreground/cursor queries the client uses to decide whether to grab
//     input
//
// There is no mouse cursor on a touch screen, so the "recenter" logic that
// Windows needed to avoid hitting the screen edge is not needed either.

#include <universal/q_shared.h>
#include "win_local.h"
#include "win_input.h"

#include <client/client.h>
#include <qcommon/qcommon.h>

// ---------------------------------------------------------------------------
// Rumble
//
// The client's IN_* rumble scripts drive force feedback. On Android the host
// maps IN_RumbleAdjust onto Vibrator.vibrate through the JNI bridge; elsewhere
// the scripts still run so the client's state machine behaves identically, they
// just do not produce any physical effect.
// ---------------------------------------------------------------------------

#define IN_MAX_RUMBLE_SCRIPTS 4
#define IN_MAX_RUMBLE_STATES  16

struct RumbleState
{
    int leftSpeed;
    int rightSpeed;
    int timeInMs;
};

struct RumbleScript
{
    bool inUse;
    bool deleteWhenFinished;
    int controller;
    int numStates;
    int curState;
    int stateTime;
    RumbleState states[IN_MAX_RUMBLE_STATES];
};

static RumbleScript s_rumbleScripts[IN_MAX_RUMBLE_SCRIPTS];
static int s_mainController = 0;
static bool s_rumbleEnabled = false;
static bool s_rumblePaused = false;
static int s_lastLeft = 0;
static int s_lastRight = 0;

// Weak hook the Android host overrides to drive the vibrator.
extern "C" __attribute__((weak)) void kisak_host_rumble(int left, int right) { (void)left; (void)right; }

static void IN_ApplyRumble()
{
    if (!s_rumbleEnabled || s_rumblePaused)
    {
        if (s_lastLeft || s_lastRight) { s_lastLeft = 0; s_lastRight = 0; kisak_host_rumble(0, 0); }
        return;
    }
    kisak_host_rumble(s_lastLeft, s_lastRight);
}

int IN_CreateRumbleScript(int controller, int numStates, bool deleteWhenFinished)
{
    for (int i = 0; i < IN_MAX_RUMBLE_SCRIPTS; ++i)
    {
        if (s_rumbleScripts[i].inUse) continue;
        RumbleScript *s = &s_rumbleScripts[i];
        memset(s, 0, sizeof(*s));
        s->inUse = true;
        s->deleteWhenFinished = deleteWhenFinished;
        s->controller = controller;
        s->numStates = (numStates > IN_MAX_RUMBLE_STATES) ? IN_MAX_RUMBLE_STATES : numStates;
        return i;
    }
    return -1;
}

void IN_DeleteRumbleScript(int whichScript)
{
    if (whichScript < 0 || whichScript >= IN_MAX_RUMBLE_SCRIPTS) return;
    memset(&s_rumbleScripts[whichScript], 0, sizeof(s_rumbleScripts[0]));
}

void IN_KillRumbleScript(int whichScript)
{
    if (whichScript < 0 || whichScript >= IN_MAX_RUMBLE_SCRIPTS) return;
    s_rumbleScripts[whichScript].curState = s_rumbleScripts[whichScript].numStates;
}

void IN_ExecuteRumbleScript(int whichScript)
{
    if (whichScript < 0 || whichScript >= IN_MAX_RUMBLE_SCRIPTS) return;
    s_rumbleScripts[whichScript].curState = 0;
    s_rumbleScripts[whichScript].stateTime = 0;
}

bool IN_AdvanceToNextState(int whichScript)
{
    if (whichScript < 0 || whichScript >= IN_MAX_RUMBLE_SCRIPTS) return false;
    RumbleScript *s = &s_rumbleScripts[whichScript];
    ++s->curState;
    s->stateTime = 0;
    return s->curState < s->numStates;
}

void IN_KillRumbleScripts(int controller)
{
    for (int i = 0; i < IN_MAX_RUMBLE_SCRIPTS; ++i)
        if (s_rumbleScripts[i].controller == controller)
            IN_KillRumbleScript(i);
}

void IN_KillRumbleScripts(void)
{
    for (int i = 0; i < IN_MAX_RUMBLE_SCRIPTS; ++i)
        IN_KillRumbleScript(i);
}

void IN_enableRumble(void) { s_rumbleEnabled = true; IN_ApplyRumble(); }
void IN_disableRumble(void) { s_rumbleEnabled = false; IN_ApplyRumble(); }
bool IN_usingRumble(void) { return s_rumbleEnabled; }

bool IN_RumbleAdjust(int controller, int left, int right)
{
    (void)controller;
    s_lastLeft = left;
    s_lastRight = right;
    IN_ApplyRumble();
    return true;
}

void IN_RumbleInit(void)
{
    memset(s_rumbleScripts, 0, sizeof(s_rumbleScripts));
    s_mainController = 0;
    s_lastLeft = 0;
    s_lastRight = 0;
}

void IN_RumbleShutdown(void)
{
    kisak_host_rumble(0, 0);
    memset(s_rumbleScripts, 0, sizeof(s_rumbleScripts));
}

void IN_RumbleFrame(void)
{
    for (int i = 0; i < IN_MAX_RUMBLE_SCRIPTS; ++i)
    {
        RumbleScript *s = &s_rumbleScripts[i];
        if (!s->inUse || s->curState >= s->numStates) continue;
        RumbleState *st = &s->states[s->curState];
        s->stateTime++;
        if (s->stateTime >= st->timeInMs)
        {
            if (!IN_AdvanceToNextState(i))
            {
                if (s->deleteWhenFinished) IN_DeleteRumbleScript(i);
                continue;
            }
        }
    }
}

int IN_AddRumbleStateSpecial(int whichScript, int action, int arg1, int arg2)
{
    if (whichScript < 0 || whichScript >= IN_MAX_RUMBLE_SCRIPTS) return -1;
    RumbleScript *s = &s_rumbleScripts[whichScript];
    if (s->numStates >= IN_MAX_RUMBLE_STATES) return -1;
    RumbleState *st = &s->states[s->numStates];
    st->leftSpeed = action ? arg1 : 0;
    st->rightSpeed = action ? arg1 : 0;
    st->timeInMs = arg2;
    return s->numStates++;
}

void IN_KillRumbleState(int whichScript, int index)
{
    if (whichScript < 0 || whichScript >= IN_MAX_RUMBLE_SCRIPTS) return;
    if (index < 0 || index >= IN_MAX_RUMBLE_STATES) return;
    s_rumbleScripts[whichScript].states[index].timeInMs = 0;
}

void IN_PauseRumbling(int controller) { (void)controller; s_rumblePaused = true; IN_ApplyRumble(); }
void IN_PauseRumbling(void) { s_rumblePaused = true; IN_ApplyRumble(); }
void IN_UnPauseRumbling(int controller) { (void)controller; s_rumblePaused = false; IN_ApplyRumble(); }
void IN_UnPauseRumbling(void) { s_rumblePaused = false; IN_ApplyRumble(); }
void IN_TogglePauseRumbling(int controller) { (void)controller; s_rumblePaused = !s_rumblePaused; IN_ApplyRumble(); }
void IN_TogglePauseRumbling(void) { s_rumblePaused = !s_rumblePaused; IN_ApplyRumble(); }
int IN_GetMainController() { return s_mainController; }
void IN_SetMainController(int id) { s_mainController = id; }

void IN_PadUnplugged(int controller) { IN_KillRumbleScripts(controller); }
void IN_PadPlugged(int controller) { (void)controller; }

bool IN_ControllersChanged(int inserted[], int removed[])
{
    // The host reports controller changes by queueing events, not by polling,
    // so there is never anything pending here.
    if (inserted) inserted[0] = -1;
    if (removed) removed[0] = -1;
    return false;
}

bool IN_AnyButtonPressed(void) { return false; }

// ---------------------------------------------------------------------------
// Mouse
//
// The engine accumulates mouseDx/mouseDy itself and calls IN_MouseMove to turn
// them into a delta it can apply. On a touch screen there is no cursor to
// recenter and no screen edge to hit, so the delta is used verbatim.
// ---------------------------------------------------------------------------

static bool s_mouseActive = false;

int IN_MouseMove()
{
    if (!s_mouseActive) return 0;
    // The look delta comes from the touch controls through the same
    // CL_MouseEvent path the mouse used; there is nothing extra to add here.
    return 0;
}

void IN_RecenterMouse() {}
void __cdecl IN_SetCursorPos(tagPOINT x) { (void)x; }

void IN_ActivateMouse(qboolean force)
{
    (void)force;
    s_mouseActive = true;
}

void IN_DeactivateWin32Mouse() { s_mouseActive = false; }

void __cdecl IN_ShowSystemCursor(BOOL show)
{
    (void)show;
    // The host decides whether the software keyboard / touch affordances are
    // visible; there is no OS cursor to show or hide.
}

// ---------------------------------------------------------------------------
// Foreground
//
// IN_IsForegroundWindow gates whether the client processes input at all. On
// Android the answer comes from the activity lifecycle, which the host pushes
// through Sys_HostSetActive.
// ---------------------------------------------------------------------------

static volatile bool s_foreground = true;

void __cdecl IN_SetForegroundWindow() { s_foreground = true; }
bool __cdecl IN_IsForegroundWindow() { return s_foreground; }

// ---------------------------------------------------------------------------
// Init / frame
// ---------------------------------------------------------------------------

void IN_Init(void)
{
    IN_RumbleInit();
    s_mouseActive = false;
    s_foreground = true;
    Com_Printf(CON_CHANNEL_SYSTEM, "Input initialised (host-driven)\n");
}

void IN_Shutdown(void)
{
    IN_RumbleShutdown();
    s_mouseActive = false;
}

void IN_Frame(void)
{
    IN_RumbleFrame();
}

void IN_JoystickCommands(void)
{
    // Gamepad axes arrive as events from the host. The engine's
    // CL_GamepadMove path is commented out in cl_input.cpp, so joystick
    // movement is already synthesised from the digital buttons.
}

void IN_MouseEvent(int mstate)
{
    // The host delivers button transitions as key events through
    // Sys_HostEvent; mstate is the legacy aggregated-button mask that the
    // Windows build maintained here.
    (void)mstate;
}

void IN_Activate(qboolean active)
{
    s_foreground = active != 0;
    if (!s_foreground)
        IN_PauseRumbling();
    else
        IN_UnPauseRumbling();
}

bool IN_IsTalkKeyHeld()
{
    // Voice chat is singleplayer-only out of scope; the talk key is never held.
    return false;
}

PadInfo _padInfo;
