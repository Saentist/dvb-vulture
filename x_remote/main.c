#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <wordexp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xcms.h>
#include "pgmstate.h"
#include "menu.h"
#include "config.h"
#include "client.h"
#include "vt_dlg.h"
//should be last... or don't include malloc.h...
#include "dmalloc.h"
#include "debug.h"
/**
 *\file main.c
 *\brief test program for videotext component
 *
 * This program has a serious problem. 
 * Specifiying geometry strings on the command line to alter window size will not give the expected results.
 * More precisely this means that the surounding window will grow/shrink, while the dialogs stay their original size.
 *
 * There doesn't seem to be a way of making the dialogs take up the whole screen automagically, 
 * so the elements of the dialogs have been hardcoded to approximately fit the window.
 *
 * changing default font/font sizes or dpi may also mess up the interface, shifting elements outside the window.
 *
 * There doesn't seem to be an easy fix for this, so there shoud probably be a reimplementation using a
 * different framework or do a lowlevel X hack and draw everything ourseleves.
 *
 * Building another container widget to do the layout may also work.
 */
int DF_MAIN;
#define FILE_DBG DF_MAIN
/*
static void pmat(float mat[3][3])
{
	printf("[ %7f, %7f, %7f ]\n[ %7f, %7f, %7f ]\n[ %7f, %7f, %7f ]\n",mat[0][0],mat[0][1],mat[0][2],mat[1][0],mat[1][1],mat[1][2],mat[2][0],mat[2][1],mat[2][2]);

}
*/

static void quit (Widget w, XEvent * event, String * params,
                  Cardinal * num_params);

static Atom wm_delete_window;
static XtActionsRec devgui_actions[] = {
  {"quit", quit},
};

int
xprintf (void *d NOTUSED, const char *f, int l, int pri NOTUSED, const char *template, ...)
{
  int n, size = 100;
  char *p, *np;
  va_list ap;
//      extern char *program_invocation_short_name;

  if ((p = malloc (size)) == NULL)
    return 0;

  while (1)
  {
    /* Try to print in the allocated space. */
    va_start (ap, template);
    n = vsnprintf (p, size, template, ap);
    va_end (ap);
    /* If that worked, return the string. */
    if (n > -1 && n < size)
      break;
    /* Else try again with more space. */
    if (n > -1)                 /* glibc 2.1 */
      size = n + 1;             /* precisely what is needed */
    else                        /* glibc 2.0 */
      size *= 2;                /* twice the old size */
    if ((np = realloc (p, size)) == NULL)
    {
      free (p);
      return 0;
    }
    else
    {
      p = np;
    }
  }

  fprintf (stderr, "File %s, Line %d: %s", f, l, p);
  free (p);
  return 0;
}

char *
find_opt (int argc, char *argv[], char *key)
{
  int i = 0;
  debugMsg ("looking for cmdline argument %s\n", key);
  for (i = 1; i < argc; i++)
  {
    debugMsg ("seen %s\n", argv[i]);
    if (!strcmp (argv[i], key))
    {
      debugMsg ("matches\n");
      if (argc > (i + 1))
      {
        debugMsg ("got value %s\n", argv[i + 1]);
        return argv[i + 1];
      }
    }
  }
  debugMsg ("not found\n");
  return NULL;
}


void
free_fav_list (void *x NOTUSED, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 3) || (which == 2))
  {
    FavList *fl = btreeNodePayload (this);
    favListClear (fl);
    free (this);
  }
}

void
free_fav_lists (BTreeNode ** fl)
{
  btreeWalk (*fl, NULL, free_fav_list);
  *fl = NULL;
}

void
pgmStateClear (PgmState * a)
{
  wordexp_t wxp;
  FILE *f;
  DListE *e, *next;
  char *fav_db;
  char *rem_db;
  char *buffer;
  char *pfx = LOC_CFG_PFX;
  size_t str_sz;

  fav_db = "fav.db";
  rcfileFindVal (&a->cfg, "FILES", "fav_db", &fav_db);

  rem_db = "rem.db";
  rcfileFindVal (&a->cfg, "FILES", "rem_db", &rem_db);

  str_sz = strlen (fav_db) + strlen (pfx) + 2;
  buffer = utlCalloc (1, str_sz);
  snprintf (buffer, str_sz, "%s/%s", pfx, fav_db);
  if (!wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
  {
    f = fopen (wxp.we_wordv[0], "w");
    if (f)
    {
      if (btreeWrite (a->favourite_lists, NULL, favListWriteNode, f))
      {
        errMsg ("error writing btree\n");
      }
      fclose (f);
    }
    wordfree (&wxp);
  }
  utlFAN (buffer);
  free_fav_lists (&a->favourite_lists);

  str_sz = strlen (rem_db) + strlen (pfx) + 2;
  buffer = utlCalloc (1, str_sz);
  snprintf (buffer, str_sz, "%s/%s", pfx, rem_db);
  if (!wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
  {
    f = fopen (wxp.we_wordv[0], "w");
    if (f)
    {
      if (reminderListWrite (f, &a->reminder_list))
      {
        errMsg ("error writing reminders\n");
      }
      fclose (f);
    }
    wordfree (&wxp);
  }
  utlFAN (buffer);
  e = dlistFirst (&a->reminder_list);
  while (e)
  {
    next = dlistENext (e);
    reminderDel (e);
    e = next;
  }
  dlistInit (&a->reminder_list);

  clientClear (a->sock);
  rcfileClear (&a->cfg);
  vtctxClear (&a->vtc);

}

extern int DF_SWAP;
extern int DF_CLIENT;
int
pgmStateInit (PgmState * a, int argc, char **argv)
{
  char *cfgfile = LOC_CFG;
  char *value;
  char *fav_db;
  char *rem_db;
  char *buffer;
  char *pfx = LOC_CFG_PFX;
  size_t str_sz;
  int sock;
  int af;
  long prt, tmp;
  FILE *f;
  wordexp_t wxp;

  debugSetFunc (NULL, xprintf, 32);
  DF_MAIN = debugAddModule ("main");
  DF_SWAP = debugAddModule ("swap");
  DF_CLIENT = debugAddModule ("client");
  debugSetFlags (argc, argv, "-dbg");

  if ((value = find_opt (argc, argv, "-cfg")))
  {
    cfgfile = value;
  }

  if (wordexp (cfgfile, &wxp, WRDE_NOCMD | WRDE_UNDEF))
    return 1;

  if (rcfileParse (wxp.we_wordv[0], &a->cfg))
  {
    wordfree (&wxp);
    return 1;
  }
  wordfree (&wxp);

  fav_db = "fav.db";
  rcfileFindVal (&a->cfg, "FILES", "fav_db", &fav_db);

  rem_db = "rem.db";
  rcfileFindVal (&a->cfg, "FILES", "rem_db", &rem_db);

  af = DEFAULT_AF;
  if (!rcfileFindValInt (&a->cfg, "NET", "af", &tmp))
  {
    if (tmp == 4)
      af = AF_INET;
    if (tmp == 6)
      af = AF_INET6;
  }

  if (rcfileFindVal (&a->cfg, "NET", "host", &value))
  {
    value = DEFAULT_SERVER (af);
    debugMsg ("No host= entry in [NET] section of configfile. using %s.\n",
              value);
  }

  prt = 2241;
  rcfileFindValInt (&a->cfg, "NET", "port", &prt);

  a->favourite_lists = NULL;
  a->current = NULL;
  str_sz = strlen (fav_db) + strlen (pfx) + 2;
  buffer = calloc (1, str_sz);
  snprintf (buffer, str_sz, "%s/%s", pfx, fav_db);
  if (wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
    return 1;
  f = fopen (wxp.we_wordv[0], "r");
  if (f)
  {
    if (btreeRead (&a->favourite_lists, NULL, favListReadNode, f))
    {
      errMsg ("error reading btree\n");
      rcfileClear (&a->cfg);
      return 1;
    }
    fclose (f);
  }
  else
    a->favourite_lists = NULL;
  wordfree (&wxp);
  free (buffer);

  dlistInit (&a->reminder_list);
  str_sz = strlen (rem_db) + strlen (pfx) + 2;
  buffer = calloc (1, str_sz);
  snprintf (buffer, str_sz, "%s/%s", pfx, rem_db);
  dlistInit (&a->reminder_list);
  if (wordexp (buffer, &wxp, WRDE_NOCMD | WRDE_UNDEF))
    return 1;
  f = fopen (wxp.we_wordv[0], "r");
  if (f)
  {
    if (reminderListRead (f, &a->reminder_list))
    {
      errMsg ("error reading reminders\n");
      rcfileClear (&a->cfg);
      return 1;
    }
    fclose (f);
  }
  wordfree (&wxp);
  free (buffer);

  sock = clientInit (af, value, prt);
  if (sock == -1)
  {
    rcfileClear (&a->cfg);
    return 1;
  }
  a->sock = sock;
  a->pos = 0;

  /*
     a->frequency=1269225;
     a->pol=0;
     a->pid=505;
     a->svc_id=16;
   */
  /*
     a->frequency=1195350;
     a->pol=0;
     a->pid=130;
     a->svc_id=16;

     a->frequency=1195350;
     a->pol=0;
     a->pid=680;
     a->svc_id=16;

     vtctxParse(&a->vtc, 100, 0);
   */
  return 0;
}


Boolean
animation (XtPointer client_data)
{
  PgmState *pgm = client_data;
  /*
     XCopyArea (display, src, dest, gc, src_x, src_y, width, height, dest_x, dest_y)
     Display *display;
     Drawable src, dest;
     GC gc;
     int src_x, src_y;
     unsigned int width, height;
     int dest_x, dest_y;
     This function uses these GC components: function, plane-mask, subwindow-mode, graphics-
     exposures, clip-x-origin, clip-y-origin, and clip-mask.
   */
  if (pgm->should_animate)
  {
//      printf("copying\n");
    XCopyArea (pgm->s.dpy, pgm->buffers[pgm->idx], pgm->s.dest, pgm->gc, 0, 0,
               336, 260, 0, 0);
    swapAction (&pgm->s);
    pgm->idx = (pgm->idx + 1) % 12;
  }
  usleep (83000);
  return False;
}

/*

Main menu structure:
Browse master list
Browse fav list
Select fav list
Rec Schedule
Reminder List
Setup
Exit

Setup Menu:
Add fav List
Del Fav List
Switch Setup
Freq Correct
Stats

Independent Toggles:
Reminders active
Client active
*/

static void
active_cb (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  Arg arglist[20];
  Cardinal i = 0;
  Boolean st;
  i = 0;
  XtSetArg (arglist[i], XtNstate, &st);
  i++;
  XtGetValues (w, arglist, i);
  if (st)
  {
    clientGoActive (pgm->sock);
  }
  else
  {
    clientGoInactive (pgm->sock);
  }
}

static void
rmd_mode (Widget w, XtPointer client_data, XtPointer call_data NOTUSED)
{
  PgmState *pgm = client_data;
  Arg arglist[20];
  Cardinal i = 0;
  Boolean st;
  i = 0;
  XtSetArg (arglist[i], XtNstate, &st);
  i++;
  XtGetValues (w, arglist, i);
  pgm->rmd_auto = st;
}

static void
init_top (Widget prnt, PgmState * pgm)
{
  Widget form, active /*,rmd_act */ , mnu_form; //,messages;
  Arg arglist[20];
  Cardinal i = 0;
  XtCallbackRec cl[2];
  cl[1].callback = NULL;
  cl[1].closure = NULL;

  pgm->rmd_auto = False;
  cl[0].callback = active_cb;
  cl[0].closure = pgm;

  i = 0;
  form =
    XtCreateManagedWidget ("RemoteForm", formWidgetClass, prnt, arglist, i);

  //active state toggle
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
  active =
    XtCreateManagedWidget ("Active", toggleWidgetClass, form, arglist, i);

  cl[0].callback = rmd_mode;
  cl[0].closure = pgm;
  //reminder mode active toggle
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNfromHoriz, active);
  i++;
  XtSetArg (arglist[i], XtNcallback, cl);
  i++;
  /*rmd_act= */ XtCreateManagedWidget ("Reminder Mode", toggleWidgetClass,
                                       form, arglist, i);


  //the form with the menu
  i = 0;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainTop);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNfromVert, active);
  i++;
//  XtSetArg(arglist[i],XtNallowVert,True);i++;
//  XtSetArg(arglist[i],XtNallowHoriz,True);i++;
//  XtSetArg(arglist[i],XtNwidth,630);i++;
//  XtSetArg(arglist[i],XtNheight,100);i++;
  mnu_form =
    XtCreateManagedWidget ("MenuForm", formWidgetClass, form, arglist, i);

  menuInit (mnu_form, pgm);


  //message output window
  i = 0;
  XtSetArg (arglist[i], XtNwidth, 560);
  i++;
  XtSetArg (arglist[i], XtNheight, 80);
  i++;
  XtSetArg (arglist[i], XtNleft, XawChainLeft);
  i++;
  XtSetArg (arglist[i], XtNright, XawChainRight);
  i++;
  XtSetArg (arglist[i], XtNbottom, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNtop, XawChainBottom);
  i++;
  XtSetArg (arglist[i], XtNwrap, XawtextWrapWord);
  i++;
  XtSetArg (arglist[i], XtNfromVert, mnu_form);
  i++;
  XtSetArg (arglist[i], XtNinternational, True);
  i++;
  pgm->messages =
    XtCreateManagedWidget ("Messages", asciiTextWidgetClass, form, arglist,
                           i);
}


int
main (int argc, char **argv)
{
  Widget top;                   //,label,form,mlist,flist,add_flist,del_flist,swset,active,fcorr,sched,rmds,stats,rmd_act,exit,btnform,mnu_form,messages;
  XtWorkProcId wp;
//      Display * dpy;
//      FILE * cfg;
  Arg arglist[20];
//      {XtNwidth,(XtArgVal)600},
  //{XtNheight,(XtArgVal)400}};
  XtAppContext ctx;
  Cardinal i = 0;
//      char * cfgfile=GLOB_CFG;
//      rc_file conf;
//      int dbe_major,dbe_minor,num_sreens;
//      Pixmap pxm;
//      XdbeScreenVisualInfo *xdvi;
//  XVisualInfo vinfo_template;
//  XVisualInfo *vinfo;
//  int num_vinfo;
  PgmState pgm;
  setlocale (LC_ALL, "");
  XtSetLanguageProc(NULL, NULL, NULL);
  if (pgmStateInit (&pgm, argc, argv))
    return 1;

  /*
     setDebugFunc(NULL,xprintf,10);
     DF_MAIN=addDebugModule("main");
     setDebugFlags(argc, argv);

     cfg=fopen(cfgfile,"r");
     if(!cfg)
     {
     errMsg("NO cfg file found at %s\n",cfgfile);
     return 1;
     }

     if(parseConfig(cfg,&conf))
     {
     errMsg("cfg file parse error\n");
     fclose(cfg);
     return 1;
     }
     fclose(cfg);
     pxm=XCreatePixmap(XtDisplay(label), label, 672, 260, 8);//depth determined by max pixel value, I think
     //   XCreateColormap(display, top,visual,AllocAll);
     for(i=0;i<32;i++)
     {
     XAllocColor(display, colormap, screen_in_out);//we get allocated pixel value inside screen_in_out
     }
     //   if(comm_init(tty))
     //   return 1;
     XtSetLanguageProc(NULL,NULL,NULL);
   */
/*	i=0;
  XtSetArg(arglist[i],XtNwidth,672);i++;
  XtSetArg(arglist[i],XtNheight,260);i++;
	
 The XtAppInitialize function calls XtToolkitInitialize followed by XtCreateApplicationCon-
text, then calls XtOpenDisplay with display_string NULL and application_name NULL, and
finally calls XtAppCreateShell with application_name NULL, widget_class applicationShell-
WidgetClass, and the specified args and num_args and returns the created shell. The modified
argc and argv returned by XtDisplayInitialize are returned in argc_in_out and argv_in_out. If
app_context_return is not NULL, the created application context is also returned. If the display
*/
/*	XtToolkitInitialize();
	ctx=XtCreateApplicationContext();
	dpy=XtOpenDisplay(ctx,NULL,NULL,"RemoteControl",NULL,0,&argc,argv);
	vinfo_template.visualid=0x64;
	vinfo=XGetVisualInfo (dpy, VisualIDMask, &vinfo_template, &num_vinfo);
	if(vinfo!=NULL)
	{
	  XtSetArg(arglist[i],XtNvisual,vinfo[0].visual);i++;
	  XtSetArg(arglist[i],XtNdepth,vinfo[0].depth);i++;
	}
	
The XtOpenDisplay function calls XOpenDisplay with the specified display_string. If dis-
play_string is NULL, XtOpenDisplay uses the current value of the -display option specified in
argv. If no display is specified in argv, the user's default display is retrieved from the environ-
ment. On POSIX-based systems, this is the value of the DISPLAY environment variable.
If this succeeds, XtOpenDisplay then calls XtDisplayInitialize and passes it the opened display
and the value of the -name option specified in argv as the application name. If no -name option
is specified and application_name is non-NULL, application_name is passed to XtDisplayIni-
tialize. If application_name is NULL and if the environment variable RESOURCE_NAME is
set, the value of RESOURCE_NAME is used. Otherwise, the application name is the name used
to invoke the program. On implementations that conform to ANSI C Hosted Environment sup-
port, the application name will be argv[0] less any directory and file type components, that is, the
final component of argv[0], if specified. If argv[0] does not exist or is the empty string, the appli-
cation name is ``main''. XtOpenDisplay returns the newly opened display or NULL if it failed.
*/
//      top=XtAppCreateShell(NULL,"RemoteControl",applicationShellWidgetClass,dpy,arglist,i);
/*
Status XGetWindowAttributes (display, w, window_attributes_return)
    Display *display;
    Window w;
    XWindowAttributes *window_attributes_return;
display           Specifies the connection to the X server.
w                 Specifies the window whose current attributes you want to obtain.
window_attributes_return
                  Returns the specified window's attributes in the XWindowAttributes structure.
The XGetWindowAttributes function returns the current attributes for the specified window to
an XWindowAttributes structure.
typedef struct {
        int x, y;                                   location of window 
        int width, height;                          width and height of window 
        int border_width;                           border width of window 
        int depth;                                  depth of window 
        Visual *visual;                             the associated visual structure 
        Window root;                                root of screen containing window
        int class;                                  InputOutput, InputOnly
        int bit_gravity;                            one of the bit gravity values 
        int win_gravity;                            one of the window gravity values 
        int backing_store;                          NotUseful, WhenMapped, Always 
        unsigned long backing_planes;               planes to be preserved if possible 
        unsigned long backing_pixel;                value to be used when restoring planes 
        Bool save_under;                            boolean, should bits under be saved? 
        Colormap colormap;                          color map to be associated with window 
        Bool map_installed;                         boolean, is color map currently installed
        int map_state;                              IsUnmapped, IsUnviewable, IsViewable 
        long all_event_masks;                       set of events all people have interest in
        long your_event_mask;                       my event mask 
        long do_not_propagate_mask;                 set of events that should not propagate 
        Bool override_redirect;                     boolean value for override-redirect 
        Screen *screen;                             back pointer to correct screen 
} XWindowAttributes;
	XGetWindowAttributes
	*/
  /*
     XtSetArg(arglist[i],XtNvisual,visual);i++;
   */
  i = 0;
  XtSetArg (arglist[i], XtNwidth, 600);
  i++;
  XtSetArg (arglist[i], XtNheight, 400);
  i++;
  top =
    XtAppInitialize (&ctx, "RemoteControl", 0, 0, &argc, argv, NULL, arglist,
                     i);
//  XtSetArg(arglist[i],XtNallowHoriz,True);i++;
  //XtSetArg(arglist[i],XtNallowVert,True);i++;

  init_top (top, &pgm);


//      w3=XtCreateManagedWidget("VT",vtViewWidgetClass,top,arglist,i);
/*
	i=0;
  XtSetArg(arglist[i],XtNleft,XawChainLeft);i++;
  XtSetArg(arglist[i],XtNright,XawChainRight);i++;
  XtSetArg(arglist[i],XtNtop,XawChainTop);i++;
	pgmlist_form=XtCreateManagedWidget("pgms",formWidgetClass,w3,arglist,i);

	create_pgm_controls(pgmlist_form);
	*/
  XtAppAddActions (ctx, devgui_actions, XtNumber (devgui_actions));
  XtOverrideTranslations (top,
                          XtParseTranslationTable
                          ("<Message>WM_PROTOCOLS: quit()"));
  XtRealizeWidget (top);
/* 	XtUnmanageChild(pgm.placeholder);
 	XtManageChild(pgm.main_form);
 	*/
  XtMapWidget (top);
  XtMapWidget (pgm.main_form);
//      swapInit(&pgm.s,XtDisplay(label),XtWindow(label));
//      buffersInit(&pgm);
//      pgm.should_animate=0;
  vtDlgInit2 (&pgm);
  wm_delete_window = XInternAtom (XtDisplay (top), "WM_DELETE_WINDOW", False);
  XSetWMProtocols (XtDisplay (top), XtWindow (top), &wm_delete_window, 1);
  wp = XtAppAddWorkProc (ctx, animation, &pgm);
  XtAppMainLoop (ctx);
  vtDlgClear2 (&pgm);
//      pgm.should_animate=0;
  XtRemoveWorkProc (wp);
//      buffersClear(&pgm);
//      swapClear(&pgm.s);
  XtDestroyApplicationContext (XtWidgetToApplicationContext (top));
//  rcfileClear(&conf);
  pgmStateClear (&pgm);
  return 0;
}

static void
quit (Widget w, XEvent * event, String * params NOTUSED, Cardinal * num_params NOTUSED)
{
  if (event->type == ClientMessage &&
      ((unsigned)event->xclient.data.l[0]) != wm_delete_window)
  {
    XBell (XtDisplay (w), 0);
    return;
  }
  XtAppSetExitFlag (XtWidgetToApplicationContext (w));
}
