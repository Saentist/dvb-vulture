#include "setup.h"
#include "mod_tp.h"
#include "switch_dlg.h"

static void
setup_mod_tp (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  modTpDisplay (pgm);
  XtUnmapWidget (pgm->setup_form);
  XtMapWidget (pgm->mod_tp_form);
}

static void
setup_switch (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  switchDlgDisplay (pgm);
  XtUnmapWidget (pgm->setup_form);
  XtMapWidget (pgm->switch_form);
}

static void
setup_ft_fe (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  feFtDisplay (pgm);
  XtUnmapWidget (pgm->setup_form);
  XtMapWidget (pgm->fe_ft_form);
}

static void
setup_ed_fav (Widget w NOTUSED, XtPointer client_data NOTUSED, XtPointer call_data NOTUSED)
{                               //TODO
}

static void
setup_back (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->setup_form);
  XtMapWidget (pgm->main_form);
}

void
setupDisplay (PgmState * pgm NOTUSED)
{
}

void
setupInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget mod_tp, sw_set, fe_ft, ed_fav; //,back;
  XtCallbackRec cl[2];
  cl[1].callback = NULL;
  cl[1].closure = NULL;

  cl[0].callback = setup_mod_tp;
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
  mod_tp =
    XtCreateManagedWidget ("Transponders", commandWidgetClass, prnt, arglist,
                           i);

  cl[0].callback = setup_switch;
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
  XtSetArg (arglist[i], XtNfromVert, mod_tp);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  sw_set =
    XtCreateManagedWidget ("Switch/Antenna Setup", commandWidgetClass, prnt,
                           arglist, i);

  cl[0].callback = setup_ft_fe;
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
  XtSetArg (arglist[i], XtNfromVert, sw_set);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  fe_ft =
    XtCreateManagedWidget ("Finetune Frontend", commandWidgetClass, prnt,
                           arglist, i);

  cl[0].callback = setup_ed_fav;
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
  XtSetArg (arglist[i], XtNfromVert, fe_ft);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  ed_fav =
    XtCreateManagedWidget ("Edit Favourites", commandWidgetClass, prnt,
                           arglist, i);

  cl[0].callback = setup_back;
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
  XtSetArg (arglist[i], XtNfromVert, ed_fav);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  /*back= */ XtCreateManagedWidget ("Back", commandWidgetClass, prnt, arglist,
                                    i);
}
