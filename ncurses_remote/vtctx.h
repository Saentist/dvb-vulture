#ifndef __vtctx_h
#define __vtctx_h
#include <ncurses.h>
#include <stdint.h>
/**
 *\file vtctx.h
 *\brief vt parsing context
 *keeps track of character attributes
 */
typedef struct
{
  short current_pair;
  short used_pairs;             ///<counts used pairs upward
  short current_pos_x;
  short current_pos_y;
  uint8_t blink;
} VtCtx;

int vtctxSetY (VtCtx * ctx, int y);
int vtctxInit (VtCtx * ctx);
int vtctxClear (VtCtx * ctx);
int vtctxSetFg (VtCtx * ctx, short fg);
int vtctxSetFlash (VtCtx * ctx);
int vtctxClearFlash (VtCtx * ctx);
int vtctxResetBkgd (VtCtx * ctx);
int vtctxSetBkgd (VtCtx * ctx);
int vtctxChar (WINDOW * vt_win, VtCtx * ctx, char c);
#endif
