#ifndef __sds_h
#define __sds_h
#include <stdint.h>
#include <stdio.h>
#include "sec_common.h"
#include "dvb_string.h"

/**
 *\file sds.h
 *\brief Service Description Section parser
 */

///service information
typedef struct
{
  uint16_t svc_id;              ///<The service_id is the same as the program_number in the corresponding program_map_section.
  uint8_t svc_type;             ///<service descriptor
  uint8_t schedule;
  uint8_t present_follow;
  uint8_t status;
  uint8_t ca_mode;
  DvbString provider_name;      ///<service descriptor
  DvbString service_name;       ///<service descriptor
  DvbString bouquet_name;       ///<bouquet name descriptor
} SVI;

///one service description section
typedef struct SDS_s SDS;
struct SDS_s
{
  SecCommon c;
  uint16_t tsid;
  uint16_t onid;
  uint16_t num_svi;
  SVI *service_infos;
};

int parse_sds (uint8_t * pat, SDS * n);
void clear_sds (SDS * n);
int write_sds (SDS * p, FILE * f);
int read_sds (SDS * p, FILE * f);

#endif
