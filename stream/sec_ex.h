#ifndef __sec_ex_h
#define __sec_ex_h
#include <stdint.h>
#include "btre.h"

/**
 *\file sec_ex.h
 *\brief extracts and returns section data from a selector input
 *
 *At First I wanted to implement a similar interface as the ts-selector (wait.. read.. release..),
 *but now with changed interface semantics (wait.. while(read..){ release..} ) it doesn't work very well anymore.
 *I'd have to buffer ts packets internally, which I wanted to avoid in the first place.
 *
 *Now single packets have to be fed to secExPut() which will return completed SecBuf elements or NULL.
 *This pointer points to an internal struct and must not be deallocated.
 *events won't be processed.
 */
#define MAX_SEC_SZ 4096
typedef struct
{
  uint16_t pid;
  uint8_t data[MAX_SEC_SZ];
} SecBuf;

typedef struct
{
  SecBuf buf;                   ///<data to pass to clients
  int buf_idx;                  ///<index into the data[] field
  uint8_t *aux;                 ///<pointer which holds buffer not done yet
  uint8_t state;                ///<deframer state
} SecExState;

typedef struct
{
  void *d;                      ///<selector (for releasing elements). can really be any pointer. used for release_element().
  BTreeNode *pid_states;        ///<contains SecExState elements sorted by pid
  void (*release_element) (void *d, void *p);   ///<what to do when we're done with packet
} SecEx;

///initializes an empty extractor
int secExInit (SecEx * ex, void *d,
               void (*release_element) (void *d, void *p));
void secExClear (SecEx * ex);
///flushes extraction states and releases pending buffers
int secExFlush (SecEx * ex);
///pass buffer to the extractor and possibly retrieve output
SecBuf const *secExPut (SecEx * ex, void *input);

#endif
