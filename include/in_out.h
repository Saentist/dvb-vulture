#ifndef __in_out_h
#define __in_out_h
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#ifdef __WIN32__
#include<winsock.h>
struct sockaddr_storage
{
  struct sockaddr_in a;
};
#else
#include <sys/socket.h>
#endif

#ifdef __WIN32__
ssize_t ioBlkRcv (int fd, void *buf, size_t count);
ssize_t ioBlkSnd (int fd, void const *buf, size_t count);
#else
#define ioBlkRcv ioBlkRd
#define ioBlkSnd ioBlkWr
#endif

///unlike read(), this will wait for the requested number of bytes to arrive
ssize_t ioBlkRd (int fd, void *buf, size_t count);
ssize_t ioBlkWr (int fd, void const *buf, size_t count);
///as above, but with timeout
ssize_t ioBlkRdTo (int fd, void *buf, size_t count, int secs);
ssize_t ioBlkWrTo (int fd, void const *buf, size_t count, int secs);
#define write_sz(_SOCK,_VAR) ioBlkWr(_SOCK,&_VAR,sizeof(_VAR))
#define read_sz(_SOCK,_VAR) ioBlkRd(_SOCK,&_VAR,sizeof(_VAR))
uint32_t ioWrStrLen (char *str);        ///<returns the size required for the string
void ioWrStr (char *str, FILE * f);     ///<write string to disk. NULL pointers allowed
char *ioRdStr (FILE * f);       ///<read string from disk. may return NULL.
void ioSetBuffer (FILE * h, int n, void **pp);

/**
 *\brief create an udp socket ip4 or ip6.
 *will not bind() to a port...
 *
 *\param af either AF_INET or AF_INET6
 *\param bcast_addr destination ip string.
 *\param port number in host order.
 *\param bcast_ttl time-to-live scope for ip4. ignored for ip6.
 *\param dest_addr gets initialized with bcast_addr,port and af.
 *\return -1 on error or a valid socket.
 */
int ioUdpSocket (int af, char *bcast_addr, uint16_t port, int bcast_ttl,
                 int loop, struct sockaddr_storage *dest_addr);
/**
 *\brief create a TCP socket on a port. 
 *
 *\param port port in host order to bind() to. if port is 0, do not bind (for client sockets). 
 *\return -1 on error or a valid socket.
 */
int ioTcpSocket (int af, uint16_t port);

///produces a printable string from an ip4 or ip6 address
const char *ioNtoP (struct sockaddr_storage *addr, char *buf, size_t len);

#endif
