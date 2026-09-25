// win_input.h -- input interface for the POSIX port.
//
// DirectInput is gone. Touch and gamepad input arrive from the host
// (Android's TouchControlsView, or the Linux SDL front end) and are translated
// into engine key events by win_input.cpp. The rumble API is kept because the
// client calls it unconditionally; on Android it drives the vibrator through
// the host's HapticFeedback bridge, and elsewhere it is a no-op.

#ifndef _WIN_INPUT_H_
#define _WIN_INPUT_H_

#include <ui/keycodes.h>

bool IN_ControllersChanged(int inserted[], int removed[]);

bool IN_AnyButtonPressed(void);

void IN_enableRumble(void);
void IN_disableRumble(void);
bool IN_usingRumble(void);

int  IN_CreateRumbleScript(int controller, int numStates, bool deleteWhenFinished);
void IN_DeleteRumbleScript(int whichScript);
void IN_KillRumbleScript(int whichScript);
void IN_ExecuteRumbleScript(int whichScript);

bool IN_AdvanceToNextState(int whichScript);

void IN_KillRumbleScripts(int controller);
void IN_KillRumbleScripts(void);

// LWSS add
void __cdecl IN_SetForegroundWindow();
bool __cdecl IN_IsForegroundWindow();

void IN_ActivateMouse(qboolean force);
void __cdecl IN_RecenterMouse();
void __cdecl IN_SetCursorPos(tagPOINT x);
// LWSS end

#define IN_CMD_GOTO_XTIMES   -5
#define IN_CMD_GOTO          -6

#define IN_CMD_DEC_ARG2      -7
#define IN_CMD_INC_ARG2      -8
#define IN_CMD_DEC_ARG1      -9
#define IN_CMD_INC_ARG1      -10

int  IN_AddRumbleStateSpecial(int whichScript, int action, int arg1, int arg2);
void IN_KillRumbleState(int whichScript, int index);

void IN_PauseRumbling(int controller);
void IN_PauseRumbling(void);

void IN_UnPauseRumbling(int controller);
void IN_UnPauseRumbling(void);

void IN_TogglePauseRumbling(int controller);
void IN_TogglePauseRumbling(void);
int  IN_GetMainController();
void IN_SetMainController(int id);

void IN_PadUnplugged(int controller);
void IN_PadPlugged(int controller);

#define IN_MAX_JOYSTICKS 2
// Stores gamepad joystick info
struct JoystickInfo
{
    bool valid;
    float x, y;
};

// Stores gamepad id and joystick info
struct PadInfo
{
    JoystickInfo joyInfo[2];
    int padId;
};

// Buffer for gamepad info
extern PadInfo _padInfo;

bool IN_RumbleAdjust(int controller, int left, int right);
void IN_RumbleInit(void);
void IN_RumbleShutdown(void);
void IN_RumbleFrame(void);

// LWSS Add
int IN_MouseMove();
// LWSS End

#endif // END _WIN_INPUT_H_
