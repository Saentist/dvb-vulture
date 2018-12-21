#ifndef __menu_strings_h
#define __menu_strings_h
#include <menu.h>
#include "fav.h"
#include "list.h"
#include "pgm_info.h"
#include "tp_info.h"
#include "epg_evt.h"
#include "vt_pk.h"
#include "rs_entry.h"
void free_strings (char **s, int sz);
char **fav_to_strings (Favourite * full, int sz);
char **fav_to_names (Favourite * full, int sz);
char **pgms_to_strings (ProgramInfo * full, int sz);
char **tp_to_strings (TransponderInfo * full, int sz);
char **sched_to_strings (EpgSchedule * sched);
char **vt_pid_to_strings (uint16_t * vtidx, int num_strings);
char **vt_svc_to_strings (uint8_t * vtidx, int num_strings);
char **rse_to_strings (RsEntry * rse, int sz);
ITEM **rem_to_items (DList * l, int *sz_ret);
int rem_items_free (ITEM ** items);
void ascii_sanitize (char *p);
int get_h (time_t t);
int get_m (time_t t);
int get_s (time_t t);
#endif
