#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "utl.h"
#include "custck.h"
#include "debug.h"

#include "dmalloc_loc.h"


#define INIT_CU_SIZE (32)
#define GROW_CU_SIZE (32)

int DF_CUSTCK;
#define FILE_DBG DF_CUSTCK

CUStack *
cuStackInit (CUStack * st)
{
  if (!st)
    return 0;
  st->Stack = utlCalloc (INIT_CU_SIZE, sizeof (CUStackElem));
  if (!st->Stack)
  {
    return 0;
  }
  st->Size = INIT_CU_SIZE;
  st->Index = 0;
  return st;
}

CUStack *
cuStackInitSize (CUStack * st, unsigned size)
{
  if (!st)
    return 0;
  st->Stack = utlCalloc (size, sizeof (CUStackElem));
  if (!st->Stack)
  {
    return 0;
  }
  st->Size = size;
  st->Index = 0;
  return st;
}


void
cuStackUninit (CUStack * st)
{
  utlFAN (st->Stack);
  st->Size = 0;
  st->Stack = NULL;
}

static void
cu_stack_fclose (void *vp)
{
  FILE *fp = (FILE *) vp;
  fclose (fp);
}

int
cuStackGrow (CUStack * st)
{
  unsigned int size = st->Size + GROW_CU_SIZE;
  CUStackElem *sp;
  debugMsg ("Growing Stack\n");
  sp = realloc (st->Stack, sizeof (CUStackElem) * size);
  if (!sp)
    return 1;
  debugMsg ("Success\n");
  st->Stack = sp;
  st->Size = size;
  return 0;
}

int
cuStackPush (void *handle, void (*ff) (void *handle), CUStack * st)
{
  if (st->Index == (st->Size))
  {
    if (cuStackGrow (st))
      return 1;
  }
  st->Stack[st->Index].ff = ff;
  st->Stack[st->Index].Handle = handle;
  st->Index++;
  debugMsg ("pushed func addr: %p\n", ff);
  debugMsg ("          Handle: %p\n", handle);
  return 0;
}


int
cuStackPop (CUStack * st)
{
  if (!st->Index)
  {
    return 1;
  }
  st->Index--;
  debugMsg ("popping func addr: %p\n", (void *) st->Stack[st->Index].ff);
  debugMsg ("          Handle: %p\n", st->Stack[st->Index].Handle);
  st->Stack[st->Index].ff (st->Stack[st->Index].Handle);
  return 0;
}


void
cuStackCleanup (CUStack * st)
{
  while (st->Index)
    cuStackPop (st);
  cuStackUninit (st);
}


void
cuStackFail (int failed, void *handle, void (*ff) (void *hnd), CUStack * st)
{
  if (failed || cuStackPush (handle, ff, st))
    cuStackCleanup (st);
}


void *
CUmalloc (size_t size, CUStack * st)
{
  void *rv = malloc (size);
  cuStackFail ((int) (!rv), (void *) (rv), free, st);
  return rv;
}


void *
CUcalloc (size_t nmemb, size_t size, CUStack * st)
{
  void *rv = calloc (nmemb, size);
  cuStackFail ((int) (!rv), (void *) (rv), free, st);
  return rv;
}

static void
cu_utl_free (void *p)
{
  utlFree (p);
}

void *
CUutlMalloc (size_t size, CUStack * st)
{
  void *rv = utlMalloc (size);
  cuStackFail ((int) (!rv), (void *) (rv), cu_utl_free, st);
  return rv;
}

void *
CUutlCalloc (size_t nmemb, size_t size, CUStack * st)
{
  void *rv = utlCalloc (nmemb, size);
  cuStackFail ((int) (!rv), (void *) (rv), cu_utl_free, st);
  return rv;
}

FILE *
CUfopen (const char *filename, const char *mode, CUStack * st)
{
  FILE *rv = fopen (filename, mode);
  cuStackFail ((int) (!rv), (void *) (rv), cu_stack_fclose, st);
  return rv;
}

void
CUclose (void *fd)
{
  int rv;
  rv = close ((int) fd);
  if (rv == -1)
    errMsg ("closing failed %s\n", strerror (errno));
}

int
CUopen (char *filename, int flags, mode_t mode, CUStack * s)
{
  int rv = open (filename, flags, mode);
  cuStackFail ((rv == -1), (void *) rv, CUclose, s);
  return rv;
}
