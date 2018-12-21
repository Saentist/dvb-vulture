#include <string.h>
#include <stdio.h>
#include "utl.h"
#include "in_out.h"
#include "rs_entry.h"
#include "reminder.h"
#include "dmalloc_loc.h"

int
reminderAdd (DList * l, DListE * rem)
{
  Reminder *rem2 = dlistEPayload (rem);
  DListE *current = dlistFirst (l);
  if (!current)
  {
    dlistInsertFirst (l, rem);
    return 0;
  }
  while (current)
  {
    Reminder *c_rem = dlistEPayload (current);
    if (c_rem->start > rem2->start)
    {
      dlistInsertBefore (l, rem, current);
      return 0;
    }
    current = dlistENext (current);
  }
  dlistInsertLast (l, rem);
  return 0;
}

DListE *
reminderNew (RSType type,
             uint8_t wd,
             time_t start,
             time_t duration,
             uint32_t pos,
             uint32_t freq,
             uint8_t pol, uint16_t pnr, DvbString * svc, char *evt)
{
  DListE *rem;
  Reminder *r;
  rem = utlCalloc (1, dlistESize (sizeof (Reminder)));
  if (NULL == rem)
    return NULL;
  r = dlistEPayload (rem);
  r->type = type;
  r->wd = wd;
  r->start = start;
  r->duration = duration;
  r->pos = pos;
  r->freq = freq;
  r->pol = pol;
  r->pnr = pnr;

  if (NULL != svc)
  {
    if (dvbStringCopy (&r->service_name, svc))
    {
      utlFAN (rem);
      return NULL;
    }
  }

  if (NULL != evt)
  {
    r->event_name = strndup (evt, 64);
    if (NULL == r->event_name)
    {
      dvbStringClear (&r->service_name);
      utlFAN (rem);
      return NULL;
    }
  }
  return rem;
}

int
reminderDel (DListE * rem)
{
  Reminder *r = dlistEPayload (rem);
  dvbStringClear (&r->service_name);
  utlFAN (r->event_name);
  utlFAN (rem);
  return 0;
}

/*
DListE *reminderIsReady(DList *l)
{
	time_t now=time(NULL);
	DListE * first=dlistFirst(l);
	Reminder * first_rem;
	if(!first)
		return NULL;
	first_rem=dlistEPayload(first);
	if(first_rem->start<=now)
		return first;
	return NULL;
}
*/

bool
reminderIsReady (Reminder * rem)
{
  time_t now = time (NULL);
  if (!rem)
    return false;
  if (rem->start <= now)
    return true;
  return false;
}

//remove or relink reminder depending on type
int
reminderDone (DList * l, DListE * rem)
{
  Reminder *this = dlistEPayload (rem);
  dlistRemove (l, rem);
  if (this->type == RST_ONCE)
  {
    reminderDel (rem);
    return 1;
  }
  this->start = rsNextTime (this->type, this->wd, this->start);
  if (0 == this->start)
  {
    reminderDel (rem);
    return 1;
  }
  reminderAdd (l, rem);
  return 0;
}

int
reminderListUpdate (DList * l, time_t t)
{
  DListE *e = NULL, *next = NULL;
  e = dlistFirst (l);
  while (e)
  {
    Reminder *current = dlistEPayload (e);
    next = dlistENext (e);      //may not be available anymore further down
    while ((current->start + current->duration) < t)    //if list is days or weeks old it may be true several times
    {
      if (reminderDone (l, e))
        break;                  //it's gone
    }
    e = next;
    next = NULL;
  }
  return 0;
}

int
reminderWrite (void *x NOTUSED, DListE * this, FILE * f)
{
  Reminder *r = dlistEPayload (this);
  fwrite (r, sizeof (Reminder) - sizeof (DvbString) - sizeof (char *), 1, f);
  dvbStringWrite (f, &r->service_name);
  ioWrStr (r->event_name, f);
  return 0;
}

int
reminderListWrite (FILE * f, DList * l)
{
  return dlistWrite (l, NULL, reminderWrite, f);
}

DListE *
reminderRead (void *x NOTUSED, FILE * f)
{
  DListE *rv = utlCalloc (1, dlistESize (sizeof (Reminder)));
  if (rv)
  {
    Reminder *r = dlistEPayload (rv);
    fread (r, sizeof (Reminder) - sizeof (DvbString) - sizeof (char *), 1, f);
    dvbStringRead (f, &r->service_name);
    r->event_name = ioRdStr (f);
  }
  return rv;
}

int
reminderListRead (FILE * f, DList * l)
{
  return dlistRead (l, NULL, reminderRead, f);
}
