#ifndef __pms_h
#define __pms_h
#include <stdint.h>
#include "sec_common.h"
#include "fp_mm.h"

/**
 *\file pms.h
 *\brief Programme Map Section parser
 */

///Videotext information
typedef struct
{
  char lang[4];
/*
	Teletext_type                       Description
    0x00      reserved for future use
    0x01      initial Teletext page
    0x02      Teletext subtitle page
    0x03      additional information page
    0x04      programme schedule page
    0x05      Teletext subtitle page for hearing impaired people
    0x06 to 0x1F  reserved for future use
*/
  uint8_t txt_type;
  uint8_t magazine;
  uint8_t page;
} VTI;

///Subtitling information
typedef struct
{
  uint32_t lang[4];
  uint8_t type;
  uint16_t composition;
  uint16_t ancillary;
} SUBI;

///Elementary stream information
typedef struct
{
  uint8_t stream_type;
  uint16_t pid;                 ///<elementary pid. pid of the program element.
  int16_t cmp_tag;              ///< Used to associate the sds component descriptor with the stream. may be -1 for none.
  uint16_t num_txti;            ///<may be zero
  uint16_t num_subi;            ///<may be zero
  VTI *txt;                     ///<informations regarding videotext presence
  SUBI *sub;                    ///<informations regarding subtitling
} ES;

/**
 *\brief one program map section
 *
 *is it possible for every section to have a different program number?!
 *If so we have to do this completely different.
 *perhaps put backpointers here to parent Table
 *and store sections in tree instead of tables. *brrr*
 *
 */
typedef struct PMS_s PMS;
struct PMS_s
{
  SecCommon c;
  uint16_t program_number;      ///<true 16 bit quantity. don't interpret as signed
  uint16_t pcr_pid;
  uint16_t num_es;
  ES *stream_infos;
};

int parse_pms (uint8_t * pat, PMS * p);
void clear_pms (PMS * p);
int write_pms (PMS * p, FILE * f);
int read_pms (PMS * p, FILE * f);
///does a deep copy of the stream information also replicating sub and vt information
int copy_es_info (ES * dest, ES * src);
void clear_es_info (ES * si);
void clear_es_infos (ES * si, uint16_t num_es);

#endif
