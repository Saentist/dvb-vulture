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
#include "fav_list.h"
#include "fav.h"
#include "config.h"
#include "debug.h"
#include "tp_info.h"
#include "utl.h"
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

int DF_SW_MNU;
#define FILE_DBG DF_SW_MNU

int
sel_preset (uint32_t * num_ret, SwitchPos ** sp_ret, AppData * a)
{
  SwitchPos *np;
  uint32_t nn;
  MENU *my_menu;
  SwitchPos *sp = *sp_ret;
  uint32_t i, maxx;
  int state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  uint32_t num = *num_ret;

  message (a,
           "Select a preset using cursor keys and Enter, or cancel using Esc or Backspace.\n");

  while (state >= 0)
  {
    num_strings = SP_NUM_PRESETS;
    state = 0;
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    for (i = 0; i < num_strings; ++i)
    {
      my_items[i] = new_item (spGetPresetStrings (i), spGetPresetStrings (i));
      if (NULL == my_items[i])
      {
        if (errno == E_BAD_ARGUMENT)
          errMsg ("badarg i=%" PRIu32 " \n", i);
        if (errno == E_SYSTEM_ERROR)
          errMsg ("new_item: syserr i=%" PRIu32 "\n", i);
      }
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Presets") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Presets");
    wrefresh (menu_win (my_menu));
    debugMsg ("starting menu loop\n");
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
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 5;
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
      case 5:                  //selected
        np = spGetPreset (idx, &nn);
        if (NULL != np)
        {
          for (i = 0; i < num; i++)
          {
            spClear (sp + i);
          }
          *sp_ret = np;
          *num_ret = nn;
          return 0;
        }
        break;
      default:
        break;
      }
    }
  }
  //cancelled
  return 0;
}

int32_t
get_num (NOTUSED AppData * a)
{
  WINDOW *menu_win, *menu_sub;
  int maxx;
  const int BUFSZ = 32;
  char buf[BUFSZ];
  maxx = getmaxx (stdscr);
  menu_win = newwin (5, maxx - 1, 0, 1);
  menu_sub = derwin (menu_win, 1, maxx - 1 - 2, 3, 1);
  mvwprintw (menu_win, 1, 2, "%s", "Enter a number, please");
  box (menu_win, 0, 0);
  wrefresh (menu_win);
  nl ();
  nodelay (menu_sub, FALSE);
  echo ();
  nocbreak ();
  keypad (menu_sub, FALSE);
  mvwgetnstr (menu_sub, 0, 0, buf, BUFSZ);
  buf[BUFSZ - 1] = '\0';
  nonl ();
  noecho ();
  raw ();
  keypad (menu_sub, TRUE);
  return atoi (buf);
}

int
get_cmd_id (AppData * a)
{
  uint32_t i, maxx;
  int state = 0;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  MENU *my_menu;
  ITEM *my_items[7];
//      WINDOW * menu_win,*menu_sub;

  message (a, "Select a Cmd Id.\n");
  while (state >= 0)
  {
    num_strings = 6;
    state = 0;
    debugMsg ("allocating items\n");
    for (i = 0; i < num_strings; i++)
      my_items[i] =
        new_item ((char *) spGetCmdStr (i), (char *) spGetCmdStr (i));
    my_items[num_strings] = NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Cmd") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Cmd");
    wrefresh (menu_win (my_menu));
    debugMsg ("starting menu loop\n");
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        idx = -1;
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
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
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
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
    }
  }
  return idx;
}


int
edit_cmd (SwitchCmd * scmd, AppData * a)
{
  uint32_t i, maxx;
  int state = 0;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  MENU *my_menu;
  SwitchCmd tmp;
  ITEM *my_items[11];
  char *mnu_str[2][10];
//      WINDOW * menu_win,*menu_sub;

  tmp = *scmd;

  message (a, "Modify Command\n");

  while (state >= 0)
  {
    num_strings = 10;           //what,len,data[8]
    state = 0;
    debugMsg ("allocating items\n");

    mnu_str[0][0] = "Cmd";
    mnu_str[1][0] = (char *) spGetCmdStr (tmp.what);
    my_items[0] = new_item (mnu_str[0][0], mnu_str[1][0]);

    mnu_str[0][1] = "Len";
    mnu_str[1][1] = utlCalloc (32, sizeof (char));
    snprintf (mnu_str[1][1], 32, "%u", tmp.len);
    my_items[1] = new_item (mnu_str[0][1], mnu_str[1][1]);

    for (i = 2; i < num_strings; i++)
    {
      mnu_str[0][i] = utlCalloc (32, sizeof (char));
      snprintf (mnu_str[0][i], 32, "data[%" PRIu32 "]", i - 2);
      mnu_str[1][i] = utlCalloc (32, sizeof (char));
      snprintf (mnu_str[1][i], 32, "%u", tmp.data[i - 2]);
      my_items[i] = new_item (mnu_str[0][i], mnu_str[1][i]);
    }

    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu,
                   O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR | O_SHOWDESC);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Cmd") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Cmd");
    wrefresh (menu_win (my_menu));
    debugMsg ("starting menu loop\n");
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
        state = 3;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 5;
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
    utlFAN (mnu_str[1][1]);
    for (i = 2; i < num_strings; ++i)
    {
      utlFAN (mnu_str[0][i]);
      utlFAN (mnu_str[1][i]);
    }
    if (state > 0)
    {
      int rv;
      switch (state)
      {
      case 3:                  //cancel
        return 1;
      case 5:                  //edit
        switch (idx)
        {
        case 0:
          rv = get_cmd_id (a);
          if (rv >= 0)
            tmp.what = rv;
          break;
        case 1:
          tmp.len = get_num (a);        //1,29,a);
          break;
        default:
          tmp.data[idx - 2] = get_num (a);      //idx,29,a);
          break;
        }
        break;
      default:
        break;
      }
    }
  }
  *scmd = tmp;                  //replace with temporary
  return 0;
}

int
get_pol (AppData * a)
{
  uint32_t i, maxx;
  int state = 0;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  MENU *my_menu;
  ITEM *my_items[5];
//      WINDOW * menu_win,*menu_sub;

  message (a, "Select a Polarisation.\n");
  while (state >= 0)
  {
    num_strings = 4;
    state = 0;
    debugMsg ("allocating items\n");
    my_items[0] = new_item ("H", "H");
    my_items[1] = new_item ("V", "V");
    my_items[2] = new_item ("L", "L");
    my_items[3] = new_item ("R", "R");
    my_items[4] = NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Pol") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Pol");
    wrefresh (menu_win (my_menu));
    debugMsg ("starting menu loop\n");
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = getch ();
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
        idx = -1;
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
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
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
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
    }
  }
  return idx;
}

int
edit_pol (SwitchPol * spol, AppData * a)
{
  uint32_t i, maxx;
  int state = 0;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  MENU *my_menu;
  SwitchPol tmp;
  SwitchCmd *scmd;
  ITEM **my_items;
  char **mnu_str[2];
//      WINDOW * menu_win,*menu_sub;
  uint32_t num;

  spPolDeepCopy (&tmp, spol);
  num = tmp.num_cmds;
  scmd = tmp.cmds;

  message (a,
           "Modify Polarisation parameters or edit Commands.\nPress a to add a command, d to delete, press c to cancel");
  while (state >= 0)
  {
    num_strings = num + 1;
    state = 0;
    debugMsg ("allocating items\n");

    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));

    mnu_str[0] = utlCalloc (num_strings, sizeof (char *));
    mnu_str[1] = utlCalloc (num_strings, sizeof (char *));

    debugMsg ("allocating items\n");
    mnu_str[0][0] = "Pol";
    mnu_str[1][0] = (char *) tpiGetPolStr (tmp.pol);
    my_items[0] = new_item (mnu_str[0][0], mnu_str[1][0]);

    for (i = 1; i < num_strings; ++i)
    {
      mnu_str[0][i] = utlCalloc (32, sizeof (char));
      mnu_str[1][i] = (char *) spGetCmdStr (scmd[i - 1].what);
      snprintf (mnu_str[0][i], 32, "Cmd%" PRIu32, i - 1);
      my_items[i] = new_item (mnu_str[0][i], mnu_str[1][i]);
      if (NULL == my_items[i])
      {
        if (errno == E_BAD_ARGUMENT)
          errMsg ("badarg i=%" PRIu32 "\n", i);
        if (errno == E_SYSTEM_ERROR)
          errMsg ("new_item: syserr i=%" PRIu32 "\n", i);
      }
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu,
                   O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR | O_SHOWDESC);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Pol") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Pol");
    wrefresh (menu_win (my_menu));
    debugMsg ("starting menu loop\n");
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
      case 'd':
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 1;
        break;
      case 'a':
        state = 2;
        break;
      case 'c':
        state = 3;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 5;
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
    for (i = 1; i < num_strings; ++i)
      utlFAN (mnu_str[0][i]);
    utlFAN (mnu_str[0]);
    utlFAN (mnu_str[1]);
    utlFAN (my_items);
    if (state > 0)
    {
      switch (state)
      {
      case 1:                  //del
        if (idx >= 1)
          spCmdDel (&scmd, &num, idx - 1);
        break;
      case 2:                  //add(at the end)
        spCmdAdd (&scmd, &num, num);
        break;
      case 3:                  //cancel
        spPolClear (&tmp);      //clear temporary
        return 1;
      case 5:                  //edit
        if (idx >= 1)
          edit_cmd (scmd + idx - 1, a);
        else
        {
          int rv;
          rv = get_pol (a);
          if (rv >= 0)
            tmp.pol = rv;
        }
        break;
      default:
        break;
      }
    }
  }
  spPolClear (spol);            //clear original
  *spol = tmp;                  //replace with temporary
  return 0;
}

int
edit_band (SwitchBand * sb, AppData * a)
{
  uint32_t i, maxx;
  int state = 0;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  MENU *my_menu;
  SwitchBand tmp;
  SwitchPol *spol;
  ITEM **my_items;
  char **mnu_str[2];
//      WINDOW * menu_win,*menu_sub;
  uint32_t num;

  spBandDeepCopy (&tmp, sb);
  num = tmp.num_pol;
  spol = tmp.pol;

  message (a, "Modify Band parameters or edit Band's polarisations.\n");

  while (state >= 0)
  {
    num_strings = num + 3;
    state = 0;
    debugMsg ("allocating items\n");

    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));

    mnu_str[0] = utlCalloc (num_strings, sizeof (char *));
    mnu_str[1] = utlCalloc (num_strings, sizeof (char *));

    debugMsg ("allocating items\n");
    mnu_str[0][0] = "lof";
    mnu_str[1][0] = utlCalloc (32, sizeof (char));
    snprintf (mnu_str[1][0], 32, "%u", sb->lof);
    my_items[0] = new_item (mnu_str[0][0], mnu_str[1][0]);

    mnu_str[0][1] = "f_min";
    mnu_str[1][1] = utlCalloc (32, sizeof (char));
    snprintf (mnu_str[1][1], 32, "%u", sb->f_min);
    my_items[1] = new_item (mnu_str[0][1], mnu_str[1][1]);

    mnu_str[0][2] = "f_max";
    mnu_str[1][2] = utlCalloc (32, sizeof (char));
    snprintf (mnu_str[1][2], 32, "%u", sb->f_max);
    my_items[2] = new_item (mnu_str[0][2], mnu_str[1][2]);

    for (i = 3; i < num_strings; ++i)
    {
      mnu_str[0][i] = utlCalloc (32, sizeof (char));
      mnu_str[1][i] = (char *) tpiGetPolStr (sb->pol[i - 3].pol);
      snprintf (mnu_str[0][i], 32, "Pol%" PRIu32, i - 3);
      my_items[i] = new_item (mnu_str[0][i], mnu_str[1][i]);
      if (NULL == my_items[i])
      {
        if (errno == E_BAD_ARGUMENT)
          errMsg ("badarg i=%" PRIu32 "\n", i);
        if (errno == E_SYSTEM_ERROR)
          errMsg ("new_item: syserr i=%" PRIu32 "\n", i);
      }
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu,
                   O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR | O_SHOWDESC);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Band") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Band");
    wrefresh (menu_win (my_menu));
    debugMsg ("starting menu loop\n");
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
      case 'd':
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 1;
        break;
      case 'a':
        state = 2;
        break;
      case 'c':
        state = 3;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 5;
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
    utlFAN (mnu_str[1][0]);
    utlFAN (mnu_str[1][1]);
    utlFAN (mnu_str[1][2]);
    for (i = 3; i < num_strings; ++i)
      utlFAN (mnu_str[0][i]);
    utlFAN (mnu_str[0]);
    utlFAN (mnu_str[1]);
    utlFAN (my_items);
    if (state > 0)
    {
      switch (state)
      {
      case 1:                  //del
        if (idx >= 3)
          spPolDel (&spol, &num, idx - 3);
        break;
      case 2:                  //add(at the end)
        spPolAdd (&spol, &num, num);
        break;
      case 3:                  //cancel
        spBandClear (&tmp);     //clear temporary
        return 1;
      case 5:                  //edit
        if (idx >= 3)
          edit_pol (spol + idx - 3, a);
        else
          switch (idx)
          {
          case 0:
            tmp.lof = get_num (a);      //3,29,a);
            break;
          case 1:
            tmp.f_min = get_num (a);    //4,29,a);
            break;
          case 2:
            tmp.f_max = get_num (a);    //5,29,a);
            break;
          }
        break;
      default:
        break;
      }
    }
  }
  spBandClear (sb);             //clear original
  *sb = tmp;                    //replace with temporary
  return 0;
}

int
edit_bands (SwitchPos * sp, AppData * a)
{
  MENU *my_menu;
  SwitchBand *sb;
  SwitchPos tmp;
  uint32_t i, maxx;
  int state = 0;
  ITEM **my_items;
  char **mnu_str;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  uint32_t num;
  spDeepCopy (&tmp, sp);
  num = tmp.num_bands;
  sb = tmp.bands;

  message (a,
           "Select and press enter to edit. Use d to delete a frequency band or a to add a new one. Use c to cancel and backspace to exit.\n");

  while (state >= 0)
  {
    num_strings = num;
    state = 0;
    maxx = getmaxx (stdscr);
    if (num)
    {

      debugMsg ("allocating items\n");
      my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
      mnu_str = utlCalloc (num_strings, sizeof (char *));
      for (i = 0; i < num_strings; ++i)
      {
        mnu_str[i] = utlCalloc (20, sizeof (char));
        snprintf (mnu_str[i], 20, "%" PRIu32, i);
        my_items[i] = new_item (mnu_str[i], mnu_str[i]);
        if (NULL == my_items[i])
        {
          if (errno == E_BAD_ARGUMENT)
            errMsg ("badarg i=%" PRIu32 "\n", i);
          if (errno == E_SYSTEM_ERROR)
            errMsg ("new_item: syserr i=%" PRIu32 "\n", i);
        }
      }
      my_items[num_strings] = (ITEM *) NULL;
      my_menu = new_menu ((ITEM **) my_items);
      set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
      appDataMakeMenuWnd (my_menu);
      lbl_x = 3 + (maxx - 6) / 2 - strlen ("Bands") / 2;
      mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Bands");
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
        case KEY_RESIZE:
          //windows need to be resized.. ignore
          break;
        case 'd':
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 1;
          break;
        case 'a':
          state = 2;
          break;
        case 'c':
          state = 3;
          break;
        case 13:               //enter
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 5;
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
        utlFAN (mnu_str[i]);
      }
      utlFAN (mnu_str);
      utlFAN (my_items);
    }
    else
    {
      WINDOW *wnd;
      wnd = appDataMakeEmptyWnd ();

      wrefresh (wnd);
      while (state == 0)
      {
        int c;
        c = getch ();
        switch (c)
        {
        case KEY_RESIZE:
          wnd = appDataResizeEmpty (wnd);
          break;
        case 263:              //ESC
        case K_BACK:           //Backspace
          state = -1;
          break;
        case 'a':
          state = 2;
          break;
        case 'c':
          state = 3;
          break;
        default:
          break;
        }
        wrefresh (wnd);
      }
      assert (ERR != delwin (wnd));
    }
    if (state > 0)
    {
      switch (state)
      {
      case 1:                  //del
        spBandDel (&sb, &num, idx);
        break;
      case 2:                  //add(at the end)
        spBandAdd (&sb, &num, num);
        break;
      case 3:                  //cancel
        spClear (&tmp);         //clear temporary
        return 1;
      case 5:                  //edit
        edit_band (sb + idx, a);
        break;
      default:
        break;
      }
    }
  }
  spClear (sp);                 //clear original
  *sp = tmp;                    //replace with temporary
  return 0;
}

int
set_itf (SwitchPos * sp, AppData * a)
{
  WINDOW *menu_win, *menu_sub;
  int maxx;
  const int BUFSZ = 1024;
  char *newitf;
  maxx = getmaxx (stdscr);
  menu_win = newwin (5, maxx, 0, 0);
  menu_sub = derwin (menu_win, 1, maxx - 2, 3, 1);
  mvwprintw (menu_win, 1, 2, "%s", "Enter a filename to use, please");
  box (menu_win, 0, 0);
  wrefresh (menu_win);
//  sp->initial_tuning_file
  newitf = utlCalloc (BUFSZ, sizeof (char));
  if (NULL == newitf)
  {
    errMsg ("allocation failed\n");
    return 1;
  }
//      def_prog_mode();
//      setsyx(4,20+2);
  nl ();
  nodelay (menu_sub, FALSE);
  echo ();
  nocbreak ();
  keypad (menu_sub, FALSE);
  mvwgetnstr (menu_sub, 0, 0, newitf, BUFSZ);
  newitf[BUFSZ - 1] = '\0';
  newitf = utlRealloc (newitf, strlen (newitf) + 1);
  nonl ();
  noecho ();
  raw ();
  keypad (menu_sub, TRUE);
//      reset_prog_mode();
  message (a, "Using file: %s, thanks.\n", newitf);
  if (sp->initial_tuning_file)
    utlFAN (sp->initial_tuning_file);
  sp->initial_tuning_file = newitf;
  return 0;
}

int
edit_pos (SwitchPos * sp, AppData * a)
{
  MENU *my_menu;
  uint32_t i, maxx;
  int state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx;

  message (a,
           "Here you can set the initial tuning file or edit the frequency bands.\n");
  while (state >= 0)
  {
    num_strings = 2;
    state = 0;
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    my_items[0] = new_item ("Bands", "Bands");
    my_items[1] = new_item ("ITF", "ITF");
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Position") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Position");
    wrefresh (menu_win (my_menu));
    debugMsg ("starting menu loop\n");
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
      case 13:                 //enter
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 5;
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
    if (state == 5)
    {
      switch (idx)
      {
      case 0:
        edit_bands (sp, a);
        break;
      case 1:
        set_itf (sp, a);
        break;
      default:
        break;
      }
    }
  }
  return 0;
}

int
edit_posns (uint32_t * num_ret, SwitchPos ** sp_ret, AppData * a)
{
  MENU *my_menu;
  SwitchPos *sp = *sp_ret;
  uint32_t i, maxx;
  int state = 0;
  ITEM **my_items;
  char **mnu_str;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;
  uint32_t num = *num_ret;

  message (a,
           "Select and press enter to edit. Use d to delete a position or a to add a new one. Use c to cancel and backspace to exit(and store!). Press p to select a preset configuration.\n");

  while (state >= 0)
  {
    num_strings = num;
    state = 0;
    maxx = getmaxx (stdscr);
    if (num)
    {

      debugMsg ("allocating items\n");
      my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
      mnu_str = utlCalloc (num_strings, sizeof (char *));
      for (i = 0; i < num_strings; ++i)
      {
        mnu_str[i] = utlCalloc (20, sizeof (char));
        snprintf (mnu_str[i], 20, "%" PRIu32, i);
        my_items[i] = new_item (mnu_str[i], mnu_str[i]);
        if (NULL == my_items[i])
        {
          if (errno == E_BAD_ARGUMENT)
            errMsg ("badarg i=%" PRIu32 "\n", i);
          if (errno == E_SYSTEM_ERROR)
            errMsg ("new_item: syserr i=%" PRIu32 "\n", i);
        }
      }
      my_items[num_strings] = (ITEM *) NULL;
      my_menu = new_menu ((ITEM **) my_items);
      set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
      appDataMakeMenuWnd (my_menu);
      lbl_x = 3 + (maxx - 6) / 2 - strlen ("Positions") / 2;
      mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Positions");
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
        case KEY_RESIZE:
          //windows need to be resized.. ignore
          break;
        case 'd':
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 1;
          break;
        case 'a':
          state = 2;
          break;
        case 'c':
          state = 3;
          break;
        case 'p':
          state = 4;
          break;
        case 13:               //enter
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 5;
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
        utlFAN (mnu_str[i]);
      }
      utlFAN (mnu_str);
      utlFAN (my_items);
    }
    else
    {
      WINDOW *wnd;
      wnd = appDataMakeEmptyWnd ();
      wrefresh (wnd);
      while (state == 0)
      {
        int c;
        c = getch ();
        switch (c)
        {
        case KEY_RESIZE:
          wnd = appDataResizeEmpty (wnd);
          break;
        case 263:              //ESC
        case K_BACK:           //Backspace
          state = -1;
          break;
        case 'a':
          state = 2;
          break;
        case 'c':
          state = 3;
          break;
        case 'p':
          state = 4;
          break;
        default:
          break;
        }
        wrefresh (wnd);
      }
      assert (ERR != delwin (wnd));
    }
    if (state > 0)
    {
      switch (state)
      {
      case 1:                  //del
        spDel (&sp, &num, idx);
        break;
      case 2:                  //add(at the end)
        spAdd (&sp, &num, num);
        break;
      case 3:                  //cancel
        *sp_ret = sp;
        *num_ret = num;
        return 1;
      case 4:                  //preset
        sel_preset (&num, &sp, a);
        break;
      case 5:                  //edit
        edit_pos (sp + idx, a);
        break;
      default:
        break;
      }
    }
  }
  *sp_ret = sp;
  *num_ret = num;
  return 0;
}
