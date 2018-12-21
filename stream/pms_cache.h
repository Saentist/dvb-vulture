#ifndef __pms_cache_h
#define __pms_cache_h
#include <pthread.h>
/**
 *\file pms_cache.h
 *\brief stores received sections
 *
we should probably use a generalized section cache.
when we receive a section we store it. Later the application requests a table and we can parse the stored data and/or wait for missing sections to arrive.
We can remove cache entries at random when cache fills up.
function put_sec(char * section) 
will interpret the size field and copy the section
the cache could be indexed by a hash value composed of table_id,section_number,program_number(for pms),
function get_sec(type,section_nr,pnr)
retrieves the section from cache


No. Can't use one for all sections. We need PAT and SDT permanently for program queries.
For the pmt however it is possible to use a single cache in the pgmdb which we don't store on exit.

*/

#define SEC_CACHE_SIZE 64
typedef struct
{
  uint8_t *data[SEC_CACHE_SIZE];        ///<unused entries are NULL
  uint32_t pos[SEC_CACHE_SIZE];
  uint32_t freq[SEC_CACHE_SIZE];
  uint8_t pol[SEC_CACHE_SIZE];
  pthread_mutex_t mx;
} PMSCache;

void put_cache_sec (uint8_t * section, uint32_t pos, uint32_t freq,
                    uint8_t pol, PMSCache * c);
int get_cache_sec (uint8_t sec_nr, uint16_t pnr, uint32_t pos, uint32_t freq,
                   uint8_t pol, PMSCache * c, uint8_t sec[4096]);
void pms_cache_init (PMSCache * c);
void pms_cache_clear (PMSCache * c);

#endif
