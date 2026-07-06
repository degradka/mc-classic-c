// net/net_socket.h: raw cross platform TCP socket, ported from
// com.mojang.a.a's SocketChannel setup (connect blocking, then switch to
// non-blocking reads/writes for the rest of the connection's life)

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#if defined(_WIN32)
  // must precede any transitive <windows.h> include (GLEW pulls one in),
  // or its old winsock.h beats winsock2.h to the punch and the build fails
  // with redefinition errors
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  typedef SOCKET sock_t;
#else
  typedef int sock_t;
#endif

// blocking connect, then TcpNoDelay/KeepAlive=false/non-blocking switch.
// returns 1 on success, 0 on failure
int NetSocket_connect(sock_t* out, const char* host, int port);
void NetSocket_close(sock_t sock);

// non-blocking read/write. returns bytes transferred, 0 if it would block,
// -1 on a real error or an orderly remote close
int NetSocket_read(sock_t sock, void* buf, int len);
int NetSocket_write(sock_t sock, const void* buf, int len);

#endif
