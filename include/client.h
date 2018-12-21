#ifndef __client_h
#define __client_h

/**
 *\file client.h
 *\brief The interface towards the server
 *
 */
#include <stdint.h>
#include "vt_pk.h"
#include "switch.h"
#include "tp_info.h"
#include "rs_entry.h"
#include "epg_evt.h"
uint16_t *clientVtGetPids (int sock, uint32_t pos, uint32_t frequency,
                           uint8_t pol, uint16_t pnr, uint16_t * num_pids);
uint8_t *clientVtGetSvc (int sock, uint32_t pos, uint32_t frequency,
                         uint8_t pol, uint16_t pid, uint16_t * num_svc);
VtPk *clientVtGetSvcPk (int sock, uint32_t pos, uint32_t frequency,
                        uint8_t pol, uint16_t pid, uint8_t data_id,
                        uint16_t * num_pk);
VtPk *clientVtGetMagPk (int sock, uint32_t pos, uint32_t frequency,
                        uint8_t pol, uint16_t pid, uint8_t data_id,
                        uint8_t magno, uint16_t * num_pk);
VtPk *clientVtGetPagPk (int sock, uint32_t pos, uint32_t frequency,
                        uint8_t pol, uint16_t pid, uint8_t data_id,
                        uint8_t magno, uint8_t pgno, uint16_t subno,
                        uint16_t * num_pk);
uint32_t clientListPos (int sock);
TransponderInfo *clientGetTransponders (int sock, uint32_t pos,
                                        uint32_t * sz_ret);
ProgramInfo *clientGetPgms (int sock, uint32_t pos, uint32_t frequency,
                            uint8_t pol, uint32_t * sz_ret);
EpgArray *clientRcvEpgArray (int sock);
EpgArray *clientGetEpg (int sock, uint32_t pos,
                        uint32_t frequency, uint8_t pol,
                        uint16_t * pgms, uint16_t num_pgm,
                        uint32_t start, uint32_t end, uint32_t num_entries);
uint8_t clientGoActive (int sock);
uint8_t clientGoInactive (int sock);
int clientRecordStart (int sock);
int clientRecordStop (int sock);
int clientScanTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol);
int clientDoTune (int sock, uint32_t pos, uint32_t freq, uint8_t pol,
                  uint32_t pnr);
int clientInit (int af, char *ip_addr, uint16_t prt);
void clientClear (int sock);
int clientRecSchedAdd (int sock, RsEntry * e);
int clientGetSched (int sock, RsEntry ** rs_ret, uint32_t * sz_ret);
int clientRecSched (int sock, RsEntry * new_sch, uint32_t new_sz,
                    RsEntry * prev_sch, uint32_t prev_sz);
int clientRecSAbort (int sock);
int clientDelTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol);
int clientFtTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol,
                int32_t ft);
int clientGetSwitch (int sock, SwitchPos ** sp, uint32_t * sz_ret);
int clientSetSwitch (int sock, SwitchPos * sp, uint32_t num);
int clientGetFcorr (int sock, int32_t * fc_ret);
int clientSetFcorr (int sock, int32_t fc);
int clientGetStatus (int sock, uint8_t * status_ret,
                     uint32_t * ber_ret, uint32_t * strength_ret,
                     uint32_t * snr_ret, uint32_t * ucblk_ret);
int clientGetTstat (int sock, uint8_t * valid, uint8_t * tuned,
                    uint32_t * pos, uint32_t * freq, uint8_t * pol,
                    int32_t * ft, int32_t * fc, uint32_t * ifreq);
/*TODO
clientGetUstat
clientGetRstat
clientGetTpft
*/
#endif
