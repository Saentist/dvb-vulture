#ifndef __nis_h
#define __nis_h
#include <stdint.h>
#include "sec_common.h"
#include "dvb_string.h"
#include "fp_mm.h"

/**
 *\file nis.h
 *\brief Network Information Section parser
 */

///Transport stream information
typedef struct
{
  uint16_t tsid;                ///<transport stream id
  uint16_t onid;                ///<originating network id

  uint8_t is_sat;               ///<else we end up with badly initialized transponders
  uint8_t is_terr;              ///<for dvb-t

  //sat del sys desc, valid if is_sat==1
  uint32_t frequency_s;         ///<absolute frequency in 10 kHz units (binary!)
  uint16_t orbi_pos;
  uint8_t east;
  uint8_t pol;
  uint8_t roff;
  uint8_t modsys;
  uint8_t modtype;
  uint32_t srate;
  uint8_t fec;
  //end sat del sys desc

  //terrestrial del sys desc, valid if is_terr==1
  uint32_t frequency_t;         ///<centre frequency in 10 Hz units
/**
signal bandwith
000     8 MHz
001     7 MHz
010     6 MHz
011     5 MHz
100 to 111 Reserved for future use
*/
  uint8_t bw;

  uint8_t pri;                  ///<high priority flag
  uint8_t slice;                //time slicing indicator
  uint8_t mp_fec;               ///<1 if mpe-fec is used
/**
00 QPSK
01 16-QAM
10 64-QAM
11 reserved for future use
*/
  uint8_t constellation;
/**
Hierarchy_information                  value
         000          non-hierarchical, native interleaver
         001           = 1, native interleaver
         010           = 2, native interleaver
         011           = 4, native interleaver
         100          non-hierarchical, in-depth interleaver
         101           = 1, in-depth interleaver
         110           = 2, in-depth interleaver
         111           = 4, in-depth interleaver
*/
  uint8_t hier;
/**
code_rate                Description
   000     1/2
   001     2/3
   010     3/4
   011     5/6
   100     7/8
101 to 111 reserved for future use

for hi pri stream or non hierarchical
*/
  uint8_t code_rate_h;
  uint8_t code_rate_l;          ///<low pri coderate
/**
guard_interval      Guard interval values
     00        1/32
     01        1/16
     10        1/8
     11        1/4
*/
  uint8_t guard;
/*
transmission_mode               Description
        00        2k mode
        01        8k mode
        10        4k mode
        11        reserved for future use
*/
  uint8_t mode;
  uint8_t oth;
  //end terr del sys desc

  DvbString netname;            ///<may be NULL

} TSI;

///One Network Information Section, parsed
typedef struct
{
  SecCommon c;
  uint16_t net_id;
  uint16_t num_tsi;
  TSI *ts_infos;
} NIS;

int parse_nis (uint8_t * pat, NIS * n);
void clear_nis (NIS * n);
int write_nis (NIS * n, FILE * f);
int read_nis (NIS * n, FILE * f);

#endif
