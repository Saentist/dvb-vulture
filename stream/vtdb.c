#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include "vtdb.h"
#include "utl.h"
#include "in_out.h"
#include "config.h"
#include "debug.h"
#include "iterator.h"
#include "dvb.h"
#include "vt_pk.h"
#include "pidbuf.h"
#include "dmalloc.h"
int DF_VTDB;
#define FILE_DBG DF_VTDB

//----------------------------------structures (internal)-----------------------------------------------------
typedef struct
{
  uint8_t pgno;
  uint16_t subno;
  VtPk hdr;
  SList packets;                ///<linked list of data packets
} VtPage;


/**
 *\brief holds the pages of a magazine
 *
 * this plays a certain role for on disk storage.
 * Every now and then we write out magazines based on certain citeria.
 * So... how should those look like?
 * We count the number of resident magazines and start writing when a certain threshold is exceeded
 * we may check for this condition every minute.
 * inside the function we could set a flag inside magazine, which gets reset when the magazine gets updated.
 * we only write those magazines which haven't seen updates since previous minute.
 * we may use a self-sorting list as a kind of LRU. on every update we move magazine one element farther to the front of list.
 * new magazines get inserted in the middle. we write out last element. 
 * on writing magazine remove corresponding element from list. On reading insert in middle.
 *
 * functions: put_mid, remove_last, remove(?), fwd
 *
 * can this be made more simple?
 *
 * Used a linked list \see lru.h
 * will perform badly on alternating equally frequent accesses of a small set of elements.
 * Eg.
 * (a,b,a,b,a...)
 * Will just swap those two without changing their overall position inside list
 *
 * TODO: clean out stale pages..
 */
typedef struct
{
  LRUE head;
  VtPage current_page;
//      VtPage mip;//magazine inventory page: mFD
//      VtPage mot;//magazine organization table: mFE
  BTreeNode *pages;
  BTreeNode *packets;
} VtMag;

typedef struct
{
  FpPointer p;
  time_t ts;
} OdVtMag;

typedef struct
{
  uint8_t id;
  uint8_t on_disk:1;
  uint8_t accessed:1;
  union
  {
    VtMag m;
    OdVtMag odm;
  };
} VtMagNode;

/**
 *\brief holds services' packets and magazines
 *
 */
typedef struct
{
  uint8_t id;
//      VtPage aci;//automatic channel installation: 1BE
//      VtPage btt;//basic top table address: 1F0
  BTreeNode *mags;
  BTreeNode *packets;
} VtSvc;

typedef struct
{
  uint16_t id;
  uint16_t pnr_refctr;
  BTreeNode *svc;
} VtPid;

/**
 *\brief holds the vt pids for a pnr
 *
 * each node in pids contains an uint16_t referencing a pid.
 * the VtPnr for a pnr gets regenerated when a PNR_ADD event is detected
 * pgmdb is used to retrieve the stream composition, and the pids with 
 * non NULL txt pointer will be added to the pids tree. Stale ones will be removed.
 *
 * when we see a TUNED event we check for no longer existing pnrs and remove those
 *
 * when we swap in magazines, we check whether the pid is still referenced by one of the pnrs, and remove pid if not.
 * Can we speed this up? Scanning all pnrs may be somewhat slow. Use refctr in VtPid?
 * Several pnrs may reference the same pid (eg ORF txt).
 * perhaps check pids periodically in bkgnd as well
 *
 */
typedef struct
{
  uint16_t id;
  BTreeNode *pids;
} VtPnr;

///on disk format for magnodes
typedef struct
{
  time_t ts;
  FpPointer p;
  uint8_t id;
} OdsMagNode;

//-----------------------------------------------fwd decls-------------------------------------------------------------

static void vtdb_read_mag (VtDb * db, VtMag * mag, FpPointer odm, FpHeap * h);
static int vtdb_read_page (VtPage * page, FILE * f);
static void vtdb_read_packet (VtPk * pk, FILE * f);
static SListE *vtdb_sl_read_packet (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_page (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_packet (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_magn (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_svc (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_pid (void *x, FILE * f);
static int vtdb_pidref_reg (VtTsid * tsid, uint16_t pid);
static BTreeNode *vtdb_btree_read_pidref (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_pnr (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_tsid (void *x, FILE * f);
static BTreeNode *vtdb_btree_read_pos (void *x, FILE * f);
static int vtdb_read_index (FpPointer od_root, VtDb * db);
static FpSize vtdb_write_mag_size (VtMag * mag);
static uint64_t vtdb_write_pk_size (VtPk * pk);
static uint64_t vtdb_sl_write_pk_size (void *x, SListE * this);
static FpSize vtdb_write_page_size (VtPage * pg);
static uint64_t vtdb_btree_write_page_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_pk_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_magn_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_svc_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_pid_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_pidref_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_pnr_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_tsid_size (void *x, BTreeNode * this);
static uint64_t vtdb_btree_write_pos_size (void *x, BTreeNode * this);
static FpSize vtdb_get_index_size (VtDb * db);
static void vtdb_write_packet (VtPk * this, FILE * f);
static int vtdb_sl_write_packet (void *x, SListE * this, FILE * f);
static void vtdb_write_page (VtPage * pg, FILE * f);
static int vtdb_btree_write_page (void *x, BTreeNode * this, FILE * f);
static int vtdb_btree_write_packet (void *x, BTreeNode * this, FILE * f);
static FpPointer vtdb_write_mag (VtMag * mag, FpHeap * h);
static int vtdb_btree_write_magn (void *x, BTreeNode * this, FILE * f);
static int vtdb_btree_write_svc (void *x, BTreeNode * this, FILE * f);
static int vtdb_btree_write_pid (void *x, BTreeNode * this, FILE * f);
static int vtdb_btree_write_pidref (void *x, BTreeNode * this, FILE * f);
static int vtdb_btree_write_pnr (void *x, BTreeNode * this, FILE * f);
static int vtdb_btree_write_tsid (void *x, BTreeNode * this, FILE * f);
static int vtdb_btree_write_pos (void *x, BTreeNode * this, FILE * f);
static int vtdb_write_index_real (FpPointer od_root, VtDb * db);
static int vtdb_write_idx (VtDb * db);
static void vtdb_delete (VtDb * db);
static void vtdb_btree_count_node (void *x, BTreeNode * this, int which,
                                   int depth);
static void vtdb_dump_svc (void *x, BTreeNode * this, int which, int depth);
static void vtdb_dump_pid (void *x, BTreeNode * this, int which, int depth);
static void vtdb_dump_tsid (void *x, BTreeNode * this, int which, int depth);
static void vtdb_dump_stats (VtDb * db);
static void vtdb_destroy_mag_node (void *x, BTreeNode * this, int which,
                                   int depth);
static void vtdb_destroy_svc (void *x, BTreeNode * this, int which,
                              int depth);
//static void vtdb_destroy_pid (void *x, BTreeNode * this, int which,
  //                            int depth);
static void vtdb_clean_pid (void *x, BTreeNode * this, int which, int depth);
static void vtdb_clean_pidref (void *x, BTreeNode * this, int which,
                               int depth);
static void vtdb_clean_pnr (void *x, BTreeNode * this, int which, int depth);
static void vtdb_clean_tsid (void *x, BTreeNode * this, int which, int depth);
static void vtdb_clean_pos (void *x, BTreeNode * this, int which, int depth);
static void vtdb_clear_page (VtPage * page);
static void vtdb_destroy_page (void *x, BTreeNode * this, int which,
                               int depth);
static void vtdb_destroy_pack (void *x, BTreeNode * this, int which,
                               int depth);
static void vtdb_clear_mag (VtMag * mag);
static void vtdb_clean_mag_node (void *x, BTreeNode * this, int which,
                                 int depth);
static void vtdb_clean_svc (void *x, BTreeNode * this, int which, int depth);
static int vtdb_cmp_svc (void *x, const void *a, const void *b);
static int vtdb_cmp_pid (void *x, const void *a, const void *b);
static int vtdb_cmp_pos (void *x, const void *a, const void *b);
static int vtdb_cmp_tsid (void *x, const void *a, const void *b);
static int vtdb_cmp_pnr (void *x, const void *a, const void *b);
static int vtdb_cmp_pidref (void *x, const void *a, const void *b);
static int vtdb_cmp_mag (void *x, const void *a, const void *b);
static int vtdb_cmp_page (void *x, const void *a, const void *b);
static int vtdb_cmp_pack (void *x, const void *a, const void *b);
static VtTsid *vtdb_get_or_ins_tsid (VtDb * db, uint16_t tsid);
static VtPos *vtdb_get_or_ins_pos (VtDb * db, uint32_t pos);
static VtPid *vtdb_get_or_ins_pid (VtTsid * tsid, VtPk * d);
static VtSvc *vtdb_get_or_ins_svc (VtPid * pid, VtPk * d);
static VtMag *vtdb_get_mag (VtDb * db, VtSvc * sv, uint8_t magno);
///must be called with selector locked, will unlock selector when reading.
static VtMag *vtdb_get_or_ins_mag (VtDb * db, VtSvc * sv, VtPk * d);
static VtPage *vtdb_get_or_ins_page (VtMag * db, uint8_t pgno,
                                     uint16_t subno);
static void vtdb_new_or_replace_pack (BTreeNode ** root, VtPk * d);
#if 0
static void vtdb_age_svc (VtDb * db, time_t now, BTreeNode ** root,
                          VtSvc * svc);
static void vtdb_age_pid (VtDb * db, time_t now, BTreeNode ** root,
                          VtPid * pid);
static void vtdb_age_tsid (VtDb * db, time_t now, BTreeNode ** root,
                           VtTsid * tsid);
static void vtdb_age (VtDb * db, time_t now);
#endif
static VtMagNode *lru_e_to_mag_node (LRUE * x);
static void vtdb_swapout_all_mags (VtDb * db);
static void vtdb_check_swapout (VtDb * db);
static int vtdb_check_packet (VtPk * d);
static void vtdb_put (void *db, VtPk * d);
static void vtdb_proc_evt (VtDb * db, SelBuf * d);
static void *vtdb_store_loop (void *p);
static VtPk *vtdb_collect_packets (VtDb * db, BTreeNode * root,
                                   uint16_t * num_pk_ret);
static VtSvc *vtdb_lookup_svc (VtDb * db, uint32_t pos, uint16_t tsid,
                               uint16_t pid, uint8_t data_id);
static VtSvc *vtdb_find_svc (VtPid * root, uint8_t data_id);
static VtTsid *vtdb_btree_find_tsid (VtPos * root, uint16_t tsid);
static VtPos *vtdb_btree_find_pos (VtDb * db, uint32_t pos);
static VtPnr *vtdb_btree_find_pnr (VtTsid * root, uint16_t pnr);
static VtPid *vtdb_btree_find_pid (VtTsid * root, uint16_t pid);
//-----------------------------------------------Public interface-------------------------------------------------------
int
vtdbInit (VtDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb)
{
  Section *s;
  long tmp;
  char *v;
  uint8_t dbinit = 0;
  FpPointer od_root = 0, tmp_ptr;
  FpSize frag_sz;
  struct stat buf;
  int setvbuf_bytes;

  memset (db, 0, sizeof (VtDb));

  db->dbfilename = DEFAULT_VTDB;
  debugMsg ("vtdbInit1\n");
  s = rcfileFindSec (cfg, "VT");
  debugMsg ("vtdbInit2\n");

  if (s && (!rcfileFindSecVal (s, "database", &v)))
  {
    db->dbfilename = v;
  }
  db->mag_limit = 8;
  if (s && (!rcfileFindSecValInt (s, "mag_limit", &tmp)))
  {
    db->mag_limit = tmp;
  }
  db->real_mag_limit = db->mag_limit;
  db->vtdb_days = 7;            //retention time for stale pages, unused
  setvbuf_bytes = -1;
  if (s && (!rcfileFindSecValInt (s, "setvbuf_bytes", &tmp)))
  {
    setvbuf_bytes = tmp;
  }

  frag_sz = 0;
  //fragment size for persistent allocator.
  //small values cause fragmentation. 
  //the default 0 leaves allocators' default setting.
  if (s && (!rcfileFindSecValInt (s, "frag_sz_mb", &tmp)))
  {
    frag_sz = tmp * 1024 * 1024;
  }

  db->niceness = 0;
  if (s && (!rcfileFindSecValInt (s, "niceness", &tmp)) && (tmp >= -20)
      && (tmp <= 19))
  {
    db->niceness = tmp;
  }
  if (stat (db->dbfilename, &buf))
  {
    if (errno == ENOENT)
    {
      int f;
      debugMsg ("%s nonexistent, creating..\n", db->dbfilename);
      f = creat (db->dbfilename, S_IRUSR | S_IWUSR);
      close (f);
      dbinit = 1;
    }
  }

  debugMsg ("opening file\n");
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
    db->root = NULL;
  }
  else
  {
    FP_GO (sizeof (FpPointer), db->handle);
    debugMsg ("attaching heap\n");
    if (fphAttach (db->handle, &(db->h)))
    {
      debugMsg ("attaching failed, trying to reinitialize\n");
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
      db->root = NULL;
    }
    else
    {                           //Geez it worked after all.. I'm sooo excited
      if (frag_sz)
        fphSetFragmentSize (&db->h, frag_sz);
      FP_GO (0, db->handle);
      debugMsg ("starting index read\n");
      fread (&od_root, sizeof (FpPointer), 1, db->handle);

      tmp_ptr = 0;
      FP_GO (0, db->handle);
      fwrite (&tmp_ptr, sizeof (FpPointer), 1, db->handle);

      if (vtdb_read_index (od_root, db))
      {
        errMsg ("failed to read saved index, invalidating database\n");
        fclose (db->handle);
        utlFAN (db->vbuf);
        return 1;
      }
      fphFree (od_root, &db->h);
    }
  }


  if (taskInit (&db->t, db, vtdb_store_loop))
  {
    errMsg ("task failed to init\n");
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }

  if (LRUInit (&db->mag_lru))
  {
    errMsg ("LRU failed to init\n");
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }
  db->o = dvbDmxOutAdd (dvb);
  db->input = svtOutSelPort (db->o);
  if ((NULL == db->o) || pesExInit (&db->pes_ex)
      || vtExInit (&db->vt_ex, db, vtdb_put))
  {
    errMsg ("extractor failed to init\n");
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }
  db->dvb = dvb;
  db->pgmdb = pgmdb;
  db->pnrs = NULL;
  db->num_pnrs = 0;
  db->current_pos = NULL;
  db->current_tsid = NULL;
  db->current_pos_idx = 0;
  db->current_freq = 0;
  db->current_pol = 0;
  debugMsg ("vtdbInit complete\n");
  return 0;
}

void
vtdbClear (VtDb * db)
{
  double h_m_ratio;
  uint64_t total;
  vtdbStop (db);
  debugMsg ("taskClear\n");
  taskClear (&db->t);
  debugMsg ("vtExClear\n");
  vtExClear (&db->vt_ex);
  pesExClear (&db->pes_ex);
  dvbDmxOutRmv (db->o);
  db->o = NULL;
  db->input = NULL;
  debugMsg ("vtdb_write_idx\n");
  if (vtdb_write_idx (db))
  {
    debugMsg ("vtdb_delete\n");
    vtdb_delete (db);
  }
  btreeWalk (db->root, NULL, vtdb_clean_pos);   //just free structures in memory
  utlFAN (db->pnrs);
  h_m_ratio = (((double) db->hits) / ((double) db->misses));
  total = db->hits + db->misses;
  debugMsg ("Hit/Miss ratio: %f\n Hits: %" PRIu64 " Misses: %" PRIu64
            " Total: %" PRIu64 "\n %f%%", h_m_ratio, db->hits, db->misses,
            total, ((double) db->hits * 100.0) / ((double) total));
}

void
CUvtdbClear (void *db)
{
  vtdbClear (db);
}

int
CUvtdbInit (VtDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb, CUStack * s)
{
  int rv;
  rv = vtdbInit (db, cfg, dvb, pgmdb);
  cuStackFail (rv, db, CUvtdbClear, s);
  return rv;
}

#if 0
int
vtdbPurgePnr (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid)
{
  VtTsid *tsidp;
  VtPnr *root;
  taskLock (&db->t);
  root = vtdb_btree_find_pnr (tsidp =
                              vtdb_btree_find_tsid (db->root, tsid), pnr);
  if (!root)
  {
    taskUnlock (&db->t);
    return 1;
  }
  btreeWalk (root->pids, db, vtdb_destroy_pid);
  btreeNodeRemove (&tsidp->pnrs, btreePayloadNode (root));
  utlFAN (btreePayloadNode (root));
  taskUnlock (&db->t);
  return 0;
}
#endif

VtPk *
vtdbGetSvcPk (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
              uint8_t data_id, uint16_t * num_pk_ret)
{
  VtPk *rv;
  VtSvc *svcp;
  taskLock (&db->t);
  svcp = vtdb_lookup_svc (db, pos, tsid, pid, data_id);
  if (!svcp)
  {
    taskUnlock (&db->t);
    *num_pk_ret = 0;
    return NULL;
  }
  rv = vtdb_collect_packets (db, svcp->packets, num_pk_ret);
  taskUnlock (&db->t);
  return rv;
}


VtPk *
vtdbGetMagPk (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
              uint8_t data_id, uint8_t magno, uint16_t * num_pk_ret)
{
  VtPk *rv;
  VtSvc *svcp;
  VtMag *magp;

  taskLock (&db->t);
  svcp = vtdb_lookup_svc (db, pos, tsid, pid, data_id);
  if (!svcp)
  {
    taskUnlock (&db->t);
    *num_pk_ret = 0;
    return NULL;
  }
  magp = vtdb_get_mag (db, svcp, magno);
  if (!magp)
  {
    taskUnlock (&db->t);
    *num_pk_ret = 0;
    return NULL;
  }
  rv = vtdb_collect_packets (db, magp->packets, num_pk_ret);
  taskUnlock (&db->t);
  return rv;
}

VtPk *
vtdbGetPg (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
           uint8_t data_id, uint8_t magno, uint8_t pgno, uint16_t subno,
           uint16_t * num_pk_ret)
{
  VtPk *rv;
  BTreeNode *pgn;
  VtSvc *svcp;
  VtPage cv3;
  VtPage *pgp;
  VtMag *magp;
  uint32_t i;
  SListE *e;
  taskLock (&db->t);

  svcp = vtdb_lookup_svc (db, pos, tsid, pid, data_id);
  if (!svcp)
  {
    taskUnlock (&db->t);
    *num_pk_ret = 0;
    return NULL;
  }
  magp = vtdb_get_mag (db, svcp, magno);
  if (!magp)
  {
    taskUnlock (&db->t);
    *num_pk_ret = 0;
    return NULL;
  }
  cv3.pgno = pgno;
  cv3.subno = subno;
  pgn = btreeNodeFind (magp->pages, &cv3, NULL, vtdb_cmp_page);
  if (!pgn)
  {
    taskUnlock (&db->t);
    *num_pk_ret = 0;
    return NULL;
  }
  pgp = btreeNodePayload (pgn);
  i = 1;
  e = slistFirst (&pgp->packets);
  while (e)
  {
    i++;
    e = slistENext (e);
  }
  rv = utlCalloc (i, EX_BUFSZ);
  if (!rv)
  {
    taskUnlock (&db->t);
    *num_pk_ret = 0;
    return NULL;
  }
  memmove (rv, &pgp->hdr, EX_BUFSZ);
  i = 1;
  e = slistFirst (&pgp->packets);
  while (e)
  {
    memmove (rv + i, slistEPayload (e), EX_BUFSZ);
    i++;
    e = slistENext (e);
  }
  *num_pk_ret = i;
  taskUnlock (&db->t);
  return rv;
}

uint16_t *
vtdbGetTsids (VtDb * db, uint32_t pos, uint16_t * num_tsid_ret)
{
  uint16_t *rv;
  uint16_t num;
  int j;
  VtTsid *ptr;
  VtPos *pp;
  Iterator iter;

  taskLock (&db->t);
  pp = vtdb_btree_find_pos (db, pos);
  if (NULL == pp)
  {
    *num_tsid_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  if (iterBTreeInit (&iter, pp->tsids))
  {
    *num_tsid_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  num = iterGetSize (&iter);
  if (!num)
  {
    *num_tsid_ret = 0;
    iterClear (&iter);
    taskUnlock (&db->t);
    return NULL;
  }
  rv = utlCalloc (num, sizeof (uint16_t));
  for (j = 0; j < num; j++)
  {
    iterSetIdx (&iter, j);
    ptr = iterGet (&iter);
    rv[j] = ptr->id;
  }
  *num_tsid_ret = num;
  iterClear (&iter);
  taskUnlock (&db->t);
  return rv;
}

uint8_t *
vtdbGetSvc (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
            uint16_t * num_ret)
{
  uint8_t *rv;
  uint16_t num;
  int j;
  VtPid *root;
  VtSvc *ptr;
  Iterator iter;

  taskLock (&db->t);
  root = vtdb_btree_find_pid (  /*vtdb_btree_find_pnr( */
                               vtdb_btree_find_tsid (vtdb_btree_find_pos
                                                     (db, pos),
                                                     tsid) /*,pnr) */ , pid);
  if (!root)
  {
    *num_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  if (iterBTreeInit (&iter, root->svc))
  {
    *num_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  num = iterGetSize (&iter);
  if (!num)
  {
    *num_ret = 0;
    iterClear (&iter);
    taskUnlock (&db->t);
    return NULL;
  }
  rv = utlCalloc (num, sizeof (uint8_t));
  for (j = 0; j < num; j++)
  {
    iterSetIdx (&iter, j);
    ptr = iterGet (&iter);
    rv[j] = ptr->id;
  }
  *num_ret = num;
  iterClear (&iter);
  taskUnlock (&db->t);
  return rv;
}

uint16_t *
vtdbGetPids (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pnr,
             uint16_t * num_ret)
{
  uint16_t *rv;
  uint16_t num;
  int j;
  uint16_t *ptr;
  VtPnr *root;
  Iterator iter;

  taskLock (&db->t);
  root =
    vtdb_btree_find_pnr (vtdb_btree_find_tsid
                         (vtdb_btree_find_pos (db, pos), tsid), pnr);
  if (NULL == root)
  {
    *num_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  if (iterBTreeInit (&iter, root->pids))
  {
    *num_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  num = iterGetSize (&iter);
  if (!num)
  {
    *num_ret = 0;
    iterClear (&iter);
    taskUnlock (&db->t);
    return NULL;
  }
  rv = utlCalloc (num, sizeof (uint16_t));
  if (NULL == rv)
  {
    *num_ret = 0;
    iterClear (&iter);
    taskUnlock (&db->t);
    return NULL;
  }
  for (j = 0; j < num; j++)
  {
    iterSetIdx (&iter, j);
    ptr = iterGet (&iter);
    rv[j] = *ptr;
  }
  *num_ret = num;
  iterClear (&iter);
  taskUnlock (&db->t);
  return rv;
}

uint16_t *
vtdbGetPnrs (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t * num_ret)
{

  uint16_t *rv;
  uint16_t num;
  int j;
  VtTsid *root;
  VtPnr *ptr;
  Iterator iter;

  taskLock (&db->t);
  root = vtdb_btree_find_tsid (vtdb_btree_find_pos (db, pos), tsid);
  if (!root)
  {
    *num_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  if (iterBTreeInit (&iter, root->pnrs))
  {
    *num_ret = 0;
    taskUnlock (&db->t);
    return NULL;
  }
  num = iterGetSize (&iter);
  if (!num)
  {
    *num_ret = 0;
    iterClear (&iter);
    taskUnlock (&db->t);
    return NULL;
  }
  rv = utlCalloc (num, sizeof (uint16_t));
  for (j = 0; j < num; j++)
  {
    iterSetIdx (&iter, j);
    ptr = iterGet (&iter);
    rv[j] = ptr->id;
  }
  *num_ret = num;
  iterClear (&iter);
  taskUnlock (&db->t);
  return rv;
}

int
vtdbStart (VtDb * db)
{
  return taskStart (&db->t);
}

int
vtdbStop (VtDb * db)
{
  return taskStop (&db->t);
}

//---------------------------------------------------------static functions, reading------------------------------------------

static void
vtdb_read_mag (VtDb * db, VtMag * mag, FpPointer odm, FpHeap * h)
{
  //first, see that we don't exceed our limit
  vtdb_check_swapout (db);
  //now read
  debugPri (2, "Reading Mag %p\n", mag);
  FP_GO (odm, h->f);
  vtdb_read_page (&mag->current_page, h->f);
  btreeRead (&mag->pages, NULL, vtdb_btree_read_page, h->f);
  btreeRead (&mag->packets, NULL, vtdb_btree_read_packet, h->f);
  fphFree (odm, h);
}

static int
vtdb_read_page (VtPage * page, FILE * f)
{
  fread (page, sizeof (VtPage) - sizeof (SList), 1, f);
  return slistRead (&page->packets, NULL, vtdb_sl_read_packet, f);
}

static void
vtdb_read_packet (VtPk * pk, FILE * f)
{
  fread (pk, sizeof (VtPk), 1, f);
}

static SListE *
vtdb_sl_read_packet (void *x NOTUSED, FILE * f)
{
  VtPk *pkp;
  SListE *e;
  e = utlCalloc (1, slistESize (sizeof (VtPk)));
  if (e)
  {
    pkp = slistEPayload (e);
    vtdb_read_packet (pkp, f);
  }
  return e;
}

static BTreeNode *
vtdb_btree_read_page (void *x NOTUSED, FILE * f)
{
  BTreeNode *n;
  VtPage *pg;
  n = utlCalloc (1, btreeNodeSize (sizeof (VtPage)));
  if (n)
  {
    pg = btreeNodePayload (n);
    fread (pg, sizeof (VtPage) - sizeof (SList), 1, f);
    slistRead (&pg->packets, NULL, vtdb_sl_read_packet, f);
  }
  return n;
}

static BTreeNode *
vtdb_btree_read_packet (void *x NOTUSED, FILE * f)
{
  VtPk *pkp;
  BTreeNode *e;
  e = utlCalloc (1, btreeNodeSize (sizeof (VtPk)));
  if (e)
  {
    pkp = btreeNodePayload (e);
    vtdb_read_packet (pkp, f);
  }
  return e;
}

static BTreeNode *
vtdb_btree_read_magn (void *x NOTUSED, FILE * f)
{
  BTreeNode *rv;
  OdsMagNode om;
  VtMagNode *mn;
  rv = utlCalloc (1, btreeNodeSize (sizeof (VtMagNode)));
  if (rv)
  {
    mn = btreeNodePayload (rv);
    fread (&om, sizeof (OdsMagNode), 1, f);
    mn->odm.p = om.p;
    mn->odm.ts = om.ts;
    mn->id = om.id;
    mn->on_disk = 1;
    mn->accessed = 0;
  }
  return rv;
}

static BTreeNode *
vtdb_btree_read_svc (void *x, FILE * f)
{
  BTreeNode *rv;
  VtSvc *t;
  rv = utlCalloc (1, btreeNodeSize (sizeof (VtSvc)));
  if (rv)
  {
    t = btreeNodePayload (rv);
    fread (&t->id, sizeof (uint8_t), 1, f);
    btreeRead (&t->mags, x, vtdb_btree_read_magn, f);
    btreeRead (&t->packets, x, vtdb_btree_read_packet, f);
  }
  return rv;
}

static BTreeNode *
vtdb_btree_read_pid (void *x, FILE * f)
{
  BTreeNode *rv;
  VtPid *t;
  rv = utlCalloc (1, btreeNodeSize (sizeof (VtPid)));
  if (rv)
  {
    t = btreeNodePayload (rv);
    fread (&t->id, sizeof (uint16_t), 1, f);
    btreeRead (&t->svc, x, vtdb_btree_read_svc, f);
  }
  return rv;
}

static int
vtdb_pidref_reg (VtTsid * tsid, uint16_t pid)
{
  VtPid cv;
  VtPid *pp;
  BTreeNode *n;

  cv.id = pid;
  cv.svc = NULL;
  n =
    btreeNodeGetOrIns (&tsid->pids, &cv, NULL, vtdb_cmp_pid, sizeof (VtPid));
  if (!n)
    return 1;
  pp = btreeNodePayload (n);
  pp->pnr_refctr++;
  return 0;
}

static BTreeNode *
vtdb_btree_read_pidref (void *x, FILE * f)
{
  BTreeNode *rv;
  uint16_t *t;
  rv = utlCalloc (1, btreeNodeSize (sizeof (VtPid)));
  if (rv)
  {
    t = btreeNodePayload (rv);
    fread (t, sizeof (uint16_t), 1, f);
    if (vtdb_pidref_reg (x, *t))        //increment/insert pids refcount
    {
      utlFAN (rv);
      return NULL;
    }
  }
  return rv;
}

static BTreeNode *
vtdb_btree_read_pnr (void *x, FILE * f)
{
  BTreeNode *rv;
  VtPnr *t;

  rv = utlCalloc (1, btreeNodeSize (sizeof (VtPnr)));
  if (rv)
  {
    t = btreeNodePayload (rv);
    fread (&t->id, sizeof (uint16_t), 1, f);
    btreeRead (&t->pids, x, vtdb_btree_read_pidref, f);
  }
  return rv;
}

static BTreeNode *
vtdb_btree_read_tsid (void *x, FILE * f)
{
  BTreeNode *rv;
  VtTsid *t;
  rv = utlCalloc (1, btreeNodeSize (sizeof (VtTsid)));
  if (rv)
  {
    t = btreeNodePayload (rv);
    fread (&t->id, sizeof (uint16_t), 1, f);
    btreeRead (&t->pids, x, vtdb_btree_read_pid, f);
    btreeRead (&t->pnrs, t, vtdb_btree_read_pnr, f);
  }
  return rv;
}

static BTreeNode *
vtdb_btree_read_pos (void *x, FILE * f)
{
  BTreeNode *rv;
  VtPos *t;
  rv = utlCalloc (1, btreeNodeSize (sizeof (VtPos)));
  if (rv)
  {
    t = btreeNodePayload (rv);
    fread (&t->pos, sizeof (uint32_t), 1, f);
    btreeRead (&t->tsids, x, vtdb_btree_read_tsid, f);
  }
  return rv;
}

static int
vtdb_read_index (FpPointer od_root, VtDb * db)
{
  FP_GO (od_root, db->handle);
  return btreeRead (&db->root, db, vtdb_btree_read_pos, db->handle);
}

//----------------------------------------------static funcs debugging------------------------------------------------------------


/**
 *\brief generate histogram of cache content
 */
static void
vtdb_cache_hist (VtDb * db)
{
  DListE *e = db->mag_lru.l.first;
  FILE *fp;
  uint64_t sz = 0;
  fp = fopen ("/home/klammerj/projects/stb/vt.hst", "r+t");
  if (NULL == fp)
    return;
  fseek (fp, 0, SEEK_END);
  fprintf (fp, "Videotext Cache histogram:\n");
  fprintf (fp, "magazine, size\n");
  while (e)
  {
    uint64_t tmp = vtdb_write_mag_size ((VtMag *) e);
    sz += tmp;
    fprintf (fp, "%p, %lu\n", e, (long unsigned int) tmp);
    e = e->next;
  }

  fprintf (fp, "Cache Total size: %lu\n", (long unsigned int) sz);
  fclose (fp);
}


static void
vtdb_pg_hist (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    FILE *fp = x;
    VtPage *pg;
    pg = btreeNodePayload (this);
    fprintf (fp, "%p, %lu\n", pg,
             (long unsigned int) vtdb_write_page_size (pg));
  }
}



static void
dump_vt_bits (FILE * fp, VtPk * pk)
{
  unsigned i;
  struct
  {
    int n;
    int b;
    char *s;
  } asdf[] =
  {
    {
    0, 3, "C4  "},
    {
    2, 2, "C5  "},
    {
    2, 3, "C6  "},
    {
    3, 0, "C7  "},
    {
    3, 1, "C8  "},
    {
    3, 2, "C9  "},
    {
    3, 3, "C10 "},
    {
    4, 0, "C11 "},
    {
    4, 1, "C12 "},
    {
    4, 2, "C13 "},
    {
  4, 3, "C14 "}};

  fprintf (fp, "BITS: ");
  for (i = 0; i < ARR_NE (asdf); i++)
  {
    uint8_t v = hamm84_r (pk->data[asdf[i].n + 7]);
    if ((v != 0xff) && (v & (1 << asdf[i].b)))
      fprintf (fp, asdf[i].s);
    else
      fprintf (fp, "    ");
  }
  fprintf (fp, "\n");
}

static void
vtdb_pg_info (VtPage * pg)
{
  FILE *fp;
  fp = fopen ("/home/klammerj/projects/stb/vt.hst", "r+t");
  if (NULL == fp)
    return;
  fseek (fp, 0, SEEK_END);
  fprintf (fp, "VT Page: 0x%" PRIx8 "\n", pg->pgno);
  fprintf (fp, "Sub: 0x%" PRIx16 "\n", pg->subno);

  fprintf (fp, "Header:\n");
  fprintf (fp, "x: 0x%x\n", get_pk_x (pg->hdr.data));
  fprintf (fp, "y: 0x%x\n", get_pk_y (pg->hdr.data));
  fprintf (fp, "pgno: 0x%x\n", get_pgno (pg->hdr.data));
  fprintf (fp, "subno: 0x%x\n", get_subno (pg->hdr.data));
  dump_vt_bits (fp, &pg->hdr);

  fclose (fp);
}

static void
vtdb_mag_hist (VtMag * db)
{
  FILE *fp;
  fp = fopen ("/home/klammerj/projects/stb/vt.hst", "r+t");
  if (NULL == fp)
    return;
  fseek (fp, 0, SEEK_END);
  fprintf (fp, "Videotext Magazine histogram. Magazine: %p\n", db);
  fprintf (fp, "Current Page size: %lu\n",
           (long unsigned int) vtdb_write_page_size (&db->current_page));
  fprintf (fp, "Mag global packets size: %lu\n",
           (long unsigned int) btreeWriteSize (db->packets, NULL,
                                               vtdb_btree_write_pk_size));

  fprintf (fp, "page, size\n");
  btreeWalk (db->pages, fp, vtdb_pg_hist);

  fclose (fp);
}

//-----------------------------------------------static funcs, sizes--------------------------------------------------------------
static FpSize
vtdb_write_mag_size (VtMag * mag)
{
  FpSize sz;
  sz = vtdb_write_page_size (&mag->current_page) +
    btreeWriteSize (mag->pages, NULL, vtdb_btree_write_page_size) +
    btreeWriteSize (mag->packets, NULL, vtdb_btree_write_pk_size);
  return sz;
}

static uint64_t
vtdb_write_pk_size (VtPk * pk NOTUSED)
{
  return sizeof (VtPk);
}

static uint64_t
vtdb_sl_write_pk_size (void *x NOTUSED, SListE * this)
{
  return vtdb_write_pk_size (slistEPayload (this));
}

static FpSize
vtdb_write_page_size (VtPage * pg)
{
  return sizeof (VtPage) - sizeof (SList) + slistWriteSize (&pg->packets,
                                                            NULL,
                                                            vtdb_sl_write_pk_size);
}

static uint64_t
vtdb_btree_write_page_size (void *x NOTUSED, BTreeNode * this)
{
  return vtdb_write_page_size (btreeNodePayload (this));
}

static uint64_t
vtdb_btree_write_pk_size (void *x NOTUSED, BTreeNode * this)
{
  return vtdb_write_pk_size (btreeNodePayload (this));
}

static uint64_t
vtdb_btree_write_magn_size (void *x NOTUSED, BTreeNode * this NOTUSED)
{
  return sizeof (OdsMagNode);
}

static uint64_t
vtdb_btree_write_svc_size (void *x NOTUSED, BTreeNode * this)
{
  VtSvc *t = btreeNodePayload (this);
  debugMsg ("vtdb_btree_write_svc_size\n");
  return btreeWriteSize (t->mags, NULL,
                         vtdb_btree_write_magn_size) +
    btreeWriteSize (t->packets, NULL,
                    vtdb_btree_write_pk_size) + sizeof (uint8_t);
}

static uint64_t
vtdb_btree_write_pid_size (void *x NOTUSED, BTreeNode * this)
{
  VtPid *t = btreeNodePayload (this);
  debugMsg ("vtdb_btree_write_pid_size\n");
  return btreeWriteSize (t->svc, NULL,
                         vtdb_btree_write_svc_size) + sizeof (uint16_t);
}

static uint64_t
vtdb_btree_write_pidref_size (void *x NOTUSED, BTreeNode * this NOTUSED)
{
  debugMsg ("vtdb_btree_write_pidref_size\n");
  return sizeof (uint16_t);
}

static uint64_t
vtdb_btree_write_pnr_size (void *x NOTUSED, BTreeNode * this)
{
  VtPnr *t = btreeNodePayload (this);
  debugMsg ("vtdb_btree_write_pnr_size\n");
  return btreeWriteSize (t->pids, NULL,
                         vtdb_btree_write_pidref_size) + sizeof (uint16_t);
}

static uint64_t
vtdb_btree_write_tsid_size (void *x NOTUSED, BTreeNode * this)
{
  VtTsid *t = btreeNodePayload (this);
  debugMsg ("vtdb_btree_write_tsid_size\n");
  return btreeWriteSize (t->pids, NULL,
                         vtdb_btree_write_pid_size) + btreeWriteSize (t->pnrs,
                                                                      NULL,
                                                                      vtdb_btree_write_pnr_size)
    + sizeof (uint16_t);
}

static uint64_t
vtdb_btree_write_pos_size (void *x NOTUSED, BTreeNode * this)
{
  VtPos *t = btreeNodePayload (this);
  debugMsg ("vtdb_btree_write_pos_size\n");
  return btreeWriteSize (t->tsids, NULL,
                         vtdb_btree_write_tsid_size) + sizeof (uint32_t);
}

static FpSize
vtdb_get_index_size (VtDb * db)
{
  return btreeWriteSize (db->root, NULL, vtdb_btree_write_pos_size);
}

//------------------------------------------------static funcs, writing---------------------------------------------------
static void
vtdb_write_packet (VtPk * this, FILE * f)
{
  fwrite (this, sizeof (VtPk), 1, f);
}

static int
vtdb_sl_write_packet (void *x NOTUSED, SListE * this, FILE * f)
{
  vtdb_write_packet (slistEPayload (this), f);
  return 0;
}

static void
vtdb_write_page (VtPage * pg, FILE * f)
{
  fwrite (pg, sizeof (VtPage) - sizeof (SList), 1, f);
  slistWrite (&pg->packets, NULL, vtdb_sl_write_packet, f);
}

static int
vtdb_btree_write_page (void *x NOTUSED, BTreeNode * this, FILE * f)
{
  vtdb_write_page (btreeNodePayload (this), f);
  return 0;
}

static int
vtdb_btree_write_packet (void *x NOTUSED, BTreeNode * this, FILE * f)
{
  vtdb_write_packet (btreeNodePayload (this), f);
  return 0;
}


static FpPointer
vtdb_write_mag (VtMag * mag, FpHeap * h)
{
  FpSize sz;
  FpPointer rv;
  sz = vtdb_write_mag_size (mag);
  rv = fphCalloc (1, sz, h);
  if (rv)
  {
    debugPri (2, "Writing Mag %p\n", mag);
    //now write
    FP_GO (rv, h->f);
    vtdb_write_page (&mag->current_page, h->f);
    btreeWrite (mag->pages, NULL, vtdb_btree_write_page, h->f);
    btreeWrite (mag->packets, NULL, vtdb_btree_write_packet, h->f);
    vtdb_clear_mag (mag);
  }
  return rv;
}

static int
vtdb_btree_write_magn (void *x, BTreeNode * this, FILE * f)
{
  VtDb *db = x;
  OdsMagNode om;
  off_t pos;
  VtMagNode *mn = btreeNodePayload (this);
  FpPointer ptr;
  if (!mn->on_disk)
  {
    LRURemove (&db->mag_lru, &mn->m.head);
    pos = ftello (f);
    ptr = vtdb_write_mag (&mn->m, &db->h);
    FP_GO (pos, f);
    if (ptr)
    {
      mn->on_disk = 1;
      mn->odm.ts = time (NULL);
      mn->odm.p = ptr;
    }
    else
      return 1;
  }
  om.p = mn->odm.p;
  om.ts = mn->odm.ts;
  om.id = mn->id;
  fwrite (&om, sizeof (OdsMagNode), 1, f);
  return 0;
}

static int
vtdb_btree_write_svc (void *x, BTreeNode * this, FILE * f)
{
  VtSvc *p = btreeNodePayload (this);
  fwrite (&p->id, sizeof (uint8_t), 1, f);
  return (btreeWrite (p->mags, x, vtdb_btree_write_magn, f) ||
          btreeWrite (p->packets, x, vtdb_btree_write_packet, f));
}

static int
vtdb_btree_write_pid (void *x, BTreeNode * this, FILE * f)
{
  VtPid *p = btreeNodePayload (this);
  fwrite (&p->id, sizeof (uint16_t), 1, f);
  return btreeWrite (p->svc, x, vtdb_btree_write_svc, f);
}

static int
vtdb_btree_write_pidref (void *x NOTUSED, BTreeNode * this, FILE * f)
{
  uint16_t *t = btreeNodePayload (this);
  return (1 != fwrite (t, sizeof (uint16_t), 1, f));

}

static int
vtdb_btree_write_pnr (void *x, BTreeNode * this, FILE * f)
{
  VtPnr *t = btreeNodePayload (this);
  fwrite (&t->id, sizeof (uint16_t), 1, f);
  return btreeWrite (t->pids, x, vtdb_btree_write_pidref, f);
}

static int
vtdb_btree_write_tsid (void *x, BTreeNode * this, FILE * f)
{
  VtTsid *t = btreeNodePayload (this);
  fwrite (&t->id, sizeof (uint16_t), 1, f);
  return btreeWrite (t->pids, x, vtdb_btree_write_pid, f)
    || btreeWrite (t->pnrs, x, vtdb_btree_write_pnr, f);
}

static int
vtdb_btree_write_pos (void *x, BTreeNode * this, FILE * f)
{
  VtPos *t = btreeNodePayload (this);
  fwrite (&t->pos, sizeof (uint32_t), 1, f);
  return btreeWrite (t->tsids, x, vtdb_btree_write_tsid, f);
}

static int
vtdb_write_index_real (FpPointer od_root, VtDb * db)
{
  FP_GO (od_root, db->handle);
  return btreeWrite (db->root, db, vtdb_btree_write_pos, db->handle);
}

static int
vtdb_write_idx (VtDb * db)
{
  FpSize size_total;
  FpPointer od_root;
  FP_GO (0, db->h.f);
  fread (&od_root, 1, sizeof (FpPointer), db->h.f);
  if (od_root)                  //this shouldn't be. perhaps we should delete the file...
  {
    debugMsg ("idx root nonzero\n");
    fclose (db->handle);
    utlFAN (db->vbuf);
    return 1;
  }
  debugMsg ("vtdb_get_index_size\n");
  size_total = vtdb_get_index_size (db);
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
  if (vtdb_write_index_real (od_root, db))
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
vtdb_delete (VtDb * db)
{
  unlink (db->dbfilename);
}

static void
vtdb_btree_count_node (void *x, BTreeNode * this NOTUSED, int which,
                       int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    int *ctrp = x;
    *ctrp = (*ctrp) + 1;
  }
}

static void
vtdb_dump_magnode (void *x, BTreeNode * this NOTUSED, int which,
                   int depth NOTUSED)
{
  if ((which == 1) || (which == 3))     //inorder walk..
  {
    VtDb *db = x;
    VtMagNode *mn = btreeNodePayload (this);
    if (mn->on_disk)
    {
      vtdb_read_mag (db, &mn->m, mn->odm.p, &db->h);
      mn->on_disk = 0;
      LRUPutMid (&db->mag_lru, &mn->m.head);
    }
    LRUFwd (&db->mag_lru, &mn->m.head);
    mn->accessed = 1;
    debugPri (1, "\t\t\t\t\tMAGNODE: %p, %hu, size: %lu\n", mn, mn->id,
              (unsigned long) vtdb_write_mag_size (&mn->m));
  }
}


static void
vtdb_dump_svc (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 1) || (which == 3))     //inorder walk..
  {
    int ctr, ctrb;
    VtSvc *svc = btreeNodePayload (this);
    ctr = 0;
    btreeWalk (svc->packets, &ctr, vtdb_btree_count_node);
    ctrb = 0;
    btreeWalk (svc->mags, &ctrb, vtdb_btree_count_node);
    debugPri (1, "\t\t\t\tVTSVC: %hhu, %d glob packets, %d mags\n", svc->id,
              ctr, ctrb);
    btreeWalk (svc->mags, x, vtdb_dump_magnode);
  }
}

static void
vtdb_dump_pid (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 1) || (which == 3))     //inorder walk..
  {
    VtPid *pid = btreeNodePayload (this);
    debugPri (1, "\t\t\tVTPID: %hu\n", pid->id);
    btreeWalk (pid->svc, x, vtdb_dump_svc);
  }
}

/*
static void vtdb_dump_pnr(void * x,BTreeNode * this,int which,int depth)
{
	if((which==2)||(which==3))
	{
		VtPnr * pnr=btreeNodePayload(this);
		errMsg("\t\tVTPNR: %hu\n",pnr->id);
		btreeWalk(pnr->pids,NULL,vtdb_dump_pid);
	}
}
*/
static void
vtdb_dump_tsid (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 1) || (which == 3))     //inorder walk..
  {
    VtTsid *tsid = btreeNodePayload (this);
    debugPri (1, "\tVTTSID: %hu\n", tsid->id);
    btreeWalk (tsid->pids, x, vtdb_dump_pid);
  }
}

static void
vtdb_dump_pos (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 1) || (which == 3))     //inorder walk..
  {
    VtPos *tsid = btreeNodePayload (this);
    debugPri (1, "VTPOS: %hu\n", tsid->pos);
    btreeWalk (tsid->tsids, x, vtdb_dump_tsid);
  }
}

static void
vtdb_dump_stats (VtDb * db)
{
  btreeWalk (db->root, db, vtdb_dump_pos);
}

//-----------------------------------------static funcs, destructors--------------------------------------------------------
static void
vtdb_destroy_mag_node (void *x, BTreeNode * this, int which,
                       int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtDb *db = x;
    VtMagNode *magn;
    magn = btreeNodePayload (this);
    if (magn->on_disk)
      fphFree (magn->odm.p, &db->h);
    else
    {
      LRURemove (&db->mag_lru, &magn->m.head);
      vtdb_clear_mag (&magn->m);
    }
    utlFAN (this);
  }
}

static void
vtdb_destroy_svc (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtSvc *svc;
    svc = btreeNodePayload (this);
    btreeWalk (svc->mags, x, vtdb_destroy_mag_node);
    btreeWalk (svc->packets, x, vtdb_destroy_pack);
    utlFAN (this);
  }
}

#if 0
static void
vtdb_destroy_pid (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtPid *pid;
    pid = btreeNodePayload (this);
    btreeWalk (pid->svc, x, vtdb_destroy_svc);
    utlFAN (this);
  }
}
#endif

static void
vtdb_clean_pid (void *x NOTUSED, BTreeNode * this, int which,
                int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtPid *pid;
    pid = btreeNodePayload (this);
    btreeWalk (pid->svc, NULL, vtdb_clean_svc);
    utlFAN (this);
  }
}

static void
vtdb_clean_pidref (void *x NOTUSED, BTreeNode * this, int which,
                   int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    utlFAN (this);
  }
}

static void
vtdb_clean_pnr (void *x NOTUSED, BTreeNode * this, int which,
                int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtPnr *tsid;
    tsid = btreeNodePayload (this);
    btreeWalk (tsid->pids, NULL, vtdb_clean_pidref);
    utlFAN (this);
  }
}

static void
vtdb_clean_tsid (void *x NOTUSED, BTreeNode * this, int which,
                 int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtTsid *tsid;
    tsid = btreeNodePayload (this);
    btreeWalk (tsid->pids, NULL, vtdb_clean_pid);
    btreeWalk (tsid->pnrs, NULL, vtdb_clean_pnr);
    utlFAN (this);
  }
}

static void
vtdb_clean_pos (void *x NOTUSED, BTreeNode * this, int which,
                int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtPos *pos;
    pos = btreeNodePayload (this);
    btreeWalk (pos->tsids, NULL, vtdb_clean_tsid);
    utlFAN (this);
  }
}

static void
vtdb_clear_page (VtPage * page)
{
  SListE *e;
  while ((e = slistRemoveFirst (&page->packets)))
    utlFAN (e);
}

static void
vtdb_destroy_page (void *x NOTUSED, BTreeNode * this, int which,
                   int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtPage *page;
    page = btreeNodePayload (this);
    vtdb_clear_page (page);
    utlFAN (this);
  }
}

static void
vtdb_destroy_pack (void *x NOTUSED, BTreeNode * this, int which,
                   int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    utlFAN (this);
  }
}

static void
vtdb_clear_mag (VtMag * mag)
{
  vtdb_clear_page (&mag->current_page);
  btreeWalk (mag->pages, NULL, vtdb_destroy_page);
  btreeWalk (mag->packets, NULL, vtdb_destroy_pack);
  memset (&mag->current_page, 0, sizeof (VtPage));
  mag->current_page.subno = 0xffff;
  mag->pages = NULL;
  mag->packets = NULL;
}

static void
vtdb_clean_mag_node (void *x NOTUSED, BTreeNode * this, int which,
                     int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
//              VtMagNode * magn;
//              magn=btreeNodePayload(this);
//              vtdb_clear_magn(magn->m); we assume they're all on disk. This is only true after writing out the index.
    utlFAN (this);
  }
}

static void
vtdb_clean_svc (void *x NOTUSED, BTreeNode * this, int which,
                int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    VtSvc *svc;
    svc = btreeNodePayload (this);
    btreeWalk (svc->mags, NULL, vtdb_clean_mag_node);
    btreeWalk (svc->packets, NULL, vtdb_destroy_pack);
    utlFAN (this);
  }
}


//-----------------------------------------static funcs, node comparison---------------------------------------------------------
static int
vtdb_cmp_svc (void *x NOTUSED, const void *a, const void *b)
{
  VtSvc const *ap = a;
  VtSvc const *bp = b;
  return (bp->id < ap->id) ? -1 : (bp->id != ap->id);
}

static int
vtdb_cmp_pid (void *x NOTUSED, const void *a, const void *b)
{
  VtPid const *ap = a;
  VtPid const *bp = b;
  return (bp->id < ap->id) ? -1 : (bp->id != ap->id);
}

static int
vtdb_cmp_pos (void *x NOTUSED, const void *a, const void *b)
{
  VtPos const *ap = a;
  VtPos const *bp = b;
  return (bp->pos < ap->pos) ? -1 : (bp->pos != ap->pos);
}

static int
vtdb_cmp_tsid (void *x NOTUSED, const void *a, const void *b)
{
  VtTsid const *ap = a;
  VtTsid const *bp = b;
  return (bp->id < ap->id) ? -1 : (bp->id != ap->id);
}

static int
vtdb_cmp_pnr (void *x NOTUSED, const void *a, const void *b)
{
  VtPnr const *ap = a;
  VtPnr const *bp = b;
  return (bp->id < ap->id) ? -1 : (bp->id != ap->id);
}

static int
vtdb_cmp_pidref (void *x NOTUSED, const void *a, const void *b)
{
  uint16_t const *ap = a;
  uint16_t const *bp = b;
  return (*bp < *ap) ? -1 : (*bp != *ap);
}

static int
vtdb_cmp_mag (void *x NOTUSED, const void *a, const void *b)
{
  VtMagNode const *ap = a;
  VtMagNode const *bp = b;
  return (bp->id < ap->id) ? -1 : (bp->id != ap->id);
}

static int
vtdb_cmp_page (void *x NOTUSED, const void *a, const void *b)
{
  VtPage const *ap = a;
  VtPage const *bp = b;
  int rv;
  rv = (bp->pgno < ap->pgno) ? -1 : (bp->pgno != ap->pgno);
  if (!rv)
    rv = (bp->subno < ap->subno) ? -1 : (bp->subno != ap->subno);
  return rv;
}

static int
vtdb_cmp_pack (void *x NOTUSED, const void *a, const void *b)
{
  VtPk *ap = (VtPk *) a;
  VtPk *bp = (VtPk *) b;
  int xa, ya, exta;
  int xb, yb, extb;
  int rv;
  xa = get_pk_x (ap->data);
  ya = get_pk_y (ap->data);
  if ((ya <= 31) && (ya >= 26))
    exta = get_pk_ext (ap->data);
  else
    exta = 16;

  xb = get_pk_x (bp->data);
  yb = get_pk_y (bp->data);
  if ((yb <= 31) && (yb >= 26))
    extb = get_pk_ext (bp->data);
  else
    extb = 16;

  rv = (xb < xa) ? -1 : (xb != xa);
  if (!rv)
    rv = (yb < ya) ? -1 : (yb != ya);
  if (!rv)
    rv = (extb < exta) ? -1 : (extb != exta);
  return rv;
}

//--------------------------------------------------static funcs, node creation----------------------------------------------------
static VtTsid *
vtdb_get_or_ins_tsid (VtDb * db, uint16_t id)
{
  VtTsid cv;
  BTreeNode *n;
  cv.id = id;
  cv.pids = NULL;
  cv.pnrs = NULL;
  if (NULL == db->current_pos)
    return NULL;
  n =
    btreeNodeGetOrIns (&db->current_pos->tsids, &cv, NULL, vtdb_cmp_tsid,
                       sizeof (VtTsid));
  if (!n)
    return NULL;
  return btreeNodePayload (n);
}

static VtPos *
vtdb_get_or_ins_pos (VtDb * db, uint32_t pos)
{
  VtPos cv;
  BTreeNode *n;
  cv.pos = pos;
  cv.tsids = NULL;
  n = btreeNodeGetOrIns (&db->root, &cv, NULL, vtdb_cmp_pos, sizeof (VtPos));
  if (!n)
    return NULL;
  return btreeNodePayload (n);
}

static VtPnr *
vtdb_get_or_ins_pnr (VtTsid * tsid, uint16_t pnr)
{
  VtPnr cv;
  BTreeNode *n;
  cv.id = pnr;
  cv.pids = NULL;
  n =
    btreeNodeGetOrIns (&tsid->pnrs, &cv, NULL, vtdb_cmp_pnr, sizeof (VtPnr));
  if (!n)
    return NULL;
  return btreeNodePayload (n);
}

static uint16_t *
vtdb_get_or_ins_pidref (VtPnr * pn, uint16_t pid)
{
  BTreeNode *n;
  if (!pn)
    return NULL;
  n =
    btreeNodeGetOrIns (&pn->pids, &pid, NULL, vtdb_cmp_pidref,
                       sizeof (uint16_t));
  if (!n)
    return NULL;
  return btreeNodePayload (n);


}

static VtPid *
vtdb_get_or_ins_pid (VtTsid * tsid, VtPk * d)
{
  VtPid cv;
  BTreeNode *n;
  cv.id = d->pid;
  cv.svc = NULL;
  if (!tsid)
    return NULL;
  n =
    btreeNodeGetOrIns (&tsid->pids, &cv, NULL, vtdb_cmp_pid, sizeof (VtPid));
  if (!n)
    return NULL;
  return btreeNodePayload (n);
}

static VtSvc *
vtdb_get_or_ins_svc (VtPid * pid, VtPk * d)
{
  VtSvc cv;
  BTreeNode *n;
  cv.id = d->data_id;
  cv.mags = NULL;
  cv.packets = NULL;
  n = btreeNodeGetOrIns (&pid->svc, &cv, NULL, vtdb_cmp_svc, sizeof (VtSvc));
  if (!n)
    return NULL;
  return btreeNodePayload (n);
}


static VtMag *
vtdb_get_mag (VtDb * db, VtSvc * sv, uint8_t magno)
{
  VtMagNode cv;
  BTreeNode *n;
  VtMagNode *mn;
  cv.id = magno;
  if (cv.id == 0xff)
    return NULL;
  n = btreeNodeFind (sv->mags, &cv, NULL, vtdb_cmp_mag);
  if (!n)
    return NULL;

  mn = btreeNodePayload (n);
  if (mn->on_disk)
  {
    vtdb_read_mag (db, &mn->m, mn->odm.p, &db->h);
    mn->on_disk = 0;
    LRUPutMid (&db->mag_lru, &mn->m.head);
  }
  LRUFwd (&db->mag_lru, &mn->m.head);
  mn->accessed = 1;
  return &mn->m;
}


static VtMag *
vtdb_get_or_ins_mag (VtDb * db, VtSvc * sv, VtPk * d)
{
  VtMagNode cv;
  BTreeNode *n;
  VtMagNode *mn;

  memset (&cv, 0, sizeof (VtMagNode));  //so the SList starts empty(cv may be used for initialisation)
  cv.id = get_magno (d->data);
  if (cv.id == 0xff)
    return NULL;
  cv.m.current_page.subno = 0xffff;     //mark as unused if new
  n = btreeNodeFind (sv->mags, &cv, NULL, vtdb_cmp_mag);
  if (!n)
  {
    void *p;
    n = utlCalloc (btreeNodeSize (sizeof (VtMagNode)), 1);
    if (!n)
    {
      errMsg ("calloc failed\n");
      return NULL;
    }
    p = btreeNodePayload (n);
    memmove (p, &cv, sizeof (VtMagNode));
    btreeNodeInsert (&sv->mags, n, NULL, vtdb_cmp_mag);
    mn = btreeNodePayload (n);
//              selectorUnlock(&db->dvb->ts_selector);//so we don't delay the dvb thread to much
    debugPri (2, "Generating new Mag %p\n", &mn->m);
    vtdb_check_swapout (db);
//              selectorLock(&db->dvb->ts_selector);
    LRUPutMid (&db->mag_lru, &mn->m.head);
  }
  mn = btreeNodePayload (n);
  if (mn->on_disk)
  {
    db->misses++;
//              selectorUnlock(&db->dvb->ts_selector);//so we don't delay the dvb thread to much
    vtdb_read_mag (db, &mn->m, mn->odm.p, &db->h);
//              selectorLock(&db->dvb->ts_selector);
    mn->on_disk = 0;
    LRUPutMid (&db->mag_lru, &mn->m.head);
  }
  else
    db->hits++;
  mn->accessed = 1;
  return &mn->m;
}


static VtPage *
vtdb_get_or_ins_page (VtMag * db, uint8_t pgno, uint16_t subno)
{
  VtPage cv;
  BTreeNode *np;
  memset (&cv, 0, sizeof (VtPage));
  cv.pgno = pgno;
  cv.subno = subno;
  if ((cv.pgno == 0xff) || (cv.subno == 0xffff))
    return NULL;
//      errMsg("Page: %x%2.2x-%x\n",get_magno(d),cv.pgno,cv.subno);
  slistInit (&cv.packets);
  np =
    btreeNodeGetOrIns (&db->pages, &cv, NULL, vtdb_cmp_page, sizeof (VtPage));
  if (!np)
    return NULL;
  return btreeNodePayload (np);
}

static void
vtdb_new_or_replace_pack (BTreeNode ** root, VtPk * d)
{
  BTreeNode *n;
  uint8_t *p;
  int ins;
  n = btreeNodeFind (*root, d, NULL, vtdb_cmp_pack);
  if (!n)
  {
    ins = 1;
    n = utlCalloc (1, btreeNodeSize (EX_BUFSZ));
    if (!n)
      return;
  }
  p = btreeNodePayload (n);
  memmove (p, d, EX_BUFSZ);
  if (ins)
    btreeNodeInsert (root, n, NULL, vtdb_cmp_pack);
}

#if 0
//------------------------------------------------static funcs, aging------------------------------------------------
static void
vtdb_age_svc (VtDb * db, time_t now, BTreeNode ** root, VtSvc * svc)
{
  Iterator mag_iter;
  uint32_t i;
  if (iterBTreeInit (&mag_iter, svc->mags))
    return;

  if (iterGetSize (&mag_iter) == 0)
  {
    iterClear (&mag_iter);
    return;
  }
  for (i = 0; i < iterGetSize (&mag_iter); i++)
  {
    VtMagNode *magn;
    iterSetIdx (&mag_iter, i);
    magn = iterGet (&mag_iter);
    if (magn->on_disk)
    {
      if ((magn->odm.ts + 3600 * 24 * db->vtdb_days) < now)
      {
        fphFree (magn->odm.p, &db->h);
        btreeNodeRemove (&svc->mags, btreePayloadNode (magn));
        utlFree (btreePayloadNode (magn));
      }
    }
  }
  if (!svc->mags)
  {
    btreeWalk (svc->packets, NULL, vtdb_destroy_pack);
    btreeNodeRemove (root, btreePayloadNode (svc));
    utlFree (btreePayloadNode (svc));
  }
  iterClear (&mag_iter);
}

static void
vtdb_age_pid (VtDb * db, time_t now, BTreeNode ** root, VtPid * pid)
{
  Iterator svc_iter;
  uint32_t i;
  if (iterBTreeInit (&svc_iter, pid->svc))
    return;

  if (iterGetSize (&svc_iter) == 0)
  {
    iterClear (&svc_iter);
    return;
  }
  for (i = 0; i < iterGetSize (&svc_iter); i++)
  {
    VtSvc *svc;
    iterSetIdx (&svc_iter, i);
    svc = iterGet (&svc_iter);
    vtdb_age_svc (db, now, &pid->svc, svc);
  }
  if (!pid->svc)
  {
    btreeNodeRemove (root, btreePayloadNode (pid));
    utlFree (btreePayloadNode (pid));
  }
  iterClear (&svc_iter);
}

/*
static void vtdb_age_pnr(VtDb* db,time_t now,BTreeNode **root,VtPnr* pnr)
{
	Iterator pid_iter;
	uint32_t i;
	if(iterBTreeInit(&pid_iter,pnr->pids))
		return;
	
	if(iterGetSize(&pid_iter)==0)
	{
		iterClear(&pid_iter);
		return;
	}
	for(i=0;i<iterGetSize(&pid_iter);i++)
	{
		VtPid *pid;
		iterSetIdx(&pid_iter,i);
		pid=iterGet(&pid_iter);
		vtdb_age_pid(db,now,&pnr->pids,pid);
	}
	if(!pnr->pids)
	{
		btreeNodeRemove(root,btreePayloadNode(pnr));
		utlFAN(btreePayloadNode(pnr));
	}
	iterClear(&pid_iter);
}
*/

static void
vtdb_age_tsid (VtDb * db, time_t now, BTreeNode ** root, VtTsid * tsid)
{
  Iterator pid_iter;
  uint32_t i;
  if (iterBTreeInit (&pid_iter, tsid->pids))
    return;

  if (iterGetSize (&pid_iter) == 0)
  {
    iterClear (&pid_iter);
    return;
  }
  for (i = 0; i < iterGetSize (&pid_iter); i++)
  {
    VtPid *pid;
    iterSetIdx (&pid_iter, i);
    pid = iterGet (&pid_iter);
    vtdb_age_pid (db, now, &tsid->pids, pid);
  }
  if (!tsid->pids)              //FIXME: remove pnrs tree too!
  {
    btreeNodeRemove (root, btreePayloadNode (tsid));
    utlFree (btreePayloadNode (tsid));
  }
  iterClear (&pid_iter);
}

static void
vtdb_age_pos (VtDb * db, time_t now, BTreeNode ** root, VtPos * pos)
{
  Iterator tsid_iter;
  uint32_t i;
  if (iterBTreeInit (&tsid_iter, pos->tsids))
    return;

  if (iterGetSize (&tsid_iter) == 0)
  {
    iterClear (&tsid_iter);
    return;
  }
  for (i = 0; i < iterGetSize (&tsid_iter); i++)
  {
    VtTsid *tsid;
    iterSetIdx (&tsid_iter, i);
    tsid = iterGet (&tsid_iter);
    vtdb_age_tsid (db, now, &pos->tsids, tsid);
  }
  iterClear (&tsid_iter);
}

static void
vtdb_age (VtDb * db, time_t now)
{
  /*
     go through all magazines in database.
     If we find one to remove, remove it.
     if all nodes are gone, remove parent.

     Kind of unwieldy. There must be a better way to do this
   */
  Iterator pos_iter;
  uint32_t i;
  if (iterBTreeInit (&pos_iter, db->root))
    return;

  if (iterGetSize (&pos_iter) == 0)
  {
    iterClear (&pos_iter);
    return;
  }

  for (i = 0; i < iterGetSize (&pos_iter); i++)
  {
    VtPos *pos;

    iterSetIdx (&pos_iter, i);
    pos = iterGet (&pos_iter);
    vtdb_age_pos (db, now, &db->root, pos);
//              vtdb_age_tsid(db,now,&db->root,tsid);
  }
  iterClear (&pos_iter);
}
#endif


//---------------------------------------------static funcs, storage----------------------------------------

static VtMagNode *
lru_e_to_mag_node (LRUE * x)
{
  VtMagNode *rv;
  rv = (VtMagNode *) (((uint8_t *) x) - offsetof (VtMagNode, m.head));
  return rv;
}

static void
vtdb_swapout_all_mags (VtDb * db)
{
  LRUE *x;
  x = LRURemoveLast (&db->mag_lru);
  while (x)
  {
    VtMagNode *mn;
    FpPointer ptr;
    mn = lru_e_to_mag_node (x);
    ptr = vtdb_write_mag (&mn->m, &db->h);
    if (ptr)
    {
      mn->on_disk = 1;
      mn->odm.p = ptr;
      mn->odm.ts = time (NULL);
    }
    else
    {                           //this may be bad
      errMsg ("couldn't write mag\n");
      LRUPutMid (&db->mag_lru, x);
      return;
    }
    x = LRURemoveLast (&db->mag_lru);
  }
}


static void
vtdb_check_swapout (VtDb * db)
{
  int num;
  //first, see that we don't exceed our limit
  num = LRUGetSize (&db->mag_lru);
  while (num >= db->real_mag_limit)
  {                             //swap out least recently used
    LRUE *x;
    VtMagNode *mn;
    FpPointer ptr;
    x = LRURemoveLast (&db->mag_lru);
    mn = lru_e_to_mag_node (x);
    ptr = vtdb_write_mag (&mn->m, &db->h);
    if (ptr)
    {
      mn->on_disk = 1;
      mn->odm.p = ptr;
      mn->odm.ts = time (NULL);
      num--;
    }
    else
    {                           //this may be bad
      errMsg ("couldn't write mag\n");
      LRUPutMid (&db->mag_lru, x);
    }
  }
}

///if this returns 0, we store packet
static int
vtdb_check_packet (VtPk * d)
{
  uint8_t xa, ya, exta = 0, pgno;
  uint16_t subno;
  xa = get_pk_x (d->data);
  ya = get_pk_y (d->data);
  if ((ya <= 31) && (ya >= 26))
    exta = get_pk_ext (d->data);

  if ((xa == 0xff) || (ya == 0xff) || (exta == 0xff) || //parity error
      ((ya == 27) && (exta >= 8) && (exta <= 15)) ||    //use not currently defined
      ((ya == 28) && (exta == 2)) ||    //Scrambling?
      ((ya == 28) && (exta >= 5) && (exta <= 15)) ||    //use not currently defined
      ((ya == 29) && (exta >= 2) && (exta <= 3)) ||     //use not currently defined
      ((ya == 29) && (exta >= 5) && (exta <= 15)) ||    //use not currently defined
      ((xa >= 1) && (xa <= 3) && (ya == 30)) || //use not currently defined/data
      ((xa >= 5) && (xa <= 7) && (ya == 30)) || //use not currently defined/data
      ((xa == 4) && (ya == 30)) ||      //audiodescript
      ((xa == 8) && (ya == 30) && (exta >= 4) && (exta <= 15)) ||       //use not currently defined
      (ya == 31)                //use not currently defined/data
    )
    return 1;
  if ((!is_svc_glob (d->data)) && (!is_mag_glob (d->data))
      && is_pg_hdr (d->data))
  {
    pgno = get_pgno (d->data);
    subno = get_subno (d->data);
    if ((pgno == 0xff) || (subno == 0xffff))
      return 1;
  }
  return 0;
}

///check for identical y and dc and extract (one) matching element
static SListE *
vtdb_page_remove (VtPage * pg, VtPk * pk)
{
  SListE *e, *prev;
  prev = NULL;
  e = slistFirst (&pg->packets);
  while (e)
  {
    VtPk *pk2 = slistEPayload (e);
    int y = get_pk_y (pk->data);
    int dc = get_pk_ext (pk->data);
    int y2 = get_pk_y (pk2->data);
    int dc2 = get_pk_ext (pk2->data);
    if (y == y2)
    {
      if (y == 26)
      {
        if (dc == dc2)
          return slistRemoveNext (&pg->packets, prev);
      }
      else
        return slistRemoveNext (&pg->packets, prev);
    }
    prev = e;
    e = slistENext (e);
  }
  return NULL;
}

static void
vtdb_merge_page (VtPage * pg, VtPage * to_add)
{
  SListE *e;
//this better be faster
  pg->hdr = to_add->hdr;
  e = slistFirst (&to_add->packets);
  while (e)
  {
    VtPk *pk2 = slistEPayload (e);
    SListE *pr = vtdb_page_remove (pg, pk2);
    utlFAN (pr);
    e = slistENext (e);
  }

  e = slistRemoveFirst (&to_add->packets);
  while (e)
  {
    slistInsertLast (&pg->packets, e);
    e = slistRemoveFirst (&to_add->packets);
  }
}

//return true if all nibbles are 0-9
static int
is_bcd (unsigned v)
{
  size_t i;
  for (i = 0; i < sizeof (v); i++)
  {
    unsigned t = v >> (i * 8);
    if ((t & 0xf) > 9)
      return 0;
    if (((t >> 4) & 0xf) > 9)
      return 0;
  }
  return 1;
}

static void
vtdb_put (void *x, VtPk * d)
{
  VtDb *db = x;
  VtSvc *svc = NULL;
  if (!db->current_pos)
    return;
  if (!db->current_tsid)
    return;
  if (vtdb_check_packet (d))
    return;
  svc = vtdb_get_or_ins_svc (vtdb_get_or_ins_pid (db->current_tsid, d), d);
  debugPri (3, "vtdb_put\n");
  /*
     how to differentiate etween displayable pages and enh pages?
     bytes:
     6       7             8   9      10      11      12       13
     X/0
     header: pgno  pgno(tens)     S1   S2+C4  S3   S4+C5+C6  C7-C10   C11-C14

     X/1..X/25
     packets: ..hamm4/8 data...

     X/26 X/28 M/29
     enhdta: DC(xcept POP GPOP)  triplets...

     X/27/0..X/27/7
     pglink: DC  ...links...
     8/30

     subpages, displayable pages:
     no subpages: Mxx-0000
     subpages:Mxx-0001 0079 (BCD sequnce)

     data pages, not displayable:
     nnXs
     nn: last packet number(S4(MSB),S3(LSB))
     1-41(BCD)
     1-25=X/1 to X/25
     26-41= X/26/0 to X/26/15
     (it may be transmitted in fragments)
     X: update indicator(S2), incr on every change
     s: S1 subpage. first: 0, increments...(only one significant to sort by..)
   */
  if (svc)
  {
    if (is_svc_glob (d->data))
    {                           //insert packet
      debugPri (3, "is_svc_glob\n");
      vtdb_new_or_replace_pack (&svc->packets, d);
    }
    else
    {                           //get (or create)mag node
      VtMag *mag;
      debugPri (3, "else\n");
      mag = vtdb_get_or_ins_mag (db, svc, d);
      if (!mag)
        return;
      if (is_mag_glob (d->data))
      {
        debugPri (3, "is_mag_glob\n");
        vtdb_new_or_replace_pack (&mag->packets, d);
      }
      else
      {                         //get page node
        debugPri (3, "else\n");
        if (is_pg_hdr (d->data))
        {                       //find or insert page, set current
          VtPage *pg;
          debugPri (3, "is_pg_hdr\n");
          if (mag->current_page.subno != 0xffff)        //set previous page completed and copy
          {
            unsigned nn = (mag->current_page.subno >> 8) & 0xff;        //for non-displ, last packet
            unsigned sp = mag->current_page.subno & 0xff;       //for displayables, subpage
            debugPri (3, "mag->current_page\n");
            if ((NULL == slistFirst (&(mag->current_page.packets))) ||
                (mag->current_page.pgno == 0xff))
            {
              //just a header... ignore
              //also remove 0xff pages(time filling)
              vtdb_clear_page (&mag->current_page);
              mag->current_page.subno = 0xffff;
            }
            else if ((nn >= 1) && (nn <= 41))
            {                   //non-displayable enhancement data page
              debugPri (3, "enhancement data\n");
              //this may be a partial page which has to be merged with one already stored
              //the db subnumber is the least significant nibble, as the other data is auxiliary info
              pg = vtdb_get_or_ins_page (mag, mag->current_page.pgno, (mag->current_page.subno & 0x0f));        //id by last nibble(subpage)
              if (pg)
              {
                unsigned int x =
                  ((mag->current_page.subno >> 4) & 7), prev_x =
                  hamm84_r (pg->hdr.data[7]);
                if ((has_c4 (mag->current_page.hdr.data)) || ((prev_x < 8) && (prev_x != x)))   //get X(S2, from header directly) and see if it changed between pg in db and current one..
                {               //new version... update
                  debugPri (3, "upd enh pg\n");
                  btreeNodeRemove (&mag->pages, btreePayloadNode (pg));
                  vtdb_clear_page (pg);
                  mag->current_page.subno &= 0x0f;
                  memmove (pg, &mag->current_page, sizeof (VtPage));
                  btreeNodeInsert (&mag->pages, btreePayloadNode (pg), NULL,
                                   vtdb_cmp_page);
                }
                else
                {
                  debugPri (3, "mrg enh pg\n");
                  vtdb_merge_page (pg, &mag->current_page);     //replace packets with same y/dc fields
                }
                memset (&mag->current_page, 0, sizeof (VtPage));
              }
              else
                vtdb_clear_page (&mag->current_page);
              mag->current_page.subno = 0xffff;
            }
            else if (is_bcd (sp) && (nn == 0) && (sp <= 0x79))
            {                   //displayable
              //find or create new page

              pg =
                vtdb_get_or_ins_page (mag, mag->current_page.pgno,
                                      mag->current_page.subno);
              if (pg)
              {
                debugPri (3, "pg\n");
                vtdb_clear_page (pg);
                memmove (pg, &mag->current_page, sizeof (VtPage));
              }
              else
                vtdb_clear_page (&mag->current_page);
              memset (&mag->current_page, 0, sizeof (VtPage));
              mag->current_page.subno = 0xffff;
            }
            else
            {                   //too strange to handle
              vtdb_clear_page (&mag->current_page);
              mag->current_page.subno = 0xffff;
            }
          }
          mag->current_page.subno = get_subno (d->data);
          if (mag->current_page.subno != 0xffff)
          {
            mag->current_page.pgno = get_pgno (d->data);
            memmove (&mag->current_page.hdr, d, sizeof (VtPk));
          }
        }
        else
        {                       //attach to current page
          if (mag->current_page.subno != 0xffff)
          {
            SListE *e;
            uint8_t *p;
            debugPri (3, "mag->current_page\n");
            e = utlCalloc (1, slistESize (EX_BUFSZ));
            if (e)
            {
              p = slistEPayload (e);
              memmove (p, d, EX_BUFSZ);
              slistInsertLast (&(mag->current_page.packets), e);
            }
          }
//                                      else errMsg("discarding pageless packet x: %x, y: %x, ext: %x\n",get_pk_x(d),get_pk_y(d),get_pk_ext(d));
        }
      }
    }
  }
}

/*
when we see a tuning event stop acquisition
when we see a tuned event, set up acquisition
when we see a pnr event, set up acquisition
when we see a config event, clear the whole database
*/

static void
vtdb_reconfig (VtDb * db, uint32_t NOTUSED num_pos)
{
  debugMsg ("Switches were reconfigured, purging vtdb\n");
  btreeWalk (db->root, NULL, vtdb_clean_pos);   //just free structures in memory, lru will hold dead ptrs afterwards, don't use it.
  db->root = NULL;
  debugMsg ("clearing File\n");
  fphZero (&db->h);
  db->current_pos = NULL;
  db->current_tsid = NULL;

  utlFAN (db->pnrs);
  db->pnrs = NULL;
  db->num_pnrs = 0;
  dvbDmxOutMod (db->o, 0, NULL, NULL, 0, NULL);
//  selectorModPort (db->input, 0,NULL,NULL,0,NULL);
  vtExFlush (&db->vt_ex);

  db->current_pos_idx = 0;
  db->current_freq = 0;
  db->current_pol = 0;

  LRUInit (&db->mag_lru);
}

void
vtdb_enter_pidref (VtDb * db, uint16_t pnr)
{
  ES *stream_info = NULL;
  int num_streams;
  uint16_t pcr_pid = 0x1fff;
  VtPnr *pn;
  int j;
  if (NULL == db->current_tsid)
    return;
  stream_info =
    pgmdbListStreams (db->pgmdb, db->current_pos_idx, db->current_freq,
                      db->current_pol, pnr, &pcr_pid, &num_streams);
  if (stream_info)
  {
    pn = vtdb_get_or_ins_pnr (db->current_tsid, pnr);
    if (pn)
    {
      for (j = 0; j < num_streams; j++)
      {
        if (stream_info[j].txt)
        {
          vtdb_get_or_ins_pidref (pn, stream_info[j].pid);
        }
      }
    }
    clear_es_infos (stream_info, num_streams);
    utlFAN (stream_info);
  }
}

static uint16_t *
vtdb_get_tuned_vtpids (VtDb * db, uint16_t * num_ret)
{
  ES *stream_info = NULL;
  uint16_t num_pids = 0;
  uint16_t *pidbuf = NULL;
  uint16_t pcr_pid;
  int num_pgms = db->num_pnrs;
  int num_streams;
  int i;
  for (i = 0; i < num_pgms; i++)
  {
    int j;
    stream_info =
      pgmdbListStreams (db->pgmdb, db->current_pos_idx, db->current_freq,
                        db->current_pol, db->pnrs[i], &pcr_pid, &num_streams);
    if (stream_info)
    {
      for (j = 0; j < num_streams; j++)
      {
        if (stream_info[j].txt)
        {
          pidbufAddPid (stream_info[j].pid, &pidbuf, &num_pids);
        }
      }
      clear_es_infos (stream_info, num_streams);
      utlFAN (stream_info);
    }
    else
      errMsg ("couldn't list streams\n");
  }
  *num_ret = num_pids;
  return pidbuf;
}

static void
vtdb_add_pids (VtDb * db, uint16_t pnr)
{
  uint16_t *funcs;
  int i;
  pidbufAddPid (pnr, &db->pnrs, &db->num_pnrs);
//  pidbuf = vtdb_get_tuned_vtpids (db, &num_pids);
  funcs = utlCalloc (db->num_pnrs, sizeof (uint16_t));
  for (i = 0; i < db->num_pnrs; i++)
    funcs[i] = FUNC_VT;
  dvbDmxOutMod (db->o, db->num_pnrs, db->pnrs, funcs, 0, NULL);
//  selectorModPort (db->input, 0,NULL,NULL,num_pids,pidbuf);
  utlFAN (funcs);
  db->real_mag_limit = db->mag_limit + 8 * db->num_pnrs;
  vtdb_check_swapout (db);
}

static void
vtdb_rmv_pids (VtDb * db, uint16_t pnr)
{
  uint16_t *funcs;
  int i;
  pidbufRmvPid (pnr, &db->pnrs, &db->num_pnrs);
//  pidbuf = vtdb_get_tuned_vtpids (db, &num_pids);
  funcs = utlCalloc (db->num_pnrs, sizeof (uint16_t));
  for (i = 0; i < db->num_pnrs; i++)
    funcs[i] = FUNC_VT;
  dvbDmxOutMod (db->o, db->num_pnrs, db->pnrs, funcs, 0, NULL);
//  selectorModPort ( db->input, 0,NULL,NULL,num_pids,pidbuf);
  vtExFlush (&db->vt_ex);
  utlFAN (funcs);
  db->real_mag_limit = db->mag_limit + 8 * db->num_pnrs;
  vtdb_check_swapout (db);
}

static uint16_t *
vtdb_get_all_vtpids (VtDb * db, uint16_t * num_ret)     //VtEx *ex,uint16_t *num_ret,uint32_t pos,uint32_t freq,uint8_t pol)
{
  ES *stream_info = NULL;
  uint16_t num_pids = 0;
//      uint16_t pidbuf_alloc=0;
  uint16_t *pidbuf = NULL;
  uint16_t pcr_pid;
  int num_pgms;
  int num_streams;
  int i;
  ProgramInfo *pi =
    pgmdbListPgm (db->pgmdb, db->current_pos_idx, db->current_freq,
                  db->current_pol, &num_pgms);
  for (i = 0; i < num_pgms; i++)
  {
    int j;
    stream_info =
      pgmdbListStreams (db->pgmdb, db->current_pos_idx, db->current_freq,
                        db->current_pol, pi[i].pnr, &pcr_pid, &num_streams);
    if (stream_info)
    {
      for (j = 0; j < num_streams; j++)
      {
        if (stream_info[j].txt)
        {
          pidbufAddPid (stream_info[j].pid, &pidbuf, &num_pids);
        }
      }
      clear_es_infos (stream_info, num_streams);
      utlFAN (stream_info);
    }
  }
  for (i = 0; i < num_pgms; i++)
    programInfoClear (&pi[i]);
  utlFAN (pi);
  *num_ret = num_pids;
  return pidbuf;
}

static void
vtdb_add_all_pids (VtDb * db)
{
  uint16_t *funcs;
  int num_pgms;
  int i;
  ProgramInfo *pi =
    pgmdbListPgm (db->pgmdb, db->current_pos_idx, db->current_freq,
                  db->current_pol, &num_pgms);
  vtdb_swapout_all_mags (db);
  for (i = 0; i < num_pgms; i++)
  {
    pidbufAddPid (pi[i].pnr, &db->pnrs, &db->num_pnrs);
    vtdb_enter_pidref (db, pi[i].pnr);
  }
  for (i = 0; i < num_pgms; i++)
    programInfoClear (&pi[i]);
  utlFAN (pi);
//  pidbuf = vtdb_get_all_vtpids (db, &num_pids);
  funcs = utlCalloc (db->num_pnrs, sizeof (uint16_t));
  for (i = 0; i < db->num_pnrs; i++)
    funcs[i] = FUNC_VT;
  dvbDmxOutMod (db->o, db->num_pnrs, db->pnrs, funcs, 0, NULL);
//  selectorModPort (db->input, 0,NULL,NULL,num_pids,pidbuf);
  utlFAN (funcs);
  db->real_mag_limit = db->mag_limit + 8 * db->num_pnrs;
  vtdb_check_swapout (db);
}

static void
vtdb_proc_evt (VtDb * db, SelBuf * d)
{
  uint32_t pos_dta;
  TuneDta *tda;
  uint16_t pda;
  int32_t tsid;
  debugMsg ("processing event\n");
//      selectorUnlock(&db->dvb->ts_selector);

  switch (selectorEvtId (d))
  {
  case E_TUNING:
    debugMsg ("E_TUNING\n");
    db->current_pos = NULL;
    db->current_tsid = NULL;
    utlFAN (db->pnrs);
    db->pnrs = NULL;
    db->num_pnrs = 0;
    dvbDmxOutMod (db->o, 0, NULL, NULL, 0, NULL);
//    selectorModPort (db->input, 0,NULL,NULL,0,NULL);
    vtExFlush (&db->vt_ex);
    break;
  case E_IDLE:
    debugMsg ("E_IDLE\n");
    db->current_pos = NULL;
    db->current_tsid = NULL;
    utlFAN (db->pnrs);
    db->pnrs = NULL;
    db->num_pnrs = 0;
    dvbDmxOutMod (db->o, 0, NULL, NULL, 0, NULL);
//    selectorModPort (db->input, 0,NULL,NULL,0,NULL);
    vtExFlush (&db->vt_ex);
    break;
  case E_TUNED:                //tuning successful
    debugMsg ("E_TUNED\n");
    tda = (TuneDta *) selectorEvtDta (d);
    db->current_pos_idx = tda->pos;
    db->current_freq = tda->freq;
    db->current_pol = tda->pol;
    db->current_pos = vtdb_get_or_ins_pos (db, tda->pos);
    if (db->current_pos)
    {
      tsid = pgmdbGetTsid (db->pgmdb, tda->pos, tda->freq, tda->pol);
      if (tsid >= 0)
      {
        db->current_tsid = vtdb_get_or_ins_tsid (db, tsid);
      }
    }
    break;
  case E_CONFIG:               //reconfigured
    debugMsg ("E_CONFIG\n");
    pos_dta = selectorEvtDta (d)->num_pos;
    vtdb_reconfig (db, pos_dta);
    break;
  case E_PNR_ADD:
    debugMsg ("E_PNR_ADD\n");
    pda = selectorEvtDta (d)->pnr;
    vtdb_enter_pidref (db, pda);
    vtdb_add_pids (db, pda);
    break;
  case E_PNR_RMV:
    debugMsg ("E_PNR_RMV\n");
    pda = selectorEvtDta (d)->pnr;
    vtdb_rmv_pids (db, pda);
    break;
  case E_ALL_VT:
    debugMsg ("E_ALL_VT\n");
    vtdb_add_all_pids (db);
    break;
  default:
    break;
  }
//      selectorLock(&db->dvb->ts_selector);
}

static void *
vtdb_store_loop (void *p)
{
  VtDb *db = p;
  int rv;                       //,i;
//      time_t now,then;
  rv = nice (db->niceness);
  debugPri (1, "nice returned %d\n", rv);
//      then=time(NULL);
//      i=0;
  taskLock (&db->t);
  while (taskRunning (&db->t))
  {

    taskUnlock (&db->t);
    rv = selectorWaitData (db->input, 300);
    taskLock (&db->t);
    if (rv)
    {
//                              debugMsg("secExWaitData failed\n");
    }
    else
    {
      void *d[256];             //TODO: number should probably be configurable
      int pkctr = 0;
      int skip = 0;             //if we saw event, skip data processing so packets don't end up in wrong tsids
      int ctr2;
      selectorLock (svtOutSelector (db->o));    // (&db->dvb->ts_selector);
      while ((pkctr < 256)
             && (d[pkctr] = selectorReadElementUnsafe (db->input)))
      {
        pkctr++;
      }
      selectorUnlock (svtOutSelector (db->o));

      for (ctr2 = 0; ctr2 < pkctr; ctr2++)
      {
        if (selectorEIsEvt (d[ctr2]))
        {
          vtdb_proc_evt (db, (SelBuf *) d[ctr2]);
          skip = 1;             //most events reinit selector and change db state, so we skip remaining data packets
        }
        else if (!skip)
        {
          PesBuf const *pb;
          pb = pesExPut (&db->pes_ex, selectorEPayload (d[ctr2]));
          if (pb)
          {
            vtExPut (&db->vt_ex, pb->data, pb->pid, pb->len, pb->start);
          }
        }
      }
      selectorLock (svtOutSelector (db->o));
      for (ctr2 = 0; ctr2 < pkctr; ctr2++)
      {
        selectorReleaseElementUnsafe (svtOutSelector (db->o), d[ctr2]);
      }
      selectorUnlock (svtOutSelector (db->o));
    }
/*		i++;can we do this, so we don't have to go through _all_ elements
		if(i>100000)
		{
			now=time(NULL);
			if(now>(then+3600))
			{
				vtdb_age(db,now);
				then=now;
			}
			i=0;
		}*/
  }

  taskUnlock (&db->t);
  return NULL;
}

//---------------------------------------------static funcs, retrieval----------------------------------------------
static VtPk *
vtdb_collect_packets (NOTUSED VtDb * db, BTreeNode * root,
                      uint16_t * num_pk_ret)
{
  Iterator pk_iter;
  VtPk *rv;
  uint32_t i;
  if (iterBTreeInit (&pk_iter, root))
  {
    *num_pk_ret = 0;
    return NULL;
  }
  if (iterGetSize (&pk_iter) == 0)
  {
    *num_pk_ret = 0;
    iterClear (&pk_iter);
    return NULL;
  }
  rv = utlCalloc (iterGetSize (&pk_iter), EX_BUFSZ);
  if (!rv)
  {
    *num_pk_ret = 0;
    iterClear (&pk_iter);
    return NULL;
  }
  for (i = 0; i < iterGetSize (&pk_iter); i++)
  {
    iterSetIdx (&pk_iter, i);
    memmove (rv + i, iterGet (&pk_iter), EX_BUFSZ);
  }
  *num_pk_ret = iterGetSize (&pk_iter);
  iterClear (&pk_iter);
  return rv;
}

static VtSvc *
vtdb_lookup_svc (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
                 uint8_t data_id)
{
  return
    vtdb_find_svc (vtdb_btree_find_pid
                   (vtdb_btree_find_tsid
                    (vtdb_btree_find_pos (db, pos), tsid), pid), data_id);
}

static VtSvc *
vtdb_find_svc (VtPid * root, uint8_t data_id)
{
  BTreeNode *rv;
  VtSvc cv;
  if (NULL == root)
    return NULL;
  cv.id = data_id;
  rv = btreeNodeFind (root->svc, &cv, NULL, vtdb_cmp_svc);
  if (NULL == rv)
    return NULL;
  return btreeNodePayload (rv);
}

static VtTsid *
vtdb_btree_find_tsid (VtPos * pp, uint16_t tsid)
{
  BTreeNode *rv;
  VtTsid cv;
  if (NULL == pp)
    return NULL;
  cv.id = tsid;
  rv = btreeNodeFind (pp->tsids, &cv, NULL, vtdb_cmp_tsid);
  if (NULL == rv)
    return NULL;
  return btreeNodePayload (rv);
}

static VtPos *
vtdb_btree_find_pos (VtDb * db, uint32_t pos)
{
  BTreeNode *rv;
  VtPos cv;
  cv.pos = pos;
  if (NULL == db)
    return NULL;
  rv = btreeNodeFind (db->root, &cv, NULL, vtdb_cmp_pos);
  if (NULL == rv)
    return NULL;
  return btreeNodePayload (rv);
}

static VtPnr *
vtdb_btree_find_pnr (VtTsid * root, uint16_t pnr)
{
  BTreeNode *rv;
  VtPnr cv;
  cv.id = pnr;
  if (NULL == root)
    return NULL;
  rv = btreeNodeFind (root->pnrs, &cv, NULL, vtdb_cmp_pnr);
  if (NULL == rv)
    return NULL;
  return btreeNodePayload (rv);
}


static VtPid *
vtdb_btree_find_pid (VtTsid * root, uint16_t pid)
{
  BTreeNode *rv;
  VtPid cv;
  cv.id = pid;
  if (NULL == root)
    return NULL;
  rv = btreeNodeFind (root->pids, &cv, NULL, vtdb_cmp_pid);
  if (NULL == rv)
    return NULL;
  return btreeNodePayload (rv);
}

int
vtdbIdle (VtDb * db)
{
  int rv;
  taskLock (&db->t);
  rv = (NULL == db->current_pos);
  taskUnlock (&db->t);
  return rv;
}
