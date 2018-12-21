#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include "timestamp.h"
#include "utl.h"
#include "menu_strings.h"
#include "reminder.h"
#include "dmalloc_f.h"
#include "debug.h"
int DF_MNU_STRINGS;
#define FILE_DBG DF_MNU_STRINGS

char *
get_svc_string (uint8_t svc_type)
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
free_strings (char **s, int sz)
{
  int i;
  for (i = 0; i < sz; i++)
  {
    utlFAN (s[i]);
  }
  utlFAN (s);
}

char **
fav_to_strings (Favourite * full, int sz)
{
  char **rv;
  int i;
  rv = utlCalloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    char *p;
    char *s_name = NULL;
    char *p_name = NULL;
    char *b_name = NULL;
    rv[i] = utlCalloc (100, sizeof (char));
    if (!rv[i])
    {
      free_strings (rv, i);
      return NULL;
    }
    s_name = dvbStringToAscii (&full[i].service_name);
    p_name = dvbStringToAscii (&full[i].provider_name);
    b_name = dvbStringToAscii (&full[i].bouquet_name);
    realsz = snprintf (rv[i], 100, "%3u,%5.5u %s, %5hu, %s, %s, %s, %s, %s",
                       full[i].freq / 100000, full[i].freq % 100000,
                       tpiGetPolStr (full[i].pol),
                       full[i].pnr,
                       get_svc_string (full[i].type),
                       full[i].ca_mode ? "PAY" : "FTA",
                       s_name, p_name, b_name);
    if (realsz >= 100)
    {
      void *np;
      np = utlRealloc (rv[i], realsz + 1);
      if (!np)
      {
        free_strings (rv, i);
        utlFAN (s_name);
        utlFAN (p_name);
        utlFAN (b_name);
        return NULL;
      }
      rv[i] = np;
      realsz =
        snprintf (rv[i], realsz, "%3u,%5.5u %s, %5hu, %s, %s, %s, %s, %s",
                  full[i].freq / 100000, full[i].freq % 100000,
                  tpiGetPolStr (full[i].pol), full[i].pnr,
                  get_svc_string (full[i].type),
                  full[i].ca_mode ? "PAY" : "FTA", s_name, p_name, b_name);
    }
    utlFAN (s_name);
    utlFAN (p_name);
    utlFAN (b_name);
    p = rv[i];
    utlAsciiSanitize (p);
  }
  return rv;
}

char **
fav_to_names (Favourite * fav, int sz)
{
  char **rv;
  int i;
  rv = utlCalloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    char *p;
    char *s_name = NULL;
    rv[i] = utlCalloc (100, sizeof (char));
    if (!rv[i])
    {
      free_strings (rv, i);
      return NULL;
    }
    s_name = dvbStringToAscii (&fav[i].service_name);
    realsz = snprintf (rv[i], 100, "%s", s_name);
    if (realsz >= 100)
    {
      void *np;
      np = utlRealloc (rv[i], realsz + 1);
      if (!np)
      {
        free_strings (rv, i);
        utlFAN (s_name);
        return NULL;
      }
      rv[i] = np;
      realsz = snprintf (rv[i], realsz, "%s", s_name);
    }
    utlFAN (s_name);
    p = rv[i];
    utlAsciiSanitize (p);
  }
  return rv;
}

char **
rse_to_strings (RsEntry * rse, int sz)
{
  char **rv;
  int i;
  char tbuf[40];
  rv = utlCalloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    char *p;
    if (NULL == ctime_r (&rse[i].start_time, tbuf))
    {
      free_strings (rv, i);
      return NULL;
    }
    //ctime fumbles in a newline there. remove it!
    p = strchr (tbuf, '\n');
    if (NULL != p)
    {
      p[0] = '\0';
    }
    rv[i] = utlCalloc (100, sizeof (char));
    if (!rv[i])
    {
      free_strings (rv, i);
      return NULL;
    }
    realsz =
      snprintf (rv[i], 100, "%s, %s, %u, %3u,%5.5u %s, %5hu, %s, %u:%u:%u",
                rsGetStateStr (rse[i].state), rsGetTypeStr (rse[i].type),
                rse[i].pos, rse[i].freq / 100000, rse[i].freq % 100000,
                tpiGetPolStr (rse[i].pol), rse[i].pnr, tbuf,
                (unsigned) rse[i].duration / 3600,
                (unsigned) (rse[i].duration / 60) % 60,
                (unsigned) rse[i].duration % 60);
    if (realsz >= 100)
    {
      void *np;
      np = utlRealloc (rv[i], realsz + 1);
      if (!np)
      {
        free_strings (rv, i);
        return NULL;
      }
      rv[i] = np;
      realsz =
        snprintf (rv[i], 100, "%s, %s, %u, %3u,%5.5u %s, %5hu, %s, %u:%u:%u",
                  rsGetStateStr (rse[i].state), rsGetTypeStr (rse[i].type),
                  rse[i].pos, rse[i].freq / 100000, rse[i].freq % 100000,
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
pgms_to_strings (ProgramInfo * pi, int sz)
{
  char **rv;
  int i;
  rv = utlCalloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
//              char * p=NULL;
    char *s_name = NULL;
    char *p_name = NULL;
    char *b_name = NULL;
    rv[i] = utlCalloc (100, sizeof (char));
    if (!rv[i])
    {
      free_strings (rv, i);
      return NULL;
    }
    s_name = dvbStringToAscii (&pi[i].service_name);
    p_name = dvbStringToAscii (&pi[i].provider_name);
    b_name = dvbStringToAscii (&pi[i].bouquet_name);
    realsz = snprintf (rv[i], 100, "%5hu, %s, %s, %s, %s, %s",
                       pi[i].pnr,
                       get_svc_string (pi[i].svc_type),
                       pi[i].ca_mode ? "PAY" : "FTA", s_name, p_name, b_name);
    if (realsz >= 100)
    {
      void *np;
      np = utlRealloc (rv[i], realsz + 1);
      if (!np)
      {
        free_strings (rv, i);
        utlFAN (s_name);
        utlFAN (p_name);
        utlFAN (b_name);
        return NULL;
      }
      rv[i] = np;
      realsz = snprintf (rv[i], realsz, "%5hu, %s, %s, %s, %s, %s",
                         pi[i].pnr,
                         get_svc_string (pi[i].svc_type),
                         pi[i].ca_mode ? "PAY" : "FTA",
                         s_name, p_name, b_name);
    }
    utlFAN (s_name);
    utlFAN (p_name);
    utlFAN (b_name);
    utlAsciiSanitize (rv[i]);
  }
  return rv;
}

char **
tp_to_strings (TransponderInfo * tpi, int sz)
{
  char **rv;
  int i;
  rv = utlCalloc (sz, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < sz; i++)
  {
    int realsz;
    rv[i] = utlCalloc (100, sizeof (char));
    if (!rv[i])
    {
      free_strings (rv, i);
      return NULL;
    }
    realsz = snprintf (rv[i], 100, "%3u,%5.5u %s",
                       tpi[i].frequency / 100000, tpi[i].frequency % 100000,
                       tpiGetPolStr (tpi[i].pol));
    if (realsz >= 100)
    {
      void *np;
      np = utlRealloc (rv[i], realsz + 1);
      if (!np)
      {
        free_strings (rv, i);
        return NULL;
      }
      rv[i] = np;
      realsz = snprintf (rv[i], realsz, "%3u,%5.5u %s",
                         tpi[i].frequency / 100000, tpi[i].frequency % 100000,
                         tpiGetPolStr (tpi[i].pol));
    }
//              errMsg(rv[i]);
  }
  return rv;
}

int
get_h (time_t t)
{
  return (t % 86400) / 3600;
}

int
get_m (time_t t)
{
  return ((t % 86400) - get_h (t) * 3600) / 60;
}

int
get_s (time_t t)
{
  return ((t % 86400) - get_h (t) * 3600 - get_m (t) * 60);
}

char **
sched_to_strings (EpgSchedule * sched)
{
  char **rv;
  int i;
  int sz = sched->num_events;
  rv = utlCalloc (sz, sizeof (char *));
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
    h = get_h (start);
    m = get_m (start);
    s = get_s (start);
    duration = evtGetDuration (sched->events[i]);
    mjd_to_ymd (ts_to_mjd (start), &y, &mo, &d);
    debugMsg ("start: %d, s2:%d, y: %d, m: %d, d: %d\n", start, y, mo, d);
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
    rv[i] = utlCalloc (100, sizeof (char));
    if (!rv[i])
    {
      free_strings (rv, i);
      utlFAN (name);
      return NULL;
    }
    realsz =
      snprintf (rv[i], 100, "%2.2d:%2.2d:%2.2d, %4ld: %s", h, m, s,
                duration / 60, name);
    if (realsz >= 100)
    {
      void *np;
      np = utlRealloc (rv[i], realsz + 1);
      if (!np)
      {
        free_strings (rv, i);
        utlFAN (name);
        return NULL;
      }
      rv[i] = np;
      realsz =
        snprintf (rv[i], 100, "%2.2d:%2.2d:%2.2d, %4ld: %s", h, m, s,
                  duration / 60, name);
    }
    utlFAN (name);
    p = rv[i];
    utlAsciiSanitize (p);
  }
  return rv;
}

char **
vt_pid_to_strings (uint16_t * vtidx, int num_strings)
{
  char **rv;
  int i;
  rv = utlCalloc (num_strings, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < num_strings; i++)
  {
    rv[i] = utlCalloc (12, sizeof (char));
    if (!rv)
    {
      free_strings (rv, i);
      return NULL;
    }
    snprintf (rv[i], 12, "%5.5" PRIu16, vtidx[i]);
  }
  return rv;
}

char **
vt_svc_to_strings (uint8_t * vtidx, int num_strings)
{
  char **rv;
  int i;
  rv = utlCalloc (num_strings, sizeof (char *));
  if (!rv)
    return NULL;
  for (i = 0; i < num_strings; i++)
  {
    rv[i] = utlCalloc (12, sizeof (char));
    if (!rv)
    {
      free_strings (rv, i);
      return NULL;
    }
    snprintf (rv[i], 12, "%3.3" PRIu8, vtidx[i]);
  }
  return rv;
}

char *
rem_mnu_str (Reminder * r)
{
  char *rv = NULL;
  char tbuf[40];
  int realsz;
  char *p;
  if (NULL == ctime_r (&r->start, tbuf))
  {
    return NULL;
  }
//ctime fumbles in a newline there. remove it!
  p = strchr (tbuf, '\n');
  if (NULL != p)
  {
    p[0] = '\0';
  }
  rv = utlCalloc (100, sizeof (char));
  if (!rv)
  {
    return NULL;
  }
  realsz =
    snprintf (rv, 100, "%-9.9s, %s, %.2u:%.2u:%.2u, %u, %3u,%5.5u %s, %5.5hu",
              rsGetTypeStr (r->type), tbuf, (unsigned) r->duration / 3600,
              (unsigned) (r->duration / 60) % 60, (unsigned) r->duration % 60,
              r->pos, r->freq / 100000, r->freq % 100000,
              tpiGetPolStr (r->pol), r->pnr);
  if (realsz >= 100)
  {
    void *np;
    np = utlRealloc (rv, realsz + 1);
    if (!np)
    {
      utlFAN (rv);
      return NULL;
    }
    rv = np;
    realsz =
      snprintf (rv, 100,
                "%-9.9s, %s, %.2u:%.2u:%.2u, %u, %3u,%5.5u %s, %5.5hu",
                rsGetTypeStr (r->type), tbuf, (unsigned) r->duration / 3600,
                (unsigned) (r->duration / 60) % 60,
                (unsigned) r->duration % 60, r->pos, r->freq / 100000,
                r->freq % 100000, tpiGetPolStr (r->pol), r->pnr);
  }
  return rv;
}

ITEM **
rem_to_items (DList * l, int *sz_ret)
{
  int sz = 0;
  int i;
  ITEM **items = NULL;
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
  items = utlCalloc (sz + 1, sizeof (ITEM *));
  if (NULL == items)
  {
    *sz_ret = 0;
    return NULL;
  }
  items[sz] = NULL;
  i = 0;
  e = dlistFirst (l);
  while (e)
  {
    char *desc = NULL;
    char *name = NULL;
    Reminder *r = dlistEPayload (e);
    desc = rem_mnu_str (dlistEPayload (e));
    if (NULL != r->event_name)
      name = strndup (r->event_name, 10);
    else
      name = strndup ("none", 10);

    utlAsciiSanitize (name);
    items[i] = new_item (name, desc);
    if ((NULL == items[i]) || (NULL == name) || (NULL == desc))
    {
      utlFAN (name);
      utlFAN (desc);
      while (i--)
      {
        free_item (items[i]);
      }
      utlFAN (items);
      *sz_ret = 0;
      return NULL;
    }
    set_item_userptr (items[i], e);
    i++;
    e = dlistENext (e);
  }
  *sz_ret = sz;
  return items;
}

int
rem_items_free (ITEM ** items)
{
  int i;
  i = 0;
  while (NULL != items[i])
  {
    utlFree ((void *) item_name (items[i]));
    utlFree ((void *) item_description (items[i]));
    free_item (items[i]);
    i++;
  }
  utlFAN (items);
  return 0;
}
