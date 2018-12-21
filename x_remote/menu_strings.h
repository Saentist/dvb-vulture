#ifndef __menu_strings_h
#define __menu_strings_h

#include "pgm_info.h"
#include "tp_info.h"
#include "epg_evt.h"
#include "vt_pk.h"
#include "rs_entry.h"
#include "fav.h"

void mnuStrAsciiSanitize (char *p);
char *mnuStrSvc (uint8_t svc_type);
void mnuStrFree (char **s, int sz);
char **mnuStrFav (Favourite * full, int sz);
char **mnuStrRse (RsEntry * rse, int sz);
char **mnuStrPgms (ProgramInfo * full, int sz);
char **mnuStrTp (TransponderInfo * full, int sz);
int mnuStrGetH (uint32_t t);
int mnuStrGetM (uint32_t t);
int mnuStrGetS (uint32_t t);
char **mnuStrSched (EpgSchedule * sched);
char **mnuStrVtPid (uint16_t * vtidx, int num_strings);
char **mnuStrVtSvc (uint8_t * vtidx, int num_strings);
char **mnuStrRemList (DList * l, int *sz_ret);

#endif
