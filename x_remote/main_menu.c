#include "main_menu.h"
#include "master_list.h"
#include "reminder_list.h"
#include "sched.h"
#include "fav_list_dlg.h"

void
mainMenuMap (Widget w NOTUSED, XtPointer client_data,
             XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmanageChild (pgm->current);
  XtManageChild (pgm->main_form);
  pgm->current = NULL;
}

static void
display_main_list (Widget w NOTUSED, XtPointer client_data,
                   XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->main_form);
  XtMapWidget (pgm->mlist_form);
/*
 	XtUnmanageChild(pgm->main_form);
 	XtManageChild(pgm->mlist_form);
 	*/
  pgm->current = pgm->mlist_form;
  masterListDisplay (pgm);
}

static void
display_fav_list (Widget w NOTUSED, XtPointer client_data,
                  XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->main_form);
  XtMapWidget (pgm->favlist_form);
  favListDlgDisplay (pgm);
  pgm->current = pgm->favlist_form;
}

static void
display_sched (Widget w NOTUSED, XtPointer client_data,
               XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->main_form);
  XtMapWidget (pgm->sched_form);
  schedDisplay (pgm);
  pgm->current = pgm->sched_form;
}

static void
display_rmd (Widget w NOTUSED, XtPointer client_data,
             XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->main_form);
  XtMapWidget (pgm->rmd_form);
  reminderListDisplay (pgm);
  pgm->current = pgm->rmd_form;
}

static void
display_setup (Widget w NOTUSED, XtPointer client_data,
               XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->main_form);
  XtMapWidget (pgm->setup_form);
  pgm->current = pgm->setup_form;
}

static void
main_menu_quit (Widget w, XtPointer client_data NOTUSED,
                XtPointer call_data NOTUSED)
{
//      PgmState * pgm=client_data;
  XtAppSetExitFlag (XtWidgetToApplicationContext (w));
}

void
mainMenuInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget mlist, flist, sched, rmds, setup;      //,exit;
  XtCallbackRec cl[2];
  cl[0].callback = display_main_list;
  cl[0].closure = pgm;
  cl[1].callback = NULL;
  cl[1].closure = NULL;

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
  mlist =
    XtCreateManagedWidget ("Browse All", commandWidgetClass, prnt, arglist,
                           i);


  cl[0].callback = display_fav_list;
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
  XtSetArg (arglist[i], XtNfromVert, mlist);
  i++;
  flist =
    XtCreateManagedWidget ("Browse Favourites", commandWidgetClass, prnt,
                           arglist, i);

  cl[0].callback = display_sched;
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
  XtSetArg (arglist[i], XtNfromVert, flist);
  i++;
  sched =
    XtCreateManagedWidget ("Rec Sched", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = display_rmd;
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
  XtSetArg (arglist[i], XtNfromVert, sched);
  i++;
  rmds =
    XtCreateManagedWidget ("Reminder List", commandWidgetClass, prnt, arglist,
                           i);

  cl[0].callback = display_setup;
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
  XtSetArg (arglist[i], XtNfromVert, rmds);
  i++;
  setup =
    XtCreateManagedWidget ("Setup", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = main_menu_quit;
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
  XtSetArg (arglist[i], XtNfromVert, setup);
  i++;
  /*exit= */ XtCreateManagedWidget ("Exit", commandWidgetClass, prnt, arglist,
                                    i);
}
