#if !defined(__WATCOMC__)

#include <pthread.h>

#else
/*
stuff that's not needed for DOS
version of client
may have to fix for win32...
*/
#define pthread_mutex_t int
#define PTHREAD_MUTEX_INITIALIZER 0
#define pthread_mutex_lock(x_)
#define pthread_mutex_unlock(x_)

#endif
