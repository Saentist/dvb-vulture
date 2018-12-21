uint8_t
pgmdb_parse_pol_s (char *s, size_t sz)
{
  if (!strncasecmp (s, "HORIZONTAL", sz))
    return 0;
  else if (!strncasecmp (s, "VERTICAL", sz))
    return 1;
  else if (!strncasecmp (s, "LEFT", sz))
    return 2;
  else if (!strncasecmp (s, "RIGHT", sz))
    return 3;
  else
    return 4;//is invalid!!!
}

uint8_t
pgmdb_parse_fec_s (char *s, size_t sz)
{
  if (!strncasecmp (s, "auto", sz))
    return 0;
  else if (!strncasecmp (s, "1/2", sz))
    return 1;
  else if (!strncasecmp (s, "2/3", sz))
    return 2;
  else if (!strncasecmp (s, "3/4", sz))
    return 3;
  else if (!strncasecmp (s, "5/6", sz))
    return 4;
  else if (!strncasecmp (s, "7/8", sz))
    return 5;
  else if (!strncasecmp (s, "8/9", sz))
    return 6;
  else if (!strncasecmp (s, "3/5", sz))
    return 7;
  else if (!strncasecmp (s, "4/5", sz))
    return 8;
  else if (!strncasecmp (s, "9/10", sz))
    return 9;
  else
    return 0;
}
uint8_t pgmdb_parse_inversion_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "OFF", sz))
    return 0;
  else if (!strncasecmp (s, "ON", sz))
    return 1;
  else if (!strncasecmp (s, "AUTO", sz))
    return 2;
  else
    return 2;
}

/**
 *\brief parse a single transponder from initial tuning file (satellite only)
 *
 * the format of the file is:
 * whatever...
 *
 * uses a Section from tuning file...
 */
int
pgmdb_get_ti_s_initial (Section *s, TransponderInfo * i)
{
  long f;
  char * p;
  
  p="NONE";
  rcfileFindSecVal(s,"DELIVERY_SYSTEM",&p);
  
  if(strncasecmp (p, "DVBS", 10)&&strncasecmp (p, "DVBS2", 10))
	  return 1;

  if(rcfileFindSecValInt(s,"FREQUENCY",&f))
  	return 1;
  i->frequency=f/10;

  p="AUTO";
  rcfileFindSecVal(s,"POLARIZATION",&p);
  i->pol = pgmdb_parse_pol_s(p,12);
  if(i->pol>3)
  	return 1;
  
  f=22000000;
  rcfileFindSecValInt(s,"SYMBOL_RATE",&f);
  i->srate = f/100;
  
  p="AUTO";
  rcfileFindSecVal(s,"INNER_FEC",&p);
  i->fec = pgmdb_parse_fec_s (p,10);
  
  p="AUTO";
  rcfileFindSecVal(s,"INVERSION",&p);
  i->inv = pgmdb_parse_inversion_t (p,20);

	return 0;
}

uint8_t
pgmdb_parse_bw_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "8000000", sz))
    return 0;
  else if (!strncasecmp (s, "7000000", sz))
    return 1;
  else if (!strncasecmp (s, "6000000", sz))
    return 2;
  else if (!strncasecmp (s, "5000000", sz))
    return 3;
  else
    return 4;
}

uint8_t
pgmdb_parse_mod_t (char *s, size_t sz)
{
  if (!strncasecmp (s, "QPSK", sz))
    return 0;
  else if (!strncasecmp (s, "QAM/16", sz))
    return 1;
  else if (!strncasecmp (s, "QAM/64", sz))
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
 */
int
pgmdb_get_ti_t_initial (Section *s, TransponderInfo * i)
{
  long f;
  char * p;
  
  p="NONE";
  rcfileFindSecVal(s,"DELIVERY_SYSTEM",&p);
  if(strncasecmp (p, "DVBT", 10)&&strncasecmp (p, "DVBT2", 10))
	  return 1;

  /*freq must be present. anything else can be guessed
  
  a lot of those things are different from tp_info.c ...
  which one is wrong? NONE...
  (DVB format nis.h VS LINUX frontend.h...)
  */
  if(rcfileFindSecValInt(s,"FREQUENCY",&f))
  	return 1;

  i->frequency=f/10;
  i->pol = 0;
  i->pri = 1;                   //?
  i->mp_fec = 0;                //BUG?

  p="AUTO";
  rcfileFindSecVal(s,"BANDWIDTH_HZ",&p);
  i->bw=pgmdb_parse_bw_t(p,20);

  p="AUTO";
  rcfileFindSecVal(s,"CODE_RATE_HP",&p);
  i->code_rate_h = pgmdb_parse_fec_t (p,20);

  p="AUTO";
  rcfileFindSecVal(s,"CODE_RATE_LP",&p);
  i->code_rate_l = pgmdb_parse_fec_t (p,20);

  p="AUTO";
  rcfileFindSecVal(s,"MODULATION",&p);
  i->constellation = pgmdb_parse_mod_t (p,20);

  p="AUTO";
  rcfileFindSecVal(s,"TRANSMISSION_MODE",&p);
  i->mode = pgmdb_parse_mode_t (p,20);

  p="AUTO";
  rcfileFindSecVal(s,"GUARD_INTERVAL",&p);
  i->guard = pgmdb_parse_guard_t (p,20);

  p="NONE";
  rcfileFindSecVal(s,"HIERARCHY",&p);
  i->hier = pgmdb_parse_hier_t (p,20);      //suspicious

  p="AUTO";
  rcfileFindSecVal(s,"INVERSION",&p);
  i->inv = pgmdb_parse_inversion_t (p,20);

  return 0;
}


int
pgmdb_parse_initial (char * tf, PgmDb * db, uint32_t pos)        //,RcFile * cfg)
{
  TransponderInfo i;
  PgmdbPos *pp;
  RcFile rcf;

  pp = pgmdb_get_or_ins_pos (db, pos);
  if (NULL != pp)
  {
    memset (&i, 0, sizeof (i));
	  if(rcfileParse(tf,&rcf))
	  {
	  	errMsg("Failed to parse %s\n",tf);
	  	return 1;
	  }
    if (dvbIsSat (db->dvb))
    {
    	int j;
    	for(j=0;j<rcf.section_count;j++)
    	{
    		if(pgmdb_get_ti_s_initial (&rcf.sections[j], &i))
    			return 1;
        debugMsg ("Adding tp:  %ukHz %s %ukSym/s %s\n", i.frequency * 10,
                  tpiGetPolStr (i.pol), i.srate / 10, tpiGetFecStr (i.fec));
        pgmdb_add_transp_real (db, pp, &i, 0);
        memset (&i, 0, sizeof (i));
    	}
    }
    else if (dvbIsTerr (db->dvb))
    {
    	int j;
    	for(j=0;j<rcf.section_count;j++)
    	{
      	if(pgmdb_get_ti_t_initial (&rcf.sections[j], &i))
      		return 1;
        debugMsg ("Adding freq:  %uHz\n", i.frequency * 10);
        pgmdb_add_transp_real (db, pp, &i, 0);
        memset (&i, 0, sizeof (i));
    	}
    }
    else
      errMsg ("unsupported delivery system\n");
    rcfileClear(&rcf);
  }
  return 0;
}

