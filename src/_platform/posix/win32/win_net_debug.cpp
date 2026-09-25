// win_net_debug.cpp -- the Radiant remote-debug socket for the POSIX build.
//
// Radiant attaches to a running game over a TCP socket to read state and push
// commands. That is a plain Berkeley socket, so the protocol is unchanged; only
// the Winsock calls become socket calls. Nothing in the singleplayer game path
// touches this -- it exists so the editor still works if someone builds it.

#include <universal/q_shared.h>
#include "win_local.h"
#include "win_net.h"
#include "win_net_debug.h"

#include <string.h>
#include <stdlib.h>

uint8_t g_debugPacket[1][8192];
int g_debugClient = -1;

static SOCKET debugSocket = INVALID_SOCKET;
static SOCKET debugListenSocket = INVALID_SOCKET;

int __cdecl Sys_IsRemoteDebugClient()
{
    return g_debugClient >= 0;
}

void NET_InitDebugStreams(void)
{
    debugSocket = INVALID_SOCKET;
    debugListenSocket = INVALID_SOCKET;
}

void NET_InitDebug(void)
{
    NET_InitDebugStreams();
}

void NET_RestartDebug(void)
{
    NET_ShutdownDebug();
    NET_InitDebug();
}

void __cdecl NET_ShutdownDebug()
{
    if (g_debugClient >= 0) { closesocket(g_debugClient); g_debugClient = -1; }
    if (debugListenSocket != INVALID_SOCKET) { closesocket(debugListenSocket); debugListenSocket = INVALID_SOCKET; }
    debugSocket = INVALID_SOCKET;
}

void Sys_DebugSocketError(const char *message)
{
    Com_PrintError(CON_CHANNEL_SYSTEM, "%s: %s\n", message, NET_ErrorString());
}

void __cdecl Sys_Listen_f()
{
    // "net_listen" opens the debug listener. Without it Radiant cannot attach.
    if (debugListenSocket != INVALID_SOCKET)
    {
        Com_Printf(CON_CHANNEL_SYSTEM, "Already listening for the debugger\n");
        return;
    }
    debugListenSocket = NET_TCPIPSocket(nullptr, 28963, SOCK_STREAM);
    if (debugListenSocket == INVALID_SOCKET)
    {
        Sys_DebugSocketError("Sys_Listen_f: listen socket");
        return;
    }
    if (listen(debugListenSocket, 1) == -1)
    {
        Sys_DebugSocketError("Sys_Listen_f: listen");
        closesocket(debugListenSocket);
        debugListenSocket = INVALID_SOCKET;
        return;
    }
    Com_Printf(CON_CHANNEL_SYSTEM, "Debug listener opened on port 28963\n");
}

int __cdecl Sys_UpdateDebugSocket()
{
    // Accept a pending Radiant connection, non-blocking. Returns 1 when a new
    // client arrived, 0 otherwise.
    struct sockaddr from;
    socklen_t fromlen = sizeof(from);

    if (debugListenSocket == INVALID_SOCKET)
        return 0;

    SOCKET c = accept(debugListenSocket, &from, &fromlen);
    if (c == INVALID_SOCKET)
        return 0;

    if (g_debugClient >= 0)
        closesocket(g_debugClient);
    g_debugClient = c;
    debugSocket = c;
    return 1;
}

int __cdecl Sys_ReadDebugSocketInt()
{
    int value = 0;
    if (g_debugClient < 0) return 0;
    if (recv(g_debugClient, (char *)&value, sizeof(value), 0) != sizeof(value))
        return 0;
    return value;
}

void __cdecl Sys_WriteDebugSocketInt(int value)
{
    if (g_debugClient < 0) return;
    send(g_debugClient, (const char *)&value, sizeof(value), 0);
}

void __cdecl Sys_WriteDebugSocketString(char *text)
{
    if (!text || g_debugClient < 0) return;
    send(g_debugClient, text, (int)strlen(text) + 1, 0);
}

char *__cdecl Sys_ReadDebugSocketString()
{
    static char buffer[8192];
    Sys_ReadDebugSocketStringBuffer(buffer, sizeof(buffer));
    if (!buffer[0])
        return nullptr;
    return buffer;
}

void __cdecl Sys_ReadDebugSocketStringBuffer(char *buffer, int len)
{
    int i;
    char c;

    if (!buffer || len <= 0 || g_debugClient < 0) { if (buffer && len > 0) buffer[0] = 0; return; }

    for (i = 0; i < len - 1; ++i)
    {
        int n = recv(g_debugClient, &c, 1, 0);
        if (n != 1) { buffer[i] = 0; return; }
        if (c == 0) { buffer[i] = 0; return; }
        buffer[i] = c;
    }
    buffer[len - 1] = 0;
}

int __cdecl Sys_ReadDebugSocketMessageType(uint8_t *type, int blocking)
{
    if (!type || g_debugClient < 0) return 0;
    int n = recv(g_debugClient, (char *)type, 1, 0);
    if (n != 1) return 0;
    (void)blocking;
    return 1;
}

int __cdecl Sys_ReadDebugSocketData(char *buffer, int len, int blocking)
{
    if (!buffer || len <= 0 || g_debugClient < 0) return 0;
    (void)blocking;
    return (int)recv(g_debugClient, buffer, len, 0);
}

void __cdecl Sys_WriteDebugSocketData(uint8_t *buffer, int len)
{
    if (!buffer || len <= 0 || g_debugClient < 0) return;
    send(g_debugClient, (const char *)buffer, len, 0);
}

void __cdecl Sys_WriteDebugSocketMessageType(uint8_t type)
{
    if (g_debugClient < 0) return;
    send(g_debugClient, (const char *)&type, 1, 0);
}

void __cdecl Sys_FlushDebugSocketData()
{
    // TCP has no user-space flush; the kernel buffers and sends. Kept for
    // interface parity with the Win32 build.
}

void __cdecl Sys_AckDebugSocket()
{
    Sys_WriteDebugSocketInt(1);
}

void __cdecl Sys_EndWriteDebugSocket()
{
    // No framing to close on a stream socket.
}

int __cdecl Sys_DebugCanSend(void)
{
    return g_debugClient >= 0;
}

int __cdecl Sys_DebugSend(void *data, int len)
{
    if (g_debugClient < 0 || !data || len <= 0) return 0;
    return (int)send(g_debugClient, (const char *)data, len, 0);
}

int __cdecl Sys_SendDebugReadBytesInternal(void *buffer, int len)
{
    if (g_debugClient < 0 || !buffer || len <= 0) return 0;
    return (int)recv(g_debugClient, (char *)buffer, len, MSG_WAITALL);
}

int __cdecl Sys_SendDebugReadBytes(void *buffer, int len)
{
    return Sys_SendDebugReadBytesInternal(buffer, len);
}
