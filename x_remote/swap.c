#include "swap.h"
#include "debug.h"
#include "dmalloc.h"
#include "utl.h"

int DF_SWAP;
#define FILE_DBG DF_SWAP

static void
swapNoAction (Swap * this NOTUSED)
{

}

static void
swapFlipAction (Swap * this)
{
  XdbeSwapInfo swap_info;
  swap_info.swap_window = this->wdw;
  swap_info.swap_action = 0;
  XdbeSwapBuffers (this->dpy, &swap_info, 1);
}

static int
swapDbInit (Swap * f, Display * dpy, Window wdw)
{
  int dbe_major, dbe_minor, num_screens, k;
  int found;
/*	Arg arglist[20];
	Cardinal i=0;
  XVisualInfo vinfo_template;
  XVisualInfo *vinfo;
  int num_vinfo;*/
  VisualID wanted_id;
  XdbeScreenVisualInfo *xdvi;
  XWindowAttributes w_attr;
  f->dpy = dpy;
  f->wdw = wdw;
  XGetWindowAttributes (dpy, wdw, &w_attr);
//      scrn=XtScreen(top);
  if (0 == XdbeQueryExtension (dpy, &dbe_major, &dbe_minor))
  {
    errMsg ("0==XdbeQueryExtension()\n");
    return 1;
  }
  debugMsg ("Got DBE Version:%u.%u\n", dbe_major, dbe_minor);
  num_screens = 1;
  xdvi = XdbeGetVisualInfo (dpy, &wdw, &num_screens);
  if (NULL == xdvi)
  {
    errMsg ("NULL==xdvi\n");
    return 1;
  }
  wanted_id = XVisualIDFromVisual (w_attr.visual);
  found = 0;
  for (k = 0; k < num_screens; k++)
  {
    int j;
    debugMsg ("Screen %d\n", k);
    for (j = 0; j < xdvi[k].count; j++)
    {
      debugMsg ("\tVisual: 0x%lX, Depth: %u, Perf: %u\n",
                xdvi[k].visinfo[j].visual, xdvi[k].visinfo[j].depth,
                xdvi[k].visinfo[j].perflevel);
      if (wanted_id == xdvi[k].visinfo[j].visual)
      {
        found = 1;
        debugMsg ("\tWill use this one\n");
      }
    }
  }
/*
	originally I tried to allocate a window of a specific visual, but that gave a bad match error.
	May have been a mistake on my side aswell.
	Now we just check if the windows' visual is supported and use it
	vinfo_template.visualid=0x64;
	vinfo=XGetVisualInfo (dpy, VisualIDMask, &vinfo_template, &num_vinfo);
	if(vinfo!=NULL)
	{
		i=0;
	  XtSetArg(arglist[i],XtNvisual,vinfo[0].visual);i++;
		XtSetValues(top,arglist,i);
	}
	else
	{
		printf("vinfo==NULL\n");
	}
	select_performant_visual(5);//...of at least 5 bits depth?
	*/
  XdbeFreeVisualInfo (xdvi);
  if (0 == found)
  {
    errMsg ("Wanted visual: 0x%lX not found\n", wanted_id);
    return 1;
  }
  f->dest = XdbeAllocateBackBufferName (dpy, wdw, 0);
  return 0;
}


int
swapInit (Swap * s, Display * dpy, Window wdw)
{
//      Window wdw;
  if (0 == swapDbInit (s, dpy, wdw))
  {
    s->variant = 1;
    s->swap_action = swapFlipAction;
    return 0;
  }
  errMsg ("Resorting to single buffer operation\n");
  s->swap_action = swapNoAction;
  s->wdw = wdw;
  s->dpy = dpy;                 //XtDisplay(top);
  s->dest = wdw;
  s->variant = 0;
  return 0;
}

static void
swapDbClear (Swap * sw)
{
//      Display * dpy;
//      dpy=XtDisplay(top);
  if (sw->dpy != NULL)
    XdbeDeallocateBackBufferName (sw->dpy, sw->dest);

}

void
swapClear (Swap * s)
{
  if (1 == s->variant)
    swapDbClear (s);
  s->dpy = NULL;
}
