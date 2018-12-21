#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/dvb/dmx.h>
#include <inttypes.h>

#include "transponder.h"
#include "utl.h"
#include "table.h"
#include "pgmdb.h"
#include "config.h"
#include "debug.h"
#include "tp_info.h"
#include "pgm_info.h"
#include "bifi.h"
#include "bits.h"
#include "size_stack.h"
#include "iterator.h"
#include "dvb.h"
#include "dmalloc.h"
int DF_PGMDB;
#define FILE_DBG DF_PGMDB
static int pgmdb_set_transp (PgmDb * db, uint32_t pos, uint32_t freq,
                             uint8_t pol);
static int pgmdb_add_transp_real (PgmDb * db, PgmdbPos * pp,
                                  TransponderInfo * i, uint8_t deletable);
static Table *get_pmt (PgmDb * db, uint16_t pnr, uint32_t pos,
                       Transponder * t);
static void clear_db_tbls (PgmDb * db);
static int pgmdb_untune (PgmDb * db);
static void *pgmdb_parse_loop (void *p);
static PgmdbPos *pgmdb_find_pos (PgmDb * db, uint32_t pos);
static Transponder *pgmdb_find_tp (PgmDb * db, PgmdbPos * pp, uint32_t freq,
                                   uint8_t pol);
static void
pgmdb_release_se (void *d, void *p)
{
  selectorReleaseElement ((Selector *) d, selectorPElement (p));
}



static int
pgmdb_compare_pos (void *x NOTUSED, const void *ap, const void *bp)
{
  PgmdbPos const *a = ap, *b = bp;
  return (b->pos < a->pos) ? -1 : (b->pos != a->pos);
}

static PgmdbPos *
pgmdb_get_or_ins_pos (PgmDb * db, uint32_t pos)
{
  BTreeNode *np;
  PgmdbPos cv;
  cv.pos = pos;
  cv.root = NULL;
  np =
    btreeNodeGetOrIns (&db->root, &cv, NULL, pgmdb_compare_pos,
                       sizeof (PgmdbPos));
  if (NULL == np)
    return NULL;
  return btreeNodePayload (np);
}

#ifdef NEW_SCAN_TABLES
#include "itf_new.inc"
#else
#include "itf_old.inc"
#endif

BTreeNode *
pgmdb_read_pos (void *x NOTUSED, FILE * f)
{
  PgmdbPos *t;
  BTreeNode *n;
  n = utlCalloc (1, btreeNodeSize (sizeof (PgmdbPos)));
  if (!n)
    return NULL;

  t = btreeNodePayload (n);
  fread (&t->pos, sizeof (t->pos), 1, f);
  t->root = tp_node_read (f);
  return n;
}

/**
 *\brief read positions from file
 *
 *\return the top Node of tree
 */
static int
pgmdb_pos_node_read (BTreeNode ** dest, FILE * f)
{                               //TODO: handle errors here
  return btreeRead (dest, NULL, pgmdb_read_pos, f);
}

static int
pgmdb_from_scratch (PgmDb * db)
{
  char *tf;
  SwitchPos *dvbpos;
  uint32_t num_pos = 0;
  /*
     here we have to read the positions of the dvb object and get the initial transponders from file
     the dvb object may not have positions yet.
   */
  debugMsg ("getting switch\n");
  dvbpos = dvbGetSwitch (db->dvb, &num_pos);
  if (dvbpos && num_pos)
  {
    uint32_t i;
    debugMsg ("got switch\n");
    for (i = 0; i < num_pos; i++)
    {
      //initialise database(in memory)
      tf = dvbpos[i].initial_tuning_file;
      if (!tf)
      {
        errMsg ("Error: no initial tuning file for pos: %" PRIu32 "\n", i);
        return 1;
      }
      if (pgmdb_parse_initial (tf, db, i))
        return 1;
    }
    return 0;
  }
  errMsg ("couldn't Get Switch\n");
  return 1;
}

int
pgmdbInit (PgmDb * db, RcFile * cfg, dvb_s * a)
{
  Section *s;
  char *v;
  long tmp;
  struct stat buf;
  FILE *dbs;
  memset (db, 0, sizeof (PgmDb));

  debugMsg ("pgmdbInit start\n");
  s = rcfileFindSec (cfg, "PGM");

  db->poll_timeout_ms = 1000;
  if (s && (!rcfileFindSecValInt (s, "poll_timeout_ms", &tmp)))
  {
    db->poll_timeout_ms = tmp;
  }
  db->pat_timeout_s = 5;
  if (s && (!rcfileFindSecValInt (s, "pat_timeout_s", &tmp)))
  {
    db->pat_timeout_s = tmp;
  }
  db->pms_timeout_s = 5;
  if (s && (!rcfileFindSecValInt (s, "pms_timeout_s", &tmp)))
  {
    db->pms_timeout_s = tmp;
  }
  db->freq_tolerance = 1000;
  if (s && (!rcfileFindSecValInt (s, "freq_tol_kHz", &tmp)))
  {
    db->freq_tolerance = tmp / 10;
  }

  db->dbfilename = DEFAULT_PGMDB;
  if (s && (!rcfileFindSecVal (s, "database", &v)))
  {
    db->dbfilename = v;
  }

  db->dvb = a;

  if (stat (db->dbfilename, &buf))
  {
    if (errno == ENOENT)
    {
      debugMsg ("%s nonexistent, reinitialising..\n", db->dbfilename);
      if (pgmdb_from_scratch (db))
      {
        db->root = NULL;
      }
    }
    else
    {
      errMsg ("stat on %s gave: %s. Aborting\n", db->dbfilename,
              strerror (errno));
      return 1;
    }
  }
  else
  {
    dbs = fopen (db->dbfilename, "r");
    if (!dbs)
    {
      errMsg ("fopen on %s gave: %s. Aborting\n", db->dbfilename,
              strerror (errno));
      return 1;
    }
    if (pgmdb_pos_node_read (&db->root, dbs))
    {
      if (pgmdb_from_scratch (db))
        db->root = NULL;
    }
    fclose (dbs);
  }
  if (!cuStackInit (&db->destroy))
    return 1;

  if (CUtaskInit (&db->t, db, pgmdb_parse_loop, &db->destroy))
  {
    errMsg ("task failed to init\n");
    return 1;
  }

  db->tuned = NULL;
  db->pmt_pid = -1;
  pthread_cond_init (&db->pat_complete_cond, NULL);
  db->nit_ver = -1;
  pthread_cond_init (&db->pms_wait_cond, NULL);
  pthread_cond_init (&db->tune_wait_cond, NULL);
  pthread_mutex_init (&db->access_mx, NULL);
  pms_cache_init (&db->pms_cache);
  debugMsg ("getting sec output\n");
  db->o = dvbDmxOutAdd (a);
  db->input = svtOutSelPort (db->o);    //dvbGetTsOutput (a, NULL, 0);
  secExInit (&db->ex, svtOutSelector (db->o), pgmdb_release_se);

  debugMsg ("pgmdbInit complete\n");
  return 0;
}

void
pgmdb_clear_tp (Transponder * t)
{
  if (t->tbl[T_PAT])
  {
    clear_tbl (t->tbl[T_PAT]);
    utlFAN (t->tbl[T_PAT]);
  }
  if (t->tbl[T_SDT])
  {
    clear_tbl (t->tbl[T_SDT]);
    utlFAN (t->tbl[T_SDT]);
  }

  memset (t, 0, sizeof (Transponder));
}


static void
pgmdb_tp_free (void *x NOTUSED, BTreeNode * n, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    Transponder *t = btreeNodePayload (n);
    debugMsg ("clearing tp\n");
    pgmdb_clear_tp (t);
    utlFAN (n);
  }
}


static void
pgmdb_pos_free (void *x NOTUSED, BTreeNode * n, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    PgmdbPos *t = btreeNodePayload (n);
    debugMsg ("clearing pos\n");
    debugMsg ("deallocating transponders\n");
    btreeWalk (t->root, NULL, pgmdb_tp_free);
    utlFAN (n);
  }
}

static int
pgmdb_write_pos (void *x NOTUSED, BTreeNode * v, FILE * f)
{
  PgmdbPos *t = btreeNodePayload (v);

  fwrite (&t->pos, sizeof (t->pos), 1, f);
  if (tp_node_write (t->root, f))
  {
    errMsg ("there was an error\n");
    return 1;
  }
  return 0;
}

static int
pgmdb_pos_node_write (BTreeNode * root, FILE * f)
{
  return btreeWrite (root, NULL, pgmdb_write_pos, f);
}

void
pgmdbClear (PgmDb * db)
{
  FILE *dbs;
  pgmdbStop (db);
  pms_cache_clear (&db->pms_cache);
  pthread_cond_destroy (&db->pat_complete_cond);
  pthread_cond_destroy (&db->pms_wait_cond);
  pthread_cond_destroy (&db->tune_wait_cond);
  pthread_mutex_destroy (&db->access_mx);
  clear_db_tbls (db);
  dbs = fopen (db->dbfilename, "w");
  if (dbs)
  {
    debugPri (1, "Writing positions\n");
    if (pgmdb_pos_node_write (db->root, dbs))
    {
      errMsg ("there was an error\n");
    }
    fclose (dbs);
  }
  else
    errMsg ("fopen on %s gave: %s. Not writing pgmdb\n", db->dbfilename,
            strerror (errno));

  cuStackCleanup (&db->destroy);

  debugMsg ("deallocating transponders\n");
  btreeWalk (db->root, NULL, pgmdb_pos_free);
  db->root = NULL;
  secExClear (&db->ex);
  dvbDmxOutRmv (db->o);
  debugMsg ("pgmdbClear done\n");
}

void
CUpgmdbClear (void *db)
{
  pgmdbClear (db);
}

int
CUpgmdbInit (PgmDb * db, RcFile * cfg, dvb_s * a, CUStack * s)
{
  int rv;
  rv = pgmdbInit (db, cfg, a);
  cuStackFail (rv, db, CUpgmdbClear, s);
  return rv;
}

static int8_t
get_tbl_idx (uint8_t type)
{
  switch (type)
  {
  case 0x00:
    return T_PAT;
  case 0x02:
    return -1;
  case 0x42:
  case 0x46:
    return T_SDT;
  default:
    return -1;
  }
}

static void
dummy_sec_action (void *x NOTUSED, PAS const *old_sec NOTUSED,
                  PAS const *new_sec NOTUSED)
{

}

static void
nis_add_tp (PgmDb * db, NIS * new_nis)
{
  int i;
//      debugMsg("nis_add_tp start\n");
  for (i = 0; i < new_nis->num_tsi; i++)
  {
    TSI *si = &new_nis->ts_infos[i];
    TransponderInfo ti;
    if (dvbIsSat (db->dvb))
    {
      if (si->is_sat)
      {
        //                      debugMsg("clearing tp info\n");
        memset (&ti, 0, sizeof (TransponderInfo));
        ti.frequency = si->frequency_s; //in 10 kHz multiples
        ti.pol = si->pol;
        ti.srate = si->srate;
        ti.fec = si->fec;
        ti.orbi_pos = si->orbi_pos;
        ti.east = si->east;
        ti.msys = si->modsys;
        ti.mtype = si->modtype;
        ti.inv = 2;

        //                      debugMsg("adding transp\n");
        pgmdb_add_transp_real (db, db->current_pos, &ti, 1);
      }
      else
      {
        debugMsg ("not a satellite delivery system\n");
      }
    }
    else if (dvbIsTerr (db->dvb))
    {
      if (si->is_terr)
      {
        memset (&ti, 0, sizeof (TransponderInfo));
        ti.frequency = si->frequency_t; //in 10Hz multiples
        //should we use pol for priority too?
        ti.pol = 0;             //so we can use the same comparison as in dvb-s case...
        ti.bw = si->bw;
        ti.pri = si->pri;
        ti.mp_fec = si->mp_fec;
        ti.constellation = si->constellation;
        ti.hier = si->hier;
        ti.code_rate_h = si->code_rate_h;
        ti.code_rate_l = si->code_rate_l;
        ti.guard = si->guard;
        ti.mode = si->mode;
        ti.inv = 2;
//                              ti.oth=si->oth;
        pgmdb_add_transp_real (db, db->current_pos, &ti, 1);
      }
      else
      {
        debugMsg ("not a terrestrial delivery system\n");
      }
    }
  }
}

static int
put_sec_simple (PgmDb * db, PAS * pas, uint8_t type, void *x,
                void (*sec_action) (void *x, PAS const *old_sec,
                                    PAS const *new_sec))
{
/*
if we are current, put in section after examining version.
if we are not current put in pat_next.
Examine last_section and resize if necessary
*/
  int idx = get_tbl_idx (type);
  if (idx == -1)
    return 1;
  if (pas->c.current)
  {
    Table *t;
    Table *t_next;

    t = db->tbl[idx];
    t_next = db->tbl_next[idx];

    if (!t)                     //no structure here
    {                           //..allocate one
      t = new_tbl (pas->c.last_section + 1, type);
      if (!t)
      {
        return 1;
      }
    }
    if ((pas->c.last_section + 1) != t->num_sections)
    {                           //number of sections mismatch
      if (resize_tbl (t, pas->c.last_section + 1, type))
      {
        return 1;
      }
    }

    if (t->version == -1)
    {                           //new pat
      check_enter_sec (t, pas, type, x, sec_action);
      db->tbl[idx] = t;
    }
    else if (t->version == pas->c.version)
    {                           //copy in new one
      check_enter_sec (t, pas, type, x, sec_action);
    }
    else
    {                           //different version
      //free current one and copy in ..._next if appropriate
      if (t_next && (t_next->version == pas->c.version))
      {                         //stored next sections are up to date. use them

        clear_tbl (db->tbl[idx]);
        utlFAN (db->tbl[idx]);

        db->tbl[idx] = t_next;
        t = t_next;
        db->tbl_next[idx] = NULL;
        t_next = NULL;
        //now copy in new one
        check_enter_sec (t, pas, type, x, sec_action);
      }
      else
      {                         //remove all sections
        int i;
        for (i = 0; i < t->num_sections; i++)
        {
          if (bifiGet (t->occupied, i))
            clear_tbl_sec (t, i, type);
        }
        BIFI_INIT (t->occupied);
        check_enter_sec (t, pas, type, x, sec_action);
      }
    }
  }
  else
  {                             //..._next
    Table *t;
    t = db->tbl_next[idx];

    if (!t)                     //no structure here
    {                           //..allocate one
      t = new_tbl (pas->c.last_section + 1, type);
      if (!t)
        return 1;
    }
    if ((pas->c.last_section + 1) != t->num_sections)
    {                           //number of sections mismatch
      if (resize_tbl (t, pas->c.last_section + 1, type))
        return 1;
    }
    if (t->version == -1)
    {                           //new pat
      check_enter_sec (t, pas, type, x, sec_action);
      db->tbl_next[idx] = t;
    }
    else if (t->version == pas->c.version)
    {                           //copy in new one
      check_enter_sec (t, pas, type, x, sec_action);
    }
    else
    {                           //different version
      {                         //remove all sections
        int i;
        for (i = 0; i < t->num_sections; i++)
        {
          if (bifiGet (t->occupied, i))
            clear_tbl_sec (t, i, type);
        }
        BIFI_INIT (t->occupied);
        check_enter_sec (t, pas, type, x, sec_action);
      }
    }
  }


  if (tbl_is_complete (db->tbl[idx]))
  {
    if ((!db->tuned->tbl[idx])
        || (db->tuned->tbl[idx]->version != db->tbl[idx]->version))
    {
      if (db->tuned->tbl[idx])
      {
        clear_tbl (db->tuned->tbl[idx]);
        utlFAN (db->tuned->tbl[idx]);
      }
      db->tuned->tbl[idx] = db->tbl[idx];
      db->tbl[idx] = NULL;
    }
  }

  return 0;
}

/*
static int nit_is_complete(PgmDb *db)
{
	int i;
	for(i=0;i<db->nit_num_sec;i++)
	{
		if(!bifiGet(db->nit_seen_sec,i))
		{
			return 0;
		}
	}
	return 1;
}
*/
static void
pgmdb_reconfig (PgmDb * db, uint32_t num_pos)
{
  SwitchPos *dvbpos;
  char *tf;
  pgmdb_untune (db);
  debugMsg ("deallocating positions\n");
  btreeWalk (db->root, NULL, pgmdb_pos_free);
  db->root = NULL;
  debugMsg ("clearing pms cache\n");
  pms_cache_clear (&db->pms_cache);
  pms_cache_init (&db->pms_cache);
  dvbpos = dvbGetSwitch (db->dvb, &num_pos);
  if (dvbpos)
  {
    uint32_t i;
    for (i = 0; i < num_pos; i++)
    {
      //initialise database(in memory)
      tf = dvbpos[i].initial_tuning_file;
      if (NULL == tf)
      {
        errMsg ("Warning: no initial tuning file for pos: %" PRIu32 "\n", i);
      }
      else
      {
        if (pgmdb_parse_initial (tf, db, i))
          return;
      }
    }
  }
}


static void
pgmdb_proc_evt (PgmDb * db, void *d)
{
//      PnrDta *pnr_dta;
  uint32_t pos_dta;
  TuneDta *tda;
  switch (selectorEvtId (d))
  {
  case E_TUNING:
    debugMsg ("pgmdb E_TUNING\n");
    pgmdb_untune (db);
    break;
  case E_IDLE:
    debugMsg ("pgmdb E_IDLE\n");
    pgmdb_untune (db);
    break;
  case E_TUNED:                //setup transponder
    debugMsg ("pgmdb E_TUNED\n");
//              secExFlush(&db->ex);
    tda = (TuneDta *) selectorEvtDta (d);
    pgmdb_set_transp (db, tda->pos, tda->freq, tda->pol);

    if (db->tune_wait && (db->tune_wait_pos == tda->pos)
        && (db->tune_wait_freq == tda->freq)
        && (db->tune_wait_pol == tda->pol))
    {
      db->tune_wait = 0;
      pthread_cond_broadcast (&db->tune_wait_cond);
    }
    db->nit_ver = -1;
    break;
  case E_CONFIG:
    debugMsg ("pgmdb E_CONFIG\n");
    pos_dta = selectorEvtDta (d)->num_pos;
    pgmdb_reconfig (db, pos_dta);
    break;
  case E_PNR_ADD:
    debugMsg ("pgmdb E_PNR_ADD\n");
//              pnr_dta=(PnrDta *)selectorEvtDta(d);
    break;
  case E_PNR_RMV:
    debugMsg ("pgmdb E_PNR_RMV\n");
//              pnr_dta=(PnrDta *)selectorEvtDta(d);
    break;
  default:
    break;
  }
}

static void
pgmdb_proc_sec (PgmDb * db, SecBuf * sec)
{
  PAS tpas;
  SDS tsds;
  NIS tnis;
  int rv;
//      debugMsg("Processing section\n\tSecPid: %hhu\n",sec->pid);
  switch (sec->pid)
  {
  case 0:
    if (sec->data[0] == 0x00)
    {
//                      debugMsg("got PAS\n");
      rv = parse_pas (sec->data, &tpas);
      if (!rv)
      {
        if (put_sec_simple (db, &tpas, sec->data[0], NULL, dummy_sec_action))
        {
          clear_pas (&tpas);
        }
        if ((!db->pat_complete)
            && (db->tbl[T_PAT] && tbl_is_complete (db->tbl[T_PAT])))
        {
          debugMsg ("PAT complete\n");
          db->pat_complete = 1;
          pthread_cond_broadcast (&db->pat_complete_cond);
        }
      }
    }
    break;
  case 0x10:
    if (sec->data[0] == 0x40)
      if (sec_is_current (sec->data) && ((db->nit_ver !=
                                          get_sec_version (sec->data))
                                         ||
                                         (!bifiGet
                                          (db->nit_seen_sec,
                                           get_sec_nr (sec->data)))))
      {
        rv = parse_nis (sec->data, &tnis);
        if (!rv)
        {
          if (tnis.c.version != db->nit_ver)
          {
            db->nit_ver = tnis.c.version;
            BIFI_INIT (db->nit_seen_sec);
          }
          db->nit_num_sec = tnis.c.last_section + 1;
          bifiSet (db->nit_seen_sec, tnis.c.section);
          nis_add_tp (db, &tnis);
          clear_nis (&tnis);
/*					if((!db->nit_complete)&&nit_is_complete(db))
					{
						debugMsg("nit is complete\n");
						db->nit_complete=1;
						pthread_cond_broadcast(&db->nit_complete_cond);
					}
*/ }
      }
    break;
  case 0x11:
//              debugMsg("SDS? BYTES: \n0x%hhx0x%hhx0x%hhx0x%hhx0x%hhx0x%hhx\n",sec->data[0],sec->data[1],sec->data[2],sec->data[3],sec->data[4],sec->data[5]);
    if (sec->data[0] == 0x42)
    {
      rv = parse_sds (sec->data, &tsds);
      if (!rv)
      {
        if (put_sec_simple
            (db, (PAS *) & tsds, sec->data[0], NULL, dummy_sec_action))
        {
          clear_sds (&tsds);
        }
/*				if((!db->sdt_complete)&&(db->tbl[T_SDT]&&tbl_is_complete(db->tbl[T_SDT])))
				{
					db->sdt_complete=1;
					pthread_cond_broadcast(&db->sdt_complete_cond);
				}*/
      }
    }
    break;
  default:
    if ((sec->pid == db->pmt_pid) && (sec->data[0] == 0x02)
        && sec_is_current (sec->data) && (!secCheckCrc (sec->data)))
    {
      put_cache_sec (sec->data, db->current_pos->pos,
                     db->tuned->frequency, db->tuned->pol, &db->pms_cache);
      if ((db->pms_wait) && (db->pms_wait_pnr == get_sec_pnr (sec->data))
          && (db->pms_wait_secnr == get_sec_nr (sec->data)))
      {
        memmove (db->pms_sec, sec->data, get_sec_size (sec->data));
//                              db->pms_sec=p;
        db->pms_wait = 0;
        pthread_cond_broadcast (&db->pms_wait_cond);
      }
    }
    break;
  }
}

/**
 *\brief updates the database
 *
 *
 * Allocate filters for pids 0(PAT),0x10(NIT),0x11(SDT)
 * PMTs run on multiple pids. use a single filter and switch periodically,or change when tuning
 */
void *
pgmdb_parse_loop (void *p)
{
  PgmDb *db = p;
//      time_t current,last;
  taskLock (&db->t);
//      current=last=time(NULL);

  while (taskRunning (&db->t))
  {
    int rv;
    taskUnlock (&db->t);
    rv = selectorWaitData (db->input, db->poll_timeout_ms);
    taskLock (&db->t);
    if (rv)
    {
//                              debugMsg("secExWaitData failed\n");
    }
    else
    {
      void *d;

      while ((d = selectorReadElement (db->input)))
      {
        if (selectorEIsEvt (d))
        {
          pgmdb_proc_evt (db, d);
          selectorReleaseElement (&db->dvb->ts_dmx.s, d);
        }
        else
        {
          if (db->tuned)
          {
            SecBuf const *sb;
            sb = secExPut (&db->ex, selectorEPayload (d));
            if (sb)
              pgmdb_proc_sec (db, (SecBuf *) sb);
          }
          else
          {
            selectorReleaseElement (&db->dvb->ts_dmx.s, d);
          }
        }
      }
    }
  }
  taskUnlock (&db->t);
  return NULL;
}

int
pgmdbStart (PgmDb * db)
{
  return taskStart (&db->t);
//      return 0;
}

int
pgmdbStop (PgmDb * db)
{
  return taskStop (&db->t);
//      return 0;
}

int
compare_transponders (void *user, void const *a, void const *b)
{
  PgmDb *db = user;
  Transponder *ta = (Transponder *) a;
  Transponder *tb = (Transponder *) b;
  int rv;
//      debugMsg("comparing transponders\n");

//      rv=ta->pos-tb->pos;

  if (abs (ta->frequency - tb->frequency) < db->freq_tolerance)
    rv = 0;
  else
    rv = (ta->frequency < tb->frequency) ? -1 : 1;

  if (!rv)
    rv = (ta->pol < tb->pol) ? -1 : (ta->pol != tb->pol);

  return rv;
}

int
pgmdbAddTransp (PgmDb * db, uint32_t pos, TransponderInfo * i)
{
  int rv = 1;
  BTreeNode *n;
  PgmdbPos *pp;
  PgmdbPos cv = {.pos = pos,.root = NULL };
  pthread_mutex_lock (&db->access_mx);
  taskLock (&db->t);
  if ((n = btreeNodeFind (db->root, &cv, db, pgmdb_compare_pos)))
  {
    pp = btreeNodePayload (n);
    rv = pgmdb_add_transp_real (db, pp, i, 1);
  }
  taskUnlock (&db->t);
  pthread_mutex_unlock (&db->access_mx);
  return rv;
}

int
pgmdbDelTransp (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol)
{
  PgmdbPos *pp;
  Transponder *tp;
  BTreeNode *n;
  int rv = 1;
  pthread_mutex_lock (&db->access_mx);
  taskLock (&db->t);
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    tp = pgmdb_find_tp (db, pp, freq, pol);
    if ((tp) && (tp != db->tuned) && (tp->deletable))
    {
      n = btreePayloadNode (tp);
      rv = btreeNodeRemove (&pp->root, n);
      taskUnlock (&db->t);
      pthread_mutex_unlock (&db->access_mx);
      return rv;
    }
  }
  taskUnlock (&db->t);
  pthread_mutex_unlock (&db->access_mx);
  return rv;
}

static int
pgmdb_add_transp_real (PgmDb * db, PgmdbPos * pp, TransponderInfo * i,
                       uint8_t deletable)
{
  BTreeNode *n;
  Transponder *t;
  Transponder tmp;
  size_t s = btreeNodeSize (sizeof (Transponder));
  if (!pp)
    return 1;

  memmove (&tmp, i, sizeof (TransponderInfo));
  tmp.deletable = deletable;
  tmp.scanned = 0;
  tmp.tbl[0] = NULL;
  tmp.tbl[1] = NULL;
//      debugMsg("searching for existing node\n");
  if (btreeNodeFind (pp->root, &tmp, db, compare_transponders))
  {
//              debugMsg("tp already exists at this freq/pol\n");
    return 1;
  }

//      debugMsg("allocating transponder, size %u\n",s);
  n = utlCalloc (1, s);
  if (!n)
    return 1;
  t = btreeNodePayload (n);
//      debugMsg("initializing transponder: %p\n",t);
  memmove (t, &tmp, sizeof (Transponder));
  debugMsg ("storing tp\n");
  if (btreeNodeInsert (&pp->root, n, db, compare_transponders))
  {
    utlFAN (n);
    return 2;
  }
  return 0;
}

static void
clear_db_tbls (PgmDb * db)
{
  if (db->tbl[T_PAT])
  {
    clear_tbl (db->tbl[T_PAT]);
    utlFAN (db->tbl[T_PAT]);
    db->tbl[T_PAT] = NULL;
  }
  if (db->tbl[T_SDT])
  {
    clear_tbl (db->tbl[T_SDT]);
    utlFAN (db->tbl[T_SDT]);
    db->tbl[T_SDT] = NULL;
  }
  if (db->tbl_next[T_PAT])
  {
    clear_tbl (db->tbl_next[T_PAT]);
    utlFAN (db->tbl_next[T_PAT]);
    db->tbl_next[T_PAT] = NULL;
  }
  if (db->tbl_next[T_SDT])
  {
    clear_tbl (db->tbl_next[T_SDT]);
    utlFAN (db->tbl_next[T_SDT]);
    db->tbl_next[T_SDT] = NULL;
  }
}

static int
pgmdb_untune (PgmDb * db)
{
  debugMsg ("untune\n");
//      selectorModPort(&db->dvb->ts_dmx.s,db->input,NULL,0);
  dvbDmxOutMod (db->o, 0, NULL, NULL, 0, NULL);
  secExFlush (&db->ex);
  db->tuned = NULL;
  db->current_pos = NULL;
  db->pmt_pid = -1;
  db->pat_complete = 0;
//      db->sdt_complete=0;
//      db->nit_complete=0;
  db->pms_wait = 0;
  clear_db_tbls (db);
  return 0;
}

static PgmdbPos *
pgmdb_find_pos (PgmDb * db, uint32_t pos)
{
  BTreeNode *n;
  PgmdbPos cv;
  cv.pos = pos;
  n = btreeNodeFind (db->root, &cv, NULL, pgmdb_compare_pos);
  if (NULL == n)
    return NULL;
  return btreeNodePayload (n);
}

static Transponder *
pgmdb_find_tp (PgmDb * db, PgmdbPos * pp, uint32_t freq, uint8_t pol)
{
  BTreeNode *n;
  Transponder cv;
  cv.frequency = freq;
  cv.pol = pol;
  n = btreeNodeFind (pp->root, &cv, db, compare_transponders);
  if (NULL == n)
    return NULL;
  return btreeNodePayload (n);
}

static int
pgmdb_set_transp (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol)
{
  Transponder *tp;
  PgmdbPos *pp;
  uint16_t pids[3] = { 0x00, 0x10, 0x11 };
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    tp = pgmdb_find_tp (db, pp, freq, pol);
    if (tp)
    {
      db->current_pos = pp;
      db->tuned = tp;
      db->pmt_pid = -1;
      db->pat_complete = 0;
//                      db->sdt_complete=0;
//                      db->nit_complete=0;
      db->nit_ver = -1;
      db->pms_wait = 0;

      clear_db_tbls (db);
      dvbDmxOutMod (db->o, 0, NULL, NULL, 3, pids);
//      selectorModPort (db->input, 0,NULL,NULL,3,pids);
      secExFlush (&db->ex);
      debugMsg ("filters set, starting parse loop\n");
      return 0;
    }
  }
  errMsg ("Transponder %" PRIu32 ", %" PRIu8 " not found\n", freq, pol);
  return 1;
}


void
node_count (void *x, BTreeNode * n NOTUSED, int which, int depth NOTUSED)
{
  if ((which == 0) || (which == 3))
  {
    uint32_t *i = x;
    *i = *i + 1;
  }
}

void
gen_tp_info (TransponderInfo * i, Transponder * t)
{

  memmove (i, t, sizeof (TransponderInfo));     //ugly
  i->num_found = 0;
  i->found = NULL;
}

/**
 *this should probably be bounds checked
 */
void
tp_list (void *x, BTreeNode * n, int which, int depth NOTUSED)
{
  if ((which == 1) || (which == 3))
  {
    TransponderInfo **ip = x;
    TransponderInfo *i = *ip;
    Transponder *t = btreeNodePayload (n);
    gen_tp_info (i, t);
    (*ip) = (*ip) + 1;
  }
}

int
pgmdbFindTransp (PgmDb * db, TransponderInfo * i, uint32_t pos, int32_t freq,
                 uint8_t pol)
{
  Transponder *tp;
  PgmdbPos *pp;
//      pthread_mutex_lock(&db->access_mx);
  taskLock (&db->t);
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    debugMsg ("found pos%" PRIu32 "\n", pos);
    tp = pgmdb_find_tp (db, pp, freq, pol);
    if (tp)
    {
      debugMsg ("found tp %p\n", tp);
      gen_tp_info (i, tp);
      taskUnlock (&db->t);
//                      pthread_mutex_unlock(&db->access_mx);
      return 0;
    }
  }
  debugMsg ("pgmdbFindTransp error\n");
  taskUnlock (&db->t);
//      pthread_mutex_unlock(&db->access_mx);
  return 1;
}

TransponderInfo *
pgmdbListTransp (PgmDb * db, uint32_t pos, int *num_tp)
{
  uint32_t num = 0;
  PgmdbPos *pp;
  TransponderInfo *t, *p;
//      pthread_mutex_lock(&db->access_mx);
  taskLock (&db->t);

  pp = pgmdb_find_pos (db, pos);
  if (NULL != pp)
  {
    btreeWalk (pp->root, &num, node_count);
    if (0 == num)
    {
      errMsg ("no transponders found\n");
      taskUnlock (&db->t);
//                      pthread_mutex_unlock(&db->access_mx);
      return NULL;
    }
    t = p = utlCalloc (num, sizeof (TransponderInfo));
    btreeWalk (pp->root, &p, tp_list);
    taskUnlock (&db->t);
//              pthread_mutex_unlock(&db->access_mx);
    *num_tp = num;
    return t;
  }
  else
  {
    errMsg ("position %" PRIu32 " not found\n", pos);
  }
  taskUnlock (&db->t);
//      pthread_mutex_unlock(&db->access_mx);
  return NULL;
}

void
count_programs (void *x, PAS * p)
{
  int *kp = x;
  int i;
  for (i = 0; i < p->pa_count; i++)
  {
    if (p->pas[i].pnr != 0)
      (*kp)++;
  }
}

int
get_pa_count (Table * pat)
{
  int k;
  k = 0;
  for_each_pas (pat, &k, count_programs);
  return k;
}

typedef struct
{
  PgmDb *db;
  Transponder *t;
  ProgramInfo *p;
  int idx;
} pgm_list_s;

int collect_pgm_info (Transponder * t, ProgramInfo * p, uint16_t pnr);

void
extract_pgm (void *x, PAS * p)
{
  pgm_list_s *l = x;
  int i;
  for (i = 0; i < p->pa_count; i++)
  {
    if (p->pas[i].pnr != 0)
    {
      debugMsg ("collecting pgm info for %hu\n", p->pas[i].pnr);
      collect_pgm_info (l->t, &l->p[l->idx], p->pas[i].pnr);
      l->p[l->idx].pnr = p->pas[i].pnr;
      l->idx++;
    }
  }
}

ProgramInfo *
pgmdbListPgm (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol,
              int *num_ret)
{
  Transponder *t;
//      Transponder cv;
  ProgramInfo *p;
  pgm_list_s l;
  int num_pgm;
  PgmdbPos *pp;
//      cv.frequency=freq;
//      cv.pol=pol;
//      pthread_mutex_lock(&db->access_mx);
  taskLock (&db->t);

  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    debugMsg ("looking for node\n");
    t = pgmdb_find_tp (db, pp, freq, pol);
    if (t)
    {
      if (t->tbl[T_PAT])
      {
        num_pgm = get_pa_count (t->tbl[T_PAT]); //go through program associations and sort out pnr==0 (NIT)
        if (!num_pgm)
        {
          debugMsg ("no programs found\n");
          taskUnlock (&db->t);
          *num_ret = 0;
          //      pthread_mutex_unlock(&db->access_mx);
          return NULL;
        }
        p = utlCalloc (num_pgm, sizeof (ProgramInfo));
        if (!p)
        {
          debugMsg ("allocation error\n");
          taskUnlock (&db->t);
          //              pthread_mutex_unlock(&db->access_mx);
          *num_ret = 0;
          return NULL;
        }

        l.t = t;
        l.db = db;
        l.p = p;
        l.idx = 0;
        for_each_pas (t->tbl[T_PAT], &l, extract_pgm);
        *num_ret = num_pgm;
        taskUnlock (&db->t);
        //              pthread_mutex_unlock(&db->access_mx);
        return p;
      }
      else
      {
        debugMsg ("no PAT, no programs\n");
        taskUnlock (&db->t);
        //                      pthread_mutex_unlock(&db->access_mx);
        *num_ret = 0;
        return NULL;
      }
    }
  }
  debugMsg ("node not found\n");
  taskUnlock (&db->t);
//      pthread_mutex_unlock(&db->access_mx);
  *num_ret = 0;
  return NULL;
}

typedef struct
{
  int num;
  uint16_t pnr;
} count_str;

void
count_pnr (void *x, PAS * sec)
{
  count_str *c = x;
  int i;
  for (i = 0; i < sec->pa_count; i++)
  {
    if (sec->pas[i].pnr == c->pnr)
      c->num++;
  }
}

typedef struct
{
  ProgramInfo *p;
  int size;
  int ctr;
  uint16_t pnr;
} pid_get_str;

void
get_pid (void *x, PAS * sec)
{
  pid_get_str *pgs = x;
  int i;
  for (i = 0; i < sec->pa_count; i++)
  {
    if (sec->pas[i].pnr == pgs->pnr)
    {
      pgs->p[pgs->ctr].pid = sec->pas[i].pid;
      pgs->ctr++;
    }
  }
}

/*
void get_pgmi_pmt(void * x,PMS * sec)
{
	pid_get_str * pgs=x;
	if(sec->program_number==pgs->pnr)
	{
		pgs->p[pgs->ctr].pcr_pid=sec->pcr_pid;
		pgs->p[pgs->ctr].num_es=sec->num_es;
		pgs->ctr++;
	}
}
*/
void
get_pgmi_sdt (void *x, SDS * sec)
{
  pid_get_str *pgs = x;
  int i;
  for (i = 0; i < sec->num_svi; i++)
  {
    if (sec->service_infos[i].svc_id == pgs->pnr)
    {
      SVI *s = &sec->service_infos[i];
      ProgramInfo *p = &pgs->p[pgs->ctr];
      debugMsg ("found sdt with matching pnr\n");
      p->svc_type = s->svc_type;
      p->schedule = s->schedule;
      p->present_follow = s->present_follow;
      p->status = s->status;
      p->ca_mode = s->ca_mode;
      p->tsid = sec->tsid;
      p->onid = sec->onid;
      dvbStringCopy (&p->provider_name, &s->provider_name);
      dvbStringCopy (&p->service_name, &s->service_name);
      dvbStringCopy (&p->bouquet_name, &s->bouquet_name);
      pgs->ctr++;
    }
  }
}



int
collect_pgm_info (Transponder * t, ProgramInfo * p, uint16_t pnr)
{
  count_str c;
  pid_get_str pgs;
//      Table cv2;
//      Table * pmt;
  //BTreeNode * n;


  c.pnr = pnr;
  c.num = 0;
  if (!t->tbl[T_PAT])
    return 1;

  debugMsg ("there's a PAT\n");

  debugMsg ("counting programs with pnr=%" PRIu16 "\n", pnr);
  for_each_pas (t->tbl[T_PAT], &c, count_pnr);
  if (!c.num)
    return 1;

  if (c.num != 1)
  {                             //anything else but one probably isn't sane here
    return 1;
  }

  debugMsg ("Ok found 1 ocurrence\n");
  pgs.p = p;
  pgs.size = c.num;
  pgs.ctr = 0;
  pgs.pnr = pnr;
  debugMsg ("getting pid\n");
  for_each_pas (t->tbl[T_PAT], &pgs, get_pid);

//      debugMsg("looking for pmt by pnr\n");
//      cv2.pnr=pnr;
  pgs.ctr = 0;
  if (t->tbl[T_SDT])
  {
    debugMsg ("retrieving information from sdt\n");
    for_each_sds (t->tbl[T_SDT], &pgs, get_pgmi_sdt);
  }
  return 0;
}

ProgramInfo *
pgmdbFindPgmPnr (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol,
                 uint16_t pnr, int *num_res)
{
        /**
	 first, get pmtpid from pat
	 then, fill in pmt info, if available
	 afterwards, look up service id in sdt and get the info
	 
	 first, just count, allocate, then really collect data
	 gee.. I wonder if it's possible to get the same pnr for several programs?
	 */

  ProgramInfo *p;
  PgmdbPos *pp;
  Transponder *t;

//      pthread_mutex_lock(&db->access_mx);
  taskLock (&db->t);
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    debugMsg ("looking for node\n");
    t = pgmdb_find_tp (db, pp, freq, pol);
    if (t)
    {
      p = utlCalloc (1, sizeof (ProgramInfo));
      if (NULL != p)
      {
        if (!collect_pgm_info (t, p, pnr))
        {
          *num_res = 1;
          taskUnlock (&db->t);
          //                              pthread_mutex_unlock(&db->access_mx);
          return p;
        }
        else
        {
          programInfoClear (p);
          utlFAN (p);
        }
      }
    }
  }

  taskUnlock (&db->t);
//      pthread_mutex_unlock(&db->access_mx);
  return NULL;
}

typedef struct
{
  Transponder *t;
  TransponderInfo *p;
  ProgramInfo *p2;
  PgmDb *db;
  int size;
  int ctr;
  int num_pgm;
  int pgm_ctr;
  uint8_t how;
  char *name;
} find_pgm_str;
/*
void count_svc_name_sdt(void * x,SDS * sec)
{
	find_pgm_str * f=x;
	int (*cmp)(const char * s1,const char * s2,size_t n);
	int i;
	if(f->how)
		cmp=strncasecmp;
	else
		cmp=strncmp;
	
	for(i=0;i<sec->num_svi;i++)
	{
		if(sec->service_infos[i].service_name)
		{
			if(!cmp(sec->service_infos[i].service_name,f->name,256))
			{
				f->size++;
			}
		}
	}
}

void is_svc_name_sdt(void * x,SDS * sec)
{
	find_pgm_str * f=x;
	int (*cmp)(const char * s1,const char * s2,size_t n);
	int i;
	if(f->how)
		cmp=strncasecmp;
	else
		cmp=strncmp;
	
	for(i=0;i<sec->num_svi;i++)
	{
		if(sec->service_infos[i].service_name)
		{
			if(!cmp(sec->service_infos[i].service_name,f->name,256))
			{
				f->ctr++;
			}
		}
	}
}

void count_tp_name(void * x,BTreeNode * this,int which,int depth)
{
	if((which==2)||(which==3))
	{
		find_pgm_str * f=x;
		Transponder * t=btreeNodePayload(this);
		if(t->tbl[T_SDT])
		{
			f->ctr=0;
			for_each_sds(t->tbl[T_SDT],f,is_svc_name_sdt);
			if(f->ctr)
				f->size++;
		}
	}
}
*/
/*
void get_svc_name_sdt(void * x,SDS * sec)
{
	find_pgm_str * f=x;
	int (*cmp)(const char * s1,const char * s2,size_t n);
	int i;
	if(f->how)
		cmp=strncasecmp;
	else
		cmp=strncmp;
	
	for(i=0;i<sec->num_svi;i++)
	{
		if(sec->service_infos[i].service_name)
		{
			if(!cmp(sec->service_infos[i].service_name,f->name,256))
			{
				f->p2[f->pgm_ctr].pnr=sec->service_infos[i].svc_id;
				collect_pgm_info(f->db, f->t, &f->p2[f->pgm_ctr], f->p2[f->pgm_ctr].pnr);
				f->pgm_ctr++;
			}
		}
	}
}
*/
/*
void get_pgm_name(void * x,BTreeNode * this,int which,int depth)
{
	if((which==2)||(which==3))
	{
		find_pgm_str * f=x;
		Transponder * t=btreeNodePayload(this);
		TransponderInfo * i=&f->p[f->ctr];
		if(t->tbl[T_SDT])
		{
			f->num_pgm=0;
			for_each_sds(t->tbl[T_SDT],f,count_svc_name_sdt);
			if(f->num_pgm)
			{
				i->num_found=f->num_pgm;
				i->found=utlCalloc(i->num_found,sizeof(ProgramInfo));
				if(i->found)
				{
					f->p2=i->found;
					f->pgm_ctr=0;
					f->t=t;
					for_each_sds(t->tbl[T_SDT],f,get_svc_name_sdt);
				}
				f->ctr++;
			}
		}
	}
}
*/
/*
TransponderInfo * pgmdbFindPgmNamed(PgmDb * db,uint32_t pos,char * name,uint8_t how,int * num_res)
{
	
	 search the sdt on every(!) transponder for service name, return corresponding transponder and programinfos
	 
	find_pgm_str f;
	taskLock(&db->t);
	f.how=how;
	f.name=name;
	f.size=0;
	f.db=db;
	btreeWalk(db->root,&f,count_tp_name);
	if(!f.size)
	{
		taskUnlock(&db->t);
		return NULL;
	}
	f.p=utlCalloc(f.size,sizeof(TransponderInfo));
	if(!f.p)
	{
		taskUnlock(&db->t);
		return NULL;
	}
	f.ctr=0;
	btreeWalk(db->root,&f,get_pgm_name);
	*num_res=f.size;
	taskUnlock(&db->t);
	return f.p;
}
*/
int
pgmdb_set_pmt_pid (PgmDb * db, uint16_t pid)
{
  uint16_t pids[4] = { 0x00, 0x10, 0x11, pid };
  dvbDmxOutMod (db->o, 0, NULL, NULL, 4, pids);
//  selectorModPort (db->input, 0,NULL,NULL,4,pids);
  db->pmt_pid = pid;
  db->pms_wait = 0;
  return 0;
}


/*
int pgmdbSetPgm(PgmDb * db,uint16_t pnr)
{
	ProgramInfo p;
	pid_get_str pgs;
	pthread_mutex_lock(&db->access_mx);
	taskLock(&db->t);
	if((!db->current_pos)||(!db->tuned)||(!db->tuned->tbl[T_PAT]))
	{
		taskUnlock(&db->t);
		pthread_mutex_unlock(&db->access_mx);
		return 1;
	}
	//get the pid

	pgs.p=&p;
	pgs.size=1;
	pgs.ctr=0;
	pgs.pnr=pnr;
	for_each_pas(db->tuned->tbl[T_PAT],&pgs,get_pid);
	
	if(pgs.ctr!=1)
	{
		errMsg("error getting pid\n");
		taskUnlock(&db->t);
		pthread_mutex_unlock(&db->access_mx);
		return 1;
	}
	
	debugMsg("setting pid\n");
	pgmdb_set_pmt_pid(db,p.pid);
	
	taskUnlock(&db->t);
	pthread_mutex_unlock(&db->access_mx);
	return 0;
}
*/

static int
pgmdb_set_pgm (PgmDb * db, uint16_t pnr)
{
  ProgramInfo p;
  pid_get_str pgs;
  if ((!db->current_pos) || (!db->tuned) || (!db->tuned->tbl[T_PAT]))
  {
    errMsg ("pgmdb_set_pgm: Not tuned\n");
    return 1;
  }
  //get the pid

  pgs.p = &p;
  pgs.size = 1;
  pgs.ctr = 0;
  pgs.pnr = pnr;
  for_each_pas (db->tuned->tbl[T_PAT], &pgs, get_pid);

  if (pgs.ctr != 1)
  {
    errMsg ("error getting pid\n");
    return 1;
  }

  debugMsg ("setting pid\n");
  pgmdb_set_pmt_pid (db, p.pid);

  return 0;
}

void
rst_scanned (void *x NOTUSED, BTreeNode * this, int which, int depth NOTUSED)
{
  Transponder *t = btreeNodePayload (this);
  if ((which == 1) || (which == 3))
  {
    t->scanned = 0;
  }
}

void
reset_tp_scanned (PgmDb * db)
{
  btreeWalk (db->root, NULL, rst_scanned);

}

void
wait_for_pat (PgmDb * db)
{
  int rv = 0;
  struct timeval current;
  struct timespec until;
  gettimeofday (&current, NULL);
  until.tv_sec = current.tv_sec + db->pat_timeout_s;
  until.tv_nsec = (current.tv_usec) * 1000;
  taskLock (&db->t);
  while ((!db->pat_complete) && (!rv))
  {
    rv = pthread_cond_timedwait (&db->pat_complete_cond, &db->t.mx, &until);
  }
  taskUnlock (&db->t);
  if (rv)
    errMsg ("pthread_cond_timedwait: %s\n", strerror (rv));
}

/*
void wait_for_sdt(PgmDb *db)
{
	int rv=0;
	struct timeval current;
	struct timespec until;
	gettimeofday(&current,NULL);
	until.tv_sec=current.tv_sec+db->sdt_timeout_s;
	until.tv_nsec=(current.tv_usec)*1000;
	taskLock(&db->t);
	while((!db->sdt_complete)&&(!rv))
	{
		rv=pthread_cond_timedwait(&db->sdt_complete_cond,&db->t.mx,&until);
	}
	taskUnlock(&db->t);
	if(rv)
		errMsg("pthread_cond_timedwait: %s\n",strerror(rv));
}

void wait_for_nit(PgmDb *db)
{
	int rv=0;
	struct timeval current;
	struct timespec until;
	gettimeofday(&current,NULL);
	until.tv_sec=current.tv_sec+db->nit_timeout_s;
	until.tv_nsec=(current.tv_usec)*1000;
	taskLock(&db->t);
	while((!db->nit_complete)&&(!rv))
	{
		rv=pthread_cond_timedwait(&db->nit_complete_cond,&db->t.mx,&until);
	}
	taskUnlock(&db->t);
	if(rv)
		errMsg("pthread_cond_timedwait: %s\n",strerror(rv));
}
*/
void
skim_pmt (PgmDb * db, Transponder * t)
{
  ProgramInfo *p;
  pgm_list_s l;
  int num_pgm;
  int i;
  if (!t->tbl[T_PAT])
  {
    debugMsg ("No PAT\n");
    return;
  }

  debugMsg ("there's a PAT\n");
  taskLock (&db->t);

  num_pgm = get_pa_count (t->tbl[T_PAT]);       //go through program associations and sort out pnr==0 (NIT)
  debugMsg ("got %u programs\n", num_pgm);
  if (!num_pgm)
  {
    taskUnlock (&db->t);
    return;
  }
  p = utlCalloc (num_pgm, sizeof (ProgramInfo));
  if (!p)
  {
    taskUnlock (&db->t);
    return;
  }

  l.t = t;
  l.db = db;
  l.p = p;
  l.idx = 0;
  debugMsg ("extracting programs\n");
  for_each_pas (t->tbl[T_PAT], &l, extract_pgm);

  for (i = 0; i < num_pgm; i++)
  {
    debugMsg ("setting pid %u\n", p[i].pid);
    pgmdb_set_pmt_pid (db, p[i].pid);
    taskUnlock (&db->t);
//              debugMsg("waiting\n");
//              wait_for_pmt(db);
    taskLock (&db->t);
  }

  debugMsg ("freeing list\n");
  for (i = 0; i < num_pgm; i++)
  {
    programInfoClear (&p[i]);
  }
  utlFAN (p);
  taskUnlock (&db->t);
  return;
}

/*
int pgmdbScanFull(PgmDb *db,void * x,int(*tune)(void * x,TransponderInfo * i))
{
	Transponder * t;
	Iterator it;
	int num_tp,i,found=0;
step0: reset transponders scanned state
step1: generate a list of all transponders
step2: tune transponder
step2.1: wait a little. if pat shows programs, skim through pmt pids to get pmts
step2.2: mark transponder
step3: repeat for remaining transponders
step4: get list of unscanned transponders
step5: if new transponders appeared continue with step2 else...
step6: reset transponders scanned state, return
	taskLock(&db->t);
	debugMsg("resetting scanned flag\n");
	reset_tp_scanned(db);
	do
	{
		debugMsg("initialising iterator\n");
		if(iterBTreeInit(&it,db->root))
		{
			errMsg("No transponders to scan ?!\n");
			taskUnlock(&db->t);
			return 1;
		}
		taskUnlock(&db->t);
		num_tp=iterGetSize(&it);
		debugMsg("size=%u\n",num_tp);
		found=0;
		for(i=0;i<num_tp;i++)
		{
			iterSetIdx(&it,i);
			t=iterGet(&it);
			debugMsg("got: %p\n",t);//b6bb9d8c
			
			if(t&&(!t->scanned))
			{
				TransponderInfo ti;
				debugMsg("getting tp info\n");
				gen_tp_info(&ti,t);
				debugMsg("stopping pgmdb\n");
//				pgmdbStop(db);
				debugMsg("tuning\n");
				if(!tune(x, &ti))
				{
					debugMsg("set_transp\n");
					if(!pgmdb_set_transp(db,ti.frequency,ti.pol))
					{
						debugMsg("waiting for pat\n");
						wait_for_pat(db);
//						debugMsg("skim_pmt\n");
//						skim_pmt(db,t);
						debugMsg("waiting for NIT\n");
						wait_for_nit(db);
						found=1;
					}
				}
				t->scanned=1;
			}
		}
		debugMsg("iterClear\n");
		iterClear(&it);
		taskLock(&db->t);
	}while(found);
	debugMsg("resetting scanned flag\n");
	reset_tp_scanned(db);
	taskUnlock(&db->t);
	return 0;

}
*/
int
pgmdbScanTp (PgmDb * db)
{
  debugMsg ("pgmdbScanTp\n");
  if (!db->tuned)
    return 1;
  wait_for_pat (db);
//      wait_for_sdt(db);
//      wait_for_nit(db);
//      skim_pmt(db,db->tuned);
  return 0;
}

/**
 *\brief list the pids of program
 *
 * does a deep copy of the elementary stream infos of the pmt, possibly merging multiple sections to form a single array<br>
 *\warning may not work on a transponder that's not tuned, as we may need to fetch sections.
 *\return pointer to an array of elementary stream information or NULL on error
 */
ES *
pgmdbListStreams (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol,
                  uint16_t pnr, uint16_t * pcr_ret, int *num_res)
{
//      BTreeNode * n1;//,*n2;
  Table *pmt;
  uint16_t pcr = 0x1fff;
//      Transponder cv1;
  ES *rv = NULL;
  PgmdbPos *pp;
  Transponder *t;
  pthread_mutex_lock (&db->access_mx);
  taskLock (&db->t);
//      cv1.frequency=freq;
//      cv1.pol=pol;
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    debugMsg ("looking for node\n");
    t = pgmdb_find_tp (db, pp, freq, pol);
    if (t != db->tuned)
    {
      errMsg ("not tuned\n");
      taskUnlock (&db->t);
      pthread_mutex_unlock (&db->access_mx);
      *num_res = 0;
      return NULL;
    }
    if (t)
    {

      //              Table cv2;
      //              t=btreeNodePayload(n1);
      //              cv2.pnr=pnr;
      debugMsg ("get_pmt in pgmdbListStreams\n");
      pmt = get_pmt (db, pnr, pos, t);
      if (pmt)
      {
        PMS *s;
        Iterator i;
        int j;
        int num;
        debugMsg ("found pmt\n");
        //                      pmt=btreeNodePayload(n2);

        debugMsg ("retrieving information from pmt\n");
        if (iterTblInit (&i, pmt))
        {
          errMsg ("iterator error\n");
          taskUnlock (&db->t);
          pthread_mutex_unlock (&db->access_mx);
          *num_res = 0;
          return NULL;
        }
        rv = NULL;
        num = 0;
        do
        {
          s = iterGet (&i);
          num += s->num_es;
        }
        while (!iterAdvance (&i));
        debugMsg ("counted %d streams\n", num);
        rv = utlCalloc (num, sizeof (ES));
        if (!rv)
        {
          errMsg ("allocation error\n");
          taskUnlock (&db->t);
          pthread_mutex_unlock (&db->access_mx);
          *num_res = 0;
          return NULL;
        }
        iterFirst (&i);
        j = 0;
        do
        {
          int k;
          s = iterGet (&i);
          for (k = 0; k < s->num_es; k++)
          {
            copy_es_info (&rv[j], &s->stream_infos[k]);
            j++;
          }
          if (s->pcr_pid != 0x1fff)
            pcr = s->pcr_pid;
        }
        while (!iterAdvance (&i));
        iterClear (&i);
        *num_res = num;
        clear_tbl (pmt);
        utlFAN (pmt);
      }
    }
  }
  taskUnlock (&db->t);
  pthread_mutex_unlock (&db->access_mx);
  *pcr_ret = pcr;
  return rv;
}

int
wait_for_pms_sec (PgmDb * db, uint8_t sec_nr, uint16_t pnr, uint8_t sec[4096])
{
  int rv = 0;
  struct timeval current;
  struct timespec until;
  gettimeofday (&current, NULL);
  until.tv_sec = current.tv_sec + db->pms_timeout_s;
  until.tv_nsec = (current.tv_usec) * 1000;
  db->pms_wait_pnr = pnr;
  db->pms_wait_secnr = sec_nr;
  db->pms_wait = 1;
//      db->pms_sec=NULL;
  while ((db->pms_wait) && (!rv))
  {
    rv = pthread_cond_timedwait (&db->pms_wait_cond, &db->t.mx, &until);
  }
  if (rv)
  {
    errMsg ("pthread_cond_timedwait: %s\n", strerror (rv));
    return 1;
  }

  memmove (sec, db->pms_sec, 4096);
  return 0;
}


/*
int pgmdb_wait_for_tuning(PgmDb *db,uint32_t pos,uint32_t freq,uint8_t pol)
{
	int rv=0;
	struct timeval current;
	struct timespec until;
	gettimeofday(&current,NULL);
	until.tv_sec=current.tv_sec+db->pms_timeout_s;
	until.tv_nsec=(current.tv_usec)*1000;
	db->tune_wait_pos=pos;
	db->tune_wait_freq=freq;
	db->tune_wait_pol=pol;
	db->tune_wait=1;
	while((db->tune_wait)&&(!rv))
	{
		rv=pthread_cond_timedwait(&db->tune_wait_cond,&db->t.mx,&until);
		
	}
	if(rv)
		errMsg("pthread_cond_timedwait: %s\n",strerror(rv));
	
	return rv;
}
*/
/**
 *\brief get a Program Map Table
 *
 * Will look in pms_cache for section first. <br>
 * If not found will wait for it to be retrieved unless we aren't tuned. <br>
 * If we aren't tuned or we timeout and whole table can't be retrieved we fail. <br>
 * this is all too complicated.
 *
 * expects to be called with lock held
 *\return NULL on failure
 *
 */
Table *
get_pmt (PgmDb * db, uint16_t pnr, uint32_t pos, Transponder * t)
{
  uint8_t sec[4096];
  int i, rv;
  uint16_t tid;
  int num_sec = 1;
  Table *tbl = NULL;
  PMS tpms;
  tbl = NULL;
  i = 0;
  do
  {
    debugMsg ("Getting pms\n");
    //get the first section
    if (get_cache_sec (i, pnr, pos, t->frequency, t->pol, &db->pms_cache, sec)
        || (tbl && (tbl->version != -1)
            && (tbl->version != get_sec_version (sec))))
    {
      if (db->tuned != t)       //wrong transponder/position.. give up?
      {
        errMsg ("wrong pos/tp\n%p,%p\nWant: %u,%u,%" PRIu32 "\nGot:%" PRIu32
                ",%" PRIu8 "\n", t, db->tuned, pos, t->frequency, t->pol,
                db->tuned ? db->tuned->frequency : 0,
                db->tuned ? db->tuned->pol : 0);
        return NULL;
      }
      debugMsg ("From stream\n");
      if ((1 == pgmdb_set_pgm (db, pnr)) || ((wait_for_pms_sec (db, i, pnr, sec))))     //not in cache, or stale. try directly
      {
        errMsg ("sec not found\n");
        if (tbl)
        {
          debugMsg ("Clearing table\n");
          clear_tbl (tbl);
          utlFAN (tbl);
        }
        return NULL;
      }
    }
    else
    {
      debugMsg ("From cache\n");
    }
    rv = parse_pms (sec, &tpms);        //*sec== "\337\337\337 ...." freed? why? cache is mssing element too..

    if (rv)
    {
      if (tbl)
      {
        clear_tbl (tbl);
        utlFAN (tbl);
      }
      errMsg ("Parsing failed\n");
      return NULL;
    }

    tid = sec[0];
    num_sec = tpms.c.last_section + 1;

    if (!tbl)
      tbl = new_tbl (num_sec, tid);
    if (!tbl)
    {
      errMsg ("Could not create table\n");
      return NULL;
    }
    if (tbl->num_sections != num_sec)
    {
      resize_tbl (tbl, num_sec, tid);
    }

    if ((tbl->version == -1) || (tbl->version == tpms.c.version))
    {                           //new table or same version
      check_enter_sec (tbl, (PAS *) & tpms, tid, NULL, dummy_sec_action);
      i++;
    }
    else
    {                           //different version. clear table, start over
      int j;
      for (j = 0; j < tbl->num_sections; j++)
      {
        if (bifiGet (tbl->occupied, j))
          clear_tbl_sec (tbl, j, tid);
      }
      BIFI_INIT (tbl->occupied);
      check_enter_sec (tbl, (PAS *) & tpms, tid, NULL, dummy_sec_action);
      i = 0;                    //start over
    }
  }
  while (i < num_sec);
  return tbl;
}

int32_t
pgmdbGetTsid (PgmDb * db, uint32_t pos, int32_t freq, uint8_t pol)
{
  Transponder *t;
  PgmdbPos *pp;
  int32_t tsid;
  pthread_mutex_lock (&db->access_mx);
  taskLock (&db->t);
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    debugMsg ("looking for node\n");
    t = pgmdb_find_tp (db, pp, freq, pol);
    if (t)
    {
      if (t->tbl[T_PAT])
      {
        int i;
        for (i = 0; i < t->tbl[T_PAT]->num_sections; i++)
        {
          if (bifiGet (t->tbl[T_PAT]->occupied, i))
          {
            tsid = t->tbl[T_PAT]->pas_sections[i].tsid;
            taskUnlock (&db->t);
            pthread_mutex_unlock (&db->access_mx);
            return tsid;
          }
        }
      }
      if (t->tbl[T_SDT])
      {
        int i;
        for (i = 0; i < t->tbl[T_SDT]->num_sections; i++)
        {
          if (bifiGet (t->tbl[T_SDT]->occupied, i))
          {
            tsid = t->tbl[T_SDT]->sds_sections[i].tsid;
            taskUnlock (&db->t);
            pthread_mutex_unlock (&db->access_mx);
            return tsid;
          }
        }
      }
    }
  }
  taskUnlock (&db->t);
  pthread_mutex_unlock (&db->access_mx);
  return -1;
}

int32_t
pgmdbGetOnid (PgmDb * db, uint32_t pos, int32_t freq, uint8_t pol)
{
  Transponder *t;
  PgmdbPos *pp;
  int32_t onid;
  pthread_mutex_lock (&db->access_mx);
  taskLock (&db->t);
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    debugMsg ("looking for node\n");
    t = pgmdb_find_tp (db, pp, freq, pol);
    if (t)
    {
      if (t->tbl[T_SDT])
      {
        int i;
        for (i = 0; i < t->tbl[T_SDT]->num_sections; i++)
        {
          if (bifiGet (t->tbl[T_SDT]->occupied, i))
          {
            onid = t->tbl[T_SDT]->sds_sections[i].onid;
            taskUnlock (&db->t);
            pthread_mutex_unlock (&db->access_mx);
            return onid;
          }
        }
      }
    }
  }
  taskUnlock (&db->t);
  pthread_mutex_unlock (&db->access_mx);
  return -1;
}

int
pgmdbUntuned (PgmDb * db)
{
  int rv = 0;
  taskLock (&db->t);
  rv = (NULL == db->tuned);
  taskUnlock (&db->t);
  return rv;
}

int
pgmdbTuned (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol)
{
  int rv = 0;
  taskLock (&db->t);
  rv = ((db->tuned) && (db->current_pos) && (pos == db->current_pos->pos)
        && (freq == db->tuned->frequency) && (pol == db->tuned->pol));
  taskUnlock (&db->t);
  return rv;
}

int
pgmdbFt (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol, int32_t ft)
{
  Transponder *t;
  PgmdbPos *pp;
  taskLock (&db->t);
//find transponder
  pp = pgmdb_find_pos (db, pos);
  if (pp)
  {
    debugMsg ("looking for node\n");
    t = pgmdb_find_tp (db, pp, freq, pol);
    if (t)
    {
      t->ftune = ft;
      taskUnlock (&db->t);
      return 0;
    }
  }
//if no transponder return 1
  taskUnlock (&db->t);
  return 1;
}
