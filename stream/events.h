#ifndef __events_h
#define __events_h
#include <stdint.h>

typedef enum
{
  E_IDLE,                       ///<not tuned
  E_TUNING,                     ///<about to tune, carries TuneDta
  E_TUNED,                      ///<tuned, carries TuneDta
  E_CLOSING,                    ///<output is closing
  E_NEWPIDS,                    ///<pids have changed
  E_SHUTDOWN,                   ///<dvb_s shutting down
  E_PNR_ADD,                    ///<a pnr has been added, carries pnr
  E_PNR_RMV,                    ///<a pnr has been removed, carries pnr
  E_CONFIG,                     ///<frontend was reconfigured, purge all databases as the switch positions may have changed. carries num_pos
  E_ALL_VT,                     ///<tell vt module to scan all vt pids on tp
  E_ALL_MHEG                    ///<tell mheg module to scan all vt pids on tp
} DvbEvt;

typedef struct
{
  uint32_t pos;
  uint32_t freq;
  uint32_t srate;
  uint8_t pol;
} TuneDta;

/**
 *E_NEWPIDS
 *First, send pnr-associated pids
 *then global pids
 *may be multiple packets
 *last one has is_last set
 */
typedef struct
{
  uint16_t is_last;             ///<last packet of pidchange if set
  uint16_t has_pnr;             ///<pids associated with pnr, otherwise global pids.
  uint16_t pnr;                 ///<program pids are associated with
  uint16_t num_pids;            ///<1-40
  uint16_t pids[40];            ///<the pids
  uint16_t funcs[40];           ///<meaningful with pnr-assoc pids/functions
} PidDta;

typedef union
{
  TuneDta tune;
  PidDta pid;
  uint16_t pnr;
  uint32_t num_pos;
} DvbEvtD;

typedef struct
{
  DvbEvt id;
  DvbEvtD d;
} DvbEvtS;



/**
 *\brief returns event's id
 */
#define selectorEvtId(_P) ((((SelBuf *)(_P)))->event.id)
/**
 *\brief returns pointer to event's data
 */
#define selectorEvtDta(_P) (&((((SelBuf *)(_P)))->event.d))

#endif
