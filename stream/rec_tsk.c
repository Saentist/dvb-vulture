#include <unistd.h>
#include <string.h>
#include "utl.h"
#include "rec_tsk.h"
#include "server.h"
#include "pgmstate.h"
#include "in_out.h"
#include "dmalloc.h"
#include "ipc.h"
#include "debug.h"

int DF_REC_TSK;
#define FILE_DBG DF_REC_TSK

static void
rt_ot_start (RecTask * rt)
{
  if ((NULL == dlistFirst (&rt->p->conn_active)) && (!recTaskActive (rt)))
    offlineTaskStart (&rt->p->ot);
}

static void
rt_ot_stop (RecTask * rt)
{
  offlineTaskStop (&rt->p->ot);
}

static int
rt_check_tp (RecTask * rt, DListE * re)
{
  DListE *ae;                   //active thread element
  EitRec *at;                   //(one)active thread
  RsEntry *rse = dlistEPayload (re);
  ae = dlistFirst (&rt->active);
  while (ae)
  {
    DListE *next = dlistENext (ae);
    at = dlistEPayload (ae);
    if ((at->e->pos != rse->pos) || (at->e->pol != rse->pol)
        || (at->e->freq != rse->freq))
    {                           //different tp .. not ok
      return 1;
    }
    ae = next;
  }
  return 0;
}

static int
rt_abort_all (RecTask * rt)
{
  EitRec *at;                   //(one)active thread
  DListE *ae = dlistFirst (&rt->active);
  while (ae)
  {
    DListE *next = dlistENext (ae);
    at = dlistEPayload (ae);
    eitRecAbort (at);
    ae = next;
  }
  return 0;
}

static void
rt_done (EitRec * er, RsEntry * rse, void *x)
{
  RecTask *rt = x;
  DListE *ee;
  DListE *re;
  taskLock (&rt->t);
  ee = dlistPElement (er);
  re = dlistPElement (rse);
  dlistRemove (&rt->active, ee);
  dlistInsertLast (&rt->done, ee);
  rsDone (rse);
  if (RS_WAITING == rse->state)
  {                             //repeating event
    rsInsertEntry (&rt->sch.sched, dlistPElement (rse));
    rsWrite (&rt->sch);         //so it does not loose data on power outage...
  }
  else
  {
    rsClearEntry (rse);
    utlFAN (re);
  }
  taskUnlock (&rt->t);
}


static int
rt_start (RecTask * c, DListE * el)
{
  RsEntry *e = dlistEPayload (el);
  EitRec *er;
  DListE *ee;
  TransponderInfo t;

  //deactivate connection if one is active
  if (NULL != c->p->master)
    c->p->master->active = 0;
  c->p->master = NULL;


  //tune here
  if (pgmdbFindTransp (&c->p->program_database, &t, e->pos, e->freq, e->pol))
  {
    errMsg ("error: failed to find transponder\n");
    return 1;
  }
  if (dvbWouldTune (&c->p->dvb, e->pos, t.frequency, t.srate, t.pol, t.ftune))
  {
    pgmRmvAllPnrs ((PgmState *) c->p);
    if (srvTuneTpi (c->p, e->pos, &t))
    {
      errMsg ("tuning error\n");
      return 1;
    }
  }
  //allocate a recorder
  ee = utlCalloc (dlistESize (sizeof (EitRec)), 1);
  if (NULL == ee)
  {
    errMsg ("allocation error\n");
    return 1;
  }
  er = dlistEPayload (ee);
  if (eitRecScan (er, e, &c->p->conf, &c->p->dvb, &c->p->program_database, c, rt_done)) //start recorder/scan tables
  {
    errMsg ("scan error\n");
    utlFAN (ee);
    return 1;
  }
  //store recorder
  dlistInsertLast (&c->active, ee);
  //remove running event from schedule(EitRec owns it for now)
  dlistRemove (&c->sch.sched, el);
//      utlFAN(el);
  return 0;
}

/**
 *\brief takes care of timed recordings
 *
 */
void *
rec_task (void *x)
{
  RecTask *rt = x;
  taskLock (&rt->t);
  while (taskRunning (&rt->t))
  {
    RsEntry *e;
    DListE *f;
    DListE *de;
    taskUnlock (&rt->t);
    debugPri (2, "rec_task iteration\n");

    pthread_mutex_lock (&rt->p->conn_mutex);    //we wouldn't want a connection thread to interfere
    taskLock (&rt->t);          //ther order here is important

    f = dlistFirst (&rt->sch.sched);
    if (NULL != f)
    {
      e = dlistEPayload (f);
      while (f && rsPending (e))
      {
        DListE *next;
        if (rt_check_tp (rt, f))
        {                       //different tp
          taskUnlock (&rt->t);
          rt_abort_all (rt);    //stop active recorders
          taskLock (&rt->t);
        }
        next = dlistENext (f);  //get next element here, as it may not be available past the next line
        rt_ot_stop (rt);        //stop data acquisition
        if (rt_start (rt, f))
          rt_ot_start (rt);     //it failed. start again
        f = next;
        if (NULL != f)
          e = dlistEPayload (f);
      }
    }
    if (NULL != (de = dlistFirst (&rt->done)))
    {
      EitRec *d = dlistEPayload (de);
      dlistRemove (&rt->done, de);
      eitRecAbort (d);
      utlFAN (de);
      rt_ot_start (rt);
    }
    taskUnlock (&rt->t);
    pthread_mutex_unlock (&rt->p->conn_mutex);
    sleep (2);
    taskLock (&rt->t);
  }
  taskUnlock (&rt->t);
  return NULL;
}

int
recTaskInit (RecTask * rt, RcFile * cfg, PgmState * p)
{
  memset (rt, 0, sizeof (RecTask));
  if (taskInit (&rt->t, rt, rec_task))
  {
    errMsg ("taskInit failed\n");
    return 1;
  }
  if (rsInit (&rt->sch, cfg))
  {
    errMsg ("Failed to init schedule\n");
    taskClear (&rt->t);
    return 1;
  }
  rt->p = p;
  return 0;
}

void
recTaskClear (RecTask * rt)
{
  errMsg ("recTaskClear\n");
  taskClear (&rt->t);
  rt_abort_all (rt);
//      rec_task_stop_all(rt);
  rsClear (&rt->sch);
}

void
CUrecTaskClear (void *rt)
{
  recTaskClear (rt);
}

int
CUrecTaskInit (RecTask * rt, RcFile * cfg, PgmState * p, CUStack * s)
{
  int rv = recTaskInit (rt, cfg, p);
  cuStackFail (rv, rt, CUrecTaskClear, s);
  return rv;
}

int
recTaskActive (RecTask * rt)
{
  if (dlistFirst (&rt->active))
    return 1;
  return 0;
}

int
recTaskSndSched (int sock, RecTask * rt)
{
  DListE *f;
  RsEntry *e;
  uint32_t a;

  a = dlistCount (&rt->sch.sched);

  ipcSndS (sock, a);

  f = dlistFirst (&rt->sch.sched);
  while (f)
  {
    e = dlistEPayload (f);
    if (rsSndEntry (e, sock))
    {
      errMsg ("rsSndEntry Failed\n");
      return 1;
    }
    f = dlistENext (f);
  }
  return 0;
}

int
recTaskRcvSched (int sock, RecTask * rt)
{
  DListE *f;
  RsEntry *e;
  uint32_t a;
  ipcRcvS (sock, a);
  f = dlistFirst (&rt->sch.sched);
  while (f)
  {
    dlistRemove (&rt->sch.sched, f);
    e = dlistEPayload (f);
    rsClearEntry (e);
    utlFAN (f);
    f = dlistFirst (&rt->sch.sched);
  }

  while (a--)
  {
    f = utlCalloc (dlistESize (sizeof (RsEntry)), 1);
    e = dlistEPayload (f);
    if (rsRcvEntry (e, sock))
    {
      errMsg ("rsRcvEntry Failed\n");
      rsClearEntry (e);
      utlFAN (f);
      return 1;
    }
    rsInsertEntry (&rt->sch.sched, f);
  }
  rsWrite (&rt->sch);
  return 0;
}

int
recTaskRcvEntry (int sock, RecTask * rt)
{
  DListE *f;
  RsEntry *e;
  f = utlCalloc (dlistESize (sizeof (RsEntry)), 1);
  e = dlistEPayload (f);
  if (rsRcvEntry (e, sock))
  {
    errMsg ("rsRcvEntry Failed\n");
    rsClearEntry (e);
    utlFAN (f);
    return 1;
  }
  rsInsertEntry (&rt->sch.sched, f);
  rsWrite (&rt->sch);           //to disk
  return 0;
}

int
recTaskCancelRunning (RecTask * rt)
{
  DListE *ae;                   //active thread element
  EitRec *at;                   //(one)active thread
  taskLock (&rt->t);
  ae = dlistFirst (&rt->active);
  while (ae)
  {
    at = dlistEPayload (ae);
    taskUnlock (&rt->t);
    eitRecAbort (at);           //locks in callback(rt_done)
    taskLock (&rt->t);
    ae = dlistFirst (&rt->active);
  }
  taskUnlock (&rt->t);
  return 0;
}
