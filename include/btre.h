#ifndef BTRE_H
#define BTRE_H
#include <stdio.h>
#include <stdint.h>
/**
 *\file btre.h
 *\brief a binary tree (now balanced)
 *
 *After some headaches I've decided on an AVL variant.
 */

/**
 *\brief the tree type 
 *fwd declaration needed as struct contains pointers to objects of same type
 */
typedef struct _BTreeNode_ BTreeNode;
///the tree structure
struct _BTreeNode_
{
  BTreeNode *Child[2];          ///<points to the two child nodes. may be NULL
  BTreeNode *Parent;            ///<points to the parent node. may be NULL
  unsigned height;
//  unsigned color:1;
  //int8_t balance;///< 0: both branches equal size, -1: left=right+1, 1: left+1=right
};

/**
 *\brief insert a node into tree
 *\return 0 on success
 *others on error(element exists etc..)
 */
int btreeNodeInsert (BTreeNode ** top, BTreeNode * elem, void *x,
                     int (*compar) (void *, const void *, const void *));
/**
 *\brief convenience function
 *
 *tries to find a matching btree node. if none exists, a new one is created and the cv parameter is used for initialisation
 *
 *\param root pointer to the root pointer of the tree(may be changed by function)
 *\param cv compare element or initialisation element
 *\param cmp compare function
 *\param size size of the element(payload) to create
 *\return pointer to the node or NULL if a creation failed(calloc).
 */
BTreeNode *btreeNodeGetOrIns (BTreeNode ** root, void *cv, void *x,
                              int (*cmp) (void *, const void *, const void *),
                              size_t size);
///find a node. user must supply comparison function
BTreeNode *btreeNodeFind (BTreeNode * top, void *elem, void *x,
                          int (*compar) (void *, const void *, const void *));
///returns the size to allocate for anode given payload size
size_t btreeNodeSize (size_t size_of_payload);

///remove a node from tree
int btreeNodeRemove (BTreeNode ** root, BTreeNode * e);
///get pointer to node contents
void *btreeNodePayload (BTreeNode * b);
///get node pointer from contents pointer
BTreeNode *btreePayloadNode (void *payload);
/**
 *\brief make full pass over the tree calling a function at each node
 *
 *the which parameter to visit is 0 for the first visit before descending Child[0], 1 after Child[0] and before Child[1], 
 *2 after Child[1] and 3 at leaf nodes.<br>
 *It is safe to just free nodes if which equals two or three.
 *\param visit function to call
 */
int btreeWalk (BTreeNode * top, void *x,
               void (*visit) (void *x, BTreeNode * this, int which,
                              int depth));

/**
 *\brief writes the tree to disk
 *
 *tree stays unchanged
 *\param top the root of the tree
 *\param x a void pointer, user specified
 *\param write a function to write out payload data (not the node itself)
 *on return of this function filepos must be beyond the end of written data as writeBTree() will proceed from there
 *\param f a writable stream
 *\return one on error zero on success.
 */
int btreeWrite (BTreeNode * top, void *x,
                int (*write) (void *x, BTreeNode * this, FILE * f), FILE * f);

/**
 *\brief calculate the size needed to write tree
 */
uint64_t btreeWriteSize (BTreeNode * top, void *x,
                         uint64_t (*size) (void *x, BTreeNode * this));

/**
 *\brief reads tree from disk
 *
 *\param top pointer to the root pointer of the tree
 *\param x a void pointer, user specified
 *\param read a function to allocate Node+payload and read payload data
 *on return of this function filepos must be beyond the end of read data as readBTree() will proceed from there
 *\param f a readable stream
 *\return one on error zero on success.
 */
int btreeRead (BTreeNode ** top, void *x,
               BTreeNode * (*read) (void *x, FILE * f), FILE * f);

#endif
