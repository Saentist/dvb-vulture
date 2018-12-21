#include <windows.h>
#include <winbase.h>

#include <string.h>             /* For the real memcpy prototype.  */

size_t
strnlen (const char *s, size_t maxlen)
{
  size_t rv = 0;
  while ((rv < maxlen) && ((*s) != '\0'))
  {
    rv++;
    s++;
  }
  return rv;
}


char *
strndup (const char *s, size_t n)
{
  size_t l;
  char *rv;
  l = strnlen (s, n);
  rv = utlCalloc (1, l + 1);
  memmove (rv, s, l + 1);
  if (l == n)
    rv[l] = '\0';
  return rv;
}
