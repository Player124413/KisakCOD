// Android port — storage header (replaces win32/win_storage.h)
#pragma once

#include <cstdint>

// Storage functions
void Storage_Init();
void Storage_Shutdown();
int Storage_GetDeviceCount();
const char *Storage_GetDeviceName(int deviceIndex);
bool Storage_DeviceAvailable(int deviceIndex);
int Storage_GetFreeSpace(int deviceIndex);
int Storage_GetUsedSpace(int deviceIndex);
int Storage_GetTotalSpace(int deviceIndex);