#ifndef __epg_ui_h
#define __epg_ui_h

#include <stdint.h>
#include "dmalloc_loc.h"
#include "rcfile.h"
#include "fav.h"
#include "epg_evt.h"

/**
 *\file epg_ui.h
 *\brief shows a two dimensional Programme Guide
 *Vertical layout is not too precise.
 */

typedef struct
{
  int start_line;
  int end_line;
  uint32_t ev_idx;
  time_t start_time;
  time_t duration;
} EpgVEntry;

typedef struct
{
  unsigned num_entries;
  EpgVEntry *entries;
  int *idx_lut;                 ///<maps pad y coordinates to event indices
  int now_on;                   ///< UiUpdate looks at event with this index to progress, will increment to next if current becomes past.
} EpgVCol;

typedef struct
{
  unsigned num_columns;
  char **pgm_names;
  EpgSchedule *sched;
  EpgVCol *cols;
  int nlines;
  int vspc;                     ///<minimum vertical number of cells per entry
  int x, y, w, h, col_w;
  int cur_y, cur_x;
  int nav_y;                    ///<y index of nav position in cell coordinates, e_pad relative. Gets set on up/down request to mid of entry or 0. Used when left/right navigating.
  unsigned sel_col;             ///<selected ui column
  unsigned sel_idx;             ///<selected item index in column
  WINDOW *sel_pad, *e_p, *s_p, *h_p;    ///<pad holding description for current selection, epg,timescale,header
  unsigned sel_line;            ///<how far we've scrolled the description, gets reset when navigating
  short colors[11][2];
} EpgView;

///will store pointers to sched, pgm_names. do not free before epgUiClear. There must not be empty schedules or schedule columns.
void epgUiInit (EpgView * ev, RcFile * cfg, EpgSchedule * sched, char **pgm_names, uint32_t sched_sz, int x, int y, int w, int h);      //,int col_w,int vspc);
///clears datastructure. won't free pgm_names/sched
void epgUiClear (EpgView * ev);
///display the program guide on screen
void epgUiDraw (EpgView * ev);
///epg navigation requests
typedef enum
{
  NAV_L,                        ///<column left of current
  NAV_R,                        ///<column to the right
  NAV_U,                        ///<previous item
  NAV_D,                        ///<next item in current column
  NAV_IFIRST,                   ///<goto first of column
  NAV_ILAST,                    ///<goto last item in current column
  SCR_U,                        ///<scroll description up one line
  SCR_D                         ///<scroll description down one line
} EpgReq;
///navigate the epg
void epgUiAction (EpgView * ev, EpgReq which);
///change viewport position/dimensions
void epgUiChange (EpgView * ev, int x, int y, int w, int h);
///get indices of currently hilighted item
void epgUiGetPos (EpgView * ev, unsigned *col_ret, unsigned *idx_ret);
///update progress indicators. call once in a while
void epgUiUpdate (EpgView * ev);
#endif //__epg_ui_h
