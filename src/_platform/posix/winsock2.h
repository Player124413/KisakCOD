// KisakCOD -- Winsock2 over Berkeley sockets.
//
// This is deliberately NOT a drop-in MinGW winsock2.h. MinGW's version pulls in
// its crt/_timeval.h, which redefines `struct timeval` against glibc, and then
// redefines fd_set, select() and gethostname() as well -- every one of those is
// a hard error on a POSIX libc.
//
// Instead, this file includes the host's socket headers and then supplies only
// the Win32 *names* the engine spells: SOCKET, INVALID_SOCKET, SOCKET_ERROR,
// sockaddr_in, the WSA* error codes and WSAStartup/WSACleanup as no-ops. The
// engine's net code (src/_platform/posix/win32/win_net.cpp and
// win_net_debug.cpp) calls socket()/bind()/listen()/accept()/connect()/send()/
// recv()/select() directly, which are the host's own.
//
// Only two engine files include this, both of them the platform layer.

#pragma once

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Socket handle
//
// On Win32 a SOCKET is a UINT_PTR so that INVALID_SOCKET (-1 cast to unsigned)
// is distinguishable from a valid descriptor 0. On POSIX, fds are signed ints
// and -1 is the error value, so int is the faithful mapping and the engine's
// `== INVALID_SOCKET` / `== SOCKET_ERROR` comparisons keep their meaning.
// ---------------------------------------------------------------------------
typedef int SOCKET;

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (SOCKET)(~0)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

// ---------------------------------------------------------------------------
// Error codes
//
// The engine checks a small number of these. They are mapped onto the host
// errno values that carry the same meaning, so the existing comparisons work
// unchanged.
// ---------------------------------------------------------------------------
#define WSAEWOULDBLOCK  EWOULDBLOCK
#define WSAEINPROGRESS  EINPROGRESS
#define WSAEALREADY     EALREADY
#define WSAEISCONN      EISCONN
#define WSAECONNRESET   ECONNRESET
#define WSAECONNABORTED ECONNABORTED
#define WSAENOTCONN     ENOTCONN
#define WSAEADDRINUSE   EADDRINUSE
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAEAFNOSUPPORT EAFNOSUPPORT
#define WSAEACCES       EACCES
#define WSAEHOSTUNREACH EHOSTUNREACH
#define WSAENETUNREACH  ENETUNREACH
#define WSAETIMEDOUT    ETIMEDOUT
#define WSAECONNREFUSED ECONNREFUSED
#define WSAEINVAL       EINVAL
#define WSAEMFILE       EMFILE
#define WSAENOBUFS      ENOBUFS
#define WSAENOTSOCK     ENOTSOCK
#define WSAEOPNOTSUPP   EOPNOTSUPP
#define WSAEFAULT       EFAULT
#define WSAENOMEM       ENOMEM
#define WSAESOCKTNOSUPPORT ESOCKTNOSUPPORT
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAEPROTOTYPE   EPROTOTYPE
#define WSAEMSGSIZE     EMSGSIZE
#define WSAEDESTADDRREQ EDESTADDRREQ
#define WSAENETRESET    ENETRESET
#define WSAENETDOWN     ENETDOWN
#define WSAEINTR        EINTR
#define WSAEALREADY_    EALREADY

#define WSABASEERR      10000
#define WSAESHUTDOWN    10058

// ---------------------------------------------------------------------------
// WSADATA / startup
//
// Berkeley sockets need no initialisation. WSAStartup is a no-op that reports
// success, so the engine's startup path is unchanged.
// ---------------------------------------------------------------------------
typedef struct _WSADATA {
    unsigned short wVersion;
    unsigned short wHighVersion;
    char           szDescription[257];
    char           szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char          *lpVendorInfo;
} WSADATA, *LPWSADATA;

#define MAKEWORD(low, high) ((unsigned short)(((unsigned char)(low)) | (((unsigned short)((unsigned char)(high))) << 8)))

static inline int WSAStartup(unsigned short versionRequested, LPWSADATA wsaData)
{
    if (wsaData) {
        wsaData->wVersion = 2;
        wsaData->wHighVersion = 2;
        wsaData->iMaxSockets = 0x7fff;
        wsaData->iMaxUdpDg = 0x1000;
        wsaData->lpVendorInfo = nullptr;
    }
    (void)versionRequested;
    return 0;
}

static inline int WSACleanup(void) { return 0; }
static inline int WSAGetLastError(void) { return errno; }
static inline void WSASetLastError(int err) { errno = (err); }

// ---------------------------------------------------------------------------
// Shutdown / ioctl helpers the engine may reference
// ---------------------------------------------------------------------------
#ifndef SD_RECEIVE
#define SD_RECEIVE SHUT_RD
#endif
#ifndef SD_SEND
#define SD_SEND SHUT_WR
#endif
#ifndef SD_BOTH
#define SD_BOTH SHUT_RDWR
#endif

#ifndef FIONBIO
#define FIONBIO 0x5421
#endif

static inline int ioctlsocket(SOCKET s, long cmd, unsigned long *argp)
{
    return ioctl(s, (unsigned long)cmd, argp);
}

// closesocket() is close(); the engine's net code calls close() directly.

#ifdef __cplusplus
}  // extern "C"
#endif
