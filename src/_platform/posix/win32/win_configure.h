// win_configure.h -- CPU/GPU detection interface for the POSIX/Android build.
//
// Same contract as src/win32/win_configure.h. The implementations differ
// because there is no registry to read and, on ARM, no CPUID.

#pragma once

#include "win_local.h"

void Sys_DetectCpuVendorAndName(char *vendor, char *name);
void __cdecl Sys_DetectVideoCard(int bufSize, char *out);
uint32_t __cdecl Sys_GetPhysicalCpuCount();
void __cdecl Sys_SetAutoConfigureGHz(SysInfo *info);
double Sys_BenchmarkGHz(void);
int __cdecl Sys_AddApicIdIfUnique(int apicId, int *ids, int *count);
