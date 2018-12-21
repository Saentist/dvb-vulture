#ifndef __recorder_h
#define __recorder_h
#include <stdlib.h>
#include "custck.h"
#include "rcfile.h"
#include "pgmdb.h"
#include "dvb.h"
#include "task.h"
#include "selector.h"
#include "dmalloc.h"

/**
 *\file recorder.h
 *\brief records (partial) transport stream to disk
 *
 */

/**
 *\brief transport stream to file recorder
 */
typedef struct
{
  Task recording_task;          ///<writes to the file
  FILE *output;                 ///<destination
  Selector *sel;
  DListE *port;
  SvcOut *o;
  dvb_s *dvb;
  time_t start_time;            ///<start timestamp
  int err;                      ///<error flag. cleared by recorderClearErr() and recorderGetState()
  uint16_t pnr;                 ///<which pgm it is
  int paused;
} Recorder;

/**
 *\brief recorder status data
 */
typedef struct
{
  time_t time;                  ///<duration so far
  off_t size;                   ///<current offset in recorded file
  int err;                      ///<error flag
} RState;

#define recorderActive(_R_P) taskRunning(&(_R_P)->recording_task)
/**
 *\brief start recording
 *safe to call multiple times. Will fail if already started.
 */
int recorderStart (Recorder * r, char *evt_name, RcFile * cfg, dvb_s * dvb,
                   PgmDb * pgmdb, uint16_t pnr, uint8_t flags);
///safe to call multiple times. Will fail if already stopped.
int recorderStop (Recorder * r);

int recorderPause (Recorder * r);
int recorderResume (Recorder * r);
int recorderGetState (Recorder * r, RState * rs);
void recorderClearErr (Recorder * r);
int recorderErr (Recorder * r);
#endif
