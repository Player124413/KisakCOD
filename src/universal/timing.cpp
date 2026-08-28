#include <universal/q_shared.h>
#include "timing.h"

#include <Windows.h>
#include <qcommon/threads.h>

#if !defined(KISAK_ANDROID)
// Original Windows implementation using x86 __rdtsc
long double msecPerRawTimerTick;
double qpc2msec;

double __cdecl SecondsPerTick()
{
    _LARGE_INTEGER tscStop;
    _LARGE_INTEGER qpcFrequency;
    _LARGE_INTEGER qpcStart;
    _LARGE_INTEGER tscStart;
    _LARGE_INTEGER qpcStop;
    double secPerTick;

    Win_SetThreadLock(THREAD_LOCK_ALL);
    Sleep(0);
    tscStart.QuadPart = 0;
    qpcStart.QuadPart = 0;
    qpcStop.QuadPart = 0;
    QueryPerformanceFrequency(&qpcFrequency);
    qpc2msec = 1000.0 / qpcFrequency.QuadPart;
    QueryPerformanceCounter(&qpcStart);
    tscStart.QuadPart = __rdtsc();
    QueryPerformanceCounter(&qpcStart);
    Sleep(0xFAu);
    tscStop.QuadPart = __rdtsc();
    QueryPerformanceCounter(&qpcStop);
    secPerTick = (double)(qpcStop.QuadPart - qpcStart.QuadPart)
        / ((double)(tscStop.QuadPart - tscStart.QuadPart)
            * (double)qpcFrequency.QuadPart);
    Win_SetThreadLock(THREAD_LOCK_NONE);
    return secPerTick;
}

void __cdecl InitTiming()
{
	msecPerRawTimerTick = SecondsPerTick() * 1000.0;
}
#else
// Android port: use monotonic clock directly
long double msecPerRawTimerTick;
double qpc2msec;

double __cdecl SecondsPerTick()
{
    LARGE_INTEGER qpcFrequency;
    QueryPerformanceFrequency(&qpcFrequency);
    qpc2msec = 1000.0 / qpcFrequency.QuadPart;
    return 1.0 / (double)qpcFrequency.QuadPart;
}

void __cdecl InitTiming()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    msecPerRawTimerTick = (1000.0 / freq.QuadPart) * 1000.0;
    qpc2msec = 1000.0 / freq.QuadPart;
}
#endif