#include <inttypes.h>
#include"fe_ft.h"
#include"utl.h"
#include"client.h"
#include"debug.h"
#include"dmalloc.h"

static void
fe_ft_back (Widget w NOTUSED, XtPointer client_data,
            XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  XtUnmapWidget (pgm->fe_ft_form);
  XtMapWidget (pgm->setup_form);
  pgm->fe_vis = 0;
}

static void
fe_ft_set (Widget w NOTUSED, XtPointer client_data,
           XtPointer call_data NOTUSED)
{
  Arg arglist[20];
  Cardinal i = 0;
  PgmState *pgm = client_data;
  char *buf;
  int32_t val;
  i = 0;
  XtSetArg (arglist[i], XtNstring, &buf);
  i++;
  XtGetValues (pgm->fc_txt, arglist, i);
  val = atoi (buf);
  clientSetFcorr (pgm->sock, val);
}

static char *fe_ft_stat[] = {
  "SIGN ",
  "CARR ",
  "VITE ",
  "SYNC ",
  "LOCK ",
  "TMEO ",
  "REIN "
};

static void
fe_ft_sgnl (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  double *v_ret = call_data;
  char buffer[256];
  Arg arglist[20];
  Cardinal i = 0;
  int j;
  static char *empty = "     ";
  if (pgm->fe_vis)
  {
    if (clientGetStatus (pgm->sock, &pgm->fe_status,
                         &pgm->fe_ber, &pgm->fe_sig,
                         &pgm->fe_snr, &pgm->fe_ucb))
    {
      errMsg ("error getting frontend status\n");
    }
    else
    {
      printf ("0x%2.2hhx, %8.8x, %8.8x, %8.8x, %8.8x\n", pgm->fe_status,
              pgm->fe_ber, pgm->fe_sig, pgm->fe_snr, pgm->fe_ucb);

      strcpy (buffer, "Status: ");
      for (j = 0; j < 7; j++)
      {
        char *p;
        if (pgm->fe_status & (1 << j))
          p = struGetStr (fe_ft_stat, (size_t)j);
        else
          p = empty;
        strcat (buffer, p);
      }
      i = 0;
      XtSetArg (arglist[i], XtNlabel, buffer);
      i++;
      XtSetValues (pgm->fc_stat_l, arglist, i);
    }
  }
  *v_ret = (double) pgm->fe_sig / 16384.0;
}

static void
fe_ft_snr (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  double *v_ret = call_data;
  *v_ret = (double) pgm->fe_snr / 16384.0;
}

static void
fe_ft_ber (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  double *v_ret = call_data;
  *v_ret = (double) pgm->fe_ber / 16384.0;
//  puts ("asdf");
}

static void
fe_ft_ucb (Widget w NOTUSED, XtPointer client_data, XtPointer call_data)
{
  PgmState *pgm = client_data;
  double *v_ret = call_data;
  *v_ret = (double) pgm->fe_ucb / 16384.0;
}

void
feFtDisplay (PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  int32_t val;
  char buffer[20];
  pgm->fe_vis = 1;
  printf ("%" PRId32 "\n", val);
  if (clientGetFcorr (pgm->sock, &val))
    val = 0;
  printf ("%" PRId32 "\n", val);
  sprintf (buffer, "%" PRId32, val);

  i = 0;
  XtSetArg (arglist[i], XtNstring, buffer);
  i++;
  XtSetValues (pgm->fc_txt, arglist, i);

}

void
feFtInit (Widget prnt, PgmState * pgm)
{
  Arg arglist[20];
  Cardinal i = 0;
  Widget back /*,set */ , sig_l, sig, snr_l, snr, ber_l, ber,
    ucb_l /*,ucb */ ;
  XtCallbackRec cl[2];
  pgm->fe_vis = 0;
  pgm->fe_ber = 0;
  pgm->fe_sig = 0;
  pgm->fe_snr = 0;
  pgm->fe_ucb = 0;
  cl[1].callback = NULL;
  cl[1].closure = NULL;
  cl[0].callback = fe_ft_back;
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
  XtSetArg (arglist[i], XtNeditType, XawtextEdit);
  i++;
  XtSetArg (arglist[i], XtNtype, XawAsciiString);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->fc_txt =
    XtCreateManagedWidget ("fc", asciiTextWidgetClass, prnt, arglist, i);

  cl[0].callback = fe_ft_set;
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
  XtSetArg (arglist[i], XtNfromHoriz, pgm->fc_txt);
  i++;
  /*set= */ XtCreateManagedWidget ("Set", commandWidgetClass, prnt, arglist,
                                   i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, back);
  i++;
  pgm->fc_stat_l =
    XtCreateManagedWidget ("Status:                                    ",
                           labelWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->fc_stat_l);
  i++;
  sig_l =
    XtCreateManagedWidget ("Signal:", labelWidgetClass, prnt, arglist, i);

  cl[0].callback = fe_ft_sgnl;
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
  XtSetArg (arglist[i], XtNgetValue, cl);
  i++;
  XtSetArg (arglist[i], XtNfromVert, sig_l);
  i++;
  XtSetArg (arglist[i], XtNupdate, 1);
  i++;
  XtSetArg (arglist[i], XtNminScale, 4);
  i++;
  sig =
    XtCreateManagedWidget ("Signal", stripChartWidgetClass, prnt, arglist, i);


  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->fc_stat_l);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, sig);
  i++;
  snr_l = XtCreateManagedWidget ("SNR:", labelWidgetClass, prnt, arglist, i);

  cl[0].callback = fe_ft_snr;
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
  XtSetArg (arglist[i], XtNgetValue, cl);
  i++;
  XtSetArg (arglist[i], XtNfromVert, snr_l);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, sig);
  i++;
  XtSetArg (arglist[i], XtNupdate, 1);
  i++;
  XtSetArg (arglist[i], XtNminScale, 4);
  i++;
  snr =
    XtCreateManagedWidget ("SNR", stripChartWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->fc_stat_l);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, snr);
  i++;
  ber_l = XtCreateManagedWidget ("BER:", labelWidgetClass, prnt, arglist, i);

  cl[0].callback = fe_ft_ber;
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
  XtSetArg (arglist[i], XtNgetValue, cl);
  i++;
  XtSetArg (arglist[i], XtNfromVert, ber_l);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, snr);
  i++;
  XtSetArg (arglist[i], XtNupdate, 1);
  i++;
  XtSetArg (arglist[i], XtNminScale, 4);
  i++;
  ber =
    XtCreateManagedWidget ("BER", stripChartWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, pgm->fc_stat_l);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, ber);
  i++;
  ucb_l = XtCreateManagedWidget ("UCB:", labelWidgetClass, prnt, arglist, i);

  cl[0].callback = fe_ft_ucb;
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
  XtSetArg (arglist[i], XtNgetValue, cl);
  i++;
  XtSetArg (arglist[i], XtNfromVert, ucb_l);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, ber);
  i++;
  XtSetArg (arglist[i], XtNupdate, 1);
  i++;
  XtSetArg (arglist[i], XtNminScale, 4);
  i++;
  /*ucb= */ XtCreateManagedWidget ("UCB", stripChartWidgetClass, prnt,
                                   arglist, i);

}
