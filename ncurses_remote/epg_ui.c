#define _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE           /* See feature_test_macros(7) */
#define _DEFAULT_SOURCE             //for timegm
#define NCURSES_WIDECHAR 1
#include <ncursesw/curses.h>
#include <wchar.h>
#include <string.h>
#include <pthread.h>            //strtok_r rand_r
#include <ncurses.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include "epg_ui.h"
#include "rcfile.h"
#include "utl.h"
#include "timestamp.h"
#include "menu_strings.h"
#include "debug.h"
#include "dmalloc_f.h"

int DF_EPG_UI;
#define FILE_DBG DF_EPG_UI
/**
 *\file epg_ui.c
 *\brief holds user interface data for the epg view
 *
 * Gee... How should I do this then...
 * grid like in those tv magazines would be nice..
 * timescale somehow aligned...
 * minimum granularity?
 * Should we hide short events?
 * adaptive granularity? May be better
 *
 * every item:
 * ptr to corresponding event(may be an index, too)
 * ptr to prev/nex. may be skipped if list is array
 *
 * constant width, longer titles get shortened.
 * Height as needed. What does this mean?
 * minimum four lines. Title/Category/Description
 * may be larger if several entries in another queue.
 * perhaps scale min size up for longer items/movies... or down for short ones
 * assign keypresses to scroll through detailed description up/down...
 * ..or use separate detail area besides epg view
 *
 * Then, paint the whole thing into a pad and scroll it around
 *
 * removed left right ptrs in items as navigation was strange, now uses a lookup table
 * to infer item indices from y cell coordinate.
 */

static void epg_init_colors (EpgView * epg, char const *spec);
static void epg_ui_draw_cols (EpgView * epg);
static void epg_ui_draw_scale (EpgView * epg);
static void epg_ui_draw_header (EpgView * epg);
static void epg_ui_draw_items (EpgView * epg);
static void epg_hilight_selection (EpgView * epg, int active);
static void epg_align (EpgView * epg);


#define CUR_E(_EV) (_EV)->cols[(_EV)->sel_col].entries[(_EV)->sel_idx]
#define CUR_C(_EV) (_EV)->cols[(_EV)->sel_col]

#if (NCURSES_WIDECHAR==1)

#define evtGetDescN evtGetDescW
#define evtGetNameN evtGetNameW

//this broken?
//#define sanitize utlWSanitize
#define sanitize
typedef wchar_t nch;
#define mvwaddnstrN mvwaddnwstr
#define mvwaddstrN mvwaddwstr

static void trim_str(nch *s,int cols)
{
	int i;
	while(*s)
	{
		i=wcwidth(*s);
		if(i==-1)
		{
			*s=L'\0';//HACK HACK (tis woz wronx for L'\032')
			break;
		}
		if(i>0)
		{
			cols-=i;
			if(cols<0)
			{//overran by at least one...
				*s=L'\0';
				break;
			}
		}
		s++;
	}
}
#else

#define evtGetDescN evtGetDesc
#define evtGetNameN evtGetName
#define sanitize utlAsciiSanitize
typedef char nch;
#define mvwaddnstrN mvwaddnstr
#define mvwaddstrN mvwaddstr

static void trim_str(nch *s,int cols)
{
	if(strlen(s)>cols)
		s[cols]='\0';
}

#endif



static void
start_seen (EpgView * ev, int next_col, uint32_t ev_idx, int *line_p)
{
  EpgVEntry *ee;
  ee = ev->cols[next_col].entries + ev_idx;
  ee->start_line = *line_p;
  ee->ev_idx = ev_idx;
}

static void
end_seen (EpgView * ev, int next_col, uint32_t * ev_idx, int *line_p)
{
  EpgVEntry *ee;
  ee = ev->cols[next_col].entries + ev_idx[next_col];
  ee->end_line = *line_p;
  if ((ee->end_line - ee->start_line) < ev->vspc)
  {                             //too short.. add some lines, so it's at least VSPC cells high
    *line_p += ev->vspc - (ee->end_line - ee->start_line);
    ee->end_line = *line_p;
  }
  ev->nlines = ee->end_line + 1;
}

void
epgUiClear (EpgView * ev)
{
  uint32_t i;
  epg_hilight_selection (ev, 0);
  for (i = 0; i < ev->num_columns; i++)
  {
    utlFAN (ev->cols[i].entries);
    utlFAN (ev->cols[i].idx_lut);
  }
  utlFAN (ev->cols);
  assert (ERR != delwin (ev->h_p));
  assert (ERR != delwin (ev->s_p));
  assert (ERR != delwin (ev->e_p));

}

void
epg_ui_init_lut (EpgView * ev)
{
  uint32_t i, j;
  int k;
  for (i = 0; i < ev->num_columns; i++)
  {
    ev->cols[i].idx_lut = utlCalloc (ev->nlines, sizeof (int));
    k = 0;
    for (j = 0; j < ev->cols[i].num_entries; j++)
    {
      for (; k <= ev->cols[i].entries[j].end_line; k++)
      {
        ev->cols[i].idx_lut[k] = j;
      }
    }
    while (k < ev->nlines)
    {
      ev->cols[i].idx_lut[k] = j - 1;
      k++;
    }
  }
}

//will take ownership of sched, pgm_names
void
epgUiInit (EpgView * ev, RcFile * cfg, EpgSchedule * sched, char **pgm_names, uint32_t sched_sz, int x, int y, int w, int h)    //,int col_w,int vspc)
{
  uint32_t i;
  int line;
  uint32_t *ev_idx;             //indices for events at current line by column
  uint8_t *which;               //0: start 1: end
  int xsz;                      //one more for final line
  int ysz;
  int got_element = 0;
  int next_col;
  int col_w = 20, vspc = 4;
  long tmp;
  char const *color_spec = NULL;
  time_t next;
  Section *epg_sec;
  memset (ev, 0, sizeof (EpgView));
  epg_sec = rcfileFindSec (cfg, "EPG");
  if (epg_sec)
  {
    if (!rcfileFindSecValInt (epg_sec, "col_w", &tmp))
    {
      col_w = tmp;
    }

    if (!rcfileFindSecValInt (epg_sec, "vspc", &tmp))
    {
      vspc = tmp;
    }

    if (rcfileFindSecVal (epg_sec, "colors", (char **) &color_spec))
    {
      color_spec = NULL;
    }
  }
  epg_init_colors (ev, color_spec);
  if (vspc < 4)
    vspc = 4;
  if (col_w < 10)
    col_w = 10;
  ev->nav_y = 0;
  ev->x = x;
  ev->y = y;
  ev->h = h;
  ev->w = w;
  ev->col_w = col_w;
  ev->vspc = vspc;
  ev->sched = sched;
  ev->pgm_names = pgm_names;
  ev->num_columns = sched_sz;
  ev->cols = utlCalloc (sched_sz, sizeof (EpgVCol));
  for (i = 0; i < sched_sz; i++)
  {
    ev->cols[i].num_entries = sched[i].num_events;
    ev->cols[i].entries =
      utlCalloc (ev->cols[i].num_entries, sizeof (EpgVEntry));
  }
  ev_idx = utlCalloc (sched_sz, sizeof (uint32_t));
  which = utlCalloc (sched_sz, sizeof (uint8_t));

  line = 0;
  /*
     Now generate view line by line
     start at line 0
     first, find the earliest event.
     Will be one of the first events

     Then find next event
     Continue
     Until done
   */
  next_col = -1;
  while (1)
  {
    got_element = 0;
    next = 0x7fffffff;          //one day, this will be a problem...
    if (next_col >= 0)          //there was a prev element
    {
      if (which[next_col] == 1) //end time
      {
        next = evtGetStart (ev->sched[next_col].events[ev_idx[next_col]]) + evtGetDuration (ev->sched[next_col].events[ev_idx[next_col]]);      //start out in this column
        got_element = 1;
      }
    }

    for (i = 0; i < ev->num_columns; i++)       //find the next(earliest start/end time) event
    {
      if (ev->sched[i].num_events > ev_idx[i])  //not at end yet
      {
        time_t s2;
        if (which[i] == 0)      //start time
        {
          s2 = evtGetStart (ev->sched[i].events[ev_idx[i]]);
          if (s2 < next)
          {
            next = s2;
            next_col = i;
          }
        }
        else
        {
          s2 =
            evtGetStart (ev->sched[i].events[ev_idx[i]]) +
            evtGetDuration (ev->sched[i].events[ev_idx[i]]);
          if (s2 < next)
          {
            next = s2;
            next_col = i;
          }
        }
        got_element = 1;
      }
/*			if(ev->sched[i].num_events>(ev_idx[i]+1))
			{
				next_ev_starts[i]=evtGetStart(ev->sched[i].events[ev_idx[i]+1]);
				got_next_element=1;
			}*/
    }

    if (got_element)            //update EPG vars
    {
      if (next != 0x7fffffff)
      {
        if (which[next_col] == 0)
        {
          start_seen (ev, next_col, ev_idx[next_col], &line);
          which[next_col] = 1;
        }
        else
        {
          end_seen (ev, next_col, ev_idx, &line);
          which[next_col] = 0;
          ev_idx[next_col]++;
        }
      }
    }

    if (!got_element)
    {
      break;                    //done
    }
  }
  utlFAN (ev_idx);
  utlFAN (which);
  xsz = col_w * ev->num_columns + 1;    //one more for final line
  ysz = ev->nlines;
  epg_ui_init_lut (ev);
  ev->e_p = newpad (ysz, xsz);
  ev->s_p = newpad (ysz, 10);
  ev->h_p = newpad (3, xsz);
  epg_ui_draw_cols (ev);
  epg_ui_draw_items (ev);
  epg_ui_draw_scale (ev);
  epg_ui_draw_header (ev);
  epg_hilight_selection (ev, 1);
  epg_align (ev);
}

/**
 *\brief Draw column headers and vertical lines
 *
 * eg.
 * -----------------
 * |AsaVaf|GNesaf 2|
 * |------|--------|
 */
static void
epg_ui_draw_header (EpgView * epg)
{
  uint32_t i;
  for (i = 0; i < epg->num_columns; i++)
  {
    int box_start = i * epg->col_w;
    mvwhline (epg->h_p, 0, 1 + box_start, 0, epg->col_w - 1);
    mvwhline (epg->h_p, 2, 1 + box_start, 0, epg->col_w - 1);
    mvwaddch (epg->h_p, 1, box_start, ACS_VLINE);
    if (i == 0)
    {
      mvwaddch (epg->h_p, 0, box_start, ACS_ULCORNER);
      mvwaddch (epg->h_p, 2, box_start, ACS_LTEE);
    }
    else
    {
      mvwaddch (epg->h_p, 0, box_start, ACS_TTEE);
      mvwaddch (epg->h_p, 2, box_start, ACS_PLUS);
    }
    mvwaddnstr (epg->h_p, 1, box_start + 1, epg->pgm_names[i],
                epg->col_w - 2);
  }
  if (epg->num_columns)
  {
    mvwaddch (epg->h_p, 1, i * epg->col_w, ACS_VLINE);
    mvwaddch (epg->h_p, 0, i * epg->col_w, ACS_URCORNER);
    mvwaddch (epg->h_p, 2, i * epg->col_w, ACS_RTEE);
  }
}

static void
epg_ui_draw_scale (EpgView * epg)
{
  uint32_t i = 0;
  uint32_t j = 0;
  for (i = 0; i < epg->num_columns; i++)
  {
    for (j = 0; j < epg->cols[i].num_entries; j++)
    {
      int st;                   //,en;
      int d, mo, y, h, m, s;
      struct tm src;
      time_t start, duration;
      st = epg->cols[i].entries[j].start_line;
      start = evtGetStart (epg->sched[i].events[j]);
      h = get_h (start);
      m = get_m (start);
      s = get_s (start);
      duration = evtGetDuration (epg->sched[i].events[j]);
      mjd_to_ymd (ts_to_mjd (start), &y, &mo, &d);
      src.tm_min = m;
      src.tm_sec = s;
      src.tm_hour = h;
      src.tm_mday = d;
      src.tm_mon = mo - 1;
      src.tm_year = y - 1900;
      start = timegm (&src);    //avoid me. I'm a gNU extension
      localtime_r (&start, &src);
      m = src.tm_min;
      s = src.tm_sec;
      h = src.tm_hour;
      mvwprintw (epg->s_p, st, 0, "%2.2d:%2.2d,%2.2d", h, m, s);
      epg->cols[i].entries[j].start_time = start;
      epg->cols[i].entries[j].duration = duration;
    }
  }
}

static void
epg_ui_draw_cols (EpgView * epg)
{
  uint32_t i;
  for (i = 0; i < epg->num_columns; i++)
  {
    mvwvline (epg->e_p, 0, i * epg->col_w, 0, epg->nlines);
  }
  if (epg->num_columns)
  {
    mvwvline (epg->e_p, 0, i * epg->col_w, 0, epg->nlines);
  }
}


static const char *cnt1_strings[] = {
  "Drama",
  "News",
  "Show",
  "Sports",
  "Kids",
  "Music",
  "Arts",
  "Social",
  "Science",
  "Leisure"
};

static const char *cnt2_strings[][12] = {
  {
   "",
   "/Crime",
   "/Adventure",
   "/Fantastic",
   "/Comedy",
   "/Soap",
   "/Romance",
   "/Serious",
   "/Adult"},
  {
   "",
   "/Weather",
   "/Mag",
   "/Documentary",
   "/Debate"},
  {
   "",
   "/Contest",
   "/Variety",
   "/Talk"},
  {
   "",
   "/Special",
   "/Mag",
   "/Soccer",
   "/Tennis",
   "/Team",
   "/Athletics",
   "/Motor",
   "/Water",
   "/Winter",
   "/Equestrian",
   "/Martial"},
  {
   "",
   "/<6",
   "/6-14",
   "/10-16",
   "/Edu",
   "/Toons"},
  {
   "",
   "/Pop",
   "/Classic",
   "/Folk",
   "/Jazz",
   "/Opera",
   "/Ballet"},
  {
   "",
   "/Performing",
   "/Fine",
   "/Religion",
   "/Pop",
   "/Literature",
   "/Film",
   "/Experimental",
   "/Press",
   "/New",
   "/Mag",
   "/Fashion"},
  {
   "",
   "/Mag",
   "/Economy",
   "/People"},
  {
   "",
   "/Nature",
   "/Tech",
   "/Med",
   "/Countries",
   "/Social",
   "/Edu",
   "/Language"},
  {
   "",
   "/Travel",
   "/Craft",
   "/Motor",
   "/Health",
   "/Food",
   "/Ads",
   "/Garden"},
  {
   ":OL",
   ":B/W",
   ":UP",
   ":LIVE"}
};

static const char *cnt_sep = ",";
static void
append_cd_string (uint16_t cd, char **buf_p, unsigned *sz_p, bool initial)
{
  unsigned n1 = (cd >> 12) & 0xf;
  unsigned n2 = (cd >> 8) & 0xf;
  unsigned n1_2;
  char *buf = *buf_p;
  unsigned sz = *sz_p;
  unsigned l;
  char *p;
  if (!initial)
  {
    l = strlen (cnt_sep);
    if (l < sz)
    {
      strcpy (buf, cnt_sep);
      buf += l;
      sz -= l;
    }
  }
  if (n1 < 12)
  {
    if (n1 > 0)
    {
      n1_2 = n1 - 1;
      if (n1_2 < 10)
      {
        p = (char *) struGetStr (cnt1_strings, n1_2);
        l = strlen (p);
        if (l < sz)
        {
          strcpy (buf, p);
          buf += l;
          sz -= l;
        }
      }
      p = (char *) struGetStr (cnt2_strings[n1_2], n2);
      if (NULL != p)
      {
        l = strlen (p);
        if (l < sz)
        {
          strcpy (buf, p);
          buf += l;
          sz -= l;
        }
      }
    }
  }
  *buf_p = buf;
  *sz_p = sz;
}

static void
gen_cd_string (char *buf, unsigned sz, uint16_t * cdp, unsigned cdps)
{
  unsigned i;
  memset (buf, '\0', sz);
  for (i = 0; i < cdps; i++)
  {
    append_cd_string (cdp[i], &buf, &sz, (i == 0));
  }
}

static void
epg_ui_draw_items (EpgView * epg)
{
  uint32_t i = 0;
  uint32_t j = 0;
  time_t start, duration;
  int day, mo, yr, hr, min, sec;
  struct tm src;
  time_t now = time (NULL);
  for (i = 0; i < epg->num_columns; i++)
  {
    epg->cols[i].now_on = -1;
    for (j = 0; j < epg->cols[i].num_entries; j++)
    {
      WINDOW *s_p;
      int s, e;
      chtype c, d;
      nch *ev_nm;
      uint16_t *cdp;
      unsigned cdps;
      unsigned k, n1;
      char buf[256];
      //draw_box
      s = epg->cols[i].entries[j].start_line;
      e = epg->cols[i].entries[j].end_line;
      //this seems necessary to extract line drawing chars
#define MSK	(A_CHARTEXT|A_ALTCHARSET)
      c = mvwinch (epg->e_p, s, i * epg->col_w);
      d = c & MSK;
      if (d == ACS_VLINE)
      {
        c = (c & (~MSK)) | ACS_LTEE;
      }
      else if (d == ACS_RTEE)
      {
        c = (c & (~MSK)) | ACS_PLUS;
      }
      mvwaddch (epg->e_p, s, i * epg->col_w, c);

      c = mvwinch (epg->e_p, s, (i + 1) * epg->col_w);
      d = c & MSK;
      if (d == ACS_VLINE)
      {
        c = (c & (~MSK)) | ACS_RTEE;
      }
      else if (d == ACS_LTEE)
      {
        c = (c & (~MSK)) | ACS_PLUS;
      }
      mvwaddch (epg->e_p, s, (i + 1) * epg->col_w, c);

      c = mvwinch (epg->e_p, e, i * epg->col_w);
      d = c & MSK;
      if (d == ACS_VLINE)
      {
        c = (c & (~MSK)) | ACS_LTEE;
      }
      else if (d == ACS_RTEE)
      {
        c = (c & (~MSK)) | ACS_PLUS;
      }
      mvwaddch (epg->e_p, e, i * epg->col_w, c);

      c = mvwinch (epg->e_p, e, (i + 1) * epg->col_w);
      d = c & MSK;
      if (d == ACS_VLINE)
      {
        c = (c & (~MSK)) | ACS_RTEE;
      }
      else if (d == ACS_LTEE)
      {
        c = (c & (~MSK)) | ACS_PLUS;
      }
      mvwaddch (epg->e_p, e, (i + 1) * epg->col_w, c);
      mvwhline (epg->e_p, s, i * epg->col_w + 1, 0, epg->col_w - 1);
      mvwhline (epg->e_p, e, i * epg->col_w + 1, 0, epg->col_w - 1);
      //now program title

      ev_nm = evtGetNameN (epg->sched[i].events[j]);
      if (ev_nm)
        sanitize (ev_nm);
      cdps = 0;
      cdp = evtGetCd (epg->sched[i].events[j], &cdps);
      for (k = 0; k < cdps; k++)
      {
        n1 = (cdp[k] >> 12) & 0xf;
        if (n1 < 11)
        {
          debugMsg ("content: 0x%4.4" PRIx16 "\n", cdp[k]);
          wcolor_set (epg->e_p, n1, NULL);
          break;
        }
      }
      wattron (epg->e_p, A_BOLD);
      if (ev_nm)
      {
        trim_str(ev_nm,epg->col_w - 2);
        mvwaddnstrN (epg->e_p, s + 1, i * epg->col_w + 2, ev_nm,
                     epg->col_w - 2);
        utlFAN (ev_nm);
      }
      wattroff (epg->e_p, A_BOLD);
      wcolor_set (epg->e_p, 0, NULL);

      gen_cd_string (buf, 256, cdp, cdps);
      debugMsg ("content2: %s\n", buf);
      mvwaddnstr (epg->e_p, s + 2, i * epg->col_w + 2, buf, epg->col_w - 2);

      utlFAN (cdp);
      ev_nm = evtGetDescN (epg->sched[i].events[j]);
      if (ev_nm)
      {
        s_p =
          subpad (epg->e_p, e - s - 3, epg->col_w - 2, s + 3,
                  i * epg->col_w + 2);
        mvwaddstrN (s_p, 0, 0, ev_nm);
        assert (ERR != delwin (s_p));
        utlFAN (ev_nm);
      }
      start = evtGetStart (epg->sched[i].events[j]);
      hr = get_h (start);
      min = get_m (start);
      sec = get_s (start);
      mjd_to_ymd (ts_to_mjd (start), &yr, &mo, &day);
      src.tm_min = min;
      src.tm_sec = sec;
      src.tm_hour = hr;
      src.tm_mday = day;
      src.tm_mon = mo - 1;
      src.tm_year = yr - 1900;
      start = timegm (&src);    //avoid me. I'm a gNU extension
      duration = evtGetDuration (epg->sched[i].events[j]);
      //number of asterisks to draw vertically inside box(progress indicator)
      if (now > (start + duration))
      {                         //past
        mvwvline (epg->e_p, s + 1, i * epg->col_w + 1, '*', e - s - 1);
      }
      else if (now >= start)
      {                         //on now
        if (duration > 0)
        {
          mvwvline (epg->e_p, s + 1, i * epg->col_w + 1, '*',
                    (now - start) * (e - s - 1) / duration);
          epg->cols[i].now_on = j;
        }
      }
      else
      {                         //not yet

      }

    }
  }
}

#if (NCURSES_WIDECHAR==1)
//those turn out wrong for some reason.. 
unsigned
epg_get_txtlen (wchar_t * desc, unsigned width)
{
  unsigned l, x;
  wchar_t c;
  x = 0;
  l = 0;
  if (NULL == desc)
  {
    return 0;
  }
  while (L'\0' != (c = *desc))
  {
    int w = wcwidth (*desc);
    if (w < 0)
      w = 0;
    x += w;
    if (x >= width)
    {
      l++;
      x -= width;
    }
    if (c == L'\n')
    {
      l++;
      x = 0;
    }
    desc++;
  }
  return l + ((x + width - 1) / width);
}

static void
epg_hilight_selection (EpgView * epg, int active)
{
  int i = epg->sel_col;
  int j = epg->sel_idx;
  unsigned txtlen;
  int s;
  unsigned e, k;
  cchar_t c;
  nch *desc;
  s = epg->cols[i].entries[j].start_line;
  e = epg->cols[i].entries[j].end_line;
  for (k = s; k < e; k++)
  {
    int l;
    for (l = i * epg->col_w; l < (i * epg->col_w + epg->col_w); l++)
    {
      mvwin_wch (epg->e_p, k, l, &c);
      if (active)
        c.attr |= A_REVERSE;
      else
        c.attr &= ~A_REVERSE;
      mvwadd_wch (epg->e_p, k, l, &c);
    }
  }
  if (active)
  {
    desc = evtGetDescN (epg->sched[i].events[j]);
    txtlen = epg_get_txtlen (desc, epg->col_w - 2);     //29
    if (txtlen)
    {
      epg->sel_pad = newpad (txtlen * 2, epg->col_w - 2);       //so we overwrite pad underneath(the prev scribble is still there..)
      debugMsg ("creating selection pad\n");
      mvwaddstrN (epg->sel_pad, 0, 0, desc);    //58 no..56 why?
      for (k = 0; k < (txtlen * 2); k++)
      {
        int l;
        for (l = 0; l < epg->col_w; l++)
        {
          mvwin_wch (epg->sel_pad, k, l, &c);
          c.attr |= A_REVERSE;
          mvwadd_wch (epg->sel_pad, k, l, &c);
        }
      }
    }
    utlFAN (desc);
  }
  else
  {
    if (epg->sel_pad)
    {
      debugMsg ("removing selection pad\n");
      assert (ERR != delwin (epg->sel_pad));
      epg->sel_pad = NULL;
    }
  }
}
#else
unsigned
epg_get_txtlen (char *desc, unsigned width)
{
  unsigned l, x;
  char c;
  x = 0;
  l = 0;
  if (NULL == desc)
  {
    return 0;
  }
  while ('\0' != (c = *desc))
  {
    x++;
    if (x >= width)
    {
      l++;
      x = 0;
    }
    if (c == '\n')
    {
      l++;
      x = 0;
    }
    desc++;
  }
  return l + ((x + width - 1) / width);
}

static void
epg_hilight_selection (EpgView * epg, int active)
{
  int i = epg->sel_col;
  int j = epg->sel_idx;
  unsigned txtlen;
  int s;
  unsigned e, k;
  chtype c;
  nch *desc;
  s = epg->cols[i].entries[j].start_line;
  e = epg->cols[i].entries[j].end_line;
  for (k = s; k < e; k++)
  {
    int l;
    for (l = i * epg->col_w; l < (i * epg->col_w + epg->col_w); l++)
    {
      c = mvwinch (epg->e_p, k, l);
      if (active)
        c |= A_REVERSE;
      else
        c &= ~A_REVERSE;
      mvwaddch (epg->e_p, k, l, c);
    }
  }
  if (active)
  {
    desc = evtGetDescN (epg->sched[i].events[j]);
    txtlen = epg_get_txtlen (desc, epg->col_w - 2);
    if (txtlen)
    {
      epg->sel_pad = newpad (txtlen * 2, epg->col_w - 2);       //so we overwrite pad underneath(the prev scribble is still there..)
      debugMsg ("creating selection pad\n");
      mvwaddstrN (epg->sel_pad, 0, 0, desc);
      for (k = 0; k < (txtlen * 2); k++)
      {
        int l;
        for (l = 0; l < epg->col_w; l++)
        {
          c = mvwinch (epg->sel_pad, k, l);
          c |= A_REVERSE;
          mvwaddch (epg->sel_pad, k, l, c);
        }
      }
    }
    utlFAN (desc);
  }
  else
  {
    if (epg->sel_pad)
    {
      debugMsg ("removing selection pad\n");
      assert (ERR != delwin (epg->sel_pad));
      epg->sel_pad = NULL;
    }
  }
}
#endif


static int
epg_enough_space (EpgView * epg)
{
  int min_w, min_h;
  min_w = 10 + epg->col_w + 3;
  min_h = 3 + 4 + 3;
  return ((epg->w > min_w) && (epg->h > min_h));
}

static void
epg_align (EpgView * epg)
{
  int i, j, s, e;
  int cur_x = epg->cur_x, cur_y = epg->cur_y, w = epg->w - 10, h = epg->h - 3;

  if (!epg_enough_space (epg))
    return;

  i = epg->sel_col;
  j = epg->sel_idx;
  s = epg->cols[i].entries[j].start_line;
  e = epg->cols[i].entries[j].end_line;


  if (((i + 1) * epg->col_w) > (cur_x + w))
    cur_x = (i + 1) * epg->col_w - w;
  if ((i * epg->col_w) < cur_x)
    cur_x = i * epg->col_w;

  if (e > (cur_y + h))
    cur_y = e - h;
  if (s < cur_y)
    cur_y = s;

  epg->cur_x = cur_x;
  epg->cur_y = cur_y;
}

static void
epg_ui_nav_down (EpgView * epg)
{
  unsigned i, j;
  i = epg->sel_col;
  j = epg->sel_idx;
  j++;
  if (j < epg->cols[i].num_entries)
  {
    epg_hilight_selection (epg, 0);
    epg->sel_idx = j;
    epg_hilight_selection (epg, 1);
    epg_align (epg);
    epg->nav_y = (CUR_E (epg).end_line + CUR_E (epg).start_line) / 2;
  }
  epg->sel_line = 0;
}

static void
epg_ui_nav_lasti (EpgView * epg)
{
  int i, j;
  i = epg->sel_col;
  j = epg->cols[i].num_entries - 1;
  epg_hilight_selection (epg, 0);
  epg->sel_idx = j;
  epg_hilight_selection (epg, 1);
  epg_align (epg);
  epg->nav_y = (CUR_E (epg).end_line + CUR_E (epg).start_line) / 2;
  epg->sel_line = 0;
}

static void
epg_ui_nav_firsti (EpgView * epg)
{
  int j;
  j = 0;
  epg_hilight_selection (epg, 0);
  epg->sel_idx = j;
  epg_hilight_selection (epg, 1);
  epg_align (epg);
  epg->nav_y = (CUR_E (epg).end_line + CUR_E (epg).start_line) / 2;
  epg->sel_line = 0;
}

static void
epg_ui_nav_up (EpgView * epg)
{
  int j;
//      i=epg->sel_col;
  j = epg->sel_idx;
  j--;
  if (j >= 0)
  {
    epg_hilight_selection (epg, 0);
    epg->sel_idx = j;
    epg_hilight_selection (epg, 1);
    epg_align (epg);
    epg->nav_y = (CUR_E (epg).end_line + CUR_E (epg).start_line) / 2;
  }
  epg->sel_line = 0;
}

#define LU_IDX(_EV) (CUR_C(_EV).idx_lut[(_EV)->nav_y])

static void
epg_ui_nav_left (EpgView * epg)
{
  int i;
  i = epg->sel_col;
  if (i > 0)
  {
    i--;
    epg_hilight_selection (epg, 0);
    epg->sel_col = i;
    epg->sel_idx = LU_IDX (epg);
    epg_hilight_selection (epg, 1);
    epg_align (epg);
  }
  epg->sel_line = 0;
}

static void
epg_ui_nav_right (EpgView * epg)
{
  unsigned i;
  i = epg->sel_col;
  if (i < (epg->num_columns - 1))
  {
    i++;
    epg_hilight_selection (epg, 0);
    epg->sel_col = i;
    epg->sel_idx = LU_IDX (epg);
    epg_hilight_selection (epg, 1);
    epg_align (epg);
  }
  epg->sel_line = 0;
}


/**
 *\brief coloring/attributes for content descriptor categories
 
typedef struct
{
	int c_pair;
	uint8_t fg_color,bg_color;
	int attr;//UNDERLINE or BLINK
}CatCol;

perhaps later..
*/

static const char *day_str[] = {
  "Sun",
  "Mon",
  "Tue",
  "Wed",
  "Thu",
  "Fri",
  "Sat"
};

static void
epg_draw_sel (EpgView * epg)
{
  int cur_y = epg->cur_y, cur_x = epg->cur_x, y = epg->y, x = epg->x, h =
    epg->h;
  int s = epg->cols[epg->sel_col].entries[epg->sel_idx].start_line, e =
    epg->cols[epg->sel_col].entries[epg->sel_idx].end_line;
  int sel_x = -cur_x + x + 12 + epg->sel_col * epg->col_w;
  int sel_y = -cur_y + y + 3 + s + 3;
  int sel_yend = sel_y + (e - s) - 4;
  time_t start, duration;
  struct tm src;

  start = epg->cols[epg->sel_col].entries[epg->sel_idx].start_time;
  duration = epg->cols[epg->sel_col].entries[epg->sel_idx].duration;
  localtime_r (&start, &src);
  /*
     Have to clear explicitly, because otherwise prev data leaks through.
     Can not clear completely, because I did not bother to allocate a window
     and I would lose the message area...
     have to change... later
   */
  mvhline (epg->y, epg->x, ' ', 10);
  mvhline (epg->y + 1, epg->x, ' ', 10);
  mvhline (epg->y + 2, epg->x, ' ', 10);
  mvprintw (epg->y, epg->x, "%s %2.2u.%2.2u.",
            struGetStr (day_str, ((unsigned) src.tm_wday)), src.tm_mday,
            src.tm_mon + 1);
  mvprintw (epg->y + 1, epg->x, "%2.2u:%2.2u:%2.2u  ", src.tm_hour,
            src.tm_min, src.tm_sec);
  mvprintw (epg->y + 2, epg->x, "%6u min", duration / 60);

  if (NULL == epg->sel_pad)
    return;
  if ((y + h - 1) < sel_yend)
  {
    sel_yend = y + h - 1;
  }
  pnoutrefresh (epg->sel_pad, epg->sel_line, 0, sel_y, sel_x, sel_yend,
                sel_x + epg->col_w - 2);
}

static unsigned
epg_sel_lim (EpgView * epg)
{
  int maxy;
  if (NULL == epg->sel_pad)
    return 0;
  maxy = getmaxy (epg->sel_pad);
  return maxy / 2 - 1;
}

static short
epg_lookup_color (char col)
{
  switch (col)
  {
  case 'w':
    return COLOR_WHITE;
    break;
  case 'r':
    return COLOR_RED;
    break;
  case 'g':
    return COLOR_GREEN;
    break;
  case 'b':
    return COLOR_BLUE;
    break;
  case 'c':
    return COLOR_CYAN;
    break;
  case 'y':
    return COLOR_YELLOW;
    break;
  case 'm':
    return COLOR_MAGENTA;
    break;
  case 'k':
    return COLOR_BLACK;
    break;
  default:
    return COLOR_BLACK;
    break;
  }

}

/**
 *\brief initialize colors for epg titles
 *
 * Programme titles will be colored according to their first
 * content descriptor. 
 * Which colors to use may be specified
 * as a character string with the following formatting:
 * The string consists of at most 11 color pair specs separated
 * by commas ','.
 * each pair specifier requires a foreground and background color
 * separated by a colon ':'
 * Colors are represented by the letters:
 * 'w','r','g','b','c','m','y' and 'k'
 * specifying white,red,green,blue,cyan,magenta,yellow and black 
 * respectively.
 * Pairs will be used in following order
 * Unspecified,Drama,News,Show,Sports,Kids,Music,Arts,Social,Science,Leisure
 * from left to right.
 * if pairs are missing, (hopefully sane)default colors will be specified.
 */
static void
epg_init_colors (EpgView * epg, char const *spec)
{
  int i = 0;
  char *dupe;
  char *saveptr;
  char *tok;
  if (NULL != spec)
  {
    dupe = strndup ((char *) spec, (11 * 4));
    tok = strtok_r (dupe, ",", &saveptr);
    while ((NULL != tok) && (strlen (tok) >= 3) && (i < 11))
    {
      epg->colors[i][0] = epg_lookup_color (tok[0]);
      epg->colors[i][1] = epg_lookup_color (tok[2]);
      i++;
      tok = strtok_r (NULL, ",", &saveptr);
    }
    utlFAN (dupe);
  }
//init remainder
  for (; i < 6; i++)
  {
    epg->colors[i][0] = i;
    epg->colors[i][1] = 0;
  }
  for (; i < 11; i++)
  {
    epg->colors[i][0] = i - 6;
    epg->colors[i][1] = 7;
  }
}

void
epgUiAction (EpgView * epg, EpgReq which)
{
  switch (which)
  {
  case NAV_D:
    epg_ui_nav_down (epg);
    break;
  case NAV_U:
    epg_ui_nav_up (epg);
    break;
  case NAV_L:
    epg_ui_nav_left (epg);
    break;
  case NAV_R:
    epg_ui_nav_right (epg);
    break;
  case NAV_IFIRST:
    epg_ui_nav_firsti (epg);
    break;
  case NAV_ILAST:
    epg_ui_nav_lasti (epg);
    break;
    /*
       case NAV_CFIRST:
       break;
       case NAV_CLAST:
       break; */
  case SCR_U:
    if (epg->sel_line < epg_sel_lim (epg))
      epg->sel_line++;
    break;
  case SCR_D:
    if (epg->sel_line > 0)
      epg->sel_line--;
    break;
  default:
    break;
  }
}

void
epgUiChange (EpgView * epg, int x, int y, int w, int h)
{
  epg->x = x;
  epg->y = y;
  epg->w = w;
  epg->h = h;
  epg_align (epg);
}

void
epgUiGetPos (EpgView * epg, unsigned *col_ret, unsigned *idx_ret)
{
  *col_ret = epg->sel_col;
  *idx_ret = epg->sel_idx;
}

/*
static void epg_ui_interact(AppData *a,EpgView *epg,Favourite * f,WINDOW * ep_pad,WINDOW * s_pad,WINDOW * OAh_pad,int x,int y,int w,int h,int col_w)
{
	int cur_x=0,cur_y=0;
//	int xsz=col_w*epg->num_columns+1;
	int state=0;
	int c;
	int i,j;//,k;
	epg->sel_col=0;
	epg->sel_idx=0;
	for(i=1;i<11;i++)
	{
		init_pair(i,epg->colors[i][0],epg->colors[i][1]);
	}
	epg_hilight_selection(a,epg,ep_pad,col_w,1);
	epg_align(epg,&cur_x,&cur_y,w-10,h-3,col_w);
	while(state==0)
	{
//		epg_ui_draw_header(a,e,cur_x,x+10,y,w-10,col_w);
//		epg_ui_draw_scale(a,e,x,y+3,h-3,col_w);
		pnoutrefresh(s_pad,cur_y,0,y+3,x,y+h-1,x+9);
		pnoutrefresh(h_pad,0,cur_x,y,x+10,y+2,x+w-1);
		pnoutrefresh(ep_pad,cur_y,cur_x,y+3,x+10,y+h-1,x+w-1);
		epg_draw_sel(a,epg,cur_y,cur_x,y,x,h,w,col_w);
		doupdate();
		c=appDataGetch(a);
		switch(c)
		{
			case KEY_BACKSPACE://backspace
			case 27://Esc
			state=-1;
			break;
			case KEY_DOWN:
			epg_ui_nav_down(a,epg,ep_pad,&cur_x,&cur_y,w-10,h-3,col_w);
			break;
			case KEY_UP:
			epg_ui_nav_up(a,epg,ep_pad,&cur_x,&cur_y,w-10,h-3,col_w);
			break;
			case KEY_LEFT:
			epg_ui_nav_left(a,epg,ep_pad,&cur_x,&cur_y,w-10,h-3,col_w);
			break;
			case KEY_RIGHT:
			epg_ui_nav_right(a,epg,ep_pad,&cur_x,&cur_y,w-10,h-3,col_w);
			break;
			case KEY_RESIZE:
			break;
			case 13:
			tune_fav(&f[epg->sel_col],a);
			break;
			case 's':
			i=epg->sel_col;
			j=epg->sel_idx;
			sched_epgevt(f[i].pos,f[i].freq,f[i].pol,f[i].pnr,epg->sched[i].events[j],a);
			break;
			case 'm':
			i=epg->sel_col;
			j=epg->sel_idx;
			remind_epgevt(f[i].pos,f[i].freq,f[i].pol,f[i].pnr,&f[i].service_name,epg->sched[i].events[j],a);
			break;
			case 'c':
			if(epg->sel_line<epg_sel_lim(epg))
				epg->sel_line++;
			break;
			case 'x':
			if(epg->sel_line>0)
				epg->sel_line--;
			break;
			default:
			break;
		}
	}
	epg_hilight_selection(a,epg,ep_pad,col_w,0);
}
*/
void
epgUiDraw (EpgView * epg)
{
  int cur_y = epg->cur_y, cur_x = epg->cur_x, y = epg->y, x = epg->x, h =
    epg->h, w = epg->w;
  int i;
  if (!epg_enough_space (epg))
    return;
  for (i = 1; i < 11; i++)
  {
    init_pair (i, epg->colors[i][0], epg->colors[i][1]);
  }
//  clear();//clears too much
//  refresh ();
  pnoutrefresh (epg->s_p, cur_y, 0, y + 3, x, y + h - 1, x + 9);
  pnoutrefresh (epg->h_p, 0, cur_x, y, x + 10, y + 2, x + w - 1);
  epgUiUpdate (epg);
  pnoutrefresh (epg->e_p, cur_y, cur_x, y + 3, x + 10, y + h - 1, x + w - 1);
  epg_draw_sel (epg);
  doupdate ();
}

/*
void epgUiRun(AppData *a,EpgView *epg,Favourite * f,int x,int y,int w,int h,int col_w)
{
	WINDOW *OAe_pad,*s_pad,*h_pad;
	int xsz=col_w*epg->num_columns+1;//one more for final line
	int ysz=epg->nlines;
	debugMsg("Creating pad. Size: %dx%d\n",xsz,ysz);
	message(a,"This is the EPG Browser. Use Cursor Keys to navigate, Press Carriage Return to tune, x and c scroll description. Use s to schedule for recording, m to memorize a reminder. Backspace exits.\n");
	epg_init_colors(epg);
	e_pad=newpad(ysz,xsz);
	s_pad=newpad(ysz,10);
	h_pad=newpad(3,xsz);
	epg_ui_draw_cols(a,epg,e_pad,col_w);
	epg_ui_draw_items(a,epg,e_pad,col_w);
	epg_ui_draw_scale(a,epg,s_pad,col_w);
	epg_ui_draw_header(a,epg,h_pad,col_w);
	epg_ui_interact(a,epg,f,e_pad,s_pad,h_pad,x,y,w,h,col_w);
	delwin(h_pad);
	delwin(s_pad);
	delwin(e_pad);
}
*/

///update progress indicators. call once in a while
void
epgUiUpdate (EpgView * epg)
{
  uint32_t i = 0;
  uint32_t j = 0;
  time_t start, duration;
  int day, mo, yr, hr, min, sec;
  struct tm src;
  time_t now = time (NULL);
  for (i = 0; i < epg->num_columns; i++)
  {
    if (epg->cols[i].now_on != -1)
    {
      j = epg->cols[i].now_on;
    }
    else
    {
      j = 0;
    }
    for (; j < epg->cols[i].num_entries; j++)
    {
      //draw_box
      int s = epg->cols[i].entries[j].start_line;
      int e = epg->cols[i].entries[j].end_line;
      chtype c;
      start = evtGetStart (epg->sched[i].events[j]);
      hr = get_h (start);
      min = get_m (start);
      sec = get_s (start);
      mjd_to_ymd (ts_to_mjd (start), &yr, &mo, &day);
      src.tm_min = min;
      src.tm_sec = sec;
      src.tm_hour = hr;
      src.tm_mday = day;
      src.tm_mon = mo - 1;
      src.tm_year = yr - 1900;
      start = timegm (&src);    //avoid me. I'm a gNU extension
      duration = evtGetDuration (epg->sched[i].events[j]);
      c = '*';
      if ((epg->sel_col == i) && (epg->sel_idx == j))
        c |= A_REVERSE;
      //number of asterisks to draw vertically inside box(progress indicator)
      if (now > (start + duration))
      {                         //past
        mvwvline (epg->e_p, s + 1, i * epg->col_w + 1, c, e - s - 1);
      }
      else if (now >= start)
      {                         //on now
        if (duration > 0)
        {
          mvwvline (epg->e_p, s + 1, i * epg->col_w + 1, c,
                    (now - start) * (e - s - 1) / duration);
          epg->cols[i].now_on = j;
        }
      }
      else
      {                         //not yet
        break;
      }
    }
  }
}
