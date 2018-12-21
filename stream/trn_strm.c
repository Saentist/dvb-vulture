#include <arpa/inet.h>
#include <string.h>
#include "trn_strm.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc.h"
#include "bits.h"

int
tsIsStuffing (uint8_t * d)
{
  uint16_t v;
  memmove (&v, d + 1, sizeof (v));
  return (ntohs (v) & 0x1fff) == 0x1fff;
}

uint16_t
tsGetPid (uint8_t * ts)
{
  uint16_t v;
  memmove (&v, ts + 1, sizeof (v));
  return ntohs (v) & ((1 << 13) - 1);
}

uint8_t
tsGetAFC (uint8_t * ts)
{
  return (ts[3] >> 4) & 3;
}

uint8_t
tsGetPUSI (uint8_t * ts)
{
  return (ts[1] >> 6) & 1;
}
