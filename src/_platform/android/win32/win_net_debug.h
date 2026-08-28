// Android port — network debug stub (replaces win32/win_net_debug.h)
#pragma once

inline void NetDebug_Init() {}
inline void NetDebug_Shutdown() {}
inline void NetDebug_Frame() {}
inline void NetDebug_AddPacket(const void *data, int len, int from, int to) {}
inline void NetDebug_AddEvent(const char *fmt, ...) {}