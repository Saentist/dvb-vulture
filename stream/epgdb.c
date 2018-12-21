#include <string.h>
#include <errno.h>
#include <stdio_ext.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "bool2.h"
#include <unistd.h>
#include "bits.h"
#include "utl.h"
#include "in_out.h"
#include "eis.h"
#include "epgdb.h"
#include "config.h"
#include "debug.h"
#include "timestamp.h"
#include "epg_pgm.h"
#include "epg_ts.h"
#include "dvb.h"
#include "dmalloc.h"
int DF_EPGDB;
#define FILE_DBG DF_EPGDB
//-----------------------some(only needed) prototypes-----------------------------------------------
static int epgdb_write_index (EpgDb * db);
static int epgdb_read_index (FpPointer od_root, EpgDb * db);
static void epgdb_delete (EpgDb * db);
static void epgdb_destroy_index (EpgDb * db);
static void *epgdb_store_loop (void *p);
static void epgdb_release_se (void *d, void *p);
static void epgdb_swap_out_node (void *x, BTreeNode * this, int which,
                                 int depth);
static int epgdb_get_evts (uint32_t * num_p, uint8_t *** evt_ppp,
                           uint8_t * sec, time_t min, time_t end,
                           uint32_t max_num);
static EpgTs *epgdb_read_ts_pgms (EpgDb * db, uint16_t tsid, uint32_t pos);
static EpgPgm *epgdb_ts_get_pgm (EpgTs * ts, uint16_t pnr);
//----------------------------------------------init/teardown---------------------------------------
int
epgdbInit (EpgDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb)
{
  Section *s;
  long tmp;
//      uint16_t epg_pid=0x0012;
  FpPointer od_root = 0, tmp_ptr;
  FpSize frag_sz;
  int dbinit = 0;
  char *v;
  struct stat buf;
  int setvbuf_bytes;
  memset (db, 0, sizeof (EpgDb));
  db->dbfilename = DEFAULT_EPGDB;
  debugPri (1, "epgdb_init1\n");
  s = rcfileFindSec (cfg, "EPG");
  debugPri (1, "epgdb_init2\n");
//      db->current_ts=NULL;
  if (s && (!rcfileFindSecVal (s, "database", &v)))
  {
    db->dbfilename = v;
  }
  db->niceness = 0;
  if (s && (!rcfileFindSecValInt (s, "niceness", &tmp)) && (tmp >= -20)
      && (tmp <= 19))
  {
    db->niceness = tmp;
  }
  setvbuf_bytes = -1;
  if (s && (!rcfileFindSecValInt (s, "setvbuf_bytes", &tmp)))
  {
    setvbuf_bytes = tmp;
    debugPri (1, "File Buffer size is %u\n", setvbuf_bytes);
  }

  frag_sz = 0;
  //fragment size for file. small values cause fragmentation. 0 leaves default.
  if (s && (!rcfileFindSecValInt (s, "frag_sz_mb", &tmp)))
  {
    frag_sz = tmp * 1024 * 1024;
  }

  if (stat (db->dbfilename, &buf))
  {
    if (errno == ENOENT)
    {
      int f;
      debugPri (1, "%s nonexistent, creating..\n", db->dbfilename);
      f = creat (db->dbfilename, S_IRUSR | S_IWUSR);
      close (f);
      dbinit = 1;
    }
    else
    {
      errMsg ("stat on %s: %s\n", db->dbfilename, strerror (errno));
      return 1;
    }
  }
  debugPri (1, "opening file\n");
  if (!(db->handle = fopen (db->dbfilename, "r+")))
  {
    errMsg ("fopen: %s\n", strerror (errno));
    return 1;
  }
  db->vbuf = NULL;
  ioSetBuffer (db->handle, setvbuf_bytes, &db->vbuf);

  if (!dbinit)
  {
    if (fseeko (db->handle, 0, SEEK_SET))
    {
      errMsg ("fseeko: %s\n", strerror (errno));
      fclose (db->handle);
      utlFAN (db->vbuf);
      return 1;
    }
    fread (&od_root, sizeof (FpPointer), 1, db->handle);
    if (od_root == 0)
    {
      dbinit = 1;
    }
/*
		if(s&&(!rcfileFindSecValInt(s,"purge",&tmp))&&tmp)
		{
			dbinit=1;
		}*/
  }

  if (taskInit (&db->t, db, epgdb_store_loop))
  {
    errMsg ("task failed to init\n");
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }

  db->dvb = dvb;
  db->pgmdb = pgmdb;
  if (!(db->o = dvbDmxOutAdd (dvb)) ||
      !(db->input = svtOutSelPort (db->o))
      || secExInit (&db->ex, svtOutSelector (db->o), epgdb_release_se))
  {
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }

  if (dbinit)
  {
    FP_GO (sizeof (FpPointer), db->handle);
    if (fphInit (db->handle, &(db->h)))
    {
      fclose (db->handle);
      utlFAN (db->vbuf);
      return 1;
    }
    if (frag_sz)
      fphSetFragmentSize (&db->h, frag_sz);
    FP_GO (0, db->handle);
    od_root = 0;
    fwrite (&od_root, sizeof (FpPointer), 1, db->handle);
/*		if(pgmHashInit(&db->root))
		{
			errMsg("failed to initialize pgm hash\n");
			fclose(db->handle);
			utlFAN(db->vbuf);
			return 1;
		}*/
    db->root = NULL;
  }
  else
  {
    FP_GO (sizeof (FpPointer), db->handle);
    debugPri (1, "attaching heap\n");
    if (fphAttach (db->handle, &(db->h)))
    {
      debugPri (1, "attaching failed, trying to reinitialize\n");
      FP_GO (sizeof (FpPointer), db->handle);
      if (fphInit (db->handle, &(db->h)))
      {
        fclose (db->handle);
        utlFAN (db->vbuf);
        return 1;
      }
      if (frag_sz)
        fphSetFragmentSize (&db->h, frag_sz);
      FP_GO (0, db->handle);
      od_root = 0;
      fwrite (&od_root, sizeof (FpPointer), 1, db->handle);
/*			if(pgmHashInit(&db->root))
			{
				errMsg("failed to initialize pgm hash\n");
				fclose(db->handle);
				utlFAN(db->vbuf);
				return 1;
			}*/
      db->root = NULL;
    }
    else
    {                           //Geez it worked after all.. I'm sooo excited
      if (frag_sz)
        fphSetFragmentSize (&db->h, frag_sz);
      FP_GO (0, db->handle);
      debugPri (1, "starting index read\n");
      fread (&od_root, sizeof (FpPointer), 1, db->handle);

      tmp_ptr = 0;
      FP_GO (0, db->handle);
      fwrite (&tmp_ptr, sizeof (FpPointer), 1, db->handle);
      if (epgdb_read_index (od_root, db))
      {
        errMsg ("failed to read saved index, invalidating database\n");
        fclose (db->handle);
        utlFAN (db->vbuf);
        return 1;
      }
      fphFree (od_root, &db->h);
    }
  }

//      db->db_base=epg_get_buf_time()-3600;//so the ..._hourly triggers as early as possible
//      pthread_mutex_init(&db->mx,NULL);
  debugPri (1, "epgdb_init complete\n");
  return 0;
}


void
epgdbClear (EpgDb * db)
{
  debugPri (1, "epgdbStop\n");
  epgdbStop (db);
  debugPri (1, "taskClear\n");
  taskClear (&db->t);
  btreeWalk (db->root, db, epgdb_swap_out_node);
  debugPri (1, "epgdb_write_index\n");
  if (epgdb_write_index (db))
    epgdb_delete (db);
  debugPri (1, "epgdb_destroy_index\n");
  epgdb_destroy_index (db);
  secExClear (&db->ex);
  dvbDmxOutRmv (db->o);
//  dvbTsOutputClear (db->dvb, db->input);
}

void
CUepgdbClear (void *db)
{
  epgdbClear (db);
}

int
CUepgdbInit (EpgDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb,
             CUStack * s)
{
  int rv;
  rv = epgdbInit (db, cfg, dvb, pgmdb);
  cuStackFail (rv, db, CUepgdbClear, s);
  return rv;
}



//-----------------------------------------------------public functions------------------------------------------------------
int
epgdbStart (EpgDb * db)
{
  return taskStart (&db->t);
}

int
epgdbStop (EpgDb * db)
{
  return taskStop (&db->t);
}

EpgArray *
epgdbGetEvents (EpgDb * db, uint32_t pos, uint16_t tsid, uint16_t * pnr_range,
                uint32_t num_pnrs, time_t start_time, time_t end_time,
                uint32_t num_entries)
{
  uint32_t i;
  int x;
  EpgArray *rv;
  CUStack st;
  EpgTs *tsp;

  if ((start_time >= end_time) || (num_pnrs == 0))
    return NULL;

  if (!cuStackInit (&st))
    return NULL;
  rv = CUutlCalloc (1, sizeof (EpgArray), &st);
  if (!rv)
  {
    return NULL;
  }
  rv->pgm_nrs = CUutlCalloc (num_pnrs, sizeof (uint16_t), &st);
  if (!rv->pgm_nrs)
  {
    return NULL;
  }
  rv->sched = CUutlCalloc (num_pnrs, sizeof (EpgSchedule), &st);
  if (!rv->sched)
  {
    return NULL;
  }
  rv->num_pgm = num_pnrs;
  taskLock (&db->t);

  tsp = epgdb_read_ts_pgms (db, tsid, pos);
  if (NULL == tsp)
  {
    taskUnlock (&db->t);
    return NULL;
  }

  for (i = 0; i < num_pnrs; i++)
  {
    EpgPgm *pgm;
    pgm = epgdb_ts_get_pgm (tsp, pnr_range[i]);
    rv->pgm_nrs[i] = pnr_range[i];
    rv->sched[i].num_events = 0;
    rv->sched[i].events = NULL;
    x = 0;
    if (!pgm)                   //!pn)
    {
      debugPri (1, "num_events: %d\n", 0);
      rv->sched[i].num_events = 0;
      rv->sched[i].events = NULL;
    }
    else
    {
      int j;
      for (j = 0; j < 16; j++)
      {
        EiTbl *t = pgm->sched + j;
        if (NULL != t->secs)
        {                       //something's here
          int k;
          for (k = 0; k <= t->last_sec; k++)
          {
            if (t->secs[k])
            {
              uint8_t tsec[4096];
              int sec_sz;
              memmove (tsec, t->secs[k], 3);
              sec_sz = get_sec_size (tsec);
              if (sec_sz > 4096)
                sec_sz = 4096;
              memmove (tsec, t->secs[k], sec_sz);
              if (2 ==
                  (x =
                   epgdb_get_evts (&rv->sched[i].num_events,
                                   &rv->sched[i].events, tsec, start_time,
                                   end_time, num_entries)))
                break;
            }
          }
          if (rv->sched[i].events)
            cuStackPush (rv->sched[i].events, free, &st);
        }
        if (x == 2)
          break;
      }
    }
  }
  cuStackUninit (&st);
  taskUnlock (&db->t);
  return rv;
}

//----------------------------------------------------static(local) functions----------------------------------------------
int
cmp_epg_pgm (void *x NOTUSED, const void *a, const void *b)
{
  const EpgPgm *ap = a;
  const EpgPgm *bp = b;
  if (ap->pnr > bp->pnr)
    return -1;
  else if (ap->pnr < bp->pnr)
    return 1;
  else
    return 0;
}

int
cmp_epg_ts (void *x NOTUSED, const void *a, const void *b)
{
  const EpgTs *ap = a;
  const EpgTs *bp = b;
  if (ap->key > bp->key)
    return -1;
  else if (ap->key < bp->key)
    return 1;
  else
    return 0;
}

static EpgPgm *
epgdb_ts_get_pgm (EpgTs * ts, uint16_t pnr)
{
  EpgPgm cv;
  EpgPgm *pgm;
  BTreeNode *pp;
  cv.pnr = pnr;
  pp = btreeNodeFind (ts->root, &cv, NULL, cmp_epg_pgm);
  if (NULL == pp)
    return NULL;
  pgm = btreeNodePayload (pp);
  return pgm;
}

static int
epgdb_eitbl_read (EiTbl * t, FILE * f)
{
  int c;
  int i;
  c = fgetc (f);
  if (c == 'V')
  {
    fread (t, sizeof (EiTbl) - sizeof (uint8_t **), 1, f);
    t->secs = utlCalloc (sizeof (uint8_t *), t->last_sec + 1);
    if (NULL == t->secs)
    {
      errMsg ("allocation failed\n");
      return 1;
    }
    for (i = 0; i <= t->last_sec; i++)
    {
      c = fgetc (f);
      if (c == 'v')
      {                         //read section
        uint8_t sec[4096];
        int sz;
        fread (sec, 3, 1, f);
        sz = get_sec_size (sec);
        if (sz <= 4096)
        {
          fread (sec + 3, sz - 3, 1, f);
          t->secs[i] = utlCalloc (sz, 1);
          if (NULL != t->secs[i])
          {
            memmove (t->secs[i], sec, sz);
          }
        }
        else
        {
          errMsg ("Size too large\n");
          t->secs[i] = NULL;
        }
      }
      else
      {
        if (c != 'n')
          errMsg ("Wrong ID want: n got: %c\n", c);
        t->secs[i] = NULL;
      }
    }
  }
  else
  {
    if (c != 'N')
      errMsg ("Wrong ID want: N got: %c\n", c);
    memset (t, 0, sizeof (EiTbl));
  }
  return 0;
}

static BTreeNode *
epgdb_read_pgm_node (void *x NOTUSED, FILE * f)
{
  int i;
  EpgPgm *pp;
  BTreeNode *p;
  p = utlCalloc (1, btreeNodeSize (sizeof (EpgPgm)));
  if (!p)
    return NULL;
  pp = btreeNodePayload (p);
  fread (pp, sizeof (EpgPgm) - sizeof (pp->sched), 1, f);
  for (i = 0; i < 16; i++)
  {
    epgdb_eitbl_read (pp->sched + i, f);
  }
  return p;
}

/**
 *\brief reads node from disk and frees it
 *
 */
static int
epgdb_swap_in_ts (EpgDb * db, EpgTs * tsp)
{
  BTreeNode *n = NULL;
  if (!tsp->od)
    return 0;
  if (0 == tsp->root_p)
  {
    errMsg ("root_p==0");
    tsp->root = NULL;
    tsp->od = false;
    return 0;
  }
  FP_GO (tsp->root_p, db->h.f);
  if (btreeRead (&n, NULL, epgdb_read_pgm_node, db->h.f))
  {
    tsp->root = NULL;
    tsp->od = false;
    errMsg ("failed reading epg pgm nodes\n");
    return 1;
  }
  fphFree (tsp->root_p, &db->h);
  tsp->root = n;
  tsp->od = false;
  return 0;
}

/**
 *\brief reads node from disk and frees it
 *
 * expects to be called with selector locked
 * unlocks on disk access
 */
static int
epgdb_swap_in_ts_locked (EpgDb * db, EpgTs * tsp)
{
  BTreeNode *n = NULL;
  if (!tsp->od)
    return 0;
  if (0 == tsp->root_p)
  {
    errMsg ("root_p==0");
    tsp->root = NULL;
    tsp->od = false;
    return 0;
  }
  selectorUnlock (svtOutSelector (db->o));
  FP_GO (tsp->root_p, db->h.f);
  if (btreeRead (&n, NULL, epgdb_read_pgm_node, db->h.f))
  {
    tsp->root = NULL;
    tsp->od = false;
    errMsg ("failed reading epg pgm nodes\n");
    selectorLock (svtOutSelector (db->o));
    return 1;
  }
  fphFree (tsp->root_p, &db->h);
  selectorLock (svtOutSelector (db->o));
  tsp->root = n;
  tsp->od = false;
  return 0;
}

static uint64_t
epgdb_eitbl_size (EiTbl * t)
{
  uint64_t sz = sizeof (char);
  if (t->secs)
  {
    int i;
    sz += sizeof (EiTbl) - sizeof (uint8_t **);
    i = 0;
    for (i = 0; i <= t->last_sec; i++)
    {
      sz += sizeof (char);
      if (t->secs[i])
      {
        int sec_sz;
        sec_sz = get_sec_size (t->secs[i]);
        if (sec_sz <= 4096)
        {
          sz += sec_sz;
        }
      }
    }
  }
  return sz;
}

static uint64_t
epgdb_pgm_size (EpgPgm * pp)
{
  uint64_t sz = 0;
  int i;
  sz = sizeof (EpgPgm) - sizeof (pp->sched);
  for (i = 0; i < 16; i++)
  {
    sz += epgdb_eitbl_size (pp->sched + i);
  }
  debugPri (1, "epgdb_pgm_size: %llu\n", sz);
  return sz;
}

static uint64_t
epgdb_pgm_node_size (void *x, BTreeNode * pn)
{
  EpgPgm *pp = btreeNodePayload (pn);
  debugPri (1, "epgdb_pgm_node_size: x: %p, pn: %p\n", x, pn);
  return epgdb_pgm_size (pp);
}

static int
epgdb_eitbl_write (EiTbl * t, FILE * f)
{
  if (t->secs)
  {
    int i;
    char buf[257];
    fputc ('V', f);
    fwrite (t, sizeof (EiTbl) - sizeof (uint8_t **), 1, f);
//              fwrite(t->secs,sizeof(FpPointer),t->last_sec+1,f);
    i = 0;
    for (i = 0; i <= t->last_sec; i++)
    {
      if (t->secs[i])
      {
        int sz;
        sz = get_sec_size (t->secs[i]);
        if (sz <= 4096)
        {
          fputc ('v', f);
          fwrite (t->secs[i], sz, 1, f);
        }
        else
        {
          errMsg ("size too large\n");
          fputc ('n', f);
        }
      }
      else
        fputc ('n', f);

      if (t->secs[i])
        buf[i] = '1';
      else
        buf[i] = '0';
    }
    buf[i] = '\0';
    debugPri (1, "Writing EiTbl: last_sec: %hhu present: %s\n", t->last_sec,
              buf);
  }
  else
  {
    fputc ('N', f);
    debugPri (1, "Writing EiTbl: no sections\n");
  }
  return 0;
}

static int
epgdb_write_pgm (EpgPgm * pp, FILE * f)
{
  int i;
//      DList * l_p;
  fwrite (pp, sizeof (EpgPgm) - sizeof (pp->sched), 1, f);
  debugPri (1, "Writing EpgPgm: pnr: %hu\n", pp->pnr);
  for (i = 0; i < 16; i++)
  {
    epgdb_eitbl_write (pp->sched + i, f);
  }
  return 0;
}

static int
epgdb_write_pgm_node (void *x NOTUSED, BTreeNode * this, FILE * f)
{
  EpgPgm *pp = btreeNodePayload (this);
  return epgdb_write_pgm (pp, f);
}

static void
epgdb_eitbl_clear (EiTbl * t)
{
  int i;
  if (t->secs)
  {
    for (i = 0; i <= t->last_sec; i++)
    {
      if (t->secs[i])
        utlFAN (t->secs[i]);
      t->secs[i] = NULL;
    }
    utlFAN (t->secs);
  }
  memset (t, 0, sizeof (EiTbl));
}

static void
epgdb_clear_pgm_node (EpgPgm * pp)
{
  int i;
//      DList * l_p;
  for (i = 0; i < 16; i++)
  {
    epgdb_eitbl_clear (pp->sched + i);
  }
  /*
     l_p=(DList*)(pp+1);
     for(i=0;i<pp->buffer_size;i++)
     {
     dlistForEach(l_p+i,NULL,epgdb_destroy_entry);
     } */
}

static void
epgdb_destroy_pgm_node (void *x NOTUSED, BTreeNode * this, int which,
                        int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    EpgPgm *ep = btreeNodePayload (this);
    epgdb_clear_pgm_node (ep);
    utlFAN (this);
  }
}

static int
epgdb_swap_out_ts (EpgDb * db, EpgTs * tsp)
{
//      BTreeNode * n=NULL;
  uint64_t sz;
  FpPointer ptr;
  if (tsp->od)
    return 0;

  sz = btreeWriteSize (tsp->root, NULL, epgdb_pgm_node_size);
  ptr = fphCalloc (sz, 1, &db->h);
  if (!ptr)
  {
    errMsg ("failed allocating epg pgm nodes\n");
    tsp->root_p = 0;
    tsp->od = true;
    return 1;
  }
  FP_GO (ptr, db->h.f);
  if (btreeWrite (tsp->root, NULL, epgdb_write_pgm_node, db->h.f))
  {
    tsp->root_p = 0;
    tsp->od = true;
    errMsg ("failed writing epg pgm nodes\n");
    return 1;
  }
  btreeWalk (tsp->root, NULL, epgdb_destroy_pgm_node);
  tsp->root_p = ptr;
  tsp->od = true;
  return 0;
}


static void
epgdb_swap_out_node (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  EpgDb *db = x;
  if ((which == 2) || (which == 3))
  {
    EpgTs *tp = btreeNodePayload (this);
    epgdb_swap_out_ts (db, tp);
  }
}

/**
 *\brief reads ts node from disk
 *
 */
static EpgTs *
epgdb_read_ts_pgms (EpgDb * db, uint16_t tsid, uint32_t pos)
{
  EpgTs cv;
  EpgTs *tsp;
  BTreeNode *pp;
  cv.tsid = tsid;
  cv.pos = pos;
  pp = btreeNodeFind (db->root, &cv, NULL, cmp_epg_ts);
  if (NULL == pp)
    return NULL;
  tsp = btreeNodePayload (pp);
  epgdb_swap_in_ts (db, tsp);
  return tsp;
}

static int
epgdb_get_evts (uint32_t * num_p, uint8_t *** evt_ppp, uint8_t * sec,
                time_t min, time_t end, uint32_t max_num)
{
  Iterator iter;
  int done = 0;
  uint32_t num = *num_p;
  unsigned array_size = num * sizeof (uint8_t *);
  unsigned alloc_sz = array_size;
  uint8_t **evt_pp = *evt_ppp;
  uint8_t *ev_start;
  time_t s, e;
  unsigned evt_sz;

  if ((0 == end) && (num >= max_num))
    return 0;

  if (iterEisSecInit (&iter, sec))
    return 1;
  if (!(ev_start = iterGet (&iter)))
  {
    iterClear (&iter);
    return 1;
  }
  do
  {
    uint8_t *evt;
    ev_start = iterGet (&iter);
    evt_sz = evtGetSize (ev_start);
    s = evtGetStart (ev_start);
    e = s + evtGetDuration (ev_start);
    if ((e > min)
        && (((end != 0) && (s < end)) || ((end == 0) && (num < max_num))))
    {
      evt = utlCalloc (evt_sz, 1);
      if (NULL == evt)
      {
        iterClear (&iter);
        *num_p = num;           //array_size/sizeof(uint8_t *);
        *evt_ppp = evt_pp;
        return 1;
      }
      memmove (evt, ev_start, evt_sz);
      utlSmartMemmove ((uint8_t **) & evt_pp, &array_size, &alloc_sz,
                       (uint8_t *) & evt, sizeof (uint8_t *));
      num++;
    }
    if (((end != 0) && (s >= end)) || ((end == 0) && (num >= max_num)))
    {
      done = 1;
    }

  }
  while (!iterAdvance (&iter));
  iterClear (&iter);
  *num_p = num;                 //array_size/sizeof(uint8_t *);
  *evt_ppp = evt_pp;
  return done ? 2 : 0;
}

static uint64_t
epgdb_ts_node_size (void *x NOTUSED, BTreeNode * pn NOTUSED)
{
  return sizeof (EpgTs);
}

static FpSize
epgdb_get_index_size (EpgDb * db)
{
//      return pgmHashSize(&db->root,NULL,epgdb_pgm_node_size);
  return btreeWriteSize (db->root, NULL, epgdb_ts_node_size);
}

static int
epgdb_write_ts_node (void *x, BTreeNode * this, FILE * f)
{
  EpgDb *db = x;
  EpgTs *tp = btreeNodePayload (this);
  epgdb_swap_out_ts (db, tp);
  return (1 != fwrite (tp, sizeof (EpgTs), 1, f));
//      return epgdb_write_pgm(pp,f);
}

static int
epgdb_write_index_real (FpPointer od_root, EpgDb * db)
{
  FP_GO (od_root, db->h.f);
  //return pgmHashWrite(&db->root,NULL,epgdb_write_pgm_node, db->h.f);
  return btreeWrite (db->root, db, epgdb_write_ts_node, db->h.f);
}

static BTreeNode *
epgdb_read_ts_node (void *x NOTUSED, FILE * f)
{
  BTreeNode *p;
  EpgTs *pp;
  p = utlCalloc (1, btreeNodeSize (sizeof (EpgTs)));
  if (!p)
    return NULL;
  pp = btreeNodePayload (p);
  fread (pp, sizeof (EpgTs), 1, f);
  pp->od = true;
  return p;
}

static int
epgdb_read_index (FpPointer od_root, EpgDb * db)
{
  FP_GO (od_root, db->h.f);
//      return pgmHashRead(&db->root,NULL,epgdb_read_pgm_node, db->h.f);
  return btreeRead (&db->root, NULL, epgdb_read_ts_node, db->h.f);
}

///all nodes must be swapped out
static int
epgdb_write_index (EpgDb * db)
{
  FpSize size_total;
  FpPointer od_root;
  FP_GO (0, db->h.f);
  fread (&od_root, 1, sizeof (FpPointer), db->h.f);
  if (od_root)                  //this shouldn't be. perhaps we should delete the file...
  {
    errMsg ("idx root nonzero\n");
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }
  size_total = epgdb_get_index_size (db);
  debugPri (1, "size_total: %llu\n", size_total);
  if (!size_total)
  {
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }
  od_root = fphCalloc (size_total, 1, &db->h);
  if (!od_root)
  {
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }
  debugPri (1, "idx base ptr: %llu\n", od_root);
  if (epgdb_write_index_real (od_root, db))
  {
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }
  debugPri (1, "pos_after: %llu\n", ftello (db->h.f));
  debugPri (1, "real size: %llu\n", ftello (db->h.f) - od_root);
  FP_GO (0, db->h.f);
  fwrite (&od_root, 1, sizeof (FpPointer), db->h.f);
  fclose (db->handle);
  utlFAN (db->vbuf);
  return 0;
}

static void
epgdb_destroy_ts_node (void *x NOTUSED, BTreeNode * this, int which,
                       int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    utlFAN (this);
  }
}

static void
epgdb_destroy_index (EpgDb * db)
{
  //dhashClear(&db->root,epgdb_destroy_pgm_node);
  btreeWalk (db->root, NULL, epgdb_destroy_ts_node);
}

static void
epgdb_delete (EpgDb * db)
{
  unlink (db->dbfilename);
}

/*
static uint32_t epgdb_pgm_hash(void* d)
{
	EpgPgm * ap=d;
	return ap->tsid^ap->pnr^ap->pos;
}
*/

/**
 *\brief creates a new ts record or retrieves existing one
 *
 * must be called with selector locked
 */
EpgTs *
epgdb_get_or_ins_ts (EpgDb * db, uint16_t tsid, uint32_t pos)
{
  EpgTs cv;
  EpgTs *tsp;
  BTreeNode *n;
  memset (&cv, 0, sizeof (EpgTs));
  cv.tsid = tsid;
  cv.pos = pos;
  n = btreeNodeGetOrIns (&db->root, &cv, NULL, cmp_epg_ts, sizeof (EpgTs));
  if (!n)
    return NULL;
  tsp = btreeNodePayload (n);
  epgdb_swap_in_ts_locked (db, tsp);
  return tsp;
}

static EpgPgm *
epgdb_get_or_ins_ts_pgm (EpgTs * tp, uint16_t pnr)
{
  BTreeNode *n;
  EpgPgm cv;
  EpgPgm *pgmp;
  memset (&cv, 0, sizeof (EpgPgm));
  cv.pnr = pnr;
  n = btreeNodeGetOrIns (&tp->root, &cv, NULL, cmp_epg_pgm, sizeof (EpgPgm));
  if (!n)
    return NULL;
  pgmp = btreeNodePayload (n);
  return pgmp;
}

static void
epgdb_put_eis (EpgDb * db, uint8_t * sec)
{
  uint8_t ver, snr, last_nr;
  uint8_t tid;
  uint16_t sec_sz;
  EiTbl *t;
  EpgTs *et;
  EpgPgm *pgm;
  int idx;
//      FpPointer od_sec;
//      uint8_t * ev_start;
//      Iterator iter;
  debugPri (7, "epgdb_put_eis\n");
  if ((sec_sz = get_sec_size (sec)) <= 30)      //probably empty. don't do that expensive lookup thingy at all.
  {
    debugPri (5, "(sec_sz=get_sec_size(sec))<=30\n");
    return;
  }
  if (!sec_is_current (sec))
  {
    debugPri (6, "!sec_is_current(sec)\n");
    return;
  }
  if (secCheckCrc (sec))        //VIVA fails here! why?
  {
    debugPri (3, "secCheckCrc(sec)\n");
    return;
  }


  tid = get_sec_id (sec);
  idx = tid & 0xf;
  ver = get_sec_version (sec);
  snr = get_sec_nr (sec);
  last_nr = get_sec_last (sec);

  if (idx >= 16)
  {
    debugPri (3, "idx>=16\n");
    return;                     //bad
  }
  if (snr > last_nr)
  {
    debugPri (3, "snr>last_nr\n");
    return;                     //bogus
  }

  et = epgdb_get_or_ins_ts (db, get_eis_tsid (sec), db->current_pos);
  if (NULL == et)
    return;

  pgm = epgdb_get_or_ins_ts_pgm (et, get_sec_pnr (sec));
  if (NULL == pgm)
    return;

  t = pgm->sched + idx;
  if (t->secs)                  //valid
  {
    if (t->ver == ver)          //same version
    {
      //check size
      if (t->last_sec < last_nr)
      {
        void *n;
        n = utlRealloc (t->secs, sizeof (uint8_t *) * (last_nr + 1));
        if (!n)
          return;
        t->secs = n;
        //initialize added space
        memset (t->secs + t->last_sec + 1, 0,
                (last_nr - t->last_sec) * sizeof (uint8_t *));
        t->last_sec = last_nr;
      }
      if (t->secs[snr])
      {
        debugPri (6, "t->secs[snr]\n");
        return;                 //already stored
      }
    }
    else                        //different version
    {                           //remove stored sections
      int i;
      debugPri (5, "new ver\n");
//                      selectorUnlock(&db->dvb->ts_dmx.s);//disk accesses
      for (i = 0; i <= t->last_sec; i++)
      {
        if (t->secs[i])
        {
          utlFAN (t->secs[i]);
          t->secs[i] = NULL;
        }
      }
//                      selectorLock(&db->dvb->ts_dmx.s);
      utlFAN (t->secs);
      t->secs = utlCalloc (sizeof (uint8_t *), last_nr + 1);
      if (NULL == t->secs)
        return;
      t->last_sec = last_nr;
      t->ver = ver;
    }
  }
  else
  {                             //invalid, create array
    debugPri (5, "new array\n");
    t->secs = utlCalloc (sizeof (FpPointer), last_nr + 1);
    if (NULL == t->secs)
      return;
    t->last_sec = last_nr;
    t->ver = ver;
  }

//      selectorUnlock(&db->dvb->ts_dmx.s);//disk accesses
  t->tid = tid;
  t->secs[snr] = utlCalloc (sec_sz, 1);
  if (t->secs[snr])
  {
//              FP_GO(t->secs[snr],h->f);
//              fwrite(sec,sec_sz,1,h->f);
    memmove (t->secs[snr], sec, sec_sz);
  }
}

static void
epgdb_reconfig (EpgDb * db, uint32_t num_pos NOTUSED)
{
  debugPri (1, "Switches were reconfigured, purging epgdb\n");

  selectorLock (svtOutSelector (db->o));
  secExFlush (&db->ex);
  selectorUnlock (svtOutSelector (db->o));
  btreeWalk (db->root, db, epgdb_swap_out_node);        //yeah.. well ... could be nicer
  epgdb_destroy_index (db);
  debugPri (1, "clearing File\n");
  fphZero (&db->h);
//      debugPri(1,"initializing Hash map\n");
  db->root = NULL;
/*	if(pgmHashInit(&db->root))
	{
		errMsg("failed to initialize pgm hash\n");
	}*/
}

static void
epgdb_proc_evt (EpgDb * db, SelBuf * d)
{
  uint16_t epg_pid = 0x0012;
  TuneDta *tda;
  uint32_t pos_dta;
  selectorUnlock (svtOutSelector (db->o));
  switch (selectorEvtId (d))
  {
  case E_TUNING:
    debugPri (2, "E_TUNING\n");
    db->tuned = 0;
    dvbDmxOutMod (db->o, 0, NULL, NULL, 0, NULL);
    btreeWalk (db->root, db, epgdb_swap_out_node);
    break;
  case E_IDLE:
    debugPri (2, "E_IDLE\n");
    db->tuned = 0;
    dvbDmxOutMod (db->o, 0, NULL, NULL, 0, NULL);
    btreeWalk (db->root, db, epgdb_swap_out_node);
    break;
  case E_TUNED:                //tuning successful
    debugPri (2, "E_TUNED\n");
    tda = (TuneDta *) selectorEvtDta (d);
    db->tuned = 1;
    db->current_pos = tda->pos;
    dvbDmxOutMod (db->o, 0, NULL, NULL, 1, &epg_pid);
//    selectorModPort (db->input, 0,NULL,NULL,1,&epg_pid);
    selectorLock (svtOutSelector (db->o));
    secExFlush (&db->ex);       //does implicit releaseElementUnsafe
    selectorUnlock (svtOutSelector (db->o));
    break;
  case E_CONFIG:
    debugPri (2, "E_CONFIG\n");
    pos_dta = selectorEvtDta (d)->num_pos;
    epgdb_reconfig (db, pos_dta);
    break;
  default:
    break;
  }
  selectorLock (svtOutSelector (db->o));
}

static void
epgdb_release_se (void *d, void *p)
{
  selectorReleaseElementUnsafe ((Selector *) d, selectorPElement (p));
}

static void *
epgdb_store_loop (void *p)
{
  EpgDb *db = p;
  int rv;
//      int tuned=0;
  nice (db->niceness);
  taskLock (&db->t);
  while (taskRunning (&db->t))
  {
    taskUnlock (&db->t);
    rv = selectorWaitData (db->input, 300);
    taskLock (&db->t);

    if (rv)
    {
      //debugMsg("pthread_cond_timedwait failed\n");
    }
    else
    {                           //there's data
      void *e;
      selectorLock (svtOutSelector (db->o));
      while ((e = selectorReadElementUnsafe (db->input)))
      {
        if (!selectorEIsEvt (e))
        {
          /*
             parsing takes a lot of time. perhaps we can avoid it. 
             Or at least reduce the number of allocations. 
             What we need to do is: find the individual events. 
             Put them into database. Remove them later. if we want minimal effort we could use
             a reference counter directly inside the table. a byte would be sufficient.
             Then we generate pointers to the start of the event fields inside section and save those along with the section.
             When removing events we decrement refctr and remove section if it's zero. To find the refctr we need an offset to the base
             inside each event. 12 bits should suffice.
             We could write the refctr over table_id or service_id. For the offset we could overwrite event_id. start time and
             duration are not possible as they take as much space as our start and duration 32 bit timestamps which are needed and will likely go there.
             Thinking of it.. I guess the time_t type is not defined in terms of bits, so we may run into trouble in future/different LINUX versions.
             better use our own timestamp with an epoch of say.. jan 1st 2010 and 32 bits width
             19 bits needed at least for duration to represent the bcd version adequately

             The timetable is set up like the first try.
             for each hour we use a linked list
             Each element in the list contains a pointer to an event
             pointers from multiple hours may point at the same event if it's long enough

             To find an event(by time), go to the corresponding hour, and read out the list elements. 
             The pointers point to elements at least partially contained in that hour.
             the sections reference counter will be used for keeping track of reference counts.

             removing those time conversion routines didn't help much either. They were slow though.

             Result: still loads cpu with 98%. Will have to keep it in ram.

             Attempt: keep the lowest level on disk, maintain upper levels in ram. small allocations will be size efficient and adequately fast.
             instead of those hashed btrees we use the usual binary tree.

             Done. stil 99%
             we may be able to cache event start and duration inside the list elements, so they don't need to be retrieved when inserting

             after initial low load it goes up to 90% after some time. perhaps it's the allocator

             Yep. fp_grow went through the whole list every time. Fixed. growing works now in constant time. Load somewhere below 10% now. 
             Still going up with increase in db size. There's probably a bug in overlap detection.

             Hmm by storing the start and duration in list elements, the corresponding fields inside events get available for storing offset....

             There was an overflow with the timestamps. Fixed now. At last the times are correct(mostly).

             Changed the whole thing completely. just storing schedule tables directly. They're supposed to be sorted chronologically anyway.
             Not sure if I can rely on this...

             Works quite ok. As expected schedule retrieval takes longer now.
             Load goes up a lot when tuning to transponder with a lot of services(+EPG).
             Perhaps should remove some disk accesses and fread/fwrite when tuning.
             Could also try to detect hi load and dynamically discard a certain 
             amount of sections.

             Ok. we write out tables when we see tuning event
             we load them when we need them.
             could end up with a lot of tables in mem.

             TODO: either allow only one resident pgm or several 
             of them and use a counter.
           */
          SecBuf const *sb;
          uint8_t tid;
          if (db->tuned)
          {
            sb = secExPut (&db->ex, selectorEPayload (e));
            if (sb)
            {
              tid = *((uint8_t *) sb->data);
              debugPri (7, "Table ID: %u\n", tid);
              if (((0x50 <= tid) && (tid < 0x60))       //||
//                                                              ((0x50<=tid)&&(tid<0x70))||
//                                                              (tid==0x4e)||
//                                                              (tid==0x4f)
                )
                epgdb_put_eis (db, (uint8_t *) sb->data);
            }
          }
          else
            selectorReleaseElementUnsafe (svtOutSelector (db->o), e);
        }
        else
        {
          epgdb_proc_evt (db, e);
          selectorReleaseElementUnsafe (svtOutSelector (db->o), e);
        }
//                              selectorReleaseElementUnsafe(&db->dvb->ts_dmx.s,e);
      }
      selectorUnlock (svtOutSelector (db->o));
    }
//              if(db->db_base<(epg_get_buf_time()-3600))//time(NULL)-3600))
//              {
//                      epgdb_hourly(db);
//              }
  }
  taskUnlock (&db->t);
  return NULL;
}

int
epgdbIdle (EpgDb * db)
{
  int rv;
  taskLock (&db->t);
  rv = (0 == db->tuned);
  taskUnlock (&db->t);
  return rv;
}
