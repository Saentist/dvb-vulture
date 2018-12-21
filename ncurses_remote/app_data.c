#include <stdio.h>
#include <ctype.h>
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
#include "opt.h"
#include "switch_mnu.h"
#include "utl.h"
#include "fav_list.h"
#include "fav.h"
#include "config.h"
#include "debug.h"
#include "tp_info.h"
#include "pgm_info.h"
#include "list.h"
#include "in_out.h"
#include "iterator.h"
#include "dmalloc_f.h"
#include "menu_strings.h"
#include "timestamp.h"
#include "cwrap.h"
#include "vt_pk.h"
#include "vtctx.h"
#include "switch.h"
#include "rec_mnu.h"
#include "reminder.h"
#include "srv_codes.h"
#include "epg_ui.h"
#include "ipc.h"
#include "chart.h"

#ifdef __WIN32__
#include"win/wordexp.c"
#endif

int DF_APP_DATA;
#define FILE_DBG DF_APP_DATA


int
appDataGetch (AppData * a)
{
  int c;
  c = getch ();
  if (c == KEY_RESIZE)
  {
    int maxy, maxx;
    getmaxyx (stdscr, maxy, maxx);
//        getmaxyx(a->outer,wmaxy,wmaxx);
    assert (ERR != delwin (a->output));
    assert (ERR != delwin (a->outer));
    a->outer = newwin ((maxy - maxy * 2 / 3), maxx, maxy * 2 / 3, 0);
    a->output = derwin (a->outer, (maxy - maxy * 2 / 3) - 2, maxx - 2, 1, 1);
    wsetscrreg (a->output, 1, (maxy - maxy * 2 / 3) - 2);       //maxy/3-3);
    scrollok (a->output, TRUE);
    box (a->outer, 0, 0);
    //phew ... this combination seems to work for xterm
    touchwin (stdscr);
    wnoutrefresh (stdscr);
    wnoutrefresh (a->outer);
    doupdate ();

//              redrawwin(a->outer);
//              redrawwin(a->output);
//              flash();
//              erase();

//              clearok(curscr,TRUE);
//              wrefresh(curscr);
//              wrefresh(a->outer);
//              wrefresh(a->output);
//              refresh();
//              redrawwin(stdscr);
//              wnoutrefresh(a->outer);
//              wnoutrefresh(a->output);
//              refresh();
  }
  return c;
}

void
appDataMakeMenuWnd (MENU * my_menu)
{
  int maxx, maxy;
  WINDOW *menu_win = NULL;
  WINDOW *menu_sub = NULL;
  getmaxyx (stdscr, maxy, maxx);
  menu_win = newwin (maxy * 2 / 3, maxx, 0, 0);
  menu_sub = derwin (menu_win, maxy * 2 / 3 - 4, maxx - 2, 3, 1);
  set_menu_win (my_menu, menu_win);
  set_menu_sub (my_menu, menu_sub);
  set_menu_format (my_menu, maxy * 2 / 3 - 4, 1);
  box (menu_win, 0, 0);
}

WINDOW *
appDataMakeEmptyWnd (void)
{
  int maxx, maxy;
  WINDOW *wnd = NULL;
  getmaxyx (stdscr, maxy, maxx);
  wnd = newwin (maxy * 2 / 3, maxx, 0, 0);
  box (wnd, 0, 0);
  return wnd;
}

void
appDataDestroyMenuWnd (MENU * my_menu)
{
  assert (ERR != delwin (menu_sub (my_menu)));
  assert (ERR != delwin (menu_win (my_menu)));
}

void
appDataResizeMnu (MENU * my_menu)
{
  unpost_menu (my_menu);
  appDataDestroyMenuWnd (my_menu);
  //resize_term(); for pdcurses I have to do this, for ncurses not...
  appDataMakeMenuWnd (my_menu);
  post_menu (my_menu);
}

WINDOW *
appDataResizeEmpty (WINDOW * in_wnd)
{
  int maxx, maxy;
  WINDOW *wnd = NULL;
  assert (ERR != delwin (in_wnd));
  getmaxyx (stdscr, maxy, maxx);
  wnd = newwin (maxy * 2 / 3, maxx, 0, 0);
  box (wnd, 0, 0);
  return wnd;
}

void
message (AppData * a, const char *fmt, ...)
{
  va_list ap;
//      int maxx,maxy;
//      int wmaxx,wmaxy;
//  getmaxyx(stdscr,maxy,maxx);
//  getmaxyx(a->outer,wmaxy,wmaxx);
/*	if((wmaxy!=(maxy/3))||(maxx!=wmaxx))
	{
		delwin(a->output);
		delwin(a->outer);
		a->outer=newwin(maxy/3,maxx,maxy*2/3,0);
		a->output=derwin(a->outer,maxy/3-2,maxx-2,1,1);
		wsetscrreg(a->output,1,maxy/3-2);//maxy/3-3);
		scrollok(a->output,TRUE);
		box(a->outer,0,0);
		wrefresh(a->outer);
		wrefresh(a->output);
	}*/
  va_start (ap, fmt);
  vw_printw (a->output, fmt, ap);
  va_end (ap);
  wrefresh (a->output);
}


void
free_fav_list (void *x NOTUSED, BTreeNode * this, int which,
               int depth NOTUSED)
{
  if ((which == 3) || (which == 2))
  {
    FavList *fl = btreeNodePayload (this);
    favListClear (fl);
    utlFAN (this);
  }
}

void
free_fav_lists (BTreeNode ** fl)
{
  btreeWalk (*fl, NULL, free_fav_list);
  *fl = NULL;
}

int
init_app_data (AppData * a, WINDOW * output, WINDOW * outer, int argc,
               char **argv)
{
  char *cfgfile = LOC_CFG;
  char *value;
  int af;                       //AF_INET or AF_INET6
  char *fav_db;
  char *rem_db;
  char *buffer;
  char *s;
  char *pfx = LOC_CFG_PFX;
  size_t str_sz;
  int sock;
  long prt;
  long tmp;
  wordexp_t wxp = { 0 };
  FILE *f;
  a->output = output;
  if ((value = find_opt_arg (argc, argv, "-cfg")))
  {
    cfgfile = value;
  }
  if (wordexp (cfgfile, &wxp, WRDE_NOCMD | WRDE_UNDEF))
  {
    errMsg ("wordexp on %s failed\n", cfgfile);
    return 1;
  }
  s = wxp.we_wordv[0];
  if ((!rcfileExists (s)) && (0 != rcfileCreate (s)))
  {
    errMsg ("rc file %s does not exist and can not be created\n", s);
    wordfree (&wxp);
    return 1;
  }
  if (rcfileParse (s, &a->cfg))
  {
    errMsg ("rcfileParse (f, &a->cfg) failed\n");
    wordfree (&wxp);
    return 1;
  }
  wordfree (&wxp);

  af = DEFAULT_AF;
  if (!rcfileFindValInt (&a->cfg, "NET", "af", &tmp))
  {
    if (tmp == 4)
    {
      debugMsg ("Using af = AF_INET;\n");
      af = AF_INET;
    }
    else if (tmp == 6)
    {
#ifdef __WIN32__
      errMsg ("IPv6 not supported\n");
      return 1;

#else
      debugMsg ("Using af = AF_INET6;\n");
      af = AF_INET6;
#endif
    }
  }

  if (rcfileFindVal (&a->cfg, "NET", "host", &value))
  {
    value = DEFAULT_SERVER (af);
    debugMsg ("No host= entry in [NET] section of configfile. using %s.\n",
              value);
  }
  fav_db = "fav.db";
  rcfileFindVal (&a->cfg, "FILES", "fav_db", &fav_db);
  debugMsg ("fav_db: %s\n", fav_db);

  rem_db = "rem.db";
  rcfileFindVal (&a->cfg, "FILES", "rem_db", &rem_db);
  debugMsg ("rem_db: %s\n", rem_db);

  prt = 2241;
  rcfileFindValInt (&a->cfg, "NET", "port", &prt);
  debugMsg ("port: %d\n", prt);

  a->favourite_lists = NULL;
  a->current = NULL;
  str_sz = strlen (fav_db) + strlen (pfx) + 2;
  buffer = utlCalloc (1, str_sz);
#ifndef __WIN32__
  snprintf (buffer, str_sz, "%s/%s", pfx, fav_db);
#else
  snprintf (buffer, str_sz, "%s", fav_db);      //Should I prefix a home dir somehow? may be able to remove ifndef completely with a decent LOC_CFG_PFX (with env vars..)
#endif
  if (wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
  {
    errMsg ("wordexp on %s failed\n", buffer);
    return 1;
  }
  f = fopen (wxp.we_wordv[0], "rb");
  if (f)
  {
    if (btreeRead (&a->favourite_lists, NULL, favListReadNode, f))
    {
      errMsg ("error reading favlist btree\n");
      rcfileClear (&a->cfg);
      return 1;
    }
    fclose (f);
  }
  else
    a->favourite_lists = NULL;
  wordfree (&wxp);
  utlFAN (buffer);


  str_sz = strlen (rem_db) + strlen (pfx) + 2;
  buffer = utlCalloc (1, str_sz);
#ifndef __WIN32__
  snprintf (buffer, str_sz, "%s/%s", pfx, rem_db);
#else
  snprintf (buffer, str_sz, "%s", rem_db);      //Should I prefix a home dir somehow? may be able to remove ifndef completely with a decent LOC_CFG_PFX (with env vars..)
#endif
  dlistInit (&a->reminder_list);
  if (wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
  {
    errMsg ("wordexp on %s failed\n", buffer);
    return 1;
  }
  f = fopen (wxp.we_wordv[0], "rb");
  if (f)
  {
    if (reminderListRead (f, &a->reminder_list))
    {
      errMsg ("error reading reminders\n");
      rcfileClear (&a->cfg);
      return 1;
    }
    fclose (f);
  }
  wordfree (&wxp);
  utlFAN (buffer);
  sock = clientInit (af, value, prt);
  if (sock == -1)
  {
    free_fav_lists (&a->favourite_lists);
    rcfileClear (&a->cfg);
    return 1;
  }
  a->sock = sock;
  a->outer = outer;
  a->tp_idx = 0;
  message (a, "initialized complete\n");
  return 0;
}


void
clear_app_data (AppData * a)
{
  wordexp_t wxp;
  FILE *f;
  DListE *e, *next;
  char *fav_db;
  char *rem_db;
  char *buffer;
  char *pfx = LOC_CFG_PFX;
  size_t str_sz;
  clientClear (a->sock);

  fav_db = "fav.db";
  rcfileFindVal (&a->cfg, "FILES", "fav_db", &fav_db);

  rem_db = "rem.db";
  rcfileFindVal (&a->cfg, "FILES", "rem_db", &rem_db);

  str_sz = strlen (fav_db) + strlen (pfx) + 2;
  buffer = utlCalloc (1, str_sz);
#ifndef __WIN32__
  snprintf (buffer, str_sz, "%s/%s", pfx, fav_db);
#else
  snprintf (buffer, str_sz, "%s", fav_db);      //Should I prefix a home dir somehow?
#endif
  if (!wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
  {
    f = fopen (wxp.we_wordv[0], "wb");
    if (f)
    {
      if (btreeWrite (a->favourite_lists, NULL, favListWriteNode, f))
      {
        errMsg ("error writing btree\n");
      }
      fclose (f);
    }
    wordfree (&wxp);
  }
  utlFAN (buffer);
  free_fav_lists (&a->favourite_lists);

  str_sz = strlen (rem_db) + strlen (pfx) + 2;
  buffer = utlCalloc (1, str_sz);
#ifndef __WIN32__
  snprintf (buffer, str_sz, "%s/%s", pfx, rem_db);
#else
  snprintf (buffer, str_sz, "%s", rem_db);      //Should I prefix a home dir somehow?
#endif
  if (!wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
  {
    f = fopen (wxp.we_wordv[0], "wb");
    if (f)
    {
      if (reminderListWrite (f, &a->reminder_list))
      {
        errMsg ("error writing reminders\n");
      }
      fclose (f);
    }
    wordfree (&wxp);
  }
  utlFAN (buffer);
  e = dlistFirst (&a->reminder_list);
  while (e)
  {
    next = dlistENext (e);
    reminderDel (e);
    e = next;
  }
  dlistInit (&a->reminder_list);
  rcfileClear (&a->cfg);
  assert (ERR != delwin (a->output));
  a->output = NULL;
  assert (ERR != delwin (a->outer));
}


char *choices[] = {
  "Master List",
  "Fav List",
  "Select Fav List",
  "Add Fav List",
  "Del Fav List",
  "Switch Setup",
  "Go Active",
  "Go Inactive",
  "F correct",
  "Rec Schedule",
  "Reminder List",
  "Reminder Mode",
  "Server Status",
  "Exit",
};

enum main_menu_idx
{
  IDX_MASTER_LIST = 0,
  IDX_FAV_LIST = 1,
  IDX_SELECT_LIST = 2,
  IDX_ADD_LIST = 3,
  IDX_DEL_LIST = 4,
  IDX_SWITCH = 5,
  IDX_ACTIVE = 6,
  IDX_INACTIVE = 7,
  IDX_F_CORRECT = 8,
  IDX_SCHED = 9,
  IDX_RLIST = 10,
  IDX_RMODE = 11,
  IDX_USTAT = 12,
  IDX_EXIT = 13
};

void
del_from_current_favlist (uint32_t idx, AppData * a)
{
  if (idx >= a->current->size)
  {
    errMsg ("fav list index overrun\n");
    return;
  }
  favClear (a->current->favourites + idx);
  if ((a->current->size - idx - 1) > 0)
    memmove (a->current->favourites + idx, a->current->favourites + idx + 1,
             (a->current->size - idx - 1) * sizeof (Favourite));
  a->current->size--;
}

void
add_to_current_favlist (uint32_t pos, TransponderInfo * tp, ProgramInfo * pgm,
                        AppData * a)
{
  Favourite tmp;
  Favourite *n;
  if (!a->current)
  {
    errMsg ("Sorry, but there's no FavList selected. Cannot add.\n");
    return;
  }

  tmp.pos = pos;
  tmp.freq = tp->frequency;
  tmp.pol = tp->pol;
  tmp.pnr = pgm->pnr;
  tmp.type = pgm->svc_type;
  tmp.ca_mode = pgm->ca_mode;
  tmp.service_name.buf = NULL;
  tmp.service_name.len = 0;
  dvbStringCopy (&tmp.service_name, &pgm->service_name);

  tmp.provider_name.buf = NULL;
  tmp.provider_name.len = 0;
  dvbStringCopy (&tmp.provider_name, &pgm->provider_name);

  tmp.bouquet_name.buf = NULL;
  tmp.bouquet_name.len = 0;
  dvbStringCopy (&tmp.bouquet_name, &pgm->bouquet_name);

  n =
    utlRealloc (a->current->favourites,
                (a->current->size + 1) * sizeof (Favourite));
  if (!n)
  {
    errMsg ("Sorry, couldn't reallocate FavList\n");
    favClear (&tmp);
    return;
  }

  a->current->favourites = n;
  memmove (a->current->favourites + a->current->size, &tmp,
           sizeof (Favourite));
  a->current->size++;

  message (a, "Added to %s. Thank you.\n", a->current->name);
  return;
}

void
tune_pgm (uint32_t pos, TransponderInfo * tp, ProgramInfo * pgm, AppData * a)
{
  cwDoTune (a->sock, pos, tp->frequency, tp->pol, pgm->pnr);
}

void
tune_fav (Favourite * f, AppData * a)
{
  cwDoTune (a->sock, f->pos, f->freq, f->pol, f->pnr);
}

void
show_epgevt_desc (uint8_t * e, AppData * a)
{
  char *desc = NULL;
  int maxx, maxy;
  WINDOW *desc_frame, *desc_win;
  char *p;
  int emph = 0;
  uint16_t id;
//      uint32_t pil;
  desc = evtGetDesc (e);
  id = evtGetId (e);
  message (a, "EvtId: 0x%4.4" PRIx16 "\n", id);
//      utlAsciiSanitize(desc);

  if (NULL != desc)
  {
    getmaxyx (stdscr, maxy, maxx);
    desc_frame = newwin (maxy * 2 / 3, maxx, 0, 0);
    desc_win = derwin (desc_frame, maxy * 2 / 3 - 2 - 1, maxx - 1 - 2, 2, 1);
    wsetscrreg (desc_win, 0, maxy * 2 / 3 - 2);
    scrollok (desc_win, TRUE);
    box (desc_frame, 0, 0);
    p = desc;
    while (*p)
    {

      if (*p == '\x86')
        emph = 1;
      else if (*p == '\x87')
        emph = 0;
      else
        waddch (desc_win, (*p) | (emph ? WA_BOLD : 0));
      p++;
    }
//              waddstr(desc_win,desc);
    wrefresh (desc_frame);
    wrefresh (desc_win);
//              message(a,desc);
    appDataGetch (a);
    utlFAN (desc);
    assert (ERR != delwin (desc_win));
    assert (ERR != delwin (desc_frame));
  }
}

/*
int sch_neq(RsEntry * a,uint32_t a_sz,RsEntry * b,uint32_t b_sz)
{
	uint32_t i;
	if(a_sz!=b_sz)
		return 1;
	for(i=0;i<a_sz;i++)
	{
		if(rsNeq(a+i,b+i))
		{
			return 2;
		}
	}
	return 0;
}
int rec_sched(RsEntry *new_sch,uint32_t new_sz,RsEntry *prev_sch,uint32_t prev_sz,AppData *a)
{
	uint8_t cmd;
	uint8_t res;
	uint8_t flg;
	uint32_t sch_sz,i;
	RsEntry *r_sch;
	cmd=REC_SCHED;
	ipcSndS(a->sock,cmd);
	ipcRcvS(a->sock,res);
	if(res!=SRV_NOERR)
		return 1;
	
	ipcRcvS(a->sock,sch_sz);
	r_sch=utlCalloc(sch_sz,sizeof(RsEntry));
	if((NULL==r_sch)&&(0!=sch_sz))
	{
		for(i=0;i<sch_sz;i++)
		{
			RsEntry tmp;
			rsRcvEntry(&tmp,a->sock);
			rsClearEntry(&tmp);
		}
		flg=1;
		ipcSndS(a->sock,flg);
		return 2;
	}
	else
	for(i=0;i<sch_sz;i++)
	{
		rsRcvEntry(r_sch+i,a->sock);
	}
	if(rsNeqSched(r_sch,sch_sz,prev_sch,prev_sz))
	{
		flg=1;
		ipcSndS(a->sock,flg);
		return 3;
	}
	
	flg=0;
	ipcSndS(a->sock,flg);
	ipcSndS(a->sock,new_sz);
	for(i=0;i<new_sz;i++)
	{
		rsSndEntry(new_sch+i,a->sock);
	}
	return 0;
}

int rec_abort(AppData *a)
{
	uint8_t cmd;
	uint8_t res;
	cmd=ABORT_REC;
	ipcSndS(a->sock,cmd);
	ipcRcvS(a->sock,res);
	if(res!=SRV_NOERR)
		return 1;
	return 0;
}

int rec_sched_add(RsEntry *e,AppData *a)
{
	uint8_t cmd;
	uint8_t res;
	cmd=ADD_SCHED;
	ipcSndS(a->sock,cmd);
	ipcRcvS(a->sock,res);
	if(res!=SRV_NOERR)
		return 1;
	rsSndEntry(e,a->sock);
	return 0;
}

uint8_t get_sched(AppData *a,RsEntry **rv,uint32_t *sz_ret)
{
	uint32_t i;
	uint8_t cmd=GET_SCHED;
	uint8_t res;
	uint32_t sch_sz;
	RsEntry *r_sch;
	
	ipcSndS(a->sock,cmd);
	ipcRcvS(a->sock,res);//?
	if(res!=SRV_NOERR)
		return res;
		
	ipcRcvS(a->sock,sch_sz);
	r_sch=utlCalloc(sch_sz,sizeof(RsEntry));
	if(!r_sch)
	for(i=0;i<sch_sz;i++)
	{
		RsEntry tmp;
		rsRcvEntry(&tmp,a->sock);
		rsClearEntry(&tmp);
	}
	else
	for(i=0;i<sch_sz;i++)
	{
		rsRcvEntry(r_sch+i,a->sock);
	}
	*sz_ret=sch_sz;
	*rv=r_sch;
	return SRV_NOERR;
}
*/

void
epg_sched_init (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t pnr,
                uint8_t * e, RsEntry * r_e)
{
  time_t start;
  char *n;
  int h, m, s;
  int d, mo, y;
  struct tm src;

  r_e->state = RS_WAITING;
  r_e->type = RST_ONCE;
  r_e->wd = 0;
  r_e->flags = 0;
  r_e->pos = pos;
  r_e->pol = pol;
  r_e->freq = freq;
  r_e->pnr = pnr;
  r_e->duration = evtGetDuration (e);
  r_e->evt_id = evtGetId (e);

  start = evtGetStart (e);
  h = get_h (start);
  m = get_m (start);
  s = get_s (start);
  mjd_to_ymd (ts_to_mjd (start), &y, &mo, &d);
  src.tm_min = m;
  src.tm_sec = s;
  src.tm_hour = h;
  src.tm_mday = d;
  src.tm_mon = mo - 1;
  src.tm_year = y - 1900;
  r_e->start_time = timegm (&src);      //avoid me. I'm a gNU extension
  n = evtGetName (e);
  utlAsciiSanitize (n);
  r_e->name = n;
}

void
sched_epgevt (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t pnr,
              uint8_t * e, AppData * a)
{
  RsEntry tmp;
  epg_sched_init (pos, freq, pol, pnr, e, &tmp);
  if (cwRecSchedAdd (a->sock, &tmp))
    message (a, "rec_sched failed\n");
  else
    message (a, "event scheduled\n");
  rsClearEntry (&tmp);
  return;
}

void
remind_epgevt (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t pnr,
               DvbString * svc_name, uint8_t * e, AppData * a)
{
  DListE *rmd;
  time_t start;
  int h, m, s;
  int d, mo, y;
  struct tm src;
  char *ev_name;

  start = evtGetStart (e);
  h = get_h (start);
  m = get_m (start);
  s = get_s (start);
  mjd_to_ymd (ts_to_mjd (start), &y, &mo, &d);
  src.tm_min = m;
  src.tm_sec = s;
  src.tm_hour = h;
  src.tm_mday = d;
  src.tm_mon = mo - 1;
  src.tm_year = y - 1900;

  ev_name = evtGetName (e);

  rmd = reminderNew (RST_ONCE, 0, timegm (&src), evtGetDuration (e), pos, freq, pol, pnr, svc_name,     //&pgm->service_name,
                     ev_name);
  utlFAN (ev_name);
  if (rmd)
  {
    reminderAdd (&a->reminder_list, rmd);
    message (a, "memorized\n");
  }
  else
  {
    message (a, "failed\n");
  }
}

static EpgArray *
get_epg (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t * pnrs,
         uint16_t num_pgm, AppData * a)
{
  uint32_t start, end, num_entries = 0;
  long num_days = 7;
  long tmp;
  debugMsg ("get epg %" PRIu32 ", %" PRIu32 ", %" PRIu8 ", %" PRIu16 "\n",
            pos, freq, pol, pnrs[0]);

  if (!rcfileFindValInt (&a->cfg, "EPG", "num_days", &tmp))
    num_days = tmp;
  if (!rcfileFindValInt (&a->cfg, "EPG", "num_entries", &tmp))
    num_entries = tmp;
  start = epg_get_buf_time ();
  if (num_days)
    end = start + num_days * 3600 * 24;
  else
    end = 0;
  return cwGetEpg (a->sock, pos, freq, pol, pnrs, num_pgm, start, end,
                   num_entries);
}

void
show_epg_fav (Favourite * f, int num_pgm, char **pgm_strings, AppData * a)
{
  int i, j, maxx, maxy, w, h;
  unsigned col, idx;
  EpgArray *tmp;
  EpgArray epga;
  char **pgm_names;
  Favourite *f2;
  EpgView ev;
  int state = 0;
  int c;
  epga.num_pgm = num_pgm;
  epga.pgm_nrs = utlCalloc (num_pgm, sizeof (uint16_t));
  epga.sched = utlCalloc (num_pgm, sizeof (EpgSchedule));
  f2 = utlCalloc (num_pgm, sizeof (Favourite));
  pgm_names = utlCalloc (num_pgm, sizeof (char *));
  j = 0;
  for (i = 0; i < num_pgm; i++)
  {
    tmp = get_epg (f[i].pos, f[i].freq, f[i].pol, &f[i].pnr, 1, a);
    if (tmp)
    {
      if ((tmp->num_pgm == 1) && (tmp->sched[0].num_events))
      {
        epga.pgm_nrs[j] = tmp->pgm_nrs[0];
        epga.sched[j] = tmp->sched[0];
        pgm_names[j] = pgm_strings[i];
        f2[j] = f[i];
        j++;
        utlFAN (tmp->pgm_nrs);
        utlFAN (tmp->sched);
        utlFAN (tmp);
        tmp = NULL;
      }
      else
        epgArrayDestroy (tmp);
    }
  }
  epga.num_pgm = j;
  if (0 == j)
  {
    message (a, "Sorry, there are no events to display\n");
    return;
  }
  getmaxyx (stdscr, maxy, maxx);
  h = maxy * 2 / 3 - 2;
  w = maxx - 2;
  epgUiInit (&ev, &a->cfg, epga.sched, pgm_names, j, 1, 1, w, h);       //,20,4);
  message (a,
           "This is the EPG Browser. Use Cursor Keys to navigate, Press Carriage Return to tune, x and c scroll description. Use s to schedule for recording, m to memorize a reminder. Backspace exits.\n");
  while (state == 0)
  {
    epgUiDraw (&ev);
    c = appDataGetch (a);
    switch (c)
    {
    case KEY_BACKSPACE:        //backspace
    case K_BACK:               //Esc
      state = -1;
      break;
    case KEY_DOWN:
      epgUiAction (&ev, NAV_D);
      break;
    case KEY_UP:
      epgUiAction (&ev, NAV_U);
      break;
    case KEY_LEFT:
      epgUiAction (&ev, NAV_L);
      break;
    case KEY_RIGHT:
      epgUiAction (&ev, NAV_R);
      break;
    case KEY_RESIZE:
      getmaxyx (stdscr, maxy, maxx);
      h = maxy * 2 / 3 - 2;
      w = maxx - 2;
      epgUiChange (&ev, 1, 1, w, h);
      break;
    case 13:
      epgUiGetPos (&ev, &col, &idx);
      tune_fav (&f2[col], a);
      break;
    case 's':
      epgUiGetPos (&ev, &col, &idx);
      sched_epgevt (f2[col].pos, f2[col].freq, f2[col].pol, f2[col].pnr,
                    epga.sched[col].events[idx], a);
      break;
    case 'm':
      epgUiGetPos (&ev, &col, &idx);
      remind_epgevt (f2[col].pos, f2[col].freq, f2[col].pol, f2[col].pnr,
                     &f2[col].service_name, epga.sched[col].events[idx], a);
      break;
    case 'c':
      epgUiAction (&ev, SCR_U);
      break;
    case 'x':
      epgUiAction (&ev, SCR_D);
      break;
    case KEY_NPAGE:
      epgUiAction (&ev, NAV_ILAST);
      break;
    case KEY_PPAGE:
      epgUiAction (&ev, NAV_IFIRST);
      break;
    default:
//                      message(a,"Got unknown key 0%o (octal)\n",c);
      break;
    }
  }
  epgUiClear (&ev);
  utlFAN (pgm_names);
  utlFAN (f2);
  epgArrayClear (&epga);
  return;
}

/*
void show_epg_fav(Favourite * f,int num_pgm,char ** pgm_strings,int pgm_idx,AppData *a)
{
	MENU * my_menu;
	int i,maxy,maxx,state=0;
	ITEM ** my_items;
//	WINDOW * menu_win,*menu_sub;
	EpgArray *epg;
	char ** epg_strings;
	int num_evt;
	int idx;
	message(a,"This is the EPG Browser. Use Cursor Keys to navigate, tap enter to show the description, use s to schedule for recording, m to memorize a reminder. Backspace exits.\n");
	while(state!=-1)
	{
		if(state==1)
		{
			if(pgm_idx>0)
				pgm_idx--;
		}
		if(state==2)
		{
			pgm_idx++;
			if(pgm_idx>=num_pgm)
				pgm_idx=num_pgm-1;
		}
		epg=get_epg(f[pgm_idx].pos,f[pgm_idx].freq,f[pgm_idx].pol,&f[pgm_idx].pnr,1,a);
		state=0;
		if((NULL!=epg)&&(0!=epg->sched[0].num_events))
		{
			int lbl_x;
			num_evt=epg->sched[0].num_events;
			debugMsg("num_events %d\n",epg->sched[0].num_events);
			epg_strings=sched_to_strings(&epg->sched[0]);
			debugMsg("allocating %u items\n",num_evt);
		  my_items = (ITEM **)utlCalloc(num_evt + 1, sizeof(ITEM *));
		  for(i = 0; i < num_evt; ++i)
		  {
				my_items[i] = new_item(epg_strings[i], epg_strings[i]);
				if(NULL==my_items[i])
				{
				  if(errno==E_BAD_ARGUMENT)
				    errMsg("badarg i=%d string=%s\n",i,epg_strings[i]);
				  if(errno==E_SYSTEM_ERROR)
				    errMsg("new_item: syserr i=%d string=%s\n",i,epg_strings[i]);
				}
		  }
		  my_items[num_evt] = (ITEM *)NULL;
		  my_menu = new_menu((ITEM **)my_items);
		  set_menu_opts(my_menu,O_ONEVALUE|O_NONCYCLIC|O_ROWMAJOR);
			getmaxyx(stdscr,maxy,maxx);
			debugMsg("make menu Window\n");
			appDataMakeMenuWnd(my_menu);
			debugMsg("post\n");
		  post_menu(my_menu);
		  lbl_x=3+(maxx-6)/2-strlen(pgm_strings[pgm_idx])/2;
			debugMsg("label\n");
		  if(pgm_idx>0)
			  mvwaddch(menu_win(my_menu),1,1,'<');
		  if(pgm_idx<(num_pgm-1))
			  mvwaddch(menu_win(my_menu),1,maxx-2,'>');
			mvwprintw(menu_win(my_menu),1,lbl_x,"%s",pgm_strings[pgm_idx]);
		  wrefresh(menu_win(my_menu));
		  
			debugMsg("starting menu loop\n");
			while(state==0)
			{
				int c;
				ITEM * selection;
				c=appDataGetch(a);
//				mvprintw(0,0,"%d\n",c);
				switch(c)
				{   
					case 263://ESC
					case K_BACK://Backspace
					state=-1;
					break;
					case KEY_DOWN:
					menu_driver(my_menu, REQ_DOWN_ITEM);
					break;
					case KEY_UP:
					menu_driver(my_menu, REQ_UP_ITEM);
					break;
					case KEY_LEFT:
					state=1;
					break;
					case KEY_RIGHT:
					state=2;
					break;
					case KEY_RESIZE:
					appDataResizeMnu(my_menu);
					//windows need to be resized.. ignore
					wrefresh(menu_win(my_menu));
					break;
					case 13://show description
					state=3;
					selection=current_item(my_menu);
					idx=item_index(selection);
					break;
					case 's'://schedule
					state=4;
					selection=current_item(my_menu);
					idx=item_index(selection);
					break;
					case 'm'://memorize
					state=5;
					selection=current_item(my_menu);
					idx=item_index(selection);
					break;
					default:
					break;
				}
			  wrefresh(menu_win(my_menu));
			}       
			unpost_menu(my_menu);
		  appDataDestroyMenuWnd(my_menu);
		  free_menu(my_menu);
			for(i = 0; i < num_evt; ++i)
			{
				free_item(my_items[i]);
			}
		  utlFAN(my_items);
			for(i = 0; i < num_evt; ++i)
			{
				utlFAN(epg_strings[i]);
			}
			utlFAN(epg_strings);
			switch(state)
			{
				case 3:
				show_epgevt_desc(epg->sched[0].events[idx],a);
				break;
				case 4:
				sched_epgevt(f[pgm_idx].pos,f[pgm_idx].freq,f[pgm_idx].pol,f[pgm_idx].pnr,epg->sched[0].events[idx],a);
				break;
				case 5:
				remind_epgevt(f[pgm_idx].pos,f[pgm_idx].freq,f[pgm_idx].pol,f[pgm_idx].pnr,&f[pgm_idx].service_name,epg->sched[0].events[idx],a);
				break;
				default:
				break;
			}
	  }
	  else
	  {
			int lbl_x;
			WINDOW *wnd;
			getmaxyx(stdscr,maxy,maxx);
			wnd=appDataMakeEmptyWnd();
//		 	menu_win=newwin(maxy*2/3,maxx-21,0,20+1);
		 	if(!wnd)
		 	{
			 	errMsg("newwin failed\n");
			 	return;
			}
//			box(wnd,0,0);
		  lbl_x=3+(maxx-6)/2-strlen(pgm_strings[pgm_idx])/2;
		  if(pgm_idx>0)
			  mvwaddch(wnd,1,1,'<');
		  if(pgm_idx<(num_pgm-1))
			  mvwaddch(wnd,1,maxx-2,'>');
			if(mvwprintw(wnd,1,lbl_x,"%s",(pgm_strings[pgm_idx])))
			 	errMsg("printw failed\n");
			debugMsg(pgm_strings[pgm_idx]);
		  wrefresh(wnd);

			while(state==0)
			{
				int c;
				c=appDataGetch(a);
//				mvprintw(0,0,"%d\n",c);
				switch(c)
				{   
					case 263://ESC
					case K_BACK://Backspace
					state=-1;
					break;
					case KEY_LEFT:
					state=1;
					break;
					case KEY_RESIZE:
					wnd=appDataResizeEmpty(wnd);
				  wrefresh(wnd);
//					refresh();
					//windows need to be resized.. ignore
					break;
					case KEY_RIGHT:
					state=2;
					break;
					default:
					break;
				}
			}       
		  delwin(wnd);
	  }
		epgArrayDestroy(epg);
	}
}
*/
void
show_epg (uint32_t pos, uint32_t freq, uint8_t pol, ProgramInfo * pgms,
          int num_pgm, char **pgm_strings, int pgm_idx, AppData * a)
{
  MENU *my_menu;
  int i, maxx, state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  EpgArray *epg;
  char **epg_strings;
  int num_evt;
  int idx = 0;
  message (a,
           "This is the EPG Browser. Use Cursor Keys to navigate, tap enter to show the description, use s to schedule for recording, m to memorize a reminder. Backspace exits.\n");
  while (state != -1)
  {
    if (state == 1)
    {
      if (pgm_idx > 0)
        pgm_idx--;
    }
    if (state == 2)
    {
      pgm_idx++;
      if (pgm_idx >= num_pgm)
        pgm_idx = num_pgm - 1;
    }
    epg = get_epg (pos, freq, pol, &pgms[pgm_idx].pnr, 1, a);
    state = 0;
    if (epg)
    {
      num_evt = epg->sched[0].num_events;
    }
    else
      num_evt = 0;
    if (num_evt)
    {
      int lbl_x;
      debugMsg ("num_events %d\n", epg->sched[0].num_events);
      epg_strings = sched_to_strings (&epg->sched[0]);
      debugMsg ("allocating items\n");
      my_items = (ITEM **) utlCalloc (num_evt + 1, sizeof (ITEM *));
      for (i = 0; i < num_evt; ++i)
      {
        my_items[i] = new_item (epg_strings[i], epg_strings[i]);
        if (NULL == my_items[i])
        {
          if (errno == E_BAD_ARGUMENT)
            errMsg ("badarg i=%d string=%s\n", i, epg_strings[i]);
          if (errno == E_SYSTEM_ERROR)
            errMsg ("new_item: syserr i=%d string=%s\n", i, epg_strings[i]);
        }
      }
      my_items[num_evt] = (ITEM *) NULL;
      my_menu = new_menu ((ITEM **) my_items);
      set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
      maxx = getmaxx (stdscr);
      appDataMakeMenuWnd (my_menu);
      post_menu (my_menu);
      lbl_x = 3 + (maxx - 6) / 2 - strlen (pgm_strings[pgm_idx]) / 2;
      if (pgm_idx > 0)
        mvwaddch (menu_win (my_menu), 1, 1, '<');
      if (pgm_idx < (num_pgm - 1))
        mvwaddch (menu_win (my_menu), 1, maxx - 2, '>');
      mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", pgm_strings[pgm_idx]);
      wrefresh (menu_win (my_menu));

      debugMsg ("starting menu loop\n");
      while (state == 0)
      {
        int c;
        ITEM *selection;
        c = appDataGetch (a);
//                              mvprintw(0,0,"%d\n",c);
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
          appDataResizeMnu (my_menu);
          //windows need to be resized.. ignore
          wrefresh (menu_win (my_menu));
          break;
        case 13:               //show description
          state = 3;
          selection = current_item (my_menu);
          idx = item_index (selection);
          break;
        case 's':              //schedule
          state = 4;
          selection = current_item (my_menu);
          idx = item_index (selection);
          break;
        case 'm':              //memorize
          state = 5;
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
      for (i = 0; i < num_evt; ++i)
      {
        free_item (my_items[i]);
      }
      utlFAN (my_items);
      for (i = 0; i < num_evt; ++i)
      {
        utlFAN (epg_strings[i]);
      }
      utlFAN (epg_strings);
      switch (state)
      {
      case 3:
        show_epgevt_desc (epg->sched[0].events[idx], a);
        break;
      case 4:
        sched_epgevt (pos, freq, pol, pgms[pgm_idx].pnr,
                      epg->sched[0].events[idx], a);
        break;
      case 5:
        remind_epgevt (pos, freq, pol, pgms[pgm_idx].pnr,
                       &pgms[pgm_idx].service_name, epg->sched[0].events[idx],
                       a);
        break;
      default:
        break;
      }
    }
    else
    {
      int lbl_x;
      WINDOW *wnd;
      maxx = getmaxx (stdscr);
      wnd = appDataMakeEmptyWnd ();
//                      menu_win=newwin(maxy*2/3,maxx-21,0,20+1);
      if (!wnd)
      {
        errMsg ("newwin failed\n");
        return;
      }
//                      box(wnd,0,0);
      lbl_x = 3 + (maxx - 6) / 2 - strlen (pgm_strings[pgm_idx]) / 2;
      if (pgm_idx > 0)
        mvwaddch (wnd, 1, 1, '<');
      if (pgm_idx < (num_pgm - 1))
        mvwaddch (wnd, 1, maxx - 2, '>');
      if (ERR == mvwprintw (wnd, 1, lbl_x, "%s", (pgm_strings[pgm_idx])))
        errMsg ("printw failed\n");
      debugMsg (pgm_strings[pgm_idx]);
      wrefresh (wnd);

      while (state == 0)
      {
        int c;
        c = appDataGetch (a);
//                              mvprintw(0,0,"%d\n",c);
        switch (c)
        {
        case 263:              //ESC
        case K_BACK:           //Backspace
          state = -1;
          break;
        case KEY_LEFT:
          state = 1;
          break;
        case KEY_RESIZE:
          wnd = appDataResizeEmpty (wnd);
          wrefresh (wnd);
//                                      refresh();
          //windows need to be resized.. ignore
          break;
        case KEY_RIGHT:
          state = 2;
          break;
        default:
          break;
        }
      }
      assert (ERR != delwin (wnd));
    }
    if (epg)
      epgArrayDestroy (epg);
  }
}

/*
uint16_t * vt_get_pids(uint32_t pos,uint32_t freq,uint8_t pol,uint16_t pnr, uint16_t* num_svc, AppData* a)
{
	uint32_t i;
	uint8_t cmd=VT_GET_PIDS;
	uint8_t res;
	uint16_t num_pk_ret;
	uint16_t * rv;
	debugMsg("vt_get_pids\n");
	ipcSndS(a->sock,cmd);
	ipcSndS(a->sock,pos);
	ipcSndS(a->sock,freq);
	ipcSndS(a->sock,pol);
	ipcSndS(a->sock,pnr);
	ipcRcvS(a->sock,res);
	if(res)
	{
		errMsg("Error: %hhu\n",res);
		*num_svc=0;
		return NULL;
	}
	ipcRcvS(a->sock,num_pk_ret);
	debugMsg("getting %d pids\n", num_pk_ret);
	if(!num_pk_ret)
	{
		errMsg("Error: no txt services\n");
		*num_svc=0;
		return NULL;
	}
	rv=utlCalloc(num_pk_ret,sizeof(uint16_t));
	if(!rv)
	{
		errMsg("Error: out of mem\n");
		*num_svc=0;
		uint8_t bb;
		for(i=0;i<num_pk_ret*sizeof(uint16_t);i++)
		{
			ipcRcvS(a->sock,bb);
		}
		return NULL;
	}
	debugMsg("reading %d bytes\n",num_pk_ret*sizeof(uint16_t));
	for(i=0;i<num_pk_ret;i++)
	{
		ipcRcvS(a->sock,rv[i]);
	}
	*num_svc=num_pk_ret;
	//debugMsg
	message(a,"got %d pids Pnr:%d\n",num_pk_ret,pnr);
	return rv;
}

uint8_t * vt_get_svc(uint32_t pos, uint32_t freq,uint8_t pol,uint16_t pid, uint16_t* num_svc, AppData* a)
{
	uint32_t i;
	uint8_t cmd=VT_GET_SVC;
	uint8_t res;
	uint16_t num_pk_ret;
	uint8_t * rv;
	debugMsg("vt_get_pids\n");
	ipcSndS(a->sock,cmd);
	ipcSndS(a->sock,pos);
	ipcSndS(a->sock,freq);
	ipcSndS(a->sock,pol);
	ipcSndS(a->sock,pid);
	ipcRcvS(a->sock,res);
	if(res)
	{
		errMsg("Error: %hhu\n",res);
		*num_svc=0;
		return NULL;
	}
	ipcRcvS(a->sock,num_pk_ret);
	debugMsg("getting %d services\n", num_pk_ret);
	if(!num_pk_ret)
	{
		errMsg("Error: no txt services\n");
		*num_svc=0;
		return NULL;
	}
	rv=utlCalloc(num_pk_ret,sizeof(uint8_t));
	if(!rv)
	{
		errMsg("Error: out of mem\n");
		*num_svc=0;
		uint8_t bb;
		for(i=0;i<num_pk_ret*sizeof(uint8_t);i++)
		{
			ipcRcvS(a->sock,bb);
		}
		return NULL;
	}
	debugMsg("reading %d bytes\n",num_pk_ret*sizeof(uint8_t));
	for(i=0;i<num_pk_ret;i++)
	{
		ipcRcvS(a->sock,rv[i]);
	}
	*num_svc=num_pk_ret;
	//debugMsg
	message(a,"got %d svcs Pid:%d\n",num_pk_ret,pid);
	return rv;
}

*/
uint8_t
unparity_rev (uint8_t in)
{
  uint8_t rv = bitrev (in);
  if (parity8 (rv) != 1)
    return 0xff;
  return rv & 0x7f;
}

#define MAGNO_OFFS 2
#define HDR_DATA_START 12
#define VT_DATA_START 4
void
vt_render_packet (WINDOW * vt_win, VtCtx * ctx, VtPk * pk,
                  AppData * a NOTUSED)
{
  uint8_t pk_y = get_pk_y (pk->data);
  int j;
  if (pk_y == 0)
  {                             //hdr
    for (j = 0; j < 32; j++)
    {
      uint8_t d = pk->data[HDR_DATA_START + j];
      d = unparity_rev (d);
      if (d < 32)
      {                         //attribute
        switch (d)
        {
        case 0:
          vtctxSetFg (ctx, COLOR_BLACK);
          break;
        case 1:
          vtctxSetFg (ctx, COLOR_RED);
          break;
        case 2:
          vtctxSetFg (ctx, COLOR_GREEN);
          break;
        case 3:
          vtctxSetFg (ctx, COLOR_YELLOW);
          break;
        case 4:
          vtctxSetFg (ctx, COLOR_BLUE);
          break;
        case 5:
          vtctxSetFg (ctx, COLOR_MAGENTA);
          break;
        case 6:
          vtctxSetFg (ctx, COLOR_CYAN);
          break;
        case 7:
          vtctxSetFg (ctx, COLOR_WHITE);
          break;
        case 8:
          vtctxSetFlash (ctx);
          break;
        case 9:
          vtctxClearFlash (ctx);
          break;
        case 0x1c:
          vtctxResetBkgd (ctx);
          break;
        case 0x1d:
          vtctxSetBkgd (ctx);
          break;
        default:
          break;
        }
        vtctxChar (vt_win, ctx, ' ');
      }
      else
        vtctxChar (vt_win, ctx, d);
    }

  }
  else if ((pk_y <= 25) && (pk_y >= 1))
  {                             //displayable
    vtctxSetY (ctx, pk_y);
    for (j = 0; j < 40; j++)
    {
      uint8_t d = pk->data[VT_DATA_START + j];
      d = unparity_rev (d);
      if (d < 32)
      {                         //attribute
        switch (d)
        {
        case 0:
          vtctxSetFg (ctx, COLOR_BLACK);
          break;
        case 1:
          vtctxSetFg (ctx, COLOR_RED);
          break;
        case 2:
          vtctxSetFg (ctx, COLOR_GREEN);
          break;
        case 3:
          vtctxSetFg (ctx, COLOR_YELLOW);
          break;
        case 4:
          vtctxSetFg (ctx, COLOR_BLUE);
          break;
        case 5:
          vtctxSetFg (ctx, COLOR_MAGENTA);
          break;
        case 6:
          vtctxSetFg (ctx, COLOR_CYAN);
          break;
        case 7:
          vtctxSetFg (ctx, COLOR_WHITE);
          break;
        case 8:
          vtctxSetFlash (ctx);
          break;
        case 9:
          vtctxClearFlash (ctx);
          break;
        case 0x1c:
          vtctxResetBkgd (ctx);
          break;
        case 0x1d:
          vtctxSetBkgd (ctx);
          break;
        default:
          break;
        }
        vtctxChar (vt_win, ctx, ' ');
      }
      else
        vtctxChar (vt_win, ctx, d);
    }
  }
}

void
vt_render_pg (WINDOW * vt_win, VtPk * page, uint16_t num_page,
              VtPk * magglob NOTUSED, uint16_t num_mag NOTUSED,
              VtPk * svcglob NOTUSED, uint16_t num_svc NOTUSED, AppData * a)
{
//      WINDOW * vt_win;
  VtCtx ctx;
  int i;
  vtctxInit (&ctx);
  werase (vt_win);
  for (i = 0; i < num_page; i++)
  {
    vt_render_packet (vt_win, &ctx, &page[i], a);
  }
}

int
parse_pgno (AppData * a, WINDOW * vt_win, int *magno_p, int *pgno_p,
            int *subno_p)
{
  int c[3];
  int magno = *magno_p, pgno = *pgno_p, subno = *subno_p;
  int num = 0;
//      wmove(vt_win,0,0);
  while (num < 3)
  {
    c[num] = appDataGetch (a);  //vt_win);
    if (c[num] == KEY_UP)
    {
      subno++;
      if ((subno & 0xf) == 10)
        subno += 6;
      *subno_p = subno;
      return 0;
    }
    if (c[num] == KEY_DOWN)
    {
      if (subno == 0)
        return 0;
      subno--;
      if ((subno & 0xf) == 0xf)
        subno -= 6;
      *subno_p = subno;
      return 0;
    }
    if (c[num] == KEY_LEFT)
    {
      if (pgno == 0)
      {
        if (magno == 1)
          return 0;
        if (magno == 0)
          magno = 7;
        else
          magno--;
        *magno_p = magno;
        *pgno_p = 0x99;
        *subno_p = 0;
        return 0;
      }
      pgno--;
      if (((pgno) & 0xf) == 0xf)
        pgno = ((pgno) & 0xf0) | 9;

      *pgno_p = pgno;
      *subno_p = 0;
      return 0;
    }
    if (c[num] == KEY_RIGHT)
    {
      if (pgno >= 0x99)
      {
        if (magno == 0)
          return 0;
        if (magno == 7)
          magno = 0;
        else
          magno++;
        *magno_p = magno;
        *pgno_p = 0;
        *subno_p = 0;
        return 0;
      }
      pgno++;
      if (((pgno) & 0xf) == 0xa)
        pgno = ((pgno) & 0xf0) + 0x10;

      *pgno_p = pgno;
      *subno_p = 0;
      return 0;
    }
    if (c[num] == K_BACK)
      return 1;
    if (c[num] == 263)
    {
      if (num == 0)
        return 1;
      mvwaddch (vt_win, 0, num, ' ');
      wmove (vt_win, 0, num);
      wrefresh (vt_win);
      num--;
    }
    else if (((num == 0) && (c[num] >= '1') && (c[num] <= '8')) ||
             ((num > 0) && (c[num] >= '0') && (c[num] <= '9')))
    {
      mvwaddch (vt_win, 0, num + 1, c[num]);
      wrefresh (vt_win);
      num++;
    }
  }
  c[0] -= '0';
  c[1] -= '0';
  c[2] -= '0';
  if (c[0] == 8)
    c[0] = 0;
  *magno_p = c[0];
  *pgno_p = 16 * c[1] + c[2];
  *subno_p = 0;
  return 0;
}

void
real_show_vt (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t pid,
              uint8_t data_id, AppData * a)
{
  int maxy, maxx, width, height, magno = 1, pgno = 0, subno = 0;
  WINDOW *vt_win;
  uint16_t num_svc;
  VtPk *svcglob;
  uint16_t num_mag;
  VtPk *magglob;
  uint16_t num_page;
  VtPk *page;
  const int min_width = 40 + 2;
  const int min_height = 25 + 2;


  message (a,
           "This is the Videotext Browser. Use Cursor Keys to navigate, or enter a page number(100-899) directly.\n");
  message (a, "Data ID: %" PRIu8 "\n", data_id);
  debugMsg ("real_show_vt\n");
  getmaxyx (stdscr, maxy, maxx);
  width = maxx;
  height = (maxy * 2 / 3);
  if ((height < min_height) || (width < min_width))
  {
    message (a,
             "I am deeply sorry, but the Screen is too small\nYou could try increasing it's size by changing window size or screen resolution");
    errMsg ("Not enough space on screen. rows: %d cols: %d\nNeed %d x %d",
            height, width, min_height, min_width);
    return;
  }
  width = min_width;
  height = min_height;
  vt_win = newwin (maxy * 2 / 3 - 2, maxx - 2, 1, 1);
//      wsetscrreg(vt_win,1,maxy*2/3-1);
//      scrollok(vt_win,TRUE);
  do
  {
    svcglob = cwVtGetSvcPk (a->sock, pos, freq, pol, pid, data_id, &num_svc);
    //vt_get_svc_pk(pos,freq,pol,pid,data_id,&num_svc,a->sock);
    magglob =
      cwVtGetMagPk (a->sock, pos, freq, pol, pid, data_id, 1, &num_mag);
    //vt_get_mag_pk(pos,freq,pol,pid,data_id,1,&num_mag,a->sock);
//              message(a,"getting pg: %d,%d\n",vtidx->pid,vtidx->data_id);
    page =
      cwVtGetPagPk (a->sock, pos, freq, pol, pid, data_id, magno, pgno,
                    subno, &num_page);
    //vt_get_pag_pk(pos,freq,pol,pid,data_id,magno,pgno,subno,&num_page,a->sock);
    if ((!page) && (subno == 0))
    {
      subno = 1;
      page =
        cwVtGetPagPk (a->sock, pos, freq, pol, pid, data_id, magno, pgno,
                      subno, &num_page);
      //vt_get_pag_pk(pos,freq,pol,pid,data_id,magno,pgno,subno,&num_page,a->sock);
    }
    if (page)
    {
      mvwprintw (vt_win, 0, 1, "%1.1x%2.2x", (magno == 0) ? 8 : magno, pgno);
      vt_render_pg (vt_win, page, num_page, magglob, num_mag, svcglob,
                    num_svc, a);
    }
    else
    {
      message (a, "no page %1.1x%2.2x-%4.4x\n", (magno == 0) ? 8 : magno,
               pgno, subno);
    }
    wrefresh (vt_win);
//              appDataGetch(a);
    utlFAN (page);
    utlFAN (magglob);
    utlFAN (svcglob);

  }
  while (!parse_pgno (a, vt_win, &magno, &pgno, &subno));
  assert (ERR != delwin (vt_win));
}

void
select_vt_svc (uint32_t vt_pos, uint32_t freq, uint8_t pol, uint16_t vt_pid,
               uint8_t * vtidx, uint16_t num_svc, AppData * a)
{
  MENU *my_menu;
  int i, state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  char **idx_strings;
  int num_strings, lbl_x, vt_sel = 0;

  message (a,
           "Select vt datastream using cursor keys.\nPress Return to display. Press Escape or Backspace to cancel.\n");
  while (state != -1)
  {
    num_strings = num_svc;
    debugMsg ("generating strings: %d\n", num_strings);
    idx_strings = vt_svc_to_strings (vtidx, num_strings);
    state = 0;
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    for (i = 0; i < num_strings; ++i)
    {
      my_items[i] = new_item (idx_strings[i], idx_strings[i]);
      if (NULL == my_items[i])
      {
        if (errno == E_BAD_ARGUMENT)
          errMsg ("badarg i=%d string=%s\n", i, idx_strings[i]);
        if (errno == E_SYSTEM_ERROR)
          errMsg ("new_item: syserr i=%d string=%s\n", i, idx_strings[i]);
      }
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
//              maxy=getmaxy(stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 4;                  //3+(maxx-21-6)/2-strlen(a->current->name)/2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "VT Services:");
    wrefresh (menu_win (my_menu));

    debugMsg ("starting menu loop\n");
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = appDataGetch (a);
      switch (c)
      {
      case 263:
      case K_BACK:
        state = -1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        appDataResizeMnu (my_menu);
        wrefresh (menu_win (my_menu));
        //windows need to be resized.. ignore
        break;
      case 13:
        //tune
        selection = current_item (my_menu);
        vt_sel = item_index (selection);
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
      utlFAN (idx_strings[i]);
    }
    utlFAN (my_items);
    utlFAN (idx_strings);
    if (state == 2)
    {
      real_show_vt (vt_pos, freq, pol, vt_pid, vtidx[vt_sel], a);
    }
  }
}

void
select_vt_pid (uint32_t vt_pos, uint32_t freq, uint8_t pol,
               uint16_t * vt_pids, uint16_t num_pid, AppData * a)
{
  MENU *my_menu;
  int i, state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  char **idx_strings;
  uint8_t *vtsvc = NULL;
  uint16_t num_svc;
  int num_strings, lbl_x, vt_sel = 0;

  message (a,
           "Select vt pid using cursor keys.\nPress Return to display. Press Escape or Backspace to cancel.\n");
  while (state != -1)
  {
    num_strings = num_pid;
    debugMsg ("generating strings: %d\n", num_strings);
    idx_strings = vt_pid_to_strings (vt_pids, num_strings);
    state = 0;
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    for (i = 0; i < num_strings; ++i)
    {
      my_items[i] = new_item (idx_strings[i], idx_strings[i]);
      if (NULL == my_items[i])
      {
        if (errno == E_BAD_ARGUMENT)
          errMsg ("badarg i=%d string=%s\n", i, idx_strings[i]);
        if (errno == E_SYSTEM_ERROR)
          errMsg ("new_item: syserr i=%d string=%s\n", i, idx_strings[i]);
      }
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    lbl_x = 4;                  //3+(maxx-21-6)/2-strlen(a->current->name)/2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "VT Pids:");
    wrefresh (menu_win (my_menu));

    debugMsg ("starting menu loop\n");
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = appDataGetch (a);
      switch (c)
      {
      case 263:
      case K_BACK:
        state = -1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        appDataResizeMnu (my_menu);
        wrefresh (menu_win (my_menu));
        //windows need to be resized.. ignore
        break;
      case 13:
        //tune
        selection = current_item (my_menu);
        vt_sel = item_index (selection);
        vtsvc =
          cwVtGetSvc (a->sock, vt_pos, freq, pol, vt_pids[vt_sel], &num_svc);
        //vt_get_svc(vt_pos,freq,pol,vt_pids[vt_sel],&num_svc,a);
        if (vtsvc)
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
      utlFAN (idx_strings[i]);
    }
    utlFAN (my_items);
    utlFAN (idx_strings);
//        delwin(menu_sub);
//        delwin(menu_win);
    if (state == 2)
    {
      if (num_svc > 1)
        select_vt_svc (vt_pos, freq, pol, vt_pids[vt_sel], vtsvc, num_svc, a);
      else
        real_show_vt (vt_pos, freq, pol, vt_pids[vt_sel], vtsvc[0], a);
      utlFAN (vtsvc);
    }
  }
}

void
show_vt (uint32_t pos, uint32_t freq, uint8_t pol, uint16_t * vt_pids,
         uint16_t num_pids, AppData * a)
{
  uint16_t num_svc;
  uint8_t *vtsvc;
  if (num_pids > 1)
  {
    debugMsg ("multi_pid\n");
    select_vt_pid (pos, freq, pol, vt_pids, num_pids, a);
  }
  else
  {
    debugMsg ("single_pid\n");
    vtsvc = cwVtGetSvc (a->sock, pos, freq, pol, vt_pids[0], &num_svc);
    //vt_get_svc(pos,freq,pol,vt_pids[0],&num_svc,a);
    if (vtsvc)
    {
      if (num_svc > 1)
        select_vt_svc (pos, freq, pol, vt_pids[0], vtsvc, num_svc, a);
      else
        real_show_vt (pos, freq, pol, vt_pids[0], vtsvc[0], a);
    }
    utlFAN (vtsvc);
  }
}


void
show_master_list (uint32_t pos, MENU * parent NOTUSED, AppData * a)
{
  MENU *my_menu;
  int i, idx = 0, maxx, state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  char **tp_strings;
  char **pgm_strings;
  TransponderInfo *tp;
  ProgramInfo *pgms;
  uint32_t num_tp, num_pgm;
  uint16_t *vtidx = NULL, vt_pnr;
  uint16_t num_svc;
  tp = cwGetTransponders (a->sock, pos, &num_tp);
  if (!tp)
    return;
  debugMsg ("generating strings: %" PRIu32 "\n", num_tp);
  tp_strings = tp_to_strings (tp, num_tp);
  if (a->tp_idx >= num_tp)
    a->tp_idx = num_tp - 1;
  if (a->tp_idx < 0)
    a->tp_idx = 0;

  message (a,
           "Select entries using up/down cursor keys. Select Transponders using left/right keys. Press Return to tune. Press Escape or Backspace to cancel. You may also add an entry to the active Favourites by pressing 'a'. Moreover, it is possible to scan the transponder by pressing 's'. e: EPG. t:videotext. r: start recording my currently tuned pgm. c: stop recording\n");
  while (state != -1)
  {
    if (state == 1)
    {
      if (a->tp_idx > 0)
        a->tp_idx--;
    }
    if (state == 2)
    {
      a->tp_idx++;
      if (a->tp_idx >= num_tp)
        a->tp_idx = num_tp - 1;
    }
    state = 0;
    pgms =
      cwGetPgms (a->sock, pos, tp[a->tp_idx].frequency, tp[a->tp_idx].pol,
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
            errMsg ("badarg i=%d string=%s\n", i, pgm_strings[i]);
          if (errno == E_SYSTEM_ERROR)
            errMsg ("new_item: syserr i=%d string=%s\n", i, pgm_strings[i]);
        }
      }
      my_items[num_pgm] = (ITEM *) NULL;
      my_menu = new_menu ((ITEM **) my_items);
      set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
      maxx = getmaxx (stdscr);
      appDataMakeMenuWnd (my_menu);
      post_menu (my_menu);
      lbl_x = 3 + (maxx - 6) / 2 - strlen (tp_strings[a->tp_idx]) / 2;
      if (a->tp_idx > 0)
        mvwaddch (menu_win (my_menu), 1, 1, '<');
      if (a->tp_idx < (num_tp - 1))
        mvwaddch (menu_win (my_menu), 1, maxx - 2, '>');
      mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", tp_strings[a->tp_idx]);
      wrefresh (menu_win (my_menu));

      debugMsg ("starting menu loop\n");
      while (state == 0)
      {
        int c;
        ITEM *selection;
        c = appDataGetch (a);
        //      mvprintw(0,0,"%d\n",c);
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
          appDataResizeMnu (my_menu);
          wrefresh (menu_win (my_menu));
//                                      refresh();
          //windows need to be resized.. ignore
          break;
        case 'a':
          //add to current favourites list
          selection = current_item (my_menu);
          add_to_current_favlist (pos, &tp[a->tp_idx],
                                  &pgms[item_index (selection)], a);
          break;
        case 's':
          cwScanTp (a->sock, pos, tp[a->tp_idx].frequency, tp[a->tp_idx].pol);
          state = 3;
          break;
        case 'e':
//                                      if( (epg=get_epg(pos,&tp[a->tp_idx],pgms,num_pgm,a)) )
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 4;
          break;
        case 't':              //prepare the videotext display
          /*
             find the vt services for the selected pgm
             if there's more than one, show selection choice
             and display page100
           */
          selection = current_item (my_menu);
          idx = item_index (selection);
          vtidx =
            cwVtGetPids (a->sock, pos, tp[a->tp_idx].frequency,
                         tp[a->tp_idx].pol, vt_pnr =
                         pgms[item_index (selection)].pnr, &num_svc);
          //vt_get_pids(pos,tp[a->tp_idx].frequency,tp[a->tp_idx].pol,vt_pnr=pgms[item_index(selection)].pnr,&num_svc,a);
          if (vtidx)
            state = 5;
          break;
        case 'r':              //record
          selection = current_item (my_menu);
          //      item_index(selection);
          if (cwRecordStart (a->sock))
            message (a,
                     "recording failed\n Perhaps there's no pgm tuned for _this_ remote?\n");
          else
            message (a, "recording started\n");
          break;
        case 'c':              //stop(cancel) recording
          if (cwRecordStop (a->sock))
            message (a, "stopping failed\n");
          else
            message (a, "recording stopped\n");
          break;
        case 13:
          //tune
          selection = current_item (my_menu);
          tune_pgm (pos, &tp[a->tp_idx], &pgms[item_index (selection)], a);
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
      if (state == 4)
      {
        show_epg (pos, tp[a->tp_idx].frequency, tp[a->tp_idx].pol, pgms,
                  num_pgm, pgm_strings, idx, a);
      }
      if (state == 5)
      {
        show_vt (pos, tp[a->tp_idx].frequency, tp[a->tp_idx].pol, vtidx,
                 num_svc, a);
        utlFAN (vtidx);
      }
      for (i = 0; i < num_pgm; ++i)
      {
        utlFAN (pgm_strings[i]);
        programInfoClear (&pgms[i]);
      }
      utlFAN (pgm_strings);
      utlFAN (pgms);

      if (state == 3)           //transponders may have been added
      {
        free_strings (tp_strings, num_tp);
        utlFAN (tp);

        tp = cwGetTransponders (a->sock, pos, &num_tp);
        if (!tp)
          return;
        debugMsg ("generating strings: %" PRIu32 "\n", num_tp);
        tp_strings = tp_to_strings (tp, num_tp);
        if (a->tp_idx >= num_tp)
          a->tp_idx = num_tp - 1;
        if (a->tp_idx < 0)
          a->tp_idx = 0;
      }
    }
    else
    {
      int lbl_x;
      WINDOW *wnd;
      wnd = appDataMakeEmptyWnd ();
      maxx = getmaxx (stdscr);

//                      menu_win=newwin(maxy*2/3,maxx-21,0,20+1);
      if (!wnd)
        errMsg ("newwin failed\n");
//                      box(menu_win,0,0);
      lbl_x = 3 + (maxx - 6) / 2 - strlen (tp_strings[a->tp_idx]) / 2;
      if (a->tp_idx > 0)
        mvwaddch (wnd, 1, 1, '<');
      if (a->tp_idx < (num_tp - 1))
        mvwaddch (wnd, 1, maxx - 2, '>');
      if (ERR == mvwprintw (wnd, 1, lbl_x, "%s", (tp_strings[a->tp_idx])))
        errMsg ("printw failed\n");
      debugMsg (tp_strings[a->tp_idx]);
      wrefresh (wnd);

      while (state == 0)
      {
        int c;
        c = appDataGetch (a);
//                              mvprintw(0,0,"%d\n",c);
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
        case 's':
          cwScanTp (a->sock, pos, tp[a->tp_idx].frequency, tp[a->tp_idx].pol);
          state = 3;
          break;
        default:
          break;
        }
      }
      assert (ERR != delwin (wnd));
      if (state == 3)           //transponders may have been added
      {
        free_strings (tp_strings, num_tp);
        utlFAN (tp);
        tp = cwGetTransponders (a->sock, pos, &num_tp);
        if (!tp)
          return;
        debugMsg ("generating strings: %" PRIu32 "\n", num_tp);
        tp_strings = tp_to_strings (tp, num_tp);
        if (a->tp_idx >= num_tp)
          a->tp_idx = num_tp - 1;
        if (a->tp_idx < 0)
          a->tp_idx = 0;
      }
    }
  }
  free_strings (tp_strings, num_tp);
  utlFAN (tp);
}

void
show_pos (MENU * parent, AppData * a)
{
  MENU *my_menu;
  uint32_t i, idx;
  int maxx, state = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_pos = cwListPos (a->sock);
  //list_pos(a)
  idx = 0;
  if (!num_pos)
    return;

  message (a, "Select Satellite position to browse, please.\n");
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
      c = appDataGetch (a);
//                      mvprintw(0,0,"%d\n",c);
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
        appDataResizeMnu (my_menu);
        wrefresh (menu_win (my_menu));
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
//        delwin(menu_sub);
//        delwin(menu_win);
    if (state == 1)
    {
      show_master_list (idx, parent, a);
    }
  }
}

void
show_fav_list (MENU * parent NOTUSED, AppData * a)
{
  MENU *my_menu;
  int i, maxx, state = 0, idx = 0;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  char **fav_strings;
  char **pgm_names;
  Favourite *cfav = NULL;
  int num_strings, lbl_x;
  uint16_t *vtidx = NULL;
  uint16_t num_svc;

  if (!a->current)
  {
    errMsg ("Sorry, no FavList selected\n");
    return;
  }
  if (!a->current->size)
  {
    errMsg ("Sorry, current FavList is empty\n");
    return;
  }
  message (a,
           "Select entries using cursor keys.\nPress Return to tune. r: start recording my currently tuned pgm. c: stop recording. e: show EPG, t: show Vt. Press Escape or Backspace to cancel. You may also delete an entry by pressing 'd'. 'f' moves an entry upward, 'v' moves down.\n");
  while (state != -1)
  {
    num_strings = a->current->size;
    debugMsg ("generating strings: %d\n", num_strings);
    fav_strings = fav_to_strings (a->current->favourites, num_strings);
    state = 0;
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    for (i = 0; i < num_strings; ++i)
    {
      my_items[i] = new_item (fav_strings[i], fav_strings[i]);
      if (NULL == my_items[i])
      {
        if (errno == E_BAD_ARGUMENT)
          errMsg ("badarg i=%d string=%s\n", i, fav_strings[i]);
        if (errno == E_SYSTEM_ERROR)
          errMsg ("new_item: syserr i=%d string=%s\n", i, fav_strings[i]);
      }
    }
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    post_menu (my_menu);
    if (idx >= num_strings)
      idx = num_strings - 1;
    if (idx < 0)
      idx = 0;
    set_current_itemXX (my_menu, my_items[idx]);        //FIXXME
    lbl_x = 3 + (maxx - 6) / 2 - strlen (a->current->name) / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", a->current->name);
    wrefresh (menu_win (my_menu));

    debugMsg ("starting menu loop\n");
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = appDataGetch (a);
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
        appDataResizeMnu (my_menu);
        wrefresh (menu_win (my_menu));
        //windows need to be resized.. ignore
        break;
      case 'd':
        //delete entry
        selection = current_item (my_menu);
        del_from_current_favlist (item_index (selection), a);
        state = 2;
        break;
      case 13:
        //tune
        selection = current_item (my_menu);
        tune_fav (&a->current->favourites[item_index (selection)], a);
        break;
      case 'e':
//                                      if( (epg=get_epg(pos,&tp[a->tp_idx],pgms,num_pgm,a)) )
        selection = current_item (my_menu);
        idx = item_index (selection);
        state = 4;
        break;
      case 't':                //prepare the videotext display
        /*
           find the vt services for the selected pgm
           if there's more than one, show selection choice
           and display page100
         */
        selection = current_item (my_menu);
        idx = item_index (selection);
        cfav = &a->current->favourites[item_index (selection)];
        vtidx =
          cwVtGetPids (a->sock, cfav->pos, cfav->freq, cfav->pol,
                       cfav->pnr, &num_svc);
        //vt_get_pids(cfav->pos,cfav->freq,cfav->pol,cfav->pnr,&num_svc,a);
        if (vtidx)
          state = 5;
        break;
      case 'r':                //record
        selection = current_item (my_menu);
        //      item_index(selection);
        if (cwRecordStart (a->sock))
          message (a,
                   "recording failed\n Perhaps there's no pgm tuned for _this_ remote?\n");
        else
          message (a, "recording started\n");
        break;
      case 'c':                //stop(cancel) recording
        if (cwRecordStop (a->sock))
          message (a, "stopping failed\n");
        else
          message (a, "recording stopped\n");
        break;
      case 'f':                //move entry up
        selection = current_item (my_menu);
        idx = item_index (selection);
        if (idx > 0)
        {
          Favourite tmp = a->current->favourites[idx - 1];
          a->current->favourites[idx - 1] = a->current->favourites[idx];
          a->current->favourites[idx] = tmp;
          idx--;
        }

        state = 1;
        break;
      case 'v':                //down
        selection = current_item (my_menu);
        idx = item_index (selection);
        if (idx < (num_strings - 1))
        {
          Favourite tmp = a->current->favourites[idx + 1];
          a->current->favourites[idx + 1] = a->current->favourites[idx];
          a->current->favourites[idx] = tmp;
          idx++;
        }
        state = 1;
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    switch (state)
    {
    case 4:
      pgm_names = fav_to_names (a->current->favourites, num_strings);
      show_epg_fav (a->current->favourites, a->current->size, pgm_names, a);
      free_strings (pgm_names, num_strings);
      break;
    case 5:
      show_vt (cfav->pos, cfav->freq, cfav->pol, vtidx, num_svc, a);
      utlFAN (vtidx);
      break;
    default:
      break;
    }
    for (i = 0; i < num_strings; ++i)
    {
      free_item (my_items[i]);
      utlFAN (fav_strings[i]);
    }
    utlFAN (my_items);
    utlFAN (fav_strings);
//        delwin(menu_sub);
//        delwin(menu_win);
  }
}

void
show_select_list (MENU * parent NOTUSED, AppData * a)
{
  MENU *my_menu;
  int i, maxx, state = 0;
  Iterator l;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  int num_strings, lbl_x;

  if (!a->favourite_lists)
  {
    errMsg ("Sorry, no FavLists here. Create some, please.\n");
    return;
  }
  if (iterBTreeInit (&l, a->favourite_lists))
  {
    errMsg ("error initializing iterator.\n");
    return;
  }

  message (a,
           "Select the list to use, please.\nPress Return to select entry. Use Escape or Backspace to cancel the action.\n");
  num_strings = iterGetSize (&l);
  state = 0;
  debugMsg ("allocating items\n");
  my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
  for (i = 0; i < num_strings; ++i)
  {
    FavList *fl;
    iterSetIdx (&l, i);
    fl = iterGet (&l);
    if (0 == strlen (fl->name))
      my_items[i] = new_item ("<empty>", "<empty>");
    else
      my_items[i] = new_item (fl->name, fl->name);
    if (NULL == my_items[i])
    {
      if (errno == E_BAD_ARGUMENT)
        errMsg ("badarg i=%d string=%s\n", i, fl->name);
      if (errno == E_SYSTEM_ERROR)
        errMsg ("new_item: syserr i=%d string=%s\n", i, fl->name);
    }
  }
  my_items[num_strings] = (ITEM *) NULL;
  my_menu = new_menu ((ITEM **) my_items);
  set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
  maxx = getmaxx (stdscr);
  appDataMakeMenuWnd (my_menu);
  post_menu (my_menu);
  lbl_x = 3 + (maxx - 6) / 2 - strlen ("Lists") / 2;
  mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Lists");
  wrefresh (menu_win (my_menu));
  debugMsg ("starting menu loop\n");
  while (state == 0)
  {
    int c;
    ITEM *selection;
    c = appDataGetch (a);
    switch (c)
    {
    case 263:                  //ESC
    case K_BACK:               //Backspace
      state = -1;
      break;
    case KEY_DOWN:
      menu_driver (my_menu, REQ_DOWN_ITEM);
      break;
    case KEY_UP:
      menu_driver (my_menu, REQ_UP_ITEM);
      break;
    case KEY_RESIZE:
      appDataResizeMnu (my_menu);
      wrefresh (menu_win (my_menu));
      //windows need to be resized.. ignore
      break;
    case 13:
      //select
      selection = current_item (my_menu);
      iterSetIdx (&l, item_index (selection));
      a->current = iterGet (&l);
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
  utlFAN (my_items);
  iterClear (&l);
//  delwin(menu_sub);
//  delwin(menu_win);
}

int
compare_fav_list (void *x NOTUSED, const void *a, const void *b)
{
  FavList *fa = (FavList *) a, *fb = (FavList *) b;
  return strcmp (fa->name, fb->name);
}

//return 1 if printable string
static int
string_is_print (char *s)
{
  char *p;
  p = s;
  while (*p)
  {
    if (!isprint (*p))
      return 0;
    p++;
  }
  return 1;
}

void
show_add_list (MENU * parent NOTUSED, AppData * a)
{
  WINDOW *menu_w, *menu_s;
  BTreeNode *new_node;
  int maxx;
  const int BUFSZ = 1024;
  FavList *fl;
  maxx = getmaxx (stdscr);
  menu_w = newwin (5, maxx, 0, 0);
  menu_s = derwin (menu_w, 1, maxx - 2, 3, 1);
  mvwprintw (menu_w, 1, 2, "%s", "Enter a name for the list, please");
  box (menu_w, 0, 0);
  wrefresh (menu_w);
  new_node = utlCalloc (1, btreeNodeSize (sizeof (FavList)));
  fl = btreeNodePayload (new_node);
  fl->name = utlCalloc (BUFSZ, sizeof (char));
  fl->size = 0;
  fl->favourites = NULL;
//      def_prog_mode();
//      setsyx(4,20+2);
  nl ();
  nodelay (menu_s, FALSE);
  echo ();
  nocbreak ();
  keypad (menu_s, FALSE);
  mvwgetnstr (menu_s, 0, 0, fl->name, BUFSZ);
  fl->name[BUFSZ - 1] = '\0';
  fl->name = utlRealloc (fl->name, strlen (fl->name) + 1);
  nonl ();
  noecho ();
  raw ();
  keypad (menu_s, TRUE);
  if (!string_is_print (fl->name))
  {
    utlFAN (fl->name);
    utlFAN (new_node);
    message (a, "Not a printable string!\n");
    return;
  }
  if (0 == strlen (fl->name))
  {
    utlFAN (fl->name);
    utlFAN (new_node);
    message (a, "Empty string!\n");
    return;
  }
  if (!fl->name)
  {
    utlFAN (new_node);
    return;
  }
//      reset_prog_mode();
  if (btreeNodeInsert (&a->favourite_lists, new_node, NULL, compare_fav_list))
  {
    errMsg
      ("List with that name already exists. Sorry, but they must be unique.\n");
    utlFAN (fl->name);
    utlFAN (new_node);
    return;
  }
  message (a, "Added List: %s, thanks.\n", fl->name);
//  debugMsg ("Added List: %s, thanks.\n", fl->name);
}

int
edit_re (Reminder * e, AppData * a)
{
  MENU *my_menu;
  uint32_t i, maxx;
  int state = 0;
  Reminder tmpe = *e;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;

  message (a,
           "Select and press enter to edit. Use c to cancel and backspace to exit.\n");

  while (state >= 0)
  {
    num_strings = 2;
    state = 0;
    debugMsg ("allocating items\n");
    my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
    my_items[0] = new_item ("Type", rsGetTypeStr (tmpe.type));
    my_items[1] = new_item ("Days", "");
    my_items[num_strings] = (ITEM *) NULL;
    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_NONCYCLIC | O_ROWMAJOR);
    appDataMakeMenuWnd (my_menu);
    maxx = getmaxx (stdscr);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Entry") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Entry");
    post_menu (my_menu);
    debugMsg ("starting menu loop\n");
    wrefresh (menu_win (my_menu));
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = appDataGetch (a);
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
        appDataResizeMnu (my_menu);
        wrefresh (menu_win (my_menu));
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
//        delwin(menu_sub);
//        delwin(menu_win);
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


void
show_rlist (AppData * a)
{
  MENU *my_menu;
  uint32_t maxx;
  int state = 0;
  void *u_p = NULL;
  ITEM **my_items = NULL;
//      WINDOW * menu_win,*menu_sub;
  int num_strings, lbl_x;

  reminderListUpdate (&a->reminder_list, time (NULL));
  message (a,
           "Select and press enter to change type. Use d to delete a reminder. Backspace exits.\n");
  while (state >= 0)
  {
    my_items = rem_to_items (&a->reminder_list, &num_strings);
    if (NULL == my_items)
    {
      message (a, "No reminders.\n");
      return;
    }
    state = 0;
    debugMsg ("allocating menu\n");
    my_menu = new_menu ((ITEM **) my_items);
    appDataMakeMenuWnd (my_menu);
    maxx = getmaxx (stdscr);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Reminders") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Reminders");
    post_menu (my_menu);
    debugMsg ("starting menu loop\n");
    wrefresh (menu_win (my_menu));
    while (state == 0)
    {
      int c;
      ITEM *selection;
      c = appDataGetch (a);
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
        appDataResizeMnu (my_menu);
        wrefresh (menu_win (my_menu));
        break;
      case 'd':
        selection = current_item (my_menu);
        u_p = item_userptr (selection);
//                      idx=item_index(selection);
        state = 1;
        break;
      case 13:                 //enter
        selection = current_item (my_menu);
        u_p = item_userptr (selection);
//                      idx=item_index(selection);
        state = 3;
        break;
      default:
        break;
      }
      wrefresh (menu_win (my_menu));
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    rem_items_free (my_items);
//        delwin(menu_sub);
//        delwin(menu_win);
    if (state > 0)
    {
      Reminder *rem;
      switch (state)
      {
      case 1:                  //delete
        dlistRemove (&a->reminder_list, u_p);
        reminderDel (u_p);
        break;
      case 3:                  //edit
        rem = dlistEPayload (u_p);
        edit_re (rem, a);
//                      select_re_type(&rem->type,a);
        break;
      default:
        break;
      }
    }
  }
}

/*
RsEntry *clone_sched(RsEntry * src,uint32_t src_sz)
{
	RsEntry * rv;
	uint32_t i;
	rv=utlCalloc(src_sz,sizeof(RsEntry));
	if(NULL==rv)
		return NULL;
	
	for(i=0;i<src_sz;i++)
	{
		rsClone(rv+i,src+i);
	}
	return rv;
}
*/

/*
void sched_destroy(RsEntry * sch,uint32_t sz)
{
	uint32_t i;
	for(i=0;i<sz;i++)
	{
		rsClearEntry(sch+i);
	}
	utlFAN(sch);
}
*/
static const char *client_error_str[] = {
  "No Error",
  "Generic Error",
  "undefined",
  "undefined",
  "Not permitted"
};


static char *
client_strerror (uint8_t e)
{
  return (char *) struGetStr (client_error_str, e);
}

static void
print_active_rec (AppData * a, WINDOW * wnd)
{
  uint8_t cmd = SRV_RSTAT;
  bool got_entry;
  uint8_t res, done = 0;
  ipcSndS (a->sock, cmd);
  ipcRcvS (a->sock, res);
  if (res != SRV_OK)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return;
  }
  got_entry = false;
  done = 0;
  wprintw (wnd, "client initiated:\n");
  while (!done)
  {
    struct sockaddr ad;
    uint8_t super;
    char buf[256] = { 0 };
    time_t t;
    off_t sz;
    uint16_t pnr;
    ipcRcvS (a->sock, res);
    switch (res)
    {
    case 0xff:
      ipcRcvS (a->sock, ad.sa_family);
      ioBlkRcv (a->sock, ad.sa_data, sizeof (ad.sa_data));
      ipcRcvS (a->sock, super);
      ipcRcvS (a->sock, pnr);
      ipcRcvS (a->sock, t);
      ipcRcvS (a->sock, sz);
      ioNtoP ((struct sockaddr_storage *) &ad, buf, sizeof (buf));
      wprintw (wnd,
               "IP: %s PNR: %5.5" PRIu16
               " Duration: %2.2u:%2.2u.%2.2u Size: %luMiB\n", buf, pnr,
               t / 3600, (t / 60) % 60, t % 60, sz / (1024 * 1024));
      got_entry = true;
      break;
    case 0:
      done = 1;
      break;
    }
  }
  if (!got_entry)
    wprintw (wnd, "(none)\n");

  done = 0;
  got_entry = false;
  wprintw (wnd, "schedule initiated:\n");
  while (!done)
  {
    RsEntry sch;
    uint8_t eit_state;
    time_t st;
    time_t last_eit;
    time_t last_match;

    ipcRcvS (a->sock, res);
    switch (res)
    {
    case 0xfc:
      rsRcvEntry (&sch, a->sock);
      ipcRcvS (a->sock, eit_state);
      ipcRcvS (a->sock, st);
      ipcRcvS (a->sock, last_eit);
      ipcRcvS (a->sock, last_match);
      wprintw (wnd, "PNR: %5.5u ", sch.pnr);
      ipcRcvS (a->sock, res);
      got_entry = true;
      if (res == 0xfd)
      {
        time_t t;
        off_t sz;
        uint16_t pnr;
        ipcRcvS (a->sock, pnr);
        ipcRcvS (a->sock, t);
        ipcRcvS (a->sock, sz);
        wprintw (wnd, "Duration: %2.2u:%2.2u.%2.2u Size: %luMiB\n",
                 t / 3600, (t / 60) % 60, t % 60, sz / (1024 * 1024));
      }
      else
      {
        wprintw (wnd, "Scanning\n");
      }
      break;
    case 1:
      done = 1;
      break;
    }
  }
  if (!got_entry)
    wprintw (wnd, "(none)\n");

}

static void
show_rstat (AppData * a)
{
  WINDOW *outer;
  WINDOW *wnd;
  int maxy, maxx;
  uint8_t valid, tuned, pol;
  uint32_t pos, freq, ifreq;
  int32_t fc, ft;

  nodelay (stdscr, TRUE);
  do
  {

    outer = appDataMakeEmptyWnd ();
    getmaxyx (stdscr, maxy, maxx);
    wnd = derwin (outer, (maxy * 2 / 3) - 2, maxx - 2, 1, 1);
    wsetscrreg (wnd, 1, (maxy * 2 / 3) - 2);
    scrollok (wnd, TRUE);


    clientGetTstat (a->sock, &valid, &tuned, &pos, &freq, &pol, &ft, &fc,
                    &ifreq);

    if (tuned)
    {
      wprintw (wnd,
               "Tuned to: %2.2" PRIu32 ":%3.3" PRIu32 ",%5.5" PRIu32 "%s\n",
               pos, freq / 100000, freq % 100000, tpiGetPolStr (pol));
      print_active_rec (a, wnd);
    }
    else
    {
      wprintw (wnd, "Not Tuned\n");
    }
    wprintw (wnd, "Press any key to return\n");
    wrefresh (outer);
    wrefresh (wnd);
    sleep (1);
    assert (ERR != delwin (wnd));
    assert (ERR != delwin (outer));
  }
  while (ERR == getch ());

  nodelay (stdscr, FALSE);
}


void
show_sched (AppData * a)
{
  uint32_t i;
  RsEntry *r_sch = NULL;
  RsEntry *prev_sch = NULL;
  uint32_t sch_sz = 0;
  uint32_t prev_sz = 0;
  MENU *my_menu;
  uint32_t maxx;
  int state = 0;
  ITEM **my_items;
  char **mnu_str;
//      WINDOW * menu_win,*menu_sub;
  uint32_t num_strings;
  int lbl_x, idx = 0;

  message (a,
           "Select and press enter to edit. Use d to delete an entry, n to add a new one, a to abort any entries active on the server. s to show server status. backspace to exit.\n");
  while (state >= 0)
  {
    uint8_t rv = cwGetSched (a->sock, &prev_sch, &prev_sz);
    if (rv)
      return;

    if (0 != prev_sz)
    {
      r_sch = rsCloneSched (prev_sch, prev_sz);
      if (NULL == r_sch)
      {
        rsDestroy (prev_sch, prev_sz);
        return;
      }
      sch_sz = prev_sz;

      mnu_str = rse_to_strings (r_sch, sch_sz);
      num_strings = sch_sz;
      state = 0;
      debugMsg ("allocating items\n");
      my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
      for (i = 0; i < num_strings; ++i)
      {
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
      maxx = getmaxx (stdscr);
//                      menu_win=newwin(maxy*2/3,maxx-21,0,20+1);
      lbl_x = 3 + (maxx - 6) / 2 - strlen ("Schedule") / 2;
      mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Schedule");
      post_menu (my_menu);
      debugMsg ("starting menu loop\n");
      wrefresh (menu_win (my_menu));
      while (state == 0)
      {
        int c;
        ITEM *selection;
        c = appDataGetch (a);
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
          appDataResizeMnu (my_menu);
          wrefresh (menu_win (my_menu));
          break;
        case 'd':
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 1;
          break;
        case 'a':
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 2;
          break;
        case 'n':
          state = 4;
          break;
        case 's':
          state = 5;
          break;
        case 13:               //enter
          selection = current_item (my_menu);
          idx = item_index (selection);
          state = 3;
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
      free_strings (mnu_str, num_strings);
      utlFAN (my_items);
    }
    else
    {
      int lbl_x;
      WINDOW *wnd;
      r_sch = NULL;
      sch_sz = 0;
      state = 0;
      maxx = getmaxx (stdscr);
      wnd = appDataMakeEmptyWnd ();
      if (!wnd)
      {
        errMsg ("newwin failed\n");
        return;
      }
      lbl_x = 3 + (maxx - 6) / 2 - strlen ("Schedule") / 2;
      mvwprintw (wnd, 1, lbl_x, "%s", "Schedule");
      wrefresh (wnd);

      while (state == 0)
      {
        int c;
        c = appDataGetch (a);
        switch (c)
        {
        case 263:              //ESC
        case K_BACK:           //Backspace
          state = -1;
          break;
        case KEY_RESIZE:
          wrefresh (wnd);
          break;
        case 'a':
          state = 2;
          break;
        case 'n':
          state = 4;
          break;
        case 's':
          state = 5;
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
      case 1:                  //delete
        rsClearEntry (r_sch + idx);
        memmove (r_sch + idx, r_sch + idx + 1,
                 (sch_sz - idx - 1) * sizeof (RsEntry));
        sch_sz--;
        if (cwRecSched (a->sock, r_sch, sch_sz, prev_sch, prev_sz))
          message (a, "REC_SCHED failed\n");
        break;
      case 2:                  //abort running schedule entries
        cwRecSAbort (a->sock);
        break;
      case 3:                  //edit
        if (!edit_rse (r_sch + idx, a))
        {
          if (cwRecSched (a->sock, r_sch, sch_sz, prev_sch, prev_sz))
            message (a, "REC_SCHED failed\n");
        }
        break;
      case 4:                  //new
        {
          void *n;
          n = utlRealloc (r_sch, sizeof (RsEntry) * (sch_sz + 1));
          if (n)
          {
            r_sch = n;
            memset (r_sch + sch_sz, 0, sizeof (RsEntry));
            if (!edit_rse (r_sch + sch_sz, a))
            {
              sch_sz++;
              if (cwRecSched (a->sock, r_sch, sch_sz, prev_sch, prev_sz))
                message (a, "REC_SCHED failed\n");
            }
          }
        }
        break;
      case 5:                  //running recording status
        show_rstat (a);
        break;
      default:
        break;
      }
    }
    rsDestroy (prev_sch, prev_sz);
    rsDestroy (r_sch, sch_sz);
  }
}

void
switch_setup (MENU * parent NOTUSED, AppData * a)
{
  uint32_t i;
  uint32_t num = 0;
//      uint8_t cmd=GET_SWITCH;
  uint8_t res;
  SwitchPos *sp = NULL;

//      ipcSndS(a->sock,cmd);

//      ipcRcvS(a->sock,res);
  res = cwGetSwitch (a->sock, &sp, &num);
//      if(res!=SRV_NOERR)
  if (res)
    return;

//      ipcRcvS(a->sock,num);
//      debugMsg("%u positions\n",num);
/*
	if(num)
	{
		sp=utlCalloc(num,sizeof(SwitchPos));
		if(sp)
		for(i=0;i<num;i++)
		{
			spRcv(sp+i,a->sock);
		}
		else
		{
			SwitchPos dummy;
			errMsg("allocation failed\n");
			for(i=0;i<num;i++)
			{
				spRcv(&dummy,a->sock);
			}
			return;
		}
	}
	*/
  if (edit_posns (&num, &sp, a))
  {
    debugMsg ("edit_posns cancelled\n");
    for (i = 0; i < num; i++)
    {
      spClear (sp + i);
    }
    utlFAN (sp);
    return;
  }
  cwSetSwitch (a->sock, sp, num);
  /*
     cmd=SET_SWITCH;
     ipcSndS(a->sock,cmd);
     ipcRcvS(a->sock,res);
     if(res!=SRV_NOERR)
     {
     for(i=0;i<num;i++)
     {
     spClear(sp+i);
     }
     utlFAN(sp);
     return;
     }

     ipcSndS(a->sock,num);
   */
  for (i = 0; i < num; i++)
  {
    spClear (sp + i);
  }
  utlFAN (sp);
}

void
show_del_list (MENU * parent NOTUSED, AppData * a)
{
  MENU *my_menu;
  int i, maxx, state = 0;
  Iterator l;
  ITEM **my_items;
//      WINDOW * menu_win,*menu_sub;
  int num_strings, lbl_x;

  if (!a->favourite_lists)
  {
    errMsg ("Sorry, no FavLists here. Create some, please.\n");
    return;
  }
  if (iterBTreeInit (&l, a->favourite_lists))
  {
    errMsg ("error initializing iterator.\n");
    return;
  }

  message (a,
           "Select the list to delete, please.\nPress Return to delete entry. Press Escape or Backspace to cancel.\n");
  num_strings = iterGetSize (&l);
  state = 0;
  debugMsg ("allocating items\n");
  my_items = (ITEM **) utlCalloc (num_strings + 1, sizeof (ITEM *));
  for (i = 0; i < num_strings; ++i)
  {
    FavList *fl;
    iterSetIdx (&l, i);
    fl = iterGet (&l);
    if (0 == strlen (fl->name))
      my_items[i] = new_item ("<empty>", "<empty>");
    else
      my_items[i] = new_item (fl->name, fl->name);
    if (NULL == my_items[i])
    {
      if (errno == E_BAD_ARGUMENT)
        errMsg ("badarg i=%d string=%s\n", i, fl->name);
      if (errno == E_SYSTEM_ERROR)
        errMsg ("new_item: syserr i=%d string=%s\n", i, fl->name);
    }
  }
  my_items[num_strings] = (ITEM *) NULL;
  my_menu = new_menu ((ITEM **) my_items);
  set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
  maxx = getmaxx (stdscr);
  appDataMakeMenuWnd (my_menu);
  post_menu (my_menu);
  lbl_x = 3 + (maxx - 6) / 2 - strlen ("Lists") / 2;
  mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Lists");
  wrefresh (menu_win (my_menu));
  debugMsg ("starting menu loop\n");
  while (state == 0)
  {
    int c;
    ITEM *selection;
    c = appDataGetch (a);
    switch (c)
    {
    case 263:                  //ESC
    case K_BACK:               //Backspace
      state = -1;
      break;
    case KEY_DOWN:
      menu_driver (my_menu, REQ_DOWN_ITEM);
      break;
    case KEY_UP:
      menu_driver (my_menu, REQ_UP_ITEM);
      break;
    case KEY_RESIZE:
      appDataResizeMnu (my_menu);
      wrefresh (menu_win (my_menu));
      //windows need to be resized.. ignore
      break;
    case 13:
      //delete
      selection = current_item (my_menu);
      iterSetIdx (&l, item_index (selection));
//              a->current=iterGet(&l);
      btreeNodeRemove (&a->favourite_lists, btreePayloadNode (iterGet (&l)));
      favListClear (iterGet (&l));
      utlFree (btreePayloadNode (iterGet (&l)));        //hmmmm...
      a->current = NULL;
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
  utlFAN (my_items);
  iterClear (&l);
}

static char *
get_rdesc (Reminder * r)
{
  char *sn = NULL, *en = NULL;
  unsigned sz = 0;
  char *rv = NULL;
  sn = dvbStringToAscii (&r->service_name);
  utlSmartCat (&rv, &sz, r->event_name);
  utlSmartCat (&rv, &sz, "@");
  utlSmartCat (&rv, &sz, sn);
  utlFAN (en);
  utlFAN (sn);
  utlSmartCatDone (&rv);
  utlAsciiSanitize (rv);
  return rv;
}

#define RMODE_WAIT 0
#define RMODE_RUN 1
static void
start_rmode (AppData * a)
{
  int ending = 0;
  int tries = 0;
  long max_tries;
  int state = RMODE_WAIT;
  DListE *e = NULL;
  DListE *next = NULL;
  //set no delay mode and use ERR!=getch() to break out of our loop
  nodelay (stdscr, TRUE);
  reminderListUpdate (&a->reminder_list, time (NULL));  //remove stale reminders
  max_tries = 0;
  rcfileFindValInt (&a->cfg, "RMD", "retries", &max_tries);
  message (a,
           "Starting reminder mode.\n Will auto tune reminded events as long as I am active.\n Press a key to exit reminder mode.\n");
  e = dlistFirst (&a->reminder_list);
  while (!ending)
  {
    sleep (1);
    if (NULL == e)
    {
      message (a, "No reminders available\n");
      ending = 1;
    }
    else
    {
      time_t now = time (NULL);
      Reminder *current = dlistEPayload (e);
      Reminder *following = NULL;
      switch (state)
      {
      case RMODE_WAIT:
        if (reminderIsReady (current))
        {
          char *rdesc = get_rdesc (current);
          if ((tries <= max_tries) || (max_tries < 0))
          {
            message (a,
                     "Tuning to reminder %s:\n Pos: %u, Freq: %u ,Pol: %s, Pnr: %hu Duration: %d Minutes %d Seconds\n",
                     rdesc, current->pos, current->freq,
                     tpiGetPolStr (current->pol), current->pnr,
                     current->duration / 60, current->duration % 60);
            utlFAN (rdesc);
            if (!cwDoTune
                (a->sock, current->pos, current->freq, current->pol,
                 current->pnr))
            {                   //successful
              message (a, "Success\n");
              state = RMODE_RUN;
            }
            else
            {
              message (a, "Failed\n Are we active?\n");
              tries++;
              //should check here for expiration etc..
            }
          }
          else
          {
            state = RMODE_RUN;
          }
        }
        break;
      case RMODE_RUN:
        next = dlistENext (e);
        if (NULL != next)
        {                       //there's a next reminder
          following = dlistEPayload (next);
          if (reminderIsReady (following))
          {                     //it's starting
            reminderDone (&a->reminder_list, e);
            e = dlistFirst (&a->reminder_list);
            state = RMODE_WAIT; //continue above
            tries = 0;
          }                     //else do nothing
        }
        else
        {                       //no next rem
          if ((current->start + current->duration) < now)
          {                     //reminder ended
            if (reminderDone (&a->reminder_list, e))
            {                   //it's gone
              message (a, "No more reminders\n");
              ending = 1;
            }
            else
            {                   //rescheduled
              e = dlistFirst (&a->reminder_list);
              state = RMODE_WAIT;       //continue above
              tries = 0;
            }
          }
        }
        break;
      }
    }
    if (ERR != appDataGetch (a))
      ending = 1;
  }
  message (a, "Ending reminder mode.\n");
  nodelay (stdscr, FALSE);
}

static void
draw_dvbflags (uint16_t fe_status, int x, int y)
{
  static const char *fe_ft_stat[] = {
    "SIGN ",
    "CARR ",
    "VITE ",
    "SYNC ",
    "LOCK ",
    "TMEO ",
    "REIN "
  };
  static char *empty = "     ";
  char buffer[256];
  unsigned j;
  strcpy (buffer, "Status: ");
  for (j = 0; j < 7; j++)
  {
    char *p;
    if (fe_status & (1 << j))
      p = (char *) struGetStr (fe_ft_stat, j);
    else
      p = empty;
    strcat (buffer, p);
  }
  mvprintw (y, x, "%s", buffer);
}

static void
draw_fcorr (int32_t fc, uint32_t ifreq, int x, int y)
{
  mvprintw (y, x,
            "Frontend freq offset: % 10.10" PRId32 ". ifreq: %10.10" PRIu32,
            fc, ifreq);
}

static void
draw_tpft (uint32_t pos, uint32_t freq, uint8_t pol, int32_t tpft, int x,
           int y)
{
  mvprintw (y, x,
            "Tp: %2.2" PRIu32 ":%3.3" PRIu32 ",%5.5" PRIu32 "%s Ft: % "
            PRId32, pos, freq / 100000, freq % 100000, tpiGetPolStr (pol),
            tpft);
}

static void
handle_err (AppData * a, int retcode, int flags)
{
  /*
     check the retcode, get server status and give advice on what to do,
     why it failed.. flags tells us if we need to be active, super or both to
     complete the request successfully
     TODO
   */
#define BE_ACTIVE 1
#define BE_SUPER  2



}

static void
f_correct (AppData * a)
{
  uint32_t maxx, maxy;
  int state = 0;
  int lbl_x;
  Chart charts[4];
  static const char *caption[] = { "ber", "strength", "snr", "ucb" };
  int i, x1, y1;
  uint8_t valid, tuned;
  uint32_t pos;
  uint32_t freq;
  uint8_t pol;
  int32_t tpft;
  int32_t fc;
  chtype c;
  uint8_t flags;
  uint32_t ifreq;
  cwGetTstat (a->sock, &valid, &tuned, &pos, &freq, &pol, &tpft, &fc, &ifreq);  //perhaps needs another boolean to indicate it's tuned?
  if (!valid)
  {
    errMsg
      ("frontend tuning parameters are not valid. Can not do finetuning. Tune in a valid transponder first.(It does not need to be successful)\n");
    return;
  }
  message (a,
           "Press 1/2/3/4 to show charts for ber, strength, snr and ucb respectively.\n");
  message (a,
           "Press x/c to change frequency correction for current transponder down/up by 10.\n");
  message (a,
           "Press s/d to change frequency correction for current transponder down/up by 1.\n");
  message (a,
           "Press e to set frequency correction for current transponder directly.\n");
  message (a,
           "Press v/b to change frequency correction for frontend down/up by 10.\n");
  message (a,
           "Press f/g to change frequency correction for frontend down/up by 1.\n");
  message (a, "Press t to set frequency correction for frontend directly.\n");
  message (a,
           "Connection must be super and active to change finetune. All changes persist. Backspace or Escape exit.\n");
  /*
     have the chart take up the top port of window
     one line label above it
     below chart are the dvb flags(LOCK etc...)
     below that the frontend finetune
     below that the tp finetune..
     one character border around all of it..
   */
  maxx = getmaxx (stdscr);
  maxy = getmaxy (stdscr);
  for (i = 0; i < 4; i++)
  {
    debugMsg ("initializing chart %d\n", i);
    chartInit (&charts[i], 1, 1 + 1, maxx - 2, maxy * 2 / 3 - 2 - 4);
  }
  i = 0;
  while (state >= 0)
  {
    uint32_t ber;
    uint32_t strength;
    uint32_t snr;
    uint32_t ucblk;
    int rv;
    state = 0;
    maxx = getmaxx (stdscr);
    lbl_x = maxx / 2 - strlen (caption[i]) / 2;
    x1 = maxx;
    y1 = getmaxy (stdscr) * 2 / 3;
    if (lbl_x < 1)
      lbl_x = 1;

    debugMsg ("starting menu loop\n");
    halfdelay (1);              //wait a sec on getch() max
    rv = cwGetStatus (a->sock, &flags, &ber, &strength, &snr, &ucblk);
    while (ERR == (c = appDataGetch (a)))
    {
      rv = cwGetStatus (a->sock, &flags, &ber, &strength, &snr, &ucblk);
      erase ();
      mvprintw (1, lbl_x, "%s", caption[i]);
      mvhline (0, 0, ACS_HLINE, x1);
      mvhline (y1 - 1, 0, ACS_HLINE, x1);

      mvvline (0, 0, ACS_VLINE, y1);
      mvvline (0, x1 - 1, ACS_VLINE, y1);

      mvaddch (0, 0, ACS_ULCORNER);
      mvaddch (y1 - 1, 0, ACS_LLCORNER);
      mvaddch (0, x1 - 1, ACS_URCORNER);
      mvaddch (y1 - 1, x1 - 1, ACS_LRCORNER);
      if (rv == 0)
      {
        chartVal (&charts[0], (int) ber);
        chartVal (&charts[1], (int) strength);
        chartVal (&charts[2], (int) snr);
        chartVal (&charts[3], (int) ucblk);
        chartDraw (&charts[i]);
        draw_dvbflags (flags, 1, 1 + 1 + maxy * 2 / 3 - 2 - 4);
      }
      rv =
        cwGetTstat (a->sock, &valid, &tuned, &pos, &freq, &pol, &tpft, &fc,
                    &ifreq);
      if (valid && (rv == 0))
      {
        draw_fcorr (fc, ifreq, 1, 1 + 1 + maxy * 2 / 3 - 2 - 4 + 1);
        draw_tpft (pos, freq, pol, tpft, 1, 1 + 1 + maxy * 2 / 3 - 2 - 4 + 2);
      }
      touchwin (a->outer);
      touchwin (a->output);
      wnoutrefresh (stdscr);
      wnoutrefresh (a->outer);
      wnoutrefresh (a->output);
      doupdate ();              //not sure if this is okay in any case...
    }
    nodelay (stdscr, FALSE);
    cbreak ();
    switch (c)
    {
    case 263:                  //ESC
    case K_BACK:               //Backspace
      state = -1;
      break;
    case KEY_RESIZE:
      maxx = getmaxx (stdscr);
      maxy = getmaxy (stdscr);
      lbl_x = maxx / 2 - strlen (caption[i]) / 2;
      if (lbl_x < 1)
        lbl_x = 1;
      mvwprintw (stdscr, 1, lbl_x, "%s", caption[i]);
      chartResize (&charts[0], 1, 1 + 1, maxx - 2, maxy * 2 / 3 - 2 - 4);
      chartResize (&charts[1], 1, 1 + 1, maxx - 2, maxy * 2 / 3 - 2 - 4);
      chartResize (&charts[2], 1, 1 + 1, maxx - 2, maxy * 2 / 3 - 2 - 4);
      chartResize (&charts[3], 1, 1 + 1, maxx - 2, maxy * 2 / 3 - 2 - 4);
      break;
    case '1':
    case '2':
    case '3':
    case '4':
      i = c - '1';
      break;
    case 'x':
      if ((tpft >= (INT32_MIN + 10)))
      {
        tpft -= 10;
        cwFtTp (a->sock, pos, freq, pol, tpft);
      }
      break;
    case 'c':
      if ((tpft <= (INT32_MAX - 10)))
      {
        tpft += 10;
        cwFtTp (a->sock, pos, freq, pol, tpft);
      }
      break;
    case 's':
      if ((tpft >= (INT32_MIN + 1)))
      {
        tpft -= 1;
        cwFtTp (a->sock, pos, freq, pol, tpft);
      }
      break;
    case 'd':
      if ((tpft <= (INT32_MAX - 1)))
      {
        tpft += 1;
        cwFtTp (a->sock, pos, freq, pol, tpft);
      }
      break;
    case 'v':
      if ((fc >= (INT32_MIN + 10)))
      {
        fc -= 10;
        cwSetFcorr (a->sock, fc);
      }
      break;
    case 'b':
      if ((fc <= (INT32_MAX - 10)))
      {
        fc += 10;
        cwSetFcorr (a->sock, fc);
      }
      break;
    case 'f':
      if ((fc >= (INT32_MIN + 1)))
      {
        fc -= 1;
        cwSetFcorr (a->sock, fc);
      }
      break;
    case 'g':
      if ((fc <= (INT32_MAX - 1)))
      {
        fc += 1;
        cwSetFcorr (a->sock, fc);
      }
      break;
    case 'e':
      tpft = get_num (a);
      cwFtTp (a->sock, pos, freq, pol, tpft);
      break;
    case 't':
      fc = get_num (a);
      cwSetFcorr (a->sock, fc);
      break;
    default:
      break;
    }
  }
  for (i = 0; i < 4; i++)
    chartClear (&charts[i]);
}

static void
show_status (AppData * a)
{
  WINDOW *outer;
  WINDOW *wnd;
  int maxy, maxx;
  uint8_t cmd = SRV_USTAT;
  uint8_t res, done = 0;
  uint8_t valid, tuned, pol;
  uint32_t pos, freq, ifreq;
  int32_t fc, ft;

  nodelay (stdscr, TRUE);
  do
  {
    outer = appDataMakeEmptyWnd ();
    getmaxyx (stdscr, maxy, maxx);
    wnd = derwin (outer, (maxy * 2 / 3) - 2, maxx - 2, 1, 1);
    wsetscrreg (wnd, 1, (maxy * 2 / 3) - 2);
    scrollok (wnd, TRUE);

    clientGetTstat (a->sock, &valid, &tuned, &pos, &freq, &pol, &ft, &fc,
                    &ifreq);

    if (tuned)
    {
      wprintw (wnd,
               "Tuned to: %2.2" PRIu32 ":%3.3" PRIu32 ",%5.5" PRIu32 "%s\n",
               pos, freq / 100000, freq % 100000, tpiGetPolStr (pol));
    }
    else
    {
      wprintw (wnd, "Not Tuned\n");
    }
    ipcSndS (a->sock, cmd);
    ipcRcvS (a->sock, res);
    if (res != SRV_OK)
    {
      errMsg ("Error: %s\n", client_strerror (res));
      return;
    }
    wprintw (wnd, "Clients\n");
    done = 0;
    while (!done)
    {
      struct sockaddr ad;
      uint8_t active, super;
      char buf[256] = { 0 };
      char *sp[2] = { "plain", "super" };
      char *ac[2] = { "idle  ", "active" };

      int32_t pnr;
      ipcRcvS (a->sock, res);
      switch (res)
      {
      case 0xff:
        ipcRcvS (a->sock, ad.sa_family);
        ioBlkRcv (a->sock, ad.sa_data, sizeof (ad.sa_data));
        ipcRcvS (a->sock, super);
        ipcRcvS (a->sock, active);
        ipcRcvS (a->sock, pnr);
        ioNtoP ((struct sockaddr_storage *) &ad, buf, sizeof (buf));
        wprintw (wnd, "%s: %s %s ", buf,
                 struGetStr (sp, super), struGetStr (ac, active));
        if (pnr == -1)
        {
          wprintw (wnd, "Not Streaming\n");
        }
        else
        {
          wprintw (wnd, "Streaming: %5.5" PRId32 "\n", pnr);
        }
        break;
      case 2:
        wprintw (wnd, "Recorder running\n");
        done = 1;
        break;
      case 1:
        wprintw (wnd, "Recorder idle\n");
        done = 1;
        break;
      default:
        errMsg ("Ouch!\n");
        abort ();
        break;
      }
    }
    wprintw (wnd, "Press any key to return\n");
    wrefresh (outer);
    wrefresh (wnd);
    sleep (1);
    assert (ERR != delwin (wnd));
    assert (ERR != delwin (outer));
  }
  while (ERR == getch ());

  nodelay (stdscr, FALSE);
}


void
app_run (AppData * a)
{
  ITEM **my_items;
  int c, maxx, lbl_x;
  MENU *my_menu;
  int state = 0, n_choices, i;
  message (a,
           "Good Morning,\nUse the cursor keys to navigate, Carriage Return to select an entry and Escape or Backspace to exit.\n");
  n_choices = sizeof (choices) / sizeof (char *);
  while (state >= 0)
  {
    state = 0;
    my_items = (ITEM **) utlCalloc (n_choices + 1, sizeof (ITEM *));
    for (i = 0; i < n_choices; ++i)
      my_items[i] = new_item (choices[i], choices[i]);
    my_items[n_choices] = (ITEM *) NULL;

    my_menu = new_menu ((ITEM **) my_items);
    set_menu_opts (my_menu, O_ONEVALUE | O_NONCYCLIC | O_ROWMAJOR);
    maxx = getmaxx (stdscr);
    appDataMakeMenuWnd (my_menu);
    lbl_x = 3 + (maxx - 6) / 2 - strlen ("Main") / 2;
    mvwprintw (menu_win (my_menu), 1, lbl_x, "%s", "Main");
    post_menu (my_menu);
    while (state == 0)
    {
      ITEM *selection;
      wrefresh (menu_win (my_menu));
      c = appDataGetch (a);
      switch (c)
      {
      case 263:                //ESC
      case K_BACK:             //Backspace
//      case K_BACK:                 //Backspace
        state = IDX_EXIT + 1;
        break;
      case KEY_DOWN:
        menu_driver (my_menu, REQ_DOWN_ITEM);
        break;
      case KEY_UP:
        menu_driver (my_menu, REQ_UP_ITEM);
        break;
      case KEY_RESIZE:
        appDataResizeMnu (my_menu);
        wrefresh (menu_win (my_menu));
        break;
      case 13:
        selection = current_item (my_menu);
        state = item_index (selection) + 1;
        break;
      default:
        break;
      }
    }
    unpost_menu (my_menu);
    appDataDestroyMenuWnd (my_menu);
    free_menu (my_menu);
    for (i = 0; i < n_choices; ++i)
      free_item (my_items[i]);
    utlFAN (my_items);
    switch (state - 1)
    {
    case IDX_MASTER_LIST:
      show_pos (my_menu, a);
      break;
    case IDX_FAV_LIST:
      show_fav_list (my_menu, a);
      break;
    case IDX_SELECT_LIST:
      show_select_list (my_menu, a);
      break;
    case IDX_ADD_LIST:
      show_add_list (my_menu, a);
      break;
    case IDX_DEL_LIST:
      show_del_list (my_menu, a);
      break;
    case IDX_SWITCH:
      switch_setup (my_menu, a);
      break;
    case IDX_ACTIVE:
      cwGoActive (a->sock);
      break;
    case IDX_INACTIVE:
      cwGoInactive (a->sock);
      break;
    case IDX_F_CORRECT:
      f_correct (a);
      break;
    case IDX_SCHED:
      show_sched (a);
      break;
    case IDX_RLIST:
      show_rlist (a);
      break;
    case IDX_RMODE:
      start_rmode (a);
      break;
    case IDX_USTAT:
      show_status (a);
      break;
/*			case IDX_TST:
			{
				uint8_t cmd=SRV_TST;
				ipcSndS(a->sock,cmd);
			}
			break;*/
    case IDX_EXIT:
      state = -1;
      break;
    default:
      break;
    }
  }
  message (a, "Bye\n");
}
