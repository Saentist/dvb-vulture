#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "rs_entry.h"
#include "in_out.h"
#include "ipc.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"
int DF_RS_ENTRY;
#define FILE_DBG DF_RS_ENTRY

static int
WR_RSE (int d, RsEntry * e)
{
  uint8_t t1, t3;
  int64_t t2, t4;
  int rv;
  t1 = e->state;
  t2 = e->start_time;
  t3 = e->type;
  t4 = e->duration;
  rv = (ipcSndS (d, t1) ||
        ipcSndS (d, t3) ||
        ipcSndS (d, e->wd) ||
        ipcSndS (d, e->flags) ||
        ipcSndS (d, e->pos) ||
        ipcSndS (d, e->pol) ||
        ipcSndS (d, e->freq) ||
        ipcSndS (d, e->pnr) ||
        ipcSndS (d, t2) || ipcSndS (d, t4) || ipcSndS (d, e->evt_id));
  return rv;
}

static int
RD_RSE (int d, RsEntry * e)
{
  uint8_t t1, t3;
  int64_t t2, t4;
  int rv;
  rv = (ipcRcvS (d, t1) ||
        ipcRcvS (d, t3) ||
        ipcRcvS (d, e->wd) ||
        ipcRcvS (d, e->flags) ||
        ipcRcvS (d, e->pos) ||
        ipcRcvS (d, e->pol) ||
        ipcRcvS (d, e->freq) ||
        ipcRcvS (d, e->pnr) ||
        ipcRcvS (d, t2) || ipcRcvS (d, t4) || ipcRcvS (d, e->evt_id));
  e->state = t1;
  e->type = t3;
  e->start_time = t2;
  e->duration = t4;
  return rv;
}

int
rsSndEntry (RsEntry * sch, int fd)
{
  if (WR_RSE (fd, sch))
    return 1;
  ipcSndStr (fd, sch->name);
  return 0;
}

int
rsRcvEntry (RsEntry * sch, int fd)
{
  if (RD_RSE (fd, sch))
    return 1;
  sch->name = ipcRcvStr (fd);
  return 0;
}

int
rsPending (RsEntry * e)
{
  time_t now;
  if (e->state == RS_WAITING)
  {
    now = time (NULL);
    if (((now >= (e->start_time - 2 * 60))))    //&&(now<(e->start_time+e->duration+2*60)))
      return 1;
  }
  return 0;
}

int
rsStart (RsEntry * e)
{
  if (e->state == RS_WAITING)
  {
    e->state = RS_RUNNING;
    return 0;
  }
  return 1;
}

int
rsIsRunning (RsEntry * e)
{
  return e->state == RS_RUNNING;
}

time_t
rs_next_month (time_t origin)
{
  struct tm bdtm;
  localtime_r (&origin, &bdtm);
  bdtm.tm_mon++;
  return mktime (&bdtm);
}

time_t
rs_next_4week (time_t origin)
{
  struct tm bdtm;
  localtime_r (&origin, &bdtm);
  bdtm.tm_mday += 28;           //sorry, no leap.. whatevers
  return mktime (&bdtm);
}

time_t
rs_next_3week (time_t origin)
{
  struct tm bdtm;
  localtime_r (&origin, &bdtm);
  bdtm.tm_mday += 21;           //sorry, no leap.. whatevers
  return mktime (&bdtm);
}

time_t
rs_next_2week (time_t origin)
{
  struct tm bdtm;
  localtime_r (&origin, &bdtm);
  bdtm.tm_mday += 14;           //sorry, no leap.. whatevers
  return mktime (&bdtm);
}

time_t
rs_next_week (time_t origin)
{
  struct tm bdtm;
  localtime_r (&origin, &bdtm);
  bdtm.tm_mday += 7;            //sorry, no leap.. whatevers
  return mktime (&bdtm);
}

time_t
rs_next_day (uint8_t wd, time_t origin)
{
  int i;
  struct tm bdtm;
  if (!(wd & 0x7f))             //no days
    return 0;
  localtime_r (&origin, &bdtm);

  for (i = 0; i < 7; i++)
  {
    bdtm.tm_wday++;
    bdtm.tm_mday++;
    if (bdtm.tm_wday == 7)
      bdtm.tm_wday = 0;
    if ((1 << bdtm.tm_wday) & wd)
    {
      return mktime (&bdtm);
    }
  }
  return 0;
}

/*
time_t rs_next_wd(time_t origin)
{
	struct tm bdtm;
	localtime_r(&origin,&bdtm);
	if(bdtm.tm_wday==5)//Friday
	{//skip to Monday
		bdtm.tm_mday+=3;
	}
	else
		bdtm.tm_mday++;
	return mktime(&bdtm);
}

time_t rs_next_wd_sa(time_t origin)
{
	struct tm bdtm;
	localtime_r(&origin,&bdtm);
	if(bdtm.tm_wday==6)//Saturday
	{//skip to Monday
		bdtm.tm_mday+=2;
	}
	else
		bdtm.tm_mday++;
	return mktime(&bdtm);
}
*/
time_t
rsNextTime (RSType tp, uint8_t wd, time_t t)
{
  switch (tp)
  {
  case RST_ONCE:
    return 0;
  case RST_DAY:
    return rs_next_day (wd, t);
  case RST_WEEK:
    return rs_next_week (t);
  case RST_2WEEK:
    return rs_next_2week (t);
  case RST_3WEEK:
    return rs_next_3week (t);
  case RST_4WEEK:
    return rs_next_4week (t);
  case RST_MONTH:
    return rs_next_month (t);
  default:
    return 0;
  }
  return 0;
}

int
rsDone (RsEntry * e)
{
  if (e->state == RS_RUNNING)
  {
    switch (e->type)
    {
    case RST_ONCE:
      e->state = RS_IDLE;
      return 0;
    default:
      e->state = RS_WAITING;
      e->start_time = rsNextTime (e->type, e->wd, e->start_time);
      if (e->start_time == 0)
      {
        e->state = RS_IDLE;
        return 1;
      }
      return 0;
    }
  }
  return 1;
}

int
rsStopping (RsEntry * e)
{
  time_t now = time (NULL);
  if (e->state == RS_RUNNING)
  {
    if (now >= (e->start_time + e->duration + 2 * 60))
    {
      return 1;
    }
  }
  return 0;
}

RsEntry *
rsCloneSched (RsEntry * src, uint32_t src_sz)
{
  RsEntry *rv;
  uint32_t i;
  rv = utlCalloc (src_sz, sizeof (RsEntry));
  if (NULL == rv)
    return NULL;

  for (i = 0; i < src_sz; i++)
  {
    rsClone (rv + i, src + i);
  }
  return rv;
}

void
rsClearEntry (RsEntry * sch)
{
  utlFAN (sch->name);
  memset (sch, 0, sizeof (RsEntry));
}

static char *rs_st_strings[] = {
  "RS_IDLE   ",
  "RS_WAITING",
  "RS_RUNNING"
};

static char *rs_tp_strings[] = {
  "RST_ONCE ",
  "RST_DAY  ",
  "RST_WEEK ",
  "RST_2WEEK",
  "RST_3WEEK",
  "RST_4WEEK",
  "RST_MONTH"
};

static char *rs_wd_strings[] = {
  "Sunday   ",
  "Monday   ",
  "Tuesday  ",
  "Wednesday",
  "Thursday ",
  "Friday   ",
  "Saturday "
};

static char *rs_flag_strings[] = {
  "SKIP_VT   ",
  "SKIP_SPU  ",
  "SKIP_DSM  ",
  "ONLY_AUDIO",
  "USE_EIT   ",
};

char const *
rsGetStateStr (uint8_t state)
{
  return struGetStr (rs_st_strings, state);
}

char const *
rsGetTypeStr (uint8_t type)
{
  return struGetStr (rs_tp_strings, type);
}

char const *
rsGetWdStr (uint8_t wd)
{
  return struGetStr (rs_wd_strings, wd);
}

char const *
rsGetFlagStr (uint8_t flag)
{
  return struGetStr (rs_flag_strings, flag);
}

int
rsNeq (RsEntry * a, RsEntry * b)
{
  if ((a->state != b->state) || (a->type != b->type) || (a->wd != b->wd) || (a->flags != b->flags) || (a->pos != b->pos) || (a->pol != b->pol) || (a->freq != b->freq) || (a->pnr != b->pnr) || (a->start_time != b->start_time) || (a->duration != b->duration) || (a->evt_id != b->evt_id) || ((a->name == NULL) && (b->name != NULL)) || ((a->name != NULL) && (b->name == NULL)) || (((a->name != NULL) && (b->name != NULL)) && (strcmp (a->name, b->name))        //will this segfault on NULLs?
      ))
    return 1;
  return 0;
}

int
rsClone (RsEntry * dest, RsEntry * src)
{
  *dest = *src;
  if (NULL != src->name)
  {
    dest->name = strdup (src->name);
  }
  return 0;
}

void
rsDestroy (RsEntry * sch, uint32_t sz)
{
  uint32_t i;
  for (i = 0; i < sz; i++)
  {
    rsClearEntry (sch + i);
  }
  if (sz)
    utlFAN (sch);
}

int
rsNeqSched (RsEntry * a, uint32_t a_sz, RsEntry * b, uint32_t b_sz)
{
  uint32_t i;
  if (a_sz != b_sz)
    return 1;
  for (i = 0; i < a_sz; i++)
  {
    if (rsNeq (a + i, b + i))
    {
      return 2;
    }
  }
  return 0;
}
