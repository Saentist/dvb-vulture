#ifndef __sec_common_h
#define __sec_common_h
#include <stdint.h>
#define MAX_SEC_SIZE 4096

/**
 *\file sec_common.h
 *\brief data common to all sections
 */

typedef struct
{
  uint8_t version;
  uint8_t current;
  uint8_t section;
  uint8_t last_section;
} SecCommon;

int get_sec_size (uint8_t * sec);       ///<gets total sec size including header
int get_sec_nr (uint8_t * sec);
int get_sec_pnr (uint8_t * sec);        ///<gets different things depending on table. pnr for PMS, svc id for EIS ...
int sec_is_current (uint8_t * sec);
int get_sec_version (uint8_t * sec);
int get_sec_id (uint8_t * sec); ///<get table id
//table ids
#define SEC_ID_PAS 0x00
#define SEC_ID_PMS 0x02
#define SEC_ID_NIS 0x40
#define SEC_ID_SDS 0x42
#define SEC_ID_AIS 0x74
//table pids(pmt pids are variable)
#define PID_PAT 0x0000
#define PID_NIT 0x0010
#define PID_SDT 0x0011
int get_sec_last (uint8_t * sec);       ///<get last section number
uint32_t sec_crc32 (char *data, int len);
uint32_t secCheckCrc (uint8_t * sec);
#endif
