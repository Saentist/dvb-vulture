#include <inttypes.h>
#include"client.h"
#include"menu_strings.h"
#include"switch.h"
#include"tp_info.h"
#include"epg_dlg.h"
#include"vt_dlg.h"
#include"switch_dlg.h"
static void
switch_dlg_band_alter (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  char **list;
  uint32_t sz;
  int nb;
  SwitchPos *sp;
  XawListReturnStruct *cur = NULL;
  if (0 != pgm->bands_loaded)
  {
    pgm->band_idx = XAW_LIST_NONE;
    cur = XawListShowCurrent (pgm->switch_dlg_band_list);
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_band_list, arglist, i);
    XawListChange (pgm->switch_dlg_band_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_band_list, False);
    mnuStrFree (list, sz);
    pgm->bands_loaded = 0;
  }

  if (pgm->pos_idx != XAW_LIST_NONE)
  {

    sp = pgm->sp + pgm->pos_idx;
    nb = sp->num_bands;

    if (0 == nb)
      return;

    list = calloc (sizeof (char *), nb);
    for (i = 0; i < (Cardinal)nb; i++)
    {
      list[i] = calloc (30, sizeof (char));
      sprintf (list[i], "%u", sp->bands[i].lof);
    }
    XawListChange (pgm->switch_dlg_band_list, list, nb, 0, True);
    if (nb)
      XtSetSensitive (pgm->switch_dlg_band_list, True);

    pgm->bands_loaded = 1;
    pgm->band_idx = XAW_LIST_NONE;
    if ((NULL != cur) && (cur->list_index != XAW_LIST_NONE))
    {
      pgm->band_idx = cur->list_index;
      XawListHighlight (pgm->switch_dlg_band_list, cur->list_index);
      free (cur);
    }
  }
}

static void
switch_dlg_pol_alter (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  char **list;
  uint32_t sz;
  int np;
  SwitchPos *sp;
  SwitchBand *sb;
  XawListReturnStruct *cur = NULL;

  if (0 != pgm->pols_loaded)
  {
    pgm->pol_idx = XAW_LIST_NONE;
    cur = XawListShowCurrent (pgm->switch_dlg_pol_list);
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_pol_list, arglist, i);
    XawListChange (pgm->switch_dlg_pol_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_pol_list, False);
    mnuStrFree (list, sz);
    pgm->pols_loaded = 0;
  }

  if (pgm->band_idx != XAW_LIST_NONE)
  {

    sp = pgm->sp + pgm->pos_idx;
    sb = sp->bands + pgm->band_idx;
    np = sb->num_pol;


    if (0 == np)
      return;

    list = calloc (sizeof (char *), np);
    for (i = 0; i < (Cardinal)np; i++)
    {
      list[i] = calloc (5, sizeof (char));
      sprintf (list[i], "%s", tpiGetPolStr (sb->pol[i].pol));
    }
    XawListChange (pgm->switch_dlg_pol_list, list, np, 0, True);
    if (np)
      XtSetSensitive (pgm->switch_dlg_pol_list, True);

    pgm->pols_loaded = 1;
    pgm->pol_idx = XAW_LIST_NONE;
    if ((NULL != cur) && (cur->list_index != XAW_LIST_NONE))
    {
      pgm->pol_idx = cur->list_index;
      XawListHighlight (pgm->switch_dlg_pol_list, cur->list_index);
      free (cur);
    }
  }
}

static void
switch_dlg_cmds_alter (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  char **list;
  uint32_t sz;
  int nc;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  XawListReturnStruct *cur = NULL;

  if (0 != pgm->cmds_loaded)
  {
    pgm->cmd_idx = XAW_LIST_NONE;
    cur = XawListShowCurrent (pgm->switch_dlg_cmd_list);
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_cmd_list, arglist, i);
    XawListChange (pgm->switch_dlg_cmd_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_cmd_list, False);
    free (list);
    pgm->cmds_loaded = 0;
//              switch_dlg_purge_pols(pgm);
  }

  if (pgm->pol_idx != XAW_LIST_NONE)
  {

    sp = pgm->sp + pgm->pos_idx;
    sb = sp->bands + pgm->band_idx;
    sl = sb->pol + pgm->pol_idx;
    nc = sl->num_cmds;


    if (0 == nc)
      return;

    list = calloc (sizeof (char *), nc);
    for (i = 0; i < (Cardinal)nc; i++)
    {
      list[i] = (char *) spGetCmdStr (sl->cmds[i].what);
    }
    XawListChange (pgm->switch_dlg_cmd_list, list, nc, 0, True);
    if (nc)
      XtSetSensitive (pgm->switch_dlg_cmd_list, True);

    pgm->cmds_loaded = 1;
    pgm->cmd_idx = XAW_LIST_NONE;
    if ((NULL != cur) && (cur->list_index != XAW_LIST_NONE))
    {
      pgm->cmd_idx = cur->list_index;
      XawListHighlight (pgm->switch_dlg_cmd_list, cur->list_index);
      free (cur);
    }
  }
}

static void
switch_dlg_dtl_alter (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  char **list;
  uint32_t sz;
  int nd;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;
  XawListReturnStruct *cur = NULL;

  if (0 != pgm->dtls_loaded)
  {
    pgm->dtl_idx = XAW_LIST_NONE;
    cur = XawListShowCurrent (pgm->switch_dlg_cmd_detail);
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_cmd_detail, arglist, i);
    XawListChange (pgm->switch_dlg_cmd_detail, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_cmd_detail, False);
    mnuStrFree (list, sz);
    pgm->dtls_loaded = 0;
//              switch_dlg_purge_pols(pgm);
  }

  if (pgm->cmd_idx != XAW_LIST_NONE)
  {
    sp = pgm->sp + pgm->pos_idx;
    sb = sp->bands + pgm->band_idx;
    sl = sb->pol + pgm->pol_idx;
    sc = sl->cmds + pgm->cmd_idx;
    nd = sc->len;

    if (0 == nd)
      return;

    list = calloc (sizeof (char *), nd);
    for (i = 0; i < (Cardinal)nd; i++)
    {
      list[i] = calloc (sizeof (char), 5);
      sprintf (list[i], "0x%2.2x", sc->data[i]);
    }
    XawListChange (pgm->switch_dlg_cmd_detail, list, nd, 0, True);
    if (nd)
      XtSetSensitive (pgm->switch_dlg_cmd_detail, True);

    pgm->dtls_loaded = 1;
    pgm->dtl_idx = XAW_LIST_NONE;
    if ((NULL != cur) && (cur->list_index != XAW_LIST_NONE))
    {
      pgm->dtl_idx = cur->list_index;
      XawListHighlight (pgm->switch_dlg_cmd_detail, cur->list_index);
      free (cur);
    }
  }
}

static void
switch_dlg_load_byte (PgmState * pgm)
{
  Arg arglist[20];
  char buf[20];
  Cardinal i = 0;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;
  uint8_t cmd_byte;
  if (pgm->byte_loaded)
    return;
  if (pgm->dtl_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  sc = sl->cmds + pgm->cmd_idx;
  cmd_byte = sc->data[pgm->dtl_idx];

  sprintf (buf, "0x%2.2" PRIx8, cmd_byte);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buf);
  i++;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetValues (pgm->switch_dlg_byte, arglist, i);

  pgm->byte_loaded = 1;
}

static void
switch_dlg_purge_byte (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;

  if (0 != pgm->byte_loaded)
  {
    i = 0;
    XtSetArg (arglist[i], XtNstring, NULL);
    i++;
    XtSetArg (arglist[i], XtNsensitive, False);
    i++;
    XtSetValues (pgm->switch_dlg_byte, arglist, i);
    pgm->byte_loaded = 0;
  }
}

static void
switch_dlg_load_dtl (PgmState * pgm)
{
  char **list;
  int i, nd;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;

  if (pgm->dtls_loaded)
    return;
  if (pgm->cmd_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  sc = sl->cmds + pgm->cmd_idx;
  nd = sc->len;

  XtSetSensitive (pgm->switch_dlg_cmd_type, True);
  XawListHighlight (pgm->switch_dlg_cmd_type, sc->what);

  if (0 == nd)
    return;
  if (nd > 8)
    nd = 8;

  list = calloc (sizeof (char *), nd);
  for (i = 0; i < nd; i++)
  {
    list[i] = calloc (sizeof (char), 5);
    sprintf (list[i], "0x%2.2x", sc->data[i]);
  }
  XawListChange (pgm->switch_dlg_cmd_detail, list, nd, 0, True);
  if (nd)
    XtSetSensitive (pgm->switch_dlg_cmd_detail, True);

  pgm->dtls_loaded = 1;
  pgm->dtl_idx = XAW_LIST_NONE;
}

static void
switch_dlg_purge_dtl (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (0 != pgm->dtls_loaded)
  {
    pgm->dtl_idx = XAW_LIST_NONE;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_cmd_detail, arglist, i);
    XawListChange (pgm->switch_dlg_cmd_detail, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_cmd_detail, False);
    XtSetSensitive (pgm->switch_dlg_cmd_type, False);
    mnuStrFree (list, sz);
    switch_dlg_purge_byte (pgm);
    pgm->dtls_loaded = 0;
  }
}

static void
switch_dlg_load_cmds (PgmState * pgm)
{
  char **list;
  int i, nc;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;

  if (pgm->cmds_loaded)
    return;
  if (pgm->pol_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  nc = sl->num_cmds;

  XtSetSensitive (pgm->switch_dlg_pol_sel, True);
  XawListHighlight (pgm->switch_dlg_pol_sel, sl->pol);

  if (0 == nc)
    return;

  list = calloc (sizeof (char *), nc);
  for (i = 0; i < nc; i++)
  {
    list[i] = (char *) spGetCmdStr (sl->cmds[i].what);
  }
  XawListChange (pgm->switch_dlg_cmd_list, list, nc, 0, True);
  if (nc)
    XtSetSensitive (pgm->switch_dlg_cmd_list, True);
  pgm->cmds_loaded = 1;
  pgm->cmd_idx = XAW_LIST_NONE;
}

static void
switch_dlg_purge_cmds (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (0 != pgm->cmds_loaded)
  {
    pgm->cmd_idx = XAW_LIST_NONE;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_cmd_list, arglist, i);
    XawListChange (pgm->switch_dlg_cmd_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_cmd_list, False);
    XtSetSensitive (pgm->switch_dlg_pol_sel, False);
    free (list);                //the strings are constant, we don't own them
    pgm->cmds_loaded = 0;
    switch_dlg_purge_dtl (pgm);
  }
}

static void
switch_dlg_load_pols (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  char buf[20];
  char **list;
  int np;
  SwitchPos *sp;
  SwitchBand *sb;

  if (pgm->pols_loaded)
    return;
  if (pgm->band_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  np = sb->num_pol;

  sprintf (buf, "%u", sb->lof);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buf);
  i++;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetValues (pgm->switch_dlg_lof, arglist, i);
  sprintf (buf, "%u", sb->f_max);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buf);
  i++;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetValues (pgm->switch_dlg_uf, arglist, i);
  sprintf (buf, "%u", sb->f_min);
  i = 0;
  XtSetArg (arglist[i], XtNstring, buf);
  i++;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetValues (pgm->switch_dlg_lf, arglist, i);

  if (0 == np)
    return;
  list = calloc (sizeof (char *), np);
  for (i = 0; i < (Cardinal)np; i++)
  {
    list[i] = calloc (5, sizeof (char));
    sprintf (list[i], "%s", tpiGetPolStr (sb->pol[i].pol));
  }
  XawListChange (pgm->switch_dlg_pol_list, list, np, 0, True);
  if (np)
    XtSetSensitive (pgm->switch_dlg_pol_list, True);
  pgm->pols_loaded = 1;
  pgm->pol_idx = XAW_LIST_NONE;


}

static void
switch_dlg_purge_pols (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (0 != pgm->pols_loaded)
  {
    pgm->pol_idx = XAW_LIST_NONE;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_pol_list, arglist, i);
    XawListChange (pgm->switch_dlg_pol_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_pol_list, False);
    mnuStrFree (list, sz);
    XtSetArg (arglist[i], XtNstring, NULL);
    i++;
    XtSetArg (arglist[i], XtNsensitive, False);
    i++;
    XtSetValues (pgm->switch_dlg_lof, arglist, i);
    XtSetArg (arglist[i], XtNstring, NULL);
    i++;
    XtSetArg (arglist[i], XtNsensitive, False);
    i++;
    XtSetValues (pgm->switch_dlg_uf, arglist, i);
    XtSetArg (arglist[i], XtNstring, NULL);
    i++;
    XtSetArg (arglist[i], XtNsensitive, False);
    i++;
    XtSetValues (pgm->switch_dlg_lf, arglist, i);

    pgm->pols_loaded = 0;
    switch_dlg_purge_cmds (pgm);
  }
}



static void
switch_dlg_load_bands (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  char **list;
  int nb;
  SwitchPos *sp;
  if (pgm->bands_loaded)
    return;
  if (pgm->pos_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  nb = sp->num_bands;
  i = 0;
  XtSetArg (arglist[i], XtNstring, sp->initial_tuning_file);
  i++;
  XtSetArg (arglist[i], XtNsensitive, True);
  i++;
  XtSetValues (pgm->itf_txt, arglist, i);

  if (0 == nb)
    return;

  list = calloc (sizeof (char *), nb);
  for (i = 0; i < (Cardinal)nb; i++)
  {
    list[i] = calloc (30, sizeof (char));
    sprintf (list[i], "%u", sp->bands[i].lof);
  }
  XawListChange (pgm->switch_dlg_band_list, list, nb, 0, True);
  if (nb)
    XtSetSensitive (pgm->switch_dlg_band_list, True);

  pgm->bands_loaded = 1;
  pgm->band_idx = XAW_LIST_NONE;
}

static void
switch_dlg_purge_bands (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (0 != pgm->bands_loaded)
  {
    pgm->band_idx = XAW_LIST_NONE;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_band_list, arglist, i);
    XawListChange (pgm->switch_dlg_band_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_band_list, False);
    mnuStrFree (list, sz);
    i = 0;
    XtSetArg (arglist[i], XtNstring, NULL);
    i++;
    XtSetArg (arglist[i], XtNsensitive, False);
    i++;
    XtSetValues (pgm->itf_txt, arglist, i);

    pgm->bands_loaded = 0;
    switch_dlg_purge_pols (pgm);
  }
}

static void
switch_dlg_purge_pos (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int sz;
  char **list;

  if (0 != pgm->pos_loaded)
  {
    pgm->pos_loaded = 0;
    pgm->pos_idx = XAW_LIST_NONE;
    i = 0;
    XtSetArg (arglist[i], XtNlist, &list);
    i++;
    XtSetArg (arglist[i], XtNnumberStrings, &sz);
    i++;
    XtGetValues (pgm->switch_dlg_pos_list, arglist, i);
    XawListChange (pgm->switch_dlg_pos_list, NULL, 0, 0, True);
    XtSetSensitive (pgm->switch_dlg_pos_list, False);
    mnuStrFree (list, sz);
    switch_dlg_purge_bands (pgm);
  }
}

static void
switch_dlg_load_pos (PgmState * pgm)
{
  char **list;
  Cardinal i;
  if ((NULL == pgm->sp) || (0 == pgm->num_pos) || (1 == pgm->pos_loaded))
    return;
  list = calloc (sizeof (char *), pgm->num_pos);
  for (i = 0; i < pgm->num_pos; i++)
  {
    list[i] = calloc (30, sizeof (char));
    sprintf (list[i], "Pos: %u", i);
  }
  XawListChange (pgm->switch_dlg_pos_list, list, pgm->num_pos, 0, True);
  if (pgm->num_pos)
    XtSetSensitive (pgm->switch_dlg_pos_list, True);
  pgm->pos_loaded = 1;

}

void
switchDlgDisplay (PgmState * pgm)
{
  pgm->num_pos = 0;
  pgm->pos_loaded = 0;
  pgm->sp = NULL;
  pgm->bands_loaded = 0;
  pgm->band_idx = XAW_LIST_NONE;
  pgm->pos_idx = XAW_LIST_NONE;
  pgm->pols_loaded = 0;
  pgm->pol_idx = XAW_LIST_NONE;
  pgm->cmds_loaded = 0;
  pgm->cmd_idx = XAW_LIST_NONE;
  pgm->dtls_loaded = 0;
  pgm->dtl_idx = XAW_LIST_NONE;
  pgm->byte_loaded = 0;
  pgm->preset_idx = XAW_LIST_NONE;
  if (0 == clientGetSwitch (pgm->sock, &pgm->sp, &pgm->num_pos))
    switch_dlg_load_pos (pgm);
}

static void
switch_dlg_back (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  switch_dlg_purge_pos (pgm);
  XtUnmapWidget (pgm->switch_form);
  XtMapWidget (pgm->setup_form);
}

static void
switch_dlg_preset (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  pgm->preset_idx = str->list_index;
}

static void
switch_dlg_use_preset (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  Cardinal i;
  uint32_t num;
  PgmState *pgm = client_data;
  if (XAW_LIST_NONE == pgm->preset_idx)
    return;
  if (NULL != pgm->sp)
    for (i = 0; i < pgm->num_pos; i++)
    {
      spClear (pgm->sp + i);
    }
  pgm->sp = spGetPreset (pgm->preset_idx, &num);
  pgm->num_pos = num;
  switch_dlg_purge_pos (pgm);
  switch_dlg_load_pos (pgm);
}

static void
switch_dlg_pos (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;

  switch_dlg_purge_bands (pgm);
  pgm->pos_idx = str->list_index;
  switch_dlg_load_bands (pgm);
}

static void
switch_dlg_band (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;

  switch_dlg_purge_pols (pgm);
  pgm->band_idx = str->list_index;
  switch_dlg_load_pols (pgm);
}

static void
switch_dlg_pol (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;

  switch_dlg_purge_cmds (pgm);
  pgm->pol_idx = str->list_index;
  switch_dlg_load_cmds (pgm);
}

static void
switch_dlg_cmd (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;

  switch_dlg_purge_dtl (pgm);
  pgm->cmd_idx = str->list_index;
  switch_dlg_load_dtl (pgm);
}

static void
switch_dlg_itf (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  String itf;
  SwitchPos *sp;
  if (pgm->pos_idx == XAW_LIST_NONE)
    return;
  XtSetArg (arglist[i], XtNstring, &itf);
  i++;
  XtGetValues (w, arglist, i);

  sp = pgm->sp + pgm->pos_idx;
  free (sp->initial_tuning_file);
  sp->initial_tuning_file = NULL;
  if (NULL != itf)
    sp->initial_tuning_file = strdup (itf);
}

static void
switch_dlg_lof (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  String itf;
  SwitchPos *sp;
  SwitchBand *sb;
  if (pgm->band_idx == XAW_LIST_NONE)
    return;
  XtSetArg (arglist[i], XtNstring, &itf);
  i++;
  XtGetValues (w, arglist, i);
  if (NULL == itf)
    return;
  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sb->lof = atoi (itf);
  switch_dlg_band_alter (pgm);
}

static void
switch_dlg_uf (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  String itf;
  SwitchPos *sp;
  SwitchBand *sb;
  if (pgm->band_idx == XAW_LIST_NONE)
    return;
  XtSetArg (arglist[i], XtNstring, &itf);
  i++;
  XtGetValues (w, arglist, i);
  if (NULL == itf)
    return;
  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sb->f_max = atoi (itf);
}

static void
switch_dlg_lf (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  String itf;
  SwitchPos *sp;
  SwitchBand *sb;
  if (pgm->band_idx == XAW_LIST_NONE)
    return;
  XtSetArg (arglist[i], XtNstring, &itf);
  i++;
  XtGetValues (w, arglist, i);
  if (NULL == itf)
    return;
  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sb->f_min = atoi (itf);
}

static void
switch_dlg_byte (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  String itf;
//      unsigned int d;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;
  if (pgm->cmd_idx == XAW_LIST_NONE)
    return;
  XtSetArg (arglist[i], XtNstring, &itf);
  i++;
  XtGetValues (w, arglist, i);
  if (NULL == itf)
    return;
  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  sc = sl->cmds + pgm->cmd_idx;
  sscanf (itf, "0x%2hhx", &sc->data[pgm->dtl_idx]);
//      sc->data[pgm->dtl_idx]=d&0xff;
//      sc->data[pgm->dtl_idx]=atoi(itf)&0xff;
  switch_dlg_dtl_alter (pgm);
}

static void
switch_dlg_cmd_tp (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
//      unsigned int d;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;
  XawListReturnStruct *str = call_data;

  if (str->list_index == XAW_LIST_NONE)
    return;
  if (pgm->cmd_idx == XAW_LIST_NONE)
    return;
  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  sc = sl->cmds + pgm->cmd_idx;
  sc->what = str->list_index;
  switch_dlg_cmds_alter (pgm);
}

static void
switch_dlg_cmd_dtl (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  XawListReturnStruct *str = call_data;
  switch_dlg_purge_byte (pgm);
  pgm->dtl_idx = str->list_index;
  switch_dlg_load_byte (pgm);
}

static void
switch_dlg_pol_set (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
//      unsigned int d;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  XawListReturnStruct *str = call_data;

  if (str->list_index == XAW_LIST_NONE)
    return;
  if (pgm->pol_idx == XAW_LIST_NONE)
    return;
  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  sl->pol = str->list_index;
  switch_dlg_pol_alter (pgm);
}

static void
switch_dlg_pos_add (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;

  sp = realloc (pgm->sp, sizeof (SwitchPos) * (pgm->num_pos + 1));
  pgm->sp = sp;
  if (NULL == sp)
  {
    pgm->num_pos = 0;
    switch_dlg_purge_pos (pgm);
    switch_dlg_load_pos (pgm);
    return;
  }
  memset (sp + pgm->num_pos, 0, sizeof (SwitchPos));
  pgm->num_pos++;
  switch_dlg_purge_pos (pgm);
  switch_dlg_load_pos (pgm);
}

static void
switch_dlg_pos_rmv (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  if (0 == pgm->num_pos)
    return;

  sp = realloc (pgm->sp, sizeof (SwitchPos) * (pgm->num_pos - 1));
  pgm->sp = sp;
  if (NULL == sp)
  {
    pgm->num_pos = 0;
    switch_dlg_purge_pos (pgm);
    switch_dlg_load_pos (pgm);
    return;
  }
  pgm->num_pos--;
  switch_dlg_purge_pos (pgm);
  switch_dlg_load_pos (pgm);
}

static void
switch_dlg_bnd_add (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;

  if (pgm->pos_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;

  sb = realloc (sp->bands, sizeof (SwitchBand) * (sp->num_bands + 1));
  sp->bands = sb;
  if (NULL == sb)
  {
    sp->num_bands = 0;
    switch_dlg_purge_bands (pgm);
    switch_dlg_load_bands (pgm);
    return;
  }
  memset (sb + sp->num_bands, 0, sizeof (SwitchBand));
  sp->num_bands++;
  switch_dlg_purge_bands (pgm);
  switch_dlg_load_bands (pgm);
}

static void
switch_dlg_bnd_rmv (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;

  if (pgm->pos_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  if (0 == sp->num_bands)
    return;
  sb = realloc (sp->bands, sizeof (SwitchBand) * (sp->num_bands - 1));
  sp->bands = sb;
  if (NULL == sb)
  {
    sp->num_bands = 0;
    switch_dlg_purge_bands (pgm);
    switch_dlg_load_bands (pgm);
    return;
  }
  sp->num_bands--;
  switch_dlg_purge_bands (pgm);
  switch_dlg_load_bands (pgm);
}

static void
switch_dlg_pol_add (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;

  if (pgm->band_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;

  sl = realloc (sb->pol, sizeof (SwitchPol) * (sb->num_pol + 1));
  sb->pol = sl;
  if (NULL == sl)
  {
    sb->num_pol = 0;
    switch_dlg_purge_pols (pgm);
    switch_dlg_load_pols (pgm);
    return;
  }
  memset (sl + sb->num_pol, 0, sizeof (SwitchPol));
  sb->num_pol++;
  switch_dlg_purge_pols (pgm);
  switch_dlg_load_pols (pgm);
}

static void
switch_dlg_pol_rmv (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;

  if (pgm->band_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  if (0 == sb->num_pol)
    return;

  sl = realloc (sb->pol, sizeof (SwitchPol) * (sb->num_pol - 1));
  sb->pol = sl;
  if (NULL == sl)
  {
    sb->num_pol = 0;
    switch_dlg_purge_pols (pgm);
    switch_dlg_load_pols (pgm);
    return;
  }
  sb->num_pol--;
  switch_dlg_purge_pols (pgm);
  switch_dlg_load_pols (pgm);

}

static void
switch_dlg_cmd_add (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;

  if (pgm->pol_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;

  sc = realloc (sl->cmds, sizeof (SwitchCmd) * (sl->num_cmds + 1));
  sl->cmds = sc;
  if (NULL == sc)
  {
    sl->num_cmds = 0;
    switch_dlg_purge_cmds (pgm);
    switch_dlg_load_cmds (pgm);
    return;
  }
  memset (sc + sl->num_cmds, 0, sizeof (SwitchCmd));
  sl->num_cmds++;
  switch_dlg_purge_cmds (pgm);
  switch_dlg_load_cmds (pgm);
}

static void
switch_dlg_cmd_rmv (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;

  if (pgm->pol_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  if (0 == sl->num_cmds)
    return;
  sc = realloc (sl->cmds, sizeof (SwitchCmd) * (sl->num_cmds - 1));
  sl->cmds = sc;
  if (NULL == sc)
  {
    sl->num_cmds = 0;
    switch_dlg_purge_cmds (pgm);
    switch_dlg_load_cmds (pgm);
    return;
  }
  sl->num_cmds--;
  switch_dlg_purge_cmds (pgm);
  switch_dlg_load_cmds (pgm);
}

static void
switch_dlg_byte_add (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;
  if (pgm->cmd_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  sc = sl->cmds + pgm->cmd_idx;

  if (sc->len > 7)
    return;
  sc->len++;                    //constant sized array
  switch_dlg_purge_dtl (pgm);
  switch_dlg_load_dtl (pgm);
}

static void
switch_dlg_byte_rmv (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  SwitchPos *sp;
  SwitchBand *sb;
  SwitchPol *sl;
  SwitchCmd *sc;
  if (pgm->cmd_idx == XAW_LIST_NONE)
    return;

  sp = pgm->sp + pgm->pos_idx;
  sb = sp->bands + pgm->band_idx;
  sl = sb->pol + pgm->pol_idx;
  sc = sl->cmds + pgm->cmd_idx;

  if (sc->len == 0)
    return;
  sc->len--;                    //constant sized array
  switch_dlg_purge_dtl (pgm);
  switch_dlg_load_dtl (pgm);
}

static void
switch_dlg_store (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  if (pgm->sp && pgm->num_pos)
    clientSetSwitch (pgm->sock, pgm->sp, pgm->num_pos);
}


void
switchDlgInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int j;
//      Widget pgm_list;
  String *list;
  Widget back, pos_vp /*,store,use_preset */ , band_vp, pol_vp, cmd_vp, cmd_dtl_vp, pos_add, /*pos_rmv, */ bnd_add, /*bnd_rmv, */ pol_add, pol_rmv, cmd_add, /*cmd_rmv, */ byte_add;    //,byte_rmv;
  XtCallbackRec cl[2];
  cl[1].callback = NULL;
  cl[1].closure = NULL;

  cl[0].callback = switch_dlg_back;
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

  cl[0].callback = switch_dlg_store;
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

  cl[0].callback = switch_dlg_preset;
  cl[0].closure = pgm;
/*	i=0;
  XtSetArg(arglist[i],XtNleft,XawChainLeft);i++;
  XtSetArg(arglist[i],XtNright,XawChainLeft);i++;
  XtSetArg(arglist[i],XtNtop,XawChainTop);i++;
  XtSetArg(arglist[i],XtNbottom,XawChainBottom);i++;
  XtSetArg(arglist[i],XtNheight,170);i++;
  XtSetArg(arglist[i],XtNwidth,80);i++;
  XtSetArg(arglist[i],XtNallowVert,True);i++;
  XtSetArg(arglist[i],XtNfromVert,back);i++;
	preset_vp=XtCreateManagedWidget("PresetVP",viewportWidgetClass,prnt,arglist,i);
*/
  list = calloc (sizeof (String), SP_NUM_PRESETS);
  for (j = 0; j < SP_NUM_PRESETS; j++)
  {
    list[j] = (String) spGetPresetStrings (j);
  }
  i = 0;
  XtSetArg (arglist[i], XtNheight, 170);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNlist, list);
  i++;
  XtSetArg (arglist[i], XtNnumberStrings, SP_NUM_PRESETS);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->switch_dlg_preset_list =
    XtCreateManagedWidget ("Presets", listWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_use_preset;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->switch_dlg_preset_list);
  i++;
  /*use_preset= */ XtCreateManagedWidget ("Use Preset", commandWidgetClass,
                                          prnt, arglist, i);


  cl[0].callback = switch_dlg_pos_add;
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->switch_dlg_preset_list);
  i++;
  pos_add = XtCreateManagedWidget ("+", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_pos_rmv;
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
  XtSetArg (arglist[i], XtNfromHoriz, pos_add);
  i++;
  /*pos_rmv= */ XtCreateManagedWidget ("-", commandWidgetClass, prnt, arglist,
                                       i);

  cl[0].callback = switch_dlg_pos;
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
  XtSetArg (arglist[i], XtNwidth, 70);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->switch_dlg_preset_list);
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

  pgm->switch_dlg_pos_list =
    XtCreateManagedWidget ("Position", listWidgetClass, pos_vp, arglist, i);


  cl[0].callback = switch_dlg_itf;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pgm->switch_dlg_preset_list);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pos_vp);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNwidth, 70);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->itf_txt =
    XtCreateManagedWidget ("ItfTxt", asciiTextWidgetClass, prnt, arglist, i);
/*
	cl[0].callback=switch_dlg_set_itf;
	cl[0].closure=pgm;
	i=0;
  XtSetArg(arglist[i],XtNleft,XawChainLeft);i++;
  XtSetArg(arglist[i],XtNright,XawChainLeft);i++;
  XtSetArg(arglist[i],XtNtop,XawChainBottom);i++;
  XtSetArg(arglist[i],XtNbottom,XawChainBottom);i++;
  XtSetArg(arglist[i],XtNcallback,cl);i++;
  XtSetArg(arglist[i],XtNfromVert,pos_vp);i++;
  XtSetArg(arglist[i],XtNfromHoriz,pgm->itf_txt);i++;
	set_itf=XtCreateManagedWidget("SetItf",commandWidgetClass,prnt,arglist,i);
*/

  cl[0].callback = switch_dlg_bnd_add;
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
  XtSetArg (arglist[i], XtNfromHoriz, pos_vp);
  i++;
  bnd_add = XtCreateManagedWidget ("+", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_bnd_rmv;
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
  XtSetArg (arglist[i], XtNfromHoriz, bnd_add);
  i++;
  /*bnd_rmv= */ XtCreateManagedWidget ("-", commandWidgetClass, prnt, arglist,
                                       i);

  cl[0].callback = switch_dlg_band;
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
  XtSetArg (arglist[i], XtNheight, 130);
  i++;
  XtSetArg (arglist[i], XtNwidth, 90);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pos_vp);
  i++;
  band_vp =
    XtCreateManagedWidget ("BandVP", viewportWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->switch_dlg_band_list =
    XtCreateManagedWidget ("Band", listWidgetClass, band_vp, arglist, i);

  cl[0].callback = switch_dlg_lof;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNwidth, 90);
  i++;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pos_vp);
  i++;
  XtSetArg (arglist[i], XtNfromVert, band_vp);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->switch_dlg_lof =
    XtCreateManagedWidget ("LofTxt", asciiTextWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_uf;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNwidth, 90);
  i++;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pos_vp);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->switch_dlg_lof);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->switch_dlg_uf =
    XtCreateManagedWidget ("UFTxt", asciiTextWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_lf;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNwidth, 90);
  i++;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pos_vp);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->switch_dlg_uf);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->switch_dlg_lf =
    XtCreateManagedWidget ("LFTxt", asciiTextWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_pol_add;
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
  XtSetArg (arglist[i], XtNfromHoriz, band_vp);
  i++;
  pol_add = XtCreateManagedWidget ("+", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_pol_rmv;
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
  XtSetArg (arglist[i], XtNfromHoriz, pol_add);
  i++;
  pol_rmv = XtCreateManagedWidget ("-", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_pol;
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
  XtSetArg (arglist[i], XtNheight, 80);
  i++;
  XtSetArg (arglist[i], XtNwidth, 40);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, band_vp);
  i++;
  pol_vp =
    XtCreateManagedWidget ("PolVP", viewportWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->switch_dlg_pol_list =
    XtCreateManagedWidget ("Pol", listWidgetClass, pol_vp, arglist, i);

  cl[0].callback = switch_dlg_pol_set;
  cl[0].closure = pgm;
  list = calloc (sizeof (String), 4);
  for (j = 0; j < 4; j++)
  {
    list[j] = (String) tpiGetPolStr (j);
  }
  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNlist, list);
  i++;
  XtSetArg (arglist[i], XtNnumberStrings, 4);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNheight, 80);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pol_vp);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, band_vp);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  pgm->switch_dlg_pol_sel =
    XtCreateManagedWidget ("PolSel", listWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_cmd_add;
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
  XtSetArg (arglist[i], XtNfromHoriz, pol_rmv);
  i++;
  cmd_add = XtCreateManagedWidget ("+", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_cmd_rmv;
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
  XtSetArg (arglist[i], XtNfromHoriz, cmd_add);
  i++;
  /*cmd_rmv= */ XtCreateManagedWidget ("-", commandWidgetClass, prnt, arglist,
                                       i);

  cl[0].callback = switch_dlg_cmd;
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
  XtSetArg (arglist[i], XtNheight, 80);
  i++;
  XtSetArg (arglist[i], XtNwidth, 90);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pol_vp);
  i++;
  cmd_vp =
    XtCreateManagedWidget ("CmdVP", viewportWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->switch_dlg_cmd_list =
    XtCreateManagedWidget ("Commands", listWidgetClass, cmd_vp, arglist, i);

  cl[0].callback = switch_dlg_cmd_tp;
  cl[0].closure = pgm;
  list = calloc (sizeof (String), SP_NUM_CMD);
  for (j = 0; j < SP_NUM_CMD; j++)
  {
    list[j] = (String) spGetCmdStr (j);
  }
  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNlist, list);
  i++;
  XtSetArg (arglist[i], XtNnumberStrings, SP_NUM_CMD);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNheight, 120);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, cmd_vp);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pol_vp);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->switch_dlg_cmd_type =
    XtCreateManagedWidget ("CommandType", listWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_byte_add;
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
  XtSetArg (arglist[i], XtNfromHoriz, cmd_vp);
  i++;
  byte_add =
    XtCreateManagedWidget ("+", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = switch_dlg_byte_rmv;
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
  XtSetArg (arglist[i], XtNfromHoriz, byte_add);
  i++;
  /*byte_rmv= */ XtCreateManagedWidget ("-", commandWidgetClass, prnt,
                                        arglist, i);

  cl[0].callback = switch_dlg_cmd_dtl;
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
  XtSetArg (arglist[i], XtNwidth, 50);
  i++;
  XtSetArg (arglist[i], XtNallowVert, True);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, cmd_vp);
  i++;
  cmd_dtl_vp =
    XtCreateManagedWidget ("CmdDtlVP", viewportWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNdefaultColumns, 1);
  i++;
  XtSetArg (arglist[i], XtNforceColumns, True);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;

  pgm->switch_dlg_cmd_detail =
    XtCreateManagedWidget ("CommandDetail", listWidgetClass, cmd_dtl_vp,
                           arglist, i);

  cl[0].callback = switch_dlg_byte;
  cl[0].closure = pgm;
  i = 0;
  XtSetArg (arglist[i], XtNsensitive, False);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, cmd_vp);
  i++;
  XtSetArg (arglist[i], XtNfromVert, cmd_dtl_vp);
  i++;
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  XtSetArg (arglist[i], XtNwidth, 50);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->switch_dlg_byte =
    XtCreateManagedWidget ("ByteTxt", asciiTextWidgetClass, prnt, arglist, i);


}
