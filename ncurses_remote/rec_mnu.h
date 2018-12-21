#ifndef __rec_mnu_h
#define __rec_mnu_h
#include "app_data.h"
#include "rs_entry.h"

int edit_rse (RsEntry * e, AppData * a);
void select_re_type (RSType * type, AppData * a);       ///<also used by reminders
void select_re_days (uint8_t * type, AppData * a);      ///<also used by reminders

#endif
