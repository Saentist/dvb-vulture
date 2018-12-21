#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#ifdef __WIN32__
#include <winsock.h>
#include <pthread.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "in_out.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"
#define FID_STR_NULL 'N'
#define FID_STR_VALID 'V'

int DF_IN_OUT;
#define FILE_DBG DF_IN_OUT

ssize_t
ioBlkRd (int fd, void *buf, size_t count)
{
  size_t sofar;
  sofar = 0;
  while (sofar < count)
  {
    ssize_t ret = read (fd, ((uint8_t *) buf) + sofar, count - sofar);
    if (ret <= 0)               //should this be ret<0 or <=?.. recv will return 0 on close, but read()?
    {
      if (ret == -1)
      {
        errMsg ("error: ret: %d error: %s, fd: %d\n", ret, strerror (errno), fd);       //got eintr here... perhaps we should wrap sockets in FILE*
        return -1;
      }
      else
      {
        errMsg ("got 0!!\n");
        return sofar;
      }
    }
    sofar += ret;
  }
  return sofar;
}

ssize_t
ioBlkRdTo (int fd, void *buf, size_t count, int secs)
{
  size_t sofar;
  time_t t = time (NULL);
  sofar = 0;

  while (sofar < count)
  {
    ssize_t ret = read (fd, ((uint8_t *) buf) + sofar, count - sofar);
    if (ret <= 0)               //should this be ret<0 or <=?
    {
      if (ret == -1)
      {
        errMsg ("error: ret: %d error: %s\n", ret, strerror (errno));   //got eintr here... perhaps we should wrap sockets in FILE*
        return -1;
      }
      else
      {
        errMsg ("got 0!!\n");
        return sofar;
      }
    }
    sofar += ret;
    if ((t + secs) < time (NULL))
    {
      sofar = -1;
      errno = ETIMEDOUT;
      break;
    }
  }
  return sofar;
}

ssize_t
ioBlkWr (int fd, void const *buf, size_t count)
{
  size_t sofar;
  sofar = 0;
  while (sofar < count)
  {
    ssize_t ret = write (fd, ((uint8_t *) buf) + sofar, count - sofar);
    if (ret <= 0)
    {
      if (ret == -1)
      {
        errMsg ("error: ret: %d error: %s\n", ret, strerror (errno));   //got eintr here... perhaps we should wrap sockets in FILE*
        return -1;
      }
      else
      {
        errMsg ("got 0!!\n");
        return sofar;
      }
    }
    sofar += ret;
  }
  return sofar;
}

ssize_t
ioBlkWrTo (int fd, void const *buf, size_t count, int secs)
{
  size_t sofar;
  time_t t = time (NULL);
  sofar = 0;
  while (sofar < count)
  {
    ssize_t ret = write (fd, ((uint8_t *) buf) + sofar, count - sofar);
    if (ret <= 0)
    {
      if (ret == -1)
      {
        errMsg ("error: ret: %d error: %s\n", ret, strerror (errno));   //got eintr here... perhaps we should wrap sockets in FILE*
        return -1;
      }
      else
      {
        errMsg ("got 0!!\n");
        return sofar;
      }
    }
    sofar += ret;
    if ((t + secs) < time (NULL))
    {
      sofar = -1;
      errno = ETIMEDOUT;
      break;
    }
  }
  return sofar;
}

uint32_t
ioWrStrLen (char *str)
{
  if (str)
    return 1 + sizeof (int) + /*strnlen(str,512) */ strlen (str) + 1;
  return 1;
}

void
ioWrStr (char *str, FILE * f)
{
  int s;
  if (!str)
  {
    fputc (FID_STR_NULL, f);
    return;
  }
  fputc (FID_STR_VALID, f);
  s = strlen (str) + 1;
  fwrite (&s, sizeof (int), 1, f);
  fwrite (str, s, 1, f);
}

char *
ioRdStr (FILE * f)
{
  char *rv;
  int i, id;
  id = fgetc (f);
  if (id == FID_STR_NULL)
    return NULL;
  else if (id == FID_STR_VALID)
  {
    fread (&i, sizeof (int), 1, f);
    rv = utlCalloc (i, 1);
    fread (rv, i, 1, f);
  }
  else
  {
    errMsg ("Invalid FID_STR: %d\n", id);
    return NULL;
  }
  return rv;
}


void
ioSetBuffer (FILE * h, int n, void **pp)
{
  char *buf;
  if (n < 0)
  {
    *pp = NULL;
    return;
  }
  if (n == 0)
  {
    setvbuf (h, NULL, _IONBF, 0);
    *pp = NULL;
    return;
  }

  buf = utlCalloc (n, 1);
  if (NULL == buf)
  {
    *pp = NULL;
    return;
  }
  setvbuf (h, buf, _IOFBF, n);
  *pp = buf;
}


static int
io_udp_socket4 (char *bcast_addr, uint16_t port, int bcast_ttl, int loop,
                struct sockaddr_in *dest_addr)
{
  int sockfd;
  debugMsg ("initialising dest_addr\n");
  memset ((void *) dest_addr, 0, sizeof (*dest_addr));
#ifdef __WIN32__
  dest_addr->sin_addr.S_un.S_addr = inet_addr (bcast_addr);
#else
  if (!inet_pton (AF_INET, bcast_addr, &dest_addr->sin_addr))
  {
    errMsg ("failed\n");
    return -1;
  }
#endif
  dest_addr->sin_port = htons (port);
  dest_addr->sin_family = AF_INET;

  sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0)
  {
    errMsg ("Failed to create bcast socket: %s\n", strerror (errno));
    return -1;
  }
#ifndef __WIN32__
  setsockopt (sockfd, SOL_IP, IP_MULTICAST_TTL, &bcast_ttl,
              sizeof (bcast_ttl));
  setsockopt (sockfd, SOL_IP, IP_MULTICAST_LOOP, &loop, sizeof (loop));
#endif
  return sockfd;
}

#ifndef __WIN32__
static int
io_udp_socket6 (char *bcast_addr, uint16_t port, int loop,
                struct sockaddr_in6 *dest_addr)
{
  int sockfd;
  debugMsg ("initialising dest_addr\n");
  memset ((void *) dest_addr, 0, sizeof (*dest_addr));
  if (!inet_pton (AF_INET6, bcast_addr, &dest_addr->sin6_addr))
  {
    errMsg ("failed\n");
    return -1;
  }
  dest_addr->sin6_port = htons (port);
  dest_addr->sin6_family = AF_INET6;

  sockfd = socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd < 0)
  {
    errMsg ("Failed to create bcast socket: %s\n", strerror (errno));
    return -1;
  }
  setsockopt (sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop,
              sizeof (loop));
  return sockfd;
}
#endif

int
ioUdpSocket (int af, char *bcast_addr, uint16_t port, int bcast_ttl, int loop,
             struct sockaddr_storage *dest_addr)
{
  if (af == AF_INET)
    return io_udp_socket4 (bcast_addr, port, bcast_ttl, loop,
                           (struct sockaddr_in *) dest_addr);
#ifndef __WIN32__
  else if (af == AF_INET6)
    return io_udp_socket6 (bcast_addr, port, loop,
                           (struct sockaddr_in6 *) dest_addr);
#endif
  else
    return -1;

}


int
ioTcpSocket (int af, uint16_t port)
{
  int sock;
  if (af == AF_INET)
  {
    struct sockaddr_in name;
    /* Create the socket. */
    sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
      errMsg ("socket: %s\n", strerror (errno));
      return -1;
    }
    debugMsg ("Opened socket: %d\n", sock);
    if (port == 0)
      return sock;
    /* Give the socket a name. */
    memset ((void *) &name, 0, sizeof (name));
    name.sin_family = af;
    name.sin_port = htons (port);
    name.sin_addr.s_addr = htonl (INADDR_ANY);  //is this a bug?
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      errMsg ("bind: %s\n", strerror (errno));  //also 'No such file or directory' in WIN32 if I allow for port==0... what is wrong?
      return -1;
    }
    return sock;
  }
#ifndef __WIN32__
  else if (af == AF_INET6)
  {
    struct sockaddr_in6 name;
    /* Create the socket. */
    sock = socket (PF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
      errMsg ("socket: %s\n", strerror (errno));
      return -1;
    }
    if (port == 0)
      return sock;
    /* Give the socket a name. */
    memset ((void *) &name, 0, sizeof (name));
    name.sin6_family = af;
    name.sin6_port = htons (port);
    name.sin6_addr = in6addr_any;
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)      //is this a bug for non-servers?
    {
      errMsg ("bind: %s\n", strerror (errno));
      return -1;
    }
    return sock;
  }
#endif
  else
    return -1;
}

const char *
ioNtoP (struct sockaddr_storage *addr, char *buf, size_t len)
{
#ifdef __WIN32__
  if (addr->a.sin_family == AF_INET)
  {
    struct sockaddr_in *p = (struct sockaddr_in *) addr;
    return strncpy (buf, inet_ntoa (p->sin_addr), len);
  }
#else
  if (addr->ss_family == AF_INET)
  {
    struct sockaddr_in *p = (struct sockaddr_in *) addr;
    return inet_ntop (AF_INET, &p->sin_addr, buf, len);
  }
  else if (addr->ss_family == AF_INET6)
  {
    struct sockaddr_in6 *p = (struct sockaddr_in6 *) addr;
    return inet_ntop (AF_INET6, &p->sin6_addr, buf, len);
  }
#endif
  else
    return NULL;
}
