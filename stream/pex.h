#ifndef __pex_h
#define __pex_h
#include <stdint.h>
/**
 *\file pex.h
 *\brief printf-like parameter expansion(roughly)
 *
 *replaces % prefixed characters with a corresponding string
 */

/**
 *\brief associates replacement strings with trigger characters
 *
 *if we would want to replace %g with the string "asdf", the trigger would be 'g' and replacement would be "asdf"
 */
typedef struct
{
  char trigger;                 ///<which character will trigger this expansion
  char *replacement;            ///<replacement string to be inserted
} PexHandler;

/**
 *\brief process a string replacing triggers with their replacements
 *
 *%% will be interpreted as a single %, regardless of any trigger
 *unknown triggers will have their % removed.
 *\return a pointer to a string holding the result, must be freed using free(). May return NULL on error.
 */
char *pexParse (PexHandler * rsp, uint32_t num_handlers, char *tpl);
#endif
