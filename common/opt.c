#include <string.h>
#include "utl.h"
#include "opt.h"
#include "debug.h"

int DF_OPT;
#define FILE_DBG DF_OPT

char *
find_opt_arg (int argc, char *argv[], char *key)
{
  int i = 0;
  debugMsg ("looking for cmdline argument %s\n", key);
  for (i = 1; i < argc; i++)
  {
    debugMsg ("seen %s\n", argv[i]);
    if (!strcmp (argv[i], key))
    {
      debugMsg ("matches\n");
      if (argc > (i + 1))
      {
        debugMsg ("got value %s\n", argv[i + 1]);
        return argv[i + 1];
      }
    }
  }
  debugMsg ("not found\n");
  return NULL;
}

int
find_opt (int argc, char *argv[], char *key)
{
  int i = 0;
  debugMsg ("looking for cmdline argument %s\n", key);
  for (i = 1; i < argc; i++)
  {
    debugMsg ("seen %s\n", argv[i]);
    if (!strcmp (argv[i], key))
    {
      debugMsg ("matches\n");
      return 1;
    }
  }
  debugMsg ("not found\n");
  return 0;
}
