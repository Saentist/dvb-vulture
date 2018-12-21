#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "wordexp_loc.h"
#include <errno.h>
#include "socket_loc.h"
#include <unistd.h>
#include <ncurses.h>
#include <assert.h>
#include <inttypes.h>

#include "app_data.h"
#include "utl.h"
#include "fav_list.h"
#include "fav.h"
#include "config.h"
#include "debug.h"
#include "tp_info.h"
#include "dmalloc_f.h"
#include "pgm_info.h"
#include "list.h"
#include "in_out.h"
#include "iterator.h"
#include "dmalloc_f.h"
#include "menu_strings.h"
#include "timestamp.h"
#include "vt_pk.h"
#include "vtctx.h"
#include "switch.h"
#include "cwrap.h"

int DF_REC_MNU;
#define FILE_DBG DF_REC_MNU

void
select_re_type (RSType * type /*RsEntry *e */ , AppData * a)
{
  MENU *my_menu;
  uint32_t i, maxx;
  int state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;

  message (a, "Press enter to select. Use c or backspace to cancel.\n");
  while (state >= 0)
  {
    num_strings = 7;
    state = 0;
    maxx = getmaxx (stdscr);
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    for (i = 0; i < num_strings; i++)
    {
      my_items[i] = new_item (rsGetTypeStr (i), rsGetTypeStr (i));
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_NONCYCLIC | O_ONEVALUE | O_ROWMAJOR);
    appDataMakeMenuWnd (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Type") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Type");
    post_menu (my_menu);
    set_current_itemXX (my_menu, my_items[*type]);
    debugMsg ("starting menu loop\n");
    wrefresh (menu_win (my_menu));
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        state = -1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        //windows need to be resized.. ignore
        appDataResizeMnu (my_menu);
        break;
      case 'c':
        state = -1;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 2;
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
    }
    utlFAN (my_items);
    if (state > 0)
    {
      *type = idx;
      return;
    }
  }
  return;
}

void
select_bits (uint8_t * flags, char const *(*get_string) (uint8_t flag),
             uint8_t num_strings, AppData * a)
{
  MENU *my_menu;
  uint32_t i, maxx;
  int state = 0;
  uint8_t wd = *flags;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  int lbl_x, idx;

  message (a,
           "Press enter to toggle. Use c to cancel or backspace to exit.\n");
  while (state >= 0)
  {
    state = 0;
    maxx = getmaxx (stdscr);
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    for (i = 0; i < num_strings; i++)
    {
      my_items[i] = new_item (get_string (i), get_string (i));
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_NONCYCLIC | O_ROWMAJOR);
    appDataMakeMenuWnd (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Bits") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Bits");
    post_menu (my_menu);
    debugMsg ("starting menu loop\n");
    for (i = 0; i < num_strings; i++)
    {
      set_item_value (my_items[i], wd & (1 << i));
    }
    wrefresh (menu_win (my_menu));
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        state = 2;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        //windows need to be resized.. ignore
        break;
      case 'c':
        state = -1;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        set_item_value (selection, !item_value (selection));
        idx = item_index (selection);
        wd = (wd) ^ (1 << idx);
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
    }
    utlFAN (my_items);
    if (state > 0)
    {
      *flags = wd;
      return;
    }
  }
  return;
}

void
select_re_flags (uint8_t * flags, AppData * a)
{
  select_bits (flags, rsGetFlagStr, 5, a);
}

void
select_re_days (uint8_t * days, AppData * a)
{
  select_bits (days, rsGetWdStr, 7, a);
}

void
get_re_state (RsEntry * e, AppData * a)
{
  MENU *my_menu;
  uint32_t i, maxx;
  int state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;

  message (a, "Press enter to select. Use c or backspace to cancel.\n");
  while (state >= 0)
  {
    num_strings = 2;
    state = 0;
    maxx = getmaxx (stdscr);
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    my_items[0] = new_item (rsGetStateStr (RS_IDLE), rsGetStateStr (RS_IDLE));
    my_items[1] =
      new_item (rsGetStateStr (RS_WAITING), rsGetStateStr (RS_WAITING));
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_NONCYCLIC | O_ONEVALUE | O_ROWMAJOR);
    appDataMakeMenuWnd (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("State") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "State");
    post_menu (my_menu);
    debugMsg ("starting menu loop\n");
    wrefresh (menu_win (my_menu));
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        state = -1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        //windows need to be resized.. ignore
        break;
      case 'c':
        state = -1;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 2;
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
    }
    utlFAN (my_items);
    if (state > 0)
    {
      e->state = idx;
      return;
    }
  }
  return;
}


void
re_show_full_list (uint32_t pos, RsEntry * e, AppData * a)
{
  MENU *my_menu = NULL;
  uint32_t i;
  int maxx, state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  char **tp_strings;
  char **pgm_strings;
  TransponderInfo *tp;
  ProgramInfo *pgms;
  uint32_t num_tp, num_pgm;
  uint32_t tp_idx = 0;
  int idx;
  tp = cwGetTransponders (a->sock, pos, &num_tp);
  if (!tp)
    return;
  debugMsg ("generating strings: %" PRIu32 "\n", num_tp);
  tp_strings = tp_to_strings (tp, num_tp);

  message (a,
           "Select entries using up/down cursor keys. Select Transponders using left/right keys. Press Return to choose. Press Escape or Backspace to cancel.\n");
  while (state != -1)
  {
    if (state == 1)
    {
      if (tp_idx > 0)
        tp_idx--;
    }
    if (state == 2)
    {
      tp_idx++;
      if (tp_idx >= num_tp)
        tp_idx = num_tp - 1;
    }
    state = 0;
    pgms =
      cwGetPgms (a->sock, pos, tp[tp_idx].frequency, tp[tp_idx].pol,
                 &num_pgm);
    if (pgms)
    {
      int lbl_x;
      pgm_strings = pgms_to_strings (pgms, num_pgm);
      debugMsg ("allocating items\n");
      my_items = (ITEM **) utlCalloc (num_pgm + 1, sizeof (ITEM *));
      for (i = 0; i < num_pgm; ++i)
      {
        my_items[i] = new_item (pgm_strings[i], pgm_strings[i]);
        if (NULL == my_items[i])
        {
          if (errno == E_BAD_ARGUMENT)
            errMsg ("badarg i=%" PRIu32 " string=%s\n", i, pgm_strings[i]);
          if (errno == E_SYSTEM_ERROR)
            errMsg ("new_item: syserr i=%" PRIu32 " string=%s\n", i,
                    pgm_strings[i]);
        }
      }
      my_items[num_pgm] = (ITEM *) NULL;
      my_menu = new_menu ((ITEM **) my_items);
      set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
      maxx = getmaxx (stdscr);
      appDataMakeMenuWnd (my_menu);
      post_menu (my_menu);
      lbl_x = 3 + (maxx - 6) / 2 - strlen (tp_strings[tp_idx]) / 2;
      if (tp_idx > 0)
        mvwaddch (menu_win (my_menu), 1, 1, '<');
      if (tp_idx < (num_tp - 1))
        mvwaddch (menu_win (my_menu), 1, maxx - 1 - 1, '>');
      mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", tp_strings[tp_idx]);
      wrefresh (menu_win (my_menu));

      debugMsg ("starting menu loop\n");
      while (state == 0)
      {
        int c;
        ITEM *selection;
        c = getch ();
        mvprintw (0, 0, "%d\n", c);
        switch (c)
        {
        case 263:              //ESC
        case K_BACK:           //Backspace
          state = -1;
          break;
        case KEY_DOWN:
          menu_driver (my_menu, REQ_DOWN_ITEM);
          break;
        case KEY_UP:
          menu_driver (my_menu, REQ_UP_ITEM);
          break;
        case KEY_LEFT:
          state = 1;
          break;
        case KEY_RIGHT:
          state = 2;
          break;
        case KEY_RESIZE:
          //windows need to be resized.. ignore
          break;
        case 13:
          //choose
          selection = current_item (my_menu);
          idx = item_index (selection);
          e->pos = pos;
          e->pol = tp[tp_idx].pol;
          e->freq = tp[tp_idx].frequency;
          e->pnr = pgms[idx].pnr;
          state = -1;
          break;
        default:
          break;
        }
        wrefresh (menu_win (my_menu));
      }
      unpost_menu (my_menu);
      appDataDestroyMenuWnd (my_menu);
      free_menu (my_menu);
      for (i = 0; i < num_pgm; ++i)
      {
        free_item (my_items[i]);
      }
      utlFAN (my_items);
      for (i = 0; i < num_pgm; ++i)
      {
        utlFAN (pgm_strings[i]);
        programInfoClear (&pgms[i]);
      }
      utlFAN (pgm_strings);
      utlFAN (pgms);

    }
    else
    {
      int lbl_x;
      WINDOW *wnd;
      maxx = getmaxx (stdscr);
      wnd = appDataMakeEmptyWnd ();
      if (!wnd)
        errMsg ("newwin failed\n");
      lbl_x = 3 + (maxx - 6) / 2 - strlen (tp_strings[tp_idx]) / 2;
      if (tp_idx > 0)
        mvwaddch (wnd, 1, 1, '<');
      if (tp_idx < (num_tp - 1))
        mvwaddch (wnd, 1, maxx - 1 - 1, '>');
      if (mvwprintw (wnd, 1, lbl_x, "%s", (tp_strings[tp_idx])))
        errMsg ("printw failed\n");
      debugMsg (tp_strings[tp_idx]);
      wrefresh (wnd);

      while (state == 0)
      {
        int c;
        c = getch ();
        mvprintw (0, 0, "%d\n", c);
        switch (c)
        {
        case KEY_RESIZE:
          wnd = appDataResizeEmpty (wnd);
          wrefresh (wnd);
          break;
        case 263:              //ESC
        case K_BACK:           //Backspace
          state = -1;
          break;
        case KEY_LEFT:
          state = 1;
          break;
        case KEY_RIGHT:
          state = 2;
          break;
        default:
          break;
        }
      }
      assert (ERR != delwin (wnd));     //menu_win(my_menu));
    }
  }
  free_strings (tp_strings, num_tp);
  utlFAN (tp);
}

void
re_show_pos (RsEntry * e, AppData * a)
{
  MENU *my_menu;
  uint32_t i, idx;
  int maxx, state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_pos = cwListPos (a->sock);       //list_pos(a)
  idx = 0;
  if (!num_pos)
    return;

  message (a,
           "Select Satellite position to use, please. Escape or Backspace cancels.\n");
  while (state != -1)
  {
    int lbl_x;
    state = 0;
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_pos + 1, sizeof (ITEM *));
    for (i = 0; i < num_pos; ++i)
    {
      char *str;
      str = utlCalloc (20, sizeof (char));
      snprintf (str, 20, "%" PRIu32, i);
      my_items[i] = new_item (str, str);
      if (NULL == my_items[i])
      {
        if (errno == E_BAD_ARGUMENT)
          errMsg ("badarg i=%" PRIu32 " string=%s\n", i, str);
        if (errno == E_SYSTEM_ERROR)
          errMsg ("new_item: syserr i=%" PRIu32 " string=%s\n", i, str);
      }
    }
    my_items[num_pos] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Pos") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Pos");
    wrefresh (menu_win (my_menu));

    debugMsg ("starting menu loop\n");
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      mvprintw (0, 0, "%d\n", c);
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        state = -1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        //windows need to be resized.. ignore
        break;
      case 13:
        //choose
        state = 1;
        selection = current_item (my_menu);
        idx = item_index (selection);
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    for (i = 0; i < num_pos; ++i)
    {
      char *str = (char *) item_name (my_items[i]);
      free_item (my_items[i]);
      utlFAN (str);
    }
    utlFAN (my_items);
    if (state == 1)
    {
      re_show_full_list (idx, e, a);
      return;
    }
  }
}


void
select_re_pgm (RsEntry * e, AppData * a)
{
  re_show_pos (e, a);
}

extern int32_t get_num (AppData * a);


void
get_re_start (RsEntry * e, AppData * a)
{
  MENU *my_menu;
  uint32_t i, maxx;
  int state = 0;
  char tbuf[6][40];
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings, lbl_x, idx = 0;
  struct tm start_time;

  message (a,
           "Select and press enter to edit. Use c to cancel and backspace to exit.\n");
  localtime_r (&e->start_time, &start_time);
  while (state >= 0)
  {
    num_strings = 6;
    state = 0;
    maxx = getmaxx (stdscr);
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    snprintf (tbuf[0], 40, "%d\n", start_time.tm_sec);
    snprintf (tbuf[1], 40, "%d\n", start_time.tm_min);
    snprintf (tbuf[2], 40, "%d\n", start_time.tm_hour);
    snprintf (tbuf[3], 40, "%d\n", start_time.tm_mday);
    snprintf (tbuf[4], 40, "%d\n", start_time.tm_mon + 1);
    snprintf (tbuf[5], 40, "%d\n", start_time.tm_year + 1900);
    my_items[0] = new_item ("Sec", tbuf[0]);
    my_items[1] = new_item ("Min", tbuf[1]);
    my_items[2] = new_item ("Hour", tbuf[2]);
    my_items[3] = new_item ("MDay", tbuf[3]);
    my_items[4] = new_item ("Mon", tbuf[4]);
    my_items[5] = new_item ("Year", tbuf[5]);
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_NONCYCLIC | O_ROWMAJOR);
    appDataMakeMenuWnd (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Start Time") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Start Time");
    post_menu (my_menu);
    debugMsg ("starting menu loop\n");
    wrefresh (menu_win (my_menu));
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        state = -1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        //windows need to be resized.. ignore
        break;
      case 'c':
        state = 1;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 2;
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
    }
    utlFAN (my_items);
    if (state > 0)
    {
      switch (state)
      {
      case 1:                  //cancel
        return;
      case 2:                  //edit
        switch (idx)
        {
        case 0:
          start_time.tm_sec = get_num (a);
          break;
        case 1:
          start_time.tm_min = get_num (a);
          break;
        case 2:
          start_time.tm_hour = get_num (a);
          break;
        case 3:
          start_time.tm_mday = get_num (a);
          break;
        case 4:
          start_time.tm_mon = get_num (a) - 1;
          break;
        case 5:
          start_time.tm_year = get_num (a) - 1900;
          break;
        default:
          break;
        }
        break;
      default:
        break;
      }
    }
  }
  //should I tamper with tm_isdst here?
  e->start_time = mktime (&start_time);
  return;
}

void
get_re_duration (RsEntry * e, AppData * a)
{
  e->duration = get_num (a) * 60;
}

int
edit_rse (RsEntry * e, AppData * a)
{
  MENU *my_menu;
  uint32_t i, maxx;
  int state = 0;
  char tbuf[40];
  char dbuf[40];
  RsEntry tmpe = *e;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;

  message (a,
           "Select and press enter to edit. Use c to cancel and backspace to exit(and save changes).\n");

  while (state >= 0)
  {
    num_strings = 7;
    state = 0;
    maxx = getmaxx (stdscr);
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    my_items[0] = new_item ("Type", rsGetTypeStr (tmpe.type));
    my_items[1] = new_item ("Days", "");
    my_items[2] = new_item ("Flags", "");
    my_items[3] = new_item ("Pgm", "");
    my_items[4] = new_item ("Start", ctime_r (&tmpe.start_time, tbuf));
    snprintf (dbuf, 40, "%umin\n", (unsigned) tmpe.duration / 60);
    my_items[5] = new_item ("Duration", dbuf);
    my_items[6] = new_item ("State", rsGetStateStr (tmpe.state));
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_NONCYCLIC | O_ROWMAJOR);
    appDataMakeMenuWnd (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Entry") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Entry");
    post_menu (my_menu);
    debugMsg ("starting menu loop\n");
    wrefresh (menu_win (my_menu));
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        state = -1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        //windows need to be resized.. ignore
        break;
      case 'c':
        state = 1;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 2;
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
    }
    utlFAN (my_items);
    if (state > 0)
    {
      switch (state)
      {
      case 1:                  //cancel
        return 1;
      case 2:                  //edit
        switch (idx)
        {
        case 0:
          select_re_type (&tmpe.type, a);
          break;
        case 1:
          select_re_days (&tmpe.wd, a);
          break;
        case 2:
          select_re_flags (&tmpe.flags, a);     //sthg wrong here.. flags observed leaking into nb entries
          break;
        case 3:
          select_re_pgm (&tmpe, a);
          break;
        case 4:
          get_re_start (&tmpe, a);
          break;
        case 5:
          get_re_duration (&tmpe, a);
          break;
        case 6:
          get_re_state (&tmpe, a);
          break;
        default:
          break;
        }
//                      edit_pos(sp+idx,a);
        break;
      default:
        break;
      }
    }
  }
  *e = tmpe;
  return 0;
}
