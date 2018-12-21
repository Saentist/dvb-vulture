#include <string.h>
#include <stdlib.h>
#include "chart.h"
#include "debug.h"
#include "dmalloc_f.h"

int DF_CHART;
#define FILE_DBG DF_CHART

///initialize chart
int
chartInit (Chart * c, int x, int y, int w, int h)
{
  memset (c, 0, sizeof (*c));
  c->vmax = 10;
  c->x = x;
  c->y = y;
  c->w = w;
  c->h = h;
  c->cur_x = 0;
  if ((c->h < 4) || (c->w < 20))
    return 1;
  c->buf = calloc (w, sizeof (int));
  if (NULL == c->buf)
    return 1;
  return 0;
}

///clear chart
void
chartClear (Chart * c)
{
  free (c->buf);
  memset (c, 0, sizeof (*c));
}

///change size
int
chartResize (Chart * c, int x, int y, int w, int h)
{
  if (c->buf)
  {
    free (c->buf);
    c->buf = NULL;
  }
  c->x = x;
  c->y = y;
  c->w = w;
  c->h = h;
  c->cur_x = 0;
  if ((h < 4) || (w < 20))
    return 1;
  c->buf = calloc (w, sizeof (int));
  if (NULL == c->buf)
    return 1;
  return 0;
}

///add a value to draw
int
chartVal (Chart * c, int v)
{
  if ((c->h < 4) || (c->w < 20))
    return 1;
  if (v > c->vmax)
    c->vmax = v;
  if (c->cur_x >= c->w)
    c->cur_x -= c->w;
  c->buf[c->cur_x] = v;
  c->cur_x++;
  return 0;
}

static int
get_vmax (Chart * c)
{
  int rv, i;
  rv = 0;
  for (i = 0; i < c->w; i++)
    if (c->buf[i] > rv)
      rv = c->buf[i];
  if (rv < 10)
    rv = 10;
  return rv;
}

///draw it
int
chartDraw (Chart * c)
{
  int i;
  int idx;

  if ((c->h < 4) || (c->w < 20))
    return 1;
  c->vmax = get_vmax (c);
  idx = c->cur_x % c->w;
  for (i = 0; i < c->w; i++)
  {
    int a = (c->h * c->buf[idx] / c->vmax);
    int b;
    if (a < 0)
      a = 0;
    b = c->h - a;
    mvvline (c->y, c->x + i, ' ', b);
    mvvline (c->y + b, c->x + i, ACS_BLOCK, a);
    idx = (idx + 1) % c->w;
  }
  mvprintw (c->y, c->x, "%d", c->vmax);
  return 0;
}

#ifdef TEST
int
main (int argc, char *argv[])
{
  int maxx, maxy;
  Chart ct;
  int v = 0;
  int c, running = 1;
  initscr ();
  start_color ();
  cbreak ();
  noecho ();
  nonl ();
  keypad (stdscr, TRUE);
  refresh ();
  getmaxyx (stdscr, maxy, maxx);
  chartInit (&ct, 0, 0, maxx, maxy);
  do
  {

    c = getch ();
    switch (c)
    {
    case 'q':
      running = 0;
      break;
    case KEY_RESIZE:
      getmaxyx (stdscr, maxy, maxx);
      chartResize (&ct, 0, 0, maxx, maxy);
      break;
    default:
      chartVal (&ct, v);
      chartDraw (&ct);
      v++;
      if (v >= 100)
        v = 0;
      break;
    }
  }
  while (running);
  endwin ();
  chartClear (&ct);
  return 0;
}
#endif
