#include <string.h>
#include "utl.h"
#include "pes_ex.h"
#include "trn_strm.h"
#include "bits.h"
#include "dmalloc.h"
#include "debug.h"
int DF_PES_EX;
#define FILE_DBG DF_PES_EX

int
pesExInit (PesEx * ex)
{
  memset (ex, 0, sizeof (PesEx));
  return 0;
}

void
pesExClear (PesEx * ex)
{
  debugMsg ("pesExClear\n");
  memset (ex, 0, sizeof (PesEx));
}

static int
pes_ex_get_ranges (int *a, int *b, int *c, void *buf)
{
  int ra, rb, rc;
  uint8_t *data;
  uint8_t afc, pusi;

  data = buf;
  afc = tsGetAFC (data);
  pusi = tsGetPUSI (data);
  switch (afc)
  {
  case 0:
  default:
    return 1;
    break;
  case 1:                      //payload only
    ra = 4;
    break;
  case 2:                      //adapt field only
    return 2;                   //no payload
    break;
  case 3:                      //adapt+payload
    ra = data[4] + 5;
    if (ra >= TS_LEN)
      return 3;
    break;
  }
  rb = TS_LEN;
  rc = -1;
  if (pusi)
  {
    rc = 0;
  }
  *a = ra, *b = rb, *c = rc;
  return 0;
}

static PesBuf *
pes_ex_process_buf (PesEx * ex, void *buf)
{
  int a, b, c;

  if (pes_ex_get_ranges (&a, &b, &c, buf))
  {                             //something went wrong
    return NULL;
  }
  ex->buf.pid = tsGetPid (buf);
  ex->buf.start = (c == 0) ? 1 : 0;
  ex->buf.len = b - a;
  ex->buf.data = buf + a;
  return &ex->buf;
}

PesBuf const *
pesExPut (PesEx * ex, void *tsbuf)
{
  return pes_ex_process_buf (ex, tsbuf);
}
