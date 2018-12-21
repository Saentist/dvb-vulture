#ifndef __sw_set
#define __sw_set
#include "switch.h"

/**
 *\file sw_set.h
 *\brief Input switch/LNB setup
 */

/**
 *\brief setup switch for dvb-s device
 *
 *generates the necessary commands to set up the switch/LNB for requested position,frequency,and polarisation
 *\param freq absolute frequency in kHz
 */
int spDoSwitch (SwitchPos * sp, uint32_t size, uint32_t pos, uint32_t freq,
                uint8_t pol, int dev);

/**
 *\brief generate intermediate frequency
 *
 *\param freq absolute frequency in kHz
 *\param if_ret returns intermediate frequency in kHz here
 */
int spGetIf (SwitchPos * sp, uint32_t size, uint32_t pos, uint32_t freq,
             uint32_t * if_ret);

#endif
