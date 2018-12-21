#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include "pth_loc.h"
#include "fp_mm.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"

#ifdef __WIN32__
#define ftello ftell
#endif


/**
 *\file fp_mm.c
 *
 * what we want:
 * dynamic allocation. efficient(?) file access.
 *
 * How can we do that:
 * dynamic allocation: use linked lists of segments. Possibly double links. possibly separate lists for free and used. possibly ordered by size.
 *
 * fragmentation: Split block only if unused portion is of reasonable size (1/256 current allocation?) else allocate whole block. Probably introduce allocation statistics.
 *
 * Datastructures:
 *
 *
 * Heap Links: use double links to ease merging.
 * to speedup allocation it may be necessary to organize the free elements as a tree like structure. 
 * on allocation, element with smallest fitting size has to be found quickly, so sorting by size may be fine.
 * on freeing, elements adjacent to current element must be identified. this works well with doubly linked list.
 * perhaps both structures can be used in parallel?
 *
 * TODO: keep free list in ram. Currently it's very small <20 entries,
 * so we skip that for now
 */

//#define INIT_EXTENTS to initialize different areas with distinct bytes

int DF_FP_MM;
#define FILE_DBG DF_FP_MM

#pragma pack(push)
#pragma pack(1)
///this resides at the start of the heap h->base
typedef struct
{
  uint32_t id;                  ///<the FP_MM_ID ("heap")
  FpPointer last;               ///<points to last link
} heap_header;

typedef struct
{
  uint32_t id;
  FpPointer prev;
  FpPointer next;
  uint8_t used;
  FpSize size;
  FpSize max;                   //<largest available node in children
  FpPointer smaller;
  FpPointer bigger;
  FpPointer parent;
} link_header;
#pragma pack(pop)

#define MIBYTES (1024*1024)
#define FP_MM_GROW MIBYTES
#define FP_MM_ID (*((uint32_t*)"heap"))
#define FP_LINK_ID (*((uint32_t*)"link"))

static void fph_remove (FpPointer e, FpHeap * h);
static FpPointer fph_post (FpPointer e, FpHeap * h);
static FpPointer fph_pre (FpPointer e, FpHeap * h);
static void fph_merge (FpPointer ap, FpPointer bp, FpHeap * h);
static void fph_insert (FpPointer ep, FpHeap * h);
static void fph_remove_single (FpPointer e, FpHeap * h);

/**
 *\return the maximum of the element pointed to by p
 */
static FpSize
fph_read_max (FpPointer p, FpHeap * h)
{
  FpSize rv;
  FP_GO (p + offsetof (link_header, max), h->f);
  fread (&rv, sizeof (rv), 1, h->f);
  return rv;
}

/**
 *\return the size of the element pointed to by p
 */
static FpSize
fph_read_size (FpPointer p, FpHeap * h)
{
  FpSize rv;
  FP_GO (p + offsetof (link_header, size), h->f);
  fread (&rv, sizeof (rv), 1, h->f);
  return rv;
}

/**
 *\brief split the link into two. one with specified size, another one with the rest
 *
 * links must not be in tree
 *@return pointer to the rest or 0 if it would be too small to split
 */
static FpPointer
fph_split (FpPointer link, FpSize sz, FpHeap * h)
{
  link_header lh, e;
  FpPointer ep;
  FpSize old_size;
  FpSize new_size;
  FpSize rest_size;
  if (!link)
    return 0;
  FP_GO (link, h->f);
  fread (&lh, sizeof (lh), 1, h->f);
  old_size = lh.size;
  new_size = sz;
  if ((old_size <= new_size) || ((old_size - new_size) <= sizeof (link_header)))        //just the link header would fit
    return 0;


  rest_size = old_size - new_size - sizeof (link_header);

  ep = link + sizeof (link_header) + new_size;
  lh.size = new_size;

  memset (&e, 0, sizeof (e));
  e.id = FP_LINK_ID;
  e.next = lh.next;
  e.prev = link;
  e.size = rest_size;
  FP_GO (ep, h->f);
  fwrite (&e, sizeof (e), 1, h->f);

  lh.next = ep;

  FP_GO (link + offsetof (link_header, next), h->f);
  fwrite (&lh.next, sizeof (lh.next), 1, h->f);

  FP_GO (link + offsetof (link_header, size), h->f);
  fwrite (&lh.size, sizeof (lh.size), 1, h->f);

  if (e.next)
  {
    FP_GO (e.next + offsetof (link_header, prev), h->f);
    fwrite (&ep, sizeof (ep), 1, h->f);
  }
  else
  {                             //last one
    FP_GO (h->base + offsetof (heap_header, last), h->f);
    fwrite (&ep, sizeof (FpPointer), 1, h->f);
  }
//      h->node_count++;
  return ep;
}

/**
 *\brief marks specified link as used
 *
 */
static void
fph_occupy (FpPointer link, FpHeap * h)
{
  uint8_t used = 1;
  FP_GO (link + offsetof (link_header, used), h->f);
  fwrite (&used, sizeof (used), 1, h->f);
}

/**
 *\brief finds and remove the smallest sized link greater or equal sz
 *
 * IF this node is larger than request THEN:
 *   IF the smaller subtree has maximum bigger than size THEN:
 *     RETURN fph_get this->smaller
 *   ELSE:
 *     remove and RETURN this node
 *   ENDIF
 * ELSE
 *   IF this size equal to requested THEN: 
 *     remove and RETURN this node
 *   ELSE IF the maximum is bigger than request THEN:
 *     RETURN fph_get this->bigger
 *   ELSE
 *     RETURN NOPE
 *   ENDIF
 * ENDIF
 *
 *
 *@param lp1 pointer to tree_links struct to start with
 *@param sz size to look for
 */
static FpPointer
fph_get (FpPointer lp1, FpSize sz, FpHeap * h)
{
  link_header lh1;

  FP_GO (lp1, h->f);
  fread (&lh1, sizeof (lh1), 1, h->f);
  debugPri (10, "fph_get\n");

  while (1)
  {
    debugPri (10, "get: ptr:%llX, sz:%llX, max:%llX\n", lp1, lh1.size,
              lh1.max);
    if (lh1.size > sz)
    {                           //look for a smaller fitting node
      debugPri (10, "lh1.size>sz\n");
      if (lh1.smaller && (fph_read_max (lh1.smaller, h) >= sz))
      {                         //there seems to be one
        debugPri (10, "lh1.smaller&&(fph_read_max(lh1.smaller,h)>=sz)\n");
        lp1 = lh1.smaller;
        FP_GO (lp1, h->f);
        fread (&lh1, sizeof (lh1), 1, h->f);
      }
      else
      {                         //no smaller one. use this one
        debugPri (10, "else\n");
        fph_remove (lp1, h);
        return lp1;
      }
    }
    else
    {
      if (lh1.size == sz)
      {                         //found exact match
        debugPri (10, "lh1.size==sz\n");
        fph_remove (lp1, h);
        return lp1;
      }
      else
      {                         //too small. see if there are bigger nodes
        debugPri (10, "else, bigger: %llX, max: %llX\n", lh1.bigger, lh1.max);
        if (lh1.bigger && (lh1.max >= sz))      //lh1.max wrong?
        {
          lp1 = lh1.bigger;
          FP_GO (lp1, h->f);
          fread (&lh1, sizeof (lh1), 1, h->f);
        }
        else                    //none available
        {
          debugPri (10, "return 0\n");
          return 0;
        }
      }
    }
  }
}

/**
 *\brief walk up the tree and adjust max fields
 */
static void
fph_remax (FpPointer e, FpHeap * h)
{
  link_header lh_e;
  link_header lh_parent;

  FP_GO (e, h->f);
  fread (&lh_e, sizeof (lh_e), 1, h->f);
  while (1)
  {
    debugPri (10, "remax: %llX, %llX\n", e, lh_e.parent);
    if (!lh_e.parent)           //at the top
    {
      debugPri (10, "!lh_e.parent\n");
      return;
    }
    FP_GO (lh_e.parent, h->f);
    fread (&lh_parent, sizeof (lh_parent), 1, h->f);
    if (lh_parent.smaller == e) //then it can't be maximum as it's smaller than parent
    {
      debugPri (10, "lh_parent.smaller==e\n");
      return;
    }
    FP_GO (lh_e.parent + offsetof (link_header, max), h->f);
    fwrite (&lh_e.max, sizeof (lh_e.max), 1, h->f);

    e = lh_e.parent;
    FP_GO (e, h->f);
    fread (&lh_e, sizeof (lh_e), 1, h->f);
  }

}

//45786
/**
 *\brief unlink the old node and hang the new one where the old one was
 * order must be preserved: if(old>parent) then new>parent and if(old<=parent) then new<=parent
 * new may be 0;
 */
static void
fph_relink (FpPointer parent, FpPointer old, FpPointer new, FpHeap * h)
{
  link_header lh1, lh2;
  debugPri (10, "relink: %llX, %llX, %llX\n", parent, old, new);
  if (new)
  {
    debugPri (10, "new\n");
    FP_GO (new + offsetof (link_header, parent), h->f);
    fwrite (&parent, sizeof (parent), 1, h->f);
  }

  FP_GO (parent, h->f);
  fread (&lh2, sizeof (lh2), 1, h->f);
  if (old == lh2.smaller)
  {
    debugPri (10, "old==lh2.smaller\n");
    FP_GO (parent + offsetof (link_header, smaller), h->f);
    fwrite (&new, sizeof (new), 1, h->f);
  }
  else
  {
    debugPri (10, "else\n");
    FP_GO (parent + offsetof (link_header, bigger), h->f);
    fwrite (&new, sizeof (new), 1, h->f);
    //walk up the tree and adjust max fields
    if (new)
    {
      debugPri (10, "new\n");
      fph_remax (new, h);
    }
    else
    {
      debugPri (10, "else\n");
      FP_GO (parent + offsetof (link_header, max), h->f);
      fwrite (&lh2.size, sizeof (lh2.size), 1, h->f);
      fph_remax (parent, h);
    }
  }


  FP_GO (old, h->f);
  fread (&lh1, sizeof (lh1), 1, h->f);
  lh1.smaller = 0;
  lh1.bigger = 0;
  lh1.parent = 0;
  lh1.max = lh1.size;
  FP_GO (old, h->f);
  fwrite (&lh1, sizeof (lh1), 1, h->f);
}

///protects our rand_r seed
static pthread_mutex_t fph_rand_mx = PTHREAD_MUTEX_INITIALIZER;
static unsigned int fph_seed;   //better put in struct..

/**
 *\brief remove an element from the tree
 *
 * IF the element has no children THEN:
 * set the parent-><whatever> pointer to 0
 * ELSE IF the element has one child THEN:
 * set the parent-><whatever> pointer to child
 * ELSE 
 * find node in subtrees immediately before or after the current
 * one and use it to reparent 2 loose nodes
 *
 * e must not be root
 *
 */
static void
fph_remove (FpPointer e, FpHeap * h)
{
  link_header lh1, lh2;
  FpPointer work;
  FpSize max;
  FP_GO (e, h->f);
  fread (&lh1, sizeof (lh1), 1, h->f);
  debugPri (10, "remove: %llX\n", e);
  if (!lh1.smaller)
  {
    debugPri (10, "!lh1.smaller\n");
    fph_relink (lh1.parent, e, lh1.bigger, h);
  }
  else if (!lh1.bigger)
  {
    debugPri (10, "!lh1.bigger\n");
    fph_relink (lh1.parent, e, lh1.smaller, h);
  }
  else
  {
    int decision;
    debugPri (10, "else\n");
    pthread_mutex_lock (&fph_rand_mx);
    decision = rand_r (&fph_seed);
    pthread_mutex_unlock (&fph_rand_mx);
    if (decision > (RAND_MAX / 2))
    {
      debugPri (10, "post\n");
      work = fph_post (e, h);
    }
    else
    {
      debugPri (10, "pre\n");
      work = fph_pre (e, h);
    }
    //now insert work instead of e

    debugPri (10, "work: %llX\n", work);
    //e may have changed. reread
    FP_GO (e, h->f);
    fread (&lh1, sizeof (lh1), 1, h->f);

    FP_GO (work, h->f);
    fread (&lh2, sizeof (lh2), 1, h->f);

    if (lh1.bigger)             //need to test here as it may have been removed
    {                           //get the maximum
      debugPri (10, "lh1.bigger\n");
      FP_GO (lh1.bigger + offsetof (link_header, max), h->f);
      fread (&max, sizeof (max), 1, h->f);
    }
    else                        //maximum is that of element
    {
      debugPri (10, "else\n");
      max = lh2.size;
    }
    lh2.max = max;
    lh2.parent = 0;
    lh2.smaller = lh1.smaller;
    lh2.bigger = lh1.bigger;
    if (lh1.bigger)
    {
      FP_GO (lh1.bigger + offsetof (link_header, parent), h->f);
      fwrite (&work, sizeof (work), 1, h->f);
    }
    if (lh1.smaller)
    {
      FP_GO (lh1.smaller + offsetof (link_header, parent), h->f);
      fwrite (&work, sizeof (work), 1, h->f);
    }
    //subtrees now at new node

    FP_GO (work, h->f);
    fwrite (&lh2, sizeof (lh2), 1, h->f);

    //reparent node
    fph_relink (lh1.parent, e, work, h);
  }
}

static FpPointer
fph_pre (FpPointer e, FpHeap * h)
{
  link_header lh1;
//      FpPointer parent;
  FP_GO (e, h->f);
  fread (&lh1, sizeof (lh1), 1, h->f);
//      parent=e;
  e = lh1.smaller;
  FP_GO (e, h->f);
  fread (&lh1, sizeof (lh1), 1, h->f);
  while (lh1.bigger)
  {
    debugPri (10, "lh1.bigger\n");
//              parent=e;
    e = lh1.bigger;
    FP_GO (e, h->f);
    fread (&lh1, sizeof (lh1), 1, h->f);
  }
  fph_remove_single (e, h);
  return e;
}

static FpPointer
fph_post (FpPointer e, FpHeap * h)
{
  link_header lh1;
//      FpPointer parent;
  FP_GO (e, h->f);
  fread (&lh1, sizeof (lh1), 1, h->f);
//      parent=e;
  e = lh1.bigger;
  FP_GO (e, h->f);
  fread (&lh1, sizeof (lh1), 1, h->f);
  while (lh1.smaller)
  {
    debugPri (10, "lh1.smaller\n");
//              parent=e;
    e = lh1.smaller;
    FP_GO (e, h->f);
    fread (&lh1, sizeof (lh1), 1, h->f);
  }
  fph_remove_single (e, h);
  return e;
}

/**
 *\brief removes an element having only one child
 *@param e element to remove
 */
static void
fph_remove_single (FpPointer e, FpHeap * h)
{
  link_header lhe, lhparent;
  FpPointer p, parent;
  FpSize newmax;
  FP_GO (e, h->f);
  fread (&lhe, sizeof (lhe), 1, h->f);
  parent = lhe.parent;
  FP_GO (parent, h->f);
  fread (&lhparent, sizeof (lhparent), 1, h->f);
  if (lhparent.bigger == e)
  {
    if (lhe.bigger)
    {
      FP_GO (parent + offsetof (link_header, bigger), h->f);
      fwrite (&lhe.bigger, sizeof (FpPointer), 1, h->f);

      FP_GO (lhe.bigger + offsetof (link_header, parent), h->f);
      fwrite (&parent, sizeof (parent), 1, h->f);
    }
    else if (lhe.smaller)
    {
      FP_GO (parent + offsetof (link_header, bigger), h->f);
      fwrite (&lhe.smaller, sizeof (FpPointer), 1, h->f);

      FP_GO (lhe.smaller + offsetof (link_header, parent), h->f);
      fwrite (&parent, sizeof (parent), 1, h->f);

      fph_remax (lhe.smaller, h);
    }
    else
    {
      FP_GO (parent + offsetof (link_header, bigger), h->f);
      p = 0;
      fwrite (&p, sizeof (FpPointer), 1, h->f);

      FP_GO (parent + offsetof (link_header, size), h->f);
      fread (&newmax, sizeof (newmax), 1, h->f);
      FP_GO (parent + offsetof (link_header, max), h->f);
      fwrite (&newmax, sizeof (newmax), 1, h->f);
      fph_remax (parent, h);
    }
  }
  else
  {
    if (lhe.bigger)
    {
      FP_GO (parent + offsetof (link_header, smaller), h->f);
      fwrite (&lhe.bigger, sizeof (FpPointer), 1, h->f);

      FP_GO (lhe.bigger + offsetof (link_header, parent), h->f);
      fwrite (&parent, sizeof (parent), 1, h->f);
    }
    else if (lhe.smaller)
    {
      FP_GO (parent + offsetof (link_header, smaller), h->f);
      fwrite (&lhe.smaller, sizeof (FpPointer), 1, h->f);

      FP_GO (lhe.smaller + offsetof (link_header, parent), h->f);
      fwrite (&parent, sizeof (parent), 1, h->f);
    }
    else
    {
      FP_GO (parent + offsetof (link_header, smaller), h->f);
      p = 0;
      fwrite (&p, sizeof (FpPointer), 1, h->f);
    }
  }
}

#if 0
/*
 *walk through the list and join empty nodes
 *as far as possible
 *Goes through the whole list, takes too long.
 *@param start pointer to the first element. Must not be the first (dummy) element.
 */
 //not used, too slow
void
fph_coalesce (FpPointer start, FpHeap * h)
{
  FpPointer ap, bp;
  link_header a, b;
  if (!start)
  {
    debugPri (10, "!start\n");
    return;
  }
  ap = start;
  FP_GO (ap, h->f);
  fread (&a, sizeof (a), 1, h->f);

  while (a.next)
  {
    debugPri (10, "1\n");
    bp = a.next;
    FP_GO (bp, h->f);
    fread (&b, sizeof (b), 1, h->f);
    if (!(a.used || b.used))
    {
      debugPri (10, "!(a.used||b.used)\n");
      fph_remove (ap, h);
      fph_remove (bp, h);
      fph_merge (ap, bp, h);
      fph_insert (ap, h);

      FP_GO (ap, h->f);
      fread (&a, sizeof (a), 1, h->f);
    }
    else
    {
      debugPri (10, "else\n");
      ap = bp;
      FP_GO (ap, h->f);
      fread (&a, sizeof (a), 1, h->f);
    }
  }
  //update last pointer
  FP_GO (h->base + offsetof (heap_header, last), h->f);
  fwrite (&ap, sizeof (FpPointer), 1, h->f);
}
#endif

/**
 *\brief Try to join current element and neighboring ones
 *
 *@param start pointer to the first link to use. 
 * Must be loose(not in tree and free). 
 * Must not be the first (dummy) element.
 *@return pointer to resulting node. Loose too.
 */
static FpPointer
fph_coalesce1 (FpPointer start, FpHeap * h)
{
  FpPointer ap, bp, cp;
  link_header a, b, c;
  if (!start)
  {
    errMsg ("!start\n");
    return 0;
  }
  bp = start;
  FP_GO (bp, h->f);
  fread (&b, sizeof (b), 1, h->f);

  if (b.next)
  {
    debugPri (10, "b.next\n");
    cp = b.next;
    FP_GO (cp, h->f);
    fread (&c, sizeof (c), 1, h->f);
    if (!(b.used || c.used))
    {
      debugPri (10, "!(b.used||c.used)\n");
      fph_remove (cp, h);
      fph_merge (bp, cp, h);
    }
  }

  if (b.prev)
  {
    debugPri (10, "b.prev\n");
    ap = b.prev;
    FP_GO (ap, h->f);
    fread (&a, sizeof (a), 1, h->f);
    if (a.prev)                 //not first one
    {
      if (!(a.used || b.used))
      {
        debugPri (10, "!(a.used||b.used)\n");
        fph_remove (ap, h);
        fph_merge (ap, bp, h);
        return ap;
      }
    }
  }
  return bp;
}

/**
 *\brief merges two adjacent elements
 *
 * elements must be unused and adjacent
 * afterwards element at ap includes bp and bp is gone
 *@param ap element before bp
 *@param bp element after ap
 */
static void
fph_merge (FpPointer ap, FpPointer bp, FpHeap * h)
{
  link_header a, b;
  FpSize i;
  FpPointer cp;

  FP_GO (ap, h->f);
  fread (&a, sizeof (a), 1, h->f);

  FP_GO (bp, h->f);
  fread (&b, sizeof (b), 1, h->f);

  cp = b.next;

  if (cp)
  {                             //update the backpointer from c(if there _is_ a c)
    debugPri (10, "cp\n");
    FP_GO (cp + offsetof (link_header, prev), h->f);
    fwrite (&ap, sizeof (ap), 1, h->f);
  }
  else
  {                             //last element. update heap header
    debugPri (10, "else\n");
    FP_GO (h->base + offsetof (heap_header, last), h->f);
    fwrite (&ap, sizeof (FpPointer), 1, h->f);
  }

  a.next = cp;
  //new size includes b's data and header sizes
  a.size += b.size + sizeof (link_header);
  //have a point at c (c may be NULL)
  FP_GO (ap + offsetof (link_header, next), h->f);
  fwrite (&cp, sizeof (cp), 1, h->f);
  //update size
  FP_GO (ap + offsetof (link_header, size), h->f);
  fwrite (&a.size, sizeof (a.size), 1, h->f);
//      h->node_count--;

  FP_GO (bp, h->f);
#ifdef INIT_EXTENTS
  for (i = 0; i < sizeof (link_header); i++)
  {
    fputc (0xDD, h->f);
  }
#endif
}

static void
fph_insert (FpPointer ep, FpHeap * h)
{
  link_header a, e;
  FpPointer ap;
  ap = h->base + sizeof (heap_header);
  FP_GO (ap, h->f);
  fread (&a, sizeof (a), 1, h->f);

  FP_GO (ep, h->f);
  fread (&e, sizeof (e), 1, h->f);
  e.smaller = 0;
  e.bigger = 0;
  e.max = e.size;

  while (1)
  {
    if (e.size <= a.size)
    {
      debugPri (10, "e.size<=a.size\n");
      if (!a.smaller)
      {
        debugPri (10, "!a.smaller\n");
        FP_GO (ap + offsetof (link_header, smaller), h->f);
        fwrite (&ep, sizeof (ep), 1, h->f);
        e.parent = ap;
        FP_GO (ep, h->f);
        fwrite (&e, sizeof (e), 1, h->f);
        fph_remax (ep, h);
        return;
      }
      else
      {
        debugPri (10, "else\n");
        ap = a.smaller;
      }
    }
    else
    {
      if (!a.bigger)
      {
        debugPri (10, "!a.bigger\n");
        FP_GO (ap + offsetof (link_header, bigger), h->f);
        fwrite (&ep, sizeof (ep), 1, h->f);
        e.parent = ap;
        FP_GO (ep, h->f);
        fwrite (&e, sizeof (e), 1, h->f);
        fph_remax (ep, h);
        return;
      }
      else
      {
        debugPri (10, "else\n");
        ap = a.bigger;
      }
    }
    FP_GO (ap, h->f);
    fread (&a, sizeof (a), 1, h->f);
  }
}

/**
 *\brief grow the file by at least size
 *
 * currently this will generate just one element at the end of the file of size sz
 * or larger
 */
static void
fph_grow (FpSize sz, FpHeap * h)
{
  link_header a, b;
  FpPointer ap, bp;
  FpSize i;
  heap_header hh;
  if (sz < h->fragment_size)
    sz = h->fragment_size;
  FP_GO (h->base, h->f);
  fread (&hh, sizeof (heap_header), 1, h->f);

  ap = hh.last;
  FP_GO (ap, h->f);
  fread (&a, sizeof (a), 1, h->f);

  a.next = ap + a.size + sizeof (link_header);

  FP_GO (ap + offsetof (link_header, next), h->f);
  fwrite (&a.next, sizeof (FpPointer), 1, h->f);

  bp = a.next;
  b.id = FP_LINK_ID;
  b.prev = ap;
  b.next = 0;
  b.size = sz;
  b.used = 0;
  b.max = sz;
  b.smaller = 0;
  b.bigger = 0;
  b.parent = 0;

/*
	Write the last byte first
	I suspect this is more efficient and faster, as the last byte gets written first, 
	and the file will have grown when we initialize the rest.. less fragmentation..
	well, it might still do something with sparse data, so blocks inbetween may not actually be
	there... how can I tell it to grow to a specified size and possibly in consecutive blocks?
	truncate.. of course...
*/

//are those necessary or not?
//      fflush(h->f);
//      fclean(h->f);
//      ftruncate(fileno(h->f),bp+sizeof(b)+sz);
//well, that didn't work, it seems.. will this do?
//      posix_fallocate(fileno(h->f),bp,sizeof(b)+sz);
//nah, still no phun. file gets scattered across even more extents...?!
//  FP_GO (bp + sizeof (b) + sz - 1, h->f);
  //fputc (0xEE, h->f);
  FP_GO (bp, h->f);
  fwrite (&b, sizeof (b), 1, h->f);     //write the header
#ifdef INIT_EXTENTS
  for (i = 0; i < sz; i++)
  {
    fputc (0xEE, h->f);         //initialize data
  }
#endif
  hh.last = bp;                 //update last pointer
  FP_GO (h->base + offsetof (heap_header, last), h->f);
  fwrite (&hh.last, sizeof (FpPointer), 1, h->f);

  fph_insert (bp, h);
//      h->node_count++;
}

/**
 *\brief shrink the file
 *
 * see if the last node is free. 
 * If it is and it isn't the first(dummy) node and 
 * at least 2*fragment_size, remove from tree,
 * update links, truncate file
 *
 * If the last node is free... repeat
 */
static void
fph_shrink (FpHeap * h)
{
  FpPointer last_p;
  link_header l;
  while (1)
  {
    FP_GO (h->base + offsetof (heap_header, last), h->f);
    fread (&last_p, sizeof (FpPointer), 1, h->f);

    FP_GO (last_p, h->f);
    fread (&l, sizeof (l), 1, h->f);

    if ((!l.used) && l.prev && (l.size >= (2 * h->fragment_size)))
    {
      int fd;
      fph_remove (last_p, h);
      fd = fileno (h->f);
      fflush (h->f);            //something stays left behind. does this solve it?
      ftruncate (fd, last_p);

      last_p = 0;
      FP_GO (l.prev + offsetof (link_header, next), h->f);
      fwrite (&last_p, sizeof (last_p), 1, h->f);

      last_p = l.prev;
      FP_GO (h->base + offsetof (heap_header, last), h->f);
      fwrite (&last_p, sizeof (last_p), 1, h->f);
    }
    else
      return;
  }
}


/**
 *\brief Basic sanity check for heap header
 */
static int
fph_heap_ok (FpHeap * h)
{
  heap_header id;
  FP_GO (h->base, h->f);
  fread (&id, sizeof (id), 1, h->f);
  return (id.id == FP_MM_ID);
}

/**
 *\brief BSC for a single heap link
 */
static int
fph_link_ok (FpPointer p, FpHeap * h)
{
  uint32_t id;
  FP_GO (p + offsetof (link_header, id), h->f);
  fread (&id, sizeof (id), 1, h->f);
  return (id == FP_LINK_ID);
}

void
fphFree (FpPointer p, FpHeap * h)
{
  uint8_t used = 0;
  FpPointer prev;
  FpSize sz, i;
  if (!p)
  {
    debugPri (10, "fphFree(0)\n");
    return;
  }
  p -= sizeof (link_header);
  debugPri (10, "fphFree: %llX\n", p);
  if (!fph_link_ok (p, h))
  {
    errMsg ("link id mismatch. cannot free");
    return;
  }
  FP_GO (p + offsetof (link_header, used), h->f);
  fread (&used, sizeof (used), 1, h->f);
  if (!used)
  {
    errMsg ("not used. cannot free");
    return;
  }
#ifdef INIT_EXTENTS
  sz = fph_read_size (p, h);
  FP_GO (p + sizeof (link_header), h->f);
  for (i = 0; i < sz; i++)
  {
    fputc (0xCC, h->f);
  }
#endif
  used = 0;
  FP_GO (p + offsetof (link_header, used), h->f);
  fwrite (&used, sizeof (used), 1, h->f);

  FP_GO (p + offsetof (link_header, prev), h->f);
  fread (&prev, sizeof (prev), 1, h->f);

//      FP_GO(h->base+offsetof(heap_header,last),h->f);
//      fread(&last,sizeof(FpPointer),1,h->f);

  if (prev)
    p = fph_coalesce1 (p, h);

  fph_insert (p, h);

  fph_shrink (h);

//      h->transaction_count++;
}

FpPointer
fphMalloc (FpSize sz, FpHeap * h)
{
  FpPointer ptr;
  FpPointer rp;
//      int64_t v;
  FpPointer fitting;
  debugPri (10, "fphMalloc: %llX\n", sz);
  if (!sz)
  {
    debugPri (10, "!sz\n");
    return 0ULL;
  }
  if (!fph_heap_ok (h))
  {
    errMsg ("heap id mismatch\n");
    return 0ULL;
  }
  rp = h->base + sizeof (heap_header);

  if (!(fitting = fph_get (rp, sz, h)))
  {
    debugPri (10, "!(fitting=fph_get(rp,sz,h))\n");
    fph_grow (sz, h);
    fitting = fph_get (rp, sz, h);
  }
  debugPri (10, "got: %llX\n", fitting);
  if (!fitting)                 //why does this happen?
  {                             //naah, it doesn't
    abort ();                   //yes, it does...
    return 0;
  }
  if (fph_read_size (fitting, h) > (sz + sz / 2 + 2 * sizeof (link_header)))
  {
    debugPri (10,
              "fph_read_size(fitting,h)>(sz+sz/2+2*sizeof(link_header))\n");
    ptr = fph_split (fitting, sz, h);
    if (ptr)
      fph_insert (ptr, h);
  }
  debugPri (10, "fph_occupy\n", fitting);
  fph_occupy (fitting, h);
  debugPri (10, "fph_occupy2\n", fitting);
  return (fitting + sizeof (link_header));
}

/**
 *\brief initialize a heap
 * heap starts with
 * |heap|
 * next is the root node(link_header) of size zero(so it doesn't get fetched)
 * |ROOT|
 * then follow several links
 * struct link_header followed by size bytes of data space
 */
int
fphInit (FILE * f, FpHeap * h)
{
  link_header start;
  heap_header hh;

  h->f = f;
  h->base = ftello (f);
  debugPri (10, "FpPointer is %u bytes\n", sizeof (FpPointer));

  hh.id = FP_MM_ID;
  hh.last = h->base + sizeof (heap_header);

  start.id = FP_LINK_ID;
  start.prev = 0;
  start.next = 0;
  start.size = 0;
  start.max = 0;
  start.smaller = 0;
  start.bigger = 0;
  start.parent = 0;
  start.used = 0;

  fwrite (&hh, sizeof (heap_header), 1, f);
  fwrite (&start, sizeof (start), 1, f);
  h->fragment_size = 10 * MIBYTES;
//      h->alloc=255;
//      h->dealloc=0;
//      h->avg_req_sz=FP_MM_GROW;
//      h->var=0;
//      h->transaction_count=0;
//      h->node_count=1;
  return 0;
}

void
fphSetFragmentSize (FpHeap * h, FpSize sz)
{
  if (sz < (2 * sizeof (link_header)))
    sz = 2 * sizeof (link_header);
  h->fragment_size = sz;
}

static FpSize
fph_count_nodes (FpHeap * h)
{
  FpSize rv = 0;
  FpPointer current;
  current = h->base + sizeof (heap_header);     //this is the dummy node which is always there, so the minimum count is one
  while (current)
  {
    uint32_t id;
    rv++;
    FP_GO (current + offsetof (link_header, id), h->f);
    fread (&id, sizeof (id), 1, h->f);
    if (id != FP_LINK_ID)
    {
      errMsg ("invalid link id\n");
      return 0;
    }
    FP_GO (current + offsetof (link_header, next), h->f);
    fread (&current, sizeof (FpPointer), 1, h->f);
  }
  return rv;
}

typedef struct
{
  uint64_t num_free;
  float block_epsilon;
  float block_sigma;
  uint64_t block_min;
  uint64_t block_max;
} FphStat;

static int
fph_count_free (FpHeap * h, FpPointer root)
{
  link_header lh;
  if (root == 0ULL)
    return 0;
  FP_GO (root, h->f);
  fread (&lh, sizeof (lh), 1, h->f);
  return 1 + fph_count_free (h, lh.smaller) + fph_count_free (h, lh.bigger);
}

static void
fph_get_sizes (uint64_t * sizes, int *ctr, FpHeap * h, FpPointer root)
{
  link_header lh;
  if (root == 0ULL)
    return;

  FP_GO (root, h->f);
  fread (&lh, sizeof (lh), 1, h->f);

  fph_get_sizes (sizes, ctr, h, lh.smaller);
  sizes[*ctr] = lh.size;
  (*ctr)++;
  fph_get_sizes (sizes, ctr, h, lh.bigger);
}

/**
 *\brief gather statistics
 *
 * enumerate over all free nodes
 * collecting numbers, sizes, minimum,maximum,average and variance
 *@param h the heap to use
 *@param s must point to a FphStat object. will store results there.
 */
static int
fph_get_stat (FphStat * s, FpHeap * h)
{
  int i;
  FpPointer current;
  uint64_t *sizes;
  int num_free;
  int ctr;
  current = h->base + sizeof (heap_header);     //this is the dummy node which is always there, so the minimum count is one
  //count free nodes
  num_free = fph_count_free (h, current);
  debugMsg ("num_free: %u\n", num_free);
  sizes = utlCalloc (sizeof (uint64_t), num_free);
  ctr = 0;
  fph_get_sizes (sizes, &ctr, h, current);

  s->block_min = UINT64_MAX;
  s->block_max = 0ULL;
  s->num_free = 0;
  s->block_epsilon = 0.0;
  s->block_sigma = 0.0;
  for (i = 0; i < ctr; i++)
  {
    if (sizes[i])
    {
      s->num_free++;
      s->block_epsilon += sizes[i];
      if (s->block_min > sizes[i])
        s->block_min = sizes[i];
      if (s->block_max < sizes[i])
        s->block_max = sizes[i];
    }
  }
  s->block_epsilon /= ctr - 1;

  for (i = 0; i < ctr; i++)
  {
    if (sizes[i])
    {
      float tmp = s->block_epsilon - sizes[i];
      tmp *= tmp;
      s->block_sigma += tmp;
    }
  }
  s->block_sigma /= ctr - 1;
  utlFAN (sizes);
  return 0;
}

int
fphAttach (FILE * f, FpHeap * h)
{
  uint32_t id;
  FphStat fps;
  h->f = f;
  h->base = ftello (f);
  fread (&id, sizeof (id), 1, f);
  if (id != FP_MM_ID)
    return 1;
  h->fragment_size = 10 * MIBYTES;
//      h->alloc=255;
//      h->dealloc=0;
//      h->avg_req_sz=FP_MM_GROW;
//      h->var=0;
//      h->transaction_count=0;
//      h->node_count=fph_count_nodes(h);
//      if(h->node_count==0)
//              return 1;
  if (debugOn ())
  {
/*
	uint64_t num_free;
	float block_epsilon;
	float block_sigma;
	uint64_t block_min;
	uint64_t block_max;
*/
    fph_get_stat (&fps, h);
    debugPri (1, "FpPointer is %u bytes\n \
Node count is %llu\n\
Num free: %llu\n\
Epsilon:  %f\n\
Sigma:    %f\n\
Min:      %llu\n\
Max:      %llu\n", sizeof (FpPointer), fph_count_nodes (h), fps.num_free, fps.block_epsilon, fps.block_sigma, fps.block_min, fps.block_max);
  }
  return 0;
}

int
fphZero (FpHeap * h)
{
  int fd;
  fd = fileno (h->f);
  fflush (h->f);
  ftruncate (fd, h->base);
  FP_GO (h->base, h->f);
  return fphInit (h->f, h);
}

FpPointer
fphCalloc (FpSize nmemb, FpSize sz, FpHeap * h)
{
  FpPointer rv;
  FpSize total = nmemb * sz;
  debugPri (10, "fphCalloc\n");
  rv = fphMalloc (total, h);
  if (rv)
  {
    int i;
    FP_GO (rv, h->f);
    for (i = 0; i < total; i++)
    {
      fputc (0, h->f);
    }
  }
  return rv;
}

FpPointer
fppRead (FpPointer p, FILE * f)
{
  FpPointer rv;
  FP_GO (p, f);
  fread (&rv, sizeof (FpPointer), 1, f);
  return rv;
}

void
fppWrite (FpPointer dest, FpPointer src, FILE * f)
{
  FP_GO (dest, f);
  fwrite (&src, sizeof (FpPointer), 1, f);
}


/*
int fph_read_at(void * data, FpSize s, FpPointer p, FpHeap * h)
{
	FP_GO(p,h->f);
	fread(data,s,1,h->f);

}

int fph_write_at(void * data, FpSize s, FpPointer p, FpHeap * h)
{
	FP_GO(p,h->f);
	fwrite(data,s,1,h->f);
}

*/
