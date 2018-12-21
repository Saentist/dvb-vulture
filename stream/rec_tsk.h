#ifndef __rec_tsk_h
#define __rec_tsk_h
#include "rec_sched.h"
#include "eit_rec.h"

/**
 *\file rec_tsk.h
 *\brief background recording schedule task
 */

//typedef struct PgmState_s PgmState;

typedef struct
{
  Task t;
  RecSchedFile sch;
  struct PgmState_s *p;
  DList active;                 ///<active recording threads \see eit_rec.h
  DList done;                   ///<ended recording threads waiting for join()
} RecTask;

int recTaskInit (RecTask * rt, RcFile * cfg, struct PgmState_s *p);
void recTaskClear (RecTask * rt);

int CUrecTaskInit (RecTask * rt, RcFile * cfg, struct PgmState_s *p,
                   CUStack * s);

#define recTaskStart(RTP_) taskStart(&((RTP_)->t))
#define recTaskStop(RTP_) taskStop(&((RTP_)->t))
#define recTaskLock(t_)  taskLock(&(t_)->t)
#define recTaskUnlock(t_)  taskUnlock(&(t_)->t)

int recTaskCancelRunning (RecTask * rt);        ///<cancel currently running recordings

/**
must be called with recTaskLock() held
*/
int recTaskSndSched (int sock, RecTask * rt);
/**
must be called with recTaskLock() held
*/
int recTaskRcvSched (int sock, RecTask * rt);
/**
must be called with recTaskLock() held
*/
int recTaskRcvEntry (int sock, RecTask * rt);

int recTaskActive (RecTask * rt);       ///<return one if at least one recording is active

#endif
