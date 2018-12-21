#include"sched_ed.h"
#include"menu_strings.h"
#include"client.h"
#include"sched.h"

void schedLoad (PgmState * a);

static void
sched_ed_load_type (PgmState * pgm)
{
  XawListHighlight (pgm->sched_ed_type, pgm->rse->type);
}

static void
sched_ed_load_wd (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Arg arglist2[20];
  Cardinal i2 = 0;
  i = 0;
  XtSetArg (arglist[i], XtNstate, True);
  i++;

  i2 = 0;
  XtSetArg (arglist2[i2], XtNstate, False);
  i2++;

  if (pgm->rse->wd & WD_SUN)
    XtSetValues (pgm->sun, arglist, i);
  else
    XtSetValues (pgm->sun, arglist2, i2);

  if (pgm->rse->wd & WD_MON)
    XtSetValues (pgm->mon, arglist, i);
  else
    XtSetValues (pgm->mon, arglist2, i2);

  if (pgm->rse->wd & WD_TUE)
    XtSetValues (pgm->tue, arglist, i);
  else
    XtSetValues (pgm->tue, arglist2, i2);

  if (pgm->rse->wd & WD_WED)
    XtSetValues (pgm->wed, arglist, i);
  else
    XtSetValues (pgm->wed, arglist2, i2);

  if (pgm->rse->wd & WD_THU)
    XtSetValues (pgm->thu, arglist, i);
  else
    XtSetValues (pgm->thu, arglist2, i2);

  if (pgm->rse->wd & WD_FRI)
    XtSetValues (pgm->fri, arglist, i);
  else
    XtSetValues (pgm->fri, arglist2, i2);

  if (pgm->rse->wd & WD_SAT)
    XtSetValues (pgm->sat, arglist, i);
  else
    XtSetValues (pgm->sat, arglist2, i2);
}

static void
sched_ed_load_flags (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Arg arglist2[20];
  Cardinal i2 = 0;
  i = 0;
  XtSetArg (arglist[i], XtNstate, True);
  i++;

  i2 = 0;
  XtSetArg (arglist2[i2], XtNstate, False);
  i2++;

  if (pgm->rse->flags & RSF_SKIP_VT)
    XtSetValues (pgm->f_vt, arglist, i);
  else
    XtSetValues (pgm->f_vt, arglist2, i2);

  if (pgm->rse->flags & RSF_SKIP_SPU)
    XtSetValues (pgm->f_spu, arglist, i);
  else
    XtSetValues (pgm->f_spu, arglist2, i2);

  if (pgm->rse->flags & RSF_SKIP_DSM)
    XtSetValues (pgm->f_dsm, arglist, i);
  else
    XtSetValues (pgm->f_dsm, arglist2, i2);

  if (pgm->rse->flags & RSF_AUD_ONLY)
    XtSetValues (pgm->f_aud, arglist, i);
  else
    XtSetValues (pgm->f_aud, arglist2, i2);

  if (pgm->rse->flags & RSF_EIT)
    XtSetValues (pgm->f_eit, arglist, i);
  else
    XtSetValues (pgm->f_eit, arglist2, i2);

}

static void
sched_ed_load_pos (PgmState * pgm)
{
  char buffer[20];
  Arg arglist[20];
  Cardinal i = 0;
  sprintf (buffer, "%hhu", pgm->rse->pos);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_pos, arglist, i);
}

static void
sched_ed_load_pol (PgmState * pgm)
{
  XawListHighlight (pgm->sched_ed_pol, pgm->rse->pol);
}

static void
sched_ed_load_freq (PgmState * pgm)
{
  char buffer[20];
  Arg arglist[20];
  Cardinal i = 0;
  sprintf (buffer, "%d,%d", pgm->rse->freq / 100000, pgm->rse->freq % 100000);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_freq, arglist, i);
}

static void
sched_ed_load_pnr (PgmState * pgm)
{
  char buffer[20];
  Arg arglist[20];
  Cardinal i = 0;
  sprintf (buffer, "%hu", pgm->rse->pnr);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_pnr, arglist, i);
}

static void
sched_ed_load_start (PgmState * pgm)
{
  struct tm start_time;
  char buffer[20];
  Arg arglist[20];
  Cardinal i = 0;

  localtime_r (&pgm->rse->start_time, &start_time);

  sprintf (buffer, "%d", start_time.tm_year + 1900);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_y, arglist, i);

  sprintf (buffer, "%d", start_time.tm_mon + 1);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_m, arglist, i);

  sprintf (buffer, "%d", start_time.tm_mday);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_d, arglist, i);

  sprintf (buffer, "%d", start_time.tm_hour);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_hr, arglist, i);

  sprintf (buffer, "%d", start_time.tm_min);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_min, arglist, i);

  sprintf (buffer, "%d", start_time.tm_sec);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_sec, arglist, i);

}

static void
sched_ed_load_duration (PgmState * pgm)
{
  char buffer[20];
  Arg arglist[20];
  Cardinal i = 0;
  sprintf (buffer, "%ld", pgm->rse->duration / 60);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_dr, arglist, i);
}

static void
sched_ed_load_evtid (PgmState * pgm)
{
  char buffer[20];
  Arg arglist[20];
  Cardinal i = 0;
  sprintf (buffer, "0x%4.4hx", pgm->rse->evt_id & 0xffff);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->sched_ed_eit, arglist, i);
}

static void
sched_ed_load_name (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  i = 0;
  XtSetArg (arglist[i], XtNstring, pgm->rse->name);
  i++;
  XtSetValues (pgm->sched_ed_nm, arglist, i);
}

static void
sched_ed_store (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *lr;
  Boolean st;
  char *str;
  uint16_t tmp;
  int a, b;
  Arg arglist[20];
  Cardinal i = 0;
  struct tm start_time;
  lr = XawListShowCurrent (pgm->sched_ed_type);
  pgm->rse->type = lr->list_index;
  free (lr);

  pgm->rse->wd = 0;
  i = 0;
  XtSetArg (arglist[i], XtNstate, &st);
  i++;

  XtGetValues (pgm->sun, arglist, i);
  if (st)
    pgm->rse->wd |= WD_SUN;

  XtGetValues (pgm->mon, arglist, i);
  if (st)
    pgm->rse->wd |= WD_MON;

  XtGetValues (pgm->tue, arglist, i);
  if (st)
    pgm->rse->wd |= WD_TUE;

  XtGetValues (pgm->wed, arglist, i);
  if (st)
    pgm->rse->wd |= WD_WED;

  XtGetValues (pgm->thu, arglist, i);
  if (st)
    pgm->rse->wd |= WD_THU;

  XtGetValues (pgm->fri, arglist, i);
  if (st)
    pgm->rse->wd |= WD_FRI;

  pgm->rse->flags = 0;
  XtGetValues (pgm->f_vt, arglist, i);
  if (st)
    pgm->rse->flags |= RSF_SKIP_VT;

  XtGetValues (pgm->f_spu, arglist, i);
  if (st)
    pgm->rse->flags |= RSF_SKIP_SPU;

  XtGetValues (pgm->f_dsm, arglist, i);
  if (st)
    pgm->rse->flags |= RSF_SKIP_DSM;

  XtGetValues (pgm->f_aud, arglist, i);
  if (st)
    pgm->rse->flags |= RSF_AUD_ONLY;

  XtGetValues (pgm->f_eit, arglist, i);
  if (st)
    pgm->rse->flags |= RSF_EIT;

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_pos, arglist, i);
  sscanf (str, "%hhu", &pgm->rse->pos);
  //free(str);

  lr = XawListShowCurrent (pgm->sched_ed_pol);
  pgm->rse->pol = lr->list_index;
  free (lr);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_freq, arglist, i);
  sscanf (str, "%d,%d", &a, &b);
  //free(str);
  pgm->rse->freq = a * 100000 + b % 100000;

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_pnr, arglist, i);
  sscanf (str, "%hu", &pgm->rse->pnr);
  //free(str);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_y, arglist, i);
  sscanf (str, "%d", &start_time.tm_year);
  start_time.tm_year -= 1900;
  //free(str);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_m, arglist, i);
  sscanf (str, "%d", &start_time.tm_mon);
  start_time.tm_mon -= 1;
  //free(str);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_d, arglist, i);
  sscanf (str, "%d", &start_time.tm_mday);
  //free(str);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_hr, arglist, i);
  sscanf (str, "%d", &start_time.tm_hour);
  //free(str);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_min, arglist, i);
  sscanf (str, "%d", &start_time.tm_min);
  //free(str);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_sec, arglist, i);
  sscanf (str, "%d", &start_time.tm_sec);
  //free(str);

  pgm->rse->start_time = mktime (&start_time);

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_dr, arglist, i);
  sscanf (str, "%ld", &pgm->rse->duration);
  //free(str);
  pgm->rse->duration *= 60;

  i = 0;
  XtSetArg (arglist[i], XtNstring, &str);
  i++;
  XtGetValues (pgm->sched_ed_eit, arglist, i);
  sscanf (str, "0x%4hx", &tmp);
  //free(str);
  pgm->rse->evt_id = tmp & 0xffff;

  i = 0;
  XtSetArg (arglist[i], XtNstring, &pgm->rse->name);
  i++;
  XtGetValues (pgm->sched_ed_nm, arglist, i);
  if (pgm->rse->name)
    pgm->rse->name = strdup (pgm->rse->name);

  pgm->rse->state = RS_WAITING;

  clientRecSched (pgm->sock, pgm->r_sch, pgm->r_sch_sz, pgm->pr_sch,
                  pgm->pr_sch_sz);
  rsDestroy (pgm->r_sch, pgm->r_sch_sz);
  rsDestroy (pgm->pr_sch, pgm->pr_sch_sz);
  schedLoad (pgm);

//      clientRecSched(pgm->sock,pgm->sched_idx,pgm->rse);
//      schedChanged(pgm);
}

static void
sched_ed_back (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->ed_sched_form);
  XtMapWidget (pgm->sched_form);
}

void
schedEdDisplay (PgmState * pgm)
{
  pgm->rse = pgm->r_sch + pgm->sched_idx;
  sched_ed_load_type (pgm);
  sched_ed_load_wd (pgm);
  sched_ed_load_flags (pgm);
  sched_ed_load_pos (pgm);
  sched_ed_load_pol (pgm);
  sched_ed_load_freq (pgm);
  sched_ed_load_pnr (pgm);
  sched_ed_load_start (pgm);
  sched_ed_load_duration (pgm);
  sched_ed_load_evtid (pgm);
  sched_ed_load_name (pgm);
}

void
schedEdInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget back, /*store,l_eit,l_sec, */ l_freq, l_pos, l_pnr, l_y, l_m, l_d,
    l_dr, l_nm, l_hr, l_min;
  XtCallbackRec cl[2];
  String *type_list;
  pgm->sched_idx = XAW_LIST_NONE;
  pgm->sched_loaded = 0;

  cl[1].callback = NULL;
  cl[1].closure = NULL;
  cl[0].callback = sched_ed_back;
  cl[0].closure = pgm;

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

  cl[0].callback = sched_ed_store;
  cl[0].closure = pgm;
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
  XtSetArg (arglist[i], XtNfromHoriz, back);
  i++;
  /*store= */ XtCreateManagedWidget ("Store", commandWidgetClass, prnt,
                                     arglist, i);

  type_list = calloc (sizeof (String), RST_COUNT);
  for (i = 0; i < RST_COUNT; i++)
  {
    type_list[i] = (String) rsGetTypeStr (i);
  }

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNverticalList, True);
  i++;
  XtSetArg (arglist[i], XtNlist, type_list);
  i++;
  XtSetArg (arglist[i], XtNnumberStrings, RST_COUNT);
  i++;
  pgm->sched_ed_type =
    XtCreateManagedWidget ("Type", listWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  pgm->sun =
    XtCreateManagedWidget ("Sun", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->sun);
  i++;
  pgm->mon =
    XtCreateManagedWidget ("Mon", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->mon);
  i++;
  pgm->tue =
    XtCreateManagedWidget ("Tue", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->tue);
  i++;
  pgm->wed =
    XtCreateManagedWidget ("Wed", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->wed);
  i++;
  pgm->thu =
    XtCreateManagedWidget ("Thu", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->thu);
  i++;
  pgm->fri =
    XtCreateManagedWidget ("Fri", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->fri);
  i++;
  pgm->sat =
    XtCreateManagedWidget ("Sat", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sat);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  pgm->f_vt =
    XtCreateManagedWidget ("No VT", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sat);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->f_vt);
  i++;
  pgm->f_spu =
    XtCreateManagedWidget ("No SPU", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sat);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->f_spu);
  i++;
  pgm->f_dsm =
    XtCreateManagedWidget ("No DSM", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sat);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->f_dsm);
  i++;
  pgm->f_aud =
    XtCreateManagedWidget ("Only Audio", toggleWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sat);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->f_aud);
  i++;
  pgm->f_eit =
    XtCreateManagedWidget ("Use EIT", toggleWidgetClass, prnt, arglist, i);


  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  l_pos =
    XtCreateManagedWidget ("Position", labelWidgetClass, prnt, arglist, i);


  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_pos);
  i++;
  l_freq =
    XtCreateManagedWidget ("Frequency", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_freq);
  i++;
  l_pnr =
    XtCreateManagedWidget ("Pgm Number", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_pnr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  pgm->sched_ed_pos =
    XtCreateManagedWidget ("Pos", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_pnr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_pos);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_freq =
    XtCreateManagedWidget ("Freq", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_pnr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_freq);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_pnr =
    XtCreateManagedWidget ("Pnr", asciiTextWidgetClass, prnt, arglist, i);

  type_list = calloc (sizeof (String), NUM_POL);
  for (i = 0; i < NUM_POL; i++)
  {
    type_list[i] = (String) tpiGetPolStr (i);
  }

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_freq);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNverticalList, True);
  i++;
  XtSetArg (arglist[i], XtNlist, type_list);
  i++;
  XtSetArg (arglist[i], XtNnumberStrings, NUM_POL);
  i++;
  pgm->sched_ed_pol =
    XtCreateManagedWidget ("Pol", listWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->sched_ed_pol);
  i++;
  l_y = XtCreateManagedWidget ("Year", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_y);
  i++;
  l_m = XtCreateManagedWidget ("Month", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_m);
  i++;
  l_d = XtCreateManagedWidget ("Day", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_d);
  i++;
  l_dr =
    XtCreateManagedWidget ("Duration", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->f_aud);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_dr);
  i++;
  /*l_eit= */ XtCreateManagedWidget ("Evt Id", labelWidgetClass, prnt,
                                     arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->sat);
  i++;
  l_nm = XtCreateManagedWidget ("Name", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_dr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->sched_ed_pol);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_y =
    XtCreateManagedWidget ("Year", asciiTextWidgetClass, prnt, arglist, i);


  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_dr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_y);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_m =
    XtCreateManagedWidget ("Month", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_dr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_m);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_d =
    XtCreateManagedWidget ("Day", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_dr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_d);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_dr =
    XtCreateManagedWidget ("Duration", asciiTextWidgetClass, prnt, arglist,
                           i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_dr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_dr);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_eit =
    XtCreateManagedWidget ("Evt Id", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_nm);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->sat);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_nm =
    XtCreateManagedWidget ("Name", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_y);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->sched_ed_pol);
  i++;
  l_hr = XtCreateManagedWidget ("H", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_y);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_hr);
  i++;
  l_min = XtCreateManagedWidget ("M", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->sched_ed_y);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_min);
  i++;
  /*l_sec= */ XtCreateManagedWidget ("S", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_hr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->sched_ed_pol);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 30);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_hr =
    XtCreateManagedWidget ("Hr", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_hr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_hr);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 30);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_min =
    XtCreateManagedWidget ("Min", asciiTextWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, l_hr);
  i++;
  XtSetArg (arglist[i], XtNfromVert, l_min);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 30);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->sched_ed_sec =
    XtCreateManagedWidget ("Sec", asciiTextWidgetClass, prnt, arglist, i);
}
