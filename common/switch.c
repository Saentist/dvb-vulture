#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "in_out.h"
#include "utl.h"
#include "switch.h"
#include "ipc.h"
#include "dmalloc_loc.h"
#include "debug.h"

int DF_SWITCH;
#define FILE_DBG DF_SWITCH
//------------------------------------------the universal LNB preset--------------------------------

///hiband, Horizontal pol
static SwitchCmd SP_PRE_U_H[2] = {
  [0] = {
         .data[0] = SEC_VOLTAGE_18,
         .len = 1,
         .what = SC_VOLTAGE},

  [1] = {
         .data[0] = SEC_TONE_ON,
         .len = 1,
         .what = SC_TONE}
};

///hiband, Vertical pol
static SwitchCmd SP_PRE_U_V[2] = {
  [0] = {
         .data[0] = SEC_VOLTAGE_13,
         .len = 1,
         .what = SC_VOLTAGE},

  [1] = {
         .data[0] = SEC_TONE_ON,
         .len = 1,
         .what = SC_TONE}
};

///loband, Horizontal pol
static SwitchCmd SP_PRE_L_H[2] = {
  [0] = {
         .data[0] = SEC_VOLTAGE_18,
         .len = 1,
         .what = SC_VOLTAGE},

  [1] = {
         .data[0] = SEC_TONE_OFF,
         .len = 1,
         .what = SC_TONE}
};

///loband, Vertical pol
static SwitchCmd SP_PRE_L_V[2] = {
  [0] = {
         .data[0] = SEC_VOLTAGE_13,
         .len = 1,
         .what = SC_VOLTAGE},

  [1] = {
         .data[0] = SEC_TONE_OFF,
         .len = 1,
         .what = SC_TONE}
};

///hiband
static SwitchPol SP_PRE_UNIV_U[2] = {

  [0] = {
         .num_cmds = sizeof (SP_PRE_U_H) / sizeof (SwitchCmd),
         .cmds = SP_PRE_U_H,
         .pol = 0}
  ,

  [1] = {
         .num_cmds = sizeof (SP_PRE_U_V) / sizeof (SwitchCmd),
         .cmds = SP_PRE_U_V,
         .pol = 1}

};

///loband
static SwitchPol SP_PRE_UNIV_L[2] = {
  [0] = {
         .num_cmds = sizeof (SP_PRE_L_H) / sizeof (SwitchCmd),
         .cmds = SP_PRE_L_H,
         .pol = 0}
  ,
  [1] = {
         .num_cmds = sizeof (SP_PRE_L_V) / sizeof (SwitchCmd),
         .cmds = SP_PRE_L_V,
         .pol = 1}
};


/*
        "Europe",
        "10800 to 11800 MHz and 11600 to 12700 Mhz",
        "Dual LO, loband 9750, hiband 10600 MHz",
*/

///univ lnb
static SwitchBand SP_PRE_UNIV_B[2] = {
  [0] = {
         .lof = 9750000,
         .f_min = 10700000,
         .f_max = 11700000,
         .num_pol = sizeof (SP_PRE_UNIV_L) / sizeof (SwitchPol),
         .pol = SP_PRE_UNIV_L}
  ,

  [1] = {
         .lof = 10600000,
         .f_min = 11700000,
         .f_max = 12800000,
         .num_pol = sizeof (SP_PRE_UNIV_U) / sizeof (SwitchPol),
         .pol = SP_PRE_UNIV_U}

};

static SwitchPos SP_PRE_UNIV[1] = {

  [0] = {
         .num_bands = sizeof (SP_PRE_UNIV_B) / sizeof (SwitchBand),
         .bands = SP_PRE_UNIV_B,
         .initial_tuning_file = NULL}

};


//----------------------------------------the dbs LNB preset-----------------------------------------------
/*
        "Expressvu, North America",
        "12200 to 12700 MHz",
        "Single LO, 11250 MHz",
*/

///dbs lnb
static SwitchBand SP_PRE_DBS_B[1] = {
  [0] = {
         .lof = 11250000,
         .f_min = 12200000,
         .f_max = 12700000,
         .num_pol = sizeof (SP_PRE_UNIV_L) / sizeof (SwitchPol),
         .pol = SP_PRE_UNIV_L}

};

static SwitchPos SP_PRE_DBS[1] = {
  [0] = {
         .num_bands = sizeof (SP_PRE_DBS_B) / sizeof (SwitchBand),
         .bands = SP_PRE_DBS_B,
         .initial_tuning_file = NULL}

};

//----------------------------------------the standard LNB preset-----------------------------------------------

/*
        "10945 to 11450 Mhz",
        "Single LO, 10000 Mhz",

*/
static SwitchBand SP_PRE_STD_B[1] = {
  [0] = {
         .lof = 10000000,
         .f_min = 10945000,
         .f_max = 11450000,
         .num_pol = sizeof (SP_PRE_UNIV_L) / sizeof (SwitchPol),
         .pol = SP_PRE_UNIV_L}
};

static SwitchPos SP_PRE_STD[1] = {
  [0] = {
         .num_bands = sizeof (SP_PRE_STD_B) / sizeof (SwitchBand),
         .bands = SP_PRE_STD_B,
         .initial_tuning_file = NULL}
};

//----------------------------------------the enhanced LNB preset-----------------------------------------------
/*
        "Astra",
        "10700 to 11700 MHz",
        "Single LO, 9750 MHz",

*/

static SwitchBand SP_PRE_ENH_B[1] = {
  [0] = {
         .lof = 9750000,
         .f_min = 10700000,
         .f_max = 11700000,
         .num_pol = sizeof (SP_PRE_UNIV_L) / sizeof (SwitchPol),
         .pol = SP_PRE_UNIV_L}
};

static SwitchPos SP_PRE_ENH[1] = {
  [0] = {
         .num_bands = sizeof (SP_PRE_ENH_B) / sizeof (SwitchBand),
         .bands = SP_PRE_ENH_B,
         .initial_tuning_file = NULL}
};

//----------------------------------------the cband LNB preset-----------------------------------------------
/*
        "Big Dish",
        "3700 to 4200 MHz",
        "Single LO, 5150 Mhz",
*/
static SwitchBand SP_PRE_CBAND_B[1] = {
  [0] = {
         .lof = 5150000,
         .f_min = 3700000,
         .f_max = 4200000,
         .num_pol = sizeof (SP_PRE_UNIV_L) / sizeof (SwitchPol),
         .pol = SP_PRE_UNIV_L}
};

static SwitchPos SP_PRE_CBAND[1] = {
  [0] = {
         .num_bands = sizeof (SP_PRE_CBAND_B) / sizeof (SwitchBand),
         .bands = SP_PRE_CBAND_B,
         .initial_tuning_file = NULL}
};

//--------------------------two universal LNBs on a tone burst switch (rather large)------------------------------



///lnb A, hiband, Horizontal pol
static SwitchCmd SP_PRE_A_U_H[3] = {
  [0] = {
         .data[0] = SEC_MINI_A,
         .len = 1,
         .what = SC_BURST},

  [1] = {
         .data[0] = SEC_VOLTAGE_18,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_ON,
         .len = 1,
         .what = SC_TONE}
};

///lnb B, hiband, Horizontal pol
static SwitchCmd SP_PRE_B_U_H[3] = {
  [0] = {
         .data[0] = SEC_MINI_B,
         .len = 1,
         .what = SC_BURST},

  [1] = {
         .data[0] = SEC_VOLTAGE_18,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_ON,
         .len = 1,
         .what = SC_TONE}
};

///lnb A, hiband, Vertical pol
static SwitchCmd SP_PRE_A_U_V[3] = {
  [0] = {
         .data[0] = SEC_MINI_A,
         .len = 1,
         .what = SC_BURST},

  [1] = {
         .data[0] = SEC_VOLTAGE_13,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_ON,
         .len = 1,
         .what = SC_TONE}
};

///lnb B, hiband, Vertical pol
static SwitchCmd SP_PRE_B_U_V[3] = {
  [0] = {
         .data[0] = SEC_MINI_B,
         .len = 1,
         .what = SC_BURST},

  [1] = {
         .data[0] = SEC_VOLTAGE_13,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_ON,
         .len = 1,
         .what = SC_TONE}
};

///lnb A, loband, Horizontal pol
static SwitchCmd SP_PRE_A_L_H[3] = {
  [0] = {
         .data[0] = SEC_MINI_A,
         .len = 1,
         .what = SC_BURST},

  [1] = {
         .data[0] = SEC_VOLTAGE_18,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_OFF,
         .len = 1,
         .what = SC_TONE}
};

///lnb B, loband, Horizontal pol
static SwitchCmd SP_PRE_B_L_H[3] = {
  [0] = {
         .data[0] = SEC_MINI_B,
         .len = 1,
         .what = SC_BURST},

  [1] = {
         .data[0] = SEC_VOLTAGE_18,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_OFF,
         .len = 1,
         .what = SC_TONE}
};

///lnb A, loband, Vertical pol
static SwitchCmd SP_PRE_A_L_V[3] = {
  [0] = {
         .data[0] = SEC_MINI_A,
         .len = 1,
         .what = SC_BURST},

  [1] = {.data[0] = SEC_VOLTAGE_13,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_OFF,
         .len = 1,
         .what = SC_TONE}
};

///lnb B, loband, Vertical pol
static SwitchCmd SP_PRE_B_L_V[3] = {
  [0] = {
         .data[0] = SEC_MINI_B,
         .len = 1,
         .what = SC_BURST},

  [1] = {
         .data[0] = SEC_VOLTAGE_13,
         .len = 1,
         .what = SC_VOLTAGE},

  [2] = {
         .data[0] = SEC_TONE_OFF,
         .len = 1,
         .what = SC_TONE}
};

///lnb a hiband
static SwitchPol SP_PRE_A_U[2] = {
  [0] = {
         .num_cmds = sizeof (SP_PRE_A_U_H) / sizeof (SwitchCmd),
         .cmds = SP_PRE_A_U_H,
         .pol = 0}
  ,
  [1] = {
         .num_cmds = sizeof (SP_PRE_A_U_V) / sizeof (SwitchCmd),
         .cmds = SP_PRE_A_U_V,
         .pol = 1}
};

///lnb a loband
static SwitchPol SP_PRE_A_L[2] = {
  [0] = {
         .num_cmds = sizeof (SP_PRE_A_L_H) / sizeof (SwitchCmd),
         .cmds = SP_PRE_A_L_H,
         .pol = 0}
  ,
  [1] = {
         .num_cmds = sizeof (SP_PRE_A_L_V) / sizeof (SwitchCmd),
         .cmds = SP_PRE_A_L_V,
         .pol = 1}
};

///lnb b hiband
static SwitchPol SP_PRE_B_U[2] = {
  [0] = {
         .num_cmds = sizeof (SP_PRE_B_U_H) / sizeof (SwitchCmd),
         .cmds = SP_PRE_B_U_H,
         .pol = 0}
  ,
  [1] = {
         .num_cmds = sizeof (SP_PRE_B_U_V) / sizeof (SwitchCmd),
         .cmds = SP_PRE_B_U_V,
         .pol = 1}
};

///lnb b loband
static SwitchPol SP_PRE_B_L[2] = {
  [0] = {
         .num_cmds = sizeof (SP_PRE_B_L_H) / sizeof (SwitchCmd),
         .cmds = SP_PRE_B_L_H,
         .pol = 0}
  ,
  [1] = {
         .num_cmds = sizeof (SP_PRE_B_L_V) / sizeof (SwitchCmd),
         .cmds = SP_PRE_B_L_V,
         .pol = 1}
};

///bands for lnb a
static SwitchBand SP_PRE_DUAL_A_B[2] = {
  [0] = {
         .lof = 9750000,
         .f_min = 10800000,
         .f_max = 11700000,
         .num_pol = sizeof (SP_PRE_A_L) / sizeof (SwitchPol),
         .pol = SP_PRE_A_L}
  ,

  [1] = {
         .lof = 10600000,
         .f_min = 11700000,
         .f_max = 12700000,
         .num_pol = sizeof (SP_PRE_A_U) / sizeof (SwitchPol),
         .pol = SP_PRE_A_U}
};

///bands for lnb b
static SwitchBand SP_PRE_DUAL_B_B[2] = {
  [0] = {
         .lof = 9750000,
         .f_min = 10800000,
         .f_max = 11700000,
         .num_pol = sizeof (SP_PRE_B_L) / sizeof (SwitchPol),
         .pol = SP_PRE_B_L}
  ,

  [1] = {
         .lof = 10600000,
         .f_min = 11700000,
         .f_max = 12700000,
         .num_pol = sizeof (SP_PRE_B_U) / sizeof (SwitchPol),
         .pol = SP_PRE_B_U}
};

///2 satellite positions
static SwitchPos SP_PRE_UNIV_DUAL[2] = {
  [0] = {
         .num_bands = sizeof (SP_PRE_DUAL_A_B) / sizeof (SwitchBand),
         .bands = SP_PRE_DUAL_A_B,
         .initial_tuning_file = NULL}
  ,
  [1] = {
         .num_bands = sizeof (SP_PRE_DUAL_B_B) / sizeof (SwitchBand),
         .bands = SP_PRE_DUAL_B_B,
         .initial_tuning_file = NULL}
};

//------------------------------------------TechniRouter5 1x8 Preset---------------------------------------------
/*
lof is always used as intermediate freq. 
downmixing is done by the router


3. When an application supports the wide RF band only one local oscillator with a frequency FLO is
present in the LNB. In this case the selection of the high or the low band for the legacy output is
performed by a dedicated SaTCR.
Two parameters are needed for the band selection:
- The tuning word for the low band selection = [(FLO (MHz) - FLow (MHz))/4] - 350: where FLow
corresponds to the Low LO frequency.
- The tuning word for the high band selection = [(FLO (MHz) - FHigh (MHz))/4] - 350: where FHigh
corresponds to the High band LO frequency.
Example: in a wide band application with FLO= 13250 MHz, for emulating a low band local oscillator at
9750 MHz, index 0x0D and index 0x0E must be loaded with the decimal value dec [0D:0E] = round
((13250-9750)/4) - 350 = 525.


First stage of a SaTCR LNB (up to the matrix) is similar to a conventional LNB. As a consequence,
the transponder frequency at the input of a SaTCR device is the same as Ftuner with a classical
LNB:
F satcr_input = F transponder - F LO
Then, the SaTCR device should translate the transponder inside the bandwidth of its associated
band pass filter. To perform that operation, the SaTCR VCO has to be set according to the
following formula:
F satcr_vco = F satcr_input + F bpf = |F transponder - F LO | + F bpf
In addition, SaTCR LNB includes new features that allow auto-detection of its parameters.

3.2.1 ODU_ChannelChange
E0 10 5A channel_byte1 channel_byte2
Allows to select which LNB feed is routed on a SaTCR device and to program the SaTCR
frequency.
channel_byte1
Bit 7
Bit 6
Bit 5
SaTCR number (Table 1)
8/12
Bit 4
Bit 3
Bit 2
Bit 2
LNB number (Table 2)
www.BDTIC.com/ST
Bit 0
T[9:8]AN2056
channel_byte2
Bit 7
Bit 6
Bit 5
Bit 4
Bit 3
Bit 2
Bit 2
Bit 0
T[7:0]
T is the tuning byte used to program SaTCR VCO frequency. In order to limit the number of
transmitter bits to 10 a constant value (350) is substracted. The transmitted tuning byte can be
computed using the following formula:
         Fvco
T = -------------- – 350
           4
Example:
–Fvco = 4300 MHz | T=(4300/4) - 350 =725 (dec) | 2D5 (hex)
–Fvco = 3100 MHz | T=(3100/4) - 350 =425 (dec) | 1A9 (hex)
–Fvco = 1900 MHz | T=(1900/4) - 350 =125 (dec) | 7D (hex)
Table 1.
SaTCR	number
SaTCR1	0
SaTCR2	1
SaTCR3	2
SaTCR4	3
SaTCR5	4
SaTCR6	5
SaTCR7	6
SaTCR8	7

By convention, the SaTCR1 is allocated the lowest IF frequency, the SaTCR2 is allocated the
second and so forth.
Table 2.
Satellite
Position A
Position B
LNB
number
Low band / vertical polarization 0
High band / vertical polarization 1
Low band /horizontal polarization 2
High band / horizontal polarization 3
Low band / vertical polarization 4
High band / vertical polarization 5
Low band /horizontal polarization 6
High band / horizontal polarization 7


perhaps do stuff with external pgm instead..
*/


//--------------------------------------------------dvb-t preset-------------------------------------------------
///polarisation without commands
static SwitchPol SP_POL_NULL[1] = {
  [0] = {
         .num_cmds = 0,
         .cmds = NULL,
         .pol = 0}
};

///A dummy band allowing all frequencies
static SwitchBand SP_BAND_NULL[1] = {
  [0] = {
         .lof = 0,
         .f_min = 0,
         .f_max = INT32_MAX,
         .num_pol = sizeof (SP_POL_NULL) / sizeof (SwitchPol),
         .pol = SP_POL_NULL}
};

static SwitchPos SP_DVB_T[1] = {
  [0] = {
         .num_bands = sizeof (SP_BAND_NULL) / sizeof (SwitchBand),
         .bands = SP_BAND_NULL,
         .initial_tuning_file = NULL}
};

///preset names
static char const *sp_preset_names[] = {
  "Universal LNB",
  "DBS Expressvu",
  "Standard",
  "Enhanced",
  "CBand",
  "Dual Universal",
  "DVB-T"
};

///positioner/polarizer command names
static char const *sp_cmd_names[] = {
  "Tone",
  "Burst",
  "Voltage",
  "Diseqc",
  "Diseqc F",
  "Delay"
};

///the presets array
static SwitchPos *sp_presets[] = {
  [SP_P_UNIVERSAL_LNB] = SP_PRE_UNIV,
  [SP_P_DBS] = SP_PRE_DBS,
  [SP_P_STANDARD] = SP_PRE_STD,
  [SP_P_ENHANCED] = SP_PRE_ENH,
  [SP_P_CBAND] = SP_PRE_CBAND,
  [SP_P_UNIVERSAL_DUAL_LNB_BURST] = SP_PRE_UNIV_DUAL,
  [SP_P_DVBT] = SP_DVB_T
};

///the preset sizes
static size_t sp_preset_sz[] = {
  [SP_P_UNIVERSAL_LNB] = sizeof (SP_PRE_UNIV) / sizeof (SwitchPos),
  [SP_P_DBS] = sizeof (SP_PRE_DBS) / sizeof (SwitchPos),
  [SP_P_STANDARD] = sizeof (SP_PRE_STD) / sizeof (SwitchPos),
  [SP_P_ENHANCED] = sizeof (SP_PRE_ENH) / sizeof (SwitchPos),
  [SP_P_CBAND] = sizeof (SP_PRE_CBAND) / sizeof (SwitchPos),
  [SP_P_UNIVERSAL_DUAL_LNB_BURST] =
    sizeof (SP_PRE_UNIV_DUAL) / sizeof (SwitchPos),
  [SP_P_DVBT] = sizeof (SP_DVB_T) / sizeof (SwitchPos)
};


//----------------------------------------------------end presets------------------------------------------------

int
spPolDeepCopy (SwitchPol * dst, SwitchPol * src)
{
  if ((NULL == dst) || (NULL == src))
    return 1;
  *dst = *src;
  if ((0 == src->num_cmds) || (NULL == src->cmds))
  {
    dst->num_cmds = 0;
    dst->cmds = NULL;
  }
  else
  {
    dst->cmds = utlCalloc (src->num_cmds, sizeof (SwitchCmd));
    if (NULL == dst->cmds)
    {
      return 1;
    }
    memmove (dst->cmds, src->cmds, src->num_cmds * sizeof (SwitchCmd));
  }
  return 0;
}

int
spBandDeepCopy (SwitchBand * dst, SwitchBand * src)
{
  uint32_t i;
  if ((NULL == dst) || (NULL == src))
    return 1;
  *dst = *src;
  if ((0 == src->num_pol) || (NULL == src->pol))
  {
    dst->num_pol = 0;
    dst->pol = NULL;
  }
  else
  {
    dst->pol = utlCalloc (src->num_pol, sizeof (SwitchPol));
    if (NULL == dst->pol)
    {
      return 1;
    }
    for (i = 0; i < src->num_pol; i++)
    {
      if (spPolDeepCopy (dst->pol + i, src->pol + i))
      {
        utlFAN (dst->pol);
        return 1;
      }
    }
  }
  return 0;
}

int
spDeepCopy (SwitchPos * dst, SwitchPos * src)
{
  uint32_t i;

  if ((NULL == dst) || (NULL == src))
    return 1;
  *dst = *src;
  if (NULL != dst->initial_tuning_file)
  {
    dst->initial_tuning_file = strdup (src->initial_tuning_file);
  }
  if ((0 == src->num_bands) || (NULL == src->bands))
  {
    dst->num_bands = 0;
    dst->bands = NULL;
  }
  else
  {
    dst->bands = utlCalloc (src->num_bands, sizeof (SwitchBand));
    if (NULL == dst->bands)
    {
      utlFAN (dst->initial_tuning_file);
      return 1;
    }
    for (i = 0; i < src->num_bands; i++)
    {
      if (spBandDeepCopy (dst->bands + i, src->bands + i))
      {
        utlFAN (dst->initial_tuning_file);
        utlFAN (dst->bands);
        return 1;
      }
    }
  }
  return 0;
}

SwitchPos *
spClone (SwitchPos * sp, uint32_t num_sp)
{
  SwitchPos *rv;
  uint32_t i;
  if ((NULL == sp) || (0 == num_sp))
    return NULL;

  rv = utlCalloc (num_sp, sizeof (SwitchPos));
  if (rv == NULL)
    return NULL;
  for (i = 0; i < num_sp; i++)
  {
    if (spDeepCopy (rv + i, sp + i))
    {
      utlFAN (rv);
      return NULL;
    }
  }
  return rv;
}


int
spInit (SwitchPos * sp, uint32_t num_bands, SwitchBand * bands,
        char *initial_tuning_file)
{
  debugMsg ("initializing SwitchPos: %p, num_bands: %" PRIu32
            ", bands: %p, itf: %s\n", sp, num_bands, bands,
            initial_tuning_file);
  sp->num_bands = num_bands;
  sp->bands = bands;
  sp->initial_tuning_file = initial_tuning_file;
  return 0;
}

void
spClear (SwitchPos * sp)
{
  uint32_t i;
  debugMsg ("Clearing SwitchPos %p\n", sp);
  for (i = 0; i < sp->num_bands; i++)
  {
    spBandClear (sp->bands + i);
  }
  utlFAN (sp->bands);
  sp->bands = NULL;
  utlFAN (sp->initial_tuning_file);
  sp->bands = NULL;
  sp->num_bands = 0;
}

int
CUspInit (SwitchPos * sp, uint32_t num_bands, SwitchBand * bands,
          char *initial_tuning_file, CUStack * s)
{
  int rv;
  rv = spInit (sp, num_bands, bands, initial_tuning_file);
  cuStackFail (rv, sp, CUspClear, s);
  return rv;
}

void
CUspClear (void *sp)
{
  spClear (sp);
}

static int
sp_write_cmd (SwitchCmd * b, FILE * f)
{
  return (1 != fwrite (b, sizeof (SwitchCmd), 1, f));
}

static int
sp_write_pol (SwitchPol * b, FILE * f)
{
  uint32_t i;
  fwrite (b, sizeof (SwitchPol) - sizeof (SwitchCmd *), 1, f);
  for (i = 0; i < b->num_cmds; i++)
  {
    sp_write_cmd (b->cmds + i, f);
  }
  return 0;
}

static int
sp_write_band (SwitchBand * b, FILE * f)
{
  uint32_t i;
  fwrite (b, sizeof (SwitchBand) - sizeof (SwitchPol *), 1, f);
  for (i = 0; i < b->num_pol; i++)
  {
    sp_write_pol (b->pol + i, f);
  }
  return 0;
}

static int
sp_read_cmd (SwitchCmd * b, FILE * f)
{
  return (1 != fread (b, sizeof (SwitchCmd), 1, f));
}

static int
sp_read_pol (SwitchPol * b, FILE * f)
{
  uint32_t i;
  fread (b, sizeof (SwitchPol) - sizeof (SwitchCmd *), 1, f);
  if (b->num_cmds)
  {
    b->cmds = utlCalloc (b->num_cmds, sizeof (SwitchCmd));
    if (!b->cmds)
    {
      b->num_cmds = 0;
      return 1;
    }
    for (i = 0; i < b->num_cmds; i++)
    {
      sp_read_cmd (b->cmds + i, f);
    }
  }
  else
  {
    b->cmds = NULL;
  }
  return 0;
}

static int
sp_read_band (SwitchBand * b, FILE * f)
{
  uint32_t i;
  fread (b, sizeof (SwitchBand) - sizeof (SwitchPol *), 1, f);
  if (b->num_pol)
  {
    b->pol = utlCalloc (b->num_pol, sizeof (SwitchPol));
    if (!b->pol)
    {
      b->num_pol = 0;
      return 1;
    }
    for (i = 0; i < b->num_pol; i++)
    {
      sp_read_pol (b->pol + i, f);
    }
  }
  else
  {
    b->pol = NULL;
  }
  return 0;
}

static int
sp_snd_cmd (SwitchCmd * b, int sock)
{
  ioBlkWr (sock, b->data, sizeof (uint8_t) * 8);
  ipcSndS (sock, b->len);
  ipcSndS (sock, b->what);
  return 0;                     //(sizeof(SwitchCmd)!=ioBlkWr(sock,b,sizeof(SwitchCmd)));
}

static int
sp_snd_pol (SwitchPol * b, int sock)
{
  uint32_t i;
  ipcSndS (sock, b->num_cmds);
  ipcSndS (sock, b->pol);
//      ioBlkWr(sock,b,sizeof(SwitchPol)-sizeof(SwitchCmd *));
  for (i = 0; i < b->num_cmds; i++)
  {
    sp_snd_cmd (b->cmds + i, sock);
  }
  return 0;
}

static int
sp_snd_band (SwitchBand * b, int sock)
{
  uint32_t i;
  ipcSndS (sock, b->lof);
  ipcSndS (sock, b->f_min);
  ipcSndS (sock, b->f_max);
  ipcSndS (sock, b->num_pol);
//      ioBlkWr(sock,b,sizeof(SwitchBand)-sizeof(SwitchPol *));
  for (i = 0; i < b->num_pol; i++)
  {
    sp_snd_pol (b->pol + i, sock);
  }
  return 0;
}

static int
sp_rcv_cmd (SwitchCmd * b, int sock)
{
  ioBlkRd (sock, b->data, sizeof (uint8_t) * 8);
  ipcRcvS (sock, b->len);
  ipcRcvS (sock, b->what);
  return 0;                     //(sizeof(SwitchCmd)!=ioBlkRd(sock,b,sizeof(SwitchCmd)));
}

static int
sp_rcv_pol (SwitchPol * b, int sock)
{
  uint32_t i;
  ipcRcvS (sock, b->num_cmds);
  ipcRcvS (sock, b->pol);
//      ioBlkRd(sock,b,sizeof(SwitchPol)-sizeof(SwitchCmd *));
  if (b->num_cmds)
  {
    b->cmds = utlCalloc (b->num_cmds, sizeof (SwitchCmd));
    if (!b->cmds)
    {
      b->num_cmds = 0;
      return 1;
    }
    for (i = 0; i < b->num_cmds; i++)
    {
      sp_rcv_cmd (b->cmds + i, sock);
    }
  }
  else
  {
    b->cmds = NULL;
  }
  return 0;
}

static int
sp_rcv_band (SwitchBand * b, int sock)
{
  uint32_t i;
  ipcRcvS (sock, b->lof);
  ipcRcvS (sock, b->f_min);
  ipcRcvS (sock, b->f_max);
  ipcRcvS (sock, b->num_pol);
//      ioBlkRd(sock,b,sizeof(SwitchBand)-sizeof(SwitchPol *));
  if (b->num_pol)
  {
    b->pol = utlCalloc (b->num_pol, sizeof (SwitchPol));
    if (!b->pol)
    {
      b->num_pol = 0;
      return 1;
    }
    for (i = 0; i < b->num_pol; i++)
    {
      sp_rcv_pol (b->pol + i, sock);
    }
  }
  else
  {
    b->pol = NULL;
  }
  return 0;
}

int
spWrite (SwitchPos * sp, FILE * f)
{
  uint32_t i;
  debugMsg ("Writing SwitchPos %p\n", sp);
  fwrite (&sp->num_bands, sizeof (sp->num_bands), 1, f);
  ioWrStr (sp->initial_tuning_file, f);
  for (i = 0; i < sp->num_bands; i++)
  {
    sp_write_band (sp->bands + i, f);
  }
  return 0;
}

int
spRead (SwitchPos * sp, FILE * f)
{
  uint32_t i;
  debugMsg ("Reading SwitchPos %p\n", sp);
  fread (&sp->num_bands, sizeof (sp->num_bands), 1, f);
  sp->initial_tuning_file = ioRdStr (f);
  sp->bands = utlCalloc (sp->num_bands, sizeof (SwitchBand));
  for (i = 0; i < sp->num_bands; i++)
  {
    sp_read_band (sp->bands + i, f);
  }
  return 0;
}


int
spSnd (SwitchPos * sp, int sock)
{
  uint32_t i;
  debugMsg ("Sending SwitchPos %p\n", sp);
  ipcSndS (sock, sp->num_bands);
  ipcSndStr (sock, sp->initial_tuning_file);
  for (i = 0; i < sp->num_bands; i++)
  {
    sp_snd_band (sp->bands + i, sock);
  }
  return 0;
}

int
spRcv (SwitchPos * sp, int sock)
{
  uint32_t i;
  debugMsg ("Receiving SwitchPos %p\n", sp);
  ipcRcvS (sock, sp->num_bands);
  sp->initial_tuning_file = ipcRcvStr (sock);
  sp->bands = utlCalloc (sp->num_bands, sizeof (SwitchBand));
  for (i = 0; i < sp->num_bands; i++)
  {
    sp_rcv_band (sp->bands + i, sock);
  }
  return 0;
}

int
spBandInit (SwitchBand * sb, uint32_t lof, uint32_t f_min, uint32_t f_max,
            uint32_t num_pol, SwitchPol * pol)
{
  debugMsg
    ("initializing SwitchBand %p, lof: %" PRIu32 ", f_min: %" PRIu32
     ", f_max: %" PRIu32 ", num_pol: %" PRIu32 ", SwitchPol: %p\n", sb, lof,
     f_min, f_max, num_pol, pol);
  sb->lof = lof;
  sb->f_min = f_min;
  sb->f_max = f_max;
  sb->num_pol = num_pol;
  sb->pol = pol;
  return 0;
}

int
spBandClear (SwitchBand * sb)
{
  debugMsg ("Clearing SwitchBand %p\n", sb);
  uint32_t i;
  for (i = 0; i < sb->num_pol; i++)
  {
    spPolClear (sb->pol + i);
  }
  utlFAN (sb->pol);
  sb->pol = NULL;
  sb->lof = 0;
  sb->f_min = 0;
  sb->f_max = 0;
  sb->num_pol = 0;
  return 0;
}

int
spPolInit (SwitchPol * sp, uint8_t pol, uint32_t num_cmds, SwitchCmd * cmds)
{
  debugMsg ("initializing SwitchPol %p, pol: %" PRIu8 ", num_cmds: %" PRIu32
            ", cmds: %p\n", sp, pol, num_cmds, cmds);
  sp->pol = pol;
  sp->num_cmds = num_cmds;
  sp->cmds = cmds;
  return 0;
}

int
spPolClear (SwitchPol * sp)
{
  utlFAN (sp->cmds);
  sp->cmds = NULL;
  sp->num_cmds = 0;
  sp->pol = 0;
  return 0;
}

static int
array_rmv (char *ptr, uint32_t num_e, uint32_t idx, uint32_t sz_e)
{
  if (num_e <= idx)
    return 1;
  if (num_e > (idx + 1))
    memmove (ptr + sz_e * idx, ptr + sz_e * (idx + 1),
             (num_e - idx - 1) * sz_e);
  return 0;
}


int
spDel (SwitchPos ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchPos *p = *sp;
  if (idx >= num)
    return 1;

  spClear (p + idx);
  if (array_rmv ((char *) p, num, idx, sizeof (SwitchPos)))
    return 1;
  num--;
  *num_sp = num;
  return 0;
}

int
spBandDel (SwitchBand ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchBand *p = *sp;
  if (idx >= num)
    return 1;

  spBandClear (p + idx);
  if (array_rmv ((char *) p, num, idx, sizeof (SwitchBand)))
    return 1;
  num--;
  *num_sp = num;
  return 0;
}

int
spPolDel (SwitchPol ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchPol *p = *sp;
  if (idx >= num)
    return 1;

  spPolClear (p + idx);
  if (array_rmv ((char *) p, num, idx, sizeof (SwitchPol)))
    return 1;
  num--;
  *num_sp = num;
  return 0;
}

int
spCmdDel (SwitchCmd ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchCmd *p = *sp;
  if (idx >= num)
    return 1;

  if (array_rmv ((char *) p, num, idx, sizeof (SwitchCmd)))
    return 1;
  num--;
  *num_sp = num;
  return 0;
}

/**
 * \brief make room for element
 * Makes room for sz_e bytes at idx*sz_e, 
 * possibly moving excess elements towards the end(after reallocating). 
 * 
 * 
 */
static int
array_ins (char **ptr, uint32_t num_e, uint32_t idx, uint32_t sz_e)
{
  char *p = *ptr;
  char *np;

  if (num_e < idx)
    return 1;
  np = utlRealloc (p, (num_e + 1) * sz_e);
  if (NULL == np)
    return 1;
  *ptr = np;
  if (num_e > idx)
    memmove (ptr + sz_e * (idx + 1), ptr + sz_e * idx, (num_e - idx) * sz_e);
  return 0;
}

int
spAdd (SwitchPos ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchPos *p;
  if (idx > num)
    return 1;
  if (array_ins ((char **) sp, num, idx, sizeof (SwitchPos)))
    return 1;
  p = *sp;
  spInit (p + idx, 0, NULL, NULL);
  num++;
  *num_sp = num;
  return 0;
}

int
spBandAdd (SwitchBand ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchBand *p;
  if (idx > num)
    return 1;
  if (array_ins ((char **) sp, num, idx, sizeof (SwitchBand)))
    return 1;
  p = *sp;
  spBandInit (p + idx, 0, 0, 0, 0, NULL);
  num++;
  *num_sp = num;
  return 0;
}

int
spPolAdd (SwitchPol ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchPol *p;
  if (idx > num)
    return 1;
  if (array_ins ((char **) sp, num, idx, sizeof (SwitchPol)))
    return 1;
  p = *sp;
  spPolInit (p + idx, 0, 0, NULL);
  num++;
  *num_sp = num;
  return 0;
}

int
spCmdAdd (SwitchCmd ** sp, uint32_t * num_sp, uint32_t idx)
{
  uint32_t num = *num_sp;
  SwitchCmd *p;
  if (idx > num)
    return 1;
  if (array_ins ((char **) sp, num, idx, sizeof (SwitchCmd)))
    return 1;
  p = *sp;
  memset (p + idx, 0, sizeof (SwitchCmd));
  num++;
  *num_sp = num;
  return 0;
}

char const *
spGetPresetStrings (sp_preset_e id)
{
  if (id >= (sizeof (sp_preset_names) / sizeof (char *)))
    return (char const *) NULL;
  return sp_preset_names[id];
}

char const *
spGetCmdStr (uint8_t id)
{
  if (id >= (sizeof (sp_cmd_names) / sizeof (char *)))
    return (char const *) NULL;
  return sp_cmd_names[id];
}

SwitchPos *
spGetPreset (sp_preset_e id, uint32_t * num_ret)
{
  if ((id < SP_P_UNIVERSAL_LNB) || (id >= SP_NUM_PRESETS))
    return NULL;
  *num_ret = sp_preset_sz[id];
  debugMsg ("Cloning Preset of size %u elements\n", sp_preset_sz[id]);
  return spClone (sp_presets[id], sp_preset_sz[id]);
}
