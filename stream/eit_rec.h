#ifndef __eit_rec_h
#define __eit_rec_h
#include <stdlib.h>
#include "custck.h"
#include "rs_entry.h"
#include "rcfile.h"
#include "pgmdb.h"
#include "dvb.h"
#include "task.h"
#include "recorder.h"
#include "sec_ex.h"
#include "selector.h"
#include "dmalloc.h"


/**
 *\file eit_rec.h
 *\brief EIT capable ts recorder
 *
 *eitRecScan() starts the recorder. 
 *recorder has to be started with the desired transponder tuned. 
 *if e->flags&RSF_EIT is true, recorder will use EIT, otherwise it starts immediately. 
 *
 *eitRecAbort() may be called to clean up recorders. 
 *Unconditionally stops and clears recorder.
 *
 */


typedef struct _EitRec EitRec;

/**
 *\brief wraps recorder to handle EIT
 */
struct _EitRec
{
  Task t;
  RsEntry *e;
  RcFile *cfg;
  dvb_s *dvb;
  PgmDb *pgmdb;
  int eit_state;
  time_t start;                 ///<real start time
  time_t last_eit_seen;         ///<when we saw last present/following eit table. used for triggering timer-controlled mode.
  time_t last_id_seen;          ///<when we saw last present/following event with matching ID
  DListE *eit_port;
  SvcOut *o;
  Selector *eit_sel;
  SecEx eit_ex;
  Recorder rec;                 ///<a simple recorder to start/stop suspend/resume
  void *x;
  void (*done_cb) (EitRec * r, RsEntry * e, void *x);
};

///async start the recorder by scanning for a PIL
int eitRecScan (EitRec * r, RsEntry * e, RcFile * cfg, dvb_s * dvb,
                PgmDb * pgmdb, void *x, void (*done_cb) (EitRec * r,
                                                         RsEntry * rse,
                                                         void *x));

/**
 *\brief hard stop
 *
 *should be called on shutdown to stop any active recorders
 */
int eitRecAbort (EitRec * r);

///returns zero if not active
int eitRecActive (EitRec * r);


#endif //__eit_rec_h
