#include "fav_list_dlg.h"
#include "menu_strings.h"
#include "iterator.h"
#include "client.h"
#include "epg_dlg.h"
#include "timestamp.h"
#include "debug.h"
#include "vt_dlg.h"

static void
fav_list_dlg_purge_fav (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (pgm->fav_loaded)
  {
    pgm->fav_loaded = 0;
    pgm->fav_idx = XAW_LIST_NONE;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->fav_list, arglist, i);
    XawListChange (pgm->fav_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->fav_list, False);
    mnuStrFree (list, sz);
  }
}

static void
fav_list_dlg_purge_favlist (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (pgm->flist_loaded)
  {
    pgm->flist_loaded = 0;
//              pgm->current_fav_list=NULL;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->flist_list, arglist, i);
    XawListChange (pgm->flist_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->flist_list, False);
    mnuStrFree (list, sz);
  }
}

void
fav_list_dlg_load_fav (PgmState * pgm)
{
  char **list;
//      int num_strings;

  if (NULL == pgm->current_fav_list)
  {
    return;
  }

  if (pgm->current_fav_list->size == 0)
  {
    return;
  }

  list =
    mnuStrFav (pgm->current_fav_list->favourites,
               pgm->current_fav_list->size);

  XawListChange (pgm->fav_list, list, pgm->current_fav_list->size, 0, True);
  if (pgm->current_fav_list->size)
    XtSetSensitive (pgm->fav_list, True);
  pgm->fav_loaded = 1;
}

static void
fav_list_dlg_load_favlist (PgmState * a)
{
  char **list;
  int i;
  Iterator l;
  int num_strings;

//      message(a,"Select the list to use, please.\nPress Return to select entry. Use Escape or Backspace to cancel the action.\n");
  if (NULL == a->favourite_lists)
  {
    errMsg ("Sorry, no FavLists here. Create some, please.\n");
    return;
  }
  if (iterBTreeInit (&l, a->favourite_lists))
  {
    errMsg ("error initializing iterator.\n");
    return;
  }

  num_strings = iterGetSize (&l);
  list = calloc (sizeof (char *), num_strings);
  for (i = 0; i < num_strings; i++)
  {
    FavList *fl;
    iterSetIdx (&l, i);
    fl = iterGet (&l);
    list[i] = strdup (fl->name);
  }
  iterClear (&l);
  XawListChange (a->flist_list, list, num_strings, 0, True);
  if (num_strings)
    XtSetSensitive (a->flist_list, True);
  a->flist_loaded = 1;
}

static void
fav_list_dlg_flist (Widget w NOTUSED, XtPointer client_data,
                    XtPointer call_data)
{
  PgmState *pgm = client_data;
  Iterator l;
  XawListReturnStruct *str = call_data;

  if (str->list_index == XAW_LIST_NONE)
    return;
  if (iterBTreeInit (&l, pgm->favourite_lists))
  {
    printf ("iter err\n");
//              errMsg("error initializing iterator.\n");
    return;
  }

  iterSetIdx (&l, str->list_index);
  pgm->current_fav_list = iterGet (&l);
  fav_list_dlg_purge_fav (pgm);
  fav_list_dlg_load_fav (pgm);
  iterClear (&l);
}

static void
fav_list_dlg_fav (Widget w NOTUSED, XtPointer client_data,
                  XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->fav_idx = str->list_index;
}

static int
fav_list_dlg_valid_fav (PgmState * pgm)
{
  return ((pgm->flist_loaded) && (pgm->fav_loaded)
          && (pgm->fav_idx != XAW_LIST_NONE));
}

static void
fav_list_dlg_tune (Widget w NOTUSED, XtPointer client_data,
                   XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (fav_list_dlg_valid_fav (pgm))
  {
    clientDoTune (pgm->sock,
                  pgm->current_fav_list->favourites[pgm->fav_idx].pos,
                  pgm->current_fav_list->favourites[pgm->fav_idx].freq,
                  pgm->current_fav_list->favourites[pgm->fav_idx].pol,
                  pgm->current_fav_list->favourites[pgm->fav_idx].pnr);
  }
}

static void
fav_list_dlg_epg (Widget w NOTUSED, XtPointer client_data,
                  XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  EpgArray *epga = NULL;
  uint32_t start, end, num_entries = 0;
  long num_days = 7;
  long tmp;
  if (!fav_list_dlg_valid_fav (pgm))
    return;

  if (!rcfileFindValInt (&pgm->cfg, "EPG", "num_days", &tmp))
    num_days = tmp;
  if (!rcfileFindValInt (&pgm->cfg, "EPG", "num_entries", &tmp))
    num_entries = tmp;
  start = epg_get_buf_time ();
  if (num_days)
    end = start + num_days * 3600 * 24;
  else
    end = 0;

  epga = clientGetEpg (pgm->sock,
                       pgm->current_fav_list->favourites[pgm->fav_idx].pos,
                       pgm->current_fav_list->favourites[pgm->fav_idx].freq,
                       pgm->current_fav_list->favourites[pgm->fav_idx].pol,
                       &pgm->current_fav_list->favourites[pgm->fav_idx].pnr,
                       1, start, end, num_entries);

  pgm->epga = epga;
  if (NULL != epga)
  {
    //hide current dialog and show epg here
    pgm->pos = pgm->current_fav_list->favourites[pgm->fav_idx].pos;
    pgm->parent = pgm->favlist_form;
    pgm->frequency = pgm->current_fav_list->favourites[pgm->fav_idx].freq;
    pgm->pol = pgm->current_fav_list->favourites[pgm->fav_idx].pol;
    pgm->pnr = pgm->current_fav_list->favourites[pgm->fav_idx].pnr;
    pgm->svc_name =
      &pgm->current_fav_list->favourites[pgm->fav_idx].service_name;
    epgDlgDisplay (pgm);
    XtUnmapWidget (pgm->favlist_form);
    XtMapWidget (pgm->epg_form);
  }
}

static void
fav_list_dlg_vt (Widget w NOTUSED, XtPointer client_data,
                 XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  uint8_t *vtsvc;
  uint16_t *vtpids;
  uint16_t num_pids, num_svc;
  //hide current dialog and show vt here
  if (!fav_list_dlg_valid_fav (pgm))
    return;

  pgm->pos = pgm->current_fav_list->favourites[pgm->fav_idx].pos;
  pgm->parent = pgm->favlist_form;
  pgm->frequency = pgm->current_fav_list->favourites[pgm->fav_idx].freq;
  pgm->pol = pgm->current_fav_list->favourites[pgm->fav_idx].pol;
  pgm->pnr = pgm->current_fav_list->favourites[pgm->fav_idx].pnr;

  vtpids =
    clientVtGetPids (pgm->sock, pgm->pos, pgm->frequency, pgm->pol, pgm->pnr,
                     &num_pids);
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
  free (vtpids);
  free (vtsvc);
  XtUnmapWidget (pgm->favlist_form);
  XtMapWidget (pgm->vt_form);
  vtDlgDisplay (pgm);
}

static void
fav_list_dlg_rec_start (Widget w NOTUSED, XtPointer client_data,
                        XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  clientRecordStart (pgm->sock);
}

static void
fav_list_dlg_rec_stop (Widget w NOTUSED, XtPointer client_data,
                       XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  clientRecordStop (pgm->sock);
}

static void
fav_list_dlg_scan_tp (Widget w NOTUSED, XtPointer client_data,
                      XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (!((pgm->flist_loaded) && (pgm->fav_loaded)))
    return;
  clientScanTp (pgm->sock,
                pgm->current_fav_list->favourites[pgm->fav_idx].pos,
                pgm->current_fav_list->favourites[pgm->fav_idx].freq,
                pgm->current_fav_list->favourites[pgm->fav_idx].pol);
}

void
favListDlgDisplay (PgmState * pgm)
{
  pgm->fav_idx = XAW_LIST_NONE;
  pgm->current_fav_list = NULL;
  fav_list_dlg_load_favlist (pgm);
}

static void
fav_list_dlg_back (Widget w NOTUSED, XtPointer client_data,
                   XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  fav_list_dlg_purge_fav (pgm);
  fav_list_dlg_purge_favlist (pgm);
  XtUnmapWidget (pgm->favlist_form);
  XtMapWidget (pgm->main_form);
}

void
favListDlgInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget back, tune, epg, rec_start /*,scan_tp,rec_stop,vt */ , fav_vp,
    fav_list_vp;
  XtCallbackRec cl[2];

  pgm->flist_loaded = 0;
  pgm->fav_loaded = 0;
  pgm->current_fav_list = NULL;
  pgm->fav_idx = XAW_LIST_NONE;

  cl[1].callback = NULL;
  cl[1].closure = NULL;
  cl[0].callback = fav_list_dlg_back;
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

  cl[0].callback = fav_list_dlg_tune;
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

  cl[0].callback = fav_list_dlg_epg;
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


  cl[0].callback = fav_list_dlg_vt;
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

  cl[0].callback = fav_list_dlg_rec_start;
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

  cl[0].callback = fav_list_dlg_rec_stop;
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

  cl[0].callback = fav_list_dlg_scan_tp;
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
  /*scan_tp= */ XtCreateManagedWidget ("Scan Tp", commandWidgetClass, prnt,
                                       arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
//  XtSetArg(arglist[i],XtNfromHoriz,pos_vp);i++;
  XtSetArg (arglist[i], XtNfromVert, tune);
  i++;
  XtSetArg (arglist[i], XtNheight, 160);
  i++;
  XtSetArg (arglist[i], XtNwidth, 100);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  fav_vp =
    XtCreateManagedWidget ("FavVP", viewportWidgetClass, prnt, arglist, i);

  cl[0].callback = fav_list_dlg_flist;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  pgm->flist_list =
    XtCreateManagedWidget ("Fav", listWidgetClass, fav_vp, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, fav_vp);
  i++;
  XtSetArg (arglist[i], XtNfromVert, tune);
  i++;
  XtSetArg (arglist[i], XtNheight, 160);
  i++;
  XtSetArg (arglist[i], XtNwidth, 170);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  fav_list_vp =
    XtCreateManagedWidget ("FavListVP", viewportWidgetClass, prnt, arglist,
                           i);

  cl[0].callback = fav_list_dlg_fav;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->fav_list =
    XtCreateManagedWidget ("FavList", listWidgetClass, fav_list_vp, arglist,
                           i);
}
