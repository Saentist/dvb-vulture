#ifndef __pgmstate_h
#define __pgmstate_h
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
#include "vtctx.h"
#include "tp_info.h"
#include "pgm_info.h"
#include "epg_evt.h"
#include "swap.h"
#include "list.h"
#include "rcfile.h"
#include "switch.h"
#include "reminder.h"
#include "fav_list.h"

/*
 perhaps organize the dialogs as a sort of tree
*/
///main program state
typedef struct
{
  Swap s;                       ///<the swapper for (hopefully smooth) vt output
  VtCtx vtc;                    ///<the vt ctx
  RcFile cfg;                   ///<the configuration file
  int buf_valid;                ///<The drawables are actually there. 0 if not
  Drawable buffers[12];         ///<drawables to hold our images
  int idx;                      ///<current index in buffers array
  GC gc;                        ///<graphics context used to transfer images
  int should_animate;           ///<used to enable the background proc
  int sock;                     ///<communication socket
  uint32_t pos;
  uint8_t pol;
  uint32_t frequency;
  uint16_t pid;
  uint8_t svc_id;
  int pageno;
  int subno;
  unsigned long pixel_lut[32];

  Widget messages;              ///<message window

  Widget main_form, switch_form, mlist_form, favlist_form, setup_form, stats_form, rmd_form, sched_form, ed_sched_form; //forms to switch between
  Widget current;               ///<used to return to main menu
//      Widget placeholder;///<used to establish a minimum size for form as formWidget likes to ignore height and width. forget it. We use a form (mlist_form) to do this

  //master pgm list display
  Widget tp_list;
  Widget pgm_list;
  Widget pos_list;

  Widget vt;                    ///<VT display area
  char vt_buf[4];               ///<input buffer for page input
  int vt_buf_idx;               ///<input buffer position
  int vt_act;                   ///<input sensitive
  Widget pg_lbl, sub_lbl;       ///<display page and sub-page index

  uint32_t num_pos;
  int pos_idx;

  int tp_idx;
  TransponderInfo *transponders;
  int num_transponders;

  int pgm_idx;
  ProgramInfo *programs;
  int num_programs;

  Widget desc_txt;              ///<holds event description
  Widget evt_list;              ///<EPG event list
  int evt_idx;
  uint16_t pnr;
  EpgArray *epga;

  Widget flist_list, fav_list;
  BTreeNode *favourite_lists;   ///<fav lists sorted by favlist name, has ownership of lists
  int flist_loaded;
  FavList *current_fav_list;    ///<currently used favlist
  int fav_loaded;
  int fav_idx;                  ///<index into list


  Widget sched_list;            ///<list for rec schedule
  int sched_idx;                ///<index into sched list
  int sched_loaded;
  uint32_t pr_sch_sz;
  RsEntry *pr_sch;              ///<previous rec schedule
  uint32_t r_sch_sz;
  RsEntry *r_sch;               ///<current rec schedule

  DList reminder_list;
  Widget rmd_list;
  int rmd_idx;
  int rmd_loaded;

  Widget mod_tp_form, mod_tp_tp_list, mod_tp_pos_list, ft_txt;

  Widget switch_dlg_preset_list, switch_dlg_pos_list, itf_txt;
  Widget switch_dlg_band_list, switch_dlg_pol_list, switch_dlg_cmd_list;
  Widget switch_dlg_cmd_detail, switch_dlg_cmd_type, switch_dlg_pol_sel;
  Widget switch_dlg_lof, switch_dlg_uf, switch_dlg_lf, switch_dlg_byte;

  SwitchPos *sp;
  int pos_loaded;
  int band_idx;
  int bands_loaded;
  int pol_idx;
  int pols_loaded;
  int cmd_idx;
  int cmds_loaded;
  int dtl_idx;
  int dtls_loaded;
  int byte_loaded;
  int preset_idx;
//      int valid;///<1 if parameters are valid, zero otherwise
  Widget parent;                ///<parent dialog to Map/Unmap for vt/epg
  Widget vt_form;               ///<vt dialog to Map(the form)
  Widget epg_form;              ///<EPG dialog to Map

  DvbString *svc_name;          ///<for epg to reminder...

  Widget sched_ed_type,
    sun, mon, tue, wed, thu, fri, sat,
    f_vt, f_spu, f_dsm, f_eit, f_aud,
    sched_ed_pos, sched_ed_pol, sched_ed_freq, sched_ed_pnr,
    sched_ed_y, sched_ed_m, sched_ed_d, sched_ed_dr,
    sched_ed_nm, sched_ed_eit, sched_ed_hr, sched_ed_min, sched_ed_sec;
  RsEntry *rse;

  Boolean rmd_auto;

  Widget rmd_ed_form,
    rmd_sun, rmd_mon, rmd_tue, rmd_wed, rmd_thu, rmd_fri, rmd_sat,
    rmd_ed_type;
  DListE *rmd;

  Widget fe_ft_form, fc_txt, fc_stat_l;
  uint8_t fe_status;
  uint32_t fe_ber, fe_ucb, fe_sig, fe_snr;
  int fe_vis;
} PgmState;

#endif //__pgmstate_h
