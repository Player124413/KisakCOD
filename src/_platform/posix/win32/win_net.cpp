// win_net.cpp -- UDP/IP networking for the POSIX/Android build.
//
// Ported from src/win32/win_net.cpp. The Winsock calls map one-to-one onto
// Berkeley sockets, which the POSIX layer already provides: WSAGetLastError is
// errno, closesocket is close, ioctlsocket is ioctl, and the WSAE* codes the
// engine compares against are the host errno values with the same meaning.
//
// IPX is gone entirely. It was already dead in the Win32 build (the code was
// commented out with "LWSS: remove IPX, noone uses this"), and there is no
// wsipx.h to include anyway. net_noipx stays registered as a dvar so configs
// and the console keep working.
//
// The port matters even for singleplayer: the engine always runs a local
// server and the client talks to it over the loopback socket, so this is not
// optional code.

#include <universal/q_shared.h>
#include "win_local.h"
#include "win_net.h"
#include "win_net_debug.h"
#include <qcommon/mem_track.h>

#if defined(KISAK_MP)
#include <qcommon/net_chan_mp.h>
#elif defined(KISAK_SP)
#include <qcommon/net_chan.h>
#include <qcommon/msg.h>
#endif

#include <string.h>
#include <stdlib.h>

static WSADATA winsockdata;
static qboolean winsockInitialized = qfalse;
static qboolean usingSocks = qfalse;
static qboolean networkingEnabled = qfalse;

static const dvar_t *net_noudp;
static const dvar_t *net_noipx;
static const dvar_t *net_forcenonlocal;
static const dvar_t *net_socksEnabled;
static const dvar_t *net_socksServer;
static const dvar_t *net_socksPort;
static const dvar_t *net_socksUsername;
static const dvar_t *net_socksPassword;
static struct sockaddr socksRelayAddr;

static SOCKET ip_socket = INVALID_SOCKET;
static SOCKET socks_socket = INVALID_SOCKET;

#define MAX_IPS 16
static int numIP;
static byte localIP[MAX_IPS][4];

//=============================================================================

const char *NET_ErrorString(void)
{
    int code;

    code = WSAGetLastError();
    switch (code)
    {
    case WSAEINTR:           return "WSAEINTR";
    case WSAEBADF:           return "WSAEBADF";
    case WSAEACCES:          return "WSAEACCES";
    case WSAEFAULT:          return "WSAEFAULT";
    case WSAEINVAL:          return "WSAEINVAL";
    case WSAEMFILE:          return "WSAEMFILE";
    case WSAEWOULDBLOCK:     return "WSAEWOULDBLOCK";
    case WSAEINPROGRESS:     return "WSAEINPROGRESS";
    case WSAEALREADY:        return "WSAEALREADY";
    case WSAENOTSOCK:        return "WSAENOTSOCK";
    case WSAEDESTADDRREQ:    return "WSAEDESTADDRREQ";
    case WSAEMSGSIZE:        return "WSAEMSGSIZE";
    case WSAEPROTOTYPE:      return "WSAEPROTOTYPE";
    case WSAENOPROTOOPT:     return "WSAENOPROTOOPT";
    case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
    case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
    case WSAEOPNOTSUPP:      return "WSAEOPNOTSUPP";
    case WSAEPFNOSUPPORT:    return "WSAEPFNOSUPPORT";
    case WSAEAFNOSUPPORT:    return "WSAEAFNOSUPPORT";
    case WSAEADDRINUSE:      return "WSAEADDRINUSE";
    case WSAEADDRNOTAVAIL:   return "WSAEADDRNOTAVAIL";
    case WSAENETDOWN:        return "WSAENETDOWN";
    case WSAENETUNREACH:     return "WSAENETUNREACH";
    case WSAENETRESET:       return "WSAENETRESET";
    case WSAECONNABORTED:    return "WSAECONNABORTED";
    case WSAECONNRESET:      return "WSAECONNRESET";
    case WSAENOBUFS:         return "WSAENOBUFS";
    case WSAEISCONN:         return "WSAEISCONN";
    case WSAENOTCONN:        return "WSAENOTCONN";
    case WSAESHUTDOWN:       return "WSAESHUTDOWN";
    case WSAETIMEDOUT:       return "WSAETIMEDOUT";
    case WSAECONNREFUSED:    return "WSAECONNREFUSED";
    case WSAEHOSTUNREACH:    return "WSAEHOSTUNREACH";
    case WSAEHOSTDOWN:       return "WSAEHOSTDOWN";
    case WSAEPROCLIM:        return "WSAEPROCLIM";
    case WSASYSNOTREADY:     return "WSASYSNOTREADY";
    case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
    case WSANOTINITIALISED:  return "WSANOTINITIALISED";
    case WSAEDISCON:         return "WSAEDISCON";
    case WSAHOST_NOT_FOUND:  return "WSAHOST_NOT_FOUND";
    case WSATRY_AGAIN:       return "WSATRY_AGAIN";
    case WSANO_RECOVERY:     return "WSANO_RECOVERY";
    case WSANO_DATA:         return "WSANO_DATA";
    default:                 return "NO ERROR";
    }
}

// ---------------------------------------------------------------------------
// Address conversion
// ---------------------------------------------------------------------------

void NetadrToSockadr(netadr_t *a, struct sockaddr *s)
{
    memset(s, 0, sizeof(*s));

    if (a->type == NA_BROADCAST)
    {
        ((struct sockaddr_in *)s)->sin_family = AF_INET;
        ((struct sockaddr_in *)s)->sin_port = a->port;
        ((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
    }
    else if (a->type == NA_IP)
    {
        ((struct sockaddr_in *)s)->sin_family = AF_INET;
        ((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
        ((struct sockaddr_in *)s)->sin_port = a->port;
    }
}

void SockadrToNetadr(struct sockaddr *s, netadr_t *a)
{
    if (s->sa_family == AF_INET)
    {
        a->type = NA_IP;
        *(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
        a->port = ((struct sockaddr_in *)s)->sin_port;
    }
}

qboolean Sys_StringToSockaddr(const char *s, struct sockaddr *sadr)
{
    struct hostent *h;

    memset(sadr, 0, sizeof(*sadr));

    ((struct sockaddr_in *)sadr)->sin_family = AF_INET;
    ((struct sockaddr_in *)sadr)->sin_port = 0;

    if (s[0] >= '0' && s[0] <= '9')
    {
        *(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(s);
    }
    else
    {
        // gethostbyname is not reentrant, but the engine only ever calls this
        // from the game thread during startup and from the "connect" command,
        // so the classic interface is fine here.
        if ((h = gethostbyname(s)) == 0)
            return qfalse;
        *(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
    }

    return qtrue;
}

qboolean Sys_StringToAdr(const char *s, netadr_t *a)
{
    struct sockaddr sadr;

    if (!Sys_StringToSockaddr(s, &sadr))
        return qfalse;

    SockadrToNetadr(&sadr, a);
    return qtrue;
}

// ---------------------------------------------------------------------------
// Sockets
// ---------------------------------------------------------------------------

uint32_t __cdecl NET_IPSocket(const char *net_interface, int port)
{
    const char *v2;
    const char *v4;
    const char *v5;
    const char *v6;
    sockaddr address;
    int _true;
    uint32_t newsocket;

    _true = 1;
    if (net_interface)
        Com_Printf(CON_CHANNEL_SYSTEM, "Opening IP socket: %s:%i\n", net_interface, port);
    else
        Com_Printf(CON_CHANNEL_SYSTEM, "Opening IP socket: localhost:%i\n", port);

    newsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (newsocket == INVALID_SOCKET)
    {
        if (WSAGetLastError() != WSAEAFNOSUPPORT)
        {
            v2 = NET_ErrorString();
            Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: UDP_OpenSocket: socket: %s\n", v2);
        }
        return 0;
    }
    else if (ioctlsocket(newsocket, FIONBIO, (unsigned long *)&_true) == -1)
    {
        v4 = NET_ErrorString();
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: UDP_OpenSocket: ioctl FIONBIO: %s\n", v4);
        return 0;
    }
    else if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (const char *)&_true, 4) == -1)
    {
        v5 = NET_ErrorString();
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", v5);
        return 0;
    }
    else
    {
        if (net_interface && *net_interface && I_stricmp(net_interface, "localhost"))
            Sys_StringToSockaddr(net_interface, &address);
        else
            *(int *)&((struct sockaddr_in *)&address)->sin_addr.s_addr = INADDR_ANY;
        if (port == -1)
            ((struct sockaddr_in *)&address)->sin_port = 0;
        else
            ((struct sockaddr_in *)&address)->sin_port = htons(port);
        ((struct sockaddr_in *)&address)->sin_family = AF_INET;
        if (bind(newsocket, &address, sizeof(address)) == -1)
        {
            v6 = NET_ErrorString();
            Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: UDP_OpenSocket: bind: %s\n", v6);
            closesocket(newsocket);
            return 0;
        }
        else
        {
            return newsocket;
        }
    }
}

uint32_t __cdecl NET_TCPIPSocket(const char *net_interface, int port, int type)
{
    // The engine opens a listening TCP socket for the remote-debug channel.
    // type is SOCK_STREAM or SOCK_DGRAM as passed by the caller.
    sockaddr address;
    int _true = 1;
    uint32_t newsocket;

    newsocket = socket(AF_INET, type, (type == SOCK_STREAM) ? IPPROTO_TCP : IPPROTO_UDP);
    if (newsocket == INVALID_SOCKET)
    {
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: NET_TCPIPSocket: socket: %s\n", NET_ErrorString());
        return 0;
    }

    if (type == SOCK_STREAM)
        setsockopt(newsocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&_true, sizeof(_true));

    if (net_interface && *net_interface && I_stricmp(net_interface, "localhost"))
        Sys_StringToSockaddr(net_interface, &address);
    else
        ((struct sockaddr_in *)&address)->sin_addr.s_addr = INADDR_ANY;
    ((struct sockaddr_in *)&address)->sin_family = AF_INET;
    ((struct sockaddr_in *)&address)->sin_port = htons((unsigned short)port);

    if (bind(newsocket, &address, sizeof(address)) == -1)
    {
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: NET_TCPIPSocket: bind: %s\n", NET_ErrorString());
        closesocket(newsocket);
        return 0;
    }
    return newsocket;
}

void __cdecl NET_GetLocalAddress(void)
{
    char hostname[256];
    struct hostent *h;
    char **p;

    numIP = 0;
    if (gethostname(hostname, sizeof(hostname)) == -1)
        return;

    Com_Printf(CON_CHANNEL_SYSTEM, "Hostname: %s\n", hostname);

    h = gethostbyname(hostname);
    if (!h)
        return;

    for (p = h->h_addr_list; *p && numIP < MAX_IPS; ++p)
    {
        memcpy(&localIP[numIP][0], *p, 4);
        Com_Printf(CON_CHANNEL_SYSTEM, "IP: %i.%i.%i.%i\n",
            localIP[numIP][0], localIP[numIP][1], localIP[numIP][2], localIP[numIP][3]);
        ++numIP;
    }
}

static void __cdecl NET_OpenSocks(u_short port)
{
    // A SOCKS relay is a Windows-era way to reach a server through a proxy.
    // It is off by default (net_socksEnabled is 0) and singleplayer never needs
    // it, so this is a stub that keeps the dvar meaningful: setting it warns
    // instead of silently doing nothing.
    (void)port;
    Com_PrintWarning(CON_CHANNEL_SYSTEM,
        "WARNING: net_socksEnabled is set but SOCKS is not supported on this platform\n");
}

void __cdecl NET_OpenIP(void)
{
    const dvar_s *v0;
    int i;
    const dvar_s *port;

    v0 = Dvar_RegisterString("net_ip", "localhost", DVAR_LATCH, "Network IP Address");
    port = Dvar_RegisterInt("net_port", 28960, 0xFFFF00000000LL, DVAR_LATCH, "Network port");
    for (i = 0;; ++i)
    {
        if (i >= 10)
        {
            Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: Couldn't allocate IP port\n");
            return;
        }
        ip_socket = NET_IPSocket(v0->current.string, i + port->current.integer);
        if (ip_socket)
            break;
    }
    Dvar_SetInt(port, i + port->current.integer);
    if (net_socksEnabled->current.enabled)
        NET_OpenSocks(i + port->current.integer);
    NET_GetLocalAddress();
}

// ---------------------------------------------------------------------------
// Packet send / receive
// ---------------------------------------------------------------------------

void NET_SendPacket(netsrc_t sock, int length, const void *data, netadr_t to)
{
    // netsrc_t selects which socket the engine thinks it is using; there is
    // only one UDP socket now that IPX is gone, and it serves both the client
    // and the server because they share the loopback.
    (void)sock;
    Sys_SendPacket(length, (uint8_t *)data, to);
}

char __cdecl Sys_SendPacket(int length, uint8_t *data, netadr_t to)
{
    int err;
    sockaddr addr;
    int ret;
    uint32_t net_socket;

    net_socket = 0;
    switch (to.type)
    {
    case NA_BROADCAST:
    case NA_IP:
        net_socket = ip_socket;
        break;
    default:
        Com_Error(ERR_FATAL, "Sys_SendPacket: bad address type");
        break;
    }
    if (!net_socket)
        return 1;

    NetadrToSockadr(&to, &addr);
    if (usingSocks && to.type == NA_IP)
    {
        ret = sendto(net_socket, (const char *)data, length, 0, &socksRelayAddr, sizeof(socksRelayAddr));
    }
    else
    {
        ret = sendto(net_socket, (const char *)data, length, 0, &addr, sizeof(addr));
    }

    if (ret != -1)
        return 1;

    err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK)
        return 1;
    if (err == WSAEADDRNOTAVAIL && (to.type == NA_BROADCAST))
        return 1;

    // A dropped packet is normal on a mobile link and the engine's own
    // sequencing recovers from it, so this stays a warning rather than an
    // error like the Win32 build.
    Com_PrintWarning(CON_CHANNEL_SYSTEM, "NET_SendPacket: %s\n", NET_ErrorString());
    return 0;
}

int __cdecl Sys_GetPacket(netadr_t *net_from, msg_t *net_message)
{
    sockaddr from;
    int err;
    int ret;
    socklen_t fromlen;
    uint32_t net_socket;

    net_socket = ip_socket;
    if (net_socket)
    {
        fromlen = sizeof(from);
        ret = recvfrom(net_socket, (char *)net_message->data, net_message->maxsize, 0, &from, &fromlen);
        if (ret == -1)
        {
            err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAECONNRESET)
                Com_PrintError(CON_CHANNEL_SYSTEM, "NET_GetPacket: %s\n", NET_ErrorString());
        }
        else
        {
            // sockaddr_in has 8 bytes of sin_zero padding after sin_addr. The
            // Win32 build cleared sa_data[6..13] -- offsets 8..15 -- which is
            // exactly that padding, not the address. It has to be zeroed so
            // SockadrToNetadr cannot copy uninitialised bytes into the cod4
            // netadr_t's 10-byte ipx tail.
            memset(((struct sockaddr_in *)&from)->sin_zero, 0,
                   sizeof(((struct sockaddr_in *)&from)->sin_zero));
            SockadrToNetadr(&from, net_from);
            net_message->readcount = 0;
            if (ret != net_message->maxsize)
            {
                net_message->cursize = ret;
                return 1;
            }
            Com_Printf(CON_CHANNEL_SYSTEM, "Oversize packet from %s\n", NET_AdrToString(*net_from));
        }
    }
    return 0;
}

qboolean Sys_GetBroadcastPacket(msg_t *net_message)
{
    // Identical to Sys_GetPacket on POSIX: there is one UDP socket and a
    // broadcast is just a packet that arrived from a broadcast address. The
    // Win32 build had the same shape once IPX was removed.
    netadr_t from;
    return Sys_GetPacket(&from, net_message) ? qtrue : qfalse;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

BOOL __cdecl NET_GetDvars()
{
    BOOL modified;

    modified = 0;
    if (net_noudp)
        modified = net_noudp->modified;
    net_noudp = Dvar_RegisterBool("net_noudp", 0, DVAR_ARCHIVE | DVAR_LATCH, "Disable UDP");
    if (net_noipx && net_noipx->modified)
        modified = 1;
    net_noipx = Dvar_RegisterBool("net_noipx", 1, DVAR_ARCHIVE | DVAR_LATCH, "Disable IPX");
    if (net_socksEnabled && net_socksEnabled->modified)
        modified = 1;
    net_socksEnabled = Dvar_RegisterBool("net_socksEnabled", 0, DVAR_ARCHIVE | DVAR_LATCH, "Enable network sockets");
    if (net_socksServer && net_socksServer->modified)
        modified = 1;
    net_socksServer = Dvar_RegisterString("net_socksServer", "", DVAR_ARCHIVE | DVAR_LATCH, "Network socket server");
    if (net_socksPort && net_socksPort->modified)
        modified = 1;
    net_socksPort = Dvar_RegisterInt("net_socksPort", 1080, 0xFFFF00000000LL, DVAR_ARCHIVE | DVAR_LATCH, "Network socket port");
    if (net_socksUsername && net_socksUsername->modified)
        modified = 1;
    net_socksUsername = Dvar_RegisterString("net_socksUsername", "", DVAR_ARCHIVE | DVAR_LATCH, "Network socket username");
    if (net_socksPassword && net_socksPassword->modified)
        modified = 1;
    net_socksPassword = Dvar_RegisterString("net_socksPassword", "", DVAR_ARCHIVE | DVAR_LATCH, "Network socket password");
    return modified;
}

void __cdecl NET_Init()
{
    int r;

    r = WSAStartup(0x101u, &winsockdata);
    if (r)
    {
        Com_PrintWarning(CON_CHANNEL_SYSTEM, "WARNING: Winsock initialization failed, returned %d\n", r);
    }
    else
    {
        winsockInitialized = 1;
        Com_Printf(CON_CHANNEL_SYSTEM, "Winsock Initialized\n");
        NET_GetDvars();
        NET_Config(1);
        NET_InitDebug();
    }
}

void __cdecl NET_Config(int enableNetworking)
{
    int start;
    int stop;
    BOOL modified;

    modified = NET_GetDvars();
    if (net_noudp->current.enabled && net_noipx->current.enabled)
        enableNetworking = 0;
    if (enableNetworking != networkingEnabled || modified)
    {
        if (enableNetworking == networkingEnabled)
        {
            stop = enableNetworking ? 1 : 0;
            start = enableNetworking ? 1 : 0;
        }
        else
        {
            stop = enableNetworking ? 0 : 1;
            start = enableNetworking ? 1 : 0;
            networkingEnabled = enableNetworking;
        }
        if (stop)
        {
            if (ip_socket && ip_socket != INVALID_SOCKET)
            {
                closesocket(ip_socket);
                ip_socket = 0;
            }
            if (socks_socket && socks_socket != INVALID_SOCKET)
            {
                closesocket(socks_socket);
                socks_socket = 0;
            }
        }
        if (start)
        {
            if (!net_noudp->current.enabled)
                NET_OpenIP();
        }
    }
}

void __cdecl NET_Shutdown()
{
    if (ip_socket && ip_socket != INVALID_SOCKET)
        closesocket(ip_socket);
    ip_socket = 0;
    if (socks_socket && socks_socket != INVALID_SOCKET)
        closesocket(socks_socket);
    socks_socket = 0;
    usingSocks = qfalse;
    if (winsockInitialized)
        WSACleanup();
    winsockInitialized = qfalse;
}

void __cdecl NET_Restart()
{
    NET_Shutdown();
    NET_Init();
}

void __cdecl NET_Sleep(int msec)
{
    // The Win32 build uses select() with a timeout so a Sleep cannot be
    // interrupted by a packet that needs handling now. Same here.
    struct timeval timeout;
    fd_set fdset;

    if (msec < 0) msec = 0;

    FD_ZERO(&fdset);
    int nfds = 0;
    if (ip_socket)
    {
        FD_SET(ip_socket, &fdset);
        nfds = (int)ip_socket + 1;
    }
    timeout.tv_sec = msec / 1000;
    timeout.tv_usec = (msec % 1000) * 1000;
    select(nfds, &fdset, nullptr, nullptr, &timeout);
}

// ---------------------------------------------------------------------------
// LAN address classification
// ---------------------------------------------------------------------------

bool __cdecl Sys_IsLANAddress_IgnoreSubnet(netadr_t adr)
{
	switch (adr.type)
	{
	case NA_LOOPBACK:
		return 1;
	case NA_BOT:
		return 1;
	}
	if (adr.type != NA_IP)
		return 0;
	if (adr.ip[0] == 10)
		return 1;
	if (adr.ip[0] == 127)
		return 1;
	if (adr.ip[0] == 169 && adr.ip[1] == 254)
		return 1;
	if (adr.ip[0] == 172 && (adr.ip[1] & 0xF0) == 0x10)
		return 1;
	return adr.ip[0] == 192 && adr.ip[1] == 168;
}

/*
==================
Sys_IsLANAddress

LAN clients will have their rate var ignored
==================
*/
bool __cdecl Sys_IsLANAddress(netadr_t adr)
{
	int i;

	if (Sys_IsLANAddress_IgnoreSubnet(adr))
		return 1;
	for (i = 0; i < numIP; ++i)
	{
		if (adr.ip[0] == localIP[i][0] && adr.ip[1] == localIP[i][1] && adr.ip[2] == localIP[i][2])
			return 1;
	}
	return 0;
}

void Sys_ShowIP(void)
{
    int i;

    for (i = 0; i < numIP; ++i)
        Com_Printf(CON_CHANNEL_SYSTEM, "IP: %i.%i.%i.%i\n",
            localIP[i][0], localIP[i][1], localIP[i][2], localIP[i][3]);
}
