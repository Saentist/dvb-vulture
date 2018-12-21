#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "pth_loc.h"
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"
#define MAXIMUM(a,b) ((a)>(b)?a:b)
#ifdef __WIN32__
#include "win/utl2.c"
#endif

char *UTL_UNDEF = "undef";

void
utlAsciiSanitize (char *p)
{
  if (p == NULL)
    return;
  while (*p)                    //sanitize strings so curses doesn't complain
  {
    if (!isprint (*p))          //TODO: better allow control chars \n \t etc. but menu might not like it..
      *p = ' ';
    p++;
  }
}


void
utlWSanitize (wchar_t * p)
{
  if (p == NULL)
    return;
  while (*p)                    //sanitize strings so curses doesn't complain
  {
    if (!iswprint (*p))         //TODO: better allow control chars \n \t etc
      *p = L' ';
    p++;
  }
}



///concat+realloc unterminated 2nd string
int
utlSmartCatUt (char **str_p, unsigned *size_p, char *str2, unsigned size2)
{
  char *str = *str_p;
  unsigned size = *size_p;
  if ((NULL == str2) || (NULL == str_p) || (NULL == size_p))
    return 1;
  if (NULL == str)
  {
    size = MAXIMUM (128, size2 + 1);
    str = utlCalloc (1, size);
    if (!str)
      return 1;
  }
  else if (size < (strlen (str) + size2 + 1))
  {
    char *p;
    size = strlen (str) + size2 + 1 + 128;
    p = utlRealloc (str, size);
    if (!p)
      return 1;
    str = p;
  }
  strncat (str, str2, size2);
  *str_p = str;
  *size_p = size;
  return 0;
}

///concat+realloc
int
utlSmartCat (char **str_p, unsigned *size_p, char *str2)
{
  if (NULL != str2)
    return utlSmartCatUt (str_p, size_p, str2, strlen (str2));
  return 1;
}

int
utlSmartCatDone (char **str_p)
{
  char *str = *str_p;
  char *p;
  unsigned minsz;
  if (NULL == str)
    return 1;
  minsz = strlen (str) + 1;
  p = utlRealloc (str, minsz);
  if (NULL == p)
    return 1;
  *str_p = p;
  return 0;
}


int
utlSmartCatW (wchar_t ** str_p, unsigned *size_p, wchar_t * str2)
{
  wchar_t *str = *str_p;
  unsigned size = *size_p;
  if ((NULL == str2) || (NULL == str_p) || (NULL == size_p))
    return 1;
  if (NULL == str)
  {
    size = MAXIMUM (128, wcslen ((wchar_t *) str2) + 1);
    str = utlCalloc (sizeof (wchar_t), size);
    if (!str)
      return 1;
  }
  else if (size < (wcslen ((wchar_t *) str) + wcslen ((wchar_t *) str2) + 1))
  {
    wchar_t *p;
    size = wcslen ((wchar_t *) str) + wcslen ((wchar_t *) str2) + 1 + 128;
    p = utlRealloc (str, size * sizeof (wchar_t));
    if (!p)
      return 1;
    str = (wchar_t *) p;
  }
  wcscat ((wchar_t *) str, (wchar_t *) str2);
  *str_p = str;
  *size_p = size;
  return 0;
}


int
utlSmartCatUtW (wchar_t ** str_p, unsigned *size_p, wchar_t * str2,
                unsigned size2)
{
  wchar_t *str = *str_p;
  unsigned size = *size_p;
  if ((NULL == str2) || (NULL == str_p) || (NULL == size_p))
    return 1;
  if (NULL == str)
  {
    size = MAXIMUM (128, size2 + 1);
    str = utlCalloc (sizeof (wchar_t), size);
    if (!str)
      return 1;
  }
  else if (size < (wcslen ((wchar_t *) str) + size2 + 1))
  {
    wchar_t *p;
    size = wcslen ((wchar_t *) str) + size2 + 1 + 128;
    p = utlRealloc (str, size * sizeof (wchar_t));
    if (!p)
      return 1;
    str = p;
  }
  wcsncat ((wchar_t *) str, (wchar_t *) str2, size2);
  *str_p = str;
  *size_p = size;
  return 0;
}

int
utlSmartCatDoneW (wchar_t ** str_p)
{
  wchar_t *str = *str_p;
  wchar_t *p;
  unsigned minsz;
  if (NULL == str)
    return 1;
  minsz = wcslen ((wchar_t *) str) + 1;
  p = utlRealloc (str, sizeof (wchar_t) * minsz);
  if (NULL == p)
    return 1;
  *str_p = (wchar_t *) p;
  return 0;
}


int
utlSmartMemmove (uint8_t ** str_p, unsigned *size_p, unsigned *alloc_p,
                 uint8_t * str2, unsigned size2)
{
  uint8_t *str = *str_p;
  unsigned alloc_sz = *alloc_p;
  unsigned size = *size_p;
  if (NULL == str2)
    return 1;
  if (NULL == str)
  {
    alloc_sz = MAXIMUM (128, size2);
    str = utlCalloc (1, alloc_sz);
    if (!str)
      return 1;
  }
  else if (alloc_sz < (size + size2))
  {
    uint8_t *p;
    alloc_sz = size + size2 + 128;
    p = utlRealloc (str, alloc_sz);
    if (!p)
      return 1;
    str = p;
  }
  if (size2 > 0)
    memmove (str + size, str2, size2);
  size += size2;
  *str_p = str;
  *size_p = size;
  *alloc_p = alloc_sz;
  return 0;
}

int
utlAppend (uint8_t ** str_p, unsigned *size_p, unsigned *alloc_p,
           uint8_t * str2, unsigned nmemb, unsigned size2)
{
  unsigned sz_p = (*size_p) * size2, alc_p = (*alloc_p) * size2, sz2 =
    nmemb * size2;
  if (utlSmartMemmove (str_p, &sz_p, &alc_p, str2, sz2))
    return 1;
  *size_p = sz_p / size2;
  *alloc_p = alc_p / size2;     //guess this may end up inexact
  return 0;
}


int
utlRmv (uint8_t * str_p, unsigned *size_p, unsigned e_sz, unsigned idx)
{
  unsigned size = *size_p;
  if (idx >= size)
    return 1;
  if (idx == (size - 1))
    memset (str_p + (e_sz * idx), 0, e_sz);
  else
    memmove (str_p + (e_sz * idx), str_p + (e_sz * (idx + 1)),
             (size - idx - 1) * e_sz);
  size--;
  *size_p = size;
  return 0;
}
