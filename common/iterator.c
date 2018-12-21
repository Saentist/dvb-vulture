#include "utl.h"
#include "iterator.h"
#include "dmalloc_loc.h"

typedef struct
{
  uint32_t num_nodes;
  uint32_t idx;
  void **nodelist;
} IterBTree;

static void
iter_count_nodes (void *x, BTreeNode * this NOTUSED, int which,
                  int depth NOTUSED)
{
  uint32_t *p = x;
  if ((which == 3) || (which == 1))
  {
    (*p)++;
  }
}

static void
iter_fetch_nodes (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  IterBTree *d = x;
  if ((which == 3) || (which == 1))
  {
    d->nodelist[d->idx] = btreeNodePayload (this);
    d->idx++;
  }
}

static void
iterBTreeDestroy (void *iter)
{
  IterBTree *d = iter;
  utlFAN (d->nodelist);
  d->nodelist = NULL;
  utlFAN (d);
}

static int
iterBTreeIdx (void *iter, uint32_t idx)
{
  IterBTree *d = iter;
  if (idx >= d->num_nodes)
    return 1;
  d->idx = idx;
  return 0;
}

static uint32_t
iterBTreeGetIdx (void *iter)
{
  IterBTree *d = iter;
  return d->idx;
}

static uint32_t
iterBTreeGetSize (void *iter)
{
  IterBTree *d = iter;
  return d->num_nodes;
}

static void *
iterBTreeGetCurrent (void *iter)
{
  IterBTree *d = iter;
  return d->nodelist[d->idx];
}

int
iterBTreeInit (Iterator * i, BTreeNode * root)
{
  uint32_t ctr = 0;
  void **nodelist;
  IterBTree *d;
  d = utlCalloc (1, sizeof (IterBTree));
  if (!d)
    return 1;

  btreeWalk (root, &ctr, iter_count_nodes);
  if (!ctr)
  {
    utlFAN (d);
    return 1;
  }
  nodelist = utlCalloc (ctr, sizeof (void *));
  if (!nodelist)
  {
    utlFAN (d);
    return 1;
  }

  d->nodelist = nodelist;
  d->num_nodes = ctr;
  d->idx = 0;
  btreeWalk (root, d, iter_fetch_nodes);
  d->idx = 0;

  i->data = d;
  i->kind = I_RAND;
  i->r.idx_element = iterBTreeIdx;
  i->r.get_idx = iterBTreeGetIdx;
  i->r.get_size = iterBTreeGetSize;
  i->get_element = iterBTreeGetCurrent;
  i->i_destroy = iterBTreeDestroy;
  return 0;
}

typedef struct
{
  SList *l;
  SListE *e;
} IterSList;

static void
iterSListFirst (void *iter)
{
  IterSList *d = iter;
  d->e = slistFirst (d->l);
}

static void
iterSListLast (void *iter)
{
  IterSList *d = iter;
  d->e = slistLast (d->l);
}

static int
iterSListAdvance (void *iter)
{
  IterSList *d = iter;
  if (!d->e->next)
    return 1;

  d->e = d->e->next;
  return 0;
}

static void
iterSListDestroy (void *iter)
{
  utlFAN (iter);
}

static void *
iterSListGetCurrent (void *iter)
{
  IterSList *d = iter;
  return slistEPayload (d->e);
}

int
iterSListInit (Iterator * i, SList * l)
{
  IterSList *d;
  d = utlCalloc (1, sizeof (IterSList));
  if (!d)
    return 1;
  d->l = l;
  d->e = slistFirst (l);
  if (!d->e)
  {
    utlFAN (d);
    return 1;
  }
  i->data = d;
  i->kind = I_SEQ_FWD;
  i->s.first_element = iterSListFirst;
  i->s.last_element = iterSListLast;
  i->s.advance = iterSListAdvance;
  i->s.rewind = NULL;
  i->get_element = iterSListGetCurrent;
  i->i_destroy = iterSListDestroy;
  return 0;
}

typedef struct
{
  DList *l;
  DListE *e;
} IterDList;

static void
iterDListFirst (void *iter)
{
  IterDList *d = iter;
  d->e = dlistFirst (d->l);
}

static void
iterDListLast (void *iter)
{
  IterDList *d = iter;
  d->e = dlistLast (d->l);
}

static int
iterDListAdvance (void *iter)
{
  IterDList *d = iter;
  if (!d->e->next)
    return 1;

  d->e = d->e->next;
  return 0;
}

static int
iterDListRewind (void *iter)
{
  IterDList *d = iter;
  if (!d->e->prev)
    return 1;

  d->e = d->e->prev;
  return 0;
}

static void
iterDListDestroy (void *iter)
{
  utlFAN (iter);
}

static void *
iterDListGetCurrent (void *iter)
{
  IterDList *d = iter;
  return dlistEPayload (d->e);
}

int
iterDListInit (Iterator * i, DList * l)
{
  IterDList *d;
  d = utlCalloc (1, sizeof (IterDList));
  if (!d)
    return 1;
  d->l = l;
  d->e = dlistFirst (l);
  if (!d->e)
  {
    utlFAN (d);
    return 1;
  }
  i->data = d;
  i->kind = I_SEQ_BOTH;
  i->s.first_element = iterDListFirst;
  i->s.last_element = iterDListLast;
  i->s.advance = iterDListAdvance;
  i->s.rewind = iterDListRewind;
  i->get_element = iterDListGetCurrent;
  i->i_destroy = iterDListDestroy;
  return 0;
}

int
iterClear (Iterator * i)
{
  i->i_destroy (i->data);
  return 0;
}

int
iterFirst (Iterator * i)
{
  if (i->kind == I_RAND)
    return 1;
  i->s.first_element (i->data);
  return 0;
}

int
iterLast (Iterator * i)
{
  if (i->kind == I_RAND)
    return 1;
  i->s.last_element (i->data);
  return 0;
}

int
iterAdvance (Iterator * i)
{
  if ((i->kind != I_SEQ_FWD) && (i->kind != I_SEQ_BOTH))
    return 1;
  return i->s.advance (i->data);
}

int
iterRewind (Iterator * i)
{
  if ((i->kind != I_SEQ_REV) && (i->kind != I_SEQ_BOTH))
    return 1;
  return i->s.rewind (i->data);
}

int
iterSetIdx (Iterator * i, uint32_t idx)
{
  if (i->kind != I_RAND)
    return 1;
  return i->r.idx_element (i->data, idx);
}

uint32_t
iterGetIdx (Iterator * i)
{
  if (i->kind != I_RAND)
    return 0;
  return i->r.get_idx (i->data);
}

uint32_t
iterGetSize (Iterator * i)
{
  if (i->kind != I_RAND)
    return 0;
  return i->r.get_size (i->data);
}

void *
iterGet (Iterator * i)
{
  return i->get_element (i->data);
}
