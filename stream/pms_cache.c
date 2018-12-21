#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "utl.h"
#include "pms_cache.h"
#include "sec_common.h"
#include "tp_info.h"
#include "debug.h"
#include "dmalloc.h"
int DF_PMS_CACHE;
#define FILE_DBG DF_PMS_CACHE
static int
pms_cache_hash_real (uint8_t sec_nr, uint16_t pnr, uint32_t pos,
                     uint32_t freq, uint8_t pol)
{
  return ((pnr + sec_nr * 3 + freq * 5 + pol * 7 +
           pos * 11) % SEC_CACHE_SIZE);
}

static int
pms_cache_hash (uint8_t * sec, uint32_t pos, uint32_t freq, uint8_t pol)
{
  return pms_cache_hash_real (get_sec_nr (sec), get_sec_pnr (sec), pos, freq,
                              pol);
}

void
put_cache_sec (uint8_t * section, uint32_t pos, uint32_t freq, uint8_t pol,
               PMSCache * c)
{
  uint32_t size;
  int h;
  if (!sec_is_current (section))
    return NULL;
  size = get_sec_size (section);
  h = pms_cache_hash (section, pos, freq, pol);

  pthread_mutex_lock (&c->mx);
  c->pos[h] = pos;
  c->freq[h] = freq;
  c->pol[h] = pol;
  memmove (c->data[h], section, size);
  pthread_mutex_unlock (&c->mx);

}

int
get_cache_sec (uint8_t sec_nr, uint16_t pnr, uint32_t pos, uint32_t freq,
               uint8_t pol, PMSCache * c, uint8_t sec[4096])
{
  int h;
  uint8_t *p;
  pthread_mutex_lock (&c->mx);
  h = pms_cache_hash_real (sec_nr, pnr, pos, freq, pol);
  if (((p = c->data[h])) &&
      (get_sec_nr (p) == sec_nr) &&
      (get_sec_pnr (p) == pnr) &&
      (c->pos[h] == pos) && (c->freq[h] == freq) && (c->pol[h] == pol))
  {
    memmove (sec, p, 4096);
    pthread_mutex_unlock (&c->mx);
    return 0;
  }
  pthread_mutex_unlock (&c->mx);
  return 1;
}

void
pms_cache_init (PMSCache * c)
{
  int i;
  memset (c, 0, sizeof (PMSCache));
  pthread_mutex_init (&c->mx, NULL);
  for (i = 0; i < SEC_CACHE_SIZE; i++)
    c->data[i] = utlCalloc (4096, 1);

}

void
pms_cache_clear (PMSCache * c)
{
  int i;
  for (i = 0; i < SEC_CACHE_SIZE; i++)
  {
    if (c->data[i])
    {
      debugMsg ("freeing bucket: sec_nr %hhu, pnr %hu, freq %" PRIu32
                ", pol %s\n", get_sec_nr (c->data[i]),
                get_sec_pnr (c->data[i]), c->freq[i],
                tpiGetPolStr (c->pol[i]));
      utlFAN (c->data[i]);
    }
  }
  pthread_mutex_destroy (&c->mx);
}
