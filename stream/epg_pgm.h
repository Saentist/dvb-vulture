#ifndef __epg_pgm_h
#define __epg_pgm_h
#include <time.h>
#include <stdint.h>

/**
 *\file epg_pgm.h
 *\brief epgdb programme schedule data, one per service
 */

/**
 *\brief stores sections of an event information table
 *
 *_WTF_ is segment_last_section_number good for
 */
typedef struct
{
  uint8_t tid;                  ///<table id
  uint8_t ver;                  ///<table version
  uint8_t last_sec;
  uint8_t **secs;               ///<if this pointer is NULL, the object is unused.
} EiTbl;

/**
 *\brief per program events array
 *
 *As It turned out to be rather time consuming looking up and sorting events for every section received, 
 *I'll now just store the epg sections indexed by their table id and section numbers, and instead do a 
 *bit more work inside accessor funcs.
 *
 */
typedef struct
{
  uint16_t pnr;                 ///<the program number
  EiTbl sched[16];              ///<stores schedule EITs 0x50-0x5f
} EpgPgm;

#endif
