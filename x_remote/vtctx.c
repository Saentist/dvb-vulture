#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "vtctx.h"
#include "vtobj.h"
#include "utl.h"
//contains a lot of questionable bit-frobbing... possibly brokenz.

/**
 *\brief blink patterns
 *
 * 12 bits used in each 16 bit word<br>
 * 1/12 sec/bit<br>
 * bit pattern in comment is depicted least significant bit first
 */
static uint16_t VtCtxFlash[] = {
  0x003f,                       //normal 50% 1 HZ flash 111111000000
  0x01c7,                       //fast phase 1 111000111000
  0x071c,                       //fast phase 2 001110001110
  0x0c71,                       //fast phase 3 100011100011
};

///default color map for 2 bit drcs character
static uint8_t VtCtxDclut4[4] = { 0, 1, 2, 3 };

///default color map for 4 bit drcs character
static uint8_t VtCtxDclut16[16] =
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

static void
vtctx_set_default_charset (VtCtx * ctx, int x)
{
  ctx->default_charset = x;
}

static void
vtctx_set_second_charset (VtCtx * ctx, int x)
{
  ctx->sld = x;
}

int
hamm24_r (uint8_t * p, int *err)
{                               //why does this give errors???
  uint8_t tmp[3];
  *err = 0;
  tmp[0] = bitrev (p[0]);
  tmp[1] = bitrev (p[1]);
  tmp[2] = bitrev (p[2]);
  return hamm24 (tmp, err);
}

#define vtctx_reframe_out_sz(_INSZ,_INBITS,_OUTBITS) (((_INSZ)*(_INBITS)+(_OUTBITS)-1)/(_OUTBITS))
/**
 *\brief reframe a data array
 *
 * read insz elements.<br>
 * interpret the inbits least significant bits of the unsigned integer and<br>
 * output (insz*inbits+outbits-1)/outbits elements with outbits bits each<br>
 *
 *\param inbits number of bits to interpret in input elements. <br>
 *should be smaller than sizeof(unsigned)*8
 *\param outbits number of bits to write in output elements. <br>
 *should be smaller than sizeof(unsigned)*8
 *\param in_vec input elements
 *\param out_vec output vector must be at least (insz*inbits+outbits-1)/outbits elements (last element may get clobbered)
 *\param insz number of in_vec elements
 */
int
vtctx_reframe (unsigned *in_vec, unsigned *out_vec, int inbits, int outbits,
               int insz)
{
  int i;
  int outsz = (insz * inbits + outbits - 1) / outbits;
  int outidx = 0, outpos = 0, inidx = 0, inpos = 0;
  unsigned out_msk = (1 << outbits) - 1;
//      unsigned in_msk=(1<<inbits)-1;
  if ((0 == inbits) || (0 == outbits) || (0 == insz) || (NULL == in_vec)
      || (NULL == out_vec))
    return 1;
  if (inbits > outbits)
  {
//              out_vec[0]=0;
    for (i = 0; i < outsz; i++)
    {
      out_vec[i] = in_vec[inidx] >> inpos;
      if ((inpos + outbits) >= inbits)
      {
        inidx++;
        if (inidx < insz)
          out_vec[i] |= (in_vec[inidx] << (inbits - inpos));
        inpos += outbits;
        inpos -= inbits;
      }
      else
        inpos += outbits;

      out_vec[i] &= out_msk;
    }
  }
  else
  {
    out_vec[outidx] = 0;
    for (i = 0; i < insz; i++)
    {
      out_vec[outidx] |= in_vec[i] << outpos;
      if ((outpos + inbits) >= outbits)
      {
        out_vec[outidx] &= out_msk;
        outidx++;
        if (outidx < outsz)
          out_vec[outidx] = in_vec[i] >> (outbits - outpos);
        outpos += inbits;
        outpos -= outbits;
      }
      else
        outpos += inbits;
    }
    out_vec[outsz - 1] &= out_msk;
  }
  return 0;
}

uint8_t
vtctx_get5bit (uint8_t * d, int start_bit)
{
/*
	000001111122222333 334444455555
                |
*/
  int start_triplet = start_bit / 18;
  int start_idx = start_bit % 18;
  int trip, trip2;
  int err;
  if (start_idx > (18 - 5))
  {
    trip = hamm24_r (d + start_triplet * 3, &err);
    trip2 = hamm24_r (d + (start_triplet + 1) * 3, &err);
    return vtobjTripBits (trip, start_idx + 1,
                          18) | (vtobjTripBits (trip2, 1,
                                                5 - (18 - start_idx)) << (18 -
                                                                          start_idx));
  }
  else
  {
    trip = hamm24_r (d + start_triplet * 3, &err);
    return vtobjTripBits (trip, start_idx + 1, start_idx + 5);
  }
}

static void
vtctx_get_dclut16 (uint8_t * lut, uint8_t * d, int start_bit)
{
  int i;
  for (i = 0; i < 16; i++)
  {
    lut[i] = vtctx_get5bit (d, start_bit + i * 5);
  }
}

static int
vtctx_parse_m29_4 (VtCtx * ctx, uint8_t * pk)
{
  int trip;
//      uint8_t dc;
  int err;
  int i, j, k;
  int x;
//      int cmp;
  unsigned in_buf[10];
  unsigned out_buf[vtctx_reframe_out_sz (10, 18, 4) + 3];

//      uint8_t nib_buf[2*8*3];
//      dc=hamm84_r(*(pk+VT_DATA_START));
  trip = hamm24_r (pk + VT_DATA_START + 1, &err);
  /*
     trip 1 bits 1..7 are 0
     1:8..14 G0 and G2 charset
     1:15..18 second G0 and nat opt
     2:1..3
     2:4 left side panel available
     2:5 right side panel available
     2:6 side panel status 0: 3.5 only 1:2.5 and 3.5
     2:7-10 number of cols in left sidepanel
     2:11-18
     3-12:1-18
     13:1-4 CLUTs 2 and 3 RRRRGGGGBBBB
     13:5-9 default screen colour
     13:10-14 default Row colour
     13:15 black bg color substitution
     13:16-18 colour table remapping
     Where packets 28/0 and 28/4 are both transmitted as part of a page, packet 28/0 takes precedence over
     28/4 for all but the colour map entry coding.
   */
  vtobjTripBits (trip, 1, 7);
  x = vtobjTripBits (trip, 8, 14);
  vtctx_set_default_charset (ctx, x);
  x = vtobjTripBits (trip, 15, 18);
  trip = hamm24_r (pk + VT_DATA_START + 4, &err);
  x = x | (vtobjTripBits (trip, 1, 3) << 4);
  vtctx_set_second_charset (ctx, x);
  ctx->left_panel_active = vtobjTripBits (trip, 4, 4);
  ctx->right_panel_active = vtobjTripBits (trip, 5, 5);
  ctx->panel_status = vtobjTripBits (trip, 6, 6);
  ctx->panel_cols = vtobjTripBits (trip, 7, 10);


//      cmp=0;
  x = vtobjTripBits (trip, 11, 18);
  /*
     01230123
     RRRRGGGG BBBBRRRRGGGGBBBBRR RRGGGGBBBBRRRRGGGG BBBB
     |                  |                  |
   */
  out_buf[0] = x & 15;
  out_buf[1] = (x >> 4) & 15;
  for (i = 0; i < 10; i++)
  {
    trip = hamm24_r (VT_PK_TRIP (pk, 3 + i), &err);
    in_buf[i] = vtobjTripBits (trip, 1, 18);
  }
  vtctx_reframe (in_buf, out_buf + 2, 18, 4, 10);

  trip = hamm24_r (VT_PK_TRIP (pk, 13), &err);
  x = vtobjTripBits (trip, 1, 4);
  out_buf[47] = x;

/*
	nib_buf[0]=x&15;
	nib_buf[1]=(x>>4)&15;
	j=2;
	for(i=0;i<10;i++)
	{
		trip=hamm24_r(pk+VT_DATA_START+7+i*3, &err);
		x=vtobjTripBits(trip,1,18);
		if(i&1)
		{
			nib_buf[j]|=(x&3)<<2;
			j++;
			nib_buf[j]=(x>>2)&15;
			j++;
			nib_buf[j]=(x>>6)&15;
			j++;
			nib_buf[j]=(x>>10)&15;
			j++;
			nib_buf[j]=(x>>14)&15;
			j++;
		}
		else
		{
			nib_buf[j]=x&15;
			j++;
			nib_buf[j]=(x>>4)&15;
			j++;
			nib_buf[j]=(x>>8)&15;
			j++;
			nib_buf[j]=(x>>12)&15;
			j++;
			nib_buf[j]=(x>>16)&3;
		}
	}
	
	trip=hamm24_r(pk+VT_DATA_START+1+12*3, &err);
	x=vtobjTripBits(trip,1,4);
	nib_buf[2*8*3-1]=x;
	*/
  for (k = 0; k < 2; k++)
  {
    for (j = 0; j < 8; j++)
    {
      ctx->clut[k * 8 + j][0] = out_buf[k * 8 * 3 + j * 3 + 0];
      ctx->clut[k * 8 + j][1] = out_buf[k * 8 * 3 + j * 3 + 1];
      ctx->clut[k * 8 + j][2] = out_buf[k * 8 * 3 + j * 3 + 2];
    }
  }


  ctx->default_screen_color = vtobjTripBits (trip, 5, 9);
  ctx->default_row_color = vtobjTripBits (trip, 10, 14);
  ctx->black_bg_subst = vtobjTripBits (trip, 15, 15);
  ctx->clut_remap = vtobjTripBits (trip, 16, 18);
  return 0;

}

static int
vtctx_parse_m29_1 (VtCtx * ctx, uint8_t * pk)
{
  int trip, trip2;
//      uint8_t dc;
  int err;
//      int i,j,k,l;
//      int x;
//      int cmp;
//      uint8_t nib_buf[2*8*3];
//      dc=hamm84_r(*(pk+VT_DATA_START));
  trip = hamm24_r (pk + VT_DATA_START + 4, &err);
  trip2 = hamm24_r (pk + VT_DATA_START + 7, &err);
  ctx->dclut4g[0] = vtobjTripBits (trip, 1, 5);
  ctx->dclut4g[1] = vtobjTripBits (trip, 6, 10);
  ctx->dclut4g[2] = vtobjTripBits (trip, 11, 15);
  ctx->dclut4g[3] =
    vtobjTripBits (trip, 16, 18) | (vtobjTripBits (trip2, 1, 2) << 2);

  trip = trip2;
  trip2 = hamm24_r (pk + VT_DATA_START + 10, &err);
  ctx->dclut4n[0] = vtobjTripBits (trip, 3, 7);
  ctx->dclut4n[1] = vtobjTripBits (trip, 8, 12);
  ctx->dclut4n[2] = vtobjTripBits (trip, 13, 17);
  ctx->dclut4n[3] =
    vtobjTripBits (trip, 18, 18) | (vtobjTripBits (trip2, 1, 4) << 1);


  vtctx_get_dclut16 (ctx->dclut16g, pk + VT_DATA_START + 10, 5);
  vtctx_get_dclut16 (ctx->dclut16n, pk + VT_DATA_START + 1 + 3 * 7, 13);
  return 0;
}

static int
vtctx_parse_m29_0 (VtCtx * ctx, uint8_t * pk)
{
  int trip;
//      uint8_t dc;
  int err;
  int i, j, k;
  int x;
//      int cmp;
  unsigned in_buf[10];
  unsigned out_buf[vtctx_reframe_out_sz (10, 18, 4) + 3];
//      uint8_t nib_buf[2*8*3];
//      dc=hamm84_r(*(pk+VT_DATA_START));
  trip = hamm24_r (pk + VT_DATA_START + 1, &err);
  /*
     trip 1 bits 1..7 are 0
     1:8..14 G0 and G2 charset
     1:15..18 second G0 and nat opt
     2:1..3
     2:4 left side panel available
     2:5 right side panel available
     2:6 side panel status 0: 3.5 only 1:2.5 and 3.5
     2:7-10 number of cols in left sidepanel
     2:11-18
     3-12:1-18
     13:1-4 CLUTs 2 and 3 RRRRGGGGBBBB
     13:5-9 default screen colour
     13:10-14 default Row colour
     13:15 black bg color substitution
     13:16-18 colour table remapping
   */
  vtobjTripBits (trip, 1, 7);
  x = vtobjTripBits (trip, 8, 14);
  vtctx_set_default_charset (ctx, x);
  x = vtobjTripBits (trip, 15, 18);
  trip = hamm24_r (pk + VT_DATA_START + 4, &err);
  x = x | (vtobjTripBits (trip, 1, 3) << 4);
  vtctx_set_second_charset (ctx, x);
  ctx->left_panel_active = vtobjTripBits (trip, 4, 4);
  ctx->right_panel_active = vtobjTripBits (trip, 5, 5);
  ctx->panel_status = vtobjTripBits (trip, 6, 6);
  ctx->panel_cols = vtobjTripBits (trip, 7, 10);


//      cmp=0;
  x = vtobjTripBits (trip, 11, 18);
  /*
     01230123
     RRRRGGGG BBBBRRRRGGGGBBBBRR RRGGGGBBBBRRRRGGGG BBBB
     0       |0                 |0                 |0

   */
  out_buf[0] = x & 15;
  out_buf[1] = (x >> 4) & 15;

  for (i = 0; i < 10; i++)
  {
    trip = hamm24_r (VT_PK_TRIP (pk, 3 + i), &err);
    in_buf[i] = vtobjTripBits (trip, 1, 18);
  }
  vtctx_reframe (in_buf, out_buf + 2, 18, 4, 10);

  trip = hamm24_r (VT_PK_TRIP (pk, 13), &err);
  x = vtobjTripBits (trip, 1, 4);
  out_buf[47] = x;
/*
	nib_buf[0]=x&15;
	nib_buf[1]=(x>>4)&15;
	j=2;
	for(i=0;i<10;i++)
	{
		trip=hamm24_r(pk+VT_DATA_START+7+i*3, &err);
		x=vtobjTripBits(trip,1,18);
		if(i&1)
		{
			nib_buf[j]|=(x&3)<<2;
			j++;
			nib_buf[j]=(x>>2)&15;
			j++;
			nib_buf[j]=(x>>6)&15;
			j++;
			nib_buf[j]=(x>>10)&15;
			j++;
			nib_buf[j]=(x>>14)&15;
			j++;
		}
		else
		{
			nib_buf[j]=x&15;
			j++;
			nib_buf[j]=(x>>4)&15;
			j++;
			nib_buf[j]=(x>>8)&15;
			j++;
			nib_buf[j]=(x>>12)&15;
			j++;
			nib_buf[j]=(x>>16)&3;
		}
	}
	
	trip=hamm24_r(pk+VT_DATA_START+1+12*3, &err);
	x=vtobjTripBits(trip,1,4);
	nib_buf[2*8*3-1]=x;
	*/
  for (k = 0; k < 2; k++)
  {
    for (j = 0; j < 8; j++)
    {
      ctx->clut[2 * 8 + k * 8 + j][0] = out_buf[k * 8 * 3 + j * 3 + 0];
      ctx->clut[2 * 8 + k * 8 + j][1] = out_buf[k * 8 * 3 + j * 3 + 1];
      ctx->clut[2 * 8 + k * 8 + j][2] = out_buf[k * 8 * 3 + j * 3 + 2];
    }
  }


  ctx->default_screen_color = vtobjTripBits (trip, 5, 9);
  ctx->default_row_color = vtobjTripBits (trip, 10, 14);
  ctx->black_bg_subst = vtobjTripBits (trip, 15, 15);
  ctx->clut_remap = vtobjTripBits (trip, 16, 18);
  return 0;
}


///return start of option area or -1 if no option chars in charset
static int
vtctx_has_option (uint16_t sld)
{
  switch (sld)
  {
  case 0:
    return VT_SUB_ENGLISH_START;
  case 1:
    return VT_SUB_GERMAN_START;
  case 2:
    return VT_SUB_SWEDISH_FINNISH_START;
  case 3:
    return VT_SUB_ITALIAN_START;
  case 4:
    return VT_SUB_FRENCH_START;
  case 5:
    return VT_SUB_PORTUGESE_SPANISH_START;
  case 6:
    return VT_SUB_CZECH_SLOVAK_START;
  case 8:
    return VT_SUB_POLISH_START;
  case 9:
    return VT_SUB_GERMAN_START;
  case 10:
    return VT_SUB_SWEDISH_FINNISH_START;
  case 11:
    return VT_SUB_ITALIAN_START;
  case 12:
    return VT_SUB_FRENCH_START;
  case 14:
    return VT_SUB_CZECH_SLOVAK_START;
  case 16:
    return VT_SUB_ENGLISH_START;
  case 17:
    return VT_SUB_GERMAN_START;
  case 18:
    return VT_SUB_SWEDISH_FINNISH_START;
  case 19:
    return VT_SUB_ITALIAN_START;
  case 20:
    return VT_SUB_FRENCH_START;
  case 21:
    return VT_SUB_PORTUGESE_SPANISH_START;
  case 22:
    return VT_SUB_TURKISH_START;
  case 29:
    return VT_SUB_SERBIAN_CROATIAN_SLOVENIAN_START;
  case 31:
    return VT_SUB_RUMANIAN_START;
  case 33:
    return VT_SUB_GERMAN_START;
  case 34:
    return VT_SUB_ESTONIAN_START;
  case 35:
    return VT_SUB_LETTISH_LITHUANIAN_START;
  case 38:
    return VT_SUB_CZECH_SLOVAK_START;
  case 54:
    return VT_SUB_TURKISH_START;
  case 57:
    return VT_SUB_ENGLISH_START;
  case 61:
    return VT_SUB_FRENCH_START;
  default:
    return -1;
  }
}

///maps charcode to glyph index for national option chars
static int
vtctx_option_glyph (uint8_t d)
{
  d -= 32;
  switch (d)
  {
  case 3:
    return 0;
  case 4:
    return 1;
  case 32:
    return 2;
  case 59:
    return 3;
  case 60:
    return 4;
  case 61:
    return 5;
  case 62:
    return 6;
  case 63:
    return 7;
  case 64:
    return 8;
  case 91:
    return 9;
  case 92:
    return 10;
  case 93:
    return 11;
  case 94:
    return 12;
  default:
    return -1;
  }
}

///get base index for language
static int
vtctx_second_lang_offs (uint8_t sld)
{
  if ((sld < 32) || ((sld >= 33) && (sld <= 35)) || (sld == 38) || (sld == 54)
      || (sld == 64) || (sld == 68))
    return VT_LATING0_START;
  switch (sld)
  {
  case 32:
    return VT_CYRG0_1_START;
  case 36:
    return VT_CYRG0_2_START;
  case 37:
    return VT_CYRG0_3_START;
  case 55:
    return VT_GREEKG0_START;
  case 71:
    return VT_ARABICG0_START;
  case 85:
    return VT_HEBREWG0_START;
  case 87:
    return VT_ARABICG0_START;
  default:
    return -1;
  }
}

///get g2 base index for language
static int
vtctx_g2_offs (uint8_t sld)
{
  if ((sld < 32) || ((sld >= 33) && (sld <= 35)) || (sld == 38)
      || (sld == 54))
    return VT_LATING2_START;
  switch (sld)
  {
  case 32:
  case 36:
  case 37:
    return VT_CYRG2_START;
  case 55:
    return VT_GREEKG2_START;
  case 64:
  case 68:
  case 71:
  case 85:
  case 87:
    return VT_ARABICG2_START;
  default:
    return -1;
  }
}

///lookup g0 glyph index for level one page
static uint16_t
vtctx_lookup_g0_glyph (uint8_t d, uint8_t ld, uint8_t enh, uint8_t u NOTUSED,
                       uint8_t i, uint8_t b)
{
  int opt_glyph, opt_offs;
  if (ld == 0x7f)               // no second lang
    return 0;                   //space glyph (latin.. but they all look the same)
  opt_glyph = vtctx_option_glyph (d);
  if ((opt_glyph == -1) || (1 == enh))
  {
    int slo = vtctx_second_lang_offs (ld);
    if (slo == -1)
      return 0;
    return VT_CHAR_BASE (u, i, b) + slo + VT_GLYPH_IDX (d);
  }
  else
  {
    opt_offs = vtctx_has_option (ld);
    if (opt_offs == -1)
    {
      int slo = vtctx_second_lang_offs (ld);
      if (slo == -1)
        return 0;
      return VT_CHAR_BASE (u, i, b) + slo + VT_GLYPH_IDX (d);
    }
    else
    {
      return VT_CHAR_BASE (u, i, b) + opt_offs + opt_glyph;
    }
  }
}

static uint16_t
vtctx_lookup_g1_glyph (uint8_t d, uint8_t charset, uint8_t enh, uint8_t u,
                       uint8_t i, uint8_t b)
{
  if ((d >= 0x40) && (d < 0x60))
    return vtctx_lookup_g0_glyph (d, charset, enh, u, i, b);
  return VT_MOSA_BASE (u, i, b) + VT_GLYPH_IDX (d);
}

static uint16_t
vtctx_lookup_g2_glyph (uint8_t d, uint8_t charset, uint8_t enh NOTUSED, uint8_t u NOTUSED,
                       uint8_t i, uint8_t b)
{
  int slo = vtctx_g2_offs (charset);
  if (slo == -1)
    return 0;
  return VT_CHAR_BASE (u, i, b) + slo + VT_GLYPH_IDX (d);
}

static uint16_t
vtctx_lookup_g3_glyph (uint8_t d, uint8_t charset NOTUSED, uint8_t enh NOTUSED, uint8_t u NOTUSED,
                       uint8_t i NOTUSED, uint8_t b NOTUSED)
{
  return VT_SMOOTH_START + VT_GLYPH_IDX (d);
}

///lookup glyph for level one page
static uint16_t
vtctx_lookup_glyph (VtCtx * ctx NOTUSED, uint8_t charset, uint8_t g, uint8_t enh,
                    uint8_t u, uint8_t i, uint8_t b, uint8_t d)
{                               //generate glyph index from char and charset selection bits in ctx
//      uint16_t rv;
  switch (g & 3)                //g0,g1,g2,g3
  {
  case 0:
    return vtctx_lookup_g0_glyph (d, charset, enh, u, i, b);
    break;
  case 1:
    return vtctx_lookup_g1_glyph (d, charset, enh, u, i, b);
    break;
  case 2:
    return vtctx_lookup_g2_glyph (d, charset, enh, u, i, b);
    break;
  case 3:
    return vtctx_lookup_g3_glyph (d, charset, enh, u, i, b);
    break;
  default:
    break;
  }
  /*
     if(ctx->is_mosaic)
     {
     if((d>=64)&&(d<96))
     {//glyph from current g0 charset
     if(ctx->g0_esc)
     return vtctx_lookup_g0_glyph(d,ctx->sld);
     else
     return vtctx_lookup_g0_glyph(d,(ctx->default_charset));
     }
     return VT_BLOCK_START+VT_GLYPH_IDX(d);
     }
     else if(ctx->g0_esc)
     {//uses second language designation
     vtctx_lookup_g0_glyph(d,ctx->sld);
     }
     else
     {//primary charset
     return vtctx_lookup_g0_glyph(d,ctx->default_charset);//(ctx->csd<<3)|ctx->nos);
     } */
  return 0;
}


static uint16_t
vtctx_lookup_diacrit (VtCtx * ctx, uint8_t charset, uint8_t enh, uint8_t u,
                      uint8_t i, uint8_t b, uint8_t diacrit)
{
  return vtctx_lookup_glyph (ctx, charset, 2, enh, u, i, b, diacrit + 0x40);
}


///put a glyph at current location and increment position (level one)
/*
static void vtctx_glyph(VtCtx *ctx,uint16_t d,uint8_t mosaics_separate)
{
	int x,y;
	x=ctx->pos_x;//vtctx_col_to_x(ctx, ctx->pos_x);
	y=ctx->pos_y;
	ctx->screen[y][x].glyph=d;
	ctx->screen[y][x].fg_color=ctx->fg_color;
	ctx->screen[y][x].bg_color=ctx->bg_color;
	ctx->screen[y][x].flash_mode=ctx->flash?1:0;
	ctx->screen[y][x].flash_rate=0;
	ctx->screen[y][x].invert=0;
	ctx->screen[y][x].conceal=ctx->conceal;
	ctx->screen[y][x].window=(ctx->box_state>=2)?1:0;
	ctx->screen[y][x].e_size=ctx->e_size;
	ctx->screen[y][x].underline=mosaics_separate;
	ctx->screen[y][x].italics=0;
	ctx->screen[y][x].bold=0;
	ctx->screen[y][x].diacrit=0;
	
	ctx->pos_x=(ctx->pos_x+1)%56;
}
*/
/*
uint8_t *vtctx_get_pk_i(uint8_t *pg,int i)
{
	return pg+(VT_UNIT_LEN*i);
}
*/



///interpret a page's x27 packets
static void
vtctx_parse_x27 (VtCtx * ctx, uint8_t * pg, int num_pk_pg)
{
  int i, err;
  for (i = 0; i < num_pk_pg; i++)
  {
    int y, dc;
    int trip1, trip2;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if (y == 27)
    {
      dc = get_pk_ext (pk);
      if ((dc == 4) || (dc == 5))
      {
        for (i = 0; i < 6; i++)
        {
          trip1 = hamm24_r (VT_PK_TRIP (pk, 1 + i * 2), &err);
          trip2 = hamm24_r (VT_PK_TRIP (pk, 2 + i * 2), &err);
          if ((0 == vtobjTripBits (trip2, 1, 1))
              && (0 == vtobjTripBits (trip1, 11, 11)))
          {                     //fmt 2

          }
          else if ((0 == vtobjTripBits (trip2, 1, 1))
                   && (1 == vtobjTripBits (trip1, 11, 11)))
          {                     //fmt 1
            VtCLink tmp;
            tmp.present = 1;
            tmp.func = vtobjTripBits (trip1, 1, 2);
            tmp.valid = vtobjTripBits (trip1, 3, 4);
            tmp.pgnu = vtobjTripBits (trip1, 7, 10);
            tmp.magrel = vtobjTripBits (trip1, 12, 14);
            tmp.pgnt = vtobjTripBits (trip1, 15, 18);
            tmp.sub = vtobjTripBits (trip2, 3, 18);
            if (tmp.valid && ((tmp.pgnu != 0xf) || (tmp.pgnt != 0xf)))
              switch (tmp.func)
              {
              case 0:
                ctx->gpop_link = tmp;
                break;
              case 1:
                ctx->pop_link = tmp;
                break;
              case 2:
                ctx->gdrcs_link = tmp;
                break;
              case 3:
                ctx->drcs_link = tmp;
                break;
              default:
                break;
              }
          }
        }
      }
//                      vtctx_page_link(ctx,pk);
    }
  }
}


///interpret a packet's x28 packets
static void
vtctx_parse_x28 (VtCtx * ctx, uint8_t * pg, int num_pk_pg)
{
  int i;
  for (i = 0; i < num_pk_pg; i++)
  {
    int y, dc;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if (y == 28)
    {
      dc = get_pk_ext (pk);
      switch (dc)
      {
      case 0:
        vtctx_parse_m29_0 (ctx, pk);    //are the same(I think)
        break;
      case 1:
        vtctx_parse_m29_1 (ctx, pk);
        break;
      case 4:
        vtctx_parse_m29_4 (ctx, pk);
        break;
      default:
        break;
      }
    }
  }
}

static uint8_t *
vtctx_find_pk_y (uint8_t * pg, int num_pk, int x)
{
  int xf, i;
  for (i = 0; i < num_pk; i++)
  {
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    xf = get_pk_y (pk);         //get one 0 and maaaaaaaaany with x==1
    if (xf == x)
      return pk;
  }
  return NULL;
}

static void
vtctx_parse_mot (VtCtx * ctx, int page, uint8_t * pg, int num_pk_pg)
{
  int pk_y = 1 + ((page % 100) / 20);
  int pk_link = ((page % 100) % 20);
  int i;
  for (i = 0; i < num_pk_pg; i++)
  {
    int y;
    uint8_t *pk = pg + (VT_UNIT_LEN * i);
    y = get_pk_y (pk);
    if (y == pk_y)
    {
      VtCLink tmp;
      uint8_t *p = VT_PK_BYTE (pk, 6 + pk_link * 2);
      uint8_t a, b;
      a = *p;
      b = *(p + 1);
//                      a=*(p+1);
//                      b=*p;
      a = hamm84_r (a);
      b = hamm84_r (b);
      if (a & 8)                //gpop
      {
        pk = vtctx_find_pk_y (pg, num_pk_pg, 22);
        if (NULL == pk)
          pk = vtctx_find_pk_y (pg, num_pk_pg, 19);
        if (pk)
        {
          p = VT_PK_BYTE (pk, 6);
          tmp.magrel = hamm84_r (*p);
          tmp.magrel = ((tmp.magrel) & 7) ^ ctx->magno;
          tmp.pgnt = hamm84_r (*(p + 1));
          tmp.pgnu = hamm84_r (*(p + 2));
          tmp.sub = hamm84_r (*(p + 3));
          tmp.sub = (1 << tmp.sub) - 1;
          tmp.valid = 3;
          tmp.func = 3;
          if ((0xf != tmp.pgnt) || (0xf != tmp.pgnu))
          {
            tmp.present = 1;
            ctx->gpop_link = tmp;
          }
          ctx->def_gobj_flags = hamm84_r (*(p + 5));
          ctx->def_gobj[0] = hamm84_r (*(p + 6)) | (hamm84_r (*(p + 7)) << 4);
          ctx->def_gobj[1] = hamm84_r (*(p + 8)) | (hamm84_r (*(p + 9)) << 4);
        }
        else
          ctx->def_gobj_flags = 0;
      }
      if (a & 7)                //pop
      {
        int l = a & 7;
        pk = vtctx_find_pk_y (pg, num_pk_pg, (l < 4) ? 22 : 23);
        if (NULL == pk)
          pk = vtctx_find_pk_y (pg, num_pk_pg, (l < 4) ? 19 : 20);

        if (pk)
        {
          p = VT_PK_BYTE (pk, 6 + 10 * (l % 4));
          tmp.magrel = hamm84_r (*p);
          tmp.magrel = ((tmp.magrel) & 7) ^ ctx->magno;
          tmp.pgnt = hamm84_r (*(p + 1));
          tmp.pgnu = hamm84_r (*(p + 2));
          tmp.sub = hamm84_r (*(p + 3));
          tmp.sub = (1 << tmp.sub) - 1;
          tmp.valid = 3;
          tmp.func = 3;
          if ((0xf != tmp.pgnt) || (0xf != tmp.pgnu))
          {
            tmp.present = 1;
            ctx->pop_link = tmp;
          }
          ctx->def_obj_flags = hamm84_r (*(p + 5));
          ctx->def_obj[0] = hamm84_r (*(p + 6)) | (hamm84_r (*(p + 7)) << 4);
          ctx->def_obj[1] = hamm84_r (*(p + 8)) | (hamm84_r (*(p + 9)) << 4);
        }
        else
          ctx->def_obj_flags = 0;
      }
      if (b & 8)                //glob drcs
      {
        int l = 6;
        VtCLink tmp;
        pk = vtctx_find_pk_y (pg, num_pk_pg, 24);
        if (NULL == pk)         //fall back on 2.5 data (is this supposed to work that way?)
          pk = vtctx_find_pk_y (pg, num_pk_pg, 21);
        if (pk)
        {
          p = VT_PK_BYTE (pk, l);
          tmp.magrel = hamm84_r (*p);
          tmp.magrel = ((tmp.magrel) & 7) ^ ctx->magno;
          tmp.pgnt = hamm84_r (*(p + 1));
          tmp.pgnu = hamm84_r (*(p + 2));
          tmp.sub = hamm84_r (*(p + 3));
          tmp.sub = (1 << tmp.sub) - 1;
          tmp.valid = 3;
          tmp.func = 3;
          if ((0xf != tmp.pgnt) || (0xf != tmp.pgnu))
          {
            tmp.present = 1;
            ctx->gdrcs_link = tmp;
          }
        }
      }
      if (b & 7)                //loc drcs
      {
        int l = 6 + 4 * (b & 7);
        VtCLink tmp;
        pk = vtctx_find_pk_y (pg, num_pk_pg, 24);
        if (NULL == pk)
          pk = vtctx_find_pk_y (pg, num_pk_pg, 21);
        if (pk)
        {
          p = VT_PK_BYTE (pk, l);
          tmp.magrel = hamm84_r (*p);
          tmp.magrel = ((tmp.magrel) & 7) ^ ctx->magno;
          tmp.pgnt = hamm84_r (*(p + 1));
          tmp.pgnu = hamm84_r (*(p + 2));
          tmp.sub = hamm84_r (*(p + 3));
          tmp.sub = (1 << tmp.sub) - 1;
          tmp.valid = 3;
          tmp.func = 3;
          if ((0xf != tmp.pgnt) || (0xf != tmp.pgnu))
          {
            tmp.present = 1;
            ctx->drcs_link = tmp;
          }
        }
      }
      break;                    //we're done
    }
  }
}

/*
static int vtctx_clip(int x,int y,int maxx,int maxy)
{
	if((x>=maxx)||(y>=maxy)||(x<0)||(y<0))
		return 1;
	return 0;
}

static int vtctx_real_col(VtCtx *ctx,int col)
{
	int col_l=0,col_r=0;
	
	col_l=ctx->panel_cols;
	if(ctx->left_panel_active)
	{
		if((!ctx->right_panel_active)&&(0==ctx->panel_cols))
		{
			col_l=16;
		}
	}
	if(ctx->right_panel_active)
		col_r=16-ctx->panel_cols;
	
	if(col<(40+col_r))
	{
		return (col_l+col);
	}
	else if(col<(72-col_l))
	{
		return 72;
	}
	else if(col<72)
	{
		return (col+col_l-72);
	}
	return 72;
}
*/
#define VT_STRIDE 672
#define VT_PUT_PIX(_B,_X,_Y,_C) _B[(_Y)*VT_STRIDE+(_X)]=(_C);


static void
vtctx_paint_glyph (uint16_t * c, uint8_t * buffer, int sz, int xpos, int ypos,
                   uint8_t fg_c, uint8_t bg_c, int t)
{
  int i, j, k, l, dh = sz & 1, dw = (sz >> 1) & 1;
  //paint bitmap char
  for (i = 0; i < 10; i++)
  {
    for (j = 0; j < 12; j++)
    {
      if (c[i] & (1 << j))
      {
        for (k = 0; k <= dh; k++)
          for (l = 0; l <= dw; l++)
            buffer[(ypos + i * (dh + 1) + k) * VT_STRIDE + xpos +
                   j * (dw + 1) + l] = fg_c;
      }
      else if (!t)
      {
        for (k = 0; k <= dh; k++)
          for (l = 0; l <= dw; l++)
            buffer[(ypos + i * (dh + 1) + k) * VT_STRIDE + xpos +
                   j * (dw + 1) + l] = bg_c;
      }
    }
  }
}

static void
vtctx_paint_underline (uint8_t * buffer, int sz, int xpos, int ypos,
                       uint8_t fg_c, uint8_t bg_c NOTUSED)
{
  int i, j, k, l, dh = sz & 1, dw = (sz >> 1) & 1;
  //paint bitmap char
  i = 8;
  for (j = 0; j < 12; j++)
  {
    for (k = 0; k <= dh; k++)
      for (l = 0; l <= dw; l++)
        buffer[(ypos + i * (dh + 1) + k) * VT_STRIDE + xpos + j * (dw + 1) +
               l] = fg_c;
  }
}

static void
vtctx_drcs_12_10_1 (VtCtx * ctx, VtCell * cell, uint8_t * buffer, int xpos,
                    int ypos, uint8_t fg_c, uint8_t bg_c, uint8_t cf NOTUSED)
{
  int i, j, k, l, dh = cell->e_size & 1, dw = (cell->e_size >> 1) & 1;
  VtDRCSPTU *c =
    &ctx->drcs[cell->drcs_normal & 1][cell->drcs_sub & 15][cell->character %
                                                           48];
  //paint bitmap char
  for (i = 0; i < 10; i++)
  {
    for (j = 0; j < 6; j++)
    {
      if (c->data[i * 2] & (1 << (j + 2)))
      {
        for (k = 0; k <= dh; k++)
          for (l = 0; l <= dw; l++)
            VT_PUT_PIX (buffer, xpos + j * (dw + 1) + l,
                        ypos + i * (dh + 1) + k, fg_c);
      }
      else
      {
        for (k = 0; k <= dh; k++)
          for (l = 0; l <= dw; l++)
            VT_PUT_PIX (buffer, xpos + j * (dw + 1) + l,
                        ypos + i * (dh + 1) + k, bg_c);
      }
    }
    for (j = 0; j < 6; j++)
    {
      if (c->data[i * 2 + 1] & (1 << (j + 2)))
      {
        for (k = 0; k <= dh; k++)
          for (l = 0; l <= dw; l++)
            VT_PUT_PIX (buffer, xpos + (j + 6) * (dw + 1) + l,
                        ypos + i * (dh + 1) + k, fg_c);
      }
      else
      {
        for (k = 0; k <= dh; k++)
          for (l = 0; l <= dw; l++)
            VT_PUT_PIX (buffer, xpos + (j + 6) * (dw + 1) + l,
                        ypos + i * (dh + 1) + k, bg_c);
      }
    }
  }
}

static void
vtctx_drcs_12_10_2 (VtCtx * ctx, VtCell * cell, uint8_t * buffer, int xpos,
                    int ypos, uint8_t fg_c NOTUSED, uint8_t bg_c NOTUSED, uint8_t cf)
{
  int i, j, k, l, dh = cell->e_size & 1, dw = (cell->e_size >> 1) & 1;
  VtDRCSPTU *c =
    &ctx->drcs[cell->drcs_normal & 1][cell->drcs_sub & 15][cell->character %
                                                           48];
  uint8_t *clut = cell->drcs_normal ? ctx->dclut4n : ctx->dclut4g;
  uint8_t idx;
  uint8_t color;
  if ((cell->character % 48) > 46)
  {
//              errMsg("PTU out of bounds\n");
    return;
  }
  //paint bitmap char
  for (i = 0; i < 10; i++)
  {
    for (j = 0; j < 6; j++)
    {
      idx = 0;
      idx |= (c[0].data[i * 2] & (1 << (j + 2))) ? 1 : 0;
      idx |= (c[1].data[i * 2] & (1 << (j + 2))) ? 2 : 0;
      color = clut[idx];

      if (cf)
        color = color ^ 8;

      for (k = 0; k <= dh; k++)
        for (l = 0; l <= dw; l++)
          VT_PUT_PIX (buffer, xpos + j * (dw + 1) + l,
                      ypos + i * (dh + 1) + k, color);
    }
    for (j = 0; j < 6; j++)
    {
      idx = 0;
      idx |= (c[0].data[i * 2 + 1] & (1 << (j + 2))) ? 1 : 0;
      idx |= (c[1].data[i * 2 + 1] & (1 << (j + 2))) ? 2 : 0;
      color = clut[idx];

      if (cf)
        color = color ^ 8;

      for (k = 0; k <= dh; k++)
        for (l = 0; l <= dw; l++)
          VT_PUT_PIX (buffer, xpos + (j + 6) * (dw + 1) + l,
                      ypos + i * (dh + 1) + k, color);
    }
  }
}

static void
vtctx_drcs_12_10_4 (VtCtx * ctx, VtCell * cell, uint8_t * buffer, int xpos,
                    int ypos, uint8_t fg_c NOTUSED, uint8_t bg_c NOTUSED, uint8_t cf)
{
  int i, j, k, l, dh = cell->e_size & 1, dw = (cell->e_size >> 1) & 1;
  VtDRCSPTU *c =
    &ctx->drcs[cell->drcs_normal & 1][cell->drcs_sub & 15][cell->character %
                                                           48];
  uint8_t *clut = cell->drcs_normal ? ctx->dclut16n : ctx->dclut16g;
  uint8_t idx;
  uint8_t color;
  if ((cell->character % 48) > 44)
  {
//              errMsg("PTU out of bounds\n");
    return;
  }
  //paint bitmap char
  for (i = 0; i < 10; i++)
  {
    for (j = 0; j < 6; j++)
    {
      idx = 0;
      idx |= (c[0].data[i * 2] & (1 << (j + 2))) ? 1 : 0;
      idx |= (c[1].data[i * 2] & (1 << (j + 2))) ? 2 : 0;
      idx |= (c[2].data[i * 2] & (1 << (j + 2))) ? 4 : 0;
      idx |= (c[3].data[i * 2] & (1 << (j + 2))) ? 8 : 0;
      color = clut[idx];

      if (cf)
        color = color ^ 8;

      for (k = 0; k <= dh; k++)
        for (l = 0; l <= dw; l++)
          VT_PUT_PIX (buffer, xpos + j * (dw + 1) + l,
                      ypos + i * (dh + 1) + k, color);
    }
    for (j = 0; j < 6; j++)
    {
      idx = 0;
      idx |= (c[0].data[i * 2 + 1] & (1 << (j + 2))) ? 1 : 0;
      idx |= (c[1].data[i * 2 + 1] & (1 << (j + 2))) ? 2 : 0;
      idx |= (c[2].data[i * 2 + 1] & (1 << (j + 2))) ? 4 : 0;
      idx |= (c[3].data[i * 2 + 1] & (1 << (j + 2))) ? 8 : 0;
      color = clut[idx];

      if (cf)
        color = color ^ 8;

      for (k = 0; k <= dh; k++)
        for (l = 0; l <= dw; l++)
          VT_PUT_PIX (buffer, xpos + (j + 6) * (dw + 1) + l,
                      ypos + i * (dh + 1) + k, color);
    }
  }
}

static void
vtctx_drcs_6_5_4 (VtCtx * ctx, VtCell * cell, uint8_t * buffer, int xpos,
                  int ypos, uint8_t fg_c NOTUSED, uint8_t bg_c NOTUSED, uint8_t cf)
{
  int i, j, k, l, dh = cell->e_size & 1, dw = (cell->e_size >> 1) & 1;
  VtDRCSPTU *c =
    &ctx->drcs[cell->drcs_normal & 1][cell->drcs_sub & 15][cell->character %
                                                           48];
  uint8_t *clut = cell->drcs_normal ? ctx->dclut16n : ctx->dclut16g;
  uint8_t idx;
  uint8_t color;
  //paint bitmap char
  for (i = 0; i < 5; i++)
  {
    for (j = 0; j < 6; j++)
    {
      idx = 0;
      idx |= (c->data[i * 4 + 0] & (1 << (j + 2))) ? 1 : 0;
      idx |= (c->data[i * 4 + 1] & (1 << (j + 2))) ? 2 : 0;
      idx |= (c->data[i * 4 + 2] & (1 << (j + 2))) ? 4 : 0;
      idx |= (c->data[i * 4 + 3] & (1 << (j + 2))) ? 8 : 0;
      color = clut[idx];

      if (cf)
        color = color ^ 8;

      for (k = 0; k < (2 * (dh + 1)); k++)
        for (l = 0; l < (2 * (dw + 1)); l++)
          VT_PUT_PIX (buffer, xpos + j * 2 * (dw + 1) + l,
                      ypos + i * 2 * (dh + 1) + k, color);
    }
  }
}

static void
vtctx_paint_drcs (VtCtx * ctx, VtCell * cell, uint8_t * buffer, int xpos,
                  int ypos, uint8_t fg_c, uint8_t bg_c, uint8_t cf)
{
  uint8_t mode =
    ctx->drcs[cell->drcs_normal][cell->drcs_sub][cell->character].mode;
  switch (mode)
  {
  case 0:
    vtctx_drcs_12_10_1 (ctx, cell, buffer, xpos, ypos, fg_c, bg_c, cf);
    break;
  case 1:
    vtctx_drcs_12_10_2 (ctx, cell, buffer, xpos, ypos, fg_c, bg_c, cf);
    break;
  case 2:
    vtctx_drcs_12_10_4 (ctx, cell, buffer, xpos, ypos, fg_c, bg_c, cf);
    break;
  case 3:
    vtctx_drcs_6_5_4 (ctx, cell, buffer, xpos, ypos, fg_c, bg_c, cf);
    break;
  default:
//              errMsg("Bad PTU mode:%u\n",mode);
    break;
  }
}

/**
 *\brief render a single character cell
 *
 *\param ctx the parsing context initialized by previous vtctxParse
 *\param state a position in the flash cycle
 *\param reveal reveal flag
 *\param xoffs x offset in destination buffer of cell to paint, in pixels. The y offset is computed as 10*row.
 *\param row row index of cell to paint. not range checked 0-25, please
 *\param col column index of cell to paint. not range checked 0-71, please
 *\param buffer buffer to paint to
 */
static void
vtctx_render_cell (VtCtx * ctx, uint8_t state, uint8_t reveal, int xoffs,
                   int row, int col, uint8_t * buffer)
{
  VtCell *cell = &ctx->screen[row][col];
  int xpos, ypos;
  int xend, yend;
  int i, j;
  int fg_c, bg_c, vis;
  int cf = 0;
  uint16_t *c;                  //to point at bitmap for character
  uint16_t *d;                  //to point at bitmap for diacrit

  state = state % 12;
  /*
     if(cell->e_size&1)
     if(row&1)
     return;
     if(cell->e_size&2)
     if(col&1)
     return; */
  //check for double sizes
  //skip if we are being overlapped
  //better do a 2 stage rendering ,painting backgrounds first, then characters
  //this allows double size characters(even obverlapping)
  //current variant doesn't really work
  //X
  //X<-
  if ((row > 0) && (ctx->screen[row - 1][col].e_size & 1))
  {
    cell->e_size = 0;
    return;
  }
  //XX<-
  if ((col > 0) && (ctx->screen[row][col - 1].e_size & 2))
  {
    cell->e_size = 0;
    return;
  }
  //XX
  //XX<-
  if ((row > 0) && (col > 0) && (ctx->screen[row - 1][col - 1].e_size & 2)
      && (ctx->screen[row - 1][col - 1].e_size & 1))
  {
    cell->e_size = 0;
    return;
  }
  //position selection
  /*
     xoffs=0;
     if(ctx->left_panel_active)
     {
     if((!ctx->right_panel_active)&&(0==ctx->panel_cols))
     {
     xoffs=12*16;
     l_cols=16;
     }
     else
     {
     l_cols=ctx->panel_cols;
     xoffs=12*ctx->panel_cols;
     }
     }
     else if(!ctx->right_panel_active)
     {//no panels, center
     l_cols=8;
     r_cols=8;
     xoffs=12*8;
     }
     if(ctx->right_panel_active)
     {
     r_cols=16-ctx->panel_cols;
     if((!ctx->left_panel_active)&&(0==ctx->panel_cols))
     {
     xoffs=0;
     r_cols=16;
     }
     else
     {
     r_cols=16-ctx->panel_cols;
     }            
     }
   */
  xpos = xoffs;                 //+col*12;upper layer should do this
  ypos = row * 10;

  //bound check
  //x (actually end plus one)
  xend = xpos + 12 * (((cell->e_size >> 1) & 1) + 1);
  if (xend > 672)
    return;
  //bound check
  //y
  yend = ypos + 10 * ((cell->e_size & 1) + 1);
  if (yend > 260)
    return;


  //color selection
  fg_c = cell->fg_color;
  bg_c = cell->bg_color;
  if (fg_c == 8)
    fg_c = ctx->full_row_color[row];
  if (bg_c == 8)
    bg_c = ctx->full_row_color[row];

  if (ctx->c5 || ctx->c6)
  {
    if (!cell->window)
    {
      for (i = 0; i < 10; i++)
        for (j = 0; j < 12; j++)
          VT_PUT_PIX (buffer, xpos + j, ypos + i, 8);
      return;
    }
  }
  else
  {
    if (cell->window)
    {
      if (cell->fg_color == 8)
      {
        fg_c = 8;
      }
      if (cell->bg_color == 8)
      {
        bg_c = 8;
      }
    }
  }

  if (cell->invert)
  {
    int tmp = fg_c;
    fg_c = bg_c;
    bg_c = tmp;
  }

  vis = VtCtxFlash[cell->flash_rate] & (1 << state);
  switch (cell->flash_mode)
  {
  case 0:                      //steady
    vis = 1;
    break;
  case 1:                      //normal flash
    break;
  case 2:                      //invert phase flash
    vis = !vis;
    break;
  case 3:                      //CLUT 0/1 2/3 color flash
    if (!vis)
    {
      fg_c = fg_c ^ 8;
      cf = 1;                   //color flash(for drcs)
    }
    vis = 1;                    //something _is_ visible
    break;
  }

  //glyph selection

  if ((cell->conceal && (!reveal)) || (!vis))
  {
    c = VT_CHARS[0];            //not visible, put space here
    vtctx_paint_glyph (c, buffer, cell->e_size, xpos, ypos, fg_c, bg_c, 0);
    return;
  }

  if (cell->is_drcs)
  {
    vtctx_paint_drcs (ctx, cell, buffer, xpos, ypos, fg_c, bg_c, cf);
  }
  else
  {
    uint16_t idx;
    if (cell->at_char)
    {
      c =
        VT_CHARS[VT_CHAR_BASE (cell->underline, cell->italics, cell->bold) +
                 VT_AT_IDX];
    }
    else
    {
      idx =
        vtctx_lookup_glyph (ctx, cell->charset, cell->is_mosaic,
                            cell->ext_char, cell->underline, cell->italics,
                            cell->bold, cell->character);
      c = VT_CHARS[idx];
    }
    idx =
      vtctx_lookup_diacrit (ctx, cell->charset, cell->ext_char,
                            cell->underline, cell->italics, cell->bold,
                            cell->diacrit);
    d = VT_CHARS[idx];
    vtctx_paint_glyph (c, buffer, cell->e_size, xpos, ypos, fg_c, bg_c, 0);
    vtctx_paint_glyph (d, buffer, cell->e_size, xpos, ypos, fg_c, bg_c, 1);     //those cause trouble in enh data..?
    if (cell->underline && ((0 == cell->is_mosaic) || (2 == cell->is_mosaic)))
    {
      vtctx_paint_underline (buffer, cell->e_size, xpos, ypos, fg_c, bg_c);
    }
  }
}


//-----------------------------------------------------------Interface functions----------------------------------------------------

int
vtctxInit (VtCtx * ctx, void *d,
           uint8_t * (*get_pk_cb) (void *d, int x, int y, int dc),
           uint8_t * (*get_pg_cb) (void *d, int magno, int pgno, int subno,
                                   int *num_ret),
           uint8_t * (*get_dpg_cb) (void *d, int magno, int pgno, int subno,
                                    int *num_ret))
{
  memset (ctx, 0, sizeof (VtCtx));
  ctx->d = d;
  ctx->get_pk_cb = get_pk_cb;
  ctx->get_pg_cb = get_pg_cb;
  ctx->get_dpg_cb = get_dpg_cb;
  return 0;
}


static int
vtctx_to_bcd (int x)
{
  int pos = 0;
  int rv = 0;
  int val = 0;
  while (x)
  {
    val = x % 10;
    rv |= val << (4 * pos);
    pos++;
    x /= 10;
  }
  return rv;
}

void vtlopParsePagePk (VtCtx * ctx, uint8_t * pk);

int
vtctxParse (VtCtx * ctx, int page, int sub)
{
  uint8_t *pk;
  int num_pk_pg = 0;
  int magno = page / 100;
  int num_pk_mot = 0;
  uint8_t *mot = NULL;
  uint8_t *pg = NULL;
  int i;

  magno &= 7;
  pg =
    ctx->get_pg_cb (ctx->d, magno, vtctx_to_bcd (page % 100),
                    vtctx_to_bcd (sub), &num_pk_pg);
  if ((NULL == pg) || (0 == num_pk_pg))
    return 1;

  ctx->magno = magno;
  ctx->pgu = page % 10;
  ctx->pgt = (page / 10) % 10;
  ctx->subno = sub;

  ctx->drcs_nsub = 0;
  ctx->drcs_gsub = 0;
  ctx->skip_lines = 0;
  ctx->default_screen_color = 0;        //default to black
  ctx->default_row_color = 0;   //default to black
  memset (ctx->screen, 0, sizeof (ctx->screen));
  memmove (ctx->clut, VtDefaultCLUT, sizeof (VtDefaultCLUT));
  memmove (ctx->dclut4g, VtCtxDclut4, sizeof (VtCtxDclut4));
  memmove (ctx->dclut4n, VtCtxDclut4, sizeof (VtCtxDclut4));
  memmove (ctx->dclut16g, VtCtxDclut16, sizeof (VtCtxDclut16));
  memmove (ctx->dclut16n, VtCtxDclut16, sizeof (VtCtxDclut16));
  memset (ctx->drcs, 0, sizeof (ctx->drcs));
  ctx->drcs_loaded = 0;

  ctx->pg = pg;
  ctx->num_pk_pg = num_pk_pg;
  mot = ctx->get_dpg_cb (ctx->d, magno, 0xfe, 0, &num_pk_mot);
  if (mot)
  {
    vtctx_parse_mot (ctx, page, mot, num_pk_mot);
    free (mot);
  }
  /*
     interpret m29/0,1,4 packets
   */
  pk = ctx->get_pk_cb (ctx->d, magno, 29, 0);
  if (pk)
  {
    vtctx_parse_m29_0 (ctx, pk);
    free (pk);
  }
  pk = ctx->get_pk_cb (ctx->d, magno, 29, 1);
  if (pk)
  {
    vtctx_parse_m29_1 (ctx, pk);
    free (pk);
  }

  pk = ctx->get_pk_cb (ctx->d, magno, 29, 4);
  if (pk)
  {
    vtctx_parse_m29_4 (ctx, pk);
    free (pk);
  }



  vtctx_parse_x28 (ctx, pg, num_pk_pg);
  //above may change default full row color
  memset (ctx->full_row_color, ctx->default_row_color,
          sizeof (ctx->full_row_color));
/*
	ctx->page=page;
	ctx->sub=sub;no way to do this now
	mot=ctx->get_pg_cb(ctx->d,magno,0xfe,0,&num_pk_mot);//what subpage to use (section A.1)? different interface?
	if(mot)
	{
		vtctx_init_gdrcs(ctx,mot,num_pk_mot);
		vtctx_init_drcs(ctx,mot,num_pk_mot);

		vtctx_init_gpop(ctx,mot,num_pk_mot);
		vtctx_init_pop(ctx,mot,num_pk_mot);
	}*/
  vtctx_parse_x27 (ctx, pg, num_pk_pg);
  //the pop/drcs links are set up, now read the tables
//      vtctx_init_drcs(ctx); do demand-loading
//      vtctx_init_pop(ctx);
  for (i = 0; i < ctx->num_pk_pg; i++)
  {
    vtlopParsePagePk (ctx, ctx->pg + (VT_UNIT_LEN * i));
  }
  vtobjParseX26 (ctx, pg, num_pk_pg);
  free (ctx->pg);
  ctx->pg = NULL;
  return 0;
}

int
vtctxRender (VtCtx * ctx, uint8_t state, uint8_t reveal, uint8_t * buffer)
{
  int i, j;
  int right_cols = 0;
  int left_cols = 0;
  int xoffs = 0;
  uint8_t bg_c;
  if (!ctx->right_panel_active && !ctx->left_panel_active)
  {                             //place it mid inside framebuffer
    xoffs = 8 * 12;
    right_cols = 0;
    left_cols = 0;
  }
  else
  {
    if (ctx->right_panel_active)
    {
      right_cols = 16 - ctx->panel_cols;
    }

    if (ctx->left_panel_active)
    {
      left_cols = ctx->panel_cols;
      if (!ctx->right_panel_active)
      {
        if (0 == ctx->panel_cols)
        {
          left_cols = 16;
        }
      }
      xoffs = left_cols * 12;
    }
  }
  bg_c = ctx->default_screen_color;
  if (ctx->c5 || ctx->c6)
  {
    bg_c = 8;
  }
  memset (buffer, bg_c, 672 * 260);

  for (j = 0; j < 26; j++)
  {
    //mid and right panel
    for (i = 0; i < (40 + right_cols); i++)
    {
      vtctx_render_cell (ctx, state, reveal, xoffs + i * 12, j, i, buffer);
    }
    //left panel
    for (i = (72 - left_cols); i < 72; i++)
    {
      vtctx_render_cell (ctx, state, reveal, (i - (72 - left_cols)) * 12, j,
                         i, buffer);
    }
  }
  return 0;
}

uint8_t const *
vtctxPal (VtCtx * ctx)
{
  return (uint8_t const *) ctx->clut;
}

void
vtctxFlush (VtCtx * ctx)
{
  int i;
  free (ctx->pg);
  ctx->pg = NULL;
  ctx->num_pk_pg = 0;
  for (i = 0; i < 16; i++)
  {
    free (ctx->pop_pg[0][i]);
    ctx->pop_pg[0][i] = NULL;
    ctx->num_pop_pk[0][i] = 0;

    free (ctx->pop_pg[1][i]);
    ctx->pop_pg[1][i] = NULL;
    ctx->num_pop_pk[1][i] = 0;
  }
}

void
vtctxClear (VtCtx * ctx)
{
  vtctxFlush (ctx);
  memset (ctx, 0, sizeof (VtCtx));
}
