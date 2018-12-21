#ifndef __debug_h
#define __debug_h

#include <stdint.h>
#include <stdbool.h>
#include "bifi.h"

#if defined (__WIN32__) || defined (__WATCOMC__)
#define LOG_DEBUG 1
#define LOG_ERR 2
#else
#include <syslog.h>
#endif

/**
 *\file debug.h
 *\brief debugging and error reporting functions
 *
 *error messages are always reported<br>
 *debug messages are only reported if their flag is set<br>
 *modules are registered using addDebugModule()<br>
 *the reporting function and worst-case number of modules must be set exactly once using setDebugFunc()<br>
 *active debugging flags may be set using setDebugFlags()<br>
 *Debug variables must be initialized using addDebugModule().<br>
 *It associates the module's name with it's debug handle which it returns.<br>
 */

extern uint8_t *DebugFlags;

/**
 *\brief use this to report errors
 *
 *uses printf-like syntax
 */
#define errMsg(a,...)\
(DebugPrintf(DebugData,__FILE__,__LINE__,LOG_ERR,a,##__VA_ARGS__))

//#define debugOn() (bifiGet(DebugFlags,FILE_DBG))
#define debugOn() (DebugFlags[FILE_DBG])
#define debugOnPri(_pri) (DebugFlags[FILE_DBG]>=(_pri))
/**
 *\brief Use this to report debug messages
 *
 *uses printf-like syntax<br>
 *errors are always reported<br>
 *Hmm... is it possible to do this without that GNU extension ## ?
 */
#define debugMsg(a,...)\
(debugOn()?DebugPrintf(DebugData,__FILE__,__LINE__,LOG_DEBUG,a,##__VA_ARGS__):0)

#define debugPri(pri,a,...)\
(debugOnPri(((pri)!=0)?(pri):1)?DebugPrintf(DebugData,__FILE__,__LINE__,LOG_DEBUG,a,##__VA_ARGS__):0)

extern void *DebugData;

/**
 *\brief holds the pointer to the output function
 *
 *\param x user pointer
 *\param f the file name
 *\param l the line number
 *\param pri logging priority. It is either LOG_DEBUG or LOG_ERR. Nice if you want to syslog.
 *\param template a printf-like format string
 *\param ... additional arguments
 */
extern int (*DebugPrintf) (void *x, const char *f, int l, int pri,
                           const char *template, ...);


/**
 *\brief use this to set the error reporting function
 *
 *Also adds the modules internal to the library(btree,custck,fp_mm,rcptr,item_list..)<br>
 *\param data the function will be called with this as its x argument. May be used to pass user context. May be NULL.
 *\param xprintf will be called to output messages
 *\warning call this exactly once
 */
int debugSetFunc (void *data,
                  int (*xprintf) (void *x, const char *f, int l, int pri,
                                  const char *template, ...),
                  uint32_t num_modules);

/**
 *\brief add a debug module with specified name
 *
 *\param s name to use
 *\return the handle to use for the module's FILE_DBG define
 */
int debugAddModule (const char *s);

/**
 *\brief interprets debugging options in the argument vector
 *
 *debugging options are specified as '-d xyz' with xyz being the name of a module previously registered.<br>
 *It is also possible to use a verbosity like '-d xyz=3'. Defaults to 255(the maximum) if omitted.<br>
 *multiple -d options may be specified. the special name 'all' enables all debugging messages.<br>
 *\param pfx The option string to use. "-d" in the above examples, but may be arbitrary. Must not be NULL.
 *\warning call this _after_ adding all debug modules as their names will not be known beforehand.
 */
int debugSetFlags (int argc, char *argv[], char *pfx);

/**
 *set this to one to have an 
 *endless loop in debugTrace.
 */
extern bool DebugSBT;
/*
use backtrace()
to dump a backtrace to log.
*/
void debugTrace (void);

#endif //__debug_h
