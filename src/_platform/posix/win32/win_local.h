// win_local.h: platform header for the POSIX/Android port.
//
// This replaces src/win32/win_local.h, which pulls in DirectInput, winsock and
// wsipx -- none of which exist on POSIX. The interface is unchanged: 58 engine
// files include <win32/win_local.h> and depend on exactly the declarations
// below, so nothing else in the tree has to change.
//
// Differences from the Win32 original, all deliberate:
//   - <dinput.h>, <winsock.h> and <wsipx.h> are gone. Input arrives through the
//     host (Android touch / Linux evdev) and is translated into engine events
//     by win_input.cpp; the network layer calls Berkeley sockets directly.
//   - WinVars_t has no hWnd/hInstance, because on Android there is no HWND --
//     the surface is an ANativeWindow and DXVK builds its Vulkan swapchain from
//     it. The fields the engine reads (activeApp, isMinimized, recenterMouse,
//     sysMsgTime) are still here and still mean the same thing.
//   - __declspec(align(N)) becomes __attribute__((aligned(N))): GCC spells it
//     "aligned", and the generic __declspec mapping would silently drop the
//     alignment entirely.

#pragma once

#include <win32_posix.h>
#include <qcommon/qcommon.h>

#if defined(KISAK_MP)
#include <qcommon/net_chan_mp.h>
#elif defined(KISAK_SP)
#include <qcommon/net_chan.h>
#elif defined(KISAK_RADIANT)
// Radiant cannot include qcommon/net_chan.h or qcommon/msg.h -- both #error on
// anything but KISAK_SP. These local defs stand in, pinned by static_asserts so
// they cannot silently drift. netadr_t is the cod4-accurate 20-byte Q3-lineage
// layout WITH the legacy ipx[10] tail, shared with win_net.h via the
// KISAK_RADIANT_NETADR_DEFINED guard so the two stay byte-identical.
#ifndef KISAK_RADIANT_NETADR_DEFINED
#define KISAK_RADIANT_NETADR_DEFINED
struct netadr_t { int type; unsigned char ip[4]; unsigned short port; unsigned char ipx[10]; };
#endif
#ifndef KISAK_RADIANT_MSG_DEFINED
#define KISAK_RADIANT_MSG_DEFINED
struct msg_t   // == qcommon/msg.h (sizeof 0x28)
{
    int overflowed;
    int readOnly;
    unsigned char *data;
    unsigned char *splitData;
    int maxsize;
    int cursize;
    int splitSize;
    int readcount;
    int bit;
    int lastEntityRef;
};
#endif
static_assert(sizeof(netadr_t) == 20,            "radiant netadr_t must match win_net.h (cod4) layout");
static_assert(offsetof(netadr_t, ip)   == 4,     "netadr_t.ip offset");
static_assert(offsetof(netadr_t, port) == 8,     "netadr_t.port offset");
static_assert(offsetof(netadr_t, ipx)  == 10,    "netadr_t.ipx offset");
static_assert(sizeof(msg_t) == 40,               "radiant msg_t must match qcommon/msg.h layout");
static_assert(offsetof(msg_t, data)      == 8,   "msg_t.data offset");
static_assert(offsetof(msg_t, maxsize)   == 16,  "msg_t.maxsize offset");
static_assert(offsetof(msg_t, readcount) == 28,  "msg_t.readcount offset");
#endif

void IN_MouseEvent(int mstate);

void Sys_CreateConsole(void);
void Sys_DestroyConsole(void);
void __cdecl Sys_ShowConsole();

char *Sys_ConsoleInput(void);

#if defined(KISAK_MP) || defined(KISAK_SP)
void Sys_ShowIP();
bool Sys_IsLANAddress(netadr_t adr);
bool Sys_IsLANAddress_IgnoreSubnet(netadr_t adr);

struct netadr_t;
struct msg_t;

qboolean Sys_GetPacket(netadr_t *net_from, msg_t *net_message);
qboolean Sys_GetBroadcastPacket(msg_t *net_message);
#endif

// Input subsystem

void IN_Init(void);
void IN_Shutdown(void);
void IN_JoystickCommands(void);

void __cdecl IN_ShowSystemCursor(BOOL show);

void IN_DeactivateWin32Mouse(void);

void IN_Activate(qboolean active);
void IN_Frame(void);

bool IN_IsTalkKeyHeld();

// window procedure -- kept for source compatibility with the Win32 build.
// On POSIX there is no HWND; the host delivers events through its own path.
LRESULT WINAPI MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Conbuf_AppendText(const char *msg);
void Conbuf_AppendTextInMainThread(const char *msg);

// LWSS: Accurate to cod4
typedef struct
{
    qboolean        activeApp;
    qboolean        isMinimized;
    qboolean        recenterMouse;

    OSVERSIONINFO   osversion;

    // when we get a windows message, we store the time off so keyboard processing
    // can know the exact time of an event
    unsigned        sysMsgTime;
} WinVars_t;

extern WinVars_t g_wv;

struct __attribute__((aligned(8))) SysInfo // sizeof=0x260
{
    long double cpuGHz;
    long double configureGHz;
    int logicalCpuCount;
    int physicalCpuCount;
    int sysMB;
    char gpuDescription[512];
    bool SSE;
    char cpuVendor[13];
    char cpuName[49];
};

#define MAX_QUED_EVENTS      256
#define MASK_QUED_EVENTS     (MAX_QUED_EVENTS - 1)

extern _RTL_CRITICAL_SECTION s_criticalSections[];

extern int client_state; // LWSS ADD. This looks similar to signonstate

#if defined(KISAK_RADIANT)
enum CriticalSection : int32_t
{
    CRITSECT_CONSOLE = 0x0,
    CRITSECT_DEBUG_SOCKET = 0x1,
    CRITSECT_COM_ERROR = 0x2,
    CRITSECT_STATMON = 0x3,
    CRITSECT_SOUND_ALLOC = 0x4,
    CRITSECT_MEM_ALLOC0 = 0x5,
    CRITSECT_MEM_ALLOC1 = 0x6,
    CRITSECT_DEBUG_LINE = 0x7,
    CRITSECT_ALLOC_MARK = 0x8,
    CRITSECT_STREAMED_SOUND = 0x9,
    CRITSECT_FAKELAG = 0xA,
    CRITSECT_CLIENT_MESSAGE = 0xB,
    CRITSECT_CLIENT_CMD = 0xC,
    CRITSECT_DOBJ_ALLOC = 0xD,
    CRITSECT_START_SERVER = 0xE,
    CRITSECT_XANIM_ALLOC = 0xF,
    CRITSECT_KEY_BINDINGS = 0x10,
    CRITSECT_FX_VIS = 0x11,
    CRITSECT_SERVER_MESSAGE = 0x12,
    CRITSECT_SCRIPT_STRING = 0x13,
    CRITSECT_MEMORY_TREE = 0x14,
    CRITSECT_ASSERT = 0x15,
    CRITSECT_SCRIPT_DEBUGGER_ALLOC = 0x16,
    CRITSECT_MISSING_ASSET = 0x17,
    CRITSECT_PHYSICS = 0x18,
    CRITSECT_LIVE = 0x19,
    CRITSECT_AUDIO_PHYSICS = 0x1A,
    CRITSECT_CINEMATIC = 0x1B,
    CRITSECT_CINEMATIC_TARGET_CHANGE = 0x1C,
    CRITSECT_FX_ALLOC = 0x1D,
    CRITSECT_NETTHREAD_OVERRIDE = 0x1E,
    CRITSECT_CBUF = 0x1F,
    CRITSECT_SYS_EVENT_QUEUE,
    CRITSECT_FATAL_ERROR,
    CRITSECT_GPU_FENCE,
    CRITSECT_COUNT,
};
#elif defined(KISAK_MP)
enum CriticalSection : int
{
    CRITSECT_CONSOLE = 0x0,
    CRITSECT_DEBUG_SOCKET = 0x1,
    CRITSECT_COM_ERROR = 0x2,
    CRITSECT_STATMON = 0x3,
    CRITSECT_DEBUG_LINE = 0x4,
    CRITSECT_ALLOC_MARK = 0x5,
    CRITSECT_SCRIPT_STRING = 0x6,
    CRITSECT_MEMORY_TREE = 0x7,
    CRITSECT_ASSERT = 0x8,
    CRITSECT_RD_BUFFER = 0x9,
    CRITSECT_SYS_EVENT_QUEUE = 0xA,
    CRITSECT_GPU_FENCE = 0xB,
    CRITSECT_FATAL_ERROR = 0xC,
    CRITSECT_SCRIPT_DEBUGGER_ALLOC = 0xD,
    CRITSECT_MISSING_ASSET = 0xE,
    CRITSECT_PHYSICS = 0xF,
    CRITSECT_LIVE = 0x10,
    CRITSECT_AUDIO_PHYSICS = 0x11,
    CRITSECT_CINEMATIC = 0x12,
    CRITSECT_CINEMATIC_TARGET_CHANGE = 0x13,
    CRITSECT_FX_ALLOC = 0x14,
    CRITSECT_CBUF = 0x15,

    CRITSECT_COUNT = 0x16,
};
#elif defined(KISAK_SP)
enum CriticalSection : int32_t
{
    CRITSECT_CONSOLE = 0x0,
    CRITSECT_DEBUG_SOCKET = 0x1,
    CRITSECT_COM_ERROR = 0x2,
    CRITSECT_STATMON = 0x3,
    CRITSECT_SOUND_ALLOC = 0x4,
    CRITSECT_MEM_ALLOC0 = 0x5,
    CRITSECT_MEM_ALLOC1 = 0x6,
    CRITSECT_DEBUG_LINE = 0x7,
    CRITSECT_ALLOC_MARK = 0x8,
    CRITSECT_STREAMED_SOUND = 0x9,
    CRITSECT_FAKELAG = 0xA,
    CRITSECT_CLIENT_MESSAGE = 0xB,
    CRITSECT_CLIENT_CMD = 0xC,
    CRITSECT_DOBJ_ALLOC = 0xD,
    CRITSECT_START_SERVER = 0xE,
    CRITSECT_XANIM_ALLOC = 0xF,
    CRITSECT_KEY_BINDINGS = 0x10,
    CRITSECT_FX_VIS = 0x11,
    CRITSECT_SERVER_MESSAGE = 0x12,
    CRITSECT_SCRIPT_STRING = 0x13,
    CRITSECT_MEMORY_TREE = 0x14,
    CRITSECT_ASSERT = 0x15,
    CRITSECT_SCRIPT_DEBUGGER_ALLOC = 0x16,
    CRITSECT_MISSING_ASSET = 0x17,
    CRITSECT_PHYSICS = 0x18,
    CRITSECT_LIVE = 0x19,
    CRITSECT_AUDIO_PHYSICS = 0x1A,
    CRITSECT_CINEMATIC = 0x1B,
    CRITSECT_CINEMATIC_TARGET_CHANGE = 0x1C,
    CRITSECT_FX_ALLOC = 0x1D,
    CRITSECT_NETTHREAD_OVERRIDE = 0x1E,
    CRITSECT_CBUF = 0x1F,

    // LWSS ADD
    CRITSECT_SYS_EVENT_QUEUE,
    CRITSECT_FATAL_ERROR,
    CRITSECT_GPU_FENCE,
    // LWSS END

    CRITSECT_COUNT,
};
#endif

struct sysEvent_t // sizeof=0x18
{
    int evTime;
    sysEventType_t evType;
    int evValue;
    int evValue2;
    int evPtrLength;
    void *evPtr;
};

struct FastCriticalSection
{
    volatile uint32_t readCount;
    volatile uint32_t writeCount;
};

void Sys_InitializeCriticalSections();
void Sys_EnterCriticalSection(int critSect);
void Sys_LeaveCriticalSection(int critSect);
void Sys_LockWrite(FastCriticalSection *critSect);
void Sys_UnlockWrite(FastCriticalSection *critSect);

int Sys_InterlockedIncrement(uint *addend);
int Sys_InterlockedDecrement(uint *addend);

void Sys_SetErrorText(const char *buf);
void Sys_Error(const char *error, ...);
void __cdecl Sys_OutOfMemErrorInternal(const char *filename, int line);
void __cdecl Sys_NormalExit();

void __cdecl Sys_OpenURL(const char *url, int doexit);
void __cdecl Sys_Quit();
void __cdecl Sys_Print(const char *msg);
char *__cdecl Sys_GetClipboardData();
int __cdecl Sys_SetClipboardData(const char *text);
void __cdecl Sys_QueEvent(uint32_t time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr);
void Sys_ShutdownEvents();
void __cdecl Sys_LoadingKeepAlive();
sysEvent_t *__cdecl Sys_GetEvent(sysEvent_t *result);
void __cdecl Sys_Init();

void Sys_In_Restart_f();
#ifdef KISAK_MP
void Sys_Net_Restart_f();
void __cdecl Sys_Listen_f();
#endif

void __cdecl Sys_Mkdir(const char *path);
BOOL __cdecl Sys_RemoveDirTree(const char *path);
int __cdecl Sys_CountFileList(char **list);
char **__cdecl Sys_ListFiles(
    const char *directory,
    const char *extension,
    const char *filter,
    int *numfiles,
    int wantsubs);
char *__cdecl Sys_Cwd();
const char *__cdecl Sys_DefaultCDPath();
char *__cdecl Sys_DefaultInstallPath();
void __cdecl Sys_QuitAndStartProcess(const char *exeName, const char *parameters);

// win_voice
bool __cdecl Voice_SendVoiceData();
bool __cdecl Voice_Init();
void __cdecl Voice_Shutdown();
double __cdecl Voice_GetVoiceLevel();
void __cdecl Voice_Playback();
int __cdecl Voice_GetLocalVoiceData();
void __cdecl Voice_IncomingVoiceData(uint8_t talker, uint8_t *data, int packetDataSize);
bool __cdecl Voice_IsClientTalking(uint32_t clientNum);
char __cdecl Voice_StartRecording();
char __cdecl Voice_StopRecording();

extern SysInfo sys_info;

// ---------------------------------------------------------------------------
// Host interface
//
// The POSIX build has no window or message pump of its own, so the host
// (Android's GameActivity, or the Linux SDL front end) drives the engine
// through these. win_main.cpp implements them.
// ---------------------------------------------------------------------------

// Called by the host once the surface exists, before R_CreateDeviceInternal.
// width/height are the surface size in pixels; refreshHz may be 0 if unknown.
void Sys_SetDisplaySize(int width, int height, int refreshHz);

// Set before Sys_Init so the engine can locate the game data. Both are copied.
void Sys_SetInstallPath(const char *path);
void Sys_SetGameDataPath(const char *path);

// Called from the host's render thread once per frame after the swapchain is
// presented. Returns false once the engine has asked to quit.
bool Sys_HostFrame();

// Post a synthetic engine event (used for touch input and lifecycle changes).
void Sys_HostEvent(sysEventType_t type, int value, int value2);

// Called when the host loses or regains the surface (activity pause/resume,
// Android surface destroy). The engine must not touch D3D9 while paused.
void Sys_HostSetActive(bool active);
