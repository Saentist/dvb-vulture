#ifndef __menu_h
#define __menu_h
#include "ncurses.h"

#define	E_OK			(0)
#define	E_SYSTEM_ERROR	 	(-1)
#define	E_BAD_ARGUMENT	 	(-2)
#define	E_POSTED	 	(-3)
#define	E_CONNECTED	 	(-4)
#define	E_BAD_STATE	 	(-5)
#define	E_NO_ROOM	 	(-6)
#define	E_NOT_POSTED		(-7)
#define	E_UNKNOWN_COMMAND	(-8)
#define	E_NO_MATCH		(-9)
#define	E_NOT_SELECTABLE	(-10)
#define	E_NOT_CONNECTED	        (-11)
#define	E_REQUEST_DENIED	(-12)
#define	E_INVALID_FIELD	        (-13)
#define	E_CURRENT		(-14)


#define        REQ_LEFT_ITEM 1
#define        REQ_RIGHT_ITEM 2
#define        REQ_UP_ITEM 3
#define        REQ_DOWN_ITEM 4
#define        REQ_SCR_ULINE 5
#define        REQ_SCR_DLINE 6
#define        REQ_SCR_UPAGE 7
#define        REQ_SCR_DPAGE 8
#define        REQ_FIRST_ITEM 9
#define        REQ_LAST_ITEM 10
#define        REQ_NEXT_ITEM 11
#define        REQ_PREV_ITEM 12
#define        REQ_TOGGLE_ITEM 13
//guess those pattern things will not work..
/*
#define        REQ_CLEAR_PATTERN 14
#define        REQ_BACK_PATTERN 15
#define        REQ_NEXT_MATCH 16
#define        REQ_PREV_MATCH 17
*/
typedef enum
{
       O_ROWMAJOR=1,
       O_SHOWDESC=2,
       O_ONEVALUE=4,
       O_IGNORECASE=8,
       O_SHOWMATCH=16,
       O_NONCYCLIC=32
}Menu_Options;

typedef struct MENU_ MENU;
typedef struct ITEM_ ITEM;

int set_menu_win(MENU *menu, WINDOW *win);
WINDOW *menu_win(const MENU *menu);

int set_menu_sub(MENU *menu, WINDOW *sub);
WINDOW *menu_sub(const MENU *menu);

int set_menu_format(MENU *menu, int rows, int cols);
void menu_format(const MENU *menu, int *rows, int *cols);

int menu_driver(MENU *menu, int c);

int set_menu_opts(MENU *menu, Menu_Options opts);

int set_current_item(MENU *menu, const ITEM *item);
ITEM *current_item(const MENU *menu);
int item_index(const ITEM *item);

ITEM *new_item(const char *name, const char *description);
int free_item(ITEM *item);

MENU *new_menu(ITEM **items);
int free_menu(MENU *menu);

int post_menu(MENU *menu);
int unpost_menu(MENU *menu);

const char *item_name(const ITEM *item);
const char *item_description(const ITEM *item);

int set_item_userptr(ITEM *item, void *userptr);
void *item_userptr(const ITEM *item);

int set_item_value(ITEM *item, bool value);
bool item_value(const ITEM *item);

#endif

