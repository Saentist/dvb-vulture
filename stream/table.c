#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "utl.h"
#include "table.h"
#include "debug.h"
#include "transponder.h"
#include "dmalloc.h"
int DF_TABLE;
#define FILE_DBG DF_TABLE


/**
 *\brief allocate a PAT and the specified number of PASsections
 *
 *
 */
Table *
new_pat (uint16_t num_sections)
{
  Table *rv;
  rv = utlCalloc (1, sizeof (Table));
  rv->pas_sections = utlCalloc (num_sections, sizeof (PAS));
  rv->num_sections = num_sections;
  rv->version = -1;
  rv->table_id = 0;
  return rv;
}

/**
 *\brief initialize pmt
 *
 *
 */
int
init_pmt (Table * rv, uint16_t num_sections)
{
  rv->pas_sections = utlCalloc (num_sections, sizeof (PMS));
  if (!rv->pas_sections)
    return 1;
  BIFI_INIT (rv->occupied);
  rv->num_sections = num_sections;
  rv->version = -1;
  rv->table_id = 2;
  return 0;
}

/**
 *\brief allocate a PMT and the specified number of PMSsections
 *
 *
 */
Table *
new_pmt (uint16_t num_sections)
{
  Table *rv;
  rv = utlCalloc (1, sizeof (Table));
  if (init_pmt (rv, num_sections))
  {
    utlFAN (rv);
    return 0;
  }
  return rv;
}

/**
 *\brief allocate a NIT and the specified number of NISsections
 *
 *
 */
Table *
new_nit (uint16_t num_sections)
{
  Table *rv;
  rv = utlCalloc (1, sizeof (Table));
  rv->pas_sections = utlCalloc (num_sections, sizeof (NIS));
  rv->num_sections = num_sections;
  rv->version = -1;
  rv->table_id = 0x40;
  return rv;
}

/**
 *\brief allocate a SDT and the specified number of SDSsections
 *
 *
 */
Table *
new_sdt (uint16_t num_sections)
{
  Table *rv;
  rv = utlCalloc (1, sizeof (Table));
  rv->pas_sections = utlCalloc (num_sections, sizeof (SDS));
  rv->num_sections = num_sections;
  rv->version = -1;
  rv->table_id = 0x42;
  return rv;
}

int
clear_tbl (Table * t)
{
  int i;
  if (NULL == t)
  {
    debugMsg ("Table is NULL\n");
    return 0;
  }
  for (i = 0; i < t->num_sections; i++)
    if (bifiGet (t->occupied, i))
      clear_tbl_sec (t, i, t->table_id);

  utlFAN (t->pas_sections);
  return 0;
}

int
secs_write (Table * t, FILE * f)
{
  //fp_pointer secs;
//      fp_size s;
  int i;
/*	switch(t->table_id)
	{
		case 0x00:
		s=sizeof(PAS)-sizeof(void*)+sizeof(fp_pointer);
		break;
		case 0x02:
		s=sizeof(PMS)-sizeof(void*)+sizeof(fp_pointer);
		break;
		case 0x40:
		case 0x41:
		s=sizeof(NIS)-sizeof(void*)+sizeof(fp_pointer);
		break;
		case 0x42:
		case 0x46:
		s=sizeof(SDS)-sizeof(void*)+sizeof(fp_pointer);
		break;
		default:
		errMsg("got table id: %u\n .. can't handle",t->table_id);
		return 0;
	}
	secs=fp_utlMalloc(s*t->num_sections,h);
	*/
  for (i = 0; i < t->num_sections; i++)
  {
    if (bifiGet (t->occupied, i))
    {
//                      FP_GO(secs+i*s,h->f);
      switch (t->table_id)
      {
      case 0x00:
        write_pas (&t->pas_sections[i], f);
        break;
      case 0x02:
        write_pms (&t->pms_sections[i], f);
        break;
      case 0x40:
      case 0x41:
        write_nis (&t->nis_sections[i], f);
        break;
      case 0x42:
      case 0x46:
        write_sds (&t->sds_sections[i], f);
        break;
      default:
        break;
      }
    }
  }
  return 0;
}

void
tbl_write2 (void *x NOTUSED, BTreeNode * this, FILE * f)
{
  Table *t = btreeNodePayload (this);
  fwrite (t, sizeof (Table) - sizeof (void *), 1, f);
  secs_write (t, f);
}

#define FID_TBL_VALID 'V'
#define FID_TBL_NULL 'N'
void
tbl_write1 (Table * t, FILE * f)
{
  if (!t)
  {
    fputc (FID_TBL_NULL, f);
    return;
  }

//      p=fp_utlMalloc(sizeof(Table)-sizeof(fp_pointer),h);
//      FP_GO(p,h->f);
  fputc (FID_TBL_VALID, f);
  fwrite (t, sizeof (Table) - sizeof (void *), 1, f);
  secs_write (t, f);
//      return p;
}


void
secs_read (Table * t, FILE * f)
{
  int i, s;
  switch (t->table_id)
  {
  case 0x00:
    s = sizeof (PAS);
    break;
  case 0x02:
    s = sizeof (PMS);
    break;
  case 0x40:
  case 0x41:
    s = sizeof (NIS);
    break;
  case 0x42:
  case 0x46:
    s = sizeof (SDS);
    break;
  default:
    errMsg ("got table id: %u\n .. can't handle", t->table_id);
    return;
  }
  t->pas_sections = utlCalloc (t->num_sections, s);
  for (i = 0; i < t->num_sections; i++)
  {
    if (bifiGet (t->occupied, i))
    {
      switch (t->table_id)
      {
      case 0x00:
        read_pas (&t->pas_sections[i], f);
        break;
      case 0x02:
        read_pms (&t->pms_sections[i], f);
        break;
      case 0x40:
      case 0x41:
        read_nis (&t->nis_sections[i], f);
        break;
      case 0x42:
      case 0x46:
        read_sds (&t->sds_sections[i], f);
        break;
      default:
        break;
      }
    }
  }
}

BTreeNode *
tbl_read2 (void *x NOTUSED, FILE * f)
{
  Table *t;
  BTreeNode *n;
  n = utlCalloc (1, btreeNodeSize (sizeof (Table)));
  t = btreeNodePayload (n);
  fread (t, sizeof (Table) - sizeof (void *), 1, f);
  secs_read (t, f);
  return n;
}

Table *
tbl_read1 (FILE * f)
{
  Table *p;
  int id;
  id = fgetc (f);
  if (id == FID_TBL_NULL)
    return NULL;
  else if (id == FID_TBL_VALID)
  {
    p = utlCalloc (sizeof (Table), 1);
    fread (p, sizeof (Table) - sizeof (void *), 1, f);
    secs_read (p, f);
    return p;
  }
  else
  {
    errMsg ("illegal FID_TBL: %d\n", id);
    return NULL;
  }
}

Table *
new_tbl (uint16_t num_sec, uint8_t type)
{
  switch (type)
  {
  case 0x00:
    return new_pat (num_sec);
  case 0x02:
    return new_pmt (num_sec);
  case 0x40:
  case 0x41:
    return new_nit (num_sec);
  case 0x42:
  case 0x46:
    return new_sdt (num_sec);
  default:
    errMsg ("got table id: %" PRIu8 "\n .. can't handle", type);
    return 0;
  }
}


int
resize_tbl (Table * pat, uint16_t num_sec, uint8_t type)
{
  uint32_t sz;
  void *ptr;
  if (!num_sec)
    return 1;
  switch (type)
  {
  case 0x00:
    sz = sizeof (PAS) * num_sec;
    break;
  case 0x02:
    sz = sizeof (PMS) * num_sec;
    break;
  case 0x40:
  case 0x41:
    sz = sizeof (NIS) * num_sec;
    break;
  case 0x42:
  case 0x46:
    sz = sizeof (SDS) * num_sec;
    break;
  default:
    errMsg ("got table id: %" PRIu8 "\n .. can't handle", type);
    return 1;
    break;
  }
  ptr = utlRealloc (pat->pas_sections, sz);
  if (!ptr)
    return 1;
  pat->pas_sections = ptr;
  pat->num_sections = num_sec;
  return 0;
}

/*
 *\brief insert(copy) a section with specified type into table at specified index
 */
int
enter_sec (Table * t, uint16_t sec_idx, void *sec, uint8_t type)
{
  PAS *pas = sec;
  uint32_t offs;
  uint32_t sz;
  char *s = (char *) t->pas_sections;
  switch (type)
  {
  case 0x00:
    sz = sizeof (PAS);
    break;
  case 0x02:
    sz = sizeof (PMS);
    break;
  case 0x40:
  case 0x41:
    sz = sizeof (NIS);
    break;
  case 0x42:
  case 0x46:
    sz = sizeof (SDS);
    break;
  default:
    errMsg ("got table id: %" PRIu8 "\n .. can't handle", type);
    return 1;
  }
  offs = sz * sec_idx;
  memmove (s + offs, sec, sz);
  bifiSet (t->occupied, pas->c.section);
  return 0;
}


/**
 *\brief delete specified section in table
 */
int
clear_tbl_sec (Table * t, uint8_t sec_idx, uint8_t type)
{
  switch (type)
  {
  case 0x00:
    clear_pas (&t->pas_sections[sec_idx]);
    break;
  case 0x02:
    clear_pms (&t->pms_sections[sec_idx]);
    break;
  case 0x40:
  case 0x41:
    clear_nis (&t->nis_sections[sec_idx]);
    break;
  case 0x42:
  case 0x46:
    clear_sds (&t->sds_sections[sec_idx]);
    break;
  default:
    errMsg ("got table id: %" PRIu8 "\n .. can't handle", type);
    return 1;
  }
  bifiClr (t->occupied, sec_idx);
  return 0;

}


static void *
get_secptr (Table * t, uint8_t sec_idx, uint8_t type)
{
  switch (type)
  {
  case 0x00:
    return &t->pas_sections[sec_idx];
  case 0x02:
    return &t->pms_sections[sec_idx];
    break;
  case 0x40:
  case 0x41:
    return &t->nis_sections[sec_idx];
    break;
  case 0x42:
  case 0x46:
    return &t->sds_sections[sec_idx];
    break;
  default:
    errMsg ("got table id: %" PRIu8 "\n .. can't handle", type);
    return NULL;
  }
}


int
check_enter_sec (Table * t, PAS * pas, uint8_t type, void *x,
                 void (*action) (void *x, PAS const *old_sec,
                                 PAS const *new_sec))
{
  PAS *old = NULL;
  if (bifiGet (t->occupied, pas->c.section))
  {
    old = get_secptr (t, pas->c.section, type);
    if (old->c.version != pas->c.version)
    {
      action (x, (PAS const *) old, (PAS const *) pas);
      clear_tbl_sec (t, pas->c.section, type);
      enter_sec (t, pas->c.section, pas, type);
      if (type == 0x02)
      {
        t->pnr = ((PMS *) pas)->program_number;
      }
      t->version = pas->c.version;
      return 0;
    }
    else
    {
      switch (type)
      {
      case 0x00:
        clear_pas (pas);
        break;
      case 0x02:
        clear_pms ((PMS *) pas);
        break;
      case 0x40:
      case 0x41:
        clear_nis ((NIS *) pas);
        break;
      case 0x42:
      case 0x46:
        clear_sds ((SDS *) pas);
        break;
      default:
        errMsg ("got table id: %" PRIu8 "\n .. can't handle", type);
        return 1;
      }
    }
  }
  else
  {
    if ((type == 0x02) && (BIFI_IS_NONZERO (t->occupied))
        && (t->pnr != ((PMS *) pas)->program_number))
    {
      //we have a problem
      errMsg ("Error: pnr mismatch: %hu != %hu\n", t->pnr,
              ((PMS *) pas)->program_number);
      return 1;
    }
    action (x, (PAS const *) NULL, (PAS const *) pas);  //no prev section
    enter_sec (t, pas->c.section, pas, type);
    if (type == 0x02)
    {
      t->pnr = ((PMS *) pas)->program_number;
    }
    t->version = pas->c.version;
  }
  return 0;
}

/**
 *\brief run func on each section in the table
 */
void
for_each_pas (Table * t, void *x, void (*func) (void *x, PAS * sec))
{
  int i;
  for (i = 0; i < t->num_sections; i++)
  {
    if (bifiGet (t->occupied, i))
    {
      func (x, &t->pas_sections[i]);
    }
  }
}

void
for_each_pms (Table * t, void *x, void (*func) (void *x, PMS * sec))
{
  int i;
  for (i = 0; i < t->num_sections; i++)
  {
    if (bifiGet (t->occupied, i))
    {
      func (x, &t->pms_sections[i]);
    }
  }
}

void
for_each_nis (Table * t, void *x, void (*func) (void *x, NIS * sec))
{
  int i;
  for (i = 0; i < t->num_sections; i++)
  {
    if (bifiGet (t->occupied, i))
    {
      func (x, &t->nis_sections[i]);
    }
  }
}

void
for_each_sds (Table * t, void *x, void (*func) (void *x, SDS * sec))
{
  int i;
  for (i = 0; i < t->num_sections; i++)
  {
    if (bifiGet (t->occupied, i))
    {
      func (x, &t->sds_sections[i]);
    }
  }
}

int
tbl_is_complete (Table * t)
{
  int i;
  for (i = 0; i < t->num_sections; i++)
  {
    if (!bifiGet (t->occupied, i))
    {
      return 0;
    }
  }
  return 1;
}

typedef struct
{
  Table *tbl;
  uint32_t idx;
} IterTbl;

static void
iterTblFirst (void *data)
{
  uint32_t ctr;
  IterTbl *d = data;
  Table *t = d->tbl;
  for (ctr = 0; ctr < t->num_sections; ctr++)
  {
    if (bifiGet (t->occupied, ctr))
    {
      d->idx = ctr;
      return;
    }
  }
  errMsg ("iterTblFirst not found\n");
}

static void
iterTblLast (void *data)
{
  uint32_t ctr;
  IterTbl *d = data;
  Table *t = d->tbl;
  ctr = t->num_sections;
  do
  {
    ctr--;
    if (bifiGet (t->occupied, ctr))
    {
      d->idx = ctr;
      return;
    }
  }
  while (ctr);
  errMsg ("iterTblLast not found\n");
}

static int
iterTblAdvance (void *data)
{
  uint32_t ctr;
  IterTbl *d = data;
  Table *t = d->tbl;
  for (ctr = (d->idx + 1); ctr < t->num_sections; ctr++)
  {
    if (bifiGet (t->occupied, ctr))
    {
      d->idx = ctr;
      return 0;
    }
  }
  return 1;
}

static int
iterTblRewind (void *data)
{
  uint32_t ctr;
  IterTbl *d = data;
  Table *t = d->tbl;
  if (!d->idx)
    return 1;
  ctr = d->idx;
  do
  {
    ctr--;
    if (bifiGet (t->occupied, ctr))
    {
      d->idx = ctr;
      return 0;
    }
  }
  while (ctr);
  return 1;
}

static void *
iterTblGet (void *data)
{
  IterTbl *d = data;
  Table *t = d->tbl;
  return get_secptr (t, d->idx, t->table_id);
}

static void
iterTblDestroy (void *data)
{
  utlFAN (data);
}

int
iterTblInit (Iterator * i, Table * tbl)
{
  uint32_t ctr = 0;
  IterTbl *d;
  if ((!tbl) || (!tbl->num_sections) || (!BIFI_IS_NONZERO (tbl->occupied)))
  {
    return 1;
  }

  d = utlCalloc (1, sizeof (IterTbl));
  if (!d)
    return 1;

  //get first entry
  d->tbl = tbl;
  for (ctr = 0; ctr < tbl->num_sections; ctr++)
  {
    if (bifiGet (tbl->occupied, ctr))
    {
      break;
    }
  }

  if (ctr >= tbl->num_sections)
  {
    utlFAN (d);
    return 1;
  }
  else
    d->idx = ctr;

  i->data = d;
  i->kind = I_SEQ_BOTH;
  i->s.first_element = iterTblFirst;
  i->s.last_element = iterTblLast;
  i->s.advance = iterTblAdvance;
  i->s.rewind = iterTblRewind;
  i->get_element = iterTblGet;
  i->i_destroy = iterTblDestroy;
  return 0;
}
