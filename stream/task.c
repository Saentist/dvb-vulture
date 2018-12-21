#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>
#include <errno.h>
#include "sec_common.h"
#include "utl.h"
#include "task.h"
#include "dmalloc.h"
#include "debug.h"

int DF_TASK;
#define FILE_DBG DF_TASK

int
taskInit (Task * r, void *x, void *(*func) (void *x))
{
  if ((!r) || (!func))
    return 1;
  memset (r, 0, sizeof (Task));
  r->x = x;
  r->task_func = func;
  pthread_mutex_init (&r->mx, NULL);
  return 0;
}

int
taskLock (Task * r)
{
  return pthread_mutex_lock (&r->mx);
}

int
taskUnlock (Task * r)
{
  return pthread_mutex_unlock (&r->mx);
}

void
CUtaskClear (void *d)
{
  taskClear (d);
}

int
CUtaskInit (Task * r, void *x, void *(*func) (void *x), CUStack * s)
{
  int rv = taskInit (r, x, func);
  cuStackFail (rv, r, CUtaskClear, s);
  return rv;
}

int
taskClear (Task * r)
{
  taskStop (r);
  pthread_mutex_destroy (&r->mx);
  memset (r, 0, sizeof (Task));
  return 0;
}

int
taskStart (Task * r)
{
//      pthread_attr_t attr;//see if we can make it crash(and where)
  debugMsg ("taskStart\n");
//      pthread_attr_init(&attr);
//      pthread_attr_setguardsize(&attr,0);
  pthread_mutex_lock (&r->mx);
  if (r->running)
  {
    debugMsg ("r->running\n");
    pthread_mutex_unlock (&r->mx);
    return 0;
  }
  r->running = 1;
  pthread_mutex_unlock (&r->mx);
  debugMsg ("creating thread\n");
  pthread_create (&r->thread, NULL /*&attr */ , r->task_func, r->x);
  debugMsg ("done\n");
  return 0;
}

int
taskStop (Task * r)
{
  void *rtn;
  debugMsg ("taskStop\n");
  pthread_mutex_lock (&r->mx);
  if (!r->running)
  {
    debugMsg ("!r->running\n");
    pthread_mutex_unlock (&r->mx);
    return 0;
  }
  r->running = 0;
  pthread_mutex_unlock (&r->mx);
  debugMsg ("joining %x %x\n", r->thread, &rtn);
  pthread_join (r->thread, &rtn);
  debugMsg ("done\n");
  return 0;
}
