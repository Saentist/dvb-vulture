#include <windows.h>
#include <winbase.h>
#include <winreg.h>

/**
 *\file rcfile2.c
 *\brief gets included for OS specific configuration storage
 */


/*
change this to use a file path
so I can take it apart and 
redirect into the registry...

if there's leading . put in HCU/Software...
else the machine-global store.. etc

*/

char *
skip_leading_slashes (char *s)
{
  if (NULL == s)
    return NULL;
  while (*s == '/')
    s++;
  return s;
}

char *
leading_dot (char *s)
{
  return strchr (s, '.');
}

char *
strip_etc (char *s)
{
  char *p;
  p = strstr (s, "/etc/");
  if (NULL == p)
    return s;
  else
    return p;
}

/*
take apart a cfg path..
if there's a component with leading '.',
is_local returns true
nm_ret is the part after the .
otherwise
is_local returns false and
nm_ret is part after leading /etc/
or all if no /etc/
*/
int
dissect_path (char *conf_file, char **nm_ret, BOOL * is_local)
{
  char *idx;
  if (NULL == conf_file)
    return 1;
  if ((idx = leading_dot (conf_file)) != NULL)
  {
    idx++;
    idx = skip_leading_slashes (idx);
    if (idx == '\0')            //end?
      return 1;
    *nm_ret = idx;
    *is_local = TRUE;
    return 0;
  }
  else
  {
    idx = strip_etc (conf_file);
    idx = skip_leading_slashes (idx);
    if (idx == '\0')            //end?
      return 1;
    *nm_ret = idx;
    *is_local = FALSE;
    return 0;
  }
  return 1;
}

void
rev_slashes (char *p)
{
  while (*p)
  {
    if (*p == '/')
      *p = '\\';
    p++;
  }
}

#define buflen 2048

static void
read_section (HKEY top, Section * a, int ctr)
{
  int result;
  int i, j, idx_sz;
  char *otherbuf = NULL, *b3 = NULL;
  LPTSTR keybuf = NULL;
  BYTE *valuebuf = NULL;
  HKEY subkey1;
  DWORD tp;

  otherbuf = utlCalloc ((buflen) + 1, sizeof (char));
  keybuf = utlCalloc ((buflen) + 1, sizeof (TCHAR));
  valuebuf = utlCalloc ((buflen) + 1, sizeof (BYTE));
  b3 = utlCalloc ((buflen) + 1, sizeof (char));

  a[ctr].name = NULL;
  a[ctr].kv_count = 0;
  a[ctr].pairs = NULL;

  result = RegEnumKey (top, ctr, otherbuf, buflen);
  if (result == ERROR_SUCCESS)
  {
    Section *s = &a[ctr];
    s->name = strdup (otherbuf);
    debugMsg ("Section: %s\n", otherbuf);
    //getting the type... only want to use non-leaf nodes
    //oh, it's OK already... keys are the non-leaf nodes
    //My key-values correspond with
    //Value Name- Value Data here...
    if (RegOpenKey (top, otherbuf, &subkey1) != ERROR_SUCCESS)
    {
      errMsg ("RegOpenKey failed: %s\n", otherbuf);
      return;
    }
    i = 0;
    j = 0;
    do
    {
      DWORD key_sz = buflen, val_sz = buflen;
      result =
        RegEnumValue (subkey1, i, keybuf, &key_sz, 0, &tp, valuebuf, &val_sz);
      if (result == ERROR_SUCCESS)
      {
        if (tp == REG_DWORD ||
            tp == REG_DWORD_LITTLE_ENDIAN ||
            tp == REG_DWORD_BIG_ENDIAN || tp == REG_EXPAND_SZ || tp == REG_SZ)
          j++;                  //number of kvs
        i++;                    //index counter...
      }
    }
    while (result == ERROR_SUCCESS);
    //...and hope nothing changes inbetween ^_^
    s->kv_count = j;
    s->pairs = utlCalloc (sizeof (KvPair), j);
    debugMsg ("Pairs buffer: %p\n", s->pairs);
    idx_sz = i;
    if (j != 0)
      for (i = 0, j = 0; i < idx_sz; i++)
      {
        DWORD key_sz = buflen, val_sz = buflen;
        result =
          RegEnumValue (subkey1, i, keybuf, &key_sz, 0, &tp, valuebuf,
                        &val_sz);
        keybuf[key_sz] = '\0';
        valuebuf[val_sz] = '\0';
        if (result == ERROR_SUCCESS)
        {
          KvPair *p = &s->pairs[j];
          char *k = NULL;
          char *v = NULL;
          switch (tp)
          {
          case REG_DWORD:      //dwords are unsigned ..trouble?
//                                      case REG_DWORD_LITTLE_ENDIAN:
            k = strdup (keybuf);
            asprintf (&v, "%lu", *((DWORD *) valuebuf));
            break;
          case REG_DWORD_BIG_ENDIAN:
            k = strdup (keybuf);
            asprintf (&v, "%lu", ntohl (*((DWORD *) valuebuf)));
            break;
          case REG_EXPAND_SZ:
            k = strdup (keybuf);
//do outside                                    ExpandEnvironmentStrings(val,keybuf,key_sz);
            v = strdup (valuebuf);
            break;
          case REG_SZ:
            k = strdup (keybuf);
            v = strdup (valuebuf);
            break;
          default:
            break;
          }
          if (k)
          {
            p->key = k;
            debugMsg ("Key: %s\n", k);
            p->value = v;
            debugMsg ("Val: %s\n", v);
            j++;
          }
        }
      }
    RegCloseKey (subkey1);
  }
  utlFAN (otherbuf);
  utlFAN (keybuf);
  utlFAN (valuebuf);
  utlFAN (b3);
}

static char *
mangle_rcname (char *cfg, HKEY * root_r)
{
  char *name, *p, *q;
  char *otherbuf;
  int result;
  BOOL is_local;
  HKEY root;

  if (dissect_path (cfg, &name, &is_local))
    return NULL;

  if (NULL == (q = strdup (name)))
    return NULL;

  rev_slashes (q);
  asprintf (&p, "Software\\%s", q);
  utlFAN (q);
  root = is_local ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  debugMsg ("Using Registry Key: %s in %s\n", p,
            is_local ? "HKEY_CURRENT_USER" : "HKEY_LOCAL_MACHINE");
  *root_r = root;
  return p;
}

int
rcfileParse (char *conf_file, RcFile * data)
{
  char *p;
  char *otherbuf;
  int result;
  HKEY root;
  HKEY subkey1;
  int ctr = 0;

  if (!data)
    return 1;

  if (conf_file == NULL)
    return 1;

  p = mangle_rcname (conf_file, &root);
  if (NULL == p)
    return 1;

  if (RegOpenKey (root, p, &subkey1) != ERROR_SUCCESS)
  {
    utlFAN (p);
    return 1;
  }

  otherbuf = utlCalloc ((3 * buflen) + 1, sizeof (char));
  if (NULL == otherbuf)
  {
    RegCloseKey (subkey1);
    utlFAN (p);
    errMsg ("calloc");
    return 1;

  }
  ctr = 0;
  do
  {
    result = RegEnumKey (subkey1, ctr, otherbuf, 3 * buflen);
    if (result == ERROR_SUCCESS)
    {
      ctr++;
    }
  }
  while (result == ERROR_SUCCESS);

  data->sections = utlCalloc (sizeof (Section), ctr);
  if (!data->sections && (ctr != 0))
  {
    utlFAN (otherbuf);
    RegCloseKey (subkey1);
    utlFAN (p);
    errMsg ("calloc");
    return 1;
  }
  data->section_count = ctr;
  for (ctr = 0; ctr < data->section_count; ctr++)
  {
    read_section (subkey1, data->sections, ctr);
  }

  utlFAN (otherbuf);
  RegCloseKey (subkey1);
  utlFAN (p);

  return 0;
}


int
rcfileExists (char *cfg)
{
  char *p;
  HKEY root;
  HKEY subkey1;

  if (cfg == NULL)
    return 0;

  p = mangle_rcname (cfg, &root);

  if (NULL == p)
    return 0;

  if (RegOpenKey (root, p, &subkey1) != ERROR_SUCCESS)
  {
    utlFAN (p);
    return 0;
  }
  RegCloseKey (subkey1);
  utlFAN (p);
  return 1;
}

#ifdef TEST
#undef errMsg
#define errMsg printf
#endif

int
rcfileCreate (char *cfg)
{
  char *p;
  HKEY root;
  HKEY k;

  if (rcfileExists (cfg))
    return 1;

  if (cfg == NULL)
    return 0;

  p = mangle_rcname (cfg, &root);


  if (ERROR_SUCCESS != RegCreateKey (root, p, &k))      //supposedly this creates all keys higher up, too.
  {
    errMsg ("RegCreateKey() failed\n");
    return 1;
  }
  RegCloseKey (k);
  utlFAN (p);
  return 0;
}
