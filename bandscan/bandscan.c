#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/dvb/frontend.h>
static char help_txt[] = "\
Use: \n\
bandscan <frontend> -s [strength|ber|snr|ucb]\n\
To band scan a frontend. \n\
If omitted, <frontend> defaults to: \n\
\"/dev/dvb/adapter0/frontend0\" \n\
Only works for universal LNBs. \n\
Assumes 10.6 and 9.75GHz LO frequencies. \n\
Outputs 5 CSV files. The band_... \n\
ones contain the amplitude values for one \n\
frequency band (all combinations of Hi/Lo, Vert/Hrz) \n\
with the intermediate frequency at the tuner. \n\
The all.csv contains the values of all bands \n\
with the true broadcast frequencies. \n\
bandscan -h \n\
shows this text. \n\
-s: what to scan for. defaults to strength. \n";

static int
voltage (int dev, int v)
{
  struct dtv_property prop = {.cmd = DTV_VOLTAGE,.u.data = v };
  struct dtv_properties props = { 1, &prop };
  return ioctl (dev, FE_SET_PROPERTY, &props);
}

static int
tone (int dev, int v)
{
  struct dtv_property prop = {.cmd = DTV_TONE,.u.data = v };
  struct dtv_properties props = { 1, &prop };
  return ioctl (dev, FE_SET_PROPERTY, &props);
}


static struct dvb_frontend_info info;     /**< Information about the front end */

static int
fe_init (int argc, char *argv[])
{
  struct dtv_property prop = {.cmd = DTV_CLEAR };
  struct dtv_properties props = { 1, &prop };
  char *fe = "/dev/dvb/adapter0/frontend0";
  int fd;
  if (argc >= 2)
  {
    if ((!strcasecmp (argv[1], "-h")) || (!strcasecmp (argv[1], "--help")))
    {
      fprintf (stderr, help_txt);
      exit (EXIT_SUCCESS);
    }
    if(argv[1][0]!='-')
	    fe = argv[1];
  }
  fd = open (fe, O_RDWR, 0);
  assert (fd != -1);
  if (ioctl (fd, FE_GET_INFO, &info) < 0)
  {
    fprintf (stderr, "Failed to get front end info: %s\n", strerror (errno));
    abort ();
  }

  fprintf (stderr, "----------------Frontend Information----------------\n");
  fprintf (stderr, "Name: %s\n", info.name);
//  fprintf(stderr, "Type: %s\n", struGetStr (fetype_strings, info.type));
  fprintf (stderr, "Freq Min: %u, Max: %u, Stepsize: %u, Tolerance: %u\n",
           info.frequency_min, info.frequency_max,
           info.frequency_stepsize, info.frequency_tolerance);
  fprintf (stderr, "Symbol Rate Min: %u, Max: %u, Tolerance: %u\n",
           info.symbol_rate_min, info.symbol_rate_max,
           info.symbol_rate_tolerance);
  fprintf (stderr, "??notifier_delay??%u\n", info.notifier_delay);
  fprintf (stderr, "Caps 0x%08x\n", info.caps);
//  dvb_bits_to_string (info.caps, fecaps_strings,
  //                    sizeof (fecaps_strings) / sizeof (char *));
  fprintf (stderr, "-------------End Frontend Information---------------\n");

  ioctl (fd, FE_SET_PROPERTY, &props);
  return fd;

}

static void
fe_end (int fd)
{
  struct dtv_property prop = {.cmd = DTV_CLEAR };
  struct dtv_properties props = { 1, &prop };
  ioctl (fd, FE_SET_PROPERTY, &props);
  close (fd);
}

static void
tune (int fd, uint32_t ifreq)   //in khz
{
  int tmp = FE_TUNE_MODE_ONESHOT;
  struct dtv_property prop[] = {
    {.cmd = DTV_CLEAR},
    {.cmd = DTV_FREQUENCY,.u.data = ifreq},
    {.cmd = DTV_MODULATION,.u.data = QPSK},
    {.cmd = DTV_SYMBOL_RATE,.u.data = 22000000},
    {.cmd = DTV_INNER_FEC,.u.data = FEC_AUTO},
    {.cmd = DTV_INVERSION,.u.data = INVERSION_AUTO},
    {.cmd = DTV_TUNE}
  };
  struct dtv_properties props = { sizeof (prop) / sizeof (prop[0]), prop };
  if (ioctl (fd, FE_SET_FRONTEND_TUNE_MODE, &tmp) < 0)
  {
    fprintf (stderr, "FE_SET_FRONTEND_TUNE_MODE: %s\n", strerror (errno));
  }
  if (ioctl (fd, FE_SET_PROPERTY, &props) < 0)
  {
    fprintf (stderr, "FE_SET_PROPERTY: %s\n", strerror (errno));
  }

}

static int what=0;
static uint32_t
strength (int fd)
{
  uint32_t rv32;
  uint16_t rv16;
  switch (what)
  {
    case 0:
    if (ioctl (fd, FE_READ_SIGNAL_STRENGTH, &rv16) < 0)
    {
      fprintf (stderr, "FE_READ_SIGNAL_STRENGTH: %s\n", strerror (errno));
      abort ();
    }
    rv32=rv16;
    break;
    case 1:
    if (ioctl (fd, FE_READ_BER, &rv32) < 0)
    {
      fprintf (stderr, "FE_READ_BER: %s\n", strerror (errno));
      abort ();
    }
    break;
    case 2:
    if (ioctl (fd, FE_READ_SNR, &rv16) < 0)
    {
      fprintf (stderr, "FE_READ_SNR: %s\n", strerror (errno));
      abort ();
    }
    rv32=rv16;
    break;
    case 3:
    if (ioctl (fd, FE_READ_UNCORRECTED_BLOCKS, &rv32) <
        0)
    {
      fprintf (stderr, "FE_READ_UNCORRECTED_BLOCKS: %s\n", strerror (errno));
      abort ();
    }
    break;
    default:
    abort();
    break;
  }
  return rv32;
}


static void
dump_hist (uint32_t * str, FILE * fp)
{
  int f;
  assert (fp);
  fprintf (fp, "Frequency[MHz], Amplitude[?]\n");
  for (f = 950; f < 2150; f++)
  {
    fprintf (fp, "%d, %" PRIu32 "\n", f, str[f - 950]);
  }
}

static void
do_scan (int dev, FILE * fp)
{
  int f;
  int start = (info.frequency_min + 999) / 1000, end =
    info.frequency_max / 1000;
  uint32_t *str;
  str = calloc (sizeof (uint32_t), end - start);
  assert (str);
//      odu_scr_signal_on(dev);
  strength (dev);//to get rid of accumulted stuff
  for (f = start; f < end; f++)
  {
    printf ("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%d/%d", f, end);
    tune (dev, f * 1000);
    usleep (5000);
    str[f - 950] = strength (dev);
  }
  printf ("\n");
  dump_hist (str, fp);
  free (str);
}

static struct
{
  int tone;
  int volt;
  char *pol;
  int lo;
} settings[4] =
{
  {
  SEC_TONE_OFF, SEC_VOLTAGE_13, "V", 9750000},
  {
  SEC_TONE_OFF, SEC_VOLTAGE_18, "H", 9750000},
  {
  SEC_TONE_ON, SEC_VOLTAGE_13, "V", 10600000},
  {
  SEC_TONE_ON, SEC_VOLTAGE_18, "H", 10600000}
};

static void
scan_band (int dev, int band)
{
  char buf[100];
  FILE *fp;
  printf ("Scanning band %d\n", band);
  assert ((band < 4) && (band >= 0));
  sprintf (buf, "band_%d_%s.csv", settings[band].lo, settings[band].pol);
  fp = fopen (buf, "w+t");
  assert (fp);

  tone (dev, settings[band].tone);
  voltage (dev, settings[band].volt);
  do_scan (dev, fp);
  fclose (fp);
}


/*
read the bands into 4 arrays
build one big array
dump it

*/
static uint32_t *
read_band (FILE * fp, int *start_ret, int *sz_ret)
{
  int c;
  int i;
  int done;
  int f;
  int sz;
  int start;
  uint32_t v, *rv;
  assert (fp);
  do
  {
    c = fgetc (fp);
    assert (EOF != c);
  }
  while (c != '\n');

  i = 0;
  done = (2 != fscanf (fp, "%d, %" SCNu32 "\n", &start, &v));
  if (!done)
  {
    i++;
  }
  while (!done)
  {
    done = (2 != fscanf (fp, "%d, %" SCNu32 "\n", &f, &v));
    if (!done)
    {
      i++;
    }
  }
  assert (i > 0);
  sz = i;

  rv = calloc (sizeof (uint32_t), sz);
  assert (rv);
  //back2start
  fseek (fp, 0, SEEK_SET);
  do
  {
    c = fgetc (fp);
    assert (EOF != c);
  }
  while (c != '\n');

  i = 0;
  do
  {
    done = (2 != fscanf (fp, "%d, %" SCNu32 "\n", &f, &v));
    if (!done)
    {
      assert (i < sz);
      rv[i] = v;
      i++;
    }
  }
  while (!done);

  *start_ret = start;
  *sz_ret = sz;
  return rv;
}

#define min(a,b) ((a)>(b)?(b):(a))
#define max(a,b) ((a)>(b)?(a):(b))
static void
combine_bands (void)
{
  FILE *fp, *res;
  int i;
  int start[4];
  int tru_start[4];
  int res_start;
  int res_end;
  int res_sz;
  int size[4];
  uint32_t *band[4];
  uint32_t *res_buf[4];
  char buf[100];
  res = fopen ("all.csv", "w+t");
  assert (res);
  fprintf (res, "Frequency[MHz]");
  for (i = 0; i < 4; i++)
  {
    sprintf (buf, "band_%d_%s.csv", settings[i].lo, settings[i].pol);
    fp = fopen (buf, "rt");
    assert (fp);
    band[i] = read_band (fp, &start[i], &size[i]);
    fclose (fp);
    fp = NULL;
    assert (band[i]);
    tru_start[i] = start[i] + settings[i].lo / 1000;
    fprintf (res, ", %d%s", settings[i].lo / 1000, settings[i].pol);
  }
  fprintf (res, "\n");

  res_start = min (tru_start[0], tru_start[1]);
  res_end = max (tru_start[2] + size[2], tru_start[3] + size[3]);
  res_sz = res_end - res_start;
  assert (res_sz > 0);
  for (i = 0; i < 4; i++)
  {
    res_buf[i] = calloc (sizeof (uint32_t), res_sz);
    assert (res_buf[i]);
  }
  for (i = 0; i < 4; i++)
  {
    int j;
    for (j = 0; j < size[i]; j++)
    {
      int idx = tru_start[i] - res_start + j;
      res_buf[i][idx] = band[i][j];
    }
  }
  for (i = 0; i < res_sz; i++)
  {
    int j;

    fprintf (res, "%d", res_start + i);
    for (j = 0; j < 4; j++)
      fprintf (res, ", %" PRIu32, res_buf[j][i]);
    fprintf (res, "\n");
  }
  for (i = 0; i < 4; i++)
  {
    free (res_buf[i]);
    res_buf[i] = NULL;
    free (band[i]);
    band[i] = NULL;
  }
  fclose (res);

}


static const char *whatstr[]={"strength","ber","snr","ucb"};
#define struGetIdx(tbl,s) struGetIdx2(tbl,ARR_NE(tbl),s)
///array number of elements
#define ARR_NE(a_) (sizeof((a_))/sizeof(*(a_)))

int struGetIdx2(char const *tbl[],size_t sz,char* s)
{
	int rv;
	size_t i;
	for(i=0;((i<sz)&&strcmp(tbl[i],s));i++)
		;
	if(i>=sz)
		rv=-1;
	else
		rv=i;
	return rv;
}

int getwhat(int argc, char *argv[])
{
	int i;
	int rv=-1;
	for(i=1;i<argc;i++)
	{
		if(!strcmp(argv[i],"-s"))
		{
			assert((i+1)<argc);
			rv=struGetIdx(whatstr,argv[i+1]);
		}
	}
	if(rv==-1)
		rv=0;
	return rv;
}


int
main (int argc, char *argv[])
{
  int dev = fe_init (argc, argv);
  int i;
  what = getwhat(argc,argv);
  for (i = 0; i < 4; i++)
    scan_band (dev, i);
  fe_end (dev);
  combine_bands ();
  return 0;
}
