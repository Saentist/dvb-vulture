#include <stdlib.h>
#include <string.h>
#include "pms.h"
#include "utl.h"
#include "debug.h"
#include "size_stack.h"
#include "bits.h"
#include "dmalloc.h"

int DF_PMS;
#define FILE_DBG DF_PMS

int
copy_es_info (ES * dest, ES * src)
{
  memmove (dest, src, sizeof (ES));
  dest->txt = NULL;
  dest->sub = NULL;

  if (dest->num_txti)
  {
    dest->txt = utlCalloc (dest->num_txti, sizeof (VTI));
    if (!dest->txt)
      return 1;
    memmove (dest->txt, src->txt, sizeof (VTI) * dest->num_txti);
  }
  if (dest->num_subi)
  {
    dest->sub = utlCalloc (dest->num_subi, sizeof (SUBI));
    if (!dest->sub)
      return 1;
    memmove (dest->sub, src->sub, sizeof (SUBI) * dest->num_subi);
  }
  return 0;
}


void
clear_es_infos (ES * si, uint16_t num_es)
{
  int i;
  for (i = 0; i < num_es; i++)
  {
    clear_es_info (&si[i]);
  }
}

void
clear_es_info (ES * si)
{
  if (si->txt)
    utlFAN (si->txt);
  if (si->sub)
    utlFAN (si->sub);
  memset (si, 0, sizeof (ES));
}

static int
get_es_info (SizeStack * s, ES * si)
{
  int32_t esi_len;
  uint8_t *ptr;

  if (sizeStackAdvance (s, 0, 5))
    return 1;
  ptr = sizeStackPtr (s);

  si->stream_type = ptr[0];
  si->pid = bitsGet (ptr, 1, 3, 13);
  esi_len = bitsGet (ptr, 3, 4, 12);

  if (sizeStackAdvance (s, 5, 2))
  {
    debugPri (10, "Error: sizeStackAdvance\n");
    return 1;
  }

  while (esi_len > 0)
  {
    uint8_t d_tag, d_len;
    if (sizeStackAdvance (s, 0, 2))
    {
      debugPri (10, "Error: sizeStackAdvance\n");
      return 1;
    }
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 0))
    {
      debugPri (10, "Error: sizeStackAdvance\n");
      return 1;
    }
    if (sizeStackPush (s, d_len))
    {
      debugPri (10, "Error: sizeStackPush\n");
      return 1;
    }
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x52:                 //stream identifier
      si->cmp_tag = ptr[0];
      debugPri (20, "ES info component tag: %u\n", si->cmp_tag);
      break;
    case 0x46:                 //vbi_teletext
    case 0x56:                 //teletext
      if (!si->txt)
      {
        int l;
        int vtctr;
        uint8_t *ptr2;
        l = d_len / 5;
        if (!l)
        {
          debugPri (11, "Error: empty teletext descriptor(ignoring)\n");
          //      return 1;//are empty descriptors okay? We get them, so we accept them
        }
        else
        {
          si->txt = utlCalloc (l, sizeof (VTI));
          if (!si->txt)
          {
            debugPri (10, "Allocation failure\n");
            return 1;
          }
          si->num_txti = l;
          ptr2 = ptr;
          for (vtctr = 0; vtctr < l; vtctr++)
          {
            memmove (si->txt[vtctr].lang, ptr2, 3);
            si->txt[vtctr].lang[3] = '\0';
            si->txt[vtctr].txt_type = bitsGet (ptr2, 3, 0, 5);
            si->txt[vtctr].magazine = bitsGet (ptr2, 3, 5, 3);
            si->txt[vtctr].page = ptr2[4];
            debugPri (20,
                      "VT lang: %s, type: %hhu, magazine: %hhu, page: %hhu\n",
                      si->txt[vtctr].lang, si->txt[vtctr].txt_type,
                      si->txt[vtctr].magazine, si->txt[vtctr].page);
            ptr2 += 5;
          }
        }
      }
      else
      {
        debugPri (11, "Multiple Vt/VBI_Vt descriptors\n");
      }
      break;
    case 0x59:                 //subtitling
      if (!si->sub)
      {
        int l;
        int subctr;
        uint8_t *ptr2;
        l = d_len / 8;
        if (!l)
        {
          debugPri (11, "Error: empty subtitling descriptor(ignoring)\n");
//                                      return 1;
        }
        else
        {
          si->sub = utlCalloc (l, sizeof (SUBI));
          if (!si->sub)
          {
            debugPri (10, "Allocation failure\n");
            return 1;
          }
          si->num_subi = l;
          ptr2 = ptr;
          for (subctr = 0; subctr < l; subctr++)
          {
            memmove (si->sub[subctr].lang, ptr2, 3);
            si->sub[subctr].lang[3] = '\0';
//                                              si->sub[subctr].lang=bitsGet(ptr2,0,0,24);
            si->sub[subctr].type = ptr2[3];
            si->sub[subctr].composition = bitsGet (ptr2, 4, 0, 16);
            si->sub[subctr].ancillary = bitsGet (ptr2, 6, 0, 16);
            debugPri (20,
                      "SUB lang: %s, type: %hhu, composition: %u, ancillary: %u\n",
                      si->sub[subctr].lang, si->sub[subctr].type,
                      si->sub[subctr].composition, si->sub[subctr].ancillary);
            ptr2 += 8;
          }
        }
      }
      else
      {
        debugPri (11, "Multiple Sub descriptors\n");
      }

      break;
    default:
      debugPri (20, "ES info ignoring descriptor: %u\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
    {
      debugPri (10, "Error: sizeStackPop\n");
      return 1;
    }
    esi_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
    {
      debugPri (10, "Error: sizeStackAdvance\n");
      return 1;
    }
  }
  return 0;
}


int
parse_pms (uint8_t * pat, PMS * p)
{
  uint16_t program_number;
  uint16_t section_length;
  uint16_t pcr_pid;
  uint16_t pi_len;
  uint32_t crc;
  uint8_t version_number;
  uint8_t current_next;
  uint8_t section_number;
  uint8_t last_section_number;
  uint8_t *ptr;
  int i, j, k;
  SizeStack s;

  if (secCheckCrc (pat))
    return 1;

  p->pcr_pid = 0x1fff;          //NULL pid
  //we crc check in pgmdb_proc_sec because of caching
  if (sizeStackInit (&s, pat) || sizeStackPush (&s, MAX_SEC_SIZE))      //maximum section size
  {
    debugPri (10,
              "Error: sizeStackInit(&s,pat)||sizeStackPush(&s,MAX_SEC_SIZE)\n");
    return 1;
  }
  if (sizeStackAdvance (&s, 1, 2))
  {
    debugPri (10, "Error: sizeStackAdvance\n");
    return 1;
  }
  ptr = sizeStackPtr (&s);

  section_length = bitsGet (ptr, 0, 4, 12);
  if (sizeStackAdvance (&s, 2, 1))
  {
    debugPri (10, "Error: sizeStackAdvance\n");
    return 1;
  }
  if (sizeStackPush (&s, section_length))
  {
    debugPri (10, "Error: sizeStackPush\n");
    return 1;
  }

  if (sizeStackAdvance (&s, 0, 9))
  {
    debugPri (10, "Error: sizeStackAdvance\n");
    return 1;
  }
  ptr = sizeStackPtr (&s);
  program_number = bitsGet (ptr, 0, 0, 16);
  version_number = bitsGet (ptr, 2, 2, 5);
  current_next = bitsGet (ptr, 2, 7, 1);
  section_number = ptr[3];
  last_section_number = ptr[4];
  pcr_pid = bitsGet (ptr, 5, 3, 13);
  debugPri (20, "PMT: pnr: %u, ver: %u, cur/nxt: %u, secnr: %u, lastnr: %u\n",
            program_number, version_number, current_next, section_number,
            last_section_number);
  pi_len = bitsGet (ptr, 7, 4, 12);

  if (sizeStackAdvance (&s, 9 + pi_len, 1))     //skip those descriptors for now
  {
    debugPri (10, "Error: sizeStackAdvance\n");
    return 1;
  }
  ptr = sizeStackPtr (&s);


//      ptr=pat+12;

  section_length -= 9 + 4 + pi_len;     //header+crc
  //count number of es_info blocks
  p->num_es = 0;
  i = section_length;
  j = 0;
  k = 0;
  while (i > 0)
  {
    j += 3;
    k = bitsGet (ptr, j, 4, 12) + 2;    //ES_info_length
    j += k;
    i -= 3;
    i -= k;
    p->num_es++;
  }

  if (!p->num_es)
  {
    debugPri (10, "No elementary streams\n");
    p->stream_infos = NULL;
    return 1;
  }
  p->stream_infos = utlCalloc (p->num_es, sizeof (ES));
  if (!p->stream_infos)
    return 1;
  i = 0;
  while (p->num_es > i)
  {
    if (get_es_info (&s, &(p->stream_infos[i])))
    {
      while (i--)
      {
        clear_es_info (&(p->stream_infos[i]));
      }
      utlFAN (p->stream_infos);
      debugPri (10, "get_es_info\n");
      return 1;
    }
    i++;
  }
  if (sizeStackAdvance (&s, 0, 4))
  {
    debugPri (10, "Error: sizeStackAdvance\n");
    while (i--)
    {
      clear_es_info (&(p->stream_infos[i]));
    }
    utlFAN (p->stream_infos);
    return 1;
  }
  ptr = sizeStackPtr (&s);
  crc = bitsGet (ptr, 0, 0, 32);
  debugPri (20, "CRC: 0x%X\n", crc);
  sizeStackPop (&s);
  p->pcr_pid = pcr_pid;
  p->program_number = program_number;
  p->c.version = version_number;
  p->c.current = current_next;
  p->c.section = section_number;
  p->c.last_section = last_section_number;
  return 0;
}

void
clear_pms (PMS * p)
{
  int i;
  for (i = 0; i < p->num_es; i++)
  {
    clear_es_info (&(p->stream_infos[i]));
  }
  utlFAN (p->stream_infos);
  p->stream_infos = NULL;
  p->num_es = 0;
}

void
write_vtis (uint16_t num_txti, VTI * txt, FILE * f)
{
  fwrite (txt, num_txti, sizeof (VTI), f);
}

void
write_subis (uint16_t num_subi, SUBI * sub, FILE * f)
{
  fwrite (sub, num_subi, sizeof (SUBI), f);
}

void
write_es_info (ES * info, FILE * f)
{
  fwrite (info, sizeof (ES) - 2 * sizeof (void *), 1, f);
  write_vtis (info->num_txti, info->txt, f);
  write_subis (info->num_subi, info->sub, f);
}

void
write_es_infos (uint16_t num_es, ES * stream_infos, FILE * f)
{
  int i;
  for (i = 0; i < num_es; i++)
  {
    write_es_info (&stream_infos[i], f);
  }
}

int
write_pms (PMS * p, FILE * f)
{
  fwrite (p, sizeof (PMS) - sizeof (void *), 1, f);
  write_es_infos (p->num_es, p->stream_infos, f);
  return 0;
}

VTI *
read_vtis (uint16_t num_txti, FILE * f)
{
  VTI *rv;
  if (!num_txti)
    return NULL;
  rv = utlCalloc (num_txti, sizeof (VTI));
  if (!rv)
    return NULL;
  fread (rv, num_txti, sizeof (VTI), f);
  return rv;
}

SUBI *
read_subis (uint16_t num_subi, FILE * f)
{
  SUBI *rv;
  if (!num_subi)
    return NULL;
  rv = utlCalloc (num_subi, sizeof (SUBI));
  if (!rv)
    return NULL;
  fread (rv, num_subi, sizeof (SUBI), f);
  return rv;
}

void
read_es_info (ES * info, FILE * f)
{
  fread (info, sizeof (ES) - 2 * sizeof (void *), 1, f);
  info->txt = read_vtis (info->num_txti, f);
  info->sub = read_subis (info->num_subi, f);
}

ES *
read_es_infos (uint16_t num_es, FILE * f)
{
  ES *rv;
  int i;
  if (!num_es)
    return NULL;
  rv = utlCalloc (num_es, sizeof (ES));
  if (!rv)
    return NULL;
  for (i = 0; i < num_es; i++)
  {
    read_es_info (&rv[i], f);
  }

  return rv;
}

int
read_pms (PMS * p, FILE * f)
{
  fread (p, sizeof (PMS) - sizeof (void *), 1, f);
  p->stream_infos = read_es_infos (p->num_es, f);
  return 0;
}
