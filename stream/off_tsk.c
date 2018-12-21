#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "off_tsk.h"
#include "utl.h"
#include "pgmstate.h"
#include "tdt.h"
#include "server.h"
#include "dmalloc.h"
#include "debug.h"
int DF_OFF_TSK;
#define FILE_DBG DF_OFF_TSK

static void *offline_task (void *x);

int
offlineTaskInit (OfflineTask * rt, RcFile * cfg, struct PgmState_s *p)
{
  Section *s;
  long tmp;
  char *cp;
  memset (rt, 0, sizeof (OfflineTask));
  rt->p = p;
  debugMsg ("offlineTaskInit start\n");
  s = rcfileFindSec (cfg, "OFFLINE");
  rt->acq_duration = 120;
  rt->idle_duration = 24 * 3600;
  rt->adjust_interval = 0;
  rt->t_pos = 0;
  rt->t_freq = 0;
  if (s)
  {
    if (!rcfileFindSecValInt (s, "adjust_interval_hr", &tmp))
    {
      rt->adjust_interval = tmp * 3600;
    }

    if (!rcfileFindSecValInt (s, "acq_duration_s", &tmp))
    {
      rt->acq_duration = tmp;
    }

    if (!rcfileFindSecValInt (s, "idle_duration_hr", &tmp))
    {
      rt->idle_duration = tmp * 3600;
    }

    if (!rcfileFindSecValInt (s, "t_pos", &tmp))
    {
      rt->t_pos = tmp;
    }
    if (!rcfileFindSecValInt (s, "t_freq_khz", &tmp))
    {
      rt->t_freq = tmp / 10;
    }
    if (!rcfileFindSecVal (s, "t_pol", &cp))
    {
      switch (cp[0])
      {
      case 'h':
      case 'H':
        rt->t_pol = 0;
        break;
      case 'v':
      case 'V':
        rt->t_pol = 1;
        break;
      case 'l':
      case 'L':
        rt->t_pol = 2;
        break;
      case 'r':
      case 'R':
        rt->t_pol = 3;
        break;
      default:
        break;
      }
    }
  }
  if (taskInit (&rt->t, rt, offline_task))
  {
    errMsg ("task failed to init\n");
    return 1;
  }
  return 0;
}

void
offlineTaskClear (OfflineTask * rt)
{
  taskClear (&rt->t);
}

void
CUofflineTaskClear (void *rt)
{
  offlineTaskClear (rt);
}

int
CUofflineTaskInit (OfflineTask * rt, RcFile * cfg, struct PgmState_s *p,
                   CUStack * s)
{
  int rv = offlineTaskInit (rt, cfg, p);
  cuStackFail (rv, rt, CUofflineTaskClear, s);
  return rv;
}

static void
offline_task_release_pk (void *d, void *p)
{
  selectorReleaseElement ((Selector *) d, selectorPElement (p));
}

static int
offline_task_adj_time (OfflineTask * rt)
{
  uint8_t *tdt;
  time_t tdt_time, current_time;
  DListE *port;
  uint16_t pid;
  SvcOut *o;
  TransponderInfo t;
  int rv;
  SecEx tdt_ex;
  //tune here. we don't run if a connection is active, so no need to lock
  if (pgmdbFindTransp
      (&rt->p->program_database, &t, rt->t_pos, rt->t_freq, rt->t_pol))
    return 1;
  pgmRmvAllPnrs (rt->p);
  debugMsg ("Tuning\n");
  if (srvTuneTpi (rt->p, rt->t_pos, &t))
  {
    errMsg ("tuning error\n");
    return 1;
  }
  //open ts port for TDT pid 0x14
  pid = 0x14;
  debugMsg ("getting Ts output\n");
  o = dvbDmxOutAdd (&rt->p->dvb);
  dvbDmxOutMod (o, 0, NULL, NULL, 1, &pid);
  port = svtOutSelPort (o);     //dvbGetTsOutput (&rt->p->dvb, &pid, 1);
  if (NULL == port)
    return 1;
  debugMsg ("secExinit\n");

  //init sec_ex
  if (secExInit (&tdt_ex, svtOutSelector (o), offline_task_release_pk))
  {
    dvbDmxOutRmv (o);
//    dvbTsOutputClear (&rt->p->dvb, port);
    return 1;
  }
  debugMsg ("while\n");
  //get section
  tdt = NULL;
  while (!tdt)
  {
    rv = selectorWaitData (port, 60005);        //Yeah... probably too much. should really be async.
    if (rv)
    {
      debugMsg ("secExWaitData failed\n");
      secExClear (&tdt_ex);
      dvbDmxOutRmv (o);
//      dvbTsOutputClear (&rt->p->dvb, port);
      return 1;
    }
    else
    {
      void *d;
      if ((d = selectorReadElement (port)))
      {
        if (selectorEIsEvt (d))
        {
          selectorReleaseElement (svtOutSelector (o), d);
        }
        else
        {
          SecBuf const *sb;
          sb = secExPut (&tdt_ex, selectorEPayload (d));
          if (sb)
            tdt = (uint8_t *) sb->data;
        }
      }
    }
  }
  //parse section
  tdt_time = tdtParse (tdt);
  //cleanup
  secExClear (&tdt_ex);
  dvbDmxOutRmv (o);
//  dvbTsOutputClear (&rt->p->dvb, port);
  //adjust time
  if (-1 != tdt_time)
  {
    struct timeval corr;
    current_time = time (NULL);
    debugMsg ("TDT: %d, CURRENT: %d\n", tdt_time, current_time);
/*		corr.tv_sec=tdt_time-current_time;
		corr.tv_usec=0;
		
		if(0!=adjtime(&corr,NULL))
			errMsg("adjtime failed: %s\n",strerror(errno));*/
    corr.tv_sec = tdt_time;
    corr.tv_usec = 0;
    if (0 != settimeofday (&corr, NULL))
    {
      errMsg ("settimeofday failed: %s\n", strerror (errno));
    }
  }
  //return
  return 0;
}


int
offline_task_vt (OfflineTask * rt)
{
  int num_tp;
  if (rt->current_tp >= rt->num_tp)
  {
    debugMsg ("rt->current_tp>=rt->num_tp\n");

    if (rt->current_pos >= rt->num_pos)
    {
      debugMsg ("rt->current_pos>=rt->num_pos\n");
      utlFAN (rt->tpi);
      rt->tpi = NULL;
      rt->current_tp = 0;
      rt->num_tp = 0;
      rt->num_pos = 0;
      rt->current_pos = 0;
      return 1;
    }
    else
    {
      debugMsg ("else\n");
      utlFAN (rt->tpi);
      rt->tpi = NULL;
      do
      {
        rt->current_pos++;
        if (rt->current_pos >= rt->num_pos)
        {
          debugMsg ("rt->current_pos>=rt->num_pos\n");
          rt->current_tp = 0;
          rt->num_tp = 0;
          rt->num_pos = 0;
          rt->current_pos = 0;
          return 1;
        }
        rt->tpi =
          pgmdbListTransp (&rt->p->program_database, rt->current_pos,
                           &num_tp);
        rt->num_tp = num_tp;
        rt->current_tp = 0;
      }
      while (NULL == rt->tpi);
    }
  }
  debugMsg ("ok\n");
  return 0;
}

int
offline_task_vt_init (OfflineTask * rt)
{
  uint32_t num = 0;
//      SwitchPos *sp=NULL;
  int num_tp;

  dvbGetSwitch (&rt->p->dvb, &num);
  rt->num_pos = num;
  rt->current_pos = 0;
  if (rt->num_pos == 0)
    return 1;

  rt->tpi = NULL;
  rt->tpi =
    pgmdbListTransp (&rt->p->program_database, rt->current_pos, &num_tp);
  rt->num_tp = num_tp;
  rt->current_tp = 0;

  while (NULL == rt->tpi)
  {
    rt->current_pos++;
    if (rt->current_pos >= rt->num_pos)
      return 1;

    rt->tpi =
      pgmdbListTransp (&rt->p->program_database, rt->current_pos, &num_tp);
    rt->num_tp = num_tp;
    rt->current_tp = 0;
  }
  return 0;
}

/**
 *\brief does background work
 *
 */
static void *
offline_task (void *x)
{
  OfflineTask *rt = x;
  rt->state = 0;
  rt->current_tp = 0;
  rt->num_tp = 0;
  rt->num_pos = 0;
  rt->current_pos = 0;
  taskLock (&rt->t);
  while (taskRunning (&rt->t))
  {
    time_t current_time;
    debugMsg ("offline_task iteration\n");
    current_time = time (NULL);
    switch (rt->state)
    {
    case 0:
      if ((rt->adjust_interval != 0)
          && ((current_time - rt->last_adjust) > rt->adjust_interval))
      {
        offline_task_adj_time (rt);
        rt->last_adjust = time (NULL);
      }
      if (rt->acq_duration != 0)
      {
        rt->state = 1;
      }
      break;
    case 1:
      //start vt acquisition
      if (!offline_task_vt_init (rt))
      {
        if (!srvTuneTpi (rt->p, rt->current_pos, &rt->tpi[rt->current_tp]))
        {
          pgmdbScanTp (&rt->p->program_database);
#ifdef WITH_VT
          selectorSndEv (&rt->p->dvb.ts_dmx.s, E_ALL_VT);
#endif
        }
        rt->acq_start = current_time;
        rt->current_tp++;
        rt->state = 2;
      }
      else
      {
        rt->state = 0;
      }

      break;
    case 2:
      //next tp
      if (current_time > (rt->acq_start + rt->acq_duration))
      {
        if (!offline_task_vt (rt))
        {
          debugMsg ("tuning\n");
          if (!srvTuneTpi (rt->p, rt->current_pos, &rt->tpi[rt->current_tp]))
          {
            debugMsg ("scanning\n");
            pgmdbScanTp (&rt->p->program_database);
#ifdef WITH_VT
            debugMsg ("E_ALL_VT\n");
            selectorSndEv (&rt->p->dvb.ts_dmx.s, E_ALL_VT);
#endif
          }
          else
            debugMsg ("tuning failed\n");
          rt->acq_start = current_time;
          rt->current_tp++;
        }
        else
        {                       //acquisition complete(tried all transponders)
          if (0 != rt->idle_duration)
          {
            dvbUnTune (&rt->p->dvb);
            while (!pgmdbUntuned (&rt->p->program_database))
            {
              debugPri (1, "pgmdbUntuned\n");
              usleep (500000);
            }
            rt->idle_start = current_time;
            rt->state = 3;
          }
          else
            rt->state = 0;
        }
      }
      break;
    case 3:
      if (current_time > (rt->idle_start + rt->idle_duration))
        rt->state = 0;
      break;
    default:
      break;
    }
    taskUnlock (&rt->t);
    sleep (2);
    taskLock (&rt->t);
  }
  dvbUnTune (&rt->p->dvb);
  while (!pgmdbUntuned (&rt->p->program_database))
  {
    debugPri (1, "pgmdbUntuned\n");
    usleep (500000);
  }
  taskUnlock (&rt->t);
  utlFAN (rt->tpi);
  rt->tpi = NULL;
  return NULL;
}
