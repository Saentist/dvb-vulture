#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "dmalloc_loc.h"
#include "utl.h"
#include "debug.h"

#include "rcfile.h"

int DF_RCFILE;
#define FILE_DBG DF_RCFILE

#ifdef __WIN32__
#include"win/rcfile2.c"
#else
#include"lin/rcfile2.c"
#endif


static int
rcfile_clear_pair (KvPair * pair)
{
  if (!pair)
    return 1;
  utlFAN (pair->key);
  utlFAN (pair->value);
  return 0;
}


static int
rcfile_clear_section (Section * sc)
{
  if (!sc)
    return 1;
  while (sc->kv_count--)
  {
    rcfile_clear_pair (&(sc->pairs[sc->kv_count]));
  }
  utlFAN (sc->pairs);
  utlFAN (sc->name);
  return 0;
}

int
rcfileClear (RcFile * data)
{
  if (!data)
    return 1;
  while (data->section_count--)
  {
    rcfile_clear_section (&(data->sections[data->section_count]));
  }
  utlFAN (data->sections);
  return 0;
}


Section *
rcfileFindSec (RcFile * data, char *section)
{
  int ctr = 0;
  while (ctr < data->section_count)
  {
    if (!strcmp (data->sections[ctr].name, section))    //found
      return &(data->sections[ctr]);
    ctr++;
  }
  return 0;
}

KvPair *
rcfileFindPair (Section * data, char *key)
{
  int ctr = 0;
  debugMsg ("kv_count: %d\n", data->kv_count);
  while (ctr < data->kv_count)
  {
    if (!strcmp (data->pairs[ctr].key, key))    //found
      return &(data->pairs[ctr]);
    ctr++;
  }
  return 0;
}

int
rcfileFindVal (RcFile * data, char *sect, char *key, char **value)
{
  Section *sec;

  sec = rcfileFindSec (data, sect);

  if (!sec)
    return 1;
  return rcfileFindSecVal (sec, key, value);
}

int
rcfileFindValInt (RcFile * data, char *section, char *key, long *value)
{
  char *val;
  long res;
  if (rcfileFindVal (data, section, key, &val))
    return 1;
  errno = 0;
  debugMsg ("before strtol\n");
  res = strtol (val, NULL, 10);
  debugMsg ("after strtol\n");
  if (errno)
  {
    errMsg ("strtol: %s", strerror (errno));
    return 1;
  }
  *value = res;
  debugMsg ("before return\n");
  return 0;
}

int
rcfileFindValDouble (RcFile * data, char *section, char *key, double *value)
{
  char *val;
  double res;
  if (rcfileFindVal (data, section, key, &val))
    return 1;
  errno = 0;
  res = strtod (val, NULL);
  if (errno)
  {
    errMsg ("strtod: %s", strerror (errno));
    return 1;
  }
  *value = res;
  return 0;
}

int
rcfileFindSecVal (Section * sec, char *key, char **value)
{
  KvPair *val;
  val = rcfileFindPair (sec, key);
  if (!val)
    return 1;

  *value = val->value;
  return 0;
}

int
rcfileFindSecValInt (Section * sec, char *key, long *value)
{
  char *val;
  long res;
  if (rcfileFindSecVal (sec, key, &val))
    return 1;
  errno = 0;
  debugMsg ("strtol\n");
  res = strtol (val, NULL, 10);
  if (errno)
  {
    errMsg ("strtol: %s", strerror (errno));
    return 1;
  }
  *value = res;
  return 0;
}

int
rcfileFindSecValDouble (Section * sec, char *key, double *value)
{
  char *val;
  long res;
  if (rcfileFindSecVal (sec, key, &val))
    return 1;
  errno = 0;
  res = strtod (val, NULL);
  if (errno)
  {
    errMsg ("strtol: %s", strerror (errno));
    return 1;
  }
  *value = res;
  return 0;
}

//#define TEST
#ifdef TEST
int DF_BITS;
int DF_BTREE;
int DF_CUSTCK;
int DF_DHASH;
int DF_EPG_EVT;
int DF_FP_MM;
int DF_ITEM_LIST;
int DF_RCPTR;
int DF_SIZESTACK;
int DF_RS_ENTRY;
int DF_RCFILE;
int DF_TS;
int DF_SWITCH;
int DF_OPT;
int DF_CLIENT;
int DF_FAV;
int DF_FAV_LIST;
int DF_IN_OUT;
int DF_IPC;


void
dump_config (RcFile * data)
{
  int ctr1 = 0;
  int ctr2;
  while (ctr1 < data->section_count)
  {
    Section *sec = &(data->sections[ctr1]);
    printf ("[%s]\n", sec->name);

    ctr2 = 0;
    while (ctr2 < sec->kv_count)
    {
      KvPair *kv = &(sec->pairs[ctr2]);
      printf ("%s=%s\n", kv->key, kv->value);
      ctr2++;
    }
    puts ("");
    ctr1++;
  }
}


int
main (int argc, char *argv[])
{

  const char *const fname = "/etc/X11/xawtvrc";
  RcFile conf;
  FILE *cfg;

  if (rcfileExists ("adfs/sddr/s/df/s"))
  {
    puts ("exists");
  }

  if (rcfileCreate ("adfs/sddr/s/df/s"))
  {                             //succeed
    puts ("create");
  }
  if (rcfileCreate ("/home/klammerj/adfs/sddr/s/df/s"))
  {                             //succeed
    puts ("create2");
  }

  if (rcfileCreate ("./adf3/sddr/s/df/s"))
  {                             //want this to succeed(on the first try..)
    puts ("create3");
  }

  if (rcfileCreate ("./adf3/sddr/s/ds/"))
  {                             //want this to fail
    puts ("create4");           //this should show up(on linux, not on win...)
  }


  if (rcfileParse (fname, &conf))
  {
    fprintf (stderr, "parse\n");
    return 1;
  }
  dump_config (&conf);

  if (rcfileClear (&conf))
  {
    fprintf (stderr, "clear\n");
  }
}

#endif
