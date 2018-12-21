#include"reminder_list.h"
#include"reminder_ed.h"
#include"menu_strings.h"

static void
reminder_list_load (PgmState * a)
{
  int num_strings;
  char **mnu_str;

  mnu_str = mnuStrRemList (&a->reminder_list, &num_strings);
  if (mnu_str == NULL)
    return;

  XawListChange (a->rmd_list, mnu_str, num_strings, 0, True);
  XtSetSensitive (a->rmd_list, True);
  a->rmd_loaded = 1;
}


static void
reminder_list_purge (PgmState * a)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;
  if (a->rmd_loaded)
  {
    a->rmd_loaded = 0;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (a->rmd_list, arglist, i);
    XawListChange (a->rmd_list, NULL, 0, 0, True);
    XtSetSensitive (a->rmd_list, False);
    mnuStrFree (list, sz);
  }
}

void
reminderListChanged (PgmState * pgm)
{
  reminder_list_purge (pgm);
  reminder_list_load (pgm);
}

static void
reminder_list_del (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  DListE *e;
  int i;
  if (pgm->rmd_idx == XAW_LIST_NONE)
    return;
  e = dlistFirst (&pgm->reminder_list);
  i = pgm->rmd_idx;
  while (i--)
  {
    e = dlistENext (e);
  }
  dlistRemove (&pgm->reminder_list, e);
  reminderDel (e);
  reminderListChanged (pgm);
  pgm->rmd_idx = XAW_LIST_NONE;
}

static void
reminder_list_ed (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (pgm->rmd_idx == XAW_LIST_NONE)
    return;
  XtUnmapWidget (pgm->rmd_form);
  XtMapWidget (pgm->rmd_ed_form);
  reminderEdDisplay (pgm);

}

static void
reminder_list_select (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->rmd_idx = str->list_index;

}

static void
reminder_list_back (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  reminder_list_purge (pgm);
  XtUnmapWidget (pgm->rmd_form);
  XtMapWidget (pgm->main_form);
}

void
reminderListDisplay (PgmState * pgm)
{
  pgm->rmd_idx = XAW_LIST_NONE;
  reminder_list_load (pgm);
}


void
reminderListInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget rmd_list_vp, back, del;        //,edit;
  XtCallbackRec cl[2];
  cl[1].callback = NULL;
  cl[1].closure = NULL;

  pgm->rmd_idx = XAW_LIST_NONE;
  pgm->rmd_loaded = 0;

  cl[0].callback = reminder_list_back;
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

  cl[0].callback = reminder_list_del;
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
  del =
    XtCreateManagedWidget ("Delete", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = reminder_list_ed;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, del);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  /*edit= */ XtCreateManagedWidget ("Edit", commandWidgetClass, prnt, arglist,
                                    i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNheight, 200);
  i++;
  XtSetArg (arglist[i], XtNwidth, 490);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  rmd_list_vp =
    XtCreateManagedWidget ("ReminderListVP", viewportWidgetClass, prnt,
                           arglist, i);

  cl[0].callback = reminder_list_select;
  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNverticalList, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  pgm->rmd_list =
    XtCreateManagedWidget ("ReminderList", listWidgetClass, rmd_list_vp,
                           arglist, i);
}
