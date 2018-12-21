#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "nis.h"
#include "utl.h"
#include "debug.h"
#include "size_stack.h"
#include "bits.h"
#include "in_out.h"
#include "dmalloc.h"
int DF_NIS;
#define FILE_DBG DF_NIS

static void
clear_ts_info (TSI * tsi)
{
  dvbStringClear (&tsi->netname);
  memset (tsi, 0, sizeof (TSI));
}

void
clear_nis (NIS * n)
{
  int i;
  for (i = 0; i < n->num_tsi; i++)
  {
    clear_ts_info (&(n->ts_infos[i]));
  }
  if (n->num_tsi)
    utlFAN (n->ts_infos);
  memset (n, 0, sizeof (NIS));
}

static uint32_t
decode_bcd (uint32_t bcd)
{
  uint8_t *ptr;
  int i, mul;
  uint32_t rv;
  rv = 0;
  mul = 1;
  ptr = (uint8_t *) & bcd;
  for (i = 0; i < 4; i++)
  {
    rv += (ptr[i] & 0xf) * mul;
    mul *= 10;
    rv += (ptr[i] >> 4) * mul;
    mul *= 10;
  }
  return rv;
}

char *pol_strings[] = { "H", "V", "L", "R" };
char *msys_strings[] = { "DVB-S", "DVB-S2" };
char *mtype_strings[] = { "Auto", "QPSK", "8PSK", "16-QAM" };
char *constellation_strings[] = { "QPSK", "16-QAM", "64-QAM", "Reserved" };

char *interleaver_strings[] = {
  "non-hier native",
  "native1",
  "native2",
  "native4",
  "non-hier in-depth",
  "in-depth1",
  "in-depth2",
  "in-depth4"
};

char *tmode_strings[] = {
  "2k",
  "8k",
  "4k",
  "resvd"
};

char *fec_strings[] = {
  "not defined",
  "1/2 conv. code rate",
  "2/3 conv. code rate",
  "3/4 conv. code rate",
  "5/6 conv. code rate",
  "7/8 conv. code rate",
  "8/9 conv. code rate",
  "3/5 conv. code rate",
  "4/5 conv. code rate",
  "9/10 conv. code rate",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "no conv. coding"
};

static int
tbw_in_mhz (int bits)
{
  if ((bits >= 0) && (bits <= 3))
    return 8 - bits;
  else
    return 0;
}

static int
get_ts_info (SizeStack * s, TSI * tsi)
{
  int32_t tsi_len;
  uint8_t *ptr;
  uint32_t freq_bcd, srate_bcd;
  uint16_t orb_pos;

  if (sizeStackAdvance (s, 0, 6))
    return 1;
  ptr = sizeStackPtr (s);
  tsi->is_sat = 0;
  tsi->tsid = bitsGet (ptr, 0, 0, 16);
  tsi->onid = bitsGet (ptr, 2, 0, 16);
  tsi_len = bitsGet (ptr, 4, 4, 12);

  if (sizeStackAdvance (s, 6, 2))
    return 1;

  while (tsi_len > 0)
  {
    uint8_t d_tag, d_len;
    if (sizeStackAdvance (s, 0, 2))
      return 1;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 0))
      return 1;
    if (sizeStackPush (s, d_len))
      return 1;
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x43:                 //satellite delivery system
      if (sizeStackAdvance (s, 0, 11))
        return 1;
      freq_bcd = bitsGet (ptr, 0, 0, 32);
      tsi->frequency_s = decode_bcd (freq_bcd);
      orb_pos = bitsGet (ptr, 4, 0, 16);
      tsi->orbi_pos = decode_bcd (orb_pos);
      tsi->east = bitsGet (ptr, 6, 0, 1);
      tsi->pol = bitsGet (ptr, 6, 1, 2);
      tsi->roff = bitsGet (ptr, 6, 3, 2);
      tsi->modsys = bitsGet (ptr, 6, 5, 1);
      tsi->modtype = bitsGet (ptr, 6, 6, 2);
      srate_bcd = bitsGet (ptr, 7, 0, 28);
      tsi->srate = decode_bcd (srate_bcd);
      tsi->fec = bitsGet (ptr, 7, 28, 4);
      debugMsg
        ("Satellite System: \n Freq: %" PRIu32 " kHz Pos: %" PRIu16 ".%"
         PRIu16 "%s Pol: %s Msys: %s Mtype: %s Srate: %" PRIu32
         "kS FEC: %s \n", tsi->frequency_s * 10, tsi->orbi_pos / 10,
         tsi->orbi_pos % 10, tsi->east ? "E" : "W", pol_strings[tsi->pol & 3],
         msys_strings[tsi->modsys & 1], mtype_strings[tsi->modtype & 3],
         tsi->srate / 10, fec_strings[tsi->fec & 15]);
      tsi->is_sat = 1;
      break;
    case 0x5A:                 //terr delivery sys
      tsi->frequency_t = bitsGet (ptr, 0, 0, 32);
      tsi->bw = bitsGet (ptr, 4, 0, 3);
      tsi->pri = bitsGet (ptr, 4, 3, 1);
      tsi->slice = bitsGet (ptr, 4, 4, 1);
      tsi->mp_fec = bitsGet (ptr, 4, 5, 1);
      tsi->constellation = bitsGet (ptr, 5, 0, 2);
      tsi->hier = bitsGet (ptr, 5, 2, 3);
      tsi->code_rate_h = bitsGet (ptr, 5, 5, 3);
      tsi->code_rate_l = bitsGet (ptr, 6, 0, 3);
      tsi->guard = bitsGet (ptr, 6, 3, 2);
      tsi->mode = bitsGet (ptr, 6, 5, 2);
      tsi->oth = bitsGet (ptr, 6, 6, 1);
      debugMsg
        ("Terrestrial System: \n Freq: %" PRIu32 " Hz BW: %" PRIu8
         "->%d MHz Pri: %" PRIu8 " Slice: %" PRIu8 " mpe-fec: %" PRIu8
         " constell: %s hier: %s FECH: %s FECL: %s Guard: 1/%d Mode: %s Oth: %"
         PRIu8 "\n", tsi->frequency_t * 10, tsi->bw, tbw_in_mhz (tsi->bw),
         tsi->pri, tsi->slice, tsi->mp_fec,
         constellation_strings[tsi->constellation & 3],
         interleaver_strings[tsi->hier & 7], struGetStr (fec_strings,
                                                         (unsigned)
                                                         tsi->code_rate_h +
                                                         1U),
         struGetStr (fec_strings, (unsigned) tsi->code_rate_l + 1U),
         32 / (1 << tsi->guard), struGetStr (tmode_strings, tsi->mode),
         tsi->oth);

      tsi->is_terr = 1;
      break;
    case 0x40:                 //network name
      if (!tsi->netname.buf)
      {
        if (d_len)
        {
          tsi->netname.buf = utlCalloc (1, d_len);
          if (!tsi->netname.buf)
            return 1;
          tsi->netname.len = d_len;
          memmove (tsi->netname.buf, ptr, d_len);
        }
      }
      else
        debugMsg ("ignoring:\n");

      debugMsg ("Network name: %s\n", tsi->netname);
      break;
    default:
      debugMsg ("ES info ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
      return 1;
    tsi_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
      return 1;
  }
  return 0;
}

int
parse_nis (uint8_t * pat, NIS * n)
{
  uint16_t net_id;
  uint16_t section_length;
  uint16_t ni_len;
  uint16_t tsl_len;
  uint32_t crc;
  uint8_t version_number;
  uint8_t current_next;
  uint8_t section_number;
  uint8_t last_section_number;
  SizeStack s;
  uint8_t *ptr;
  int i, j, k;

  if (secCheckCrc (pat))
    return 1;

  if (sizeStackInit (&s, pat) || sizeStackPush (&s, MAX_SEC_SIZE))      //maximum section size
    return 1;

  if (sizeStackAdvance (&s, 1, 2))
    return 1;
  ptr = sizeStackPtr (&s);
  section_length = bitsGet (ptr, 0, 4, 12);

  if (sizeStackAdvance (&s, 2, 1))
    return 1;

  if (sizeStackPush (&s, section_length))
    return 1;

  if (sizeStackAdvance (&s, 0, 7))
    return 1;

  ptr = sizeStackPtr (&s);

  net_id = bitsGet (ptr, 0, 0, 16);
  version_number = bitsGet (ptr, 2, 2, 5);
  current_next = bitsGet (ptr, 2, 7, 1);
  section_number = ptr[3];
  last_section_number = ptr[4];
  ni_len = bitsGet (ptr, 5, 4, 12);

  debugMsg ("NIS: nid: %" PRIu16 ", ver: %" PRIu8 ", cur/nxt: %" PRIu8
            ", secnr: %" PRIu8 ", lastnr: %" PRIu8 "\n", net_id,
            version_number, current_next, section_number,
            last_section_number);
//      p=pat+8;

  if (sizeStackAdvance (&s, 7 + ni_len, 1))     //skip descriptors
    return 1;
  ptr = sizeStackPtr (&s);
  section_length -= 7 + 4 + ni_len;     //header+crc
  //count number of net_info blocks
  tsl_len = bitsGet (ptr, 0, 4, 12);

  if (sizeStackAdvance (&s, 2, 1))
    return 1;
  if (sizeStackPush (&s, tsl_len))
    return 1;
  ptr = sizeStackPtr (&s);
  n->num_tsi = 0;
  i = tsl_len;
  j = 0;
  k = 0;
  while (i > 0)
  {
    j += 4;
    k = bitsGet (ptr, j, 4, 12) + 2;    //ES_info_length
    j += k;
    i -= 4;
    i -= k;
    n->num_tsi++;
  }

  if (!n->num_tsi)
  {
    n->ts_infos = NULL;
    return 1;
  }
  n->ts_infos = utlCalloc (n->num_tsi, sizeof (TSI));
  if (!n->ts_infos)
    return 1;
  i = 0;
  while (n->num_tsi > i)
  {
    if (get_ts_info (&s, &(n->ts_infos[i])))
    {
      while (i--)
      {
        clear_ts_info (&(n->ts_infos[i]));
      }
      utlFAN (n->ts_infos);
      return 1;
    }
    i++;
  }
  sizeStackPop (&s);
  if (sizeStackAdvance (&s, 0, 4))
    return 1;
  ptr = sizeStackPtr (&s);
  crc = bitsGet (ptr, 0, 0, 32);
  debugMsg ("CRC: 0x%" PRIX32 "\n", crc);
  sizeStackPop (&s);
  n->net_id = net_id;
  n->c.version = version_number;
  n->c.current = current_next;
  n->c.section = section_number;
  n->c.last_section = last_section_number;
  return 0;
}

void
write_ts_info (TSI * info, FILE * f)
{
  fwrite (info, sizeof (TSI) - sizeof (DvbString), 1, f);
  dvbStringWrite (f, &info->netname);
}

void
write_ts_infos (uint16_t num_tsi, TSI * stream_infos, FILE * f)
{
  int i;
  for (i = 0; i < num_tsi; i++)
  {
    write_ts_info (&stream_infos[i], f);
  }
}

int
write_nis (NIS * n, FILE * f)
{
  fwrite (n, sizeof (NIS) - sizeof (void *), 1, f);
  write_ts_infos (n->num_tsi, n->ts_infos, f);
  return 0;
}

void
read_ts_info (TSI * info, FILE * f)
{
  fread (info, sizeof (TSI) - sizeof (DvbString), 1, f);
  dvbStringRead (f, &info->netname);
}

TSI *
read_ts_infos (uint16_t num_tsi, FILE * f)
{
  TSI *rv;
  int i;
  if (!num_tsi)
    return NULL;
  rv = utlCalloc (num_tsi, sizeof (TSI));
  if (!rv)
    return NULL;
  for (i = 0; i < num_tsi; i++)
  {
    read_ts_info (&rv[i], f);
  }
  return rv;
}

int
read_nis (NIS * n, FILE * f)
{
  fread (n, sizeof (NIS) - sizeof (void *), 1, f);
  n->ts_infos = read_ts_infos (n->num_tsi, f);
  return 0;
}
