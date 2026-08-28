// Android port — platform local header (replaces win32/win_local.h)
// Provides engine-specific types and function declarations that the
// original win32/win_local.h provides.
//
// NOTE: Basic Windows types (DWORD, HANDLE, CRITICAL_SECTION, etc.) are
// provided by win32_compat.h which is included first.
#pragma once

#include "win32_compat.h"

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <ctime>
#include <csetjmp>
#include <cassert>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <climits>
#include <cfloat>
#include <cinttypes>
#include <cwchar>
#include <mutex>

#include <qcommon/qcommon.h>

// ---- Windows structs not in win32_compat.h ----
typedef struct tagPOINT { int x, y; } POINT;
typedef struct tagRECT { int left, top, right, bottom; } RECT;
typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;

// ---- Android-specific platform types ----

// WinVars_t for Android
typedef struct {
    void *reflib_library;
    bool reflib_active;
    HWND hWnd;
    HINSTANCE hInstance;
    bool activeApp;
    bool isMinimized;
    bool recenterMouse;
    unsigned sysMsgTime;
} WinVars_t;
extern WinVars_t g_wv;

// SysInfo
struct SysInfo {
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
extern SysInfo sys_info;

#define MAX_QUED_EVENTS   256
#define MASK_QUED_EVENTS  ( MAX_QUED_EVENTS - 1 )

extern std::mutex s_criticalSections[];
extern int client_state;

// CriticalSection enum matching KISAK_MP layout
enum CriticalSection : int {
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

// sysEvent_t (same layout as win32 version)
struct sysEvent_t {
    int evTime;
    sysEventType_t evType;
    int evValue;
    int evValue2;
    int evPtrLength;
    void *evPtr;
};

struct FastCriticalSection {
    volatile uint32_t readCount;
    volatile uint32_t writeCount;
};

// ---- function declarations ----

void Sys_InitializeCriticalSections();
void Sys_EnterCriticalSection(int critSect);
void Sys_LeaveCriticalSection(int critSect);
void Sys_LockWrite(FastCriticalSection* critSect);
void Sys_UnlockWrite(FastCriticalSection* critSect);

int Sys_InterlockedIncrement(uint32_t *addend);
int Sys_InterlockedDecrement(uint32_t *addend);

void Sys_SetErrorText(const char* buf);
void Sys_Error(const char *error, ...);
void Sys_OutOfMemErrorInternal(const char* filename, int line);
void Sys_NormalExit();
void Sys_OpenURL(const char *url, int doexit);
void Sys_Quit();
void Sys_Print(const char *msg);
char *Sys_GetClipboardData();
int Sys_SetClipboardData(const char *text);
void Sys_QueEvent(uint32_t time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr);
void Sys_ShutdownEvents();
void Sys_LoadingKeepAlive();
sysEvent_t *Sys_GetEvent(sysEvent_t *result);
void Sys_Init();

void Sys_In_Restart_f();
#ifdef KISAK_MP
void Sys_Net_Restart_f();
void Sys_Listen_f();
#endif

void Sys_Mkdir(const char *path);
BOOL Sys_RemoveDirTree(const char *path);
int Sys_CountFileList(char **list);
char **Sys_ListFiles(const char *directory, const char *extension, const char *filter, int *numfiles, int wantsubs);
char *Sys_Cwd();
const char *Sys_DefaultCDPath();
char *Sys_DefaultInstallPath();
void Sys_QuitAndStartProcess(const char *exeName, const char *parameters);

// win_voice
bool Voice_SendVoiceData();
bool Voice_Init();
void Voice_Shutdown();
double Voice_GetVoiceLevel();
void Voice_Playback();
int Voice_GetLocalVoiceData();
void Voice_IncomingVoiceData(unsigned char talker, unsigned char *data, int packetDataSize);
bool Voice_IsClientTalking(uint32_t clientNum);
char Voice_StartRecording();
char Voice_StopRecording();