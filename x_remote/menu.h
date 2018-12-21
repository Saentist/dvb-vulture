#ifndef __menu_h
#define __menu_h
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Tree.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/StripChart.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Porthole.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/List.h>
#include "main_menu.h"
#include "master_list.h"
#include "fav_list.h"
#include "reminder_list.h"
#include "reminder_ed.h"
#include "sched.h"
#include "sched_ed.h"
#include "setup.h"
#include "stats.h"


void menuInit (Widget prnt, PgmState * pgm);


#endif //__menu_h
