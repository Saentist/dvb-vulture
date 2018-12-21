#include "mheg.h"
#include "debug.h"
#include "pidbuf.h"
#include "dsmcc.h"
#include "module.h"
#include "biop.h"
#include "utils.h"
#include "utl.h"
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include "dmalloc.h"

int DF_MHEG;
#define FILE_DBG DF_MHEG

//----------------local prototypes-------------------
static void *mheg_store_loop (void *p);
static void *mheg_dl (void *p);
static void mheg_proc_evt (Mheg * db, DvbEvtS * d);
static MhegNid *mheg_get_or_ins_onid (Mheg * db, uint16_t onid);
static int mheg_cmp_onid (void *x, const void *a, const void *b);
static void mheg_release_se (void *d, void *p);
static Carousel *ocar_new (MhegSvc * svc, uint32_t carousel_id,
                           uint16_t boot_pid);
static void ocars_purge (MhegSvc * s);
static void modules_purge (Carousel * c);

typedef struct
{
  SvcOut *o;
  SecEx ex;
  int want_tid;
  int want_svc_id;

  int last_ver;
  int last_secno;
    BIFI_DECLARE (sec_seen, 256);

  //for next sections
  uint8_t **next_secs;
  int last_secno_next;
} SecOut;


int
secOutComplete (SecOut * so)
{
  int i;
  if (so->last_secno < 0)
    return 0;
  for (i = 0; i <= so->last_secno; i++)
  {
    if (!bifiGet (so->sec_seen, i))
      return 0;
  }
  return 1;
}

/*
want_svc_id field can be -1 to get all
*/
int
secOutInit (SecOut * so, dvb_s * dvb, uint16_t want_pid, int want_tid,
            int want_svc_id)
{
  so->o = dvbDmxOutAdd (dvb);
  if (NULL == so->o)
    return 1;
  dvbDmxOutMod (so->o, 0, NULL, NULL, 1, &want_pid);
  if (secExInit (&so->ex, svtOutSelector (so->o), mheg_release_se))
  {
    dvbDmxOutRmv (so->o);
    return 1;
  }
  BIFI_INIT (so->sec_seen);
  so->next_secs = NULL;
  so->last_secno_next = -1;
  so->want_tid = want_tid;
  so->want_svc_id = want_svc_id;
  so->last_ver = -1;
  so->last_secno = -1;
  return 0;
}


static int
want_sec (SecOut * so, SecBuf const *sb)
{
  if (so->want_tid != -1)
  {
    if (so->want_tid != get_sec_id ((uint8_t *) sb->data))
      return 0;
  }

  if (so->want_svc_id != -1)
  {
    if (so->want_svc_id != get_sec_pnr ((uint8_t *) sb->data))
      return 0;
  }
  return 1;
}

static int
transfer_next_sec (SecOut * so, uint8_t * sec_ret)
{
  int i;
  int secno;
  if (NULL == so->next_secs)
    return 0;
  for (i = 0; i <= so->last_secno_next; i++)
  {
    if ((so->next_secs[i]) &&
        (so->last_ver == get_sec_version (so->next_secs[i])))
    {
      secno = get_sec_nr (so->next_secs[i]);
      if ((secno <= so->last_secno) && (!bifiGet (so->sec_seen, secno)))
      {                         //secno in range and not seen yet
        bifiSet (so->sec_seen, secno);
        memmove (sec_ret, so->next_secs[i], get_sec_size (so->next_secs[i]));
        utlFAN (so->next_secs[i]);
        return 1;
      }
      else
      {                         //already seen or invalid.. free and NULL
        utlFAN (so->next_secs[i]);
      }
    }
  }
  return 0;
}

///might end up with too small arrays if realloc does not work..
static void
sec_out_resize (SecOut * so, int lsecno)
{
  uint8_t **p;
  p = utlRealloc (so->next_secs, sizeof (uint8_t *) * (lsecno + 1));
  if (p != NULL)
  {
    so->next_secs = p;
    so->last_secno_next = lsecno;
  }
}

///stores a next section for later use..
static int
sec_out_next (SecOut * so, SecBuf const *sb)
{
  int lsecno = get_sec_last ((uint8_t *) sb->data);
  int secno;
  int sec_sz;
  if (so->last_secno_next != lsecno)
  {                             //resize
    sec_out_resize (so, lsecno);
  }
  secno = get_sec_nr ((uint8_t *) sb->data);
  if ((secno <= so->last_secno_next) && (NULL == so->next_secs[secno]) && (0 == secCheckCrc ((uint8_t *) sb->data)))    //BUG?:this might depend on section_syntax_indicator..
  {
    sec_sz = get_sec_size ((uint8_t *) sb->data);
    so->next_secs[secno] = utlCalloc (1, sec_sz);
    if (NULL != so->next_secs[secno])
    {
      memmove (so->next_secs[secno], sb->data, sec_sz);
    }
  }
  return 5;
}

static int
sec_out_process_section (SecOut * so, uint8_t * sec_ret, SecBuf const *sb)
{
  int secno;
  if (want_sec (so, sb))
  {
    if (!sec_is_current ((uint8_t *) sb->data))
    {
      if (so->last_ver != -1)
      {
        if (((so->last_ver + 1) % 32) ==
            get_sec_version ((uint8_t *) sb->data))
        {                       //next version
          return sec_out_next (so, sb);
        }
      }
    }
    else
    {                           //current section
      if (so->last_ver != get_sec_version ((uint8_t *) sb->data))
      {                         //new version, zero bits
        BIFI_INIT (so->sec_seen);
        so->last_ver = get_sec_version ((uint8_t *) sb->data);  //update version
      }
      so->last_secno = get_sec_last ((uint8_t *) sb->data);
      secno = get_sec_nr ((uint8_t *) sb->data);
      if ((secno <= so->last_secno) &&
          (!bifiGet (so->sec_seen, secno)) &&
          (0 == secCheckCrc ((uint8_t *) sb->data)))
      {                         //not seen before, use it
        //crc pass
        bifiSet (so->sec_seen, get_sec_nr ((uint8_t *) sb->data));
        memmove (sec_ret, sb->data, get_sec_size ((uint8_t *) sb->data));
        return 0;
      }
    }
  }
  return 5;                     //stored.. ignored? get more
}

///process a transport packet carrying section data
static int
sec_out_tsp (SecOut * so, uint8_t * sec_ret, void *e)
{
  SecBuf const *sb;
  sb = secExPut (&so->ex, selectorEPayload (e));
  if (sb)
  {
    return sec_out_process_section (so, sec_ret, sb);
  }
  return 5;                     //want more
}

///process a packet from sw demuxer
static int
sec_out_process_packet (SecOut * so, uint8_t * sec_ret, void *e)
{
  if (selectorEIsEvt (e))
  {                             //an event
    SelBuf *b = e;
    memmove (sec_ret, &b->event, sizeof (DvbEvtS));
    svtOutReleaseElement (so->o, e);
    return 1;
  }
  else
  {
    return sec_out_tsp (so, sec_ret, e);
  }
}

/**
 nonblocking..
 returns 2 on timeout and sec_ret unchanged
 1 on event
 0 on section
 */
int
secOutPoll (SecOut * so, uint8_t * sec_ret, int timeout_ms)
{
  while (timeout_ms > 0)
  {
    int rv;
    if (transfer_next_sec (so, sec_ret))
    {
      return 0;
    }
    selectorLock (svtOutSelector (so->o));
    rv = selectorDataAvail (svtOutSelPort (so->o));
    selectorUnlock (svtOutSelector (so->o));
    if (1 == rv)
    {
      int z;
      z = sec_out_process_packet (so, sec_ret, svtOutReadElement (so->o));
      if (z != 5)
        return z;
    }
    else
    {
      usleep (30000);
      timeout_ms -= 30;
    }
  }
  return 2;
}

void
secOutClear (SecOut * so)
{
  int i;
  secExClear (&so->ex);
  dvbDmxOutRmv (so->o);
  if ((NULL != so->next_secs) && (so->last_secno_next >= 0))
  {
    for (i = 0; i <= so->last_secno_next; i++)
    {
      utlFAN (so->next_secs[i]);
    }
    utlFAN (so->next_secs);
  }
  memset (so, 0, sizeof (*so));
}


/* stream_types we are interested in */
#define STREAM_TYPE_VIDEO_MPEG2		0x02
#define STREAM_TYPE_AUDIO_MPEG1		0x03
#define STREAM_TYPE_AUDIO_MPEG2		0x04
#define STREAM_TYPE_ISO13818_6_B	0x0b
#define STREAM_TYPE_AUDIO_MPEG4		0x11

/* descriptors we want */
#define TAG_LANGUAGE_DESCRIPTOR			0x0a
#define TAG_CAROUSEL_ID_DESCRIPTOR		0x13
#define TAG_STREAM_ID_DESCRIPTOR		0x52
#define TAG_DATA_BROADCAST_ID_DESCRIPTOR	0x66

#define TAG_APP_SIG_DESCRIPTOR			0x6f
#define TAG_APP_LOC_DESCRIPTOR			0x15
#define TAG_TRN_PROTO_DESCRIPTOR			0x02
#define TAG_APP_NAME_DESCRIPTOR			0x01

struct app_name_descriptor
{
  char lang_code[3];
  uint8_t name_len;
  char name[0];
} __attribute__ ((__packed__));

struct app_sig_descriptor
{
  uint16_t app_type;            ///<lowest 15 bits(I hope)
  uint8_t app_ver;              ///<lowest 5 bits(I hope)
} __attribute__ ((__packed__));

struct app_loc_descriptor
{
  uint8_t path_byte[0];         ///the initial(boot?) path for the app
} __attribute__ ((__packed__));

struct sel_rmt
{
  uint16_t onid;
  uint16_t tsid;
  uint16_t sid;
  uint8_t cmp_tag;
} __attribute__ ((__packed__));

struct sel_oc
{
  uint8_t is_remote;            ///<if lowest bit set is remote connection
  union
  {
    struct sel_rmt rmt;
    uint8_t cmp_tag;
  };
} __attribute__ ((__packed__));

struct trn_proto_descriptor
{
  uint16_t protocol_id;         ///<should be 1 for object carousel
  uint8_t protocol_label;       ///<what's this? links app desc to protocol...
  struct sel_oc oc;             ///<needs a union here
} __attribute__ ((__packed__));

/* bits of the descriptors we care about */
struct carousel_id_descriptor
{
  uint32_t carousel_id;
};

struct stream_id_descriptor
{
  uint8_t component_tag;
};

struct language_descriptor
{
  char language_code[3];
  uint8_t audio_type;
} __attribute__ ((__packed__));

struct data_broadcast_id_descriptor
{
  uint16_t data_broadcast_id;
  uint16_t application_type_code;
  uint8_t boot_priority_hint;
} __attribute__ ((__packed__));

/* data_broadcast_id_descriptor values we want */
#define DATA_BROADCAST_ID		0x0106
#define UK_APPLICATION_TYPE_CODE	0x0101
#define NZ_APPLICATION_TYPE_CODE	0x0505

/* DSMCC table ID's we want */
#define TID_DSMCC_CONTROL	0x3b    /* DSI or DII */
#define TID_DSMCC_DATA		0x3c    /* DDB */

//----------------public funcs-----------------------
int
mhegInit (Mheg * mheg, RcFile * c NOTUSED, dvb_s * dvb, PgmDb * pgmdb)
{
  memset (mheg, 0, sizeof (*mheg));
  if (taskInit (&mheg->t, mheg, mheg_store_loop))       //,&db->destroy))
  {
    errMsg ("task failed to init\n");
    return 1;
  }
  mheg->dvb = dvb;
  mheg->pgmdb = pgmdb;
  mheg->root = NULL;            //TODO init me
  mheg->current_onid = NULL;
  mheg->tuned = 0;
  mheg->o = dvbDmxOutAdd (dvb);
  return 0;
}



void
mhegClear (Mheg * mheg)
{
  taskClear (&mheg->t);
  dvbDmxOutRmv (mheg->o);

}

int
mhegStart (Mheg * mheg)
{
  return taskStart (&mheg->t);
}

int
mhegStop (Mheg * mheg)
{
  return taskStop (&mheg->t);
}

int
mhegGet (Mheg * mheg NOTUSED, uint16_t onid NOTUSED, uint16_t pnr NOTUSED,
         char **ret NOTUSED, uint16_t * num_ret NOTUSED)
{
  return 0;
}

void
CUmhegClear (void *db)
{
  mhegClear (db);
}

int
CUmhegInit (Mheg * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb, CUStack * s)
{
  int rv;
  rv = mhegInit (db, cfg, dvb, pgmdb);
  cuStackFail (rv, db, CUmhegClear, s);
  return rv;
}


//-----------------static funcs---------------------------

/*
generate a carousel struct(s) on 'pnr add' and 
'get all mheg' events, put it in db.

lookup/open pids

then grab tables until untune, pnr rmv etc..

what is dii,dsi,ddb?


*/
static int
mheg_cmp_onid (void *x NOTUSED, const void *a, const void *b)
{
  MhegNid const *ap = a;
  MhegNid const *bp = b;
  return (bp->id < ap->id) ? -1 : (bp->id != ap->id);
}


static int
mheg_cmp_svc (void *x NOTUSED, const void *a, const void *b)
{
  MhegSvc const *ap = a;
  MhegSvc const *bp = b;
  return (bp->pnr < ap->pnr) ? -1 : (bp->pnr != ap->pnr);
}


static MhegNid *
mheg_get_or_ins_onid (Mheg * db, uint16_t id)
{
  MhegNid cv;
  BTreeNode *n;
  cv.id = id;
  cv.svc = NULL;
  cv.backptr = db;
  n =
    btreeNodeGetOrIns (&db->root, &cv, NULL, mheg_cmp_onid, sizeof (MhegNid));
  if (!n)
    return NULL;
  return btreeNodePayload (n);
}

void
add_dsmcc_pid (Carousel * s, uint16_t pid)
{
  pidbufAddPid (pid, &s->pids, &s->npids);
}

static bool
is_audio_stream (uint8_t stream_type)
{
  switch (stream_type)
  {
  case STREAM_TYPE_AUDIO_MPEG1:
  case STREAM_TYPE_AUDIO_MPEG2:
  case STREAM_TYPE_AUDIO_MPEG4:
    return true;

  default:
    return false;
  }
}


/*
pmt tracker
*/
static void
svc_proc_pmt (MhegSvc * s, uint8_t * pmt)
{
  uint16_t section_length;
  uint16_t offset;
  uint8_t stream_type;
  uint16_t elementary_pid;
  uint16_t info_length;
  uint8_t desc_tag;
  uint8_t desc_length;
  uint16_t component_tag;
  int desc_boot_pid;
  int desc_carousel_id;
  int sec_ver;
  int carousel_id = -1;         //read from pmt

  /*
     when a new version shows up, stop and destroy all AIT trackers
     generate them again...
   */
  sec_ver = get_sec_version (pmt);
  if (sec_ver != s->pmt_ver)
  {
    ocars_purge (s);
    s->pmt_ver = sec_ver;
  }

  section_length = 3 + (((pmt[1] & 0x0f) << 8) + pmt[2]);

  /* skip the program_info descriptors */
  offset = 10;
  info_length = ((pmt[offset] & 0x0f) << 8) + pmt[offset + 1];
  offset += 2 + info_length;

  /*
   *  find the Descriptor Tags we are interested in
   */
  while (offset < (section_length - 4))
  {
    stream_type = pmt[offset];
    offset += 1;
    elementary_pid = ((pmt[offset] & 0x1f) << 8) + pmt[offset + 1];
    offset += 2;
    /* is it the default video stream for this service */
    if (stream_type == STREAM_TYPE_VIDEO_MPEG2)
    {
      s->video_pid = elementary_pid;
      s->video_type = stream_type;
      vverbose ("PID=%u video stream_type=0x%x", elementary_pid, stream_type);
    }
    /* it's not the boot PID yet */
//              desc_boot_pid = -1;
    //      desc_carousel_id = -1;
    /* read the descriptors */
    info_length = ((pmt[offset] & 0x0f) << 8) + pmt[offset + 1];
    offset += 2;
    while (info_length != 0)
    {
      desc_tag = pmt[offset];
      desc_length = pmt[offset + 1];
      offset += 2;
      info_length -= 2;
      /* ignore boot PID if we explicitly chose a carousel ID */
      if (desc_tag == TAG_DATA_BROADCAST_ID_DESCRIPTOR && carousel_id == -1)
      {
        struct data_broadcast_id_descriptor *desc;
        desc = (struct data_broadcast_id_descriptor *) &pmt[offset];
        if (ntohs (desc->data_broadcast_id) == DATA_BROADCAST_ID)
        {
          desc_boot_pid = elementary_pid;
          vverbose ("PID=%u boot_priority_hint=%u", elementary_pid,
                    desc->boot_priority_hint);
          /* haven't seen the NZ MHEG Profile, but let's download the data anyway */
          if (ntohs (desc->application_type_code) == UK_APPLICATION_TYPE_CODE)
            vverbose ("UK application_type_code (0x%04x)",
                      UK_APPLICATION_TYPE_CODE);
          else if (ntohs (desc->application_type_code) ==
                   NZ_APPLICATION_TYPE_CODE)
            vverbose ("NZ application_type_code (0x%04x)",
                      NZ_APPLICATION_TYPE_CODE);
          else
            vverbose ("Unknown application_type_code (0x%04x)",
                      ntohs (desc->application_type_code));
        }
        else
        {
          vverbose ("PID=%u data_broadcast_id=0x%x", elementary_pid,
                    ntohs (desc->data_broadcast_id));
          vhexdump (&pmt[offset], desc_length);
        }
      }
      else if (desc_tag == TAG_CAROUSEL_ID_DESCRIPTOR)
      {
        struct carousel_id_descriptor *desc;
        desc = (struct carousel_id_descriptor *) &pmt[offset];
        if (carousel_id == -1
            || carousel_id == ((int) ntohl (desc->carousel_id)))
        {
          /* if we chose this carousel, set the boot PID */
//                                      if(carousel_id != -1)
          desc_boot_pid = elementary_pid;
          desc_carousel_id = ntohl (desc->carousel_id);
          /*
             if carousel_id is 0x100-0xffffffff,
             it is a global carousel owned by org_id((carousel_id>>8)&0xffffff)
             what's the lifetime?
             how to store/find them?
             Carousel objects?
             what about data carousels?

           */
          ocar_new (s, desc_carousel_id, desc_boot_pid);
        }
        vverbose ("PID=%u carousel_id=%u", elementary_pid,
                  ntohl (desc->carousel_id));
      }
      else if (desc_tag == TAG_STREAM_ID_DESCRIPTOR)
      {
        struct stream_id_descriptor *desc;
        desc = (struct stream_id_descriptor *) &pmt[offset];
        component_tag = desc->component_tag;
        vverbose ("PID=%u component_tag=%u", elementary_pid, component_tag);
        add_assoc (&s->assoc, elementary_pid, desc->component_tag,
                   stream_type);
      }
      else if (desc_tag == TAG_LANGUAGE_DESCRIPTOR
               && is_audio_stream (stream_type))
      {
        struct language_descriptor *desc;
        desc = (struct language_descriptor *) &pmt[offset];
        /* only remember the normal audio stream (not visually impaired stream) */
        if (desc->audio_type == 0)
        {
          s->audio_pid = elementary_pid;
          s->audio_type = stream_type;
          vverbose ("PID=%u audio stream_type=0x%x", elementary_pid,
                    stream_type);
        }
      }
      else if (desc_tag == TAG_APP_SIG_DESCRIPTOR)      //application signalling descriptor
      {
        struct app_sig_descriptor *desc;
        desc = (struct app_sig_descriptor *) &pmt[offset];
        vverbose
          ("PID=%u application signalling descriptor\n application_type: 0x%hx, AIT_ver: 0x%hhx\n",
           elementary_pid, htons (desc->app_type) & 0x7fff,
           desc->app_ver & 31);
      }
      else
      {
        vverbose ("PID=%u descriptor=0x%x", elementary_pid, desc_tag);
        vhexdump (&pmt[offset], desc_length);
      }
      offset += desc_length;
      info_length -= desc_length;
    }
  }
}


static int
mheg_cmp_ocar (void *x NOTUSED, const void *a, const void *b)
{
  Carousel const *ap = a;
  Carousel const *bp = b;
  return (bp->boot_pid < ap->boot_pid) ? -1 : (bp->boot_pid != ap->boot_pid);
}


static Carousel *
ocar_new (MhegSvc * svc, uint32_t carousel_id, uint16_t boot_pid)
{
  BTreeNode *n;
  Carousel *car;
  n = utlCalloc (1, btreeNodeSize (sizeof (Carousel)));
  if (!n)
  {
    errMsg ("allocation failed\n");
    return NULL;
  }
  car = btreeNodePayload (n);
  car->carousel_id = carousel_id;
  car->boot_pid = boot_pid;
  car->pids = NULL;
  car->npids = 0;
  car->nmodules = 0;
  car->got_dsi = false;
  car->modules = NULL;
  car->backptr = svc;
  if (btreeNodeInsert (&svc->ocars, n, NULL, mheg_cmp_ocar))
  {
    errMsg ("Carousel already exists. id: %" PRIu32 ", pid: %" PRIu16 "\n",
            carousel_id, boot_pid);
    utlFAN (n);
    return NULL;
  }
  taskInit (&car->t, car, mheg_dl);
  return car;
}

static void
ocar_start (void *x NOTUSED, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    Carousel *c = btreeNodePayload (this);
    taskStart (&c->t);
  }
}

static void
ocars_start (MhegSvc * s)
{
  btreeWalk (s->ocars, s, ocar_start);
}

static void
modules_purge (Carousel * c)
{
  unsigned i;
  for (i = 0; i < c->nmodules; i++)
  {
    free_module (&c->modules[i]);
  }
  c->nmodules = 0;
  utlFAN (c->modules);
}

static void
ocar_purge (void *x NOTUSED, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    Carousel *c = btreeNodePayload (this);
    taskClear (&c->t);
    modules_purge (c);
    utlFAN (c->pids);

    free (this);
  }
}


static void
ocars_purge (MhegSvc * s)
{
  btreeWalk (s->ocars, s, ocar_purge);
  s->ocars = NULL;
}

static void *
mheg_pmt_trk (void *p)
{
  MhegSvc *s = p;
  Mheg *db = s->backptr->backptr;
  int rv;
  uint8_t sec_ret[MAX_SEC_SZ];
  SecOut pmt_o;
  secOutInit (&pmt_o, db->dvb, s->pmt_pid, SEC_ID_PMS, s->pnr);
  rv = nice (db->niceness);
  debugPri (1, "nice returned %d\n", rv);
  taskLock (&s->t2);
  while (taskRunning (&s->t2))
  {
    taskUnlock (&s->t2);
    rv = secOutPoll (&pmt_o, sec_ret, 500);
    taskLock (&s->t2);
    if (1 == rv)
    {
      debugPri (10, "event\n");
    }
    else if (0 == rv)
    {
      debugPri (10, "section\n");
      svc_proc_pmt (s, sec_ret);
      if (secOutComplete (&pmt_o))
      {
        debugPri (10, "pmt complete, starting downloadserz.\n");
        ocars_start (s);
      }
    }
    else
    {                           //timeout.. nothing...
      debugPri (10, "timedout\n");
    }
  }

  taskUnlock (&s->t2);
  ocars_purge (s);
  secOutClear (&pmt_o);
  return NULL;
}




        /*
           can not use secOut here..
           version/next etc do not work as usual
           from dvbsnoop:
           TS sub-decoding (1 packet(s) stored for PID 0x087b):
           =====================================================
           TS contains Section...
           SI packet (length=112): 
           PID:  2171 (0x087b)

           Guess table from table id...
           DSM-CC-decoding....
           Table_ID: 59 (0x3b)  [= DSM-CC - U-N messages (DSI or DII)]
           Section_syntax_indicator: 1 (0x01)
           private_indicator: 0 (0x00)
           reserved_1: 3 (0x03)
           dsmcc_section_length: 109 (0x006d)
           table_id_extension: 0 (0x0000)
           reserved_3: 3 (0x03)
           Version_number: 0 (0x00)
           Current_next_indicator: 1 (0x01)  [= valid now]
           Section_number: 0 (0x00)
           Last_section_number: 0 (0x00)
           DSM-CC Message Header:
           protocolDiscriminator: 17 (0x11)
           dsmccType: 3 (0x03)  [= Download message]
           messageId: 4102 (0x1006)  [= DownloadServerInitiate (DSI)]
           transactionID: 2147483648 (0x80000000)
           ==> originator: 2 (0x02)  [= assigned by the network]
           ==> version: 0 (0x0000)
           ==> identification: 0 (0x0000)
           ==> update toggle flag: 0 (0x00)
           reserved: 255 (0xff)
           adaptationLength: 0 (0x00)
           messageLength: 88 (0x0058)
           Server-ID  (NSAP address):
           Authority and Format Identifier: 255 (0xff)
           Type: 255 (0xff)
           address bytes:
           0000:  ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff   ................
           0010:  ff ff                                              ..
           DSMCC_Compatibility Descriptor (loop):
           compatibilityDescriptorLength: 0 (0x0000)
           privateDataLength: 64 (0x0040)
           guessing private data: BIOP::ServiceGatewayInfo:
           IOP::IOR:
           type_id_length: 4 (0x00000004)
           type_id: "srg."
           taggedProfiles_count: 1 (0x00000001)
           BIOPProfileBody
           profileId_tag: 1230196486 (0x49534f06)  [= TAG_BIOP]
           profile_data_length: 40 (0x00000028)
           profile_data_byte_order: 0 (0x00)  [= Big Endian]
           lite_component_count: 2 (0x02)
           LiteComponents:
           component_tag: 1230196560 (0x49534f50)  [= TAG_ObjectLocation]
           component_data_length: 10 (0x0a)
           carouselId: 10 (0x0000000a)
           moduleId: 3 (0x0003)
           version.major: 1 (0x01)
           version.minor: 0 (0x00)
           objectKey_length: 1 (0x01)
           objectKey_data:
           0000:  01                                                 .

           DSM::ConnBinder
           componentId_tag: 1230196544 (0x49534f40)  [= TAG_ConnBinder]
           component_data_length: 18 (0x12)
           tap_count: 1 (0x01)
           <B1><CE>^G^H<88><86><EB><BF>^A::TAP:
           id: 0 (0x0000)
           use: 22 (0x0016)  [= BIOP_DELIVERY_PARA_USE]
           association_tag: 20 (0x0014)
           selector_length: 10 (0x0a)
           selector_type: 1 (0x0001)  [= MessageSelector]
           transactionID: 2147483650 (0x80000002)
           ==> originator: 2 (0x02)  [= assigned by the network]
           ==> version: 0 (0x0000)
           ==> identification: 1 (0x0001)
           ==> update toggle flag: 0 (0x00)
           timeout: 4294967295 (0xffffffff)  [= usec.]




           downloadTaps_count: 0 (0x00)
           serviceContextList_count: 0 (0x00)
           userInfoLength: 0 (0x0000)
           CRC: 3777486849 (0xe127e001)


         */

static void
svc_proc_pat (MhegSvc * s, uint8_t * pat)
{
  uint16_t section_length;
  uint16_t offset;
  uint16_t service_id = s->pnr;
  uint16_t map_pid = 0;
  bool found;
  section_length = 3 + (((pat[1] & 0x0f) << 8) + pat[2]);

  taskStop (&s->t2);
  /* find the PMT for this service_id */
  found = false;
  offset = 8;
  /* -4 for the CRC at the end */
  while ((offset < (section_length - 4)) && !found)
  {
    if ((pat[offset] << 8) + pat[offset + 1] == service_id)
    {
      map_pid = ((pat[offset + 2] & 0x1f) << 8) + pat[offset + 3];
      found = true;
      s->pmt_pid = map_pid;
    }
    else
    {
      offset += 4;
    }
  }

  if (!found)
    errMsg ("Unable to find PMT PID for service_id %" PRIu16 "\n",
            service_id);
  else
    taskStart (&s->t2);

  vverbose ("PMT PID: %u", map_pid);
}

static void *
mheg_pat_trk (void *p)
{
  MhegSvc *s = p;
  Mheg *db = s->backptr->backptr;
  int rv;
  uint8_t sec_ret[MAX_SEC_SZ];
  SecOut pat_o;
  secOutInit (&pat_o, db->dvb, PAT_PID, SEC_ID_PAS, -1);
  rv = nice (db->niceness);
  debugPri (1, "nice returned %d\n", rv);
  taskLock (&s->t);
  while (taskRunning (&s->t))
  {
    taskUnlock (&s->t);
    rv = secOutPoll (&pat_o, sec_ret, 500);
    taskLock (&s->t);
    if (1 == rv)
    {
      debugPri (10, "event\n");
    }
    else if (0 == rv)
    {
      debugPri (10, "section\n");
      svc_proc_pat (s, sec_ret);
    }
    else
    {                           //timeout.. nothing...
      debugPri (10, "timedout\n");
    }
  }

  taskUnlock (&s->t);
  taskStop (&s->t2);
  secOutClear (&pat_o);
  return NULL;
}

void
process_dii (Carousel * car, struct DownloadInfoIndication *dii,
             uint32_t transactionId)
{
  unsigned int nmodules;
  unsigned int i;
/*
is there only one per carousel, or
will I have to store multiple transaction Ids (versions))
dii-objects?
*/
  verbose ("DownloadInfoIndication");
  vverbose ("transactionId: %u", transactionId);
  if (transactionId != car->dii_tid)
  {
    modules_purge (car);
    car->dii_tid = transactionId;
  }
  vverbose ("downloadId: %u", ntohl (dii->downloadId));

  nmodules = DII_numberOfModules (dii);
  vverbose ("numberOfModules: %u", nmodules);

  for (i = 0; i < nmodules; i++)
  {
    struct DIIModule *mod;
    mod = DII_module (dii, i);
    vverbose ("Module %u", i);
    vverbose (" moduleId: %u", ntohs (mod->moduleId));
    vverbose (" moduleVersion: %u", mod->moduleVersion);
    vverbose (" moduleSize: %u", ntohl (mod->moduleSize));
    if (find_module
        (car, ntohs (mod->moduleId), mod->moduleVersion,
         ntohl (dii->downloadId)) == NULL)
      add_module (car, dii, mod);
  }
  return;
}



void
process_dsi (uint16_t current_pid, Carousel * car,
             struct DownloadServerInitiate *dsi, uint32_t transactionId)
{
  uint16_t elementary_pid;

  verbose ("DownloadServerInitiate");

  /* only download the DSI from the boot PID */
  if ((current_pid != car->boot_pid) ||
      (car->got_dsi && (transactionId == car->dsi_tid)))
    return;

  /* TODO: purge old directory on version increment */
  car->got_dsi = true;
  if (car->dsi_tid != transactionId)
  {
    debugMsg ("DSI Version change: %" PRIu32 " -> %" PRIu32 "\n",
              car->dsi_tid, transactionId);
    car->dsi_tid = transactionId;
    modules_purge (car);
  }
  elementary_pid =
    process_biop_service_gateway_info (car->backptr->backptr->id,
                                       car->backptr->pnr,
                                       &car->backptr->assoc,
                                       DSI_privateDataByte (dsi),
                                       ntohs (dsi->privateDataLength));
  /*
     can this be on a differeny pid than DII?
     oh,yes..
     guess I should memorize the obj key,module id too..

     perhaps turn the whole dsmcc thing upside down...
     not interpret all sections I get but more synchronously..
     ...
     if(is_one_layer(car))
     {
     dsi=get_dsi();
     n_dirs=get_service_gateway(dsi,&dir_array);
     }
     else
     {
     get_dii
     }
     for(i=0;i<n_dirs;i++)
     {
     download_dir(i,dir_array[i]); those perhaps in parallel...
     }
     ...



   */

  /* make sure we are downloading data from the PID the DSI refers to */
  add_dsmcc_pid (car, elementary_pid);

  return;
}

void
process_ddb (uint16_t tsid, uint16_t current_pid, Carousel * car,
             struct DownloadDataBlock *ddb, uint32_t downloadId,
             uint32_t blockLength)
{
  unsigned char *block;
  struct module *mod;

  verbose ("DownloadDataBlock");

  vverbose ("downloadId: %u", downloadId);
  vverbose ("moduleId: %u", ntohs (ddb->moduleId));
  vverbose ("moduleVersion: %u", ddb->moduleVersion);
  vverbose ("blockNumber: %u", ntohs (ddb->blockNumber));

  vverbose ("blockLength: %u", blockLength);
  block = DDB_blockDataByte (ddb);
  vhexdump (block, blockLength);

  if ((mod =
       find_module (car, ntohs (ddb->moduleId), ddb->moduleVersion,
                    downloadId)) != NULL)
    download_block (tsid, current_pid, car, mod, ntohs (ddb->blockNumber),
                    block, blockLength);

  return;
}


static void
mheg_proc_dsmcc (Carousel * s, SecBuf const *sb)
{
  unsigned char *table = (unsigned char *) sb->data;
  struct dsmccMessageHeader *dsmcc;
  if (table[0] != TID_DSMCC_CONTROL && table[0] != TID_DSMCC_DATA)
    return;

  dsmcc = (struct dsmccMessageHeader *) &table[8];
  if (dsmcc->protocolDiscriminator == DSMCC_PROTOCOL
      && dsmcc->dsmccType == DSMCC_TYPE_DOWNLOAD)
  {
    if (ntohs (dsmcc->messageId) == DSMCC_MSGID_DII)
      process_dii (s, (struct DownloadInfoIndication *) dsmccMessage (dsmcc),
                   ntohl (dsmcc->transactionId));
    else if (ntohs (dsmcc->messageId) == DSMCC_MSGID_DSI)
      process_dsi (sb->pid, s,
                   (struct DownloadServerInitiate *) dsmccMessage (dsmcc),
                   ntohl (dsmcc->transactionId));
    else if (ntohs (dsmcc->messageId) == DSMCC_MSGID_DDB)
      process_ddb (s->backptr->backptr->id, sb->pid, s,
                   (struct DownloadDataBlock *) dsmccMessage (dsmcc),
                   ntohl (dsmcc->transactionId), DDB_blockDataLength (dsmcc));
    else
      error ("Unknown DSMCC messageId: 0x%x", ntohs (dsmcc->messageId));
  }
}


static void *
mheg_dl (void *p)
{
  Carousel *s = p;
  /*
     might need multiple downloaders.. 
     with distinct objects... 
     differences.. boot pid... 
     more specific path... onid/svc/carousel id? 

     'parts of no more than four sections shall be delivered in a single TS packet' ???!!!
     WTF
     may have to redesign secex
     currently it expects no more than one pointer field in ts packet..
     and 1 section/packet (+1 partial)


     what are those numbers in the dvb:// url? ?
     dvb://tf1.B8/weather/data.png (where B8 is the component tag of the carousel).
     the first thing is prolly the svc name...

     firefox plugin needs channel list for vlc integration..
     (can then play videos)
     try to generate from channel list?..

     or modify firehbbtv plugin to contact server and stream channel..
     is it possible to control it entirely from ff?

     fbvlc seems to be a windoze thang...
   */
  Mheg *db = s->backptr->backptr->backptr;
  SecEx ex;
  int rv;
  void *d;
  SvcOut *o;
  add_dsmcc_pid (s, s->boot_pid);
  o = dvbDmxOutAdd (db->dvb);
  dvbDmxOutMod (o, 0, NULL, NULL, s->npids, s->pids);
  secExInit (&ex, svtOutSelector (o), mheg_release_se);

  taskLock (&s->t);
  while (taskRunning (&s->t))
  {
    taskUnlock (&s->t);
    rv = svtOutWaitData (o, 500);
    taskLock (&s->t);
    if (rv == 0)
    {
      d = svtOutReadElement (o);
      if (!selectorEIsEvt (d))
      {
        SecBuf const *sb;
        sb = secExPut (&ex, svtEPayload (d));
        if (sb)
          mheg_proc_dsmcc (s, sb);
      }
      else
      {
        svtOutReleaseElement (o, d);
      }
    }
  }

  taskUnlock (&s->t);
  secExClear (&ex);
  dvbDmxOutRmv (o);
  return NULL;
}

static MhegSvc *
mheg_get_or_ins_svc (Mheg * db NOTUSED, MhegNid * nid, uint16_t id)
{
  MhegSvc cv, *s;
  BTreeNode *n;
  cv.pnr = id;
  n = btreeNodeFind (nid->svc, &cv, NULL, mheg_cmp_svc);
  if (n == NULL)
  {
    n = calloc (1, btreeNodeSize (sizeof (MhegSvc)));
    if (!n)
      return NULL;
    s = btreeNodePayload (n);
    s->pnr = id;
    s->pmt_pid = -1;
    taskInit (&s->t, s, mheg_pat_trk);
    taskInit (&s->t2, s, mheg_pmt_trk);
    s->backptr = nid;
    s->video_pid = -1;
    s->video_type = -1;
    s->audio_pid = -1;
    s->audio_type = -1;
    init_assoc (&s->assoc);
    btreeNodeInsert (&nid->svc, n, NULL, mheg_cmp_svc);
  }
  s = btreeNodePayload (n);
  return s;
}

static void
mheg_release_se (void *d, void *p)
{
  selectorReleaseElement ((Selector *) d, selectorPElement (p));
}




  /*
     better do mheg acq in a separate thread... 
     will have to start a thread for each pnr
     open a demux port, secex
     grab the dsmcc pids,
     and if valid, get the download..

     what about the events?
     can I miss any?
     could it get stuck?

     may have to do a synchronisation thingy on tune like the other modules do..

     can I do this download without changing the original sources too much?

     have to move from fds to sw demuxing... somehow...
     may have to change error feedback..
     also ..processes->threads..

     register a dmx for pat
     and for pmt
     if pat changes
     reopen pmt
     if pmt changes
     update/flush carousel..


     several threads to keep contexts separated/code simple?

     one thread for pat
     every time it gets new pat, it iterates thru
     active mheg pmt threads and stops/restarts those that changed pmt pid

     one thread for each service
     every time new pmt version shows up, w changed pids,
     stops download and restarts it

     one download thread for each service
     downloads the dsm data itself..? not sure about that last part..
     dsmcc dloader
     reads table data until control or data table Id encountered
     then processes dii/dsi/ddb data

     there is another level of indirection for HbbTV, namely AIT tables
     so need another thread for those and at least another to handle 
     carousels signalled from AIT tables..

     downloadinfoindication:
     module inventory, I think
     several modules... versioned
     modules composed of blocks..
     transaction Id is a version number? redownload on increase(change?)

     downloadserverinitiate:
     on the boot pid... gives pid to download data from..

     downloaddatablock:
     data block itself
     part of a module w specific version
     module has 16 bit module id 8 bit version...
     32 bit download id. look up by those..?
     block reassembly... bitfield...
     module may be compressed.. zlib, it seems..

     biop processing splits the module into separate BIOP messages
     directories
     files
     service gateway(root dir)
     streams
     stream events

     storing all in a file tree may work alright...
     can leave lo-level codes alone then..
     what about bloat...
     do I have to purge stuff, to avoid boundless growth?
     how?
     persistence?
     do i have to store an index on exit
     or is the dir tree sufficient for continuing after a restart?

     what about displaying?
     can I use existing client(not mine, butKilvingtons rb-browser?)
     likely... but does tuning... either fail always or succeed always?
     also uses orig network id and svc nr to 
     identify carousels... not pos/tp/svc...etc..

     commands are text strings(telnet-like):
     { "assoc", "",                                             cmd_assoc,      "List component tag to PID mappings" },
     { "ademux", "[<ServiceID>] <ComponentTag>",                cmd_ademux,     "Demux the given audio component tag" },
     think I can skip the demux.. as that's for local clients, methinks..
     { "astream", "[<ServiceID>] <ComponentTag>",               cmd_astream,    "Stream the given audio component tag" },
     { "available", "<ServiceID>",                              cmd_available,  "Return OK if the ServiceID is available" },
     { "avdemux", "[<ServiceID>] <AudioTag> <VideoTag>",        cmd_avdemux,    "Demux the given audio and video component tags" },
     { "avstream", "[<ServiceID>] <AudioTag> <VideoTag>",       cmd_avstream,   "Stream the given audio and video component tags" },
     { "check", "<ContentReference>",                   cmd_check,      "Check if the given file exists on the carousel" },
     { "exit", "",                                              cmd_quit,       "Close the connection" },
     { "file", "<ContentReference>",                            cmd_file,       "Retrieve the given file from the carousel" },
     { "help", "",                                              cmd_help,       "List available commands" },
     { "quit", "",                                              cmd_quit,       "Close the connection" },
     { "retune", "<ServiceID>",                         cmd_retune,     "Start downloading the carousel from ServiceID" },
     { "service", "",                                   cmd_service,    "Show the current service ID" },
     { "vdemux", "[<ServiceID>] <ComponentTag>",                cmd_vdemux,     "Demux the given video component tag" },
     { "vstream", "[<ServiceID>] <ComponentTag>",               cmd_vstream,    "Stream the given video component tag" },
     { NULL, NULL, NULL, NULL }

     have read up about component tags...
     PMT has optionally one stream identifier descriptor per elementary stream
     it associates an elementary stream with an 8 bit value(the component tag)
     this tag may be used in other descriptors in other tables to refer to that PID/ElemStream
     tag may only be unique for a service..

     what about hbbTv?
     how to interpret html stuff..?
     webKit?
     opera?
     firefox?

     rb-browser suffers from libav incompatibilities
     have to fix...

     in retune command:
     * service should be in the form "dvb://<network_id>..<service_id>", eg "dvb://233a..4C80"
     is car->network_id used somewhere in downloader?
     retune?
     only uses svc id it seems...(SIGHUP...)

   */


static void
mheg_add_pids (Mheg * db, uint16_t pnr)
{
  MhegSvc *s;
  /*
     better do mheg acq in a separate thread... 
     will have to start a thread for each pnr
     open a demux port, secex
     grab the dsmcc pids,
     and if valid, get the download..

     what about the events?
     can I miss any?
     could it get stuck?

     several tasks:

     one for pat, which starts pmt thread(when it gets the pid), and stops it on exit
     or restarts on pidchange(only need one of this in total)

     one for pmt, which starts downloader once it has the pids, and stops it on exit, 
     or restarts it on pidchange (need one for each service..)

     one for dsmcc reassembly/storage..


     here, I have to

     generate onid nodes... better do on tuning...
     might have to wait for sdt... blechh
     qwik anddirty for now
     can find it in NIS transport stream info...
     in SDS(1)
     EIS(1)
     sdt is in transponder.. but not always valid...
     could add onid to transp...
     but no way to generate from initial tuning file..
     go via SDT and pgmdb then... opening a demux just for this would be overkill...
     pgmdbGetonid

     put svc number in there

   */
  if (NULL == db->current_onid)
  {                             //cannot do if it is null..(should prolly have a thread listen for sdts too...)
    return;
  }
  s = mheg_get_or_ins_svc (db, db->current_onid, pnr);
  taskStart (&s->t);
  /*
     now there's the issue with the pmt_pid...
     could grab from pgmdb,  but really want to interpret pat directly('cause I can ract to changes/updates..)..
     either have to flush sec out bitmap to get all sections again
     or add parameter to not use bitmap...
     or get a new secout for each task in s->t..
     instead of main mheg thread...
   */
}

static void
mheg_rmv_pids (Mheg * db, uint16_t pnr)
{
  MhegSvc cv, *s;
  cv.pnr = pnr;
  BTreeNode *n;
  if (NULL == db->current_onid)
    return;
  n = btreeNodeFind (db->current_onid->svc, &cv, NULL, mheg_cmp_svc);
  if (NULL == n)
  {
    errMsg ("service node not found\n");
    return;
  }
  s = btreeNodePayload (n);
  taskStop (&s->t);

}

static void
stop_pat_trk (void *x NOTUSED, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    MhegSvc *s = btreeNodePayload (this);
    taskStop (&s->t);
    ocars_purge (s);
    clear_assoc (&s->assoc);
  }
}

static void
stop_all_threads (Mheg * db)
{
  if (NULL == db->current_onid)
    return;
  btreeWalk (db->current_onid->svc, NULL, stop_pat_trk);
}


static void
mheg_proc_evt (Mheg * db, DvbEvtS * d)
{
  TuneDta *tda;
  uint16_t pda;
  int32_t onid;
  int i;
  debugMsg ("processing event\n");

  switch (d->id)
  {
  case E_TUNING:
    debugMsg ("E_TUNING\n");
    stop_all_threads (db);
    db->current_onid = NULL;
    db->tuned = 0;
    break;
  case E_IDLE:
    debugMsg ("E_IDLE\n");
    stop_all_threads (db);
    db->current_onid = NULL;
    db->tuned = 0;
    break;
  case E_TUNED:                //tuning successful
    debugMsg ("E_TUNED\n");
    tda = &d->d.tune;
    i = 0;
    do
    {
      onid = pgmdbGetTsid (db->pgmdb, tda->pos, tda->freq, tda->pol);
      usleep (500000);
      i++;
    }
    while ((onid == -1) && (i < 10));

    if (onid >= 0)
    {
      debugMsg ("onid OK\n");
      db->current_onid = mheg_get_or_ins_onid (db, onid);
    }
    else
      debugMsg ("onid failed\n");
    db->tuned = 1;
    break;
  case E_CONFIG:               //reconfigured
    debugMsg ("E_CONFIG\n");
    break;
  case E_PNR_ADD:
    debugMsg ("E_PNR_ADD\n");
    pda = d->d.pnr;
    mheg_add_pids (db, pda);
    break;
  case E_PNR_RMV:
    debugMsg ("E_PNR_RMV\n");
    pda = d->d.pnr;
    mheg_rmv_pids (db, pda);
    break;
  case E_ALL_MHEG:
    debugMsg ("E_ALL_MHEG\n");
//    mheg_add_all_pids (db);
    break;
  default:
    break;
  }
}

static void *
mheg_store_loop (void *p)
{
  Mheg *db = p;
  int rv;
  rv = nice (db->niceness);
  debugPri (1, "nice returned %d\n", rv);
  taskLock (&db->t);
  while (taskRunning (&db->t))
  {
    taskUnlock (&db->t);
    rv = svtOutWaitData (db->o, 500);
    taskLock (&db->t);
    if (0 == rv)
    {
      void *d;
      d = svtOutReadElement (db->o);
      if (selectorEIsEvt (d))
      {
        debugPri (10, "event\n");
        mheg_proc_evt (db, (DvbEvtS *) selectorEPayload (d));
      }
      else
      {
        errMsg ("not an event\n");
//              abort();
      }
    }
    else
    {                           //timeout.. nothing...
      debugPri (10, "timedout\n");
    }
  }

  taskUnlock (&db->t);
  return NULL;
}

int
mhegIdle (Mheg * mheg)
{
  int rv;
  taskLock (&mheg->t);
  rv = !mheg->tuned;
  taskUnlock (&mheg->t);
  return rv;
}
