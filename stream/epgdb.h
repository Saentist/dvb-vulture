#ifndef __epgdb_h
#define __epgdb_h
#include "bool2.h"
#include "fp_mm.h"
#include "custck.h"
//#include "buffer.h"
//#include "btre.h"
#include "pgmdb.h"
#include "eis.h"
#include "dvb.h"
#include "tp_info.h"
#include "rcfile.h"
#include "epg_evt.h"
#include "sec_ex.h"
#include "task.h"
#include "dhash.h"
#include "svt.h"
/**
 *\file epgdb.h
 *\brief collects EIT information
 *
 *I _think_ epg data can be separated using transponder(freq/pol) and program number
 *possible to use network id or transport stream id instead of transponder
 *
 *Btree uses a combined key of pos/stream id.
 *
 *BUG: on TV5Europe I get an event 
 *at 15:53:00(2min) followed by one at 2:53:40(45Min) 
 *both on same day... messes up schedule...
 *perhaps fix the remote to skip events that are back in time?
 *or the server to sort the tables by time, remove duplicates?...
 */
typedef struct
{
  Task t;
  char *dbfilename;
  FILE *handle;
  uint32_t current_pos;
  bool tuned;
  FpHeap h;
  BTreeNode *root;
  int niceness;
  DListE *input;
  SvcOut *o;
  SecEx ex;
  PgmDb *pgmdb;
  dvb_s *dvb;
  void *vbuf;
} EpgDb;


void epgdbClear (EpgDb * db);
int epgdbInit (EpgDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb);
int CUepgdbInit (EpgDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb,
                 CUStack * s);
int epgdbStart (EpgDb * db);
int epgdbStop (EpgDb * db);
int epgdbIdle (EpgDb * db);
/**
 *\brief retrieve a range of epg data
 *
 *\param db the database
 *\param pos the satellite position
 *\param tsid the transport stream id
 *\param pnr_range points to array of pnrs to get data for. may be NULL to get all data for transponder.
 *\param num_pnrs the size of the pnr_range array. may be zero to get all data for transponder.
 *\param start_time unix timestamp specifying the start time of array to retrieve
 *\param end_time the end time of array to retrieve
 *\param num_entries if end_time==0 this specifies the number of events to return
 *\return an EpgArray with the requested data or NULL on error
 */
EpgArray *epgdbGetEvents (EpgDb * db, uint32_t pos, uint16_t tsid,
                          uint16_t * pnr_range, uint32_t num_pnrs,
                          time_t start_time, time_t end_time,
                          uint32_t num_entries);

#endif
