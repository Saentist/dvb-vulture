/*
 * utils.h
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include "debug.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

extern int DF_MHEG;
#define FILE_DBG DF_MHEG


#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif


char *skip_ws (char *);

char hex_digit (uint8_t);

#undef safe_malloc
#define safe_malloc(a_) \
dmalloc_safe_malloc(a_,__FILE__,__LINE__)

#undef safe_realloc
#define safe_realloc(a_,b_) \
dmalloc_safe_realloc(a_,b_,__FILE__,__LINE__)

void *dmalloc_safe_malloc (size_t, char *file, int line);
void *dmalloc_safe_realloc (void *, size_t, char *file, int line);
void safe_free (void *);

//void hexdump(unsigned char *, size_t);

#define error errMsg
//void error(char *, ...);
//void fatal(char *, ...);
#define fatal errMsg

/* in rb-download.c */
#define verbose(a,rest...) debugMsg(a,## rest)
//void verbose(char *, ...);
#define vverbose(a,rest...) debugPri(3,a,## rest)
//void vverbose(char *, ...);
#define vhexdump(d_, n_) (debugOnPri(3)?hexdump(d_, n_):0)
void hexdump (unsigned char *data, size_t nbytes);

#endif /* __UTILS_H__ */
