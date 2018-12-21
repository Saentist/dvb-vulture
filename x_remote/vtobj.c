#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "vtctx.h"
#include "utl.h"
extern int hamm24_r (uint8_t * p, int *err);
extern unsigned vtobjTripBits (unsigned trip, unsigned start, unsigned end);
extern int vtctx_reframe (unsigned *in_vec, unsigned *out_vec, int inbits,
                          int outbits, int insz);

static uint8_t vtobj_enh_row_addr (uint8_t address);
static void vtobj_flush_row (VtCtx * ctx, VtObjCtx * obj_ctx);
static void vtobj_init_pop (VtCtx * ctx, uint8_t loc, uint8_t s1);
static void vtobj_init_drcs (VtCtx * ctx, uint8_t loc, uint8_t s1);
static void vtobj_put_obj (VtCtx * ctx, VtObjCtx * prnt, uint8_t obj_type,
                           uint8_t * pg, int num_pk, int y, int dc, int trip);
static VtCell *vtobj_get_cell (VtCtx * ctx, VtObjCtx * obj, uint8_t x,
                               uint8_t y);
static uint8_t *vtobj_find_obj_pk (uint8_t * pg, int num_pk, int y, int dc);
static void vtobj_enhance (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                           uint8_t mode, uint8_t data);
static void vtobj_flush (VtCtx * ctx, VtObjCtx * obj_ctx, int column);
///invoke pop/gpop objects
static void vtobj_pop_obj (VtCtx * ctx, VtObjCtx * prnt, uint8_t obj_type,
                           uint8_t is_pop, uint16_t obj_n);

//-------------------------------Row address triplets---------------------------------------
static void
vtobj_row_fscolor (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address NOTUSED,
                   uint8_t data)
{
  if (obj_ctx->type < 2)
    ctx->default_screen_color = data;
}

static void
vtobj_row_color (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                 uint8_t data)
{
  uint8_t where;
  int row = vtobj_enh_row_addr (address);
  if (obj_ctx->type > 1)
    return;
  vtobj_flush_row (ctx, obj_ctx);
  obj_ctx->pos_y = row;
  obj_ctx->pos_x = 0;
  where = data >> 5;
  if (where == 0)
  {
//              int i;
    ctx->full_row_color[row] = data;
  }
  else if (where == 3)
  {
//              int i;
    for (; row < 26; row++)
      ctx->full_row_color[row] = data;
  }
}

static void
vtobj_row_pos (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address, uint8_t data)
{
  vtobj_flush_row (ctx, obj_ctx);
  obj_ctx->pos_y = vtobj_enh_row_addr (address);
  if (data < 40)
  {
    obj_ctx->pos_x = data;
  }
}

static void
vtobj_row_home (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                uint8_t data)
{
  uint8_t where;
  if (obj_ctx->type > 1)
    return;
  if (address != 63)
    return;
  vtobj_flush_row (ctx, obj_ctx);

  obj_ctx->pos_x = 0;
  obj_ctx->pos_y = 0;

  where = data >> 5;
  if (where == 0)
  {
//              int i;
    int row = 0;
    ctx->full_row_color[row] = data;
  }
  else if (where == 3)
  {
//              int i;
    int row = 0;
    for (; row < 26; row++)
      ctx->full_row_color[row] = data;
  }
}

static void
vtobj_row_origin (VtCtx * ctx NOTUSED, VtObjCtx * obj_ctx, uint8_t address,
                  uint8_t data)
{                               //origin modifier
  obj_ctx->next_org_x = (data < 72) ? data : 0;
  obj_ctx->next_org_y = address - 40;
}

static void
vtobj_row_object_invoke (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t mode,
                         uint8_t address, uint8_t data)
{
  uint16_t obj_ptr = data | (((uint16_t) address & 3) << 7);
  uint8_t obj_trip;
  uint8_t src = (address >> 3) & 3;
//      int y,dc;
  uint8_t obj_type = mode & 3;  //1: active, 2: adaptive, 3: passive, 0: local?

  if (src > 1)                  //pop or gpop
  {
    vtobj_pop_obj (ctx, obj_ctx, obj_type, 3 - src, obj_ptr);
  }
  else if (src == 1)            //local object
  {
    uint8_t dc = (obj_ptr >> 4) & 15;
    obj_trip = (obj_ptr & 15) + 1;
    if (obj_trip > 13)
      obj_trip = 13;
    vtobj_put_obj (ctx, obj_ctx, obj_type, ctx->pg, ctx->num_pk_pg, 26, dc,
                   obj_trip);
  }
}

static void
vtobj_row_object_define (VtCtx * ctx NOTUSED, VtObjCtx * obj_ctx NOTUSED, uint8_t mode NOTUSED,
                         uint8_t address NOTUSED, uint8_t data NOTUSED)
{

}

static void
vtobj_row_drcs_mode (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address NOTUSED,
                     uint8_t data)
{
  uint8_t src = (data >> 6) & 1;        //0: global 1: normal
  uint8_t level = (data >> 4) & 3;
  uint8_t s1 = data & 15;
  if (level > 1)
  {
    vtobj_init_drcs (ctx, src, s1);
  }
  if (src)
  {
    obj_ctx->drcs_nsub = s1;
  }
  else
  {
    obj_ctx->drcs_gsub = s1;
  }
}

static void
vtobj_row_terminate (VtCtx * ctx NOTUSED, VtObjCtx * obj_ctx NOTUSED, uint8_t address NOTUSED,
                     uint8_t data NOTUSED)
{

}

static void
vtobj_enh_row (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address, uint8_t mode,
               uint8_t data)
{
  mode &= 31;
  switch (mode)
  {
  case 0:
    if (!(data & 0x60))
      vtobj_row_fscolor (ctx, obj_ctx, address, data);
    break;
  case 1:
    if (!(data & 0x60))
      vtobj_row_color (ctx, obj_ctx, address, data);
    break;
  case 4:
    vtobj_row_pos (ctx, obj_ctx, address, data);
    break;
  case 7:
    vtobj_row_home (ctx, obj_ctx, address, data);
    break;
  case 16:
    vtobj_row_origin (ctx, obj_ctx, address, data);
    break;
  case 17:
  case 18:
  case 19:
    vtobj_row_object_invoke (ctx, obj_ctx, mode, address, data);
    break;
  case 21:
  case 22:
  case 23:
    vtobj_row_object_define (ctx, obj_ctx, mode, address, data);
    break;
  case 24:
    vtobj_row_drcs_mode (ctx, obj_ctx, address, data);
    break;
  case 31:
    vtobj_row_terminate (ctx, obj_ctx, address, data);
    break;
  default:
    break;
  }
}



///prnt may be NULL for local context
static void
vtobj_obj_init (VtCtx * ctx, VtObjCtx * obj_ctx, VtObjCtx * prnt,
                uint8_t obj_type, uint8_t * pg NOTUSED, int num_pk NOTUSED, int y NOTUSED, int dc NOTUSED,
                int trip NOTUSED)
{
  obj_ctx->fg_color_set = 0;
  obj_ctx->bg_color_set = 0;
//      obj_ctx->attr_set=0;
  obj_ctx->underline_set = 0;
  obj_ctx->flash_set = 0;
  obj_ctx->window_set = 0;
  obj_ctx->size_set = 0;
  obj_ctx->conceal_set = 0;
  obj_ctx->invert_set = 0;
  obj_ctx->org_x = 0;
  obj_ctx->org_y = 0;
  obj_ctx->pos_x = 0;
  obj_ctx->pos_y = 0;
  obj_ctx->mod_charset = ctx->default_charset;
  obj_ctx->next_org_x = 0;      ///<set by origin modifier
  obj_ctx->next_org_y = 0;
  obj_ctx->type = obj_type;     ///<0: local enh, 1: active, 2: adaptive, 3: passive
  obj_ctx->drcs_nsub = 0;
  obj_ctx->drcs_gsub = 0;

  obj_ctx->character = ' ';
  obj_ctx->diacrit = 0;
  obj_ctx->at_char = 0;
  obj_ctx->is_mosaic = 0;
  obj_ctx->is_drcs = 0;
  obj_ctx->drcs_normal = 0;

  switch (obj_type)
  {
  case 1:                      //active
  case 2:                      //adaptive
  default:
    obj_ctx->org_x = prnt->pos_x + prnt->next_org_x;
    obj_ctx->org_y = prnt->pos_y + prnt->next_org_y;
    obj_ctx->fg_color = prnt->fg_color;
    obj_ctx->bg_color = prnt->bg_color;
    obj_ctx->flash_mode = prnt->flash_mode;
    obj_ctx->flash_rate = prnt->flash_rate;
    obj_ctx->e_size = prnt->e_size;
//              obj_ctx->flash=prnt->flash;
    obj_ctx->window = prnt->window;
    obj_ctx->is_mosaic = prnt->is_mosaic;
    obj_ctx->conceal = prnt->conceal;
//              obj_ctx->mosaics_separate=prnt->mosaics_separate;
//              vtobj_get_obj_bound(pg,num_pk,y,dc,start_trip,&obj_ctx->max_x,&obj_ctx->max_y);
    break;
  case 3:                      //passive
    obj_ctx->org_x = prnt->pos_x + prnt->next_org_x;
    obj_ctx->org_y = prnt->pos_y + prnt->next_org_y;
  case 0:                      //local
    obj_ctx->fg_color_set = 1;
    obj_ctx->bg_color_set = 1;
    obj_ctx->fg_color = 7;
    obj_ctx->bg_color = 0;

//              obj_ctx->attr_set=1;
    obj_ctx->underline_set = 1;
    obj_ctx->underline = 0;
    obj_ctx->italic = 0;
    obj_ctx->bold = 0;

    obj_ctx->flash_set = 1;
    obj_ctx->flash_mode = 0;
    obj_ctx->flash_rate = 0;

    obj_ctx->window_set = 1;
    obj_ctx->window = 0;

    obj_ctx->size_set = 1;
    obj_ctx->e_size = 0;

    obj_ctx->conceal_set = 1;
    obj_ctx->conceal = 0;

    obj_ctx->invert_set = 1;
    obj_ctx->invert = 0;
//              obj_ctx->flash=0;
    obj_ctx->is_mosaic = 1;
//              obj_ctx->mosaics_separate=0;
    break;
  }
}

static int
vtobj_next_obj_pk (int *y_p, int *dc_p)
{
  int y = *y_p, dc = *dc_p;
  if (y == 25)
  {
    y = 26;
    dc = 0;
  }
  else if (y == 26)
  {
    if (dc >= 15)               //this is bad
      return 1;
    dc++;
  }
  else if (y < 25)
  {
    y++;
  }
  else                          //this is bad
    return 1;
  *y_p = y;
  *dc_p = dc;
  return 0;
}

static void
vtobj_put_obj (VtCtx * ctx, VtObjCtx * prnt, uint8_t obj_type, uint8_t * pg,
               int num_pk, int y, int dc, int trip)
{
  uint8_t *pk = vtobj_find_obj_pk (pg, num_pk, y, dc);
  int first = 1;
  VtObjCtx obj_ctx;
  switch (prnt->type & 3)
  {
  case 1:
    if (obj_type <= 1)
      return;
    break;
  case 2:
    if (obj_type <= 2)
      return;
    break;
  case 3:
    return;
  case 0:
    break;
  }

  if (!pk)
    return;
  vtobj_obj_init (ctx, &obj_ctx, prnt, obj_type, pg, num_pk, y, dc, trip);
  do
  {
    uint8_t *d = VT_PK_TRIP (pk, trip);
    int x;
    int err;
    unsigned address, mode, data;
    x = hamm24_r (d, &err);
    if (!(err & 0xf000))
    {
      address = vtobjTripBits (x, 1, 6);
      mode = vtobjTripBits (x, 7, 11);
      data = vtobjTripBits (x, 12, 18);
      if (0 == first)
        if ((address >= 40)
            && ((mode == 31) || (mode == 21) || (mode == 22) || (mode == 23)))
        {
          vtobj_flush_row (ctx, &obj_ctx);
          return;               //termination
        }
      vtobj_enhance (ctx, &obj_ctx, address, mode, data);
    }
    else
      return;
    first = 0;
    trip++;
    if (trip > 13)
    {
      if (vtobj_next_obj_pk (&y, &dc))
        return;
      pk = vtobj_find_obj_pk (pg, num_pk, y, dc);
      if (NULL == pk)
        return;
      trip = 1;
    }
  }
  while (1);
}

//-------------------------------------Column address triplets-----------------------------------------

static void
vtobj_col_compose (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                   uint8_t data, uint8_t mode)
{
  vtobj_flush (ctx, obj_ctx, address);
  if ((data == 0x2a) && (mode == 16))
    obj_ctx->at_char = 1;
  mode &= 15;
  obj_ctx->diacrit = mode;
  obj_ctx->character = data;
  obj_ctx->char_set = 1;
}

static void
vtobj_col_set_fg (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                  uint8_t color)
{
//      int i;
  if (color & 0x60)
    return;
  vtobj_flush (ctx, obj_ctx, address);
  obj_ctx->fg_color_set = 1;
  obj_ctx->fg_color = color;
}

static void
vtobj_col_block (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                 uint8_t data)
{
//      int i;
  vtobj_flush (ctx, obj_ctx, address);
  if (data >= 32)
  {
    obj_ctx->is_mosaic = 1;
    obj_ctx->is_drcs = 0;
    obj_ctx->at_char = 0;
    obj_ctx->character = data;
    obj_ctx->char_set = 1;
  }
}

static void
vtobj_col_smooth (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                  uint8_t data)
{
//      int i;
  vtobj_flush (ctx, obj_ctx, address);
  if (data >= 32)
  {
    obj_ctx->is_mosaic = 3;
    obj_ctx->is_drcs = 0;
    obj_ctx->at_char = 0;
    obj_ctx->character = data;
    obj_ctx->char_set = 1;
  }
}

static void
vtobj_col_set_bg (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                  uint8_t color)
{
//      int i;
  if (color & 0x60)
    return;
  vtobj_flush (ctx, obj_ctx, address);
  obj_ctx->bg_color_set = 1;
  obj_ctx->bg_color = color;
}

static void
vtobj_col_flash (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                 uint8_t phase, uint8_t mode)
{
//      int i;
//      int pctr;
  vtobj_flush (ctx, obj_ctx, address);
  obj_ctx->flash_set = 1;
  obj_ctx->phase_ctr = 1;
  obj_ctx->flash_mode = mode;
  obj_ctx->flash_rate = phase;
}

static void
vtobj_col_charset (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                   uint8_t data)
{
//      int i;
  vtobj_flush (ctx, obj_ctx, address);
  obj_ctx->mod_charset = data;
//      obj_ctx->mod_charset_set=1;
}

static void
vtobj_col_char_g0 (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                   uint8_t data)
{
  vtobj_flush (ctx, obj_ctx, address);
  if (data >= 32)
  {
    obj_ctx->is_mosaic = 0;
    obj_ctx->is_drcs = 0;
    obj_ctx->at_char = 0;
    obj_ctx->char_set = 1;
    obj_ctx->character = data;
  }
}

static void
vtobj_col_char_smooth (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                       uint8_t data)
{
  vtobj_flush (ctx, obj_ctx, address);
  if (data >= 32)
  {
    obj_ctx->is_mosaic = 3;
    obj_ctx->is_drcs = 0;
    obj_ctx->at_char = 0;
    obj_ctx->char_set = 1;
    obj_ctx->character = data;
  }
}

static void
vtobj_col_attr (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                uint8_t data)
{
//      int i;
  vtobj_flush (ctx, obj_ctx, address);
  obj_ctx->e_size = ((data >> 5) & 2) | (data & 1);
  obj_ctx->size_set = 1;
  obj_ctx->underline = (data >> 5) & 1;
//      obj_ctx->attr_set=1;
  obj_ctx->underline_set = 1;
  obj_ctx->invert = (data >> 4) & 1;
  obj_ctx->invert_set = 1;
  obj_ctx->conceal = (data >> 2) & 1;
  obj_ctx->conceal_set = 1;
  obj_ctx->window = (data >> 1) & 1;
  obj_ctx->window_set = 1;
}

static void
vtobj_col_drcs (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                uint8_t data)
{
  vtobj_flush (ctx, obj_ctx, address);
  obj_ctx->is_mosaic = 0;
  obj_ctx->is_drcs = 1;
  obj_ctx->drcs_normal = (data & (1 << 6)) ? 1 : 0;
  obj_ctx->at_char = 0;
  obj_ctx->char_set = 1;
  obj_ctx->character = data % 48;
  vtobj_init_drcs (ctx, obj_ctx->drcs_normal, obj_ctx->drcs_normal ? obj_ctx->drcs_nsub : obj_ctx->drcs_gsub);  //there may not be a drcs designation active, defaults to subtable 0
}

#define MINIMUM(_A,_B) (((_A)<(_B))?(_A):(_B))

static void
vtobj_col_style (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                 uint8_t data)
{
  int i, j;
  obj_ctx->pos_x = address;
  for (j = obj_ctx->pos_y; j <= MINIMUM ((obj_ctx->pos_y + (data >> 4)), 25);
       j++)
  {
    for (i = obj_ctx->pos_x; i <= 71; i++)
    {
      VtCell *cell;
      cell = vtobj_get_cell (ctx, obj_ctx, i, j);
      cell->italics = (data >> 2) & 1;
      cell->bold = (data >> 1) & 1;
    }
  }
}

static void
vtobj_col_char_g2 (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address,
                   uint8_t data)
{
  vtobj_flush (ctx, obj_ctx, address);
  if (data >= 32)
  {
    obj_ctx->is_mosaic = 2;
    obj_ctx->is_drcs = 0;
    obj_ctx->drcs_normal = (data & (1 << 6)) ? 1 : 0;
    obj_ctx->at_char = 0;
    obj_ctx->char_set = 1;
    obj_ctx->character = data;
  }
}

static void
vtobj_enh_col (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address, uint8_t mode,
               uint8_t data)
{
  mode &= 31;
  if ((mode >= 16) && (mode <= 31))
  {
    vtobj_col_compose (ctx, obj_ctx, address, data, mode);
  }
  else
    switch (mode & 15)
    {
    case 0:
      if (!(data & 0x60))
        vtobj_col_set_fg (ctx, obj_ctx, address, data);
      break;
    case 1:
      vtobj_col_block (ctx, obj_ctx, address, data);
      break;
    case 2:
      vtobj_col_smooth (ctx, obj_ctx, address, data);
      break;
    case 3:
      if (!(data & 0x60))
        vtobj_col_set_bg (ctx, obj_ctx, address, data);
      break;
    case 7:
      if (!(data & 0x60))
        vtobj_col_flash (ctx, obj_ctx, address, (data >> 2), data & 3);
      break;
    case 8:
      vtobj_col_charset (ctx, obj_ctx, address, data);
      break;
    case 9:
      vtobj_col_char_g0 (ctx, obj_ctx, address, data);
      break;
    case 11:
      vtobj_col_char_smooth (ctx, obj_ctx, address, data);
      break;
    case 12:
      vtobj_col_attr (ctx, obj_ctx, address, data);
      break;
    case 13:
      vtobj_col_drcs (ctx, obj_ctx, address, data);
      break;
    case 14:
      vtobj_col_style (ctx, obj_ctx, address, data);    //oh no ..more bitmaps.. can we generate those automated with a dilation kernel and transformations?
      break;
    case 15:
      vtobj_col_char_g2 (ctx, obj_ctx, address, data);
      break;
    default:
      break;
    }
}

//------------------------------------misc funcs-----------------------------------------------------

static int
vtobj_parse_drcs (VtCtx * ctx, uint8_t * pg, int num_pk_pg, int loc)
{
  int i;
  int s1 = 0;                   //TODO: is this a BUG?
  for (i = 0; i < num_pk_pg; i++)
  {
    int y;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if (y == 0)
    {
      s1 = hamm84_r (*VT_PK_BYTE (pk, 8));
      if ((s1 == 0xff) || (y == 0xff))
        return 1;
    }
  }
  for (i = 0; i < num_pk_pg; i++)
  {
    int y;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if ((y > 0) && (y <= 24))
    {
      int idx = (y - 1) * 2;
      memmove (ctx->drcs[loc][s1][idx].data, VT_PK_BYTE (pk, 6), 20);
      ctx->drcs[loc][s1][idx + 1].mode = 0;
      memmove (ctx->drcs[loc][s1][idx + 1].data, VT_PK_BYTE (pk, 6 + 20), 20);
      ctx->drcs[loc][s1][idx + 1].mode = 0;
    }
  }
  for (i = 0; i < num_pk_pg; i++)
  {
    int y, dc;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if (y == 28)
    {
      dc = get_pk_ext (pk);
      if (dc == 3)
      {
        int j;
        int err;
        unsigned a[11];
        unsigned b[50];
        uint8_t *p = VT_PK_BYTE (pk, 10);
        for (j = 0; j < 11; j++)
          a[j] = hamm24_r (p + 3 * j, &err);
        a[10] &= (1 << 12) - 1;
        vtctx_reframe (a, b, 18, 4, 11);
        for (j = 0; j < 48; j++)
          ctx->drcs[loc][s1][j].mode = b[j];
      }
    }
  }
  return 0;
}

static void
vtobj_init_drcs (VtCtx * ctx, uint8_t loc, uint8_t s1)
{
  uint8_t *pg;
  int num_pk;
  if (ctx->drcs_loaded & (1 << (s1 + (loc * 16))))
  {
    return;
  }
  if (loc)
  {
    if (ctx->drcs_link.present)
    {
      int magno = ctx->magno ^ ctx->drcs_link.magrel;
      pg =
        ctx->get_dpg_cb (ctx->d, magno,
                         (ctx->drcs_link.pgnt << 4) | ctx->drcs_link.pgnu, s1,
                         &num_pk);
      vtobj_parse_drcs (ctx, pg, num_pk, loc);
      free (pg);
      ctx->drcs_loaded |= (1 << (s1 + (loc * 16)));
    }
  }
  else
  {
    if (ctx->gdrcs_link.present)
    {
      int magno = ctx->magno ^ ctx->gdrcs_link.magrel;
      pg =
        ctx->get_dpg_cb (ctx->d, magno,
                         (ctx->gdrcs_link.pgnt << 4) | ctx->gdrcs_link.pgnu,
                         s1, &num_pk);
      vtobj_parse_drcs (ctx, pg, num_pk, loc);
      free (pg);
      ctx->drcs_loaded |= (1 << (s1 + (loc * 16)));
    }
  }
}

static uint8_t *
vtobj_find_obj_pk (uint8_t * pg, int num_pk, int y, int dc)
{
  int yf, i, dcf;
  for (i = 0; i < num_pk; i++)
  {
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    yf = get_pk_y (pk);
    if ((y == 26) && (yf == y))
    {
      dcf = get_pk_ext (pk);
      if (dcf == dc)
      {
        return pk;
      }
    }
    else if ((y < 26) && (yf == y))
    {
      return pk;
    }
  }
  return NULL;
}

static void
vtobj_enhance (VtCtx * ctx, VtObjCtx * obj_ctx, uint8_t address, uint8_t mode,
               uint8_t data)
{
  if (address < 40)
  {
    vtobj_enh_col (ctx, obj_ctx, address, mode, data);
  }
  else
  {
    vtobj_enh_row (ctx, obj_ctx, address, mode, data);
  }
}

#define OBJECT_TYPE_ACTIVE 1
#define OBJECT_TYPE_ADAPTIVE 2
#define OBJECT_TYPE_PASSIVE 3
#define ROWS 25
#define COLUMNS 72
static void
vtobj_flush (VtCtx * ctx, VtObjCtx * obj_ctx, int column)
{
  int row = obj_ctx->org_y + obj_ctx->pos_y;
  int i;

  if (row >= ROWS)
    return;

  if (obj_ctx->type == OBJECT_TYPE_PASSIVE && !obj_ctx->char_set)       //&& !mac.unicode)
  {
    obj_ctx->pos_x = column;
    return;
  }

  for (i = obj_ctx->org_x + obj_ctx->pos_x; i < obj_ctx->org_x + column;)
  {
    VtCell c;

    if (i > 71)
      break;

    c = ctx->screen[row][i];
    if ((obj_ctx->type != OBJECT_TYPE_PASSIVE) || obj_ctx->char_set)
    {
      if (obj_ctx->invert_set)
      {
        c.invert = obj_ctx->invert;
      }
      if (obj_ctx->underline_set)
      {
        c.underline = obj_ctx->underline;
      }
      if (obj_ctx->char_set)
      {
        c.ext_char = 1;
        c.character = obj_ctx->character;
        c.charset = obj_ctx->mod_charset;
        c.diacrit = obj_ctx->diacrit;
        c.at_char = obj_ctx->at_char;
        c.is_mosaic = obj_ctx->is_mosaic;
        c.is_drcs = obj_ctx->is_drcs;
        c.drcs_normal = obj_ctx->drcs_normal;
        if (c.drcs_normal)
          c.drcs_sub = obj_ctx->drcs_nsub;
        else
          c.drcs_sub = obj_ctx->drcs_gsub;
        obj_ctx->char_set = 0;
      }
      if (obj_ctx->size_set)
      {
        c.e_size = obj_ctx->e_size;
      }
      if (obj_ctx->window_set)
      {
        c.window = obj_ctx->window;
      }
      if (obj_ctx->conceal_set)
      {
        c.conceal = obj_ctx->conceal;
      }

      if (obj_ctx->flash_set)
      {
        //phase: 1..3
        static const int next_phase[4] = { 0, 2, 3, 1 };
        static const int prev_phase[4] = { 0, 3, 2, 1 };
        c.flash_mode = obj_ctx->flash_mode;
        c.flash_rate = obj_ctx->phase_ctr;
        if (obj_ctx->flash_rate > 3)
        {
          if (obj_ctx->flash_rate == 4)
            obj_ctx->phase_ctr = next_phase[obj_ctx->phase_ctr];
          else if (obj_ctx->flash_rate == 5)
            obj_ctx->phase_ctr = prev_phase[obj_ctx->phase_ctr];
        }
      }
      if (obj_ctx->fg_color_set)
      {
        c.fg_color = obj_ctx->fg_color;
      }
      if (obj_ctx->bg_color_set)
      {
        c.bg_color = obj_ctx->bg_color;
      }
    }
    ctx->screen[row][i] = c;
    //        acp[i] = c;
    if (obj_ctx->type == OBJECT_TYPE_PASSIVE)
    {
      break;
    }
    i++;
    if (obj_ctx->type != OBJECT_TYPE_PASSIVE
        && obj_ctx->type != OBJECT_TYPE_ADAPTIVE)
    {
      int raw;
//                      if(i=0)
      if (i == 0)
        raw = 0x0;
      else
        raw = (row == 0 && i < 9) ? 0x0 : ctx->screen[row][i - 1].attr;
      if (raw == 0x00)
        raw = 0x20;
      else
        raw--;

      /* set-after spacing attributes cancelling non-spacing */

      switch (raw)
      {
      case 0x00 ... 0x07:      /* alpha + foreground color */
      case 0x10 ... 0x17:      /* mosaic + foreground color */
        //                    printv("... fg term %d %02x\n", i, raw);
        obj_ctx->fg_color_set = 0;
        //                    mac.foreground = 0;
        //                    mac.conceal = 0;
        break;

      case 0x08:               /* flash */
        obj_ctx->flash_set = 0;
        //                    mac.flash = 0;
        break;

      case 0x0A:               /* end box */
      case 0x0B:               /* start box */
        if (i < COLUMNS && (ctx->screen[row][i].attr - 1) == raw)
        {
          obj_ctx->window_set = 0;
          //                          printv("... boxed term %d %02x\n", i, raw);
          //                          mac.opacity = 0;
        }

        break;

      case 0x0D:               /* double height */
      case 0x0E:               /* double width */
      case 0x0F:               /* double size */
        //                  printv("... size term %d %02x\n", i, raw);
        obj_ctx->size_set = 0;
        //                    mac.size = 0;
        break;
      }

      if (i > 71)
        break;

//                      if(i=0)
//                      if(i==0)
      //                      raw=0x0;
      //      else
      raw = (row == 0 && i < 8) ? 0x0 : ctx->screen[row][i].attr;
      if (raw == 0x00)
        raw = 0x20;
      else
        raw--;

      /* set-at spacing attributes cancelling non-spacing */

      switch (raw)
      {
      case 0x09:               /* steady */
        obj_ctx->flash_set = 0;
        break;

      case 0x0C:               /* normal size */
//                              printv("... size term %d %02x\n", i, raw);
        obj_ctx->size_set = 0;
        break;

      case 0x18:               /* conceal */
        obj_ctx->conceal_set = 0;
        //                    mac.conceal = 0;
        break;

        /*
         *  Non-spacing underlined/separated display attribute
         *  cannot be cancelled by a subsequent spacing attribute.
         */

      case 0x1C:               /* black background */
      case 0x1D:               /* new background */
        //                    printv("... bg term %d %02x\n", i, raw);
        obj_ctx->bg_color_set = 0;
        break;
      }
    }
  }
  obj_ctx->pos_x = column;
}

static void
vtobj_flush_row (VtCtx * ctx, VtObjCtx * obj_ctx)
{
  if (obj_ctx->type == OBJECT_TYPE_PASSIVE
      || obj_ctx->type == OBJECT_TYPE_ADAPTIVE)
    vtobj_flush (ctx, obj_ctx, obj_ctx->pos_x + 1);
  else
    vtobj_flush (ctx, obj_ctx, COLUMNS);

  if (obj_ctx->type != OBJECT_TYPE_ADAPTIVE)
  {
    obj_ctx->mod_charset = ctx->default_charset;
  }

  if (obj_ctx->type != OBJECT_TYPE_PASSIVE)
  {
    obj_ctx->fg_color_set = 0;
    obj_ctx->bg_color_set = 0;
//              obj_ctx->attr_set=0;
    obj_ctx->underline_set = 0;
    obj_ctx->char_set = 0;
    obj_ctx->flash_set = 0;
    obj_ctx->invert_set = 0;
    obj_ctx->window_set = 0;
    obj_ctx->size_set = 0;
    obj_ctx->conceal_set = 0;
  }
}

//get objects' boundaries without origin modifier or active position
/*static void vtobj_get_obj_bound(uint8_t * pg,int num_pk,int y,int dc,int trip,int *max_col,int * max_row)
{
	uint8_t * cur_pk;
	uint8_t * cur_trip;
	int mx_col=0;
	int mx_row=0;
	int trip;
	int err;
	unsigned address,mode,data;
	cur_pk=vtobj_find_obj_pk(pg,num_pk,y,dc);
	while(start_pk<num_pk)
	{
		while(start_trip<14)
		{
			cur_trip=VT_PK_TRIP(cur_pk,start_trip);
			start_trip++;
			trip=hamm24_r(cur_trip, &err);
			if(!err)
			{
				address=vtobjTripBits(trip,1,6);
				mode=vtobjTripBits(trip,7,11);
				data=vtobjTripBits(trip,12,18);
				if(address<40)
				{
					switch(mode)
					{
						case 4:
						case 5:
						case 6:
						case 10:
						//not modified
						break;
						default:
						if(address>mx_col)
							mx_col=address;
						break;
					}
				}
				else
				{
					int row;
					switch(mode)
					{
						case 31:
						//termination
						*max_col=mx_col;
						*max_row=mx_row;
						return;
						break;
						case 1:
						//full row color
						if(address==40)
							row=24;
						else
							row=address-40;
						if(row>mx_row)
							mx_row=row;
						if(0>mx_col)
							mx_col=0;
						break;
						case 4:
						//set act pos
						if(address==40)
							row=24;
						else
							row=address-40;
						if(row>mx_row)
							mx_row=row;
						if(data<40)
						{
							if(data>mx_col)
								mx_col=data;
						}
						break;
						case 7:
						//address row 0
						if(address==63)
						{
							if(0>mx_row)
								mx_row=0;
							if(0>mx_col)
								mx_col=0;
						}
						break;
						default:
						break;
					}
				}
			}
		}
		if(vtobj_next_obj_pk(&y,&dc))
		{
			*max_col=mx_col;
			*max_row=mx_row;
			return;
		}
		cur_pk=vtobj_find_obj_pk(pg,num_pk,y,dc);
		if(NULL==cur_pk)
		{
			*max_col=mx_col;
			*max_row=mx_row;
			return;
		}
		trip=1;
	}
	*max_col=mx_col;
	*max_row=mx_row;
	return;
}
*/
static uint8_t
vtobj_enh_row_addr (uint8_t address)
{
  return (address == 40) ? 24 : (address - 40);
}

static VtCell *
vtobj_get_cell (VtCtx * ctx, VtObjCtx * obj, uint8_t x, uint8_t y)
{
  unsigned cx, cy;
  cx = obj->org_x + x;
  cy = obj->org_y + y;
  if (cx > 71)
    cx = 71;
  if (cy > 25)
    cy = 25;
  return &ctx->screen[cy][cx];
}

static void
vtobj_init_pop (VtCtx * ctx, uint8_t loc, uint8_t s1)
{
//      uint8_t * pg;
//      int num_pk;
  if (NULL != ctx->pop_pg[loc][s1])
  {
    return;
  }
  if (loc)
  {
    if (ctx->pop_link.present)
    {
      int magno = ctx->magno ^ ctx->pop_link.magrel;
      ctx->pop_pg[loc][s1] =
        ctx->get_dpg_cb (ctx->d, magno,
                         (ctx->pop_link.pgnt << 4) | ctx->pop_link.pgnu, s1,
                         &ctx->num_pop_pk[loc][s1]);
//                      ctx->drcs_loaded|=(1<<(s1+(loc*16)));
    }
  }
  else
  {
    if (ctx->gpop_link.present)
    {
      int magno = ctx->magno ^ ctx->gpop_link.magrel;
      ctx->pop_pg[loc][s1] =
        ctx->get_dpg_cb (ctx->d, magno,
                         (ctx->gpop_link.pgnt << 4) | ctx->gpop_link.pgnu, s1,
                         &ctx->num_pop_pk[loc][s1]);
//                      ctx->drcs_loaded|=(1<<(s1+(loc*16)));
    }
  }
}

//----------------------------------------------------------Interface-------------------------------------------------------

///return selected bits in triplet. one based bit indices.
unsigned
vtobjTripBits (unsigned trip, unsigned start, unsigned end)
{
  if (start > end)
  {
//              errMsg("start>end\n");
    return 0;
  }
  return (trip >> (start - 1)) & ((1 << (end - start + 1)) - 1);
}

///return 1 if page has x26 packets
static int
vtobj_has26 (uint8_t * pg, int num_pk_pg)
{
  int i;
  for (i = 0; i < num_pk_pg; i++)
  {
    int y;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if (y == 26)
      return 1;
  }
  return 0;
}

///invoke pop/gpop objects
static void
vtobj_pop_obj (VtCtx * ctx, VtObjCtx * prnt, uint8_t obj_type, uint8_t is_pop,
               uint16_t obj_n)
{
  uint8_t *pg;
  uint8_t *ptr_pk;
  uint8_t *ptr_trip;
  int num_pk, err;
  int obj_ptr2, packet_idx, obj_trip;
  uint32_t dta;
  uint8_t y, s1, dc, ploc, trip, pp;

  is_pop = ! !is_pop;

  s1 = obj_n & 0xf;
  ploc = (obj_n >> 7) & 3;
  pp = (obj_n >> 4) & 1;
  trip = ((obj_n >> 5) & 3) * 3 + obj_type;     //active triplet(pointer)
  vtobj_init_pop (ctx, is_pop, s1);
  pg = ctx->pop_pg[is_pop][s1];
  if (NULL == pg)               //nothing here
    return;
  num_pk = ctx->num_pop_pk[is_pop][s1];
  if (0 == num_pk)
    return;
  ptr_pk = vtobj_find_obj_pk (pg, num_pk, ploc + 1, 0);
  if (NULL == ptr_pk)
    return;
  ptr_trip = VT_PK_TRIP (ptr_pk, trip + 1);

  dta = hamm24_r (ptr_trip, &err);
  if (!(err & 0xf000))
  {
    if (pp)
    {
      obj_ptr2 = vtobjTripBits (dta, 10, 18);
    }
    else
    {
      obj_ptr2 = vtobjTripBits (dta, 1, 9);
    }
    if (obj_ptr2 <= 506)        //last trip in last packet(X/26/15) of page
    {
      packet_idx = 3 + obj_ptr2 / 13;
      obj_trip = 1 + (obj_ptr2 % 13);
      if (packet_idx < 26)
      {
        y = packet_idx;
        dc = 0;
        vtobj_put_obj (ctx, prnt, obj_type, pg, num_pk, y, dc, obj_trip);
      }
      else
      {
        y = 26;
        dc = 3 + obj_ptr2 / 13 - 26;
        vtobj_put_obj (ctx, prnt, obj_type, pg, num_pk, y, dc, obj_trip);
      }
    }
  }
}

///invoke default objects glob1,glob2,loc1,loc2
static void
vtobj_defaults (VtCtx * ctx, VtObjCtx * prnt)
{
  int obj_type;

  if (ctx->def_gobj_flags & 3)
  {
    obj_type = ctx->def_gobj_flags & 3;
    vtobj_pop_obj (ctx, prnt, obj_type, 0, ctx->def_gobj[0]);
  }

  if ((ctx->def_gobj_flags >> 2) & 3)
  {
    obj_type = (ctx->def_gobj_flags >> 2) & 3;
    vtobj_pop_obj (ctx, prnt, obj_type, 0, ctx->def_gobj[1]);
  }

  if (ctx->def_obj_flags & 3)
  {
    obj_type = ctx->def_obj_flags & 3;
    vtobj_pop_obj (ctx, prnt, obj_type, 1, ctx->def_obj[0]);
  }

  if ((ctx->def_obj_flags >> 2) & 3)
  {
    obj_type = (ctx->def_obj_flags >> 2) & 3;
    vtobj_pop_obj (ctx, prnt, obj_type, 1, ctx->def_obj[1]);
  }
}

///interpret a pages x26 packets
void
vtobjParseX26 (VtCtx * ctx, uint8_t * pg, int num_pk_pg)
{
  int i;
  VtObjCtx obj_ctx;
  vtobj_obj_init (ctx, &obj_ctx, NULL, 0, pg, num_pk_pg, 26, 0, 1);

  if (!vtobj_has26 (pg, num_pk_pg))
  {
    vtobj_defaults (ctx, &obj_ctx);
  }

  for (i = 0; i < num_pk_pg; i++)
  {
    int y;                      //,dc;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if (y == 26)
    {
      for (i = ENH_DATA_START; i < VT_UNIT_LEN; i += 3)
      {
        int trip;
        int err;
        unsigned address, mode, data;
        err = 0;
        trip = hamm24_r (pk + i, &err);
        if (!(err & 0xf000))
        {
          address = vtobjTripBits (trip, 1, 6);
          mode = vtobjTripBits (trip, 7, 11);
          data = vtobjTripBits (trip, 12, 18);
          if ((address >= 40) && (mode == 31))
          {
            vtobj_flush_row (ctx, &obj_ctx);
            return;             //termination
          }
          vtobj_enhance (ctx, &obj_ctx, address, mode, data);
        }
      }
    }
  }
}
