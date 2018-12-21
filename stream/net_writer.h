#ifndef __net_writer_h
#define __net_writer_h
#include <pthread.h>
#include "bool2.h"
#include <netinet/ip.h>
#include "btre.h"
#include "dvb.h"
#include "custck.h"
#include "rcfile.h"
#include "task.h"
#include "dvb.h"
#include "svt.h"
#include "pgmdb.h"
/**
 *\file net_writer.h
 *\brief broadcasts data on the network
 *
 *How we _want_ it to behave:<br>
 *listen on a distributor for packets and send them to network<br>
 *other things use the same distributor to eg record to disk<br>
 *requires transport stream remultiplexing capabilities<br>
 *requires ts reader<p>
 *
 *How we do it now:<br>
 *set up pid filters (here)<br>
 *read ts directly from ts_tap and stream it<br>
 *
 *Latest update:<br>
 *ts_reading now done inside dvb.c<br>
 *distributor now called selector.<br>
 *dvb_s holds the one used to output ts packets.<br>
 */

///holds the writer's state
typedef struct
{
  Task t;
  int tuned;
//      pthread_t thread;///<the worker thread
//      BTreeNode * filter_fds;///<holds the active filter fds sorted by pid
  int stream_eit;               ///<1 to have eit tables in broadcast, 0 otherwise.
  int bcast_sock;               ///<the network socket
  uint16_t *active_pnrs;
  uint16_t num_pnrs;
  struct sockaddr_storage dest_addr;
//      pthread_mutex_t in_mx;///<for synchronisation
  dvb_s *dvb;
  SvcOut *o;
  DListE *input;
  PgmDb *pgmdb;
  uint16_t sequence;

//      uint8_t * smi_ts;
//      int num_smi_ts;
  uint8_t smi_ts_buf[TS_LEN];
  int smi_intvl;
  bool smi_pk;

  uint8_t *sit_ts;
  int num_sit_ts;
  int sit_ver;
  int sit_cc;
  int sit_ctr;

  uint8_t *dit_ts;
  int num_dit_ts;
  bool ts_changed;
  int dit_cc;
  int dit_ctr;

  Task est;                     ///<rate estimator
  uint32_t pk_ctr;              ///<counts packets for estimating data rate and inter_packet time
  uint32_t delta_t;             ///<estimated duration per ts packet in 90 khz clock units
  uint64_t t_ctr;               ///<counts 90 kHz timestamp upwards
} NetWriter;

int netWriterAddPnr (NetWriter * w, uint16_t pnr);
int netWriterRmvPnr (NetWriter * w, uint16_t pnr);
int netWriterRmvAll (NetWriter * w);
int netWriterInit (NetWriter * w, RcFile * cfg, dvb_s * dvb, PgmDb * db);
int netWriterIdle (NetWriter * w);
#define netWriterLock(_NWP)	taskLock(&(_NWP)->t);
#define netWriterUnlock(_NWP)	taskUnlock(&(_NWP)->t);

/*
int netWriterAddFilter(NetWriter * w, uint16_t pid);///<adds a PES filter fd with specified pid
int netWriterRemoveFilter(NetWriter * w, uint16_t pid);///<removes filter with specified pid
int netWriterRemoveFilters(NetWriter * w);///<removes all filter
*/
int netWriterStart (NetWriter * w);     ///<starts writer
int netWriterStop (NetWriter * w);      ///<stops writer
/*
int netWriterSetSit(NetWriter * w,uint8_t *sit_ts,int num_sit_ts);
int netWriterClearSit(NetWriter * w);
*/

//int netWriterInit(NetWriter * w, RcFile * cfg, dvb_s * dvb);
int CUnetWriterInit (NetWriter * w, RcFile * cfg, dvb_s * dvb, PgmDb * db,
                     CUStack * s);
void netWriterClear (NetWriter * w);
void CUnetWriterClear (void *v);

#endif
