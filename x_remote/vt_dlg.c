#include <X11/Xcms.h>
#include"vt_dlg.h"
#include"client.h"

static int
vt_dlg_buffers_alloc (PgmState * pgm)
{
  int i;
  XGCValues gcv;
  XWindowAttributes w_attr;
  pgm->should_animate = 0;
  pgm->idx = 0;
  XGetWindowAttributes (pgm->s.dpy, pgm->s.wdw, &w_attr);
  pgm->gc = XCreateGC (pgm->s.dpy, pgm->s.wdw, 0, &gcv);
  for (i = 0; i < 12; i++)
  {
    pgm->buffers[i] =
      XCreatePixmap (pgm->s.dpy, pgm->s.wdw, 336, 260, w_attr.depth);
  }
  pgm->buf_valid = 1;
  return 0;
}

static int
vt_dlg_buffers_init (PgmState * pgm)
{
  int i;
  XGCValues gcv;
  uint8_t const *pal;
  GC drawgc2[32];
  XWindowAttributes w_attr;
  XcmsColor xc;                 //,cr,cg,cb,cw;
//      XcmsCCC ccc;
//      float m[3][3],mi[3][3],df,s[3];
  if (!pgm->buf_valid)          //clear them first
    return 1;
  pgm->should_animate = 0;
  pgm->idx = 0;
  XGetWindowAttributes (pgm->s.dpy, pgm->s.wdw, &w_attr);
/*
	gcv.foreground=XWhitePixelOfScreen(w_attr.screen);
	drawgc=XCreateGC(pgm->s.dpy, pgm->s.wdw,GCForeground,&gcv);
	gcv.foreground=XBlackPixelOfScreen(w_attr.screen);
	drawgc2=XCreateGC(pgm->s.dpy, pgm->s.wdw,GCForeground,&gcv);
*/
  pal = vtctxPal (&pgm->vtc);
  /*
     ccc=XcmsCCCOfColormap(pgm->s.dpy,w_attr.colormap);
     XcmsQueryRed(  ccc,XcmsRGBiFormat,&cr);
     XcmsQueryGreen(ccc,XcmsRGBiFormat,&cg);
     XcmsQueryBlue( ccc,XcmsRGBiFormat,&cb);
     XcmsQueryWhite(ccc,XcmsRGBiFormat,&cw);

     printf("RED  : R: %f G: %f B: %f \n",cr.spec.RGBi.red,cr.spec.RGBi.green,cr.spec.RGBi.blue);
     printf("GREEN: R: %f G: %f B: %f \n",cg.spec.RGBi.red,cg.spec.RGBi.green,cg.spec.RGBi.blue);
     printf("BLUE : R: %f G: %f B: %f \n",cb.spec.RGBi.red,cb.spec.RGBi.green,cb.spec.RGBi.blue);
     printf("WHITE: R: %f G: %f B: %f \n",cw.spec.RGBi.red,cw.spec.RGBi.green,cw.spec.RGBi.blue);
     Is color correction possible or necessary?
     ccc=XcmsCCCOfColormap(pgm->s.dpy,w_attr.colormap);
     XcmsQueryRed(ccc,XcmsCIEXYZFormat,&cr);
     XcmsQueryGreen(ccc,XcmsCIEXYZFormat,&cg);
     XcmsQueryBlue(ccc,XcmsCIEXYZFormat,&cb);
     XcmsQueryWhite(ccc,XcmsCIEXYZFormat,&cw);

     printf("RED  : X: %f Y: %f Z: %f \n",cr.spec.CIEXYZ.X,cr.spec.CIEXYZ.Y,cr.spec.CIEXYZ.Z);
     printf("GREEN: X: %f Y: %f Z: %f \n",cg.spec.CIEXYZ.X,cg.spec.CIEXYZ.Y,cg.spec.CIEXYZ.Z);
     printf("BLUE : X: %f Y: %f Z: %f \n",cb.spec.CIEXYZ.X,cb.spec.CIEXYZ.Y,cb.spec.CIEXYZ.Z);
     printf("WHITE: X: %f Y: %f Z: %f \n",cw.spec.CIEXYZ.X,cw.spec.CIEXYZ.Y,cw.spec.CIEXYZ.Z);



     m[0][0]=cr.spec.CIEXYZ.X/cr.spec.CIEXYZ.Y;
     m[0][1]=cg.spec.CIEXYZ.X/cg.spec.CIEXYZ.Y;
     m[0][2]=cb.spec.CIEXYZ.X/cb.spec.CIEXYZ.Y;
     m[1][0]=1.0;*cw.spec.RGBi.red;
     m[1][1]=1.0;*cw.spec.RGBi.green;
     m[1][2]=1.0;*cw.spec.RGBi.blue;

     m[2][0]=(1.0-cr.spec.CIEXYZ.X-cr.spec.CIEXYZ.Y)/cr.spec.CIEXYZ.Y;
     m[2][1]=(1.0-cg.spec.CIEXYZ.X-cg.spec.CIEXYZ.Y)/cg.spec.CIEXYZ.Y;
     m[2][2]=(1.0-cb.spec.CIEXYZ.X-cb.spec.CIEXYZ.Y)/cb.spec.CIEXYZ.Y;

     pmat(m);

     df=((-m[0][2]*m[1][1]+m[0][1]*m[1][2])*m[2][0]+
     ( m[0][2]*m[1][0]-m[0][0]*m[1][2])*m[2][1]+
     (-m[0][1]*m[1][0]+m[0][0]*m[1][1])*m[2][2]);
     mi[0][0]=(-m[1][2]*m[2][1]+m[1][1]*m[2][2])/df;
     mi[0][1]=( m[0][2]*m[2][1]-m[0][1]*m[2][2])/df;
     mi[0][2]=(-m[0][2]*m[1][1]+m[0][1]*m[1][2])/df;

     mi[1][0]=( m[1][2]*m[2][0]-m[1][0]*m[2][2])/df;
     mi[1][1]=(-m[0][2]*m[2][0]+m[0][0]*m[2][2])/df;
     mi[1][2]=( m[0][2]*m[1][0]-m[0][0]*m[1][2])/df;

     mi[2][0]=(-m[1][1]*m[2][0]+m[1][0]*m[2][1])/df;
     mi[2][1]=( m[0][1]*m[2][0]+m[0][0]*m[2][1])/df;
     mi[2][2]=(-m[0][1]*m[1][0]+m[0][0]*m[1][1])/df;

     s[0]=mi[0][0]*cw.spec.CIEXYZ.X+mi[0][1]*cw.spec.CIEXYZ.Y+mi[0][2]*cw.spec.CIEXYZ.Z;
     s[1]=mi[1][0]*cw.spec.CIEXYZ.X+mi[1][1]*cw.spec.CIEXYZ.Y+mi[1][2]*cw.spec.CIEXYZ.Z;
     s[2]=mi[2][0]*cw.spec.CIEXYZ.X+mi[2][1]*cw.spec.CIEXYZ.Y+mi[2][2]*cw.spec.CIEXYZ.Z;
     printf("s: %f %f %f\n",s[0],s[1],s[2]);
     m[0][0]*=s[0];
     m[0][1]*=s[1];
     m[0][2]*=s[2];
     m[1][0]*=s[0];
     m[1][1]*=s[1];
     m[1][2]*=s[2];
     m[2][0]*=s[0];
     m[2][1]*=s[1];
     m[2][2]*=s[2];

     pmat(m);
   */

  for (i = 0; i < 32; i++)
  {
//              float r,g,b;
    /*
       xc.format=XcmsCIEXYZFormat;
       xc.spec.CIEXYZ.X=(   0.49*pal[i*3+0]+   0.31*pal[i*3+1]+   0.20*pal[i*3+2])/(15.0);
       xc.spec.CIEXYZ.Y=(0.17697*pal[i*3+0]+0.81240*pal[i*3+1]+0.01063*pal[i*3+2])/(15.0);
       xc.spec.CIEXYZ.Z=(                      0.01*pal[i*3+1]+   0.99*pal[i*3+2])/(15.0);
     */

    xc.format = XcmsRGBiFormat;
    xc.spec.RGBi.red = pal[i * 3 + 0] / 15.0;
    xc.spec.RGBi.green = pal[i * 3 + 1] / 15.0;
    xc.spec.RGBi.blue = pal[i * 3 + 2] / 15.0;
    /*
       xc.format=XcmsCIEXYZFormat;
       r=pal[i*3+0]/15.0;
       g=pal[i*3+1]/15.0;
       b=pal[i*3+2]/15.0;
       xc.spec.CIEXYZ.X=m[0][0]*r+m[0][1]*g+m[0][2]*b;
       xc.spec.CIEXYZ.Y=m[1][0]*r+m[1][1]*g+m[1][2]*b;
       xc.spec.CIEXYZ.Z=m[2][0]*r+m[2][1]*g+m[2][2]*b;
       printf("X: %f Y: %f Z: %f \n",xc.spec.CIEXYZ.X,xc.spec.CIEXYZ.Y,xc.spec.CIEXYZ.Z);
     */
    XcmsAllocColor (pgm->s.dpy, w_attr.colormap, &xc, XcmsCIEXYZFormat);
    pgm->pixel_lut[i] = xc.pixel;
    gcv.foreground = xc.pixel;
    drawgc2[i] = XCreateGC (pgm->s.dpy, pgm->s.wdw, GCForeground, &gcv);
  }
  for (i = 0; i < 12; i++)
  {
    int j, k;
    uint8_t vtbuf[260][672];
//              pgm->buffers[i]=XCreatePixmap(pgm->s.dpy, pgm->s.wdw, 672, 520, w_attr.depth);
//              XFillRectangle(pgm->s.dpy, pgm->buffers[i], (i&1)?drawgc:drawgc2, 0, 0, 672, 520);
    vtctxRender (&pgm->vtc, i, 0, (uint8_t *) vtbuf);
/*
		for(j=0;j<260;j++)
		{
			for(k=0;k<672;k++)
			{
				XDrawPoint(pgm->s.dpy,pgm->buffers[i],drawgc2[vtbuf[j][k]&31],k,j*2);//possibly not quite the right aspect ratio (non-square pixels on real PAL systems)
				XDrawPoint(pgm->s.dpy,pgm->buffers[i],drawgc2[vtbuf[j][k]&31],k,j*2+1);
			}
		}
		*/
    for (j = 0; j < 260; j++)
    {
      for (k = 0; k < 336; k++)
      {
        XDrawPoint (pgm->s.dpy, pgm->buffers[i], drawgc2[vtbuf[j][k * 2] & 31], k, j);  //possibly not quite the right aspect ratio (non-square pixels on real PAL systems)
//                              XDrawPoint(pgm->s.dpy,pgm->buffers[i],drawgc2[vtbuf[j][k]&31],k,j*2+1);
      }
    }
  }
  for (i = 0; i < 32; i++)
    XFreeGC (pgm->s.dpy, drawgc2[i]);
  return 0;
}

static void
vt_dlg_buffers_destroy (PgmState * pgm)
{
  int i;
  XWindowAttributes w_attr;
  if (!pgm->buf_valid)          //none there
    return;
  pgm->should_animate = 0;
  XGetWindowAttributes (pgm->s.dpy, pgm->s.wdw, &w_attr);
//      XFreeColors(pgm->s.dpy,w_attr.colormap,pgm->pixel_lut,32,0);
  for (i = 0; i < 12; i++)
  {
    XFreePixmap (pgm->s.dpy, pgm->buffers[i]);
  }
  XFreeGC (pgm->s.dpy, pgm->gc);
  pgm->buf_valid = 0;
}

static uint8_t *
vt_dlg_get_pk (void *d, int x, int y, int dc)
{
  PgmState *p = d;
  uint8_t *rv = NULL;
  uint16_t num_sp;
  uint16_t num_mp;
  int magno = x;
  int i;
  VtPk *spackets =
    clientVtGetSvcPk (p->sock, p->pos, p->frequency, p->pol, p->pid,
                      p->svc_id, &num_sp);
  VtPk *mpackets =
    clientVtGetMagPk (p->sock, p->pos, p->frequency, p->pol, p->pid,
                      p->svc_id, magno, &num_mp);
  for (i = 0; i < num_sp; i++)
  {
    if ((get_pk_x (spackets[i].data) == x)
        && (get_pk_y (spackets[i].data) == y)
        && (get_pk_ext (spackets[i].data) == dc))
    {
      rv = malloc (VT_UNIT_LEN);
      if (rv)
        memmove (rv, spackets[i].data, VT_UNIT_LEN);
      break;
    }
  }
  if (NULL == rv)
    for (i = 0; i < num_mp; i++)
    {
      if ((get_pk_x (mpackets[i].data) == x)
          && (get_pk_y (mpackets[i].data) == y)
          && (get_pk_ext (mpackets[i].data) == dc))
      {
        rv = malloc (VT_UNIT_LEN);
        if (rv)
          memmove (rv, mpackets[i].data, VT_UNIT_LEN);
        break;
      }
    }
  free (mpackets);
  free (spackets);
  return rv;
}

static uint8_t *
vt_dlg_get_pg (void *d, int magno, int pgno, int subno, int *num_ret)
{
  PgmState *p = d;
  uint8_t *rv = NULL;
  int i;
  uint16_t num_pk;
  VtPk *packets =
    clientVtGetPagPk (p->sock, p->pos, p->frequency, p->pol, p->pid,
                      p->svc_id, magno, pgno, subno, &num_pk);
  if ((NULL == packets) || (0 == num_pk))
  {
    *num_ret = 0;
    return NULL;
  }
  rv = malloc (num_pk * VT_UNIT_LEN);
  if (NULL == rv)
  {
    *num_ret = 0;
    free (packets);
    return NULL;
  }

  for (i = 0; i < num_pk; i++)
  {
    memmove (rv + i * VT_UNIT_LEN, packets[i].data, VT_UNIT_LEN);
  }
  *num_ret = num_pk;
  free (packets);
  return rv;
}

static uint8_t *
vt_dlg_get_dpg (void *d, int magno, int pgno, int subno, int *num_ret)
{
  PgmState *p = d;
  int i;
  uint16_t num_pk;
  uint8_t *rv;
  VtPk *packets =
    clientVtGetPagPk (p->sock, p->pos, p->frequency, p->pol, p->pid,
                      p->svc_id, magno, pgno, subno, &num_pk);
  if ((NULL == packets) || (0 == num_pk))
  {
    *num_ret = 0;
    return NULL;
  }
  rv = malloc (num_pk * VT_UNIT_LEN);
  if (NULL == rv)
  {
    *num_ret = 0;
    free (packets);
    return NULL;
  }

  for (i = 0; i < num_pk; i++)
  {
    memmove (rv + i * VT_UNIT_LEN, packets[i].data, VT_UNIT_LEN);
  }
  *num_ret = num_pk;
  free (packets);
  return rv;
}

static void
vt_dlg_refresh (PgmState * pgm)
{
//      vt_dlg_buffers_clear(pgm);
  vtctxFlush (&pgm->vtc);
  if (vtctxParse (&pgm->vtc, pgm->pageno, pgm->subno))
    return;
  vt_dlg_buffers_init (pgm);
  pgm->should_animate = 1;
}

static void
vt_dlg_choose (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  Arg arglist[20];
//      int pg;
//      int sub;
  Cardinal i = 0;
  String label;

  XtSetArg (arglist[i], XtNlabel, &label);
  i++;
  XtGetValues (w, arglist, i);
  if (!strcmp ("NIL", label))
    return;

  pgm->pageno = atoi (label);
  pgm->subno = 0;
  vt_dlg_refresh (pgm);
}

static void
vt_set_lbl (Widget w, String lbl)
{
  Arg arglist[20];
  Cardinal i = 0;
  i = 0;
  XtSetArg (arglist[i], XtNlabel, lbl);
  i++;
  XtSetValues (w, arglist, i);
}


static void
vt_dlg_prev (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  char buf[20];
  pgm->pageno--;
  if (pgm->pageno < 100)
    pgm->pageno = 100;

  pgm->vt_buf_idx = 0;
  sprintf (buf, "%u\n", pgm->pageno);
  vt_set_lbl (pgm->pg_lbl, buf);

  pgm->subno = 0;
  sprintf (buf, "%u\n", pgm->subno);
  vt_set_lbl (pgm->sub_lbl, buf);

  vt_dlg_refresh (pgm);
}

static void
vt_dlg_next (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  char buf[20];
  pgm->pageno++;
  if (pgm->pageno >= 900)
    pgm->pageno = 899;

  pgm->vt_buf_idx = 0;
  sprintf (buf, "%u\n", pgm->pageno);
  vt_set_lbl (pgm->pg_lbl, buf);

  pgm->subno = 0;
  sprintf (buf, "%u\n", pgm->subno);
  vt_set_lbl (pgm->sub_lbl, buf);

  vt_dlg_refresh (pgm);
}

static void
vt_dlg_prev_s (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  char buf[20];
  pgm->subno--;
  if (pgm->subno < 0)
    pgm->subno = 0;

  pgm->vt_buf_idx = 0;
  sprintf (buf, "%u\n", pgm->subno);
  vt_set_lbl (pgm->sub_lbl, buf);
  vt_dlg_refresh (pgm);
}

static void
vt_dlg_next_s (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  char buf[20];
  pgm->subno++;
  if (pgm->subno >= 80)
    pgm->subno = 79;

  pgm->vt_buf_idx = 0;
  sprintf (buf, "%u\n", pgm->subno);
  vt_set_lbl (pgm->sub_lbl, buf);
  vt_dlg_refresh (pgm);
}


static void
vt_dlg_back (Widget w NOTUSED, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
//      vt_dlg_buffers_destroy(pgm);
  pgm->should_animate = 0;
  pgm->vt_act = 0;
  vtctxFlush (&pgm->vtc);
  XtUnmapWidget (pgm->vt_form);
  XtMapWidget (pgm->parent);
}

void
vtDlgDisplay (PgmState * pgm)
{
  char buf[20];
  swapInit (&pgm->s, XtDisplay (pgm->vt), XtWindow (pgm->vt));
  pgm->pageno = 100;
  pgm->vt_act = 1;

  pgm->vt_buf_idx = 0;
  sprintf (buf, "%u\n", pgm->pageno);
  vt_set_lbl (pgm->pg_lbl, buf);

  pgm->subno = 0;
  sprintf (buf, "%u\n", pgm->subno);
  vt_set_lbl (pgm->sub_lbl, buf);

  vt_dlg_refresh (pgm);
}

void
vtDlgClear2 (PgmState * pgm)
{
  vt_dlg_buffers_destroy (pgm);
  swapClear (&pgm->s);
}


void
vtDlgInit2 (PgmState * pgm)
{
  pgm->buf_valid = 0;
  swapInit (&pgm->s, XtDisplay (pgm->vt), XtWindow (pgm->vt));
  vt_dlg_buffers_alloc (pgm);
}

void
vt_dlg_inp (Widget w NOTUSED, XtPointer clnt, XEvent * evt, Boolean * cont)
{
  PgmState *pgm = clnt;
  if (pgm->vt_act && (evt->type == KeyPress))
  {
    char buf[3];
    buf[0] = '\0';
    XLookupString ((XKeyEvent *) evt, buf, 3, NULL, NULL);
    if ((buf[0] >= '0') && (buf[0] <= '9'))
    {
      if ((pgm->vt_buf_idx != 0) || ((buf[0] < '9') && (buf[0] > '0')))
      {
        pgm->vt_buf[pgm->vt_buf_idx] = buf[0];
        pgm->vt_buf[pgm->vt_buf_idx + 1] = '\0';
        vt_set_lbl (pgm->pg_lbl, pgm->vt_buf);
        if (pgm->vt_buf_idx >= 2)
        {
          pgm->pageno = atoi (pgm->vt_buf);
          pgm->subno = 0;
          vt_set_lbl (pgm->sub_lbl, "0");
          pgm->vt_buf_idx = 0;
          vt_dlg_refresh (pgm);
        }
        else
          pgm->vt_buf_idx++;
      }
    }
  }
  *cont = True;
}

void
vtDlgInit (Widget prnt, PgmState * pod)
{
  Arg arglist[20];
  Cardinal i = 0;
  int j, k;
  Widget /*evt_list,desc_txt,vt, */ next, prev, next_s, prev_s, links[4][6];    //,back;
  XtCallbackRec cl[2];
  cl[0].callback = vt_dlg_choose;
  cl[0].closure = pod;
  cl[1].callback = NULL;
  cl[1].closure = NULL;
  pod->should_animate = 0;
  XtAddEventHandler (prnt, KeyPressMask, False, vt_dlg_inp, pod);
//      XtAppAddActions(XtWidgetToApplicationContext(prnt), vtTable, 1);
  pod->vt_buf_idx = 0;
  pod->vt_buf[3] = '\0';
  pod->vt_act = 0;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  /*
     XtSetArg(arglist[i],XtNwidth,672);i++;
     XtSetArg(arglist[i],XtNheight,520);i++;
   */
  XtSetArg (arglist[i], XtNwidth, 336);
  i++;
  XtSetArg (arglist[i], XtNheight, 260);
  i++;
  pod->vt = XtCreateManagedWidget ("VT", coreWidgetClass, prnt, arglist, i);

//      vttrns=XtParseTranslationTable(vtTranslations);
//      XtOverrideTranslations(pod->vt,vttrns);


  cl[0].callback = vt_dlg_prev;
  cl[0].closure = pod;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pod->vt);
  i++;
//      XtSetArg(arglist[i],XtNfromVert,links[3][0]);i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  prev = XtCreateManagedWidget ("<P", commandWidgetClass, prnt, arglist, i);

  cl[0].callback = vt_dlg_next;
  cl[0].closure = pod;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
//      XtSetArg(arglist[i],XtNfromVert,links[3][0]);i++;
  XtSetArg (arglist[i], XtNfromHoriz, prev);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  next = XtCreateManagedWidget ("P>", commandWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, next);
  i++;
  pod->pg_lbl =
    XtCreateManagedWidget ("100", labelWidgetClass, prnt, arglist, i);

  cl[0].callback = vt_dlg_prev_s;
  cl[0].closure = pod;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, prev);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pod->vt);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  prev_s = XtCreateManagedWidget ("<S", commandWidgetClass, prnt, arglist, i);


  cl[0].callback = vt_dlg_next_s;
  cl[0].closure = pod;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, prev);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, prev_s);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  next_s = XtCreateManagedWidget ("S>", commandWidgetClass, prnt, arglist, i);

  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, prev);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, next_s);
  i++;
  pod->sub_lbl =
    XtCreateManagedWidget ("0 ", labelWidgetClass, prnt, arglist, i);

  for (k = 0; k < 6; k++)
  {
    for (j = 0; j < 4; j++)
    {
      i = 0;
      XtSetArg (arglist[i], XtNleft, XawChainLeft);
      i++;
      XtSetArg (arglist[i], XtNright, XawChainLeft);
      i++;
      XtSetArg (arglist[i], XtNtop, XawChainTop);
      i++;
      XtSetArg (arglist[i], XtNbottom, XawChainTop);
      i++;
      (k > 0) ? (XtSetArg (arglist[i], XtNfromVert, links[j][k - 1]),
                 i++) : (XtSetArg (arglist[i], XtNfromVert, next_s), i++);
      (j > 0) ? (XtSetArg (arglist[i], XtNfromHoriz, links[j - 1][k]),
                 i++) : (XtSetArg (arglist[i], XtNfromHoriz, pod->vt), i++);
      XtSetArg (arglist[i], XtNcallback, cl);
      i++;
      links[j][k] =
        XtCreateManagedWidget ("NIL", commandWidgetClass, prnt, arglist, i);
    }
  }

  cl[0].callback = vt_dlg_back;
  cl[0].closure = pod;
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromVert, links[3][5]);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, pod->vt);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  /*back= */ XtCreateManagedWidget ("Back", commandWidgetClass, prnt, arglist,
                                    i);

  vtctxInit (&pod->vtc, pod, vt_dlg_get_pk, vt_dlg_get_pg, vt_dlg_get_dpg);
}
