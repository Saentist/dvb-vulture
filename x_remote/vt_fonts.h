#ifndef __vt_fonts_h
#define __vt_fonts_h

#include <stdint.h>

/**
 *\brief bitmapped characters
 *
perhaps we should use a contingous font table for intermediary representation?
first all g0/g2 sets, 96 glyphs per table
then the national sub option chars 13x13 glyphs
then block/smooth mosaics 2*96 glyphs.

then there's bold/italics/underline
should we generate prefabs or fumble with topological ops to render chars?

we draw underlines directly, so there's 2 bits left for bold/italics
mosaic chars don't have underline/bold/italics but seperate/joined

we get 4*(11*96+13*13)+2*96+96 for 11 charsets and 13 option sets in 4 variants plus 1 mosaic set in 2 variants and smooth mosaics

global and local drcs chars are not static, so we use a per-object allocation/definition
*/

extern uint16_t VT_CHARS[4 * (11 * 96 + 13 * 13) + 2 * 96 + 96][10];
///generates base charater offset for italic/bold flags
#define VT_CHAR_BASE(_U,_I,_B) (((_I<<1)|(_B))*(11*96+13*13))
///generate base offset for block mosaic chars _U set for separate mosaic chars
#define VT_MOSA_BASE(_U,_I,_B) (4*(11*96+13*13)+(_U)*96)
///there's an 'at' @ character at index 2 of english natl subset
#define VT_AT_IDX (VT_LATING0_START+0x20)
///generate glyph index offset for a character code
#define VT_GLYPH_IDX(_C) (((_C)>=32)?((_C)-32):0)

///charsets 96 chars each.
#define VT_LATING0_START  0
#define VT_LATING2_START  96
#define VT_CYRG0_1_START  (2*96)
#define VT_CYRG0_2_START  (3*96)
#define VT_CYRG0_3_START  (4*96)
#define VT_CYRG2_START    (5*96)
#define VT_GREEKG0_START  (6*96)
#define VT_GREEKG2_START  (7*96)
#define VT_ARABICG0_START (8*96)
#define VT_ARABICG2_START (9*96)
#define VT_HEBREWG0_START (10*96)
///national option sub-sets 13 chars each
#define VT_SUB_CZECH_SLOVAK_START                (11*96)
#define VT_SUB_ENGLISH_START                     (11*96+1*13)
#define VT_SUB_ESTONIAN_START                    (11*96+2*13)
#define VT_SUB_FRENCH_START                      (11*96+3*13)
#define VT_SUB_GERMAN_START                      (11*96+4*13)
#define VT_SUB_ITALIAN_START                     (11*96+5*13)
#define VT_SUB_LETTISH_LITHUANIAN_START          (11*96+6*13)
#define VT_SUB_POLISH_START                      (11*96+7*13)
#define VT_SUB_PORTUGESE_SPANISH_START           (11*96+8*13)
#define VT_SUB_RUMANIAN_START                    (11*96+9*13)
#define VT_SUB_SERBIAN_CROATIAN_SLOVENIAN_START  (11*96+10*13)
#define VT_SUB_SWEDISH_FINNISH_START             (11*96+11*13)
#define VT_SUB_TURKISH_START                     (11*96+12*13)

#define VT_BLOCK_START    (4*(11*96+13*13))
#define VT_SMOOTH_START   (4*(11*96+13*13)+2*96)
#endif
