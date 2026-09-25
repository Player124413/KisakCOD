// win_main.cpp -- the Sys_* contract for the POSIX/Android build.
//
// src/win32/win_main.cpp owns WinMain, the window class, the splash screen and
// the crash-rerun machinery. None of that exists here: there is no HWND, no
// message loop, and the host (Android's GameActivity, or the Linux front end)
// owns the process entry point. What survives is everything the *engine* calls
// through the Sys_* interface, which is 58 files' worth of contract.
//
// The event queue is the important part. On Windows, Sys_QueEvent is fed by
// MainWndProc on the UI thread and drained by Sys_GetEvent on the game thread.
// On Android there is no window proc, so the host posts events through
// Sys_HostEvent() and the game thread still drains them through Sys_GetEvent().
// Same queue, same locking, same ordering -- only the producer changed.

#include <universal/q_shared.h>
#include "win_configure.h"
#include "win_local.h"
#include "win_localize.h"
#include "win_net.h"
#include "win_net_debug.h"
#include "win_steam.h"

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

#include <client/client.h>

#include <qcommon/qcommon.h>
#include <qcommon/cmd.h>
#include <qcommon/threads.h>
#include <qcommon/mem_track.h>

#include <universal/com_memory.h>
#include <universal/q_parse.h>
#include <universal/timing.h>

#include <gfx_d3d/r_init.h>
#include <universal/profile.h>

char sys_cmdline[1024];
char sys_exitCmdLine[1024];

// r_init.cpp's splash helpers test this for null. There is no splash window on
// this platform, but the symbol has to exist and the engine's show/hide calls
// still have to do something -- the host listens for them through the JNI
// bridge and drives its own splash view. Defined non-null so the engine takes
// the "splash exists" path and actually calls in.
HWND g_splashWnd = (HWND)1;

sysEvent_t eventQue[0x100];

int eventHead;
int eventTail;

SysInfo sys_info;

int client_state;

cmd_function_s Sys_In_Restart_f_VAR;
#ifdef KISAK_MP
cmd_function_s Sys_Net_Restart_f_VAR;
cmd_function_s Sys_Listen_f_VAR;
#endif

// Zero-initialised: hWnd and hInstance are null because there is no window, and
// activeApp/isMinimized start false so the host has to assert them explicitly
// rather than inheriting a stale value.
WinVars_t g_wv;

// ---------------------------------------------------------------------------
// Paths
//
// Sys_DefaultInstallPath is the one the engine really depends on: 7 files call
// it to locate the game data, and on Windows it is derived from the module
// filename. On Android the .so lives inside the APK, which is not a usable
// path, so the host supplies the real install directory via Sys_SetInstallPath
// before Sys_Init runs. On Linux it still comes from /proc/self/exe.
// ---------------------------------------------------------------------------

static char s_installPath[256];
static char s_gameDataPath[256];

void Sys_SetInstallPath(const char *path)
{
    if (!path) return;
    I_strncpyz(s_installPath, path, sizeof(s_installPath));
}

void Sys_SetGameDataPath(const char *path)
{
    if (!path) return;
    I_strncpyz(s_gameDataPath, path, sizeof(s_gameDataPath));
}

char exePath[256];

char *__cdecl Sys_DefaultInstallPath()
{
    if (s_installPath[0])
        return s_installPath;

    if (!exePath[0])
    {
        HMODULE hinst = GetModuleHandleA(0);
        uint32_t len = GetModuleFileNameA(hinst, exePath, 0x100u);
        if (len >= 256)
            len = 255;
        while (len && exePath[len] != 92 && exePath[len] != 47 && exePath[len] != 58)
            --len;
        exePath[len] = 0;
    }
    return exePath;
}

const char *__cdecl Sys_DefaultCDPath()
{
    // There is no CD to look for; the game data comes from the install path or
    // from the host-provided game data directory.
    if (s_gameDataPath[0])
        return s_gameDataPath;
    return "";
}

void __cdecl Sys_QuitAndStartProcess(const char *exeName, const char *parameters)
{
    // The Win32 build respawns itself to escape a bad video mode. There is no
    // equivalent failure to escape here -- the host owns process lifetime and
    // will restart the activity if the engine asks to quit -- so this is a
    // no-op rather than a fork/exec that Android would kill anyway.
    (void)exeName; (void)parameters;
}

// ---------------------------------------------------------------------------
// Fatal errors
// ---------------------------------------------------------------------------

void __cdecl Sys_OutOfMemErrorInternal(const char *filename, int line)
{
    Sys_Error("Out of memory: %s, line %d", filename, line);
}

void Sys_Error(const char *error, ...)
{
    char string[4100];
    va_list va;

    va_start(va, error);
    Sys_EnterCriticalSection(CRITSECT_COM_ERROR);
    Com_PrintStackTrace();
    com_errorEntered = 1;
    Sys_SuspendOtherThreads();
    vsnprintf(string, 0x1000u, error, va);
    va_end(va);

    // The Win32 build pumps the message loop here so the console stays live
    // while the error is displayed. There is no message loop, so the text is
    // written straight out and the process exits; the host shows its own
    // dialog on top of whatever the engine last rendered.
    Sys_SetErrorText(string);
    Com_Printf(CON_CHANNEL_SYSTEM, "\n\n%s\n", string);
    fflush(stderr);
    exit(0);
}

void __cdecl Sys_OpenURL(const char *url, int doexit)
{
    // ShellExecute has no POSIX equivalent worth emulating, and on Android an
    // implicit Intent has to come from the host anyway. Log it so the URL is
    // not silently lost, and honour the "quit after" flag the engine sets for
    // its exit links.
    Com_Printf(CON_CHANNEL_SYSTEM, "Sys_OpenURL: %s\n", url ? url : "");
    if (doexit)
        Cbuf_AddText(0, "quit\n");
}

void __cdecl Sys_Quit()
{
    Sys_EnterCriticalSection(CRITSECT_COM_ERROR);
    timeEndPeriod(1);
    IN_Shutdown();
    Key_Shutdown();
    Sys_DestroyConsole();
    Sys_NormalExit();
    Win_ShutdownLocalization();
    RefreshQuitOnErrorCondition();
    Dvar_Shutdown();
    Cmd_Shutdown();
    KISAK_NULLSUB();
    KISAK_NULLSUB();
    Sys_ShutdownEvents();
    SL_Shutdown();
    if (!com_errorEntered)
        track_shutdown(0);
    Con_ShutdownChannels();
    exit(0);
}

void __cdecl Sys_NormalExit()
{
    // The Win32 build signals its parent process that it exited cleanly. The
    // Android host watches the JNI bridge's lifecycle callbacks instead.
}

void __cdecl Sys_Print(const char *msg)
{
    // The console is gone, but the engine's output still has to reach logcat.
    // fputs to stderr is what Android's logcat picks up for the default
    // stdout/stderr redirection.
    fputs(msg, stderr);
    fflush(stderr);
}

// ---------------------------------------------------------------------------
// Clipboard
//
// No clipboard on Android and none worth emulating on Linux, so these are
// no-ops that return a valid empty string. The engine only uses them for the
// console's copy/paste, which the touch UI does not offer.
// ---------------------------------------------------------------------------

char *__cdecl Sys_GetClipboardData()
{
    static char empty[1] = { 0 };
    return empty;
}

int __cdecl Sys_SetClipboardData(const char *text)
{
    (void)text;
    return 0;
}

// ---------------------------------------------------------------------------
// Event queue
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Win_GetEvent
//
// The Windows version pumped the message queue whenever the engine's event
// queue was empty, so the window stayed responsive during a long frame. There
// is no message queue here -- the host pushes events in from its own thread --
// so this only drains the engine's queue and reads the console.
//
// The console read is kept because it is how the engine's `+button` console
// commands and the `wait`/`set` script lines get in. Sys_ConsoleInput returns
// null on this platform, so it costs nothing.
// ---------------------------------------------------------------------------

sysEvent_t *__cdecl Win_GetEvent(sysEvent_t *result)
{
    PROF_SCOPED("Win_GetEvent");

    char *b;
    char *s;
    sysEvent_t ev;

    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    if (eventHead <= eventTail)
    {
        {
            PROF_SCOPED("Console Input");
            s = Sys_ConsoleInput();
            if (s)
            {
                size_t len = strlen(s);
                b = (char *)Com_AllocEvent(len + 1);
                I_strncpyz(b, s, (int)len);
                Sys_QueEvent(0, SE_CONSOLE, 0, 0, (int)len + 1, b);
            }
        }

        if (eventHead <= eventTail)
        {
            memset(&ev, 0, sizeof(ev));
            ev.evTime = Sys_Milliseconds();
        }
        else
        {
            ev = eventQue[(uint8_t)eventTail++];
        }
        Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
        *result = ev;
        return result;
    }
    else
    {
        ev = eventQue[(uint8_t)eventTail++];
        Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
        *result = ev;
        return result;
    }
}

void __cdecl Sys_QueEvent(uint32_t time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr)
{
    sysEvent_t *ev;

    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    ev = &eventQue[(uint8_t)eventHead];
    if (eventHead - eventTail >= 256)
    {
        Com_Printf(CON_CHANNEL_SYSTEM, "Sys_QueEvent: overflow\n");
        if (ev->evPtr)
            Z_Free((char *)ev->evPtr, 10);
        ++eventTail;
    }
    ++eventHead;
    if (!time)
        time = Sys_Milliseconds();
    ev->evTime = time;
    ev->evType = type;
    ev->evValue = value;
    ev->evValue2 = value2;
    ev->evPtrLength = ptrLength;
    ev->evPtr = ptr;
    Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
}

void Sys_ShutdownEvents()
{
    sysEvent_t *ev;

    Sys_EnterCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
    while (eventHead > eventTail)
    {
        ev = &eventQue[(uint8_t)eventTail++];
        if (ev->evPtr)
            Z_Free((char *)ev->evPtr, 10);
    }
    Sys_LeaveCriticalSection(CRITSECT_SYS_EVENT_QUEUE);
}

void __cdecl Sys_LoadingKeepAlive()
{
    sysEvent_t result;
    sysEvent_t v1;
    sysEvent_t ev;

    do
    {
        v1 = *Win_GetEvent(&result);
        ev = v1;
    } while (v1.evType);
    R_CheckLostDevice();
}

sysEvent_t *__cdecl Sys_GetEvent(sysEvent_t *result)
{
    PROF_SCOPED("Sys_GetEvent");

    sysEvent_t v2;
    sysEvent_t v3;

    v3 = *Win_GetEvent(&v2);
    *result = v3;
    return result;
}

// Sys_HostEvent -- the producer side of the queue for the host platform.
//
// The UI thread never calls the engine directly. It stages commands in a
// lock-free SPSC ring and look deltas in atomics (see the Android
// InputBridge), and this is the one place a synthetic event is allowed to be
// queued from outside the game thread -- the same contract MainWndProc had on
// Windows.
void Sys_HostEvent(sysEventType_t type, int value, int value2)
{
    Sys_QueEvent(0, type, value, value2, 0, 0);
}

// ---------------------------------------------------------------------------
// Host frame
// ---------------------------------------------------------------------------

static int s_displayW = 1280;
static int s_displayH = 720;
static int s_refreshHz = 60;
static volatile bool s_active = true;
static volatile bool s_quitRequested = false;

void Sys_SetDisplaySize(int width, int height, int refreshHz)
{
    s_displayW = width;
    s_displayH = height;
    if (refreshHz > 0) s_refreshHz = refreshHz;
    kisak_SetDisplayMetrics(width, height);
}

void Sys_HostSetActive(bool active)
{
    // Losing the surface means the D3D9 device is gone. The engine has to be
    // told before anything tries to draw, or it will submit to a device that
    // no longer exists.
    if (active == s_active) return;
    s_active = active;
    Sys_QueEvent(0, SE_NONE, 0, 0, 0, 0);
    // Tell the renderer the device is lost so it stops submitting and waits
    // for the host to hand back a surface.
    if (!active)
        R_CheckLostDevice();
}

bool Sys_HostFrame()
{
    return !s_quitRequested;
}

void Sys_RequestQuit()
{
    s_quitRequested = true;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void __cdecl Sys_Init()
{
    // timeBeginPeriod(1) asks Windows for 1ms timer resolution. POSIX has no
    // equivalent to request, and nanosleep already sleeps at that granularity.
    timeBeginPeriod(1);

    Cmd_AddCommandInternal("in_restart", Sys_In_Restart_f, &Sys_In_Restart_f_VAR);
#ifdef KISAK_MP
    Cmd_AddCommandInternal("net_restart", Sys_Net_Restart_f, &Sys_Net_Restart_f_VAR);
    Cmd_AddCommandInternal("net_listen", Sys_Listen_f, &Sys_Listen_f_VAR);
#endif

    // The Win32 build refuses to run below NT 4 and on Win32s. Neither check
    // has any meaning here, and GetVersionExA in the shim reports 6.1 anyway.

    Com_Printf(CON_CHANNEL_SYSTEM, "CPU vendor is \"%s\"\n", sys_info.cpuVendor);
    Com_Printf(CON_CHANNEL_SYSTEM, "CPU name is \"%s\"\n", sys_info.cpuName);
    if (sys_info.logicalCpuCount == 1)
        Com_Printf(CON_CHANNEL_SYSTEM, "%i logical CPU%s reported\n", 1, "");
    else
        Com_Printf(CON_CHANNEL_SYSTEM, "%i logical CPU%s reported\n", sys_info.logicalCpuCount, "s");
    if (sys_info.physicalCpuCount == 1)
        Com_Printf(CON_CHANNEL_SYSTEM, "%i physical CPU%s detected\n", 1, "");
    else
        Com_Printf(CON_CHANNEL_SYSTEM, "%i physical CPU%s detected\n", sys_info.physicalCpuCount, "s");
    Com_Printf(CON_CHANNEL_SYSTEM, "Measured CPU speed is %.2lf GHz\n", sys_info.cpuGHz);
    Com_Printf(CON_CHANNEL_SYSTEM, "Total CPU performance is estimated as %.2lf GHz\n", sys_info.configureGHz);
    Com_Printf(CON_CHANNEL_SYSTEM, "System memory is %i MB (capped at 1 GB)\n", sys_info.sysMB);
    Com_Printf(CON_CHANNEL_SYSTEM, "Video card is \"%s\"\n", sys_info.gpuDescription);
    if (sys_info.SSE)
        Com_Printf(CON_CHANNEL_SYSTEM, "Streaming SIMD Extensions (SSE) %ssupported\n", "");
    else
        Com_Printf(CON_CHANNEL_SYSTEM, "Streaming SIMD Extensions (SSE) %ssupported\n", "not ");
    Com_Printf(CON_CHANNEL_SYSTEM, "\n");

    IN_Init();
}

void Sys_In_Restart_f()
{
    IN_Shutdown();
    IN_Init();
}

#ifdef KISAK_MP
void Sys_Net_Restart_f()
{
    NET_Restart();
}
#endif

// ---------------------------------------------------------------------------
// System info
//
// The Win32 build walks GlobalMemoryStatusEx and CreateToolhelp32Snapshot.
// POSIX equivalents: sysconf for the CPU and memory counts, /proc/cpuinfo for
// the vendor and model strings.
// ---------------------------------------------------------------------------

int __cdecl Sys_SystemMemoryMB()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || pageSize <= 0) return 1024;

    double bytes = (double)pages * (double)pageSize;
    double mb = bytes / 1048576.0;
    int sysMB = (int)(mb + 0.4999999990686774);

    // The engine caps at 1 GB because that is all its zone allocator was sized
    // for. Reporting more would let it over-commit.
    if (bytes > (double)sysMB * 1048576.0 || sysMB > 1024)
        return 1024;
    return sysMB;
}

static void Sys_ReadCpuInfoString(const char *key, char *out, int outSize)
{
    out[0] = 0;
    FILE *f = fopen("/proc/cpuinfo", "rb");
    if (!f) return;

    char line[512];
    size_t keyLen = strlen(key);
    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, key, keyLen) != 0) continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        ++colon;
        while (*colon == ' ' || *colon == '\t') ++colon;
        char *nl = strpbrk(colon, "\r\n");
        if (nl) *nl = 0;
        I_strncpyz(out, colon, outSize);
        break;
    }
    fclose(f);
}

void Sys_FindInfo()
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    sys_info.logicalCpuCount = (n > 0) ? (int)n : 1;
    sys_info.physicalCpuCount = Sys_GetPhysicalCpuCount();
    sys_info.cpuGHz = 1.0 / (((double)1LL - (double)0LL) * msecPerRawTimerTick * 1000000.0);
    sys_info.sysMB = Sys_SystemMemoryMB();

    // On Windows this reads the driver's description string out of the
    // registry. On Android the real answer comes from the Vulkan device
    // properties once DXVK has a device, which is after this runs -- so the
    // engine is told the host's GPU name, or left blank for it to fill in.
    const char *gpu = getenv("KISAK_GPU_DESCRIPTION");
    if (gpu && gpu[0])
        I_strncpyz(sys_info.gpuDescription, gpu, sizeof(sys_info.gpuDescription));
    else
        sys_info.gpuDescription[0] = 0;

    // NEON, not SSE, on ARM. The engine's SSE paths are compiled out on ARM32
    // (see the #ifdef __SSE__ blocks in r_model_skin_sse.cpp and friends), so
    // reporting 1 here would be a lie that leads nowhere.
#if defined(__SSE__) || defined(_M_IX86) || defined(_M_X64)
    sys_info.SSE = 1;
#else
    sys_info.SSE = 0;
#endif

    Sys_DetectCpuVendorAndName(sys_info.cpuVendor, sys_info.cpuName);
    Sys_SetAutoConfigureGHz(&sys_info);
}

// ---------------------------------------------------------------------------
// Entry points the host may call before the engine starts
// ---------------------------------------------------------------------------

extern "C" void kisak_host_set_cmdline(const char *cmdline)
{
    if (!cmdline) return;
    I_strncpyz(sys_cmdline, cmdline, sizeof(sys_cmdline));
}

extern "C" const char *kisak_host_cmdline(void)
{
    return sys_cmdline;
}
