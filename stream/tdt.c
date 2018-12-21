#include <time.h>
#include "tdt.h"
#include "bits.h"
#include "utl.h"
#include "timestamp.h"
#include "debug.h"
#include "dmalloc.h"
int DF_TDT;
#define FILE_DBG DF_TDT

time_t
tdtParse (uint8_t * raw_tdt)
{
  uint16_t section_length;
  uint8_t syntax;
  uint16_t mjd;
  int y, m, d;
  struct tm tms;
  if (raw_tdt[0] != 0x70)
  {
    return (time_t) - 1;
  }
  syntax = bitsGet (raw_tdt, 1, 0, 1);
  if (syntax)
    return (time_t) - 1;
  section_length = bitsGet (raw_tdt, 1, 4, 12);
  if (section_length != 5)
    return (time_t) - 1;
  mjd = bitsGet (raw_tdt, 3, 0, 16);
  tms.tm_hour = bitsDecodeBcd8 (bitsGet (raw_tdt, 3, 16, 8));
  tms.tm_min = bitsDecodeBcd8 (bitsGet (raw_tdt, 3, 24, 8));
  tms.tm_sec = bitsDecodeBcd8 (bitsGet (raw_tdt, 3, 32, 8));

  debugMsg ("H: %d, M: %d, S: %d\n", tms.tm_hour, tms.tm_min, tms.tm_sec);
  mjd_to_ymd (mjd, &y, &m, &d);
  tms.tm_mday = d;
  tms.tm_mon = m - 1;
  tms.tm_year = y - 1900;
  debugMsg ("Day: %d, Mon: %d, year: %d\n", tms.tm_mday, tms.tm_mon,
            tms.tm_year);

  return timegm (&tms);
}
