#include "utl.h"
#include "list.h"
#include "dmalloc_loc.h"
#include "debug.h"

#define FID_NULL '0'
#define FID_VALID 'X'

void
slistInit (SList * l)
{
  l->first = NULL;
  l->last = NULL;
}

void *
slistEPayload (SListE * e)
{
  return (void *) (e + 1);
}

int
slistForEach (SList * l, void *v, void (*func) (void *, SListE * e))
{
  SListE *e;
  e = l->first;
  while (e)
  {
    SListE *next = e->next;
    func (v, e);
    e = next;
  }
  return 0;
}

SListE *
slistFirst (SList * l)
{
  return l->first;
}

SListE *
slistLast (SList * l)
{
  return l->last;
}

SListE *
slistENext (SListE * e)
{
  return e->next;
}

SListE *
slistRemoveFirst (SList * l)
{
  SListE *f;
  if (!l->first)
    return NULL;
  f = l->first;
  l->first = f->next;
  if (!l->first)
    l->last = NULL;
  f->next = NULL;
  return f;
}

int
slistInsertFirst (SList * l, SListE * e)
{
  e->next = l->first;
  if (!e->next)
    l->last = e;
  l->first = e;
  return 0;
}

int
slistInsertLast (SList * l, SListE * e)
{
  e->next = NULL;
  if (!l->last)
    l->first = e;
  else
    l->last->next = e;
  l->last = e;
  return 0;
}

int
slistCount (SList * l)
{
  int rv = 0;
  SListE *e;
  e = slistFirst (l);
  while (e)
  {
    rv++;
    e = slistENext (e);
  }
  return rv;
}

SListE *
slistRemoveNext (SList * l, SListE * e)
{
  SListE *next;
  if (!e)
    return slistRemoveFirst (l);

  next = e->next;
  if (!next)
    return NULL;

  e->next = next->next;
  next->next = NULL;
  if (!e->next)
    l->last = e;

  return next;
}

int
slistInsertAfter (SList * l, SListE * a, SListE * b)
{
  if (!a)
  {
    b->next = l->first;
    l->first = b;
  }
  else
  {
    b->next = a->next;
    a->next = b;
  }
  if (!b->next)
    l->last = b;
  return 0;
}

size_t
slistESize (size_t size_of_payload)
{
  return sizeof (SListE) + size_of_payload;
}

int
slistWrite (SList * l, void *x,
            int (*write) (void *x, SListE * this, FILE * f), FILE * f)
{
  SListE *e;
  e = slistFirst (l);
  while (e)
  {
    fputc (FID_VALID, f);
    if (write (x, e, f))
      return 1;
    e = slistENext (e);
  }
  fputc (FID_NULL, f);
  return 0;
}

int
slistRead (SList * l, void *x, SListE * (*read) (void *x, FILE * f), FILE * f)
{
  SListE *e;
  int node_id = fgetc (f);
  l->first = 0;
  l->last = 0;
  while (node_id == FID_VALID)
  {
    e = read (x, f);
    if (!e)
      return 1;
    slistInsertLast (l, e);
    node_id = fgetc (f);
  }
  if (node_id != FID_NULL)
    return 1;
  return 0;
}

uint64_t
slistWriteSize (SList * l, void *x, uint64_t (*size) (void *x, SListE * this))
{
  uint64_t rv = 0;
  SListE *e;
  e = slistFirst (l);
//      errMsg("l: %p, e: %p, size: %p, x:%p\n",l,e,size,x);
  while (e)
  {
    rv += 1 + size (x, e);
    e = slistENext (e);
//      errMsg("e: %p\n",e);
  }
  rv += 1;
  return rv;
}

int
dlistForEach (DList * l, void *v, void (*func) (void *, DListE * e))
{
  DListE *e;
//      errMsg("dlist_for_each(%p,%p,%p)\n",l,v,func);
  e = l->first;
//      errMsg("first: %p last: %p\n",l->first,l->last);

  while (e)
  {
    DListE *next = e->next;
//              errMsg("next:%p\n",next);
    func (v, e);
    e = next;
  }
  return 0;
}

int
dlistForEachRev (DList * l, void *v, void (*func) (void *, DListE * e))
{
  DListE *e;
  e = l->last;
  while (e)
  {
    DListE *prev = e->prev;
    func (v, e);
    e = prev;
  }
  return 0;
}

void *
dlistEPayload (DListE * e)
{
  return (void *) (e + 1);
}


void
dlistInit (DList * l)
{
  l->first = NULL;
  l->last = NULL;
}

DListE *
dlistFirst (DList * l)
{
  return l->first;
}

DListE *
dlistLast (DList * l)
{
  return l->last;
}

DListE *
dlistENext (DListE * e)
{
  return e->next;
}

DListE *
dlistEPrev (DListE * e)
{
  return e->prev;
}

int
dlistRemove (DList * l, DListE * e)
{
  DListE *prev, *next;
  prev = e->prev;
  next = e->next;

  if (!prev)
    l->first = next;
  else
    prev->next = next;

  if (!next)
    l->last = prev;
  else
    next->prev = prev;

  e->next = NULL;
  e->prev = NULL;
  return 0;
}

int
dlistInsertFirst (DList * l, DListE * e)
{
  e->next = l->first;
  l->first = e;
  e->prev = NULL;

  if (e->next)
    e->next->prev = e;
  else
    l->last = e;
  return 0;
}

int
dlistInsertLast (DList * l, DListE * e)
{
  e->prev = l->last;
  l->last = e;
  e->next = NULL;

  if (e->prev)
    e->prev->next = e;
  else
    l->first = e;
  return 0;
}

int
dlistInsertAfter (DList * l, DListE * a, DListE * b)
{
  b->next = a->next;
  b->prev = a;
  a->next = b;
  if (b->next)
    b->next->prev = b;
  else
    l->last = b;
  return 0;
}

int
dlistInsertBefore (DList * l, DListE * a, DListE * b)
{
  a->prev = b->prev;
  a->next = b;
  b->prev = a;
  if (a->prev)
    a->prev->next = a;
  else
    l->first = a;
  return 0;
}

size_t
dlistESize (size_t size_of_payload)
{
  return sizeof (DListE) + size_of_payload;
}

int
dlistCount (DList * l)
{
  int rv = 0;
  DListE *e;
  e = dlistFirst (l);
  while (e)
  {
    rv++;
    e = dlistENext (e);
  }
  return rv;
}

int
dlistWrite (DList * l, void *x,
            int (*write) (void *x, DListE * this, FILE * f), FILE * f)
{
  DListE *e;
  e = dlistFirst (l);
  while (e)
  {
    fputc (FID_VALID, f);
    if (write (x, e, f))
      return 1;
    e = dlistENext (e);
  }
  fputc (FID_NULL, f);
  return 0;
}

int
dlistRead (DList * l, void *x, DListE * (*read) (void *x, FILE * f), FILE * f)
{
  DListE *e;
  int node_id = fgetc (f);
  l->first = 0;
  l->last = 0;
  while (node_id == FID_VALID)
  {
    e = read (x, f);
    if (!e)
      return 1;
    dlistInsertLast (l, e);
    node_id = fgetc (f);
  }
  if (node_id != FID_NULL)
    return 1;
  return 0;
}

uint64_t
dlistWriteSize (DList * l, void *x, uint64_t (*size) (void *x, DListE * this))
{
  uint64_t rv = 0;
  DListE *e;
  e = dlistFirst (l);
//      errMsg("write_dlist_size %p %p %p %p\n",l,x,size,e);
  while (e)
  {
    rv += 1 + size (x, e);
//              errMsg("e: %p\n",e);
    e = dlistENext (e);
  }
  rv += 1;
  return rv;
}
