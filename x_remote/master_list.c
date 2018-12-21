#include <inttypes.h>
#include"master_list.h"
#include"client.h"
#include"menu_strings.h"
#include"epg_dlg.h"
#include"timestamp.h"
#include"vt_dlg.h"
#define tpInfoDestroy(_TPI) free(_TPI)

static void
pgmInfoDestroy (ProgramInfo * pgi, int sz)
{
  int i;
  if (NULL == pgi)
    return;
  for (i = 0; i < sz; i++)
  {
    programInfoClear (pgi + i);
  }
  free (pgi);
}

static void
master_list_purge_pos (PgmState * pgm)
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
    XtGetValues (pgm->pos_list, arglist, i);
    XawListChange (pgm->pos_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->pos_list, False);
    mnuStrFree (list, sz);
  }

}

static void
master_list_purge_tp (PgmState * pgm)
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
    XtGetValues (pgm->tp_list, arglist, i);
    XawListChange (pgm->tp_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->tp_list, False);
    mnuStrFree (list, sz);
  }

}

static void
master_list_purge_pgm (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (NULL != pgm->programs)
  {
    pgmInfoDestroy (pgm->programs, pgm->num_programs);
    pgm->programs = NULL;
    pgm->num_programs = 0;
    pgm->pgm_idx = XAW_LIST_NONE;

    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->pgm_list, arglist, i);
    XawListChange (pgm->pgm_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->pgm_list, False);
    mnuStrFree (list, sz);
  }
}

void
masterListLoadPgm (PgmState * pgm)
{
  char **list;
  uint32_t num_pgm;
  ProgramInfo *pgi;

  master_list_purge_pgm (pgm);
  if ((XAW_LIST_NONE == pgm->tp_idx) || (XAW_LIST_NONE == pgm->pos_idx))
  {
    return;
  }

  pgi =
    clientGetPgms (pgm->sock, pgm->pos_idx,
                   pgm->transponders[pgm->tp_idx].frequency,
                   pgm->transponders[pgm->tp_idx].pol, &num_pgm);
  pgm->programs = pgi;
  pgm->num_programs = num_pgm;
  list = mnuStrPgms (pgi, num_pgm);
  XawListChange (pgm->pgm_list, list, num_pgm, 0, True);
  if (num_pgm)
    XtSetSensitive (pgm->pgm_list, True);
}

void
masterListLoadTp (PgmState * pgm)
{
  char **list;
  uint32_t num_tp;
  TransponderInfo *tpi;

  master_list_purge_tp (pgm);
  master_list_purge_pgm (pgm);

  if (XAW_LIST_NONE == pgm->pos_idx)
  {
    return;
  }

  pgm->transponders = tpi =
    clientGetTransponders (pgm->sock, pgm->pos_idx, &num_tp);
  pgm->num_transponders = num_tp;
  list = mnuStrTp (tpi, num_tp);
  XawListChange (pgm->tp_list, list, num_tp, 0, True);
  if (num_tp)
    XtSetSensitive (pgm->tp_list, True);
}

void
masterListLoadPos (PgmState * pgm)
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
  XawListChange (pgm->pos_list, list, num_pos, 0, True);
  if (num_pos)
    XtSetSensitive (pgm->pos_list, True);
}

static void
master_list_pos (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->pos_idx = str->list_index;
  masterListLoadTp (pgm);
}

static void
master_list_tp (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->tp_idx = str->list_index;
  masterListLoadPgm (pgm);
}

static void
master_list_pgm (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->pgm_idx = str->list_index;
}


static int
master_list_valid_pgm (PgmState * pgm)
{
  return ((pgm->pgm_idx != XAW_LIST_NONE) && (pgm->tp_idx != XAW_LIST_NONE)
          && (pgm->pos_idx != XAW_LIST_NONE));
}

static void
master_list_tune (Widget w NOTUSED, XtPointer client_data,
                  XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (master_list_valid_pgm (pgm))
  {
    clientDoTune (pgm->sock, pgm->pos_idx,
                  pgm->transponders[pgm->tp_idx].frequency,
                  pgm->transponders[pgm->tp_idx].pol,
                  pgm->programs[pgm->pgm_idx].pnr);
  }
}

static void
master_list_epg (Widget w NOTUSED, XtPointer client_data,
                 XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  EpgArray *epga = NULL;
  uint32_t start, end, num_entries = 0;
  long num_days = 7;
  long tmp;

  if (master_list_valid_pgm (pgm))
  {
    if (!rcfileFindValInt (&pgm->cfg, "EPG", "num_days", &tmp))
      num_days = tmp;
    if (!rcfileFindValInt (&pgm->cfg, "EPG", "num_entries", &tmp))
      num_entries = tmp;
    start = epg_get_buf_time ();
    if (num_days)
      end = start + num_days * 3600 * 24;
    else
      end = 0;
    epga =
      clientGetEpg (pgm->sock, pgm->pos_idx,
                    pgm->transponders[pgm->tp_idx].frequency,
                    pgm->transponders[pgm->tp_idx].pol,
                    &pgm->programs[pgm->pgm_idx].pnr, 1, start, end,
                    num_entries);
  }
  pgm->epga = epga;
  if (NULL != epga)
  {
    //hide current dialog and show epg here
    pgm->pos = pgm->pos_idx;
    pgm->parent = pgm->mlist_form;
    pgm->frequency = pgm->transponders[pgm->tp_idx].frequency;
    pgm->pol = pgm->transponders[pgm->tp_idx].pol;
    pgm->pnr = pgm->programs[pgm->pgm_idx].pnr;
    pgm->svc_name = &pgm->programs[pgm->pgm_idx].service_name;
    epgDlgDisplay (pgm);
    XtUnmapWidget (pgm->mlist_form);
    XtMapWidget (pgm->epg_form);
/*	 	XtUnrealizeWidget(pgm->mlist_form);
	 	XtRealizeWidget(pgm->epg_form);*/
  }
}

static void
master_list_vt (Widget w NOTUSED, XtPointer client_data,
                XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  uint8_t *vtsvc;
  uint16_t *vtpids;
  uint16_t num_pids, num_svc;
  //hide current dialog and show vt here
  if ((XAW_LIST_NONE == pgm->tp_idx) || (XAW_LIST_NONE == pgm->pos_idx)
      || (XAW_LIST_NONE == pgm->pgm_idx))
    return;
  pgm->pos = pgm->pos_idx;
  pgm->frequency = pgm->transponders[pgm->tp_idx].frequency;
  pgm->pol = pgm->transponders[pgm->tp_idx].pol;        //&pgm->programs[pgm->pgm_idx].pnr
  vtpids =
    clientVtGetPids (pgm->sock, pgm->pos, pgm->frequency, pgm->pol,
                     pgm->programs[pgm->pgm_idx].pnr, &num_pids);
  if (NULL == vtpids)
    return;
  vtsvc =
    clientVtGetSvc (pgm->sock, pgm->pos, pgm->frequency, pgm->pol, vtpids[0],
                    &num_svc);
  if (NULL == vtsvc)
  {
    free (vtpids);
    return;
  }

  //TODO: allow selection of pid/service
  pgm->pid = vtpids[0];
  pgm->svc_id = vtsvc[0];
  pgm->parent = pgm->mlist_form;
  free (vtpids);
  free (vtsvc);
  XtUnmapWidget (pgm->mlist_form);
  XtMapWidget (pgm->vt_form);
  vtDlgDisplay (pgm);
}

static void
master_list_rec_start (Widget w NOTUSED, XtPointer client_data,
                       XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  clientRecordStart (pgm->sock);
}

static void
master_list_rec_stop (Widget w NOTUSED, XtPointer client_data,
                      XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  clientRecordStop (pgm->sock);
}

static void
master_list_scan_tp (Widget w NOTUSED, XtPointer client_data,
                     XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (!((pgm->tp_idx != XAW_LIST_NONE) && (pgm->pos_idx != XAW_LIST_NONE)))
    return;
  clientScanTp (pgm->sock, pgm->pos_idx,
                pgm->transponders[pgm->tp_idx].frequency,
                pgm->transponders[pgm->tp_idx].pol);
}

static void
master_list_add_fav (Widget w NOTUSED, XtPointer client_data,
                     XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  Favourite tmp;
  Favourite *n = NULL;
  if (NULL == pgm->current_fav_list)
    return;
  if (!master_list_valid_pgm (pgm))
    return;
  tmp.pos = pgm->pos_idx;
  tmp.freq = pgm->transponders[pgm->tp_idx].frequency;
  tmp.pol = pgm->transponders[pgm->tp_idx].pol;
  tmp.pnr = pgm->programs[pgm->pgm_idx].pnr;
  tmp.type = pgm->programs[pgm->pgm_idx].svc_type;

  tmp.service_name.buf = NULL;
  tmp.service_name.len = 0;
  dvbStringCopy (&tmp.service_name,
                 &pgm->programs[pgm->pgm_idx].service_name);

  tmp.provider_name.buf = NULL;
  tmp.provider_name.len = 0;
  dvbStringCopy (&tmp.provider_name,
                 &pgm->programs[pgm->pgm_idx].provider_name);

  tmp.bouquet_name.buf = NULL;
  tmp.bouquet_name.len = 0;
  dvbStringCopy (&tmp.bouquet_name,
                 &pgm->programs[pgm->pgm_idx].bouquet_name);

  n =
    realloc (pgm->current_fav_list->favourites,
             (pgm->current_fav_list->size + 1) * sizeof (Favourite));
  if (!n)
  {
//              errMsg("Sorry, couldn't reallocate FavList\n");
    favClear (&tmp);
    return;
  }
  pgm->current_fav_list->favourites = n;
  memmove (pgm->current_fav_list->favourites + pgm->current_fav_list->size,
           &tmp, sizeof (Favourite));
  pgm->current_fav_list->size++;
  return;
}

void
masterListDisplay (PgmState * pgm)
{
  pgm->transponders = NULL;
  pgm->num_transponders = 0;
  pgm->num_pos = 0;
  pgm->tp_idx = XAW_LIST_NONE;
  pgm->programs = NULL;
  pgm->num_programs = 0;
  pgm->pgm_idx = XAW_LIST_NONE;
  pgm->pos_idx = XAW_LIST_NONE;
  masterListLoadPos (pgm);
}

static void
master_list_back (Widget w NOTUSED, XtPointer client_data,
                  XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  master_list_purge_pos (pgm);
  master_list_purge_tp (pgm);
  master_list_purge_pgm (pgm);
  XtUnmapWidget (pgm->mlist_form);
  XtMapWidget (pgm->main_form);
  /*
     XtUnmanageChild(pgm->mlist_form);
     XtManageChild(pgm->main_form);
   */
}

void
masterListInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
//      Widget pgm_list;
  Widget back, tune, epg, rec_start /*,rec_stop,vt,add_fav */ , scan_tp,
    pos_vp, tp_vp, pgm_vp;
  XtCallbackRec cl[2];
  cl[1].callback = NULL;
  cl[1].closure = NULL;

  cl[0].callback = master_list_back;
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

  cl[0].callback = master_list_tune;
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
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  tune = XtCreateManagedWidget ("Tune", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = master_list_epg;
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
  epg = XtCreateManagedWidget ("EPG", commandWidgetClass, prnt, arglist, i);


  cl[0].callback = master_list_vt;
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
  XtSetArg (arglist[i], XtNfromVert, epg);
  i++;
  /*vt= */ XtCreateManagedWidget ("VT", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = master_list_rec_start;
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
  XtSetArg (arglist[i], XtNfromHoriz, epg);
  i++;
  rec_start =
    XtCreateManagedWidget ("Rec Start", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = master_list_rec_stop;
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
  XtSetArg (arglist[i], XtNfromHoriz, epg);
  i++;
  XtSetArg (arglist[i], XtNfromVert, rec_start);
  i++;
  /*rec_stop= */ XtCreateManagedWidget ("Rec Stop", commandWidgetClass, prnt,
                                        arglist, i);

  cl[0].callback = master_list_scan_tp;
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
  XtSetArg (arglist[i], XtNfromHoriz, rec_start);
  i++;
  scan_tp =
    XtCreateManagedWidget ("Scan Tp", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = master_list_add_fav;
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
  XtSetArg (arglist[i], XtNfromHoriz, rec_start);
  i++;
  XtSetArg (arglist[i], XtNfromVert, scan_tp);
  i++;
  /*add_fav= */ XtCreateManagedWidget ("Add To Fav", commandWidgetClass, prnt,
                                       arglist, i);

  cl[0].callback = master_list_pos;
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
  XtSetArg (arglist[i], XtNfromVert, tune);
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

  pgm->pos_list =
    XtCreateManagedWidget ("Position", listWidgetClass, pos_vp, arglist, i);

  cl[0].callback = master_list_tp;
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
  XtSetArg (arglist[i], XtNfromVert, tune);
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

  pgm->tp_list =
    XtCreateManagedWidget ("Transponders", listWidgetClass, tp_vp, arglist,
                           i);

  cl[0].callback = master_list_pgm;
  cl[0].closure = pgm;

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, tp_vp);
  i++;
  XtSetArg (arglist[i], XtNfromVert, tune);
  i++;
  XtSetArg (arglist[i], XtNheight, 170);
  i++;
  XtSetArg (arglist[i], XtNwidth, 230);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  pgm_vp =
    XtCreateManagedWidget ("ProgramsVP", viewportWidgetClass, prnt, arglist,
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

  pgm->pgm_list =
    XtCreateManagedWidget ("Programs", listWidgetClass, pgm_vp, arglist, i);

}
