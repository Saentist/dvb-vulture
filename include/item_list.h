#ifndef __item_list_h
#define __item_list_h
#include <stdint.h>
#include "list.h"

/**
 *\file item_list.h
 *\brief an allocator for fixed sized blocks
 *
 *caches some blocks in a linked list for quick retrieval
 *(not synchronized)
 *TODO: use mmap, so it's better localized and does not lock down heap/fragment
 */


typedef struct
{
  SList l;                      ///<the list holding our elements
  uint32_t high_water;          ///<start freeing if more than this number of items in list
  uint32_t count;               ///<number of items in list
  uint32_t size;                ///<size of items to allocate. has to be larger than sizeof(SListE)
} ItemList;

///initialize item list for specified size
int itemListInit (ItemList * l, uint32_t size, uint32_t high_water);
///free any elements in cache
int itemListClear (ItemList * l);
///retrieve a pointer to a memory area
void *itemAcquire (ItemList * l);
///put back a block of memory
void itemRelease (ItemList * l, void *ptr);

#endif
