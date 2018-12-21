#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include "selector.h"
#include "sit_com.h"
#include "utl.h"
#include "rcptr.h"
#include "pidbuf.h"
#include "bits.h"
#include "dmalloc.h"
#include "debug.h"

int DF_SELECTOR;
#define FILE_DBG DF_SELECTOR

static uint16_t *selector_get_pids_internal (Selector * b,
                                             uint16_t * num_ret);

static void
selector_release_element_internal (Selector * d, void *p)
{
  if (1 == rcptrReleaseIl (&d->payload, p))
    d->num_buffers--;
}

static void *
selector_read_element_internal (Selector * d, DListE * port)
{
  SListE *e;
  SelE *s;
  SelPort *prt;
  void *rv = NULL;
  prt = (SelPort *) port;

  e = slistRemoveFirst (&prt->q);
  if (e)
  {
    s = (SelE *) e;
    rv = s->data;
    itemRelease (&d->elements, e);
  }
  return rv;
}

int
selectorLock (Selector * s)
{
  return pthread_mutex_lock (&s->mx);
}

int
selectorUnlock (Selector * s)
{
  return pthread_mutex_unlock (&s->mx);
}

int
selectorRemovePort (DListE * port)
{
  uint16_t *pids_before = NULL;
  uint16_t *pids_after = NULL;
  uint16_t sz_before = 0;
  uint16_t sz_after = 0;
  uint16_t *add_set = NULL;
  uint16_t *rmv_set = NULL;
  uint16_t sz_add = 0;
  uint16_t sz_rmv = 0;
  Selector *s = NULL;
  SelPort *sp = NULL;
  debugMsg ("removing port\n");
  if (NULL == port)
  {
    errMsg ("Error: NULL ptr port\n");
    return 1;
  }
  sp = (SelPort *) port;
  s = sp->backptr;
  if (NULL == s)
  {
    errMsg ("Error: NULL ptr selector\n");
    return 1;
  }
  pthread_mutex_lock (&s->mx);
  pids_before = selector_get_pids_internal (s, &sz_before);
  while (selectorDataAvail (port))
  {
    void *p;
    p = selector_read_element_internal (s, port);
    if (p)
      selector_release_element_internal (s, p);
  }
  dlistRemove (&s->ports, port);
  pids_after = selector_get_pids_internal (s, &sz_after);
  rmv_set =
    pidbufDifference (pids_before, sz_before, pids_after, sz_after, &sz_rmv);
  add_set =
    pidbufDifference (pids_after, sz_after, pids_before, sz_before, &sz_add);
  if (sz_add || sz_rmv)
    s->pid_change (add_set, sz_add, rmv_set, sz_rmv, s->dta);
  utlFAN (pids_after);
  utlFAN (pids_before);
  utlFAN (add_set);
  utlFAN (rmv_set);
  utlFAN (sp->pids);
  memset (sp, 0, sizeof (*sp));
  utlFAN (port);
  pthread_mutex_unlock (&s->mx);
  debugMsg ("removing port done\n");
  return 0;
}

/*
uint32_t selector_pid_hash(void* d)
{
Some ideas...

Hmm the ports service a range of pids, so don't have a single hash. We can either insert another level of indirection or build a custom hashmap
several ports may sit on the same pid aswell.

SelPort ** hmap;///<hashmap
unsigned hmap_size;///<number of elements

use ref counted ptrs for ports
use open addressing
reinitialize hashmap when ports are added/removed

The number of ports is(typically) a small integer as well as the number
of pids in a port, hashmap won't speed up processing significantly.

if we want to select the ouput qeue in constant time, we definitely 
need a SelPid struct which references a pid's port

typedef struct
{
	uint16_t pid;
	SelPort * port;
}SelPid;

for inserting a port with n pids
we set up the port,
generate n SelPid Objects,
have them point at the port
and insert each one with it's pid

for removing a port with n pids
we look up each n pids in turn
for each SelPid found, which may be several for a single pid, 
we compare the port ptr with SelPid::port
If it matches, remove SelPid.

for demuxing a packet, look up it's pid,
and queue at the found ports.
same port must not be registered multiple 
times for the same pid

no need to bother optimizing here, other things are far worse
most of them inside the kernel

}
*/
int
selectorInit (Selector * s,
              void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                                  uint16_t * rmv_set, uint16_t rmv_size,
                                  void *dta), void *dta, uint32_t limit)
{
  debugMsg ("selector init\n");
  memset (s, 0, sizeof (Selector));
  rcptrListInit (&s->payload, sizeof (SelBuf), limit);
  itemListInit (&s->elements, sizeof (SelE), 3 * limit);
  s->limit = limit;
  s->dta = dta;
  s->pid_change = pid_change;
  pthread_mutex_init (&s->mx, NULL);
  debugMsg ("selector init done\n");
  return 0;
}

void
selectorClear (Selector * d)
{
  DListE *e;
  debugMsg ("selector clear\n");
  e = dlistFirst (&d->ports);
  while (e)
  {
    DListE *next = dlistENext (e);
    selectorRemovePort (e);
    e = next;
  }
//      pthread_cond_destroy(&d->avail_cond);
  pthread_mutex_destroy (&d->mx);
  itemListClear (&d->elements);
  rcptrListClear (&d->payload);
  memset (d, 0, sizeof (Selector));
  debugMsg ("selector clear done\n");
}

void
CUselectorClear (void *d)
{
  selectorClear (d);
}

int
CUselectorInit (Selector * d,
                void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                                    uint16_t * rmv_set, uint16_t rmv_size,
                                    void *dta), void *dta, uint32_t limit,
                CUStack * s)
{
  int rv = selectorInit (d, pid_change, dta, limit);
  cuStackFail (rv, d, CUselectorClear, s);
  return rv;
}

static void
buf_packet_here (SelPort * sp, NOTUSED void *dta, void *p)
{
  //put it in the queue
  SelE *s = NULL;
  s = itemAcquire (&sp->backptr->elements);
  s->data = p;
  slistInsertLast (&sp->q, &s->e);
}

/*
how to do the initialisation here?
the func_change has to start/stop
svc trackers...
*/


DListE *
selectorAddPortSyncUnsafe (Selector * d,
                           void (*packet_here) (SelPort * p, void *dta,
                                                void *pk), void *dta)
{
  DListE *rv = NULL;
  SelPort *p = NULL;
  if (NULL == d)
    return NULL;
  debugMsg ("adding port\n");
  rv = utlCalloc (1, sizeof (SelPort));
  if (rv)
  {
    p = (SelPort *) rv;
    p->backptr = d;
    if (packet_here)
    {
      p->packet_here = packet_here;
      debugMsg ("Packet Sync port\n");
    }
    else
    {
      p->packet_here = buf_packet_here;
      debugMsg ("async/buffering port\n");
    }
    p->dta = dta;
    dlistInsertFirst (&d->ports, rv);
    debugMsg ("no pids, events only\n");
  }
  else
  {
    errMsg ("out of mem\n");
  }
  debugMsg ("adding port done\n");
  return rv;

}

DListE *
selectorAddPortSync (Selector * d,
                     void (*packet_here) (SelPort * p, void *dta, void *pk),
                     void *dta)
{
  DListE *rv;
  pthread_mutex_lock (&d->mx);
  rv = selectorAddPortSyncUnsafe (d, packet_here, dta);
  pthread_mutex_unlock (&d->mx);
  return rv;
}

DListE *
selectorAddPort (Selector * d)
{
  DListE *rv;
  pthread_mutex_lock (&d->mx);
  rv = selectorAddPortUnsafe (d);
  pthread_mutex_unlock (&d->mx);
  return rv;
}

DListE *
selectorAddPortUnsafe (Selector * d)
{
  return selectorAddPortSyncUnsafe (d, NULL, NULL);
}

static int
selector_queue_element (NOTUSED Selector * d, SelPort * port, void *p)
{
  if (port->packet_here)
  {
    rcptrAcquire (p);
    port->packet_here (port, port->dta, p);
  }
  return 0;
}

int
selectorFeedUnsafe (Selector * d, void *p)
{
  DListE *e;
  uint16_t pid = tsGetPid (selectorEPayload (p));
  e = dlistFirst (&d->ports);
  while (e)
  {
    int i;
    SelPort *port;
    port = (SelPort *) e;
    if (selectorEIsEvt (p))
    {
      selector_queue_element (d, port, p);
    }
    else
    {
      for (i = 0; i < port->num_pids; i++)
        if (port->pids[i] == pid)       //TODO use some array or hash for this... 
        {
          selector_queue_element (d, port, p);
        }
    }
    e = dlistENext (e);
  }
  if (1 == rcptrReleaseIl (&d->payload, p))     //will go bye here if no ports
  {                             //usually this indicates stale elements got fed into the queue... do not know how to avoid this yet...
    debugMsg ("DISCARDED Element: %" PRIu16 "\n", pid);
    d->num_buffers--;
  }
  return 0;
}

///does nothing...
int
selectorSignalUnsafe (Selector * d NOTUSED)
{
//      DListE *e;
  int rv = 0;
//      e=dlistFirst(&d->ports);
/*	while(e)
	{
		SelPort * port;
		port=dlistEPayload(e);
		if(selectorDataAvail(d,e))
		{
			rv=rv||pthread_cond_broadcast(&port->avail_cond);
		}
		e=dlistENext(e);
	}*/
  return rv;
}

int
selectorFeed (Selector * d, void *p)
{
  /*
     go through all ports
     and add elements for payload if pid matches
   */
//      debugMsg("selector feed\n");
  pthread_mutex_lock (&d->mx);
  selectorFeedUnsafe (d, p);
  selectorSignalUnsafe (d);
  pthread_mutex_unlock (&d->mx);
//      debugMsg("selector feed done %u\n",d->num_buffers);
  return 0;
}

void *
selectorReadElementUnsafe (DListE * port)
{
  Selector *d = NULL;
  SelPort *p = NULL;
  if (!port)
    return NULL;
  p = (SelPort *) port;
  d = p->backptr;
  if (!d)
    return NULL;
  return selector_read_element_internal (d, port);
}


void *
selectorReadElement (DListE * port)
{
  void *rv = NULL;
  SelPort *p = (SelPort *) port;
  Selector *d = p->backptr;     //FIXXME this is a problem
  pthread_mutex_lock (&d->mx);
  rv = selectorReadElementUnsafe (port);
  pthread_mutex_unlock (&d->mx);
  return rv;
}

void
selectorReleaseElementUnsafe (Selector * d, void *p)
{
  selector_release_element_internal (d, p);     //annoying, isn't it ?
}

void
selectorReleaseElement (Selector * d, void *p)
{
//      debugMsg("selector release element\n");
  pthread_mutex_lock (&d->mx);
  selectorReleaseElementUnsafe (d, p);
  pthread_mutex_unlock (&d->mx);
//      debugMsg("selector release element done: %u\n",d->num_buffers);
}

int
selectorDataAvail (DListE * port)
{
  SelPort *prt;
  if (!port)
    return 0;
  prt = (SelPort *) port;
  return (slistFirst (&prt->q) != NULL);
}

void *
selectorAllocElementUnsafe (Selector * d)
{
  void *rv;
  if (d->num_buffers >= d->limit)
  {
    errMsg ("selector: out of buffers\n");
    return NULL;
  }
  rv = rcptrMallocIl (&d->payload);
  if (rv)
    d->num_buffers++;
  return rv;
}

void *
selectorAllocElement (Selector * d)
{
  void *rv;
//      debugMsg("selector alloc element\n");
  pthread_mutex_lock (&d->mx);
  rv = selectorAllocElementUnsafe (d);
  pthread_mutex_unlock (&d->mx);
//      debugMsg("selector alloc element done: %u\n",   d->num_buffers);
  return rv;
}

int
selectorIsEmpty (Selector * d)
{
  int rv;
  pthread_mutex_lock (&d->mx);
  rv = (d->num_buffers == 0);
  pthread_mutex_unlock (&d->mx);
  debugMsg ("selectorIsEmpty: %" PRIu32 "\n", d->num_buffers);
  return rv;
}

int
selectorWaitData (DListE * port, int timeout_ms)
{
  int rv = 0;
  SelPort *prt;
  Selector *b = NULL;
  if (!port)
    return 1;
  prt = (SelPort *) port;
  b = prt->backptr;
  pthread_mutex_lock (&b->mx);
  if (timeout_ms == 0)
    timeout_ms = 1;

  while ((!prt->q.first) && (timeout_ms > 0))
  {
    pthread_mutex_unlock (&b->mx);
    usleep (300000);
    timeout_ms -= 300;
    pthread_mutex_lock (&b->mx);
  }
  rv = (prt->q.first == NULL);
  pthread_mutex_unlock (&b->mx);
  return (rv);
}

static uint16_t *
selector_get_pids_internal (Selector * b, uint16_t * num_ret)
{
  uint16_t *rv = NULL;
  DListE *port = NULL;
  uint16_t num_alloc = 128;
  uint16_t i, pid_ctr = 0;
  debugMsg ("selector get pids internal\n");
  rv = utlCalloc (num_alloc, sizeof (uint16_t));
  if (!rv)
  {
    return NULL;
  }
  pid_ctr = 0;
  port = dlistFirst (&b->ports);
  while (port)
  {
    SelPort *prt = (SelPort *) port;
    for (i = 0; i < prt->num_pids; i++)
    {
      if (!pidbufContains (prt->pids[i], rv, pid_ctr))
      {
        if (pid_ctr >= num_alloc)
        {
          void *p;
          num_alloc *= 2;
          p = utlRealloc (rv, num_alloc * sizeof (uint16_t));
          if (!p)
          {
            utlFAN (rv);
            return NULL;
          }
          rv = p;
        }
        rv[pid_ctr] = prt->pids[i];
        pid_ctr++;
      }
    }
    port = dlistENext (port);
  }
  if (0 == pid_ctr)
  {
    utlFAN (rv);
    rv = NULL;
  }
  *num_ret = pid_ctr;
  debugMsg ("selector get pids internal done\n");
  return rv;
}

uint16_t *
selectorGetPids (Selector * b, uint16_t * num_ret)
{
  uint16_t *rv = NULL;
  pthread_mutex_lock (&b->mx);
  rv = selector_get_pids_internal (b, num_ret);
  pthread_mutex_unlock (&b->mx);
  return rv;
}

/**
 *\brief removes elements with wrong pid from queue
 *
 *\warning must be called with mutex already held
 */
void
selPortRSU (DListE * port)
{
  SelPort *p = (SelPort *) port;
  Selector *d = p->backptr;
  SListE *prev, *this;
  SelE *e2;
  debugMsg ("removing stale elements\n");
  prev = NULL;
  this = slistFirst (&p->q);
  while (this)
  {
    e2 = (SelE *) this;
    if ((!selectorEIsEvt (e2->data)) &&
        (!pidbufContains
         (tsGetPid (selectorEPayload (e2->data)), p->pids, p->num_pids)))
    {                           //not an event and not filtered pid... remove it
      if (1 == rcptrReleaseIl (&d->payload, e2->data))
        d->num_buffers--;
      slistRemoveNext (&p->q, prev);    //yes, this works with null ptr
      itemRelease (&d->elements, this);
      this = (NULL != prev) ? slistENext (prev) : slistFirst (&p->q);   //but this doesn't
      debugMsg ("remove_stale_elements: %" PRIu32 "\n", d->num_buffers);
    }
    else
    {
      prev = this;
      this = slistENext (this);
    }
  }
}

static void
selector_port_remove_packets (Selector * d, DListE * port)      //something may be wrong here
{
  SelPort *p = (SelPort *) port;
  SListE *prev, *this;
  SelE *e2;
  debugMsg ("removing ports' elements\n");
  prev = NULL;
  this = slistFirst (&p->q);
  while (this)
  {
    e2 = (SelE *) this;
    if (!selectorEIsEvt (e2->data))
    {                           //not an event... remove it
      if (1 == rcptrReleaseIl (&d->payload, e2->data))
        d->num_buffers--;
      slistRemoveNext (&p->q, prev);    //yes, this works with null ptr
      itemRelease (&d->elements, this);
      this = (NULL != prev) ? slistENext (prev) : slistFirst (&p->q);   //but this doesn't... list Interface needs improving
      debugMsg ("remove_packets: %" PRIu32 "\n", d->num_buffers);
    }
    else
    {
      prev = this;
      this = slistENext (this);
    }
  }
}

int
selectorReinit (Selector * d)
{
  DListE *port;
  debugMsg ("selectorReinit\n");
  pthread_mutex_lock (&d->mx);
  port = dlistFirst (&d->ports);
  while (port)
  {
    SelPort *p = (SelPort *) port;
    selector_port_remove_packets (d, port);
    utlFAN (p->pids);
    p->num_pids = 0;
    p->pids = NULL;
    port = dlistENext (port);
  }
  pthread_mutex_unlock (&d->mx);
  return 0;
}


int
selectorModPortUnsafe (DListE * port, uint16_t n_pids, uint16_t * pids)
{
  SelPort *p = NULL;
  uint16_t *pids_before = NULL;
  uint16_t *pids_after = NULL;
  uint16_t sz_before = 0;
  uint16_t sz_after = 0;
  uint16_t *add_set = NULL;
  uint16_t *rmv_set = NULL;
  uint16_t sz_add = 0;
  uint16_t sz_rmv = 0;
  Selector *d = NULL;
  debugMsg ("selector mod port unsafe\n");
  if (NULL == port)
    return 1;
  p = (SelPort *) port;
  d = p->backptr;
  if (NULL == d)
    return 1;

  pids_before = selector_get_pids_internal (d, &sz_before);
  pidbufDump ("mod port before", pids_before, sz_before);

//  selector_port_remove_packets (d, port);

  utlFAN (p->pids);
  if ((n_pids == 0) || (pids == NULL))
  {
    p->num_pids = 0;
    p->pids = NULL;
  }
  else
  {
    p->num_pids = n_pids;
    p->pids = pidbufDup (pids, n_pids);
  }
  pids_after = selector_get_pids_internal (d, &sz_after);
  pidbufDump ("mod port after", pids_after, sz_after);
  rmv_set =
    pidbufDifference (pids_before, sz_before, pids_after, sz_after, &sz_rmv);
  add_set =
    pidbufDifference (pids_after, sz_after, pids_before, sz_before, &sz_add);
  if (sz_add || sz_rmv)
  {
    d->pid_change (add_set, sz_add, rmv_set, sz_rmv, d->dta);
    selPortRSU (port);
  }
  utlFAN (pids_after);
  utlFAN (pids_before);
  utlFAN (add_set);
  utlFAN (rmv_set);
  debugMsg ("selector mod port unsafe done\n");
  return 0;

}

int
selectorModPort (DListE * port, uint16_t n_pids, uint16_t * pids)
{
  SelPort *p = NULL;
  Selector *d = NULL;
  int rv;
  debugMsg ("selector mod port\n");
  if (NULL == port)
    return 1;
  p = (SelPort *) port;
  d = p->backptr;
  if (NULL == d)
    return 1;

  pthread_mutex_lock (&d->mx);
  rv = selectorModPortUnsafe (port, n_pids, pids);
  pthread_mutex_unlock (&d->mx);
  debugMsg ("selector mod port done\n");
  return rv;
}

uint16_t *
selectorPortGetPids (DListE * port, uint16_t * num_pids)
{
  uint16_t *rv = NULL;
  Selector *d = NULL;
  SelPort *prt = NULL;
  debugMsg ("selector get pids\n");
  prt = (SelPort *) port;
  d = prt->backptr;
  pthread_mutex_lock (&d->mx);
  if (0 != prt->num_pids)
    rv = utlCalloc (prt->num_pids, sizeof (uint16_t));
  *num_pids = prt->num_pids;
  if (rv)
    memmove (rv, prt->pids, prt->num_pids * sizeof (uint16_t));
  pthread_mutex_unlock (&d->mx);
  debugMsg ("selector get pids done\n");
  return rv;
}

int
selectorSndEvt (Selector * b, uint8_t evt, void *e, size_t sz)
{
  SelBuf *d;
  debugMsg ("selector send event\n");
  if (sz > sizeof (DvbEvtD))
    return 1;

  pthread_mutex_lock (&b->mx);
  d = rcptrMallocIl (&b->payload);
  if (d)
    b->num_buffers++;
  else
  {
    pthread_mutex_unlock (&b->mx);
    return 1;
  }
  pthread_mutex_unlock (&b->mx);
  d->type = 1;
  d->event.id = evt;
  if ((e != NULL) && sz)
    memmove (&(d->event.d), e, sz);
  debugMsg ("selector send event done?\n");
  return selectorFeed (b, d);
}
