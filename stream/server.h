#ifndef __server_h
#define __server_h
#include <pthread.h>
#include "srv_codes.h"
#include "pgmstate.h"
#include "recorder.h"

/**
 *\file server.h
 *\brief per connection state and client methods
 */

///one connection
struct Connection_s
{
  DListE e;
  int sockfd;                   ///<the socket
  pthread_t thread;             ///<connection thread
  PgmState *p;                  ///<a backpointer
  int32_t pnr;                  ///< -1 if we are not streaming, else program number
  uint8_t dead;                 ///<tells the connection thread to exit
  Recorder quickrec;            ///<for quick recording the currently watched stream
        /**
	 *if super is one the user may become active anytime as long as no other connections are super _and_ active. <br>
	 *connections without super set may only go active if no one is active.<br>
	 *connections need to be active if they want to tune or scan.<br>
	 *connections need to be active _and_ super if they want to add or del transponders or configure switch positions/LNBs.<br>
	 *whether a connection is super is determined by the super=addr entry in the [SERVER] section.<br>
	 *connections from the specified host are super.
	 */
  uint8_t active;
  uint8_t super;
  int niceness;
};

int connectionInit (Connection * c, int fd, PgmState * p,
                    struct sockaddr_storage *peer_addr, int niceness);
void connectionClear (Connection * c);
int srvGetDvbStats (Connection * c);
int srvGetSwitch (Connection * c);
int srvSetSwitch (Connection * c);
int srvNumPos (Connection * c);
int srvListTp (Connection * c);
int srvScanTp (Connection * c);
int srvListPg (Connection * c);
int srvTunePg (Connection * c);
int srvSetFcorr (Connection * c);
int srvGetFcorr (Connection * c);
//int tune_name_pg(Connection * c);
//int scan_full(Connection * c);
int srvAddTp (Connection * c);
int srvDelTp (Connection * c);
int srvEpgGetData (Connection * c);
int srvGoInactive (Connection * c);
int srvGoActive (Connection * c);
//int list_pids(Connection * c);
int srvVtGetSvcPk (Connection * c);
int srvVtGetMagPk (Connection * c);
int srvVtGetPagPk (Connection * c);
int srvVtGetPnrs (Connection * c);
int srvVtGetPids (Connection * c);
int srvVtGetSvc (Connection * c);
int srvRecPgm (Connection * c);
int srvRecPgmStop (Connection * c);
int srvRecSched (Connection * c);
int srvGetSched (Connection * c);
int srvTpFt (Connection * c);
int srvAbortRec (Connection * c);       ///<stop scheduled&active recordings
int srvAddSched (Connection * c);       ///<add one entry to recording schedule
int srvTpStatus (Connection * c);
int srvRstat (Connection * c);
int srvUstat (Connection * c);
//not for direct client use
int srvTuneTpi (PgmState * p, uint32_t pos, TransponderInfo * t);

#endif
