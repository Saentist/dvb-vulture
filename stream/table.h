#ifndef __table_h
#define __table_h
#include <stdio.h>
#include <stdint.h>
#include "pas.h"
#include "pms.h"
#include "nis.h"
#include "sds.h"
#include "bifi.h"
#include "btre.h"
#include "iterator.h"
/**
 *\file table.h
 *\brief Tables hold several sections
 */

///A table
struct Table_s
{
  uint8_t table_id;
  int version;                  ///<0..255: table version, -1: no version
  uint16_t pnr;                 ///<only used with pmt for error checking purposes
  uint8_t active;               ///<used for PMTs to indicate it's being demultiplexed. unused for all others. now unused, maybe remove...
    BIFI_DECLARE (occupied, 256);       ///<records which sections[] are initialized
  uint16_t num_sections;        ///<how large the following array is (derived from last_section)
  union
  {
    PAS *pas_sections;
    PMS *pms_sections;
    NIS *nis_sections;
    SDS *sds_sections;
  };
};

typedef struct Table_s Table;

Table *new_pat (uint16_t num_sections);
Table *new_pmt (uint16_t num_sections);
Table *new_nit (uint16_t num_sections);
Table *new_sdt (uint16_t num_sections);


void for_each_pas (Table * pat, void *x, void (*func) (void *x, PAS * sec));
void for_each_pms (Table * pmt, void *x, void (*func) (void *x, PMS * sec));
void for_each_nis (Table * nit, void *x, void (*func) (void *x, NIS * sec));
void for_each_sds (Table * sdt, void *x, void (*func) (void *x, SDS * sec));

///return 1 if all sections are here
int tbl_is_complete (Table * t);

void tbl_write1 (Table * t, FILE * f);
void tbl_write2 (void *x, BTreeNode * this, FILE * f);
int secs_write (Table * t, FILE * f);
Table *tbl_read1 (FILE * f);
BTreeNode *tbl_read2 (void *x, FILE * f);
void secs_read (Table * t, FILE * f);
Table *new_tbl (uint16_t num_sec, uint8_t type);
int resize_tbl (Table * pat, uint16_t num_sec, uint8_t type);
int enter_sec (Table * t, uint16_t sec_idx, void *sec, uint8_t type);
int clear_tbl_sec (Table * t, uint8_t sec_idx, uint8_t type);
int clear_tbl (Table * t);
int check_enter_sec (Table * t, PAS * pas, uint8_t type, void *x,
                     void (*action) (void *x, PAS const *old_sec,
                                     PAS const *new_sec));
int init_pmt (Table * rv, uint16_t num_sections);
int iterTblInit (Iterator * i, Table * tbl);

#endif
