#include"sched_ed.h"
#include"sched.h"
#include"menu_strings.h"
#include"client.h"


void schedLoad (PgmState * a);

static void
sched_purge (PgmState * a)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;
  if (a->sched_loaded)
  {

    rsDestroy (a->r_sch, a->r_sch_sz);
    rsDestroy (a->pr_sch, a->pr_sch_sz);
    a->sched_loaded = 0;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (a->sched_list, arglist, i);
    XawListChange (a->sched_list, NULL, 0, 0, True);
    XtSetSensitive (a->sched_list, False);
    mnuStrFree (list, sz);
  }
}

void
schedChanged (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;
  int num_strings;
  char **mnu_str;
  int idx;
//      XawListReturnStruct *lr;

//      lr=XawListShowCurrent(pgm->sched_list);



  i = 0;
  XtSetArg (arglist[i], XtNlist, &list);
  i++;
  XtSetArg (arglist[i], XtNnumberStrings, &sz);
  i++;
  XtGetValues (pgm->sched_list, arglist, i);
  XawListChange (pgm->sched_list, NULL, 0, 0, True);
  XtSetSensitive (pgm->sched_list, False);
  mnuStrFree (list, sz);

  mnu_str = mnuStrRse (pgm->r_sch, pgm->r_sch_sz);
  if (mnu_str == NULL)
    return;

  num_strings = pgm->r_sch_sz;
  XawListChange (pgm->sched_list, mnu_str, num_strings, 0, True);
  XtSetSensitive (pgm->sched_list, True);
  if (pgm->sched_idx == XAW_LIST_NONE)
    idx = XAW_LIST_NONE;
  else
    idx =
      (((unsigned)pgm->sched_idx) < pgm->r_sch_sz) ? pgm->sched_idx : ((int)pgm->r_sch_sz - 1);
  XawListHighlight (pgm->sched_list, idx);
//      free(lr);
}

static void
sched_back (Widget w NOTUSED, XtPointer client_data,
            XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  sched_purge (pgm);
  XtUnmapWidget (pgm->sched_form);
  XtMapWidget (pgm->main_form);
}

static void
sched_edit (Widget w NOTUSED, XtPointer client_data,
            XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (pgm->sched_idx == XAW_LIST_NONE)
    return;
  XtUnmapWidget (pgm->sched_form);
  XtMapWidget (pgm->ed_sched_form);
  schedEdDisplay (pgm);
}

static void
sched_del (Widget w NOTUSED, XtPointer client_data,
           XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (pgm->sched_idx == XAW_LIST_NONE)
    return;
  rsClearEntry (pgm->r_sch + pgm->sched_idx);
  memmove (pgm->r_sch + pgm->sched_idx, pgm->r_sch + pgm->sched_idx + 1,
           (pgm->r_sch_sz - pgm->sched_idx - 1) * sizeof (RsEntry));
  pgm->r_sch_sz--;
  if (clientRecSched
      (pgm->sock, pgm->r_sch, pgm->r_sch_sz, pgm->pr_sch, pgm->pr_sch_sz))
  {
//              rsDestroy(pgm->r_sch,pgm->r_sch_sz);
//              if(!pgm->r_sch=rsCloneSched(pgm->pr_sch,pgm->pr_sch_sz))
//                      return;
  }
  rsDestroy (pgm->r_sch, pgm->r_sch_sz);
  rsDestroy (pgm->pr_sch, pgm->pr_sch_sz);
  schedLoad (pgm);
//      rsClearEntry(pgm->r_sch+pgm->sched_idx);
//      clientRecSched(pgm->sock,pgm->sched_idx,pgm->r_sch+pgm->sched_idx);

//      schedChanged(pgm);
}


static void
sched_add (Widget w NOTUSED, XtPointer client_data,
           XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  void *n;
  if (pgm->sched_idx == XAW_LIST_NONE)
    return;
  n = realloc (pgm->r_sch, sizeof (RsEntry) * (pgm->r_sch_sz + 1));
  if (n)
  {
    pgm->r_sch = n;
    rsClearEntry (pgm->r_sch + pgm->r_sch_sz);
    pgm->sched_idx = pgm->r_sch_sz;
    pgm->r_sch_sz++;
    schedChanged (pgm);
    XtUnmapWidget (pgm->sched_form);
    XtMapWidget (pgm->ed_sched_form);
    schedEdDisplay (pgm);
//              if(clientRecSched(pgm->sock,pgm->r_sch,pgm->r_sch_sz,pgm->pr_sch,pgm->pr_sch_sz))
//              {
//              }
//              rsDestroy(pgm->r_sch,pgm->r_sch_sz);
//              rsDestroy(pgm->pr_sch,pgm->pr_sch_sz);
//              schedLoad(pgm);
//                              message(a,"REC_SCHED failed\n");
  }
//      schedChanged(pgm);
}

static void
sched_list (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->sched_idx = str->list_index;
}

void
schedLoad (PgmState * a)
{
  int num_strings;
  char **mnu_str;

  if (clientGetSched (a->sock, &a->pr_sch, &a->pr_sch_sz))
    return;

  if (!(a->r_sch = rsCloneSched (a->pr_sch, a->pr_sch_sz)))
    return;

  a->r_sch_sz = a->pr_sch_sz;

  mnu_str = mnuStrRse (a->r_sch, a->r_sch_sz);
  if (mnu_str == NULL)
    return;

  num_strings = a->r_sch_sz;
  XawListChange (a->sched_list, mnu_str, num_strings, 0, True);
  XtSetSensitive (a->sched_list, True);
  a->sched_idx = XAW_LIST_NONE;
  a->sched_loaded = 1;
}


void
schedDisplay (PgmState * pgm)
{
  pgm->sched_idx = XAW_LIST_NONE;
  schedLoad (pgm);
}


void
schedInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget back, edit, add /*,del */ , sched_list_vp;
  XtCallbackRec cl[2];

  pgm->sched_idx = XAW_LIST_NONE;
  pgm->sched_loaded = 0;

  cl[1].callback = NULL;
  cl[1].closure = NULL;
  cl[0].callback = sched_back;
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

  cl[0].callback = sched_edit;
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
  edit = XtCreateManagedWidget ("Edit", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = sched_add;
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
  XtSetArg (arglist[i], XtNfromHoriz, edit);
  i++;
  add = XtCreateManagedWidget ("Add", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = sched_del;
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
  XtSetArg (arglist[i], XtNfromHoriz, add);
  i++;
  /*del= */ XtCreateManagedWidget ("Del", commandWidgetClass, prnt, arglist,
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
//  XtSetArg(arglist[i],XtNfromHoriz,pos_vp);i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNheight, 200);
  i++;
  XtSetArg (arglist[i], XtNwidth, 490);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  sched_list_vp =
    XtCreateManagedWidget ("SchedListVP", viewportWidgetClass, prnt, arglist,
                           i);

  cl[0].callback = sched_list;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNverticalList, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  pgm->sched_list =
    XtCreateManagedWidget ("SchedList", listWidgetClass, sched_list_vp,
                           arglist, i);
}
