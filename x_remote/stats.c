#include "stats.h"
void
statsInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget stats;                 //,back;
  XtCallbackRec cl[2];
  cl[0].callback = mainMenuMap;
  cl[0].closure = pgm;
  cl[1].callback = NULL;
  cl[1].closure = NULL;

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;

  stats = XtCreateManagedWidget ("Stats", labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, stats);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  /*back= */ XtCreateManagedWidget ("Back", commandWidgetClass, prnt, arglist,
                                    i);
}
