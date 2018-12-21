#include"reminder_ed.h"
#include"reminder_list.h"
#include"menu_strings.h"

static void
rmd_ed_store (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *lr;
  Boolean st;
  Arg arglist[20];
  Cardinal i = 0;
  Reminder *r = NULL;
  r = dlistEPayload (pgm->rmd);

  lr = XawListShowCurrent (pgm->rmd_ed_type);
  r->type = lr->list_index;
  free (lr);

  r->wd = 0;
  i = 0;
  XtSetArg (arglist[i], XtNstate, &st);
  i++;

  XtGetValues (pgm->rmd_sun, arglist, i);
  if (st)
    r->wd |= WD_SUN;

  XtGetValues (pgm->rmd_mon, arglist, i);
  if (st)
    r->wd |= WD_MON;

  XtGetValues (pgm->rmd_tue, arglist, i);
  if (st)
    r->wd |= WD_TUE;

  XtGetValues (pgm->rmd_wed, arglist, i);
  if (st)
    r->wd |= WD_WED;

  XtGetValues (pgm->rmd_thu, arglist, i);
  if (st)
    r->wd |= WD_THU;

  XtGetValues (pgm->rmd_fri, arglist, i);
  if (st)
    r->wd |= WD_FRI;

  reminderListChanged (pgm);
}

static void
rmd_ed_load_type (PgmState * pgm)
{
  Reminder *r = NULL;
  r = dlistEPayload (pgm->rmd);
  XawListHighlight (pgm->rmd_ed_type, r->type);
}

static void
rmd_ed_load_wd (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Arg arglist2[20];
  Cardinal i2 = 0;
  Reminder *r = NULL;
  i = 0;
  XtSetArg (arglist[i], XtNstate, True);
  i++;

  i2 = 0;
  XtSetArg (arglist2[i2], XtNstate, False);
  i2++;

  r = dlistEPayload (pgm->rmd);

  if (r->wd & WD_SUN)
    XtSetValues (pgm->rmd_sun, arglist, i);
  else
    XtSetValues (pgm->rmd_sun, arglist2, i2);

  if (r->wd & WD_MON)
    XtSetValues (pgm->rmd_mon, arglist, i);
  else
    XtSetValues (pgm->rmd_mon, arglist2, i2);

  if (r->wd & WD_TUE)
    XtSetValues (pgm->rmd_tue, arglist, i);
  else
    XtSetValues (pgm->rmd_tue, arglist2, i2);

  if (r->wd & WD_WED)
    XtSetValues (pgm->rmd_wed, arglist, i);
  else
    XtSetValues (pgm->rmd_wed, arglist2, i2);

  if (r->wd & WD_THU)
    XtSetValues (pgm->rmd_thu, arglist, i);
  else
    XtSetValues (pgm->rmd_thu, arglist2, i2);

  if (r->wd & WD_FRI)
    XtSetValues (pgm->rmd_fri, arglist, i);
  else
    XtSetValues (pgm->rmd_fri, arglist2, i2);

  if (r->wd & WD_SAT)
    XtSetValues (pgm->rmd_sat, arglist, i);
  else
    XtSetValues (pgm->rmd_sat, arglist2, i2);
}

static void
rmd_ed_back (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->rmd_ed_form);
  XtMapWidget (pgm->rmd_form);
}

void
reminderEdDisplay (PgmState * pgm)
{
  DListE *e;
  int i;

  e = dlistFirst (&pgm->reminder_list);
  i = pgm->rmd_idx;
  while (i--)
  {
    e = dlistENext (e);
  }
  pgm->rmd = e;

  rmd_ed_load_type (pgm);
  rmd_ed_load_wd (pgm);


//      pgm->rmd_idx=XAW_LIST_NONE;
//      reminder_list_load(pgm);
}


void
reminderEdInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget back;                  //,store;
  XtCallbackRec cl[2];
  String *type_list;

  cl[1].callback = NULL;
  cl[1].closure = NULL;
  cl[0].callback = rmd_ed_back;
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

  cl[0].callback = rmd_ed_store;
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
  pgm->rmd_ed_type =
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->rmd_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  pgm->rmd_sun =
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->rmd_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->rmd_sun);
  i++;
  pgm->rmd_mon =
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->rmd_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->rmd_mon);
  i++;
  pgm->rmd_tue =
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->rmd_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->rmd_tue);
  i++;
  pgm->rmd_wed =
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->rmd_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->rmd_wed);
  i++;
  pgm->rmd_thu =
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->rmd_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->rmd_thu);
  i++;
  pgm->rmd_fri =
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->rmd_ed_type);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->rmd_fri);
  i++;
  pgm->rmd_sat =
    XtCreateManagedWidget ("Sat", toggleWidgetClass, prnt, arglist, i);

}
