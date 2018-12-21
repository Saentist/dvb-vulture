#ifndef __utl_h
#define __utl_h
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#include <time.h>
/**
 *\file utl.h
 *
 *\brief some handy utilities for character strings and allocations
 *
 *Slow, but good enough.
 */

///replace non ascii printable chars with spaces(I think this eats newlines...)
void utlAsciiSanitize (char *p);
void utlWSanitize (wchar_t * p);

/**
 *\brief concat+realloc
 *
 *Will fail if called with str_p, size_p or str2 equal NULL or if allocation fails.
 *
 *\param str_p points to a char pointer pointing to a c string. 
 *May point to a char * holding NULL when calling, in which case 
 *memory will be allocated to hold the string to append. 
 *space may be reallocated to accomodate the string.
 *\param size_p points to an unsigned holding allocated size. May be modified.
 *\param str2 string to append. Must be NUL terminated. Must not be NULL.
 *\returns 1 on error, 0 on success.
 */
int utlSmartCat (char **str_p, unsigned *size_p, char *str2);
///concat+realloc unterminated second string. _Always_ uses size2 bytes from str2.
int utlSmartCatUt (char **str_p, unsigned *size_p, char *str2,
                   unsigned size2);
///realloc to strlen+1 (not actually necessary)
int utlSmartCatDone (char **str_p);
//the same for ucs4. size_p and size2 in wchar. 
int utlSmartCatW (wchar_t ** str_p, unsigned *size_p, wchar_t * str2);
int utlSmartCatUtW (wchar_t ** str_p, unsigned *size_p, wchar_t * str2,
                    unsigned size2);
int utlSmartCatDoneW (wchar_t ** str_p);

///smart cat for general arrays
int utlSmartMemmove (uint8_t ** str_p, unsigned *size_p, unsigned *alloc_p,
                     uint8_t * str2, unsigned size2);
///like above, but with size and alloc counting size2 units, not bytes. appends nmemb*size2 bytes.
int utlAppend (uint8_t ** str_p, unsigned *size_p, unsigned *alloc_p,
               uint8_t * str2, unsigned nmemb, unsigned size2);
///like above, but for a single object
#define utlAppend1(a_,b_,c_,d_,e_) utlAppend(a_, b_, c_, d_, 1, e_)
/**
 *\brief removes element at index idx
 *
 * by memmoving elements at i>idx forward overwriting element at idx
 * does not realloc() down..
 *
 *\param str_p is pointing to array
 *\param size_p points to the size of array(in elements, not bytes). gets decremented by one
 *\param e_sz is the size of one element
 *\idx is the index of element to remove(in elements, not bytes)
 *\return 1 if idx out if range, 0 otherwise.
 */
int utlRmv (uint8_t * str_p, unsigned *size_p, unsigned e_sz, unsigned idx);


extern char *UTL_UNDEF;
/**
 *\brief string lookup
 *
 *look up string at idx
 *\param idx index to look up. Must be some integer type.
 *\param tbl _has_ to be of type char *tbl[]
 *\return a char * from tbl or STRU_UNDEF if out of bounds. STRU_UNDEF is a printable char *.
 */
#define struGetStr(tbl,idx) (((idx)>=(sizeof(tbl)/sizeof(char*)))?UTL_UNDEF:tbl[(idx)])

// warning: those are macros, so they do not fight with dmalloc. May evaluate arguments often... have to do differently..
#define utlFree(p_) do{if((p_)!=NULL)free(p_);}while(0)
#define utlMalloc(sz_) (((sz_)!=0)?malloc(sz_):NULL)
#define utlCalloc(n_,sz_) ((((n_)*(sz_))!=0)?calloc(n_,sz_):NULL)
#define utlRealloc(p_,sz_) realloc(p_,sz_)
#define utlFAN(p_) do{utlFree(p_);p_=NULL;}while(0)

///to silence some warnings
#ifndef __WATCOMC__
#define NOTUSED __attribute__((unused))
#else
#define NOTUSED
#endif

///array number of elements
#define ARR_NE(a_) (sizeof((a_))/sizeof(*(a_)))


#ifdef __WIN32__
char *strndup (const char *s, size_t n);
#define timegm _mkgmtime

#endif

#endif
