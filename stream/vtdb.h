#ifndef __vtdb_h
#define __vtdb_h
#include <pthread.h>
#include "fp_mm.h"
#include "custck.h"
#include "rcfile.h"
#include "dvb.h"
//#include "buffer.h"
#include "btre.h"
#include "pgmdb.h"
#include "pes_ex.h"
#include "task.h"
#include "lru.h"
#include "vt_extractor.h"
/**
 *\file vtdb.h
 *\brief routines for storing and retrieving teletext pages.
 *
 *
 *teletext data is embedded in pes packets. several teletext streams are possible in one pes stream.
 *the database shall seperate them by their data_identifier.<p>
 *
 *there may be several magazines (up to eight?) per teletext stream.<p>
 *every stream may contain service related data packets.
 *
 *each magazine consists of several pages and possibly magazine-global packets.
 *each page consists of several packages.<p>
 *
 *it shall be possible to acquire pages on several file descriptors at once.<p>
 *
 *database organisation:
 *Level1: satellite position
 *Level2: transport stream Id
 *Level3: pes pid
 *Level4: data_identifier
 *Level5: magazine/svc-global-packet
 *Level6: page/mag-global-packet
 *Level7: page-packet
 *
 *Level3b: pnrs
 *
 *possibly maintain the lowest level directly on disk.
 *possibly save when tuning transponder
 *possibly just store packets instead of page
 *
 *The access units are aligned with the pes packets.
 *
 *Update: magazines are swapped out to disk in LRU order
 *the maximum number of resident magazines allowed is configurable(mag_limit=)
 */

///holds the VtTsids for a sat pos
typedef struct
{
  uint32_t pos;
  BTreeNode *tsids;
} VtPos;

typedef struct
{
  uint16_t id;
  BTreeNode *pids;
  BTreeNode *pnrs;
} VtTsid;

typedef struct
{
  Task t;
  LRU mag_lru;
  int vtdb_days;
  int niceness;
  int mag_limit;                //maximum number of magazines allowed to be resident
  int real_mag_limit;           //maximum number of magazines allowed to be resident. =mag_limit+8*num_pnrs
  char *dbfilename;
  FILE *handle;
  FpHeap h;
  BTreeNode *root;
  VtPos *current_pos;
  VtTsid *current_tsid;
  uint32_t current_pos_idx;
  uint32_t current_freq;
  uint8_t current_pol;
  uint16_t *pnrs;
  uint16_t num_pnrs;
  SvcOut *o;
  DListE *input;
  PesEx pes_ex;
  VtEx vt_ex;
  dvb_s *dvb;                   ///<for requesting input
  PgmDb *pgmdb;                 ///<for looking up all those tasty vt pids
  uint64_t hits;
  uint64_t misses;
  void *vbuf;                   ///<setvbuf file buffer. NULL if unbuffered or default.
} VtDb;

/**
 *\brief get the service-specific packets not associated with magazine or page
 */
VtPk *vtdbGetSvcPk (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
                    uint8_t data_id, uint16_t * num_pk_ret);

/**
 *\brief get the magazine-specific packets not associated with a page
 */
VtPk *vtdbGetMagPk (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
                    uint8_t data_id, uint8_t magno, uint16_t * num_pk_ret);

/**
 *\brief get the specified page
 *
 *returns all packets belonging to page. each packet is EX_BUFSZ bytes long
 *
 *\param num_pk_ret the number of packets inside buffer is returned in the integer to which this pointer points
 *
 */
VtPk *vtdbGetPg (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
                 uint8_t data_id, uint8_t magno, uint8_t pgno, uint16_t subno,
                 uint16_t * num_pk_ret);

/**
 *\brief get a list of tsids carrying stored vt data
 */
uint16_t *vtdbGetTsids (VtDb * db, uint32_t pos, uint16_t * num_tsid_ret);

/**
 *\brief get a list of pnrs carrying vt data on transport stream
 */
uint16_t *vtdbGetPnrs (VtDb * db, uint32_t pos, uint16_t tsid,
                       uint16_t * num_pnr_ret);

/**
 *\brief get a list of pids carrying vt data on pnr
 */
uint16_t *vtdbGetPids (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pnr,
                       uint16_t * num_pid_ret);

/**
 *\brief get a list of services carrying vt data on pid
 */
uint8_t *vtdbGetSvc (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid,
                     uint16_t * num_ret);

/**
 *\brief Manually remove a service from database(not implemented)
 */
int vtdbPurgePnr (VtDb * db, uint32_t pos, uint16_t tsid, uint16_t pid);

void vtdbClear (VtDb * db);
int vtdbInit (VtDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb);
int CUvtdbInit (VtDb * db, RcFile * cfg, dvb_s * dvb, PgmDb * pgmdb,
                CUStack * s);
int vtdbStart (VtDb * db);
int vtdbStop (VtDb * db);
int vtdbIdle (VtDb * db);

#define vtdbLock(_VTP)	taskLock(&(_VTP)->t);
#define vtdbUnlock(_VTP)	taskUnlock(&(_VTP)->t);

#endif
