// net/net_socket.h: raw cross platform TCP socket, ported from
// com.mojang.a.a (ConnectionListener, the listening/accepting socket) and
// com.mojang.a.b (the per-connection read/write wrapper). The client's
// equivalent only ever connects out; this side only ever listens and
// accepts, so there's no NetSocket_connect here at all

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stddef.h>

#if defined(_WIN32)
  // must precede any transitive <windows.h> include, or its old winsock.h
  // beats winsock2.h to the punch and the build fails with redefinition errors
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  typedef SOCKET sock_t;
#else
  typedef int sock_t;
#endif

// binds and listens, non-blocking. Returns 1 on success, 0 on failure
int NetSocket_listen(sock_t* out, int port);

// non-blocking accept: returns 1 and fills *outClient on a new connection,
// 0 if none pending, -1 on a real error
int NetSocket_accept(sock_t listenSock, sock_t* outClient);

// applies TcpNoDelay=true/KeepAlive=false and switches to non-blocking,
// matching com.mojang.a.b's constructor. Called once per accepted socket
void NetSocket_configure(sock_t sock);

void NetSocket_close(sock_t sock);

// non-blocking read/write. returns bytes transferred, 0 if it would block,
// -1 on a real error or an orderly remote close
int NetSocket_read(sock_t sock, void* buf, int len);
int NetSocket_write(sock_t sock, const void* buf, int len);

// matches com.mojang.a.b's captured InetAddress string, used for IP ban
// checks and logging. Writes an empty string on failure
void NetSocket_getRemoteAddress(sock_t sock, char* out, size_t outSize);

#endif
