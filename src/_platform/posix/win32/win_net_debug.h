// win_net_debug.h -- the Radiant remote-debug socket interface.
//
// The debug socket is how the editor attaches to a running game. It is a plain
// TCP connection, so it works unchanged on POSIX; win_net_debug.cpp talks to
// Berkeley sockets instead of Winsock.

#pragma once

extern int g_debugClient;

int __cdecl Sys_IsRemoteDebugClient();

void __cdecl NET_ShutdownDebug();
void NET_InitDebug();
void NET_RestartDebug();

void __cdecl Sys_Listen_f();

void Sys_DebugSocketError(const char *message);

int __cdecl Sys_ReadDebugSocketInt();
void __cdecl Sys_WriteDebugSocketInt(int value);
void __cdecl Sys_WriteDebugSocketString(char *text);
int __cdecl Sys_ReadDebugSocketMessageType(uint8_t *type, int blocking);
int __cdecl Sys_UpdateDebugSocket();
int __cdecl Sys_ReadDebugSocketData(char *buffer, int len, int blocking);
void __cdecl Sys_ReadDebugSocketStringBuffer(char *buffer, int len);
void __cdecl Sys_FlushDebugSocketData();
void __cdecl Sys_AckDebugSocket();
char *__cdecl Sys_ReadDebugSocketString();

void __cdecl Sys_WriteDebugSocketData(uint8_t *buffer, int len);
void __cdecl Sys_WriteDebugSocketMessageType(uint8_t type);
void __cdecl Sys_EndWriteDebugSocket();

extern uint8_t g_debugPacket[1][8192];
