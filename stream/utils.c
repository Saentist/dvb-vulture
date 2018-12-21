/*
 * utils.c
 */

/*
 * Copyright (C) 2005, Simon Kilvington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "utils.h"
#include "dmalloc.h"

/*
 * move str to the next non-white space character (or the end of the string)
 */

char *
skip_ws (char *str)
{
  if (str == NULL)
    return NULL;

  while (*str != '\0' && isspace ((int) *str))
    str++;

  return str;
}

/*
 * returns a single ASCII char for the values 0-15
 */

char
hex_digit (uint8_t val)
{
  /* assert */
  if (val > 0xf)
    fatal ("Not a single hex digit");

  return (val < 10) ? '0' + val : 'a' + (val - 10);
}

/*
 * I don't want to double the size of my code just to deal with malloc failures
 * if you've run out of memory you're fscked anyway, me trying to recover is not gonna help...
 */

#ifdef dmalloc_tag
#define d_t(p_,f_,l_) ((NULL!=p_)?dmalloc_tag(p_,f_,l_):(1))
#else
///no-op define
#define d_t(p_,f_,l_) (1)
#endif

void *
dmalloc_safe_malloc (size_t nbytes, char *file, int line)
{
  void *buf;

  if ((buf = malloc (nbytes)) == NULL)
    fatal ("Out of memory");
  else
    d_t (buf, file, line);
  return buf;
}

/*
 * safe_realloc(NULL, n) == safe_malloc(n)
 */

void *
dmalloc_safe_realloc (void *oldbuf, size_t nbytes, char *file, int line)
{
  void *newbuf;

  if (oldbuf == NULL)
    return safe_malloc (nbytes);

  if ((newbuf = realloc (oldbuf, nbytes)) == NULL)
    fatal ("Out of memory");
  else
    d_t (newbuf, file, line);

  return newbuf;
}

/*
 * safe_free(NULL) is okay
 */

void
safe_free (void *buf)
{
  if (buf != NULL)
    free (buf);

  return;
}


/* number of bytes per line */
#define HEXDUMP_WIDTH	16

void
hexdump (unsigned char *data, size_t nbytes)
{
//perhaps later..
#if 0
  size_t nout;
  int i;
#define TBUF_SZ 2048*10
  char tbuf[TBUF_SZ] = { '\0' };
  char *p = tbuf;
  int rv;
  size_t sz = TBUF_SZ;
  nout = 0;
  while (nout < nbytes)
  {
    /* byte offset at start of line */
    if ((nout % HEXDUMP_WIDTH) == 0)
    {
      rv = snprintf (p, sz, "0x%.8x  ", nout);
      if (rv > 0)
      {
        p += rv;
        sz -= rv;
      }
    }
    /* the byte value in hex */
    rv = snprintf (p, sz, "%.2x ", data[nout]);
    if (rv > 0)
    {
      p += rv;
      sz -= rv;
    }
    /* the ascii equivalent at the end of the line */
    if ((nout % HEXDUMP_WIDTH) == (HEXDUMP_WIDTH - 1))
    {
      rv = snprintf (p, sz, " ");
      if (rv > 0)
      {
        p += rv;
        sz -= rv;
      }
      for (i = HEXDUMP_WIDTH - 1; i >= 0; i--)
      {
        rv =
          snprintf (p, sz, "%c",
                    isprint (data[nout - i]) ? data[nout - i] : '.');
        if (rv > 0)
        {
          p += rv;
          sz -= rv;
        }
      }
      rv = snprintf (p, sz, "\n");
      if (rv > 0)
      {
        p += rv;
        sz -= rv;
      }
    }
    nout++;
  }

  /* the ascii equivalent if we haven't just done it */
  if ((nout % HEXDUMP_WIDTH) != 0)
  {
    /* pad to the start of the ascii equivalent */
    for (i = (nout % HEXDUMP_WIDTH); i < HEXDUMP_WIDTH; i++)
    {
      rv = snprintf (p, sz, "   ");
      if (rv > 0)
      {
        p += rv;
        sz -= rv;
      }
    }
    rv = snprintf (p, sz, " ");
    if (rv > 0)
    {
      p += rv;
      sz -= rv;
    }
    /* print the ascii equivalent */
    nout--;
    for (i = (nout % HEXDUMP_WIDTH); i >= 0; i--)
    {
      rv =
        snprintf (p, sz, "%c",
                  isprint (data[nout - i]) ? data[nout - i] : '.');
      if (rv > 0)
      {
        p += rv;
        sz -= rv;
      }
    }
    rv = snprintf (p, sz, "\n");
    if (rv > 0)
    {
      p += rv;
      sz -= rv;
    }
  }
  verbose (tbuf);
#endif
  return;
}
