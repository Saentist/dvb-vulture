#ifndef __dvb_h
#define __dvb_h

#include <linux/dvb/version.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include "rcfile.h"
#include "events.h"
#include "custck.h"
#include "svt.h"
#include "pes_ex.h"
#include "sec_ex.h"
#include "trn_strm.h"
#include "task.h"
#include "sw_set.h"

/**
 *\file dvb.h
 *\brief dvb hardware interfacing
 *
 *dvb_s contains a demuxer which receives all requested pids from TS_TAP
 *dvb_s has a function to request a port delivering a set of pids and services.
 *dvb_s has a function to release a port. If no port uses pid anymore, it is removed from filter chain.
 *Users that need a sec fiter may request a secextractor directly
 *Users that need a pes fiter may request a pesextractor directly
 *we may also inject events into the datastream indicating for example: tsid changed, idle state(not tuned or failed), possibly more
 *\see events.h
 *the id in packets payload indicates whether it's event or ts packet. 
 *If it's zero it's a ts packet, 
 *If it's one it's an event.
 *
 *Can setup DiSEqC LNBs/switches on tuning as specified in positions array
 *
 */

typedef struct
{
  uint16_t pid;
  int fd;
} PidFd;

struct _dvb_s
{
  CUStack destroy;              ///<a kind of destructor
  Task t;
  SwDmx ts_dmx;                 ///<transports our TS_TAP packets to recipients
  char *switchconf;             ///<file to store the switch configuration
  uint32_t num_positions;       ///<the number of switch positions
  SwitchPos *positions;         ///<the switch positions
  uint32_t pos;                 ///<current position
  uint32_t freq;                ///<tuned freq
  uint32_t ifreq;               ///<intermediate or real frequency after processing( finetune/fcorr)
  int32_t ft;                   ///<transponder finetune
  uint32_t srate;               ///<sym rate
  uint8_t pol;                  ///<polarization
  int32_t f_corr;               ///<frequency correction value to use for frontend. gets added to frequency before tuning, signed value... in kHz for dvb-s, Hz for dvb-t
  unsigned valid:1;             ///<whether tuning parameters are valid(gets set on first tuning attempt even if it failed to lock)
  unsigned tuned:1;             ///<whether frontend is tuned
  int tune_wait_s;              ///<how long to wait for LOCK
  BTreeNode *pid_fds;           ///<our dmx file descriptors sorted by pid
  pthread_mutex_t fd_mx;        ///<protects the pid tree
  int adapter;                        /**< The adapter number ie /dev/dvb/adapter<#adapter> */
  int fe;                        /**< The frontend number ie /dev/dvb/adapter<#adapter>/frontend<#fe> */
  int dmx;                        /**< The demux number ie /dev/dvb/adapter<#adapter>/demux<#dmx> */
  int dvr;                        /**< The dvr number ie /dev/dvb/adapter<#adapter>/dvr<#dvr> */
  int num_buffers;              ///<how many buffers the selector can allocate
  unsigned long dmx_bufsz;

  struct dvb_frontend_info info;     /**< Information about the front end */
  int frontend_fd;                     /**< File descriptor for the frontend device */

  struct dvb_frontend_parameters frontend_parm;   /**< The current frontend configuration parameters. These may be updated when the frontend locks. */

  char demux_path[32];                 /**< Path to the demux device */
  char dvr_path[32];                 /**< Path to the dvr device */
  int dvr_fd;                          /**< File descriptor for the dvr device */

  void (*tuning_success) (uint32_t pos, uint32_t freq, uint32_t srate,
                          uint8_t pol, void *user);
  void *user;
  int niceness;
};

typedef struct _dvb_s dvb_s;

int dvbInit (dvb_s * d, RcFile * cfg,
             void (*tuning_success) (uint32_t pos, uint32_t freq,
                                     uint32_t srate, uint8_t pol, void *user),
             void *user);
void dvbClear (dvb_s * d);
int CUdvbInit (dvb_s * d, RcFile * cfg,
               void (*tuning_success) (uint32_t pos, uint32_t freq,
                                       uint32_t srate, uint8_t pol,
                                       void *user), void *user, CUStack * s);

/**
 *\brief tune the frontend dvb-s version
 *
 *\param freq true freq in 10 kHz units
 *\param srate symbol rate in symbols per second
 *\param pol 0 for horizontal 1 for vertical (I hope)
 *\param ft signed offset to frequency
 */
int dvbTuneS (dvb_s * adapter, __u32 pos, __u32 freq, __u32 srate, __u8 pol,
              __s32 ft, fe_rolloff_t roff, fe_modulation_t mod);
/**
 *\brief tune frontend dvb-t version
 *
 *\param freq true freq in 10 Hz units
 *loads of parameters...
 */
int dvbTuneT (dvb_s * adapter, __u32 pos, __u32 freq,
              fe_spectral_inversion_t inv, fe_bandwidth_t bw,
              fe_code_rate_t crh, fe_code_rate_t crl, fe_modulation_t mod,
              fe_transmit_mode_t mode, fe_guard_interval_t guard,
              fe_hierarchy_t hier, __s32 ft);

///closes demuxer
int dvbUnTune (dvb_s * adapter);
int dvbFeStatus (dvb_s * adapter, fe_status_t * status,
                 unsigned int *ber, unsigned int *strength,
                 unsigned int *snr, unsigned int *ucblock);
void dvbFeStatusBits (fe_status_t s);

/**
 *\brief lock the mutex
 */
//int dvbLock (dvb_s * adapter);
/**
 *\brief unlock the mutex
 */
//int dvbUnlock (dvb_s * adapter);

//demux interface
SvcOut *dvbDmxOutAdd (dvb_s * a);
void dvbDmxOutRmv (SvcOut * o);
int dvbDmxOutMod (SvcOut * o,
                  uint16_t n_funcs, uint16_t * pnrs, uint16_t * funcs,
                  uint16_t n_pids, uint16_t * pids);

//unlocked variants
SvcOut *dvbDmxOutAddU (dvb_s * a);
void dvbDmxOutRmvU (SvcOut * o);
int dvbDmxOutModU (SvcOut * o,
                   uint16_t n_funcs, uint16_t * pnrs, uint16_t * funcs,
                   uint16_t n_pids, uint16_t * pids);

int dvbTuned (dvb_s * a);       ///<return 1 if frontend is tuned
int dvbWouldTune (dvb_s * a, __u32 pos, __u32 freq, __u32 srate, __u8 pol, __s32 ft);   ///<return 1 if frontend would tune

/**
 *\brief set the adapters' SwitchPos array
 *
 *adapter assumes ownership of array
 */
int dvbSetSwitch (dvb_s * a, SwitchPos * sp, uint32_t num);

/**
 *\brief return the adapters' SwitchPos array
 *
 *\return NULL on error/not dvb-s/no positions configured
 */
SwitchPos *dvbGetSwitch (dvb_s * a, uint32_t * num_ret);

///inject a pnr add event
int dvbPnrAddEvt (dvb_s * a, uint16_t pnr);
///inject a pnr remove event
int dvbPnrRmvEvt (dvb_s * a, uint16_t pnr);

int dvbSetFCorr (dvb_s * a, int32_t corr);
int32_t dvbGetFCorr (dvb_s * a);

int dvbIsSat (dvb_s * d);
//int dvbIsSat2(dvb_s * d);
int dvbIsTerr (dvb_s * d);

int dvbGetTstat (dvb_s * a, uint8_t * valid, uint8_t * tuned, uint32_t * pos,
                 uint32_t * freq, uint8_t * pol, int32_t * ft, int32_t * fc,
                 uint32_t * ifreq);

#endif
