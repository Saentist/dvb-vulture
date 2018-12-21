#ifndef __fav_list_h
#define __fav_list_h
#include "btre.h"
#include "fav.h"
typedef struct
{
  uint32_t size;                ///<number of elements
  char *name;                   ///<list name
  Favourite *favourites;        ///<fav list
} FavList;

BTreeNode *favListReadNode (void *x, FILE * f); ///<alloc and read a node _containing_ a FavList
int favListWriteNode (void *x, BTreeNode * this, FILE * f);     ///<write a node _containing_ a FavList
void favListClear (FavList * fl);

#endif
