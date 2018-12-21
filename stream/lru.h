#ifndef __lru_h
#define __lru_h
#include "list.h"
#include "custck.h"
/**
 *\file lru.h
 *\brief least recently used list
 *
 *Actually this is neither LRU nor LFU, but some kind of self sorting linked list thingy.
 *Will perform badly under some workloads
 */

/**
 *\brief elements of self-sorting list
 *
 *get included in client struct<br>
 *<CODE>
 *struct asdf <br>
 *{ <br>
 *	int a; <br>
 *	int b; <br>
 *	LRUE my_e; <br>
 *	char c <br>
 *	.... <br>
 *}; <br>
 *</CODE>
 *use offsetof or similar ptr arithmetics to get to containing object
 */
typedef struct
{
  DListE e;                     ///<element's header
  uint8_t where;                ///<where this element is. 0: first half 1: second half 2: mid element
} LRUE;

///self sorting list
typedef struct
{
  DList l;                      ///<the list
  int before, behind;           ///<elements before and behind mid element
  LRUE *mid;                    ///<points to the mid element, may be NULL
} LRU;

int LRUInit (LRU * l);
int LRUClear (LRU * l, void *x, void (*destroy) (void *x, LRUE * e));
int LRUPutMid (LRU * l, LRUE * e);
int LRUGetSize (LRU * l);
LRUE *LRURemoveLast (LRU * l);
int LRURemove (LRU * l, LRUE * e);
int LRUFwd (LRU * l, LRUE * e);

#endif
