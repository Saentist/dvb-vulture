#ifndef __app_data_h
#define __app_data_h
#include <ncurses.h>
#include <menu.h>
#include "rcfile.h"
#include "btre.h"
#include "custck.h"
#include "fav_list.h"
#include "list.h"

typedef struct
{
  RcFile cfg;                   ///<the configuration file
  int sock;                     ///<communication socket
  BTreeNode *favourite_lists;   ///<fav lists sorted by favlist name, has ownership of lists
  FavList *current;             ///<currently used favlist
  DList reminder_list;          ///<our list of active reminders
  uint32_t tp_idx;              ///<transponder index

  WINDOW *output;               ///<message window text
  WINDOW *outer;                ///<message window border
} AppData;

void message (AppData * a, const char *fmt, ...);
int init_app_data (AppData * a, WINDOW * output, WINDOW * outer, int argc,
                   char **argv);
void clear_app_data (AppData * a);
int add_window (AppData * a, WINDOW * w, void *v,
                void (*layout) (int newy, int newx, WINDOW * w, void *u));
int remove_window (AppData * a, WINDOW * w);
void app_run (AppData * a);
int appDataGetch (AppData * a);
void appDataMakeMenuWnd (MENU * my_menu);
WINDOW *appDataMakeEmptyWnd (void);
void appDataDestroyMenuWnd (MENU * my_menu);
void appDataResizeMnu (MENU * my_menu);
WINDOW *appDataResizeEmpty (WINDOW * in_wnd);
void sched_epgevt (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t pnr,
                   uint8_t * e, AppData * a);
void remind_epgevt (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t pnr,
                    DvbString * svc_name, uint8_t * e, AppData * a);

//coz it does not work...
#if(NCURSES_VERSION_MAJOR <5 || \
(NCURSES_VERSION_MAJOR==5 && NCURSES_VERSION_MINOR<9) || \
(NCURSES_VERSION_MAJOR==5 && NCURSES_VERSION_MINOR==9 && NCURSES_VERSION_PATCH<20140315))
#define set_current_itemXX(a_,b_) do{\
int i=item_index(b_);\
menu_driver(a_,REQ_FIRST_ITEM);\
while(i--)\
	menu_driver(a_,REQ_NEXT_ITEM);\
}while(0)
#else
#define set_current_itemXX(a_,b_) set_current_item(a_,b_)
#endif

#endif
