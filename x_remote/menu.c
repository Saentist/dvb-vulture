#include"menu.h"
#include"vt_dlg.h"
#include"mod_tp.h"
#include"epg_dlg.h"
#include"switch_dlg.h"
#include"fav_list_dlg.h"

void
menuInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;

  //the form with initial menu
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
//  XtSetArg(arglist[i],XtNwidth,550);i++;
//  XtSetArg(arglist[i],XtNheight,200);i++;
  XtSetArg (arglist[i], XtNmappedWhenManaged, False);
  i++;

  pgm->main_form =
    XtCreateManagedWidget ("MainMenuForm", formWidgetClass, prnt, arglist, i);
  mainMenuInit (pgm->main_form, pgm);

  //the setup form
  pgm->setup_form =
    XtCreateManagedWidget ("SetupForm", formWidgetClass, prnt, arglist, i);
  setupInit (pgm->setup_form, pgm);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNmappedWhenManaged, False);
  i++;

  //the form with master list, next/prev tp commands etc... and a back button
  pgm->mlist_form =
    XtCreateManagedWidget ("MasterListForm", formWidgetClass, prnt, arglist,
                           i);
  masterListInit (pgm->mlist_form, pgm);

  //program guide
  pgm->epg_form =
    XtCreateManagedWidget ("EpgForm", formWidgetClass, prnt, arglist, i);
  epgDlgInit (pgm->epg_form, pgm);

  //videotext
  pgm->vt_form =
    XtCreateManagedWidget ("VtForm", formWidgetClass, prnt, arglist, i);
  vtDlgInit (pgm->vt_form, pgm);

  //the form with favlist
  pgm->favlist_form =
    XtCreateManagedWidget ("FavListForm", formWidgetClass, prnt, arglist, i);
  favListDlgInit (pgm->favlist_form, pgm);

  //scheduled recordings
  pgm->sched_form =
    XtCreateManagedWidget ("SchedForm", formWidgetClass, prnt, arglist, i);
  schedInit (pgm->sched_form, pgm);

  //the switch form
  pgm->switch_form =
    XtCreateManagedWidget ("SwitchForm", formWidgetClass, prnt, arglist, i);
  switchDlgInit (pgm->switch_form, pgm);

/*
	pgm->stats_form=XtCreateManagedWidget("StatsForm",formWidgetClass,prnt,arglist,i);
	statsInit(pgm->stats_form,pgm);
*/

  pgm->rmd_form =
    XtCreateManagedWidget ("ReminderForm", formWidgetClass, prnt, arglist, i);
  reminderListInit (pgm->rmd_form, pgm);

  pgm->rmd_ed_form =
    XtCreateManagedWidget ("ReminderEdForm", formWidgetClass, prnt, arglist,
                           i);
  reminderEdInit (pgm->rmd_ed_form, pgm);

  pgm->mod_tp_form =
    XtCreateManagedWidget ("ModTpForm", formWidgetClass, prnt, arglist, i);
  modTpInit (pgm->mod_tp_form, pgm);

  pgm->ed_sched_form =
    XtCreateManagedWidget ("EdSched", formWidgetClass, prnt, arglist, i);
  schedEdInit (pgm->ed_sched_form, pgm);

  pgm->fe_ft_form =
    XtCreateManagedWidget ("FeFt", formWidgetClass, prnt, arglist, i);
  feFtInit (pgm->fe_ft_form, pgm);

//      pgm->fav_ed_form=XtCreateManagedWidget("FavEd",formWidgetClass,prnt,arglist,i);
//      favEdInit(pgm->fav_ed_form,pgm);
}
