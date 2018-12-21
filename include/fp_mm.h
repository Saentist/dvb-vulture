#ifndef __fp_mm_h
#define __fp_mm_h
/**
 *\file fp_mm.h
 *\brief file pointer memory management (unconstrained allocator)
 *
 *provides a malloc like interface to files
 *is not portable between different architectures or different compilers or different compilation flags 
 *as it may use offsets into structures
 *why can't I define _LARGEFILE_SOURCE and _FILE_OFFSET_BITS 64 here and have 64 bit offsets? Does the compiler implicitly set linker options?
 */
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

typedef off_t FpPointer;
typedef off_t FpSize;

typedef struct
{
  FILE *f;
  FpPointer base;
        /**
	 *allocation counter
	 *
	 *upon allocation we increment alloc if below 255
	 *else we decrement dealloc if above zero
	 *these values allow us to form the ratio of allocations/deallocations
	 *which aids us in deciding by how much we grow the file
	 *currently unused
	 */
//      uint8_t alloc;
//      uint8_t dealloc;
        /**
	 *holds a floating average of the request size
	 *
	 *this value helps us decide if we merge blocks on freeing and how large they should be
	 *computed: new=old*(1-f)+req*f
	 *f=1/256
	 *currently unused
	 */
//      FpSize avg_req_sz;
        /**
	 *average variance in request size. currently unused
	 */
//      FpSize var;
/**
 *\brief fragmentation size
 *
 *When needed, grows the file by this amount 
 *at least and shrinks by at least twice this 
 *amount to reduce file fragmentation.
 *Defaults to 10 Megabytes(MeBiBytes)
 */
  FpSize fragment_size;
//      FpSize node_count;///<total number of nodes. gets counted on attach or set on init.
//      FpSize transaction_count;///<number of fp_frees. set to zero on init, attach or coalesce. Incremented on fp_free, Used to linearize coalescing effort.
} FpHeap;


/**
 *returns a pointer to use with FP_GO and fwrite/fread
 */
FpPointer fphMalloc (FpSize sz, FpHeap * h);

/**
 *\brief calloc look-alike
 *
 *alocates and zeroes block of size sz*nmemb
 */
FpPointer fphCalloc (FpSize nmemb, FpSize sz, FpHeap * h);

/**
 *frees an allocation at p
 *
 *@param p must be a pointer previously returned by fp_malloc
 */
void fphFree (FpPointer p, FpHeap * h);

/**
 *initializes a new heap
 *new heap starts at current fileposition
 *@param f file pointer to use. Must be open.
 */
int fphInit (FILE * f, FpHeap * h);
/**
 *\brief sets the fragment size(in bytes)
 *
 *is not restored on attach. Set to 10 Megabytes on Init and Attach.
 *Sizes smaller than 2*sizeof(link_header) are rounded up.
 */
void fphSetFragmentSize (FpHeap * h, FpSize sz);
/**
 *attaches to an already existing heap
 *filepos must be at start of heap
 */
int fphAttach (FILE * f, FpHeap * h);

/**
 *remove all data from a heap
 */
int fphZero (FpHeap * h);

///read a file pointer from location p
FpPointer fppRead (FpPointer p, FILE * f);
///write file pointer src to location dest
void fppWrite (FpPointer dest, FpPointer src, FILE * f);

#if 0
/**
 *\brief macro to read a structure element at specified file address
 */
#define GET_STRUCT_ELEMENT(RTN,BASEPTR,TYPE,MEMBER,HEAP) fp_read_at((void*)(&(RTN)),(FpSize)sizeof(MEMBER),(FpPointer)(BASEPTR)+offsetof((TYPE),(MEMBER)),(HEAP))

/**
 *\brief macro to write a structure element at specified file address
 */
#define SET_STRUCT_ELEMENT(VAL,BASEPTR,TYPE,MEMBER,HEAP) fp_write_at((void*)(&(VAL)),(FpSize)sizeof(MEMBER),(FpPointer)(BASEPTR)+offsetof((TYPE),(MEMBER)),(HEAP))
#endif

/**
 *\brief macro to set file position
 */
#ifdef __WIN32__
#define FP_GO(POINTER,FILE) fseek(FILE,POINTER,SEEK_SET);
#else
#define FP_GO(POINTER,FILE) fseeko(FILE,POINTER,SEEK_SET);
#endif

#endif
