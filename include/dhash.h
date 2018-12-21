#ifndef __dhash_h
#define __dhash_h
/**
 *\file dhash.h
 *\brief dynamic hash table
 *
 *A hash table that can hold arbitrary data. hash and equality function must be provided.
 *equality function shall return zero if equality exists.
 *adding and removal possible. dynamic resizing.
 */
#include <stdint.h>
#include "list.h"

typedef struct
{
  uint32_t (*hash) (void *d);
  int (*equals) (void *a, void *b);
  ///number of lists in table
  uint32_t tbl_size;
  ///number of entries in the table
  uint32_t num_entries;
  ///array of lists with elements holding the pointers
  SList *tbl;
} DHash;

int dhashInit (DHash * h, uint32_t (*hash) (void *d),
               int (*equals) (void *a, void *b));
///calls func() on every element. unordered.
void dhashForEach (DHash * h, void *x, void (*func) (void *x, void *this));
void dhashClear (DHash * h, void (*destroy) (void *d));
///find element
void *dhashGet (DHash * h, void *ref);
///like dhashGet, also removes element from map
void *dhashRemove (DHash * h, void *ref);
///deposit an element in the map
int dhashPut (DHash * h, void *d);

int dhashWrite (DHash * h, void *x,
                int (*write) (void *x, void *this, FILE * f), FILE * f);
uint64_t dhashWriteSize (DHash * h, void *x,
                         uint64_t (*size) (void *x, void *this));
int dhashRead (DHash * h, uint32_t (*hash) (void *d),
               int (*equals) (void *a, void *b), void *x,
               void *(*read) (void *x, FILE * f), FILE * f);


#endif
