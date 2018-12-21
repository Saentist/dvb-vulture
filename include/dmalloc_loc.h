//local dmalloc proxy to avoid it for win32 compile
#if !(defined (__WIN32__) || defined (__WATCOMC__))
#include "dmalloc.h"
#endif
