#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "pas.h"
#include "utl.h"
#include "debug.h"
#include "size_stack.h"
#include "bits.h"
#include "table.h"
#include "dmalloc.h"

int DF_PAS;
#define FILE_DBG DF_PAS

/*
For SI specified within the present document the minimum time interval between the arrival of the last byte of a section
to the first byte of the next transmitted section with the same PID, table_id and table_id_extension and with the same or
different section_number shall be 25 ms. This limit applies for TSs with a total data rate of up to 100 Mbit/s.
*/

int
parse_pas (uint8_t * pat, PAS * p)
{
  uint16_t transport_stream_id;
  uint16_t section_length;
  uint32_t crc;
  uint8_t version_number;
  uint8_t current_next;
  uint8_t section_number;
  uint8_t last_section_number;
  SizeStack s;
  uint8_t *ptr;
  int i;

  debugMsg ("parse_pas start\n");

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

  if (sizeStackAdvance (&s, 0, 5))
    return 1;

  ptr = sizeStackPtr (&s);

  transport_stream_id = bitsGet (ptr, 0, 0, 16);
  version_number = bitsGet (ptr, 2, 2, 5);
  current_next = bitsGet (ptr, 2, 7, 1);
  section_number = ptr[3];
  last_section_number = ptr[4];

  debugMsg ("PAT: tsid: %" PRIu16 ", ver: %" PRIu8 ", cur/nxt: %" PRIu8
            ", secnr: %" PRIu8 ", lastnr: %" PRIu8 "\n", transport_stream_id,
            version_number, current_next, section_number,
            last_section_number);
//      p=pat+8;

  if (sizeStackAdvance (&s, 5, 4))
    return 1;
  ptr = sizeStackPtr (&s);

  section_length -= 5 + 4;
  p->pa_count = section_length / 4;
  if (!p->pa_count)
  {
    p->pas = NULL;
    return 1;
  }
  p->pas = utlCalloc (p->pa_count, sizeof (PA));
  if (!p->pas)
    return 1;
  i = 0;
  while (section_length > 0)
  {
    uint16_t pnr;
    uint16_t pid;
    pnr = bitsGet (ptr, 0, 0, 16);
    pid = bitsGet (ptr + 2, 0, 3, 13);
    if (sizeStackAdvance (&s, 4, 4))
    {
      utlFAN (p->pas);
      return 1;
    }
    ptr = sizeStackPtr (&s);
    section_length -= 4;
    p->pas[i].pnr = pnr;
    p->pas[i].pid = pid;
    i++;
    debugMsg ("PNR: %" PRIu16 ", PID: %" PRIu16 "\n", pnr, pid);
  }
  crc = bitsGet (ptr, 0, 0, 32);
  debugMsg ("CRC: 0x%" PRIX32 "\n", crc);
  p->tsid = transport_stream_id;
  p->c.version = version_number;
  p->c.current = current_next;
  p->c.section = section_number;
  p->c.last_section = last_section_number;
  return 0;
}

void
clear_pas (PAS * p)
{
  utlFAN (p->pas);
  p->pas = NULL;
  p->pa_count = 0;
}

int
write_pas (PAS * p, FILE * f)
{
  fwrite (p, sizeof (PAS) - sizeof (void *), 1, f);
  fwrite (p->pas, sizeof (PA), p->pa_count, f);
  return 0;
}

int
read_pas (PAS * p, FILE * f)
{
  fread (p, sizeof (PAS) - sizeof (void *), 1, f);
  if (!p->pa_count)
    return 1;
  p->pas = utlCalloc (p->pa_count, sizeof (PA));
  if (!p->pas)
  {
    p->pa_count = 0;
    return 1;
  }
  fread (p->pas, sizeof (PA), p->pa_count, f);
  return 0;
}
