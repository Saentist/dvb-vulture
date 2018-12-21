//#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "utl.h"
#include "debug.h"
#include "timestamp.h"
#include "dmalloc_loc.h"
int DF_TS;
#define FILE_DBG DF_TS

//static pthread_mutex_t tzmutex = PTHREAD_MUTEX_INITIALIZER;

#define MJD_EPOCH 50000         ///<Evil Hack to avoid overflow

/**
 *\param y:Year from 1900 (e.g. for 2003, Y = 103)
 *\param d:Day of month from 1 to 31
 *\param m:Month from January (= 1) to December (= 12)
 */
uint32_t
get_mjd (int d, int m, int y)
{
  int l;
  l = 0;
  if ((m == 1) || (m == 2))
  {
    debugMsg ("l=1\n");
    l = 1;
  }
  return 14956 + d + ((uint32_t) ((y - l) * 365.25)) +
    ((uint32_t) ((m + 1 + l * 12) * 30.6001));
}

/**
 *\brief get a timestamp truncated to the past hour
 */
time_t
epg_get_buf_time (void)
{
  time_t a;
  long d, m, y;
  uint32_t mjd;
  struct tm result;
//  pthread_mutex_lock (&tzmutex);
  a = time (NULL);
#ifdef __WATCOMC__
  _gmtime (&a, &result);
#else
  gmtime_r (&a, &result);
#endif
  result.tm_min = 0;
  result.tm_sec = 0;
  /*
     b) To find MJD from Y, M, D
     If M = 1 or M = 2, then L = 1; else L = 0
     MJD = 14 956 + D + int [ (Y - L) × 365,25] + int [ (M + 1 + L × 12) × 30,6001 ]
   */
  d = result.tm_mday;
  m = result.tm_mon + 1;
  y = result.tm_year;
  mjd = get_mjd (d, m, y);
  debugMsg ("BUF_MJD: %" PRIu32 "\n", mjd);
//  pthread_mutex_unlock (&tzmutex);
  return (mjd - MJD_EPOCH) * 86400 + get_time_hms (result.tm_hour, 0, 0);
//      b=mktime(&result);
//      return b;
}

time_t
get_time_hms (uint8_t h, uint8_t m, uint8_t s)
{
  return s + m * 60 + h * 3600;
}

//this should probably use mjd_to_ymd
time_t
get_time_mjd (uint32_t mjd)
{
  if (mjd > 0)                  //hmmmmm...
  {
    debugMsg ("SEC_MJD: %" PRIu32 "\n", mjd);
    return (mjd - MJD_EPOCH) * 86400;
  }                             /*
                                   time_t ret;
                                   long   y,m,d ,k;
                                   struct tm btime;
                                   // algo: ETSI EN 300 468 - ANNEX C

                                   y =  (long) ((mjd  - 15078.2) / 365.25);
                                   m =  (long) ((mjd - 14956.1 - (long)(y * 365.25) ) / 30.6001);
                                   d =  (long) (mjd - 14956 - (long)(y * 365.25) - (long)(m * 30.6001));
                                   k =  (m == 14 || m == 15) ? 1 : 0;
                                   y = y + k + 1900;
                                   m = m - 1 - k*12;

                                   btime.tm_sec=0;
                                   btime.tm_min=0;
                                   btime.tm_hour=0;
                                   btime.tm_mday=d;
                                   btime.tm_mon=m-1;
                                   btime.tm_year=y-1900;
                                   debugMsg("ymd: %u, %u, %u\n",y,m,d);
                                   pthread_mutex_lock(&tzmutex);

                                   //ret = mktime(&btime); this takes definitely too long
                                   pthread_mutex_unlock(&tzmutex);
                                   debugMsg("ret: %u\n",ret);
                                   return ret;
                                   } */
  errMsg ("invalid MJD\n");
  return 0;
}

int
ts_to_mjd (uint32_t ts)
{
  return (ts / 86400) + MJD_EPOCH;

}

/**
 *\param y_p: returns Year from 1900 (e.g. for 2003, Y = 103)
 *\param d_p: returns Day of month from 1 to 31
 *\param m_p: returns Month from January (= 1) to December (= 12)
 */
int
mjd_to_ymd (int mjd, int *y_p, int *m_p, int *d_p)
{
  long y, m, d, k;
  if (mjd > 0)
  {
    y = (long) ((mjd - 15078.2) / 365.25);
    m = (long) ((mjd - 14956.1 - (long) (y * 365.25)) / 30.6001);
    d = (long) (mjd - 14956 - (long) (y * 365.25) - (long) (m * 30.6001));
    k = (m == 14 || m == 15) ? 1 : 0;
    y = y + k + 1900;
    m = m - 1 - k * 12;

    *y_p = y;
    *m_p = m;
    *d_p = d;

    return 0;
  }
  return 1;
}

uint32_t
get_hour (time_t base, time_t t)
{
  return (t - base) / 3600;
}

uint32_t
get_min (time_t base, time_t t)
{
  return (t - base) / 60;
}
