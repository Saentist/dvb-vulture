/**
 *\brief parse a single transponder from initial tuning file (satellite only)
 *
 * the format of the file is:
 * S freq pol rate fec
 * whitespaces
 * and single line comments preceded by a # each
 *
 * hmm.. will it work with this minimal amount of information provided?
 * ..no modulation system etc...
 * TODO make it work for weird files. eg one of the satmex files has insane line breaks and hyphenations(!) in there
 */
void
pgmdb_parse_tp (FILE * f, TransponderInfo * i)
{
  const size_t FEC_BUF_SZ = 32;
  char pol;
  char fec[FEC_BUF_SZ];
  fscanf (f, " %u %c %u %31s", &i->frequency, &pol, &i->srate, fec);
  i->frequency /= 10;
  i->srate /= 100;

  i->fec = 0;
  if (!strncasecmp (fec, "auto", FEC_BUF_SZ))
    i->fec = 0;
  else if (!strncasecmp (fec, "1/2", FEC_BUF_SZ))
    i->fec = 1;
  else if (!strncasecmp (fec, "2/3", FEC_BUF_SZ))
    i->fec = 2;
  else if (!strncasecmp (fec, "3/4", FEC_BUF_SZ))
    i->fec = 3;
  else if (!strncasecmp (fec, "5/6", FEC_BUF_SZ))
    i->fec = 4;
  else if (!strncasecmp (fec, "7/8", FEC_BUF_SZ))
    i->fec = 5;
  else if (!strncasecmp (fec, "8/9", FEC_BUF_SZ))
    i->fec = 6;
  else if (!strncasecmp (fec, "3/5", FEC_BUF_SZ))
    i->fec = 7;
  else if (!strncasecmp (fec, "4/5", FEC_BUF_SZ))
    i->fec = 8;
  else if (!strncasecmp (fec, "9/10", FEC_BUF_SZ))
    i->fec = 9;

	i->inv = 2;
  switch (pol)
  {
  case 'H':
  case 'h':
    i->pol = 0;
    break;
  case 'V':
  case 'v':
    i->pol = 1;
    break;
  case 'L':
  case 'l':
    i->pol = 2;
    break;
  default:
  case 'R':
  case 'r':
    i->pol = 3;
    break;
  }

}

uint8_t
pgmdb_parse_bw_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "8MHz", sz))
    return 0;
  else if (!strncasecmp (s, "7MHz", sz))
    return 1;
  else if (!strncasecmp (s, "6MHz", sz))
    return 2;
  else if (!strncasecmp (s, "5MHz", sz))
    return 3;
  else
    return 4;
}

uint8_t
pgmdb_parse_mod_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "QPSK", sz))
    return 0;
  else if (!strncasecmp (s, "QAM16", sz))
    return 1;
  else if (!strncasecmp (s, "QAM64", sz))
    return 2;
  else
    return 3;
}

uint8_t
pgmdb_parse_hier_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "NONE", sz))
    return 0;
  else if (!strncasecmp (s, "1", sz))
    return 1;
  else if (!strncasecmp (s, "2", sz))
    return 2;
  else if (!strncasecmp (s, "4", sz))
    return 3;
  else
    return 4;
}

uint8_t
pgmdb_parse_fec_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "1/2", sz))
    return 0;
  else if (!strncasecmp (s, "2/3", sz))
    return 1;
  else if (!strncasecmp (s, "3/4", sz))
    return 2;
  else if (!strncasecmp (s, "5/6", sz))
    return 3;
  else if (!strncasecmp (s, "7/8", sz))
    return 4;
  else
    return 5;
}

uint8_t
pgmdb_parse_guard_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "1/32", sz))
    return 0;
  else if (!strncasecmp (s, "1/16", sz))
    return 1;
  else if (!strncasecmp (s, "1/8", sz))
    return 2;
  else if (!strncasecmp (s, "1/4", sz))
    return 3;
  else
    return 4;
}

uint8_t
pgmdb_parse_mode_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "2k", sz))
    return 0;
  else if (!strncasecmp (s, "8k", sz))
    return 1;
  else if (!strncasecmp (s, "4k", sz))
    return 2;
  else
    return 3;
}

/**
 *\brief parse a single terrestrial frequency
 *
 * the format of the file is:
 * T freq bw fec_hi fec_lo mod transmission-mode guard-interval hierarchy
 * whitespaces
 * and single line comments preceded by a # each
 *
 */
void
pgmdb_parse_freq (FILE * f, TransponderInfo * i)
{
  const size_t FEC_BUF_SZ = 32;
  char bw[FEC_BUF_SZ];
  char fec_hi[FEC_BUF_SZ];
  char fec_lo[FEC_BUF_SZ];
  char mod[FEC_BUF_SZ];
  char mode[FEC_BUF_SZ];
  char guard[FEC_BUF_SZ];
  char hier[FEC_BUF_SZ];
  fscanf (f, " %u %5s %31s %31s %31s %31s %31s %31s", &i->frequency, bw,
          fec_hi, fec_lo, mod, mode, guard, hier);
  i->frequency /= 10;
  i->pol = 0;
  i->bw = pgmdb_parse_bw_t (bw, FEC_BUF_SZ);
  i->pri = 1;                   //?
  i->mp_fec = 0;                //BUG?
  i->constellation = pgmdb_parse_mod_t (mod, FEC_BUF_SZ);
  i->hier = pgmdb_parse_hier_t (hier, FEC_BUF_SZ);      //suspicious
  i->code_rate_h = pgmdb_parse_fec_t (fec_hi, FEC_BUF_SZ);
  i->code_rate_l = pgmdb_parse_fec_t (fec_lo, FEC_BUF_SZ);
  i->guard = pgmdb_parse_guard_t (guard, FEC_BUF_SZ);
  i->mode = pgmdb_parse_mode_t (mode, FEC_BUF_SZ);
	i->inv = 2;
}

void
pgmdb_skip_line (FILE * f)
{
  int c;
  do
  {
    c = fgetc (f);
  }
  while ((c != '\n') && (c != EOF));
}
int
pgmdb_get_ti_t_initial (FILE * f, TransponderInfo * i)
{
  int c, got_tp = 0, done = 0;
  do
  {
    c = fgetc (f);
    if (c == EOF)
      done = 1;
    else
      switch (c)
      {
      case 'T':
        pgmdb_parse_freq (f, i);
        got_tp = 1;
        break;
      case '#':
        pgmdb_skip_line (f);
        break;
      default:
        break;
      }
  }
  while ((!got_tp) && (!done));

  if (done)
    return 1;

  return 0;
}

int
pgmdb_get_ti_s_initial (FILE * f, TransponderInfo * i)
{
  int c, got_tp = 0, done = 0;
  do
  {
    c = fgetc (f);
    if (c == EOF)
      done = 1;
    else
      switch (c)
      {
      case 'S':
        pgmdb_parse_tp (f, i);
        got_tp = 1;
        break;
      case '#':
        pgmdb_skip_line (f);
        break;
      default:
        break;
      }
  }
  while ((!got_tp) && (!done));

  if (done)
    return 1;

  return 0;
}
int
pgmdb_parse_initial (char * tf, PgmDb * db, uint32_t pos)        //,RcFile * cfg)
{
  TransponderInfo i;
  PgmdbPos *pp;
  FILE * f;
  f = fopen (tf, "r");
  if (!f)
  {
     errMsg ("couldn't open initial tuning file %s.\n", tf);
     return 1;
  }
  pp = pgmdb_get_or_ins_pos (db, pos);
  if (NULL != pp)
  {
    memset (&i, 0, sizeof (i));
    if (dvbIsSat (db->dvb))
      while (!pgmdb_get_ti_s_initial (f, &i))
      {
        debugMsg ("Adding tp:  %ukHz %s %ukSym/s %s\n", i.frequency * 10,
                  tpiGetPolStr (i.pol), i.srate / 10, tpiGetFecStr (i.fec));
        pgmdb_add_transp_real (db, pp, &i, 0);
        memset (&i, 0, sizeof (i));
      }
    else if (dvbIsTerr (db->dvb))
    {
      while (!pgmdb_get_ti_t_initial (f, &i))
      {
        debugMsg ("Adding freq:  %uHz\n", i.frequency * 10);
        pgmdb_add_transp_real (db, pp, &i, 0);
        memset (&i, 0, sizeof (i));
      }
    }
    else
    {
      errMsg ("unsupported delivery system\n");
      return 1;
    }
  }
  return 0;
}
