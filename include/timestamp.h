#ifndef __timestamp_h
#define __timestamp_h
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
time_t epg_get_buf_time (void);
time_t get_time_hms (uint8_t h, uint8_t m, uint8_t s);
time_t get_time_mjd (uint32_t mjd);
uint32_t get_hour (time_t base, time_t t);
uint32_t get_min (time_t base, time_t t);
uint32_t get_mjd (int d, int m, int y);
int ts_to_mjd (uint32_t ts);
int mjd_to_ymd (int mjd, int *y_p, int *m_p, int *d_p);
#endif
