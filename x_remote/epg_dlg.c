#include"epg_dlg.h"
#include"timestamp.h"
#include"client.h"
#include"reminder.h"
#include"menu_strings.h"

void
epgDlgDisplay (PgmState * pgm)
{
  char **list;
  Arg arglist[20];
  Cardinal i = 0;
  if (NULL == pgm->epga)
    return;
  if ((NULL == pgm->epga->sched) || (0 == pgm->epga->sched[0].num_events))
  {
    epgArrayDestroy (pgm->epga);
    pgm->epga = NULL;
    return;
  }
  list = mnuStrSched (&pgm->epga->sched[0]);
  i = 0;
  XtSetArg (arglist[i], XtNlist, list);
  i++;
  XtSetArg (arglist[i], XtNnumberStrings, pgm->epga->sched[0].num_events);
  i++;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetValues (pgm->evt_list, arglist, i);
}

static void
epg_dlg_event (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *edd = client_data;
  XawListReturnStruct *str = call_data;

  if (edd->epga)
  {
    char *dest = NULL;
    wchar_t *desc = NULL;
    desc=evtGetDescW (edd->epga->sched[0].events[str->list_index]);
    if(!desc)
    {
      desc=wcsdup(L"");
    }
    dest=calloc(wcstombs(NULL,desc,0)+1,1);
    wcstombs(dest,desc,wcstombs(NULL,desc,0)+1);
    edd->evt_idx = str->list_index;
    i = 0;
    XtSetArg (arglist[i], XtNstring, dest);
    i++;
    XtSetValues (edd->desc_txt, arglist, i);
    free (desc);
    free (dest);
  }
}

static void
epg_sched_init (uint32_t pos, uint32_t frequency, uint8_t pol, uint16_t pnr,
                uint8_t * e, RsEntry * r_e)
{
  time_t start;
  int h, m, s;
  int d, mo, y;
  struct tm src;
  char *n;


  r_e->state = RS_WAITING;
  r_e->type = RST_ONCE;
  r_e->wd = 0;
  r_e->flags = 0;
  r_e->pos = pos;
  r_e->pol = pol;
  r_e->freq = frequency;
  r_e->pnr = pnr;
  r_e->duration = evtGetDuration (e);
  r_e->evt_id = evtGetId (e);

  start = evtGetStart (e);
  h = mnuStrGetH (start);
  m = mnuStrGetM (start);
  s = mnuStrGetS (start);
  mjd_to_ymd (ts_to_mjd (start), &y, &mo, &d);
  src.tm_min = m;
  src.tm_sec = s;
  src.tm_hour = h;
  src.tm_mday = d;
  src.tm_mon = mo - 1;
  src.tm_year = y - 1900;
  r_e->start_time = timegm (&src);      //avoid me. I'm a gNU extension

  n = evtGetName (e);
  if (n)
    utlAsciiSanitize (n);
  r_e->name = n;
}

static void
epg_dlg_sched (Widget w NOTUSED, XtPointer client_data,
               XtPointer call_data NOTUSED)
{
  PgmState *edd = client_data;
  RsEntry r_sch;
  if (edd->evt_idx < 0)
    return;
  epg_sched_init (edd->pos, edd->frequency, edd->pol, edd->pnr,
                  edd->epga->sched[0].events[edd->evt_idx], &r_sch);
  clientRecSchedAdd (edd->sock, &r_sch);
}

static void
epg_dlg_remind (uint32_t pos, uint32_t frequency, uint8_t pol, uint16_t pnr,
                DvbString * svc, uint8_t * e, PgmState * edd)
{
  DListE *rmd;
  time_t start;
  int h, m, s;
  int d, mo, y;
  struct tm src;
  char *ev_name;

  start = evtGetStart (e);
  h = mnuStrGetH (start);
  m = mnuStrGetM (start);
  s = mnuStrGetS (start);
  mjd_to_ymd (ts_to_mjd (start), &y, &mo, &d);
  src.tm_min = m;
  src.tm_sec = s;
  src.tm_hour = h;
  src.tm_mday = d;
  src.tm_mon = mo - 1;
  src.tm_year = y - 1900;

  ev_name = evtGetName (e);

  rmd = reminderNew (RST_ONCE,
                     0,
                     timegm (&src),
                     evtGetDuration (e),
                     pos, frequency, pol, pnr, svc, ev_name);
  free (ev_name);
  if (rmd)
  {
    reminderAdd (&edd->reminder_list, rmd);
//              message(a,"memorized\n");
  }
  else
  {
//      message(a,"failed\n");
  }
}

static void
epg_dlg_mem (Widget w NOTUSED, XtPointer client_data,
             XtPointer call_data NOTUSED)
{
  PgmState *edd = client_data;
  DvbString *snm = edd->svc_name;
  if (edd->evt_idx < 0)
    return;
  epg_dlg_remind (edd->pos, edd->frequency, edd->pol, edd->pnr, snm,
                  edd->epga->sched[0].events[edd->evt_idx], edd);
}

static void
epg_dlg_back (Widget w NOTUSED, XtPointer client_data,
              XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;
  if (NULL != pgm->epga)
  {
    epgArrayDestroy (pgm->epga);
    pgm->evt_idx = XAW_LIST_NONE;
    pgm->epga = NULL;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->evt_list, arglist, i);
    XawListChange (pgm->evt_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->evt_list, False);
    mnuStrFree (list, sz);

  }
  i = 0;
  XtSetArg (arglist[i], XtNstring, NULL);
  i++;
  XtSetValues (pgm->desc_txt, arglist, i);
  XtUnmapWidget (pgm->epg_form);
  XtMapWidget (pgm->parent);
}

void
epgDlgInit (Widget prnt, PgmState * pod)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget back, sched, /*mem, */ evt_vp;
  XtCallbackRec cl[2];

  pod->evt_idx = XAW_LIST_NONE;

  cl[1].callback = NULL;
  cl[1].closure = NULL;

  cl[0].callback = epg_dlg_back;
  cl[0].closure = pod;

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  back = XtCreateManagedWidget ("Back", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = epg_dlg_sched;
  cl[0].closure = pod;

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, back);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  sched =
    XtCreateManagedWidget ("Schedule", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = epg_dlg_mem;
  cl[0].closure = pod;

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, sched);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  /*mem= */ XtCreateManagedWidget ("Memorize", commandWidgetClass, prnt,
                                   arglist, i);


  cl[0].callback = epg_dlg_event;
  cl[0].closure = pod;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawRubber);
  i++;
  XtSetArg (arglist[i], XtNheight, 110);
  i++;
  XtSetArg (arglist[i], XtNwidth, 520);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  evt_vp =
    XtCreateManagedWidget ("EventsVP", viewportWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  pod->evt_list =
    XtCreateManagedWidget ("Events", listWidgetClass, evt_vp, arglist, i);

/*	i=0;
  XtSetArg(arglist[i],XtNleft,XawRubber);i++;
  XtSetArg(arglist[i],XtNright,XawChainRight);i++;
  XtSetArg(arglist[i],XtNtop,XawChainTop);i++;
  XtSetArg(arglist[i],XtNbottom,XawChainBottom);i++;
  XtSetArg(arglist[i],XtNheight,150);i++;
  XtSetArg(arglist[i],XtNwidth,170);i++;
  XtSetArg(arglist[i],XtNallowVert,True);i++;
  XtSetArg(arglist[i],XtNfromHoriz,evt_vp);i++;
	desc_vp=XtCreateManagedWidget("DescVP",viewportWidgetClass,prnt,arglist,i);
	*/
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawRubber);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNheight, 110);
  i++;
  XtSetArg (arglist[i], XtNwidth, 520);
  i++;
  XtSetArg (arglist[i], XtNfromVert, evt_vp);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNstring, NULL);
  i++;
  XtSetArg (arglist[i], XtNresize, XawtextResizeNever);
  i++;
  XtSetArg (arglist[i], XtNscrollVertical, XawtextScrollAlways);
  i++;
  XtSetArg (arglist[i], XtNwrap, XawtextWrapWord);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pod->desc_txt =
    XtCreateManagedWidget ("Event", asciiTextWidgetClass, prnt, arglist, i);

}
