#ifndef __dvb_string_h
#define __dvb_string_h
/**
 *\file dvb_string.h
 *\brief converting strings from dvb descriptors to UTF8
 *
 *How can we do this best? We may see character data from different character sets here.
 *Perhaps we can convert to unicode?
 */

#include <stdint.h>
#include <wchar.h>
#include <stdio.h>

typedef struct
{
  int len;                      ///<how long it is.. 
  char *buf;                    ///<string data. not NUL terminated
} DvbString;

void dvbStringClear (DvbString * s);

///return dyn allocated ascii string, NUL terminated. Possibly lossy
char *dvbStringToAscii (DvbString * s);
///convert to w
wchar_t *dvbStringToW (DvbString * s);

int dvbStringCopy (DvbString * to, DvbString * from);
int dvbStringWrite (FILE * f, DvbString * s);
int dvbStringRead (FILE * f, DvbString * s);
int dvbStringRcv (int f, DvbString * s);
int dvbStringSnd (int f, DvbString * s);

#endif //__dvb_string_h
