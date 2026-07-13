// net/net_socket.h: tcp socket, non blocking after the connect

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#if defined(_WIN32)
  // must come before anything that pulls in windows.h, glew does
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  typedef SOCKET sock_t;
#else
  typedef int sock_t;
#endif

// a blocking connect, then non blocking with TcpNoDelay and KeepAlive off
// 1 connected, 0 the name doesn't resolve, -1 failed
int NetSocket_connect(sock_t* out, const char* host, int port);
void NetSocket_close(sock_t sock);

// bytes moved, 0 if it would block or the other end closed, -1 on an error
int NetSocket_read(sock_t sock, void* buf, int len);
int NetSocket_write(sock_t sock, const void* buf, int len);

#endif
