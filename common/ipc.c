#ifdef __WIN32__
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "ipc.h"
#include "in_out.h"
#include "utl.h"
#include "debug.h"

int DF_IPC;
#define FILE_DBG DF_IPC

#ifdef __WIN32__
#include <winsock.h>
ssize_t
ioBlkSnd (int fd, void const *buf, size_t count)
{
  size_t sofar;
  sofar = 0;
  while (sofar < count)
  {
    ssize_t ret = send (fd, ((uint8_t *) buf) + sofar, count - sofar, 0);
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
ioBlkRcv (int fd, void *buf, size_t count)
{
  size_t sofar;
  sofar = 0;
  while (sofar < count)
  {
    ssize_t ret = recv (fd, ((uint8_t *) buf) + sofar, count - sofar, 0);
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

///should return 0 on success
static int
blk_wr (void *x, const void *buf, size_t count)
{
  return (((ssize_t) count) != ioBlkSnd ((int) x, buf, count));
}

///should return 0 on success
static int
blk_rd (void *x, void *buf, size_t count)
{
  return (((ssize_t) count) != ioBlkRcv ((int) x, buf, count));
}

#else
///should return 0 on success
static int
blk_wr (void *x, const void *buf, size_t count)
{
  return (((ssize_t) count) != ioBlkWr ((int) x, buf, count));  //*shudder*
}

///should return 0 on success
static int
blk_rd (void *x, void *buf, size_t count)
{
  return (((ssize_t) count) != ioBlkRd ((int) x, buf, count));
}
#endif
///exponentiation a**b. recursive. a,b positive constants, please.
static inline uint64_t
c_exp (uint64_t a, uint64_t b)
{
  return (((b) == 0) ? 1 : (a) * c_exp ((a), (b) - 1));
}

/**
 *\brief byte swap 64 bit quantity
 *
 * I've read somewhere that arithmetic operators are better than 
 * shifts here. There's supposedly less ambiguities between what's 'left' and 
 * 'right' shifting...
 */
static uint64_t
ipc_swap64 (uint64_t v)
{
  uint64_t tmp;
  tmp = ((v % 256) * c_exp (256ULL, 7ULL)) |
    (((v / 256ULL) % 256) * c_exp (256ULL, 6ULL)) |
    (((v / c_exp (256ULL, 2ULL)) % 256) * c_exp (256ULL, 5ULL)) |
    (((v / c_exp (256ULL, 3ULL)) % 256) * c_exp (256ULL, 4ULL)) |
    (((v / c_exp (256ULL, 4ULL)) % 256) * c_exp (256ULL, 3ULL)) |
    (((v / c_exp (256ULL, 5ULL)) % 256) * c_exp (256ULL, 2ULL)) |
    (((v / c_exp (256ULL, 6ULL)) % 256) * 256ULL) |
    (((v / c_exp (256ULL, 7ULL)) % 256));
  return tmp;
}

/**
 *\brief convert endianness of 64 bit numeric to net order
 */
uint64_t
ipcHtoNll (uint64_t v)
{
#if(__BYTE_ORDER==__LITTLE_ENDIAN)
  v = ipc_swap64 (v);
#endif
  return v;
}

/**
 *\brief convert endianness of 64 bit numeric to host order
 */
uint64_t
ipcNtoHll (uint64_t v)
{
#if(__BYTE_ORDER==__LITTLE_ENDIAN)
  v = ipc_swap64 (v);
#endif
  return v;
}

///uses size of destination operand
#define mv_tp(d_, s_) memmove(d_,s_,sizeof(*(d_)))

int
ipcOut (int (*out_f) (void *x, const void *buf, size_t count), void *x,
        void const *d, size_t sz)
{
  uint16_t v2;
  uint32_t v4;
  uint64_t v8;
  switch (sz)
  {
  case 1:
    return out_f (x, d, sz);
  case 2:
    mv_tp (&v2, d);
    v2 = htons (v2);
    return out_f (x, &v2, sz);
  case 4:
    mv_tp (&v4, d);
    v4 = htonl (v4);
    return out_f (x, &v4, sz);
  case 8:
    mv_tp (&v8, d);
    v8 = ipcHtoNll (v8);
    return out_f (x, &v8, sz);
  default:
    abort ();                   //programming error
  }
}

#undef mv_tp
///uses size of source operand
#define mv_tp(d_, s_) memmove(d_,s_,sizeof(*(s_)))
int
ipcIn (int (*in_f) (void *x, void *buf, size_t count), void *x, void *d,
       size_t sz)
{
  uint16_t v2;
  uint32_t v4;
  uint64_t v8;
  int rv;
  switch (sz)
  {
  case 1:
    return in_f (x, d, sz);
  case 2:
    rv = in_f (x, &v2, sz);
    v2 = ntohs (v2);
    mv_tp (d, &v2);
    return rv;
  case 4:
    rv = in_f (x, &v4, sz);
    v4 = ntohl (v4);
    mv_tp (d, &v4);
    return rv;
  case 8:
    rv = in_f (x, &v8, sz);
    v8 = ipcNtoHll (v8);        //does this work as expected?
    mv_tp (d, &v8);
    return rv;
  default:
    abort ();                   //programming error
  }
}

int
ipcSndN (int fd, void const *d, size_t sz)
{
  return ipcOut (blk_wr, (void *) fd, d, sz);
}

int
ipcRcvN (int fd, void *d, size_t sz)
{
  return ipcIn (blk_rd, (void *) fd, d, sz);
}

char *
ipcRcvStr (int sock)
{
  uint32_t sz;
  char *p = NULL;
  ipcRcvS (sock, sz);
  if (!sz)
    return NULL;
  p = utlCalloc (sz, sizeof (char));
  if (!p)
    return NULL;
  ioBlkRcv (sock, p, sz);
  p[sz - 1] = '\0';
  return p;
}

//#define MAX_WS_SZ 512
void
ipcSndStr (int sock, char *s)
{
  uint32_t sz;
  if (!s)
  {
    sz = 0;
    ipcSndS (sock, sz);
    return;
  }

  sz = strlen (s) + 1;
  ipcSndS (sock, sz);
  ioBlkSnd (sock, s, sz);
  return;
}
