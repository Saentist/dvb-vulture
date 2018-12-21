#ifndef __swap_h
#define __swap_h
#include<stdint.h>
#include<X11/Xlib.h>
#include<X11/extensions/Xdbe.h>
/**
 *\file swap.h
 *\brief used for handling double buffering of X Windows
 *
 *Will transparently fall back to conventional drawing when dbe is not available
 *
 */

///should be called when drawing is complete. will swap buffers (if necessary)
#define swapAction(_S) ((_S)->swap_action(_S))
typedef struct Swap_s Swap;
struct Swap_s
{
  uint8_t variant;              ///<0: slow, 1: fast
  Window wdw;
  Display *dpy;
  Drawable dest;                ///<should be used for drawing to window. Is either backbuffer or the window itself.
  void (*swap_action) (Swap * this);
};

/**
 *\brief clears swapper
 *
 *Must be called before corresponding window is destroyed
 */
void swapClear (Swap * s);

/**
 *\brief initialize the swapper with an X Window
 *Window has to exist.. 
 */
int swapInit (Swap * s, Display * dpy, Window wdw);

#endif
