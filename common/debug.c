#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <execinfo.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"

extern int DF_BTREE;
extern int DF_RCFILE;
extern int DF_CUSTCK;
extern int DF_FP_MM;
extern int DF_RCPTR;
extern int DF_ITEM_LIST;
extern int DF_EPG_EVT;
extern int DF_BITS;
extern int DF_SIZESTACK;
extern int DF_TS;
extern int DF_DHASH;
extern int DF_SWITCH;
extern int DF_RS_ENTRY;
extern int DF_OPT;
extern int DF_CLIENT;
extern int DF_IN_OUT;
extern int DF_FAV;
extern int DF_FAV_LIST;
extern int DF_IPC;

void *DebugData;
int (*DebugPrintf) (void *x, const char *f, int l, int pri,
                    const char *template, ...);

//#define DEFAULT_DEBUG_FLAGS 0

static uint32_t DebugNumModules;
static char **DebugNames;       //[32];
static uint8_t DebugDummy = 0;
uint8_t *DebugFlags = &DebugDummy;      ///<avoid segfaults if macros are called early
bool DebugSBT = 0;

uint32_t DebugSize;
int
debugAddModule (const char *s)
{
  int flag;
  if (DebugNumModules >= DebugSize)
  {
    DebugPrintf (DebugData, __FILE__, __LINE__, LOG_ERR,
                 "Error: too many debug modules\n");
    return 0;
  }
  flag = DebugNumModules;       //&31;
  DebugNames[flag] = (char *) s;
  DebugNumModules++;
  return flag;
}

int
debugSetFunc (void *data,
              int (*xprintf) (void *x, const char *f, int l, int pri,
                              const char *template, ...),
              uint32_t num_modules)
{
  num_modules += 20;
  if (DebugNames)
  {
    utlFAN (DebugFlags);
    utlFAN (DebugNames);
  }
  DebugFlags = utlCalloc (num_modules, sizeof (uint8_t));
  DebugNames = utlCalloc (num_modules, sizeof (char *));
  if ((!DebugFlags) || (!DebugNames))
    return 1;
  DebugSize = num_modules;
  DebugPrintf = xprintf;
  DebugData = data;
  DF_BITS = debugAddModule ("bits");    //bitfield manipulation
  DF_BTREE = debugAddModule ("btree");  //binary tree ops
  DF_CUSTCK = debugAddModule ("custck");        //stacked cleanup handlers. Verbosity 1.
  DF_DHASH = debugAddModule ("dhash");  //hash map. Verbosity 1.
  DF_EPG_EVT = debugAddModule ("epg_evt");      //EPG. Verbosity 1.
  DF_FP_MM = debugAddModule ("fp_mm");  //FILE * allocator. Verbosity 1,10.
  DF_ITEM_LIST = debugAddModule ("item_list");  //Linked lists. Verbosity 1.
  DF_RCPTR = debugAddModule ("rcptr");  //Reference counting allocators. Verbosity 1.
  DF_SIZESTACK = debugAddModule ("size_stack"); //bounds checking for section processing. Verbosity 1.
  DF_RS_ENTRY = debugAddModule ("rs_entry");    //recording schedule
  DF_RCFILE = debugAddModule ("rcfile");        //config file handling
  DF_TS = debugAddModule ("timestamp"); //PSI MJD<->timestamp conversions. Verbosity 1.
  DF_SWITCH = debugAddModule ("switch");        //switch setup. Verbosity 1.
  DF_OPT = debugAddModule ("opt");      //cheap cmdline handling
  DF_CLIENT = debugAddModule ("client");        //client side operations
  DF_IN_OUT = debugAddModule ("in_out");        //input/output and socket utility funcs
  DF_FAV = debugAddModule ("fav");      //fav storage
  DF_FAV_LIST = debugAddModule ("fav_list");    //fav list storage
  DF_IPC = debugAddModule ("ipc");      //byte swapping send, receive routines
  return 0;
}

//uint32_t DebugFlags = DEFAULT_DEBUG_FLAGS;


int
debugSetFlags (int argc, char *argv[], char *pfx)
{
  int i;
  uint32_t j;
  uint8_t pri;
  i = 1;
  while (i < argc)
  {
    if (!strcmp (argv[i], pfx))
    {
      i++;
      if (i < argc)
      {
        j = 0;
        if (!strncmp (argv[i], "all", 3))
        {
          pri = 255;
          if (argv[i][3] == '=')
          {
            pri = atoi (argv[i] + 4);
          }
          for (j = 0; j < DebugSize; j++)
            DebugFlags[j] = pri;
//                                      DebugFlags = 0xffffffff;
        }
        else
          while (j < DebugNumModules)
          {
            int len;
            if (!strncmp
                (argv[i], DebugNames[j], len = strlen (DebugNames[j])))
            {
              pri = 255;
              if (argv[i][len] == '=')
              {
                pri = atoi (argv[i] + len + 1);
              }
              DebugFlags[j] = pri;
              j = DebugNumModules;
            }
            j++;
          }
      }
    }
    i++;
  }
  return 0;
}

void
debugTrace (void)
{
  int j, nptrs;
#define SIZE 100
  void *buffer[SIZE];
  char **strings;

  nptrs = backtrace (buffer, SIZE);
  errMsg ("backtrace() returned %d addresses\n", nptrs);

  /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
     would produce similar output to the following: */

  strings = backtrace_symbols (buffer, nptrs);
  if (strings == NULL)
  {
    errMsg ("backtrace_symbols %s", strerror (errno));
    return;
  }

  for (j = 1; j < nptrs; j++)
    errMsg ("%s\n", strings[j]);

  free (strings);

  while (DebugSBT)
    usleep (100000);
}
