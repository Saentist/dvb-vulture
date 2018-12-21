#include <stdlib.h>
#include "fav.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"
int DF_FAV;
#define FILE_DBG DF_FAV

extern void write_string (char *str, FILE * f);
extern char *read_string (FILE * f);

int
favRead (Favourite * dest, FILE * f)
{
  debugMsg ("reading fav\n");
  fread (dest, sizeof (Favourite) - 3 * sizeof (DvbString), 1, f);
  dvbStringRead (f, &dest->service_name);
  dvbStringRead (f, &dest->provider_name);
  dvbStringRead (f, &dest->bouquet_name);
  return 0;
}

int
favWrite (Favourite * src, FILE * f)
{
  debugMsg ("writing fav\n");
  fwrite (src, sizeof (Favourite) - 3 * sizeof (DvbString), 1, f);
  dvbStringWrite (f, &src->service_name);
  dvbStringWrite (f, &src->provider_name);
  dvbStringWrite (f, &src->bouquet_name);
  return 0;
}

void
favClear (Favourite * fav)
{
  debugMsg ("clearing fav\n");
  dvbStringClear (&fav->service_name);
  dvbStringClear (&fav->provider_name);
  dvbStringClear (&fav->bouquet_name);
}
