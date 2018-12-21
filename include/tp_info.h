#ifndef __tp_info_h
#define __tp_info_h
#include <stdint.h>
#include "pgm_info.h"
#include "dvb_string.h"
/**
 *\brief contains a subset of transponder information
 *
 *some fields are only valid for dvb-t frontends
 *some fields are only valid for dvb-s frontends..
 */
typedef struct
{
  uint32_t frequency;           ///<true frequency in 10 kHz units for dvb-s, 10 Hz units for dvb-t

  uint8_t pol;                  ///<signal polarization 0=H 1=V 2=L 3=R (dvb-s)
  int32_t ftune;                ///<fine tune signed offset to real frequency value same units as frequency 
  uint32_t srate;               ///<symbol rate in units of 100 Symbols/sec 
  uint8_t fec;                  ///<forward error corr (dvb-s)
  uint16_t orbi_pos;            ///<orbital position (dvb-s)
  uint8_t east;                 ///<east flag (dvb-s)
  uint8_t roff;                 ///<roll off (dvb-s)
  uint8_t msys;                 ///<modulation system (dvb-s)
  uint8_t mtype;                ///<modulation type (dvb-s)

  ///added for dvb-t
  uint8_t bw;
  ///added for dvb-t
  uint8_t pri;
  ///added for dvb-t
  uint8_t mp_fec;
  ///added for dvb-t
  uint8_t constellation;
  ///added for dvb-t
  uint8_t hier;
  ///added for dvb-t
  uint8_t code_rate_h;
  ///added for dvb-t
  uint8_t code_rate_l;
  ///added for dvb-t
  uint8_t guard;
  ///added for dvb-t
  uint8_t mode;
  ///inversion setting
  uint8_t inv;

  uint8_t deletable;            ///<1 if this tp was generated from nit entry or add_transp() and may be removed using del_transp. 0 for tp from initial tuning file.

  ///used by pgmdb_find_pgm_named() should be zero otherwise
  uint16_t num_found;
  ///used by pgmdb_find_pgm_named() should be NULL otherwise
  ProgramInfo *found;

} TransponderInfo;


///frees the found array after clearing every Tpinfo there
void tpiClear (TransponderInfo * t);
///number of possible polarisations (H,V,L,R)
#define NUM_POL 4
///return a string describing polarisation value
char const *tpiGetPolStr (uint8_t pol);
///return a string describing modulation system
char const *tpiGetMsysStr (uint8_t msys);
///return a string describing modulation type
char const *tpiGetMtypeStr (uint8_t mtype);
///return a string describing forward error correction
char const *tpiGetFecStr (uint8_t fec);
///return a string describing east flag
char const *tpiGetWeStr (uint8_t fec);
///write transponder to socket
void tpiSnd (int sock, TransponderInfo * tp);
///read transponder from socket
void tpiRcv (int sock, TransponderInfo * tp);

#endif
