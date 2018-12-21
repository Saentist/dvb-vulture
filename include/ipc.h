#ifndef __ipc_h
#define __ipc_h
//for size_t
#include<stdio.h>

/**
 *\file ipc.h
 *\brief prototypes for inter-process communications.
 *
 *Assumes two's complement numbers 1,2,4,8 bytes in length. 
 *Will byte swap if needed. 
 */

///send a numeric(integer) value, byte swapping if necessary. \return 1 on error, 0 otherwise.
int ipcSndN (int fd, const void *d, size_t sz);
///send a numeric value, byte swapping if necessary. deduce size from type.
#define ipcSndS(fd_,n_) ipcSndN((fd_),&(n_),sizeof(n_))
///receive a numeric value, byte swapping if necessary. \return 1 on error, 0 otherwise.
int ipcRcvN (int fd, void *d, size_t sz);
///receive a numeric value byte swapping if necessary. deduce size from type.
#define ipcRcvS(fd_,n_) ipcRcvN((fd_),&(n_),sizeof(n_))

///same as above, but with custom output/input functions. \return what out_f returns
int ipcOut (int (*out_f) (void *x, const void *buf, size_t count), void *x,
            void const *d, size_t sz);
///same as above, but with custom output/input functions. \return what in_f returns
int ipcIn (int (*in_f) (void *x, void *buf, size_t count), void *x, void *d,
           size_t sz);

char *ipcRcvStr (int sock);
void ipcSndStr (int sock, char *s);

#endif
