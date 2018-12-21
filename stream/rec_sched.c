#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "utl.h"
#include "rec_sched.h"
#include "in_out.h"
#include "config.h"
#include "debug.h"
#include "dmalloc.h"
int DF_REC_SCHED;
#define FILE_DBG DF_REC_SCHED

static DListE *
rs_list_read (void *x NOTUSED, FILE * f)
{
  ssize_t len = sizeof (RsEntry) - sizeof (char *);
  DListE *e;
  RsEntry *sch;
  e = utlCalloc (dlistESize (sizeof (RsEntry)), 1);
  if (NULL != e)
  {
    sch = dlistEPayload (e);
    fread (sch, len, 1, f);
    sch->name = ioRdStr (f);
  }
  return e;
}

static int
rs_list_write (void *x NOTUSED, DListE * e, FILE * f)
{
  ssize_t len = sizeof (RsEntry) - sizeof (char *);
  RsEntry *sch;
  sch = dlistEPayload (e);
  fwrite (sch, len, 1, f);
  ioWrStr (sch->name, f);
  return 0;
}

int
rsInit (RecSchedFile * sch, RcFile * conf)
{
  char *fname = DEFAULT_SCHDB;
  struct stat buf;
  char *tmp;
  FILE *f;
  if (!rcfileFindVal (conf, "SCH", "filename", &tmp))
  {
    fname = tmp;
  }
  memset (sch, 0, sizeof (*sch));
  sch->file_name = fname;
  debugPri (1, "using %s\n", sch->file_name);
  if (0 == stat (fname, &buf))
  {                             //file exists
    f = fopen (fname, "rb");
    if (NULL != f)
    {
      dlistRead (&sch->sched, NULL, rs_list_read, f);
      fclose (f);
    }
  }
  return 0;
}

void
rs_list_destroy (void *x NOTUSED, DListE * e)
{
  RsEntry *rse = dlistEPayload (e);
  rsClearEntry (rse);
  utlFAN (e);
}

void
rsClear (RecSchedFile * sch)
{
  FILE *f;
  debugPri (1, "opening %s..\n", sch->file_name);
  f = fopen (sch->file_name, "wb");
  if (f)
  {
    dlistWrite (&sch->sched, NULL, rs_list_write, f);
    fclose (f);
  }
  else
    errMsg ("open failed\n");
  dlistForEach (&sch->sched, NULL, rs_list_destroy);
  return;
}

int
rsWrite (RecSchedFile * sch)
{
  FILE *f;
  debugPri (1, "opening %s..\n", sch->file_name);
  f = fopen (sch->file_name, "wb");
  if (f)
  {
    dlistWrite (&sch->sched, NULL, rs_list_write, f);
    fclose (f);
  }
  else
  {
    errMsg ("open failed\n");
    return 1;
  }
  return 0;
}

int
rsEntryCmp (RsEntry * ap, RsEntry * bp)
{
  int rv;
  rv =
    (bp->start_time < ap->start_time) ? -1 : (bp->start_time !=
                                              ap->start_time);
  if (0 == rv)
    rv = (ap->pos < bp->pos) ? -1 : (ap->pos != bp->pos);
  if (0 == rv)
    rv = (ap->freq < bp->freq) ? -1 : (ap->freq != bp->freq);
  if (0 == rv)
    rv = (ap->pol < bp->pol) ? -1 : (ap->pol != bp->pol);
  if (0 == rv)
    rv = (ap->pnr < bp->pnr) ? -1 : (ap->pnr != bp->pnr);
  if (0 == rv)
    rv = (ap->evt_id < bp->evt_id) ? -1 : (ap->evt_id != bp->evt_id);
  return rv;
}

int
rsInsertEntry (DList * sch, DListE * e)
{
  DListE *x;
  RsEntry *xe;
  RsEntry *ee = dlistEPayload (e);
  x = dlistFirst (sch);
  while (x)
  {
    xe = dlistEPayload (x);
    if (rsEntryCmp (ee, xe) >= 0)       //ee->start_time <= xe->start_time)
    {
      dlistInsertBefore (sch, e, x);
      return 0;
    }
    x = dlistENext (x);
  }
  dlistInsertLast (sch, e);
  return 0;
}

/*
RsEntry *rsGet(RecSchedFile *sch,int idx)
{
	return (idx>=REC_SCHED_SIZE)?NULL:&sch->sched[idx];
}
*/
