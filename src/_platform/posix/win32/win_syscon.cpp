// win_syscon.cpp -- the engine console for the POSIX/Android build.
//
// On Windows this file creates a real console window with an edit control and
// routes the engine's output into it. There is no window here, so the console
// degenerates to "write to stderr", which is exactly where logcat reads from on
// Android and where a terminal reads from on Linux.
//
// Sys_ConsoleInput matters more than it looks: the engine calls it every frame
// while the console is down, and the Win32 build returns a line when the user
// presses Enter. There is no keyboard here in the console sense -- the touch UI
// has its own command entry -- so it always returns null, which is the same
// answer the Win32 build gives when nothing was typed.

#include <universal/q_shared.h>
#include "win_local.h"

#include <client/client.h>
#include <qcommon/qcommon.h>

static char s_errorText[4100];

void __cdecl Sys_CreateConsole(void)
{
    // Nothing to allocate. The engine's Con_* channels all write through
    // Sys_Print, which goes to stderr.
}

void __cdecl Sys_DestroyConsole(void) {}

void __cdecl Sys_ShowConsole()
{
    // The host shows its own console overlay; there is no window to raise.
}

char *Sys_ConsoleInput(void)
{
    return nullptr;
}

void Conbuf_AppendText(const char *msg)
{
    if (!msg) return;
    fputs(msg, stderr);
    fflush(stderr);
}

void Conbuf_AppendTextInMainThread(const char *msg)
{
    // On Windows this posts a message so the UI thread does the append. There
    // is only one writer here -- the game thread -- and stderr is already
    // line-buffered, so appending directly is equivalent and avoids a copy.
    Conbuf_AppendText(msg);
}

void Sys_SetErrorText(const char *buf)
{
    if (!buf) return;
    I_strncpyz(s_errorText, buf, sizeof(s_errorText));
}
