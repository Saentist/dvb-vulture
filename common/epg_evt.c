#include <string.h>
#ifndef __WIN32__
#include <arpa/inet.h>
#else
#include <winsock.h>
#endif
#include <inttypes.h>
#include "epg_evt.h"
#include "bits.h"
#include "size_stack.h"
#include "dvb_string.h"
#include "timestamp.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"
int DF_EPG_EVT;
#define FILE_DBG DF_EPG_EVT



void
evtClear (EVT * e)
{
  if (e->name)
    utlFAN (e->name);
  if (e->description)
    utlFAN (e->description);
}

/*
void slist_destroy_event(void * x,SListE * e)
{
	EVT * evt;
	evt=slist_e_payload(e);
	clear_event(evt);
	utlFAN(e);
}
*/

void
epgArrayClear (EpgArray * a)
{
  uint32_t i, j;
  for (i = 0; i < a->num_pgm; i++)
  {
    for (j = 0; j < a->sched[i].num_events; j++)
    {
      utlFAN (a->sched[i].events[j]);
    }
    utlFAN (a->sched[i].events);
  }
  utlFAN (a->sched);
  utlFAN (a->pgm_nrs);
}

void
epgArrayDestroy (EpgArray * a)
{
  if (a)
  {
    epgArrayClear (a);
    utlFAN (a);
  }
}

uint16_t
evtGetSize (uint8_t * ev_p)
{
  return bitsGet (ev_p, 10, 4, 12) + 2 + 10;
}

time_t
evtGetStart (uint8_t * ev_start)
{
  uint8_t *ptr = ev_start;
  uint16_t mjd;
  uint8_t h1, m1, s1;
  time_t start;
  mjd = bitsGet (ptr, 2, 0, 16);
  h1 = bitsDecodeBcd8 (bitsGet (ptr, 2, 16, 8));
  m1 = bitsDecodeBcd8 (bitsGet (ptr, 2, 24, 8));
  s1 = bitsDecodeBcd8 (bitsGet (ptr, 2, 32, 8));
  start = get_time_mjd (mjd) + get_time_hms (h1, m1, s1);
  return start;
}

time_t
evtGetDuration (uint8_t * ev_start)
{
  uint8_t *ptr = ev_start;
  uint8_t h2, m2, s2;
  time_t duration;
  h2 = bitsDecodeBcd8 (bitsGet (ptr, 7, 0, 8));
  m2 = bitsDecodeBcd8 (bitsGet (ptr, 7, 8, 8));
  s2 = bitsDecodeBcd8 (bitsGet (ptr, 7, 16, 8));
  duration = get_time_hms (h2, m2, s2);
  return duration;
}

uint16_t
evtGetId (uint8_t * ev_start)
{
  return bitsGet (ev_start, 0, 0, 16);
}

uint8_t
evtGetRst (uint8_t * ev_start)
{
  return (ev_start[10] >> 5) & 7;
}

char *
evtGetName (uint8_t * ev_p)
{
  SizeStack st;
  SizeStack *s = &st;
  uint16_t desc_len;
  uint8_t *ptr;
  char *name = NULL;


  if (sizeStackInit (s, ev_p))
    return NULL;

  if (sizeStackAdvance (s, 0, 12))
    return NULL;
  ptr = sizeStackPtr (s);

  desc_len = bitsGet (ptr, 10, 4, 12);
  if (sizeStackAdvance (s, 12, 0))
    return NULL;
  if (sizeStackPush (s, desc_len))
    return NULL;
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    DvbString tmp = { 0, NULL };
    if (sizeStackAdvance (s, 0, 2))
      return NULL;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 1))
      return NULL;
    if (sizeStackPush (s, d_len))
      return NULL;
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x4d:                 //short event desc
      if (sizeStackAdvance (s, 0, 4))
        return NULL;
      if (ptr[3])
      {
        if (sizeStackAdvance (s, 0, 4 + ptr[3]))
          return NULL;
        tmp.buf = utlCalloc (1, ptr[3]);
        tmp.len = ptr[3];
        memmove (tmp.buf, ptr + 4, ptr[3]);
      }
      name = dvbStringToAscii (&tmp);
      utlFAN (tmp.buf);
      return name;
    default:
      debugMsg ("Event ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
      return NULL;
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
      return NULL;
  }
  if (sizeStackPop (s))
    return NULL;
  return NULL;
}

char *
evtGetDesc (uint8_t * ev_p)
{
  SizeStack st;
  SizeStack *s = &st;
  uint16_t desc_len;
  int item_len, ds_len, i_len, t_len;
  uint8_t *ptr2;
  uint8_t *ptr;
  char *description = NULL;
  unsigned alloc_size = 0;

  if (sizeStackInit (s, ev_p))
    return NULL;

  if (sizeStackAdvance (s, 0, 12))
    return NULL;
  ptr = sizeStackPtr (s);

  desc_len = bitsGet (ptr, 10, 4, 12);
  if (sizeStackAdvance (s, 12, 0))
    return NULL;
  if (sizeStackPush (s, desc_len))
    return NULL;
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    DvbString tmp = { 0, NULL };
    char *tmp2 = NULL;
    if (sizeStackAdvance (s, 0, 2))
      return description;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 1))
      return description;
    if (sizeStackPush (s, d_len))
      return description;
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x4d:                 //short event desc
      debugMsg ("short ev desc\n");
      if (sizeStackAdvance (s, 0, 4))
        return description;
      if (sizeStackAdvance (s, 0, 5 + ptr[3]))
        return description;
      if (ptr[4 + ptr[3]])
      {
        tmp.buf = utlCalloc (1, ptr[4 + ptr[3]]);
        tmp.len = ptr[4 + ptr[3]];
        if (sizeStackAdvance (s, 0, 5 + ptr[3] + ptr[4 + ptr[3]]))
        {
          utlFAN (tmp.buf);
          return description;
        }
        memmove (tmp.buf, ptr + 5 + ptr[3], ptr[4 + ptr[3]]);
        tmp2 = dvbStringToAscii (&tmp);
        utlSmartCat (&description, &alloc_size, tmp2);
        utlSmartCat (&description, &alloc_size, "\n");
        utlFAN (tmp.buf);
        tmp.buf = NULL;
        tmp.len = 0;
        utlFAN (tmp2);
        tmp2 = NULL;
      }
      break;
    case 0x4e:
      item_len = ptr[4];
      ptr2 = ptr + 5;
      debugMsg ("item_len:%d\n", item_len);
      while (item_len > 0)
      {
        ds_len = ptr2[0];

        item_len--;
        ptr2++;

        tmp.buf = utlCalloc (1, ds_len);
        tmp.len = ds_len;
        memmove (tmp.buf, ptr2, ds_len);
        tmp2 = dvbStringToAscii (&tmp);
        utlSmartCat (&description, &alloc_size, tmp2);
        utlSmartCat (&description, &alloc_size, ": ");
        utlFAN (tmp.buf);
        tmp.buf = NULL;
        tmp.len = 0;
        utlFAN (tmp2);
        tmp2 = NULL;

//                              utlSmartCatUt(&description,&alloc_size,(char *)ptr2,ds_len);
//                              utlSmartCat(&description,&alloc_size,": ");

        ptr2 += ds_len;
        item_len -= ds_len;

        i_len = ptr2[0];

        item_len--;
        ptr2++;

        tmp.buf = utlCalloc (1, i_len);
        tmp.len = i_len;
        memmove (tmp.buf, ptr2, i_len);
        tmp2 = dvbStringToAscii (&tmp);
        utlSmartCat (&description, &alloc_size, tmp2);
        utlSmartCat (&description, &alloc_size, "\n");
        utlFAN (tmp.buf);
        tmp.buf = NULL;
        tmp.len = 0;
        utlFAN (tmp2);
        tmp2 = NULL;

//                              utlSmartCatUt(&description,&alloc_size,(char *)ptr2,i_len);
//                              utlSmartCat(&description,&alloc_size,"\n");

        ptr2 += i_len;
        item_len -= i_len;
      }
      t_len = ptr2[0];
      ptr2++;

      tmp.buf = utlCalloc (1, t_len);
      tmp.len = t_len;
      memmove (tmp.buf, ptr2, t_len);
      tmp2 = dvbStringToAscii (&tmp);
      utlSmartCat (&description, &alloc_size, tmp2);
      utlFAN (tmp.buf);
      tmp.buf = NULL;
      tmp.len = 0;
      utlFAN (tmp2);
      tmp2 = NULL;

//                      utlSmartCatUt(&description,&alloc_size,(char *)ptr2,t_len);
      break;
    default:
      debugMsg ("Event ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
      return description;
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
      return description;
  }
  if (sizeStackPop (s))
    return description;
  return description;
}

//2toow
wchar_t *
evtGetNameW (uint8_t * ev_p)
{
  SizeStack st;
  SizeStack *s = &st;
  uint16_t desc_len;
  uint8_t *ptr;
  wchar_t *name = NULL;


  if (sizeStackInit (s, ev_p))
    return NULL;

  if (sizeStackAdvance (s, 0, 12))
    return NULL;
  ptr = sizeStackPtr (s);

  desc_len = bitsGet (ptr, 10, 4, 12);
  if (sizeStackAdvance (s, 12, 0))
    return NULL;
  if (sizeStackPush (s, desc_len))
    return NULL;
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    DvbString tmp;
    if (sizeStackAdvance (s, 0, 2))
      return NULL;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 1))
      return NULL;
    if (sizeStackPush (s, d_len))
      return NULL;
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x4d:                 //short event desc
      if (sizeStackAdvance (s, 0, 4))
        return NULL;
      tmp.buf = NULL;
      tmp.len = 0;
      if (ptr[3])
      {
        if (sizeStackAdvance (s, 0, 4 + ptr[3]))
          return NULL;
        tmp.buf = utlCalloc (1, ptr[3]);
        tmp.len = ptr[3];
        memmove (tmp.buf, ptr + 4, ptr[3]);
        name = dvbStringToW (&tmp);
        utlFAN (tmp.buf);
        return name;
      }
//                      name[ptr[3]]='\0';
    default:
      debugMsg ("Event ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
      return NULL;
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
      return NULL;
  }
  if (sizeStackPop (s))
    return NULL;
  return NULL;
}


wchar_t *
evtGetDescW (uint8_t * ev_p)
{
  SizeStack st;
  SizeStack *s = &st;
  uint16_t desc_len;
  int item_len, ds_len, i_len, t_len;
  uint8_t *ptr2;
  uint8_t *ptr;
  wchar_t *description = NULL;
  unsigned alloc_size = 0;

  if (sizeStackInit (s, ev_p))
    return NULL;

  if (sizeStackAdvance (s, 0, 12))
    return NULL;
  ptr = sizeStackPtr (s);

  desc_len = bitsGet (ptr, 10, 4, 12);
  if (sizeStackAdvance (s, 12, 0))
    return NULL;
  if (sizeStackPush (s, desc_len))
    return NULL;
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    DvbString tmp = { 0, NULL };
    wchar_t *tmp2 = NULL;
    if (sizeStackAdvance (s, 0, 2))
      return description;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 1))
      return description;
    if (sizeStackPush (s, d_len))
      return description;
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x4d:                 //short event desc
      debugMsg ("short ev desc\n");
      if (sizeStackAdvance (s, 0, 4))
        return description;
      if (sizeStackAdvance (s, 0, 5 + ptr[3]))
        return description;
      if (ptr[4 + ptr[3]])
      {
        tmp.buf = utlCalloc (1, ptr[4 + ptr[3]]);
        tmp.len = ptr[4 + ptr[3]];
        if (sizeStackAdvance (s, 0, 5 + ptr[3] + ptr[4 + ptr[3]]))
        {
          utlFAN (tmp.buf);
          return description;
        }
        memmove (tmp.buf, ptr + 5 + ptr[3], ptr[4 + ptr[3]]);
        tmp2 = dvbStringToW (&tmp);
        utlSmartCatW (&description, &alloc_size, tmp2);
        utlSmartCatW (&description, &alloc_size, L"\n");
        utlFAN (tmp.buf);
        tmp.buf = NULL;
        tmp.len = 0;
        utlFAN (tmp2);
        tmp2 = NULL;
      }
      break;
    case 0x4e:
      item_len = ptr[4];
      ptr2 = ptr + 5;
      debugMsg ("item_len:%d\n", item_len);
      while (item_len > 0)
      {
        ds_len = ptr2[0];

        item_len--;
        ptr2++;

        tmp.buf = utlCalloc (1, ds_len);
        tmp.len = ds_len;
        memmove (tmp.buf, ptr2, ds_len);
        tmp2 = dvbStringToW (&tmp);
        utlSmartCatW (&description, &alloc_size, tmp2);
        utlSmartCatW (&description, &alloc_size, L": ");
        utlFAN (tmp.buf);
        tmp.buf = NULL;
        tmp.len = 0;
        utlFAN (tmp2);
        tmp2 = NULL;

//                              utlSmartCatUt(&description,&alloc_size,(char *)ptr2,ds_len);
//                              utlSmartCat(&description,&alloc_size,": ");

        ptr2 += ds_len;
        item_len -= ds_len;

        i_len = ptr2[0];

        item_len--;
        ptr2++;

        tmp.buf = utlCalloc (1, i_len);
        tmp.len = i_len;
        memmove (tmp.buf, ptr2, i_len);
        tmp2 = dvbStringToW (&tmp);
        utlSmartCatW (&description, &alloc_size, tmp2);
        utlSmartCatW (&description, &alloc_size, L"\n");
        utlFAN (tmp.buf);
        tmp.buf = NULL;
        tmp.len = 0;
        utlFAN (tmp2);
        tmp2 = NULL;

//                              utlSmartCatUt(&description,&alloc_size,(char *)ptr2,i_len);
//                              utlSmartCat(&description,&alloc_size,"\n");

        ptr2 += i_len;
        item_len -= i_len;
      }
      t_len = ptr2[0];
      ptr2++;

      tmp.buf = utlCalloc (1, t_len);
      tmp.len = t_len;
      memmove (tmp.buf, ptr2, t_len);
      tmp2 = dvbStringToW (&tmp);
      utlSmartCatW (&description, &alloc_size, tmp2);
      utlFAN (tmp.buf);
      tmp.buf = NULL;
      tmp.len = 0;
      utlFAN (tmp2);
      tmp2 = NULL;

//                      utlSmartCatUt(&description,&alloc_size,(char *)ptr2,t_len);
      break;
    default:
      debugMsg ("Event ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
      return description;
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
      return description;
  }
  if (sizeStackPop (s))
    return description;
  return description;
}


uint32_t
evtGetPIL (uint8_t * ev_p)
{
  SizeStack st;
  SizeStack *s = &st;
  uint16_t desc_len;
  uint8_t *ptr;
  uint32_t pil;

  if (sizeStackInit (s, ev_p))
    return PIL_NONE;

  if (sizeStackAdvance (s, 0, 12))
    return PIL_NONE;
  ptr = sizeStackPtr (s);

  desc_len = bitsGet (ptr, 10, 4, 12);
  if (sizeStackAdvance (s, 12, 0))
    return PIL_NONE;
  if (sizeStackPush (s, desc_len))
    return PIL_NONE;
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    if (sizeStackAdvance (s, 0, 2))
      return PIL_NONE;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 1))
      return PIL_NONE;
    if (sizeStackPush (s, d_len))
      return PIL_NONE;
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x69:                 //PDC desc
      if (sizeStackAdvance (s, 0, 3))
        return PIL_NONE;
      pil = ptr[2] | (ptr[1] << 8) | (ptr[0] << 16);
      debugMsg ("PIL: 0x%" PRIx32 "\n", pil);
      return pil & PIL_NONE;
    default:
      debugMsg ("Event ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
      return PIL_NONE;
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
      return PIL_NONE;
  }
  if (sizeStackPop (s))
    return PIL_NONE;
  return PIL_NONE;
}

uint16_t *
evtGetCd (uint8_t * ev_p, unsigned *sz_ret)
{
  SizeStack st;
  SizeStack *s = &st;
  uint16_t desc_len;
  uint8_t *ptr;
  uint16_t cd;
  unsigned sz = 0, alc = 0;
  uint8_t cl;
  uint16_t *rv = NULL;

  if (sizeStackInit (s, ev_p))
    return NULL;

  if (sizeStackAdvance (s, 0, 12))
    return NULL;
  ptr = sizeStackPtr (s);

  desc_len = bitsGet (ptr, 10, 4, 12);
  if (sizeStackAdvance (s, 12, 0))
    return NULL;
  if (sizeStackPush (s, desc_len))
    return NULL;
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    if (sizeStackAdvance (s, 0, 2))
    {
      utlFAN (rv);
      return NULL;
    }
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 1))
    {
      utlFAN (rv);
      return NULL;
    }
    if (sizeStackPush (s, d_len))
    {
      utlFAN (rv);
      return NULL;
    }
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x54:                 //Content desc
      cl = d_len / sizeof (uint16_t);
      while (cl--)
      {
        uint16_t tmp16;
        memmove (&tmp16, ptr, sizeof tmp16);
        cd = ntohs (tmp16);
        ptr += sizeof (uint16_t);
        utlSmartMemmove ((uint8_t **) & rv, &sz, &alc, (uint8_t *) & cd,
                         sizeof (uint16_t));
      }
    default:
      debugMsg ("Event ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
    {
      utlFAN (rv);
      return NULL;
    }
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
    {
      utlFAN (rv);
      return NULL;
    }
  }
  if (sizeStackPop (s))
  {
    utlFAN (rv);
    return NULL;
  }
  *sz_ret = sz / sizeof (uint16_t);
  return rv;
}

uint8_t
evtPILDay (uint32_t pil)
{
  uint8_t b1 = (pil >> 16) & 0x0f;
  uint8_t b2 = (pil >> 8) & 0xff;
  return (b1 << 1) | (b2 >> 7);
}

uint8_t
evtPILMonth (uint32_t pil)
{
  return ((pil >> 8) & 0x78) >> 3;
}

uint8_t
evtPILHour (uint32_t pil)
{
  uint8_t b1 = (pil >> 8) & 0x07;
  uint8_t b2 = (pil) & 0xC0;
  return (b1 << 2) | (b2 >> 6);
}

uint8_t
evtPILMinute (uint32_t pil)
{
  return (pil) & 0x3f;
}
