#include "dmalloc_f.h"

#ifdef dmalloc_tag
#define d_t(p_,f_,l_) ((NULL!=p_)?dmalloc_tag(p_,f_,l_):(1))
#else
///no-op define
#define d_t(p_,f_,l_) (1)
#endif



#undef new_item
ITEM *
dmalloc_l_new_item (const char *name, const char *description, char *file,
                    int line)
{
  ITEM *rv;
  rv = new_item (name, description);
  d_t (rv, file, line);
  return rv;
}

#undef new_menu
MENU *
dmalloc_l_new_menu (ITEM ** items, char *file, int line)
{
  MENU *rv;
  rv = new_menu (items);
  d_t (rv, file, line);
  return rv;
}

#undef newpad
WINDOW *
dmalloc_l_newpad (int nlines, int ncols, char *file, int line)
{
  WINDOW *rv;
  rv = newpad (nlines, ncols);
  d_t (rv, file, line);
  return rv;
}

#undef newwin
WINDOW *
dmalloc_l_newwin (int nlines, int ncols, int begin_y, int begin_x, char *file,
                  int line)
{
  WINDOW *rv;
  rv = newwin (nlines, ncols, begin_y, begin_x);
  d_t (rv, file, line);
  return rv;
}

#undef subwin
WINDOW *
dmalloc_l_subwin (WINDOW * orig, int nlines, int ncols, int begin_y,
                  int begin_x, char *file, int line)
{
  WINDOW *rv = subwin (orig, nlines, ncols, begin_y, begin_x);
  d_t (rv, file, line);
  return rv;
}

#undef derwin
WINDOW *
dmalloc_l_derwin (WINDOW * orig, int nlines, int ncols, int begin_y,
                  int begin_x, char *file, int line)
{
  WINDOW *rv = derwin (orig, nlines, ncols, begin_y, begin_x);
  d_t (rv, file, line);
  return rv;

}

#undef dupwin
WINDOW *
dmalloc_l_dupwin (WINDOW * win, char *file, int line)
{
  WINDOW *rv = dupwin (win);
  d_t (rv, file, line);
  return rv;

}

#undef subpad
WINDOW *
dmalloc_l_subpad (WINDOW * orig, int nlines, int ncols, int begin_y,
                  int begin_x, char *file, int line)
{
  WINDOW *rv = subpad (orig, nlines, ncols, begin_y, begin_x);
  d_t (rv, file, line);
  return rv;


}

/*
#undef initscr
WINDOW *dmalloc_l_initscr(char * file,int line)
{
	WINDOW *rv=initscr();
	d_t(rv,file,line);
	return rv;

}
*/
#undef newterm
SCREEN *
dmalloc_l_newterm (char *type, FILE * outfd, FILE * infd, char *file,
                   int line)
{
  SCREEN *rv = newterm (type, outfd, infd);
  d_t (rv, file, line);
  return rv;

}

/*
#undef set_term
SCREEN *dmalloc_l_set_term(SCREEN *new,char * file,int line)
{
	SCREEN *rv=set_term(new);
}
*/
