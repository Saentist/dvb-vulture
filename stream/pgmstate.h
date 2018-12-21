#ifndef __pgmstate_h
#define __pgmstate_h
#include <pthread.h>
#include "dvb.h"
#include "rcfile.h"
#include "vtdb.h"
#include "pgmdb.h"
//#include "buffer.h"
//#include "distributor.h"
#include "list.h"
#include "custck.h"
#include "net_writer.h"
//#include "sec_reader.h"
//#include "vt_reader.h"
#include "rec_tsk.h"
#include "epgdb.h"
#include "mheg.h"
#include "sap.h"
#include "off_tsk.h"
/**
 *\file pgmstate.h
 *\brief overall program state
 */

typedef struct Connection_s Connection;

///contains almost everything else
struct PgmState_s
{
  RcFile conf;
  char *pidfile;
  dvb_s dvb;
  RecTask recorder_task;
  OfflineTask ot;
  useconds_t abort_timeout_us;
  int abort_poll_count;
  PgmDb program_database;
  VtDb videotext_database;
  Mheg mheg;
  EpgDb epg_database;
  pthread_mutex_t conn_mutex;
  char *super_addr;             ///<connections from here get super. points into conf. do not free().
//  struct sockaddr_storage super_addr;///<connections from here get super
  Connection *master;           ///<the connection that is active, may be NULL. there may only be one. That one is allowed to do actions which require exclusive access.
  DList conn_active;            ///<all connections
  DList conn_dead;              ///<connections to be destroyed
  NetWriter net_writer;
  DvbSap sap_announcer;
  CUStack destroy;
};

typedef struct PgmState_s PgmState;


/**
 *\brief sets the connection's pnrs to -1 (invalid)
 *
 *Does not actually generate a pnr remove event.
 *Should be used right before tuning.
 *must be called with conn_mutex locked already
 */
int pgmRmvAllPnrs (PgmState * p);
///returns true if pnr is already being streamed by any connection
int pgmStreaming (PgmState * p, uint16_t pnr);
#endif
