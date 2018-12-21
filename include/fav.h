#ifndef __fav_h
#define __fav_h
#include <stdint.h>
#include "dvb_string.h"
#include "btre.h"
typedef struct
{
  uint32_t pos;                 ///<sat position
  uint32_t freq;                ///<transponder frequency
  uint8_t pol;                  ///<polarisation
  uint16_t pnr;                 ///<program number
  uint8_t type;
  uint8_t ca_mode;
  DvbString service_name;
  DvbString provider_name;
  DvbString bouquet_name;
} Favourite;

int favRead (Favourite * dest, FILE * f);       ///<read a Favourite
int favWrite (Favourite * src, FILE * f);       ///<write a Favourite
void favClear (Favourite * fav);

#endif
