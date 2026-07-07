// net/net_socket.h: non blocking tcp sockets that act like nio channels
// errors throw an IOException with the system's text for it

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stddef.h>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  typedef SOCKET sock_t;
#else
  typedef int sock_t;
#endif

// ServerSocketChannel.open, bind and configureBlocking(false), the BindException's toString on failure
int  NetSocket_listen(int port, sock_t* out, char* exception, size_t size);
int  NetSocket_accept(sock_t listen, sock_t* out);
// no delay, traffic class 24, no keep alive, no reuse address
void NetSocket_configure(sock_t sock);
// getInetAddress().toString(), like "/127.0.0.1"
void NetSocket_address(sock_t sock, char* out, size_t size);
int  NetSocket_read(sock_t sock, void* buf, int len);
int  NetSocket_write(sock_t sock, const void* buf, int len);
void NetSocket_close(sock_t sock);

#endif
