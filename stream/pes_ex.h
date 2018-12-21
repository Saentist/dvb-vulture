#ifndef __pes_ex_h
#define __pes_ex_h
#include <stdint.h>

/**
 *\file pes_ex.h
 *\brief extracts and returns PES data from a selector input
 *
 */

typedef struct
{
  uint8_t *data;                ///<points to len bytes(in the passed buffer)
  uint16_t pid;                 ///<ts bufs PID
  uint8_t len;                  ///<length of data buffer
  unsigned start:1;             ///<whether this is the first chunk of a PESPacket
} PesBuf;

typedef struct
{
  PesBuf buf;
} PesEx;

int pesExInit (PesEx * ex);
void pesExClear (PesEx * ex);
PesBuf const *pesExPut (PesEx * ex, void *tsbuf);
#endif
