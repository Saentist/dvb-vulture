#ifndef __dmalloc_l_h
#define __dmalloc_l_h
#include <ncurses.h>
#include <menu.h>
#include "dmalloc_loc.h"
/**
 *\file dmalloc_f.h
 *\brief instrumentation code for ncurses allocations
 *
 *requires a hacked libdmalloc for operation, but should be a no-op if not available
 */

ITEM *dmalloc_l_new_item (const char *name, const char *description,
                          char *file, int line);
MENU *dmalloc_l_new_menu (ITEM ** items, char *file, int line);
WINDOW *dmalloc_l_newpad (int nlines, int ncols, char *file, int line);
WINDOW *dmalloc_l_newwin (int nlines, int ncols, int begin_y, int begin_x,
                          char *file, int line);
WINDOW *dmalloc_l_subwin (WINDOW * orig, int nlines, int ncols, int begin_y,
                          int begin_x, char *file, int line);
WINDOW *dmalloc_l_derwin (WINDOW * orig, int nlines, int ncols, int begin_y,
                          int begin_x, char *file, int line);
WINDOW *dmalloc_l_dupwin (WINDOW * win, char *file, int line);
WINDOW *dmalloc_l_subpad (WINDOW * orig, int nlines, int ncols, int begin_y,
                          int begin_x, char *file, int line);
WINDOW *dmalloc_l_initscr (char *file, int line);
SCREEN *dmalloc_l_newterm (char *type, FILE * outfd, FILE * infd, char *file,
                           int line);

#undef new_item
#define new_item(n_,d_) \
 dmalloc_l_new_item(n_,d_,__FILE__,__LINE__)

#undef new_menu
#define new_menu(i_) \
 dmalloc_l_new_menu(i_,__FILE__,__LINE__)

#undef newpad
#define newpad(a_,b_) \
dmalloc_l_newpad(a_, b_,__FILE__,__LINE__)

#undef newwin
#define newwin(a_,b_,c_,d_) \
dmalloc_l_newwin(a_,b_,c_,d_,__FILE__,__LINE__)

#undef subwin
#define subwin(a_,b_,c_,d_,e_) \
dmalloc_l_subwin(a_,b_,c_,d_,e_,__FILE__,__LINE__)

#undef derwin
#define derwin(a_,b_,c_,d_,e_) \
dmalloc_l_derwin(a_,b_,c_,d_,e_,__FILE__,__LINE__)

#undef dupwin
#define dupwin(a_) \
dmalloc_l_dupwin(a_,__FILE__,__LINE__)

#undef subpad
#define subpad(a_,b_,c_,d_,e_) \
dmalloc_l_subpad(a_,b_,c_,d_,e_,__FILE__,__LINE__)
/*
#undef initscr
#define initscr() \
dmalloc_l_initscr(__FILE__,__LINE__)
*/
#undef newterm
#define newterm(a_,b_,c_) \
dmalloc_l_newterm(a_,b_,c_,__FILE__,__LINE__)

#endif
