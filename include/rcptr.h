#ifndef __rcptr_h
#define __rcptr_h
#include "item_list.h"
/**
 *\file rcptr.h
 *\brief implements a reference counted pointer
 *
 * rcptrMalloc is used to allocate a block of memory
 * rcptrAcquire() and rcptrRelease() are used to keep track of referencees
 * malloc and acquiring increases the reference count releasing decreases it.
 * when it reaches zero, the memory block is freed using free().
 *
 * there is also another interface using the constant-size item_list allocator
 * rcptrMallocIl() is similar to rcptrMalloc().
 * rcptrReleaseIl() is similar to rcptrRelease().
 * the two variants are not interoperable and must not be mixed (eg. rcptrMalloc() and rcptrReleaseIl() is definitely a bad idea)
 *
 * rcptrRefcount() and rcptrAcquire() may be used with both variants
 */

/**
 *\brief get the reference counter
 *
 * works with both variants of rcpointers(malloc and item_list)
 *
 * \return the value of the reference counter or 0 on error
 */
uint32_t rcptrRefcount (void *ptr);

/**
 *\brief increments the internal reference counter
 *
 * works with both variants of rcpointers(malloc and item_list)
 * Has to be done every time another owner acquires a reference (when the pointer gets copied)
 * ptr must have been allocated using rcptr_malloc()
 *
 * \return -1 on error, 0 otherwise
 */
int rcptrAcquire (void *ptr);


//--------------NEXT ARE TWO DIFFERENT INTERFACES WHICH MUST NOT BE MIXED-----------------

//---------------------------------(1)Using malloc()-----------------------------------------



/**
 *\brief returns a pointer with reference count of one
 *
 * Pointer returned must not be freed using free as it's not pointing at the base of an allocated block
 * Use rcptrRelease() instead
 *
 * \param size The desired size of the allocation.
 *
 * \return pointer to the allocated memory of specified size or NULL on error
 */
void *rcptrMalloc (size_t size);


/**
 *\brief decrement internal reference count
 *
 * if counter reaches zero, free() will be called on base pointer implicitly 
 *
 * \return 1 if the object was freed and release must not be called again, 
 * 0 if the counter was decremented but object was not freed, 
 * -1 on error (freed twice or no RCPtr).
 */
int rcptrRelease (void *ptr);




//------------------------(2)Using itemListAcquire()------------------------------



/**
 * \brief like itemListInit() but with reference counted items
 */
int rcptrListInit (ItemList * l, uint32_t size, uint32_t high_water);

/**
 * \brief does the same as itemListClear()
 *
 * must not be used until all of the list's items have been released
 */
int rcptrListClear (ItemList * l);

/**
 * \brief allocates an item and sets ref counter to one
 */
void *rcptrMallocIl (ItemList * l);

/**
 * \brief like rcptrRelease() but with itemRelease() instead of free()
 */
int rcptrReleaseIl (ItemList * l, void *ptr);

#endif
