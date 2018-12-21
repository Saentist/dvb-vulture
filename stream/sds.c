#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "in_out.h"
#include "utl.h"
#include "sds.h"
#include "debug.h"
#include "size_stack.h"
#include "bits.h"
#include "dmalloc.h"
int DF_SDS;
#define FILE_DBG DF_SDS
//leaks memory at lines 100 and 124 (especially if we throw nonsense at it

static int
get_sv_info (SizeStack * s, SVI * svi)
{
  uint8_t *ptr;
  uint16_t desc_len;
  uint8_t spnlen, snlen;


  if (sizeStackAdvance (s, 0, 5))
  {
    debugMsg ("Fail\n");
    return 1;
  }
  ptr = sizeStackPtr (s);
  svi->svc_id = bitsGet (ptr, 0, 0, 16);
  svi->schedule = bitsGet (ptr, 2, 6, 1);
  svi->present_follow = bitsGet (ptr, 2, 7, 1);
  svi->status = bitsGet (ptr, 3, 0, 3);
  svi->ca_mode = bitsGet (ptr, 3, 3, 1);
  debugMsg
    ("service info id:%" PRIu16 ", schedule:%" PRIu8 ", present_follow:%"
     PRIu8 ", status:%" PRIu8 "\n", svi->svc_id, svi->schedule,
     svi->present_follow, svi->status);
  desc_len = bitsGet (ptr, 3, 4, 12);
  if (sizeStackAdvance (s, 5, 0))
  {
    debugMsg ("Fail\n");
    return 1;
  }
  if (sizeStackPush (s, desc_len))
  {
    debugMsg ("Fail\n");
    return 1;
  }
  while (desc_len > 0)
  {
    uint8_t d_tag, d_len;
    if (sizeStackAdvance (s, 0, 2))
      return 1;
    ptr = sizeStackPtr (s);
    d_tag = ptr[0];
    d_len = ptr[1];
    if (sizeStackAdvance (s, 2, 0))
    {
      debugMsg ("Fail\n");
      return 1;
    }
    if (sizeStackPush (s, d_len))
    {
      debugMsg ("Fail\n");
      return 1;
    }
    ptr = sizeStackPtr (s);
    switch (d_tag)
    {
    case 0x47:                 //bouquet name descriptor
      if (!svi->bouquet_name.buf)
      {
        if (d_len)
        {
          svi->bouquet_name.buf = utlCalloc (1, d_len);
          if (NULL == svi->bouquet_name.buf)
          {
            svi->bouquet_name.len = 0;
            return 1;
          }
          svi->bouquet_name.len = d_len;
          memmove (svi->bouquet_name.buf, ptr, d_len);
        }
      }
      else
        debugMsg ("ignoring:\n");

//                      debugMsg("Bouquet name: %s\n",svi->bouquet_name);
      break;
    case 0x48:                 //service descriptor
      svi->svc_type = ptr[0];
      spnlen = ptr[1];


      if (sizeStackAdvance (s, 0, 2 + spnlen))
      {
        debugMsg ("Fail\n");
        return 1;
      }
      ptr = sizeStackPtr (s);

      if (!svi->provider_name.buf)
      {
        if (spnlen)
        {
          svi->provider_name.buf = utlCalloc (1, spnlen);
          if (NULL == svi->provider_name.buf)
          {
            svi->provider_name.len = 0;
            return 1;
          }
          svi->provider_name.len = spnlen;
          memmove (svi->provider_name.buf, ptr + 2, spnlen);
        }
      }
      else
        debugMsg ("ignoring:\n");
//                      debugMsg("Provider name: %s\n",svi->provider_name);

      snlen = ptr[2 + spnlen];

      if (sizeStackAdvance (s, 0, 3 + spnlen + snlen))  //perhaps I should do this differently
      {
        debugMsg ("Fail\n");
        return 1;
      }

      if ((!svi->service_name.buf) && snlen)
      {
        svi->service_name.buf = utlCalloc (1, snlen);
        if (NULL == svi->service_name.buf)
        {
          svi->service_name.len = 0;
          return 1;
        }
        svi->service_name.len = snlen;
        memmove (svi->service_name.buf, ptr + 3 + spnlen, snlen);
      }
      else
        debugMsg ("ignoring:\n");
//                      debugMsg("Service name: %s\n",svi->service_name);

      break;
    default:
      debugMsg ("Service info ignoring descriptor: %" PRIu8 "\n", d_tag);
      break;
    }
    if (sizeStackPop (s))
    {
      debugMsg ("Fail\n");
      return 1;
    }
    desc_len -= 2 + d_len;
    if (sizeStackAdvance (s, d_len, 0))
    {
      debugMsg ("Fail\n");
      return 1;
    }
  }
  if (sizeStackPop (s))
  {
    debugMsg ("Fail\n");
    return 1;
  }
  return 0;
}

static void
clear_sv_info (SVI * svi)
{
  dvbStringClear (&svi->provider_name);
  dvbStringClear (&svi->service_name);
  dvbStringClear (&svi->bouquet_name);
}

void
clear_sds (SDS * n)
{
  int i;
  for (i = 0; i < n->num_svi; i++)
  {
    clear_sv_info (&(n->service_infos[i]));
  }
  if (n->num_svi)
    utlFAN (n->service_infos);
  memset (n, 0, sizeof (SDS));
}

int
parse_sds (uint8_t * pat, SDS * n)
{
  uint16_t section_length;
  uint16_t tsid;
  uint16_t onid;
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

  if (sizeStackAdvance (&s, 0, 8))
    return 1;

  ptr = sizeStackPtr (&s);

  tsid = bitsGet (ptr, 0, 0, 16);
  version_number = bitsGet (ptr, 2, 2, 5);
  current_next = bitsGet (ptr, 2, 7, 1);
  section_number = ptr[3];
  last_section_number = ptr[4];
  onid = bitsGet (ptr, 5, 0, 16);

  debugMsg
    ("SDS: tsid: %" PRIu16 ", onid: %" PRIu16 ", ver: %" PRIu8 ", cur/nxt: %"
     PRIu8 ", secnr: %" PRIu8 ", lastnr: %" PRIu8 "\n", tsid, onid,
     version_number, current_next, section_number, last_section_number);

  if (sizeStackAdvance (&s, 8, 0))
    return 1;
  ptr = sizeStackPtr (&s);

  section_length -= 8 + 4;      //header+crc
  //count number of svc_info blocks
  n->num_svi = 0;
  i = section_length;
  j = 0;
  k = 0;
  while (i > 0)
  {
    j += 3;
    k = bitsGet (ptr, j, 4, 12) + 2;    //descriptor_loop_length
    j += k;
    i -= 3;
    i -= k;
    n->num_svi++;
  }

  if (!n->num_svi)
  {
    n->service_infos = NULL;
    return 1;
  }
  n->service_infos = utlCalloc (n->num_svi, sizeof (SVI));
  if (!n->service_infos)
    return 1;
  i = 0;
  while (n->num_svi > i)
  {
    if (get_sv_info (&s, &(n->service_infos[i])))
    {
//                      i++;//clear this one too. it should work, as long as utlFAN(NULL) does not f*** up and NULL pointers are zero
      while (i--)
      {
        clear_sv_info (&(n->service_infos[i]));
      }
      utlFAN (n->service_infos);
      return 1;
    }
    i++;
  }
  sizeStackPop (&s);
  if (sizeStackAdvance (&s, 0, 4))
  {
    clear_sds (n);
    return 1;
  }
  ptr = sizeStackPtr (&s);
  crc = bitsGet (ptr, 0, 0, 32);
  debugMsg ("CRC: 0x%" PRIX32 "\n", crc);
  sizeStackPop (&s);
  n->tsid = tsid;
  n->onid = onid;
  n->c.version = version_number;
  n->c.current = current_next;
  n->c.section = section_number;
  n->c.last_section = last_section_number;
  return 0;
}

void
write_service_info (SVI * info, FILE * f)
{
  fwrite (info, sizeof (SVI) - 3 * sizeof (DvbString), 1, f);
  dvbStringWrite (f, &info->provider_name);
  dvbStringWrite (f, &info->service_name);
  dvbStringWrite (f, &info->bouquet_name);
}

void
write_service_infos (uint16_t num_svi, SVI * service_infos, FILE * f)
{
  int i;
  for (i = 0; i < num_svi; i++)
  {
    write_service_info (&service_infos[i], f);
  }
}

int
write_sds (SDS * p, FILE * f)
{
  fwrite (p, sizeof (SDS) - sizeof (void *), 1, f);
  write_service_infos (p->num_svi, p->service_infos, f);
  return 0;
}

void
read_service_info (SVI * info, FILE * f)
{
  fread (info, sizeof (SVI) - 3 * sizeof (DvbString), 1, f);
  dvbStringRead (f, &info->provider_name);
  dvbStringRead (f, &info->service_name);
  dvbStringRead (f, &info->bouquet_name);
}

SVI *
read_service_infos (uint16_t num_svi, FILE * f)
{
  SVI *ptr;
  int i;
  if (0 == num_svi)
    return NULL;
  ptr = utlCalloc (num_svi, sizeof (SVI));
  if (0 == ptr)
    return NULL;

  for (i = 0; i < num_svi; i++)
  {
    read_service_info (&ptr[i], f);
  }
  return ptr;
}

int
read_sds (SDS * p, FILE * f)
{
  fread (p, sizeof (SDS) - sizeof (void *), 1, f);
  p->service_infos = read_service_infos (p->num_svi, f);
  return 0;
}
