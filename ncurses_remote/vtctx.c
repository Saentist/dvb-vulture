#include <ctype.h>
#include "utl.h"
#include "dmalloc_f.h"
#include "vtctx.h"

int
vtctxInit (VtCtx * ctx)
{
  ctx->current_pos_x = 8;
  ctx->current_pos_y = 0;
  ctx->current_pair = 0;
  ctx->used_pairs = 1;
  ctx->blink = 0;
  return 0;
}

int
vtctxClear (VtCtx * ctx)
{
  ctx->current_pos_x = 8;
  ctx->current_pos_y = 0;
  ctx->current_pair = 0;
  ctx->used_pairs = 1;
  ctx->blink = 0;
  return 0;
}

int
vtctxSetFg (VtCtx * ctx, short nfg)
{
/*
look for a color pair with desired combination.
If none found, create new one
*/
  int i;
  short fg, bg;
  pair_content (ctx->current_pair, &fg, &bg);
  if ((nfg == COLOR_WHITE) && (bg == COLOR_BLACK))
  {
    ctx->current_pair = 0;
    return 0;
  }
  for (i = 1; i < ctx->used_pairs; i++)
  {
    short tmpfg, tmpbg;
    pair_content (i, &tmpfg, &tmpbg);
    if ((bg == tmpbg) && (nfg == tmpfg))
    {
      ctx->current_pair = i;
      return 0;
    }
  }
  if (ctx->used_pairs >= COLOR_PAIRS)
    return 1;
  init_pair (ctx->used_pairs, nfg, bg);
  ctx->current_pair = ctx->used_pairs;
  ctx->used_pairs++;
  return 0;
}

int
vtctxSetFlash (VtCtx * ctx)
{
  ctx->blink = 1;
  return 0;
}

int
vtctxClearFlash (VtCtx * ctx)
{
  ctx->blink = 0;
  return 0;
}

int
vtctxResetBkgd (VtCtx * ctx)
{                               //set black bg
  short fg, nbg = COLOR_BLACK, bg;
  int i;
  pair_content (ctx->current_pair, &fg, &bg);
  if (fg == COLOR_WHITE)
  {
    ctx->current_pair = 0;
    return 0;
  }
  for (i = 1; i < ctx->used_pairs; i++)
  {
    short tmpfg, tmpbg;
    pair_content (i, &tmpfg, &tmpbg);
    if ((nbg == tmpbg) && (fg == tmpfg))
    {
      ctx->current_pair = i;
      return 0;
    }
  }
  if (ctx->used_pairs >= COLOR_PAIRS)
    return 1;
  init_pair (ctx->used_pairs, fg, nbg);
  ctx->current_pair = ctx->used_pairs;
  ctx->used_pairs++;
  return 0;

}

int
vtctxSetBkgd (VtCtx * ctx)
{                               //set background to current foreground color(they are equal afterwards)
  short nbg, bg;
  int i;
  pair_content (ctx->current_pair, &nbg, &bg);
  for (i = 1; i < ctx->used_pairs; i++)
  {
    short tmpfg, tmpbg;
    pair_content (i, &tmpfg, &tmpbg);
    if ((nbg == tmpbg) && (nbg == tmpfg))
    {
      ctx->current_pair = i;
      return 0;
    }
  }
  if (ctx->used_pairs >= COLOR_PAIRS)
    return 1;
  init_pair (ctx->used_pairs, nbg, nbg);
  ctx->current_pair = ctx->used_pairs;
  ctx->used_pairs++;
  return 0;

}

int
vtctxSetY (VtCtx * ctx, int y)
{
  ctx->current_pos_y = y;
  vtctxClearFlash (ctx);
  ctx->current_pair = 0;
  ctx->current_pos_x = 0;
  return 0;
}

int
vtctxChar (WINDOW * vt_win, VtCtx * ctx, char c)
{
  if (ctx->current_pos_y >= 25)
    return 1;
  if (!isprint (c))
    c = ' ';
  mvwaddch (vt_win, ctx->current_pos_y, ctx->current_pos_x,
            c | ((ctx->blink) ? A_BLINK : 0) |
            COLOR_PAIR (ctx->current_pair));
  ctx->current_pos_x++;
  return 0;
}
