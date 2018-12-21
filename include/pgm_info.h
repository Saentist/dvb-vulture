#ifndef __pgm_info_h
#define __pgm_info_h
#include <stdint.h>
#include "dvb_string.h"
/**
 *\file pgm_info.h
 *\brief structure definition used in returning program information
 */

typedef struct
{
  //from PAT
  uint16_t pnr;
  uint16_t pid;

  //from SDT (may not be valid, pointers may be null, etc)
  uint8_t svc_type;             ///<service descriptor
  uint8_t schedule;             ///<1 if program carries scheduling information
  uint8_t present_follow;       ///<1 if program carries now/next information
  uint8_t status;               ///<running status
  uint8_t ca_mode;              ///<1:scrambled 0:unscrambled
  DvbString provider_name;      ///<service descriptor
  DvbString service_name;       ///<service descriptor
  DvbString bouquet_name;       ///<bouquet name descriptor
  uint16_t tsid;
  uint16_t onid;

  //from PMT. These two fields are currently unused, as it required tuning under certain circumstances. They will be removed in future versions.
  uint16_t pcr_pid;
  uint16_t num_es;              ///<number of elementary streams. may not be valid (0)
} ProgramInfo;

///return a c string describing service type
char const *programInfoGetSvcTypeStr (uint8_t status);
///return a string describing running status
char const *programInfoGetRunningStatusStr (uint8_t status);
///deallocate(free()) the c strings inside structure
void programInfoClear (ProgramInfo * pg);
void programInfoSnd (int sock, ProgramInfo * pg);
void programInfoRcv (int sock, ProgramInfo * pg);

#endif
