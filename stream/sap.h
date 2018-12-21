#ifndef __sap_h
#define __sap_h
#include "dvb.h"
#include "pgmdb.h"
#include "epgdb.h"
#include "task.h"
#include "svt.h"
/**
 *\file sap.h
 *\brief session announcement for dvb streams
 *
 *on TUNING or IDLE event, we remove announcement, on PNR_ADD and PNR_RMV we update announcement
 */

typedef struct
{
  PgmDb *pgm_db;
  EpgDb *epg_db;
  dvb_s *dvb;
  Task t;
  uint8_t sap_packet[1024];
  uint16_t sap_len;
  DListE *input;
  SvcOut *o;
  uint16_t *pnrs;
  uint16_t num_pnrs;

  char *my_addr;                ///<address in the origin field of sap packet. dotted decimal representation

  struct sockaddr_storage sap_addr;     ///<for sendto()

  char *bcast_addr;             ///<address to advertise (the one net_writer uses). Dotted decimal.
  uint16_t bcast_port;          ///<port to advertise (host byte order)
  int bcast_af;                 ///<the address family (the one net_writer uses). AF_INET or AF_INET6.

  char *username;               ///<username to use in advert
  uint32_t s_id;                ///<sap id. doesn't change for session
  uint32_t s_ver;               ///<sap version. changes with content
  unsigned ttl;                 ///<ttls for our broadcasts

  uint8_t tuned;                ///<current state, we set it to one when pnr gets added or removed, we set it to zero when we start tuning or go idle
  int idle;
  int bcast_sock;               ///<socket used for sending announcements
  time_t last_announce;

  uint32_t pos;
  uint32_t freq;
  uint8_t pol;
} DvbSap;
/**
 *\brief initialize session announcement server
 *
 *receives events from dvb module, gets service name from pgmdb module, gets scheduling information from epgdb
 */
int sapInit (DvbSap * s, RcFile * cfg, dvb_s * d, PgmDb * pgmdb,
             EpgDb * epgdb);
void sapClear (DvbSap * s);
int CUsapInit (DvbSap * sap, RcFile * cfg, dvb_s * d, PgmDb * pgmdb,
               EpgDb * epgdb, CUStack * s);
int sapStart (DvbSap * s);
int sapStop (DvbSap * s);
int sapIdle (DvbSap * s);

#endif // __sap_h
