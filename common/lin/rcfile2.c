#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

/**
 *\file rcfile2.c
 *\brief gets included for OS specific configuration storage
 *plain files in linux, registry in windows..
 */

/**
 *returns  0: nothing/empty/unidentified
 *         1: section
 *         2: key_value
 *        -1: error(eof)
 */
static int
rcfile_parse_ln (FILE * fp, char *temp, char **start, int n)
{
  char *end, *eq_sgn;
  if (!fgets (temp, n, fp))
    return -1;

  *start = temp + strspn (temp, " \t");
  if (*start[0] == '#')
    return 0;
  end = strchr (temp, ']');
  if ((!end) && (*start[0] != '['))
  {
    eq_sgn = strchr (temp, '=');
    if (eq_sgn)
    {
      return 2;
    }

    return 0;
  }
  if ((!end) || (!(*start[0] == '[')) || (end < *start))
  {
    return 0;
  }

  (*start)++;
  end[0] = '\0';


  return 1;
}

static int
rcfile_split_kv (char *string, char **key, char **value)
{
  char *t1, *t2;
  char *ptr;
  int l;

  if (!string)
    return 1;
  if (!key)
    return 1;
  if (!value)
    return 1;
//  puts("4.2.4");

  t1 = strtok_r (string, "=", &ptr);
  //puts("4.2.5");

  //fgets doesn't convert newlines, so it's done here
  t2 = strtok_r (NULL, "=\n", &ptr);

//  puts("4.2.6");
  if (!t1)
    return 1;
  if (!t2)
    return 1;

  *key = strtok_r (t1, " \t", &ptr);
  t2 += strspn (t2, " \t");
  l = strlen (t2);
  while (l > 0)
  {
    l--;
    if (isblank (t2[l]))
      t2[l] = '\0';
    else
      break;
  }
  if ((t2[l] == '\"') && (t2[0] == '\"') && l)
  {
    t2[l] = '\0';
    t2 = &(t2[1]);
  }

  ptr = t2;
  while ((ptr = index (ptr, '\\')))
  {
    if (ptr[1] == 'n')          //"\n" newline translation
    {
      ptr[0] = '\n';            //insert newline
      memmove (ptr + 1, ptr + 2, strlen (ptr + 2));     //move remaining characters
    }
    else if (ptr[1] == '\\')    //"\\"backslash translation
    {
      ptr[0] = '\\';            //insert backslash
      memmove (ptr + 1, ptr + 2, strlen (ptr + 2));     //move remaining characters
    }
    ptr++;
  }

  *value = t2;
  if (!*key)
    return 1;
  if (!*value)
    return 1;
  return 0;
}

static int
count_sections (FILE * conf_file, char *temp, int buf_size)
{
  char *start;
  int ctr, result;
  ctr = 0;
  do
  {
    result = rcfile_parse_ln (conf_file, temp, &start, buf_size);
    if (result == 1)            //count sections
      ctr++;
  }
  while (result != -1);
  rewind (conf_file);
  return ctr;
}

static int
skip_to_first_section (FILE * conf_file, char *temp, char **start,
                       int buf_size)
{
  int result;
  do
  {
    result = rcfile_parse_ln (conf_file, temp, start, buf_size);
  }
  while ((result != 1) && (result != -1));      //skip to first section
  return result;
}

static int
count_pairs (FILE * conf_file, char *temp, char **start, int buf_size)
{
  long pos;
  int kctr = 0;
  int result;
  //store filepos
  pos = ftell (conf_file);
  if (pos == -1)
  {
    errMsg ("ftell: %s", strerror (errno));
    return -1;
  }

  do                            //count KvPairs
  {
    result = rcfile_parse_ln (conf_file, temp, start, buf_size);
    if (result == 2)
    {
      kctr++;
    }
  }
  while ((result != 1) && (result != -1));

  //reset pos to  start of section
  if (-1 == fseek (conf_file, pos, SEEK_SET))
  {
    errMsg ("fseek: %s", strerror (errno));
    return -1;
  }
  return kctr;
}

static int
parse_pairs (Section * sec, FILE * conf_file, char *temp, char **start_r,
             int buf_size)
{
  int result;
  int kctr = 0;
  char *start = *start_r, *key = NULL, *value = NULL;
  do
  {
    result = rcfile_parse_ln (conf_file, temp, &start, buf_size);
    *start_r = start;
    if (result == 2)            //key-value
    {
      KvPair *val = &(sec->pairs[kctr]);
      if (rcfile_split_kv (start, &key, &value))
      {
        errMsg ("rcfile_split_kv");
        return 20;
      }
      val->key = strdup (key);
      val->value = strdup (value);
      if ((!val->key) || (!val->value))
      {
        errMsg ("strdup: %s", strerror (errno));
        return 20;
      }
      kctr++;
    }
  }
  while ((result != 1) && (result != -1));      //end at next section or end
  return result;
}

static int
parse_sections (int ctr, RcFile * data, FILE * conf_file, char *temp,
                int buf_size)
{
  int result;
  char *start;
  data->sections = utlCalloc (sizeof (Section), ctr);
  if (NULL == data->sections)
    return 1;

  data->section_count = ctr;
  result = skip_to_first_section (conf_file, temp, &start, buf_size);
  ctr = 0;
  while (result != -1)
  {
    int kctr;
    Section *sec = &(data->sections[ctr]);
    kctr = 0;

    sec->name = strdup (start);
    if (!sec->name)
    {
      errMsg ("strdup: %s", strerror (errno));
      return 1;
    }
    kctr = count_pairs (conf_file, temp, &start, buf_size);
    if (kctr < 0)
      return 1;

    sec->pairs = utlCalloc (sizeof (KvPair), kctr);
    if ((!sec->pairs) && (kctr != 0))
    {
      errMsg ("calloc");
      return 1;
    }
    sec->kv_count = kctr;
    result = parse_pairs (sec, conf_file, temp, &start, buf_size);
    if (result == 20)
      return 1;
    ctr++;
  }
  return 0;
}

int
rcfileParse (char *cfg, RcFile * data)
{
  FILE *conf_file;
#define buf_size 2048
  char *temp;
  int rv = 0;
  int ctr = 0;

  if ((!data) || (!cfg))
    return 1;
  if (NULL == (conf_file = fopen (cfg, "r")))
    return 1;

  memset (data, 0, sizeof (RcFile));
  if (NULL != (temp = utlCalloc (1, buf_size + 1)))
  {
    ctr = count_sections (conf_file, temp, buf_size);
    data->sections = NULL;
    data->section_count = 0;
    rv = 0;
    if (ctr != 0)
    {
      rv = parse_sections (ctr, data, conf_file, temp, buf_size);
      if (rv)
        rcfileClear (data);
    }
    free (temp);
  }
  else
    rv = 1;
  fclose (conf_file);
  return rv;
}

/**
 *check if rcfile exists
 *either as a file(LINUX)
 *or as registry key(WIN32)
 */
int
rcfileExists (char *cfg)
{
  return !access (cfg, R_OK);
}

/*
/bin/asdf/ffcc
./aasdd/sdsa
asdd/ssdf
asdd
*/
#ifdef TEST
#undef errMsg
#define errMsg printf
#endif
static int
create_path (char *cfg)
{
  char *p = NULL;
  int rv = 0;
  char *s = NULL;

  if ((cfg[0] == '\0') || (NULL == (s = strdup (cfg))))
    return 0;

  p = strrchr (s, '/');
  rv = 0;
  if (p != NULL)
  {
    *p = '\0';
    rv = create_path (s);
  }
  if (0 == rv)
  {
    rv = mkdir (cfg, 0777);
    if (rv)
    {
      if (errno == EEXIST)
        rv = 0;
      else
        errMsg ("mkdir(%s,0777): %s\n", strerror (errno));
    }
  }
  free (s);
  return rv;
}


int
rcfileCreate (char *cfg)
{
  int fd, rv;
  char *s, *p;

  if (rcfileExists (cfg))
    return 1;

  s = strdup (cfg);
  if (NULL == s)
    return 1;

  rv = 0;
  p = strrchr (s, '/');
  if (p != NULL)
  {
    *p = '\0';
    rv = create_path (s);
  }
  free (s);
  if (rv)
    return 1;

  fd = creat (cfg, 0644);
  if (fd == -1)
  {
    errMsg ("creat(): %s\n", strerror (errno));
    return 1;
  }
  close (fd);
  return 0;
}
