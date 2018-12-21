#include <inttypes.h>
#include"client.h"
#include"menu_strings.h"
#include"epg_dlg.h"
#include"vt_dlg.h"
#include"mod_tp.h"
#define tpInfoDestroy(_TPI) free(_TPI)

static void
mod_tp_purge_tp (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (NULL != pgm->transponders)
  {
    tpInfoDestroy (pgm->transponders);
    pgm->transponders = NULL;
    pgm->num_transponders = 0;
    pgm->tp_idx = XAW_LIST_NONE;

    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->mod_tp_tp_list, arglist, i);
    XawListChange (pgm->mod_tp_tp_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->mod_tp_tp_list, False);
    mnuStrFree (list, sz);
  }
}

static void
mod_tp_purge_pos (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (0 != pgm->num_pos)
  {
    pgm->num_pos = 0;
    pgm->pos_idx = XAW_LIST_NONE;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->mod_tp_pos_list, arglist, i);
    XawListChange (pgm->mod_tp_pos_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->mod_tp_pos_list, False);
    mnuStrFree (list, sz);
  }

}

void
mod_tp_load_tp (PgmState * pgm)
{
  char **list;
  uint32_t num_tp;
  TransponderInfo *tpi;

  mod_tp_purge_tp (pgm);

  if (XAW_LIST_NONE == pgm->pos_idx)
  {
    return;
  }

  tpi = clientGetTransponders (pgm->sock, pgm->pos_idx, &num_tp);
  pgm->transponders = tpi;
  pgm->num_transponders = num_tp;
  list = mnuStrTp (tpi, num_tp);
  XawListChange (pgm->mod_tp_tp_list, list, num_tp, 0, True);
  if (num_tp)
    XtSetSensitive (pgm->mod_tp_tp_list, True);
}

static void
mod_tp_load_pos (PgmState * pgm)
{
  uint32_t num_pos;
  char **list;
  uint32_t i;

  pgm->num_pos = num_pos = clientListPos (pgm->sock);
  list = calloc (sizeof (char *), num_pos);
  for (i = 0; i < num_pos; i++)
  {
    list[i] = calloc (30, sizeof (char));
    sprintf (list[i], "Pos: %" PRIu32, i);
  }
  XawListChange (pgm->mod_tp_pos_list, list, num_pos, 0, True);
  if (num_pos)
    XtSetSensitive (pgm->mod_tp_pos_list, True);
}

void
modTpDisplay (PgmState * pgm)
{
  pgm->transponders = NULL;
  pgm->num_transponders = 0;
  pgm->num_pos = 0;
  pgm->tp_idx = XAW_LIST_NONE;
  pgm->pos_idx = XAW_LIST_NONE;
  mod_tp_load_pos (pgm);
}

static void
mod_tp_pos (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->pos_idx = str->list_index;
  mod_tp_load_tp (pgm);
}

static void
mod_tp_tp (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  char buf[20];
  XawListReturnStruct *str = call_data;
  pgm->tp_idx = str->list_index;
  if (pgm->pos_idx == XAW_LIST_NONE)
    return;
  if (pgm->tp_idx == XAW_LIST_NONE)
    return;
  snprintf (buf, 20, "%d", pgm->transponders[pgm->tp_idx].ftune);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buf);
  i++;
  XtSetValues (pgm->ft_txt, arglist, i);
}

static void
del_tp (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  TransponderInfo *tpi;
  if (pgm->pos_idx == XAW_LIST_NONE)
    return;
  if (pgm->tp_idx == XAW_LIST_NONE)
    return;
  tpi = pgm->transponders + pgm->tp_idx;
  clientDelTp (pgm->sock, pgm->pos_idx, tpi->frequency, tpi->pol);
  mod_tp_load_tp (pgm);
}

static void
mod_tp_back (Widget w NOTUSED, XtPointer client_data,
             XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  mod_tp_purge_pos (pgm);
  mod_tp_purge_tp (pgm);
  XtUnmapWidget (pgm->mod_tp_form);
  XtMapWidget (pgm->setup_form);
}

static void
mod_tp_ft (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  TransponderInfo *tpi;
  int ft;
  char *buf = NULL;
  snprintf (buf, 20, "%d", pgm->transponders[pgm->tp_idx].ftune);
  if (pgm->pos_idx == XAW_LIST_NONE)
    return;
  if (pgm->tp_idx == XAW_LIST_NONE)
    return;
  i = 0;
  XtSetArg (arglist[i], XtNstring, &buf);
  i++;
  XtGetValues (pgm->ft_txt, arglist, i);
  ft = atoi (buf);
  tpi = pgm->transponders + pgm->tp_idx;
  if (clientFtTp (pgm->sock, pgm->pos_idx, tpi->frequency, tpi->pol, ft))
    return;
  tpi->ftune = ft;
}

void
modTpInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
//      Widget pgm_list;
  Widget back, del, pos_vp, tp_vp;      //,ft;
  XtCallbackRec cl[2];
  cl[1].callback = NULL;
  cl[1].closure = NULL;

  cl[0].callback = mod_tp_back;
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

  cl[0].callback = del_tp;
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
  del = XtCreateManagedWidget ("Del", commandWidgetClass, prnt, arglist, i);

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
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->ft_txt =
    XtCreateManagedWidget ("FtTxt", asciiTextWidgetClass, prnt, arglist, i);

  cl[0].callback = mod_tp_ft;
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->ft_txt);
  i++;
  /*ft= */ XtCreateManagedWidget ("Finetune", commandWidgetClass, prnt,
                                  arglist, i);

  cl[0].callback = mod_tp_pos;
  cl[0].closure = pgm;

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNheight, 170);
  i++;
  XtSetArg (arglist[i], XtNwidth, 80);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  pos_vp =
    XtCreateManagedWidget ("PositionVP", viewportWidgetClass, prnt, arglist,
                           i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->mod_tp_pos_list =
    XtCreateManagedWidget ("Position", listWidgetClass, pos_vp, arglist, i);

  cl[0].callback = mod_tp_tp;
  cl[0].closure = pgm;

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pos_vp);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNheight, 170);
  i++;
  XtSetArg (arglist[i], XtNwidth, 170);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  tp_vp =
    XtCreateManagedWidget ("TranspondersVP", viewportWidgetClass, prnt,
                           arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->mod_tp_tp_list =
    XtCreateManagedWidget ("Transponders", listWidgetClass, tp_vp, arglist,
                           i);

}
