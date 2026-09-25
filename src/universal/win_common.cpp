#include <universal/q_shared.h>
#include <win32/win_local.h>
#include <win32/win_net.h>

#include <universal/assertive.h>

#include <qcommon/qcommon.h>
#include <qcommon/threads.h>

#include "com_memory.h"
#include "profile.h"

// Win32 critical sections on every target: MSVC has them natively, and the
// POSIX/Android build gets them from win32_posix.cpp, which implements them as
// recursive pthread mutexes. The recursive behaviour matters -- the engine
// re-enters (Sys_EnterCriticalSection is taken on paths that already hold it),
// which is exactly what MSVC's critical sections allow and what a plain
// std::mutex would deadlock on.
_RTL_CRITICAL_SECTION s_criticalSections[CRITSECT_COUNT];

void Sys_InitializeCriticalSections()
{
	for (int critSect = 0; critSect < CRITSECT_COUNT; critSect++) {
		InitializeCriticalSection(&s_criticalSections[critSect]);
	}
}

void Sys_EnterCriticalSection(int critSect)
{
    PROF_SCOPED("Sys_EnterCriticalSection");

	iassert(critSect >= 0 && critSect < CRITSECT_COUNT);
	EnterCriticalSection(&s_criticalSections[critSect]);
}

void Sys_LeaveCriticalSection(int critSect)
{
	iassert(critSect >= 0 && critSect < CRITSECT_COUNT);
	LeaveCriticalSection(&s_criticalSections[critSect]);
}

void Sys_LockWrite(FastCriticalSection* critSect)
{
    while (1)
    {
        if (critSect->readCount == 0)
        {
            if (InterlockedIncrement(&critSect->writeCount) == 1 && critSect->readCount == 0)
            {
                break;
            }
            InterlockedDecrement(&critSect->writeCount);
        }
        NET_Sleep(0);
    }
}

void Sys_UnlockWrite(FastCriticalSection* critSect)
{
    iassert(critSect->writeCount > 0);
    InterlockedDecrement(&critSect->writeCount);
}

int Sys_InterlockedIncrement(uint *addend)
{
    return InterlockedIncrement(addend);
}

int Sys_InterlockedDecrement(uint *addend)
{
    return InterlockedDecrement(addend);
}

uint32_t Win_InitThreads()
{
    HANDLE CurrentProcess;
    unsigned long result; 
    unsigned long cpuCount; 
    DWORD_PTR systemAffinityMask;
    unsigned long cpuOffset; 
    DWORD_PTR threadAffinityMask;
    DWORD_PTR affinityMaskBits[33];
    DWORD_PTR processAffinityMask; 

    CurrentProcess = GetCurrentProcess();
    result = GetProcessAffinityMask(CurrentProcess, &processAffinityMask, &systemAffinityMask);
    s_affinityMaskForProcess = processAffinityMask;
    cpuCount = 0;
    for (threadAffinityMask = 1; (processAffinityMask & ((DWORD_PTR)0 - threadAffinityMask)) != 0; threadAffinityMask *= 2)
    {
        if ((processAffinityMask & threadAffinityMask) != 0)
        {
            result = cpuCount;
            affinityMaskBits[cpuCount++] = threadAffinityMask;
            if (cpuCount == 32)
                break;
        }
        result = 2 * threadAffinityMask;
    }
    if (cpuCount > 1)
    {
        s_cpuCount = cpuCount;
        s_affinityMaskForCpu[0] = affinityMaskBits[0];
        result = affinityMaskBits[cpuCount - 1];
        s_affinityMaskForCpu[1] = result;
        if (cpuCount != 2)
        {
            if (cpuCount == 3)
            {
                s_affinityMaskForCpu[2] = affinityMaskBits[1];
            }
            else if (cpuCount == 4)
            {
                s_affinityMaskForCpu[2] = affinityMaskBits[1];
                result = affinityMaskBits[2];
                s_affinityMaskForCpu[3] = affinityMaskBits[2];
            }
            else
            {
                cpuOffset = (cpuCount - 2) / 3;
                iassert(1 + cpuOffset < (cpuCount - 1) - cpuOffset);
                s_affinityMaskForCpu[2] = affinityMaskBits[cpuOffset + 1];
                result = affinityMaskBits[cpuCount - 1 - cpuOffset];
                s_affinityMaskForCpu[3] = result;
                s_cpuCount = 4;
            }
        }
    }
    else
    {
        s_cpuCount = 1;
        s_affinityMaskForCpu[0] = -1;
    }
    return result;
}

// *(_DWORD *)(*(_DWORD *)(*((_DWORD *)NtCurrentTeb()->ThreadLocalStoragePointer + _tls_index) + 4)

void __cdecl Sys_Mkdir(const char *path)
{
    CreateDirectoryA(path, 0);
}

BOOL __cdecl Sys_RemoveDirTree(const char *path)
{
    bool v2; // [esp+8h] [ebp-250h]
    HANDLE handle;
    char childPath[256];
    WIN32_FIND_DATAA find;
    bool hasError; // [esp+252h] [ebp-6h]
    bool hasTrailingSeparater; // [esp+253h] [ebp-5h]
    int length; // [esp+254h] [ebp-4h]

    length = strlen(path);
    v2 = path[length - 1] == 92 || path[length - 1] == 47;
    hasTrailingSeparater = v2;
    if (v2)
        Com_sprintf(childPath, 0x100u, "%s*", path);
    else
        Com_sprintf(childPath, 0x100u, "%s\\*", path);
    handle = FindFirstFileA(childPath, &find);
    if (handle == INVALID_HANDLE_VALUE)
        return RemoveDirectoryA(path);
    hasError = 0;
    do
    {
        if (find.cFileName[0] != 46 || find.cFileName[1] && (find.cFileName[1] != 46 || find.cFileName[2]))
        {
            if (hasTrailingSeparater)
                Com_sprintf(childPath, 0x100u, "%s%s", path, find.cFileName);
            else
                Com_sprintf(childPath, 0x100u, "%s\\%s", path, find.cFileName);
            if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                hasError = !Sys_RemoveDirTree(childPath);
            else
                hasError = remove(childPath) == -1;
        }
    } while (!hasError && FindNextFileA(handle, &find));
    FindClose(handle);
    return !hasError && RemoveDirectoryA(path);
}

void __cdecl Sys_ListFilteredFiles(
    HunkUser *user,
    const char *basedir,
    const char *subdirs,
    const char *filter,
    char **list,
    int *numfiles)
{
    char filename[256];
    WIN32_FIND_DATAA findinfo;
    HANDLE findhandle;
    char search[260];

    if (*numfiles < 0x1FFF)
    {
        if (strlen(subdirs))
            Com_sprintf(search, 0x100u, "%s\\%s\\*", basedir, subdirs);
        else
            Com_sprintf(search, 0x100u, "%s\\*", basedir);
        findhandle = FindFirstFileA(search, &findinfo);
        if (findhandle != INVALID_HANDLE_VALUE)
        {
            do
            {
                if ((findinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                    || I_stricmp(findinfo.cFileName, ".") && I_stricmp(findinfo.cFileName, "..") && I_stricmp(findinfo.cFileName, "CVS"))
                {
                    if (*numfiles >= 0x1FFF)
                        break;
                    if (subdirs)
                        Com_sprintf(filename, 0x100u, "%s\\%s", subdirs, findinfo.cFileName);
                    else
                        Com_sprintf(filename, 0x100u, "%s", findinfo.cFileName);
                    if (Com_FilterPath(filter, filename, 0))
                        list[(*numfiles)++] = Hunk_CopyString(user, filename);
                }
            } while (FindNextFileA(findhandle, &findinfo));
            FindClose(findhandle);
        }
    }
}

BOOL __cdecl HasFileExtension(const char *name, const char *extension)
{
    char search[260]; // [esp+0h] [ebp-108h] BYREF

    Com_sprintf(search, 0x100u, "*.%s", extension);
    return I_stricmpwild(search, name) == 0;
}

int __cdecl Sys_CountFileList(char **list)
{
    int i; // [esp+0h] [ebp-4h]

    i = 0;
    if (list)
    {
        while (*list)
        {
            ++list;
            ++i;
        }
    }
    return i;
}

char **__cdecl Sys_ListFiles(
    const char *directory,
    const char *extension,
    const char *filter,
    int *numfiles,
    int wantsubs)
{
    char *v6; // eax
    char **v7; // [esp+4h] [ebp-264h]
    WIN32_FIND_DATAA findinfo;
    int flag; // [esp+140h] [ebp-128h]
    char **listCopy; // [esp+144h] [ebp-124h]
    HANDLE findhandle; // [esp+148h] [ebp-120h]
    char *(*list)[8192]; // [esp+14Ch] [ebp-11Ch]
    int nfiles; // [esp+150h] [ebp-118h] BYREF
    HunkUser *user; // [esp+154h] [ebp-114h]
    char search[256]; // [esp+160h] [ebp-108h] BYREF
    int i; // [esp+264h] [ebp-4h]

    LargeLocal list_large_local(0x8000); // [esp+158h] [ebp-110h] BYREF
    //LargeLocal::LargeLocal(&list_large_local, 0x8000);
    //list = (char *(*)[8192])LargeLocal::GetBuf(&list_large_local);
    list = (char *(*)[8192])list_large_local.GetBuf();
    if (filter)
    {
        user = Hunk_UserCreate(0x20000, "Sys_ListFiles", 0, 0, 3);
        nfiles = 0;
        Sys_ListFilteredFiles(user, directory, "", filter, (char **)list, &nfiles);
        (*list)[nfiles] = 0;
        *numfiles = nfiles;
        if (nfiles)
        {
            listCopy = (char **)Hunk_UserAlloc(user, 4 * nfiles + 8, 4);
            *listCopy++ = (char *)user;
            for (i = 0; i < nfiles; ++i)
                listCopy[i] = (*list)[i];
            listCopy[i] = 0;
            //LargeLocal::~LargeLocal(&list_large_local);
            return listCopy;
        }
        else
        {
            Hunk_UserDestroy(user);
            //LargeLocal::~LargeLocal(&list_large_local);
            return 0;
        }
    }
    else
    {
        if (!extension)
            extension = "";
        if (*extension != 47 || extension[1])
        {
            flag = 16;
        }
        else
        {
            extension = "";
            flag = 0;
        }
        if (*extension)
            Com_sprintf(search, 0x100u, "%s\\*.%s", directory, extension);
        else
            Com_sprintf(search, 0x100u, "%s\\*", directory);
        nfiles = 0;
        findhandle = FindFirstFileA(search, &findinfo);
        if (findhandle == INVALID_HANDLE_VALUE)
        {
            *numfiles = 0;
            //LargeLocal::~LargeLocal(&list_large_local);
            return 0;
        }
        else
        {
            user = Hunk_UserCreate(0x20000, "Sys_ListFiles", 0, 0, 3);
            do
            {
                if ((!wantsubs && flag != (findinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || wantsubs && (findinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                    && ((findinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                        || I_stricmp(findinfo.cFileName, ".") && I_stricmp(findinfo.cFileName, "..") && I_stricmp(findinfo.cFileName, "CVS"))
                    && (!*extension || HasFileExtension(findinfo.cFileName, extension)))
                {
                    v6 = Hunk_CopyString(user, findinfo.cFileName);
                    (*list)[nfiles++] = v6;
                    if (nfiles == 0x1FFF)
                        break;
                }
            } while (FindNextFileA(findhandle, &findinfo));
            (*list)[nfiles] = 0;
            FindClose(findhandle);
            *numfiles = nfiles;
            if (nfiles)
            {
                listCopy = (char **)Hunk_UserAlloc(user, 4 * nfiles + 8, 4);
                *listCopy++ = (char *)user;
                for (i = 0; i < nfiles; ++i)
                    listCopy[i] = (*list)[i];
                listCopy[i] = 0;
                v7 = listCopy;
                //LargeLocal::~LargeLocal(&list_large_local);
                return v7;
            }
            else
            {
                Hunk_UserDestroy(user);
                //LargeLocal::~LargeLocal(&list_large_local);
                return 0;
            }
        }
    }
}


char cwd[256];
char *__cdecl Sys_Cwd()
{
    GetCurrentDirectoryA(255, cwd);
    cwd[255] = 0;
    return cwd;
}

const char *__cdecl Sys_DefaultCDPath()
{
    return "";
}

char exePath[256];
char *__cdecl Sys_DefaultInstallPath()
{
    char *v0; // eax
    uint32_t len; // [esp+0h] [ebp-8h]
    HINSTANCE__ *hinst; // [esp+4h] [ebp-4h]

    if (!exePath[0])
    {
        if (IsDebuggerPresent())
        {
            v0 = Sys_Cwd();
            I_strncpyz(exePath, v0, 256);
        }
        else
        {
            hinst = GetModuleHandleA(0);
            len = GetModuleFileNameA(hinst, exePath, 0x100u);
            if (len == 256)
                len = 255;
            while (len && exePath[len] != 92 && exePath[len] != 47 && exePath[len] != 58)
                --len;
            exePath[len] = 0;
        }
    }
    return exePath;
}