#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include "utl.h"
#include "dhash.h"
#include "dmalloc_loc.h"
#include "debug.h"
int DF_DHASH;
#define FILE_DBG DF_DHASH

int
dhashInit (DHash * h, uint32_t (*hash) (void *d),
           int (*equals) (void *a, void *b))
{
  h->tbl = utlCalloc (32, sizeof (SList));
  if (!h->tbl)
    return 1;

  h->hash = hash;
  h->num_entries = 0;
  h->equals = equals;
  h->tbl_size = 32;
  debugMsg ("dhashInit done\n");
  return 0;
}

///calls func() on every element. unordered.
void
dhashForEach (DHash * h, void *x, void (*func) (void *x, void *this))
{
  uint32_t i;
  for (i = 0; i < h->tbl_size; i++)
  {
    SList *l = &(h->tbl[i]);
    SListE *e;
    e = slistFirst (l);
    while (e)
    {
      func (x, *((void **) slistEPayload (e)));
      e = slistENext (e);
    }
  }
}


void
dhashClear (DHash * h, void (*destroy) (void *d))
{
  uint32_t i;
  uint32_t last_size = h->tbl_size;
  uint32_t longest_cluster = 0;
  for (i = 0; i < h->tbl_size; i++)
  {
    SList *l = &(h->tbl[i]);
    SListE *e;
    uint32_t j;
    j = 0;
    e = slistRemoveFirst (l);
    while (e)
    {
      j++;
      destroy (*((void **) slistEPayload (e)));
      utlFAN (e);
      e = slistRemoveFirst (l);
    }
    if (j > longest_cluster)
    {
      longest_cluster = j;
    }
  }
  utlFAN (h->tbl);
  memset (h, 0, sizeof (DHash));
  errMsg ("dhashClear done. Longest Cluster seen: %" PRIu32 "\n",
          longest_cluster);
  errMsg ("\t final table size: %" PRIu32 "\n", last_size);
}

///find element
void *
dhashGet (DHash * h, void *ref)
{
  uint32_t hv;
  SList *l;
  SListE *e;
  hv = h->hash (ref);
  hv = hv % h->tbl_size;
  l = &(h->tbl[hv]);
  debugMsg ("dhashGet\n");

  e = slistFirst (l);
  while (e)
  {
    void **r;
    void *p;
    r = slistEPayload (e);
    p = *r;
    if (!h->equals (p, ref))
      return p;
    e = slistENext (e);
  }
  return NULL;
}

static int
dhashResize (DHash * h, uint32_t new_size)
{
  SList *new_tbl;
  uint32_t i;
  new_tbl = utlCalloc (new_size, sizeof (SList));
  if (!new_tbl)
    return 1;

  for (i = 0; i < h->tbl_size; i++)
  {
    SList *l = &(h->tbl[i]);
    SListE *e;
    e = slistRemoveFirst (l);
    while (e)
    {
      void **xp = slistEPayload (e);
      void *x = *xp;
      uint32_t idx;
      idx = h->hash (x) % new_size;
      slistInsertLast (&new_tbl[idx], e);
      e = slistRemoveFirst (l);
    }
  }
  utlFAN (h->tbl);
  h->tbl = new_tbl;
  h->tbl_size = new_size;
  return 0;
}

static int
dhashGrow (DHash * h)
{
  debugMsg ("dhashGrow\n");
  return dhashResize (h, h->tbl_size * 2);
}

static int
dhashShrink (DHash * h)
{
  debugMsg ("dhashShrink\n");
  return dhashResize (h, h->tbl_size / 2);
}

void *
dhashRemove (DHash * h, void *ref)
{
  uint32_t hv;
  SList *l;
  SListE *prev, *e;
  hv = h->hash (ref);
  hv = hv % h->tbl_size;
  l = &(h->tbl[hv]);

  prev = NULL;
  e = slistFirst (l);
  while (e)
  {
    void **r;
    void *rv;
    r = slistEPayload (e);
    rv = *r;
    if (!h->equals (rv, ref))
    {
      slistRemoveNext (l, prev);
      utlFAN (e);
      h->num_entries--;
      if ((h->num_entries < (h->tbl_size / 3)) && (h->tbl_size > 32))
        dhashShrink (h);
      return rv;
    }
    prev = e;
    e = slistENext (e);
  }
  return NULL;
}

///deposit an element in the map TODO: check if identical element already exists. Perhaps we should allow dup elements and create a get funtion to retrieve all of them
int
dhashPut (DHash * h, void *d)
{
  uint32_t hv;
  SList *l;
  SListE *e;
  void **x;
  if (dhashGet (h, d))
    return 1;
  debugMsg ("dhashPut\n");
  hv = h->hash (d);
  hv = hv % h->tbl_size;
  l = &(h->tbl[hv]);
  e = utlCalloc (1, slistESize (sizeof (void *)));
  if (!e)
    return 1;
  x = slistEPayload (e);
  *x = d;
  slistInsertLast (l, e);
  h->num_entries++;
  if ((h->num_entries > (h->tbl_size * 2 / 3)) && dhashGrow (h))
    return 1;
  return 0;
}

#define FID_DHASH 'H'
typedef struct
{
  void *x;
  int (*write) (void *x, void *this, FILE * f);
} wb_s;

static int
dhashWriteBucket (void *x, SListE * this, FILE * f)
{
  wb_s *d = x;
  return d->write (d->x, *((void **) slistEPayload (this)), f);
}

int
dhashWrite (DHash * h, void *x, int (*write) (void *x, void *this, FILE * f),
            FILE * f)
{
  uint32_t i;
  wb_s wb;
  uint8_t id = FID_DHASH;
  fwrite (&id, sizeof (uint8_t), 1, f);
  fwrite (&h->tbl_size, sizeof (uint32_t), 1, f);
  fwrite (&h->num_entries, sizeof (uint32_t), 1, f);
  wb.x = x;
  wb.write = write;
  for (i = 0; i < h->tbl_size; i++)
  {
    SList *l = &(h->tbl[i]);
    if (slistWrite (l, &wb, dhashWriteBucket, f))
      return 1;
  }
  return 0;
}

typedef struct
{
  void *x;
    uint64_t (*size) (void *x, void *this);
} sb_s;

static uint64_t
dhashBucketSize (void *x, SListE * this)
{
  sb_s *wb = x;
  debugMsg ("dhashBucketSize: %p, %p, %p\n", wb->size, wb->x,
            slistEPayload (this));
  return wb->size (wb->x, *((void **) slistEPayload (this)));
}

uint64_t
dhashWriteSize (DHash * h, void *x, uint64_t (*size) (void *x, void *this))
{
  uint32_t i;
  uint64_t rv = 0;
  sb_s wb;
  debugMsg ("dhashWriteSize\n");
  rv = sizeof (uint8_t) + sizeof (uint32_t) + sizeof (uint32_t);
  wb.x = x;
  wb.size = size;
  for (i = 0; i < h->tbl_size; i++)
  {
    SList *l = &(h->tbl[i]);
    debugMsg ("loop %" PRIu64 "\n", rv);
    rv += slistWriteSize (l, &wb, dhashBucketSize);
  }
  return rv;
}

typedef struct
{
  void *x;
  void *(*read) (void *x, FILE * f);
} rb_s;

static SListE *
dhashReadBucket (void *x, FILE * f)
{
  rb_s *d = x;
  SListE *e;
  void **vp;
  e = utlCalloc (1, slistESize (sizeof (void *)));
  if (e)
  {
    vp = slistEPayload (e);
    *vp = d->read (d->x, f);
  }
  return e;
}

int
dhashRead (DHash * h, uint32_t (*hash) (void *d),
           int (*equals) (void *a, void *b), void *x, void *(*read) (void *x,
                                                                     FILE *
                                                                     f),
           FILE * f)
{
  uint32_t i;
  rb_s wb;
  int node_id = fgetc (f);
  if (node_id != FID_DHASH)
  {
    errMsg ("wrong dhash FID %x\n", node_id);
    return 1;
  }
  fread (&h->tbl_size, sizeof (uint32_t), 1, f);
  if (h->tbl_size & (h->tbl_size - 1))
  {
    errMsg ("wrong dhash tbl_size %x\n", h->tbl_size);
    return 1;
  }
  fread (&h->num_entries, sizeof (uint32_t), 1, f);
  h->tbl = utlCalloc (h->tbl_size, sizeof (SList));
  if (!h->tbl)
  {
    errMsg ("error allocating %u elements\n", h->tbl_size);
    return 1;
  }
  h->hash = hash;
  h->equals = equals;
  wb.x = x;
  wb.read = read;
  for (i = 0; i < h->tbl_size; i++)
  {
    SList *l = &(h->tbl[i]);
    if (slistRead (l, &wb, dhashReadBucket, f))
      return 1;
  }
  return 0;
}
