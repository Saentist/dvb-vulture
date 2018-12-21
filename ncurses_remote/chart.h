#ifndef __chart_h
#define __chart_h
#include <ncurses.h>
/**
 *\file chart.h
 *\brief draws a stripchart
 */
typedef struct
{
  int x, y, w, h;
  int cur_x;                    ///<rightmost valid value
  int *buf;
  int vmax;                     ///<vertical scale max
} Chart;
///initialize chart
int chartInit (Chart * c, int x, int y, int w, int h);
///clear chart
void chartClear (Chart * c);
///change size
int chartResize (Chart * c, int x, int y, int w, int h);
///add a value to draw
int chartVal (Chart * c, int v);
///draw it
int chartDraw (Chart * c);

#endif
