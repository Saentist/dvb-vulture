#include <string.h>
#include <inttypes.h>
#include "svt.h"
#include "utl.h"
#include "pidbuf.h"
#include "rcptr.h"
#include "assert.h"
#include "debug.h"

int DF_SVT;
#define FILE_DBG DF_SVT
static SvcTrk *find_or_new_svt (SwDmx * d, uint16_t pnr);
static uint16_t *get_out_pids (SvcOut * o, uint16_t * n_pids_r);

static int
includes_stream (uint16_t func, ES * s)
{
  if (func == FUNC_ALL)
  {
    return 1;
  }
  if (func & FUNC_VT)
  {
    if (s->txt)
      return 1;
  }
  if (func & FUNC_SPU)
  {
    if (s->sub)
      return 1;
  }
  if (func & FUNC_AUDIO)
  {
    uint8_t t = s->stream_type;
    if ((t == 3) || (t == 4) || (t == 0xf) || (t == 0x11))
      return 1;
  }
  if (func & FUNC_VIDEO)
  {
    uint8_t t = s->stream_type;
    if ((t == 1) || (t == 2) || (t == 0x10))
      return 1;
  }
  return 0;
}

int
includes_pcr (uint16_t func)
{
  return (func & FUNC_AUDIO) || (func & FUNC_VIDEO);
}

struct funccs
{
  uint16_t func;
  uint16_t n_pids;
  uint16_t *pids;
};

void
get_pms_func (void *x, PMS * sec)
{
  struct funccs *fc = x;
  int i;
  uint16_t pcr_pid = 0x1fff;
  pcr_pid = sec->pcr_pid;
  for (i = 0; i < sec->num_es; i++)
  {
    if (includes_stream (fc->func, &sec->stream_infos[i]))
    {
      pidbufAddPid (sec->stream_infos[i].pid, &fc->pids, &fc->n_pids);
    }
  }
  if (includes_pcr (fc->func) && (pcr_pid != 0x1fff))
  {
    pidbufAddPid (pcr_pid, &fc->pids, &fc->n_pids);
  }
}

//n_pids_ret and pids_ret have to be already initialized
static void
collect_pmt_pids (uint16_t ** pids_ret, uint16_t * n_pids_ret, uint16_t func,
                  Table * pmt)
{
  struct funccs fc = { func, *n_pids_ret, *pids_ret };
  for_each_pms (pmt, &fc, get_pms_func);
  *pids_ret = fc.pids;
  *n_pids_ret = fc.n_pids;
}

/**
 *try to get pids for func. may fail if no pmts available.
 */
static void
get_pids_for_func (uint16_t * n_pids, uint16_t ** pids, SvcTrk * t,
                   uint16_t func)
{
  debugMsg ("collect_pmt_pids. func: 0x%" PRIx16 "\n", func);
  if (t->pmt[0])
  {
    debugMsg ("collect_pmt_pids for pmt.\n");
    collect_pmt_pids (pids, n_pids, func, t->pmt[0]);
  }
  if (t->pmt[1])
  {
    debugMsg ("collect_pmt_pids for pmt_next.\n");
    collect_pmt_pids (pids, n_pids, func, t->pmt[1]);
  }
  if (func & FUNC_PSI)
  {
    pidbufAddPid (0, pids, n_pids);
    pidbufAddPid (17, pids, n_pids);
    if (t->pid != 0x1fff)
      pidbufAddPid (t->pid, pids, n_pids);
  }
}




/*
add indicated functions to svc tracker
may have to associate tracker if it's empty...
selector is not locked
*/
void
add_func (SvcOut * o, uint16_t pnr, uint16_t func)
{
  unsigned i;
  debugMsg ("add_func o=%p pnr=%hu func=0x%hx\n", o, pnr, func);
  /*
     tracker may be new, pids unknown, what to do?
     will have to defer
   */
  for (i = 0; i < o->n_funcs; i++)
  {
    if (o->r[i].pnr == pnr)
    {
      break;
    }
  }
  if (i >= o->n_funcs)
  {
    FuncRec r;
    unsigned s = o->n_funcs;
    r.pnr = pnr;
    r.func = func;
    /*
       svt may already exist, but is not associated with this svcOut
       may have to create new svt
       if new, will not know any pids. starts out empty
       listening to pat or perhaps grabbing one from pgmdb
       (rather not as it is blocking operation...)
     */
    debugMsg ("looking for tracker\n");
    r.trk = find_or_new_svt (o->dmx, pnr);      //refcount gets incremented here
    //append an element
    utlAppend1 ((uint8_t **) & o->r, &o->n_funcs, &s, (uint8_t *) & r,
                sizeof (r));
    //associate the other way around
    s = r.trk->n_out;
    utlAppend1 ((uint8_t **) & r.trk->output, &r.trk->n_out, &s,
                (uint8_t *) & o, sizeof (o));
    debugMsg ("add_func done\n");
    return;
  }
  /*
     here, svt already exists
     and is associated 
     it may already know all pids,
     and may be able to supply us with them later ..
   */
  o->r[i].func = func;
  debugMsg ("add_func done2\n");
  return;
}

/*
selector must not be locked
*/
static void
svt_clear (SvcTrk * t)
{
  debugMsg ("svt_clear \n");
  assert (t->n_out == 0);
  selectorRemovePort (t->input);
  t->input = NULL;
  utlFAN (t->output);
  clear_tbl (t->pmt[0]);
  utlFAN (t->pmt[0]);
  clear_tbl (t->pmt[1]);
  utlFAN (t->pmt[1]);
  debugMsg ("svt_clear done\n");
}

/*
selector must not be locked

*/
static void
svt_release (SwDmx * d, SvcTrk * t)
{
  debugMsg ("svt_release d=%p t=%p\n", d, t);
  if (1 == rcptrRefcount (t))
  {
    debugMsg ("removing tracker\n");
    selectorLock (&d->s);
    secExClear (&t->ex);        //selector has to be locked here
    selectorUnlock (&d->s);
    btreeNodeRemove (&d->svt_root, &t->n);
    svt_clear (t);
  }
  rcptrRelease (t);
  debugMsg ("svt_release done\n");
}

/*selector must not be locked*/
static void
remove_tracker (SvcOut * o, unsigned i)
{
  FuncRec *r = &o->r[i];
  SvcTrk *t = r->trk;

  debugMsg ("remove_tracker o=%p i=%u\n", o, i);

  utlRmv ((uint8_t *) o->r, &o->n_funcs, sizeof (*r), i);
  for (i = 0; i < t->n_out; i++)
    if (t->output[i] == o)
      break;
  assert (i < t->n_out);
  utlRmv ((uint8_t *) t->output, &t->n_out, sizeof (t->output[0]), i);
  svt_release (o->dmx, t);
  debugMsg ("remove_tracker done\n");
}

/*
remove indicated functions from port
does not remove corresponding pids from output port
has to be done later.
may have to modify tracker and dissociate it, if 
it's not used anymore by this port

selector is not locked
*/
void
rmv_func (SvcOut * o, uint16_t pnr, uint16_t func)
{
  unsigned i;
  /*
     remove func from out port
     iterate over SvcOut s FuncRec array
   */
  debugMsg ("rmv_func o=%p pnr=%hu func=0x%hx\n", o, pnr, func);
  for (i = 0; i < o->n_funcs; i++)
  {
    if (o->r[i].pnr == pnr)
    {
      break;
    }
  }
  if (i >= o->n_funcs)
  {
    errMsg ("pnr not found: 0x%" PRIx16 ",0x%" PRIx16 "\nin SvcOut:%p\n", pnr,
            func, o);
    return;
  }
  /*
     clear func bits in svcout
     if it gets zero, remove tracker
   */
  o->r[i].func &= ~func;        //clear func
  if (o->r[i].func == 0)
  {
    remove_tracker (o, i);
  }

}

static int
get_idx (uint16_t * array, uint16_t sz, uint16_t e)
{
  int i;
  for (i = 0; i < sz; i++)
  {
    if (array[i] == e)
      return i;
  }
  return i;
}

///figure out set of funcs/services to remove when transitioning from set a to set b
static uint16_t
get_funcs_to_remove (uint16_t ** f_rmv_ret, uint16_t ** p_rmv_ret,
                     uint16_t * funcs_a, uint16_t * pnrs_a,
                     uint16_t n_funcs_a, uint16_t * funcs_b,
                     uint16_t * pnrs_b, uint16_t n_funcs_b)
{
  uint16_t n_rmv = 0;
  uint16_t *f_rmv = NULL;
  uint16_t *p_rmv = NULL;
  unsigned i;
  /*
     those pnrs that disappear completely,
     get removed with those func bits set in the a set(not necesarily all)

     those pnrs that stay but change func bits get removed when bits set in a set and cleared in b set.
     func[i]=funcs_a&(~funcs_b);

     perhaps flatten the whole function list, so it's easier to do?
     __________
     |pnr|func|
     ----------
     instead of func bitfield, func idx combined with pnr in 1 word...
     so each element in array corresponds to exactly 1 func?
     the pnr is a full 16 bits, so would have up word size.
     would it break stuff somewhere else? would not be able to use
     pidbuf funcs...

     naaahhh
   */

  //start out with complete removals
  p_rmv = pidbufDifference (pnrs_a, n_funcs_a, pnrs_b, n_funcs_b, &n_rmv);
  //got the pnrs, now find the funcs they had..
  f_rmv = utlCalloc (n_rmv, sizeof (uint16_t));
  for (i = 0; i < n_rmv; i++)
  {
    int j = get_idx (pnrs_a, n_funcs_a, p_rmv[i]);      //lookup pnrs index in a array
    f_rmv[i] = funcs_a[j];      //copy func bits (they all get removed)
  }

  //now those pnrs that are in both arrays
  for (i = 0; i < n_funcs_a; i++)
  {                             //iterate over a array and see if it's in b, too
    if (pidbufContains (pnrs_a[i], pnrs_b, n_funcs_b))
    {                           //yes it is
      int j = get_idx (pnrs_b, n_funcs_b, pnrs_a[i]);
      uint16_t tmp;
      if ((tmp = (funcs_a[i] & (~funcs_b[j]))))
      {                         //function bits got unset
        pidbufAddPid (pnrs_a[i], &p_rmv, &n_rmv);
        n_rmv--;                //'cause it got incremented above.. needs to get less ugly...
        pidbufAddPid (tmp, &f_rmv, &n_rmv);
      }
      //otherwise something got added
    }
  }
  *f_rmv_ret = f_rmv;
  *p_rmv_ret = p_rmv;
  return n_rmv;
}

///figure out set of funcs/services to add when transitioning from set a to set b
static uint16_t
get_funcs_to_add (uint16_t ** f_add_ret, uint16_t ** p_add_ret,
                  uint16_t * funcs_a, uint16_t * pnrs_a, uint16_t n_funcs_a,
                  uint16_t * funcs_b, uint16_t * pnrs_b, uint16_t n_funcs_b)
{
  uint16_t n_add = 0;
  uint16_t *f_add = NULL;
  uint16_t *p_add = NULL;
  unsigned i;
  //start out with newly added pnrs
  p_add = pidbufDifference (pnrs_b, n_funcs_b, pnrs_a, n_funcs_a, &n_add);
  //got the pnrs, now find the funcs they will have..
  f_add = utlCalloc (n_add, sizeof (uint16_t));
  for (i = 0; i < n_add; i++)
  {
    int j = get_idx (pnrs_b, n_funcs_b, p_add[i]);      //lookup pnrs index in b array
    f_add[i] = funcs_b[j];      //copy func bits (they all get added)
  }

  //now those pnrs that are in both arrays
  for (i = 0; i < n_funcs_a; i++)
  {                             //iterate over a array and see if it's in b, too
    if (pidbufContains (pnrs_a[i], pnrs_b, n_funcs_b))
    {                           //yes it is
      int j = get_idx (pnrs_b, n_funcs_b, pnrs_a[i]);
      uint16_t tmp;
      if ((tmp = ((~funcs_a[i]) & funcs_b[j])))
      {                         //function bits got set
        pidbufAddPid (pnrs_a[i], &p_add, &n_add);
        n_add--;                //'cause it got incremented above.. needs to get less ugly...
        pidbufAddPid (tmp, &f_add, &n_add);
      }
    }
  }
  *f_add_ret = f_add;
  *p_add_ret = p_add;
  return n_add;
}

int
trk_cmp (NOTUSED void *x, const void *p, const void *cv)
{
  SvcTrk *ta = (SvcTrk *) btreePayloadNode ((void *) p);
  SvcTrk *tb = (SvcTrk *) cv;
  return (tb->pnr < ta->pnr) ? -1 : (tb->pnr != ta->pnr);
}

static SvcTrk *
find_svt (SwDmx * d, uint16_t pnr)
{
  BTreeNode *n;
  SvcTrk *rv;
  SvcTrk cv;
  cv.pnr = pnr;
  n = btreeNodeFind (d->svt_root, &cv, NULL, trk_cmp);
  if (n)
  {
    rv = (SvcTrk *) n;
  }
  else
    rv = NULL;
  return rv;
}

static void
reinit_pmt (Table * t, int sz)
{
  clear_tbl (t);
  init_pmt (t, sz);
}

static int
svt_handle_pms2 (uint8_t * s, SvcTrk * t, int idx)
{
  int do_reopen = 0;
  debugPri (10, "handle_pms2: new ver: %d ver: %d idx:%d\n",
            get_sec_version (s), t->pmt[idx] ? t->pmt[idx]->version : -2,
            idx);
  if (NULL == t->pmt[idx])
  {
    debugPri (10, "new table\n");
    t->pmt[idx] = new_tbl (get_sec_last (s) + 1, SEC_ID_PMS);
  }
  if (NULL != t->pmt[idx])
  {
    if ((idx == 0) &&
        (get_sec_version (s) != t->pmt[idx]->version) &&
        (NULL != t->pmt[1]) && (t->pmt[1]->version == get_sec_version (s)))
    {                           //matching next version. copy it over...
      debugPri (10, "copied next\n");
      clear_tbl (t->pmt[0]);
      utlFAN (t->pmt[0]);
      t->pmt[0] = t->pmt[1];
      t->pmt[1] = NULL;
      do_reopen = 1;
    }
    if ((t->pmt[idx]->num_sections != (get_sec_last (s) + 1)) ||
        (get_sec_version (s) != t->pmt[idx]->version))
    {
      debugPri (10, "reinit\n");
      reinit_pmt (t->pmt[idx], get_sec_last (s) + 1);
      do_reopen = 1;
    }

    if (!bifiGet (t->pmt[idx]->occupied, get_sec_nr (s)))
    {
      PMS tpms;
      int rv = parse_pms (s, &tpms);
      if (0 == rv)
      {
        debugPri (10, "added section\n");
        enter_sec (t->pmt[idx], tpms.c.section, &tpms, SEC_ID_PMS);
        t->pmt[idx]->pnr = tpms.program_number;
        t->pmt[idx]->version = tpms.c.version;
        do_reopen = 1;
      }
    }
  }
  return do_reopen;
}

static int
svt_handle_pms (uint8_t * s, SvcTrk * t)
{
  int do_reopen = 0;
  if (sec_is_current (s))
    do_reopen = svt_handle_pms2 (s, t, 0);
  else
    do_reopen = svt_handle_pms2 (s, t, 1);
  return do_reopen;
}

static void
refresh_output (SvcOut * o)
{
  uint16_t *svc = NULL, *total = NULL;
  uint16_t n_svc = 0, n_total = 0;
  svc = get_out_pids (o, &n_svc);
  pidbufDump ("refresh_output svc pids", svc, n_svc);
  pidbufDump ("refresh_output hpids", o->hpids, o->n_hpids);
  total = pidbufMergeUniq (svc, n_svc, o->hpids, o->n_hpids, &n_total);
  selectorModPortUnsafe (o->output, n_total, total);
//      pidchange_notify(o);//TODO send those NEWPIDS events.. not really used anywhere.. can omit..
  utlFAN (svc);
  utlFAN (total);
}

/*
selector will be locked already..
*/
void
svt_packet_put (SelPort * p, void *dta, void *pk)
{
  SvcTrk *t = dta;
  SecBuf const *sb;
  int do_reopen = 0;
  debugPri (10, "svt_packet_put\n");
  if (selectorEIsEvt (pk))
  {
    debugMsg ("was an event\n");
    selectorReleaseElementUnsafe (p->backptr, pk);
    return;
  }

  sb = secExPut (&t->ex, selectorEPayload (pk));
  if (!sb)
  {
    return;
  }
  debugPri (10, "section complete\n");
  //after this we have a section..
  /*
     see if it is a pat
     if not current, exit early
     else try to find pmt pid
     if found, modport with pmt pid
   */
  if (get_sec_id ((uint8_t *) sb->data) == SEC_ID_PAS)
  {                             //A PAS
    debugMsg ("a PAS\n");

    if (sec_is_current ((uint8_t *) sb->data))
    {
      PAS tpas;
      int rv = parse_pas ((uint8_t *) sb->data, &tpas);
      int i;
      if (0 == rv)
      {
        for (i = 0; i < tpas.pa_count; i++)
        {
          if (tpas.pas[i].pnr == t->pnr)
          {                     //found it!!
            PA *a = tpas.pas + i;
            if (a->pid != t->pid)
            {
              uint16_t pid[2] = { 0, a->pid };
              pidbufDump ("svt_packet_put", pid, 2);
              selectorModPortUnsafe (t->input, 2, pid);
              t->pid = a->pid;
              clear_tbl (t->pmt[0]);
              clear_tbl (t->pmt[1]);
              do_reopen = 1;
            }
            break;
          }
        }
        clear_pas (&tpas);
      }
    }
  }
  else if ((get_sec_id ((uint8_t *) sb->data) == SEC_ID_PMS)
           && (get_sec_pnr ((uint8_t *) sb->data) == t->pnr))
  {
    debugMsg ("a (matching) PMS\n");
    do_reopen = svt_handle_pms ((uint8_t *) sb->data, t);
  }

  /*
     if pat, and pmtpid differs, clear pmt tables, remove pmtpid from port
     add new pmtpid to port..
     else if it is a pmt
     if it is newer than present one
     replace present pid
     if it is current and same ver as next
     discard current and replace current with next

     if any pmt changed, modify pids of dependent outputs

   */
  if (do_reopen)
  {
    /*
       something has changed..
       refresh all associated outputs */
    unsigned i;
    debugPri (10, "do_reopen\n");
    for (i = 0; i < t->n_out; i++)
    {
      refresh_output (t->output[i]);
    }
  }
  debugPri (10, "svt_packet_put done\n");
}

void
svt_release_pk (void *d, void *p)
{
  SelPort *prt = d;
  selectorReleaseElementUnsafe (prt->backptr, selectorPElement (p));
}

/**
 *\brief finds or creates a new Trk object
 *
 *if a new one is created, refcount will be at 1 on return.
 *if one already exists and is found, refcount will be incremented by 1 on return.
 *selector is not locked
 */
static SvcTrk *
find_or_new_svt (SwDmx * d, uint16_t pnr)
{
  SvcTrk *t;
  uint16_t pat_pid = 0;
  debugMsg ("looking up tracker for pnr: %" PRIu16 "\n", pnr);
  t = find_svt (d, pnr);
  debugMsg ("found tracker %p\n", t);
  if (t == NULL)
  {
    t = rcptrMalloc (sizeof (*t));
    debugMsg ("creating new tracker %p\n", t);
    memset (t, 0, sizeof (*t));
    t->pnr = pnr;
    t->pid = 0x1fff;
    t->input = selectorAddPortSync (&d->s, svt_packet_put, t);
    pidbufDump ("find_or_new_svt", &pat_pid, 1);
    selectorModPort (t->input, 1, &pat_pid);
    t->pmt[0] = NULL;           //those start out empty... also will have to dump pms_cache
    t->pmt[1] = NULL;
    t->n_out = 0;
    t->output = NULL;
    secExInit (&t->ex, t->input, svt_release_pk);       //svt_release_pk assumes selector is not locked...
    debugMsg ("tracker created\n");
  }
  else
    rcptrAcquire (t);           //so it has +1 refcount in both cases..
  debugMsg ("returning tracker: %p\n", t);
  return t;
}


/**
 *\brief create a svtOut module
 *on the spec selector.
 */
SvcOut *
svtOutAdd (SwDmx * dmx)
{
/*
create out port
set up the port, associate with svcOut

svtOut sits in front of the SelPort
and has similar functionality but adds
service tracking..

*/
  Selector *sel = &dmx->s;
  SvcOut *rv = utlCalloc (sizeof (SvcOut), 1);
  if (NULL == rv)
    return NULL;
  rv->output = selectorAddPort (sel);
  if (NULL == rv->output)
  {
    free (rv);
    return NULL;
  }
  rv->n_funcs = 0;
  rv->r = NULL;
  rv->n_hpids = 0;
  rv->hpids = NULL;
  rv->dmx = dmx;
  dlistInsertFirst (&dmx->outputs, &rv->e);
  return rv;
}

/**
 *\brief put pnrs and func bitmasks in buffers
 */
uint16_t
get_current_funcs (uint16_t ** a_funcs_r, uint16_t ** a_pnrs_r, SvcOut * o)
{
  uint16_t *a_funcs = NULL, *a_pnrs = NULL;
  unsigned i;
  a_funcs = utlCalloc (o->n_funcs, sizeof (uint16_t));
  a_pnrs = utlCalloc (o->n_funcs, sizeof (uint16_t));

  if ((o->n_funcs != 0) && (NULL != a_funcs) && (NULL != a_pnrs))
  {
    for (i = 0; i < o->n_funcs; i++)
    {
      a_funcs[i] = o->r[i].func;
      a_pnrs[i] = o->r[i].pnr;
    }
  }
  else
  {
    utlFAN (a_funcs);
    utlFAN (a_pnrs);
  }
  *a_funcs_r = a_funcs;
  *a_pnrs_r = a_pnrs;
  return o->n_funcs;
}


/**
 *\brief get service pids for this output
 *may be incomplete if trackers do not have data...
 */
static uint16_t *
get_out_pids (SvcOut * o, uint16_t * n_pids_r)
{
  uint16_t *rv = NULL;
  uint16_t n_pids = 0;
  unsigned i;
  for (i = 0; i < o->n_funcs; i++)
  {
    get_pids_for_func (&n_pids, &rv, o->r[i].trk, o->r[i].func);
  }
  *n_pids_r = n_pids;
  return rv;
}

/**
 *\brief change output
 *
 *selector is not locked on entry...
 *
 */
int
svtOutMod (SvcOut * o,
           uint16_t n_funcs, uint16_t * pnrs, uint16_t * funcs,
           uint16_t n_pids, uint16_t * pids)
{
  int i;
  /*
     find funcs to add/remove
     add/remove them from tracker(s)

     fill a buffer with all pids for updated functions
     combine with pids parameter.

     modport with new pidset
   */
  debugMsg ("svtOutMod\n");
  uint16_t *f_rmv = NULL, *p_rmv = NULL, n_rmv = 0;
  uint16_t *f_add = NULL, *p_add = NULL, n_add = 0;
  uint16_t *a_funcs = NULL, *a_pnrs = NULL, a_n_funcs = 0;
  uint16_t n_svc = 0, *svc = NULL;
  uint16_t n_total = 0, *total = NULL;
  a_n_funcs = get_current_funcs (&a_funcs, &a_pnrs, o);

  n_rmv =
    get_funcs_to_remove (&f_rmv, &p_rmv, a_funcs, a_pnrs, a_n_funcs, funcs,
                         pnrs, n_funcs);

  n_add =
    get_funcs_to_add (&f_add, &p_add, a_funcs, a_pnrs, a_n_funcs, funcs, pnrs,
                      n_funcs);
  for (i = 0; i < n_add; i++)
  {
    add_func (o, p_add[i], f_add[i]);
  }

  for (i = 0; i < n_rmv; i++)
  {
    rmv_func (o, p_rmv[i], f_rmv[i]);
  }

  svc = get_out_pids (o, &n_svc);
  pidbufDump ("svtOutMod svc pids", svc, n_svc);
  pidbufDump ("svtOutMod hpids", pids, n_pids);
  total = pidbufMergeUniq (svc, n_svc, pids, n_pids, &n_total);
  utlFAN (f_rmv);
  utlFAN (p_rmv);
  utlFAN (f_add);
  utlFAN (p_add);
  pidbufDump ("ModPort pids", total, n_total);
  selectorModPort (o->output, n_total, total);
  utlFAN (o->hpids);
  o->hpids = pidbufDup (pids, n_pids);
  o->n_hpids = n_pids;
  utlFAN (svc);
  utlFAN (total);
  return 0;
}

DListE *
svtOutSelPort (SvcOut * o)
{
  return o->output;
}

void
svtOutRmv (SvcOut * o)
{
  SwDmx *d = o->dmx;
  //1: release all associated trackers
  while (o->n_funcs)
  {
    remove_tracker (o, 0);
  }
  //2:close port
  selectorRemovePort (o->output);
  //3:free arrays
  utlFAN (o->hpids);
  utlFAN (o->r);
  //4:remove from list
  dlistRemove (&d->outputs, &o->e);
  //5:free object
  memset (o, 0, sizeof (*o));
  utlFAN (o);
}

int
CUswdmxInit (SwDmx * dmx,
             void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                                 uint16_t * rmv_set, uint16_t rmv_size,
                                 void *dta), void *dta, uint32_t limit,
             CUStack * s)
{
  int rv = swdmxInit (dmx, pid_change, dta, limit);
  cuStackFail (rv, dmx, CUswdmxClear, s);
  return rv;
}

void
CUswdmxClear (void *d)
{
  swdmxClear (d);
}

int
swdmxInit (SwDmx * dmx,
           void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                               uint16_t * rmv_set, uint16_t rmv_size,
                               void *dta), void *dta, uint32_t limit)
{
  memset (dmx, 0, sizeof (*dmx));
  dlistInit (&dmx->outputs);
  return selectorInit (&dmx->s, pid_change, dta, limit);
}

void
swdmxClear (SwDmx * dmx)
{
  SvcOut *o = NULL;
  while ((o = (SvcOut *) dlistFirst (&dmx->outputs)))
  {                             //release all outputs
    svtOutRmv (o);
  }
  selectorClear (&dmx->s);
  memset (dmx, 0, sizeof (*dmx));
}

Selector *
svtOutSelector (SvcOut * o)
{
  return &o->dmx->s;
}
