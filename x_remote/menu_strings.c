#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "timestamp.h"
#include "utl.h"
#include "menu_strings.h"
#include "reminder.h"
#include "dmalloc.h"
#include "debug.h"
int DF_MNU_STRINGS;
#define FILE_DBG DF_MNU_STRINGS

void
mnuStrAsciiSanitize (char *p)
{
  while (*p)
  {
    if (!isascii (*p))
      *p = ' ';
    p++;
  }
}

char *
mnuStrSvc (uint8_t svc_type)
{
  switch (svc_type)
  {
  case 1:
    return "T";
  case 2:
  case 10:
    return "R";
  case 17:
  case 22:
  case 25:
    return "T";
  default:
    return "?";
  }
}

void
mnuStrFree (char **s, int sz)
{
  int i;
  for (i = 0; i < sz; i++)
  {
    free (s[i]);
  }
  free (s);
}

char **
mnuStrFav (Favourite * full, int sz)
{
  char **rv;
  int i;
  rv = calloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    char *p;
    char *s_name = NULL;
    char *p_name = NULL;
    char *b_name = NULL;
    rv[i] = calloc (100, sizeof (char));
    if (!rv[i])
    {
      mnuStrFree (rv, i);
      return NULL;
    }
    s_name = dvbStringToAscii (&full[i].service_name);
    p_name = dvbStringToAscii (&full[i].provider_name);
    b_name = dvbStringToAscii (&full[i].bouquet_name);
    /*
       realsz=snprintf(rv[i],100,"%3u,%5.5u %s, %5hu, %s, %s, %s, %s, %s",
       full[i].freq/100000,full[i].freq%100000,
       tpiGetPolStr(full[i].pol),
       full[i].pnr,
       mnuStrSvc(full[i].type),
       full[i].ca_mode?"PAY":"FTA",
       s_name,
       p_name,
       b_name);
     */
    realsz = snprintf (rv[i], 100, "%s", s_name);
    if (realsz >= 100)
    {
      void *np;
      np = realloc (rv[i], realsz + 1);
      if (!np)
      {
        mnuStrFree (rv, i);
        free (s_name);
        free (p_name);
        free (b_name);
        return NULL;
      }
      rv[i] = np;
      /*
         realsz=snprintf(rv[i],realsz,"%3u,%5.5u %s, %5hu, %s, %s, %s, %s, %s",
         full[i].freq/100000,full[i].freq%100000,
         tpiGetPolStr(full[i].pol),
         full[i].pnr,
         mnuStrSvc(full[i].type),
         full[i].ca_mode?"PAY":"FTA",
         s_name,
         p_name,
         b_name);
       */
      realsz = snprintf (rv[i], 100, "%s", s_name);
    }
    free (s_name);
    free (p_name);
    free (b_name);
    p = rv[i];
    utlAsciiSanitize (p);
  }
  return rv;
}

static char *mnu_st_strings[] = {
  "I",
  "W",
  "R"
};

static char *mnu_tp_strings[] = {
  "Onc",
  "Day",
  "07d",
  "14d",
  "21d",
  "28d",
  "Mnt"
};

char **
mnuStrRse (RsEntry * rse, int sz)
{
  char **rv;
  int i;
  char tbuf[40];
  rv = calloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    char *p;
    if (NULL == ctime_r (&rse[i].start_time, tbuf))
    {
      mnuStrFree (rv, i);
      return NULL;
    }
    //ctime fumbles in a newline there. remove it!
    p = strchr (tbuf, '\n');
    if (NULL != p)
    {
      p[0] = '\0';
    }
    rv[i] = calloc (100, sizeof (char));
    if (!rv[i])
    {
      mnuStrFree (rv, i);
      return NULL;
    }
    realsz =
      snprintf (rv[i], 100,
                "%s, %s, %1.1u, %3.3u,%5.5u %s, %5.5hu, %s, %2.2u:%2.2u:%2.2u",
                struGetStr (mnu_st_strings, rse[i].state),
                struGetStr (mnu_tp_strings, rse[i].type), rse[i].pos,
                rse[i].freq / 100000, rse[i].freq % 100000,
                tpiGetPolStr (rse[i].pol), rse[i].pnr, tbuf,
                (unsigned) rse[i].duration / 3600,
                (unsigned) (rse[i].duration / 60) % 60,
                (unsigned) rse[i].duration % 60);
    if (realsz >= 100)
    {
      void *np;
      np = realloc (rv[i], realsz + 1);
      if (!np)
      {
        mnuStrFree (rv, i);
        return NULL;
      }
      rv[i] = np;
      realsz =
        snprintf (rv[i], 100,
                  "%s, %s, %1.1u, %3.3u,%5.5u %s, %5.5hu, %s, %2.2u:%2.2u:%2.2u",
                  struGetStr (mnu_st_strings, rse[i].state),
                  struGetStr (mnu_tp_strings, rse[i].type), rse[i].pos,
                  rse[i].freq / 100000, rse[i].freq % 100000,
                  tpiGetPolStr (rse[i].pol), rse[i].pnr, tbuf,
                  (unsigned) rse[i].duration / 3600,
                  (unsigned) (rse[i].duration / 60) % 60,
                  (unsigned) rse[i].duration % 60);
    }
    p = rv[i];
  }
  return rv;
}

char **
mnuStrPgms (ProgramInfo * full, int sz)
{
  char **rv;
  int i;
  if (NULL == full)
    return NULL;
  rv = calloc (sz, sizeof (char *));
  if (NULL == rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
//              char * p;
    char *s_name = NULL;
    char *p_name = NULL;
    char *b_name = NULL;
    rv[i] = calloc (100, sizeof (char));
    if (!rv[i])
    {
      mnuStrFree (rv, i);
      return NULL;
    }
    s_name = dvbStringToAscii (&full[i].service_name);
    p_name = dvbStringToAscii (&full[i].provider_name);
    b_name = dvbStringToAscii (&full[i].bouquet_name);
    realsz = snprintf (rv[i], 100, "%5hu, %s, %s, %s, %s, %s",
                       full[i].pnr,
                       mnuStrSvc (full[i].svc_type),
                       full[i].ca_mode ? "PAY" : "FTA",
                       s_name, p_name, b_name);
    if (realsz >= 100)
    {
      void *np;
      np = realloc (rv[i], realsz + 1);
      if (!np)
      {
        mnuStrFree (rv, i);
        free (s_name);
        free (p_name);
        free (b_name);
        return NULL;
      }
      rv[i] = np;
      realsz = snprintf (rv[i], realsz, "%5hu, %s, %s, %s, %s, %s",
                         full[i].pnr,
                         mnuStrSvc (full[i].svc_type),
                         full[i].ca_mode ? "PAY" : "FTA",
                         s_name, p_name, b_name);
    }
    free (s_name);
    free (p_name);
    free (b_name);
    //p=rv[i];
    mnuStrAsciiSanitize (rv[i]);
  }
  return rv;
}

char **
mnuStrTp (TransponderInfo * full, int sz)
{
  char **rv;
  int i;
  rv = calloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    rv[i] = calloc (100, sizeof (char));
    if (!rv[i])
    {
      mnuStrFree (rv, i);
      return NULL;
    }
    realsz = snprintf (rv[i], 100, "%3u,%5.5u %s",
                       full[i].frequency / 100000, full[i].frequency % 100000,
                       tpiGetPolStr (full[i].pol));
    if (realsz >= 100)
    {
      void *np;
      np = realloc (rv[i], realsz + 1);
      if (!np)
      {
        mnuStrFree (rv, i);
        return NULL;
      }
      rv[i] = np;
      realsz = snprintf (rv[i], realsz, "%3u,%5.5u %s",
                         full[i].frequency / 100000,
                         full[i].frequency % 100000,
                         tpiGetPolStr (full[i].pol));
    }
//              errMsg(rv[i]);
  }
  return rv;
}

int
mnuStrGetH (uint32_t t)
{
  return (t % 86400) / 3600;
}

int
mnuStrGetM (uint32_t t)
{
  return ((t % 86400) - mnuStrGetH (t) * 3600) / 60;
}

int
mnuStrGetS (uint32_t t)
{
  return ((t % 86400) - mnuStrGetH (t) * 3600 - mnuStrGetM (t) * 60);
}

char **
mnuStrSched (EpgSchedule * sched)
{
  char **rv;
  int i;
  int sz = sched->num_events;
  rv = calloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    int d, mo, y;
    char *name;
    time_t start, duration;
    struct tm src;
    int h, m, s;
    char *p;
    start = evtGetStart (sched->events[i]);
    h = mnuStrGetH (start);
    m = mnuStrGetM (start);
    s = mnuStrGetS (start);
    duration = evtGetDuration (sched->events[i]);
    mjd_to_ymd (ts_to_mjd (start), &y, &mo, &d);
    debugMsg ("start: %d, y: %d, m: %d, d: %d\n", start, y, mo, d);
    src.tm_min = m;
    src.tm_sec = s;
    src.tm_hour = h;
    src.tm_mday = d;
    src.tm_mon = mo - 1;
    src.tm_year = y - 1900;
    start = timegm (&src);      //avoid me. I'm a gNU extension
    localtime_r (&start, &src);
    m = src.tm_min;
    s = src.tm_sec;
    h = src.tm_hour;

    debugMsg ("start: 0x%x, h: %u, m: %u, s: %u, duration: 0x%x\n", start, h,
              m, s, duration);
    name = evtGetName (sched->events[i]);
    rv[i] = calloc (100, sizeof (char));
    if (!rv[i])
    {
      mnuStrFree (rv, i);
      free (name);
      return NULL;
    }
    realsz =
      snprintf (rv[i], 100, "%2.2d:%2.2d:%2.2d, %3ld: %s", h, m, s,
                duration / 60, name);
    if (realsz >= 100)
    {
      void *np;
      np = realloc (rv[i], realsz + 1);
      if (!np)
      {
        mnuStrFree (rv, i);
        free (name);
        return NULL;
      }
      rv[i] = np;
      realsz =
        snprintf (rv[i], 100, "%2.2d:%2.2d:%2.2d, %3ld: %s", h, m, s,
                  duration / 60, name);
    }
    free (name);
    p = rv[i];
    utlAsciiSanitize (p);
  }
  return rv;
}

char **
mnuStrVtPid (uint16_t * vtidx, int num_strings)
{
  char **rv;
  int i;
  rv = calloc (num_strings, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < num_strings; i++)
  {
    rv[i] = calloc (12, sizeof (char));
    if (!rv)
    {
      mnuStrFree (rv, i);
      return NULL;
    }
    snprintf (rv[i], 12, "%5.5" PRIu16, vtidx[i]);
  }
  return rv;
}

char **
mnuStrVtSvc (uint8_t * vtidx, int num_strings)
{
  char **rv;
  int i;
  rv = calloc (num_strings, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < num_strings; i++)
  {
    rv[i] = calloc (12, sizeof (char));
    if (!rv)
    {
      mnuStrFree (rv, i);
      return NULL;
    }
    snprintf (rv[i], 12, "%3.3" PRIu8, vtidx[i]);
  }
  return rv;
}

char *
mnuStrRem (Reminder * r)
{
  char *rv = NULL;
  char tbuf[40];
  int realsz;
  char *p;
  char *sn = NULL;
  if (NULL == ctime_r (&r->start, tbuf))
  {
    return NULL;
  }

  sn = dvbStringToAscii (&r->service_name);

//ctime fumbles in a newline there. remove it!
  p = strchr (tbuf, '\n');
  if (NULL != p)
  {
    p[0] = '\0';
  }
  rv = calloc (100, sizeof (char));
  if (!rv)
  {
    free (sn);
    return NULL;
  }
  realsz = snprintf (rv, 100, "%s@%s, %s, %s, %.2u:%.2u:%.2u",
                     r->event_name,
                     sn,
                     struGetStr (mnu_tp_strings, r->type),
                     tbuf,
                     (unsigned) r->duration / 3600,
                     (unsigned) (r->duration / 60) % 60,
                     (unsigned) r->duration % 60);
  if (realsz >= 100)
  {
    void *np;
    np = realloc (rv, realsz + 1);
    if (!np)
    {
      free (rv);
      free (sn);
      return NULL;
    }
    rv = np;
    realsz = snprintf (rv, 100, "%s@%s, %s, %s, %.2u:%.2u:%.2u",
                       r->event_name,
                       sn,
                       struGetStr (mnu_tp_strings, r->type),
                       tbuf,
                       (unsigned) r->duration / 3600,
                       (unsigned) (r->duration / 60) % 60,
                       (unsigned) r->duration % 60);
  }
  free (sn);
  return rv;
}


char **
mnuStrRemList (DList * l, int *sz_ret)
{
  int sz = 0;
  int i;
  char **rv;
  DListE *e = dlistFirst (l);
  while (e)
  {
    sz++;
    e = dlistENext (e);
  }
  if (!sz)
  {
    *sz_ret = 0;
    return NULL;
  }
  rv = calloc (sz, sizeof (char *));
  if (NULL == rv)
  {
    *sz_ret = 0;
    return NULL;
  }
  i = 0;
  e = dlistFirst (l);
  while (e)
  {
    char *desc = NULL;
    Reminder *r = dlistEPayload (e);
    desc = mnuStrRem (r);
    utlAsciiSanitize (desc);
    rv[i] = desc;
    i++;
    e = dlistENext (e);
  }
  *sz_ret = sz;
  return rv;
}
