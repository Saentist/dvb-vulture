#ifndef __eis_h
#define __eis_h
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include "sec_common.h"
#include "iterator.h"
#include "fp_mm.h"
#include "epg_evt.h"

/**
 *\file eis.h
 *\brief event information section services
 *
 *epgdb and eit_rec use iterator
 */
///one event description section
typedef struct EIS_s EIS;
struct EIS_s
{
  SecCommon c;
  uint16_t svc_id;              ///<same as pmt program number
  uint8_t segment_last_section_number;  ///<what's this good for?
  uint8_t last_table_id;        ///<probably useful for schedule table
  uint8_t type;                 ///<0x4e:present/following, actual TS, 0x4f:present/following, other TS, 0x50-0x5f:schedule, actual TS, 0x60-0x6f:schedule, other TS,
  uint16_t tsid;
  uint16_t onid;
  uint16_t num_events;
  EVT *events;
};

int parse_eis (uint8_t * eis, EIS * p);
void clear_eis (EIS * p);
int get_eis_tsid (uint8_t * sec);
int iterEisSecInit (Iterator * iter, uint8_t * sec);    ///<inits I_SEQ_FWD iterator

#endif
