#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "eis.h"
#include "size_stack.h"
#include "iterator.h"
#include "utl.h"
#include "debug.h"
#include "bits.h"
#include "item_list.h"
#include "timestamp.h"
#include "dmalloc.h"
int DF_EIS;
#define FILE_DBG DF_EIS
/*
a) To find Y, M, D from MJD
   Y' = int [ (MJD - 15 078,2) / 365,25 ]
   M' = int { [ MJD - 14 956,1 - int (Y' × 365,25) ] / 30,6001 }
   D = MJD - 14 956 - int (Y' × 365,25) - int (M' × 30,6001)
   If M' = 14 or M' = 15, then K = 1; else K = 0
   Y = Y' + K
   M = M' - 1 - K × 12
b) To find MJD from Y, M, D
   If M = 1 or M = 2, then L = 1; else L = 0
   MJD = 14 956 + D + int [ (Y - L) × 365,25] + int [ (M + 1 + L × 12) × 30,6001 ]
c) To find WD from MJD
   WD = [ (MJD + 2) mod 7 ] + 1
d) To find MJD from WY, WN, WD
   MJD = 15 012 + WD + 7 × { WN + int [ (WY × 1 461 / 28) + 0,41] }
e) To find WY, WN from MJD
   W = int [ (MJD / 7) - 2 144,64 ]
   WY = int [ (W × 28 / 1 461) - 0,0079]
   WN = W - int [ (WY × 1 461 / 28) + 0,41]
EXAMPLE:       MJD    =     45 218           W        = 4 315
               Y      =     (19)82           WY       = (19)82
               M      =     9 (September)    N        = 36
               D      =     6                WD       = 1 (Monday)
NOTE:   These formulas are applicable between the inclusive dates 1900 March 1 to 2100 February 28.
*/


int
get_eis_tsid (uint8_t * sec)
{
  return bitsGet (sec, 8, 0, 16);
}

static int
get_event (SizeStack * s, EVT * evt)
{
  uint8_t *ptr;
  uint16_t desc_len;
  uint16_t mjd;
  uint8_t h1, m1, s1, h2, m2, s2;

  if (sizeStackAdvance (s, 0, 12))
    return 1;
  ptr = sizeStackPtr (s);
  evt->evt_id = bitsGet (ptr, 0, 0, 16);

  mjd = bitsGet (ptr, 2, 0, 16);
  h1 = bitsDecodeBcd8 (bitsGet (ptr, 2, 16, 8));
  m1 = bitsDecodeBcd8 (bitsGet (ptr, 2, 24, 8));
  s1 = bitsDecodeBcd8 (bitsGet (ptr, 2, 32, 8));
  evt->start = get_time_mjd (mjd) + get_time_hms (h1, m1, s1);

  h2 = bitsDecodeBcd8 (bitsGet (ptr, 7, 0, 8));
  m2 = bitsDecodeBcd8 (bitsGet (ptr, 7, 8, 8));
  s2 = bitsDecodeBcd8 (bitsGet (ptr, 7, 16, 8));
  evt->duration = get_time_hms (h2, m2, s2);
  evt->running_status = bitsGet (ptr, 10, 0, 3);
  evt->ca_mode = bitsGet (ptr, 10, 3, 1);

  debugPri (2,
            "event id:%hu, start:%hhu:%hhu:%hhu, duration:%hhu:%hhu:%hhu, status:%hhu, ca:%hhu\n",
            evt->evt_id, h1, m1, s1, h2, m2, s2, evt->running_status,
            evt->ca_mode);
  desc_len = bitsGet (ptr, 10, 4, 12);
  if (sizeStackAdvance (s, 12, 0))
    return 1;
  if (sizeStackPush (s, desc_len))
    return 1;
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    if (sizeStackAdvance (s, 0, 2))
      return 1;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 0))
      return 1;
    if (sizeStackPush (s, d_len))
      return 1;
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x4d:                 //short event desc
      if (!evt->name)
      {
        if (sizeStackAdvance (s, 0, 4))
          return 1;
        evt->name = utlCalloc (1, ptr[3] + 1);
        if (sizeStackAdvance (s, 0, 4 + ptr[3]))
          return 1;
        memmove (evt->name, ptr + 4, ptr[3]);
        evt->name[ptr[3]] = '\0';

        if (sizeStackAdvance (s, 0, 5 + ptr[3]))
          return 1;

        evt->description = utlCalloc (1, ptr[4 + ptr[3]] + 1);
        if (sizeStackAdvance (s, 0, 5 + ptr[3] + ptr[4 + ptr[3]]))
          return 1;
        memmove (evt->description, ptr + 5 + ptr[3], ptr[4 + ptr[3]]);
        evt->description[ptr[4 + ptr[3]]] = '\0';
      }
      else
        debugPri (2, "Ignoring short event desc\n");
      break;
//                      case 0x4e://extended event desc
      //              break;ignore for now
    case 0x54:                 //content desc
      if ((!evt->content1) && (!evt->content2))
      {
        evt->content1 = bitsGet (ptr, 0, 0, 4);
        evt->content2 = bitsGet (ptr, 0, 4, 4);
      }
      break;
    case 0x55:                 //parental rating desc
      if (!evt->rating)
        evt->rating = ptr[3];
      break;
    default:
      debugPri (2, "Event ignoring descriptor: %u\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
      return 1;
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
      return 1;
  }
  if (sizeStackPop (s))
    return 1;
  return 0;
}

int
parse_eis (uint8_t * eis, EIS * p)
{
  uint16_t section_length;
  uint16_t svid;
  uint16_t onid;
  uint16_t tsid;
  uint32_t crc;
  uint8_t version_number;
  uint8_t current_next;
  uint8_t section_number;
  uint8_t last_section_number;
  uint8_t segment_last_section_number;
  uint8_t last_table_id;
  SizeStack s;
  uint8_t *ptr;
  int i, j, k;


  if (sizeStackInit (&s, eis) || sizeStackPush (&s, MAX_SEC_SIZE))      //maximum section size
    return 1;

  if (sizeStackAdvance (&s, 1, 2))
    return 1;
  ptr = sizeStackPtr (&s);
  section_length = bitsGet (ptr, 0, 4, 12);

  if (sizeStackAdvance (&s, 2, 1))
    return 1;

  if (sizeStackPush (&s, section_length))
    return 1;

  if (sizeStackAdvance (&s, 0, 11))
    return 1;

  ptr = sizeStackPtr (&s);

  svid = bitsGet (ptr, 0, 0, 16);
  version_number = bitsGet (ptr, 2, 2, 5);
  current_next = bitsGet (ptr, 2, 7, 1);
  section_number = ptr[3];
  last_section_number = ptr[4];
  tsid = bitsGet (ptr, 5, 0, 16);
  onid = bitsGet (ptr, 7, 0, 16);
  segment_last_section_number = ptr[9];
  last_table_id = ptr[10];
  debugPri (1,
            "EIS: svid: %u, tsid: %u, onid: %u, slast: %u lastid: %u ver: %u, cur/nxt: %u, secnr: %u, lastnr: %u\n",
            svid, tsid, onid, segment_last_section_number, last_table_id,
            version_number, current_next, section_number,
            last_section_number);

  if (sizeStackAdvance (&s, 11, 0))
    return 1;
  ptr = sizeStackPtr (&s);

  section_length -= 11 + 4;     //header+crc
  //count number of event blocks
  p->num_events = 0;
  i = section_length;
  j = 0;
  k = 0;
  while (i > 0)
  {
    j += 10;
    k = bitsGet (ptr, j, 4, 12) + 2;    //descriptor_loop_length
    j += k;
    i -= 10;
    i -= k;
    p->num_events++;
  }
  if (!p->num_events)
  {
    debugPri (1, "num_events=0\n");
    p->events = NULL;
    return 1;
  }
  p->events = utlCalloc (p->num_events, sizeof (EVT));
  if (!p->events)
    return 1;
  i = 0;
  while (p->num_events > i)
  {
    if (get_event (&s, &(p->events[i])))
    {
      while (i--)
      {
        evtClear (&(p->events[i]));
      }
      utlFAN (p->events);
      return 1;
    }
    i++;
  }
  sizeStackPop (&s);
  if (sizeStackAdvance (&s, 0, 4))
  {
    while (i--)
    {
      evtClear (&(p->events[i]));
    }
    utlFAN (p->events);
    debugPri (1, "bound error\n");
    return 1;
  }
  ptr = sizeStackPtr (&s);
  crc = bitsGet (ptr, 0, 0, 32);
  debugPri (1, "CRC: 0x%X\n", crc);
  sizeStackPop (&s);
  p->svc_id = svid;
  p->onid = onid;
  p->tsid = tsid;
  p->segment_last_section_number = segment_last_section_number;
  p->last_table_id = last_table_id;
  p->type = eis[0];
  p->c.version = version_number;
  p->c.current = current_next;
  p->c.section = section_number;
  p->c.last_section = last_section_number;
  return 0;
}


void
clear_eis (EIS * n)
{
  int i;
  for (i = 0; i < n->num_events; i++)
  {
    evtClear (&(n->events[i]));
  }
  if (n->num_events)
    utlFAN (n->events);
  memset (n, 0, sizeof (EIS));
}

typedef struct
{
  uint8_t *sec;
  SizeStack s;
} IterEisSec;


static int
get_first_ev (IterEisSec * iter)
{
//      uint8_t *ptr;
//      uint16_t mjd;
//      uint8_t h1,m1,s1,h2,m2,s2;
//      time_t start,duration;
  uint16_t section_length;
  if (sizeStackInit (&iter->s, iter->sec) || sizeStackPush (&iter->s, MAX_SEC_SIZE))    //maximum section size
    return 1;

  section_length = bitsGet (iter->sec, 1, 4, 12);

  if (secCheckCrc (iter->sec))
  {
    return 1;
  }

  if (sizeStackAdvance (&iter->s, 3, 1))
    return 1;

  if (sizeStackPush (&iter->s, section_length))
    return 1;

  if (sizeStackAdvance (&iter->s, 11, 16))      //or 14??
    return 1;

//convert time
/*
	ptr=sizeStackPtr(&iter->s);
	mjd=bitsGet(ptr,2,0,16);
	h1=bitsDecodeBcd8(bitsGet(ptr,2,16,8));
	m1=bitsDecodeBcd8(bitsGet(ptr,2,24,8));
	s1=bitsDecodeBcd8(bitsGet(ptr,2,32,8));
	start=get_time_mjd(mjd)+get_time_hms(h1,m1,s1);
	
	h2=bitsDecodeBcd8(bitsGet(ptr,7,0,8));
	m2=bitsDecodeBcd8(bitsGet(ptr,7,8,8));
	s2=bitsDecodeBcd8(bitsGet(ptr,7,16,8));
	duration=get_time_hms(h2,m2,s2);

	*((time_t*)(ptr+2))=start;
	*((time_t*)(ptr+2+sizeof(time_t)))=duration;
	*/
  return 0;
}

static void *
iterEisSecGetCurrent (void *it)
{
  IterEisSec *iter = it;
  return sizeStackPtr (&iter->s);
}

static void
iterEisSecFirst (void *it)
{
  get_first_ev (it);
}

static int
iterEisSecAdvance (void *it)
{
  IterEisSec *iter = it;
//      uint8_t *ptr;
//      uint16_t mjd;
//      uint8_t h1,m1,s1,h2,m2,s2;
  int k;
//      time_t start,duration;
  debugPri (1, "iterEisSecAdvance\n");
  if (sizeStackAdvance (&iter->s, 10, 2))
    return 1;
  k = bitsGet (sizeStackPtr (&iter->s), 0, 4, 12) + 2;  //descriptors_loop_length
  if (sizeStackAdvance (&iter->s, k, 16))       //or 14??
    return 1;
//convert time
/*this is bullshit. tampering with the data has no effect after the section is written. Avoid it. Use time data from List elements instead.*/
/*
	ptr=sizeStackPtr(&iter->s);
	mjd=bitsGet(ptr,2,0,16);
	h1=bitsDecodeBcd8(bitsGet(ptr,2,16,8));
	m1=bitsDecodeBcd8(bitsGet(ptr,2,24,8));
	s1=bitsDecodeBcd8(bitsGet(ptr,2,32,8));
	start=get_time_mjd(mjd)+get_time_hms(h1,m1,s1);
	
	h2=bitsDecodeBcd8(bitsGet(ptr,7,0,8));
	m2=bitsDecodeBcd8(bitsGet(ptr,7,8,8));
	s2=bitsDecodeBcd8(bitsGet(ptr,7,16,8));
	duration=get_time_hms(h2,m2,s2);

	*((time_t*)(ptr+2))=start;
	*((time_t*)(ptr+2+sizeof(time_t)))=duration;
	*/
  return 0;
}

/*
static pthread_mutex_t IterEisMx=PTHREAD_MUTEX_INITIALIZER;
static ItemList IterEisAllocator;
static int IterEisAllocatorInit=0;
*/
static void
iterEisSecDestroy (void *iter)
{
  utlFAN (iter);
  /*
     pthread_mutex_lock(&IterEisMx);
     itemRelease(&IterEisAllocator,iter);
     pthread_mutex_unlock(&IterEisMx); */
}

int
iterEisSecInit (Iterator * i, uint8_t * sec)
{
  IterEisSec *d;
  memset (i, 0, sizeof (Iterator));
  /*
     pthread_mutex_lock(&IterEisMx);

     if(!IterEisAllocatorInit)
     {
     if(itemListInit(&IterEisAllocator,sizeof(IterEisSec),32))
     {
     pthread_mutex_unlock(&IterEisMx);
     return 1;
     }
     IterEisAllocatorInit=1;
     }
   */
  d = utlCalloc (1, sizeof (IterEisSec));

//      d=itemAcquire(&IterEisAllocator);
//      pthread_mutex_unlock(&IterEisMx);
  if (!d)
    return 1;
  d->sec = sec;
  if (get_first_ev (d))
  {
    utlFAN (d);
//              pthread_mutex_lock(&IterEisMx);
//              itemRelease(&IterEisAllocator,d);
//              pthread_mutex_unlock(&IterEisMx);
    return 1;
  }

  i->s.first_element = iterEisSecFirst;
  i->s.advance = iterEisSecAdvance;
  i->data = d;
  i->kind = I_SEQ_FWD;
  i->get_element = iterEisSecGetCurrent;
  i->i_destroy = iterEisSecDestroy;
  return 0;
}
