#include <string.h>
#include "utl.h"
#include "pex.h"
#include "dmalloc.h"
static char *
pex_size_check (uint32_t * buf_size, uint32_t need_size, char **rv)
{
  if ((*buf_size) < need_size)
  {
    void *np;
    *buf_size = (*buf_size) * 2;
    np = utlRealloc (*rv, *buf_size);
    if (!np)
    {
      utlFAN (*rv);
      *buf_size = 0;
      *rv = NULL;
      return NULL;
    }
    else
      *rv = np;
  }
  return *rv;
}

static char *
pex_exp (char p, PexHandler * rsp, uint32_t num_handlers, uint32_t * buf_size,
         uint32_t * i, char **rv)
{
  uint32_t j;
  uint32_t idx = *i;
  char *ptr = *rv;
  if (p != '%')
  {
    for (j = 0; j < num_handlers; j++)
    {
      if (p == rsp[j].trigger)
      {
        char *rp;
        if (NULL == rsp[j].replacement)
          rp = "NULL";
        else
          rp = rsp[j].replacement;

        if (NULL == pex_size_check (buf_size, idx + 1 + strlen (rp), rv))
          return NULL;
        ptr = *rv;
        strcpy (ptr + idx, rp);
        idx += strlen (rp);
        break;
      }
    }
  }
  else
    j = num_handlers;
  if (j == num_handlers)
  {                             //no replacement found
    if (NULL == pex_size_check (buf_size, idx + 1, rv))
    {
      return NULL;
    }
    ptr = *rv;
    ptr[idx] = p;
    idx++;
  }
  *i = idx;
  *rv = ptr;
  return ptr;
}

char *
pexParse (PexHandler * rsp, uint32_t num_handlers, char *tpl)
{
  char *rv;
  char *p;
  uint32_t i;
  int trig;
  uint32_t buf_size = 1024;
  rv = utlCalloc (1, buf_size);
  p = tpl;
  i = 0;
  trig = 0;
  while (*p)
  {
    if (*p == '%')
    {
      trig = 1;
      p++;
    }
    else
    {
      if (trig)
      {
        if (NULL == pex_exp (*p, rsp, num_handlers, &buf_size, &i, &rv))
          return NULL;
        trig = 0;
        p++;
      }
      else
      {
        if (NULL == pex_size_check (&buf_size, i + 1, &rv))
          return NULL;
//                              if(utlSmartCatUt(&rv,&buf_size,p,sizeof(char)))
        //                      {
        //                      utlFAN(rv);
        //              return NULL;
        //}
        rv[i] = *p;
        i++;
        p++;
      }
    }
  }
  if (NULL == pex_size_check (&buf_size, i + 1, &rv))
    return NULL;
  rv[i] = *p;                   //'\0'
  i++;
  p = utlRealloc (rv, i);
  if (NULL == p)
  {
    utlFAN (rv);
    rv = NULL;
  }
  else
    rv = p;
  return rv;
}
