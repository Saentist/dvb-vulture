#ifndef __off_tsk_h
#define __off_tsk_h
/**
 *\file off_tsk.h
 *\brief task wich manages offline activities
 *
 *Adjusts system time at specified intervals and collects epg and videotext on known transponders
 *
 *We start this task when the last connection terminates and no recording is active. 
 *We stop it when we get new connection or start recording.
 */
#include <stdint.h>
#include <time.h>
#include "task.h"
#include "list.h"
#include "tp_info.h"
#include "rcfile.h"
typedef struct
{
  Task t;
  struct PgmState_s *p;         ///<backpointer
  int state;
  time_t last_adjust;           ///<when our clock was last adjusted
  time_t adjust_interval;       ///<how long we wait at least between successive time adjustments
  time_t acq_start;             ///<start of acquisition phase
  time_t acq_duration;          ///<how long to acquire on one transponder
  time_t idle_duration;         ///<how long to do nothing
  time_t idle_start;            ///<start of idle phase
  uint32_t t_pos;               ///<position to use
  uint32_t t_freq;              ///<frequency to use
  uint8_t t_pol;                ///<polarisation to use(for TDT adjtime)
  int num_pos;                  ///<how many positions in total
  int current_pos;              ///<current one for vt scan
  TransponderInfo *tpi;         ///<positions' transponders
  int current_tp;               ///<current transponder for vt scan
  int num_tp;                   ///<total number of transponders in array
} OfflineTask;

int offlineTaskInit (OfflineTask * rt, RcFile * cfg, struct PgmState_s *p);
void offlineTaskClear (OfflineTask * rt);
int CUofflineTaskInit (OfflineTask * rt, RcFile * cfg, struct PgmState_s *p,
                       CUStack * s);
#define offlineTaskStart(_RTP) taskStart(&((_RTP)->t))
#define offlineTaskStop(_RTP) taskStop(&((_RTP)->t))
#endif
