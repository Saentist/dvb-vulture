#include <ctype.h>
#include "vtctx.h"
#include <stdlib.h>

#define VT_SPACE 32
///put a level one page char
static void
vtlop_char (VtCtx * ctx, uint8_t d)
{
  int y = ctx->pos_y, x = ctx->pos_x;

//      int glyph=vtctx_lookup_glyph(ctx,d);
  if (ctx->is_mosaic)
  {
    ctx->last_mosaic_char = d;
    ctx->last_mosaic_separate = ctx->mosaics_separate;
  }
  ctx->screen[y][x].character = d;
  ctx->screen[y][x].fg_color = ctx->fg_color;
  ctx->screen[y][x].bg_color = ctx->bg_color;
  ctx->screen[y][x].flash_mode = ctx->flash ? 1 : 0;
  ctx->screen[y][x].flash_rate = 0;
  ctx->screen[y][x].invert = 0;
  ctx->screen[y][x].conceal = ctx->conceal;
  ctx->screen[y][x].window = (ctx->box_state >= 2) ? 1 : 0;
  ctx->screen[y][x].e_size = ctx->e_size;
  ctx->screen[y][x].underline = ctx->mosaics_separate;
  ctx->screen[y][x].italics = 0;
  ctx->screen[y][x].bold = 0;
  ctx->screen[y][x].charset = ctx->g0_esc ? ctx->sld : ctx->default_charset;
  ctx->screen[y][x].ext_char = 0;
  ctx->screen[y][x].is_mosaic = ctx->is_mosaic;
  ctx->screen[y][x].at_char = 0;
  ctx->screen[y][x].at_char = 0;
  ctx->screen[y][x].is_drcs = 0;
  ctx->screen[y][x].diacrit = 0;
  ctx->pos_x = (ctx->pos_x + 1) % 72;

//      vtctx_glyph(ctx,glyph,ctx->mosaics_separate);
}

static void
vtlop_pos (VtCtx * ctx, uint8_t col, uint8_t row)
{
  ctx->pos_x = col;
  ctx->pos_y = row;
}

static void
vtlop_alpha_color (VtCtx * ctx, uint8_t d)
{
  ctx->fg_color = d;
  ctx->is_mosaic = 0;
  ctx->conceal = 0;
  ctx->last_mosaic_char = VT_SPACE;

}

static void
vtlop_mosaic_color (VtCtx * ctx, uint8_t d)
{
  ctx->fg_color = d;
  ctx->is_mosaic = 1;
  ctx->conceal = 0;
//      ctx->last_mosaic_char=VT_SPACE;
}

static void
vtlop_flash (VtCtx * ctx, uint8_t state)
{
  ctx->flash = state;
}

static void
vtlop_mosaic_joined (VtCtx * ctx, uint8_t state)
{
  ctx->mosaics_separate = !state;
}

static void
vtlop_g0_default (VtCtx * ctx)
{
  ctx->g0_esc = 0;
}

static void
vtlop_g0_toggle (VtCtx * ctx)
{
  ctx->g0_esc = ctx->g0_esc ^ 1;
}

static void
vtlop_bg_black (VtCtx * ctx)
{
  ctx->bg_color = 0;
}

static void
vtlop_mosaic_hold (VtCtx * ctx, uint8_t state)
{
  ctx->m_hold = state;
}

static void
vtlop_box_default (VtCtx * ctx)
{
  ctx->box_state = 0;
}

static void
vtlop_box (VtCtx * ctx, uint8_t flag)
{
  if (flag)
  {
    switch (ctx->box_state)
    {
    case 0:
    case 1:
      ctx->box_state++;
    default:
      break;
    }
  }
  else
  {
    switch (ctx->box_state)
    {
    case 2:
    case 3:
      ctx->box_state++;
    default:
      break;
    }
  }
}

static void
vtlop_size (VtCtx * ctx, uint8_t size)
{
  if (ctx->e_size != size)
    ctx->last_mosaic_char = VT_SPACE;
  ctx->e_size = size;
  if ((ctx->pos_y < 25) && (size & 1)
      && (!(ctx->skip_lines & (1 << (ctx->pos_y + 1)))))
  {
    ctx->skip_lines |= (1 << (ctx->pos_y + 1));
  }
}

static void
vtlop_conceal (VtCtx * ctx)
{
  ctx->conceal = 1;
}

static void
vtlop_bg_set (VtCtx * ctx)
{
  ctx->bg_color = ctx->fg_color;
}

static uint8_t
vtlop_unparity_rev (uint8_t v)
{
  v = bitrev (v);
  return v & 0x7f;              //whatever...
}

static void
vtlop_select_lang_hdr (VtCtx * ctx, uint8_t * pk)
{
  uint8_t cb = hamm84_r (*VT_PK_BYTE (pk, 13));

  ctx->default_charset = (ctx->default_charset & (~7)) | (cb & 7);
  return;
}



void
vtlopParsePagePk (VtCtx * ctx, uint8_t * pk)
{
  int y;
  y = get_pk_y (pk);
  if ((y >= 0) && (y <= 25))
  {                             //directly displayable packets X/1 to X/25
    int i;
    int start = VT_DATA_START;
    if (ctx->skip_lines & (1 << y))
      return;
    if (y == 0)
    {
      vtlop_select_lang_hdr (ctx, pk);
      start = HDR_DATA_START;
      vtlop_pos (ctx, 4, y);    //column 8 row y
    }
    else
      vtlop_pos (ctx, 0, y);    //column 0 row y
    vtlop_alpha_color (ctx, 7); //start of row default condition
    vtlop_flash (ctx, 0);       //start of row default condition
    vtlop_mosaic_joined (ctx, 1);
    vtlop_g0_default (ctx);
    vtlop_bg_black (ctx);
    vtlop_mosaic_hold (ctx, 0);
    vtlop_box_default (ctx);
    vtlop_size (ctx, 0);
    ctx->last_mosaic_char = VT_SPACE;
    for (i = start; i < VT_UNIT_LEN; i++)
    {
      uint8_t d = pk[i];
//                      uint8_t d;
      int yp = ctx->pos_y, xp = ctx->pos_x = i - VT_DATA_START;
      d = vtlop_unparity_rev (d);
      ctx->screen[yp][xp].fg_color = ctx->fg_color;
      ctx->screen[yp][xp].bg_color = ctx->bg_color;
      ctx->screen[yp][xp].flash_mode = ctx->flash ? 1 : 0;
      ctx->screen[yp][xp].flash_rate = 0;
      ctx->screen[yp][xp].invert = 0;
      ctx->screen[yp][xp].conceal = ctx->conceal;
      ctx->screen[yp][xp].window = (ctx->box_state >= 2) ? 1 : 0;
      ctx->screen[yp][xp].underline = ctx->mosaics_separate;
      ctx->screen[yp][xp].italics = 0;
      ctx->screen[yp][xp].bold = 0;
      ctx->screen[yp][xp].is_mosaic = ctx->is_mosaic;
      ctx->screen[yp][xp].charset =
        ctx->g0_esc ? ctx->sld : ctx->default_charset;
      ctx->screen[yp][xp].e_size = ctx->e_size;
//                      ctx->screen[yp][xp].character=d;
      ctx->screen[yp][xp].diacrit = 0;
      if (ctx->m_hold)
      {
        ctx->screen[yp][xp].underline = ctx->last_mosaic_separate;
        ctx->screen[yp][xp].is_mosaic = 1;
        ctx->screen[yp][xp].character = ctx->last_mosaic_char;
      }

      if (d < 32)
      {                         //attribute
        ctx->screen[yp][xp].attr = d + 1;
        if (d <= 7)
          vtlop_alpha_color (ctx, d);
        else if ((d >= 0x10) && (d <= 0x17))
          vtlop_mosaic_color (ctx, d - 0x10);
        else
          switch (d)
          {
          case 8:
            vtlop_flash (ctx, 1);
            break;
          case 9:
            vtlop_flash (ctx, 0);       //set at
            ctx->screen[yp][xp].flash_mode = 0;
            break;
          case 0xA:
            vtlop_box (ctx, 0);
            break;
          case 0xB:
            vtlop_box (ctx, 1);
            break;
          case 0xC:
            vtlop_size (ctx, 0);        //normal, set at
            ctx->screen[yp][xp].e_size = 0;
            break;
          case 0xD:
            vtlop_size (ctx, 1);        //double height
            break;
          case 0xE:
            vtlop_size (ctx, 2);        //double width
            break;
          case 0xF:
            vtlop_size (ctx, 3);        //double size (height and width)
            break;
          case 0x18:           //conceal set at
            vtlop_conceal (ctx);
            ctx->screen[yp][xp].conceal = 1;
            break;
          case 0x19:           //set at
            vtlop_mosaic_joined (ctx, 1);
            ctx->screen[yp][xp].underline = 0;
            break;
          case 0x1A:
            vtlop_mosaic_joined (ctx, 0);
            ctx->screen[yp][xp].underline = 1;
            break;
          case 0x1B:
            vtlop_g0_toggle (ctx);
            break;
          case 0x1C:
            vtlop_bg_black (ctx);
            ctx->screen[yp][xp].bg_color = ctx->bg_color;
            break;
          case 0x1D:
            vtlop_bg_set (ctx); //...to current foreground colour
            ctx->screen[yp][xp].bg_color = ctx->bg_color;
            break;
          case 0x1E:
            vtlop_mosaic_hold (ctx, 1);
            ctx->screen[yp][xp].underline = ctx->last_mosaic_separate;
            ctx->screen[yp][xp].is_mosaic = 1;
            ctx->screen[yp][xp].character = ctx->last_mosaic_char;
            break;
          case 0x1F:
            vtlop_mosaic_hold (ctx, 0);
            break;
          default:
            break;
          }
//                              vtlop_spacing_attr(ctx);//space or held mosaic...
      }
      else
        vtlop_char (ctx, d);
    }
    if (ctx->skip_lines & (1 << (ctx->pos_y + 1)))
      for (i = 0; i < 40; i++)
      {
        ctx->screen[ctx->pos_y + 1][i] = ctx->screen[ctx->pos_y][i];
        ctx->screen[ctx->pos_y + 1][i].character = 0;
        ctx->screen[ctx->pos_y + 1][i].is_mosaic = 0;
        ctx->screen[ctx->pos_y + 1][i].e_size = 0;
        ctx->screen[ctx->pos_y + 1][i].underline = 0;
        ctx->screen[ctx->pos_y + 1][i].italics = 0;
        ctx->screen[ctx->pos_y + 1][i].bold = 0;
        ctx->screen[ctx->pos_y + 1][i].charset = 0;
        ctx->screen[ctx->pos_y + 1][i].is_mosaic = 0;
      }
  }
}
