#ifndef __vtctx_h
#define __vtctx_h
#include <stdint.h>

#include "vt_fonts.h"
#include "vt_cmap.h"
#include "vt_pk.h"
/**
 *\file vtctx.h
 *\brief vt parsing context
 *
 *keeps track of character attributes, colours etc <br>
 */

#define VT_DATA_START 4
#define HDR_DATA_START 12
#define ENH_DATA_START (VT_DATA_START+1)

/**
 *\brief single character cell
 *
 */
typedef struct
{
  unsigned fg_color:5;
  unsigned bg_color:5;
  unsigned flash_mode:2;
  unsigned flash_rate:2;
  unsigned invert:1;            ///<inverted color
  unsigned conceal:1;
  unsigned window:1;            ///<box/window
  unsigned e_size:2;            ///<size of characters 0: normal, 1: dbl hght, 2: dbl width, 3: dbl sz
  unsigned underline:1;         ///<and separate mosaics
  unsigned italics:1;
  unsigned bold:1;
  unsigned charset:7;
  unsigned ext_char:1;          ///<character was placed by enhancement data, affects glyph selection
  unsigned character:7;         ///<character code
  unsigned attr:6;              ///<spacing attribute value plus one. zero: no attribute
  unsigned is_mosaic:2;         ///<0:g0,1:g1 for alpha/mosaic spacing attr 2:g2 supplementary set 3:g3 smooth line drawing set (for non spacing attr)
  unsigned diacrit:4;           ///<diacritics
  unsigned at_char:1;           ///<place an at @ (at) character here
  unsigned is_drcs:1;           ///<drcs character
  unsigned drcs_normal:1;       ///<use global(0) or normal(1) drcs page
  unsigned drcs_sub:4;          ///<drcs sub_table to use
//      unsigned opacity:1;
//      unsigned enh_flag;1;
} VtCell;

///A pattern transfer unit used for redefinable characters
typedef struct
{
        /**
	 *\brief set by packet 28/3, default to zero if no 28/3
	 *<TABLE>
	 *<TR>
	 *<TD>0</TD><TD>12 x 10 x 1</TD>
	 *</TR>
	 *<TR>
	 *<TD>1</TD><TD>12 x 10 x 2</TD>
	 *</TR>
	 *<TR>
	 *<TD>2</TD><TD>12 x 10 x 4</TD>
	 *</TR>
	 *<TR>
	 *<TD>3</TD><TD>6 x 5  x 4</TD>
	 *</TR>
	 *</TABLE>
	 *Subsequent PTU of a Mode 1 or 2 character
	 *No data for the corresponding character
	 */
  unsigned mode:4;
  uint8_t data[20];             ///<drcs pixel data
} VtDRCSPTU;

///compositional link
typedef struct
{
  unsigned func:2, valid:2, pgnu:4, pgnt:4, magrel:3, present:1;
  uint16_t sub;
} VtCLink;

/**
 *\brief the object's context. each object has one, possibly partially copied from it's parent.
 *
 *I guess we need a virtual screen per object, render it there and combine them afterwards according to object type.
 *also requires an opacity bit per cell which gets set when we put a char. needed for passive objects.
 *also requires an additional enh_flag per cell which gets set on col enh triplet. needed for adaptive objects.
 *
 *libzvbi does it directly, but they rely on triplets being in ascending column order. The standard does specify this, after all.
 *Font style is problematic, multiple rows.
 *we'll try that, too
 */
typedef struct VtObjCtx_s VtObjCtx;
struct VtObjCtx_s
{
  uint8_t org_x;                ///<offset of object, default to invocation pos, but may be changed by origin mod before invocation.
  uint8_t org_y;
  uint8_t pos_x;                ///<active position (relative to origin)
  uint8_t pos_y;
  int8_t next_org_x;            ///<set by origin modifier, -1 if unset
  int8_t next_org_y;
  uint8_t type;                 ///<0: local enh, 1: active, 2: adaptive, 3: passive
  uint8_t fg_color;             ///<current fg color index
  uint8_t bg_color;             ///<current bg color index
  unsigned flash_mode:2;
  unsigned flash_rate:3;
  unsigned phase_ctr:2;
  unsigned e_size:2;            ///<size of characters 0: normal, 1: dbl hght, 2: dbl width, 3: dbl sz
  unsigned window:1;            ///<box/window
  unsigned invert:1;            ///<box/window
  unsigned conceal:1;           ///<conceal flag
  unsigned underline:1;         ///<guess we need yet another charset for those...
  unsigned italic:1;            ///<italics characters
  unsigned bold:1;              ///<bold chars
  unsigned prop:1;              ///<proportional font(How?)
//      uint8_t style_set///<if the style is valid(for adaptive objects)
  unsigned drcs_nsub:4;         ///<drcs sub_table to use. 0 on start of object
  unsigned drcs_gsub:4;         ///<drcs global sub_table to use. 0 on start of object
  unsigned mod_charset:7;       ///<modified G0/G2 charset for x26 packets col triplet 01000, reinitialize to defaults for each row
//      unsigned mod_charset_set:1;///<if the mod charset is valid(for adaptive objects)


  unsigned character:7;
  unsigned diacrit:4;
  unsigned at_char:1;
  unsigned is_mosaic:2;
  unsigned is_drcs:1;
  unsigned drcs_normal:1;


  unsigned fg_color_set:1;      ///<if the fg color is valid
  unsigned bg_color_set:1;      ///<if the bg color is valid
//      unsigned attr_set:1;///<attrs are valid
  unsigned underline_set:1;     ///<attrs are valid
  unsigned char_set:1;          ///<if a char was set
  unsigned flash_set:1;         ///<if the flash is valid
  unsigned invert_set:1;        ///<if the invert is valid
  unsigned window_set:1;        ///<if the window is valid
  unsigned size_set:1;          ///<if the size is valid
  unsigned conceal_set:1;       ///<if the conceal is valid
};

typedef struct
{
  void *d;                      ///<client data
  uint8_t *(*get_pk_cb) (void *d, int x, int y, int dc);        ///<called to get single packet
  uint8_t *(*get_pg_cb) (void *d, int magno, int pgno, int subno, int *num_ret);        ///<called to get all packets for a page
  uint8_t *(*get_dpg_cb) (void *d, int magno, int pgno, int subno, int *num_ret);       ///<called to get all packets for a data page (MOT/POP/GPOP/DRCS/MIP) subno is the s1 code of the subpage

  uint8_t clut[32][3];          ///<color map entries are rgb values with range 0..15 max. entry 8 is transparent and meaningless
  unsigned left_panel_active:1; ///<1 if the left panel is active
  unsigned right_panel_active:1;        ///<1 if right panel active
  unsigned panel_status:1;      ///<panel status 0: req at 3.5 only 1: req at 2.5 and 3.5
  unsigned panel_cols:5;        ///<number of columns in the left panel
  unsigned default_screen_color:5;
  unsigned default_row_color:5;
  unsigned black_bg_subst:1;    ///<use full row color for black background spacing attr 1/C
  unsigned clut_remap:3;        ///<which cluts to use for spacing color attributes

  uint8_t pos_x;                ///<cursor position for basic LOP(no panels added) we also set these for enhancement packets. Is this right?
  uint8_t pos_y;
  uint8_t fg_color;             ///<current fg color index
  uint8_t bg_color;             ///<current bg color index
  unsigned is_mosaic:1;         ///<0:g0,1:g1 for alpha/mosaic spacing attr
  unsigned conceal:1;           ///<display spaces unless...
  unsigned flash:1;
  unsigned mosaics_separate:1;  ///<guess we need yet another charset for those...
  unsigned g0_esc:1;            ///<alternate g0 set active
  unsigned e_size:2;            ///<size of characters 
  unsigned c5:1;                ///<for box
  unsigned c6:1;                ///<for box
  unsigned default_charset:7;   ///<default charset
//      unsigned csd:4;///<character set designation(table 32) default to latin
//      unsigned nos:3;///<national option selection(table 32)
  unsigned sld:7;               ///<second language designation (table33) default to latin
  unsigned box_state:2;         ///<end2 seen, start1 seen, start2 seen, end1 seen
  uint8_t last_mosaic_char;     ///<mosaic char for hold mosaics
  unsigned last_mosaic_separate:1;      ///<hold mosaics separated (underline attribute?)
  unsigned m_hold:1;            ///<0: inactive 1:hold mosaics active
  uint32_t skip_lines;          ///<bitfield with bits set for lines to skip due to double height


  unsigned drcs_nsub:4;         ///<drcs sub_table to use
  unsigned drcs_gsub:4;         ///<drcs global sub_table to use


        /**
	 *\brief 26 rows by 72 columns virtual character display
	 *
	 */
  VtCell screen[26][72];
        /**
	 *\brief color(s) to use instead of black background for rows
	 */
  uint8_t full_row_color[26];

  uint32_t links[24];           ///<editorial links

  unsigned magno:3;             ///<mag number
  unsigned pgu:4;               ///<page units
  unsigned pgt:4;               ///<page tens
  uint16_t subno;               ///<subpage number

  VtCLink gpop_link;
  uint8_t def_gobj_flags;
  uint8_t def_gobj[2];
  VtCLink pop_link;
  uint8_t def_obj_flags;
  uint8_t def_obj[2];
  VtCLink gdrcs_link;
  VtCLink drcs_link;

  uint8_t *pop_pg[2][16];       ///<pop[1]/gpop[0], 16 sub pages each, demand loaded.
  int num_pop_pk[2][16];        ///<sizes of pop/gpop, 16 sub pages each, demand loaded.

  uint8_t dclut4g[4];           ///<colors for global drcs characters 2 bit
  uint8_t dclut4n[4];           ///<colors for normal drcs characters 2 bit
  uint8_t dclut16g[16];         ///<colors for global drcs characters 4 bit
  uint8_t dclut16n[16];         ///<colors for normal drcs characters 4 bit        
  VtDRCSPTU drcs[2][16][48];    ///<glob/local drcs, 16 sub pages, 48 ptus each
  uint32_t drcs_loaded;         ///<which drcs subtables have been loaded in above array. 32 bits, one for each of the 2*16 tables

  uint8_t *pg;                  ///<current page data
  int num_pk_pg;                ///<number of packets in page buffer
} VtCtx;

/**
 *\brief initializes parser
 *
 *sets up callback functions and client pointer
 */
int vtctxInit (VtCtx * ctx, void *d,
               uint8_t * (*get_pk_cb) (void *d, int x, int y, int dc),
               uint8_t * (*get_pg_cb) (void *d, int magno, int pgno,
                                       int subno, int *num_ret),
               uint8_t * (*get_dpg_cb) (void *d, int magno, int pgno,
                                        int subno, int *num_ret));
/**
 *\brief request and process page and associated packets
 */
int vtctxParse (VtCtx * ctx, int page, int sub);
/**
 *\brief start rendering the page
 *
 *call after vtctxParse to paint the page<br>
 *may be called multiple times with different parameters
 *
 *\param state 0-11 to select wanted blink state
 *\param reveal to display concealed text
 *\param buffer must point to a buffer of 260*672 bytes to hold output. each byte holds a palette index 0-31.
 */
int vtctxRender (VtCtx * ctx, uint8_t state, uint8_t reveal,
                 uint8_t * buffer);
/**
 *\brief get palette pointer
 *
 *\return the contexts palette pointer. a buffer of 32*3 bytes to hold palette. 
 *rgb values in range 0-15 each. 
 *entry 8 is transparent. 
 *It is the same for all blink states/revals. 
 *Valid after vtctxParse.
 */
uint8_t const *vtctxPal (VtCtx * ctx);
/**
 *\brief call this to clear stored packet data
 *
 *
 */
void vtctxFlush (VtCtx * ctx);
/**
 *\brief Completely clears parser
 *
 *
 */
void vtctxClear (VtCtx * ctx);
#endif
