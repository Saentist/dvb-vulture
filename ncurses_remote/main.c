#include <sys/time.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include "help.h"
#include "utl.h"
#include "list.h"
#include "opt.h"
#include "debug.h"
#include "app_data.h"
#include "dmalloc_f.h"
#define FILE_DBG DF_MAIN
/**
 *\file main.c
 *\brief remote control for stream agent
 *
 * shows a menu and
 * Accepts various commands
 *
 */



int DF_MAIN;
extern int DF_APP_DATA;
extern int DF_FAV;
extern int DF_FAV_LIST;
extern int DF_MNU_STRINGS;
extern int DF_SW_MNU;
extern int DF_REC_MNU;
extern int DF_EPG_UI;

#define RBUF_SZ 4096
static char rbuf[RBUF_SZ] = { '\0' };   ///<retains debug/error messages during init

static int store_dbg = 1;

int
dbgprintf (NOTUSED void *d, const char *f, int l, NOTUSED int pri,
           const char *template, ...)
{
  va_list ap;
#ifdef DMALLOC_DISABLE
  AppData *a = d;
  int n, size = 100;
  char *p, *np;

#endif

#ifndef DMALLOC_DISABLE
  extern char *program_invocation_short_name;
#endif

#ifdef DMALLOC_DISABLE

  if ((p = utlMalloc (size)) == NULL)
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
      utlFAN (p);
      return 0;
    }
    else
    {
      p = np;
    }
  }

  if (a->output)
  {
    wprintw (a->output, "File %s, Line %d: %s", f, l, p);
    wrefresh (a->output);
  }
  else
    printf ("File %s, Line %d: %s", f, l, p);

  if (store_dbg)
  {
    assert ((strlen (rbuf) + strlen (p)) < RBUF_SZ);
    strcat (rbuf, p);
  }
  utlFAN (p);
#else

  dmalloc_message ("%s, ", program_invocation_short_name);
  dmalloc_message ("File %s, ", f);
  dmalloc_message ("Line %d: ", l);
  va_start (ap, template);
  dmalloc_vmessage (template, ap);
  va_end (ap);
  if (store_dbg)
  {
    assert (strlen (rbuf) < RBUF_SZ);
    va_start (ap, template);
    vsnprintf (rbuf + strlen (rbuf), RBUF_SZ - strlen (rbuf), template, ap);
    va_end (ap);
  }
#endif
  return 0;
}

//in app_data.c

#define IN_BUF_SZ 256
/**
 *\brief the main function
 *
 * cmdline arguments to process
 * -cfg <configfile>
 * (and the debug flags)
 */
int
main (int argc, char **argv)
{
  AppData a = { 0 };
  WINDOW *output, *outer;
  int maxx, maxy;
  setlocale (LC_ALL, "");
  a.output = NULL;
  if (find_opt (argc, argv, "-h") || find_opt (argc, argv, "--help"))
  {
    fprintf (stderr, "%s\n", help_txt);
    fprintf (stderr,
             "For more information try the texinfo manual: info dvbv-nr\n");
    fprintf (stderr, "%s", rbuf);
    return 0;
  }
//      mtrace();
  initscr ();                   //ncurses crashes here (in windows executable with wine)
  //(by binary search)no Idea how to get source lvl debugging w wine..
  //also in wineconsole...
  start_color ();
  cbreak ();
  noecho ();
  nonl ();
#ifdef PDCURSES
//      resize_term(52,135);naah
#endif
  keypad (stdscr, TRUE);
  getmaxyx (stdscr, maxy, maxx);
  refresh ();
  outer = newwin (maxy - maxy * 2 / 3, maxx, maxy * 2 / 3, 0);
  output = derwin (outer, maxy - maxy * 2 / 3 - 2, maxx - 2, 1, 1);
  wsetscrreg (output, 1, maxy - maxy * 2 / 3 - 2);
  scrollok (output, TRUE);
  box (outer, 0, 0);
  wrefresh (outer);
  wrefresh (output);
  debugSetFunc (&a, dbgprintf, 32);
  DF_MAIN = debugAddModule ("main");
  DF_APP_DATA = debugAddModule ("app_data");
  DF_MNU_STRINGS = debugAddModule ("menu_strings");
  DF_SW_MNU = debugAddModule ("switch_mnu");
  DF_REC_MNU = debugAddModule ("rec_mnu");
  DF_EPG_UI = debugAddModule ("epg_ui");
  debugSetFlags (argc, argv, "-d");

  if (init_app_data (&a, output, outer, argc, argv))
  {
    errMsg ("init failed\n");
    assert (ERR != delwin (output));
    assert (ERR != delwin (outer));
    endwin ();
    store_dbg = 0;
    fprintf (stderr, "%s", rbuf);
    return 1;
  }
  store_dbg = 0;

  //will this keep read() from failing with EINTR ?
//      siginterrupt(SIGWINCH,false); Yes, but resizng doesn't work
  app_run (&a);

  clear_app_data (&a);

  endwin ();
  //about 34821 bytes here left allocated without main program
//      _nc_freeall();
  return 0;
}
