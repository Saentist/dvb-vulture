#ifndef __srv_codes_h
#define __srv_codes_h

#define CONN_ID 0x32f6eb10

enum server_cmd
{
  LIST_TP = 0,
  SCAN_TP = 1,
  LIST_PG = 2,
  TUNE_PG = 3,
  SET_SWITCH = 4,               ///<set switch configuration. This implicitly purges databases.
  GET_SWITCH = 5,               ///<send current switch configuration.
  NUM_POS = 6,
  ADD_TP = 7,
  DEL_TP = 8,
  EPG_GET_DATA = 9,
  GO_INACTIVE = 11,
  GO_ACTIVE = 12,
  TP_FT = 13,
  GET_STATS = 14,               ///<signal status: flags, snr,strength,ucb,ber...
  STREAM_STOP = 15,
  VT_GET_SVC_PK = 16,
  VT_GET_MAG_PK = 17,
  VT_GET_PAG_PK = 18,
  VT_GET_PNRS = 19,
  VT_GET_PIDS = 20,
  VT_GET_SVC = 21,
  REC_PGM = 22,
  REC_PGM_STOP = 23,
  REC_SCHED = 24,
  GET_SCHED = 25,
  GET_FCORR = 26,
  SET_FCORR = 27,
  ABORT_REC = 28,
  ADD_SCHED = 29,
  SRV_TST = 30,
  SRV_USTAT = 31,               //   TODO user status. return active, super flags, pnrs, if someone else is active, if recording in running.
  SRV_RSTAT = 32,               //   TODO status of running recordings. freq/pnr/name/filesizes/initiators
  SRV_TSTAT = 33,               ///<frontends transponder tuning status: if it is valid,tuned,freq,pos,finetune,fc etc...
//  SRV_GET_TPFT = 34 ///<TODO get finetune for tp
};

#define SRV_E_AUTH 4
#define SRV_E 1
#define SRV_OK 0

#endif
