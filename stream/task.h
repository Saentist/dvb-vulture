#ifndef __task_h
#define __task_h
#include <pthread.h>
#include "custck.h"
/**
 *\file task.h
 *\brief task struct and operations
 *
 *task to include in structs which have one maintenance thread per object.<br>
 *Allows one to start stop and lock/unlock tasks mutex.<br>
 *will not start multiple tasks per object.
 */
typedef struct
{
  void *x;                      ///<data to pass to function
  void *(*task_func) (void *x); ///<taskStart will run this as pthread
  pthread_t thread;             ///<pthread
  pthread_mutex_t mx;           ///<mutex used by Lock and Unlock. Protects running.
  int running;                  ///<one if function is extiting/has exited. Zero otherwise. Func should respond to it.
} Task;

int taskInit (Task * r, void *x, void *(*func) (void *x));
int CUtaskInit (Task * r, void *x, void *(*func) (void *x), CUStack * s);
int taskClear (Task * r);
int taskLock (Task * r);        ///<acquires the mutex
int taskUnlock (Task * r);      ///<releases the mutex
int taskStart (Task * r);       ///<start task if it's not running
int taskStop (Task * r);        ///<stops task. on return task has joined(if it was running before)
#define taskDetach(_TSK) pthread_detach((_TSK)->thread)
#define taskRunning(_TSK) (((_TSK)->running))
#endif
