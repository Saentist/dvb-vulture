#include <stdlib.h>
#include <inttypes.h>
#include "rcptr.h"
#include "utl.h"
#include "assert.h"
#include "debug.h"
#include "dmalloc_loc.h"
int DF_RCPTR;
#define FILE_DBG DF_RCPTR

///This gives us a chance to detect invalid pointers
#define RCPTR_ID 0xa32f801b


/**
 *\brief reference counter
 */
typedef struct
{
  uint32_t ctr;                 ///<reference counter
  uint32_t id;                  ///<is checked for valid id
} RCPtr;

void *
rcptrMalloc (size_t size)
{
  RCPtr *p;
  p = utlCalloc (sizeof (RCPtr) + size, 1);
  if (!p)
  {
    debugMsg ("allocation failed\n");
    return NULL;
  }
  p->ctr = 1;
  p->id = RCPTR_ID;
  debugMsg ("allocated size: %u at:%" PRIx32 "\n", size, (uint32_t) (p + 1));
  return p + 1;                 //add offset
}

int
rcptrAcquire (void *ptr)
{
  RCPtr *p = ptr;
  p -= 1;                       //remove offset
  if (p->id != RCPTR_ID)
  {
    errMsg ("rcptr %x id mismatch , want: %x, got: %" PRIx32 "\n", p + 1,
            RCPTR_ID, p->id);
    debugTrace ();
    return -1;
  }
  p->ctr++;
  debugMsg ("incremented refcount of rcptr %x, new refcount: %" PRIu32 "\n",
            p + 1, p->ctr);
  return 0;
}

int
rcptrRelease (void *ptr)
{
  RCPtr *p = ptr;
  p -= 1;                       //remove offset
  if ((p->id != RCPTR_ID) || (p->ctr == 0))
  {
    errMsg ("rcptr %x id mismatch , want: %x, got: %" PRIx32 "\n", p + 1,
            RCPTR_ID, p->id);
    return -1;
  }

  p->ctr--;
  debugMsg ("decremented refcount of rcptr %x, new refcount: %" PRIu32 "\n",
            p + 1, p->ctr);
  if (!p->ctr)
  {
    debugMsg ("freeing rcptr %x\n", p + 1);
    p->id = 0;
    utlFAN (p);
    return 1;
  }
  return 0;
}

uint32_t
rcptrRefcount (void *ptr)
{
  RCPtr *p = ptr;
  p -= 1;                       //remove offset
  if ((p->id != RCPTR_ID) || (p->ctr == 0))
  {
    errMsg ("counter zero or rcptr %x id mismatch , want: %x, got: %" PRIx32
            "\n", p + 1, RCPTR_ID, p->id);
    return 0;
  }

  return p->ctr;
}

int
rcptrListInit (ItemList * l, uint32_t size, uint32_t high_water)
{
  return itemListInit (l, size + sizeof (RCPtr), high_water);
}

int
rcptrListClear (ItemList * l)
{
  return itemListClear (l);
}

void *
rcptrMallocIl (ItemList * l)
{
  RCPtr *p;
  p = itemAcquire (l);
  if (!p)
  {
    debugMsg ("allocation failed\n");
    return NULL;
  }
  p->id = RCPTR_ID;
  p->ctr = 1;
  p++;
  return p;
}

int
rcptrReleaseIl (ItemList * l, void *ptr)
{
  RCPtr *p = ptr;
  p -= 1;                       //remove offset
  if ((p->id != RCPTR_ID) || (p->ctr == 0))
  {
    errMsg ("counter zero or rcptr %x id mismatch, want: %x, got: %" PRIx32
            "\n", p + 1, RCPTR_ID, p->id);
    assert (p->id == RCPTR_ID); //where does it break?
    return -1;
  }

  p->ctr--;
  debugMsg ("decremented refcount of rcptr %x, new refcount: %" PRIu32 "\n",
            p + 1, p->ctr);
  if (!p->ctr)
  {
    debugMsg ("freeing rcptr %x\n", p + 1);
    p->id = 0;
    itemRelease (l, p);
    return 1;
  }
  return 0;
}
