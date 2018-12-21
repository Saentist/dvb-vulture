#ifndef __pgmdb_h
#define __pgmdb_h
#include <pthread.h>
#include "fp_mm.h"
#include "custck.h"
#include "rcfile.h"
//#include "buffer.h"
#include "transponder.h"
#include "tp_info.h"
#include "pgm_info.h"
#include "dvb.h"
#include "pms_cache.h"
#include "task.h"
#include "svt.h"
/**
 *\file pgmdb.h
 *\brief maintains information on available programs
 *
 *may load data from disk on startup.
 *will save collected data to disk on exit.
 *keeps a list of transponders.
 *transponders may be added manually.
 *transponders are read from initial tuning database on init.
 *will collect section data when tuned.
 *data is associated with tuned transponder.
 *may auto-add transponders when it sees NIT.
 *except if there is already a transponder with same parameters in database.
 *tables age dynamically as long as we are tuned
 *if table exceeds timeout, it is removed.
 *timeouts are reset if new section of same version shows up
 *if new version is available, current sections are replaced with stored next sections and previous versions are discarded.
 *there are calls to lock database and prevent changes.
 *may be necessary for querying parameters.
 *
 *Hmm the data access interface...
 *find program by name.. may not be unique
 *
 *I _think_ a program is uniquely identified by the transponder(delivery sys) it's on and it's program number
 *So we provide a func to get program info from those parameters(freq,pol,pnr)
 *
 *also provide functions for listing all transponders, all pgms on transponder<br>
 *manually remove transponders<p>
 *
 *we interpret the NIS when we get one, as we generate our transponders that way.<br>
 *the other sections may be stored raw in some kind of cache to be parsed when they're needed.<br>
 *when we need a table, we look in the cache, and if it's not there we setup filter and wait for it.<br>
 *if we reach a timeout we fail. Else we parse it. The cache should be able to discard old sections.<br>
 *sections are old if they are currently being filtered and exceed a timeout.<br>
 *without being refreshed. pmts may get stuck as they're only filtered when their program is tuned. <br>
 *So we have to remove those by checking the pat when it gets changed, to see what pmt's are outdated.
 *may not work for corrupted pnrs. so instead we check every hour all pmts against the pat and discard
 *invalid pmts. 
 *
 *DONE: made sure only complete tables are stored in transponders. maintain tbl and tbl_next in PgmDb structure and copy only when complete. 
 *Reinitialize (clear,free) on transponder change. Leave tbl[] in transponder and put only complete tables there.
 *
 *Update:SDT and PAT are persistent and stored on exit. when tuned they are updated in background
 *PMTs will be fetched when needed and is cached but not stored. fetching will only work when tuned. May fail otherwise.
 */

/**
 *\brief hold all transponders for a satellite position
 */
typedef struct
{
//  BTreeNode n;///<node in pgmdb->root tree no.. cannot do this, btre.c makes allocations... assumes things...
  uint32_t pos;                 ///<dvb module position index
  BTreeNode *root;              ///<tree of transponders
} PgmdbPos;

//typedef struct _dvb_s dvb_s;
/**
 *\brief the program database
 *
 *Interprets and stores relevant program stream information in the background
 */
typedef struct PgmDb
{
  Task t;
  CUStack destroy;
  int32_t pmt_pid;              ///<which pmt we are currently listening to. may be -1 if pmt filter not set up. gets set to pmt pid when requesting a program on transponder. gets set to -1 when tuning to different transponder.
  uint32_t pat_timeout_s;       ///<the timeout used for pat acquisition
  uint32_t pms_timeout_s;       ///<the timeout used for pmt section acquisition
//      uint32_t nit_timeout_s;///<the timeout used for nit acquisition
//      uint32_t sdt_timeout_s;///<the timeout used for sdt acquisition
  long poll_timeout_ms;
  char *dbfilename;
  BTreeNode *root;              ///<root of sorted tree of positions
//  BTreeNode *svt;               ///<service trackers, keeping track of running services and pid updates in PMT. sorted by pnr
  int freq_tolerance;           ///<transponders whose frequencies differ by less than this amount are considered equal
  Transponder *tuned;           ///<tuned in transponder
  PgmdbPos *current_pos;        ///<current position
        /**
	 *protects the interface. 
	 *
	 *Some functions release the task lock for waiting on events.
	 *This prevents parallel calls(from other threads) to change pgmdb state
	 *during that period.
	 */
  pthread_mutex_t access_mx;
  pthread_cond_t pat_complete_cond;
  uint8_t pat_complete;
//      pthread_cond_t sdt_complete_cond;
//      uint8_t sdt_complete;

  Table *tbl[2];                ///<currently being assembled tables(PAT and SDT)
  Table *tbl_next[2];           ///<next tables we're collecting

  pthread_cond_t pms_wait_cond;
  uint16_t pms_wait_pnr;
  uint8_t pms_wait_secnr;
  uint8_t pms_wait;
//      uint8_t * pms_sec;///<this is a problem, may be outdated pointing at the wrong section... Naah it seems to get freed... by pms_cache perhaps... why?
  uint8_t pms_sec[4096];        ///<stores found program_map_section here
  PMSCache pms_cache;

  pthread_cond_t tune_wait_cond;        ///<waiting for a tuning event if a pms section cannot be retrieved
  uint32_t tune_wait_pos;
  uint32_t tune_wait_freq;
  uint8_t tune_wait_pol;
  uint8_t tune_wait;

//      pthread_cond_t nit_complete_cond;///<we don't keep the NIT persistently, we just add the transponders reported, and report when we have seen all.
  int8_t nit_ver;               ///<-1 on start, else table version
  uint8_t nit_num_sec;
    BIFI_DECLARE (nit_seen_sec, 256);
//      uint8_t nit_complete;

  dvb_s *dvb;
  SvcOut *o;
  DListE *input;
  SecEx ex;
} PgmDb;

/**
 *\brief set a transponder
 *tell the database thread to update specified transponder
 *will start db update thread
 *use pgmdb_stop() before calling
 */
//int pgmdbSetTransp(PgmDb *db,uint32_t freq,uint8_t pol);now internal with events

/**
 *\brief call this to start fetching packets
 *
 *starts a separate thread to parse sections
 */
int pgmdbStart (PgmDb * db);

/**
 *\brief call this to stop fetching packets
 *
 *joins the section parse thread
 */
int pgmdbStop (PgmDb * db);

void pgmdbClear (PgmDb * db);
int pgmdbInit (PgmDb * db, RcFile * cfg, dvb_s * a);
int CUpgmdbInit (PgmDb * db, RcFile * cfg, dvb_s * a, CUStack * s);

/**
 *\brief list transponders in database
 *
 *transponders may be added by calling add_transp or when program reads Network information table
 *the latter may change dynamically
 *
 *\param num_tp must point to an int. after the call it will hold the number of elements in t
 *\return NULL on error or no tps, array of TransponderInfo on success
 */
TransponderInfo *pgmdbListTransp (PgmDb * db, uint32_t pos, int *num_tp);

/**
 *\brief return info of transponder with freq/pol (there may only be one)
 *
 *\param i points to TransponderInfo struct to fill
 *\param freq frequency
 *\param pol polarization
 *\return 0 on success, 1 if transponder doesn't exist in database
 */
int pgmdbFindTransp (PgmDb * db, TransponderInfo * i, uint32_t pos,
                     int32_t freq, uint8_t pol);

/**
 *\brief get the transport stream id for tp
 *
 *gets the id from pat or sdt(in that order) if available.
 *\param pos satellite pos index
 *\param freq frequency
 *\param pol polarization
 *\return 16 bit positive tsid for transponder or -1 on error
 */
int32_t pgmdbGetTsid (PgmDb * db, uint32_t pos, int32_t freq, uint8_t pol);

/**
 *\brief get the originating network id for transponder..(are those onid/pnrs unique?)
 *
 *gets the id from sdt(in that order) if available.
 *\param pos satellite pos index
 *\param freq frequency
 *\param pol polarization
 *\return 16 bit positive onid for transponder or -1 on error(sdt not present)
 */
int32_t pgmdbGetOnid (PgmDb * db, uint32_t pos, int32_t freq, uint8_t pol);

/**
 *\brief list programs on transponder
 *
 *go through each Program association section in the table
 *filling ProgramInfo struct for each one except pnr==0
 */
ProgramInfo *pgmdbListPgm (PgmDb * db, uint32_t pos, uint32_t freq,
                           uint8_t pol, int *num_ret);

/**
 *\brief find program on transponder by pnr
 *
 */
ProgramInfo *pgmdbFindPgmPnr (PgmDb * db, uint32_t pos, uint32_t freq,
                              uint8_t pol, uint16_t pnr, int *num_res);

/**
 *\brief find program by name
 *
 *return possibly several programs with specified name
 *returns possibly several transponders with found and num_found fields filled in
 */
//TransponderInfo * pgmdbFindPgmNamed(PgmDb * db, uint32_t pos,char * name, uint8_t case_insensitive, int * num_res);

//int pgmdbSetPgm(PgmDb * db,uint16_t pnr); should be set on_demand

/**
 *\brief add a transponder to database
 *
 *may fail if one with specified tuning params already exists
 */
int pgmdbAddTransp (PgmDb * db, uint32_t pos, TransponderInfo * i);

/**
 *\brief tries to remove transponder and all its pgms/services etc...
 *may not work for transponders read from initial tuning table
 *section scanner may also add removed transponders again if their NITs are still transmitted
 *primarily thought for removal of stale entries
 *won't work on currently tuned transponder
 *\return 1 on error
 */
int pgmdbDelTransp (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol);

/**
 *\brief do a transponder scan/service discovery
 *
 *will tune to each known transponder in turn, scanning it's tables for a brief period of time
 *if new transponders are discovered will scan those too until no more show up.
 *
 */
//int pgmdbScanFull(PgmDb *db,void * x,int(*tune)(void * x,TransponderInfo * i));

/**
 *\brief scans currently tuned transponder
 *
 */
int pgmdbScanTp (PgmDb * db);

/**
 *\brief list the pids of program
 *
 *does a deep copy of the elementary stream infos of the pmt, possibly merging multiple sections to form a single array
 *\param pcr_ret is used to return a valid pcr_pid or 0x1fff on success, won't be written on error(NULL return). If sections carry different pcr_pid, the
 *last seen valid one is returned.
 *\return pointer to an array of elementary stream information or NULL on error
 */
ES *pgmdbListStreams (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol,
                      uint16_t pnr, uint16_t * pcr_ret, int *num_res);

/**
 *\brief returns one if tuned to specified parameters
 */
int pgmdbTuned (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol);

/**
 *\brief returns true if the pgmdb has gone idle
 */
int pgmdbUntuned (PgmDb * db);

/**
 *\brief set per-transponder finetune
 *
 *\return 0 on success
 */
int pgmdbFt (PgmDb * db, uint32_t pos, uint32_t freq, uint8_t pol,
             int32_t ft);


#endif
