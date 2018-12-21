#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <ncurses.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "tsv.h"
#include "dmalloc.h"
#define FILE_DBG DF_MAIN
/**
 *\file main.c
 *\brief Transport Stream Verification program
 *
 * checks for sync bytes and continuity counter correctness
 * and outputs locations on stdout.
 *
 * May optionally output packets with wrong cc and non sync bytes to dump file.
 *
 * May optionally output found sync correct ts packets to output file.
 * (It tries to throw away garbage between packets..)
 *
 * Can not reassemble faulty cc sequences.
 */
int DF_MAIN;
extern int DF_TSV;

int
dbgprintf (void *d, const char *f, int l, int pri, const char *template, ...)
{
  va_list ap;
#ifdef DMALLOC_DISABLE
  int n, size = 100;
  char *p, *np;

#endif

#ifndef DMALLOC_DISABLE
  extern char *program_invocation_short_name;
#endif

#ifdef DMALLOC_DISABLE

  if ((p = malloc (size)) == NULL)
    return 0;

  while (1)
  {
    /* Try to print in the allocated space. */
    va_start (ap, template);
    n = vsnprintf (p, size, template, ap);
    va_end (ap);
    /* If that worked, return the string. */
    if (n > -1 && n < size)
      break;
    /* Else try again with more space. */
    if (n > -1)                 /* glibc 2.1 */
      size = n + 1;             /* precisely what is needed */
    else                        /* glibc 2.0 */
      size *= 2;                /* twice the old size */
    if ((np = realloc (p, size)) == NULL)
    {
      free (p);
      return 0;
    }
    else
    {
      p = np;
    }
  }

  printf ("File %s, Line %d: %s", f, l, p);
  free (p);
#else

  dmalloc_message ("%s, ", program_invocation_short_name);
  dmalloc_message ("File %s, ", f);
  dmalloc_message ("Line %d: ", l);
  va_start (ap, template);
  dmalloc_vmessage (template, ap);
  va_end (ap);
#endif
  return 0;
}


#define MY_BUF_SZ (1024*1024*10)

static char MY_IN_BUF[MY_BUF_SZ];
static char MY_OUT_BUF[MY_BUF_SZ];
static char MY_DUMP_BUF[MY_BUF_SZ];

static char usage[] = "Usage: \n\
tsverify -i<infile> -o<outfile> -x<dumpfile>\n\
\n\
 checks for sync bytes and continuity counter correctness\n\
 and outputs locations on stdout.\n\
\n\
 May optionally output packets with wrong cc and non sync bytes to dump file.\n\
\n\
 May optionally output found sync correct ts packets to output file.\n\
 (It tries to throw away garbage between packets..)\n\
\n\
 Can not reassemble faulty cc sequences.\n\
";

int
main (int argc, char **argv)
{
  char *in_name = NULL, *out_name = NULL, *dump_name = NULL;
  FILE *in_f = NULL, *out_f = NULL, *dump_f = NULL;
  int rv, opt;
  debugSetFunc (NULL /*output */ , dbgprintf, 32);
  DF_MAIN = debugAddModule ("main");
  DF_TSV = debugAddModule ("tsv");
  debugSetFlags (argc, argv, "-d");
  while ((opt = getopt (argc, argv, "i:o:x:")) != -1)
  {
    switch (opt)
    {
    case 'i':
      in_name = strdup (optarg);
      break;
    case 'o':
      out_name = strdup (optarg);
      break;
    case 'x':
      dump_name = strdup (optarg);
      break;
    default:
      fprintf (stderr, "unknown option %c\n", opt);
      fputs (usage, stderr);
      break;
    }
  }

  if (NULL == in_name)
  {
    fprintf (stderr, "No input file specified. aborting\n");
    fputs (usage, stderr);
    return 1;
  }
  in_f = fopen (in_name, "rb");
  if (NULL == in_f)
  {
    fprintf (stderr, "Could not open input file %s: %s\n", in_name,
             strerror (errno));
    fputs (usage, stderr);
    return 1;
  }
  setvbuf (in_f, MY_IN_BUF, _IOFBF, MY_BUF_SZ);
  if (out_name)
  {
    out_f = fopen (out_name, "wb");
    if (out_f)
      setvbuf (out_f, MY_OUT_BUF, _IOFBF, MY_BUF_SZ);
  }
  if (dump_name)
  {
    dump_f = fopen (dump_name, "wb");
    if (dump_f)
      setvbuf (dump_f, MY_DUMP_BUF, _IOFBF, MY_BUF_SZ);
  }

  rv = tsVerify (in_f, out_f, dump_f);
  if (dump_f)
    fclose (dump_f);
  if (out_f)
    fclose (out_f);
  if (in_f)
    fclose (in_f);

  if (in_name)
    free (in_name);
  if (out_name)
    free (out_name);
  if (dump_name)
    free (dump_name);
  return rv;
}
