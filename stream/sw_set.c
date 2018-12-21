#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/dvb/frontend.h>
#include <inttypes.h>
#include "utl.h"
#include "sw_set.h"
#include "debug.h"

int DF_SW_SET;
#define FILE_DBG DF_SW_SET

static int sp_run_cmds (uint32_t num_cmds, SwitchCmd * cmds, uint32_t freq,
                        int dev);

//return intermediate frequency
int
spGetIf (SwitchPos * sp, uint32_t size, uint32_t pos, uint32_t freq,
         uint32_t * if_ret)
{
  uint32_t i;
  uint32_t i_f;
  debugMsg ("getting ifreq for sp: %p, size: %" PRIu32 ", pos: %" PRIu32
            ", freq: %" PRIu32 "\n", sp, size, pos, freq);
  if (pos >= size)
    return 1;
  sp = sp + pos;
  for (i = 0; i < sp->num_bands; i++)
  {
    debugMsg ("f_min: %u, f_max: %u\n", sp->bands[i].f_min,
              sp->bands[i].f_max);
    if ((freq >= sp->bands[i].f_min) && (freq < sp->bands[i].f_max))
    {
      i_f = freq - sp->bands[i].lof;
      *if_ret = i_f;
      return 0;
    }
  }
  return 1;
}

int
spDoSwitch (SwitchPos * sp, uint32_t size, uint32_t pos, uint32_t freq,
            uint8_t pol, int dev)
{
  uint32_t i;
//      uint32_t i_f;
  debugMsg
    ("Doing Switch Setup for sp: %p, size: %" PRIu32 ", pos: %" PRIu32
     ", freq: %" PRIu32 ", pol: %" PRIu8 ", dev: %d\n", sp, size, pos, freq,
     pol, dev);
  if (pos >= size)
    return 1;
  sp = sp + pos;
  for (i = 0; i < sp->num_bands; i++)
  {
    debugMsg ("f_min: %u, f_max: %u\n", sp->bands[i].f_min,
              sp->bands[i].f_max);
    if ((freq >= sp->bands[i].f_min) && (freq < sp->bands[i].f_max))
    {
      uint32_t j;
//                      i_f=freq-sp->bands[i].lof;
      for (j = 0; j < sp->bands[i].num_pol; j++)
      {
        if (sp->bands[i].pol[j].pol == pol)
        {
          if (sp_run_cmds
              (sp->bands[i].pol[j].num_cmds, sp->bands[i].pol[j].cmds, freq,
               dev))
            return 1;
//                                      *if_ret=i_f;
          return 0;
        }
      }
    }
  }
  return 1;
}

static int
sp_tone (int v, int dev)
{
  struct dtv_property prop = {.cmd = DTV_TONE,.u.data = v };
  struct dtv_properties props = { 1, &prop };
  debugMsg ("sp_tone\n");
  return ioctl (dev, FE_SET_PROPERTY, &props);
}

static int
sp_burst (int v, int dev)
{
  debugMsg ("sp_burst\n");
  return ioctl (dev, FE_DISEQC_SEND_BURST, (fe_sec_tone_mode_t *) & v);
}

static int
sp_voltage (int v, int dev)
{
  struct dtv_property prop = {.cmd = DTV_VOLTAGE,.u.data = v };
  struct dtv_properties props = { 1, &prop };
  debugMsg ("sp_voltage\n");
  return ioctl (dev, FE_SET_PROPERTY, &props);
}

static int
sp_diseqc (uint8_t * data, uint8_t len, int dev)
{
  struct dtv_property prop = {.cmd = DTV_DISEQC_MASTER };
  struct dtv_properties props = { 1, &prop };
  int i;
  debugMsg ("sp_diseqc\n");
  prop.u.buffer.len = len;
  for (i = 0; i < len; i++)
  {
    prop.u.buffer.data[i] = data[i];
  }
  return ioctl (dev, FE_SET_PROPERTY, &props);
}

static uint8_t
sp_f_to_bcd (uint32_t freq, uint8_t idx)
{
  uint8_t rv;
  switch (idx)
  {
  case 0:
    freq = (freq / 1000) % 100;
    break;
  case 1:
    freq = (freq / 10) % 100;
    break;
  case 2:
    freq = (freq * 10) % 100;
    break;
  }

  rv = (freq % 10) | (((freq / 10) % 10) << 4);
  debugMsg ("sp_f_to_bcd: %" PRIu8 " %" PRIx8 "\n", idx, rv);
  return rv;
}

static int
sp_diseqc_f (uint8_t * data, uint32_t freq, int dev)
{
  struct dtv_property prop = {.cmd = DTV_DISEQC_MASTER };
  struct dtv_properties props = { 1, &prop };
  uint32_t i;
  debugMsg ("sp_diseqc_f\n");
  prop.u.buffer.data[0] = data[0];
  prop.u.buffer.data[1] = data[1];
  prop.u.buffer.data[2] = 0x58;
  for (i = 3; i < 6; i++)
  {
    prop.u.buffer.data[i] = sp_f_to_bcd (freq, i - 3);
    if (!prop.u.buffer.data[i])
      break;
  }
  prop.u.buffer.len = i;
  return ioctl (dev, FE_SET_PROPERTY, &props);
}

static int
sp_run_cmd (SwitchCmd * cmd, uint32_t freq, int dev)
{
  switch (cmd->what)
  {
  case SC_TONE:
    return sp_tone (cmd->data[0], dev);
  case SC_BURST:
    return sp_burst (cmd->data[0], dev);
  case SC_VOLTAGE:
    return sp_voltage (cmd->data[0], dev);
  case SC_WCMD:
    return sp_diseqc (cmd->data, cmd->len, dev);
  case SC_WCMD_F:
    return sp_diseqc_f (cmd->data, freq, dev);
  case SC_DELAY:
    debugMsg ("sp_delay\n");
    return usleep (1000 * cmd->data[0]);
  default:
    return 1;
  }
  return 0;
}

static int
sp_run_cmds (uint32_t num_cmds, SwitchCmd * cmds, uint32_t freq, int dev)
{
  uint32_t i;
  for (i = 0; i < num_cmds; i++)
  {
    debugMsg ("Running cmd\n");
    if (sp_run_cmd (cmds + i, freq, dev))
    {
      errMsg ("Switch Cmd Failed\n");
      return 1;
    }
  }
  return 0;
}
