#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "utl.h"
#include "sit_com.h"
#include "sec_common.h"
#include "dmalloc.h"
#include "bits.h"

#define DIT_TID 0x7E
#define mv_tp(d_, s_) memmove(d_,s_,sizeof(*(s_)))

int
ditCompile (bool ts_change, uint8_t * section)
{
  uint16_t slen = 1;
  uint16_t v;
  section[0] = DIT_TID;
  v = htons ((slen) | (7 << 12) | (1 << 15));
  mv_tp (section + 1, &v);
  section[3] = ((! !ts_change) << 7) | 0x7f;
  return slen + 3;
}

#define SIT_TID 0x7F


int
sitCompile (uint16_t * pnrs, int num_pnrs, uint8_t vnr, uint8_t * section)
{
  uint16_t slen = 0;
  uint16_t v;
  uint32_t v2;
  int i;
  section[0] = SIT_TID;
  v = 0xffff;                   //Oh! They should be 'all bits set'! strange.. different standards give different values..
  mv_tp (section + 3, &v);
  vnr = vnr & 31;
  *(section + 5) = (vnr << 1) | (1) | (3 << 6); //version number, current,reserved
  *(section + 6) = 0;           //section
  *(section + 7) = 0;           //last section
  v = htons (0 | 0xf000);
  mv_tp (section + 8, &v);      //8+9 loop length.. empty for now.. should be filled with descriptors..
  slen = 10;
  for (i = 0; i < num_pnrs; i++)
  {
    if ((slen + 4) > 4092)
      return 0;
    v = htons (pnrs[i]);        //8+9 svc id
    mv_tp (section + slen, &v);
    slen += 2;
    v = htons ((4 << 12) | (1 << 15));  //running status+service_loop_len(=0)+rsvd. we just set it running and omit any descriptors.
    mv_tp (section + slen, &v);
    slen += 2;
  }
  slen = slen - 3 + 4;          //minus header, plus crc
  //section length,reserved bits, section syntax indicator
  v = htons ((slen) | (7 << 12) | (1 << 15));
  mv_tp (section + 1, &v);
  v2 = htonl (sec_crc32 ((char *) section, slen + 3 - 4));
  mv_tp (section + slen + 3 - 4, &v2);
  return slen + 3;
}

int
ts_put_ctr (uint8_t * ts, uint8_t ctr)
{
  ts[3] = (ts[3] & (~(15))) | ((ctr & 15));
  return 0;
}

uint8_t *
sec_to_ts (uint8_t * raw_section, uint32_t len, uint16_t pid,
           uint32_t * num_ts_ret)
{

  uint8_t *rv;
  uint8_t *p;
  uint16_t v;
  uint32_t i;
  int j, k;
  uint32_t num_ts, raw_content_sz, payload_ts;

  /*
     available ts payload size in ts TS_LEN-4 bytes for ts header.
     Per section 1 additional pointer field 1 byte.
   */

  raw_content_sz = len + 1;
  payload_ts = (TS_LEN - 4);
  num_ts = (raw_content_sz + (payload_ts - 1)) / payload_ts;
  rv = utlCalloc (num_ts, TS_LEN);
  j = TS_LEN;
  k = -1;
  p = TS_PTR (rv, 0);

  for (i = 0; i < len; i++)
  {
    if (j >= TS_LEN)
    {
      j = 0;
      k++;
      p = TS_PTR (rv, k);
      p[0] = TS_SYNC_BYTE;
      v = htons (((k == 0) ? (1 << 14) : (0)) | (pid)); //(no)error/unit_start/priority/PID
      mv_tp (p + 1, &v);
      p[3] = (1 << 4) | ((k & 15));     //scrambling=00 adaptation=01(payload_only) continuity=k mod 16
      if (k == 0)
      {
        p[4] = 0;               //pointer field
        j = 5;
      }
      else
        j = 4;
    }
    p[j] = raw_section[i];
    j++;
  }
  for (; j < TS_LEN; j++)
    p[j] = 0xff;
  *num_ts_ret = num_ts;
  return rv;
}
