#include <string.h>
#include <stdlib.h>
#include "debug.h"
#include "utl.h"
#include "fav_list.h"
#include "fav.h"
#include "in_out.h"
#include "dmalloc_loc.h"
int DF_FAV_LIST;
#define FILE_DBG DF_FAV_LIST

BTreeNode *
favListReadNode (void *x NOTUSED, FILE * f)
{
  BTreeNode *n;
  uint32_t i;
  FavList *fl;
  n = utlCalloc (1, btreeNodeSize (sizeof (FavList)));
  fl = btreeNodePayload (n);
  fread (&fl->size, sizeof (fl->size), 1, f);
  fl->name = ioRdStr (f);
  fl->favourites = utlCalloc (fl->size, sizeof (Favourite));
  if (!fl->favourites)
  {
    errMsg ("Sorry, couldn't calloc favourites\n");
  }
  for (i = 0; i < fl->size; i++)
  {
    favRead (fl->favourites + i, f);
  }
  return n;
}

int
favListWriteNode (void *x NOTUSED, BTreeNode * this, FILE * f)
{
  FavList *fl;
  uint32_t i;
  fl = btreeNodePayload (this);
  fwrite (&fl->size, sizeof (fl->size), 1, f);
  ioWrStr (fl->name, f);
  for (i = 0; i < fl->size; i++)
  {
    if (favWrite (fl->favourites + i, f))
      return 1;
  }
  return 0;
}

void
favListClear (FavList * fl)
{
  uint32_t i;
  if (fl->name)
    utlFAN (fl->name);
  for (i = 0; i < fl->size; i++)
  {
    favClear (fl->favourites + i);
  }
  utlFAN (fl->favourites);
  fl->size = 0;
  fl->favourites = NULL;
}
