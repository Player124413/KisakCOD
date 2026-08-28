#pragma once

// Android port — network header (replaces win32/win_net.h)
// BSD sockets on Android, types matching the engine expectations.
#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
struct msg_t;
#include <sys/ioctl.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>

// Winsock compatibility shims
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SD_SEND SHUT_WR
#define SD_BOTH SHUT_RDWR

// IOCTL
#if !defined(FIONBIO)
#define FIONBIO ioctl
#endif
#define ioctlsocket ioctl
#define closesocket close

// Socket functions
#define WSAGetLastError() errno
#define WSACleanup()
#define WSAStartup(a,b) 0
typedef struct { int dummy; } WSADATA;
typedef struct { int dummy; } WSAPROTOCOL_INFO;

// Windows -> POSIX errno mapping
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAECONNRESET ECONNRESET
#define WSAEMSGSIZE EMSGSIZE
#define WSAETIMEDOUT ETIMEDOUT
#define WSAECONNREFUSED ECONNREFUSED
#define WSAHOST_NOT_FOUND 11001

// netadr_t (same layout as the engine expects)
struct netadr_t {
    int type;
    unsigned char ip[4];
    unsigned short port;
    unsigned char ipx[10];
};

// Network functions
void Sys_ShowIP();
bool Sys_IsLANAddress(netadr_t adr);
bool Sys_IsLANAddress_IgnoreSubnet(netadr_t adr);
qboolean Sys_GetPacket(netadr_t *net_from, msg_t *net_message);
qboolean Sys_GetBroadcastPacket(msg_t *net_message);

// SOCKADDR_IN size (for ioctl/select)
typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr SOCKADDR;
typedef in_addr IN_ADDR;