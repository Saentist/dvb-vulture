#include <stdlib.h>
#include <string.h>
#include "utl.h"
#include "lru.h"
#include "dmalloc.h"

#define lru_for_each(_A,_B,_C) dlistForEach((_A),(_B),(void (*) (void *x,DListE *e))(_C))
#define lru_e_next(_A) ((LRUE *)dlistENext((DListE*)(_A)))
#define lru_e_prev(_A) ((LRUE *)dlistEPrev((DListE*)(_A)))
#define lru_insert_first(_A,_B) dlistInsertFirst((_A),(DListE*)(_B))
#define lru_insert_before(_A,_B,_C) dlistInsertBefore((_A),(DListE*)(_B),(DListE*)(_C))
#define lru_insert_after(_A,_B,_C) dlistInsertAfter((_A),(DListE*)(_B),(DListE*)(_C))
#define lru_first(_A) ((LRUE *)dlistFirst((_A)))
#define lru_last(_A) ((LRUE *)dlistLast((_A)))
#define lru_remove(_A,_B) dlistRemove((_A),(DListE*)(_B))

int
LRUInit (LRU * l)
{
  memset (l, 0, sizeof (LRU));
  return 0;
}

int
LRUClear (LRU * l, void *x, void (*destroy) (void *x, LRUE * e))
{
  lru_for_each (&l->l, x, destroy);
  memset (l, 0, sizeof (LRU));
  return 0;
}

static int
lru_l (LRU * l)
{
  if (l->before >= 1)
  {
    l->mid->where = 1;
    l->mid = lru_e_prev (l->mid);
    l->mid->where = 2;
    l->before--;
    l->behind++;
  }
  return 0;
}

static int
lru_r (LRU * l)
{
  if (l->behind >= 1)
  {
    l->mid->where = 0;
    l->mid = lru_e_next (l->mid);
    l->mid->where = 2;
    l->before++;
    l->behind--;
  }
  return 0;
}

/**
 *\brief balances lru
 * try to maintain the following invariant:
 * ((before==behind)||(before==(behind-1)))
 * but will not change balance by more than one
 *
 */
static int
lru_balance (LRU * l)
{
  if ((!l->before) && (!l->behind))
    return 0;
  if (l->before < (l->behind - 1))
    return lru_r (l);
  if (l->before > l->behind)
    return lru_l (l);
  return 0;
}


int
LRUPutMid (LRU * l, LRUE * e)
{
/*
****X****
we have to maintain a correct mid pointer on insert
*/
  if (!lru_first (&l->l))
  {                             //special case. list is empty. just insert and make it mid
    l->before = 0;
    l->behind = 0;
    lru_insert_first (&l->l, e);
    l->mid = e;
    e->where = 2;
    return 0;
  }
/*	if(l->before==l->behind)
	{do I need this?
		lru_insert_after(&l->l,l->mid,e);
		l->mid=e;
		l->before++;
		return 0;
	}*/

  if (l->before < l->behind)
  {
    lru_insert_before (&l->l, e, l->mid);
    l->before++;
    e->where = 0;
    return 0;
  }
  else
  {
    lru_insert_after (&l->l, l->mid, e);
    l->behind++;
    e->where = 1;
    return 0;
  }
}

int
LRUGetSize (LRU * l)
{
  if (!l->mid)
    return 0;
  return l->before + l->behind + 1;
}

LRUE *
LRURemoveLast (LRU * l)
{
  LRUE *e;
  if (!lru_first (&l->l))
  {
    return NULL;
  }
  lru_remove (&l->l, e = lru_last (&l->l));
  if (!lru_first (&l->l))
  {
    l->mid = NULL;
    l->behind = 0;
    l->before = 0;
    return e;
  }
  l->behind--;
  if (l->behind < l->before)
    lru_l (l);
  return e;
}

int
LRUFwd (LRU * l, LRUE * e)
{
  /*
     remove element and insert before previous one
     if e==mid
     mid=prev
     if prev==mid
     mid=e
   */
  LRUE *prev;
  prev = lru_e_prev (e);
  if (!prev)                    //already first one
    return 0;
  if (l->mid == e)
  {
    e->where = 0;
    prev->where = 2;
    l->mid = prev;

  }
  else
  {
    if (l->mid == prev)
    {
      prev->where = 1;
      e->where = 2;
      l->mid = e;
    }
  }
  lru_remove (&l->l, e);
  lru_insert_before (&l->l, e, prev);
  return 0;
}

//this is broken
int
LRURemove (LRU * l, LRUE * e)
{
  if (!lru_first (&l->l))
  {
    return 1;
  }
// *((before==behind)||(before==(behind-1)))

  if (e == l->mid)
  {                             //we wouldn't want to have a stale pointer
    if ((l->mid = lru_e_prev (e)))
    {
      e->where = 1;
      l->mid->where = 2;
      l->before--;
      l->behind++;
    }
    else
    {
      if ((l->mid = lru_e_next (e)))
      {
        e->where = 0;
        l->mid->where = 2;
        l->before++;
        l->behind--;
      }
    }
  }

  lru_remove (&l->l, e);
  if (e->where == 0)
  {
    l->before--;
  }
  else if (e->where == 1)
  {
    l->behind--;
  }
  else
  {                             //true if this is only element in list
    l->mid = NULL;
    l->behind = 0;
    l->before = 0;
  }
  lru_balance (l);

  return 0;
}
