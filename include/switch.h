#ifndef __switch_h
#define __switch_h

#include <stdint.h>
#ifndef __WIN32__
#include <linux/dvb/frontend.h>
#else
typedef enum fe_sec_voltage
{
  SEC_VOLTAGE_13,
  SEC_VOLTAGE_18,
  SEC_VOLTAGE_OFF
} fe_sec_voltage_t;

typedef enum fe_sec_tone_mode
{
  SEC_TONE_ON,
  SEC_TONE_OFF
} fe_sec_tone_mode_t;

typedef enum fe_sec_mini_cmd
{
  SEC_MINI_A,
  SEC_MINI_B
} fe_sec_mini_cmd_t;

#endif
#include "custck.h"
/**
 *\file switch.h
 *\brief manages antenna switch control
 *
 *Switch control is organized in a Tree like fashion
 *a dvb-s adapter contains at least one switch pos. every switch pos contains at least one switch band.
 *each switch band contains at least one switch pol. every switch pol contains the commands necessary to set up the
 *respective combination of pos/band/pol
 *
 *SwitchCmd:
 *\li for SC_TONE/SC_BURST/SC_VOLTAGE data[0] indicate the value to set
 *
 *\li SC_TONE:
 *	SEC_TONE_ON=0,
 *	SEC_TONE_OFF=1
 *
 *\li SC_BURST:
 *	SEC_MINI_A=0
 *	SEC_MINI_B=1
 *
 *\li SC_VOLTAGE:
 *	SEC_VOLTAGE_13=0
 *	SEC_VOLTAGE_18=1
 *	SEC_VOLTAGE_OFF=2
 *(they're just passed to driver funcs and may change if definition in header changes)
 *
 *\li SC_WCMD:
 *the len field is the number of DiSEqC bytes to send.
 *the data portion of the SwitchCmd is also relevant for DiSEqC commands.
 *
 *\li SC_WCMD_F:
 *The diseqc Write Freq(0x58) command. The first two data[] bytes must be the diseqc framing and address bytes.
 *
 *\li For SC_DELAY, data[0] represents a timeout in milliseconds to wait before continuing, with a maximum of 255ms.
 *
 *\warning initialisation function in this file assume ownership of their array arguments. 
 *They must be acquired using malloc/calloc and will be freed by a call to the ..Clear function.
 */

#define SC_TONE				0
#define SC_BURST			1
#define SC_VOLTAGE		2
#define SC_WCMD				3
#define SC_WCMD_F				4
//#define SC_XCMD                               4
#define SC_DELAY			5

#define SP_NUM_CMD 6
/**
 *\brief represents a switch/LNB command
 */
typedef struct
{
  uint8_t data[8];
  uint8_t len;
  uint8_t what;
} SwitchCmd;

/**
 *\brief represents a polarisation
 *forms the bottom of the switching tree and holds commands to setup the input
 */
typedef struct
{
  uint32_t num_cmds;
  uint8_t pol;                  ///<0=H 1=V 2=L 3=R
  SwitchCmd *cmds;
} SwitchPol;

/**
 *\brief represents a single, contingous frequency range
 */
typedef struct
{
  uint32_t lof;                 ///<local oscillator frequency in kHz
  uint32_t f_min;               ///<lower bound of frequency band in kHz
  uint32_t f_max;               ///<upper bound of frequency (not inclusive) band in kHz
  uint32_t num_pol;             ///<number of polarizations 
  SwitchPol *pol;
} SwitchBand;

/**
 *\brief holds a single switch position with associated frequency ranges and polarisations
 */
typedef struct
{
  uint32_t num_bands;           ///<number of frequency bands
  SwitchBand *bands;
  char *initial_tuning_file;    ///<to read initial transponders from
} SwitchPos;

/**
 *\brief initializes a SwitchPos structure
 *
 *\warning assumes ownership of the passed pointers(bands,initial_tuning_file). Must be malloc()/calloc()'ed and will be free()'d by spClear.
 */
int spInit (SwitchPos * sp, uint32_t num_bands, SwitchBand * bands,
            char *initial_tuning_file);
int CUspInit (SwitchPos * sp, uint32_t num_bands, SwitchBand * bands,
              char *initial_tuning_file, CUStack * s);
void CUspClear (void *sp);
void spClear (SwitchPos * sp);

int spPolDeepCopy (SwitchPol * dst, SwitchPol * src);
int spBandDeepCopy (SwitchBand * dst, SwitchBand * src);
int spDeepCopy (SwitchPos * dst, SwitchPos * src);
/**
 *\brief deep copies the SwitchPos array
 *
 *\return pointer to newly allocated SwitchPos array, zero on error.
 */
SwitchPos *spClone (SwitchPos * sp, uint32_t num_sp);

///write the whole tree to disk
int spWrite (SwitchPos * sp, FILE * f);
///read it from disk
int spRead (SwitchPos * sp, FILE * f);
///send it over network(tcp stream) socket
int spSnd (SwitchPos * sp, int sock);
///receive it from network
int spRcv (SwitchPos * sp, int sock);

int spBandInit (SwitchBand * sb, uint32_t lof, uint32_t f_min, uint32_t f_max,
                uint32_t num_pol, SwitchPol * pol);
int spBandClear (SwitchBand * sb);

int spPolInit (SwitchPol * sp, uint8_t pol, uint32_t num_cmds,
               SwitchCmd * cmds);
int spPolClear (SwitchPol * sp);

///remove entry from array, deallocating any descendants. idx may be at most num_sp-1.
int spDel (SwitchPos ** sp, uint32_t * num_sp, uint32_t idx);
int spBandDel (SwitchBand ** sp, uint32_t * num_sp, uint32_t idx);
int spPolDel (SwitchPol ** sp, uint32_t * num_sp, uint32_t idx);
int spCmdDel (SwitchCmd ** sp, uint32_t * num_sp, uint32_t idx);

///add entry to array at idx, existing entries may be moved backwards. idx may be at most num_sp, inserting at the end.
int spAdd (SwitchPos ** sp, uint32_t * num_sp, uint32_t idx);
int spBandAdd (SwitchBand ** sp, uint32_t * num_sp, uint32_t idx);
int spPolAdd (SwitchPol ** sp, uint32_t * num_sp, uint32_t idx);
int spCmdAdd (SwitchCmd ** sp, uint32_t * num_sp, uint32_t idx);

typedef enum
{
  SP_P_UNIVERSAL_LNB = 0,       ///<99% of cases
  SP_P_DBS = 1,                 ///<probably not useful in Europe
  SP_P_STANDARD = 2,            ///<probably too old
  SP_P_ENHANCED = 3,            ///<what's this?
  SP_P_CBAND = 4,               ///<probably not useful in Europe
  SP_P_UNIVERSAL_DUAL_LNB_BURST = 5,    ///<Oh, yes this one is great. I just do not know if it actually works. Should work for those twin LNBs that stare at two different satellites.
  SP_P_DVBT = 6                 ///<useful for ... dvb-t. Actually dvbt tuning only uses the initial tuning file, and that has to be manually configured anyway.
} sp_preset_e;

#define SP_NUM_PRESETS 7

///generates a SwitchPos array representing the requested configuration
SwitchPos *spGetPreset (sp_preset_e id, uint32_t * num_ret);
///returns pointer to (constant) preset name. don't free()
char const *spGetPresetStrings (sp_preset_e id);
///returns pointer to const command name given a SC_... 
char const *spGetCmdStr (uint8_t id);
#endif
