#ifndef CUSTCK_H
#define CUSTCK_H
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include "utl.h"

/**
 *\file custck.h
 *\brief Stack for cleanup
 *
 *defines a handy datastructure and utility functions 
 *for cleaning up allocated memory, file handles etc..
 */
//#pragma library (custck);

typedef struct _CUStackElem_ CUStackElem;

///An Element in the Stack
struct _CUStackElem_
{
  union
  {
    ///a handler to use with Handle
    void (*ff) (void *handle);
    uint32_t Id;
  };
///a handle 
  void *Handle;
};

typedef struct _CUStack_ CUStack;
///A stack
struct _CUStack_
{
  ///Number of elements allocated
  unsigned Size;
  ///Number of elements used
  unsigned Index;
  ///Array (dynamic) of Stack elements
  CUStackElem *Stack;
};


/**
 *\brief initializes a CUStack object. 
 *
 *\return NULL on error
 */
CUStack *cuStackInit (CUStack * st);
/**
 *\brief Like cuStackInit, but initializes a CUStack object to a specified number of elements
 */
CUStack *cuStackInitSize (CUStack * st, unsigned size);

/**
 *\brief clears the stack without calling handlers
 */
void cuStackUninit (CUStack * st);

/**
 *\brief malloc wrapper
 *
 *allocates a block of memory and puts pointer on the stack the handle that is put on the stack will implicitly call free.
 *If the allocation fails, stack will be cleaned and NULL returned.
 */
void *CUmalloc (size_t size, CUStack * st);

/**
 *\brief calloc wrapper
 *
 *allocates a block of memory using calloc and puts pointer on the stack the handle that is put on the stack will implicitly call free.
 *If the allocation fails, stack will be cleaned and NULL returned.
 */
void *CUcalloc (size_t nmemb, size_t size, CUStack * st);

void *CUutlMalloc (size_t size, CUStack * st);
void *CUutlCalloc (size_t nmemb, size_t size, CUStack * st);

/**
 *\brief fopen wrapper
 *
 *tries to open specified file and push it.
 *if it fails Stack will be cleaned and NULL returned
 */
FILE *CUfopen (const char *filename, const char *mode, CUStack * st);

/**
 *\brief oopen wrapper
 *
 *tries to open specified file and push it.
 *if it fails Stack will be cleaned and NULL returned
 */
int CUopen (char *filename, int flags, mode_t mode, CUStack * s);

/**
 *\brief puts handler and handle on the stack. 
 *
 *stack resizes if necessary
 */
int cuStackPush (void *handle, void (*ff) (void *handle), CUStack * st);
/**
 *\brief grows a stack
 */
int cuStackGrow (CUStack * st);
/**
 *\brief clean up the stack
 *
 *clears the stack after calling handlers on handles in reverse order
 */
void cuStackCleanup (CUStack * st);

/**
 *\brief utility func for implementing wrappers
 *
 *\param failed if nonzero Stack will be cleaned.
 *else handle and handler will be pushed
 */
void cuStackFail (int failed, void *handle, void (*ff) (void *hnd),
                  CUStack * st);

/**
 *\brief pop (clean) an element
 *
 *calls the handler of the topmost element on the stack and removes it afterwards.
 */
int cuStackPop (CUStack * st);

#endif
