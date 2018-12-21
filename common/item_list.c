#include <stdlib.h>
#include "item_list.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"

int DF_ITEM_LIST;
#define FILE_DBG DF_ITEM_LIST
/**
 *\file item_list.c
 * List allocator for equal sized objects
 *
 */

static void
item_list_free (void *v NOTUSED, SListE * e)
{
  debugMsg ("item_list_free: %p\n", e);
  utlFAN (e);
}

/**
 *\brief clear item_list and free any unused objects
 */
int
itemListClear (ItemList * l)
{
  slistForEach (&l->l, 0, item_list_free);
  slistInit (&l->l);
  l->count = 0;
  return 0;
}

/**
 *\brief initialize ItemList l to specified parameters
 *
 *@return 0 on success. something else on error
 */
int
itemListInit (ItemList * l, uint32_t size, uint32_t high_water)
{
  if (size < sizeof (SListE))
  {
    errMsg ("size too small\n");
    return 1;
  }
  l->count = 0;
  l->size = size;
  l->high_water = high_water;
  slistInit (&l->l);
  return 0;
}

/**
 *\brief acquire a block of memory
 *
 *@return void * to an item of l->size or NULL if calloc fails or inconsistency.
 */
void *
itemAcquire (ItemList * l)
{
  SListE *i;
  if (slistFirst (&l->l))
  {
    debugMsg ("fetching from list\n");
    if (!l->count)
    {
      errMsg
        ("ItemList inconsistency!! count is zero but elements in list?!\n");
      return NULL;
    }

    i = slistRemoveFirst (&l->l);
    l->count--;
  }
  else
  {
    debugMsg ("using calloc\n");
    i = utlCalloc (l->size, 1);
  }
  debugMsg ("PTR: %p\n", i);
  return i;
}

/**
 *\brief release a block of memory
 *
 * should have been allocated using item_acquire, of course
 * will append Item at start of list or call free on it
 */
void
itemRelease (ItemList * l, void *p)
{
  SListE *i = (SListE *) p;
  if (l->count >= l->high_water)
  {
    debugMsg ("freeing: %p\n", p);
    utlFAN (i);
  }
  else
  {
    debugMsg ("putting in list: %p\n", p);
    slistInsertFirst (&l->l, i);
    l->count++;
  }
}
