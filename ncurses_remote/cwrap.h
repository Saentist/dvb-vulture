#ifndef __cwrap_h
#include "client.h"

/**
 *\brief wrapper for client... calls
 *to handle SIGWINCH properly...
 *this 'fix' is terrible...
 */

int cwDoTune (int sock, uint32_t pos, uint32_t freq, uint8_t pol,
              uint32_t pnr);
int cwRecSchedAdd (int sock, RsEntry * e);
EpgArray *cwGetEpg (int sock, uint32_t pos,
                    uint32_t frequency, uint8_t pol,
                    uint16_t * pgms, uint16_t num_pgm,
                    uint32_t start, uint32_t end, uint32_t num_entries);
VtPk *cwVtGetSvcPk (int sock, uint32_t pos, uint32_t frequency,
                    uint8_t pol, uint16_t pid, uint8_t data_id,
                    uint16_t * num_pk);
VtPk *cwVtGetMagPk (int sock, uint32_t pos, uint32_t frequency,
                    uint8_t pol, uint16_t pid, uint8_t data_id,
                    uint8_t magno, uint16_t * num_pk);
VtPk *cwVtGetPagPk (int sock, uint32_t pos, uint32_t frequency,
                    uint8_t pol, uint16_t pid, uint8_t data_id,
                    uint8_t magno, uint8_t pgno, uint16_t subno,
                    uint16_t * num_pk);
uint8_t *cwVtGetSvc (int sock, uint32_t pos, uint32_t frequency,
                     uint8_t pol, uint16_t pid, uint16_t * num_svc);
TransponderInfo *cwGetTransponders (int sock, uint32_t pos,
                                    uint32_t * sz_ret);
ProgramInfo *cwGetPgms (int sock, uint32_t pos, uint32_t frequency,
                        uint8_t pol, uint32_t * sz_ret);
int cwScanTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol);
uint16_t *cwVtGetPids (int sock, uint32_t pos, uint32_t frequency,
                       uint8_t pol, uint16_t pnr, uint16_t * num_pids);
int cwRecordStart (int sock);
int cwRecordStop (int sock);
uint32_t cwListPos (int sock);
int cwGetSched (int sock, RsEntry ** rs_ret, uint32_t * sz_ret);
int cwRecSched (int sock, RsEntry * new_sch, uint32_t new_sz,
                RsEntry * prev_sch, uint32_t prev_sz);
int cwRecSAbort (int sock);
int cwGetSwitch (int sock, SwitchPos ** sp, uint32_t * sz_ret);
int cwSetSwitch (int sock, SwitchPos * sp, uint32_t num);
int cwGetTstat (int sock, uint8_t * valid, uint8_t * tuned, uint32_t * pos,
                uint32_t * freq, uint8_t * pol, int32_t * ft, int32_t * fc,
                uint32_t * ifreq);
int cwGetStatus (int sock, uint8_t * status_ret, uint32_t * ber_ret,
                 uint32_t * strength_ret, uint32_t * snr_ret,
                 uint32_t * ucblk_ret);
int cwFtTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol, int32_t ft);
int cwSetFcorr (int sock, int32_t fc);
uint8_t cwGoActive (int sock);
uint8_t cwGoInactive (int sock);

#endif
