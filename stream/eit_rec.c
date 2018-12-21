#include <string.h>
#include <inttypes.h>
#include "eit_rec.h"
#include "eis.h"
#include "utl.h"
#include "rs_entry.h"
#include "epg_evt.h"
#include "task.h"
#include "dmalloc.h"
#include "debug.h"

int DF_EIT_REC;
#define FILE_DBG DF_EIT_REC

#define EIT_DONE    0           ///<recording finished or interrupted.
#define EIT_SCAN    1           ///<wait for event
#define EIT_RUNNING 2           ///<recording active
#define EIT_PAUSE   3           ///<May be restarted.
#define EIT_TC      4           ///<TIMER control. no eit seen for 60 seconds
#define EIT_NO_EIT  5           ///<TIMER control only. No eit.

static void *eit_rec_loop (void *p);
static void *plain_rec_loop (void *p);

void
eit_rec_release (void *d, void *p)
{
  selectorReleaseElementUnsafe ((Selector *) d, selectorPElement (p));
}

int
eitRecScan (EitRec * r, RsEntry * e, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb,
            void *x, void (*done_cb) (EitRec * r, RsEntry * e, void *x))
{
  DListE *eit_port;
  SvcOut *o;
  uint16_t epg_pid = 0x0012;

  if (!(r->eit_state == EIT_DONE))
    return 1;

  memset (r, 0, sizeof (EitRec));

  r->done_cb = done_cb;
  r->x = x;
  r->start = time (NULL);
  r->e = e;
  r->cfg = cfg;
  r->dvb = dvb;
  r->pgmdb = pgmdb;
  if (taskInit
      (&r->t, r, (e->flags & RSF_EIT) ? eit_rec_loop : plain_rec_loop))
  {
    return 1;
  }

  r->eit_port = NULL;
  r->eit_sel = NULL;
  r->eit_state = EIT_SCAN;
  if (e->flags & RSF_EIT)
  {
    o = dvbDmxOutAdd (dvb);
    eit_port = svtOutSelPort (o);
    if ((NULL == eit_port)
        || secExInit (&r->eit_ex, svtOutSelector (o), eit_rec_release))
    {
      errMsg ("error initializing EitRec\n");
      return 1;
    }
    dvbDmxOutMod (o, 0, NULL, NULL, 1, &epg_pid);
    r->o = o;
    r->eit_port = eit_port;
    r->eit_sel = &dvb->ts_dmx.s;
  }
  debugMsg ("starting task\n");
  taskStart (&r->t);
  return 0;
}


static void
eit_rec_process (EitRec * r, uint16_t id NOTUSED, uint8_t rst)
{
  r->last_eit_seen = time (NULL);
  switch (r->eit_state)
  {
  case EIT_DONE:
    break;
  case EIT_SCAN:
    switch (rst)
    {
    case EIT_RST_SOON:
    case EIT_RST_RUNNING:
      if (0 ==
          recorderStart (&r->rec, r->e->name, r->cfg, r->dvb, r->pgmdb,
                         r->e->pnr, r->e->flags))
        r->eit_state = EIT_RUNNING;
      break;
    default:
      break;
    }
    break;
  case EIT_RUNNING:
    switch (rst)
    {
    case EIT_RST_PAUSED:
    case EIT_RST_OFF:
      r->eit_state = EIT_PAUSE;
      recorderPause (&r->rec);
      break;
    case EIT_RST_NOT:
      debugMsg ("seen RST_NOT\n");
      recorderStop (&r->rec);
      r->eit_state = EIT_DONE;
      break;
    default:
      break;
    }
    break;
  case EIT_PAUSE:
    switch (rst)
    {
    case EIT_RST_RUNNING:
      r->eit_state = EIT_RUNNING;
      recorderResume (&r->rec);
      break;
    case EIT_RST_NOT:
      r->eit_state = EIT_DONE;
      recorderStop (&r->rec);
      break;
    default:
      break;
    }
    break;
  case EIT_TC:                 //running under timer control
    switch (rst)
    {
    case EIT_RST_RUNNING:
      r->eit_state = EIT_RUNNING;
      break;
    case EIT_RST_NOT:
      r->eit_state = EIT_DONE;
      recorderStop (&r->rec);
      break;
    case EIT_RST_PAUSED:
    case EIT_RST_OFF:
      r->eit_state = EIT_PAUSE;
      recorderPause (&r->rec);
      break;
    default:
      break;
    }
    break;
  case EIT_NO_EIT:
    break;
  default:
    break;
  }
}

static void
eit_rec_proc_evt (EitRec * r, void *d)
{
  debugMsg ("processing event\n");
  switch (selectorEvtId (d))
  {
  case E_TUNING:
    r->eit_state = EIT_DONE;
    break;
  case E_CONFIG:               //reconfigured
    r->eit_state = EIT_DONE;
    break;
  default:
    break;
  }
}

static void
eit_rec_proc_eis (EitRec * r, uint8_t * data)
{
  Iterator iter;
  uint16_t id;
  uint8_t rst;
  uint8_t *ev_start;
  if (iterEisSecInit (&iter, data))
    return;
  if (!(ev_start = iterGet (&iter)))
  {
    iterClear (&iter);
    return;
  }

  r->last_eit_seen = time (NULL);
  do
  {
    id = evtGetId (ev_start);   //got weird id here, perhaps section parser is broken
    debugMsg ("Got Id: %" PRIu16 "\n", id);
    rst = evtGetRst (ev_start);
    if (id == r->e->evt_id)
    {
      r->last_id_seen = time (NULL);
      selectorUnlock (r->eit_sel);
      eit_rec_process (r, id, rst);
      selectorLock (r->eit_sel);
    }
  }
  while (!iterAdvance (&iter));
  iterClear (&iter);
}

static void *
eit_rec_loop (void *p)
{
  EitRec *r = p;
  int rv;
  time_t now;
  r->last_eit_seen = r->last_id_seen = time (NULL);
  taskLock (&r->t);
  r->e->state = RS_RUNNING;
  while (taskRunning (&r->t) && (r->eit_state != EIT_DONE))
  {
    taskUnlock (&r->t);
//              debugMsg("A\n");
    rv = selectorWaitData (r->eit_port, 1000);
    taskLock (&r->t);
    if (rv)
    {
    }
    else
    {                           //there's data
      void *e;
      selectorLock (r->eit_sel);
      while ((e = selectorReadElementUnsafe (r->eit_port)))
      {
        if (!selectorEIsEvt (e))
        {
          SecBuf const *sb;
          uint8_t tid;
          sb = secExPut (&r->eit_ex, selectorEPayload (e));
          if (sb)
          {
            tid = *((uint8_t *) sb->data);
            if ((tid == 0x4e)
                && (r->e->pnr == get_sec_pnr ((uint8_t *) sb->data)))
            {
              eit_rec_proc_eis (r, (uint8_t *) sb->data);
            }                   //3:11:16-3:15:22    4:6
          }
        }
        else
        {
          eit_rec_proc_evt (r, e);
          selectorReleaseElementUnsafe (r->eit_sel, e);
        }                       //e39b,eb9b
      }
      selectorUnlock (r->eit_sel);
    }
    now = time (NULL);

    if (((r->last_eit_seen + 60) < now) && (r->eit_state == EIT_SCAN))
    {
      errMsg ("No eit within 60 seconds. Starting timed mode\n");
      if (0 ==
          recorderStart (&r->rec, r->e->name, r->cfg, r->dvb, r->pgmdb,
                         r->e->pnr, r->e->flags))
        r->eit_state = EIT_TC;
    }

    if ((r->last_id_seen + 60) < now)
    {
      if (((r->eit_state == EIT_RUNNING) || (r->eit_state == EIT_PAUSE))
          && (now >= (r->e->start_time + r->e->duration + 60)))
      {
        errMsg
          ("recording active, evt id 0x%hx not seen for a minute and end time exceeded by a minute. stopping\n",
           r->e->evt_id);
        r->eit_state = EIT_DONE;
        recorderStop (&r->rec);
      }
    }

    if ((r->last_id_seen + 3600) < now)
    {
      if (r->eit_state == EIT_SCAN)
      {
        errMsg
          ("Scanned for an hour, without seeing event id 0x%hx. stopping\n",
           r->e->evt_id);
        r->eit_state = EIT_DONE;
      }
    }

    if ((r->eit_state == EIT_TC)
        && (now >= (r->e->start_time + r->e->duration + 2 * 60)))
    {
      debugMsg ("EIT_TC ended.\n");
      recorderStop (&r->rec);
      r->eit_state = EIT_DONE;
    }
  }
  taskUnlock (&r->t);

  selectorLock (r->eit_sel);
  secExClear (&r->eit_ex);
  selectorUnlock (r->eit_sel);

  dvbDmxOutRmv (r->o);
  r->o = NULL;
  r->eit_port = NULL;
  if ((r->eit_state == EIT_TC) || (r->eit_state == EIT_RUNNING)
      || (r->eit_state == EIT_PAUSE))
  {
    recorderStop (&r->rec);
  }
  r->done_cb (r, r->e, r->x);
  return NULL;
}

static void *
plain_rec_loop (void *p)
{
  EitRec *r = p;
  time_t now;
  time_t end = r->e->start_time + r->e->duration + 2 * 60;
  taskLock (&r->t);
  r->e->state = RS_RUNNING;
  //this may fail , so we retry
  r->eit_state = EIT_SCAN;
  now = time (NULL);
  while (taskRunning (&r->t) && (now < end))
  {
    switch (r->eit_state)
    {
    case EIT_SCAN:
      if (!recorderStart
          (&r->rec, r->e->name, r->cfg, r->dvb, r->pgmdb, r->e->pnr,
           r->e->flags))
        r->eit_state = EIT_TC;
      break;
    default:
      break;
    }
    taskUnlock (&r->t);
    sleep (2);
    now = time (NULL);
    taskLock (&r->t);
  }
  if (r->eit_state == EIT_TC)
    recorderStop (&r->rec);
  taskUnlock (&r->t);
  r->done_cb (r, r->e, r->x);
  return NULL;
}

///hard stop
int
eitRecAbort (EitRec * r)
{
  taskClear (&r->t);
  r->eit_state = EIT_DONE;
  return 0;
}

///returns zero if not active
int
eitRecActive (EitRec * r)
{
  return (r->eit_state != EIT_DONE);
}
